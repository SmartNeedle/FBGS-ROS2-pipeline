# CPP Interrogator to 3D Slicer Protocol

This is the recommended end-to-end workflow for this repository.

## Canonical path

1. `ros2_fbg_shape_pipeline_cpp` receives interrogator TCP data and publishes `/needle/state/current_shape`
2. `smartneedle_interface` converts that `PoseArray` into:
   - `IGTL_POINT_OUT` with device name `NeedleShape`
   - `IGTL_STRING_OUT` with device name `NeedleShapeHeader`
3. `ros2_igtl_bridge` serves those topics over OpenIGTLink
4. `SmartNeedleIGTL-3DSlicer` receives the OpenIGTLink stream in 3D Slicer and renders the centerline

## 1. Build OpenIGTLink

From the repository root on Linux:

```bash
cd /path/to/FBGS-ROS2-pipeline
cmake -S OpenIGTLink -B OpenIGTLink-build -DBUILD_SHARED_LIBS=ON
cmake --build OpenIGTLink-build -j"$(nproc)"
```

Use `OpenIGTLink-build` as the `OpenIGTLink_DIR` input for the ROS build helper.

## 2. Build the ROS 2 stack

```bash
cd /path/to/FBGS-ROS2-pipeline
bash ./scripts/build_cpp_ros2_stack.sh \
  --ros-distro humble \
  --openigtlink-dir ./OpenIGTLink-build
```

Then source the merged install:

```bash
source "$HOME/.cache/fbg_colcon/FBGS-ROS2 pipeline_slicer/install/setup.bash"
```

If your cache/install path differs, just use the exact setup path echoed by the build script.

The canonical build includes:

- `ros2_fbg_shape_pipeline_cpp`
- `ros2_igtl_bridge`
- `smartneedle_interface`

## 3. Verify the shape topic first

Before opening Slicer, verify the ROS topic exists and contains reasonable points:

```bash
source "$HOME/.cache/fbg_colcon/FBGS-ROS2 pipeline_slicer/install/setup.bash"
ros2 topic echo /needle/state/current_shape --once
ros2 topic hz /needle/state/current_shape
```

Assumptions used by the bridge:

- point order is base -> tip unless you enable `reverse_point_order`
- incoming ROS points are in meters
- OpenIGTLink/Slicer points are sent in millimeters by default via `point_scale:=1000.0`

## 4. Start the ROS side

### Option A: bridge an already-running shape topic

```bash
cd /path/to/FBGS-ROS2-pipeline
bash ./scripts/launch_slicer_bridge.sh \
  --install-setup "$HOME/.cache/fbg_colcon/FBGS-ROS2 pipeline_slicer/install/setup.bash"
```

### Option B: launch the full interrogator-to-Slicer stack

```bash
cd /path/to/FBGS-ROS2-pipeline
bash ./scripts/launch_interrogator_to_slicer_stack.sh \
  --install-setup "$HOME/.cache/fbg_colcon/FBGS-ROS2 pipeline_slicer/install/setup.bash"
```

### Option C: hardware-free test path

```bash
source "$HOME/.cache/fbg_colcon/FBGS-ROS2 pipeline_slicer/install/setup.bash"
ros2 launch smartneedle_interface test.launch.py
```

That test launch publishes a virtual `PoseArray`, converts it to OpenIGTLink, and exposes it on port `18944`.

## 5. Install the Slicer-side pieces

In 3D Slicer:

1. Open `Extension Manager`
2. Install `SlicerOpenIGTLink` (this extension provides the `OpenIGTLinkIF` module)
3. Install `CurveMaker` if it is not already installed
4. Open `Edit -> Application Settings -> Modules`
5. Add this additional module path:
   - `/path/to/FBGS-ROS2-pipeline/SmartNeedleIGTL-3DSlicer/SmartNeedle`
6. Restart Slicer

Path note: point Slicer to the `SmartNeedle` subfolder that contains `SmartNeedle.py`, not only the repository-level `SmartNeedleIGTL-3DSlicer` folder.

Important:

- `SmartNeedleIGTL-3DSlicer` is the lightweight scripted module used for this workflow
- install `SlicerOpenIGTLink` from the Slicer extension catalog instead of building a local source copy
- in recent Slicer versions, searching for `OpenIGTLinkIF` may return no direct hit because it is a module shipped inside the `SlicerOpenIGTLink` extension

If you cannot find the `SmartNeedle` module after restart:

1. Go to `Edit -> Application Settings -> Modules` and confirm the additional module path points to:
   - `/path/to/FBGS-ROS2-pipeline/SmartNeedleIGTL-3DSlicer/SmartNeedle`
2. Click `Apply`, then restart Slicer again.
3. Open `View -> Error Log` and check for Python import errors related to `SmartNeedle`.
4. Make sure the folder still contains `SmartNeedle.py` and was not moved/renamed.
5. In the module search box, try both `SmartNeedle` and `Needle`.

## 6. Connect Slicer

In Slicer (beginner-friendly walkthrough):

1. Confirm the ROS bridge is already running (Section 4) and listening on port `18944`.
2. In the module selector (top-left search), open `OpenIGTLinkIF`.
3. In `OpenIGTLinkIF`:
   - go to the `Connectors` area
   - click `+` (Add connector) if no connector exists yet
   - set `Type` to `Client`
   - set `Hostname` to `127.0.0.1`
   - set `Port` to `18944`
   - click `Active` / `Start` for that connector
4. Watch the connector status:
   - `OFF` or `WAIT` means not connected yet
   - `ON` means the socket connection is established
5. Open the `SmartNeedle` module (use the module search box).
6. In SmartNeedle, choose the same OpenIGTLink connector node you just started.
7. Confirm incoming message/device names appear as:
   - `NeedleShapeHeader`
   - `NeedleShape`

Quick sanity checks if you do not see data:

- verify `Hostname` is `127.0.0.1` and `Port` is `18944`
- ensure only one process is bound to port `18944`
- restart connector (`Stop` then `Start`) after restarting ROS launch files
- check ROS logs from `ros2_igtl_bridge` / `smartneedle_interface` for publish activity

## 7. Debugging/tuning

If the geometry is reversed or mirrored, relaunch with one or more of:

```bash
ros2 launch smartneedle_interface bridge.launch.py reverse_point_order:=true
ros2 launch smartneedle_interface bridge.launch.py invert_x:=true
ros2 launch smartneedle_interface bridge.launch.py invert_y:=true
ros2 launch smartneedle_interface bridge.launch.py invert_z:=true
```

Recommended tuning order:

1. Fix units first with `point_scale`
2. Fix point order with `reverse_point_order`
3. Fix axis handedness with `invert_x`, `invert_y`, `invert_z`

## 8. Validation checklist

Confirm all of the following:

- `/needle/state/current_shape` publishes valid points
- the bridge logs show active ROS/OpenIGTLink nodes
- SmartNeedle in Slicer connects successfully
- the displayed point count matches the ROS point count
- the tip moves in the expected direction
- the rendered needle shape bends consistently with a known test motion or the virtual dataset

## 9. Message contract

The bridge publishes:

- OpenIGTLink `POINT` device: `NeedleShape`
- OpenIGTLink `STRING` device: `NeedleShapeHeader`

Header format:

```text
<timestamp_sec.nanosec>;<sequence_number>;<num_points>;<frame_id>
```

## 10. Workspace note

The working directory has already been trimmed to the canonical pipeline, so the folders described in this protocol are the only ones you need to navigate for the interrogator-to-Slicer workflow.
