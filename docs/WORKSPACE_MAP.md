# Workspace Map

## Remaining folders

- `OpenIGTLink/`
  Build this library first. Its build directory is passed to colcon as `OpenIGTLink_DIR`.

- `ros2_fbg_shape_pipeline_cpp/`
  Canonical interrogator-to-shape ROS 2 stack.
  Topic of interest for Slicer bridging: `/needle/state/current_shape` (`geometry_msgs/msg/PoseArray`).

- `ros2_igtl_bridge/`
  Canonical OpenIGTLink transport package used by the lightweight interface.

- `ws_smartneedle/src/smartneedle_interface/smartneedle_interface/`
  Canonical lightweight ROS2-to-Slicer adapter package from the collaborator files.
  Important launch files:
  - `bridge.launch.py`: bridge an existing `/needle/state/current_shape` topic to OpenIGTLink
  - `test.launch.py`: hardware-free virtual needle + OpenIGTLink demo
  - `full_pipeline.launch.py`: full interrogator-to-Slicer stack using `fbg_shape_pipeline_cpp`

- `SmartNeedleIGTL-3DSlicer/`
  Add this folder to Slicer's additional module paths.

## Result

The working directory now contains only the canonical pipeline folders above, plus `docs/`, `scripts/`, and repository metadata files.
