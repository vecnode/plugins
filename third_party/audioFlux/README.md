# audioFlux (third-party)

Vendored [audioFlux](https://github.com/libAudioFlux/audioFlux) C library for pitch and MIR analysis in CamelotSynth.

| Field | Value |
|-------|-------|
| Version | 0.1.9 (`v0.1.9` tag) |
| License | MIT |
| Upstream | https://github.com/libAudioFlux/audioFlux |

## Setup

Upstream sources are not committed. Fetch once after cloning this repo:

```powershell
.\scripts\setup-third-party.ps1
```

This clones the pinned tag into `third_party/audioFlux/upstream/`.

## Build

CamelotSynth links `audioflux` as a static library via `third_party/audioFlux/CMakeLists.txt`. On Windows/MSVC the built-in radix-2 FFT is used (no MKL/FFTW required).

Used for real-time note detection from the embedded sample player (YIN pitch and related MIR APIs).
