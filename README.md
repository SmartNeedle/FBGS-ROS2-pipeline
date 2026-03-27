# FBGS-ROS2 Pipeline

This working directory has been trimmed to the canonical interrogator-to-Slicer path.

## Canonical pipeline

Use these components together:

- `OpenIGTLink/`: source tree for building the OpenIGTLink library required by `ros2_igtl_bridge`
- `ros2_fbg_shape_pipeline_cpp/`: interrogator TCP receiver, curvature processor, and shape publisher
- `ros2_igtl_bridge/`: ROS 2 <-> OpenIGTLink bridge library/package
- `ws_smartneedle/src/smartneedle_interface/smartneedle_interface/`: lightweight ROS2-to-Slicer adapter from the collaborator files
- `SmartNeedleIGTL-3DSlicer/`: lightweight 3D Slicer module to visualize the incoming needle shape

## Start here

- Build guide and end-to-end test protocol: `docs/SLICER_ROS2_OPENIGTLINK_PROTOCOL.md`
- Workspace/layout guide: `docs/WORKSPACE_MAP.md`
- Build helper: `scripts/build_cpp_ros2_stack.sh`
- Bridge-only launch helper: `scripts/launch_slicer_bridge.sh`
- Full interrogator-to-Slicer launch helper: `scripts/launch_interrogator_to_slicer_stack.sh`

## Current workspace shape

Only the folders needed for the canonical path remain in the working directory. The older custom bridge, duplicate Slicer module, legacy Python publisher packages, duplicate workspace packages, and other historical reference folders were removed to keep navigation simple.
