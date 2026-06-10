#pragma once

#include "IControls.h"
#include "PlayheadOverlayControl.h"
#include "UiPaintPolicy.h"
#include <algorithm>
#include <functional>
#include <vector>

BEGIN_IGRAPHICS_NAMESPACE

/**
 * Waveform envelope and scrub interaction. Playhead is a separate overlay control.
 * Envelope is drawn every frame (no ILayer cache). See UiPaintPolicy.h for repaint rules.
 */
class WaveformTrackControl : public IControl
{
public:
  using SeekHandler = std::function<void(float normalizedPosition)>;

  WaveformTrackControl(const IRECT& bounds,
                       SeekHandler onSeek,
                       int ctrlTag = kNoTag,
                       const IColor& plotBg = IColor(42, 44, 50),
                       const IColor& borderColor = IColor(96, 186, 132),
                       const IColor& fillColor = IColor(72, 148, 220, 140),
                       const IColor& outlineColor = IColor(96, 186, 132))
  : IControl(bounds, ctrlTag)
  , mOnSeek(std::move(onSeek))
  , mPlotBg(plotBg)
  , mBorderColor(borderColor)
  , mFillColor(fillColor)
  , mOutlineColor(outlineColor)
  {
    mIgnoreMouse = false;
  }

  void SetPlayheadOverlay(PlayheadOverlayControl* pOverlay)
  {
    mPlayheadOverlay = pOverlay;
    if (mPlayheadOverlay)
      mPlayheadOverlay->SetPlotBounds(PlotBounds());
  }

  void SetEnvelope(const std::vector<float>& maxPeaks, const std::vector<float>& minPeaks)
  {
    mMax = maxPeaks;
    mMin = minPeaks;
    SetDirty(false);
    RequestFullRepaint(GetUI());
  }

  bool IsScrubbing() const { return mScrubbing; }

  void SyncPlayheadFromDSP(float normPos)
  {
    if (mScrubbing || !mPlayheadOverlay)
      return;

    mPlayheadOverlay->SetNormalizedPosition(normPos);
  }

  void OnResize() override
  {
    if (mPlayheadOverlay)
      mPlayheadOverlay->SetPlotBounds(PlotBounds());
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    DrawContent(g, PlotBounds());
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (!PlotBounds().Contains(x, y))
      return;

    BeginInteraction();
    mScrubbing = true;
    mDragged = false;
    PreviewSeekAt(x);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    if (!mScrubbing)
      return;

    mDragged = true;
    CommitSeekAt(x);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    if (mScrubbing && !mDragged)
      CommitSeekAt(x);

    mScrubbing = false;
    mDragged = false;
    EndInteraction();
    SetDirty(false);
    RequestFullRepaint(GetUI());
  }

  IRECT PlotBounds() const
  {
    return mRECT.GetPadded(-6.f);
  }

private:
  void PreviewSeekAt(float x)
  {
    if (mPlayheadOverlay)
      mPlayheadOverlay->SetNormalizedPosition(NormalizedX(x));

    SetDirty(false);
    RequestFullRepaint(GetUI());
  }

  void CommitSeekAt(float x)
  {
    const float norm = NormalizedX(x);
    if (mPlayheadOverlay)
      mPlayheadOverlay->SetNormalizedPosition(norm);

    if (mOnSeek)
      mOnSeek(norm);

    SetDirty(false);
    RequestFullRepaint(GetUI());
  }

  float NormalizedX(float x) const
  {
    const IRECT plot = PlotBounds();
    if (plot.W() <= 0.f)
      return 0.f;

    return (std::max)(0.f, (std::min)(1.f, (x - plot.L) / plot.W()));
  }

  void DrawContent(IGraphics& g, const IRECT& plot)
  {
    const IColor grid(58, 62, 70);

    g.FillRect(mPlotBg, mRECT);
    g.DrawRect(mBorderColor, mRECT, nullptr, 2.f);
    g.DrawLine(grid, plot.L, plot.MH(), plot.R, plot.MH(), nullptr, 1.f);

    if (mMax.size() < 2 || mMin.size() != mMax.size())
      return;

    DrawEnvelope(g, plot, mFillColor, mOutlineColor);
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

  SeekHandler mOnSeek;
  PlayheadOverlayControl* mPlayheadOverlay = nullptr;
  std::vector<float> mMax;
  std::vector<float> mMin;
  IColor mPlotBg;
  IColor mBorderColor;
  IColor mFillColor;
  IColor mOutlineColor;
  bool mScrubbing = false;
  bool mDragged = false;
};

using SampleWaveformControl = WaveformTrackControl;

END_IGRAPHICS_NAMESPACE
