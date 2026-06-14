#pragma once

#include "PitchStreamCache.h"
#include "SamplePitchProcessor.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace audioagent
{

/**
 * Background block processor — full audioFlux pitchShift on ~10 s windows ahead of playhead.
 * Never called from ProcessBlock.
 */
class PitchStreamWorker
{
public:
  static constexpr int kReadAheadBlocks = 2;

  PitchStreamWorker() = default;

  ~PitchStreamWorker() { Stop(); }

  PitchStreamWorker(const PitchStreamWorker&) = delete;
  PitchStreamWorker& operator=(const PitchStreamWorker&) = delete;

  void SetCache(PitchStreamCache* cache) { mCache = cache; }

  void Stop()
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mStop.store(true, std::memory_order_release);
      mJobPending.store(true, std::memory_order_release);
    }
    mCv.notify_all();

    if (mThread.joinable())
      mThread.join();

    mStop.store(false, std::memory_order_release);
    mJobPending.store(false, std::memory_order_release);
  }

  void BeginStream(const std::vector<float>& dryLeft,
                   const std::vector<float>& dryRight,
                   int sampleRate,
                   int streamOrigin,
                   int semitones,
                   uint32_t generation)
  {
    EnsureThread();

    {
      std::lock_guard<std::mutex> lock(mMutex);
      mDryLeft = dryLeft;
      mDryRight = dryRight;
      mSampleRate = sampleRate;
      mSemitones = semitones;
      mGeneration = generation;
      mLength = static_cast<int>(dryLeft.size());
      mStreamOrigin = streamOrigin;
      mPlayhead.store(streamOrigin, std::memory_order_release);
      mUrgentBlockStart.store(BlockStart(streamOrigin), std::memory_order_release);
      mStreaming.store(true, std::memory_order_release);
    }

    mJobPending.store(true, std::memory_order_release);
    mCv.notify_one();
  }

  void EndStream()
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mStreaming.store(false, std::memory_order_release);
    mJobPending.store(true, std::memory_order_release);
    mCv.notify_one();
  }

  void SetPlayhead(int sampleIndex)
  {
    mPlayhead.store(sampleIndex, std::memory_order_release);
    mUrgentBlockStart.store(BlockStart(sampleIndex), std::memory_order_release);
    mJobPending.store(true, std::memory_order_release);
    mCv.notify_one();
  }

  bool IsStreaming() const { return mStreaming.load(std::memory_order_acquire); }

  bool IsBusy() const { return mWorkerBusy.load(std::memory_order_acquire); }

private:
  int BlockStart(int sample) const
  {
    if (!mCache)
      return 0;

    return mCache->BlockStart(sample);
  }

  int BlockLength(int blockStart) const
  {
    if (!mCache)
      return 0;

    return mCache->BlockEnd(blockStart) - blockStart;
  }

  void EnsureThread()
  {
    if (mThread.joinable())
      return;

    mThread = std::thread([this] { WorkerLoop(); });
  }

  int SelectNextBlockLocked() const
  {
    if (!mStreaming.load(std::memory_order_acquire) || !mCache || mLength <= 0)
      return -1;

    const int ready = mCache->GetReadyThrough();
    const int playhead = mPlayhead.load(std::memory_order_acquire);
    const int urgent = mUrgentBlockStart.load(std::memory_order_acquire);
    const int blockSamples = mCache->GetBlockSamples();

    if (urgent >= 0 && urgent < mLength && !mCache->IsBlockReady(urgent))
      return urgent;

    const int playBlock = BlockStart(playhead);
    if (playBlock < mLength && !mCache->IsBlockReady(playBlock))
      return playBlock;

    const int nextBlock = BlockStart(ready);
    if (nextBlock < mLength && ready < mCache->BlockEnd(nextBlock))
      return nextBlock;

    const int prefetchBlock = BlockStart(playhead + blockSamples);
    if (prefetchBlock < mLength && !mCache->IsBlockReady(prefetchBlock))
      return prefetchBlock;

    for (int start = 0; start < mLength; start += blockSamples)
    {
      if (!mCache->IsBlockReady(start))
        return start;
    }

    return -1;
  }

  bool HasEnoughReadAhead() const
  {
    if (!mCache || mLength <= 0)
      return true;

    const int ready = mCache->GetReadyThrough();
    const int playhead = mPlayhead.load(std::memory_order_acquire);
    const int blockSamples = mCache->GetBlockSamples();
    const int target = (std::min)(mLength, playhead + kReadAheadBlocks * blockSamples);
    return ready >= target;
  }

  void ProcessBurst()
  {
    while (mStreaming.load(std::memory_order_acquire) && mCache)
    {
      if (HasEnoughReadAhead())
        break;

      int blockStart = -1;
      int semitones = 0;
      int sampleRate = 0;
      uint32_t generation = 0;
      int length = 0;
      std::vector<float> dryLeft;
      std::vector<float> dryRight;

      {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mStreaming.load(std::memory_order_acquire))
          break;

        blockStart = SelectNextBlockLocked();
        if (blockStart < 0)
          break;

        semitones = mSemitones;
        sampleRate = mSampleRate;
        generation = mGeneration;
        length = mLength;
        dryLeft = mDryLeft;
        dryRight = mDryRight;
      }

      const int blockLength = (std::min)(mCache->GetBlockSamples(), length - blockStart);
      if (blockLength <= 0)
        break;

      std::vector<float> inL(static_cast<size_t>(blockLength));
      std::vector<float> inR(static_cast<size_t>(blockLength));
      std::copy(dryLeft.begin() + blockStart, dryLeft.begin() + blockStart + blockLength, inL.begin());
      std::copy(dryRight.begin() + blockStart, dryRight.begin() + blockStart + blockLength, inR.begin());

      mWorkerBusy.store(true, std::memory_order_release);

      const PitchProcessResult processed =
        SamplePitchProcessor::ShiftBySemitones(inL, inR, sampleRate, semitones);

      mWorkerBusy.store(false, std::memory_order_release);

      if (!mStreaming.load(std::memory_order_acquire) || generation != mGeneration)
        break;

      if (processed.ok)
      {
        mCache->CommitBlock(blockStart,
                           blockLength,
                           processed.left.data(),
                           processed.right.data(),
                           generation);
      }
    }
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
      lock.unlock();

      if (!mStreaming.load(std::memory_order_acquire) || !mCache)
        continue;

      ProcessBurst();
    }
  }

  PitchStreamCache* mCache = nullptr;

  mutable std::mutex mMutex;
  std::condition_variable mCv;
  std::thread mThread;

  std::vector<float> mDryLeft;
  std::vector<float> mDryRight;
  int mSampleRate = 0;
  int mSemitones = 0;
  int mLength = 0;
  int mStreamOrigin = 0;
  uint32_t mGeneration = 0;

  std::atomic<bool> mStop {false};
  std::atomic<bool> mJobPending {false};
  std::atomic<bool> mStreaming {false};
  std::atomic<bool> mWorkerBusy {false};
  std::atomic<int> mPlayhead {0};
  std::atomic<int> mUrgentBlockStart {-1};
};

} // namespace audioagent
