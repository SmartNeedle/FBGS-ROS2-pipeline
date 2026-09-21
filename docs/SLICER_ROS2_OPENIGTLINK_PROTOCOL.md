# CPP Interrogator to 3D Slicer Protocol

This is the recommended end-to-end workflow for this repository.

## Canonical path

1. `ros2_fbg_shape_pipeline_cpp` receives interrogator TCP data and publishes `/needle/state/current_shape`
2. `ros2_smartneedle_adapter` converts that `PoseArray` into:
   - `IGTL_POINT_OUT` with device name `NeedleShape`
   - `IGTL_STRING_OUT` with device name `NeedleShapeHeader`
3. The untouched collaborator `ros2_igtl_bridge` serves those topics over OpenIGTLink
4. The untouched collaborator `SmartNeedleIGTL-3DSlicer` receives the OpenIGTLink stream in 3D Slicer and renders the centerline

## 0. Initialize external dependencies

From the repository root:

```bash
git submodule update --init --recursive
```

The submodules are pinned by `.gitmodules`; do not edit their contents in this repository.

## 1. Build OpenIGTLink

From the repository root on Linux:

```bash
cd /path/to/FBGS-ROS2-pipeline
cmake -S "external dependencies/OpenIGTLink" -B OpenIGTLink-build -DBUILD_SHARED_LIBS=ON
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

Then source the merged install (and expose OpenIGTLink shared libraries):

```bash
source "$HOME/.cache/fbg_colcon/FBGS-ROS2 pipeline_slicer/install/setup.bash"
export LD_LIBRARY_PATH="/path/to/FBGS-ROS2-pipeline/OpenIGTLink-build/bin:/path/to/FBGS-ROS2-pipeline/OpenIGTLink-build/lib:${LD_LIBRARY_PATH}"
```

If your cache/install path differs, just use the exact setup path echoed by the build script.

The canonical build includes:

- `ros2_fbg_shape_pipeline_cpp`
- external collaborator `ros2_igtl_bridge`
- local `ros2_smartneedle_adapter`

## 3. Verify the shape topic first

Before opening Slicer, verify the ROS topic exists and contains reasonable points:

```bash
source "$HOME/.cache/fbg_colcon/FBGS-ROS2 pipeline_slicer/install/setup.bash"
ros2 topic echo /needle/state/current_shape --once
ros2 topic hz /needle/state/current_shape
```

Assumptions used by the bridge:

- point order is base -> tip
- incoming ROS points are in millimeters
- OpenIGTLink/Slicer points are forwarded in millimeters without an additional scale

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
  --install-setup "$HOME/.cache/fbg_colcon/FBGS-ROS2 pipeline_slicer/install/setup.bash" \
  --tcp-host 192.168.50.1 \
  --tcp-port 50012
```

Use the interrogator sender IP for `--tcp-host`.
Do **not** use `127.0.0.1` unless the interrogator TCP sender process is running on the same Linux machine.

If logs show `TCP receiver error: connect: Connection refused`:

1. Verify target endpoint from Linux:
   ```bash
   nc -vz 192.168.50.1 50012
   ```
2. Confirm the Windows interrogator stream app is running and listening on TCP `50012`.
3. Confirm Windows firewall allows inbound TCP `50012` on the Ethernet/private profile.
4. Re-run Option B with the correct `--tcp-host`.

Note: `igtl_node: Waiting for connection.` is expected until Slicer starts the OpenIGTLink connector.

### Option C: hardware-free test path

```bash
source "$HOME/.cache/fbg_colcon/FBGS-ROS2 pipeline_slicer/install/setup.bash"
ros2 launch ros2_smartneedle_adapter full_pipeline.launch.py \
  tcp_host:=127.0.0.1 tcp_port:=50012
```

That launch starts the real TCP receiver and bridge. For a hardware-free simulator, run the simulator separately and publish its frames to the same receiver input contract; the downstream ROS and Slicer path is unchanged.

### Optional hardware networking: direct Ethernet (no-router) setup

Use this when Linux and Windows are connected with a direct Ethernet cable and no DHCP router.

Goal:

- create a stable point-to-point network so Linux can reach the Windows TCP stream host on port `50012`

1. Physical setup:
   - connect one Ethernet cable directly between the Windows interrogator PC and Linux ROS 2 PC
   - disable Wi-Fi on both machines during initial validation to avoid route ambiguity
2. Assign static IP addresses (example `/24`):
   - Windows: `192.168.50.1`
   - Linux: `192.168.50.2`
   - netmask: `255.255.255.0`
   - gateway: leave blank for this isolated link
3. Configure Windows IPv4 manually:
   - `Control Panel -> Network and Internet -> Network Connections`
   - right-click Ethernet adapter -> `Properties`
   - open `Internet Protocol Version 4 (TCP/IPv4)`
   - choose `Use the following IP address`
   - set IP `192.168.50.1`, subnet mask `255.255.255.0`, gateway blank
4. Configure Linux IPv4 manually (NetworkManager GUI):
   - open Ethernet interface settings
   - set IPv4 method to `Manual`
   - add address `192.168.50.2`, prefix `24`, gateway blank

Linux CLI alternative (temporary; replace `eth0`):

```bash
sudo ip addr flush dev eth0
sudo ip addr add 192.168.50.2/24 dev eth0
sudo ip link set eth0 up
```

Validate link and stream reachability from Linux:

```bash
ping -c 4 192.168.50.1
nc -vz 192.168.50.1 50012
```

If `nc` fails:

- confirm the Windows stream source is running and bound to the Ethernet IP
- allow inbound TCP `50012` in Windows Firewall (Private profile)
- confirm no VPN/firewall policy is overriding routes

Windows Firewall CLI examples (Administrator Command Prompt):

```bash
netsh advfirewall firewall add rule name="Allow ICMPv4-In" protocol=icmpv4:8,any dir=in action=allow
netsh advfirewall firewall add rule name="Allow TCP 50012" dir=in action=allow protocol=TCP localport=50012
```

Then run the full interrogator-to-Slicer stack and point the interrogator TCP host to the Windows direct-link IP (`192.168.50.1`).

## 5. Install the Slicer-side pieces

In 3D Slicer:

1. Open `Extension Manager`
2. Install `SlicerOpenIGTLink` (this extension provides the `OpenIGTLinkIF` module)
3. Install `CurveMaker` if it is not already installed
4. Open `Edit -> Application Settings -> Modules`
5. Add this additional module path:
   - `/path/to/FBGS-ROS2-pipeline/external dependencies/SmartNeedleIGTL-3DSlicer/SmartNeedle`
6. Restart Slicer

Path note: point Slicer to the `SmartNeedle` subfolder that contains `SmartNeedle.py`, not only the repository-level `SmartNeedleIGTL-3DSlicer` folder.

Important:

- `SmartNeedleIGTL-3DSlicer` is the lightweight scripted module used for this workflow
- install `SlicerOpenIGTLink` from the Slicer extension catalog instead of building a local source copy
- in recent Slicer versions, searching for `OpenIGTLinkIF` may return no direct hit because it is a module shipped inside the `SlicerOpenIGTLink` extension

If you cannot find the `SmartNeedle` module after restart:

1. Go to `Edit -> Application Settings -> Modules` and confirm the additional module path points to:
   - `/path/to/FBGS-ROS2-pipeline/external dependencies/SmartNeedleIGTL-3DSlicer/SmartNeedle`
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

### If connector status stays `WAIT` in hardware-free mode

Symptom pattern:

- Slicer connector stays `WAIT`
- ROS launch shows `igtl_node` exit code `127`
- log includes `error while loading shared libraries: libOpenIGTLink.so.3: cannot open shared object file`

This means the OpenIGTLink shared library is not on your runtime library path.

```bash
# 1) locate the library directory
find /path/to/FBGS-ROS2-pipeline/OpenIGTLink-build -name 'libOpenIGTLink.so*'

# 2) add likely build output directories to LD_LIBRARY_PATH (both .../bin and .../lib)
export LD_LIBRARY_PATH="/path/to/FBGS-ROS2-pipeline/OpenIGTLink-build/bin:/path/to/FBGS-ROS2-pipeline/OpenIGTLink-build/lib:${LD_LIBRARY_PATH}"

# 3) re-source ROS install and relaunch the hardware-free test
source "$HOME/.cache/fbg_colcon/FBGS-ROS2 pipeline_slicer/install/setup.bash"
ros2 launch ros2_smartneedle_adapter slicer_bridge.launch.py
```

Optional verification before relaunch:

```bash
ldd "$HOME/.cache/fbg_colcon/FBGS-ROS2-pipeline_slicer/install/lib/ros2_igtl_bridge/igtl_node" | grep -i OpenIGTLink
```

You should see `libOpenIGTLink.so.3 => /.../OpenIGTLink-build/bin/libOpenIGTLink.so.3` or `/.../OpenIGTLink-build/lib/libOpenIGTLink.so.3`, not `not found`.

The current millimeter pipeline forwards coordinates without an additional
scale or axis inversion. Coordinate-frame changes should be made explicitly
in the reconstruction or adapter contract after confirming them against the
collaborator Slicer module.

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
