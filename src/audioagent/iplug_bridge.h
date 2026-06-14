#pragma once

/** Minimal iPlug2 DSP surface used by audioagent (sample type, buffers, smoothers, resources). */
#include "IPlugConstants.h"
#include "Smoothers.h"
#include "heapbuf.h"

namespace audioagent
{
using sample = iplug::sample;
template<typename T, int NChannels>
using LogParamSmooth = iplug::LogParamSmooth<T, NChannels>;
}
