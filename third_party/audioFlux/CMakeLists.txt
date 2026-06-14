cmake_minimum_required(VERSION 3.14)

set(AUDIOFLUX_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/upstream")
if(NOT EXISTS "${AUDIOFLUX_UPSTREAM}/src/CMakeLists.txt")
  message(FATAL_ERROR
    "audioFlux upstream sources not found at:\n"
    "  ${AUDIOFLUX_UPSTREAM}\n"
    "Run: .\\scripts\\setup-third-party.ps1")
endif()

set(AUDIOFLUX_SRC "${AUDIOFLUX_UPSTREAM}/src")

file(GLOB _audioflux_root_sources CONFIGURE_DEPENDS "${AUDIOFLUX_SRC}/*.c")
file(GLOB _audioflux_vector_sources CONFIGURE_DEPENDS "${AUDIOFLUX_SRC}/vector/*.c")
file(GLOB _audioflux_util_sources CONFIGURE_DEPENDS "${AUDIOFLUX_SRC}/util/*.c")
file(GLOB _audioflux_dsp_sources CONFIGURE_DEPENDS "${AUDIOFLUX_SRC}/dsp/*.c")
file(GLOB _audioflux_classic_sources CONFIGURE_DEPENDS "${AUDIOFLUX_SRC}/classic/*.c")
file(GLOB _audioflux_filterbank_sources CONFIGURE_DEPENDS "${AUDIOFLUX_SRC}/filterbank/*.c")
file(GLOB _audioflux_feature_sources CONFIGURE_DEPENDS "${AUDIOFLUX_SRC}/feature/*.c")
file(GLOB _audioflux_mir_sources CONFIGURE_DEPENDS "${AUDIOFLUX_SRC}/mir/*.c")
file(GLOB _audioflux_track_sources CONFIGURE_DEPENDS "${AUDIOFLUX_SRC}/track/*.c")

add_library(audioflux STATIC
  ${_audioflux_root_sources}
  ${_audioflux_vector_sources}
  ${_audioflux_util_sources}
  ${_audioflux_dsp_sources}
  ${_audioflux_classic_sources}
  ${_audioflux_filterbank_sources}
  ${_audioflux_feature_sources}
  ${_audioflux_mir_sources}
  ${_audioflux_track_sources}
)

target_include_directories(audioflux
  PUBLIC
    "${AUDIOFLUX_UPSTREAM}/include"
    "${AUDIOFLUX_SRC}"
)

target_compile_definitions(audioflux
  PRIVATE
    _USE_MATH_DEFINES
)

if(MSVC)
  target_compile_options(audioflux PRIVATE /W3 /wd4244 /wd4267 /wd4996)
  set_source_files_properties(
    "${AUDIOFLUX_SRC}/vector/flux_vectorOp.c"
    PROPERTIES COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/audioflux_exp2f.h"
  )
else()
  target_compile_options(audioflux PRIVATE -Wno-unused-parameter -Wno-sign-compare)
endif()

set_target_properties(audioflux PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  POSITION_INDEPENDENT_CODE ON
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
