#pragma once

// Plugin entry — implementation modules under src/ (see src/ARCHITECTURE.md).
#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"

const int kNumPresets = 1;

enum EParams
{
  kParamGain = 0,
  kParamTrigPlay,
  kParamTrigPause,
  kParamTrigStop,
  kParamSeek,
  kParamTrigDetectNote,
  kParamTrigPitchUpOne,
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
  kNumCtrlTags
};

#if IPLUG_DSP
#include "SampleTransport.h"
#include "Smoothers.h"
#include "WaveformEnvelope.h"
#include "UiPlayheadBridge.h"
#include "OfflineSampleWorker.h"

#include <atomic>
#endif

using namespace iplug;
using namespace igraphics;

class CamelotSynth final : public Plugin
{
public:
  CamelotSynth(const InstanceInfo& info);
  ~CamelotSynth();

#if IPLUG_EDITOR
  void OnUIOpen() override;
  /** First OnIdle after UI attach; triggers ForceInitialFullPaint once (see UiPaintPolicy.h). */
  bool mEditorPaintPrimed = false;
#endif

#if IPLUG_DSP
public:
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnParamChange(int paramIdx, EParamSource source, int sampleOffset) override;
  void OnIdle() override;

private:
  void LoadEmbeddedSample();
  void RebuildProcessSnapshot();
  void ResetTransportTrigger(int paramIdx);
  void SyncOfflineWorkerState();
  void ProcessPendingOfflineJobs();
  void ApplyOfflineWorkerUiUpdates(IGraphics* pGraphics);

  SampleTransport mSampleTransport;
  WaveformEnvelope mWaveformEnvelope;
  UiPlayheadBridge mUiPlayheadBridge;
  OfflineSampleWorker mOfflineWorker;
  DetectedNote mReferenceNote;
  float mPitchRequestPlayheadNorm = 0.f;
  std::atomic<bool> mPendingDetectRequest {false};
  std::atomic<bool> mPendingPitchRequest {false};
  IPeakAvgSender<2> mMeterSender;
  LogParamSmooth<sample, 1> mGainSmoother;
#endif
};
