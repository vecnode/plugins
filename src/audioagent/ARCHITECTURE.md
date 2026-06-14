# audioagent - architecture

Embedded-sample playback engine with offline MIR. Real-time audio runs in the host's `ProcessBlock`; heavy analysis runs on a background worker. The host plugin (e.g. CamelotSynth) owns IGraphics and parameter wiring.

---

## Directory layout

```
src/audioagent/
├── audioagent.h              Umbrella include
├── iplug_bridge.h            Minimal iPlug2 DSP types (sample, smoothers, heapbuf)
├── SamplerEngine.h           Facade — transport + worker + waveform orchestration
├── CMakeLists.txt            INTERFACE lib; links audioflux
├── analysis/
│   ├── OfflineSampleWorker.h
│   ├── SampleNoteDetector.h
│   ├── SamplePitchProcessor.h
│   └── SampleProcessSnapshot.h
├── dsp/
│   ├── SampleBuffer.h
│   ├── SamplePlayer.h
│   ├── SampleTransport.h
│   └── OutputLimiter.h
├── model/
│   └── WaveformEnvelope.h
└── camelot/
    └── WheelLayout.h         Pure geometry + hit-test (no IGraphics)
```

**Plugin shell (CamelotSynth):** `CamelotSynth.h/.cpp` — params, meter, `OnIdle` UI sync. UI under `CamelotSynth/src/ui/` and `CamelotSynth/src/editor/`.

---

## Threading and data flow

```
[Embedded WAV]
      │
      ▼
 SampleBuffer ──LoadEmbedded──► SamplePlayer ◄── atomic swap (pitch result)
      │                                │
      ▼                                ▼
WaveformEnvelope              ProcessBlock → outputs
 (1024 peak buckets)                  │
      │                         (host IPeakAvgSender)
      │                                │
      └──── SamplerEngine::Tick() ─────┘
                         │
              OfflineSampleWorker (background)
                         │
              audioFlux PitchYIN / pitchShift
```

- **Audio thread:** `SamplerEngine::ProcessBlock` → `SampleTransport::ProcessBlock`. At block start, `ApplyPendingSwapIfReady()` may replace the playback buffer (O(1)).
- **Audio thread (param scheduling):** `SchedulePlay/Pause/Stop/Seek` with `sampleOffset`; detect/+1 set atomic flags only.
- **UI timer:** `SamplerEngine::Tick()` — snapshot capture, worker queue, pitch buffer staging, `WorkerUiState` for labels.
- **Worker thread:** PitchYIN or pitchShift on immutable snapshot; never touches UI.

Cross-thread pitch handoff: `StageProcessedBuffer` (Tick) → `ApplyPendingSwapIfReady` (ProcessBlock).

---

## SamplerEngine API


| Method                                    | Thread                | Purpose                                     |
| ----------------------------------------- | --------------------- | ------------------------------------------- |
| `LoadEmbedded`                            | Init / OnReset        | Decode embedded WAV, build waveform         |
| `ProcessBlock`                            | Audio                 | Mix sample + apply pending swap             |
| `Schedule`*                               | Audio (OnParamChange) | Sample-accurate transport                   |
| `RequestDetectNote` / `RequestPitchUpOne` | Audio                 | O(1) atomic flags                           |
| `Tick`                                    | UI timer              | Queue jobs, poll worker, stage pitch result |
| `GetWorkerUiState`                        | UI timer              | Detect/pitch phase for editor labels        |


---

## Sample transport and seek (de-clicking)

### Why clicks happened

1. Immediate seek from `OnParamChange` instead of `sampleOffset` inside `ProcessBlock`
2. Crossfade restart on every drag event
3. Dip-through-silence between unrelated buffer regions
4. Audible seeks while paused or during transport fades
5. Mouse-down DSP commit before the user finishes scrubbing

### Architecture

```
UI (WaveformTrackControl)
      │  drag / click-release → kParamSeek
      ▼
OnParamChange → SamplerEngine::ScheduleSeek(norm, offset)
      ▼
SamplePlayer — atomic pending seek (latest norm wins)
      ├── not actively playing  → silent seek
      └── actively playing      → retargetable linear crossfade (20 ms)
```

**Silent seeks:** paused, stopped, or transport fade active.  
**Audible seeks:** playing, 20 ms linear dual-head crossfade (`kSeekCrossfadeMs`). Rapid scrub retargets incoming head only.

Transport play/pause/stop uses 12 ms equal-power fade (`kTransportFadeMs`).

---

## Offline analysis (audioFlux)

### Note detection (PitchYIN)

- Middle **75%** of file, FFT 4096, slide 1024
- Range 27–2000 Hz, threshold **0.12**
- Weighted MIDI histogram → `DetectedNote`

### Pitch +1 (offline pitchShift)

- Phase vocoder + time-stretch + resample
- Result staged to `SampleTransport`; atomic swap at block boundary
- Label transposed via `SampleNoteDetector::Transpose`

---

## Camelot wheel (`camelot/WheelLayout`)

- **12 spokes × 3 zones** → B1–B36
- `BuildLineLayout` / `BuildBlockRegions` from axis-aligned `Bounds`
- `HitTestBlockIndex` for pointer input
- Rendering stays in the host (`CamelotCircleControl` maps `IRECT` ↔ `Bounds`)

---

## Module dependency rules


| Layer           | May include                     | Must not                  |
| --------------- | ------------------------------- | ------------------------- |
| `analysis/`     | audioFlux, `dsp/SampleBuffer.h` | IGraphics, `ProcessBlock` |
| `dsp/`          | `iplug_bridge.h`                | IGraphics, audioFlux      |
| `model/`        | `dsp/`                          | IGraphics                 |
| `camelot/`      | Standard C++ only               | IGraphics, iPlug          |
| `SamplerEngine` | All audioagent modules          | IGraphics, Plugin class   |


---

## Extension notes

- New semitone steps: extend `OfflineSampleWorker::JobType` + `SamplePitchProcessor`
- Real-time pitch: separate RT path; keep offline worker unchanged
- Non-iPlug hosts: replace `LoadEmbedded` with `AssignFromFloat` on a decoded buffer
- Highlight detected note on wheel: map `DetectedNote.midiNote` → `WheelLayout::HitTestBlockIndex` in the plugin UI

