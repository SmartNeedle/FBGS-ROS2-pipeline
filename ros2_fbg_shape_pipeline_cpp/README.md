# ROS 2 FBG Shape Pipeline

The three C++ nodes implement TCP acquisition, calibrated angle processing, and
piecewise-constant SE(3) reconstruction. See [architecture](docs/ARCHITECTURE.md)
and the [canonical full-stack protocol](../docs/SLICER_ROS2_OPENIGTLINK_PROTOCOL.md).

## Calibration

The source configuration is fbg_shape_pipeline_cpp/config/needle_config.txt.
Full-stack launch helpers use that source file; direct ros2 launch uses the
installed copy unless FBG_NEEDLE_CONFIG_FILE names another file.
Rebuild after changing installed code or use the source configuration override.

Lengths labelled mm or without a unit are millimeters. Explicit legacy m keys
are converted to millimeters by the calibration loader. Missing, duplicate,
nonfinite, or inconsistent calibration values fail at launch.
ROS parameters themselves use only needle_length_mm and sensor_arc_lengths_mm.

First FBG is 1-based; selected positions already correspond to the selected
measurements. Curvature scales must all equal one. Angle transformation is
sign * incoming_angle + offset_rad, wrapped to [-pi, pi].
The current signs are all -1 and offsets all zero.

## Optional core-only operation

These commands deliberately omit the external bridge and Slicer adapter:

```bash
cd ros2_fbg_shape_pipeline_cpp
bash scripts/build_workspace.sh --ros-distro humble
bash scripts/launch_pipeline.sh --tcp-host 127.0.0.1 --tcp-port 50012
```

Do not source the core-only install when following the full-stack protocol:
it uses a different cache path. Both workflows use the same source and calibration.

