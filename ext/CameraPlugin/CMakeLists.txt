option(BUILD_CAMERA_SAMPLE_PLUGIN "Building a Camera sample plugin" ON)
if(NOT BUILD_CAMERA_SAMPLE_PLUGIN)
  return()
endif()

if(CMAKE_PROJECT_NAME STREQUAL "Choreonoid")
  # Build inside the Choreonoid project
  set(target CnoidCameraPlugin)
  choreonoid_add_plugin(${target} CameraPlugin.cpp CameraView.cpp)
  target_link_libraries(${target} PUBLIC CnoidBase)
  apply_common_setting_for_plugin(${target})

else()
  # Stand alone build
  cmake_minimum_required(VERSION 3.1.0)
  project(CameraPlugin)
  find_package(Choreonoid REQUIRED)
  add_definitions(${CHOREONOID_COMPILE_DEFINITIONS})
  include_directories(${CHOREONOID_INCLUDE_DIRS})
  link_directories(${CHOREONOID_LIBRARY_DIRS})
  set(target CnoidCameraPlugin)
  add_library(${target} SHARED CameraPlugin.cpp CameraView.cpp)  # Include CameraView.cpp
  target_link_libraries(${target} ${CHOREONOID_BASE_LIBRARIES})
  install(TARGETS ${target} LIBRARY DESTINATION ${CHOREONOID_PLUGIN_DIR})
endif()
