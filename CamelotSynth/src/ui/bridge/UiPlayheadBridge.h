#pragma once

#include "SamplePlayer.h"

BEGIN_IPLUG_NAMESPACE

/** Bridges DSP transport/seek state to the editor waveform (called from OnIdle). */
class UiPlayheadBridge
{
public:
  void MarkWaveformDirty() { mWaveformDirty = true; }

  void MarkPlayheadDirty() { mForcePlayhead = true; }

  bool ConsumeWaveformDirty()
  {
    const bool dirty = mWaveformDirty;
    mWaveformDirty = false;
    return dirty;
  }

  /** Sync playhead while playing or right after transport/seek messages. */
  bool ShouldSyncPlayhead(const SamplePlayer& player) const
  {
    return mForcePlayhead || player.IsPlaying();
  }

  void ClearPlayheadForce() { mForcePlayhead = false; }

private:
  bool mWaveformDirty = false;
  bool mForcePlayhead = false;
};

END_IPLUG_NAMESPACE
