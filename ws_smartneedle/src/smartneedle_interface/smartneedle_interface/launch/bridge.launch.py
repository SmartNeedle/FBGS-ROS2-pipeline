from launch import LaunchDescription, actions
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    igtl_bridge = Node(
        package="ros2_igtl_bridge",
        executable="igtl_node",
        parameters=[
            {"RIB_server_ip": LaunchConfiguration("ip")},
            {"RIB_port": LaunchConfiguration("port")},
            {"RIB_type": LaunchConfiguration("mode")},
        ],
    )

    smartneedle_igtl = Node(
        package="smartneedle_interface",
        executable="smartneedle_igtl",
        parameters=[
            {
                "input_topic": LaunchConfiguration("input_topic"),
                "point_scale": LaunchConfiguration("point_scale"),
                "reverse_point_order": LaunchConfiguration("reverse_point_order"),
                "invert_x": LaunchConfiguration("invert_x"),
                "invert_y": LaunchConfiguration("invert_y"),
                "invert_z": LaunchConfiguration("invert_z"),
                "output_frame_id": LaunchConfiguration("output_frame_id"),
            }
        ],
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("mode", default_value="server", description="OpenIGTLink bridge mode: client or server."),
            actions.LogInfo(msg=["mode: ", LaunchConfiguration("mode")]),
            DeclareLaunchArgument("port", default_value="18944", description="OpenIGTLink bridge port number."),
            actions.LogInfo(msg=["port: ", LaunchConfiguration("port")]),
            DeclareLaunchArgument("ip", default_value="127.0.0.1", description="OpenIGTLink bridge IP address."),
            actions.LogInfo(msg=["ip: ", LaunchConfiguration("ip")]),
            DeclareLaunchArgument("input_topic", default_value="/needle/state/current_shape", description="PoseArray topic forwarded to OpenIGTLink."),
            DeclareLaunchArgument("point_scale", default_value="1000.0", description="Scale applied before sending points to Slicer (meters to millimeters by default)."),
            DeclareLaunchArgument("reverse_point_order", default_value="false", description="Reverse the incoming centerline point order before publishing."),
            DeclareLaunchArgument("invert_x", default_value="false", description="Negate the X coordinate before publishing."),
            DeclareLaunchArgument("invert_y", default_value="false", description="Negate the Y coordinate before publishing."),
            DeclareLaunchArgument("invert_z", default_value="false", description="Negate the Z coordinate before publishing."),
            DeclareLaunchArgument("output_frame_id", default_value="zFrame", description="Frame id encoded into the OpenIGTLink header string."),
            igtl_bridge,
            smartneedle_igtl,
        ]
    )
