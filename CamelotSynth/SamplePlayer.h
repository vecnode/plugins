#pragma once

#include "SampleBuffer.h"
#include <atomic>

BEGIN_IPLUG_NAMESPACE

/** One-shot sample playback from a decoded SampleBuffer. */
class SamplePlayer
{
public:
  void SetBuffer(const SampleBuffer& buffer)
  {
    mLeft = buffer.GetLeft();
    mRight = buffer.GetRight();
    mLength = buffer.GetLength();
    mPlayhead.store(0);
    mPlaying.store(false);
    mTriggerPending.store(false);
  }

  void RequestTrigger()
  {
    mTriggerPending.store(true);
  }

  void RequestStop()
  {
    mTriggerPending.store(false);
    mPlaying.store(false);
    mPlayhead.store(0);
  }

  void ProcessBlock(sample** outputs, int nOutputs, int nFrames, sample gain)
  {
    if (mTriggerPending.exchange(false))
    {
      mPlayhead.store(0);
      mPlaying.store(true);
    }

    if (!mPlaying.load() || !mLeft || mLength <= 0)
      return;

    int playhead = mPlayhead.load();

    for (int s = 0; s < nFrames; s++)
    {
      if (playhead >= mLength)
      {
        mPlaying.store(false);
        break;
      }

      const sample l = mLeft[playhead] * gain;
      const sample r = (mRight ? mRight[playhead] : l) * gain;

      if (nOutputs > 0)
        outputs[0][s] = l;
      if (nOutputs > 1)
        outputs[1][s] = r;

      playhead++;
    }

    mPlayhead.store(playhead);
  }

  bool IsPlaying() const { return mPlaying.load(); }

private:
  const sample* mLeft = nullptr;
  const sample* mRight = nullptr;
  int mLength = 0;
  std::atomic<int> mPlayhead {0};
  std::atomic<bool> mPlaying {false};
  std::atomic<bool> mTriggerPending {false};
};

END_IPLUG_NAMESPACE
