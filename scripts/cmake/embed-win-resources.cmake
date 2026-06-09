# Embed fonts/images into Windows plugin DLLs via main.rc.
# iPlug2 CMake adds main.rc for APP only; VST3/CLAP need this for Reaper.

function(iplug_embed_win_resources project_name)
  if(NOT WIN32)
    return()
  endif()

  set(rc_file "${CMAKE_CURRENT_SOURCE_DIR}/resources/main.rc")
  if(NOT EXISTS "${rc_file}")
    return()
  endif()

  set(rc_flags "/I\"${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts\" /I\"${CMAKE_CURRENT_SOURCE_DIR}/resources/img\" /I\"${CMAKE_CURRENT_SOURCE_DIR}/resources/audio\" /I\"${CMAKE_CURRENT_SOURCE_DIR}/resources\"")
  foreach(target IN ITEMS ${project_name}-vst3 ${project_name}-clap ${project_name}-vst2 ${project_name}-aax)
    if(TARGET ${target})
      target_sources(${target} PRIVATE "${rc_file}")
      set_source_files_properties("${rc_file}" PROPERTIES COMPILE_FLAGS "${rc_flags}")
    endif()
  endforeach()
endfunction()
