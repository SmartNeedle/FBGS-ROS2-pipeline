# ROS 2 -> 3D Slicer Needle Visualization Protocol

This repository now includes a complete ROS 2 adapter for the SmartNeedle Slicer module:

- your existing pipeline publishes `geometry_msgs/msg/PoseArray` on `/needle/state/current_shape`
- `fbg_slicer_bridge/needle_shape_to_igtl_node` converts that topic into:
  - `IGTL_STRING_OUT` with device name `NeedleShapeHeader`
  - `IGTL_POINT_OUT` with device name `NeedleShape`
- `ros2_igtl_bridge` forwards those ROS topics to OpenIGTLink
- the SmartNeedle Slicer module consumes those two incoming nodes and renders the needle

## 1. Linux prerequisites

Install:

- ROS 2 Humble or Jazzy
- build tools: `build-essential cmake git python3-colcon-common-extensions python3-rosdep`
- 3D Slicer on the same Linux machine
- OpenIGTLink built from source

Build OpenIGTLink:

```bash
cd ~
git clone https://github.com/openigtlink/OpenIGTLink.git
cmake -S OpenIGTLink -B OpenIGTLink-build -DBUILD_SHARED_LIBS=ON
cmake --build OpenIGTLink-build -j"$(nproc)"
```

Use this CMake directory when building the ROS workspace:

```bash
~/OpenIGTLink-build
```

## 2. Build the ROS 2 integration stack

From the `CPP` folder:

```bash
cd /path/to/FBGS/CPP
bash ./scripts/build_cpp_ros2_stack.sh \
  --ros-distro humble \
  --openigtlink-dir ~/OpenIGTLink-build
```

Then source the merged install:

```bash
source ~/.cache/fbg_colcon/CPP_slicer/install/setup.bash
```

If your OpenIGTLink build directory is somewhere else, pass that path instead.

## 3. Verify your needle shape topic first

Before opening Slicer, verify the reconstructed shape:

```bash
source ~/.cache/fbg_colcon/CPP_slicer/install/setup.bash
ros2 topic echo /needle/state/current_shape --once
ros2 topic hz /needle/state/current_shape
```

The integration assumes:

- point order is base -> tip
- units on the ROS topic are meters
- the SmartNeedle/OpenIGTLink stream should be millimeters

The adapter converts meters -> millimeters by default with `point_scale=1000.0`.

## 4. Start the ROS side

### Option A: launch only the bridge for an already-running pipeline

```bash
cd /path/to/FBGS/CPP
bash ./scripts/launch_slicer_bridge.sh \
  --install-setup ~/.cache/fbg_colcon/CPP_slicer/install/setup.bash
```

### Option B: launch the full FBG + bridge stack

```bash
source ~/.cache/fbg_colcon/CPP_slicer/install/setup.bash
ros2 launch fbg_slicer_bridge full_fbg_slicer_stack.launch.py \
  tcp_host:=127.0.0.1 \
  tcp_port:=50012 \
  bridge_port:=18944
```

## 5. Install the Slicer-side pieces

In 3D Slicer:

1. Open the `Extension Manager`.
2. Install `OpenIGTLinkIF` from the extension catalog.
3. Install `CurveMaker` and `ZFrameRegistration` if they are not already present.
4. Open `Edit -> Application Settings -> Modules`.
5. Add this additional module path:
   - `/path/to/FBGS/CPP/SmartNeedle-3DSlicer`
6. Restart Slicer.
7. Confirm these modules are available:
   - `SmartNeedle`
   - `OpenIGTLinkIF`
   - `CurveMaker`
   - `ZFrameRegistration`

Important:

- `SmartNeedle-3DSlicer` is a scripted module, so adding its folder as an additional module path is enough.
- `SlicerOpenIGTLink` in this repository is extension source code, not a prebuilt extension. For the fastest working setup, use the prebuilt `OpenIGTLinkIF` from the Slicer Extension Manager instead of trying to load the source tree directly.

## 6. Connect Slicer to the ROS bridge

In Slicer:

1. Open the `SmartNeedle` module.
2. Set:
   - `Hostname`: `127.0.0.1`
   - `Port`: `18944`
3. Select your Z-frame transform if you have one.
4. Click `Start client`.

The ROS bridge runs as an OpenIGTLink server by default, which matches the SmartNeedle client workflow.

When connected, SmartNeedle should receive:

- `NeedleShapeHeader`
- `NeedleShape`

## 7. Coordinate debugging

If the needle appears mirrored, upside down, or reversed:

```bash
source ~/.cache/fbg_colcon/CPP_slicer/install/setup.bash
ros2 launch fbg_slicer_bridge needle_to_slicer_bridge.launch.py \
  invert_x:=true
```

Other useful variants:

```bash
ros2 launch fbg_slicer_bridge needle_to_slicer_bridge.launch.py invert_y:=true
ros2 launch fbg_slicer_bridge needle_to_slicer_bridge.launch.py invert_z:=true
ros2 launch fbg_slicer_bridge needle_to_slicer_bridge.launch.py reverse_point_order:=true
```

Recommended tuning order:

1. Fix units first with `point_scale`
2. Fix point ordering with `reverse_point_order`
3. Fix handedness with `invert_x`, `invert_y`, `invert_z`

## 8. Runtime validation checklist

Confirm all of the following:

- `ros2 topic echo /needle/state/current_shape --once` shows valid points
- the bridge terminal reports connection to Slicer
- Slicer SmartNeedle status changes to connected
- the number of points shown in SmartNeedle matches the ROS shape point count
- the needle tip in Slicer moves in the expected direction
- the rendered needle bends consistently with your known test motion

## 9. Message contract used by this integration

The adapter publishes:

- OpenIGTLink `POINT` device: `NeedleShape`
- OpenIGTLink `STRING` device: `NeedleShapeHeader`

Header format:

```text
<timestamp_sec.nanosec>;<sequence_number>;<num_points>;<frame_id>
```

This matches the SmartNeedle parser in:

- `SmartNeedle-3DSlicer/SmartNeedle/SmartNeedle.py`

## 10. Important notes

- The SmartNeedle module is used as the visualization endpoint; you do not need to write custom Slicer rendering code.
- The new adapter assumes the ROS shape topic already represents the needle centerline.
- The bridge is intended for visualization feedback, not for real-time control timing.
