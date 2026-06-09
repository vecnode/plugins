# iPlug2 Plugins

Out-of-source plugin projects linked to a sibling iPlug2 checkout.

## Layout

```
Dev/
├── iPlug2/
└── plugins/
    ├── GainPlugin/      # gain effect
    ├── ControlsDemo/    # GUI widgets
    └── scripts/
```

## Setup (once)

```powershell
.\scripts\setup-iplug2.ps1
code plugins.code-workspace
```

## Build and test in Reaper

```powershell
$p = "GainPlugin"   # or ControlsDemo
.\scripts\build.ps1 -Plugin $p -Format vst3 -Config Release
Copy-Item -Recurse -Force "$p\build\out\$p.vst3" "$env:LOCALAPPDATA\Programs\Common\VST3\"
start $p\$p.RPP
```

Built binary: `Plugin/build/out/Plugin.vst3/Contents/x86_64-win/Plugin.vst3`

Reaper scans `%LOCALAPPDATA%\Programs\Common\VST3\`. Rescan if the plugin does not appear.

## New plugin

```powershell
.\scripts\new-plugin.ps1 -Name MySynth -Template IPlugInstrument -Manufacturer Vecnode
```
