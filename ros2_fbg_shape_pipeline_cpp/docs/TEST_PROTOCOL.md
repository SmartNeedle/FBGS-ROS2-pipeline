# Test Protocol

Use [the canonical protocol](../../docs/SLICER_ROS2_OPENIGTLINK_PROTOCOL.md) for
build, simulation, hardware, source switching, and Slicer setup.

Run portable contract tests from the repository root:

```bash
ROS_DOMAIN_ID=99 python3 -m unittest discover -s tests -v
```

After building and sourcing the full stack, run the registered C++ tests:

```bash
CACHE="$HOME/.cache/fbg_colcon/$(basename "$PWD")_slicer"
colcon test --base-paths ros2_fbg_shape_pipeline_cpp \
  --build-base "$CACHE/build" --install-base "$CACHE/install" --merge-install \
  --packages-select fbg_shape_pipeline_cpp
colcon test-result --test-result-base "$CACHE/build" --verbose
```

These verify straight and bent SE(3) geometry, physical-base origin, midpoint
boundaries, nonuniform spacing, a single measurement, a measurement at the tip,
variable-length arrays, and malformed packet lengths. Python tests
cover calibration parsing, current sensor selection, simulator packet framing,
and the curvature bound. ROS-specific tests skip when ROS is unavailable.
The Linux workflow builds the external library, bridge, processing, and adapter.

On the target machine record source, curvature, shape, and IGTL topic rates;
sequence increments; receipt-to-shape latency; CPU load; Slicer behavior;
disconnect recovery; stale-data pause; and repeated real/simulated switches.
Run for at least 60 seconds after warmup. Do not equate repeated 100 Hz adapter
output with 100 distinct sensor frames or 100 Slicer renders per second.

For a straight frame expect 19 points, first point zero, and final z about
196.391633 mm. Points represent segment boundaries, not FBG locations.
For the default simulator expect a bending shape with fixed
reconstructed arc length. Real validation additionally requires comparing
known physical bends against the calibration and coordinate conventions.
