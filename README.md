# iPlug2 VST Plugins

Out-of-source plugin projects linked to a sibling iPlug2 checkout.

1. Embedded-sample player built on iPlug2. Real-time audio runs in `ProcessBlock`; the editor is a fixed-layout IGraphics UI with transport, a Camelot note circle, gain control, waveform.


## Layout

```
Dev/
├── iPlug2/
└── plugins/
    ├── src/
    │   └── audioagent/  # shared DSP + MIR + Camelot wheel library
    ├── CamelotSynth/    # iPlug2 plugin (uses audioagent)
    ├── assets/          # shared audio (gitignored)
    └── scripts/
```

## Setup (once)

```powershell
.\scripts\setup-iplug2.ps1
code plugins.code-workspace
```

## Build and test in Reaper

```powershell
$p = "CamelotSynth"
.\scripts\build.ps1 -Plugin $p -Format vst3 -Config Release -Install
.\scripts\install-plugin.ps1 -Plugin $p -Format vst3
```

If Reaper has the plugin loaded, install stages to `CamelotSynth.vst3.pending` — close Reaper, then run `install-plugin.ps1` again. Build output: `Plugin/build/out/Plugin.vst3/`.

## New plugin

```powershell
.\scripts\new-plugin.ps1 -Name MySynth -Template IPlugInstrument -Manufacturer Vecnode
```

