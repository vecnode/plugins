#include "CamelotSynth.h"
#include "CamelotSynthEditor.h"
#include "WaveformControl.h"
#include "UiPaintPolicy.h"
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

  // Full-surface repaint when the host opens the editor (avoids host background bleed).
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
  // Meter peaks are sent here; each update marks the meter control dirty on the UI thread.
  mMeterSender.TransmitData(*this);

#if IPLUG_EDITOR
  if (auto* pGraphics = GetUI())
  {
    // Graphics may not paint until the first idle tick after LayoutUI; prime once here too.
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

    SyncAnalysisEditorState();
  }
#endif
}

void CamelotSynth::SyncAnalysisEditorState()
{
#if IPLUG_EDITOR
  if (auto* pGraphics = GetUI())
  {
    AsyncSampleAnalyzer::Phase phase = AsyncSampleAnalyzer::Phase::Idle;
    DetectedNote note;
    if (!mSampleAnalyzer.ConsumeEditorUpdate(phase, note))
      return;

    if (auto* pNote = pGraphics->GetControlWithTag(kCtrlTagDetectedNote))
    {
      auto* pLabel = pNote->As<::igraphics::DetectedNoteLabelControl>();
      switch (phase)
      {
        case AsyncSampleAnalyzer::Phase::Queued:
        case AsyncSampleAnalyzer::Phase::Running:
          pLabel->SetAnalyzing();
          break;
        case AsyncSampleAnalyzer::Phase::Succeeded:
          pLabel->SetText(note.text);
          break;
        case AsyncSampleAnalyzer::Phase::Failed:
          pLabel->SetText("Note: --");
          break;
        default:
          break;
      }
      ::igraphics::RequestControlRepaint(pNote);
    }

    if (auto* pButton = pGraphics->GetControlWithTag(kCtrlTagDetectNote))
    {
      const bool busy = phase == AsyncSampleAnalyzer::Phase::Queued
                     || phase == AsyncSampleAnalyzer::Phase::Running;
      pButton->SetDisabled(busy);
      ::igraphics::RequestControlRepaint(pButton);
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
  LoadEmbeddedSample();
}

void CamelotSynth::LoadEmbeddedSample()
{
  if (mSampleTransport.LoadEmbedded(ATMOS_SAMPLE_FN, GetSampleRate()))
  {
    mWaveformEnvelope.BuildFrom(mSampleTransport.GetBuffer(), kWaveformPoints);
    RebuildAnalysisSnapshot();
  }
}

void CamelotSynth::RebuildAnalysisSnapshot()
{
  const SampleBuffer& buffer = mSampleTransport.GetBuffer();
  if (!buffer.IsLoaded())
  {
    mSampleAnalyzer.SetSnapshot({});
    return;
  }

  AsyncSampleAnalyzer::Snapshot snapshot;
  snapshot.sampleRate = static_cast<int>(std::lround(buffer.GetHostSampleRate()));
  snapshot.mono.resize(static_cast<size_t>(buffer.GetLength()));

  const sample* left = buffer.GetLeft();
  const sample* right = buffer.GetRight();
  for (int i = 0; i < buffer.GetLength(); ++i)
    snapshot.mono[static_cast<size_t>(i)] = static_cast<float>((left[i] + right[i]) * 0.5);

  mSampleAnalyzer.SetSnapshot(std::move(snapshot));
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
          RebuildAnalysisSnapshot();

        mSampleAnalyzer.RequestAnalysis();
      }
      ResetTransportTrigger(kParamTrigDetectNote);
      break;

    default:
      break;
  }
}
#endif
