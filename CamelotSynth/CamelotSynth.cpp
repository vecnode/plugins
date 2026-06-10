#include "CamelotSynth.h"
#include "WaveformControl.h"
#include "IPlug_include_in_plug_src.h"

namespace
{
constexpr int kWaveformPoints = 512;
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
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    const IRECT b = pGraphics->GetBounds().GetPadded(-20.f);
    const IRECT buttons = b.GetFromTop(50.f).GetMidHPadded(280.f);
    const IRECT playRect = buttons.GetGridCell(0, 0, 1, 3).GetCentredInside(88, 40);
    const IRECT pauseRect = buttons.GetGridCell(0, 1, 1, 3).GetCentredInside(88, 40);
    const IRECT stopRect = buttons.GetGridCell(0, 2, 1, 3).GetCentredInside(88, 40);
    const IRECT waveRect = b.GetFromBottom(90.f).GetPadded(-2.f);
    const IRECT middle = b.GetReducedFromTop(50.f).GetReducedFromBottom(95.f);

    pGraphics->AttachControl(new IVButtonControl(playRect, [this](IControl* pCaller) {
      SendArbitraryMsgFromUI(kMsgPlaySample);
      SplashClickActionFunc(pCaller);
    }, "Play", DEFAULT_STYLE.WithLabelText({16.f, EVAlign::Middle})));

    pGraphics->AttachControl(new IVButtonControl(pauseRect, [this](IControl* pCaller) {
      SendArbitraryMsgFromUI(kMsgPauseSample);
      SplashClickActionFunc(pCaller);
    }, "Pause", DEFAULT_STYLE.WithLabelText({16.f, EVAlign::Middle})));

    pGraphics->AttachControl(new IVButtonControl(stopRect, [this](IControl* pCaller) {
      SendArbitraryMsgFromUI(kMsgStopSample);
      SplashClickActionFunc(pCaller);
    }, "Stop", DEFAULT_STYLE.WithLabelText({16.f, EVAlign::Middle})));

    pGraphics->AttachControl(new IVKnobControl(middle.GetCentredInside(90).GetVShifted(10.f), kParamGain, "Gain"));
    pGraphics->AttachControl(new IVLEDMeterControl<2>(middle.GetFromRight(70).GetMidVPadded(100)), kCtrlTagMeter);
    pGraphics->AttachControl(new ::igraphics::SampleWaveformControl(waveRect, kCtrlTagWaveform), kCtrlTagWaveform);
  };
#endif
}

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
  mMeterSender.TransmitData(*this);

#if IPLUG_EDITOR
  if (auto* pGraphics = GetUI())
  {
    if (auto* pWaveform = pGraphics->GetControlWithTag(kCtrlTagWaveform))
    {
      if (mWaveformPeaksDirty)
      {
        pWaveform->As<::igraphics::SampleWaveformControl>()->SetPeaks(mWaveformPeaks);
        mWaveformPeaksDirty = false;
      }

      pWaveform->As<::igraphics::SampleWaveformControl>()->SetPlayhead(mSamplePlayer.GetPlayheadNorm());
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
    mSampleBuffer.BuildWaveformPeaks(mWaveformPeaks, kWaveformPoints);
    mWaveformPeaksDirty = true;
  }
}

void CamelotSynth::OnParamChange(int paramIdx)
{
  (void) paramIdx;
}

bool CamelotSynth::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  (void) ctrlTag;
  (void) dataSize;
  (void) pData;

  if (msgTag == kMsgPlaySample)
  {
    if (!mSampleBuffer.IsLoaded())
      LoadEmbeddedSample();

    mSamplePlayer.RequestPlay();
    return true;
  }

  if (msgTag == kMsgPauseSample)
  {
    mSamplePlayer.RequestPause();
    return true;
  }

  if (msgTag == kMsgStopSample)
  {
    mSamplePlayer.RequestStop();
    return true;
  }

  return false;
}
#endif
