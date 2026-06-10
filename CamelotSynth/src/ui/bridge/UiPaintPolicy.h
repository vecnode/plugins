#pragma once

#include "IGraphics.h"

BEGIN_IGRAPHICS_NAMESPACE

/*
 * Windows NanoVG paint policy
 *
 * Each frame: BeginFrame clears the FBO, only the WM_PAINT update region is drawn,
 * EndFrame composites the entire buffer to the HWND. Partial regions leave stale
 * pixels (trails during knob drag, playhead scrub, etc.).
 *
 * Policy:
 *  - SetStrictDrawing(true) + collapse invalidation to full bounds (IGraphics.cpp)
 *  - IGRAPHICS_OPAQUE_CLEAR on BeginFrame (CMakeLists.txt)
 *  - While interacting or any control dirty: mark the full surface dirty
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

/** Mark the full-window background dirty (control 0). */
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

/** Request one full-plugin paint on the next display tick. */
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

/** Call at end of editor layout — avoids host-colour bleed before first click. */
inline void ForceInitialFullPaint(IGraphics* pGraphics)
{
  if (!pGraphics)
    return;

  RequestFullRepaint(pGraphics);
}

END_IGRAPHICS_NAMESPACE
