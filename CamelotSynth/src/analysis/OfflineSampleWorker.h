#pragma once

#include "SampleNoteDetector.h"
#include "SamplePitchProcessor.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

BEGIN_IPLUG_NAMESPACE

/**
 * Single background worker for offline MIR and pitch processing.
 *
 * Contract:
 *  - Request* methods are O(1): queue state and wake the worker thread.
 *  - Heavy work (audioFlux YIN, pitch shift) runs only in WorkerLoop().
 *  - ProcessBlock never waits on this class; SampleTransport swaps buffers atomically.
 *  - OnIdle polls Consume*Update() to refresh the editor.
 */
class OfflineSampleWorker
{
public:
  enum class Phase
  {
    Idle,
    Queued,
    Running,
    Succeeded,
    Failed
  };

  enum class JobType
  {
    None,
    DetectNote,
    PitchUpOne
  };

  struct Snapshot
  {
    std::vector<float> mono;
    std::vector<float> left;
    std::vector<float> right;
    int sampleRate = 0;
  };

  struct PitchResult
  {
    bool ok = false;
    std::vector<float> left;
    std::vector<float> right;
    DetectedNote note;
  };

  OfflineSampleWorker() = default;

  ~OfflineSampleWorker()
  {
    StopWorker();
  }

  OfflineSampleWorker(const OfflineSampleWorker&) = delete;
  OfflineSampleWorker& operator=(const OfflineSampleWorker&) = delete;

  void SetSnapshot(Snapshot snapshot)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mSnapshot = std::move(snapshot);
  }

  void RequestDetect()
  {
    QueueJob(JobType::DetectNote);
  }

  void RequestPitchUpOne(const DetectedNote& referenceNote)
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mReferenceNote = referenceNote;
    }
    QueueJob(JobType::PitchUpOne);
  }

  bool ConsumeDetectUpdate(Phase& phaseOut, DetectedNote& noteOut)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mActiveJob != JobType::DetectNote)
      return false;

    phaseOut = mPhase;
    noteOut = mLastDetectResult;

    const bool changed = phaseOut != mLastPolledDetectPhase || mDetectSerial != mLastPolledDetectSerial;
    if (changed)
    {
      mLastPolledDetectPhase = phaseOut;
      mLastPolledDetectSerial = mDetectSerial;
    }

    return changed;
  }

  bool ConsumePitchUpdate(Phase& phaseOut, PitchResult& resultOut)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mActiveJob != JobType::PitchUpOne)
      return false;

    phaseOut = mPhase;
    resultOut = mLastPitchResult;

    const bool changed = phaseOut != mLastPolledPitchPhase || mPitchSerial != mLastPolledPitchSerial;
    if (changed)
    {
      mLastPolledPitchPhase = phaseOut;
      mLastPolledPitchSerial = mPitchSerial;
    }

    return changed;
  }

  JobType GetActiveJob() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mActiveJob;
  }

  Phase GetPhase() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mPhase;
  }

  bool IsBusy() const
  {
    const Phase phase = GetPhase();
    return phase == Phase::Queued || phase == Phase::Running;
  }

private:
  void QueueJob(JobType job)
  {
    EnsureWorkerRunning();

    {
      std::lock_guard<std::mutex> lock(mMutex);
      QueueJobLocked(job);
    }

    mJobPending.store(true, std::memory_order_release);
    mCv.notify_one();
  }

  void QueueJobLocked(JobType job)
  {
    ++mJobSerial;
    mActiveJob = job;
    mPhase = Phase::Queued;
  }

  void EnsureWorkerRunning()
  {
    if (mWorker.joinable())
      return;

    mStop.store(false, std::memory_order_release);
    mWorker = std::thread([this] { WorkerLoop(); });
  }

  void StopWorker()
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mStop.store(true, std::memory_order_release);
    }
    mCv.notify_all();

    if (mWorker.joinable())
      mWorker.join();
  }

  void WorkerLoop()
  {
    for (;;)
    {
      std::unique_lock<std::mutex> lock(mMutex);

      mCv.wait(lock, [this] {
        return mStop.load(std::memory_order_acquire)
               || mJobPending.load(std::memory_order_acquire);
      });

      if (mStop.load(std::memory_order_acquire))
        return;

      mJobPending.store(false, std::memory_order_release);

      const JobType job = mActiveJob;
      Snapshot snapshot = mSnapshot;
      DetectedNote referenceNote = mReferenceNote;
      const uint32_t jobSerial = mJobSerial;
      mPhase = Phase::Running;
      lock.unlock();

      if (job == JobType::DetectNote)
      {
        DetectedNote result;
        if (snapshot.mono.empty() || snapshot.sampleRate <= 0)
          result.valid = false;
        else
          result = SampleNoteDetector::AnalyzeMono(snapshot.mono.data(),
                                                   static_cast<int>(snapshot.mono.size()),
                                                   snapshot.sampleRate);

        lock.lock();
        if (jobSerial != mJobSerial)
          continue;

        mLastDetectResult = result;
        ++mDetectSerial;
        mPhase = result.valid ? Phase::Succeeded : Phase::Failed;
      }
      else if (job == JobType::PitchUpOne)
      {
        PitchResult result;
        if (!referenceNote.valid || snapshot.left.empty() || snapshot.sampleRate <= 0)
          result.ok = false;
        else
        {
          const PitchProcessResult processed = SamplePitchProcessor::ShiftBySemitones(
            snapshot.left, snapshot.right, snapshot.sampleRate, 1);

          result.ok = processed.ok;
          result.left = std::move(processed.left);
          result.right = std::move(processed.right);
          if (result.ok)
            result.note = SampleNoteDetector::Transpose(referenceNote, 1);
        }

        lock.lock();
        if (jobSerial != mJobSerial)
          continue;

        mLastPitchResult = result;
        ++mPitchSerial;
        mPhase = result.ok ? Phase::Succeeded : Phase::Failed;
      }
    }
  }

  mutable std::mutex mMutex;
  std::condition_variable mCv;
  std::thread mWorker;

  Snapshot mSnapshot;
  DetectedNote mReferenceNote;
  DetectedNote mLastDetectResult;
  PitchResult mLastPitchResult;

  JobType mActiveJob = JobType::None;
  Phase mPhase = Phase::Idle;
  Phase mLastPolledDetectPhase = Phase::Idle;
  Phase mLastPolledPitchPhase = Phase::Idle;

  uint32_t mJobSerial = 0;
  uint32_t mDetectSerial = 0;
  uint32_t mPitchSerial = 0;
  uint32_t mLastPolledDetectSerial = 0;
  uint32_t mLastPolledPitchSerial = 0;

  std::atomic<bool> mStop {false};
  std::atomic<bool> mJobPending {false};
};

END_IPLUG_NAMESPACE
