#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include "builtin_interfaces/msg/time.hpp"
#include "geometry_msgs/msg/pose_array.hpp"
#include "geometry_msgs/msg/point.hpp"
#include "rclcpp/rclcpp.hpp"
#include "ros2_igtl_bridge/msg/point_array.hpp"
#include "ros2_igtl_bridge/msg/string.hpp"

namespace
{

std::string format_timestamp(const builtin_interfaces::msg::Time & stamp)
{
  std::ostringstream stream;
  stream << stamp.sec << "." << std::setw(9) << std::setfill('0') << stamp.nanosec;
  return stream.str();
}

}  // namespace

class NeedleShapeToIgtlNode : public rclcpp::Node
{
public:
  NeedleShapeToIgtlNode()
  : Node("needle_shape_to_igtl_node")
  {
    input_topic_ = declare_parameter<std::string>("input_topic", "/needle/state/current_shape");
    point_output_topic_ = declare_parameter<std::string>("point_output_topic", "IGTL_POINT_OUT");
    string_output_topic_ = declare_parameter<std::string>("string_output_topic", "IGTL_STRING_OUT");
    point_device_name_ = declare_parameter<std::string>("point_device_name", "NeedleShape");
    header_device_name_ = declare_parameter<std::string>("header_device_name", "NeedleShapeHeader");
    output_frame_id_ = declare_parameter<std::string>("output_frame_id", "");
    point_scale_ = declare_parameter<double>("point_scale", 1000.0);
    reverse_point_order_ = declare_parameter<bool>("reverse_point_order", false);
    invert_x_ = declare_parameter<bool>("invert_x", false);
    invert_y_ = declare_parameter<bool>("invert_y", false);
    invert_z_ = declare_parameter<bool>("invert_z", false);

    point_publisher_ =
      create_publisher<ros2_igtl_bridge::msg::PointArray>(point_output_topic_, rclcpp::QoS(10));
    string_publisher_ =
      create_publisher<ros2_igtl_bridge::msg::String>(string_output_topic_, rclcpp::QoS(10));

    pose_subscription_ = create_subscription<geometry_msgs::msg::PoseArray>(
      input_topic_,
      rclcpp::QoS(10),
      std::bind(&NeedleShapeToIgtlNode::handle_pose_array, this, std::placeholders::_1));

    RCLCPP_INFO(
      get_logger(),
      "Needle shape adapter ready. input=%s point_topic=%s string_topic=%s scale=%0.3f",
      input_topic_.c_str(),
      point_output_topic_.c_str(),
      string_output_topic_.c_str(),
      point_scale_);
  }

private:
  geometry_msgs::msg::Point convert_point(const geometry_msgs::msg::Point & input) const
  {
    geometry_msgs::msg::Point output;
    output.x = input.x * point_scale_ * (invert_x_ ? -1.0 : 1.0);
    output.y = input.y * point_scale_ * (invert_y_ ? -1.0 : 1.0);
    output.z = input.z * point_scale_ * (invert_z_ ? -1.0 : 1.0);
    return output;
  }

  void handle_pose_array(const geometry_msgs::msg::PoseArray::SharedPtr msg)
  {
    if (msg->poses.empty()) {
      RCLCPP_WARN_THROTTLE(
        get_logger(),
        *get_clock(),
        5000,
        "Received empty PoseArray on %s; skipping bridge publish.",
        input_topic_.c_str());
      return;
    }

    ros2_igtl_bridge::msg::PointArray point_msg;
    point_msg.name = point_device_name_;
    point_msg.pointdata.reserve(msg->poses.size());

    if (reverse_point_order_) {
      for (auto it = msg->poses.rbegin(); it != msg->poses.rend(); ++it) {
        point_msg.pointdata.push_back(convert_point(it->position));
      }
    } else {
      for (const auto & pose : msg->poses) {
        point_msg.pointdata.push_back(convert_point(pose.position));
      }
    }

    ros2_igtl_bridge::msg::String header_msg;
    header_msg.name = header_device_name_;
    const std::string frame_id =
      output_frame_id_.empty() ? msg->header.frame_id : output_frame_id_;
    header_msg.data =
      format_timestamp(msg->header.stamp) + ";" + std::to_string(sequence_id_++) + ";" +
      std::to_string(point_msg.pointdata.size()) + ";" + frame_id;

    string_publisher_->publish(header_msg);
    point_publisher_->publish(point_msg);
  }

  std::string input_topic_;
  std::string point_output_topic_;
  std::string string_output_topic_;
  std::string point_device_name_;
  std::string header_device_name_;
  std::string output_frame_id_;
  double point_scale_ {1000.0};
  bool reverse_point_order_ {false};
  bool invert_x_ {false};
  bool invert_y_ {false};
  bool invert_z_ {false};
  std::size_t sequence_id_ {0};

  rclcpp::Publisher<ros2_igtl_bridge::msg::PointArray>::SharedPtr point_publisher_;
  rclcpp::Publisher<ros2_igtl_bridge::msg::String>::SharedPtr string_publisher_;
  rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr pose_subscription_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<NeedleShapeToIgtlNode>());
  rclcpp::shutdown();
  return 0;
}
