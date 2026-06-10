#pragma once

#include "SampleBuffer.h"
#include "Smoothers.h"
#include <algorithm>
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
    mPaused.store(false);
    mTriggerPending.store(false);
  }

  void RequestPlay()
  {
    if (mPaused.load())
    {
      mPaused.store(false);
      mPlaying.store(true);
      return;
    }

    mTriggerPending.store(true);
  }

  void RequestPause()
  {
    if (!mPlaying.load())
      return;

    mPlaying.store(false);
    mPaused.store(true);
    mTriggerPending.store(false);
  }

  void RequestStop()
  {
    mTriggerPending.store(false);
    mPlaying.store(false);
    mPaused.store(false);
    mPlayhead.store(0);
  }

  void SeekToNormalized(float norm)
  {
    if (!mLeft || mLength <= 0)
      return;

    norm = (std::max)(0.f, (std::min)(1.f, norm));
    int pos = static_cast<int>(norm * static_cast<float>(mLength));
    if (pos >= mLength)
      pos = mLength - 1;
    if (pos < 0)
      pos = 0;

    mPlayhead.store(pos);

    if (mPlaying.load())
      mPaused.store(false);
    else if (pos > 0)
      mPaused.store(true);
    else
      mPaused.store(false);
  }

  void ProcessBlock(sample** outputs, int nOutputs, int nFrames, sample targetGain, LogParamSmooth<sample, 1>& gainSmoother)
  {
    if (mTriggerPending.exchange(false))
    {
      mPlayhead.store(0);
      mPlaying.store(true);
      mPaused.store(false);
    }

    if (!mPlaying.load() || !mLeft || mLength <= 0)
      return;

    int playhead = mPlayhead.load();

    for (int s = 0; s < nFrames; s++)
    {
      if (playhead >= mLength)
      {
        mPlaying.store(false);
        mPaused.store(false);
        break;
      }

      const sample gain = gainSmoother.Process(targetGain);
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

  float GetPlayheadNorm() const
  {
    if (mLength <= 0)
      return 0.f;

    return static_cast<float>(mPlayhead.load()) / static_cast<float>(mLength);
  }

private:
  const sample* mLeft = nullptr;
  const sample* mRight = nullptr;
  int mLength = 0;
  std::atomic<int> mPlayhead {0};
  std::atomic<bool> mPlaying {false};
  std::atomic<bool> mPaused {false};
  std::atomic<bool> mTriggerPending {false};
};

END_IPLUG_NAMESPACE
