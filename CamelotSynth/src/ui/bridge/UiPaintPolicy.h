#pragma once

#include "IGraphics.h"
#include "IGraphicsRepaintPolicy.h"

BEGIN_IGRAPHICS_NAMESPACE

/*
 * UiPaintPolicy — Windows NanoVG repaint contract (see src/ARCHITECTURE.md).
 *
 * Full-plugin repaints run only while IsInteracting() (knob drag, waveform scrub, etc.).
 * Lightweight hovers (CamelotCircle) mark only the control dirty — partial bounds are safe
 * with IGRAPHICS_OPAQUE_CLEAR + SetFullRepaintQuery(IsInteracting).
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

inline bool QueryFullRepaint()
{
  return detail::InteractionDepth() > 0;
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

/** Clears a leaked interaction depth (e.g. missed mouse-up). */
inline void ResetInteractionDepth()
{
  detail::InteractionDepth() = 0;
}

inline bool IsInteracting()
{
  return detail::InteractionDepth() > 0;
}

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

/** Full-plugin repaint — use for drag/scrub controls only. */
inline void RequestFullRepaint(IGraphics* pGraphics)
{
  if (!pGraphics)
    return;

  pGraphics->SetAllControlsDirty();
  CoalesceFullSurface(pGraphics);
}

/** Local control update — use for hover highlights (CamelotCircle). */
inline void RequestControlRepaint(IControl* pControl)
{
  if (!pControl)
    return;

  pControl->SetDirty(false);
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
  SetFullRepaintQuery(detail::QueryFullRepaint);

  pGraphics->SetDisplayTickFunc([pGraphics]() {
    bool anyDirty = false;

    pGraphics->ForAllControlsFunc([&](IControl* pControl) {
      if (pControl->IsDirty())
        anyDirty = true;
    });

    if (!anyDirty)
      return;

    if (IsInteracting())
    {
      pGraphics->SetAllControlsDirty();
      CoalesceFullSurface(pGraphics);
    }
    else
    {
      CoalesceFullSurface(pGraphics);
    }
  });
}

inline void ForceInitialFullPaint(IGraphics* pGraphics)
{
  if (!pGraphics)
    return;

  RequestFullRepaint(pGraphics);
}

END_IGRAPHICS_NAMESPACE
