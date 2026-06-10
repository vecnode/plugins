#pragma once

#include "IGraphics.h"

BEGIN_IGRAPHICS_NAMESPACE

/*
 * Single-pass full-window redraw policy (Windows + NanoVG)
 *
 * PROBLEM
 * -------
 * Each paint: BeginFrame() clears the GL framebuffer to transparent, only the
 * WM_PAINT update region is drawn, then EndFrame() composites the *entire*
 * framebuffer to the HWND. Transparent pixels show the previous frame — trails
 * on the knob value, playhead, meter, etc. Worse during drag: mouse events mark
 * one control dirty immediately, but the 60 Hz paint timer may still composite
 * a smaller region over the old HWND pixels.
 *
 * FIX
 * ---
 * 1. Register control 0 (AttachPanelBackground) as the full-window anchor.
 * 2. Every display tick, mark it dirty *before* IsDirty() collects regions so
 *    the invalidation region is always the full plugin bounds.
 * 3. SetStrictDrawing(true) — one Draw() pass per frame.
 * 4. Interactive controls call RequestFullRepaint() on drag (same message).
 * 5. No transparent layout panels; no ILayer caching on moving overlays.
 */

namespace detail
{
inline IControl*& RegisteredBackground()
{
  static IControl* sBackground = nullptr;
  return sBackground;
}
} // namespace detail

inline void RegisterFullWindowBackground(IControl* pBackground)
{
  detail::RegisteredBackground() = pBackground;
}

inline void CoalesceRedraw(IGraphics* pGraphics)
{
  if (!pGraphics)
    return;

  IControl* pBG = detail::RegisteredBackground();
  if (!pBG && pGraphics->NControls() > 0)
    pBG = pGraphics->GetControl(0);

  if (pBG)
    pBG->SetDirty(false);
}

inline void RequestFullRepaint(IGraphics* pGraphics)
{
  CoalesceRedraw(pGraphics);
}

inline void AttachAndRegisterFullWindowBackground(IGraphics* pGraphics, const IColor& color)
{
  if (!pGraphics)
    return;

  pGraphics->AttachPanelBackground(color);
  RegisterFullWindowBackground(pGraphics->GetControl(0));
}

inline void InstallSinglePassRedraw(IGraphics* pGraphics)
{
  if (!pGraphics)
    return;

  pGraphics->SetStrictDrawing(true);

  // Always expand to full window before IsDirty() gathers partial control rects.
  pGraphics->SetDisplayTickFunc([pGraphics]() {
    CoalesceRedraw(pGraphics);
  });
}

END_IGRAPHICS_NAMESPACE
