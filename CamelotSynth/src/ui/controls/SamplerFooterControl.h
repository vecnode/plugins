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

/** Detected note label — white text; accent blue when a result is shown. */
class DetectedNoteLabelControl : public IControl
{
public:
  DetectedNoteLabelControl(const IRECT& bounds,
                           const IColor& textColor,
                           const IColor& mutedColor)
  : IControl(bounds)
  , mTextColor(textColor)
  , mMutedColor(mutedColor)
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
      mBusy = false;
      SetDirty(false);
    }
  }

  void SetBusy(const char* message)
  {
    if (!message)
      message = "Working...";

    if (!mBusy || std::strcmp(mText, message) != 0)
    {
      std::strncpy(mText, message, sizeof(mText));
      mText[sizeof(mText) - 1] = '\0';
      mBusy = true;
      SetDirty(false);
    }
  }

  void SetAnalyzing() { SetBusy("Detecting..."); }

  void SetProcessing() { SetBusy("Processing pitch..."); }

  void Draw(IGraphics& g) override
  {
    const IColor& color = mBusy ? mMutedColor : mTextColor;
    const IText textStyle(15.f, color, "Roboto-Regular", EAlign::Near, EVAlign::Middle);
    g.DrawText(textStyle, mText, mRECT);
  }

private:
  IColor mTextColor;
  IColor mMutedColor;
  bool mBusy = false;
  char mText[40] = {};
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
