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
    ├── analysis/             Offline MIR (worker thread only)
    │   ├── SampleNoteDetector.h
    │   ├── SamplePitchProcessor.h
    │   ├── SampleProcessSnapshot.h
    │   └── OfflineSampleWorker.h
    ├── dsp/                  Real-time audio (no IGraphics)
    │   ├── SampleBuffer.h    Decode embedded WAV; hold interleaved channel pointers
    │   ├── SamplePlayer.h    Play / pause / stop / scheduled seek; dip-through-silence
    │   └── SampleTransport.h Buffer + player facade; sample-accurate transport/seek
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
    │       └── CamelotCircleControl.h
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
| Middle | Remaining centre | 460 px Camelot circle; gain knob bottom-right |
| Wave section | 210 px | Label + waveform track |

Control z-order in `Attach.h`: background panel → meter → chrome → note circle → gain → waveform **track** → playhead **overlay** (overlay must be attached after the track).

---

## Threading and data flow

```
[Embedded WAV]
      │
      ▼
 SampleBuffer ──OnReset / LoadEmbedded──► SamplePlayer ◄── atomic swap (pitch result)
      │                                        │
      ▼                                        ▼
WaveformEnvelope                      ProcessBlock → outputs
 (1024 peak buckets)                         │
      │                                 IPeakAvgSender → meter
      │                                        │
      └──────────── OnIdle (~60 Hz) ───────────┘
                         │
              UiPlayheadBridge + OfflineSampleWorker UI sync
                         │
         WaveformTrackControl + footer (detect / +1 / note label)
                         │
              OfflineSampleWorker (background thread)
                         │
              audioFlux PitchYIN / pitchShift on SampleProcessSnapshot
```

- **Audio thread (`ProcessBlock`):** mix sample, apply gain, peak meter; at block start, `SampleTransport::ApplyPendingSwapIfReady()` may replace the playback buffer (O(1) flag + assign). No audioFlux, no full-buffer copies.
- **Audio thread (`OnParamChange`):** schedule transport/seek with `sampleOffset`; for detect/+1, only set `std::atomic` request flags and save playhead norm — **O(1)**.
- **UI timer (`OnIdle`):** meter transmit, playhead sync, `ProcessPendingOfflineJobs()` (snapshot capture + worker queue), `ApplyOfflineWorkerUiUpdates()` (labels, waveform, `StageProcessedBuffer`). Snapshot capture must not run in `OnParamChange`.
- **Worker thread:** `OfflineSampleWorker` runs PitchYIN or pitchShift on an immutable snapshot copy; results polled by `OnIdle`.

Cross-thread UI → DSP for transport: hidden meta parameters (`kParamTrigPlay`, etc.) via `SendParameterValueFromUI`. Avoid `SendArbitraryMsgFromUI` for transport — host messaging can add large latency.

Cross-thread worker → DSP for pitch: `StageProcessedBuffer` (UI/OnIdle) sets staging vectors + `mSwapPending`; `ApplyPendingSwapIfReady` (ProcessBlock) commits to `SampleBuffer` and silent-seeks to preserved playhead norm.

`UiPlayheadBridge` holds two flags:

- `mWaveformDirty` — envelope needs pushing to the track control after load/build.
- `mForcePlayhead` — playhead must sync immediately after transport or seek (not only while playing).

---

## Sample transport and seek (de-clicking)

### Why clicks happened

1. **Immediate seek application** — seeks must use `sampleOffset` and apply inside `ProcessBlock`, not mid-call from `OnParamChange`.
2. **Crossfade restart on every drag event** — resetting the fade timer and jumping the outgoing head causes a level mismatch at the splice point.
3. **Dip-through-silence** — fading to zero between unrelated buffer regions produces an audible gap that sounds like a click on every reposition.
4. **Audible seeks while paused / during transport fades** — `mPlaying` stays true during pause/stop fades, so seeks could still trigger audible splices when the user expects silence.
5. **Mouse-down DSP commit** — sending `kParamSeek` on press (before release) triggers audio on the initial click even when the user only wanted to preview the target.

### Architecture

```
UI (WaveformTrackControl)
      │  mouse down  → overlay preview only (no kParamSeek)
      │  mouse drag  → SendParameterValueFromUI(kParamSeek, norm)
      │  mouse up    → single kParamSeek if click without drag
      ▼
OnParamChange (audio thread, sampleOffset)
      │  SampleTransport::ScheduleSeek(norm, offset)
      ▼
SamplePlayer — atomic pending seek (latest norm wins)
      │  ApplyScheduledSeekAt(sampleIndex) inside ProcessBlock
      ├── not actively playing  → silent seek (position only, no output)
      └── actively playing      → retargetable linear crossfade
```

### Audible vs silent seeks (`SamplePlayer`)

**Silent** (position update only, limiter/output state cleared, no splice):

- Transport is paused or stopped (`!ShouldAudiblySeek()`).
- Transport fade is active (pause/stop/play fade in progress).

**Audible** (20 ms linear dual-head crossfade):

- `mPlaying && !mPaused &&` transport fade is idle.
- Outgoing head is seeded from `CurrentAudiblePosition()` (last rendered sample, not the next read index).
- Rapid scrub **retargets** `mIncomingReadFrac` only — fade progress and outgoing head are never restarted.
- Linear weights (`gOut = 1 − t`, `gIn = t`) keep summed gain at unity.
- Limiter resets at crossfade start so a post-splice peak does not slam gain down.

Seek positions map directly into the loaded `SampleBuffer` (`mReadHeadFrac` / `mIncomingReadFrac` are fractional indices into `mLeft` / `mRight`). Scrubbing while playing crossfades from the old buffer region to the new one; paused/stopped seeks update that index silently for the next play.

**Play** always starts from the current `mReadHeadFrac` (set by the last silent or audible seek). **Stop** while already idle is a silent no-op (no transport fade, so pressing Stop at position 0 does not click). Stop while playing fades out, then resets the playhead to 0.

### Tuning

Seek crossfade length is `kSeekCrossfadeMs` in `SamplePlayer.h` (20 ms default). Transport play/pause/stop uses the 12 ms equal-power fade (`kTransportFadeMs`).

---

## Custom controls

### `WaveformTrackControl` (`WaveformControl.h`)

- Draws the envelope path every frame (no `ILayer` cache — avoids stale pixels during drag).
- Mouse down previews the playhead overlay only; drag and click-release commit `kParamSeek` to the DSP.
- Uses `BeginInteraction` / `EndInteraction` and `RequestFullRepaint` from `UiPaintPolicy.h`.

### `PlayheadOverlayControl`

- Separate control with **stable plot bounds** as `mRECT` (does not shrink to a narrow column on move).
- Draws only the red playhead line; the waveform track redraws the plot area on each full-surface paint.
- Linked via `WaveformTrackControl::SetPlayheadOverlay`.

### `GainKnobControl`

- Extends `IVKnobControl`: live label via `GetDisplay`, `SendParameterValueFromUI` during drag, full repaint on interaction.

### `CamelotCircle`

- **LineLayout** — spoke angles and zone ring radii (the visible grid).
- **BlockRegion** — 36 cells computed from that layout (`BuildBlockRegions`); each stores angle/radius bounds and `ContainsPoint`.
- **Draw** — base disc, highlight fill, grid lines, then **B1–B36** labels centred in each block.
- **Input** — pressed block follows the pointer while the button is held (`OnMouseDrag` + `OnMouseOver` with `mod.L`).
- Public accessors: `GetLineLayout()`, `GetBlock(spoke, zone)`, `GetBlocks()`.

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
- **`Draw(IRECTList)`** — full `GetBounds()` while `IsInteracting()` (drag/scrub); otherwise `rects.Bounds()` for lightweight control updates (CamelotCircle hover). Hook: `IGraphicsRepaintPolicy.h`.

**Editor open:** `Attach.h`, `OnUIOpen`, and the first `OnIdle` call `ForceInitialFullPaint` so the first frame is correct before user input.

**Interaction tracking:** `BeginInteraction` / `EndInteraction` keep the display tick repainting at 60 Hz for the whole drag, even between per-control dirty flags.

### Adding new interactive controls

1. Include `UiPaintPolicy.h`.
2. On pointer down: `BeginInteraction()`; on up: `EndInteraction()`.
3. Drag/scrub: `RequestFullRepaint(GetUI())`. Hover-only (CamelotCircle): `RequestControlRepaint(this)`.
4. Keep `mRECT` stable for overlays — do not call `SetTargetAndDrawRECTs` with a moving sub-rect.
5. Do not cache moving content in `ILayer` if another control draws on top of the same area.

Reference implementations: `GainKnobControl`, `WaveformTrackControl`, `PlayheadOverlayControl`, `CamelotCircle` (hover only — no `BeginInteraction` unless pointer down).

---

## Module dependency rules

| Layer | May include | Must not |
|-------|-------------|----------|
| `analysis/` | audioFlux C API, `dsp/SampleBuffer.h` (types only) | IGraphics, `ProcessBlock` |
| `dsp/` | iPlug DSP headers | IGraphics, audioFlux |
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
- New UI → DSP actions: prefer hidden meta parameters scheduled in `OnParamChange` with `sampleOffset`; optionally extend `UiPlayheadBridge` if the editor needs deferred sync.
- New animated or draggable controls: call `RequestFullRepaint(GetUI())` after mutating state; use the paint policy rather than relying on per-control dirty rects alone on Windows.
- Sample asset: copied at configure time from `../assets/` into `resources/audio/`; loaded via `SampleBuffer::LoadEmbedded` using `gHINSTANCE` (not `GetModuleHandle(nullptr)`).
