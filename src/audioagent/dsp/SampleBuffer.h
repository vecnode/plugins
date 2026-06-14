#pragma once

#include "audioagent/iplug_bridge.h"
#include "platform/ResourceLoader.h"
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>

namespace audioagent
{

/** Decodes an embedded PCM WAV resource into host-rate float buffers (RT-safe after load). */
class SampleBuffer
{
public:
  bool LoadEmbedded(const char* resourceFileName, double hostSampleRate)
  {
    mHostSampleRate = hostSampleRate;
    mLeft.Resize(0);
    mRight.Resize(0);
    mLength = 0;
    mLoaded = false;

#ifdef OS_WIN
    const platform::EmbeddedResource resource = platform::LoadEmbeddedResource(resourceFileName, "wav");
    if (!resource.ok || resource.sizeBytes < 44)
      return false;

    return DecodeAndResample(static_cast<const uint8_t*>(resource.data), resource.sizeBytes, hostSampleRate);
#else
    (void) resourceFileName;
    (void) hostSampleRate;
    return false;
#endif
  }

  bool IsLoaded() const { return mLoaded; }
  int GetLength() const { return mLength; }
  double GetHostSampleRate() const { return mHostSampleRate; }

  double GetDurationSeconds() const
  {
    return (mLoaded && mHostSampleRate > 0.) ? static_cast<double>(mLength) / mHostSampleRate : 0.;
  }

  const sample* GetLeft() const { return mLeft.Get(); }
  const sample* GetRight() const { return mRight.Get(); }

  bool AssignFromFloat(const std::vector<float>& left,
                       const std::vector<float>& right,
                       double hostSampleRate)
  {
    if (left.empty() || left.size() != right.size())
    {
      mLoaded = false;
      mLength = 0;
      return false;
    }

    const int length = static_cast<int>(left.size());
    mLeft.Resize(length);
    mRight.Resize(length);

    for (int i = 0; i < length; ++i)
    {
      mLeft.Get()[i] = static_cast<sample>(left[static_cast<size_t>(i)]);
      mRight.Get()[i] = static_cast<sample>(right[static_cast<size_t>(i)]);
    }

    mLength = length;
    mHostSampleRate = hostSampleRate;
    mLoaded = true;
    return true;
  }

private:
  static int16_t ReadLE16(const uint8_t* p)
  {
    return static_cast<int16_t>(p[0] | (p[1] << 8));
  }

  static uint16_t ReadU16(const uint8_t* p)
  {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
  }

  static uint32_t ReadU32(const uint8_t* p)
  {
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
  }

  static void ResampleChannel(const WDL_TypedBuf<sample>& src, double srcRate, WDL_TypedBuf<sample>& dst, double dstRate)
  {
    const int srcLen = src.GetSize();
    if (srcLen <= 0 || srcRate <= 0. || dstRate <= 0.)
    {
      dst.Resize(0);
      return;
    }

    const int dstLen = static_cast<int>(std::ceil(srcLen * dstRate / srcRate));
    dst.Resize(dstLen);

    const double ratio = srcRate / dstRate;
    for (int i = 0; i < dstLen; i++)
    {
      const double pos = i * ratio;
      const int idx = static_cast<int>(pos);
      const double frac = pos - idx;

      if (idx >= srcLen - 1)
        dst.Get()[i] = src.Get()[srcLen - 1];
      else
        dst.Get()[i] = static_cast<sample>(src.Get()[idx] * (1. - frac) + src.Get()[idx + 1] * frac);
    }
  }

  bool DecodeAndResample(const uint8_t* data, int size, double hostSampleRate)
  {
    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0)
      return false;

    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    const uint8_t* pcm = nullptr;
    uint32_t pcmBytes = 0;

    int offset = 12;
    while (offset + 8 <= size)
    {
      const char* chunkID = reinterpret_cast<const char*>(data + offset);
      const uint32_t chunkSize = ReadU32(data + offset + 4);
      const uint8_t* chunkData = data + offset + 8;

      if (std::memcmp(chunkID, "fmt ", 4) == 0 && chunkSize >= 16)
      {
        audioFormat = ReadU16(chunkData);
        numChannels = ReadU16(chunkData + 2);
        sampleRate = ReadU32(chunkData + 4);
        bitsPerSample = ReadU16(chunkData + 14);
      }
      else if (std::memcmp(chunkID, "data", 4) == 0)
      {
        pcm = chunkData;
        pcmBytes = chunkSize;
      }

      offset += 8 + static_cast<int>(chunkSize);
      if (chunkSize % 2)
        offset++;
    }

    if (!pcm || pcmBytes == 0 || sampleRate == 0 || numChannels < 1 || numChannels > 2)
      return false;

    if (audioFormat != 1 && audioFormat != 3)
      return false;

    const int bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample <= 0)
      return false;

    const int numFrames = static_cast<int>(pcmBytes / (bytesPerSample * numChannels));
    if (numFrames <= 0)
      return false;

    WDL_TypedBuf<sample> srcL;
    WDL_TypedBuf<sample> srcR;
    srcL.Resize(numFrames);
    srcR.Resize(numFrames);

    for (int i = 0; i < numFrames; i++)
    {
      const uint8_t* frame = pcm + i * bytesPerSample * numChannels;

      auto decodeSample = [&](const uint8_t* s) -> sample {
        if (audioFormat == 3 && bitsPerSample == 32)
          return *reinterpret_cast<const float*>(s);
        if (audioFormat == 1 && bitsPerSample == 16)
          return ReadLE16(s) / 32768.f;
        if (audioFormat == 1 && bitsPerSample == 24)
          return ((s[0] | (s[1] << 8) | (s[2] << 16)) / 8388608.f);
        if (audioFormat == 1 && bitsPerSample == 32)
          return ReadU32(s) / 2147483648.f;
        return 0.f;
      };

      srcL.Get()[i] = decodeSample(frame);
      srcR.Get()[i] = numChannels > 1 ? decodeSample(frame + bytesPerSample) : srcL.Get()[i];
    }

    ResampleChannel(srcL, static_cast<double>(sampleRate), mLeft, hostSampleRate);
    ResampleChannel(srcR, static_cast<double>(sampleRate), mRight, hostSampleRate);
    mLength = mLeft.GetSize();
    mLoaded = mLength > 0;
    return mLoaded;
  }

  WDL_TypedBuf<sample> mLeft;
  WDL_TypedBuf<sample> mRight;
  int mLength = 0;
  double mHostSampleRate = 44100.;
  bool mLoaded = false;
};

} // namespace audioagent
