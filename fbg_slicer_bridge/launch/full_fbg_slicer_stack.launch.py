from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    tcp_host = LaunchConfiguration("tcp_host")
    tcp_port = LaunchConfiguration("tcp_port")
    bridge_port = LaunchConfiguration("bridge_port")
    point_scale = LaunchConfiguration("point_scale")
    reverse_point_order = LaunchConfiguration("reverse_point_order")
    invert_x = LaunchConfiguration("invert_x")
    invert_y = LaunchConfiguration("invert_y")
    invert_z = LaunchConfiguration("invert_z")
    output_frame_id = LaunchConfiguration("output_frame_id")

    pipeline_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [FindPackageShare("fbg_shape_pipeline_cpp"), "launch", "fbg_shape_pipeline.launch.py"]
            )
        ),
        launch_arguments={"tcp_host": tcp_host, "tcp_port": tcp_port}.items(),
    )

    bridge_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [FindPackageShare("fbg_slicer_bridge"), "launch", "needle_to_slicer_bridge.launch.py"]
            )
        ),
        launch_arguments={
            "bridge_port": bridge_port,
            "point_scale": point_scale,
            "reverse_point_order": reverse_point_order,
            "invert_x": invert_x,
            "invert_y": invert_y,
            "invert_z": invert_z,
            "output_frame_id": output_frame_id,
        }.items(),
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("tcp_host", default_value="127.0.0.1"),
            DeclareLaunchArgument("tcp_port", default_value="50012"),
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
