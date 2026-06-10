#pragma once

#include "IControls.h"
#include "UiRedraw.h"

BEGIN_IGRAPHICS_NAMESPACE

/** Gain knob that triggers full-window repaint on drag (avoids partial FBO trails). */
class GainKnobControl : public IVKnobControl
{
public:
  GainKnobControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style)
  : IVKnobControl(bounds, paramIdx, label, style)
  {
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    IVKnobControl::OnMouseDown(x, y, mod);
    RequestFullRepaint(GetUI());
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    double gearing = IsFineControl(mod, false) ? mGearing * 10.0 : mGearing;

    IRECT dragBounds = GetKnobDragBounds();

    if (mDirection == EDirection::Vertical)
      mMouseDragValue += static_cast<double>(dY / static_cast<double>(dragBounds.T - dragBounds.B) / gearing);
    else
      mMouseDragValue += static_cast<double>(dX / static_cast<double>(dragBounds.R - dragBounds.L) / gearing);

    mMouseDragValue = Clip(mMouseDragValue, 0., 1.);

    double v = mMouseDragValue;
    if (const IParam* pParam = GetParam())
    {
      if (pParam->GetStepped() && pParam->GetStep() > 0)
        v = pParam->ConstrainNormalized(mMouseDragValue);
    }

    SetValue(v);
    SetDirty(false);
    RequestFullRepaint(GetUI());
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    IVKnobControl::OnMouseUp(x, y, mod);
    RequestFullRepaint(GetUI());
  }
};

END_IGRAPHICS_NAMESPACE
