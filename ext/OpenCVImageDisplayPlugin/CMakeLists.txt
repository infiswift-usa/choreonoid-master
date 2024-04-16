option(BUILD_OpenCVImageDisplayPlugin "Building a OpenCVImageDisplayPlugin" ON)
if(NOT BUILD_OpenCVImageDisplayPlugin)
  return()
endif()

set(OpenCV_DIR "C:/opencv/build/x64/vc16/lib")

# Find the OpenCV package
find_package(OpenCV REQUIRED)

# Build inside the Choreonoid project
set(target CnoidOpenCVImageDisplayPlugin)
choreonoid_add_plugin(${target} OpenCVImageDisplayPlugin.cpp OpenCVImageView.cpp)

# Include OpenCV directories and link OpenCV libraries
target_include_directories(${target} PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${target} PUBLIC CnoidBase ${OpenCV_LIBS})
target_link_libraries(${target} PUBLIC CnoidBase)
apply_common_setting_for_plugin(${target})