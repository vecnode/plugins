#pragma once

#include "IControls.h"
#include "UiPaintPolicy.h"
#include <algorithm>
#include <array>
#include <cmath>

BEGIN_IGRAPHICS_NAMESPACE

/**
 * CamelotCircle — 12×3 button grid (B1–B36) bounded by spoke lines and zone rings.
 *
 * Pointer contract:
 * - Capture rect is the parent panel (e.g. middle); wheel geometry lives in drawBounds.
 * - While the left button is held, active highlight follows the block under the pointer.
 * - On mouse-up, blue selected highlight stays on the last block under the pointer.
 * - No hover highlight when the button is up.
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

  CamelotCircle(const IRECT& captureBounds,
                const IRECT& drawBounds,
                const IColor& blockColor,
                const IColor& activeColor,
                const IColor& selectedColor,
                const IColor& lineColor,
                float lineThickness = 2.5f)
  : IControl(captureBounds)
  , mDrawRECT(drawBounds)
  , mBlockColor(blockColor)
  , mActiveColor(activeColor)
  , mSelectedColor(selectedColor)
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

  int GetSelectedBlockIndex() const { return mSelectedIndex; }

  void Draw(IGraphics& g) override
  {
    if (!mLines.valid)
      RebuildFromBounds();

    g.FillEllipse(mBlockColor, mLines.outerBounds);

    if (mSelectedIndex >= 0)
      FillBlockRegion(g, mBlocks[static_cast<size_t>(mSelectedIndex)], mSelectedColor);

    if (mActiveIndex >= 0 && mActiveIndex != mSelectedIndex)
      FillBlockRegion(g, mBlocks[static_cast<size_t>(mActiveIndex)], mActiveColor);

    DrawGridLines(g);
    DrawBlockLabels(g);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (!mod.L)
      return;

    mPointerDown = true;
    UpdateActiveIndex(BlockIndexAt(x, y));
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    if (!mod.L)
    {
      ClearActiveState();
      return;
    }

    mPointerDown = true;
    UpdateActiveIndex(BlockIndexAt(x, y));
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    const int releasedBlock = BlockIndexAt(x, y);
    ClearActiveState();

    if (releasedBlock >= 0)
      SetSelectedIndex(releasedBlock);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    if (mod.L)
    {
      mPointerDown = true;
      UpdateActiveIndex(BlockIndexAt(x, y));
    }
    else if (mPointerDown || mActiveIndex >= 0)
    {
      ClearActiveState();
    }

    IControl::OnMouseOver(x, y, mod);
  }

  void OnMouseOut() override
  {
    ClearActiveState();
    IControl::OnMouseOut();
  }

  void OnTouchCancelled(float x, float y, const IMouseMod& mod) override
  {
    ClearActiveState();
  }

  bool IsHit(float x, float y) const override
  {
    return IsInsideWheel(x, y);
  }

private:
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

  void RebuildFromBounds()
  {
    BuildLineLayout(mLines, mDrawRECT);
    BuildBlockRegions(mBlocks, mLines);

    for (int i = 0; i < kBlockCount; i++)
      mBlockLabels[static_cast<size_t>(i)].SetFormatted(8, "B%d", i + 1);
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

  bool IsInsideWheel(float x, float y) const
  {
    if (!mLines.valid)
      return false;

    const float dx = x - mLines.cx;
    const float dy = y - mLines.cy;
    const float distSq = dx * dx + dy * dy;
    const float r = mLines.outerRadius;
    const float outerRadiusSq = r * r * 1.0025f;
    return distSq <= outerRadiusSq;
  }

  int HitTestBlockIndex(float x, float y) const
  {
    if (!IsInsideWheel(x, y))
      return -1;

    const float dx = x - mLines.cx;
    const float dy = y - mLines.cy;
    const float distSq = dx * dx + dy * dy;
    const float r = mLines.outerRadius;

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

  int BlockIndexAt(float x, float y) const
  {
    return HitTestBlockIndex(x, y);
  }

  void ClearActiveState()
  {
    if (!mPointerDown && mActiveIndex < 0)
      return;

    mPointerDown = false;
    mActiveIndex = -1;
    RequestControlRepaint(this);
  }

  void UpdateActiveIndex(int index)
  {
    if (index == mActiveIndex)
      return;

    mActiveIndex = index;
    RequestControlRepaint(this);
  }

  void SetSelectedIndex(int index)
  {
    if (index == mSelectedIndex)
      return;

    mSelectedIndex = index;
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

  void DrawBlockLabels(IGraphics& g) const
  {
    const IColor labelColor(225, 228, 235);

    for (const BlockRegion& block : mBlocks)
    {
      const float midAngle = BlockMidAngleDeg(block);
      const float midRadius = (block.rInner + block.rOuter) * 0.5f;
      float lx = 0.f;
      float ly = 0.f;
      PointAtAngle(mLines.cx, mLines.cy, midRadius, midAngle, lx, ly);

      const IText labelStyle(LabelFontSize(block.zone), labelColor, "Roboto-Regular",
                               EAlign::Center, EVAlign::Middle);
      const IRECT labelBounds(lx - 18.f, ly - 8.f, lx + 18.f, ly + 8.f);
      const int idx = block.FlatIndex();

      g.DrawText(labelStyle, mBlockLabels[static_cast<size_t>(idx)].Get(), labelBounds);
    }
  }

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

  IRECT mDrawRECT;
  IColor mBlockColor;
  IColor mActiveColor;
  IColor mSelectedColor;
  IColor mLineColor;
  float mLineThickness;

  LineLayout mLines;
  std::array<BlockRegion, kBlockCount> mBlocks {};
  std::array<WDL_String, kBlockCount> mBlockLabels {};
  int mActiveIndex = -1;
  int mSelectedIndex = -1;
  bool mPointerDown = false;
};

END_IGRAPHICS_NAMESPACE
