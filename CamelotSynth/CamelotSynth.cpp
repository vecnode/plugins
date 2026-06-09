#include "CamelotSynth.h"
#include "IPlug_include_in_plug_src.h"

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
    const IRECT buttons = b.GetFromTop(60.f).GetMidHPadded(160.f);
    const IRECT playRect = buttons.GetGridCell(0, 1, 2).GetCentredInside(120, 44);
    const IRECT stopRect = buttons.GetGridCell(0, 1, 2, true).GetCentredInside(120, 44);

    pGraphics->AttachControl(new IVButtonControl(playRect, [this](IControl* pCaller) {
      SendArbitraryMsgFromUI(kMsgPlaySample);
      SplashClickActionFunc(pCaller);
    }, "Play", DEFAULT_STYLE.WithLabelText({18.f, EVAlign::Middle})));

    pGraphics->AttachControl(new IVButtonControl(stopRect, [this](IControl* pCaller) {
      SendArbitraryMsgFromUI(kMsgStopSample);
      SplashClickActionFunc(pCaller);
    }, "Stop", DEFAULT_STYLE.WithLabelText({18.f, EVAlign::Middle})));

    pGraphics->AttachControl(new IVKnobControl(b.GetCentredInside(90).GetVShifted(30.f), kParamGain, "Gain"));
    pGraphics->AttachControl(new IVLEDMeterControl<2>(b.GetFromRight(80).GetMidVPadded(120)), kCtrlTagMeter);
  };
#endif
}

#if IPLUG_DSP
void CamelotSynth::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();
  for (int c = 0; c < nChans; c++)
    memset(outputs[c], 0, nFrames * sizeof(sample));

  const sample gain = static_cast<sample>(GetParam(kParamGain)->Value() / 100.);
  mSamplePlayer.ProcessBlock(outputs, nChans, nFrames, gain);

  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
}

void CamelotSynth::OnIdle()
{
  mMeterSender.TransmitData(*this);
}

void CamelotSynth::OnReset()
{
  mMeterSender.Reset(GetSampleRate());
  LoadEmbeddedSample();
}

void CamelotSynth::LoadEmbeddedSample()
{
  if (mSampleBuffer.LoadEmbedded(ATMOS_SAMPLE_FN, GetSampleRate()))
    mSamplePlayer.SetBuffer(mSampleBuffer);
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
    mSamplePlayer.RequestTrigger();
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
