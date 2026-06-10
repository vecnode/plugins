#pragma once

#include "IGraphics.h"

BEGIN_IGRAPHICS_NAMESPACE

/*
 * UiPaintPolicy — Windows NanoVG full-surface repaint contract
 *
 * Context (see src/ARCHITECTURE.md § Windows rendering):
 *   NanoVG renders to an off-screen FBO, then composites the full texture in EndFrame().
 *   Per-control dirty rectangles cause partial WM_PAINT regions; without a full FBO clear
 *   and full-surface redraw, undrawn pixels retain the previous frame (playhead trails,
 *   knob label smear, meter LED ghosts).
 *
 * Required stack (all three):
 *   1. IGRAPHICS_OPAQUE_CLEAR in CMake — FBO cleared after bind in IGraphicsNanoVG::BeginFrame
 *   2. SetStrictDrawing(true) — IsDirty() and Draw() use full GetBounds() (IGraphics.cpp)
 *   3. This header — coalesce dirty state and keep repainting while the user is dragging
 *
 * Interactive control checklist:
 *   - Call BeginInteraction() on mouse down, EndInteraction() on mouse up
 *   - After updating visuals: SetDirty(false) on self, then RequestFullRepaint(GetUI())
 *   - Do not shrink mRECT on move (overlays); redraw the full plot/control area instead
 *   - Avoid ILayer caches for content under moving overlays; draw directly each frame
 */

namespace detail
{
inline IControl*& RegisteredBackground()
{
  static IControl* sBackground = nullptr;
  return sBackground;
}

inline int& InteractionDepth()
{
  static int sDepth = 0;
  return sDepth;
}
} // namespace detail

inline void RegisterFullWindowBackground(IControl* pBackground)
{
  detail::RegisteredBackground() = pBackground;
}

/** Increments drag/scrub depth; display tick keeps full repaints active while > 0. */
inline void BeginInteraction()
{
  ++detail::InteractionDepth();
}

inline void EndInteraction()
{
  if (detail::InteractionDepth() > 0)
    --detail::InteractionDepth();
}

inline bool IsInteracting()
{
  return detail::InteractionDepth() > 0;
}

/** Marks control 0 (full-window IPanelBackground) dirty for invalidation coalescing. */
inline void CoalesceFullSurface(IGraphics* pGraphics)
{
  if (!pGraphics)
    return;

  IControl* pBG = detail::RegisteredBackground();
  if (!pBG && pGraphics->NControls() > 0)
    pBG = pGraphics->GetControl(0);

  if (pBG)
    pBG->SetDirty(false);
}

/** Schedules a full-plugin paint on the next graphics timer tick. */
inline void RequestFullRepaint(IGraphics* pGraphics)
{
  if (!pGraphics)
    return;

  pGraphics->SetAllControlsDirty();
  CoalesceFullSurface(pGraphics);
}

inline void AttachAndRegisterFullWindowBackground(IGraphics* pGraphics, const IColor& color)
{
  if (!pGraphics)
    return;

  pGraphics->AttachPanelBackground(color);
  RegisterFullWindowBackground(pGraphics->GetControl(0));
}

/** Install once at end of editor Attach(). Enables strict drawing and display-tick coalescing. */
inline void InstallPaintPolicy(IGraphics* pGraphics)
{
  if (!pGraphics)
    return;

  pGraphics->SetStrictDrawing(true);

  pGraphics->SetDisplayTickFunc([pGraphics]() {
    bool anyDirty = false;

    pGraphics->ForAllControlsFunc([&](IControl* pControl) {
      if (pControl->IsDirty())
        anyDirty = true;
    });

    if (anyDirty || IsInteracting())
    {
      pGraphics->SetAllControlsDirty();
      CoalesceFullSurface(pGraphics);
    }
  });
}

/** Ensures the first visible frame is fully opaque (layout, OnUIOpen, first OnIdle). */
inline void ForceInitialFullPaint(IGraphics* pGraphics)
{
  if (!pGraphics)
    return;

  RequestFullRepaint(pGraphics);
}

END_IGRAPHICS_NAMESPACE
