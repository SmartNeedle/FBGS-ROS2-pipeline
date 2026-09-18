import rclpy
import os
import numpy as np

from rclpy.node import Node
from geometry_msgs.msg import PoseArray
from geometry_msgs.msg import Pose
from scipy.io import loadmat
from builtin_interfaces.msg import Time
from ament_index_python.packages import get_package_share_directory

class VirtualSmartNeedle(Node):

    def __init__(self):
        super().__init__('virtual_smartneedle')

        #Declare node parameters
        self.declare_parameter('dataset', 'fbg_10') #Dataset file name
        self.declare_parameter('rate_hz', 100.0)

        #Published topics

        #Topics from sensorized needle node
        self.publisher_shape = self.create_publisher(PoseArray, '/needle/state/current_shape', 10)
        rate_hz = float(self.get_parameter('rate_hz').value)
        if rate_hz <= 0.0:
            raise ValueError('rate_hz must be positive')
        timer_period = 1.0 / rate_hz
        self.timer = self.create_timer(timer_period, self.timer_callback)

        #Load data from matlab file
        try:
            path_share_directory = get_package_share_directory('smartneedle_interface')
            file_path  = os.path.join(path_share_directory,'files',self.get_parameter('dataset').get_parameter_value().string_value + '.mat')
            trial_data = loadmat(file_path, mat_dtype=True)
        except IOError:
            self.get_logger().info('Could not find .mat file')

        self.sensor = trial_data['sensor'][0]
        self.time_stamp = trial_data['time_stamp'][0]
        self.i=0
        
    # Publish current needle shape (PoseArray of 3D points)
    def timer_callback(self):
        
        now = self.get_clock().now().to_msg()
    
        msg = PoseArray()
        msg.header.stamp = now
        msg.header.frame_id = 'needle'

        # Populate message with X data from matlab file
        X = self.sensor[self.i]
        for j in range(X.shape[1]):
            pose = Pose()
            # The MAT recording stores coordinates in millimeters; ROS uses meters.
            pose.position.x = float(X[0][j]) / 1000.0
            pose.position.y = float(X[1][j]) / 1000.0
            pose.position.z = float(X[2][j]) / 1000.0
            msg.poses.append(pose)
        self.i = (self.i + 1) % self.sensor.size
        self.publisher_shape.publish(msg)

def main(args=None):
    rclpy.init(args=args)

    virtual_smartneedle = VirtualSmartNeedle()

    rclpy.spin(virtual_smartneedle)

    # Destroy the node explicitly
    # (optional - otherwise it will be done automatically
    # when the garbage collector destroys the node object)
    virtual_smartneedle.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
