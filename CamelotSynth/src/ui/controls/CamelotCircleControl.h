#pragma once

#include "IControls.h"
#include "UiPaintPolicy.h"
#include <algorithm>
#include <array>
#include <cmath>

BEGIN_IGRAPHICS_NAMESPACE

/**
 * CamelotCircle — 12×3 button grid bounded by spoke lines and zone rings.
 *
 * LineLayout defines the grid; BlockRegion cells are derived from it.
 * Hit-testing and highlight fills share the same PointAtAngle / AngularContains math
 * (do not use PathArc — iPlug PathArc applies a -90° offset that misaligns fills).
 */
class CamelotCircle : public IControl
{
public:
  static constexpr int kSpokeCount = 12;
  static constexpr int kZoneCount = 3;
  static constexpr int kBlockCount = kSpokeCount * kZoneCount;
  static constexpr int kArcSegments = 24;
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
    IRECT outerBounds;
    std::array<float, kSpokeCount> spokeAnglesDeg {};
    std::array<float, kZoneCount - 1> zoneRingRadii {};
    bool valid = false;
  };

  CamelotCircle(const IRECT& bounds,
                const IColor& blockColor,
                const IColor& hoverColor,
                const IColor& pressedColor,
                const IColor& lineColor,
                float lineThickness = 2.5f)
  : IControl(bounds)
  , mBlockColor(blockColor)
  , mHoverColor(hoverColor)
  , mPressedColor(pressedColor)
  , mLineColor(lineColor)
  , mLineThickness(lineThickness)
  {
    mIgnoreMouse = false;
    RebuildFromBounds();
  }

  void OnResize() override
  {
    RebuildFromBounds();
    SetDirty(false);
  }

  const LineLayout& GetLineLayout() const { return mLines; }

  const BlockRegion& GetBlock(int spoke, int zone) const
  {
    return mBlocks.at(static_cast<size_t>(spoke * kZoneCount + zone));
  }

  const std::array<BlockRegion, kBlockCount>& GetBlocks() const { return mBlocks; }

  void Draw(IGraphics& g) override
  {
    if (!mLines.valid)
      RebuildFromBounds();

    g.FillEllipse(mBlockColor, mLines.outerBounds);

    if (mHoverIndex >= 0 && mHoverIndex != mPressedIndex)
      FillBlockRegion(g, mBlocks[static_cast<size_t>(mHoverIndex)], mHoverColor);

    if (mPressedIndex >= 0)
      FillBlockRegion(g, mBlocks[static_cast<size_t>(mPressedIndex)], mPressedColor);

    DrawGridLines(g);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    const int hit = HitTestBlockIndex(x, y);
    if (hit < 0)
      return;

    mPressedIndex = hit;
    RequestControlRepaint(this);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    if (mPressedIndex >= 0)
    {
      mPressedIndex = -1;
      RequestControlRepaint(this);
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    if (mPressedIndex >= 0 && !mod.L)
    {
      mPressedIndex = -1;
      RequestControlRepaint(this);
    }

    SetHoverIndex(HitTestBlockIndex(x, y));
    IControl::OnMouseOver(x, y, mod);
  }

  void OnMouseOut() override
  {
    if (mPressedIndex >= 0)
    {
      mPressedIndex = -1;
      ResetInteractionDepth();
    }

    SetHoverIndex(-1);
    IControl::OnMouseOut();
  }

  bool IsHit(float x, float y) const override
  {
    return HitTestBlockIndex(x, y) >= 0;
  }

private:
  /** Same polar→XY as spoke lines (standard math degrees, -90° = 12 o'clock). */
  static void PointAtAngle(float cx, float cy, float radius, float angleDeg, float& x, float& y)
  {
    const float rad = angleDeg * kDegToRad;
    x = cx + radius * std::cos(rad);
    y = cy + radius * std::sin(rad);
  }

  /** Pointer angle relative to kStartAngleDeg, in [0, 360). */
  static float RelativeAngleDeg(float x, float y, float cx, float cy)
  {
    float rel = std::atan2(y - cy, x - cx) * kRadToDeg - kStartAngleDeg;
    while (rel < 0.f)
      rel += 360.f;
    while (rel >= 360.f)
      rel -= 360.f;
    return rel;
  }

  static void NormalizeRelSpan(float angleStartDeg, float angleEndDeg, float& startRel, float& endRel)
  {
    startRel = angleStartDeg - kStartAngleDeg;
    endRel = angleEndDeg - kStartAngleDeg;
    while (startRel < 0.f)
      startRel += 360.f;
    while (endRel < 0.f)
      endRel += 360.f;
    while (startRel >= 360.f)
      startRel -= 360.f;
    while (endRel >= 360.f)
      endRel -= 360.f;
  }

  static bool AngularContains(float relDeg, float angleStartDeg, float angleEndDeg)
  {
    float startRel = 0.f;
    float endRel = 0.f;
    NormalizeRelSpan(angleStartDeg, angleEndDeg, startRel, endRel);

    if (endRel > startRel)
      return relDeg >= startRel && relDeg < endRel;

    if (endRel < startRel)
      return relDeg >= startRel || relDeg < endRel;

    return false;
  }

  static bool BlockContainsPoint(const BlockRegion& block, float x, float y, float cx, float cy)
  {
    const float dx = x - cx;
    const float dy = y - cy;
    const float distSq = dx * dx + dy * dy;

    if (distSq < block.rInner * block.rInner || distSq > block.rOuter * block.rOuter)
      return false;

    return AngularContains(RelativeAngleDeg(x, y, cx, cy), block.angleStartDeg, block.angleEndDeg);
  }

  void RebuildFromBounds()
  {
    BuildLineLayout(mLines, mRECT);
    BuildBlockRegions(mBlocks, mLines);
  }

  static void BuildLineLayout(LineLayout& lines, const IRECT& bounds)
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

  int HitTestBlockIndex(float x, float y) const
  {
    if (!mLines.valid)
      return -1;

    const float dx = x - mLines.cx;
    const float dy = y - mLines.cy;
    const float distSq = dx * dx + dy * dy;
    const float r = mLines.outerRadius;

    if (distSq > r * r)
      return -1;

    const float relDeg = RelativeAngleDeg(x, y, mLines.cx, mLines.cy);
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

  void SetHoverIndex(int index)
  {
    if (index == mHoverIndex)
      return;

    mHoverIndex = index;
    RequestControlRepaint(this);
  }

  void DrawGridLines(IGraphics& g) const
  {
    for (int ring = 1; ring < kZoneCount; ring++)
    {
      const float r = mLines.zoneRingRadii[static_cast<size_t>(ring - 1)];
      const IRECT ringBounds(mLines.cx - r, mLines.cy - r, mLines.cx + r, mLines.cy + r);
      g.DrawEllipse(mLineColor, ringBounds, nullptr, mLineThickness);
    }

    for (int i = 0; i < kSpokeCount; i++)
    {
      float x2 = 0.f;
      float y2 = 0.f;
      PointAtAngle(mLines.cx, mLines.cy, mLines.outerRadius, mLines.spokeAnglesDeg[static_cast<size_t>(i)], x2, y2);
      g.DrawLine(mLineColor, mLines.cx, mLines.cy, x2, y2, nullptr, mLineThickness);
    }

    g.DrawEllipse(mLineColor, mLines.outerBounds, nullptr, mLineThickness);
  }

  /** Tessellated annular sector — same angles/radii as BlockContainsPoint. */
  void FillBlockRegion(IGraphics& g, const BlockRegion& block, const IColor& fill) const
  {
    float a0 = block.angleStartDeg;
    float a1 = block.angleEndDeg;
    if (a1 <= a0)
      a1 += 360.f;

    const float sweep = a1 - a0;
    g.PathClear();

    if (block.rInner <= 0.001f)
    {
      g.PathMoveTo(mLines.cx, mLines.cy);
      for (int i = 0; i <= kArcSegments; i++)
      {
        const float t = a0 + sweep * (static_cast<float>(i) / static_cast<float>(kArcSegments));
        float x = 0.f;
        float y = 0.f;
        PointAtAngle(mLines.cx, mLines.cy, block.rOuter, t, x, y);
        g.PathLineTo(x, y);
      }
      g.PathClose();
    }
    else
    {
      for (int i = 0; i <= kArcSegments; i++)
      {
        const float t = a0 + sweep * (static_cast<float>(i) / static_cast<float>(kArcSegments));
        float x = 0.f;
        float y = 0.f;
        PointAtAngle(mLines.cx, mLines.cy, block.rOuter, t, x, y);
        if (i == 0)
          g.PathMoveTo(x, y);
        else
          g.PathLineTo(x, y);
      }

      for (int i = kArcSegments; i >= 0; i--)
      {
        const float t = a0 + sweep * (static_cast<float>(i) / static_cast<float>(kArcSegments));
        float x = 0.f;
        float y = 0.f;
        PointAtAngle(mLines.cx, mLines.cy, block.rInner, t, x, y);
        g.PathLineTo(x, y);
      }

      g.PathClose();
    }

    g.PathFill(fill);
  }

  IColor mBlockColor;
  IColor mHoverColor;
  IColor mPressedColor;
  IColor mLineColor;
  float mLineThickness;

  LineLayout mLines;
  std::array<BlockRegion, kBlockCount> mBlocks {};
  int mHoverIndex = -1;
  int mPressedIndex = -1;
};

END_IGRAPHICS_NAMESPACE
