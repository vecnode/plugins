# CamelotSynth

Embedded-sample player for iPlug2 (VST3). Playback, gain, and metering run on the **audio thread**; note detection and pitch shifting run on a **background worker**. The editor stays responsive because no heavy MIR work runs in `ProcessBlock`, `OnParamChange`, or the IGraphics paint path.

## Features

| Feature | Implementation |
|---------|----------------|
| Sample playback | Embedded WAV → `SampleBuffer` → `SamplePlayer` |
| Transport | Sample-accurate play / pause / stop / seek via hidden meta parameters |
| Waveform | 1024-point peak envelope; playhead overlay synced on `OnIdle` |
| Note detect | audioFlux **PitchYIN** on a worker thread |
| Pitch +1 | audioFlux offline **pitchShift** (+1 semitone); atomic buffer swap at block boundary |

## Thread model

Three execution contexts cooperate without blocking each other:

```
┌─────────────────────────────────────────────────────────────────┐
│  UI (IGraphics + OnIdle ~60 Hz)                                 │
│  • Paint controls, meter display                                │
│  • ProcessPendingOfflineJobs — snapshot copy + queue worker     │
│  • ApplyOfflineWorkerUiUpdates — labels, waveform, stage swap   │
└───────────────┬───────────────────────────────┬─────────────────┘
                │ hidden meta params (O(1))      │ StageProcessedBuffer
                ▼                                │ (vectors + atomic flag)
┌───────────────────────────┐    ┌──────────────▼──────────────────┐
│  Audio (ProcessBlock)      │    │  Worker (OfflineSampleWorker)    │
│  • ApplyPendingSwapIfReady │    │  • PitchYIN detect               │
│  • Mix sample + gain       │    │  • pitchShift +1 semitone        │
│  • Peak meter              │    │  • Never touches IGraphics       │
└───────────────────────────┘    └──────────────────────────────────┘
```

| Context | Allowed work | Forbidden |
|---------|--------------|-----------|
| `ProcessBlock` | Mixing, smoothing, atomic buffer swap | audioFlux, heap churn, locks, file I/O |
| `OnParamChange` | Schedule transport; set atomic request flags | Full-buffer copies, MIR |
| `OnIdle` | Snapshot capture, UI updates, staging processed audio | Blocking on worker (`join`, `wait`) |
| Worker thread | audioFlux YIN / pitchShift on snapshot copy | Plugin UI, `ProcessBlock` |

**Rule:** `SampleProcessSnapshot::Capture` runs on **OnIdle only** — never on the audio thread.

## User workflow

1. **Play** the embedded sample (transport row).
2. **Detect Note** — label shows `Note: D · Octave 5` (example); reference MIDI stored for pitch jobs.
3. **+1** — sample is shifted up one semitone (D → E); waveform and label update; playback uses the new buffer after the next block boundary.

Buttons disable while the worker is busy. Pending clicks are retried on the next `OnIdle` tick when the worker becomes idle.

## Source layout

```
CamelotSynth/
├── CamelotSynth.h / .cpp     Params, ProcessBlock, OnIdle, offline job orchestration
├── config.h                  Dimensions, bundle IDs, embedded WAV resource name
├── CMakeLists.txt            iPlug2 target + audioFlux static link
└── src/
    ├── analysis/             Offline MIR (worker thread only)
    │   ├── SampleNoteDetector.h     PitchYIN → DetectedNote
    │   ├── SamplePitchProcessor.h   +N semitone via pitchShiftObj_pitchShift
    │   ├── SampleProcessSnapshot.h  Float snapshot for worker (OnIdle only)
    │   └── OfflineSampleWorker.h    Job queue + condition variable worker
    ├── dsp/                  Real-time audio
    │   ├── SampleBuffer.h    Embedded WAV decode + AssignFromFloat
    │   ├── SamplePlayer.h    Transport, seek crossfade, playhead
    │   └── SampleTransport.h Playback facade + pending buffer swap
    ├── model/
    │   └── WaveformEnvelope.h
    ├── ui/                   IGraphics controls + paint policy
    └── editor/               Layout.h, Styles.h, Attach.h
```

See [`src/ARCHITECTURE.md`](src/ARCHITECTURE.md) for transport/seek de-clicking, Windows paint policy, and module dependency rules.

## UI layout

Row-based regions from `Layout.h` (`RowLayout` helper — no ad-hoc pixel math in `Attach.h`).

| Row | Contents |
|-----|----------|
| Tab bar | Logo badge |
| Transport | Start · Pause · Stop |
| Middle | Camelot circle + gain knob |
| Wave title | “Sampler” |
| Wave plot | Waveform + playhead overlay |
| Footer row 1 | Detect Note · **Note: …** · Length |
| Footer row 2 | **+1** (under Detect) |

## Offline analysis (audioFlux)

### Note detection (PitchYIN)

| Step | Detail |
|------|--------|
| API | `mir/_pitch_yin.h` |
| Region | Middle **75%** of file (skip attack/release tails) |
| Window | FFT 4096 (`radix2Exp=12`), slide 1024 |
| Range | 27–2000 Hz; threshold **0.12** |
| Output | Weighted MIDI histogram → `Note: D · Octave 5` |

Reference: de Cheveigné & Kawahara, *YIN* (JASA 2002).

### Pitch +1 (offline pitchShift)

| Step | Detail |
|------|--------|
| API | `mir/pitchShift_algorithm.h` — phase vocoder + time-stretch + resample |
| Input | Stereo snapshot + `mReferenceNote` from detect |
| Output | Processed L/R staged to `SampleTransport`; label transposed +1 semitone |
| RT handoff | `StageProcessedBuffer` (OnIdle) → `ApplyPendingSwapIfReady` (ProcessBlock start) |

audioFlux pitch shift is **batch/offline** by design. Real-time pitch would require a separate low-latency algorithm (not used here).

## Libraries

| Library | Location | Role |
|---------|----------|------|
| [iPlug2](https://github.com/iPlug2/iPlug2) | Sibling `../iPlug2` | VST3 framework, IGraphics, parameter scheduling |
| [audioFlux](https://github.com/libAudioFlux/audioFlux) | `../third_party/audioFlux` | PitchYIN + offline pitchShift (C API, static lib) |

### Third-party setup

```powershell
.\scripts\setup-third-party.ps1
```

Pinned: `third_party/audioFlux/VERSION` (**0.1.9**). MSVC builds apply a local `exp2f` workaround in the audioFlux CMake wrapper (static CRT / iPlug2 `/MT`).

## Build & install

```powershell
.\scripts\setup-third-party.ps1
.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release -Install
```

Output: `CamelotSynth/build/out/CamelotSynth.vst3/`  
Install: `%LOCALAPPDATA%\Programs\Common\VST3\CamelotSynth.vst3` (close the host before reinstalling).

## Planned extensions

- Highlight detected note on the Camelot circle
- Additional semitone steps (−1, +12) via the same worker + swap pattern
- Real-time pitch tracking while playing (separate RT path; offline jobs unchanged)
