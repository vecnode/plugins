#pragma once

#include "IControls.h"
#include "UiPaintPolicy.h"
#include "camelot/WheelLayout.h"
#include <array>

BEGIN_IGRAPHICS_NAMESPACE

using WheelLayout = audioagent::camelot::WheelLayout;
using WheelBlockRegion = audioagent::camelot::WheelLayout::BlockRegion;
using WheelLineLayout = audioagent::camelot::WheelLayout::LineLayout;

/**
 * CamelotCircle — 12×3 button grid (B1–B36) bounded by spoke lines and zone rings.
 * Geometry and hit-testing delegate to audioagent::camelot::WheelLayout.
 */
class CamelotCircle : public IControl
{
public:
  static constexpr int kSpokeCount = WheelLayout::kSpokeCount;
  static constexpr int kZoneCount = WheelLayout::kZoneCount;
  static constexpr int kBlockCount = WheelLayout::kBlockCount;
  static constexpr int kArcSegments = 24;

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

  const WheelLineLayout& GetLineLayout() const { return mLines; }

  const WheelBlockRegion& GetBlock(int spoke, int zone) const
  {
    return mBlocks.at(static_cast<size_t>(spoke * kZoneCount + zone));
  }

  const std::array<WheelBlockRegion, kBlockCount>& GetBlocks() const { return mBlocks; }

  int GetSelectedBlockIndex() const { return mSelectedIndex; }

  void Draw(IGraphics& g) override
  {
    if (!mLines.valid)
      RebuildFromBounds();

    g.FillEllipse(mBlockColor, ToIrect(mLines.outerBounds));

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
    return WheelLayout::IsInsideWheel(mLines, x, y);
  }

private:
  static audioagent::camelot::Bounds ToBounds(const IRECT& r)
  {
    return audioagent::camelot::Bounds::From(r.L, r.T, r.R, r.B);
  }

  static IRECT ToIrect(const audioagent::camelot::Bounds& b)
  {
    return IRECT(b.L, b.T, b.R, b.B);
  }

  void RebuildFromBounds()
  {
    WheelLayout::BuildLineLayout(mLines, ToBounds(mDrawRECT));
    WheelLayout::BuildBlockRegions(mBlocks, mLines);

    for (int i = 0; i < kBlockCount; i++)
      mBlockLabels[static_cast<size_t>(i)].SetFormatted(8, "B%d", i + 1);
  }

  int BlockIndexAt(float x, float y) const
  {
    return WheelLayout::HitTestBlockIndex(mLines, x, y);
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
      WheelLayout::PointAtAngle(mLines.cx, mLines.cy, mLines.outerRadius, mLines.spokeAnglesDeg[static_cast<size_t>(i)], x2, y2);
      g.DrawLine(mLineColor, mLines.cx, mLines.cy, x2, y2, nullptr, mLineThickness);
    }

    g.DrawEllipse(mLineColor, ToIrect(mLines.outerBounds), nullptr, mLineThickness);
  }

  void DrawBlockLabels(IGraphics& g) const
  {
    const IColor labelColor(255, 255, 255);

    for (const WheelBlockRegion& block : mBlocks)
    {
      const float midAngle = WheelLayout::BlockMidAngleDeg(block);
      const float midRadius = (block.rInner + block.rOuter) * 0.5f;
      float lx = 0.f;
      float ly = 0.f;
      WheelLayout::PointAtAngle(mLines.cx, mLines.cy, midRadius, midAngle, lx, ly);

      const IText labelStyle(WheelLayout::LabelFontSize(block.zone), labelColor, "Roboto-Regular",
                               EAlign::Center, EVAlign::Middle);
      const IRECT labelBounds(lx - 18.f, ly - 8.f, lx + 18.f, ly + 8.f);
      const int idx = block.FlatIndex();

      g.DrawText(labelStyle, mBlockLabels[static_cast<size_t>(idx)].Get(), labelBounds);
    }
  }

  void FillBlockRegion(IGraphics& g, const WheelBlockRegion& block, const IColor& fill) const
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
        WheelLayout::PointAtAngle(mLines.cx, mLines.cy, block.rOuter, t, x, y);
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
        WheelLayout::PointAtAngle(mLines.cx, mLines.cy, block.rOuter, t, x, y);
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
        WheelLayout::PointAtAngle(mLines.cx, mLines.cy, block.rInner, t, x, y);
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

  WheelLineLayout mLines;
  std::array<WheelBlockRegion, kBlockCount> mBlocks {};
  std::array<WDL_String, kBlockCount> mBlockLabels {};
  int mActiveIndex = -1;
  int mSelectedIndex = -1;
  bool mPointerDown = false;
};

END_IGRAPHICS_NAMESPACE
