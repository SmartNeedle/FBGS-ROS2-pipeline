#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"

#include "geometry_msgs/msg/pose_array.hpp"
#include "fbg_shape_msgs/msg/curvature_frame.hpp"
#include "fbg_shape_msgs/msg/fbg_frame.hpp"
#include "fbg_shape_pipeline_cpp/shape_reconstruction.hpp"

namespace fbg_shape_pipeline_cpp
{

namespace
{

constexpr double kPi = 3.14159265358979323846;
double wrap_pi(double angle)
{
  return std::remainder(angle, 2.0 * kPi);
}

std::vector<double> arithmetic_mean(const std::deque<std::vector<double>> & values)
{
  if (values.empty()) {
    return {};
  }

  std::vector<double> mean(values.front().size(), 0.0);
  for (const auto & row : values) {
    for (std::size_t i = 0; i < row.size(); ++i) {
      mean[i] += row[i];
    }
  }

  for (auto & value : mean) {
    value /= static_cast<double>(values.size());
  }

  return mean;
}

std::vector<double> circular_mean(const std::deque<std::vector<double>> & values)
{
  if (values.empty()) {
    return {};
  }

  std::vector<double> mean(values.front().size(), 0.0);
  for (std::size_t i = 0; i < mean.size(); ++i) {
    double sum_sin = 0.0;
    double sum_cos = 0.0;
    for (const auto & row : values) {
      sum_sin += std::sin(row[i]);
      sum_cos += std::cos(row[i]);
    }
    mean[i] = std::atan2(sum_sin, sum_cos);
  }

  return mean;
}

std::vector<double> subvector_to_double(
  const std::vector<float> & input,
  std::size_t start,
  std::size_t count)
{
  if (start >= input.size()) {
    return {};
  }
  const auto begin = input.begin() + static_cast<std::ptrdiff_t>(start);
  const auto end = input.begin() + static_cast<std::ptrdiff_t>(std::min(input.size(), start + count));
  return std::vector<double>(begin, end);
}

void push_latest(
  std::deque<std::vector<double>> & queue,
  std::vector<double> values,
  std::size_t max_size)
{
  queue.push_back(std::move(values));
  while (queue.size() > max_size) {
    queue.pop_front();
  }
}

}  // namespace

class CurvatureProcessorNode : public rclcpp::Node
{
public:
  CurvatureProcessorNode()
  : Node("curvature_processor_node")
  {
    input_topic_ = declare_parameter<std::string>("input_topic", "/needle/fbg_frame");
    output_topic_ = declare_parameter<std::string>("output_topic", "/needle/state/curvatures");
    publish_shape_ = declare_parameter<bool>("publish_shape", false);
    shape_output_topic_ = declare_parameter<std::string>(
      "shape_output_topic", "/needle/state/current_shape");
    shape_frame_id_ = declare_parameter<std::string>("shape_frame_id", "needle");
    needle_length_mm_ = declare_parameter<double>("needle_length_mm", 200.0);

    sensor_arc_lengths_mm_ = declare_parameter<std::vector<double>>(
      "sensor_arc_lengths_mm", std::vector<double>{0.0, 10.0, 20.0, 30.0});
    curvature_scale_ = declare_parameter<std::vector<double>>(
      "curvature_scale", std::vector<double>{1.0, 1.0, 1.0, 1.0});
    orientation_sign_raw_ = declare_parameter<std::vector<double>>(
      "orientation_sign", std::vector<double>{1.0, 1.0, 1.0, 1.0});
    orientation_offset_rad_ = declare_parameter<std::vector<double>>(
      "orientation_offset_rad", std::vector<double>{0.0, 0.0, 0.0, 0.0});
    first_fbg_index_ = static_cast<std::size_t>(declare_parameter<int>("first_fbg_index", 1));

    enable_data_averaging_ = declare_parameter<bool>("enable_data_averaging", false);
    data_average_window_ = static_cast<std::size_t>(
      declare_parameter<int>("data_average_window", 1));
    enable_temporal_averaging_ = declare_parameter<bool>("enable_temporal_averaging", false);
    temporal_average_window_ = static_cast<std::size_t>(
      declare_parameter<int>("temporal_average_window", 1));
    declare_parameter<bool>("timing_diagnostics", false);
    diagnostics_timer_ = create_wall_timer(
      std::chrono::seconds(2), std::bind(&CurvatureProcessorNode::report_counts, this));

    normalize_parameters();

    // Keep only the newest message in the DDS queue because this pipeline is
    // intended for real-time feedback, not historical replay.
    publisher_ = create_publisher<fbg_shape_msgs::msg::CurvatureFrame>(output_topic_, rclcpp::QoS(1));
    if (publish_shape_) {
      shape_publisher_ = create_publisher<geometry_msgs::msg::PoseArray>(
        shape_output_topic_, rclcpp::QoS(1));
    }
    subscription_ = create_subscription<fbg_shape_msgs::msg::FbgFrame>(
      input_topic_,
      rclcpp::QoS(1),
      std::bind(&CurvatureProcessorNode::handle_frame, this, std::placeholders::_1));

    RCLCPP_INFO(
      get_logger(),
      "Curvature processor ready. Subscribing to %s and publishing %s",
      input_topic_.c_str(),
      output_topic_.c_str());
  }

private:
  void report_counts()
  {
    const bool requested = get_parameter("timing_diagnostics").as_bool();
    if (requested && diagnostics_enabled_) {
      const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - window_start_).count();
      RCLCPP_INFO(get_logger(),
        "Curvature counts: received=%zu published=%zu rate=%.1f Hz; "
        "first/last published line=%llu/%llu",
        received_, published_, static_cast<double>(published_) / seconds,
        static_cast<unsigned long long>(first_published_line_),
        static_cast<unsigned long long>(last_published_line_));
      if (publish_shape_) {
        const double divisor = shapes_published_ == 0 ? 1.0 :
          static_cast<double>(shapes_published_);
        RCLCPP_INFO(get_logger(),
          "Fused shape: published=%zu rate=%.1f Hz; reconstruct and publish "
          "mean/max=%.3f/%.3f ms",
          shapes_published_, static_cast<double>(shapes_published_) / seconds,
          shape_sum_ms_ / divisor, shape_max_ms_);
      }
    }
    diagnostics_enabled_ = requested;
    window_start_ = std::chrono::steady_clock::now();
    received_ = published_ = 0;
    shapes_published_ = 0;
    shape_sum_ms_ = shape_max_ms_ = 0.0;
    first_published_line_ = last_published_line_ = 0;
  }

  void normalize_parameters()
  {
    if (sensor_arc_lengths_mm_.empty()) {
      throw std::runtime_error("sensor_arc_lengths_mm must not be empty.");
    }

    const std::size_t n = sensor_arc_lengths_mm_.size();
    if (
      curvature_scale_.size() != n ||
      orientation_sign_raw_.size() != n ||
      orientation_offset_rad_.size() != n)
    {
      throw std::runtime_error(
              "Calibration vectors must all have the same length as sensor_arc_lengths_mm.");
    }

    orientation_sign_.resize(n, 1);
    for (std::size_t i = 0; i < n; ++i) {
      if (curvature_scale_[i] != 1.0 || !std::isfinite(orientation_sign_raw_[i]) ||
        std::abs(orientation_sign_raw_[i]) != 1.0 || !std::isfinite(orientation_offset_rad_[i]) ||
        !std::isfinite(sensor_arc_lengths_mm_[i]) || sensor_arc_lengths_mm_[i] < 0.0 ||
        (i > 0 && sensor_arc_lengths_mm_[i] <= sensor_arc_lengths_mm_[i - 1]))
      {
        throw std::runtime_error("Invalid calibration: require unity scales, signs +/-1, finite offsets and increasing positions.");
      }
      orientation_sign_[i] = orientation_sign_raw_[i] >= 0.0 ? 1 : -1;
    }

    data_average_window_ = std::max<std::size_t>(1U, data_average_window_);
    temporal_average_window_ = std::max<std::size_t>(1U, temporal_average_window_);
    first_fbg_index_ = std::max<std::size_t>(1U, first_fbg_index_);
  }

  std::vector<double> to_double_vector(const std::vector<float> & input) const
  {
    return std::vector<double>(input.begin(), input.end());
  }

  void handle_frame(const fbg_shape_msgs::msg::FbgFrame::SharedPtr msg)
  {
    if (diagnostics_enabled_) {
      ++received_;
    }
    if (msg->error != 0) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
        "Interrogator error %u: suppressing reconstruction", msg->error);
      return;
    }
    const std::size_t n = sensor_arc_lengths_mm_.size();
    const std::size_t start = first_fbg_index_ - 1U;
    const std::size_t required = start + n;
    if (msg->curvature.size() < required || msg->angle.size() < required) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000,
        "Incoming curvature/angle lengths (%zu, %zu) are too short for first_fbg_index=%zu and calibration length=%zu.",
        msg->curvature.size(), msg->angle.size(), first_fbg_index_, n);
      return;
    }

    auto raw_curvature = subvector_to_double(msg->curvature, start, n);
    auto raw_angle = subvector_to_double(msg->angle, start, n);
    for (std::size_t i = 0; i < n; ++i) {
      if (!std::isfinite(raw_curvature[i]) || !std::isfinite(raw_angle[i])) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Dropping nonfinite curvature/angle frame");
        return;
      }
    }
    std::vector<double> raw_temperature;
    if (msg->temperature.size() >= required) {
      raw_temperature = subvector_to_double(msg->temperature, start, n);
    } else if (msg->temperature.size() == n) {
      raw_temperature = to_double_vector(msg->temperature);
    } else if (!msg->temperature.empty()) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000,
        "Incoming temperature length %zu is incompatible with first_fbg_index=%zu and calibration length=%zu. Publishing without temperature.",
        msg->temperature.size(), first_fbg_index_, n);
    }

    if (enable_data_averaging_) {
      push_latest(raw_curvature_history_, raw_curvature, data_average_window_);
      push_latest(raw_angle_history_, raw_angle, data_average_window_);
      if (!raw_temperature.empty()) {
        push_latest(raw_temperature_history_, raw_temperature, data_average_window_);
      }

      raw_curvature = arithmetic_mean(raw_curvature_history_);
      raw_angle = circular_mean(raw_angle_history_);
      if (!raw_temperature_history_.empty()) {
        raw_temperature = arithmetic_mean(raw_temperature_history_);
      }
    }

    std::vector<double> calibrated_curvature(n, 0.0);
    std::vector<double> calibrated_angle(n, 0.0);
    std::vector<double> kappa_x(n, 0.0);
    std::vector<double> kappa_y(n, 0.0);
    std::vector<double> kappa_z(n, 0.0);

    for (std::size_t i = 0; i < n; ++i) {
      // Keep the interrogator convention: curvature is expressed in 1/mm.
      calibrated_curvature[i] = raw_curvature[i];
      calibrated_angle[i] = wrap_pi(
        static_cast<double>(orientation_sign_[i]) * raw_angle[i] + orientation_offset_rad_[i]);
      kappa_x[i] = calibrated_curvature[i] * std::cos(calibrated_angle[i]);
      kappa_y[i] = calibrated_curvature[i] * std::sin(calibrated_angle[i]);
      kappa_z[i] = 0.0;
    }

    if (enable_temporal_averaging_) {
      push_latest(kappa_x_history_, kappa_x, temporal_average_window_);
      push_latest(kappa_y_history_, kappa_y, temporal_average_window_);
      push_latest(kappa_z_history_, kappa_z, temporal_average_window_);
      push_latest(curvature_history_, calibrated_curvature, temporal_average_window_);
      push_latest(angle_history_, calibrated_angle, temporal_average_window_);
      if (!raw_temperature.empty()) {
        push_latest(temperature_history_, raw_temperature, temporal_average_window_);
      }

      kappa_x = arithmetic_mean(kappa_x_history_);
      kappa_y = arithmetic_mean(kappa_y_history_);
      kappa_z = arithmetic_mean(kappa_z_history_);
      calibrated_curvature = arithmetic_mean(curvature_history_);
      calibrated_angle = circular_mean(angle_history_);
      if (!temperature_history_.empty()) {
        raw_temperature = arithmetic_mean(temperature_history_);
      }
    }

    fbg_shape_msgs::msg::CurvatureFrame output;
    output.header = msg->header;
    output.line_number = msg->line_number;
    output.source_timestamp = msg->source_timestamp;
    output.arc_lengths = sensor_arc_lengths_mm_;
    output.curvature = calibrated_curvature;
    output.angle = calibrated_angle;
    output.kappa_x = kappa_x;
    output.kappa_y = kappa_y;
    output.kappa_z = kappa_z;
    output.temperature = raw_temperature;
    publisher_->publish(output);
    if (publish_shape_) {
      const auto started = diagnostics_enabled_ ?
        std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
      auto shape = reconstruct_shape(
        output.arc_lengths, output.kappa_x, output.kappa_y, output.kappa_z,
        needle_length_mm_, shape_frame_id_);
      shape.header.stamp = output.header.stamp;
      shape_publisher_->publish(shape);
      if (diagnostics_enabled_) {
        const double elapsed_ms = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - started).count();
        ++shapes_published_;
        shape_sum_ms_ += elapsed_ms;
        shape_max_ms_ = std::max(shape_max_ms_, elapsed_ms);
      }
    }
    if (diagnostics_enabled_) {
      if (published_++ == 0) {
        first_published_line_ = msg->line_number;
      }
      last_published_line_ = msg->line_number;
    }
  }

  std::string input_topic_;
  std::string output_topic_;
  std::string shape_output_topic_;
  std::string shape_frame_id_;
  double needle_length_mm_ {200.0};
  bool publish_shape_ {false};

  std::vector<double> sensor_arc_lengths_mm_;
  std::vector<double> curvature_scale_;
  std::vector<double> orientation_sign_raw_;
  std::vector<int> orientation_sign_;
  std::vector<double> orientation_offset_rad_;
  std::size_t first_fbg_index_ {1U};

  bool enable_data_averaging_ {false};
  std::size_t data_average_window_ {1U};
  bool enable_temporal_averaging_ {false};
  std::size_t temporal_average_window_ {1U};
  bool diagnostics_enabled_ {false};
  std::size_t received_ {0};
  std::size_t published_ {0};
  std::size_t shapes_published_ {0};
  double shape_sum_ms_ {0.0};
  double shape_max_ms_ {0.0};
  std::uint64_t first_published_line_ {0};
  std::uint64_t last_published_line_ {0};
  std::chrono::steady_clock::time_point window_start_ {std::chrono::steady_clock::now()};
  rclcpp::TimerBase::SharedPtr diagnostics_timer_;

  std::deque<std::vector<double>> raw_curvature_history_;
  std::deque<std::vector<double>> raw_angle_history_;
  std::deque<std::vector<double>> raw_temperature_history_;

  std::deque<std::vector<double>> curvature_history_;
  std::deque<std::vector<double>> angle_history_;
  std::deque<std::vector<double>> kappa_x_history_;
  std::deque<std::vector<double>> kappa_y_history_;
  std::deque<std::vector<double>> kappa_z_history_;
  std::deque<std::vector<double>> temperature_history_;

  rclcpp::Publisher<fbg_shape_msgs::msg::CurvatureFrame>::SharedPtr publisher_;
  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr shape_publisher_;
  rclcpp::Subscription<fbg_shape_msgs::msg::FbgFrame>::SharedPtr subscription_;
};

}  // namespace fbg_shape_pipeline_cpp

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<fbg_shape_pipeline_cpp::CurvatureProcessorNode>());
  rclcpp::shutdown();
  return 0;
}
