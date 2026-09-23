import datetime
import math
import time

from geometry_msgs.msg import Point, PoseArray
import rclpy
from rclpy.node import Node

from ros2_igtl_bridge.msg import PointArray, String


class SmartNeedleIgtl100Hz(Node):
    def __init__(self):
        super().__init__("smart_needle_interface_100hz")
        self.declare_parameter("rate_hz", 100.0)
        self.declare_parameter("stale_timeout_sec", 0.5)
        self.declare_parameter("input_topic", "/needle/state/current_shape")
        self.stale_timeout = float(self.get_parameter("stale_timeout_sec").value)
        self.rate_hz = float(self.get_parameter("rate_hz").value)
        if not math.isfinite(self.rate_hz) or self.rate_hz <= 0.0:
            raise ValueError("rate_hz must be finite and positive")
        if not math.isfinite(self.stale_timeout) or self.stale_timeout <= 0.0:
            raise ValueError("stale_timeout_sec must be finite and positive")

        self.latest_shape = None
        self.sequence_id = 0
        self.last_received = None
        self.stale_logged = False
        self.subscription = self.create_subscription(
            PoseArray,
            self.get_parameter("input_topic").value,
            self.shape_callback,
            1,
        )
        self.publisher_header = self.create_publisher(String, "IGTL_STRING_OUT", 10)
        self.publisher_shape = self.create_publisher(PointArray, "IGTL_POINT_OUT", 10)
        self.timer = self.create_timer(1.0 / self.rate_hz, self.publish_latest)

    def shape_callback(self, msg):
        if not msg.poses or any(
            not math.isfinite(value)
            for pose in msg.poses
            for value in (pose.position.x, pose.position.y, pose.position.z)
        ):
            return
        self.latest_shape = msg
        self.sequence_id += 1
        self.last_received = time.monotonic()
        if self.stale_logged:
            self.get_logger().info("Shape input resumed")
        self.stale_logged = False

    def publish_latest(self):
        if self.latest_shape is None or not self.latest_shape.poses:
            return
        if time.monotonic() - self.last_received >= self.stale_timeout:
            if not self.stale_logged:
                self.get_logger().warning("Shape input stale; OpenIGTLink publication paused")
                self.stale_logged = True
            return

        poses = self.latest_shape.poses
        points = []
        for pose in poses:
            point = Point()
            point.x = pose.position.x
            point.y = pose.position.y
            point.z = pose.position.z
            points.append(point)

        header = String()
        header.name = "NeedleShapeHeader"
        stamp = self.latest_shape.header.stamp
        timestamp = datetime.datetime.fromtimestamp(stamp.sec).strftime("%Y-%m-%d %H:%M:%S")
        header.data = (
            f"{timestamp}.{stamp.nanosec // 1_000_000:03d};"
            f"{self.sequence_id};{len(points)};needle"
        )

        shape = PointArray()
        shape.name = "NeedleShape"
        shape.pointdata = points

        self.publisher_header.publish(header)
        self.publisher_shape.publish(shape)


def main(args=None):
    rclpy.init(args=args)
    node = SmartNeedleIgtl100Hz()
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
