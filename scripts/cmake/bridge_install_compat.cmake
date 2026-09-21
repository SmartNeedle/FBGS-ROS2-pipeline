# The pinned upstream bridge installs a launch directory absent from its tree.
# Keep upstream untouched and skip only that nonexistent directory install.
function(install)
  if(PROJECT_NAME STREQUAL "ros2_igtl_bridge"
      AND ARGC EQUAL 4
      AND ARGV0 STREQUAL "DIRECTORY"
      AND ARGV1 STREQUAL "launch"
      AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/launch")
    message(STATUS "Skipping missing upstream bridge launch directory; local adapter supplies launch files")
    return()
  endif()
  _install(${ARGV})
endfunction()
