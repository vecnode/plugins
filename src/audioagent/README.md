# audioagent

Pure C++ library for embedded-sample playback, offline MIR (note detection and pitch shift), waveform modelling, and Camelot wheel geometry. Host plugins link it and wire their own UI — [CamelotSynth](../../CamelotSynth/) is the reference iPlug2 integration.

## What lives here

| Module | Role |
|--------|------|
| `dsp/` | Real-time playback — `SampleBuffer`, `SamplePlayer`, `SampleTransport`, `OutputLimiter` |
| `analysis/` | Background worker + audioFlux PitchYIN / pitchShift |
| `model/` | `WaveformEnvelope` (display data derived from buffers) |
| `camelot/` | `WheelLayout` — B1–B36 spoke/zone geometry and hit-testing |
| `SamplerEngine.h` | Facade orchestrating transport, worker jobs, and buffer swaps |

## Thread model

| Context | Allowed | Forbidden |
|---------|---------|-----------|
| `ProcessBlock` | Mix, gain, atomic buffer swap | audioFlux, heap churn, locks |
| Param scheduling | O(1) transport + atomic request flags | Full-buffer copies, MIR |
| `SamplerEngine::Tick()` | Snapshot capture, worker queue, pitch staging | Blocking on worker |
| Worker thread | audioFlux YIN / pitchShift | Any UI |

See [ARCHITECTURE.md](ARCHITECTURE.md) for transport/seek de-clicking and module dependency rules.

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

// OnIdle (UI timer)
mEngine.Tick();
const auto& ui = mEngine.GetWorkerUiState();
// map ui.detectPhase / ui.pitchPhase to editor controls
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
