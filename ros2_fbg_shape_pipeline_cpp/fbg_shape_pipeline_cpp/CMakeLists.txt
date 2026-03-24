cmake_minimum_required(VERSION 3.8)
project(fbg_shape_pipeline_cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(fbg_shape_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

include_directories(include)

add_library(fbg_shape_pipeline_core
  src/frame_parser.cpp
  src/tcp_interrogator_client.cpp
  src/shape_reconstruction.cpp
)

target_link_libraries(fbg_shape_pipeline_core
  Boost::system
)

ament_target_dependencies(fbg_shape_pipeline_core
  fbg_shape_msgs
  geometry_msgs
  rclcpp
  std_msgs
)

add_executable(tcp_receiver_node src/tcp_receiver_node.cpp)
target_link_libraries(tcp_receiver_node fbg_shape_pipeline_core)
ament_target_dependencies(tcp_receiver_node
  fbg_shape_msgs
  rclcpp
)

add_executable(curvature_processor_node src/curvature_processor_node.cpp)
target_link_libraries(curvature_processor_node fbg_shape_pipeline_core)
ament_target_dependencies(curvature_processor_node
  fbg_shape_msgs
  rclcpp
)

add_executable(shape_publisher_node src/shape_publisher_node.cpp)
target_link_libraries(shape_publisher_node fbg_shape_pipeline_core)
ament_target_dependencies(shape_publisher_node
  fbg_shape_msgs
  geometry_msgs
  rclcpp
)

install(
  TARGETS
    fbg_shape_pipeline_core
    tcp_receiver_node
    curvature_processor_node
    shape_publisher_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
