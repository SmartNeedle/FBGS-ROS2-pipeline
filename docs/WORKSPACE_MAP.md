# Workspace Map

## Repository folders

- `external dependencies/OpenIGTLink/`
  Git submodule pinned by `.gitmodules`. Build this library first.

- `ros2_fbg_shape_pipeline_cpp/`
  Canonical interrogator-to-shape ROS 2 stack.
  Topic of interest for Slicer bridging: `/needle/state/current_shape` (`geometry_msgs/msg/PoseArray`).

- `external dependencies/ws_smartneedle/src/ros2_igtl_bridge/`
  Untouched collaborator OpenIGTLink transport package.

- `external dependencies/ws_smartneedle/src/smartneedle_interface/`
  Untouched collaborator package, retained as an external dependency.

- `ros2_smartneedle_adapter/`
  Local 100 Hz adapter and full-pipeline launch orchestration.

- `external dependencies/SmartNeedleIGTL-3DSlicer/`
  Git submodule. Add its `SmartNeedle` subfolder to Slicer's module paths.

## Result

The dependency URLs and pinned commits are recorded in `.gitmodules`. Initialize all dependencies with `git submodule update --init --recursive`.
