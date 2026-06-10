#pragma once

#include "IControls.h"
#include "UiPaintPolicy.h"
#include <algorithm>
#include <cmath>

BEGIN_IGRAPHICS_NAMESPACE

/** Playhead line overlay. Uses stable plot bounds so invalidation never shrinks to a column. */
class PlayheadOverlayControl : public IControl
{
public:
  PlayheadOverlayControl(const IRECT& plotBounds, float columnWidth = 5.f, int ctrlTag = kNoTag)
  : IControl(plotBounds, ctrlTag)
  , mPlot(plotBounds)
  , mColumnWidth(columnWidth)
  , mNorm(0.f)
  {
    mIgnoreMouse = true;
  }

  void SetPlotBounds(const IRECT& plot)
  {
    mPlot = plot;
    SetTargetRECT(plot);
    SetDirty(false);
  }

  void SetNormalizedPosition(float norm)
  {
    norm = (std::max)(0.f, (std::min)(1.f, norm));

    const float pixelStep = mPlot.W() > 1.f ? 1.f / mPlot.W() : 1.f;
    if (std::fabs(norm - mNorm) < pixelStep)
      return;

    mNorm = norm;
    SetDirty(false);
    RequestFullRepaint(GetUI());
  }

  float GetNormalizedPosition() const { return mNorm; }

  void Draw(IGraphics& g) override
  {
    const float playX = mPlot.L + mNorm * mPlot.W();
    g.DrawLine(IColor(255, 88, 88), playX, mPlot.T, playX, mPlot.B, nullptr, 2.f);
  }

private:
  IRECT mPlot;
  float mColumnWidth;
  float mNorm;
};

END_IGRAPHICS_NAMESPACE
