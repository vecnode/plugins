#pragma once

#include "flux_base.h"
#include "mir/pitchShift_algorithm.h"

#include <cstring>
#include <vector>

BEGIN_IPLUG_NAMESPACE

struct PitchProcessResult
{
  bool ok = false;
  std::vector<float> left;
  std::vector<float> right;
};

/** Offline pitch shift via audioFlux (time-stretch + resample) — worker thread only. */
class SamplePitchProcessor
{
public:
  static PitchProcessResult ShiftBySemitones(const std::vector<float>& leftIn,
                                             const std::vector<float>& rightIn,
                                             int sampleRate,
                                             int semitones)
  {
    PitchProcessResult result;

    if (leftIn.empty() || leftIn.size() != rightIn.size() || sampleRate <= 0)
      return result;

    if (semitones < -12 || semitones > 12)
      return result;

    const int length = static_cast<int>(leftIn.size());
    result.left.resize(static_cast<size_t>(length));
    result.right.resize(static_cast<size_t>(length));

    int radix2Exp = 12;
    int slideLength = (1 << radix2Exp) / 4;
    WindowType windowType = Window_Hann;

    PitchShiftObj pitchObj = nullptr;
    if (pitchShiftObj_new(&pitchObj, &radix2Exp, &slideLength, &windowType) != 0 || !pitchObj)
      return {};

    int sr = sampleRate;
    ShiftChannel(pitchObj, sr, semitones, leftIn.data(), length, result.left.data());
    ShiftChannel(pitchObj, sr, semitones, rightIn.data(), length, result.right.data());
    pitchShiftObj__free(pitchObj);

    result.ok = true;
    return result;
  }

private:
  static void ShiftChannel(PitchShiftObj pitchObj,
                           int sampleRate,
                           int semitones,
                           const float* input,
                           int length,
                           float* output)
  {
    std::vector<float> channelInput(static_cast<size_t>(length));
    std::memcpy(channelInput.data(), input, static_cast<size_t>(length) * sizeof(float));
    pitchShiftObj_pitchShift(pitchObj, sampleRate, semitones, channelInput.data(), length, output);
  }
};

END_IPLUG_NAMESPACE
