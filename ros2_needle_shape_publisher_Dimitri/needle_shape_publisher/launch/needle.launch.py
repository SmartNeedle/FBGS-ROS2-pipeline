import os
from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import LaunchConfigurationEquals
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch.launch_description_sources import PythonLaunchDescriptionSource

from launch_ros.actions import Node

pkg_needle_shape_publisher = get_package_share_directory('needle_shape_publisher')

def generate_launch_description():
    ld = LaunchDescription()

    default_needleparam_file = "3CH-4AA-0005_needle_params_2022-01-26_Jig-Calibration_best_weights.json"

    # arguments
    arg_params = DeclareLaunchArgument(
        'needleParamFile',
        default_value=default_needleparam_file,
        description="The shape-sensing needle parameter json file.",
    )

    arg_numsignals = DeclareLaunchArgument(
        'numSignals',
        default_value="200",
        description="The number of FBG signals to collect.",
    )

    arg_optim_maxiter = DeclareLaunchArgument(
        'optimMaxIterations',
        default_value="15",
        description="The maximum number of iterations for needle shape optimizer.",
    )

    arg_temp_compensate = DeclareLaunchArgument(
        'tempCompensate',
        default_value="True",
        choices=["True", "False"],
        description="Whether to perform temperature compensation or not.",
    )

    arg_optim_update_ornt_airgap = DeclareLaunchArgument(
        'optimNeedleUpdateOrientationAirGap',
        default_value="True",
        choices=["True", "False"],
        description="Whether to update the needle's tissue orientation based on estimated air gap orientation",
    )

    arg_plotter = DeclareLaunchArgument(
        'plotShape',
        default_value="False",
        choices=["True", "False"],
        description="Live plot the needle shape"
    )

    # included launch files
    ld_needlepub = IncludeLaunchDescription( # needle shape publisher
        PythonLaunchDescriptionSource(
        os.path.join(pkg_needle_shape_publisher, 'sensorized_shapesensing_needle_decomposed.launch.py')),
        launch_arguments = {
            'needleParamFile'                   : PathJoinSubstitution([pkg_needle_shape_publisher, 'needle_data', LaunchConfiguration('needleParamFile')]),
            'numSignals'                        : LaunchConfiguration(arg_numsignals.name),
            'optimMaxIterations'                : LaunchConfiguration(arg_optim_maxiter.name),
            'tempCompensate'                    : LaunchConfiguration(arg_temp_compensate.name),
            'optimNeedleUpdateOrientationAirGap': LaunchConfiguration(arg_optim_update_ornt_airgap.name),
        }.items()
    )

    # nodes
    node_plotter = Node(
        package="needle_shape_publisher",
        namespace="needle",
        executable="shape_plotter",
        output="screen",
        emulate_tty=True,
        condition=LaunchConfigurationEquals(arg_plotter.name, "True"),
    )

    # configure launch description
    ld.add_action(arg_params)
    ld.add_action(arg_numsignals)
    ld.add_action(arg_optim_maxiter)
    ld.add_action(arg_temp_compensate)
    ld.add_action(arg_optim_update_ornt_airgap)
    ld.add_action(arg_plotter)

    ld.add_action(ld_needlepub)

    ld.add_action(node_plotter)

    return ld

# generate_launch_descrtiption