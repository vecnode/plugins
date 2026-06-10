# CamelotSynth — source architecture

Embedded-sample player built on iPlug2. Real-time audio runs in `ProcessBlock`; the editor is a fixed-layout IGraphics UI with transport, a Camelot note circle, gain control, waveform.

**Plugin shell:** `720 × 920` (`config.h`), 60 FPS editor timer.

---

## Directory layout

```
CamelotSynth/
├── CamelotSynth.h / .cpp     Plugin entry — params, messages, DSP, OnIdle, OnUIOpen
├── CamelotSynthEditor.h      Thin include → src/editor/Attach.h
├── config.h                  Dimensions, bundle IDs, embedded resource names
└── src/
    ├── dsp/                  Real-time audio (no IGraphics)
    │   ├── SampleBuffer.h    Decode embedded WAV; hold interleaved channel pointers
    │   └── SamplePlayer.h    Play / pause / stop / seek; atomic playhead
    ├── model/                Derived, non-real-time display data
    │   └── WaveformEnvelope.h  Min/max peak buckets for waveform drawing
    ├── ui/
    │   ├── bridge/
    │   │   ├── UiPlayheadBridge.h   DSP → editor sync flags (OnIdle)
    │   │   └── UiPaintPolicy.h      Windows full-surface paint policy
    │   └── controls/
    │       ├── WaveformControl.h      WaveformTrackControl (+ SampleWaveformControl alias)
    │       ├── PlayheadOverlayControl.h
    │       ├── GainKnobControl.h
    │       └── CamelotNoteCircleControl.h
    └── editor/
        ├── Layout.h          Region computation from plugin bounds
        ├── Styles.h          Panel colours and IVStyle presets
        └── Attach.h          Control wiring and paint-policy install
```

CMake adds include paths for each `src/` subtree and defines `IGRAPHICS_DISABLE_VSYNC` and `IGRAPHICS_OPAQUE_CLEAR` (see [Windows rendering](#windows-rendering)).

---

## UI layout

Regions are computed top-to-bottom inside padded content (`Layout.h`):

| Region | Height / size | Contents |
|--------|----------------|----------|
| Meter strip | 44 px wide, right edge | `IVLEDMeterControl<2>` (`kCtrlTagMeter`) |
| Tab bar | 40 px | Placeholder tab switch |
| Transport | 56 px | Play, Pause, Stop |
| Middle | Remaining centre | 360 px Camelot circle; gain knob bottom-right |
| Wave section | 210 px | Label + waveform track |

Control z-order in `Attach.h`: background panel → meter → chrome → note circle → gain → waveform **track** → playhead **overlay** (overlay must be attached after the track).

---

## Threading and data flow

```
[Embedded WAV]
      │
      ▼
 SampleBuffer ──OnReset / LoadEmbedded──► SamplePlayer
      │                                        │
      ▼                                        ▼
WaveformEnvelope                      ProcessBlock → outputs
 (1024 peak buckets)                         │
      │                                 IPeakAvgSender → meter
      │                                        │
      └──────────── OnIdle (~50 Hz) ───────────┘
                         │
              UiPlayheadBridge
                         │
         WaveformTrackControl + PlayheadOverlayControl
```

- **DSP thread:** `ProcessBlock` reads gain (smoothed), mixes the sample into outputs, sends meter peaks.
- **UI thread:** `OnIdle` transmits meter data, pushes envelope updates when the sample reloads, and syncs playhead position while playing or after transport/seek.
- **Cross-thread UI → DSP:** `SendArbitraryMsgFromUI` with `EMsgTags` (`kMsgPlaySample`, `kMsgPauseSample`, `kMsgStopSample`, `kMsgSeekSample`). Handled in `OnMessage` on the audio thread.

`UiPlayheadBridge` holds two flags:

- `mWaveformDirty` — envelope needs pushing to the track control after load/build.
- `mForcePlayhead` — playhead must sync immediately after transport or seek (not only while playing).

---

## Custom controls

### `WaveformTrackControl` (`WaveformControl.h`)

- Draws the envelope path every frame (no `ILayer` cache — avoids stale pixels during drag).
- Mouse down/drag calls the seek handler and moves the linked playhead overlay.
- Uses `BeginInteraction` / `EndInteraction` and `RequestFullRepaint` from `UiPaintPolicy.h`.

### `PlayheadOverlayControl`

- Separate control with **stable plot bounds** as `mRECT` (does not shrink to a narrow column on move).
- Draws only the red playhead line; the waveform track redraws the plot area on each full-surface paint.
- Linked via `WaveformTrackControl::SetPlayheadOverlay`.

### `GainKnobControl`

- Extends `IVKnobControl`: live label via `GetDisplay`, `SendParameterValueFromUI` during drag, full repaint on interaction.

### `CamelotNoteCircleControl`

- Decorative 12-spoke Camelot wheel with two zone rings (static, no DSP coupling).

---

## Control tags

| Tag | Value | Control |
|-----|-------|---------|
| `kCtrlTagMeter` | 0 | LED meter |
| `kCtrlTagWaveform` | 1 | Waveform track |
| `kCtrlTagPlayhead` | 2 | Playhead overlay |

---

## Windows rendering

iPlug2 NanoVG on Windows renders into an off-screen **FBO** (`mMainFrameBuffer`), then composites the entire FBO to the HWND in `EndFrame()`. Trails during drag happen when FBO pixels are not cleared before redraw — the old `BeginFrame()` called `glClear` on the **default** framebuffer *before* binding the NanoVG FBO, so the FBO kept previous-frame content in undrawn areas (playhead lines, knob labels, meter LEDs).

CamelotSynth addresses this at three levels:

### 1. Framework — FBO clear + opaque fill (`IGraphicsNanoVG::BeginFrame`)

`IGRAPHICS_OPAQUE_CLEAR` (set in `CMakeLists.txt`) clears the **bound FBO** after `nvgBindFramebuffer(mMainFrameBuffer)` to the panel colour `(34, 36, 42)`. Clearing must happen after the bind; clearing earlier only wiped the default framebuffer.

### 2. Plugin — full-surface paint policy (`UiPaintPolicy.h`)

| API | Role |
|-----|------|
| `AttachAndRegisterFullWindowBackground` | Inserts full-bounds `IPanelControl` as control 0; registers it for coalescing |
| `InstallPaintPolicy` | Enables `SetStrictDrawing(true)`; on any dirty control **or active interaction**, marks all controls dirty and re-dirties the background |
| `RequestFullRepaint` | `SetAllControlsDirty()` + background coalesce; called from interactive controls after local `SetDirty(false)` |
| `ForceInitialFullPaint` | Alias for `RequestFullRepaint` at layout / open time |

### 3. Framework — strict-mode full paint (`IGraphics.cpp`)

- **`IsDirty()`** — when `SetStrictDrawing(true)`, replaces per-control dirty rectangles with a single `GetBounds()` entry (one full `InvalidateRect`).
- **`Draw(IRECTList)`** — in strict mode, always calls `Draw(GetBounds())`, not `rects.Bounds()` from `WM_PAINT`. Windows update regions can be smaller than the plugin; drawing must cover the full surface every tick.

**Editor open:** `Attach.h`, `OnUIOpen`, and the first `OnIdle` call `ForceInitialFullPaint` so the first frame is correct before user input.

**Interaction tracking:** `BeginInteraction` / `EndInteraction` keep the display tick repainting at 60 Hz for the whole drag, even between per-control dirty flags.

---

## Module dependency rules

| Layer | May include | Must not |
|-------|-------------|----------|
| `dsp/` | iPlug DSP headers | IGraphics |
| `model/` | `dsp/` | IGraphics |
| `ui/bridge/` | IGraphics, `dsp/` (bridge headers only) | Plugin class |
| `ui/controls/` | IControls, `ui/bridge/` | Plugin class |
| `editor/` | All `src/` modules + `CamelotSynth.h` | `ProcessBlock` implementation |

Keep new real-time code in `dsp/`, derived display data in `model/`, IGraphics widgets in `ui/controls/`, and wiring in `editor/Attach.h`.

---

## Build and install

```powershell
.\scripts\build.ps1 -Plugin CamelotSynth -Format vst3 -Config Release -Install
```

Output: `%LOCALAPPDATA%\Programs\Common\VST3\CamelotSynth.vst3`. Close the host before reinstalling so the loaded DLL is replaced.

---

## Extension notes

- New parameters: add to `EParams`, init in the constructor, attach controls in `Attach.h`.
- New UI → DSP actions: add an `EMsgTags` value and handle it in `OnMessage`; optionally extend `UiPlayheadBridge` if the editor needs deferred sync.
- New animated or draggable controls: call `RequestFullRepaint(GetUI())` after mutating state; use the paint policy rather than relying on per-control dirty rects alone on Windows.
- Sample asset: copied at configure time from `../assets/` into `resources/audio/`; loaded via `SampleBuffer::LoadEmbedded` using `gHINSTANCE` (not `GetModuleHandle(nullptr)`).
