# CamelotSynth

Embedded sample player — plays `AtmosSynth1 D#maj.wav` from `plugins/assets/`.

## UI (720×450)

Layout is defined in `CamelotSynthEditor.h`:

- **Top** — `hello` tab bar
- **Middle** — Play / Pause / Stop and Gain knob
- **Bottom** — DAW-style waveform track with playhead
- **Right** — isolated stereo meter column

## Build

```powershell
.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release -Install
```

Output: `CamelotSynth/build/out/CamelotSynth.vst3/`
