# FBGS ROS 2 Pipeline

Real interrogator data and simulated TCP data use the same receiver, calibration,
curvature processing, SE(3) reconstruction, and OpenIGTLink output.

Start with [the Linux/Slicer protocol](docs/SLICER_ROS2_OPENIGTLINK_PROTOCOL.md).
See [architecture](ros2_fbg_shape_pipeline_cpp/docs/ARCHITECTURE.md),
[dependencies](docs/EXTERNAL_DEPENDENCIES.md), [workspace map](docs/WORKSPACE_MAP.md),
and [audit findings](docs/AUDIT.md).

## Checkout

```bash
git clone https://github.com/jfcoeur/FBGS-ROS2-pipeline.git
cd FBGS-ROS2-pipeline
bash scripts/init_dependencies.sh
```

Do not use recursive submodule initialization: the untouched collaborator
workspace contains two unused legacy gitlinks without URLs.
The three top-level dependencies are pinned to commits by this repository.
GitHub main is the source of truth; commit and push development changes, then
pull and initialize dependencies on the Linux test machine.

## Contract

- Curvature: 1/mm. Angles: radians. Coordinates and lengths: millimeters.
- Current calibration: 20 incoming values, First FBG 3, 18 selected measurements.
- Curvature scales must be unity; angle signs and offsets come from calibration.
- Reconstruction starts at the first selected FBG and uses one SE(3) exponential
  per measurement interval, including the final interval to the tip.
- No curvature interpolation or uniform output resampling.
- Adapter target: 100 Hz. After 0.5 seconds without a valid shape, publication stops.
- Real/simulated source switching changes receiver parameters without restarting nodes.

The millimeter PoseArray convention is deliberate and differs from standard ROS
SI position units. Robot consumers must explicitly convert millimeters to meters
where needed. Slicer rendering rate is separate from ROS publication rate.
