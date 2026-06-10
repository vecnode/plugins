#pragma once

#include "IControls.h"

namespace CamelotSynthEditor
{

inline IColor PanelColor()
{
  return IColor(34, 36, 42);
}

inline IColor NoteCircleFillColor()
{
  return IColor(88, 92, 100);
}

inline IColor NoteCircleLineColor()
{
  return IColor(168, 174, 186);
}

inline IColor CamelotBlockHoverColor()
{
  return IColor(118, 124, 136);
}

inline IColor CamelotBlockPressedColor()
{
  return IColor(150, 156, 170);
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

} // namespace CamelotSynthEditor
