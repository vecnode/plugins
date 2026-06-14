#pragma once

#include "audioagent/iplug_bridge.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace audioagent
{

/**
 * Streaming pitch shifter for Live mode (±1 semitone).
 * Always outputs audio — dry passthrough during warmup; smoothed ratio changes.
 */
class RTPitchShifter
{
public:
  static constexpr int kRingSize = 4096;
  static constexpr float kRatioSmooth = 0.004f;

  void Reset(double sampleRate)
  {
    (void) sampleRate;
    mSemitones = 0;
    mRatio = 1.f;
    mTargetRatio = 1.f;
    mWritePos = 0;
    mReadPhase = 0.f;
    mPrimed = 0;
    std::fill(std::begin(mRingL), std::end(mRingL), 0.f);
    std::fill(std::begin(mRingR), std::end(mRingR), 0.f);
  }

  void SetSemitones(int semitones)
  {
    semitones = (std::max)(-1, (std::min)(1, semitones));
    mSemitones = semitones;
    mTargetRatio = (semitones == 0) ? 1.f : std::pow(2.f, static_cast<float>(semitones) / 12.f);
  }

  int GetSemitones() const { return mSemitones; }

  void ProcessSample(sample inL, sample inR, sample& outL, sample& outR)
  {
    mRingL[static_cast<size_t>(mWritePos)] = inL;
    mRingR[static_cast<size_t>(mWritePos)] = inR;
    mWritePos = (mWritePos + 1) % kRingSize;

    if (mSemitones == 0)
    {
      mRatio = 1.f;
      mTargetRatio = 1.f;
      mReadPhase = static_cast<float>(mWritePos);
      outL = inL;
      outR = inR;
      return;
    }

    mRatio += (mTargetRatio - mRatio) * kRatioSmooth;

    if (mPrimed < kRingSize / 2)
    {
      ++mPrimed;
      outL = inL;
      outR = inR;
      mReadPhase = static_cast<float>(mWritePos);
      return;
    }

    mReadPhase += mRatio;
    while (mReadPhase >= static_cast<float>(kRingSize))
      mReadPhase -= static_cast<float>(kRingSize);
    while (mReadPhase < 0.f)
      mReadPhase += static_cast<float>(kRingSize);

    const sample pitchedL = ReadRing(mRingL, mReadPhase);
    const sample pitchedR = ReadRing(mRingR, mReadPhase);

    // Light blend with dry keeps transients stable and avoids boundary clicks.
    constexpr sample kDryBlend = 0.12f;
    outL = pitchedL * (1.f - kDryBlend) + inL * kDryBlend;
    outR = pitchedR * (1.f - kDryBlend) + inR * kDryBlend;
  }

private:
  static sample ReadRing(const float* ring, float position)
  {
    const int i0 = static_cast<int>(position) % kRingSize;
    const int i1 = (i0 + 1) % kRingSize;
    const float frac = position - std::floor(position);
    return ring[static_cast<size_t>(i0)] * (1.f - frac) + ring[static_cast<size_t>(i1)] * frac;
  }

  float mRingL[kRingSize] {};
  float mRingR[kRingSize] {};
  int mWritePos = 0;
  float mReadPhase = 0.f;
  float mRatio = 1.f;
  float mTargetRatio = 1.f;
  int mSemitones = 0;
  int mPrimed = 0;
};

} // namespace audioagent
