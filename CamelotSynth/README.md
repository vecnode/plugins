# CamelotSynth

iPlug2 VST3 shell for the [audioagent](../src/audioagent/) sampler library. Playback, gain, limiting, and live pitch run on the **audio thread**; note detection runs on a **background worker**. The editor stays responsive because no heavy MIR work runs in `ProcessBlock`, `OnParamChange`, or the IGraphics paint path.

## Features

| Feature | Implementation |
|---------|----------------|
| Sample playback | audioagent `SampleBuffer` → `SamplePlayer` |
| Transport | Sample-accurate play / pause / stop / seek via hidden meta parameters |
| Waveform | 1024-point peak envelope; playhead overlay synced on `OnIdle` |
| Note detect | audioFlux **PitchYIN** on a worker thread (via audioagent) |
| Pitch **+1 / −1** | **Latch** at exactly ±1 from detected note until **Reset** or the opposite button |
| Pitch reset | Returns to detected note |
| Live pitch (default) | `RTPitchShifter` on audio thread — continuous, crossfaded, no block waits |
| HPF | Optional 30 Hz high-pass — `kParamHPF` |
| Output safety | `LimiterStage` — always on, ceiling 0.95 |
| Camelot wheel | Interactive B1–B36 grid (no auto-highlight from detect) |

## Pitch behaviour

1. Click **Detect Note** — stores the reference pitch.
2. Click **+1** — latches **+1 semitone** until **Reset** or **−1**.
3. Click **−1** — latches **−1 semitone** until **Reset** or **+1**.
4. Click **Reset** — back to detected note. Pitch does **not stack**.

Audio uses the **Live** shifter by default: light real-time processing with dry/wet crossfade — no 10 s block wait and no silence gaps.

## Real-time processing chain

```
ProcessBlock (audio thread)
  ├─ mEngine.ProcessBlock  →  source → transport mix → HPF → gain → limiter
  └─ peak meter → UI
```

| # | Stage | Control |
|---|-------|---------|
| 0 | **Source** | Dry sample + optional live `RTPitchShifter` (±1) |
| 1 | **Transport mix** | Play / pause / stop / seek crossfades |
| 2 | **HPF** | `kParamHPF` (off by default) |
| 3 | **Gain** | `kParamGain` |
| 4 | **Limiter** | Always on |

Background: **Detect note** only (`OfflineSampleWorker`).

## UI layout

| Row | Contents |
|-----|----------|
| Transport | Start · Pause · Stop |
| Middle | Camelot circle + gain knob |
| Wave plot | Waveform + playhead |
| Footer | Detect · Note label · Length |
| Footer pitch | **−1 · +1 · Reset · Live** switch |

## Build & install

```powershell
.\scripts\setup-third-party.ps1
.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release -Install
```

Output: `CamelotSynth/build/out/CamelotSynth.vst3/`

See [../src/audioagent/README.md](../src/audioagent/README.md) and [../DEVELOPMENT_PLAN.md](../DEVELOPMENT_PLAN.md).
