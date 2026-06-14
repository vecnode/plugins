#include "CamelotSynth.h"
#include "CamelotSynthEditor.h"
#include "WaveformControl.h"
#include "UiPaintPolicy.h"
#include "IPlug_include_in_plug_src.h"

namespace
{
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
  mEngine.ProcessBlock(outputs, nChans, nFrames, targetGain, mGainSmoother);

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

      if (mEngine.ConsumeWaveformDirty() && mEngine.GetWaveform().IsValid())
      {
        pCtrl->SetEnvelope(mEngine.GetWaveform().Max(), mEngine.GetWaveform().Min());

        if (auto* pLength = pGraphics->GetControlWithTag(kCtrlTagSampleLength))
        {
          pLength->As<::igraphics::SampleLengthLabelControl>()->SetDurationSeconds(
            mEngine.GetTransport().GetBuffer().GetDurationSeconds());
        }
      }

      if (!pCtrl->IsScrubbing() && mEngine.ShouldSyncPlayhead())
        pCtrl->SyncPlayheadFromDSP(mEngine.GetTransport().GetPlayheadNorm());

      mEngine.ClearPlayheadForce();
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
    mEngine.Tick();
    ApplyOfflineWorkerUiUpdates(pGraphics);
  }
#else
  mEngine.Tick();
#endif
}

void CamelotSynth::ApplyOfflineWorkerUiUpdates(IGraphics* pGraphics)
{
#if IPLUG_EDITOR
  const bool workerBusy = mEngine.IsWorkerBusy();
  const bool hasReference = mEngine.HasReferenceNote();
  const auto& ui = mEngine.GetWorkerUiState();

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

  if (ui.detectChanged)
  {
    if (auto* pNote = pGraphics->GetControlWithTag(kCtrlTagDetectedNote))
    {
      auto* pLabel = pNote->As<::igraphics::DetectedNoteLabelControl>();
      switch (ui.detectPhase)
      {
        case audioagent::OfflineSampleWorker::Phase::Queued:
        case audioagent::OfflineSampleWorker::Phase::Running:
          pLabel->SetAnalyzing();
          break;
        case audioagent::OfflineSampleWorker::Phase::Succeeded:
          pLabel->SetText(ui.detectNote.text);
          break;
        case audioagent::OfflineSampleWorker::Phase::Failed:
          pLabel->SetText("Note: --");
          break;
        default:
          break;
      }
      ::igraphics::RequestControlRepaint(pNote);
    }
  }

  if (ui.pitchChanged)
  {
    if (auto* pNote = pGraphics->GetControlWithTag(kCtrlTagDetectedNote))
    {
      auto* pLabel = pNote->As<::igraphics::DetectedNoteLabelControl>();
      switch (ui.pitchPhase)
      {
        case audioagent::OfflineSampleWorker::Phase::Queued:
        case audioagent::OfflineSampleWorker::Phase::Running:
          pLabel->SetProcessing();
          break;
        case audioagent::OfflineSampleWorker::Phase::Succeeded:
          if (ui.pitchResult.ok)
            pLabel->SetText(mEngine.GetReferenceNote().text);
          else
            pLabel->SetText("Note: --");
          break;
        case audioagent::OfflineSampleWorker::Phase::Failed:
          pLabel->SetText(mEngine.HasReferenceNote() ? mEngine.GetReferenceNote().text : "Note: --");
          break;
        default:
          break;
      }
      ::igraphics::RequestControlRepaint(pNote);
    }
  }

  mEngine.ClearWorkerUiState();
#else
  (void) pGraphics;
#endif
}

void CamelotSynth::OnReset()
{
  mMeterSender.Reset(GetSampleRate());
  mGainSmoother.SetSmoothTime(kGainSmoothMs, GetSampleRate());
  mGainSmoother.SetValue(static_cast<sample>(GetParam(kParamGain)->Value() / 100.));
  mEngine.SetSampleRate(GetSampleRate());
  mEngine.Reset();
  LoadEmbeddedSample();
}

void CamelotSynth::LoadEmbeddedSample()
{
  mEngine.LoadEmbedded(ATMOS_SAMPLE_FN, GetSampleRate());
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
        if (!mEngine.GetTransport().IsSampleLoaded())
          LoadEmbeddedSample();

        mEngine.SchedulePlay(offset);
      }
      ResetTransportTrigger(kParamTrigPlay);
      break;

    case kParamTrigPause:
      if (GetParam(kParamTrigPause)->Bool())
        mEngine.SchedulePause(offset);
      ResetTransportTrigger(kParamTrigPause);
      break;

    case kParamTrigStop:
      if (GetParam(kParamTrigStop)->Bool())
        mEngine.ScheduleStop(offset);
      ResetTransportTrigger(kParamTrigStop);
      break;

    case kParamSeek:
      mEngine.ScheduleSeek(static_cast<float>(GetParam(kParamSeek)->Value()), offset);
      break;

    case kParamTrigDetectNote:
      if (GetParam(kParamTrigDetectNote)->Bool())
        mEngine.RequestDetectNote();
      ResetTransportTrigger(kParamTrigDetectNote);
      break;

    case kParamTrigPitchUpOne:
      if (GetParam(kParamTrigPitchUpOne)->Bool() && mEngine.HasReferenceNote())
        mEngine.RequestPitchUpOne();
      ResetTransportTrigger(kParamTrigPitchUpOne);
      break;

    default:
      break;
  }
}
#endif
