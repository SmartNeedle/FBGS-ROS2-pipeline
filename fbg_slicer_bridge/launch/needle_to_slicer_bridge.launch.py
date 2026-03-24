from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    bridge_mode = LaunchConfiguration("bridge_mode")
    bridge_ip = LaunchConfiguration("bridge_ip")
    bridge_port = LaunchConfiguration("bridge_port")
    input_topic = LaunchConfiguration("input_topic")
    point_scale = LaunchConfiguration("point_scale")
    reverse_point_order = LaunchConfiguration("reverse_point_order")
    invert_x = LaunchConfiguration("invert_x")
    invert_y = LaunchConfiguration("invert_y")
    invert_z = LaunchConfiguration("invert_z")
    output_frame_id = LaunchConfiguration("output_frame_id")

    bridge_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [FindPackageShare("ros2_igtl_bridge"), "launch", "bridge.launch.py"]
            )
        ),
        launch_arguments={
            "mode": bridge_mode,
            "ip": bridge_ip,
            "port": bridge_port,
        }.items(),
    )

    adapter_node = Node(
        package="fbg_slicer_bridge",
        executable="needle_shape_to_igtl_node",
        name="needle_shape_to_igtl_node",
        output="screen",
        parameters=[
            {
                "input_topic": input_topic,
                "point_scale": point_scale,
                "reverse_point_order": reverse_point_order,
                "invert_x": invert_x,
                "invert_y": invert_y,
                "invert_z": invert_z,
                "output_frame_id": output_frame_id,
            }
        ],
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("bridge_mode", default_value="server"),
            DeclareLaunchArgument("bridge_ip", default_value="127.0.0.1"),
            DeclareLaunchArgument("bridge_port", default_value="18944"),
            DeclareLaunchArgument(
                "input_topic", default_value="/needle/state/current_shape"
            ),
            DeclareLaunchArgument("point_scale", default_value="1000.0"),
            DeclareLaunchArgument("reverse_point_order", default_value="false"),
            DeclareLaunchArgument("invert_x", default_value="false"),
            DeclareLaunchArgument("invert_y", default_value="false"),
            DeclareLaunchArgument("invert_z", default_value="false"),
            DeclareLaunchArgument("output_frame_id", default_value="zFrame"),
            bridge_launch,
            adapter_node,
        ]
    )
