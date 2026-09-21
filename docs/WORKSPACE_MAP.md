# Workspace Map

| Path | Purpose |
| --- | --- |
| ros2_fbg_shape_pipeline_cpp/fbg_shape_msgs | Local parsed frame and curvature messages |
| ros2_fbg_shape_pipeline_cpp/fbg_shape_pipeline_cpp | Receiver, processor, reconstruction, calibration, C++ tests |
| ros2_fbg_shape_pipeline_cpp/tools | TCP simulator and latency probe |
| ros2_fbg_shape_pipeline_cpp/scripts | Optional core-only build/launch and Linux dependency installation |
| ros2_smartneedle_adapter | Local 100 Hz adapter and complete stack launch |
| external dependencies | Three pinned, untouched upstream repositories |
| scripts | Full-stack build/launch and dependency initialization |
| tests | Portable contract tests and ROS integration checks |
| docs | Canonical setup, audit, and dependency documentation |
| .github/workflows | ROS Humble Linux build and regression tests |

OpenIGTLink-build is generated locally and ignored. Colcon build/install/log
outputs normally live under ~/.cache/fbg_colcon/<repository-name>_slicer.
Do not copy generated build trees between machines.
