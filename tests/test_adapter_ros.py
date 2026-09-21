import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
try:
    import rclpy
    from geometry_msgs.msg import Pose, PoseArray
    from ros2_igtl_bridge.msg import PointArray
    HAS_ROS = True
except ImportError:
    HAS_ROS = False


@unittest.skipUnless(HAS_ROS, "requires built ROS 2 workspace")
class AdapterTests(unittest.TestCase):
    def test_stale_cutoff_resume_and_sequence(self):
        path = ROOT / "ros2_smartneedle_adapter/ros2_smartneedle_adapter/smartneedle_igtl_100hz.py"
        spec = importlib.util.spec_from_file_location("adapter_under_test", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        rclpy.init()
        node = module.SmartNeedleIgtl100Hz()
        published = []
        class Publisher:
            def publish(self, value):
                published.append(value)
        node.publisher_header = Publisher()
        node.publisher_shape = Publisher()
        try:
            shape = PoseArray()
            pose = Pose()
            pose.position.z = 196.391633
            shape.poses = [pose]
            with patch.object(module.time, "monotonic", return_value=1.0):
                node.shape_callback(shape)
                node.publish_latest()
            self.assertEqual(len(published), 2)
            self.assertEqual(published[1].pointdata[0].z, pose.position.z)
            original_header = published[0].data
            with patch.object(module.time, "monotonic", return_value=1.2):
                node.publish_latest()
            self.assertEqual(published[2].data, original_header)
            with patch.object(module.time, "monotonic", return_value=1.5):
                node.publish_latest()
            self.assertEqual(len(published), 4)
            self.assertTrue(node.stale_logged)
            with patch.object(module.time, "monotonic", return_value=1.6):
                node.shape_callback(shape)
                node.publish_latest()
            self.assertEqual(len(published), 6)
            self.assertEqual(node.sequence_id, 2)
            self.assertFalse(node.stale_logged)
        finally:
            node.destroy_node()
            rclpy.shutdown()
