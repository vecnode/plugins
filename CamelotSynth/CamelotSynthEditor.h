#pragma once

#include "CamelotSynth.h"
#include "src/GainKnobControl.h"
#include "src/WaveformControl.h"
#include "src/UiRedraw.h"

namespace CamelotSynthEditor
{
constexpr float kPadding = 16.f;
constexpr float kMeterWidth = 44.f;
constexpr float kTabHeight = 40.f;
constexpr float kWaveSectionHeight = 140.f;

inline IColor PanelColor()
{
  return IColor(34, 36, 42);
}

inline IVStyle ControlStyle()
{
  return DEFAULT_STYLE
    .WithColor(kBG, IColor(48, 52, 60))
    .WithColor(kFG, IColor(225, 228, 235))
    .WithColor(kFR, IColor(72, 78, 90))
    .WithColor(kPR, IColor(88, 156, 220))
    .WithColor(kHL, IColor(110, 175, 235))
    .WithDrawFrame(true)
    .WithDrawShadows(false)
    .WithRoundness(0.f)
    .WithLabelText(IText(14.f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle));
}

inline IVStyle ButtonStyle()
{
  return ControlStyle().WithRoundness(0.f);
}

inline IVStyle TabStyle()
{
  return ControlStyle()
    .WithColor(kBG, IColor(42, 45, 52))
    .WithColor(kPR, IColor(58, 62, 72))
    .WithColor(kHL, IColor(72, 148, 220))
    .WithLabelText(IText(15.f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle));
}

struct Regions
{
  IRECT meter;
  IRECT tabBar;
  IRECT middle;
  IRECT waveSection;
  IRECT waveform;
};

inline Regions ComputeRegions(const IRECT& bounds)
{
  Regions r;
  const IRECT padded = bounds.GetPadded(-kPadding);

  r.meter = padded.GetFromRight(kMeterWidth);
  const IRECT content = padded.GetReducedFromRight(kMeterWidth + 10.f);

  r.tabBar = content.GetFromTop(kTabHeight);
  r.waveSection = content.GetFromBottom(kWaveSectionHeight);
  r.middle = content.GetReducedFromTop(kTabHeight + 8.f).GetReducedFromBottom(kWaveSectionHeight + 8.f);
  r.waveform = r.waveSection.GetReducedFromTop(22.f).GetPadded(-4.f);

  return r;
}

inline ::igraphics::SampleWaveformControl::SeekHandler MakeSeekHandler(CamelotSynth& plugin)
{
  return [&plugin](float norm) {
    plugin.SendArbitraryMsgFromUI(kMsgSeekSample, kNoTag, sizeof(float), &norm);
  };
}

inline void Attach(IGraphics* pGraphics, CamelotSynth& plugin)
{
  const IVStyle controlStyle = ControlStyle();
  const IVStyle buttonStyle = ButtonStyle();
  const Regions layout = ComputeRegions(pGraphics->GetBounds());

  ::igraphics::AttachAndRegisterFullWindowBackground(pGraphics, IColor(26, 28, 32));

  pGraphics->AttachControl(new IVLEDMeterControl<2>(layout.meter, "", controlStyle), kCtrlTagMeter);

  pGraphics->AttachControl(new IPanelControl(layout.tabBar, PanelColor(), true));
  const IRECT tabSwitchBounds = layout.tabBar.GetFromLeft(110.f).GetPadded(-6.f);
  pGraphics->AttachControl(new IVTabSwitchControl(tabSwitchBounds,
    [](IControl*) {},
    {"hello"},
    "",
    TabStyle(),
    EVShape::EndsRounded));

  // Opaque middle — transparent panels leave holes in the partial FBO composite.
  pGraphics->AttachControl(new IPanelControl(layout.middle, PanelColor(), false));

  const IRECT transportRow = layout.middle.GetFromTop(56.f).GetMidHPadded(300.f);
  const IRECT playRect = transportRow.GetGridCell(0, 0, 1, 3).GetCentredInside(92, 42);
  const IRECT pauseRect = transportRow.GetGridCell(0, 1, 1, 3).GetCentredInside(92, 42);
  const IRECT stopRect = transportRow.GetGridCell(0, 2, 1, 3).GetCentredInside(92, 42);
  const IRECT gainRect = layout.middle.GetFromBottom(120.f).GetMidHPadded(90.f).GetCentredInside(96, 96);

  pGraphics->AttachControl(new IVButtonControl(playRect, [&plugin](IControl* pCaller) {
    plugin.SendArbitraryMsgFromUI(kMsgPlaySample);
    SplashClickActionFunc(pCaller);
  }, "Play", buttonStyle.WithLabelText({15.f, EVAlign::Middle}), true, true, EVShape::Rectangle));

  pGraphics->AttachControl(new IVButtonControl(pauseRect, [&plugin](IControl* pCaller) {
    plugin.SendArbitraryMsgFromUI(kMsgPauseSample);
    SplashClickActionFunc(pCaller);
  }, "Pause", buttonStyle.WithLabelText({15.f, EVAlign::Middle}), true, true, EVShape::Rectangle));

  pGraphics->AttachControl(new IVButtonControl(stopRect, [&plugin](IControl* pCaller) {
    plugin.SendArbitraryMsgFromUI(kMsgStopSample);
    SplashClickActionFunc(pCaller);
  }, "Stop", buttonStyle.WithLabelText({15.f, EVAlign::Middle}), true, true, EVShape::Rectangle));

  pGraphics->AttachControl(new ::igraphics::GainKnobControl(gainRect, kParamGain, "Gain", controlStyle));

  pGraphics->AttachControl(new IPanelControl(layout.waveSection, PanelColor(), true));
  const IRECT waveLabel = layout.waveSection.GetFromTop(20.f).GetPadded(-8.f);
  pGraphics->AttachControl(new ITextControl(waveLabel, "Sample", IText(13.f, IColor(160, 168, 180), "Roboto-Regular", EAlign::Near)));
  pGraphics->AttachControl(
    new ::igraphics::SampleWaveformControl(layout.waveform, MakeSeekHandler(plugin), kCtrlTagWaveform),
    kCtrlTagWaveform);

  ::igraphics::InstallSinglePassRedraw(pGraphics);
}

} // namespace CamelotSynthEditor
