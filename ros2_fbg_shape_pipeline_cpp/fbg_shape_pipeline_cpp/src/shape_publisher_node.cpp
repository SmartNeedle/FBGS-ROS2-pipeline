#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "geometry_msgs/msg/pose_array.hpp"

#include "fbg_shape_msgs/msg/curvature_frame.hpp"
#include "fbg_shape_pipeline_cpp/frame_types.hpp"
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
    worker_period_ms_ = declare_parameter<int>("worker_period_ms", 1);

    // Keep only the newest sample in the DDS queue and in the local queue.
    publisher_ = create_publisher<geometry_msgs::msg::PoseArray>(output_topic_, rclcpp::QoS(1));
    subscription_ = create_subscription<fbg_shape_msgs::msg::CurvatureFrame>(
      input_topic_,
      rclcpp::QoS(1),
      std::bind(&ShapePublisherNode::handle_curvature, this, std::placeholders::_1));
    timer_ = create_wall_timer(
      std::chrono::milliseconds(std::max(1, worker_period_ms_)),
      std::bind(&ShapePublisherNode::process_latest_frame, this));

    RCLCPP_INFO(
      get_logger(),
      "Shape publisher ready. Subscribing to %s and publishing %s",
      input_topic_.c_str(),
      output_topic_.c_str());
  }

private:
  void handle_curvature(const fbg_shape_msgs::msg::CurvatureFrame::SharedPtr msg)
  {
    std::scoped_lock lock(latest_mutex_);
    latest_msg_ = *msg;
  }

  void process_latest_frame()
  {
    std::optional<fbg_shape_msgs::msg::CurvatureFrame> local_msg;
    {
      std::scoped_lock lock(latest_mutex_);
      if (!latest_msg_.has_value()) {
        return;
      }
      local_msg = std::move(latest_msg_);
      latest_msg_.reset();
    }

    CurvatureFrameData frame;
    frame.line_number = local_msg->line_number;
    frame.source_timestamp = local_msg->source_timestamp;
    frame.arc_lengths = local_msg->arc_lengths;
    frame.curvature = local_msg->curvature;
    frame.angle = local_msg->angle;
    frame.kappa_x = local_msg->kappa_x;
    frame.kappa_y = local_msg->kappa_y;
    frame.kappa_z = local_msg->kappa_z;
    frame.temperature = local_msg->temperature;

    auto pose_array = reconstruct_shape(
      frame, needle_length_mm_, frame_id_);
    pose_array.header.stamp = local_msg->header.stamp;
    publisher_->publish(pose_array);
  }

  std::string input_topic_;
  std::string output_topic_;
  std::string frame_id_;
  double needle_length_mm_ {200.0};
  int worker_period_ms_ {1};

  std::mutex latest_mutex_;
  std::optional<fbg_shape_msgs::msg::CurvatureFrame> latest_msg_;

  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr publisher_;
  rclcpp::Subscription<fbg_shape_msgs::msg::CurvatureFrame>::SharedPtr subscription_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace fbg_shape_pipeline_cpp

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<fbg_shape_pipeline_cpp::ShapePublisherNode>());
  rclcpp::shutdown();
  return 0;
}
