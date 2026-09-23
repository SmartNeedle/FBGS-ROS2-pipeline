from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument("mode", default_value="server"),
        DeclareLaunchArgument("ip", default_value="127.0.0.1"),
        DeclareLaunchArgument("port", default_value="18944"),
        DeclareLaunchArgument("rate_hz", default_value="100.0"),
        DeclareLaunchArgument("input_topic", default_value="/needle/state/current_shape"),
        Node(
            package="ros2_igtl_bridge",
            executable="igtl_node",
            output="log",
            parameters=[{
                "RIB_server_ip": LaunchConfiguration("ip"),
                "RIB_port": LaunchConfiguration("port"),
                "RIB_type": LaunchConfiguration("mode"),
            }],
        ),
        Node(
            package="ros2_smartneedle_adapter",
            executable="smartneedle_igtl_100hz",
            output="screen",
            parameters=[{"rate_hz": LaunchConfiguration("rate_hz"),
                         "input_topic": LaunchConfiguration("input_topic")}],
        ),
    ])
