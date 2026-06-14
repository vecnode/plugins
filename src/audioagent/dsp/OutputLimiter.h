#pragma once

#include "audioagent/iplug_bridge.h"
#include <algorithm>
#include <cmath>

namespace audioagent
{

/**
 * Stereo-linked peak limiter for sample playback.
 * Fast attack catches single-sample seek/transport spikes; soft knee avoids hard-clip artifacts.
 */
class OutputLimiter
{
public:
  static constexpr float kCeiling = 0.95f;
  static constexpr float kKnee = 0.08f;
  static constexpr double kReleaseMs = 60.;

  void SetSampleRate(double sampleRate)
  {
    mSampleRate = sampleRate > 0. ? sampleRate : 44100.;
    mReleaseCoeff = static_cast<float>(std::exp(-1. / (mSampleRate * kReleaseMs * 0.001)));
  }

  void Reset() { mGain = 1.f; }

  void ProcessStereo(sample& left, sample& right)
  {
    left = SoftKnee(left);
    right = SoftKnee(right);

    const sample peak = (std::max)(std::fabs(left), std::fabs(right));
    float targetGain = 1.f;

    if (peak > 1.e-12f)
      targetGain = (std::min)(1.f, kCeiling / static_cast<float>(peak));

    if (targetGain < mGain)
      mGain = targetGain;
    else
      mGain += (targetGain - mGain) * (1.f - mReleaseCoeff);

    left *= static_cast<sample>(mGain);
    right *= static_cast<sample>(mGain);
  }

private:
  static constexpr float kHalfPi = 1.5707963267948966f;

  static sample SoftKnee(sample x)
  {
    const sample ax = std::fabs(x);
    const sample kneeStart = static_cast<sample>(kCeiling - kKnee);

    if (ax <= kneeStart)
      return x;

    if (ax >= static_cast<sample>(kCeiling))
      return std::copysign(static_cast<sample>(kCeiling), x);

    const float t = static_cast<float>((ax - kneeStart) / kKnee);
    const float shaped = static_cast<float>(kneeStart) + kKnee * (1.f - std::cos(t * kHalfPi));
    return std::copysign(static_cast<sample>(shaped), x);
  }

  double mSampleRate = 44100.;
  float mReleaseCoeff = 0.999f;
  float mGain = 1.f;
};

} // namespace audioagent
