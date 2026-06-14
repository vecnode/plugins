#pragma once

#include "IControls.h"

namespace CamelotSynthEditor
{

inline IColor TextColor()
{
  return IColor(255, 255, 255);
}

inline IColor TextMutedColor()
{
  return IColor(170, 195, 225);
}

inline IColor PanelColor()
{
  return IColor(10, 12, 18);
}

inline IColor PanelBorderColor()
{
  return IColor(48, 68, 96);
}

inline IColor PanelAccentColor()
{
  return IColor(120, 185, 255);
}

inline IColor SamplerAccentColor()
{
  return IColor(100, 170, 240);
}

inline IColor WaveformPlotBgColor()
{
  return IColor(6, 8, 14);
}

inline IColor WaveformFillColor()
{
  return IColor(100, 170, 240, 130);
}

inline IColor NoteCircleFillColor()
{
  return IColor(18, 24, 36);
}

inline IColor NoteCircleLineColor()
{
  return IColor(140, 190, 245);
}

inline IColor CamelotBlockActiveColor()
{
  return IColor(65, 95, 135);
}

inline IColor CamelotBlockSelectedColor()
{
  return IColor(150, 205, 255);
}
inline IColor PlayheadColor()
{
  return IColor(255, 255, 255);
}

inline IVStyle ControlStyle()
{
  return DEFAULT_STYLE
    .WithColor(kBG, IColor(16, 20, 30))
    .WithColor(kFG, TextColor())
    .WithColor(kFR, IColor(70, 100, 140))
    .WithColor(kPR, IColor(90, 155, 230))
    .WithColor(kHL, IColor(140, 200, 255))
    .WithDrawFrame(true)
    .WithDrawShadows(false)
    .WithRoundness(0.f)
    .WithLabelText(IText(14.f, TextColor(), "Roboto-Regular", EAlign::Center, EVAlign::Middle));
}

inline IVStyle ButtonStyle()
{
  return ControlStyle().WithRoundness(0.f);
}

inline IVStyle TabStyle()
{
  return ControlStyle()
    .WithColor(kBG, IColor(14, 18, 28))
    .WithColor(kPR, IColor(55, 80, 115))
    .WithColor(kHL, IColor(120, 185, 255))
    .WithLabelText(IText(15.f, TextColor(), "Roboto-Regular", EAlign::Center, EVAlign::Middle));
}

} // namespace CamelotSynthEditor
