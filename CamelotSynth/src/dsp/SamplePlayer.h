#pragma once

#include "OutputLimiter.h"
#include "SampleBuffer.h"
#include "Smoothers.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

BEGIN_IPLUG_NAMESPACE

/** One-shot sample playback with interpolated reads and dual-playhead seek crossfades. */
class SamplePlayer
{
public:
  enum class ScheduledCommand : uint8_t
  {
    None = 0,
    Play,
    Pause,
    Stop
  };

  void SetSampleRate(double sampleRate)
  {
    mSampleRate = sampleRate > 0. ? sampleRate : 44100.;
    mSeekCrossfadeSamples = static_cast<int>(std::ceil(mSampleRate * kSeekCrossfadeMs * 0.001));
    mSeekCrossfadeSamples = (std::max)(mSeekCrossfadeSamples, 256);
    mTransportFadeSamples = static_cast<int>(std::ceil(mSampleRate * kTransportFadeMs * 0.001));
    mTransportFadeSamples = (std::max)(mTransportFadeSamples, 32);
    mLimiter.SetSampleRate(mSampleRate);
  }

  void SetBuffer(const SampleBuffer& buffer)
  {
    mLeft = buffer.GetLeft();
    mRight = buffer.GetRight();
    mLength = buffer.GetLength();
    mReadHeadFrac = 0.;
    mPlayhead.store(0);
    mPlaying.store(false);
    mPaused.store(false);
    mTriggerPending.store(false);
    mTransportFadeRemaining = 0;
    mTransportFadeTotal = 0;
    mTransportFadeDirection = TransportFadeDirection::None;
    mAfterTransportFade = AfterTransportFade::None;
    mOutputGain = 1.f;
    EndSeekCrossfade();
    mLastOutL = 0.f;
    mLastOutR = 0.f;
    mLimiter.Reset();
    ClearSchedule();
  }

  void ScheduleCommand(ScheduledCommand command, int sampleOffset)
  {
    mScheduledCommand.store(command);
    mScheduledOffset.store(sampleOffset < 0 ? 0 : sampleOffset);
  }

  void RequestPlay()
  {
    if (mPaused.load())
    {
      mPaused.store(false);
      mPlaying.store(true);
      BeginTransportFadeIn();
      return;
    }

    mTriggerPending.store(true);
  }

  void RequestPause()
  {
    if (!mPlaying.load() && mTransportFadeDirection != TransportFadeDirection::Out)
      return;

    CommitSeekCrossfadePosition();
    mAfterTransportFade = AfterTransportFade::Pause;
    mTriggerPending.store(false);
    BeginTransportFadeOut();
  }

  void RequestStop()
  {
    mTriggerPending.store(false);
    mAfterTransportFade = AfterTransportFade::Stop;
    CommitSeekCrossfadePosition();
    BeginTransportFadeOut();
  }

  void SeekToNormalized(float norm)
  {
    if (!mLeft || mLength <= 0)
      return;

    const double pos = NormalizedToPosition(norm);
    mPlayhead.store(static_cast<int>(pos));

    if (ShouldCrossfadeSeek())
    {
      BeginSeekCrossfade(pos);
      return;
    }

    EndSeekCrossfade();
    mReadHeadFrac = pos;
    mPaused.store(pos > 0.5);
    mOutputGain = 0.f;
    mTransportFadeDirection = TransportFadeDirection::None;
  }

  void ProcessBlock(sample** outputs, int nOutputs, int nFrames, sample targetGain, LogParamSmooth<sample, 1>& gainSmoother)
  {
    if (mTriggerPending.exchange(false))
    {
      mReadHeadFrac = 0.;
      mPlayhead.store(0);
      mPlaying.store(true);
      mPaused.store(false);
      mAfterTransportFade = AfterTransportFade::None;
      EndSeekCrossfade();
      BeginTransportFadeIn();
    }

    for (int s = 0; s < nFrames; s++)
    {
      ApplyScheduledCommandAt(s);
      AdvanceTransportFade();

      if (!IsAudible() || !mLeft || mLength <= 0)
        continue;

      sample outL = 0.f;
      sample outR = 0.f;

      if (mSeekCrossfadeRemaining > 0)
        RenderSeekCrossfadeSample(outL, outR);
      else
        RenderPlaybackSample(outL, outR);

      if (mSeekCrossfadeRemaining <= 0 && mReadHeadFrac >= static_cast<double>(mLength - 1))
      {
        mPlaying.store(false);
        mPaused.store(false);
        mOutputGain = 0.f;
        mTransportFadeDirection = TransportFadeDirection::None;
        mLastOutL = 0.f;
        mLastOutR = 0.f;
        break;
      }

      const sample gain = gainSmoother.Process(targetGain) * static_cast<sample>(mOutputGain);
      outL *= gain;
      outR *= gain;
      mLimiter.ProcessStereo(outL, outR);

      if (nOutputs > 0)
        outputs[0][s] = outL;
      if (nOutputs > 1)
        outputs[1][s] = outR;

      mLastOutL = outL;
      mLastOutR = outR;
    }

    if (mPlaying.load() && mSeekCrossfadeRemaining <= 0 && mTransportFadeDirection == TransportFadeDirection::None)
      mPlayhead.store(static_cast<int>(mReadHeadFrac));
  }

  bool IsPlaying() const { return mPlaying.load(); }

  float GetPlayheadNorm() const
  {
    if (mLength <= 1)
      return 0.f;

    if (mSeekCrossfadeRemaining > 0)
      return static_cast<float>(mIncomingReadFrac / static_cast<double>(mLength - 1));

    return static_cast<float>(mReadHeadFrac / static_cast<double>(mLength - 1));
  }

private:
  static constexpr double kSeekCrossfadeMs = 72.;
  static constexpr double kTransportFadeMs = 12.;
  static constexpr double kHalfPi = 1.5707963267948966;

  enum class TransportFadeDirection : uint8_t
  {
    None,
    Out,
    In
  };

  enum class AfterTransportFade : uint8_t
  {
    None,
    Pause,
    Stop
  };

  bool ShouldCrossfadeSeek() const
  {
    return mPlaying.load() || mPaused.load() || mSeekCrossfadeRemaining > 0
        || mTransportFadeDirection != TransportFadeDirection::None;
  }

  bool IsAudible() const
  {
    return mPlaying.load() || mSeekCrossfadeRemaining > 0 || mTransportFadeDirection != TransportFadeDirection::None;
  }

  double NormalizedToPosition(float norm) const
  {
    norm = (std::max)(0.f, (std::min)(1.f, norm));
    return static_cast<double>(norm) * static_cast<double>(mLength - 1);
  }

  static float EqualPowerGainOut(float t)
  {
    const float c = std::cos(t * static_cast<float>(kHalfPi));
    return c * c;
  }

  static float EqualPowerGainIn(float t)
  {
    const float s = std::sin(t * static_cast<float>(kHalfPi));
    return s * s;
  }

  sample ReadChannel(const sample* channel, double position) const
  {
    if (!channel || mLength <= 0)
      return 0.f;

    if (mLength == 1)
      return channel[0];

    position = (std::max)(0., (std::min)(position, static_cast<double>(mLength - 1)));
    const int i0 = static_cast<int>(position);
    const int i1 = (std::min)(i0 + 1, mLength - 1);
    const float frac = static_cast<float>(position - static_cast<double>(i0));
    return channel[i0] * (1.f - frac) + channel[i1] * frac;
  }

  void ClearSchedule()
  {
    mScheduledCommand.store(ScheduledCommand::None);
    mScheduledOffset.store(0);
  }

  void EndSeekCrossfade()
  {
    mSeekCrossfadeRemaining = 0;
    mOutgoingReadFrac = 0.;
    mIncomingReadFrac = 0.;
  }

  void CommitSeekCrossfadePosition()
  {
    if (mSeekCrossfadeRemaining > 0)
    {
      mReadHeadFrac = mIncomingReadFrac;
      mPlayhead.store(static_cast<int>(mReadHeadFrac));
    }

    EndSeekCrossfade();
  }

  void BeginSeekCrossfade(double targetPos)
  {
    if (mSeekCrossfadeRemaining > 0)
      mOutgoingReadFrac = mIncomingReadFrac;
    else
      mOutgoingReadFrac = mReadHeadFrac;

    mIncomingReadFrac = targetPos;
    mSeekCrossfadeRemaining = mSeekCrossfadeSamples;
    mAfterTransportFade = AfterTransportFade::None;
    mTransportFadeDirection = TransportFadeDirection::None;
    mOutputGain = 1.f;
  }

  void RenderSeekCrossfadeSample(sample& outL, sample& outR)
  {
    const float t = 1.f - static_cast<float>(mSeekCrossfadeRemaining) / static_cast<float>(mSeekCrossfadeSamples);
    const float gOut = EqualPowerGainOut(t);
    const float gIn = EqualPowerGainIn(t);

    const sample oldL = ReadChannel(mLeft, mOutgoingReadFrac);
    const sample oldR = ReadChannel(mRight ? mRight : mLeft, mOutgoingReadFrac);
    const sample newL = ReadChannel(mLeft, mIncomingReadFrac);
    const sample newR = ReadChannel(mRight ? mRight : mLeft, mIncomingReadFrac);

    outL = static_cast<sample>(oldL * gOut + newL * gIn);
    outR = static_cast<sample>(oldR * gOut + newR * gIn);

    mOutgoingReadFrac += 1.;
    mIncomingReadFrac += 1.;
    mSeekCrossfadeRemaining--;

    if (mSeekCrossfadeRemaining <= 0)
    {
      mReadHeadFrac = mIncomingReadFrac;
      EndSeekCrossfade();
    }
  }

  void RenderPlaybackSample(sample& outL, sample& outR)
  {
    outL = ReadChannel(mLeft, mReadHeadFrac);
    outR = ReadChannel(mRight ? mRight : mLeft, mReadHeadFrac);
    mReadHeadFrac += 1.;
  }

  void ApplyScheduledCommandAt(int sampleIndex)
  {
    if (mScheduledCommand.load() == ScheduledCommand::None)
      return;

    if (mScheduledOffset.load() != sampleIndex)
      return;

    const ScheduledCommand command = mScheduledCommand.exchange(ScheduledCommand::None);

    switch (command)
    {
      case ScheduledCommand::Play:
        RequestPlay();
        break;
      case ScheduledCommand::Pause:
        RequestPause();
        break;
      case ScheduledCommand::Stop:
        RequestStop();
        break;
      default:
        break;
    }
  }

  void BeginTransportFadeOut()
  {
    mTransportFadeTotal = mTransportFadeSamples;
    mTransportFadeRemaining = mTransportFadeTotal;
    mTransportFadeDirection = TransportFadeDirection::Out;
  }

  void BeginTransportFadeIn()
  {
    mTransportFadeTotal = mTransportFadeSamples;
    mTransportFadeRemaining = mTransportFadeTotal;
    mTransportFadeDirection = TransportFadeDirection::In;
    mOutputGain = 0.f;
  }

  void AdvanceTransportFade()
  {
    if (mTransportFadeDirection == TransportFadeDirection::None || mTransportFadeTotal <= 0)
    {
      if (mSeekCrossfadeRemaining <= 0)
        mOutputGain = mPlaying.load() ? 1.f : 0.f;
      return;
    }

    const float t = 1.f - static_cast<float>(mTransportFadeRemaining) / static_cast<float>(mTransportFadeTotal);

    if (mTransportFadeDirection == TransportFadeDirection::Out)
    {
      mOutputGain = EqualPowerGainOut(t);
      mTransportFadeRemaining--;

      if (mTransportFadeRemaining <= 0)
        CompleteTransportFadeOut();
      return;
    }

    mOutputGain = EqualPowerGainIn(t);
    mTransportFadeRemaining--;

    if (mTransportFadeRemaining <= 0)
    {
      mOutputGain = 1.f;
      mTransportFadeDirection = TransportFadeDirection::None;
    }
  }

  void CompleteTransportFadeOut()
  {
    mOutputGain = 0.f;

    switch (mAfterTransportFade)
    {
      case AfterTransportFade::Pause:
        mPlaying.store(false);
        mPaused.store(true);
        break;

      case AfterTransportFade::Stop:
        mPlaying.store(false);
        mPaused.store(false);
        mReadHeadFrac = 0.;
        mPlayhead.store(0);
        mLastOutL = 0.f;
        mLastOutR = 0.f;
        ClearSchedule();
        break;

      default:
        break;
    }

    mAfterTransportFade = AfterTransportFade::None;
    mTransportFadeDirection = TransportFadeDirection::None;
  }

  const sample* mLeft = nullptr;
  const sample* mRight = nullptr;
  int mLength = 0;
  double mSampleRate = 44100.;
  int mSeekCrossfadeSamples = 792;
  int mSeekCrossfadeRemaining = 0;
  int mTransportFadeSamples = 528;
  int mTransportFadeRemaining = 0;
  int mTransportFadeTotal = 0;
  double mReadHeadFrac = 0.;
  double mOutgoingReadFrac = 0.;
  double mIncomingReadFrac = 0.;
  sample mLastOutL = 0.f;
  sample mLastOutR = 0.f;
  float mOutputGain = 1.f;
  TransportFadeDirection mTransportFadeDirection = TransportFadeDirection::None;
  AfterTransportFade mAfterTransportFade = AfterTransportFade::None;

  std::atomic<int> mPlayhead {0};
  std::atomic<bool> mPlaying {false};
  std::atomic<bool> mPaused {false};
  std::atomic<bool> mTriggerPending {false};

  std::atomic<ScheduledCommand> mScheduledCommand {ScheduledCommand::None};
  std::atomic<int> mScheduledOffset {0};

  OutputLimiter mLimiter;
};

END_IPLUG_NAMESPACE
