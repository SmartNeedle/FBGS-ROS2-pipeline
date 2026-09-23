# Shape callback timing

After rebuilding and restarting the default fused stack, enable diagnostics with:

```bash
ros2 param set /tcp_receiver_node timing_diagnostics true
ros2 param set /curvature_processor_node timing_diagnostics true
```

Within four seconds the pipeline terminal reports two-second windows of raw
frames published by the TCP receiver, raw frames received by the curvature
processor, curvature frames published, and fused shapes published. It
also reports the mean/maximum time spent reconstructing and calling shape
publish. Curvature and shape publication happen in the same callback, once
per valid frame. A separate listener may still observe fewer samples.
The report includes the first and last interrogator line numbers published;
line numbers can skip before ROS receives the raw frame.
The TCP receiver also reports its last published line number. The node timers
are not synchronized; compare several windows rather than one pair of counts.
If TCP publication is near 100 Hz while curvature receipt is lower, the raw
ROS handoff is losing samples. If TCP publication is already below 100 Hz,
investigate TCP input and parsing before changing the ROS shape path.
With the default compact input, the TCP counter counts complete raw messages
after both compact and full publication; the processor counts compact messages.
The two publications come from the same parsed frame and share its timestamp.

For the older separate-node path, launch with --separate-shape and enable
diagnostics on both nodes:

```bash
ros2 param set /curvature_processor_node timing_diagnostics true
ros2 param set /shape_publisher_node timing_diagnostics true
```

Durations use a monotonic clock. Fused shape timing covers reconstruction and
the ROS shape publish call; it does not measure subscriber delivery,
OpenIGTLink transmission, or Slicer rendering. Callback intervals also include
scheduling effects; these measurements are not CPU-only profiling.

First collect about 20 seconds with other topic monitors stopped, then compare
against the lightweight latency probe if needed. Diagnostics add three clock
reads per frame and log messages per reporting window. They default to off.
Disable them at runtime with:

```bash
ros2 param set /curvature_processor_node timing_diagnostics false
ros2 param set /tcp_receiver_node timing_diagnostics false
```

Parameter changes take effect at the next reporting tick (up to two seconds).
An idle input reports zero callbacks; zero durations then mean no samples.

The local Slicer bridge launch routes the external bridge's verbose stdout to
the ROS launch log directory, printed at startup, instead of the terminal.
This does not modify the external bridge; inspect that log if connection details
are needed. Compare rates with the same source and Slicer connection after this
change to see whether terminal output affected scheduling.
