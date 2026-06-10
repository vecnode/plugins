#pragma once

#include "OutputLimiter.h"
#include "SampleBuffer.h"
#include "Smoothers.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

BEGIN_IPLUG_NAMESPACE

/**
 * One-shot sample playback with interpolated reads.
 *
 * Seeks while actively playing use a retargetable linear dual-head crossfade
 * (outgoing head never restarts on rapid scrub). Seeks while paused, stopped,
 * or during transport fades are silent position updates with no audio output.
 */
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
    mSeekCrossfadeSamples = (std::max)(mSeekCrossfadeSamples, 128);
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
    ClearScheduledSeek();
  }

  void ScheduleCommand(ScheduledCommand command, int sampleOffset)
  {
    mScheduledCommand.store(command);
    mScheduledOffset.store(sampleOffset < 0 ? 0 : sampleOffset);
  }

  /** Queue a seek for sample-accurate application inside ProcessBlock. */
  void ScheduleSeek(float norm, int sampleOffset)
  {
    mScheduledSeekNorm.store(norm);
    mScheduledSeekOffset.store(sampleOffset < 0 ? 0 : sampleOffset);
    mScheduledSeekPending.store(true);
  }

  void RequestPlay()
  {
    if (mPlaying.load() && !mPaused.load()
        && mTransportFadeDirection == TransportFadeDirection::None
        && mSeekCrossfadeRemaining <= 0)
      return;

    EndSeekCrossfade();
    mPaused.store(false);
    mPlaying.store(true);
    mAfterTransportFade = AfterTransportFade::None;
    mLastOutL = 0.f;
    mLastOutR = 0.f;
    mLimiter.Reset();
    mPlayhead.store(static_cast<int>(mReadHeadFrac));
    BeginTransportFadeIn();
  }

  void RequestPause()
  {
    if (!mPlaying.load() && mTransportFadeDirection != TransportFadeDirection::Out)
      return;

    CommitSeekCrossfadePosition();
    mAfterTransportFade = AfterTransportFade::Pause;
    BeginTransportFadeOut();
  }

  void RequestStop()
  {
    CommitSeekCrossfadePosition();

    if (!mPlaying.load() && mSeekCrossfadeRemaining <= 0)
    {
      FinishStopImmediate();
      return;
    }

    mAfterTransportFade = AfterTransportFade::Stop;
    BeginTransportFadeOut();
  }

  void ProcessBlock(sample** outputs, int nOutputs, int nFrames, sample targetGain, LogParamSmooth<sample, 1>& gainSmoother)
  {
    for (int s = 0; s < nFrames; s++)
    {
      ApplyScheduledCommandAt(s);
      ApplyScheduledSeekAt(s);
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
  static constexpr double kSeekCrossfadeMs = 20.;
  static constexpr double kTransportFadeMs = 12.;
  static constexpr double kHalfPi = 1.5707963267948966;
  static constexpr double kSeekEpsilon = 0.5;

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

  bool IsAudible() const
  {
    return mPlaying.load() || mSeekCrossfadeRemaining > 0
        || mTransportFadeDirection != TransportFadeDirection::None;
  }

  bool ShouldAudiblySeek() const
  {
    return mPlaying.load()
        && !mPaused.load()
        && mTransportFadeDirection == TransportFadeDirection::None;
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

  void ClearScheduledSeek()
  {
    mScheduledSeekPending.store(false);
    mScheduledSeekNorm.store(0.f);
    mScheduledSeekOffset.store(0);
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

  double CurrentAudiblePosition() const
  {
    if (mSeekCrossfadeRemaining > 0)
      return mOutgoingReadFrac;

    return (std::max)(0., mReadHeadFrac - 1.);
  }

  void ApplySilentSeek(double pos)
  {
    EndSeekCrossfade();
    mReadHeadFrac = pos;
    mLastOutL = 0.f;
    mLastOutR = 0.f;
    mLimiter.Reset();
  }

  void QueueAudibleSeek(double targetPos)
  {
    if (std::fabs(targetPos - CurrentAudiblePosition()) < kSeekEpsilon)
      return;

    if (mSeekCrossfadeRemaining > 0)
    {
      mIncomingReadFrac = targetPos;
      return;
    }

    mOutgoingReadFrac = CurrentAudiblePosition();
    mIncomingReadFrac = targetPos;
    mSeekCrossfadeRemaining = mSeekCrossfadeSamples;
    mLimiter.Reset();
  }

  void ApplyScheduledSeekAt(int sampleIndex)
  {
    if (!mScheduledSeekPending.load())
      return;

    if (mScheduledSeekOffset.load() != sampleIndex)
      return;

    mScheduledSeekPending.store(false);

    const float norm = mScheduledSeekNorm.load();
    const double pos = NormalizedToPosition(norm);
    mPlayhead.store(static_cast<int>(pos));

    if (!ShouldAudiblySeek())
    {
      ApplySilentSeek(pos);
      return;
    }

    QueueAudibleSeek(pos);
  }

  void RenderSeekCrossfadeSample(sample& outL, sample& outR)
  {
    const float t = 1.f - static_cast<float>(mSeekCrossfadeRemaining) / static_cast<float>(mSeekCrossfadeSamples);
    const float gOut = 1.f - t;
    const float gIn = t;

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

  void FinishStopImmediate()
  {
    mPlaying.store(false);
    mPaused.store(false);
    mReadHeadFrac = 0.;
    mPlayhead.store(0);
    mOutputGain = 0.f;
    mLastOutL = 0.f;
    mLastOutR = 0.f;
    mAfterTransportFade = AfterTransportFade::None;
    mTransportFadeDirection = TransportFadeDirection::None;
    mTransportFadeRemaining = 0;
    EndSeekCrossfade();
    mLimiter.Reset();
    ClearSchedule();
  }

  void CompleteTransportFadeOut()
  {
    mOutputGain = 0.f;

    if (mAfterTransportFade == AfterTransportFade::Stop)
    {
      FinishStopImmediate();
      return;
    }

    if (mAfterTransportFade == AfterTransportFade::Pause)
    {
      mPlaying.store(false);
      mPaused.store(true);
    }

    mAfterTransportFade = AfterTransportFade::None;
    mTransportFadeDirection = TransportFadeDirection::None;
  }

  const sample* mLeft = nullptr;
  const sample* mRight = nullptr;
  int mLength = 0;
  double mSampleRate = 44100.;
  int mSeekCrossfadeSamples = 882;
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
  std::atomic<ScheduledCommand> mScheduledCommand {ScheduledCommand::None};
  std::atomic<int> mScheduledOffset {0};

  std::atomic<bool> mScheduledSeekPending {false};
  std::atomic<float> mScheduledSeekNorm {0.f};
  std::atomic<int> mScheduledSeekOffset {0};

  OutputLimiter mLimiter;
};

END_IPLUG_NAMESPACE
