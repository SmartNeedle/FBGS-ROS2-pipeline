# FBGS ROS 2 Shape Pipeline

This repository reconstructs a fiber-Bragg-grating needle shape and streams it to 3D Slicer over OpenIGTLink. Until the physical interrogator is available, collaborators can use the included TCP simulator. The simulator replaces only the incoming interrogator stream: receiver, calibration, curvature processing, SE(3) reconstruction, ROS topics, bridge, and Slicer interface are the same.

## System architecture

```text
REAL: interrogator/ShapeCore TCP server        SIMULATED: mock_fbg_stream_server.py
Windows, <INTERROGATOR_IP>:50012               Linux, 127.0.0.1:50012
                     \                         /
                      \-- same packet contract /
                                  |
                                  v
                         tcp_receiver_node
                   parse, validate, source reconnect
                       |                    |
                       |                    +--> /needle/fbg_frame
                       |                         full raw FbgFrame
                       +--> /needle/fbg_sensor_frame
                            compact FbgFrame (default input)
                                      |
                                      v
                           curvature_processor_node
                        calibration + angle conventions
                              /               \
                             v                 v
             /needle/state/curvatures   /needle/state/current_shape
                 CurvatureFrame              PoseArray (mm)
                                                    |
                                                    v
                                  ros2_smartneedle_adapter (100 Hz)
                                                    |
                                       /            \ 
                                      v              v
                         /IGTL_POINT_OUT      /IGTL_STRING_OUT
                                      \            /
                                       v          v
                              external ros2_igtl_bridge
                                         |
                                OpenIGTLink TCP :18944
                                         |
                                         v
                          Slicer OpenIGTLinkIF -> SmartNeedle
```

POINT and STRING messages are separate, not atomic. Slicer rendering rate is independent of ROS rates.

## Data and calibration contract

- Current calibration expects 20 incoming sensor values. First FBG = 3 selects values 3..20, yielding 18 measurements.
- Curvature is 1/mm, angle is radians, temperature is Celsius, coordinates and lengths are mm.
- Sensor-specific positions, angle signs/offsets, and needle length come from `ros2_fbg_shape_pipeline_cpp/fbg_shape_pipeline_cpp/config/needle_config.txt`.
- Each curvature is constant over one segment. Boundaries are the base, midpoints between adjacent FBG positions, and full calibrated tip. SE(3) integration returns 19 points (base plus 18 segment ends), spanning 196.391633 mm for the current calibration. No curvature interpolation or uniform resampling is used.
- PoseArray uses mm intentionally, not ROS SI meters. Robot consumers must convert units if needed.
- The adapter targets 100 Hz and pauses after 0.5 s without valid shape input. This is not hard real time.

## Prerequisites

Tested on Ubuntu 22.04 with ROS 2 Humble. Follow the official [Humble Ubuntu installation](https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debs.html). Git, Python 3, CMake, a C++ compiler, colcon, and access to the pinned repositories are needed.

## 1. Clone and initialize dependencies

```bash
mkdir -p ~/Documents
cd ~/Documents
git clone https://github.com/jfcoeur/FBGS-ROS2-pipeline.git
cd FBGS-ROS2-pipeline
bash scripts/init_dependencies.sh
git submodule status
```

Pinned external repositories:
- [ws_smartneedle](https://github.com/SmartNeedle/ws_smartneedle.git)
- [SmartNeedleIGTL-3DSlicer](https://github.com/SmartNeedle/SmartNeedleIGTL-3DSlicer.git)
- [OpenIGTLink](https://github.com/openigtlink/OpenIGTLink.git)

Do not initialize submodules recursively: the collaborator workspace contains two unused legacy gitlinks without URL mappings. If an upstream repository is private, collaborators need access to it too. See [dependency notes](docs/EXTERNAL_DEPENDENCIES.md). External source is pinned and must remain untouched.

## 2. Install dependencies and build

```bash
cd ~/Documents/FBGS-ROS2-pipeline
source /opt/ros/humble/setup.bash
bash ros2_fbg_shape_pipeline_cpp/scripts/install_linux_dependencies.sh --ros-distro humble

cmake -S "external dependencies/OpenIGTLink" -B OpenIGTLink-build \
  -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF
cmake --build OpenIGTLink-build -j"$(nproc)"

bash scripts/build_cpp_ros2_stack.sh \
  --ros-distro humble \
  --openigtlink-dir "$PWD/OpenIGTLink-build"
```

Expect four packages to finish and “Build complete”. For every new terminal that runs the stack, source these paths:

```bash
cd ~/Documents/FBGS-ROS2-pipeline
source /opt/ros/humble/setup.bash
source "$HOME/.cache/fbg_colcon/$(basename "$PWD")_slicer/install/setup.bash"
export LD_LIBRARY_PATH="$PWD/OpenIGTLink-build/bin:$PWD/OpenIGTLink-build/lib:${LD_LIBRARY_PATH:-}"
```

The library path is needed by the external bridge to load OpenIGTLink.

## 3. Configure Slicer

Install 3D Slicer. In its Extension Manager install the extension providing OpenIGTLinkIF (SlicerOpenIGTLink) and CurveMaker. Add the following absolute directory under **Edit -> Application Settings -> Modules -> Additional module paths**, then restart Slicer:

```text
~/Documents/FBGS-ROS2-pipeline/external dependencies/SmartNeedleIGTL-3DSlicer/SmartNeedle
```

Open SmartNeedle, select/create its connector, then in OpenIGTLinkIF configure a Client to 127.0.0.1 port 18944 (when Slicer and ROS run on the same Linux computer). Connect and start the SmartNeedle display; confirm the connector is ON. WAIT before the bridge starts listening is normal.

## 4. Run automated tests (no hardware required)

In a terminal with the ROS/install environment sourced:

```bash
ROS_DOMAIN_ID=99 python3 -m unittest discover -s tests -v
CACHE="$HOME/.cache/fbg_colcon/$(basename "$PWD")_slicer"
colcon test --base-paths ros2_fbg_shape_pipeline_cpp \
  --build-base "$CACHE/build" --install-base "$CACHE/install" --merge-install \
  --packages-select fbg_shape_pipeline_cpp
colcon test-result --test-result-base "$CACHE/build" --verbose
```

The Python integration suite exercises the TCP-to-OpenIGTLink path, source switching, error suppression, stale timeout, and recovery. C++ tests cover packet parsing and SE(3) reconstruction. The verified Linux suite had 7 Python tests and 12 colcon tests pass.

## 5. Run the simulated pipeline

Use three terminals. Keep Slicer open and connected.

**Terminal A: simulator**

```bash
cd ~/Documents/FBGS-ROS2-pipeline
python3 ros2_fbg_shape_pipeline_cpp/tools/mock_fbg_stream_server.py \
  --host 127.0.0.1 --port 50012 --rate-hz 100
```

Expect “Mock stream listening on 127.0.0.1:50012 at 100.0 Hz”. The default simulator reads the needle calibration and generates 20 sensor values. Curvature and angle create controlled motion; spectra/temperature/packet-shape fields are synthetic placeholders. Packet dimensions match one inspected ShapeCore frame, not all real timing jitter or packet-size variation.

**Terminal B: ROS pipeline**

Source the environment from section 2, then run:

```bash
ros2 launch ros2_smartneedle_adapter full_pipeline.launch.py \
  tcp_host:=127.0.0.1 \
  tcp_port:=50012
```

Expect tcp receiver, curvature processor, external igtl bridge, and local adapter nodes to start and a client connection in Terminal A.

**Terminal C: rate and content checks**

Source the environment from section 2, then run each rate check separately:

```bash
timeout 5s ros2 topic hz /needle/fbg_frame
timeout 5s ros2 topic hz /needle/state/curvatures
timeout 5s ros2 topic hz /needle/state/current_shape
timeout 5s ros2 topic hz /IGTL_POINT_OUT
```

Expect near 100 Hz after warmup and a moving needle in Slicer. One-shot samples:

```bash
ros2 topic echo --once /needle/state/curvatures
ros2 topic echo --once /needle/state/current_shape
```

Expect 18 selected curvatures and 19 poses. The first point is the base at zero; the last reaches the calibrated tip (straight case z about 196.392 mm). To measure ROS receipt-to-shape latency, run `python3 ros2_fbg_shape_pipeline_cpp/tools/latency_probe.py`. It does not measure sensor acquisition, IGTL delivery, or Slicer rendering. Slicer may redraw slower than the ROS publish rate. After 0.5 s without valid input, adapter output pauses; Slicer may retain its last geometry.

## 6. Switch between simulation and real data

Run both sources, the ROS launch, and Slicer. Keep ROS nodes and Slicer running. In a sourced terminal:

```bash
# Simulation
ros2 param set /tcp_receiver_node tcp_host 127.0.0.1

# Real interrogator (substitute its actual reachable IP)
ros2 param set /tcp_receiver_node tcp_host <INTERROGATOR_IP>
```

Each update reconnects the receiver without restarting the pipeline. Use port 50012 on each endpoint for this one-parameter switch; tcp_port can also be changed live. Expect a brief gap or geometry discontinuity; the system does not blend sources or automatically fall back.

For one tested direct Ethernet setup, Windows ShapeCore was 10.100.51.10/24 and Linux was 10.100.51.11/24, port 50012. These are examples; verify the current addresses. ShapeCore must be listening and Windows Firewall must allow inbound TCP 50012 from Linux.

```bash
ip -br -4 addr
nc -vz -w 3 <INTERROGATOR_IP> 50012
```

Switch back to simulation with host 127.0.0.1. Stop the simulator using Ctrl+C when finished.

## Troubleshooting

- **No simulator bind:** port 50012 is already in use. Stop the other server, or choose a port and set it on both simulator and receiver.
- **Waiting/reconnecting:** verify endpoint IP/port, server listening state, firewall, and `nc` reachability. A TCP connection alone does not prove valid parsing; look for valid frame logs and increasing line numbers on /needle/fbg_frame.
- **No curvature/shape:** inspect packet lengths, calibration, array sizes, and interrogator error. Nonzero errors intentionally suppress reconstruction.
- **Bridge says libOpenIGTLink.so.3 missing:** source the install and export LD_LIBRARY_PATH as in section 2.
- **Slicer WAIT/no display:** verify a Client connector at 127.0.0.1:18944, bridge process, SmartNeedle module path (must end in SmartNeedle), and that SmartNeedle display is started.
- **Shape remains visible after disconnect:** expected external module behavior; adapter pauses, but does not command the module to hide geometry.

## More detail

[Linux/Slicer protocol](docs/SLICER_ROS2_OPENIGTLINK_PROTOCOL.md) | [Architecture](ros2_fbg_shape_pipeline_cpp/docs/ARCHITECTURE.md) | [External dependencies](docs/EXTERNAL_DEPENDENCIES.md) | [Test protocol](ros2_fbg_shape_pipeline_cpp/docs/TEST_PROTOCOL.md) | [Audit](docs/AUDIT.md) | [Workspace map](docs/WORKSPACE_MAP.md)
