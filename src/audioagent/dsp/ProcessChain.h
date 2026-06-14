#pragma once

#include "audioagent/iplug_bridge.h"

namespace audioagent
{

/** Block I/O context for RT processing stages (planar stereo). */
struct ProcessContext
{
  sample* left = nullptr;
  sample* right = nullptr;
  int numChannels = 0;
  int numFrames = 0;
  double sampleRate = 44100.;
};

/** RT processing stage — allocate in prepare; never heap-allocate in ProcessBlock. */
class IProcessStage
{
public:
  virtual ~IProcessStage() = default;
  virtual void Reset(double sampleRate) = 0;
  virtual void ProcessBlock(ProcessContext& ctx) = 0;
};

/** Fixed-capacity ordered stage list (no heap growth after Prepare). */
class ProcessChain
{
public:
  static constexpr int kMaxStages = 8;

  void Reset(double sampleRate)
  {
    for (int i = 0; i < mNumStages; ++i)
      mStages[i]->Reset(sampleRate);
  }

  void Clear()
  {
    mNumStages = 0;
  }

  bool AddStage(IProcessStage* stage)
  {
    if (!stage || mNumStages >= kMaxStages)
      return false;

    mStages[mNumStages++] = stage;
    return true;
  }

  void ProcessBlock(ProcessContext& ctx)
  {
    for (int i = 0; i < mNumStages; ++i)
      mStages[i]->ProcessBlock(ctx);
  }

  int GetNumStages() const { return mNumStages; }

private:
  IProcessStage* mStages[kMaxStages] = {};
  int mNumStages = 0;
};

} // namespace audioagent
