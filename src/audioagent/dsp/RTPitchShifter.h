#pragma once

#include "audioagent/iplug_bridge.h"
#include <algorithm>
#include <cmath>

namespace audioagent
{

/**
 * Real-time pitch shifter for Live mode (±1 semitone), audio-thread safe.
 *
 * Time-domain two-tap crossfading delay line: the input is written into a ring
 * at rate 1 while a read offset drifts at rate (1 - ratio). Two read taps half a
 * window apart are blended with complementary triangular weights, so the moment a
 * tap laps the window boundary it carries zero weight — no periodic click. Always
 * outputs audio: dry passthrough during warmup, smoothed ratio changes.
 *
 * Memory is O(1) in the file length — a fixed ring, independent of sample size.
 * Latency ≈ kWindow/2 samples (~21 ms at 48 kHz).
 */
class RTPitchShifter
{
public:
  static constexpr int kRingSize = 4096;
  static constexpr int kWindow = kRingSize / 2; // crossfade period / tap span
  static constexpr float kRatioSmooth = 0.004f;

  void Reset(double sampleRate)
  {
    (void) sampleRate;
    mSemitones = 0;
    mRatio = 1.f;
    mTargetRatio = 1.f;
    mWritePos = 0;
    mPhase = 0.f;
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
    const int writeHead = mWritePos;
    mRingL[static_cast<size_t>(writeHead)] = static_cast<float>(inL);
    mRingR[static_cast<size_t>(writeHead)] = static_cast<float>(inR);
    mWritePos = (mWritePos + 1) % kRingSize;

    if (mSemitones == 0)
    {
      mRatio = 1.f;
      mTargetRatio = 1.f;
      mPhase = 0.f;
      outL = inL;
      outR = inR;
      return;
    }

    mRatio += (mTargetRatio - mRatio) * kRatioSmooth;

    if (mPrimed < kWindow)
    {
      ++mPrimed;
      mPhase = 0.f;
      outL = inL;
      outR = inR;
      return;
    }

    // Read offset (distance behind the write head) drifts at (1 - ratio):
    // ratio > 1 (pitch up) shrinks the offset, reading newer samples faster.
    mPhase += (1.f - mRatio);
    while (mPhase >= static_cast<float>(kWindow))
      mPhase -= static_cast<float>(kWindow);
    while (mPhase < 0.f)
      mPhase += static_cast<float>(kWindow);

    const float offA = mPhase;
    float offB = mPhase + static_cast<float>(kWindow) * 0.5f;
    if (offB >= static_cast<float>(kWindow))
      offB -= static_cast<float>(kWindow);

    // Triangular crossfade weights (peak at window centre); they sum to 1.
    const float wA = 1.f - std::fabs(2.f * (offA / static_cast<float>(kWindow)) - 1.f);
    const float wB = 1.f - wA;

    const float headPos = static_cast<float>(writeHead);
    const float aL = ReadRing(mRingL, headPos - offA);
    const float aR = ReadRing(mRingR, headPos - offA);
    const float bL = ReadRing(mRingL, headPos - offB);
    const float bR = ReadRing(mRingR, headPos - offB);

    outL = static_cast<sample>(aL * wA + bL * wB);
    outR = static_cast<sample>(aR * wA + bR * wB);
  }

private:
  static float ReadRing(const float* ring, float position)
  {
    while (position < 0.f)
      position += static_cast<float>(kRingSize);
    while (position >= static_cast<float>(kRingSize))
      position -= static_cast<float>(kRingSize);

    const int i0 = static_cast<int>(position) % kRingSize;
    const int i1 = (i0 + 1) % kRingSize;
    const float frac = position - std::floor(position);
    return ring[static_cast<size_t>(i0)] * (1.f - frac) + ring[static_cast<size_t>(i1)] * frac;
  }

  float mRingL[kRingSize] {};
  float mRingR[kRingSize] {};
  int mWritePos = 0;
  float mPhase = 0.f;
  float mRatio = 1.f;
  float mTargetRatio = 1.f;
  int mSemitones = 0;
  int mPrimed = 0;
};

} // namespace audioagent
