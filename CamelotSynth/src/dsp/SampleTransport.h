#pragma once

#include "SampleBuffer.h"
#include "SamplePlayer.h"
#include "Smoothers.h"

#include <atomic>
#include <vector>

BEGIN_IPLUG_NAMESPACE

/**
 * Embedded sample playback engine (buffer + player).
 *
 * Transport is driven by hidden trigger parameters scheduled with sample-accurate
 * offsets in OnParamChange (VST3 applies these in the same process() call as audio).
 * Avoid SendArbitraryMsgFromUI for transport — host messaging can add large latency.
 */
class SampleTransport
{
public:
  bool LoadEmbedded(const char* resourceFileName, double hostSampleRate)
  {
    mPlayer.SetSampleRate(hostSampleRate);

    if (!mBuffer.LoadEmbedded(resourceFileName, hostSampleRate))
      return false;

    mPlayer.SetBuffer(mBuffer);
    mWaveformDirty = true;
    return true;
  }

  void SetSampleRate(double hostSampleRate)
  {
    mPlayer.SetSampleRate(hostSampleRate);
  }

  bool IsSampleLoaded() const { return mBuffer.IsLoaded(); }

  void SchedulePlay(int sampleOffset)
  {
    if (!mBuffer.IsLoaded())
      return;

    mPlayer.ScheduleCommand(SamplePlayer::ScheduledCommand::Play, sampleOffset);
  }

  void SchedulePause(int sampleOffset)
  {
    mPlayer.ScheduleCommand(SamplePlayer::ScheduledCommand::Pause, sampleOffset);
  }

  void ScheduleStop(int sampleOffset)
  {
    mPlayer.ScheduleCommand(SamplePlayer::ScheduledCommand::Stop, sampleOffset);
  }

  void ScheduleSeek(float norm, int sampleOffset)
  {
    mPlayer.ScheduleSeek(norm, sampleOffset);
  }

  void ProcessBlock(sample** outputs, int nOutputs, int nFrames, sample targetGain, LogParamSmooth<sample, 1>& gainSmoother)
  {
    ApplyPendingSwapIfReady();
    mPlayer.ProcessBlock(outputs, nOutputs, nFrames, targetGain, gainSmoother);
  }

  bool IsPlaying() const { return mPlayer.IsPlaying(); }

  float GetPlayheadNorm() const { return mPlayer.GetPlayheadNorm(); }

  /** Apply a buffer prepared on the worker thread — call at the start of ProcessBlock only. */
  void ApplyPendingSwapIfReady()
  {
    if (!mSwapPending.load(std::memory_order_acquire))
      return;

    mSwapPending.store(false, std::memory_order_release);

    mBuffer.AssignFromFloat(mStagingLeft, mStagingRight, mStagingSampleRate);
    mPlayer.SetBuffer(mBuffer);
    mPlayer.SilentSeekToNorm(mStagingPlayheadNorm);
    mWaveformDirty = true;
  }

  void StageProcessedBuffer(std::vector<float>&& left,
                            std::vector<float>&& right,
                            double sampleRate,
                            float playheadNorm)
  {
    mStagingLeft = std::move(left);
    mStagingRight = std::move(right);
    mStagingSampleRate = sampleRate;
    mStagingPlayheadNorm = playheadNorm;
    mSwapPending.store(true, std::memory_order_release);
  }

  bool ConsumeWaveformDirty()
  {
    const bool dirty = mWaveformDirty;
    mWaveformDirty = false;
    return dirty;
  }

  const SampleBuffer& GetBuffer() const { return mBuffer; }

private:
  SampleBuffer mBuffer;
  SamplePlayer mPlayer;
  bool mWaveformDirty = false;

  std::vector<float> mStagingLeft;
  std::vector<float> mStagingRight;
  double mStagingSampleRate = 44100.;
  float mStagingPlayheadNorm = 0.f;
  std::atomic<bool> mSwapPending {false};
};

END_IPLUG_NAMESPACE
