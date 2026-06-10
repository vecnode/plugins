#include "CamelotSynth.h"
#include "CamelotSynthEditor.h"
#include "WaveformControl.h"
#include "UiPaintPolicy.h"
#include "IPlug_include_in_plug_src.h"

namespace
{
constexpr int kWaveformPoints = 1024;
constexpr double kGainSmoothMs = 20.;
}

CamelotSynth::CamelotSynth(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100.0, 0.01, "%");

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    CamelotSynthEditor::Attach(pGraphics, *this);
  };
#endif
}

#if IPLUG_EDITOR
void CamelotSynth::OnUIOpen()
{
  Plugin::OnUIOpen();
  mEditorPaintPrimed = false;

  if (auto* pGraphics = GetUI())
    ::igraphics::ForceInitialFullPaint(pGraphics);
}
#endif

#if IPLUG_DSP
void CamelotSynth::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();
  for (int c = 0; c < nChans; c++)
    memset(outputs[c], 0, nFrames * sizeof(sample));

  const sample targetGain = static_cast<sample>(GetParam(kParamGain)->Value() / 100.);
  mSamplePlayer.ProcessBlock(outputs, nChans, nFrames, targetGain, mGainSmoother);

  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
}

void CamelotSynth::OnIdle()
{
  // Meter peaks are sent here; each update marks the meter control dirty on the UI thread.
  mMeterSender.TransmitData(*this);

#if IPLUG_EDITOR
  // Playhead sync runs on the ~50 Hz idle timer; paint policy in src/ui/bridge/UiPaintPolicy.h.
  if (auto* pGraphics = GetUI())
  {
    if (!mEditorPaintPrimed)
    {
      mEditorPaintPrimed = true;
      ::igraphics::ForceInitialFullPaint(pGraphics);
    }

    if (auto* pWaveform = pGraphics->GetControlWithTag(kCtrlTagWaveform))
    {
      auto* pCtrl = pWaveform->As<::igraphics::SampleWaveformControl>();

      if (mUiPlayheadBridge.ConsumeWaveformDirty() && mWaveformEnvelope.IsValid())
        pCtrl->SetEnvelope(mWaveformEnvelope.Max(), mWaveformEnvelope.Min());

      if (mUiPlayheadBridge.ShouldSyncPlayhead(mSamplePlayer))
        pCtrl->SyncPlayheadFromDSP(mSamplePlayer.GetPlayheadNorm());

      mUiPlayheadBridge.ClearPlayheadForce();
    }
  }
#endif
}

void CamelotSynth::OnReset()
{
  mMeterSender.Reset(GetSampleRate());
  mGainSmoother.SetSmoothTime(kGainSmoothMs, GetSampleRate());
  mGainSmoother.SetValue(static_cast<sample>(GetParam(kParamGain)->Value() / 100.));
  LoadEmbeddedSample();
}

void CamelotSynth::LoadEmbeddedSample()
{
  if (mSampleBuffer.LoadEmbedded(ATMOS_SAMPLE_FN, GetSampleRate()))
  {
    mSamplePlayer.SetBuffer(mSampleBuffer);
    mWaveformEnvelope.BuildFrom(mSampleBuffer, kWaveformPoints);
    mUiPlayheadBridge.MarkWaveformDirty();
  }
}

void CamelotSynth::OnParamChange(int paramIdx)
{
  (void) paramIdx;
}

bool CamelotSynth::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  (void) ctrlTag;

  if (msgTag == kMsgPlaySample)
  {
    if (!mSampleBuffer.IsLoaded())
      LoadEmbeddedSample();

    mSamplePlayer.RequestPlay();
    mUiPlayheadBridge.MarkPlayheadDirty();
    return true;
  }

  if (msgTag == kMsgPauseSample)
  {
    mSamplePlayer.RequestPause();
    mUiPlayheadBridge.MarkPlayheadDirty();
    return true;
  }

  if (msgTag == kMsgStopSample)
  {
    mSamplePlayer.RequestStop();
    mUiPlayheadBridge.MarkPlayheadDirty();
    return true;
  }

  if (msgTag == kMsgSeekSample && pData && dataSize >= static_cast<int>(sizeof(float)))
  {
    const float norm = *static_cast<const float*>(pData);
    mSamplePlayer.SeekToNormalized(norm);
    mUiPlayheadBridge.MarkPlayheadDirty();
    return true;
  }

  (void) dataSize;
  (void) pData;
  return false;
}
#endif
