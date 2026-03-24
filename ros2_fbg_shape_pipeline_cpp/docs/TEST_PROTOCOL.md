# Test Protocol

This protocol is written so you can validate the system in increasing levels of realism.

## 1. What success looks like

The pipeline is considered validated when all of the following are true:

- the Linux receiver connects to the Windows TCP stream,
- packets are parsed without crashes or drift,
- `FbgFrame` messages are published at the expected rate,
- curvature messages are published at the expected rate,
- shape messages are published at the expected rate,
- reconnect works after a cable unplug or process restart,
- measured end-to-end latency is acceptable for your application,
- subscribers can consume the final `PoseArray` directly.

## 2. Test stages

Run the tests in this order:

1. Build test
2. Mock stream smoke test
3. Latency and rate test with mock data
4. Reconnect test
5. Real Windows-to-Linux network test
6. Full integration test with a downstream subscriber

## 3. Build test

On the Linux ROS 2 machine:

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp
bash ./scripts/build_workspace.sh --ros-distro humble
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")/install/setup.bash"
```

Expected result:

- both packages build successfully,
- executables appear in the install space,
- launch file is available.

Note:

- `build_workspace.sh` performs a clean rebuild by default to prevent stale setup-path errors; use `--no-clean` only for intentional incremental builds,
- `build_workspace.sh` defaults to build/install/log folders under `$HOME/.cache/fbg_colcon/...`, which avoids symlink and executable-permission (`noexec`) issues on OneDrive/synced mounts,
- for local Linux filesystems, you can use `bash ./scripts/build_workspace.sh --ros-distro humble --use-local-build-dirs`.

## 4. Mock stream smoke test

Terminal 1 (mock data source):

Use this terminal to run a fake TCP interrogator stream at the requested rate.
This simulates the Windows sender so you can validate parsing and ROS publication without hardware.

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp/tools
python3 mock_fbg_stream_server.py --host 0.0.0.0 --port 50012 --rate-hz 100
```

Terminal 2 (pipeline launch):

This launches all three ROS 2 nodes (`tcp_receiver_node`, `curvature_processor_node`, `shape_publisher_node`) with the configured TCP endpoint.

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")/install/setup.bash"
bash ./scripts/launch_pipeline.sh --tcp-host 127.0.0.1 --tcp-port 50012
```

Terminal 3 (receiver output rate):

This verifies how fast parsed `FbgFrame` messages are being published by the receiver stage.

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")/install/setup.bash"
ros2 topic hz /needle/fbg_frame
```

Terminal 4 (processor output rate):

This checks the intermediate curvature topic to confirm processing keeps pace with input frames.

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")/install/setup.bash"
ros2 topic hz /needle/state/curvatures
```

Terminal 5 (final shape output rate):

This checks the end of the pipeline and confirms `PoseArray` shape updates are being published continuously.

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")/install/setup.bash"
ros2 topic hz /needle/state/current_shape
```

Expected result:

- all topics publish near the configured mock rate,
- no steady growth in memory or CPU,
- no disconnect/reconnect loop.

## 5. Latency test with mock data

Run this in a separate terminal while the pipeline is still running:

This subscriber measures source-to-receiver and shape receive latency statistics over time.

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp/tools
python3 latency_probe.py
```

Expected result:

- the script prints receive rate,
- the script prints source-to-ROS latency estimates,
- the script prints frame-to-shape latency estimates.
- if source timestamps come from an unsynchronized clock domain, the script reports skipped source samples and prints a relative source-latency summary (clock-epoch independent, based on frame-to-frame timing deltas).

Acceptance target:

- start by documenting median, 95th percentile, and max latency,
- then decide whether the result is clinically and robotically acceptable.
- note that all-time `max` is cumulative and can only stay the same or increase; use the printed rolling `window_max` to track current jitter.

## 6. Reconnect test

While the pipeline is running:

1. Stop the mock stream server.
2. Confirm the receiver logs reconnect attempts.
3. Restart the mock stream server.
4. Confirm the receiver resumes publishing without restarting ROS 2.

Expected result:

- no node crash,
- automatic reconnection,
- topic publication resumes.

## 6.5 Direct Ethernet (no-router) setup protocol

Use this section when Linux and Windows are connected with a direct Ethernet cable and no DHCP router.

### Goal

Create a stable point-to-point network so Linux can reach the Windows TCP stream host (`tcp_host`) on port `50012`.

### A. Physical setup

1. Connect one Ethernet cable directly between the Windows interrogator PC and the Linux ROS 2 PC.
2. Disable Wi-Fi on both machines during initial validation to avoid route ambiguity.

### B. Assign static IP addresses

Use a private /24 network reserved for this link. Example:

- Windows: `192.168.50.1`
- Linux: `192.168.50.2`
- Netmask: `255.255.255.0` (`/24`)
- Gateway: leave empty for this isolated link

Windows steps:

1. Open **Control Panel -> Network and Internet -> Network Connections**.
2. Right-click the Ethernet adapter -> **Properties**.
3. Open **Internet Protocol Version 4 (TCP/IPv4)**.
4. Select **Use the following IP address** and enter:
   - IP: `192.168.50.1`
   - Subnet mask: `255.255.255.0`
   - Default gateway: blank
5. Save and close.

Linux steps (NetworkManager GUI):

1. Open Network settings for the Ethernet interface.
2. Set IPv4 method to **Manual**.
3. Add:
   - Address: `192.168.50.2`
   - Prefix: `24`
   - Gateway: blank
4. Save and reconnect interface.

Linux CLI alternative (temporary, replace `eth0` with your interface):

```bash
sudo ip addr flush dev eth0
sudo ip addr add 192.168.50.2/24 dev eth0
sudo ip link set eth0 up
```

### C. Verify link connectivity

On Linux:

```bash
ping -c 4 192.168.50.1
```

Expected:

- replies received (0% packet loss ideally).

### D. Verify stream port reachability

On Linux:

```bash
nc -vz 192.168.50.1 50012
```

Expected:

- connection succeeds when the Windows stream source is running and firewall permits TCP/50012.

If it fails:

- confirm Windows stream application is active and bound to the Ethernet IP,
- allow inbound TCP port `50012` in Windows Firewall (Private profile),
- confirm no VPN/firewall policy is overriding routes.

Windows Firewall CLI examples (run in **Administrator Command Prompt**):

```bash
netsh advfirewall firewall add rule name="Allow ICMPv4-In" protocol=icmpv4:8,any dir=in action=allow
netsh advfirewall firewall add rule name="Allow TCP 50012" dir=in action=allow protocol=TCP localport=50012
```

### E. Run the pipeline against direct-link Windows IP

On Linux:

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp
bash ./scripts/build_workspace.sh --ros-distro humble
bash ./scripts/launch_pipeline.sh --tcp-host 192.168.50.1 --tcp-port 50012
```

In another Linux terminal:

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")/install/setup.bash"
ros2 topic echo /needle/fbg_frame --once
ros2 topic echo /needle/state/curvatures --once
ros2 topic echo /needle/state/current_shape --once
```

Expected:

- all three topics publish at least one message,
- timestamps are populated,
- no repeated reconnect loop in launch logs.

### F. Debugging checklist (based on common setup failures)

If something fails, run these checks in order.

#### 1) Verify Windows adapter really has the static IPv4 you set

On Windows (Command Prompt):

```bash
ipconfig
```

Confirm the Ethernet adapter shows exactly:

- IPv4 address (example): `192.168.50.1`
- Subnet mask: `255.255.255.0`

If not, reapply the static IPv4 settings and disable/re-enable the adapter.

#### 2) Verify Linux adapter and route

On Linux:

```bash
ip -4 addr
ip route
```

Confirm your Ethernet interface is `192.168.50.2/24` and traffic to `192.168.50.1` goes through that interface.

#### 3) If `ping` fails

- ensure Wi-Fi/VPN is disabled temporarily on both machines,
- confirm cable link LEDs are active,
- ensure both ends are in the same `/24` subnet (`192.168.50.x`),
- retry:

```bash
ping -c 4 192.168.50.1
```

#### 4) If `nc -vz 192.168.50.1 50012` fails

- start/restart the Windows stream server,
- confirm stream app is bound to the Ethernet IP,
- allow inbound TCP `50012` in Windows Firewall (Private profile),
- retest:

```bash
nc -vz 192.168.50.1 50012
```

#### 5) If launch fails with `PermissionError: [Errno 13] .../install/lib/...`

This usually means your shell is resolving to a non-executable install path on a synced/noexec mount.

Recovery:

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp
bash ./scripts/build_workspace.sh --ros-distro humble
bash ./scripts/launch_pipeline.sh --tcp-host 192.168.50.1 --tcp-port 50012
```

#### 6) If launch fails with `COLCON_TRACE` / `AMENT_TRACE_SETUP_FILES` unbound variable

Use the helper script (it handles setup sourcing safely):

```bash
bash ./scripts/launch_pipeline.sh --tcp-host 192.168.50.1 --tcp-port 50012
```

Avoid manually sourcing setup files in shells that enforce `set -u`.

#### 7) If helper says setup file not found

Rebuild and choose one layout explicitly:

```bash
# cache layout (default)
bash ./scripts/build_workspace.sh --ros-distro humble

# or local layout
bash ./scripts/build_workspace.sh --ros-distro humble --use-local-build-dirs
```

Then launch again with `launch_pipeline.sh`.

## 7. Real network test

Replace the mock server with the real Windows stream source.

On Linux (pipeline against the real Windows source):

This starts the same three ROS 2 nodes but points the TCP client at the real interrogator host.

```bash
cd /path/to/repo/ros2_fbg_shape_pipeline_cpp
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")/install/setup.bash"
bash ./scripts/launch_pipeline.sh --tcp-host WINDOWS_IP --tcp-port 50012
```

Then validate in another terminal:

```bash
ros2 topic echo /needle/fbg_frame --once
ros2 topic echo /needle/state/curvatures --once
ros2 topic echo /needle/state/current_shape --once
```

Expected result:

- messages arrive,
- timestamps are populated,
- shape topic is available to any ROS 2 subscriber.


## 8. Downstream subscriber validation

Create or reuse a subscriber that consumes:

- `/needle/state/current_shape`

Minimum checks:

- it can subscribe without custom parsing,
- it receives updates continuously,
- it can handle reconnects upstream.

## 9. Recommended benchmark matrix

Record results for:

- 50 Hz
- 100 Hz
- 150 Hz, if your interrogator can support it

For each rate, record:

- CPU usage on Linux
- packet loss or dropped topic samples
- median latency
- 95th percentile latency
- reconnect recovery time

## 10. Before clinical or robot tests

Do not use the reference shape model for final experiments without validating:

- calibration,
- coordinate frame conventions,
- insertion-depth conventions,
- units,
- and the final reconstruction model against ground truth.
