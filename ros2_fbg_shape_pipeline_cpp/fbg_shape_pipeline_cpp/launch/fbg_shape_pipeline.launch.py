import os
import runpy

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import UnlessCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue



def generate_launch_description():
    tcp_host = LaunchConfiguration("tcp_host")
    tcp_port = LaunchConfiguration("tcp_port")
    fused_shape = LaunchConfiguration("fused_shape")
    compact_topic = LaunchConfiguration("compact_topic")
    sensor_input_topic = LaunchConfiguration("sensor_input_topic")
    package_share = get_package_share_directory("fbg_shape_pipeline_cpp")
    config_file = os.path.join(package_share, "config", "pipeline.yaml")
    env_needle_config_file = os.environ.get("FBG_NEEDLE_CONFIG_FILE", "").strip()
    needle_config_file = env_needle_config_file or os.path.join(package_share, "config", "needle_config.txt")
    loader = runpy.run_path(os.path.join(package_share, "config", "needle_calibration.py"))
    needle_overrides = loader["load_needle_config"](needle_config_file)
    if env_needle_config_file:
        print(f"[fbg_shape_pipeline.launch] Using FBG_NEEDLE_CONFIG_FILE={needle_config_file}")
    if needle_overrides:
        print(f"[fbg_shape_pipeline.launch] Loaded needle_config overrides from {needle_config_file}")
        print(f"[fbg_shape_pipeline.launch] Override keys: {sorted(needle_overrides.keys())}")

    curvature_overrides = {}
    shape_overrides = {}
    for key in ("first_fbg_index", "sensor_arc_lengths_mm", "curvature_scale", "orientation_sign", "orientation_offset_rad"):
        if key in needle_overrides:
            curvature_overrides[key] = needle_overrides[key]
    if "needle_length_mm" in needle_overrides:
        shape_overrides["needle_length_mm"] = needle_overrides["needle_length_mm"]
        curvature_overrides["needle_length_mm"] = needle_overrides["needle_length_mm"]
    curvature_overrides["publish_shape"] = ParameterValue(fused_shape, value_type=bool)
    curvature_overrides["input_topic"] = sensor_input_topic

    return LaunchDescription([
        DeclareLaunchArgument("tcp_host", default_value="127.0.0.1"),
        DeclareLaunchArgument("tcp_port", default_value="50012"),
        DeclareLaunchArgument("fused_shape", default_value="true"),
        DeclareLaunchArgument("compact_topic", default_value="/needle/fbg_sensor_frame"),
        DeclareLaunchArgument("sensor_input_topic", default_value="/needle/fbg_sensor_frame"),
        Node(
            package="fbg_shape_pipeline_cpp",
            executable="tcp_receiver_node",
            name="tcp_receiver_node",
            output="screen",
            parameters=[config_file, {"tcp_host": tcp_host, "tcp_port": tcp_port,
                                      "compact_output_topic": ParameterValue(compact_topic, value_type=str)}],
        ),
        Node(
            package="fbg_shape_pipeline_cpp",
            executable="curvature_processor_node",
            name="curvature_processor_node",
            output="screen",
            parameters=[config_file, curvature_overrides],
        ),
        Node(
            package="fbg_shape_pipeline_cpp",
            executable="shape_publisher_node",
            name="shape_publisher_node",
            condition=UnlessCondition(fused_shape),
            output="screen",
            parameters=[config_file, shape_overrides],
        ),
    ])
