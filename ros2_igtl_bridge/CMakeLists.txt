cmake_minimum_required(VERSION 3.5)
project(ros2_igtl_bridge)

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(OpenIGTLink REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/String.msg"
  "msg/Transform.msg"
  "msg/PointArray.msg"
  "msg/Image.msg"
  "msg/PoseArray.msg"
  "msg/PolyData.msg"
  "msg/Vector.msg"
  DEPENDENCIES builtin_interfaces geometry_msgs sensor_msgs
)

ament_export_dependencies(rosidl_default_runtime)

include_directories(include ${OpenIGTLink_INCLUDE_DIRS})

add_executable(igtl_node
  src/main.cpp
  src/igtl_node.cpp
  src/rib_converter_manager.cpp
  src/rib_converter_base.cpp
  src/rib_converter_string.cpp
  src/rib_converter_transform.cpp
  src/rib_converter_image.cpp
  src/rib_converter_pointarray.cpp
  src/rib_converter_posearray.cpp
)

ament_target_dependencies(igtl_node rclcpp std_msgs sensor_msgs OpenIGTLink)
rosidl_target_interfaces(igtl_node ${PROJECT_NAME} rosidl_typesupport_cpp)

target_include_directories(igtl_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

install(TARGETS
  igtl_node
  DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

install(
  DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
