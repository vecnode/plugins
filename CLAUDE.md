# CLAUDE.md

This project's agent guidance lives in **[AGENTS.md](AGENTS.md)** — read it first.

Quick reminders for Claude Code specifically:

- This is an **iPlug2** workspace (iPlug3 is announced but not released — don't assume its APIs).
- It needs a **sibling `../iPlug2` checkout**; this repo is not self-contained. Run `scripts/setup-iplug2.ps1` and `scripts/setup-third-party.ps1` once.
- Primary toolchain is **Windows / PowerShell / VS 2022 / CMake**. Build with `.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release -Install`.
- The shared `src/audioagent` library must stay **IGraphics-free** and touch iPlug2 only through `iplug_bridge.h` and `platform/ResourceLoader.h`.
- Before committing audio-path changes, run `.\scripts\check-rt-audio.ps1` — no audioFlux / heap / locks in `ProcessBlock`.

See [AGENTS.md](AGENTS.md) for layout, the iPlug2 surface, the RT contract, and known issues.
