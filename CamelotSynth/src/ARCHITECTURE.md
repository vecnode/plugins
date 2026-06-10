# CamelotSynth source layout

```
CamelotSynth/
├── CamelotSynth.h / .cpp    Plugin entry (params, messages, ProcessBlock, OnIdle)
├── CamelotSynthEditor.h     Thin include → src/editor/Attach.h
├── config.h                 PLUG_WIDTH / HEIGHT, resource names
└── src/
    ├── dsp/                 Real-time audio (no IGraphics)
    │   ├── SampleBuffer.h
    │   └── SamplePlayer.h
    ├── model/               Derived display data
    │   └── WaveformEnvelope.h
    ├── ui/
    │   ├── bridge/
    │   │   ├── UiPlayheadBridge.h
    │   │   └── UiPaintPolicy.h    Windows full-surface paint policy
    │   └── controls/
    │       ├── WaveformTrackControl (WaveformControl.h)
    │       ├── PlayheadOverlayControl.h
    │       ├── GainKnobControl.h
    │       └── CamelotNoteCircleControl.h
    └── editor/
        ├── Layout.h
        ├── Styles.h
        └── Attach.h
```

## Windows paint contract

iPlug2 NanoVG on Windows clears the framebuffer to **transparent** each frame and
composites the full buffer to the HWND. Partial update regions leave holes that show
the previous frame (playhead trails, colour shift on first click).

**UiPaintPolicy.h** fixes this:

1. `InstallPaintPolicy()` — when any control is dirty, mark full-window background dirty
2. `ForceInitialFullPaint()` — opaque first frame on editor open
3. `RequestFullRepaint()` — on every interaction end (knob, waveform scrub)

**Playhead** is a separate `PlayheadOverlayControl` with its own moving bounds —
not a vector line drawn on the waveform layer.

## Data flow

```
[Embedded WAV] → SampleBuffer → SamplePlayer → ProcessBlock → outputs + meter
                      ↓
               WaveformEnvelope (OnReset)
                      ↓
OnIdle → UiPlayheadBridge → WaveformTrackControl + PlayheadOverlayControl
```

## Layer rules

| Layer | May include | Must not |
|-------|-------------|----------|
| `dsp/` | iPlug DSP headers | IGraphics |
| `model/` | `dsp/` | IGraphics |
| `ui/controls/` | IControls, `ui/bridge/` | Plugin class |
| `editor/` | All above + `CamelotSynth.h` | ProcessBlock |
