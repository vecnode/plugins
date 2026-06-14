# CamelotSynth

Embedded-sample player built on iPlug2. Real-time audio runs in `ProcessBlock`; the editor is a fixed-layout IGraphics UI with transport, a Camelot note circle, gain control, and waveform.

## Plugin structure

```
CamelotSynth/
├── CamelotSynth.h / .cpp     Plugin entry — params, messages, DSP, OnIdle, OnUIOpen
├── CamelotSynthEditor.h      Thin include → src/editor/Attach.h
├── config.h                  Dimensions, bundle IDs, embedded resource names
├── CMakeLists.txt            iPlug2 target + third_party/audioFlux link
├── resources/
│   ├── audio/                Embedded WAV (copied from ../assets/ at configure time)
│   └── fonts/
└── src/
    ├── analysis/             Offline MIR on the loaded sample (not real-time)
    │   ├── SampleNoteDetector.h   PitchYIN fundamental → note name
    │   └── AsyncSampleAnalyzer.h  Background worker; never blocks DSP/UI
    ├── dsp/                  Real-time audio (no IGraphics)
    │   ├── SampleBuffer.h    Decode embedded WAV; hold interleaved channel pointers
    │   ├── SamplePlayer.h    Play / pause / stop / scheduled seek; dip-through-silence
    │   ├── SampleTransport.h Buffer + player facade; sample-accurate transport/seek
    │   └── OutputLimiter.h   Output peak limiting
    ├── model/                Derived, non-real-time display data
    │   └── WaveformEnvelope.h  Min/max peak buckets for waveform drawing
    ├── ui/
    │   ├── bridge/
    │   │   ├── UiPlayheadBridge.h   DSP → editor sync flags (OnIdle)
    │   │   └── UiPaintPolicy.h      Windows full-surface paint policy
    │   └── controls/
    │       ├── WaveformControl.h
    │       ├── PlayheadOverlayControl.h
    │       ├── GainKnobControl.h
    │       ├── CamelotCircleControl.h
    │       ├── SamplerSectionControl.h
    │       └── SamplerFooterControl.h
    └── editor/
        ├── Layout.h          Row-based region computation from plugin bounds
        ├── Styles.h          Panel colours and IVStyle presets
        └── Attach.h          Control wiring and paint-policy install
```

See `src/ARCHITECTURE.md` for threading, transport/seek behaviour, and Windows rendering notes.

## UI layout (row-based)

Regions are computed in `Layout.h` using fixed **row heights** and a shared `RowLayout` helper that places cells left-to-right (fixed width or fractional share, remainder for the last column). Attach only wires controls into named cells — no ad-hoc pixel math in `Attach.h`.

| Row | Height | Cells (left → right) |
|-----|--------|----------------------|
| Tab bar | 40 px | Logo badge |
| Transport | 56 px | Start · Pause · Stop (compact, left-aligned) |
| Middle | flex | Camelot circle + gain knob |
| Wave title | 22 px | “Sampler” |
| Wave plot | 50% of wave body | Waveform + playhead |
| Wave footer | 50% of wave body | Detect Note · **Note: …** · Length: …s |

Compact button size: `100×44` design base at `0.5` scale → **50×22** px (transport and Detect Note).

## Note detection (audioFlux PitchYIN)

Triggered by **Detect Note**. Analysis runs on a **dedicated worker thread** — `ProcessBlock` and the UI message loop are never blocked.

### Threading

```
UI click → kParamTrigDetectNote → OnParamChange (O(1) queue only)
                                        ↓
                              AsyncSampleAnalyzer worker
                                        ↓
                              OnIdle → SyncAnalysisEditorState → footer label
```

| Thread | Work |
|--------|------|
| Audio (`ProcessBlock`) | Sample playback, gain, metering only |
| Audio/UI (`OnParamChange`) | Queue analysis job; copy snapshot already prepared at load |
| Worker | `SampleNoteDetector::AnalyzeMono` (audioFlux YIN) |
| UI (`OnIdle`) | Poll result, update label, enable/disable button |

Mono snapshot is built once when the embedded WAV loads (`RebuildAnalysisSnapshot`). The worker reads that immutable copy — no races with playback.

### Algorithm

| Step | Detail |
|------|--------|
| Library | [audioFlux](https://github.com/libAudioFlux/audioFlux) `mir/_pitch_yin.h` |
| Method | **YIN** — difference function + cumulative mean normalized difference; parabolic trough interpolation |
| Region | Middle **75%** of file (skip 12.5% attack/release tails) |
| Window | `radix2Exp=12` → FFT 4096; `slideLength=1024`; `autoLength=2048` |
| Range | 27–2000 Hz; YIN threshold **0.12** |
| Gating | Per-frame RMS + YIN confidence (`minArr`); discard weak frames |
| Aggregation | Weighted **MIDI histogram** (±1 semitone refine) → robust fundamental for pads/chords |
| Output | `Note: D5` (12-TET, A4=440); `confidence` stored for future UI |

Reference: de Cheveigné & Kawahara, *YIN, a fundamental frequency estimator for speech and music*, JASA 2002.

### Planned follow-ups

- Map `DetectedNote` → Camelot circle block highlight
- Real-time tracking while playing (separate low-latency path; offline detect stays on worker)

## Libraries

| Library | Location | Role |
|---------|----------|------|
| [iPlug2](https://github.com/iPlug2/iPlug2) | Sibling `../iPlug2` | Plugin framework, VST3 host API, IGraphics UI |
| [audioFlux](https://github.com/libAudioFlux/audioFlux) | `../third_party/audioFlux` | Pitch / MIR analysis (YIN C API) |

### Third-party setup (once)

audioFlux upstream sources are fetched, not committed:

```powershell
.\scripts\setup-third-party.ps1
```

Pinned version: `third_party/audioFlux/VERSION` (currently **0.1.9**).

## Build

```powershell
.\scripts\setup-third-party.ps1
.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release -Install
```

Output: `CamelotSynth/build/out/CamelotSynth.vst3/`

Install target: `%LOCALAPPDATA%\Programs\Common\VST3\CamelotSynth.vst3` (close the host before reinstalling).
