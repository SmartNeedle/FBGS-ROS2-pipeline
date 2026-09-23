# Shape callback timing

After rebuilding and restarting the stack, enable diagnostics with:

```bash
ros2 param set /shape_publisher_node timing_diagnostics true
ros2 param set /curvature_processor_node timing_diagnostics true
```

Within four seconds the pipeline terminal starts reporting non-overlapping
two-second windows of completed callbacks, callback rate, and mean/maximum
reconstruction and ROS publish-call durations in milliseconds. Each completed
callback processes one received curvature message and calls publish once.
The count cannot identify messages discarded before the callback runs.
The curvature processor separately reports received raw frames, published
curvature frames, and its publication rate. Both nodes report the first and last
interrogator line numbers observed in each window. Compare multiple windows;
their two-second timers are not synchronized. Line numbers can also skip before
ROS receives the raw frame, so a single discontinuity does not prove where a
frame was lost.

Durations use a monotonic clock. Reconstruction timing includes assigning the
output header stamp. Publish-call timing does not measure subscriber delivery,
OpenIGTLink transmission, or Slicer rendering. Callback intervals also include
scheduling effects; these measurements are not CPU-only profiling.

First collect about 20 seconds with other topic monitors stopped, then compare
against the lightweight latency probe if needed. Diagnostics add three clock
reads per frame and one log message per reporting window. They default to off.
Disable them at runtime with:

```bash
ros2 param set /shape_publisher_node timing_diagnostics false
ros2 param set /curvature_processor_node timing_diagnostics false
```

Parameter changes take effect at the next reporting tick (up to two seconds).
An idle input reports zero callbacks; zero durations then mean no samples.
