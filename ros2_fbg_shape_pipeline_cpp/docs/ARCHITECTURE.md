# Architecture

## One processing path

```text
Real interrogator TCP server OR local simulated TCP server
  -> tcp_receiver_node -> /needle/fbg_sensor_frame (sensor-only FbgFrame)
                       -> /needle/fbg_frame (complete FbgFrame for inspection)
  -> curvature_processor_node -> /needle/state/curvatures (CurvatureFrame)
                            -> /needle/state/current_shape (PoseArray, mm)
  -> local ros2_smartneedle_adapter
  -> IGTL_POINT_OUT + IGTL_STRING_OUT
  -> untouched external ros2_igtl_bridge
  -> Slicer OpenIGTLinkIF + untouched SmartNeedle module
```

The simulator replaces only the incoming TCP data. The receiver publishes the
compact sensor fields first so processing can start without deserializing the
large spectra payload. The full raw topic still includes the packet's optional
shape and spectra fields for inspection. Neither field drives reconstruction.
The --full-frame-input launcher option restores processing from the full raw
topic and disables the compact publication for comparison.

## Packet contract

The stream uses a little-endian uint32 payload length followed by a uint8 fiber
index. Each field has an int32 length (including its uint16 ID, excluding the
length itself) and a payload. Fields 0..7 are error uint16, line uint64, source
timestamp double, curvature float array, angle float array, shape matrix,
temperature float array, and spectra blocks. Arrays include a uint32 count.
Shape includes uint32 width and height. Spectra blocks include their own length,
channel, and nested typed fields. A spectra block length excludes its four-byte
length prefix, as verified with a real ShapeCore capture. The receiver validates framing and lengths.
Packet size is bounded at 64 MiB. The supported Linux targets are little-endian.

The layout was checked against a locally supplied ShapeCore packet. The private
capture is not distributed. Regression tests use synthetic zero-valued spectra
with the same length convention; compatibility with every vendor version is not established.

## Calibration and reconstruction

Current total length: 196.391633064447 mm.
Selected measurements: 18 positions from 11.592254970012 through
181.592254970012 mm, spaced by 10 mm; First FBG = 3 selects values 3..20.
Curvature stays in 1/mm. Gains are unity. Angles use calibrated signs/offsets.
kx = curvature*cos(angle), ky = curvature*sin(angle), kz = 0.

Each selected curvature represents one constant-curvature longitudinal segment.
For N measurement positions p, boundaries are b[0]=0,
b[i]=(p[i-1]+p[i])/2 for i=1..N-1, and b[N]=total length.
The measurements need not be exact geometric centers of the end segments.
Assign kappa[i] to [b[i], b[i+1]] and integrate ds=b[i+1]-b[i].
g initially equals identity; r[0]=(0,0,0) at the physical needle base.
For each segment, g = g * exp(ds * [skew(kappa[i]), e3; 0,0]).
This preserves the MATLAB SE(3) update, with explicitly constructed segment
boundaries replacing measurement positions. The closed-form exponential uses
quaternion composition and its matching closed-form translation; a small-angle
series avoids cancellation near zero. Output is N+1 boundary points:
19 for this sensor, including base and tip. No curvature interpolation,
1 mm sampling, or integration substeps are used.

The first segment is 16.592254970012 mm, the 16 interior segments are 10 mm,
and the last is 19.799378094435 mm. Their sum is the full calibrated length,
196.391633064447 mm. A single measurement covers the whole length.
Using measurements over the unmeasured end portions is a modeling assumption.
The untouched Slicer CurveMaker module may smooth its visual tube between
received points; it does not alter the ROS reconstruction or transmitted points.

## Timing and failures

Internal ROS queues have depth one. By default the curvature processor
reconstructs and publishes the shape in the same FbgFrame callback that
publishes CurvatureFrame. This removes the curvature-to-shape DDS handoff while
keeping both topic contracts. The separate shape node remains available through
the --separate-shape launcher option for comparison. DDS may replace queued
raw frames if the curvature callback cannot keep pace with input.
The simulator uses monotonic deadlines targeting 100 Hz. Curvature magnitude
is 0.002..0.003 1/mm (2..3 1/m); temperature is a Celsius placeholder and angle
is radians. Target publishing rates are not hard real-time guarantees.
Its synthetic 20-value packet matches one observed ShapeCore frame's 15,767-byte
wire size and array dimensions. This does not emulate network jitter or packet
size variation.

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
