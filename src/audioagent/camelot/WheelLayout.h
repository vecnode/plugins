#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace audioagent::camelot
{

/** Axis-aligned bounds (framework-agnostic; maps to iPlug IRECT at the UI layer). */
struct Bounds
{
  float L = 0.f;
  float T = 0.f;
  float R = 0.f;
  float B = 0.f;

  static Bounds From(float left, float top, float right, float bottom)
  {
    return {left, top, right, bottom};
  }

  float W() const { return R - L; }
  float H() const { return B - T; }
  float MW() const { return (L + R) * 0.5f; }
  float MH() const { return (T + B) * 0.5f; }
};

/**
 * Camelot wheel geometry — 12 spokes × 3 zones (B1–B36).
 * Pure C++ layout and hit-testing; rendering stays in the host UI framework.
 */
class WheelLayout
{
public:
  static constexpr int kSpokeCount = 12;
  static constexpr int kZoneCount = 3;
  static constexpr int kBlockCount = kSpokeCount * kZoneCount;
  static constexpr float kStartAngleDeg = -90.f;
  static constexpr float kSliceDeg = 360.f / static_cast<float>(kSpokeCount);
  static constexpr float kDegToRad = 0.017453292519943295f;
  static constexpr float kRadToDeg = 57.29577951308232f;

  struct BlockRegion
  {
    int spoke = 0;
    int zone = 0;
    float angleStartDeg = 0.f;
    float angleEndDeg = 0.f;
    float rInner = 0.f;
    float rOuter = 0.f;

    int FlatIndex() const { return spoke * kZoneCount + zone; }
  };

  struct LineLayout
  {
    float cx = 0.f;
    float cy = 0.f;
    float outerRadius = 0.f;
    Bounds outerBounds;
    std::array<float, kSpokeCount> spokeAnglesDeg {};
    std::array<float, kZoneCount - 1> zoneRingRadii {};
    bool valid = false;
  };

  static void PointAtAngle(float cx, float cy, float radius, float angleDeg, float& x, float& y)
  {
    const float rad = angleDeg * kDegToRad;
    x = cx + radius * std::cos(rad);
    y = cy + radius * std::sin(rad);
  }

  static float RelativeAngleDeg(float x, float y, float cx, float cy)
  {
    float rel = std::atan2(y - cy, x - cx) * kRadToDeg - kStartAngleDeg;
    while (rel < 0.f)
      rel += 360.f;
    while (rel >= 360.f)
      rel -= 360.f;
    return rel;
  }

  static float BlockMidAngleDeg(const BlockRegion& block)
  {
    float a0 = block.angleStartDeg;
    float a1 = block.angleEndDeg;
    if (a1 <= a0)
      a1 += 360.f;
    return (a0 + a1) * 0.5f;
  }

  static float LabelFontSize(int zone)
  {
    switch (zone)
    {
      case 0: return 8.f;
      case 1: return 10.f;
      default: return 11.f;
    }
  }

  static void BuildLineLayout(LineLayout& lines, const Bounds& bounds)
  {
    lines.cx = bounds.MW();
    lines.cy = bounds.MH();
    lines.outerRadius = (std::min)(bounds.W(), bounds.H()) * 0.5f;
    lines.outerBounds = bounds;

    for (int i = 0; i < kSpokeCount; i++)
      lines.spokeAnglesDeg[static_cast<size_t>(i)] = kStartAngleDeg + static_cast<float>(i) * kSliceDeg;

    for (int ring = 1; ring < kZoneCount; ring++)
    {
      lines.zoneRingRadii[static_cast<size_t>(ring - 1)] =
        lines.outerRadius * (static_cast<float>(ring) / static_cast<float>(kZoneCount));
    }

    lines.valid = lines.outerRadius > 0.f;
  }

  static void BuildBlockRegions(std::array<BlockRegion, kBlockCount>& blocks, const LineLayout& lines)
  {
    for (int spoke = 0; spoke < kSpokeCount; spoke++)
    {
      const float a0 = lines.spokeAnglesDeg[static_cast<size_t>(spoke)];
      float a1 = lines.spokeAnglesDeg[static_cast<size_t>((spoke + 1) % kSpokeCount)];

      if (spoke == kSpokeCount - 1)
        a1 += 360.f;

      for (int zone = 0; zone < kZoneCount; zone++)
      {
        BlockRegion& block = blocks[static_cast<size_t>(spoke * kZoneCount + zone)];
        block.spoke = spoke;
        block.zone = zone;
        block.angleStartDeg = a0;
        block.angleEndDeg = a1;
        block.rInner = lines.outerRadius * (static_cast<float>(zone) / static_cast<float>(kZoneCount));
        block.rOuter = lines.outerRadius * (static_cast<float>(zone + 1) / static_cast<float>(kZoneCount));
      }
    }
  }

  static bool IsInsideWheel(const LineLayout& lines, float x, float y)
  {
    if (!lines.valid)
      return false;

    const float dx = x - lines.cx;
    const float dy = y - lines.cy;
    const float distSq = dx * dx + dy * dy;
    const float r = lines.outerRadius;
    const float outerRadiusSq = r * r * 1.0025f;
    return distSq <= outerRadiusSq;
  }

  static int HitTestBlockIndex(const LineLayout& lines, float x, float y)
  {
    if (!IsInsideWheel(lines, x, y))
      return -1;

    const float dx = x - lines.cx;
    const float dy = y - lines.cy;
    const float distSq = dx * dx + dy * dy;
    const float r = lines.outerRadius;

    const float relDeg = RelativeAngleDeg(x, y, lines.cx, lines.cy);
    int spoke = static_cast<int>(relDeg / kSliceDeg);
    spoke = (std::max)(0, (std::min)(kSpokeCount - 1, spoke));

    const float dist = std::sqrt(distSq);
    int zone = 0;
    if (dist > r * (2.f / 3.f))
      zone = 2;
    else if (dist > r * (1.f / 3.f))
      zone = 1;

    return spoke * kZoneCount + zone;
  }
};

} // namespace audioagent::camelot
