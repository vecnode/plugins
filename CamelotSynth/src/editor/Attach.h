#pragma once

#include "CamelotSynth.h"
#include "Layout.h"
#include "Styles.h"
#include "FramedPanelControl.h"
#include "GainKnobControl.h"
#include "WaveformControl.h"
#include "PlayheadOverlayControl.h"
#include "CamelotCircleControl.h"
#include "SamplerSectionControl.h"
#include "SamplerFooterControl.h"
#include "UiPaintPolicy.h"

namespace CamelotSynthEditor
{

inline ::igraphics::WaveformTrackControl::SeekHandler MakeSeekHandler(CamelotSynth& plugin)
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

  ::igraphics::AttachAndRegisterFullWindowBackground(pGraphics, PanelColor());

  pGraphics->AttachControl(new IVLEDMeterControl<2>(layout.meter, "", controlStyle), kCtrlTagMeter);

  pGraphics->AttachControl(new FramedPanelControl(layout.tabBar));
  const IRECT tabSwitchBounds = layout.tabBar.GetFromLeft(110.f).GetPadded(-6.f);
  pGraphics->AttachControl(new LogoBadgeControl(tabSwitchBounds, "LOGO - NAME (2026)"));

  pGraphics->AttachControl(new FramedPanelControl(layout.transport));

  const IRECT transportRow = layout.transport.GetPadded(-8.f).GetMidHPadded(320.f);
  const IRECT playRect = transportRow.GetGridCell(0, 0, 1, 3).GetCentredInside(100, 44);
  const IRECT pauseRect = transportRow.GetGridCell(0, 1, 1, 3).GetCentredInside(100, 44);
  const IRECT stopRect = transportRow.GetGridCell(0, 2, 1, 3).GetCentredInside(100, 44);

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

  pGraphics->AttachControl(new FramedPanelControl(layout.middle));
  pGraphics->AttachControl(new ::igraphics::CamelotCircle(
    layout.middle,
    layout.noteCircle,
    NoteCircleFillColor(),
    CamelotBlockActiveColor(),
    CamelotBlockSelectedColor(),
    NoteCircleLineColor(),
    2.5f));
  pGraphics->AttachControl(new ::igraphics::GainKnobControl(layout.gain, kParamGain, "Gain", controlStyle));

  pGraphics->AttachControl(new ::igraphics::SamplerSectionControl(
    layout.waveSection, PanelColor(), PanelAccentColor(), 2.5f));

  pGraphics->AttachControl(new ITextControl(
    layout.waveTitle,
    "Sampler",
    IText(13.f, IColor(176, 182, 192), "Roboto-Regular", EAlign::Near)));

  pGraphics->AttachControl(new ::igraphics::SamplerFooterControl(
    layout.samplerFooter,
    PanelColor(),
    SamplerAccentColor(),
    IColor(176, 182, 192),
    "Lorem Ipsum"));

  const IRECT footerButtons = ::igraphics::SamplerFooterControl::ButtonColumn(layout.samplerFooter);
  const IRECT btn0 = footerButtons.GetGridCell(0, 0, 1, 3).GetCentredInside(56.f, 28.f);
  const IRECT btn1 = footerButtons.GetGridCell(0, 1, 1, 3).GetCentredInside(56.f, 28.f);
  const IRECT btn2 = footerButtons.GetGridCell(0, 2, 1, 3).GetCentredInside(56.f, 28.f);

  pGraphics->AttachControl(new IVButtonControl(btn0, SplashClickActionFunc, "One", buttonStyle, true));
  pGraphics->AttachControl(new IVButtonControl(btn1, SplashClickActionFunc, "Two", buttonStyle, true));
  pGraphics->AttachControl(new IVButtonControl(btn2, SplashClickActionFunc, "Three", buttonStyle, true));

  const IRECT plotBounds = layout.waveform.GetPadded(-6.f);
  auto* pPlayhead = new ::igraphics::PlayheadOverlayControl(plotBounds, 5.f, kCtrlTagPlayhead);
  auto* pTrack = new ::igraphics::WaveformTrackControl(
    layout.waveform,
    MakeSeekHandler(plugin),
    kCtrlTagWaveform,
    IColor(42, 44, 50),
    SamplerAccentColor(),
    IColor(72, 148, 220, 140),
    SamplerAccentColor());
  pTrack->SetPlayheadOverlay(pPlayhead);

  pGraphics->AttachControl(pTrack, kCtrlTagWaveform);
  pGraphics->AttachControl(pPlayhead, kCtrlTagPlayhead);

  // Windows NanoVG paint contract — see src/ui/bridge/UiPaintPolicy.h and ARCHITECTURE.md
  ::igraphics::InstallPaintPolicy(pGraphics);
  ::igraphics::ForceInitialFullPaint(pGraphics);
}

} // namespace CamelotSynthEditor
