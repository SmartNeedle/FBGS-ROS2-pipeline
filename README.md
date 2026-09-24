# FBGS ROS 2 Pipeline

Real interrogator data and simulated TCP data use the same receiver, calibration,
curvature processing, SE(3) reconstruction, and OpenIGTLink output.
The curvature callback publishes curvature and reconstructed shape together,
avoiding an extra ROS message handoff.
The receiver publishes a compact sensor-only frame for this processing path
while retaining complete raw frames on /needle/fbg_frame. Use
--full-frame-input to compare against the original raw-frame handoff.

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
- Curvature is used directly from the interrogator in 1/mm; sensor angle signs
  and offsets come from calibration.
- Reconstruction spans the physical base to the calibrated tip, using one SE(3)
  exponential per constant-curvature segment bounded by measurement midpoints.
- No curvature interpolation or uniform output resampling.
- Adapter target: 100 Hz. After 0.5 seconds without a valid shape, publication stops.
- Real/simulated source switching changes receiver parameters without restarting nodes.

The millimeter PoseArray convention is deliberate and differs from standard ROS
SI position units. Robot consumers must explicitly convert millimeters to meters
where needed. Slicer rendering rate is separate from ROS publication rate.
