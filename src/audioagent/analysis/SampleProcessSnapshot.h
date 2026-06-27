#pragma once

#include "OfflineSampleWorker.h"
#include "dsp/SampleBuffer.h"

#include <cmath>

namespace audioagent
{

/**
 * Builds the immutable float snapshot consumed by OfflineSampleWorker.
 *
 * Threading: call from the UI timer only. Never from ProcessBlock or
 * OnParamChange — copying the full buffer on the audio thread would violate
 * real-time constraints for long samples.
 */
class SampleProcessSnapshot
{
public:
  static void Capture(const SampleBuffer& buffer, OfflineSampleWorker& worker)
  {
    if (!buffer.IsLoaded())
    {
      worker.SetSnapshot({});
      return;
    }

    CaptureChannels(buffer.GetLeft(),
                    buffer.GetRight(),
                    buffer.GetLength(),
                    static_cast<int>(std::lround(buffer.GetHostSampleRate())),
                    worker);
  }

  static void CaptureChannels(const float* left,
                              const float* right,
                              int length,
                              int sampleRate,
                              OfflineSampleWorker& worker)
  {
    if (!left || !right || length <= 0 || sampleRate <= 0)
    {
      worker.SetSnapshot({});
      return;
    }

    OfflineSampleWorker::Snapshot snapshot;
    snapshot.sampleRate = sampleRate;
    snapshot.mono.resize(static_cast<size_t>(length));
    snapshot.left.resize(static_cast<size_t>(length));
    snapshot.right.resize(static_cast<size_t>(length));

    for (int i = 0; i < length; ++i)
    {
      snapshot.left[static_cast<size_t>(i)] = left[i];
      snapshot.right[static_cast<size_t>(i)] = right[i];
      snapshot.mono[static_cast<size_t>(i)] = (left[i] + right[i]) * 0.5f;
    }

    worker.SetSnapshot(std::move(snapshot));
  }

private:
  static void CaptureChannels(const sample* left,
                              const sample* right,
                              int length,
                              int sampleRate,
                              OfflineSampleWorker& worker)
  {
    if (!left || !right || length <= 0 || sampleRate <= 0)
    {
      worker.SetSnapshot({});
      return;
    }

    OfflineSampleWorker::Snapshot snapshot;
    snapshot.sampleRate = sampleRate;
    snapshot.mono.resize(static_cast<size_t>(length));
    snapshot.left.resize(static_cast<size_t>(length));
    snapshot.right.resize(static_cast<size_t>(length));

    for (int i = 0; i < length; ++i)
    {
      snapshot.left[static_cast<size_t>(i)] = static_cast<float>(left[i]);
      snapshot.right[static_cast<size_t>(i)] = static_cast<float>(right[i]);
      snapshot.mono[static_cast<size_t>(i)] = static_cast<float>((left[i] + right[i]) * 0.5);
    }

    worker.SetSnapshot(std::move(snapshot));
  }
};

} // namespace audioagent
