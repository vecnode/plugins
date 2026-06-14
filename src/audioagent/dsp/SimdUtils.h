#pragma once

#include <cmath>

namespace audioagent
{

/** SIMD hooks — scalar fallbacks today; replace inner loops when targeting AVX/NEON. */
struct SimdUtils
{
  static void ScaleStereoBlock(sample* left, sample* right, sample gain, int numFrames)
  {
    for (int i = 0; i < numFrames; ++i)
    {
      left[i] *= gain;
      right[i] *= gain;
    }
  }

  static void FlushDenormalsBlock(sample* left, sample* right, int numFrames)
  {
    constexpr sample kEps = 1.e-15f;
    for (int i = 0; i < numFrames; ++i)
    {
      if (std::fabs(left[i]) < kEps)
        left[i] = 0.f;
      if (std::fabs(right[i]) < kEps)
        right[i] = 0.f;
    }
  }
};

} // namespace audioagent
