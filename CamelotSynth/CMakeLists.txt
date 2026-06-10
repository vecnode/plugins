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

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    CamelotSynth.cpp
    CamelotSynth.h
    CamelotSynthEditor.h
    src/SampleBuffer.h
    src/SamplePlayer.h
    src/WaveformEnvelope.h
    src/WaveformControl.h
    src/GainKnobControl.h
    src/UiPlayheadBridge.h
    src/UiRedraw.h
    resources/resource.h
  DEFINES
    IGRAPHICS_DISABLE_VSYNC
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
    resources/audio/AtmosSynth1_Dmaj.wav
)

# Headers under src/ are included as "src/Filename.h" from the plugin root.
foreach(_fmt vst3 vst2 clap aax app)
  if(TARGET ${PROJECT_NAME}-${_fmt})
    target_include_directories(${PROJECT_NAME}-${_fmt} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  endif()
endforeach()

include(${CMAKE_CURRENT_SOURCE_DIR}/../scripts/cmake/embed-win-resources.cmake)
iplug_embed_win_resources(${PROJECT_NAME})
