cmake_minimum_required(VERSION 3.14)
project(audioagent VERSION 1.0.0 LANGUAGES CXX)

if(NOT DEFINED IPLUG2_DIR)
  get_filename_component(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../iPlug2" ABSOLUTE)
  file(TO_CMAKE_PATH "${IPLUG2_DIR}" IPLUG2_DIR)
  set(IPLUG2_DIR "${IPLUG2_DIR}" CACHE PATH "iPlug2 root directory")
endif()

set(_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(_third_party_dir "${_repo_root}/third_party")

if(NOT TARGET audioflux)
  add_subdirectory("${_third_party_dir}/audioFlux" "${CMAKE_BINARY_DIR}/third_party/audioFlux")
endif()

add_library(audioagent INTERFACE)

target_include_directories(audioagent INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/dsp
  ${CMAKE_CURRENT_SOURCE_DIR}/analysis
  ${CMAKE_CURRENT_SOURCE_DIR}/model
  ${CMAKE_CURRENT_SOURCE_DIR}/camelot
  ${CMAKE_CURRENT_SOURCE_DIR}/platform
  ${IPLUG2_DIR}/IPlug
  ${IPLUG2_DIR}/WDL
)

target_link_libraries(audioagent INTERFACE audioflux)

target_compile_features(audioagent INTERFACE cxx_std_17)

if(MSVC)
  target_compile_definitions(audioagent INTERFACE _USE_MATH_DEFINES)
endif()
