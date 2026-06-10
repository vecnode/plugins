#include "CamelotSynth.h"
#include "CamelotSynthEditor.h"
#include "WaveformControl.h"
#include "UiPaintPolicy.h"
#include "IPlug_include_in_plug_src.h"

namespace
{
constexpr int kWaveformPoints = 1024;
constexpr double kGainSmoothMs = 20.;
constexpr int kHiddenTransportFlags = IParam::kFlagMeta | IParam::kFlagCannotAutomate;
}

CamelotSynth::CamelotSynth(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100.0, 0.01, "%");
  GetParam(kParamTrigPlay)->InitBool("Play", false, "", kHiddenTransportFlags);
  GetParam(kParamTrigPause)->InitBool("Pause", false, "", kHiddenTransportFlags);
  GetParam(kParamTrigStop)->InitBool("Stop", false, "", kHiddenTransportFlags);
  GetParam(kParamSeek)->InitDouble("Seek", 0., 0., 1., 0.001, "", kHiddenTransportFlags);

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

  // Full-surface repaint when the host opens the editor (avoids host background bleed).
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
  mSampleTransport.ProcessBlock(outputs, nChans, nFrames, targetGain, mGainSmoother);

  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
}

void CamelotSynth::OnIdle()
{
  // Meter peaks are sent here; each update marks the meter control dirty on the UI thread.
  mMeterSender.TransmitData(*this);

#if IPLUG_EDITOR
  if (auto* pGraphics = GetUI())
  {
    // Graphics may not paint until the first idle tick after LayoutUI; prime once here too.
    if (!mEditorPaintPrimed)
    {
      mEditorPaintPrimed = true;
      ::igraphics::ForceInitialFullPaint(pGraphics);
    }

    if (auto* pWaveform = pGraphics->GetControlWithTag(kCtrlTagWaveform))
    {
      auto* pCtrl = pWaveform->As<::igraphics::SampleWaveformControl>();

      if (mSampleTransport.ConsumeWaveformDirty() && mWaveformEnvelope.IsValid())
        pCtrl->SetEnvelope(mWaveformEnvelope.Max(), mWaveformEnvelope.Min());

      if (!pCtrl->IsScrubbing() && mUiPlayheadBridge.ShouldSyncPlayhead(mSampleTransport))
        pCtrl->SyncPlayheadFromDSP(mSampleTransport.GetPlayheadNorm());

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
  mSampleTransport.SetSampleRate(GetSampleRate());
  LoadEmbeddedSample();
}

void CamelotSynth::LoadEmbeddedSample()
{
  if (mSampleTransport.LoadEmbedded(ATMOS_SAMPLE_FN, GetSampleRate()))
    mWaveformEnvelope.BuildFrom(mSampleTransport.GetBuffer(), kWaveformPoints);
}

void CamelotSynth::ResetTransportTrigger(int paramIdx)
{
  GetParam(paramIdx)->Set(0.);
}

void CamelotSynth::OnParamChange(int paramIdx, EParamSource source, int sampleOffset)
{
  (void) source;
  const int offset = sampleOffset < 0 ? 0 : sampleOffset;

  switch (paramIdx)
  {
    case kParamTrigPlay:
      if (GetParam(kParamTrigPlay)->Bool())
      {
        if (!mSampleTransport.IsSampleLoaded())
          LoadEmbeddedSample();

        mSampleTransport.SchedulePlay(offset);
        mUiPlayheadBridge.MarkPlayheadDirty();
      }
      ResetTransportTrigger(kParamTrigPlay);
      break;

    case kParamTrigPause:
      if (GetParam(kParamTrigPause)->Bool())
      {
        mSampleTransport.SchedulePause(offset);
        mUiPlayheadBridge.MarkPlayheadDirty();
      }
      ResetTransportTrigger(kParamTrigPause);
      break;

    case kParamTrigStop:
      if (GetParam(kParamTrigStop)->Bool())
      {
        mSampleTransport.ScheduleStop(offset);
        mUiPlayheadBridge.MarkPlayheadDirty();
      }
      ResetTransportTrigger(kParamTrigStop);
      break;

    case kParamSeek:
      (void) offset;
      mSampleTransport.SeekTo(static_cast<float>(GetParam(kParamSeek)->Value()));
      mUiPlayheadBridge.MarkPlayheadDirty();
      break;

    default:
      break;
  }
}
#endif
