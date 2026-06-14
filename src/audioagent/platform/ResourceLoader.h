#pragma once

#include <cstdint>

#ifdef OS_WIN
#include "IPlugPaths.h"
extern HINSTANCE gHINSTANCE;
#endif

namespace audioagent::platform
{

struct EmbeddedResource
{
  const void* data = nullptr;
  int sizeBytes = 0;
  bool ok = false;
};

#ifdef OS_WIN

inline EmbeddedResource LoadEmbeddedResource(const char* resourceFileName, const char* resourceType)
{
  EmbeddedResource result;

  void* hInstance = gHINSTANCE;
  if (!hInstance || !resourceFileName)
    return result;

  WDL_String resID;
  const iplug::EResourceLocation found =
    iplug::LocateResource(resourceFileName, resourceType, resID, nullptr, hInstance, nullptr);
  if (found != iplug::kWinBinary)
    return result;

  int size = 0;
  const void* data = iplug::LoadWinResource(resID.Get(), resourceType, size, hInstance);
  if (!data || size <= 0)
    return result;

  result.data = data;
  result.sizeBytes = size;
  result.ok = true;
  return result;
}
#else
inline EmbeddedResource LoadEmbeddedResource(const char*, const char*)
{
  return {};
}
#endif

} // namespace audioagent::platform
