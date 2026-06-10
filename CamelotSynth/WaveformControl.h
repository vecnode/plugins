#pragma once

#include "IControls.h"
#include <algorithm>
#include <vector>

BEGIN_IGRAPHICS_NAMESPACE

/** Static waveform preview with a playback position marker. */
class SampleWaveformControl : public IControl
{
public:
  SampleWaveformControl(const IRECT& bounds, int ctrlTag = kNoTag)
  : IControl(bounds, ctrlTag)
  {
    mIgnoreMouse = true;
  }

  void SetPeaks(const std::vector<float>& peaks)
  {
    mPeaks = peaks;
    SetDirty(true);
  }

  void SetPlayhead(float normPos)
  {
    const float clamped = std::max(0.f, std::min(1.f, normPos));
    if (clamped != mPlayhead)
    {
      mPlayhead = clamped;
      SetDirty(true);
    }
  }

  void Draw(IGraphics& g) override
  {
    const IRECT r = mRECT;
    g.FillRect(IColor(30, 30, 30), r);
    g.DrawRect(COLOR_WHITE.WithOpacity(0.4f), r);

    if (mPeaks.size() < 2)
      return;

    const float midY = r.MH();
    const float halfH = r.H() * 0.42f;

    for (size_t i = 1; i < mPeaks.size(); i++)
    {
      const float x0 = r.L + (static_cast<float>(i - 1) / static_cast<float>(mPeaks.size() - 1)) * r.W();
      const float x1 = r.L + (static_cast<float>(i) / static_cast<float>(mPeaks.size() - 1)) * r.W();
      const float y0 = midY - mPeaks[i - 1] * halfH;
      const float y1 = midY - mPeaks[i] * halfH;
      g.DrawLine(IColor(180, 220, 255), x0, y0, x1, y1, nullptr, 1.f);
    }

    const float playX = r.L + mPlayhead * r.W();
    g.DrawLine(IColor(255, 90, 90), playX, r.T, playX, r.B, nullptr, 1.5f);
  }

private:
  std::vector<float> mPeaks;
  float mPlayhead = 0.f;
};

END_IGRAPHICS_NAMESPACE
