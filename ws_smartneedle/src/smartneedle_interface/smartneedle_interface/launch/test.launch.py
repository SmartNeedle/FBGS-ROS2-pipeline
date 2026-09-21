from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription, actions
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node


def generate_launch_description():
    package_share = get_package_share_directory("smartneedle_interface")

    bridge_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([package_share, "launch", "bridge.launch.py"])
        ),
        launch_arguments={
            "mode": LaunchConfiguration("mode"),
            "port": LaunchConfiguration("port"),
            "ip": LaunchConfiguration("ip"),
            "point_scale": LaunchConfiguration("point_scale"),
            "reverse_point_order": LaunchConfiguration("reverse_point_order"),
            "invert_x": LaunchConfiguration("invert_x"),
            "invert_y": LaunchConfiguration("invert_y"),
            "invert_z": LaunchConfiguration("invert_z"),
            "output_frame_id": LaunchConfiguration("output_frame_id"),
        }.items(),
    )

    virtual_needle = Node(
        package="smartneedle_interface",
        executable="virtual_smartneedle",
        parameters=[{
                "dataset": LaunchConfiguration("dataset"),
                "rate_hz": LaunchConfiguration("rate_hz"),
                "point_count": LaunchConfiguration("point_count"),
                "needle_length_m": LaunchConfiguration("needle_length_m"),
        }],
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("dataset", default_value="fbg_10"),
            DeclareLaunchArgument("rate_hz", default_value="100.0"),
            DeclareLaunchArgument("point_count", default_value="21"),
            DeclareLaunchArgument("needle_length_m", default_value="0.110"),
            DeclareLaunchArgument("mode", default_value="server"),
            DeclareLaunchArgument("port", default_value="18944"),
            DeclareLaunchArgument("ip", default_value="127.0.0.1"),
            DeclareLaunchArgument("point_scale", default_value="1000.0"),
            DeclareLaunchArgument("reverse_point_order", default_value="false"),
            DeclareLaunchArgument("invert_x", default_value="false"),
            DeclareLaunchArgument("invert_y", default_value="false"),
            DeclareLaunchArgument("invert_z", default_value="false"),
            DeclareLaunchArgument("output_frame_id", default_value="zFrame"),
            actions.LogInfo(msg=["dataset: ", LaunchConfiguration("dataset")]),
            bridge_launch,
            virtual_needle,
        ]
    )
