#pragma once

#include "OutputLimiter.h"
#include "ProcessChain.h"

namespace audioagent
{

/** Stereo limiter stage wrapping OutputLimiter (per-sample within block). */
class LimiterStage final : public IProcessStage
{
public:
  void Reset(double sampleRate) { mLimiter.SetSampleRate(sampleRate); }

  void ProcessBlock(ProcessContext& ctx) override
  {
    sample* left = ctx.left;
    sample* right = ctx.right;

    if (ctx.numChannels <= 1)
    {
      for (int i = 0; i < ctx.numFrames; ++i)
      {
        sample mono = left[i];
        mLimiter.ProcessStereo(mono, mono);
        left[i] = mono;
      }
      return;
    }

    for (int i = 0; i < ctx.numFrames; ++i)
      mLimiter.ProcessStereo(left[i], right[i]);
  }

  void ResetState() { mLimiter.Reset(); }

private:
  OutputLimiter mLimiter;
};

} // namespace audioagent
