#pragma once

#include "IControls.h"
#include "UiPaintPolicy.h"
#include <algorithm>
#include <cmath>

BEGIN_IGRAPHICS_NAMESPACE

/** Thin moving overlay — playhead line with its own bounds (proper invalidation). */
class PlayheadOverlayControl : public IControl
{
public:
  PlayheadOverlayControl(const IRECT& plotBounds, float columnWidth = 5.f, int ctrlTag = kNoTag)
  : IControl(IRECT(), ctrlTag)
  , mPlot(plotBounds)
  , mColumnWidth(columnWidth)
  , mNorm(-1.f)
  {
    mIgnoreMouse = true;
    MoveToNormalized(0.f);
  }

  void SetPlotBounds(const IRECT& plot)
  {
    mPlot = plot;
    MoveToNormalized(mNorm);
  }

  void SetNormalizedPosition(float norm)
  {
    MoveToNormalized(norm);
  }

  float GetNormalizedPosition() const { return mNorm; }

  void Draw(IGraphics& g) override
  {
    const IColor eraseBg(22, 24, 28);
    g.FillRect(eraseBg, mRECT);

    const float playX = mPlot.L + mNorm * mPlot.W();
    g.DrawLine(IColor(255, 88, 88), playX, mPlot.T, playX, mPlot.B, nullptr, 2.f);
  }

private:
  void MoveToNormalized(float norm)
  {
    norm = (std::max)(0.f, (std::min)(1.f, norm));

    const float pixelStep = mPlot.W() > 1.f ? 1.f / mPlot.W() : 1.f;
    if (std::fabs(norm - mNorm) < pixelStep && mRECT.W() > 0.f)
      return;

    mNorm = norm;

    const float playX = mPlot.L + mNorm * mPlot.W();
    const float halfW = mColumnWidth * 0.5f;
    const IRECT column(playX - halfW, mPlot.T, playX + halfW, mPlot.B);

    SetTargetAndDrawRECTs(column);
    SetDirty(false);
    RequestFullRepaint(GetUI());
  }

  IRECT mPlot;
  float mColumnWidth;
  float mNorm;
};

END_IGRAPHICS_NAMESPACE
