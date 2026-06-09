#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"

const int kNumPresets = 1;

enum EMsgTags
{
  kMsgPlaySample = 0x7101,
  kMsgStopSample = 0x7102
};

enum EParams
{
  kParamGain = 0,
  kNumParams
};

#if IPLUG_DSP
#include "SampleBuffer.h"
#include "SamplePlayer.h"
#endif

enum EControlTags
{
  kCtrlTagMeter = 0,
  kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class CamelotSynth final : public Plugin
{
public:
  CamelotSynth(const InstanceInfo& info);

#if IPLUG_DSP
public:
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  void OnIdle() override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

private:
  void LoadEmbeddedSample();

  SampleBuffer mSampleBuffer;
  SamplePlayer mSamplePlayer;
  IPeakAvgSender<2> mMeterSender;
#endif
};
