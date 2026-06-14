#pragma once

#include "IControls.h"
#include <cstdio>
#include <cstring>

BEGIN_IGRAPHICS_NAMESPACE

/** Length label — last cell of the sampler footer row. */
class SampleLengthLabelControl : public IControl
{
public:
  SampleLengthLabelControl(const IRECT& bounds, const IColor& textColor)
  : IControl(bounds)
  , mTextColor(textColor)
  {
    mIgnoreMouse = true;
    SetDurationSeconds(-1.);
  }

  void SetDurationSeconds(double seconds)
  {
    char next[32];
    if (seconds < 0.)
      std::snprintf(next, sizeof(next), "Length: --");
    else
      std::snprintf(next, sizeof(next), "Length: %.1fs", seconds);

    if (std::strcmp(next, mText) != 0)
    {
      std::strncpy(mText, next, sizeof(mText));
      mText[sizeof(mText) - 1] = '\0';
      SetDirty(false);
    }
  }

  void Draw(IGraphics& g) override
  {
    const IText textStyle(13.f, mTextColor, "Roboto-Regular", EAlign::Far, EVAlign::Middle);
    g.DrawText(textStyle, mText, mRECT);
  }

private:
  IColor mTextColor;
  char mText[32] = {};
};

/** Detected note label — prominent, left-aligned after the Detect Note button. */
class DetectedNoteLabelControl : public IControl
{
public:
  DetectedNoteLabelControl(const IRECT& bounds, const IColor& textColor, const IColor& accentColor)
  : IControl(bounds)
  , mTextColor(textColor)
  , mAccentColor(accentColor)
  , mMutedColor(textColor.WithOpacity(0.65f))
  {
    mIgnoreMouse = true;
    SetText("Note: --");
  }

  void SetText(const char* text)
  {
    if (!text)
      return;

    if (std::strcmp(text, mText) != 0)
    {
      std::strncpy(mText, text, sizeof(mText));
      mText[sizeof(mText) - 1] = '\0';
      mAnalyzing = false;
      SetDirty(false);
    }
  }

  void SetAnalyzing()
  {
    if (!mAnalyzing || std::strcmp(mText, "Analyzing...") != 0)
    {
      std::strncpy(mText, "Analyzing...", sizeof(mText));
      mText[sizeof(mText) - 1] = '\0';
      mAnalyzing = true;
      SetDirty(false);
    }
  }

  void Draw(IGraphics& g) override
  {
    const IColor& color = mAnalyzing ? mMutedColor : mAccentColor;
    const IText textStyle(15.f, color, "Roboto-Regular", EAlign::Near, EVAlign::Middle);
    g.DrawText(textStyle, mText, mRECT);
  }

private:
  IColor mTextColor;
  IColor mAccentColor;
  IColor mMutedColor;
  bool mAnalyzing = false;
  char mText[24] = {};
};

/** Sampler footer panel — single row hosts button, note, and length controls. */
class SamplerFooterControl : public IControl
{
public:
  SamplerFooterControl(const IRECT& bounds,
                       const IColor& fillColor,
                       const IColor& borderColor)
  : IControl(bounds)
  , mFillColor(fillColor)
  , mBorderColor(borderColor)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(mFillColor, mRECT);
    g.DrawLine(mBorderColor, mRECT.L, mRECT.T, mRECT.R, mRECT.T, nullptr, 1.5f);
  }

private:
  IColor mFillColor;
  IColor mBorderColor;
};

END_IGRAPHICS_NAMESPACE
