from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution


def generate_launch_description():
    pipeline_share = get_package_share_directory("fbg_shape_pipeline_cpp")
    adapter_share = get_package_share_directory("ros2_smartneedle_adapter")

    pipeline = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([pipeline_share, "launch", "fbg_shape_pipeline.launch.py"])
        ),
        launch_arguments={
            "tcp_host": LaunchConfiguration("tcp_host"),
            "tcp_port": LaunchConfiguration("tcp_port"),
            "compact_topic": LaunchConfiguration("compact_topic"),
            "compact_enabled": LaunchConfiguration("compact_enabled"),
            "sensor_input_topic": LaunchConfiguration("sensor_input_topic"),
        }.items(),
    )
    bridge = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([adapter_share, "launch", "slicer_bridge.launch.py"])
        ),
        launch_arguments={
            "mode": LaunchConfiguration("mode"),
            "ip": LaunchConfiguration("bridge_ip"),
            "port": LaunchConfiguration("bridge_port"),
            "rate_hz": LaunchConfiguration("rate_hz"),
        }.items(),
    )

    return LaunchDescription([
        DeclareLaunchArgument("tcp_host", default_value="127.0.0.1"),
        DeclareLaunchArgument("tcp_port", default_value="50012"),
        DeclareLaunchArgument("compact_topic", default_value="/needle/fbg_sensor_frame"),
        DeclareLaunchArgument("compact_enabled", default_value="true"),
        DeclareLaunchArgument("sensor_input_topic", default_value="/needle/fbg_sensor_frame"),
        DeclareLaunchArgument("mode", default_value="server"),
        DeclareLaunchArgument("bridge_ip", default_value="127.0.0.1"),
        DeclareLaunchArgument("bridge_port", default_value="18944"),
        DeclareLaunchArgument("rate_hz", default_value="100.0"),
        pipeline,
        bridge,
    ])
