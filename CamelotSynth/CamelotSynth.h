#pragma once

// Plugin entry + DSP/editor glue. Implementation modules live under src/.
#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include <vector>

const int kNumPresets = 1;

enum EMsgTags
{
  kMsgPlaySample = 0x7101,
  kMsgStopSample = 0x7102,
  kMsgPauseSample = 0x7103,
  kMsgSeekSample = 0x7104
};

enum EParams
{
  kParamGain = 0,
  kNumParams
};

#if IPLUG_DSP
#include "src/SampleBuffer.h"
#include "src/SamplePlayer.h"
#include "Smoothers.h"
#include "src/WaveformEnvelope.h"
#include "src/UiPlayheadBridge.h"
#endif

enum EControlTags
{
  kCtrlTagMeter = 0,
  kCtrlTagWaveform = 1,
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
  WaveformEnvelope mWaveformEnvelope;
  UiPlayheadBridge mUiPlayheadBridge;
  IPeakAvgSender<2> mMeterSender;
  LogParamSmooth<sample, 1> mGainSmoother;
#endif
};
