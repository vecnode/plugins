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
    plugin.SendParameterValueFromUI(kParamSeek, static_cast<double>(norm));
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

  const IVStyle compactButtonStyle = buttonStyle.WithLabelText({11.f, EVAlign::Middle});

  pGraphics->AttachControl(new IVButtonControl(layout.transportButtons.start, [&plugin](IControl* pCaller) {
    plugin.SendParameterValueFromUI(kParamTrigPlay, 1.);
    SplashClickActionFunc(pCaller);
  }, "Start", compactButtonStyle, true, true, EVShape::Rectangle));

  pGraphics->AttachControl(new IVButtonControl(layout.transportButtons.pause, [&plugin](IControl* pCaller) {
    plugin.SendParameterValueFromUI(kParamTrigPause, 1.);
    SplashClickActionFunc(pCaller);
  }, "Pause", compactButtonStyle, true, true, EVShape::Rectangle));

  pGraphics->AttachControl(new IVButtonControl(layout.transportButtons.stop, [&plugin](IControl* pCaller) {
    plugin.SendParameterValueFromUI(kParamTrigStop, 1.);
    SplashClickActionFunc(pCaller);
  }, "Stop", compactButtonStyle, true, true, EVShape::Rectangle));

  pGraphics->AttachControl(new FramedPanelControl(layout.middle));
  pGraphics->AttachControl(new ::igraphics::CamelotCircle(
    layout.middle,
    layout.noteCircle,
    NoteCircleFillColor(),
    CamelotBlockActiveColor(),
    CamelotBlockSelectedColor(),
    NoteCircleLineColor(),
    2.5f),
    kCtrlTagCamelotCircle);
  pGraphics->AttachControl(new ::igraphics::GainKnobControl(layout.gain, kParamGain, "Gain", controlStyle));

  pGraphics->AttachControl(new ::igraphics::SamplerSectionControl(
    layout.waveSection, PanelColor(), PanelAccentColor(), 2.5f));

  pGraphics->AttachControl(new ITextControl(
    layout.waveTitle,
    "Sampler",
    IText(13.f, TextColor(), "Roboto-Regular", EAlign::Near)));

  pGraphics->AttachControl(new ::igraphics::SamplerFooterControl(
    layout.samplerFooter.panel,
    PanelColor(),
    SamplerAccentColor()));

  pGraphics->AttachControl(new IVButtonControl(
    layout.samplerFooter.detectNote,
    [&plugin](IControl* pCaller) {
      plugin.SendParameterValueFromUI(kParamTrigDetectNote, 1.);
      SplashClickActionFunc(pCaller);
    },
    "Detect Note",
    compactButtonStyle,
    true),
    kCtrlTagDetectNote);

  pGraphics->AttachControl(new IVButtonControl(
    layout.samplerFooter.pitchDownOne,
    [&plugin](IControl* pCaller) {
      plugin.SendParameterValueFromUI(kParamTrigPitchDownOne, 1.);
      SplashClickActionFunc(pCaller);
    },
    "-1",
    compactButtonStyle,
    true),
    kCtrlTagPitchDownOne);

  pGraphics->AttachControl(new IVButtonControl(
    layout.samplerFooter.pitchUpOne,
    [&plugin](IControl* pCaller) {
      plugin.SendParameterValueFromUI(kParamTrigPitchUpOne, 1.);
      SplashClickActionFunc(pCaller);
    },
    "+1",
    compactButtonStyle,
    true),
    kCtrlTagPitchUpOne);

  pGraphics->AttachControl(new IVButtonControl(
    layout.samplerFooter.pitchReset,
    [&plugin](IControl* pCaller) {
      plugin.SendParameterValueFromUI(kParamTrigPitchReset, 1.);
      SplashClickActionFunc(pCaller);
    },
    "Reset",
    compactButtonStyle,
    true),
    kCtrlTagPitchReset);

  pGraphics->AttachControl(new IVSwitchControl(
    layout.samplerFooter.pitchMode,
    kParamPitchMode,
    "Live",
    compactButtonStyle));

  pGraphics->AttachControl(new ::igraphics::DetectedNoteLabelControl(
    layout.samplerFooter.detectedNote,
    TextColor(),
    TextMutedColor()),
    kCtrlTagDetectedNote);

  pGraphics->AttachControl(new ::igraphics::SampleLengthLabelControl(
    layout.samplerFooter.length,
    TextColor()),
    kCtrlTagSampleLength);

  const IRECT plotBounds = layout.waveform.GetPadded(-6.f);
  auto* pPlayhead = new ::igraphics::PlayheadOverlayControl(plotBounds, 5.f, kCtrlTagPlayhead);
  auto* pTrack = new ::igraphics::WaveformTrackControl(
    layout.waveform,
    MakeSeekHandler(plugin),
    kCtrlTagWaveform,
    WaveformPlotBgColor(),
    SamplerAccentColor(),
    WaveformFillColor(),
    SamplerAccentColor());
  pTrack->SetPlayheadOverlay(pPlayhead);

  pGraphics->AttachControl(pTrack, kCtrlTagWaveform);
  pGraphics->AttachControl(pPlayhead, kCtrlTagPlayhead);

  ::igraphics::InstallPaintPolicy(pGraphics);
  ::igraphics::ForceInitialFullPaint(pGraphics);
}

} // namespace CamelotSynthEditor
