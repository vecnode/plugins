#pragma once

#include "IGraphics.h"

BEGIN_IGRAPHICS_NAMESPACE

/*
 * Windows NanoVG paint policy
 *
 * BeginFrame() clears the GL framebuffer to transparent. EndFrame() composites
 * the entire buffer to the HWND. Any pixel not redrawn shows the previous frame.
 *
 * Policy: whenever any control is dirty, also mark the full-window background
 * dirty so IsDirty() invalidates the entire plugin bounds — one opaque frame.
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

/** Expand the next paint to the full plugin bounds. */
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

inline void RequestFullRepaint(IGraphics* pGraphics)
{
  CoalesceFullSurface(pGraphics);
}

inline void AttachAndRegisterFullWindowBackground(IGraphics* pGraphics, const IColor& color)
{
  if (!pGraphics)
    return;

  pGraphics->AttachPanelBackground(color);
  RegisterFullWindowBackground(pGraphics->GetControl(0));
}

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

    if (anyDirty)
      CoalesceFullSurface(pGraphics);
  });
}

/** Call at end of editor layout — avoids host-colour bleed before first click. */
inline void ForceInitialFullPaint(IGraphics* pGraphics)
{
  if (!pGraphics)
    return;

  pGraphics->SetAllControlsDirty();
  CoalesceFullSurface(pGraphics);
}

END_IGRAPHICS_NAMESPACE
