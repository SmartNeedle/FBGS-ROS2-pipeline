# Architecture

## One processing path

```text
Real interrogator TCP server OR local simulated TCP server
  -> tcp_receiver_node -> /needle/fbg_frame (FbgFrame)
  -> curvature_processor_node -> /needle/state/curvatures (CurvatureFrame)
  -> shape_publisher_node -> /needle/state/current_shape (PoseArray, mm)
  -> local ros2_smartneedle_adapter
  -> IGTL_POINT_OUT + IGTL_STRING_OUT
  -> untouched external ros2_igtl_bridge
  -> Slicer OpenIGTLinkIF + untouched SmartNeedle module
```

The simulator replaces only the incoming TCP data. The packet's optional shape
and spectra fields are placeholders, preserved for inspection but never used
to reconstruct the displayed shape.

## Packet contract

The stream uses a little-endian uint32 payload length followed by a uint8 fiber
index. Each field has an int32 length (including its uint16 ID, excluding the
length itself) and a payload. Fields 0..7 are error uint16, line uint64, source
timestamp double, curvature float array, angle float array, shape matrix,
temperature float array, and spectra blocks. Arrays include a uint32 count.
Shape includes uint32 width and height. Spectra blocks include their own length,
channel, and nested typed fields. The receiver validates framing and lengths.
Packet size is bounded at 64 MiB. The supported Linux targets are little-endian.

The available implementation defines the packet layout; no separate vendor
protocol or captured hardware fixture is present in the active repository.
Simulator/parser agreement alone cannot prove compatibility with every vendor version.

## Calibration and reconstruction

Current total length: 196.391633064447 mm.
Selected measurements: 18 positions from 11.592254970012 through
181.592254970012 mm, spaced by 10 mm; First FBG = 3 selects values 3..20.
Curvature stays in 1/mm. Gains are unity. Angles use calibrated signs/offsets.
kx = curvature*cos(angle), ky = curvature*sin(angle), kz = 0.

s_vals consists of the selected positions followed by total length.
g initially equals identity; r[0] = (0,0,0) at the first selected FBG.
For interval i, g = g * exp(ds * [skew(kappa[i-1]), e3; 0,0]).
The closed-form SE(3) exponential implements the supplied MATLAB algorithm,
with a small-angle series to avoid division by zero. There is one output per
s_vals entry: 19 points for this sensor. No prepended physical-base segment,
interpolation, 1 mm sampling, or integration substeps are used.

The represented arc length is 184.799378094435 mm (total length minus first FBG).
That is intentional: the coordinate origin is the first FBG, as in MATLAB.
The untouched Slicer CurveMaker module may smooth its visual tube between
received points; it does not alter the ROS reconstruction or transmitted points.

## Timing and failures

Internal ROS queues have depth one; the shape node stores only the newest
unprocessed frame and checks it on a 1 ms timer. Overload can drop old samples.
The simulator uses monotonic deadlines targeting 100 Hz. Curvature magnitude
is 0.002..0.003 1/mm (2..3 1/m); temperature is a Celsius placeholder and angle
is radians. Target publishing rates are not hard real-time guarantees.

The adapter samples the latest shape at 100 Hz. Repeated outputs preserve the
same input sequence number and receipt timestamp. At 0.5 s without a valid
shape it stops output and logs once, then logs recovery. Slicer may retain its
last displayed geometry; no automatic hiding is implemented in external code.
Nonzero interrogator error fields suppress reconstruction.

The receiver uses interruptible asynchronous I/O in a dedicated thread.
Changing tcp_host or tcp_port reconnects to the selected source while all ROS
nodes stay alive. Existing downstream samples may finish during the switch;
new samples take over without smoothing or blending.

Header strings retain the collaborator format:
YYYY-MM-DD HH:MM:SS.mmm;input_sequence;point_count;needle.
Source timestamps are retained in FbgFrame/CurvatureFrame; PoseArray uses ROS
receipt time. POINT and STRING are separate ROS messages and are not atomic.
Slicer frame rate and synchronized UI updates require target-machine testing.
