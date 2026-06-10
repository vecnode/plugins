#pragma once

#include "IGraphics.h"

namespace CamelotSynthEditor
{

/** Fixed layout constants (see src/ARCHITECTURE.md). */
struct LayoutConstants
{
  static constexpr float kPadding = 16.f;
  static constexpr float kMeterWidth = 44.f;
  static constexpr float kTabHeight = 40.f;
  static constexpr float kTransportHeight = 56.f;
  static constexpr float kWaveSectionHeight = 210.f;
  static constexpr float kGainKnobSize = 96.f;
  static constexpr float kNoteCircleDiameter = 360.f;
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
  IRECT waveform;
};

inline Regions ComputeRegions(const IRECT& bounds)
{
  using C = LayoutConstants;

  Regions r;
  const IRECT padded = bounds.GetPadded(-C::kPadding);

  r.meter = padded.GetFromRight(C::kMeterWidth);
  const IRECT content = padded.GetReducedFromRight(C::kMeterWidth + 10.f);

  r.tabBar = content.GetFromTop(C::kTabHeight);

  const IRECT belowTab = content.GetReducedFromTop(C::kTabHeight + 8.f);
  r.transport = belowTab.GetFromTop(C::kTransportHeight);

  r.waveSection = content.GetFromBottom(C::kWaveSectionHeight);
  r.middle = belowTab.GetReducedFromTop(C::kTransportHeight + 8.f)
                    .GetReducedFromBottom(C::kWaveSectionHeight + 8.f);
  r.noteCircle = r.middle.GetCentredInside(C::kNoteCircleDiameter, C::kNoteCircleDiameter);

  r.gain = r.middle.GetFromBottom(C::kGainKnobSize + 8.f)
                   .GetFromRight(C::kGainKnobSize + 8.f)
                   .GetCentredInside(C::kGainKnobSize, C::kGainKnobSize);

  r.waveform = r.waveSection.GetReducedFromTop(22.f).GetPadded(-4.f);

  return r;
}

} // namespace CamelotSynthEditor
