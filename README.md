# FBGS-ROS2 Pipeline

This working directory has been trimmed to the canonical interrogator-to-Slicer path.

## Canonical pipeline

Use these components together:

- `external dependencies/OpenIGTLink/`: collaborator dependency used to build OpenIGTLink
- `ros2_fbg_shape_pipeline_cpp/`: interrogator TCP receiver, curvature processor, and shape publisher
- `external dependencies/ws_smartneedle/src/ros2_igtl_bridge/`: untouched collaborator bridge package
- `external dependencies/ws_smartneedle/src/smartneedle_interface/`: untouched collaborator interface package
- `ros2_smartneedle_adapter/`: local 100 Hz adapter and launch orchestration
- `external dependencies/SmartNeedleIGTL-3DSlicer/`: untouched collaborator 3D Slicer module

## Start here

- Build guide and end-to-end test protocol: `docs/SLICER_ROS2_OPENIGTLINK_PROTOCOL.md`
- Workspace/layout guide: `docs/WORKSPACE_MAP.md`
- Build helper: `scripts/build_cpp_ros2_stack.sh`
- Bridge-only launch helper: `scripts/launch_slicer_bridge.sh`
- Full interrogator-to-Slicer launch helper: `scripts/launch_interrogator_to_slicer_stack.sh`

## Current workspace shape

The collaborator repositories are kept under `external dependencies/` and are not modified or committed into this repository. The build scripts use those paths directly. Their repository URLs should be recorded in the dependency setup documentation before distributing the project.
