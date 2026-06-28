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
- **PitchStreamWorker:** audioFlux `pitchShift` on **~10 s blocks** ahead of the playhead (Quality mode).
- **OfflineSampleWorker:** PitchYIN detect only.

## Pitch shifting — two modes

`kParamPitchMode` selects how ±1 semitone is realised. Both are driven by the same **−1 / +1 / Reset** controls and both latch at exactly ±1 from the detected reference note (no stacking — `ApplyPitchSemitones` and `SetLivePitchSemitones` clamp to `[-1, +1]`).

| | **Quality** (`PitchMode::Quality`, default) | **Live** (`PitchMode::Live`) |
| --- | --- | --- |
| Engine | audioFlux `pitchShift` on background `PitchStreamWorker` | `RTPitchShifter` on the audio thread |
| Latency to pitched audio | Dry until the block is ready (block fills in the background) | Immediate (smoothed wet/dry ramp) |
| Algorithm | Phase-vocoder quality, offline-grade | Two-tap crossfading delay line (time-domain) |
| Per-tap latency | n/a | ≈ `kWindow/2` = 1024 samples (~21 ms @ 48 kHz) |
| Memory | Full-length pitched cache (see below) | **O(1)** — fixed 4096-sample ring, independent of file length |
| Best for | DJ-style fixed transpose | Live performance / instant response |

### Quality mode — block read-ahead pipeline

| Property | Value |
| -------- | ----- |
| Algorithm | audioFlux `pitchShift` on **~10 s blocks** (`PitchStreamWorker`) |
| Read-ahead | Worker keeps **2 blocks (~20 s)** ahead of the playhead (`kReadAheadBlocks`) |
| Playback | Dry until the current block is ready, then continuous pitched audio for the block |
| Range | ±1 semitone, latched from the detected note (always shifted from dry) |
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

Full-buffer swap (`ReplaceBufferKeepingTransport`) is retained for optional offline bake only — live ±1 uses the stream cache.

### Live mode — `RTPitchShifter`

A self-contained time-domain shifter run per sample in `SamplePlayer::RenderPlaybackSample` (no worker, no allocation). Input is written into a 4096-sample ring while a read offset drifts at `(1 - ratio)`; two read taps half a window apart are blended with complementary triangular weights, so a tap carries zero weight exactly when it laps the window boundary — no periodic click. It always outputs audio (dry passthrough during warmup) and the wet/dry mix is smoothed by `SamplePlayer` (`mPitchMix`). Because the ring is fixed-size, **Live mode adds no per-sample memory** regardless of how long the loaded file is.

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

## Memory and maximum sample length

The whole sample is decoded and resampled to the host rate, then held in RAM for the lifetime of the plugin (`SampleBuffer`). `sample` is `double` (iPlug2 `SAMPLE_TYPE_DOUBLE`), so the dry buffer costs **16 bytes per stereo frame**. There is no disk streaming — file length is bounded by memory, not I/O.

### Resident cost per stereo frame (host rate)

| Buffer | When | Bytes/frame |
|--------|------|-------------|
| Dry `SampleBuffer` (`double` L/R) | always | 16 |
| `PitchStreamCache` pitched copy (`float` L/R, full length) | Quality pitch engaged | +8 |
| `PitchStreamWorker` dry copy (`float` L/R, full length) | Quality pitch engaged | +8 |
| `RTPitchShifter` ring (fixed 4096) | Live pitch engaged | ~0 (~32 KB total, **not** per-frame) |

- **No pitch / Live mode:** ~16 B/frame resident. **Live pitch adds no per-frame memory** — its ring is fixed size.
- **Quality mode active:** ~32 B/frame resident, peaking ~40 B/frame during a worker burst (a transient full-length `float` copy plus the ~10 s block + audioFlux scratch).
- **One-time load peak** (`SampleBuffer::DecodeAndResample`): the embedded WAV image + source-rate temp (`double`) + host-rate output (`double`) coexist briefly — budget for roughly the source size plus `16 × (srcFrames + dstFrames)` bytes while decoding.

### Practical maximum (x64 build)

Sample-buffer budget only — add the one-time decode peak and the rest of the host/plugin footprint on top. Durations at 48 kHz:

| RAM for sample buffers | Quality mode (~32 B/frame) | Live / no pitch (~16 B/frame) |
|------------------------|----------------------------|-------------------------------|
| 256 MB | ~2.8 min | ~5.6 min |
| 512 MB | ~5.6 min | ~11 min |
| 1 GB | ~11 min | ~22 min |
| 2 GB | ~22 min | ~44 min |

(44.1 kHz is ~9% longer per byte. Multiply both columns by ~1.09.)

### Hard format/type ceilings (RAM usually bites first)

| Limit | Value | Reason |
|-------|-------|--------|
| WAV `data` chunk | ~4 GB (≈6.2 h of 16-bit stereo @ 48 k) | `pcmBytes` read as `uint32` — a RIFF/WAV format limit |
| Frame count | 2,147,483,647 frames (≈12.4 h @ 48 k) | `mLength` / `numFrames` are `int` |
| Address space | not the limit | builds are x64 (`build.ps1 -A x64`) — physical RAM binds first |

To go beyond an embedded WAV (e.g. a host-decoded file or a longer/other format), feed PCM through `SampleBuffer::AssignFromFloat`; the same per-frame budgets apply.

---

## Extension notes

- Wider transpose range: relax the `[-1, +1]` clamp in `SamplerEngine::ApplyPitchSemitones` and `SamplePlayer::SetLivePitchSemitones` (and add UI), then extend `RTPitchShifter::SetSemitones`
- New chain stages: implement `IProcessStage` and insert in `SamplePlayer::PrepareProcessChain` — see [DEVELOPMENT_PLAN.md](../../DEVELOPMENT_PLAN.md)
- Offline bake (optional): re-enable worker pitchShift when stopped for export-quality freeze
- Non-iPlug hosts: replace `LoadEmbedded` with `AssignFromFloat` on a decoded buffer
- Highlight detected note on wheel: map `DetectedNote.midiNote` → `WheelLayout::HitTestBlockIndex` in the plugin UI

