# CamelotSynth

Embedded sample player — plays `AtmosSynth1 D#maj.wav` from `plugins/assets/`.

## UI (720×1350)

- **Top** — tab bar + Play / Pause / Stop
- **Middle** — Camelot note circle (360px) + gain knob (bottom-right of middle zone)
- **Bottom** — waveform sample track
- **Right** — stereo meter

Source layout and data flow: **`src/ARCHITECTURE.md`**

## Build

```powershell
.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release -Install
```

Output: `CamelotSynth/build/out/CamelotSynth.vst3/`
