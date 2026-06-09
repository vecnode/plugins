# CamelotSynth

Embedded sample player — plays `AtmosSynth1 D#maj.wav` from `plugins/assets/`.

## UI

- **Play** — start sample from the beginning
- **Stop** — stop playback immediately
- **Gain** — output level

## Build

```powershell
.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release -Install
```

Output: `CamelotSynth/build/out/CamelotSynth.vst3/`
