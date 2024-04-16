# @author Shin'ichiro Nakaoka

function(install_project_files)
  install(FILES ${ARGV} DESTINATION ${CNOID_SHARE_SUBDIR}/project)
  if(UNIX AND NOT CNOID_SOURCE_SHARE_DIR STREQUAL CNOID_BINARY_SHARE_DIR)
    get_filename_component(sample_dir ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    foreach(file ${ARGV})
      add_custom_target(
	symlink_to_${sample_dir}_sample_${file} ALL
	COMMAND ${CMAKE_COMMAND} -E create_symlink
	${CMAKE_CURRENT_SOURCE_DIR}/${file} ${CNOID_BINARY_SHARE_DIR}/project/${file})
    endforeach()
  endif()
endfunction()

file(GLOB subdirs "*")
list(SORT subdirs)

foreach(subdir ${subdirs})
  if(NOT subdir MATCHES "\\.disabled$" AND EXISTS ${subdir}/CMakeLists.txt)
    add_subdirectory(${subdir})
  endif()
endforeach()
