#include "CamelotSynth.h"
#include "CamelotSynthEditor.h"
#include "WaveformControl.h"
#include "UiPaintPolicy.h"
#include "IPlug_include_in_plug_src.h"

namespace
{
constexpr double kGainSmoothMs = 20.;
constexpr int kHiddenTransportFlags = IParam::kFlagMeta | IParam::kFlagCannotAutomate;
constexpr int kStateMagic = 0x434D5354; // 'CMST'
constexpr int kStateVersion = 1;
}

CamelotSynth::CamelotSynth(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // RT chain order (audioagent ProcessChain): source → transport mix → HPF → gain → limiter
  GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100.0, 0.01, "%");
  GetParam(kParamHPF)->InitBool("HPF", false);
  GetParam(kParamPitchMode)->InitEnum("Pitch Mode", 1, {"Quality", "Live"});
  GetParam(kParamTrigPlay)->InitBool("Play", false, "", kHiddenTransportFlags);
  GetParam(kParamTrigPause)->InitBool("Pause", false, "", kHiddenTransportFlags);
  GetParam(kParamTrigStop)->InitBool("Stop", false, "", kHiddenTransportFlags);
  GetParam(kParamSeek)->InitDouble("Seek", 0., 0., 1., 0.001, "", kHiddenTransportFlags);
  GetParam(kParamTrigDetectNote)->InitBool("Detect Note", false, "", kHiddenTransportFlags);
  GetParam(kParamTrigPitchDownOne)->InitBool("Pitch -1", false, "", kHiddenTransportFlags);
  GetParam(kParamTrigPitchUpOne)->InitBool("Pitch +1", false, "", kHiddenTransportFlags);
  GetParam(kParamTrigPitchReset)->InitBool("Pitch Reset", false, "", kHiddenTransportFlags);

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
    SyncPitchControls(pGraphics);
  }
#else
  mEngine.Tick();
#endif
}

void CamelotSynth::SyncPitchControls(IGraphics* pGraphics)
{
#if IPLUG_EDITOR
  const bool hasReference = mEngine.HasReferenceNote();
  const int semitones = mEngine.GetPitchSemitones();

  if (auto* pPitchUp = pGraphics->GetControlWithTag(kCtrlTagPitchUpOne))
  {
    pPitchUp->SetDisabled(!hasReference);
    ::igraphics::RequestControlRepaint(pPitchUp);
  }

  if (auto* pPitchDown = pGraphics->GetControlWithTag(kCtrlTagPitchDownOne))
  {
    pPitchDown->SetDisabled(!hasReference);
    ::igraphics::RequestControlRepaint(pPitchDown);
  }

  if (auto* pPitchReset = pGraphics->GetControlWithTag(kCtrlTagPitchReset))
  {
    pPitchReset->SetDisabled(!hasReference || semitones == 0);
    ::igraphics::RequestControlRepaint(pPitchReset);
  }
#else
  (void) pGraphics;
#endif
}

void CamelotSynth::ApplyOfflineWorkerUiUpdates(IGraphics* pGraphics)
{
#if IPLUG_EDITOR
  const bool workerBusy = mEngine.IsWorkerBusy();
  const auto& ui = mEngine.GetWorkerUiState();

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

  if (ui.pitchLabelChanged)
  {
    if (auto* pNote = pGraphics->GetControlWithTag(kCtrlTagDetectedNote))
    {
      pNote->As<::igraphics::DetectedNoteLabelControl>()->SetText(ui.pitchLabelNote.text);
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
  mEngine.SetHPFEnabled(GetParam(kParamHPF)->Bool());
  mEngine.SetPitchMode(GetParam(kParamPitchMode)->Int() == 1 ? audioagent::PitchMode::Live
                                                               : audioagent::PitchMode::Quality);
  LoadEmbeddedSample();
}

bool CamelotSynth::SerializeState(IByteChunk& chunk) const
{
  chunk.Put(&kStateMagic);
  chunk.Put(&kStateVersion);
  const int hpf = GetParam(kParamHPF)->Int();
  const int pitchMode = GetParam(kParamPitchMode)->Int();
  chunk.Put(&hpf);
  chunk.Put(&pitchMode);
  return true;
}

int CamelotSynth::UnserializeState(const IByteChunk& chunk, int startPos)
{
  int magic = 0;
  int version = 0;
  startPos = chunk.Get(&magic, startPos);
  if (magic != kStateMagic)
    return startPos;

  startPos = chunk.Get(&version, startPos);
  if (version < 1)
    return startPos;

  int hpf = 0;
  int pitchMode = 0;
  startPos = chunk.Get(&hpf, startPos);
  startPos = chunk.Get(&pitchMode, startPos);

  GetParam(kParamHPF)->Set(hpf);
  GetParam(kParamPitchMode)->Set(pitchMode);
  mEngine.SetHPFEnabled(hpf != 0);
  mEngine.SetPitchMode(pitchMode == 1 ? audioagent::PitchMode::Live : audioagent::PitchMode::Quality);

  return startPos;
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
    case kParamGain:
      break;

    case kParamHPF:
      mEngine.SetHPFEnabled(GetParam(kParamHPF)->Bool());
      break;

    case kParamPitchMode:
      mEngine.SetPitchMode(GetParam(kParamPitchMode)->Int() == 1 ? audioagent::PitchMode::Live
                                                                   : audioagent::PitchMode::Quality);
      break;

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

    case kParamTrigPitchDownOne:
      if (GetParam(kParamTrigPitchDownOne)->Bool() && mEngine.HasReferenceNote())
        mEngine.RequestPitchDownOne();
      ResetTransportTrigger(kParamTrigPitchDownOne);
      break;

    case kParamTrigPitchUpOne:
      if (GetParam(kParamTrigPitchUpOne)->Bool() && mEngine.HasReferenceNote())
        mEngine.RequestPitchUpOne();
      ResetTransportTrigger(kParamTrigPitchUpOne);
      break;

    case kParamTrigPitchReset:
      if (GetParam(kParamTrigPitchReset)->Bool())
        mEngine.RequestPitchReset();
      ResetTransportTrigger(kParamTrigPitchReset);
      break;

    default:
      break;
  }
}
#endif
