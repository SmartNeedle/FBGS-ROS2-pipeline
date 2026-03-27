from geometry_msgs.msg import Point, PoseArray
import rclpy
from rclpy.node import Node

from ros2_igtl_bridge.msg import PointArray, String


class SmartNeedleInterface(Node):
    def __init__(self):
        super().__init__("smart_needle_interface")

        self.input_topic = self.declare_parameter("input_topic", "/needle/state/current_shape").value
        self.point_output_topic = self.declare_parameter("point_output_topic", "IGTL_POINT_OUT").value
        self.string_output_topic = self.declare_parameter("string_output_topic", "IGTL_STRING_OUT").value
        self.point_device_name = self.declare_parameter("point_device_name", "NeedleShape").value
        self.header_device_name = self.declare_parameter("header_device_name", "NeedleShapeHeader").value
        self.output_frame_id = self.declare_parameter("output_frame_id", "").value
        self.point_scale = float(self.declare_parameter("point_scale", 1000.0).value)
        self.reverse_point_order = bool(self.declare_parameter("reverse_point_order", False).value)
        self.invert_x = bool(self.declare_parameter("invert_x", False).value)
        self.invert_y = bool(self.declare_parameter("invert_y", False).value)
        self.invert_z = bool(self.declare_parameter("invert_z", False).value)
        self.sequence_id = 0

        self.publisher_shape = self.create_publisher(PointArray, self.point_output_topic, 10)
        self.publisher_shapeheader = self.create_publisher(String, self.string_output_topic, 10)
        self.subscription_sensor = self.create_subscription(PoseArray, self.input_topic, self.shape_callback, 10)

        self.get_logger().info(
            "SmartNeedle interface ready. input=%s point_topic=%s string_topic=%s scale=%.3f"
            % (self.input_topic, self.point_output_topic, self.string_output_topic, self.point_scale)
        )

    def _convert_point(self, input_point):
        point = Point()
        point.x = input_point.x * self.point_scale * (-1.0 if self.invert_x else 1.0)
        point.y = input_point.y * self.point_scale * (-1.0 if self.invert_y else 1.0)
        point.z = input_point.z * self.point_scale * (-1.0 if self.invert_z else 1.0)
        return point

    @staticmethod
    def _format_timestamp(stamp):
        return f"{stamp.sec}.{stamp.nanosec:09d}"

    def shape_callback(self, msg_sensor):
        if not msg_sensor.poses:
            self.get_logger().warning(
                "Received empty PoseArray on %s; skipping OpenIGTLink publish." % self.input_topic
            )
            return

        pointarray_msg = PointArray()
        pointarray_msg.name = self.point_device_name

        poses = list(msg_sensor.poses)
        if self.reverse_point_order:
            poses.reverse()

        pointarray_msg.pointdata = [self._convert_point(pose.position) for pose in poses]

        frame_id = self.output_frame_id or msg_sensor.header.frame_id
        string_msg = String()
        string_msg.name = self.header_device_name
        string_msg.data = (
            f"{self._format_timestamp(msg_sensor.header.stamp)};"
            f"{self.sequence_id};"
            f"{len(pointarray_msg.pointdata)};"
            f"{frame_id}"
        )
        self.sequence_id += 1

        self.publisher_shape.publish(pointarray_msg)
        self.publisher_shapeheader.publish(string_msg)


def main(args=None):
    rclpy.init(args=args)
    smart_needle_interface = SmartNeedleInterface()
    rclpy.spin(smart_needle_interface)
    smart_needle_interface.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
