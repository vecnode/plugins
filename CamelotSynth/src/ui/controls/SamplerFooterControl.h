#pragma once

#include "IControls.h"

BEGIN_IGRAPHICS_NAMESPACE

/**
 * Sampler footer row — divider, fill, and caption text below the waveform plot.
 * Place interactive buttons in the left column via Attach (higher z-order).
 */
class SamplerFooterControl : public IControl
{
public:
  static constexpr float kButtonColumnWidth = 200.f;

  SamplerFooterControl(const IRECT& bounds,
                       const IColor& fillColor,
                       const IColor& borderColor,
                       const IColor& textColor,
                       const char* caption = "Lorem Ipsum")
  : IControl(bounds)
  , mFillColor(fillColor)
  , mBorderColor(borderColor)
  , mTextColor(textColor)
  , mCaption(caption)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(mFillColor, mRECT);
    g.DrawLine(mBorderColor, mRECT.L, mRECT.T, mRECT.R, mRECT.T, nullptr, 1.5f);

    const IRECT textArea = mRECT.GetPadded(-8.f).GetReducedFromLeft(kButtonColumnWidth);
    const IText textStyle(13.f, mTextColor, "Roboto-Regular", EAlign::Near, EVAlign::Middle);
    g.DrawText(textStyle, mCaption, textArea);
  }

  static IRECT ButtonColumn(const IRECT& footerBounds)
  {
    return footerBounds.GetPadded(-8.f).GetFromLeft(kButtonColumnWidth);
  }

private:
  IColor mFillColor;
  IColor mBorderColor;
  IColor mTextColor;
  const char* mCaption;
};

END_IGRAPHICS_NAMESPACE
