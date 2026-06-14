# Development plan — real-time DSP and processing chains

Status: **Phases 1–4 implemented** (March 2026). See git history for the full diff.

---

## Implemented architecture

### RT processing chain (`ProcessChain`)

```
SamplePlayer render (source + transport mix)
  → scratch buffers (pre-allocated, max 8192 frames)
  → DenormalFlush + SimdUtils
  → HPFStage (optional, kParamHPF)
  → GainStage (kParamGain)
  → LimiterStage (always on)
  → host outputs
```

New modules under `src/audioagent/dsp/`:

| File | Role |
|------|------|
| `ProcessChain.h` | `IProcessStage` + fixed-capacity chain |
| `GainStage.h` | Smoothed gain |
| `LimiterStage.h` | Output limiter wrapper |
| `HPFStage.h` | 30 Hz one-pole HPF (atomic bypass) |
| `DenormalFlush.h` | Subnormal flush (not named `Denormal.h` — avoids shadowing WDL `denormal.h`) |
| `RTPitchShifter.h` | Live-mode grain pitch shifter |
| `PitchMode.h` | `Quality` vs `Live` |
| `SimdUtils.h` | Scalar SIMD hooks |

### Threading fixes

- `KickScheduler()` removed from `SampleTransport::ProcessBlock`; runs only in `SamplerEngine::Tick()`.
- `PitchStreamCache` uses per-block atomic ready flags + release fence before marking blocks readable.

### CamelotSynth params (chain order documented in `CamelotSynth.cpp`)

| Param | Chain / feature |
|-------|-----------------|
| `kParamGain` | GainStage |
| `kParamHPF` | HPFStage |
| `kParamPitchMode` | Quality (read-ahead) vs Live (RTPitchShifter) |
| `kParamTrigPitchDownOne` / `Up` / `Reset` | ±1 semitone, reset to detected note |

Preset chunk: HPF + pitch mode via `SerializeState`.

### Phase 3 UI

- Camelot wheel highlights detected/transposed note (`WheelLayout::BlockIndexFromMidiNote`).
- Footer: **−1 · +1 · Reset · Live** switch.

### Phase 4 hardening

| Item | Location |
|------|----------|
| Host resource load | `platform/ResourceLoader.h` |
| SIMD hooks | `dsp/SimdUtils.h` |
| RT CI grep | `scripts/check-rt-audio.ps1` + `.github/workflows/rt-audit.yml` |
| Benchmark placeholder | `scripts/benchmark-render.ps1` |
| TSan CI placeholder | `rt-audit.yml` job |

---

## Verification

```powershell
.\scripts\check-rt-audio.ps1
.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release
```

---

## Future work

- Wire `benchmark-render.ps1` to a headless ProcessBlock loop.
- Enable TSan build target in CI when CMake supports it.
- AVX/NEON implementations in `SimdUtils`.
- Additional chain stages (EQ) as new `IProcessStage` subclasses.
