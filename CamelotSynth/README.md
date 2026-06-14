# CamelotSynth

iPlug2 VST3 shell for the [audioagent](../src/audioagent/) sampler library. Playback, gain, and metering run on the **audio thread**; note detection and pitch shifting run on a **background worker**. The editor stays responsive because no heavy MIR work runs in `ProcessBlock`, `OnParamChange`, or the IGraphics paint path.

## Features

| Feature | Implementation |
|---------|----------------|
| Sample playback | audioagent `SampleBuffer` → `SamplePlayer` |
| Transport | Sample-accurate play / pause / stop / seek via hidden meta parameters |
| Waveform | 1024-point peak envelope; playhead overlay synced on `OnIdle` |
| Note detect | audioFlux **PitchYIN** on a worker thread (via audioagent) |
| Pitch +1 | audioFlux offline **pitchShift** (+1 semitone); atomic buffer swap at block boundary |
| Camelot wheel | audioagent `WheelLayout` geometry; `CamelotCircleControl` draws in IGraphics |

## Architecture split

| Layer | Location |
|-------|----------|
| DSP + MIR + wheel math | [`../src/audioagent/`](../src/audioagent/) — see [README](../src/audioagent/README.md) and [ARCHITECTURE](../src/audioagent/ARCHITECTURE.md) |
| iPlug2 plugin shell | `CamelotSynth.h/.cpp` — params, `SamplerEngine`, meter |
| IGraphics UI | `src/ui/`, `src/editor/` |

## Source layout

```
CamelotSynth/
├── CamelotSynth.h / .cpp     Params, ProcessBlock, OnIdle — delegates to audioagent::SamplerEngine
├── CamelotSynthEditor.h      Thin include → src/editor/Attach.h
├── config.h                  Dimensions, bundle IDs, embedded WAV resource name
├── CMakeLists.txt            iPlug2 target + links audioagent
└── src/
    ├── ui/                   IGraphics controls + Windows paint policy
    └── editor/               Layout, Styles, Attach
```

## UI layout

Row-based regions from `Layout.h`:

| Row | Contents |
|-----|----------|
| Tab bar | Logo badge |
| Transport | Start · Pause · Stop |
| Middle | Camelot circle + gain knob |
| Wave title | “Sampler” |
| Wave plot | Waveform + playhead overlay |
| Footer row 1 | Detect Note · **Note: …** · Length |
| Footer row 2 | **+1** (under Detect) |

## Libraries

| Library | Location | Role |
|---------|----------|------|
| [audioagent](../src/audioagent/) | `src/audioagent` | DSP, MIR, Camelot wheel geometry |
| [iPlug2](https://github.com/iPlug2/iPlug2) | Sibling `../iPlug2` | VST3 framework, IGraphics, parameters |
| [audioFlux](https://github.com/libAudioFlux/audioFlux) | `third_party/audioFlux` | Linked by audioagent |

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
- Real-time pitch tracking while playing (separate RT path)
