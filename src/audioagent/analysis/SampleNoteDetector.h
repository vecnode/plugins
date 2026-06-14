#pragma once

#include "SampleBuffer.h"
#include "mir/_pitch_yin.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace audioagent
{

struct DetectedNote
{
  bool valid = false;
  int midiNote = -1;
  float frequencyHz = 0.f;
  float confidence = 0.f;
  char text[40] = "Note: --";
};

/** Offline fundamental estimation (audioFlux PitchYIN) — runs off the audio thread only. */
class SampleNoteDetector
{
public:
  static DetectedNote AnalyzeMono(const float* mono, int length, int sampleRate)
  {
    DetectedNote result;

    if (!mono || length <= 0 || sampleRate <= 0)
      return result;

    const int fftLength = 1 << 12;
    if (length < fftLength)
      return result;

    const int regionStart = length / 8;
    const int regionEnd = length - length / 8;
    const int regionLength = regionEnd - regionStart;
    if (regionLength < fftLength)
      return result;

    int sr = sampleRate;
    float lowFre = 27.f;
    float highFre = 2000.f;
    int radix2Exp = 12;
    int slideLength = fftLength / 4;
    int autoLength = fftLength / 2;
    int isContinue = 0;

    PitchYINObj pitchObj = nullptr;
    if (pitchYINObj_new(&pitchObj, &sr, &lowFre, &highFre, &radix2Exp, &slideLength, &autoLength, &isContinue) != 0
        || !pitchObj)
    {
      return result;
    }

    pitchYINObj_setThresh(pitchObj, 0.12f);

    const int timeLength = pitchYINObj_calTimeLength(pitchObj, regionLength);
    if (timeLength <= 0)
    {
      pitchYINObj_free(pitchObj);
      return result;
    }

    std::vector<float> regionMono(static_cast<size_t>(regionLength));
    std::memcpy(regionMono.data(), mono + regionStart, static_cast<size_t>(regionLength) * sizeof(float));

    std::vector<float> freArr(static_cast<size_t>(timeLength));
    std::vector<float> troughArr(static_cast<size_t>(timeLength));
    std::vector<float> minArr(static_cast<size_t>(timeLength));

    pitchYINObj_pitch(pitchObj, regionMono.data(), regionLength, freArr.data(), troughArr.data(), minArr.data());
    pitchYINObj_free(pitchObj);

    struct FrameEstimate
    {
      float hz;
      float weight;
    };

    std::vector<FrameEstimate> frames;
    frames.reserve(static_cast<size_t>(timeLength));

    for (int i = 0; i < timeLength; ++i)
    {
      const float hz = freArr[static_cast<size_t>(i)];
      const float yinMin = minArr[static_cast<size_t>(i)];

      if (hz < lowFre || hz > highFre || yinMin <= 0.f || yinMin >= 0.35f)
        continue;

      const int frameStart = regionStart + i * slideLength;
      const int frameEnd = std::min(frameStart + fftLength, length);
      if (frameEnd <= frameStart)
        continue;

      const float rms = ComputeRms(mono + frameStart, frameEnd - frameStart);
      if (rms < 1e-4f)
        continue;

      const float confidence = (1.f - yinMin) * rms;
      frames.push_back({hz, confidence});
    }

    if (frames.empty())
      return result;

    std::unordered_map<int, double> midiWeights;
    midiWeights.reserve(frames.size());

    for (const FrameEstimate& frame : frames)
    {
      const int midi = FrequencyToMidi(frame.hz);
      midiWeights[midi] += static_cast<double>(frame.weight);
    }

    int bestMidi = 0;
    double bestWeight = 0.;
    for (const auto& entry : midiWeights)
    {
      if (entry.second > bestWeight)
      {
        bestWeight = entry.second;
        bestMidi = entry.first;
      }
    }

    if (bestWeight <= 0.)
      return result;

    double weightedHz = 0.;
    double weightSum = 0.;
    for (const FrameEstimate& frame : frames)
    {
      if (std::abs(FrequencyToMidi(frame.hz) - bestMidi) <= 1)
      {
        weightedHz += frame.hz * frame.weight;
        weightSum += frame.weight;
      }
    }

    if (weightSum <= 0.)
      return result;

    result.frequencyHz = static_cast<float>(weightedHz / weightSum);
    result.confidence = static_cast<float>(std::min(1., bestWeight / frames.size()));
    result.valid = true;
    result.midiNote = bestMidi;
    FormatNoteName(result.frequencyHz, result.text, sizeof(result.text));
    return result;
  }

  static DetectedNote Transpose(const DetectedNote& note, int semitones)
  {
    DetectedNote result = note;
    if (!note.valid || note.midiNote < 0)
    {
      result.valid = false;
      result.midiNote = -1;
      std::snprintf(result.text, sizeof(result.text), "Note: --");
      return result;
    }

    result.midiNote = note.midiNote + semitones;
    result.valid = true;
    FormatNoteFromMidi(result.midiNote, result.text, sizeof(result.text));
    return result;
  }

  static DetectedNote Analyze(const SampleBuffer& buffer)
  {
    if (!buffer.IsLoaded())
      return {};

    std::vector<float> mono(static_cast<size_t>(buffer.GetLength()));
    const sample* left = buffer.GetLeft();
    const sample* right = buffer.GetRight();
    for (int i = 0; i < buffer.GetLength(); ++i)
      mono[static_cast<size_t>(i)] = static_cast<float>((left[i] + right[i]) * 0.5);

    return AnalyzeMono(mono.data(), buffer.GetLength(), static_cast<int>(std::lround(buffer.GetHostSampleRate())));
  }

private:
  static float ComputeRms(const float* data, int length)
  {
    if (!data || length <= 0)
      return 0.f;

    double sum = 0.;
    for (int i = 0; i < length; ++i)
      sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);

    return static_cast<float>(std::sqrt(sum / length));
  }

  static int FrequencyToMidi(float hz)
  {
    return static_cast<int>(std::lround(69. + 12. * std::log2(hz / 440.)));
  }

  static void FormatNoteFromMidi(int midi, char* out, size_t outSize)
  {
    static const char* kNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    int octave = midi / 12 - 1;
    int pc = midi % 12;
    if (pc < 0)
    {
      pc += 12;
      --octave;
    }

    std::snprintf(out, outSize, "Note: %s · Octave %d", kNames[pc], octave);
  }

  static void FormatNoteName(float hz, char* out, size_t outSize)
  {
    if (hz <= 0.f)
    {
      std::snprintf(out, outSize, "Note: --");
      return;
    }

    const int midi = FrequencyToMidi(hz);
    FormatNoteFromMidi(midi, out, outSize);
  }
};

} // namespace audioagent
