option(BUILD_OpenCVVideoStreamPlugin "Building a OpenCVVideoStreamPlugin" ON)
if(NOT BUILD_OpenCVVideoStreamPlugin)
  return()
endif()

set(OpenCV_DIR "C:/opencv/build/x64/vc16/lib")

# Find the OpenCV package
find_package(OpenCV REQUIRED)

# Build inside the Choreonoid project
set(target CnoidOpenCVVideoStreamPlugin)
choreonoid_add_plugin(${target} OpenCVVideoStreamPlugin.cpp OpenCVVideoStreamView.cpp)

# Include OpenCV directories and link OpenCV libraries
target_include_directories(${target} PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${target} PUBLIC CnoidBase ${OpenCV_LIBS})
target_link_libraries(${target} PUBLIC CnoidBase)
apply_common_setting_for_plugin(${target})