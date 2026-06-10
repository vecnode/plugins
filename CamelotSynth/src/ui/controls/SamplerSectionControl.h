#pragma once

#include "IControls.h"

BEGIN_IGRAPHICS_NAMESPACE

/** Framed sampler footer panel — fill plus accent border (matches CamelotCircle line weight). */
class SamplerSectionControl : public IControl
{
public:
  SamplerSectionControl(const IRECT& bounds,
                        const IColor& fillColor,
                        const IColor& borderColor,
                        float borderThickness = 2.5f)
  : IControl(bounds)
  , mFillColor(fillColor)
  , mBorderColor(borderColor)
  , mBorderThickness(borderThickness)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(mFillColor, mRECT);
    g.DrawRect(mBorderColor, mRECT, nullptr, mBorderThickness);
  }

private:
  IColor mFillColor;
  IColor mBorderColor;
  float mBorderThickness;
};

END_IGRAPHICS_NAMESPACE
