#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "geometry_msgs/msg/pose_array.hpp"

#include "fbg_shape_msgs/msg/curvature_frame.hpp"
#include "fbg_shape_pipeline_cpp/shape_reconstruction.hpp"

namespace fbg_shape_pipeline_cpp
{

class ShapePublisherNode : public rclcpp::Node
{
public:
  ShapePublisherNode()
  : Node("shape_publisher_node")
  {
    input_topic_ = declare_parameter<std::string>("input_topic", "/needle/state/curvatures");
    output_topic_ = declare_parameter<std::string>("output_topic", "/needle/state/current_shape");
    frame_id_ = declare_parameter<std::string>("frame_id", "needle");
    needle_length_mm_ = declare_parameter<double>("needle_length_mm", 200.0);
    // The subscription queue keeps the work current; reconstruct as each frame arrives.
    publisher_ = create_publisher<geometry_msgs::msg::PoseArray>(output_topic_, rclcpp::QoS(1));
    subscription_ = create_subscription<fbg_shape_msgs::msg::CurvatureFrame>(
      input_topic_,
      rclcpp::QoS(1),
      std::bind(&ShapePublisherNode::handle_curvature, this, std::placeholders::_1));

    RCLCPP_INFO(
      get_logger(),
      "Shape publisher ready. Subscribing to %s and publishing %s",
      input_topic_.c_str(),
      output_topic_.c_str());
  }

private:
  void handle_curvature(const fbg_shape_msgs::msg::CurvatureFrame::SharedPtr msg)
  {
    auto pose_array = reconstruct_shape(
      msg->arc_lengths, msg->kappa_x, msg->kappa_y, msg->kappa_z,
      needle_length_mm_, frame_id_);
    pose_array.header.stamp = msg->header.stamp;
    publisher_->publish(pose_array);
  }

  std::string input_topic_;
  std::string output_topic_;
  std::string frame_id_;
  double needle_length_mm_ {200.0};
  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr publisher_;
  rclcpp::Subscription<fbg_shape_msgs::msg::CurvatureFrame>::SharedPtr subscription_;
};

}  // namespace fbg_shape_pipeline_cpp

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<fbg_shape_pipeline_cpp::ShapePublisherNode>());
  rclcpp::shutdown();
  return 0;
}
