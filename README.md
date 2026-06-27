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
├── iPlug2/                  # sibling SDK (not in this repo)
└── plugins/
    ├── build_all.bat        # build audioagent + every plugin
    ├── install_all.bat      # install built bundles into the user VST3/CLAP folder
    ├── src/
    │   └── audioagent/      # shared DSP + MIR + Camelot wheel library
    ├── CamelotSynth/        # iPlug2 plugin (uses audioagent)
    ├── assets/              # shared audio (gitignored)
    └── scripts/             # build.ps1, install-plugin.ps1, setup, RT-audit
```

For agent/contributor guidance (the iPlug2 surface, RT-audio rules, known issues) see [AGENTS.md](AGENTS.md).

## Setup (once)

```powershell
.\scripts\setup-iplug2.ps1
.\scripts\setup-third-party.ps1   # audioFlux for audioagent
code plugins.code-workspace
```

## Build and install everything

```bat
build_all.bat            :: build every plugin (vst3, Release) — audioagent is compiled into each
install_all.bat          :: install the built VST3s into %LOCALAPPDATA%\Programs\Common\VST3
```

Both accept optional arguments: `build_all.bat [Format] [Config]` (default `vst3 Release`) and `install_all.bat [Format]` (default `vst3`). A "plugin" is any top-level folder containing `config.h`. `audioagent` is a header-only INTERFACE library, so it has no separate build step — it is compiled into each plugin.

### Single plugin

```powershell
$p = "CamelotSynth"
.\scripts\build.ps1 -Plugin $p -Format vst3 -Config Release -Install
.\scripts\install-plugin.ps1 -Plugin $p -Format vst3
```

If a DAW has the plugin loaded, install stages to `*.vst3.pending` — close the DAW, then run `install_all.bat` (or `install-plugin.ps1`) again. Build output: `<Plugin>/build/out/<Plugin>.vst3/`.

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
