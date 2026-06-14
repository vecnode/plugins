#pragma once

/** audioagent — pure C++ sampler DSP, offline MIR, and Camelot wheel geometry. */

#include "analysis/PitchStreamCache.h"
#include "analysis/PitchStreamWorker.h"
#include "dsp/DenormalFlush.h"
#include "dsp/GainStage.h"
#include "dsp/HPFStage.h"
#include "dsp/LimiterStage.h"
#include "dsp/PitchMode.h"
#include "dsp/PitchStreamPipeline.h"
#include "dsp/ProcessChain.h"
#include "dsp/RTPitchShifter.h"
#include "dsp/SimdUtils.h"
#include "SamplerEngine.h"
#include "camelot/WheelLayout.h"
#include "analysis/OfflineSampleWorker.h"
#include "analysis/SampleNoteDetector.h"
#include "analysis/SampleProcessSnapshot.h"
#include "dsp/SampleBuffer.h"
#include "dsp/SampleTransport.h"
#include "model/WaveformEnvelope.h"
#include "platform/ResourceLoader.h"
