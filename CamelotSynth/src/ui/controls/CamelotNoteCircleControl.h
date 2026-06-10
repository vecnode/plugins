#pragma once

#include "IControls.h"
#include <cmath>

BEGIN_IGRAPHICS_NAMESPACE

/**
 * Camelot wheel: filled disc + 12 chromatic spokes + 2 concentric rings
 * at 1/3 and 2/3 radius (three equal radial zones).
 */
class CamelotNoteCircleControl : public IControl
{
public:
  static constexpr int kNoteDivisions = 12;
  static constexpr int kRadialZones = 3;

  CamelotNoteCircleControl(const IRECT& bounds,
                           const IColor& fillColor,
                           const IColor& lineColor,
                           float lineThickness = 2.5f)
  : IControl(bounds)
  , mFillColor(fillColor)
  , mLineColor(lineColor)
  , mLineThickness(lineThickness)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float radius = (std::min)(mRECT.W(), mRECT.H()) * 0.5f;

    g.FillEllipse(mFillColor, mRECT);
    DrawZoneRings(g, cx, cy, radius);
    DrawNoteDivisions(g, cx, cy, radius);
    g.DrawEllipse(mLineColor, mRECT, nullptr, mLineThickness);
  }

private:
  void DrawZoneRings(IGraphics& g, float cx, float cy, float radius) const
  {
    for (int ring = 1; ring < kRadialZones; ring++)
    {
      const float r = radius * (static_cast<float>(ring) / static_cast<float>(kRadialZones));
      const IRECT ringBounds(cx - r, cy - r, cx + r, cy + r);
      g.DrawEllipse(mLineColor, ringBounds, nullptr, mLineThickness);
    }
  }

  void DrawNoteDivisions(IGraphics& g, float cx, float cy, float radius) const
  {
    constexpr float kTwoPi = 6.283185307179586f;
    constexpr float kStartAngle = -1.5707963267948966f; // 12 o'clock

    for (int i = 0; i < kNoteDivisions; i++)
    {
      const float angle = kStartAngle + (static_cast<float>(i) / static_cast<float>(kNoteDivisions)) * kTwoPi;
      const float x2 = cx + radius * std::cos(angle);
      const float y2 = cy + radius * std::sin(angle);
      g.DrawLine(mLineColor, cx, cy, x2, y2, nullptr, mLineThickness);
    }
  }

  IColor mFillColor;
  IColor mLineColor;
  float mLineThickness;
};

END_IGRAPHICS_NAMESPACE
