#pragma once

#include "Styles.h"

namespace CamelotSynthEditor
{

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
    g.DrawRect(PanelBorderColor(), mRECT);
  }
};

} // namespace CamelotSynthEditor
