#pragma once

#include "SampleBuffer.h"
#include "SamplePlayer.h"
#include "Smoothers.h"

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
    mPlayer.ProcessBlock(outputs, nOutputs, nFrames, targetGain, gainSmoother);
  }

  bool IsPlaying() const { return mPlayer.IsPlaying(); }

  float GetPlayheadNorm() const { return mPlayer.GetPlayheadNorm(); }

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
};

END_IPLUG_NAMESPACE
