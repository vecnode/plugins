#pragma once

#include "SampleBuffer.h"
#include <algorithm>
#include <vector>

namespace audioagent
{

/** Downsampled min/max envelope used for waveform display. */
class WaveformEnvelope
{
public:
  void BuildFrom(const SampleBuffer& buffer, int numPoints)
  {
    mMax.clear();
    mMin.clear();

    if (!buffer.IsLoaded() || buffer.GetLength() <= 0 || numPoints <= 0)
      return;

    mMax.resize(static_cast<size_t>(numPoints), 0.f);
    mMin.resize(static_cast<size_t>(numPoints), 0.f);

    const int blockSize = (std::max)(1, buffer.GetLength() / numPoints);
    const sample* left = buffer.GetLeft();
    float norm = 0.f;

    for (int i = 0; i < numPoints; i++)
    {
      const int start = i * blockSize;
      const int end = (std::min)(start + blockSize, buffer.GetLength());
      float bucketMax = 0.f;
      float bucketMin = 0.f;

      for (int s = start; s < end; s++)
      {
        const float v = static_cast<float>(left[s]);
        bucketMax = (std::max)(bucketMax, v);
        bucketMin = (std::min)(bucketMin, v);
        norm = (std::max)(norm, std::fabs(v));
      }

      mMax[static_cast<size_t>(i)] = bucketMax;
      mMin[static_cast<size_t>(i)] = bucketMin;
    }

    if (norm > 0.f)
    {
      for (float& v : mMax)
        v /= norm;
      for (float& v : mMin)
        v /= norm;
    }
  }

  bool IsValid() const { return mMax.size() >= 2 && mMax.size() == mMin.size(); }

  const std::vector<float>& Max() const { return mMax; }
  const std::vector<float>& Min() const { return mMin; }

private:
  std::vector<float> mMax;
  std::vector<float> mMin;
};

} // namespace audioagent
