#pragma once

#include "Styles.h"

namespace CamelotSynthEditor
{

/** Non-interactive caption with orange panel border. */
class LogoBadgeControl final : public igraphics::IControl
{
public:
  LogoBadgeControl(const igraphics::IRECT& bounds, const char* text)
  : IControl(bounds)
  , mText(text)
  {
    mIgnoreMouse = true;
  }

  void Draw(igraphics::IGraphics& g) override
  {
    g.FillRect(PanelColor(), mRECT);
    g.DrawRect(PanelAccentColor(), mRECT, nullptr, 2.5f);
    const IText style(12.f, IColor(176, 182, 192), "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(style, mText, mRECT.GetPadded(-4.f));
  }

private:
  const char* mText;
};

/** Panel fill with waveform-style border (matches WaveformTrackControl). */
class FramedPanelControl final : public igraphics::IControl
{
public:
  FramedPanelControl(const igraphics::IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(igraphics::IGraphics& g) override
  {
    g.FillRect(PanelColor(), mRECT);
    g.DrawRect(PanelAccentColor(), mRECT, nullptr, 2.5f);
  }
};

} // namespace CamelotSynthEditor
