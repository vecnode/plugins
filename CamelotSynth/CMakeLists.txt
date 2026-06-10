cmake_minimum_required(VERSION 3.14)
project(CamelotSynth VERSION 1.0.0)

if(NOT DEFINED IPLUG2_DIR)
  get_filename_component(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../iPlug2" ABSOLUTE)
  file(TO_CMAKE_PATH "${IPLUG2_DIR}" IPLUG2_DIR)
  set(IPLUG2_DIR "${IPLUG2_DIR}" CACHE PATH "iPlug2 root directory")
endif()

set(_asset_src "${CMAKE_CURRENT_SOURCE_DIR}/../assets/AtmosSynth1 D#maj.wav")
set(_asset_dst_dir "${CMAKE_CURRENT_SOURCE_DIR}/resources/audio")
set(_asset_dst "${_asset_dst_dir}/AtmosSynth1_Dmaj.wav")

if(EXISTS "${_asset_src}")
  file(MAKE_DIRECTORY "${_asset_dst_dir}")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_asset_src}" "${_asset_dst}"
    RESULT_VARIABLE _asset_copy_result
  )
  if(NOT _asset_copy_result EQUAL 0)
    message(WARNING "Failed to copy ${_asset_src} to ${_asset_dst}")
  endif()
else()
  message(WARNING "Shared asset not found: ${_asset_src}")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

set(_camelot_src_modules
  src/dsp/SampleBuffer.h
  src/dsp/SamplePlayer.h
  src/model/WaveformEnvelope.h
  src/ui/bridge/UiPlayheadBridge.h
  src/ui/bridge/UiPaintPolicy.h
  src/ui/controls/WaveformControl.h
  src/ui/controls/PlayheadOverlayControl.h
  src/ui/controls/GainKnobControl.h
  src/ui/controls/CamelotCircleControl.h
  src/editor/Layout.h
  src/editor/Styles.h
  src/editor/Attach.h
)

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    CamelotSynth.cpp
    CamelotSynth.h
    CamelotSynthEditor.h
    ${_camelot_src_modules}
    resources/resource.h
  DEFINES
    IGRAPHICS_DISABLE_VSYNC
    # Opaque FBO clear in IGraphicsNanoVG::BeginFrame (Windows drag-trail fix; see ARCHITECTURE.md)
    IGRAPHICS_OPAQUE_CLEAR
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
    resources/audio/AtmosSynth1_Dmaj.wav
)

set(_camelot_include_dirs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/model
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/bridge
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/controls
  ${CMAKE_CURRENT_SOURCE_DIR}/src/editor
)

foreach(_fmt vst3 vst2 clap aax app)
  if(TARGET ${PROJECT_NAME}-${_fmt})
    target_include_directories(${PROJECT_NAME}-${_fmt} PRIVATE ${_camelot_include_dirs})
  endif()
endforeach()

include(${CMAKE_CURRENT_SOURCE_DIR}/../scripts/cmake/embed-win-resources.cmake)
iplug_embed_win_resources(${PROJECT_NAME})
