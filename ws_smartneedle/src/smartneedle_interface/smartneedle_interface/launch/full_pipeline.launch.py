from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution


def generate_launch_description():
    smartneedle_share = get_package_share_directory("smartneedle_interface")
    pipeline_share = get_package_share_directory("fbg_shape_pipeline_cpp")

    pipeline_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([pipeline_share, "launch", "fbg_shape_pipeline.launch.py"])
        ),
        launch_arguments={
            "tcp_host": LaunchConfiguration("tcp_host"),
            "tcp_port": LaunchConfiguration("tcp_port"),
        }.items(),
    )

    bridge_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([smartneedle_share, "launch", "bridge.launch.py"])
        ),
        launch_arguments={
            "mode": LaunchConfiguration("mode"),
            "port": LaunchConfiguration("bridge_port"),
            "ip": LaunchConfiguration("bridge_ip"),
            "point_scale": LaunchConfiguration("point_scale"),
            "reverse_point_order": LaunchConfiguration("reverse_point_order"),
            "invert_x": LaunchConfiguration("invert_x"),
            "invert_y": LaunchConfiguration("invert_y"),
            "invert_z": LaunchConfiguration("invert_z"),
            "output_frame_id": LaunchConfiguration("output_frame_id"),
        }.items(),
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("tcp_host", default_value="127.0.0.1"),
            DeclareLaunchArgument("tcp_port", default_value="50012"),
            DeclareLaunchArgument("mode", default_value="server"),
            DeclareLaunchArgument("bridge_ip", default_value="127.0.0.1"),
            DeclareLaunchArgument("bridge_port", default_value="18944"),
            DeclareLaunchArgument("point_scale", default_value="1000.0"),
            DeclareLaunchArgument("reverse_point_order", default_value="false"),
            DeclareLaunchArgument("invert_x", default_value="false"),
            DeclareLaunchArgument("invert_y", default_value="false"),
            DeclareLaunchArgument("invert_z", default_value="false"),
            DeclareLaunchArgument("output_frame_id", default_value="zFrame"),
            pipeline_launch,
            bridge_launch,
        ]
    )
