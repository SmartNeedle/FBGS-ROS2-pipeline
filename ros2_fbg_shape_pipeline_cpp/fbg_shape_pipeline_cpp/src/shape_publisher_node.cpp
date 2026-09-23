#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
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
    declare_parameter<bool>("timing_diagnostics", false);
    diagnostics_timer_ = create_wall_timer(
      std::chrono::seconds(2), std::bind(&ShapePublisherNode::report_timing, this));
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
  using Clock = std::chrono::steady_clock;

  void report_timing()
  {
    const bool requested = get_parameter("timing_diagnostics").as_bool();
    const auto now = Clock::now();
    if (requested && timing_enabled_) {
      const double seconds = std::chrono::duration<double>(now - window_start_).count();
      const double divisor = received_ == 0 ? 1.0 : static_cast<double>(received_);
      RCLCPP_INFO(get_logger(),
        "Shape timing: completed callbacks=%zu rate=%.1f Hz; "
        "first/last received line=%llu/%llu; "
        "reconstruct mean/max=%.3f/%.3f ms; publish call mean/max=%.3f/%.3f ms",
        received_, static_cast<double>(received_) / seconds,
        static_cast<unsigned long long>(first_received_line_),
        static_cast<unsigned long long>(last_received_line_),
        reconstruct_sum_ / divisor, reconstruct_max_, publish_sum_ / divisor, publish_max_);
    }
    timing_enabled_ = requested;
    window_start_ = now;
    received_ = 0;
    first_received_line_ = last_received_line_ = 0;
    reconstruct_sum_ = reconstruct_max_ = publish_sum_ = publish_max_ = 0.0;
  }

  void handle_curvature(const fbg_shape_msgs::msg::CurvatureFrame::SharedPtr msg)
  {
    const auto started = timing_enabled_ ? Clock::now() : Clock::time_point{};
    auto pose_array = reconstruct_shape(
      msg->arc_lengths, msg->kappa_x, msg->kappa_y, msg->kappa_z,
      needle_length_mm_, frame_id_);
    pose_array.header.stamp = msg->header.stamp;
    const auto reconstructed = timing_enabled_ ? Clock::now() : Clock::time_point{};
    publisher_->publish(pose_array);
    if (timing_enabled_) {
      const auto published = Clock::now();
      if (received_++ == 0) {
        first_received_line_ = msg->line_number;
      }
      last_received_line_ = msg->line_number;
      const double reconstruction_ms =
        std::chrono::duration<double, std::milli>(reconstructed - started).count();
      const double publication_ms =
        std::chrono::duration<double, std::milli>(published - reconstructed).count();
      reconstruct_sum_ += reconstruction_ms;
      reconstruct_max_ = std::max(reconstruct_max_, reconstruction_ms);
      publish_sum_ += publication_ms;
      publish_max_ = std::max(publish_max_, publication_ms);
    }
  }

  std::string input_topic_;
  std::string output_topic_;
  std::string frame_id_;
  double needle_length_mm_ {200.0};
  // Timer and subscription share the default mutually exclusive callback group.
  bool timing_enabled_ {false};
  std::size_t received_ {0};
  std::uint64_t first_received_line_ {0};
  std::uint64_t last_received_line_ {0};
  Clock::time_point window_start_ {Clock::now()};
  double reconstruct_sum_ {0.0}, reconstruct_max_ {0.0};
  double publish_sum_ {0.0}, publish_max_ {0.0};
  rclcpp::TimerBase::SharedPtr diagnostics_timer_;
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
