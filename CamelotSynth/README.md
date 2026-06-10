# CamelotSynth

Embedded sample player — plays `AtmosSynth1 D#maj.wav` from `plugins/assets/`.

## UI

- **Play** — start from the beginning, or resume if paused
- **Pause** — hold playback at the current position
- **Stop** — stop and return to the start
- **Gain** — output level (20 ms smoothing to avoid clicks)
- **Waveform** — downsampled preview of the embedded sample with a playhead marker

## Build

```powershell
.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release -Install
```

Output: `CamelotSynth/build/out/CamelotSynth.vst3/`
