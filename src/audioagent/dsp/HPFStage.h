#pragma once

#include "ProcessChain.h"
#include <atomic>
#include <cmath>

namespace audioagent
{

/** One-pole high-pass (DC blocker) — bypass when cutoff <= 0. */
class HPFStage final : public IProcessStage
{
public:
  static constexpr double kDefaultCutoffHz = 30.;

  void Reset(double sampleRate)
  {
    mSampleRate = sampleRate > 0. ? sampleRate : 44100.;
    UpdateCoeff();
    mZL = 0.f;
    mZR = 0.f;
    mYP = 0.f;
  }

  void SetEnabled(bool enabled) { mEnabled.store(enabled, std::memory_order_release); }

  bool IsEnabled() const { return mEnabled.load(std::memory_order_acquire); }

  void SetCutoffHz(double hz)
  {
    mCutoffHz = hz;
    UpdateCoeff();
  }

  void ProcessBlock(ProcessContext& ctx) override
  {
    if (!IsEnabled() || ctx.numFrames <= 0)
      return;

    sample* left = ctx.left;
    sample* right = ctx.right;

    if (ctx.numChannels <= 1)
    {
      for (int i = 0; i < ctx.numFrames; ++i)
        left[i] = ProcessSample(left[i], mZL, mYP);
      return;
    }

    float yPL = mYP;
    float yPR = mYP;
    for (int i = 0; i < ctx.numFrames; ++i)
    {
      left[i] = ProcessSample(left[i], mZL, yPL);
      right[i] = ProcessSample(right[i], mZR, yPR);
    }
    mYP = yPL;
  }

private:
  void UpdateCoeff()
  {
    if (mCutoffHz <= 0. || mSampleRate <= 0.)
    {
      mCoeff = 0.f;
      return;
    }

    const double x = std::exp(-2. * 3.141592653589793 * mCutoffHz / mSampleRate);
    mCoeff = static_cast<float>(x);
  }

  sample ProcessSample(sample x, float& z, float& yPrev) const
  {
    const sample y = x - z + mCoeff * yPrev;
    z = x;
    yPrev = static_cast<float>(y);
    return y;
  }

  double mSampleRate = 44100.;
  double mCutoffHz = kDefaultCutoffHz;
  float mCoeff = 0.f;
  float mZL = 0.f;
  float mZR = 0.f;
  float mYP = 0.f;
  std::atomic<bool> mEnabled {false};
};

} // namespace audioagent
