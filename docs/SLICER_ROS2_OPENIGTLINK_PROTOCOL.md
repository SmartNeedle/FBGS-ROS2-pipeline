# Linux to Slicer Protocol

Use ROS 2 Humble on Linux. Run commands from the repository root unless stated.
Both real and simulated operation use the same calibration and processing nodes.

## 1. Synchronize and build

```bash
cd ~/Documents/FBGS-ROS2-pipeline
git status --short
git pull --ff-only
bash scripts/init_dependencies.sh
source /opt/ros/humble/setup.bash
bash ros2_fbg_shape_pipeline_cpp/scripts/install_linux_dependencies.sh --ros-distro humble
```

If git status reports local source edits, preserve and reconcile them before
pulling. Never copy Windows build artifacts to Linux.

On the first migration from the old vendored dependency layout, remove the
generated OpenIGTLink-build directory (its CMake cache points to the old source).
This directory must contain build outputs only. Then build:

```bash
# One-time migration cleanup; run from the repository root.
rm -rf -- ./OpenIGTLink-build
cmake -S "external dependencies/OpenIGTLink" -B OpenIGTLink-build \
  -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF
cmake --build OpenIGTLink-build -j"$(nproc)"
bash scripts/build_cpp_ros2_stack.sh --ros-distro humble \
  --openigtlink-dir "$PWD/OpenIGTLink-build"
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")_slicer/install/setup.bash"
export LD_LIBRARY_PATH="$PWD/OpenIGTLink-build/bin:$PWD/OpenIGTLink-build/lib:${LD_LIBRARY_PATH:-}"
ros2 pkg executables ros2_smartneedle_adapter
```

The executable list must include smartneedle_igtl_100hz. The build contains four
packages: fbg_shape_msgs, fbg_shape_pipeline_cpp, ros2_igtl_bridge, and
ros2_smartneedle_adapter. The collaborator interface remains a source reference.

Before manual testing, run the automated checks in this sourced terminal.
The Python integration test launches and stops its own complete stack; do not
start the manual pipeline yet. Use a separate ROS domain to avoid other nodes:

```bash
ROS_DOMAIN_ID=99 python3 -m unittest discover -s tests -v
CACHE="$HOME/.cache/fbg_colcon/$(basename "$PWD")_slicer"
colcon test --base-paths ros2_fbg_shape_pipeline_cpp \
  --build-base "$CACHE/build" --install-base "$CACHE/install" --merge-install \
  --packages-select fbg_shape_pipeline_cpp
colcon test-result --test-result-base "$CACHE/build" --verbose
```

Expect no failures and no skipped Python tests on a built Linux workspace.
The checks include wire-level OpenIGTLink output, live source switching between
two synthetic TCP servers, error/stale suppression, recovery, and SE(3).

## 2. Start simulated input

Terminal 1:

```bash
python3 ros2_fbg_shape_pipeline_cpp/tools/mock_fbg_stream_server.py \
  --host 127.0.0.1 --port 50012 --rate-hz 100
```

The simulator reads the current needle_config.txt and derives 20 incoming
values. To select another sensor, pass --needle-config PATH and use the same
path as FBG_NEEDLE_CONFIG_FILE in the pipeline terminal. Temperature, spectra,
and packet shape contain synthetic values; only curvature and angle drive
reconstruction. The synthetic frame matches the field order and dimensions of
one locally inspected ShapeCore capture: 20 sensor values, a 192x3 packet-shape
array, and four spectra cores with 512 spectrum samples and 20 peaks each.
At 20 sensor values this is 15,767 bytes per TCP frame including the length
prefix. No captured packet contents are included in the repository. This
matches one observed packet size, not the real stream's jitter, size variation,
or error frequency; compare rates with live interrogator diagnostics too.

Terminal 2:

Open a fresh terminal and return to ~/Documents/FBGS-ROS2-pipeline before running:

```bash
bash scripts/launch_interrogator_to_slicer_stack.sh \
  --install-setup "$HOME/.cache/fbg_colcon/$(basename "$PWD")_slicer/install/setup.bash" \
  --tcp-host 127.0.0.1 --tcp-port 50012
```

The helper uses the source-tree calibration unless FBG_NEEDLE_CONFIG_FILE is
already set. It also sets the library search path. Waiting for an IGTL connection
is normal until Slicer starts its connector.
By default the curvature processor publishes both the curvature topic and the
reconstructed shape topic in one callback. Add --separate-shape to the helper
command to restore the two-node path for rate and latency comparisons. Only
one of these paths publishes /needle/state/current_shape per launch.
The receiver also publishes a compact /needle/fbg_sensor_frame for processing
before publishing the complete /needle/fbg_frame for inspection. The compact
message has the same header, error, line number, source timestamp, curvature,
angle, and temperature, with empty packet shape and spectra fields. Add
--full-frame-input to process the complete raw message instead; this disables
the compact topic and allows a direct rate/latency comparison.

## 3. Configure Slicer

1. Install SlicerOpenIGTLink (which provides OpenIGTLinkIF) and CurveMaker.
2. Add the absolute path to external dependencies/SmartNeedleIGTL-3DSlicer/SmartNeedle
   in Settings -> Modules -> Additional module paths, and restart Slicer.
3. Open SmartNeedle and select/create its connector. The external module initializes
   a newly selected connector to localhost:18944.
4. In OpenIGTLinkIF verify Client, host 127.0.0.1 for Slicer on the Linux machine
   (otherwise the Linux machine's IP), and port 18944. Set these AFTER first
   selecting the connector in SmartNeedle so defaults do not overwrite them.
5. Start the connector and press Start in SmartNeedle. Confirm NeedleShape and
   NeedleShapeHeader appear and the connector is ON.

The module path must end in SmartNeedle, the directory containing SmartNeedle.py.
Remove the old root-level module path to avoid loading a stale copy.
Coordinates are millimeters, with no extra scale or axis inversion.

## 4. Verify

In another sourced terminal:

```bash
cd ~/Documents/FBGS-ROS2-pipeline
source /opt/ros/humble/setup.bash
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")_slicer/install/setup.bash"
ros2 topic echo /needle/state/current_shape --once
ros2 topic hz /needle/fbg_frame
# Stop each hz command with Ctrl-C before starting the next.
ros2 topic hz /needle/state/curvatures
ros2 topic hz /needle/state/current_shape
ros2 topic hz /IGTL_POINT_OUT
python3 ros2_fbg_shape_pipeline_cpp/tools/latency_probe.py
```

Expect 19 points starting at zero, a changing bent shape, and rates near 100 Hz
after warmup. Points are the base and segment ends, not FBG locations.
The represented arc length is 196.391633 mm; endpoint distance and
the polyline chord length need not equal arc length. Curvature stays below
0.004 1/mm. Slicer need not redraw at the ROS publishing rate.

Stop the simulator: after 0.5 seconds without valid shapes, the adapter logs
stale input and stops publishing. Slicer keeps its last geometry. Restart the
simulator: acquisition and adapter publication should resume automatically.
Verify Ctrl-C stops the receiver even while the sender is idle.

## 5. Real input and live switching

Start the Windows interrogator TCP sender. For a direct Ethernet link, assign
Windows 192.168.50.1/24 and Linux 192.168.50.2/24 without a gateway. Permit inbound
TCP 50012 in the Windows private-network firewall. Confirm reachability:

```bash
ping -c 4 192.168.50.1
nc -vz 192.168.50.1 50012
```

Both sources can remain running. Switch the active receiver without restarting
ROS nodes, the bridge, or Slicer:

```bash
# Real sensor
ros2 param set /tcp_receiver_node tcp_host 192.168.50.1
# Simulated sensor
ros2 param set /tcp_receiver_node tcp_host 127.0.0.1
```

Use the same port on both sources for a one-command switch. tcp_port can also
change at runtime. Switching may produce a short gap and an immediate geometry
change; there is no interpolation between sources. Verify topic timestamps and
motion after each switch. No automatic fallback to simulation is performed.

Perform at least five real/simulated/real cycles. Keep the same 20-value sensor
calibration for both sources. Observe the direction and amplitude of a known
physical bend and compare the simulator's controlled motion. The connector
should remain ON and the ROS node processes should remain running.

Stop the real sender while selected: verify output pauses, then resumes after
restarting it. Finally stop the pipeline with Ctrl-C while its selected source
is idle or disconnected; it should exit without an indefinite receiver wait.

## 6. Troubleshooting and collaborators

If the bridge exits with a missing libOpenIGTLink error, inspect:

```bash
ldd "$HOME/.cache/fbg_colcon/$(basename "$PWD")_slicer/install/lib/ros2_igtl_bridge/igtl_node"
```

Source the install and export the library path from step 1 in every manual
launch terminal. If Slicer stays WAIT, check host, port, firewall, and bridge logs.
If ROS emits no shapes, check calibration validation, incoming array lengths,
interrogator error codes, and the receiver connection.

Share the GitHub repository URL and this protocol. Collaborators must have read
access to this repository and all three pinned upstream repositories.
Use [the test protocol](../ros2_fbg_shape_pipeline_cpp/docs/TEST_PROTOCOL.md) and
[audit report](AUDIT.md) for checks and remaining validation limits.
