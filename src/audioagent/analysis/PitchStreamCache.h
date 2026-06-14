#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace audioagent
{

/**
 * Pitched sample cache filled in multi-second blocks by PitchStreamWorker.
 * audioFlux runs offline per block; playback reads continuously from ready regions.
 */
class PitchStreamCache
{
public:
  static constexpr double kBlockSeconds = 10.0;

  void Reset()
  {
    mLeft.clear();
    mRight.clear();
    mLength = 0;
    mBlockSamples = 0;
    mStreamOrigin = 0;
    mReadyThrough.store(0, std::memory_order_release);
    mGeneration.store(0, std::memory_order_release);
    mActive.store(false, std::memory_order_release);
  }

  void Prepare(int length, int sampleRate)
  {
    if (length <= 0 || sampleRate <= 0)
    {
      Reset();
      return;
    }

    mLength = length;
    mBlockSamples = (std::max)(4096, static_cast<int>(std::lround(sampleRate * kBlockSeconds)));
    mLeft.assign(static_cast<size_t>(length), 0.f);
    mRight.assign(static_cast<size_t>(length), 0.f);
    mStreamOrigin = 0;
    mReadyThrough.store(0, std::memory_order_release);
    mActive.store(false, std::memory_order_release);
  }

  void BeginStream(int streamOrigin, uint32_t generation)
  {
    mStreamOrigin = (std::max)(0, streamOrigin);
    mReadyThrough.store(mStreamOrigin, std::memory_order_release);
    mGeneration.store(generation, std::memory_order_release);
    mActive.store(true, std::memory_order_release);
  }

  void EndStream()
  {
    mActive.store(false, std::memory_order_release);
  }

  bool IsActive() const { return mActive.load(std::memory_order_acquire); }

  int GetLength() const { return mLength; }

  int GetBlockSamples() const { return mBlockSamples; }

  int GetStreamOrigin() const { return mStreamOrigin; }

  int GetReadyThrough() const { return mReadyThrough.load(std::memory_order_acquire); }

  uint32_t GetGeneration() const { return mGeneration.load(std::memory_order_acquire); }

  int BlockStart(int sample) const
  {
    if (mBlockSamples <= 0)
      return 0;

    return (sample / mBlockSamples) * mBlockSamples;
  }

  int BlockEnd(int blockStart) const
  {
    return (std::min)(mLength, blockStart + mBlockSamples);
  }

  bool IsBlockReady(int blockStart) const
  {
    if (!IsActive() || blockStart < 0 || blockStart >= mLength)
      return false;

    return GetReadyThrough() >= BlockEnd(blockStart);
  }

  bool IsSampleReady(int index) const
  {
    if (!IsActive() || index < mStreamOrigin || index >= mLength)
      return false;

    return index < GetReadyThrough();
  }

  void CommitBlock(int blockStart,
                   int blockLength,
                   const float* left,
                   const float* right,
                   uint32_t generation)
  {
    if (!left || !right || blockLength <= 0 || blockStart < 0)
      return;

    if (generation != mGeneration.load(std::memory_order_acquire))
      return;

    const int end = (std::min)(mLength, blockStart + blockLength);
    const int count = end - blockStart;
    if (count <= 0)
      return;

    for (int i = 0; i < count; ++i)
    {
      const int idx = blockStart + i;
      mLeft[static_cast<size_t>(idx)] = left[i];
      mRight[static_cast<size_t>(idx)] = right[i];
    }

    const int readyEnd = (std::max)(GetReadyThrough(), end);
    mReadyThrough.store(readyEnd, std::memory_order_release);
  }

  void ReadPitched(double position, float& outL, float& outR) const
  {
    if (mLength <= 0)
    {
      outL = 0.f;
      outR = 0.f;
      return;
    }

    position = (std::max)(0., (std::min)(position, static_cast<double>(mLength - 1)));
    const int i0 = static_cast<int>(position);
    const int i1 = (std::min)(i0 + 1, mLength - 1);
    const float frac = static_cast<float>(position - static_cast<double>(i0));

    const float l0 = mLeft[static_cast<size_t>(i0)];
    const float r0 = mRight[static_cast<size_t>(i0)];
    const float l1 = mLeft[static_cast<size_t>(i1)];
    const float r1 = mRight[static_cast<size_t>(i1)];

    outL = l0 * (1.f - frac) + l1 * frac;
    outR = r0 * (1.f - frac) + r1 * frac;
  }

private:
  std::vector<float> mLeft;
  std::vector<float> mRight;
  int mLength = 0;
  int mBlockSamples = 0;
  int mStreamOrigin = 0;
  std::atomic<int> mReadyThrough {0};
  std::atomic<uint32_t> mGeneration {0};
  std::atomic<bool> mActive {false};
};

} // namespace audioagent
