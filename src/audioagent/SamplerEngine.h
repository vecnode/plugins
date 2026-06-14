#pragma once

#include "analysis/OfflineSampleWorker.h"
#include "analysis/SampleProcessSnapshot.h"
#include "dsp/SampleBuffer.h"
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

  bool pitchChanged = false;
  OfflineSampleWorker::Phase pitchPhase = OfflineSampleWorker::Phase::Idle;
  OfflineSampleWorker::PitchResult pitchResult;
};

/**
 * Sampler engine — orchestrates transport, offline MIR, and waveform model.
 *
 * Threading:
 *  - ProcessBlock / OnParamChange scheduling: real-time safe (O(1) flags + mixing).
 *  - Tick(): UI-timer work — snapshot capture, worker queue, pitch buffer staging.
 *  - Host plugin maps Tick() results to IGraphics; this class never touches UI.
 */
class SamplerEngine
{
public:
  static constexpr int kWaveformPoints = 1024;

  void SetSampleRate(double sampleRate) { mTransport.SetSampleRate(sampleRate); }

  void Reset()
  {
    mReferenceNote = {};
    mPendingDetectRequest.store(false, std::memory_order_release);
    mPendingPitchRequest.store(false, std::memory_order_release);
    mForcePlayhead = false;
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

  void RequestPitchUpOne()
  {
    if (!mReferenceNote.valid)
      return;

    mPitchRequestPlayheadNorm = mTransport.GetPlayheadNorm();
    mPendingPitchRequest.store(true, std::memory_order_release);
  }

  /** UI timer — queue offline jobs and apply non-UI pitch staging. */
  void Tick()
  {
    ProcessPendingOfflineJobs();
    PollWorkerUiState();
  }

  bool IsWorkerBusy() const { return mWorker.IsBusy(); }

  bool HasReferenceNote() const { return mReferenceNote.valid; }

  const DetectedNote& GetReferenceNote() const { return mReferenceNote; }

  const SampleTransport& GetTransport() const { return mTransport; }

  SampleTransport& GetTransport() { return mTransport; }

  const WaveformEnvelope& GetWaveform() const { return mWaveform; }

  bool ConsumeWaveformDirty() { return mTransport.ConsumeWaveformDirty(); }

  bool ShouldSyncPlayhead() const { return mForcePlayhead || mTransport.IsPlaying(); }

  void ClearPlayheadForce() { mForcePlayhead = false; }

  const WorkerUiState& GetWorkerUiState() const { return mUiWorkerState; }

  void ClearWorkerUiState() { mUiWorkerState = {}; }

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

    if (mPendingDetectRequest.exchange(false))
    {
      if (!mTransport.IsSampleLoaded())
        return;

      SampleProcessSnapshot::Capture(mTransport.GetBuffer(), mWorker);
      mWorker.RequestDetect();
      return;
    }

    if (mPendingPitchRequest.exchange(false) && mReferenceNote.valid)
    {
      SampleProcessSnapshot::Capture(mTransport.GetBuffer(), mWorker);
      mWorker.RequestPitchUpOne(mReferenceNote);
    }
  }

  void PollWorkerUiState()
  {
    mUiWorkerState = {};

    DetectedNote detectNote;
    if (mWorker.ConsumeDetectUpdate(mUiWorkerState.detectPhase, detectNote))
    {
      mUiWorkerState.detectChanged = true;
      mUiWorkerState.detectNote = detectNote;

      if (mUiWorkerState.detectPhase == OfflineSampleWorker::Phase::Succeeded)
        mReferenceNote = detectNote;
      else if (mUiWorkerState.detectPhase == OfflineSampleWorker::Phase::Failed)
        mReferenceNote = {};
    }

    OfflineSampleWorker::PitchResult pitchResult;
    if (mWorker.ConsumePitchUpdate(mUiWorkerState.pitchPhase, pitchResult))
    {
      mUiWorkerState.pitchChanged = true;
      mUiWorkerState.pitchResult = pitchResult;

      if (mUiWorkerState.pitchPhase == OfflineSampleWorker::Phase::Succeeded && pitchResult.ok)
      {
        const double hostRate = mTransport.GetBuffer().GetHostSampleRate();
        SampleBuffer preview;
        preview.AssignFromFloat(pitchResult.left, pitchResult.right, hostRate);
        mWaveform.BuildFrom(preview, kWaveformPoints);
        mReferenceNote = pitchResult.note;

        SampleProcessSnapshot::CaptureChannels(pitchResult.left.data(),
                                               pitchResult.right.data(),
                                               static_cast<int>(pitchResult.left.size()),
                                               static_cast<int>(std::lround(hostRate)),
                                               mWorker);

        mTransport.StageProcessedBuffer(std::move(pitchResult.left),
                                        std::move(pitchResult.right),
                                        hostRate,
                                        mPitchRequestPlayheadNorm);
        mForcePlayhead = true;
      }
    }
  }

  SampleTransport mTransport;
  WaveformEnvelope mWaveform;
  OfflineSampleWorker mWorker;
  DetectedNote mReferenceNote;
  WorkerUiState mUiWorkerState;
  float mPitchRequestPlayheadNorm = 0.f;
  bool mForcePlayhead = false;

  std::atomic<bool> mPendingDetectRequest {false};
  std::atomic<bool> mPendingPitchRequest {false};
};

} // namespace audioagent
