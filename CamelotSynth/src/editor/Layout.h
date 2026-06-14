#pragma once

#include "IGraphics.h"
#include <algorithm>

namespace CamelotSynthEditor
{

/** Fixed layout constants — row heights and gaps; cell widths come from RowLayout helpers. */
struct LayoutConstants
{
  static constexpr float kPadding = 16.f;
  static constexpr float kInnerPadding = 8.f;
  static constexpr float kSectionGap = 8.f;
  static constexpr float kRowGap = 8.f;

  static constexpr float kMeterWidth = 44.f;
  static constexpr float kTabHeight = 40.f;
  static constexpr float kTransportHeight = 56.f;
  static constexpr float kWaveSectionHeight = 210.f;
  static constexpr float kWaveTitleHeight = 22.f;
  static constexpr float kWavePlotFraction = 0.5f;

  static constexpr float kGainKnobSize = 96.f;
  static constexpr float kNoteCircleDiameter = 460.f;

  static constexpr float kCompactButtonBaseW = 100.f;
  static constexpr float kCompactButtonBaseH = 44.f;
  static constexpr float kCompactButtonScale = 0.5f;

  static constexpr float kDetectNoteButtonWidth = 84.f;
  static constexpr float kFooterDetectedNoteShare = 0.42f;
};

/** Horizontal row splitter — fixed-width cells from the left, remainder for the last column. */
struct RowLayout
{
  RowLayout(const IRECT& row, float gap)
  : mCursor(row)
  , mGap(gap)
  {
  }

  IRECT TakeFixed(float width, float height)
  {
    const float clampedW = std::min(width, mCursor.W());
    const IRECT slot(mCursor.L, mCursor.T, mCursor.L + clampedW, mCursor.B);
    IRECT cell = slot.GetCentredInside(clampedW, height);
    mCursor.L = slot.R + mGap;
    return cell;
  }

  IRECT TakeShare(float fraction)
  {
    const float clampedW = std::max(0.f, mCursor.W() * fraction);
    const IRECT cell(mCursor.L, mCursor.T, mCursor.L + clampedW, mCursor.B);
    mCursor.L = cell.R + mGap;
    return cell;
  }

  IRECT Remainder() const
  {
    return mCursor;
  }

private:
  IRECT mCursor;
  float mGap;
};

inline float CompactButtonWidth()
{
  return LayoutConstants::kCompactButtonBaseW * LayoutConstants::kCompactButtonScale;
}

inline float CompactButtonHeight()
{
  return LayoutConstants::kCompactButtonBaseH * LayoutConstants::kCompactButtonScale;
}

inline IRECT RowContent(const IRECT& region)
{
  return region.GetPadded(-LayoutConstants::kInnerPadding);
}

struct TransportButtons
{
  IRECT start;
  IRECT pause;
  IRECT stop;
};

struct SamplerFooterLayout
{
  IRECT panel;
  IRECT rowDetect;
  IRECT rowPitch;
  IRECT detectNote;
  IRECT detectedNote;
  IRECT length;
  IRECT pitchDownOne;
  IRECT pitchUpOne;
  IRECT pitchReset;
  IRECT pitchMode;
};

struct Regions
{
  IRECT meter;
  IRECT tabBar;
  IRECT transport;
  IRECT middle;
  IRECT noteCircle;
  IRECT gain;
  IRECT waveSection;
  IRECT waveTitle;
  IRECT waveform;
  TransportButtons transportButtons;
  SamplerFooterLayout samplerFooter;
};

inline TransportButtons ComputeTransportButtons(const IRECT& transportRegion)
{
  using C = LayoutConstants;

  RowLayout row(RowContent(transportRegion), C::kRowGap);
  const float btnW = CompactButtonWidth();
  const float btnH = CompactButtonHeight();

  TransportButtons buttons;
  buttons.start = row.TakeFixed(btnW, btnH);
  buttons.pause = row.TakeFixed(btnW, btnH);
  buttons.stop = row.TakeFixed(btnW, btnH);
  return buttons;
}

inline SamplerFooterLayout ComputeSamplerFooter(const IRECT& footerBounds)
{
  using C = LayoutConstants;

  SamplerFooterLayout layout;
  layout.panel = footerBounds;

  const IRECT padded = footerBounds.GetPadded(-C::kInnerPadding);
  const float rowGap = 4.f;
  const float rowH = (padded.H() - rowGap) * 0.5f;

  layout.rowDetect = IRECT(padded.L, padded.T, padded.R, padded.T + rowH);
  layout.rowPitch = IRECT(padded.L, layout.rowDetect.B + rowGap, padded.R, padded.B);

  RowLayout detectRow(RowContent(layout.rowDetect), C::kRowGap);
  layout.detectNote = detectRow.TakeFixed(C::kDetectNoteButtonWidth, CompactButtonHeight());
  layout.detectedNote = detectRow.TakeShare(C::kFooterDetectedNoteShare);
  layout.length = detectRow.Remainder();

  RowLayout pitchRow(RowContent(layout.rowPitch), C::kRowGap);
  layout.pitchDownOne = pitchRow.TakeFixed(CompactButtonWidth(), CompactButtonHeight());
  layout.pitchUpOne = pitchRow.TakeFixed(CompactButtonWidth(), CompactButtonHeight());
  layout.pitchReset = pitchRow.TakeFixed(CompactButtonWidth(), CompactButtonHeight());
  layout.pitchMode = pitchRow.Remainder().GetFromLeft(120.f);

  return layout;
}

inline Regions ComputeRegions(const IRECT& bounds)
{
  using C = LayoutConstants;

  Regions r;
  const IRECT padded = bounds.GetPadded(-C::kPadding);

  r.meter = padded.GetFromRight(C::kMeterWidth);
  const IRECT content = padded.GetReducedFromRight(C::kMeterWidth + C::kSectionGap);

  r.tabBar = content.GetFromTop(C::kTabHeight);

  const IRECT belowTab = content.GetReducedFromTop(C::kTabHeight + C::kSectionGap);
  r.transport = belowTab.GetFromTop(C::kTransportHeight);

  r.waveSection = content.GetFromBottom(C::kWaveSectionHeight);
  r.middle = belowTab.GetReducedFromTop(C::kTransportHeight + C::kSectionGap)
                    .GetReducedFromBottom(C::kWaveSectionHeight + C::kSectionGap);
  r.noteCircle = r.middle.GetCentredInside(C::kNoteCircleDiameter, C::kNoteCircleDiameter);

  r.gain = r.middle.GetFromBottom(C::kGainKnobSize + C::kSectionGap)
                   .GetFromRight(C::kGainKnobSize + C::kSectionGap)
                   .GetCentredInside(C::kGainKnobSize, C::kGainKnobSize);

  r.waveTitle = r.waveSection.GetFromTop(C::kWaveTitleHeight).GetPadded(-C::kInnerPadding);

  const IRECT waveBody = r.waveSection.GetReducedFromTop(C::kWaveTitleHeight).GetPadded(-4.f);
  const float plotHeight = waveBody.H() * C::kWavePlotFraction;
  const IRECT footerBounds = waveBody.GetReducedFromTop(plotHeight);
  r.waveform = waveBody.GetFromTop(plotHeight);

  r.transportButtons = ComputeTransportButtons(r.transport);
  r.samplerFooter = ComputeSamplerFooter(footerBounds);

  return r;
}

} // namespace CamelotSynthEditor
