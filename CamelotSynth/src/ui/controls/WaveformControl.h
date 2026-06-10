#pragma once

#include "IControls.h"
#include "PlayheadOverlayControl.h"
#include "UiPaintPolicy.h"
#include <algorithm>
#include <functional>
#include <vector>

BEGIN_IGRAPHICS_NAMESPACE

/** Static waveform envelope + scrub interaction (playhead is a separate overlay). */
class WaveformTrackControl : public IControl
{
public:
  using SeekHandler = std::function<void(float normalizedPosition)>;

  WaveformTrackControl(const IRECT& bounds, SeekHandler onSeek, int ctrlTag = kNoTag)
  : IControl(bounds, ctrlTag)
  , mOnSeek(std::move(onSeek))
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
    if (mLayer)
      mLayer->Invalidate();
    SetDirty(false);
    RequestFullRepaint(GetUI());
  }

  void SyncPlayheadFromDSP(float normPos)
  {
    if (mPlayheadOverlay)
      mPlayheadOverlay->SetNormalizedPosition(normPos);
  }

  void OnResize() override
  {
    if (mLayer)
      mLayer->Invalidate();
    if (mPlayheadOverlay)
      mPlayheadOverlay->SetPlotBounds(PlotBounds());
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    DrawStaticContent(g, PlotBounds());
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (!PlotBounds().Contains(x, y))
      return;

    BeginInteraction();
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
    EndInteraction();
    if (mLayer)
      mLayer->Invalidate();
    SetDirty(false);
    RequestFullRepaint(GetUI());
  }

  IRECT PlotBounds() const
  {
    return mRECT.GetPadded(-6.f);
  }

private:
  void SeekAt(float x)
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

    if (mScrubbing)
    {
      DrawEnvelope(g, plot, fill, outline);
      return;
    }

    if (!g.CheckLayer(mLayer))
    {
      g.StartLayer(this, mRECT);
      DrawEnvelope(g, plot, fill, outline);
      mLayer = g.EndLayer();
    }

    g.DrawLayer(mLayer);
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
  ILayerPtr mLayer;
  bool mScrubbing = false;
};

// Backward-compatible alias used by CamelotSynth.cpp
using SampleWaveformControl = WaveformTrackControl;

END_IGRAPHICS_NAMESPACE
