#pragma once

#include "ProcessChain.h"
#include "audioagent/iplug_bridge.h"

namespace audioagent
{

/** Applies smoothed gain to planar stereo buffers. */
class GainStage final : public IProcessStage
{
public:
  void Reset(double sampleRate) { (void) sampleRate; }

  void SetSmoother(LogParamSmooth<sample, 1>* smoother) { mSmoother = smoother; }

  void SetTargetGain(sample target) { mTargetGain = target; }

  void ProcessBlock(ProcessContext& ctx) override
  {
    if (!mSmoother || ctx.numFrames <= 0)
      return;

    sample* left = ctx.left;
    sample* right = ctx.right;

    if (ctx.numChannels <= 1)
    {
      for (int i = 0; i < ctx.numFrames; ++i)
      {
        const sample g = mSmoother->Process(mTargetGain);
        left[i] *= g;
      }
      return;
    }

    for (int i = 0; i < ctx.numFrames; ++i)
    {
      const sample g = mSmoother->Process(mTargetGain);
      left[i] *= g;
      right[i] *= g;
    }
  }

private:
  LogParamSmooth<sample, 1>* mSmoother = nullptr;
  sample mTargetGain = 1.f;
};

} // namespace audioagent
