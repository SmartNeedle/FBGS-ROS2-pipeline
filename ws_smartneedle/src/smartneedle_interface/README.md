# Package: smartneedle_interface

## Description

This package is the recommended lightweight ROS2-to-Slicer adapter for this repository.

It subscribes to:

- `/needle/state/current_shape` (`geometry_msgs/msg/PoseArray`)

It publishes:

- `IGTL_STRING_OUT` (`ros2_igtl_bridge/msg/String`) with device name `NeedleShapeHeader`
- `IGTL_POINT_OUT` (`ros2_igtl_bridge/msg/PointArray`) with device name `NeedleShape`

The package does not depend on the old robot-specific workflow.

## Build

Build it together with the canonical CPP stack from the repository root:

```bash
bash ./scripts/build_cpp_ros2_stack.sh --openigtlink-dir ./OpenIGTLink-build
```

## Launch files

- `bridge.launch.py`: bridge an existing `/needle/state/current_shape` topic to OpenIGTLink
- `test.launch.py`: hardware-free virtual SmartNeedle test
- `full_pipeline.launch.py`: full interrogator-to-Slicer path using `fbg_shape_pipeline_cpp`

## Examples

Bridge an existing shape topic:

```bash
ros2 launch smartneedle_interface bridge.launch.py
```

Run the virtual test path:

```bash
ros2 launch smartneedle_interface test.launch.py
```

Launch the full interrogator-to-Slicer stack:

```bash
ros2 launch smartneedle_interface full_pipeline.launch.py
```
