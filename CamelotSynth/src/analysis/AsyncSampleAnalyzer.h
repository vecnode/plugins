#pragma once

#include "SampleNoteDetector.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

BEGIN_IPLUG_NAMESPACE

/** Background worker for offline MIR — never blocks ProcessBlock or the UI message loop. */
class AsyncSampleAnalyzer
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

  struct Snapshot
  {
    std::vector<float> mono;
    int sampleRate = 0;
  };

  AsyncSampleAnalyzer() = default;

  ~AsyncSampleAnalyzer()
  {
    StopWorker();
  }

  AsyncSampleAnalyzer(const AsyncSampleAnalyzer&) = delete;
  AsyncSampleAnalyzer& operator=(const AsyncSampleAnalyzer&) = delete;

  void SetSnapshot(Snapshot snapshot)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mSnapshot = std::move(snapshot);
  }

  /** O(1) — safe from OnParamChange (audio or UI thread). */
  void RequestAnalysis()
  {
    EnsureWorkerRunning();

    {
      std::lock_guard<std::mutex> lock(mMutex);
      ++mJobSerial;
      mPhase = Phase::Queued;
    }

    mJobPending.store(true, std::memory_order_release);
    mCv.notify_one();
  }

  /** Non-blocking UI sync — call from OnIdle only. Returns true when the editor should refresh. */
  bool ConsumeEditorUpdate(Phase& phaseOut, DetectedNote& noteOut)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    phaseOut = mPhase;
    noteOut = mLastResult;

    const bool changed = phaseOut != mLastPolledPhase || mResultSerial != mLastPolledResultSerial;
    if (changed)
    {
      mLastPolledPhase = phaseOut;
      mLastPolledResultSerial = mResultSerial;
    }

    return changed;
  }

private:
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

      Snapshot snapshot = mSnapshot;
      const uint32_t jobSerial = mJobSerial;
      mPhase = Phase::Running;
      lock.unlock();

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

      mLastResult = result;
      ++mResultSerial;
      mPhase = result.valid ? Phase::Succeeded : Phase::Failed;
    }
  }

  mutable std::mutex mMutex;
  std::condition_variable mCv;
  std::thread mWorker;

  Snapshot mSnapshot;
  DetectedNote mLastResult;
  Phase mPhase = Phase::Idle;
  uint32_t mJobSerial = 0;
  uint32_t mResultSerial = 0;
  uint32_t mLastPolledResultSerial = 0;
  Phase mLastPolledPhase = Phase::Idle;

  std::atomic<bool> mStop {false};
  std::atomic<bool> mJobPending {false};
};

END_IPLUG_NAMESPACE
