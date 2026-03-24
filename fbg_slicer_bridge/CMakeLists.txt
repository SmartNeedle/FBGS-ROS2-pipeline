cmake_minimum_required(VERSION 3.8)
project(fbg_slicer_bridge)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(ros2_igtl_bridge REQUIRED)

add_executable(needle_shape_to_igtl_node
  src/needle_shape_to_igtl_node.cpp
)

ament_target_dependencies(needle_shape_to_igtl_node
  geometry_msgs
  rclcpp
  ros2_igtl_bridge
)

install(
  TARGETS needle_shape_to_igtl_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
