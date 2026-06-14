#pragma once

#include "PitchStreamPipeline.h"
#include "SampleBuffer.h"
#include "SamplePlayer.h"
#include "audioagent/iplug_bridge.h"

namespace audioagent
{

/**
 * Embedded sample playback engine with streaming audioFlux pitch read-ahead.
 *
 * Real-time contract:
 *  - ProcessBlock only mixes audio; pitch chunks run on PitchStreamWorker.
 *  - Dry buffer is never replaced; pitched audio is read from PitchStreamPipeline cache.
 */
class SampleTransport
{
public:
  bool LoadEmbedded(const char* resourceFileName, double hostSampleRate)
  {
    mPlayer.SetSampleRate(hostSampleRate);

    if (!mBuffer.LoadEmbedded(resourceFileName, hostSampleRate))
      return false;

    mPitchStream.BindDryBuffer(mBuffer);
    mPlayer.SetBuffer(mBuffer);
    mPlayer.SetPitchStream(&mPitchStream);
    mWaveformDirty = true;
    return true;
  }

  void SetSampleRate(double hostSampleRate)
  {
    mPlayer.SetSampleRate(hostSampleRate);
  }

  void ResetPitchStream()
  {
    mPitchStream.Reset();
    mPlayer.SetPitchStream(&mPitchStream);
  }

  void BeginPitchStream(int originSample, int semitones) { mPitchStream.BeginStream(originSample, semitones); }

  void EndPitchStream() { mPitchStream.EndStream(); }

  bool IsPitchStreamActive() const { return mPitchStream.IsActive(); }

  bool IsPitchStreamCatchingUp() const { return mPitchStream.IsCatchingUp(); }

  bool IsPitchWorkerBusy() const { return mPitchStream.IsWorkerBusy(); }

  int GetPitchSemitones() const { return mPitchStream.GetSemitones(); }

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

  void ScheduleSeek(float norm, int sampleOffset) { mPlayer.ScheduleSeek(norm, sampleOffset); }

  void ProcessBlock(sample** outputs, int nOutputs, int nFrames, sample targetGain, LogParamSmooth<sample, 1>& gainSmoother)
  {
    mPlayer.ProcessBlock(outputs, nOutputs, nFrames, targetGain, gainSmoother);
    SyncPitchPlayhead();
    mPitchStream.KickScheduler();
  }

  void SyncPitchPlayhead()
  {
    if (!mBuffer.IsLoaded() || mBuffer.GetLength() <= 1)
      return;

    const float norm = mPlayer.GetPlayheadNorm();
    const int sampleIndex = static_cast<int>(std::lround(norm * static_cast<float>(mBuffer.GetLength() - 1)));
    mPitchStream.SetPlayheadSample(sampleIndex);
  }

  void KickPitchScheduler() { mPitchStream.KickScheduler(); }

  bool IsPlaying() const { return mPlayer.IsPlaying(); }

  float GetPlayheadNorm() const { return mPlayer.GetPlayheadNorm(); }

  int GetPlayheadSample() const
  {
    if (!mBuffer.IsLoaded() || mBuffer.GetLength() <= 1)
      return 0;

    const float norm = mPlayer.GetPlayheadNorm();
    return static_cast<int>(std::lround(norm * static_cast<float>(mBuffer.GetLength() - 1)));
  }

  bool ConsumeWaveformDirty()
  {
    const bool dirty = mWaveformDirty;
    mWaveformDirty = false;
    return dirty;
  }

  const SampleBuffer& GetBuffer() const { return mBuffer; }

  PitchStreamPipeline& GetPitchStream() { return mPitchStream; }

private:
  SampleBuffer mBuffer;
  SamplePlayer mPlayer;
  PitchStreamPipeline mPitchStream;
  bool mWaveformDirty = false;
};

} // namespace audioagent
