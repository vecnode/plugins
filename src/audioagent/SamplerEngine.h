#pragma once

#include "analysis/OfflineSampleWorker.h"
#include "analysis/SampleNoteDetector.h"
#include "analysis/SampleProcessSnapshot.h"
#include "dsp/SampleTransport.h"
#include "model/WaveformEnvelope.h"
#include "audioagent/iplug_bridge.h"

#include <atomic>
#include <cmath>

namespace audioagent
{

struct WorkerUiState
{
  bool detectChanged = false;
  OfflineSampleWorker::Phase detectPhase = OfflineSampleWorker::Phase::Idle;
  DetectedNote detectNote;

  bool pitchLabelChanged = false;
  DetectedNote pitchLabelNote;
};

/**
 * Sampler engine — transport, audioFlux detect on worker, streaming pitch read-ahead.
 *
 * +1 pitch: audioFlux chunk pipeline fills cache ahead of playhead; playback never stops.
 */
class SamplerEngine
{
public:
  static constexpr int kWaveformPoints = 1024;
  static constexpr int kMaxPitchSemitones = 12;

  void SetSampleRate(double sampleRate) { mTransport.SetSampleRate(sampleRate); }

  void Reset()
  {
    mReferenceNote = {};
    mBaseReferenceNote = {};
    mPitchSemitones = 0;
    mPendingDetectRequest.store(false, std::memory_order_release);
    mForcePlayhead = false;
    mTransport.ResetPitchStream();
  }

  bool LoadEmbedded(const char* resourceFileName, double hostSampleRate)
  {
    if (!mTransport.LoadEmbedded(resourceFileName, hostSampleRate))
      return false;

    mWaveform.BuildFrom(mTransport.GetBuffer(), kWaveformPoints);
    RebuildProcessSnapshot();
    return true;
  }

  void ProcessBlock(sample** outputs, int nChans, int nFrames, sample targetGain, LogParamSmooth<sample, 1>& gainSmoother)
  {
    mTransport.ProcessBlock(outputs, nChans, nFrames, targetGain, gainSmoother);
  }

  void SchedulePlay(int sampleOffset)
  {
    mTransport.SchedulePlay(sampleOffset);
    mForcePlayhead = true;
  }

  void SchedulePause(int sampleOffset)
  {
    mTransport.SchedulePause(sampleOffset);
    mForcePlayhead = true;
  }

  void ScheduleStop(int sampleOffset)
  {
    mTransport.ScheduleStop(sampleOffset);
    mForcePlayhead = true;
  }

  void ScheduleSeek(float norm, int sampleOffset)
  {
    mTransport.ScheduleSeek(norm, sampleOffset);
    mForcePlayhead = true;
  }

  void RequestDetectNote() { mPendingDetectRequest.store(true, std::memory_order_release); }

  /** Start / extend streaming audioFlux +1 from the current playhead. */
  void RequestPitchUpOne()
  {
    if (!mBaseReferenceNote.valid)
      return;

    if (mPitchSemitones >= kMaxPitchSemitones)
      return;

    const int nextSemitones = mPitchSemitones + 1;
    const int origin = mTransport.GetPlayheadSample();

    mTransport.BeginPitchStream(origin, nextSemitones);
    mPitchSemitones = nextSemitones;
    mReferenceNote = SampleNoteDetector::Transpose(mBaseReferenceNote, mPitchSemitones);

    mUiWorkerState.pitchLabelChanged = true;
    mUiWorkerState.pitchLabelNote = mReferenceNote;
  }

  int GetPitchSemitones() const { return mPitchSemitones; }

  bool IsPitchStreamActive() const { return mTransport.IsPitchStreamActive(); }

  bool IsPitchCatchingUp() const
  {
    return mTransport.IsPitchStreamCatchingUp() || mTransport.IsPitchWorkerBusy();
  }

  void Tick()
  {
    mTransport.KickPitchScheduler();
    ProcessPendingOfflineJobs();
    PollWorkerUiState();
  }

  bool IsWorkerBusy() const { return mWorker.IsBusy(); }

  bool HasReferenceNote() const { return mBaseReferenceNote.valid; }

  const DetectedNote& GetReferenceNote() const { return mReferenceNote; }

  const SampleTransport& GetTransport() const { return mTransport; }

  SampleTransport& GetTransport() { return mTransport; }

  const WaveformEnvelope& GetWaveform() const { return mWaveform; }

  bool ConsumeWaveformDirty() { return mTransport.ConsumeWaveformDirty(); }

  bool ShouldSyncPlayhead() const { return mForcePlayhead || mTransport.IsPlaying(); }

  void ClearPlayheadForce() { mForcePlayhead = false; }

  const WorkerUiState& GetWorkerUiState() const { return mUiWorkerState; }

  void ClearWorkerUiState()
  {
    mUiWorkerState.detectChanged = false;
    mUiWorkerState.pitchLabelChanged = false;
  }

  OfflineSampleWorker& GetWorker() { return mWorker; }

private:
  void RebuildProcessSnapshot()
  {
    SampleProcessSnapshot::Capture(mTransport.GetBuffer(), mWorker);
  }

  void ProcessPendingOfflineJobs()
  {
    if (mWorker.IsBusy())
      return;

    if (!mPendingDetectRequest.exchange(false))
      return;

    if (!mTransport.IsSampleLoaded())
      return;

    SampleProcessSnapshot::Capture(mTransport.GetBuffer(), mWorker);
    mWorker.RequestDetect();
  }

  void PollWorkerUiState()
  {
    DetectedNote detectNote;
    if (mWorker.ConsumeDetectUpdate(mUiWorkerState.detectPhase, detectNote))
    {
      mUiWorkerState.detectChanged = true;
      mUiWorkerState.detectNote = detectNote;

      if (mUiWorkerState.detectPhase == OfflineSampleWorker::Phase::Succeeded)
      {
        mBaseReferenceNote = detectNote;
        mPitchSemitones = 0;
        mReferenceNote = detectNote;
        mTransport.EndPitchStream();
      }
      else if (mUiWorkerState.detectPhase == OfflineSampleWorker::Phase::Failed)
      {
        mBaseReferenceNote = {};
        mReferenceNote = {};
        mPitchSemitones = 0;
        mTransport.EndPitchStream();
      }
    }
  }

  SampleTransport mTransport;
  WaveformEnvelope mWaveform;
  OfflineSampleWorker mWorker;
  DetectedNote mBaseReferenceNote;
  DetectedNote mReferenceNote;
  WorkerUiState mUiWorkerState;
  int mPitchSemitones = 0;
  bool mForcePlayhead = false;

  std::atomic<bool> mPendingDetectRequest {false};
};

} // namespace audioagent
