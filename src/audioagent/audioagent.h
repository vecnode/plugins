#pragma once

/** audioagent — pure C++ sampler DSP, offline MIR, and Camelot wheel geometry. */

#include "analysis/PitchStreamCache.h"
#include "analysis/PitchStreamWorker.h"
#include "dsp/PitchStreamPipeline.h"
#include "SamplerEngine.h"
#include "camelot/WheelLayout.h"
#include "analysis/OfflineSampleWorker.h"
#include "analysis/SampleNoteDetector.h"
#include "analysis/SampleProcessSnapshot.h"
#include "dsp/SampleBuffer.h"
#include "dsp/SampleTransport.h"
#include "model/WaveformEnvelope.h"
