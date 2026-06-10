#pragma once

#include "CamelotSynth.h"
#include "Layout.h"
#include "Styles.h"
#include "GainKnobControl.h"
#include "WaveformControl.h"
#include "PlayheadOverlayControl.h"
#include "CamelotNoteCircleControl.h"
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

  pGraphics->AttachControl(new IPanelControl(layout.tabBar, PanelColor(), true));
  const IRECT tabSwitchBounds = layout.tabBar.GetFromLeft(110.f).GetPadded(-6.f);
  pGraphics->AttachControl(new IVTabSwitchControl(tabSwitchBounds,
    [](IControl*) {},
    {"hello"},
    "",
    TabStyle(),
    EVShape::EndsRounded));

  pGraphics->AttachControl(new IPanelControl(layout.transport, PanelColor(), false));

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

  pGraphics->AttachControl(new IPanelControl(layout.middle, PanelColor(), false));
  pGraphics->AttachControl(new ::igraphics::CamelotNoteCircleControl(
    layout.noteCircle, NoteCircleFillColor(), NoteCircleLineColor(), 2.5f));
  pGraphics->AttachControl(new ::igraphics::GainKnobControl(layout.gain, kParamGain, "Gain", controlStyle));

  pGraphics->AttachControl(new IPanelControl(layout.waveSection, PanelColor(), true));
  const IRECT waveLabel = layout.waveSection.GetFromTop(20.f).GetPadded(-8.f);
  pGraphics->AttachControl(new ITextControl(waveLabel, "Sample", IText(13.f, IColor(160, 168, 180), "Roboto-Regular", EAlign::Near)));

  const IRECT plotBounds = layout.waveform.GetPadded(-6.f);
  auto* pPlayhead = new ::igraphics::PlayheadOverlayControl(plotBounds, 5.f, kCtrlTagPlayhead);
  auto* pTrack = new ::igraphics::WaveformTrackControl(layout.waveform, MakeSeekHandler(plugin), kCtrlTagWaveform);
  pTrack->SetPlayheadOverlay(pPlayhead);

  pGraphics->AttachControl(pTrack, kCtrlTagWaveform);
  pGraphics->AttachControl(pPlayhead, kCtrlTagPlayhead);

  // Windows NanoVG paint contract — see src/ui/bridge/UiPaintPolicy.h and ARCHITECTURE.md
  ::igraphics::InstallPaintPolicy(pGraphics);
  ::igraphics::ForceInitialFullPaint(pGraphics);
}

} // namespace CamelotSynthEditor
