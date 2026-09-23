#!/usr/bin/env python3
"""Bounded receipt-to-shape latency diagnostics; source clock is not guessed."""
from collections import deque
import statistics
import time

import rclpy
from geometry_msgs.msg import PoseArray
from rclpy.node import Node
from fbg_shape_msgs.msg import FbgFrame


class LatencyProbe(Node):
    def __init__(self):
        super().__init__("latency_probe")
        self.receipt_to_shape = deque(maxlen=2000)
        self.frame_times = deque(maxlen=2000)
        self.shape_times = deque(maxlen=2000)
        self.declare_parameter("source_unix_seconds", False)
        self.source_latency = deque(maxlen=2000)
        self.create_subscription(FbgFrame, "/needle/fbg_frame", self.handle_frame, 1)
        self.create_subscription(PoseArray, "/needle/state/current_shape", self.handle_shape, 1)
        self.create_timer(2.0, self.report)

    def handle_frame(self, msg):
        self.frame_times.append(time.monotonic())
        if self.get_parameter("source_unix_seconds").value:
            self.source_latency.append((time.time() - msg.source_timestamp) * 1000.0)

    def handle_shape(self, msg):
        self.shape_times.append(time.monotonic())
        stamp = msg.header.stamp.sec * 1_000_000_000 + msg.header.stamp.nanosec
        self.receipt_to_shape.append((self.get_clock().now().nanoseconds - stamp) / 1e6)

    def report(self):
        def rate(values):
            if len(values) < 2 or time.monotonic() - values[-1] > 0.5:
                return 0.0
            return (len(values) - 1) / (values[-1] - values[0])

        def stats(values):
            if not values:
                return "no samples"
            ordered = sorted(values)
            return (f"n={len(ordered)} median={statistics.median(ordered):.2f} "
                    f"p95={ordered[max(0, int(0.95*len(ordered))-1)]:.2f} "
                    f"max={max(ordered):.2f} ms")

        self.get_logger().info(
            f"Window: input={rate(self.frame_times):.1f} Hz shape={rate(self.shape_times):.1f} Hz; "
            f"ROS receipt -> shape subscriber: {stats(self.receipt_to_shape)}"
        )
        if self.get_parameter("source_unix_seconds").value:
            self.get_logger().info(f"Source -> subscriber (requires synchronized Unix clocks): {stats(self.source_latency)}")


def main():
    rclpy.init()
    node = LatencyProbe()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    except RuntimeError:
        if rclpy.ok():
            raise
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
