#pragma once

#include "IControls.h"

namespace CamelotSynthEditor
{

inline IColor PanelColor()
{
  return IColor(56, 58, 64);
}

inline IColor PanelBorderColor()
{
  return IColor(72, 76, 84);
}

inline IColor PanelAccentColor()
{
  return IColor(220, 140, 72);
}

inline IColor SamplerAccentColor()
{
  return IColor(96, 186, 132);
}

inline IColor NoteCircleFillColor()
{
  return IColor(100, 104, 112);
}

inline IColor NoteCircleLineColor()
{
  return IColor(168, 174, 186);
}

inline IColor CamelotBlockActiveColor()
{
  return IColor(150, 156, 170);
}

inline IColor CamelotBlockSelectedColor()
{
  return IColor(72, 148, 220);
}

inline IVStyle ControlStyle()
{
  return DEFAULT_STYLE
    .WithColor(kBG, IColor(64, 68, 76))
    .WithColor(kFG, IColor(225, 228, 235))
    .WithColor(kFR, IColor(88, 92, 102))
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
    .WithColor(kBG, IColor(52, 55, 62))
    .WithColor(kPR, IColor(68, 72, 82))
    .WithColor(kHL, IColor(72, 148, 220))
    .WithLabelText(IText(15.f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle));
}

} // namespace CamelotSynthEditor
