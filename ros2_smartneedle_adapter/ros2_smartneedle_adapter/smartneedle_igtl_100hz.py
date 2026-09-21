from geometry_msgs.msg import Point, PoseArray
import rclpy
from rclpy.node import Node

from ros2_igtl_bridge.msg import PointArray, String


class SmartNeedleIgtl100Hz(Node):
    def __init__(self):
        super().__init__("smart_needle_interface_100hz")
        self.declare_parameter("rate_hz", 100.0)
        self.rate_hz = float(self.get_parameter("rate_hz").value)
        if self.rate_hz <= 0.0:
            raise ValueError("rate_hz must be positive")

        self.latest_shape = None
        self.sequence_id = 0
        self.subscription = self.create_subscription(
            PoseArray,
            "/needle/state/current_shape",
            self.shape_callback,
            10,
        )
        self.publisher_header = self.create_publisher(String, "IGTL_STRING_OUT", 10)
        self.publisher_shape = self.create_publisher(PointArray, "IGTL_POINT_OUT", 10)
        self.timer = self.create_timer(1.0 / self.rate_hz, self.publish_latest)

    def shape_callback(self, msg):
        self.latest_shape = msg

    def publish_latest(self):
        if self.latest_shape is None or not self.latest_shape.poses:
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
        header.data = (
            f"{stamp.sec}.{stamp.nanosec:09d};"
            f"{self.sequence_id};{len(points)};needle"
        )

        shape = PointArray()
        shape.name = "NeedleShape"
        shape.pointdata = points

        self.publisher_header.publish(header)
        self.publisher_shape.publish(shape)
        self.sequence_id += 1


def main(args=None):
    rclpy.init(args=args)
    node = SmartNeedleIgtl100Hz()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
