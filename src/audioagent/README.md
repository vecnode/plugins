# audioagent

Pure C++ library for embedded-sample playback, streaming pitch (read-ahead), offline MIR (note detection), waveform modelling, and Camelot wheel geometry. Host plugins link it and wire their own UI — [CamelotSynth](../../CamelotSynth/) is the reference iPlug2 integration.

## What lives here

| Module | Role |
|--------|------|
| `dsp/` | Real-time playback — `ProcessChain`, `GainStage`, `HPFStage`, `LimiterStage`, `RTPitchShifter`, transport, pitch pipeline |
| `analysis/` | Background workers + audioFlux (detect on worker; pitch blocks on `PitchStreamWorker`) |
| `model/` | `WaveformEnvelope` (display data derived from buffers) |
| `camelot/` | `WheelLayout` — B1–B36 spoke/zone geometry and hit-testing |
| `SamplerEngine.h` | Facade orchestrating transport, streaming pitch, detect worker, waveform |

## Real-time processing chain (today)

All of this runs on the host audio thread inside `SamplerEngine::ProcessBlock` → `SampleTransport::ProcessBlock` → `SamplePlayer::ProcessBlock`:

| Step | Component | Notes |
|------|-----------|-------|
| 1 | Source read | Dry sample; ±1 via `RTPitchShifter` with wet/dry crossfade (no block waits) |
| 2 | Crossfade mix | 20 ms seek crossfade; 12 ms equal-power transport fade |
| 3 | HPF | Optional 30 Hz (`HPFStage`) |
| 4 | Gain | Host passes `LogParamSmooth` |
| 5 | Limiter | `LimiterStage` — soft knee + stereo peak limiter |

**Pitch +1 / −1** latch at exactly ±1 from the detected reference (no stacking). Use **Reset** to return to the detected note. Two engines, selected by `kParamPitchMode`: **Quality** (background audioFlux read-ahead) and **Live** (`RTPitchShifter`, a two-tap crossfading delay line on the audio thread, ~21 ms latency, always outputs audio). See [ARCHITECTURE.md](ARCHITECTURE.md#pitch-shifting--two-modes).

### Sample size in memory

The whole file is decoded to the host rate and held in RAM (`sample` = `double` → 16 B/stereo frame); there is no disk streaming. Live pitch adds **no** per-frame memory (fixed ring); Quality pitch roughly doubles it (full-length pitched cache). Rough x64 limits at 48 kHz — Quality ≈ 11 min/GB, Live/no-pitch ≈ 22 min/GB; hard ceilings are the ~4 GB WAV `data` chunk and the `int` frame count (≈12 h). Full breakdown: [ARCHITECTURE.md → Memory and maximum sample length](ARCHITECTURE.md#memory-and-maximum-sample-length).

## Thread model

| Context | Allowed | Forbidden |
|---------|---------|-----------|
| `ProcessBlock` | Mix, gain, read pitched cache / dry fallback | audioFlux, heap churn, locks |
| Param scheduling | O(1) transport + `BeginPitchStream` | Full-buffer copies, MIR |
| `SamplerEngine::Tick()` | Kick pitch scheduler, detect worker queue | Blocking on worker |
| `PitchStreamWorker` | audioFlux pitchShift per **~10 s block** | Any UI, ProcessBlock |
| `OfflineSampleWorker` | PitchYIN detect | ProcessBlock |

See [ARCHITECTURE.md](ARCHITECTURE.md) for transport/seek de-clicking and module dependency rules.

## RT DSP guarantees (implemented)

| Property | How |
|----------|-----|
| **Composable chain** | `ProcessChain` + `IProcessStage`, assembled in `SamplePlayer::PrepareProcessChain` (HPF → gain → limiter) |
| **Scheduler placement** | Pitch worker kicked from `SamplerEngine::Tick()`, never from `ProcessBlock` |
| **Cache safety** | `PitchStreamCache` uses per-block atomic ready flags + release fence before a block is readable |
| **Two pitch modes** | Quality read-ahead for DJ-style ±1 steps; low-latency `RTPitchShifter` (`PitchMode::Live`) for live performance |
| **RT audit** | `scripts/check-rt-audio.ps1` greps the audio path for forbidden APIs |

Still open (see [DEVELOPMENT_PLAN.md](../../DEVELOPMENT_PLAN.md) "Future work"): headless render benchmark (`scripts/benchmark-render.ps1` is a stub), a TSan CI target, and AVX/NEON `SimdUtils` implementations.

## iPlug2 integration

audioagent is framework-agnostic C++ but uses a **minimal iPlug2 DSP surface** for host compatibility:

- `sample` type and `LogParamSmooth` via `iplug_bridge.h`
- `WDL_TypedBuf` / `heapbuf` for RT-safe buffers
- `IPlugPaths` for embedded WAV resource loading (`gHINSTANCE` on Windows)

A plugin owns parameters and IGraphics; it instantiates `audioagent::SamplerEngine` and delegates:

```cpp
// ProcessBlock
mEngine.ProcessBlock(outputs, nChans, nFrames, targetGain, mGainSmoother);

// OnParamChange
mEngine.SchedulePlay(sampleOffset);
mEngine.RequestDetectNote();
mEngine.RequestPitchUpOne();   // latch +1 until Reset or -1
mEngine.RequestPitchDownOne(); // latch -1 until Reset or +1
mEngine.RequestPitchReset();   // back to detected note

// OnIdle (UI timer)
mEngine.Tick();
const auto& ui = mEngine.GetWorkerUiState();
// ui.detectPhase for detect; ui.pitchLabelChanged for +1 label
```

## Third-party dependencies

| Library | Location | Linked by |
|---------|----------|-----------|
| [audioFlux](https://github.com/libAudioFlux/audioFlux) 0.1.9 | `third_party/audioFlux` | `audioagent` (static) |

Setup once:

```powershell
.\scripts\setup-third-party.ps1
```

## CMake

```cmake
add_subdirectory("${CMAKE_SOURCE_DIR}/../src/audioagent" "${CMAKE_BINARY_DIR}/audioagent")
target_link_libraries(MyPlugin-vst3 PRIVATE audioagent)
```

`audioagent` is an `INTERFACE` library: headers + propagated include paths + `audioflux` link. iPlug2 headers come from `IPLUG2_DIR` (cached by the audioagent CMakeLists).

## Build CamelotSynth (reference host)

```powershell
.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release -Install
```

## Roadmap

Phased plan: [DEVELOPMENT_PLAN.md](../../DEVELOPMENT_PLAN.md) — ProcessChain, RT pitch mode, Camelot wheel note highlight, library hardening.
