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
│   ├── PitchStreamCache.h
│   ├── PitchStreamWorker.h
│   ├── SampleNoteDetector.h
│   ├── SamplePitchProcessor.h
│   └── SampleProcessSnapshot.h
├── dsp/
│   ├── SampleBuffer.h
│   ├── SamplePlayer.h
│   ├── SampleTransport.h
│   ├── PitchStreamPipeline.h
│   ├── OutputLimiter.h
│   ├── ProcessChain.h        IProcessStage + fixed-capacity chain
│   ├── GainStage.h           Smoothed gain stage
│   ├── HPFStage.h            30 Hz one-pole HPF (atomic bypass)
│   ├── LimiterStage.h        OutputLimiter as a chain stage
│   ├── DenormalFlush.h       Subnormal flush (avoids WDL denormal.h shadow)
│   ├── RTPitchShifter.h      Live-mode grain pitch shifter
│   ├── PitchMode.h           Quality vs Live
│   └── SimdUtils.h           Scalar SIMD hooks
├── model/
│   └── WaveformEnvelope.h
├── camelot/
│   └── WheelLayout.h         Pure geometry + hit-test (no IGraphics)
└── platform/
    └── ResourceLoader.h      Windows embedded-resource load (iPlug IPlugPaths)
```

**Plugin shell (CamelotSynth):** `CamelotSynth.h/.cpp` — params, meter, `OnIdle` UI sync. UI under `CamelotSynth/src/ui/` and `CamelotSynth/src/editor/`.

---

## Threading and data flow

```
[Embedded WAV]
      │
      ▼
 SampleBuffer ──LoadEmbedded──► SamplePlayer ──► outputs
      │              │              │
      │              │              └── PitchStreamPipeline::ReadStereo
      │              │                        │
      ▼              │                        ▼
WaveformEnvelope     │              PitchStreamWorker (audioFlux chunks)
      │              │
      └──── SamplerEngine::Tick() ───► OfflineSampleWorker (detect only)
                         │
              audioFlux PitchYIN
```

- **Audio thread:** `SamplePlayer` reads dry or pitched cache via `PitchStreamPipeline`; dry buffer is never replaced.
- **Audio thread (param scheduling):** transport + `BeginPitchStream` + atomic detect flag only.
- **UI timer:** kick pitch scheduler, detect worker queue; +1 updates label immediately via `pitchLabelChanged`.
- **PitchStreamWorker:** audioFlux `pitchShift` on 4096-sample windows ahead of playhead.
- **OfflineSampleWorker:** PitchYIN detect only.

### Real-time pitch (+1) — block read-ahead pipeline

| Property | Value |
| -------- | ----- |
| Algorithm | audioFlux `pitchShift` on **~10 s offline blocks** (`PitchStreamWorker`) |
| Read-ahead | Worker keeps **2 blocks (~20 s)** ahead of the playhead |
| Playback | Dry until the current block is ready, then continuous pitched audio for the block |
| Range | +1 per press, cumulative to +12 semitones (always shifted from dry) |
| Paused | Worker fills blocks while stopped; pitched audio on next play when ready |

```
SamplePlayer
      │
      ├── dry buffer (immutable)
      └── PitchStreamPipeline::ReadStereo
                │
                ├── block ready → pitched L/R (full audioFlux quality)
                └── block pending → dry at playhead
                │
                ▼
          PitchStreamWorker (background thread, 10 s blocks)
```

Full-buffer swap (`ReplaceBufferKeepingTransport`) is retained for optional offline bake only — live +1 uses the stream cache.

---

## SamplerEngine API


| Method                                    | Thread                | Purpose                                     |
| ----------------------------------------- | --------------------- | ------------------------------------------- |
| `LoadEmbedded`                            | Init / OnReset        | Decode embedded WAV, build waveform         |
| `ProcessBlock`                            | Audio                 | Mix sample via pitch stream or dry          |
| `Schedule`*                               | Audio (OnParamChange) | Sample-accurate transport                   |
| `RequestDetectNote` / `RequestPitchUpOne` | Audio                 | O(1) flags; +1 starts `BeginPitchStream`    |
| `Tick`                                    | UI timer              | Kick pitch scheduler, queue detect, poll UI |
| `GetWorkerUiState`                        | UI timer              | Detect phase + instant pitch label updates  |
| `IsPitchCatchingUp`                       | UI                    | Cache behind playhead or worker busy        |


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

### Pitch +1 (block read-ahead)

- `PitchStreamWorker` runs full audioFlux `pitchShift` on **10-second blocks** ahead of the playhead
- Worker prefetches **2 blocks (~20 s)** while playback uses the ready block
- `PitchStreamPipeline::ReadStereo` plays pitched audio only inside ready blocks; dry until the first block completes
- Label transposed immediately via `SampleNoteDetector::Transpose`
- Works during playback **or** paused (stream fills while stopped)

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

## Real-time processing chain

`SamplePlayer` renders the source (dry or `PitchStreamPipeline`) plus seek/transport crossfades into pre-allocated scratch buffers, then runs a block-based `ProcessChain` of `IProcessStage` objects (`SamplePlayer::PrepareProcessChain`):

```
source (dry | PitchStreamPipeline)
  → seek / transport crossfade        (per-sample, in SamplePlayer)
  → scratch buffers + DenormalFlush / SimdUtils::FlushDenormalsBlock
  → ProcessChain.ProcessBlock:
        HPFStage      (optional, kParamHPF)
        GainStage     (host LogParamSmooth)
        LimiterStage  (wraps OutputLimiter, always on)
  → outputs
```

Add a new effect by implementing `IProcessStage` and inserting it in `PrepareProcessChain` (see [DEVELOPMENT_PLAN.md](../../DEVELOPMENT_PLAN.md)).

### RT contract checklist

| Rule | Current |
|------|---------|
| No audioFlux in `ProcessBlock` | Yes |
| No mutex on audio thread | Yes — pitch worker is kicked from `SamplerEngine::Tick()`, not `ProcessBlock` |
| No heap alloc in hot path | Yes — scratch buffers and chain stages bound at load/reset |
| Pitch cache: worker write vs audio read | Per-block atomic ready flags + release fence before a block is marked readable (`PitchStreamCache`) |
| Documented chain order | Yes (this section + CamelotSynth README) |
| Enforced in CI | `scripts/check-rt-audio.ps1` greps the audio path for forbidden APIs |

---

## Extension notes

- New semitone steps: adjust `SamplerEngine::RequestPitchUpOne` / add `RequestPitchDownOne`
- Composable chain: `ProcessChain` + stages — see [DEVELOPMENT_PLAN.md](../../DEVELOPMENT_PLAN.md)
- Live RT pitch: add `RTPitchShifter` alongside read-ahead pipeline
- Offline bake (optional): re-enable worker pitchShift when stopped for export-quality freeze
- Non-iPlug hosts: replace `LoadEmbedded` with `AssignFromFloat` on a decoded buffer
- Highlight detected note on wheel: map `DetectedNote.midiNote` → `WheelLayout::HitTestBlockIndex` in the plugin UI

