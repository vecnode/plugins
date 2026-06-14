# iPlug2 Extensions

Out-of-source plugin projects linked to a sibling iPlug2 checkout.

**CamelotSynth** is an embedded-sample player built on iPlug2. Real-time audio runs in `ProcessBlock`; the editor is a fixed-layout IGraphics UI with transport, a Camelot note circle, gain control, and waveform. DSP lives in the shared **audioagent** library under `src/audioagent/`.

## Architecture (short)

```
CamelotSynth (iPlug2 shell: params, UI, meter)
       │
       ▼
SamplerEngine ──► SampleTransport ──► SamplePlayer
                         │                  │
                         │                  ├── PitchStreamPipeline (pitched cache / dry)
                         │                  ├── gain (LogParamSmooth)
                         │                  └── OutputLimiter
                         │
                         └── background: PitchStreamWorker, OfflineSampleWorker (audioFlux)
```

- **Real-time path:** sample playback, crossfades, gain, limiting — no locks, no audioFlux on the audio thread.
- **Background path:** note detection (PitchYIN) and quality pitch shift (~10 s blocks with read-ahead).

See [src/audioagent/ARCHITECTURE.md](src/audioagent/ARCHITECTURE.md) for threading rules and [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) for the roadmap toward composable processing chains and live RT pitch.

## Layout

```
Dev/
├── iPlug2/
└── plugins/
    ├── src/
    │   └── audioagent/       # shared DSP + MIR + Camelot wheel library
    ├── CamelotSynth/       # iPlug2 plugin (uses audioagent)
    ├── assets/             # shared audio (gitignored)
    └── scripts/
```

## Setup (once)

```powershell
.\scripts\setup-iplug2.ps1
.\scripts\setup-third-party.ps1   # audioFlux for audioagent
code plugins.code-workspace
```

## Build and test in Reaper

```powershell
$p = "CamelotSynth"
.\scripts\build.ps1 -Plugin $p -Format vst3 -Config Release -Install
.\scripts\install-plugin.ps1 -Plugin $p -Format vst3
```

If Reaper has the plugin loaded, install stages to `CamelotSynth.vst3.pending` — close Reaper, then run `install-plugin.ps1` again. Build output: `CamelotSynth/build/out/CamelotSynth.vst3/`.

## Documentation

| Doc | Contents |
|-----|----------|
| [src/audioagent/README.md](src/audioagent/README.md) | Library modules, thread model, integration |
| [src/audioagent/ARCHITECTURE.md](src/audioagent/ARCHITECTURE.md) | Data flow, transport, pitch pipeline |
| [CamelotSynth/README.md](CamelotSynth/README.md) | Plugin features, UI, processing chain |
| [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) | RT DSP roadmap and phased tasks |

## New plugin

```powershell
.\scripts\new-plugin.ps1 -Name MySynth -Template IPlugInstrument -Manufacturer Vecnode
```

Link `audioagent` from your plugin `CMakeLists.txt` (see [audioagent README](src/audioagent/README.md)).
