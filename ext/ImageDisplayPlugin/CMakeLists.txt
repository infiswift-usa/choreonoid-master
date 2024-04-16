option(BUILD_IMAGE_DISPLAY_PLUGIN "Building a Image Display plugin" ON)
if(NOT BUILD_IMAGE_DISPLAY_PLUGIN)
  return()
endif()

if(CMAKE_PROJECT_NAME STREQUAL "Choreonoid")
    # Build inside the Choreonoid project
    set(target CnoidImageDisplayPlugin)
    choreonoid_add_plugin(${target} ImageDisplayPlugin.cpp ImageDisplayView.cpp)
    # target_link_libraries(${target} CnoidBase)
    apply_common_setting_for_plugin(${target})
else()
  # Stand alone build
  cmake_minimum_required(VERSION 3.1.0)
  project(ImageDisplayPlugin)
  find_package(Choreonoid REQUIRED)
  add_definitions(${CHOREONOID_COMPILE_DEFINITIONS})
  include_directories(${CHOREONOID_INCLUDE_DIRS})
  link_directories(${CHOREONOID_LIBRARY_DIRS})
  set(target CnoidImageDisplayPlugin)
  add_library(${target} SHARED ImageDisplayPlugin.cpp ImageDisplayView.cpp)  # Include CameraView.cpp
  target_link_libraries(${target} ${CHOREONOID_BASE_LIBRARIES})
  install(TARGETS ${target} LIBRARY DESTINATION ${CHOREONOID_PLUGIN_DIR})
endif()