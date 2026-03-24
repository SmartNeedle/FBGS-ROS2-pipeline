# Architecture

## 1. Design goals

The system should:

- connect from Linux to the existing TCP stream exposed by the Windows machine,
- minimize end-to-end latency,
- keep acquisition, processing, and ROS 2 publication logically separate,
- allow each stage to be tested independently,
- preserve timestamps so latency can be measured, and
- make it easy to replace the reference processing/model with your final calibrated algorithm.

## 2. Recommended deployment

### Windows machine

- Runs the interrogator software.
- Exposes the existing TCP stream at approximately 100 Hz.
- Does not need to run ROS 2.

### Linux ROS 2 machine

Runs a single ROS 2 launch file containing three C++ nodes:

1. `tcp_receiver_node`
2. `curvature_processor_node`
3. `shape_publisher_node`

These nodes should ideally run:

- in one process using ROS 2 components later, or
- in one launch file with intra-process communication enabled.

For the first implementation, separate executables are easier to understand and debug. Once stable, you can convert them into composable nodes for lower copy overhead.

## 3. Data flow

```text
Windows interrogator
    -> TCP binary stream
Linux tcp_receiver_node
    -> /needle/fbg_frame
Linux curvature_processor_node
    -> /needle/state/curvatures
Linux shape_publisher_node
    -> /needle/state/current_shape
```

### Why this split?

This follows the same high-level pattern as Dimitri's package:

- one stage for acquisition and sensor-side handling,
- one stage for processed curvature/state,
- one stage for final shape publication.

That separation makes the system much easier to test, explain, and replace in parts.

## 4. Low-latency design choices

### TCP connection from Linux directly to Windows

This avoids an unnecessary relay process. The Linux ROS 2 machine reads the interrogator stream directly.

### Dedicated blocking read thread

The receiver node uses a dedicated socket thread with exact-length reads:

- read 4 bytes for packet length,
- read the full payload,
- parse immediately,
- publish immediately.

A dedicated blocking thread is simple and reliable. It also matches the packet logic in your current Windows client.

### Minimal processing in the acquisition node

The receiver node should only:

- receive bytes,
- parse the packet,
- attach a ROS receive timestamp,
- publish a parsed message.

Heavy computation should not happen in the socket callback path.

### Small ROS 2 queues

For low latency, use:

- queue depth 1 or 5,
- best effort for visualization topics when acceptable,
- reliable only where packet loss is unacceptable.

For the reference implementation:

- internal pipeline topics use small queues,
- final shape uses a small queue because only the newest shape is useful.

### Avoid large history windows

Dimitri's older architecture collected many signals before updating curvature. That improves robustness but increases latency. For your current requirement, prefer:

- passthrough or very light filtering first,
- then add larger windows only if accuracy demands it.

## 5. Node responsibilities

## `tcp_receiver_node`

Responsibilities:

- maintain TCP connection,
- reconnect automatically,
- parse the existing packet structure,
- publish `fbg_shape_msgs/msg/FbgFrame`.

Published topic:

- `/needle/fbg_frame`

Message includes:

- ROS receipt time,
- interrogator source timestamp,
- line number,
- fiber index,
- curvature,
- angle,
- temperature,
- optional shape vector if the packet already contains one.

## `curvature_processor_node`

Responsibilities:

- subscribe to `/needle/fbg_frame`,
- optionally smooth or calibrate curvature values,
- republish a clean curvature message.

Published topic:

- `/needle/state/curvatures`

This node is the right place for:

- gain/offset correction,
- temperature compensation,
- active-area weighting,
- outlier rejection,
- future calibration model insertion.

## `shape_publisher_node`

Responsibilities:

- subscribe to `/needle/state/curvatures`,
- reconstruct the 3D needle shape,
- publish `geometry_msgs/msg/PoseArray`.

Published topic:

- `/needle/state/current_shape`

In the reference package, this uses a simple constant-curvature baseline. In your final system, this is where you should insert the validated needle model.

## 6. Why define custom ROS 2 messages?

Using only `Float64MultiArray` is tempting, but it has three drawbacks:

1. no header timestamp,
2. poor self-documentation,
3. harder latency measurement.

Custom messages make the pipeline much easier to debug and explain.

## 7. Future optimization path

Once the pipeline works end to end:

1. Convert the three nodes into components.
2. Use an intra-process container.
3. Pre-allocate vectors where practical.
4. Replace the baseline shape model with the calibrated algorithm.
5. If needed, fuse processing and shape publication into one node after benchmarking.

## 8. Practical recommendation

Start with the exact three-node architecture in this folder. It is the best balance of:

- low latency,
- clarity,
- debuggability,
- and ease of explanation.

After it is validated, optimize only the stages that actually dominate latency.
