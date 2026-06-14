#pragma once

#include "audioagent/iplug_bridge.h"
#include <cmath>

namespace audioagent
{

/** Flush subnormal floats to zero — call at chain input to avoid FPU spikes. */
inline sample FlushDenormal(sample x)
{
  return (std::fabs(x) < 1.e-15f) ? 0.f : x;
}

inline void FlushDenormalsStereo(sample& left, sample& right)
{
  left = FlushDenormal(left);
  right = FlushDenormal(right);
}

} // namespace audioagent
