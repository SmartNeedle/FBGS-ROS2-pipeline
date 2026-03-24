#!/usr/bin/env python3
from collections import deque
import statistics
import time

import rclpy
from geometry_msgs.msg import PoseArray
from rclpy.node import Node

from fbg_shape_msgs.msg import CurvatureFrame, FbgFrame


class LatencyProbe(Node):
    def __init__(self):
        super().__init__("latency_probe")
        self.frame_source_latencies_ms = []
        self.frame_source_relative_latencies_ms = []
        self.shape_receive_latencies_ms = []
        self.frame_source_window_ms = deque(maxlen=2000)
        self.frame_source_relative_window_ms = deque(maxlen=2000)
        self.shape_receive_window_ms = deque(maxlen=2000)
        self.unsynced_source_timestamps = 0
        self.source_scale_ms = None
        self.prev_source_raw = None
        self.prev_now_ms = None

        self.create_subscription(FbgFrame, "/needle/fbg_frame", self.handle_frame, 10)
        self.create_subscription(CurvatureFrame, "/needle/state/curvatures", self.handle_curvature, 10)
        self.create_subscription(PoseArray, "/needle/state/current_shape", self.handle_shape, 10)
        self.create_timer(2.0, self.report)

    def handle_frame(self, msg: FbgFrame):
        now_ms = time.time() * 1000.0
        source_raw = float(msg.source_timestamp)

        candidates = {
            "seconds": source_raw * 1000.0,
            "milliseconds": source_raw,
            "microseconds": source_raw / 1000.0,
            "nanoseconds": source_raw / 1_000_000.0,
        }
        source_ms = min(candidates.values(), key=lambda v: abs(now_ms - v))
        latency_ms = now_ms - source_ms

        # Relative source latency is still useful when source timestamps come
        # from a different clock epoch, as long as source increments are stable.
        if self.prev_source_raw is not None and self.prev_now_ms is not None:
            delta_source_raw = source_raw - self.prev_source_raw
            delta_now_ms = now_ms - self.prev_now_ms
            scale_candidates = {
                "seconds": 1000.0,
                "milliseconds": 1.0,
                "microseconds": 0.001,
                "nanoseconds": 1e-6,
            }
            best_scale = min(
                scale_candidates.values(),
                key=lambda s: abs(delta_source_raw * s - delta_now_ms),
            )
            self.source_scale_ms = best_scale
            relative_latency_ms = delta_now_ms - (delta_source_raw * self.source_scale_ms)
            self.frame_source_relative_latencies_ms.append(relative_latency_ms)
            self.frame_source_relative_window_ms.append(relative_latency_ms)

        # If no interpreted unit is even remotely close to wall-clock time,
        # source timestamp likely comes from another unsynchronized clock domain.
        if abs(latency_ms) > 60_000.0:
            self.unsynced_source_timestamps += 1
        else:
            self.frame_source_latencies_ms.append(latency_ms)
            self.frame_source_window_ms.append(latency_ms)

        self.prev_source_raw = source_raw
        self.prev_now_ms = now_ms

    def handle_curvature(self, msg: CurvatureFrame):
        _ = msg

    def handle_shape(self, msg: PoseArray):
        shape_stamp_ns = msg.header.stamp.sec * 1_000_000_000 + msg.header.stamp.nanosec
        latency_ms = (time.time_ns() - shape_stamp_ns) / 1e6
        self.shape_receive_latencies_ms.append(latency_ms)
        self.shape_receive_window_ms.append(latency_ms)

    def report(self):
        def summarize(values):
            if not values:
                return "no samples yet"
            ordered = sorted(values)
            median = statistics.median(ordered)
            p95 = ordered[max(0, int(len(ordered) * 0.95) - 1)]
            return f"n={len(ordered)} median={median:.2f} ms p95={p95:.2f} ms max={max(ordered):.2f} ms"

        source_text = summarize(self.frame_source_latencies_ms)
        if source_text == "no samples yet" and self.frame_source_relative_latencies_ms:
            source_text = (
                "absolute=no samples yet (unsynced clock domain) | "
                f"relative={summarize(self.frame_source_relative_latencies_ms)}"
            )
            if self.frame_source_relative_window_ms:
                source_text += (
                    f" | relative_window_max(2000)={max(self.frame_source_relative_window_ms):.2f} ms"
                )
        if self.unsynced_source_timestamps > 0:
            source_text += f" | skipped_unsynced={self.unsynced_source_timestamps}"

        shape_text = summarize(self.shape_receive_latencies_ms)
        if self.shape_receive_window_ms:
            shape_text += (
                f" | window_max(2000)={max(self.shape_receive_window_ms):.2f} ms"
            )

        self.get_logger().info(
            "Source->receiver latency: %s | Shape receive latency: %s"
            % (source_text, shape_text)
        )


def main():
    rclpy.init()
    node = LatencyProbe()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
