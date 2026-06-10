#pragma once

#include "IControls.h"
#include "UiRedraw.h"
#include <algorithm>
#include <functional>
#include <vector>

BEGIN_IGRAPHICS_NAMESPACE

/**
 * Interactive waveform with click/drag seek.
 * Envelope is drawn directly each frame (no ILayer cache) so moving overlays
 * never leave stale pixels when combined with full-window repaint policy.
 */
class SampleWaveformControl : public IControl
{
public:
  using SeekHandler = std::function<void(float normalizedPosition)>;

  SampleWaveformControl(const IRECT& bounds, SeekHandler onSeek, int ctrlTag = kNoTag)
  : IControl(bounds, ctrlTag)
  , mOnSeek(std::move(onSeek))
  {
    mIgnoreMouse = false;
  }

  void SetEnvelope(const std::vector<float>& maxPeaks, const std::vector<float>& minPeaks)
  {
    mMax = maxPeaks;
    mMin = minPeaks;
    SetDirty(false);
  }

  void SyncPlayheadFromDSP(float normPos)
  {
    SetPlayheadInternal(normPos, false);
  }

  void OnResize() override
  {
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const IRECT plot = PlotBounds();
    DrawStaticContent(g, plot);
    DrawPlayhead(g, plot, mPlayhead);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (!PlotBounds().Contains(x, y))
      return;

    mScrubbing = true;
    SeekAt(x);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    if (!mScrubbing)
      return;

    SeekAt(x);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    mScrubbing = false;
  }

private:
  IRECT PlotBounds() const
  {
    return mRECT.GetPadded(-6.f);
  }

  float NormalizedX(float x) const
  {
    const IRECT plot = PlotBounds();
    if (plot.W() <= 0.f)
      return 0.f;

    return (std::max)(0.f, (std::min)(1.f, (x - plot.L) / plot.W()));
  }

  void SeekAt(float x)
  {
    const float norm = NormalizedX(x);
    SetPlayheadInternal(norm, true);

    if (mOnSeek)
      mOnSeek(norm);
  }

  void SetPlayheadInternal(float normPos, bool fromUser)
  {
    const float clamped = (std::max)(0.f, (std::min)(1.f, normPos));
    const IRECT plot = PlotBounds();
    const float pixelNorm = plot.W() > 1.f ? 1.f / plot.W() : 1.f;

    if (!fromUser && std::fabs(clamped - mPlayhead) < pixelNorm)
      return;

    mPlayhead = clamped;
    SetDirty(false);

    if (fromUser)
      RequestFullRepaint(GetUI());
  }

  void DrawStaticContent(IGraphics& g, const IRECT& plot)
  {
    const IColor bg(22, 24, 28);
    const IColor border(55, 58, 66);
    const IColor grid(40, 43, 50);
    const IColor fill(72, 148, 220, 140);
    const IColor outline(130, 190, 255);

    g.FillRect(bg, mRECT);
    g.DrawRect(border, mRECT);
    g.DrawLine(grid, plot.L, plot.MH(), plot.R, plot.MH(), nullptr, 1.f);

    if (mMax.size() < 2 || mMin.size() != mMax.size())
      return;

    DrawEnvelope(g, plot, fill, outline);
  }

  void DrawEnvelope(IGraphics& g, const IRECT& plot, const IColor& fill, const IColor& outline) const
  {
    const float midY = plot.MH();
    const float halfH = plot.H() * 0.46f;
    const size_t n = mMax.size();

    const auto xAt = [&](size_t i) {
      return plot.L + (static_cast<float>(i) / static_cast<float>(n - 1)) * plot.W();
    };

    g.PathClear();
    g.PathMoveTo(xAt(0), midY - mMax[0] * halfH);
    for (size_t i = 1; i < n; i++)
      g.PathLineTo(xAt(i), midY - mMax[i] * halfH);

    for (int i = static_cast<int>(n) - 1; i >= 0; i--)
      g.PathLineTo(xAt(static_cast<size_t>(i)), midY - mMin[static_cast<size_t>(i)] * halfH);

    g.PathClose();
    g.PathFill(fill);
    g.PathStroke(outline, 1.f);
  }

  void DrawPlayhead(IGraphics& g, const IRECT& plot, float normPos) const
  {
    const float playX = plot.L + normPos * plot.W();
    g.DrawLine(IColor(255, 88, 88), playX, plot.T, playX, plot.B, nullptr, 1.5f);
  }

  SeekHandler mOnSeek;
  std::vector<float> mMax;
  std::vector<float> mMin;
  float mPlayhead = 0.f;
  bool mScrubbing = false;
};

END_IGRAPHICS_NAMESPACE
