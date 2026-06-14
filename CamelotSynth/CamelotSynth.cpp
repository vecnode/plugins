#include "CamelotSynth.h"
#include "CamelotSynthEditor.h"
#include "WaveformControl.h"
#include "UiPaintPolicy.h"
#include "SampleBuffer.h"
#include "IPlug_include_in_plug_src.h"

namespace
{
constexpr int kWaveformPoints = 1024;
constexpr double kGainSmoothMs = 20.;
constexpr int kHiddenTransportFlags = IParam::kFlagMeta | IParam::kFlagCannotAutomate;
}

CamelotSynth::CamelotSynth(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100.0, 0.01, "%");
  GetParam(kParamTrigPlay)->InitBool("Play", false, "", kHiddenTransportFlags);
  GetParam(kParamTrigPause)->InitBool("Pause", false, "", kHiddenTransportFlags);
  GetParam(kParamTrigStop)->InitBool("Stop", false, "", kHiddenTransportFlags);
  GetParam(kParamSeek)->InitDouble("Seek", 0., 0., 1., 0.001, "", kHiddenTransportFlags);
  GetParam(kParamTrigDetectNote)->InitBool("Detect Note", false, "", kHiddenTransportFlags);
  GetParam(kParamTrigPitchUpOne)->InitBool("Pitch +1", false, "", kHiddenTransportFlags);

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    CamelotSynthEditor::Attach(pGraphics, *this);
  };
#endif
}

#if IPLUG_EDITOR
void CamelotSynth::OnUIOpen()
{
  Plugin::OnUIOpen();
  mEditorPaintPrimed = false;

  if (auto* pGraphics = GetUI())
    ::igraphics::ForceInitialFullPaint(pGraphics);
}
#endif

#if IPLUG_DSP
CamelotSynth::~CamelotSynth() = default;

void CamelotSynth::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();
  for (int c = 0; c < nChans; c++)
    memset(outputs[c], 0, nFrames * sizeof(sample));

  const sample targetGain = static_cast<sample>(GetParam(kParamGain)->Value() / 100.);
  mSampleTransport.ProcessBlock(outputs, nChans, nFrames, targetGain, mGainSmoother);

  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
}

void CamelotSynth::OnIdle()
{
  mMeterSender.TransmitData(*this);

#if IPLUG_EDITOR
  if (auto* pGraphics = GetUI())
  {
    if (!mEditorPaintPrimed)
    {
      mEditorPaintPrimed = true;
      ::igraphics::ForceInitialFullPaint(pGraphics);
    }

    if (auto* pWaveform = pGraphics->GetControlWithTag(kCtrlTagWaveform))
    {
      auto* pCtrl = pWaveform->As<::igraphics::SampleWaveformControl>();

      if (mSampleTransport.ConsumeWaveformDirty() && mWaveformEnvelope.IsValid())
      {
        pCtrl->SetEnvelope(mWaveformEnvelope.Max(), mWaveformEnvelope.Min());

        if (auto* pLength = pGraphics->GetControlWithTag(kCtrlTagSampleLength))
        {
          pLength->As<::igraphics::SampleLengthLabelControl>()->SetDurationSeconds(
            mSampleTransport.GetBuffer().GetDurationSeconds());
        }
      }

      if (!pCtrl->IsScrubbing() && mUiPlayheadBridge.ShouldSyncPlayhead(mSampleTransport))
        pCtrl->SyncPlayheadFromDSP(mSampleTransport.GetPlayheadNorm());

      mUiPlayheadBridge.ClearPlayheadForce();
    }

    SyncOfflineWorkerState();
  }
#endif
}

void CamelotSynth::SyncOfflineWorkerState()
{
#if IPLUG_EDITOR
  if (auto* pGraphics = GetUI())
  {
    const bool workerBusy = mOfflineWorker.IsBusy();
    const bool hasReference = mReferenceNote.valid;

    if (auto* pPitchButton = pGraphics->GetControlWithTag(kCtrlTagPitchUpOne))
    {
      pPitchButton->SetDisabled(workerBusy || !hasReference);
      ::igraphics::RequestControlRepaint(pPitchButton);
    }

    if (auto* pDetectButton = pGraphics->GetControlWithTag(kCtrlTagDetectNote))
    {
      pDetectButton->SetDisabled(workerBusy);
      ::igraphics::RequestControlRepaint(pDetectButton);
    }

    OfflineSampleWorker::Phase phase = OfflineSampleWorker::Phase::Idle;
    DetectedNote detectNote;
    if (mOfflineWorker.ConsumeDetectUpdate(phase, detectNote))
    {
      if (auto* pNote = pGraphics->GetControlWithTag(kCtrlTagDetectedNote))
      {
        auto* pLabel = pNote->As<::igraphics::DetectedNoteLabelControl>();
        switch (phase)
        {
          case OfflineSampleWorker::Phase::Queued:
          case OfflineSampleWorker::Phase::Running:
            pLabel->SetAnalyzing();
            break;
          case OfflineSampleWorker::Phase::Succeeded:
            mReferenceNote = detectNote;
            pLabel->SetText(detectNote.text);
            break;
          case OfflineSampleWorker::Phase::Failed:
            mReferenceNote = {};
            pLabel->SetText("Note: --");
            break;
          default:
            break;
        }
        ::igraphics::RequestControlRepaint(pNote);
      }
    }

    OfflineSampleWorker::PitchResult pitchResult;
    if (mOfflineWorker.ConsumePitchUpdate(phase, pitchResult))
    {
      if (auto* pNote = pGraphics->GetControlWithTag(kCtrlTagDetectedNote))
      {
        auto* pLabel = pNote->As<::igraphics::DetectedNoteLabelControl>();
        switch (phase)
        {
          case OfflineSampleWorker::Phase::Queued:
          case OfflineSampleWorker::Phase::Running:
            pLabel->SetAnalyzing();
            break;
          case OfflineSampleWorker::Phase::Succeeded:
            if (pitchResult.ok)
            {
              SampleBuffer preview;
              preview.AssignFromFloat(pitchResult.left,
                                      pitchResult.right,
                                      mSampleTransport.GetBuffer().GetHostSampleRate());
              mWaveformEnvelope.BuildFrom(preview, kWaveformPoints);
              mReferenceNote = pitchResult.note;

              mSampleTransport.StageProcessedBuffer(std::move(pitchResult.left),
                                                    std::move(pitchResult.right),
                                                    preview.GetHostSampleRate(),
                                                    mPitchRequestPlayheadNorm);
              RebuildProcessSnapshot();
              mUiPlayheadBridge.MarkPlayheadDirty();
              pLabel->SetText(mReferenceNote.text);
            }
            else
            {
              pLabel->SetText("Note: --");
            }
            break;
          case OfflineSampleWorker::Phase::Failed:
            pLabel->SetText(mReferenceNote.valid ? mReferenceNote.text : "Note: --");
            break;
          default:
            break;
        }
        ::igraphics::RequestControlRepaint(pNote);
      }
    }
  }
#else
  (void) 0;
#endif
}

void CamelotSynth::OnReset()
{
  mMeterSender.Reset(GetSampleRate());
  mGainSmoother.SetSmoothTime(kGainSmoothMs, GetSampleRate());
  mGainSmoother.SetValue(static_cast<sample>(GetParam(kParamGain)->Value() / 100.));
  mSampleTransport.SetSampleRate(GetSampleRate());
  mReferenceNote = {};
  LoadEmbeddedSample();
}

void CamelotSynth::LoadEmbeddedSample()
{
  if (mSampleTransport.LoadEmbedded(ATMOS_SAMPLE_FN, GetSampleRate()))
  {
    mWaveformEnvelope.BuildFrom(mSampleTransport.GetBuffer(), kWaveformPoints);
    RebuildProcessSnapshot();
  }
}

void CamelotSynth::RebuildProcessSnapshot()
{
  const SampleBuffer& buffer = mSampleTransport.GetBuffer();
  if (!buffer.IsLoaded())
  {
    mOfflineWorker.SetSnapshot({});
    return;
  }

  OfflineSampleWorker::Snapshot snapshot;
  const int length = buffer.GetLength();
  snapshot.sampleRate = static_cast<int>(std::lround(buffer.GetHostSampleRate()));
  snapshot.mono.resize(static_cast<size_t>(length));
  snapshot.left.resize(static_cast<size_t>(length));
  snapshot.right.resize(static_cast<size_t>(length));

  const sample* left = buffer.GetLeft();
  const sample* right = buffer.GetRight();
  for (int i = 0; i < length; ++i)
  {
    snapshot.left[static_cast<size_t>(i)] = static_cast<float>(left[i]);
    snapshot.right[static_cast<size_t>(i)] = static_cast<float>(right[i]);
    snapshot.mono[static_cast<size_t>(i)] = static_cast<float>((left[i] + right[i]) * 0.5);
  }

  mOfflineWorker.SetSnapshot(std::move(snapshot));
}

void CamelotSynth::ResetTransportTrigger(int paramIdx)
{
  GetParam(paramIdx)->Set(0.);
}

void CamelotSynth::OnParamChange(int paramIdx, EParamSource source, int sampleOffset)
{
  (void) source;
  const int offset = sampleOffset < 0 ? 0 : sampleOffset;

  switch (paramIdx)
  {
    case kParamTrigPlay:
      if (GetParam(kParamTrigPlay)->Bool())
      {
        if (!mSampleTransport.IsSampleLoaded())
          LoadEmbeddedSample();

        mSampleTransport.SchedulePlay(offset);
        mUiPlayheadBridge.MarkPlayheadDirty();
      }
      ResetTransportTrigger(kParamTrigPlay);
      break;

    case kParamTrigPause:
      if (GetParam(kParamTrigPause)->Bool())
      {
        mSampleTransport.SchedulePause(offset);
        mUiPlayheadBridge.MarkPlayheadDirty();
      }
      ResetTransportTrigger(kParamTrigPause);
      break;

    case kParamTrigStop:
      if (GetParam(kParamTrigStop)->Bool())
      {
        mSampleTransport.ScheduleStop(offset);
        mUiPlayheadBridge.MarkPlayheadDirty();
      }
      ResetTransportTrigger(kParamTrigStop);
      break;

    case kParamSeek:
      mSampleTransport.ScheduleSeek(static_cast<float>(GetParam(kParamSeek)->Value()), offset);
      mUiPlayheadBridge.MarkPlayheadDirty();
      break;

    case kParamTrigDetectNote:
      if (GetParam(kParamTrigDetectNote)->Bool())
      {
        if (!mSampleTransport.IsSampleLoaded())
          LoadEmbeddedSample();
        else
          RebuildProcessSnapshot();

        mOfflineWorker.RequestDetect();
      }
      ResetTransportTrigger(kParamTrigDetectNote);
      break;

    case kParamTrigPitchUpOne:
      if (GetParam(kParamTrigPitchUpOne)->Bool() && mReferenceNote.valid)
      {
        mPitchRequestPlayheadNorm = mSampleTransport.GetPlayheadNorm();
        RebuildProcessSnapshot();
        mOfflineWorker.RequestPitchUpOne(mReferenceNote);
      }
      ResetTransportTrigger(kParamTrigPitchUpOne);
      break;

    default:
      break;
  }
}
#endif
