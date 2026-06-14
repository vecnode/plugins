#pragma once

#include "PitchStreamCache.h"
#include "PitchStreamWorker.h"
#include "SampleBuffer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace audioagent
{

/**
 * Block-based audioFlux pitch pipeline — ~10 s offline blocks + background read-ahead.
 *
 * Audio thread reads pitched cache when ready; worker converts the next blocks while playing.
 */
class PitchStreamPipeline
{
public:
  static constexpr int kOriginFade = 128;

  void Reset()
  {
    mWorker.Stop();
    mCache.Reset();
    mDryLeft.clear();
    mDryRight.clear();
    mSampleRate = 0;
    mSemitones = 0;
    mGeneration = 0;
    mPlayheadSample = 0;
  }

  void BindDryBuffer(const SampleBuffer& buffer)
  {
    mWorker.Stop();
    mCache.Reset();

    if (!buffer.IsLoaded() || buffer.GetLength() <= 0)
    {
      mDryLeft.clear();
      mDryRight.clear();
      mSampleRate = 0;
      return;
    }

    const int length = buffer.GetLength();
    mSampleRate = static_cast<int>(std::lround(buffer.GetHostSampleRate()));
    mDryLeft.resize(static_cast<size_t>(length));
    mDryRight.resize(static_cast<size_t>(length));

    const sample* left = buffer.GetLeft();
    const sample* right = buffer.GetRight();
    for (int i = 0; i < length; ++i)
    {
      mDryLeft[static_cast<size_t>(i)] = static_cast<float>(left[i]);
      mDryRight[static_cast<size_t>(i)] = static_cast<float>(right[i]);
    }

    mCache.Prepare(length, mSampleRate);
    mWorker.SetCache(&mCache);
  }

  void BeginStream(int originSample, int semitones)
  {
    if (mDryLeft.empty() || semitones == 0)
      return;

    ++mGeneration;
    mSemitones = semitones;
    mPlayheadSample = (std::max)(0, originSample);

    mCache.BeginStream(mPlayheadSample, mGeneration);
    mWorker.BeginStream(mDryLeft,
                        mDryRight,
                        mSampleRate,
                        mPlayheadSample,
                        mSemitones,
                        mGeneration);
  }

  void EndStream()
  {
    mWorker.EndStream();
    mCache.EndStream();
    mSemitones = 0;
  }

  void SetPlayheadSample(int sampleIndex)
  {
    mPlayheadSample = sampleIndex;
  }

  void KickScheduler()
  {
    if (!mCache.IsActive())
      return;

    mWorker.SetPlayhead(mPlayheadSample);
  }

  bool IsActive() const { return mCache.IsActive(); }

  int GetSemitones() const { return mSemitones; }

  bool IsCatchingUp() const
  {
    if (!mCache.IsActive())
      return false;

    const int playBlock = mCache.BlockStart(mPlayheadSample);
    return !mCache.IsBlockReady(playBlock);
  }

  bool IsWorkerBusy() const { return mWorker.IsBusy(); }

  void ReadStereo(double position,
                  const sample* dryLeft,
                  const sample* dryRight,
                  int length,
                  sample& outL,
                  sample& outR) const
  {
    if (!dryLeft || length <= 0)
    {
      outL = 0.f;
      outR = 0.f;
      return;
    }

    const sample* dryR = dryRight ? dryRight : dryLeft;
    const sample dryLVal = ReadDryChannel(dryLeft, length, position);
    const sample dryRVal = ReadDryChannel(dryR, length, position);

    if (!mCache.IsActive())
    {
      outL = dryLVal;
      outR = dryRVal;
      return;
    }

    const int idx = static_cast<int>(position);
    const int origin = mCache.GetStreamOrigin();

    if (idx < origin || !mCache.IsSampleReady(idx))
    {
      outL = dryLVal;
      outR = dryRVal;
      return;
    }

    float pitchL = 0.f;
    float pitchR = 0.f;
    mCache.ReadPitched(position, pitchL, pitchR);

    float wet = 1.f;
    if (idx - origin < kOriginFade)
    {
      wet = static_cast<float>(idx - origin) / static_cast<float>(kOriginFade);
      wet = (std::max)(0.f, (std::min)(1.f, wet));
    }

    outL = static_cast<sample>(dryLVal * (1.f - wet) + static_cast<sample>(pitchL) * wet);
    outR = static_cast<sample>(dryRVal * (1.f - wet) + static_cast<sample>(pitchR) * wet);
  }

private:
  static sample ReadDryChannel(const sample* channel, int length, double position)
  {
    if (!channel || length <= 1)
      return channel ? channel[0] : 0.f;

    position = (std::max)(0., (std::min)(position, static_cast<double>(length - 1)));
    const int i0 = static_cast<int>(position);
    const int i1 = (std::min)(i0 + 1, length - 1);
    const float frac = static_cast<float>(position - static_cast<double>(i0));
    return channel[i0] * (1.f - frac) + channel[i1] * frac;
  }

  PitchStreamCache mCache;
  PitchStreamWorker mWorker;

  std::vector<float> mDryLeft;
  std::vector<float> mDryRight;
  int mSampleRate = 0;
  int mSemitones = 0;
  uint32_t mGeneration = 0;
  int mPlayheadSample = 0;
};

} // namespace audioagent
