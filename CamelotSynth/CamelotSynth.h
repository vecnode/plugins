#pragma once

// Plugin entry — DSP and sampler logic live in src/audioagent (see audioagent/ARCHITECTURE.md).
#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include "audioagent/SamplerEngine.h"

const int kNumPresets = 1;

enum EParams
{
  kParamGain = 0,
  /** Chain stage 3 — high-pass (30 Hz), off by default. */
  kParamHPF,
  /** Pitch mode: 0 = Quality (read-ahead), 1 = Live (RT shifter). */
  kParamPitchMode,
  kParamTrigPlay,
  kParamTrigPause,
  kParamTrigStop,
  kParamSeek,
  kParamTrigDetectNote,
  kParamTrigPitchDownOne,
  kParamTrigPitchUpOne,
  kParamTrigPitchReset,
  kNumParams
};

enum EControlTags
{
  kCtrlTagMeter = 0,
  kCtrlTagWaveform = 1,
  kCtrlTagPlayhead = 2,
  kCtrlTagSampleLength = 3,
  kCtrlTagDetectedNote = 4,
  kCtrlTagDetectNote = 5,
  kCtrlTagPitchUpOne = 6,
  kCtrlTagCamelotCircle = 7,
  kCtrlTagPitchDownOne = 8,
  kCtrlTagPitchReset = 9,
  kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class CamelotSynth final : public Plugin
{
public:
  CamelotSynth(const InstanceInfo& info);
  ~CamelotSynth();

#if IPLUG_EDITOR
  void OnUIOpen() override;
  bool mEditorPaintPrimed = false;
#endif

#if IPLUG_DSP
public:
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnParamChange(int paramIdx, EParamSource source, int sampleOffset) override;
  void OnIdle() override;
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;

private:
  void LoadEmbeddedSample();
  void ResetTransportTrigger(int paramIdx);
  void SyncOfflineWorkerState();
  void ApplyOfflineWorkerUiUpdates(IGraphics* pGraphics);
  void SyncPitchControls(IGraphics* pGraphics);

  audioagent::SamplerEngine mEngine;
  IPeakAvgSender<2> mMeterSender;
  LogParamSmooth<sample, 1> mGainSmoother;
#endif
};
