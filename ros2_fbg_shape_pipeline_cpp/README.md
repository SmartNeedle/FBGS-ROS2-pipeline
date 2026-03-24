# ROS 2 FBG Shape Pipeline Reference (C++)

This folder contains a reference ROS 2 C++ architecture for integrating:

- a Windows machine that streams FBG interrogator data over TCP, and
- a Linux ROS 2 machine that processes the stream and publishes a needle shape topic.

The design is based on:

- the packet structure used in the existing `CPP Stream Client`, and
- the node split used in `ros2_needle_shape_publisher_Dimitri`.

The package is intentionally verbose and heavily commented so it can be used as:

- an implementation starting point,
- an architecture reference,
- a teaching aid for ROS 2 and C++, and
- a test harness before the final shape-sensing model is fully ported.

## Contents

- `fbg_shape_msgs/`: ROS 2 messages used to preserve timestamps and metadata.
- `fbg_shape_pipeline_cpp/`: ROS 2 C++ package with three nodes:
  - `tcp_receiver_node`: connects to the Windows TCP stream and publishes parsed frames.
  - `curvature_processor_node`: performs lightweight processing/filtering and republishes a clean curvature topic.
  - `shape_publisher_node`: reconstructs and publishes a `geometry_msgs/msg/PoseArray`.
- `docs/ARCHITECTURE.md`: detailed architecture explanation.
- `docs/TEST_PROTOCOL.md`: step-by-step test and validation procedure, including direct Ethernet (no-router) setup.
- `tools/mock_fbg_stream_server.py`: mock interrogator stream generator.
- `tools/latency_probe.py`: latency/rate validation subscriber.
- `scripts/build_workspace.sh`: OneDrive/synced-folder-safe colcon build helper.
- `scripts/launch_pipeline.sh`: launches from cache install by default, with fallback to local `./install` when present.

## Important note

The shape reconstruction in this reference package is a low-risk baseline implementation intended to be easy to understand and test. It is the right place to plug in your calibrated shape-sensing model once you are ready to port or wrap it in C++.


## One-command Linux dependency setup

To install all required Linux dependencies for this folder in one step, run:

```bash
cd /path/to/repo/CPP/ros2_fbg_shape_pipeline_cpp
bash ./scripts/install_linux_dependencies.sh --ros-distro humble
```

To build in a way that avoids OneDrive/synced-folder symlink errors, run:

```bash
bash ./scripts/build_workspace.sh --ros-distro humble
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")/install/setup.bash"
```

The build helper performs a clean rebuild by default (removes prior build/install/log outputs) to avoid stale setup path issues after workflow changes. By default it installs under `$HOME/.cache/fbg_colcon/...` so launch can execute binaries even if your repo is on a `noexec` synced mount. Use `--no-clean` only when you explicitly want incremental rebuilds.

Optional for local Linux filesystems only (in-workspace build dirs):

```bash
bash ./scripts/build_workspace.sh --ros-distro humble --use-local-build-dirs
source install/setup.bash
```

## Calibration parameters

At launch time, the pipeline attempts to load calibration values from:

- `fbg_shape_pipeline_cpp/config/needle_config.txt`

If the file is present, it overrides the corresponding values from `pipeline.yaml`.
When launching through `scripts/launch_pipeline.sh`, the source-tree file
`fbg_shape_pipeline_cpp/config/needle_config.txt` is exported explicitly so edits
are picked up immediately.

Supported keys in `needle_config.txt`:

- `needle_length_m` (or `needle_length`)
- `first_fbg_index` (1-based, FBGs before this index are ignored)
- `sensor_arc_lengths_m`
- `curvature_scale`
- `orientation_sign`
- `orientation_offset_rad`

Important: `first_fbg_index` is used to offset incoming interrogator arrays.
The calibration vectors in `needle_config.txt` should already represent the
included sensors (i.e., after ignoring leading FBGs).

The current implementation expects the calibration quantities from the paper:

- `sensor_arc_lengths_m`: sensing locations along the needle
- `curvature_scale`: the curvature calibration factor array `C(s)`
- `orientation_sign`: the selected sign array `S-hat(s)`
- `orientation_offset_rad`: the selected offset array `b-hat(s)`

Unit note:

- incoming interrogator curvature values are treated as `1/mm` and converted to `1/m` inside the curvature processor before calibration scaling.

The default placeholders live in:

- `fbg_shape_pipeline_cpp/config/pipeline.yaml`

Replace those placeholder values with your experimentally identified calibration data before running the real system.
