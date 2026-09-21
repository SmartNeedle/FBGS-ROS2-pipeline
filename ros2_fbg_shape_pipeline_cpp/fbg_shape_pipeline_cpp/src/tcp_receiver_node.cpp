#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "fbg_shape_msgs/msg/fbg_frame.hpp"
#include "fbg_shape_msgs/msg/spectra_core.hpp"
#include "fbg_shape_pipeline_cpp/tcp_interrogator_client.hpp"

namespace fbg_shape_pipeline_cpp
{

class TcpReceiverNode : public rclcpp::Node
{
public:
  TcpReceiverNode()
  : Node("tcp_receiver_node")
  {
    const auto host = declare_parameter<std::string>("tcp_host", "127.0.0.1");
    const auto port_value = declare_parameter<int>("tcp_port", 50012);
    const auto reconnect_ms = declare_parameter<int>("reconnect_ms", 1000);
    if (host.empty() || port_value < 1 || port_value > 65535 || reconnect_ms < 0) {
      throw std::invalid_argument("Invalid TCP host, port, or reconnect_ms");
    }
    const auto port = static_cast<std::uint16_t>(port_value);
    const auto topic_name = declare_parameter<std::string>("output_topic", "/needle/fbg_frame");

    publisher_ = create_publisher<fbg_shape_msgs::msg::FbgFrame>(topic_name, rclcpp::QoS(1));

    client_ = std::make_unique<TcpInterrogatorClient>(
      host,
      port,
      [this](const FbgFrameData & frame) { publish_frame(frame); },
      [this](const std::string & message) { RCLCPP_INFO(get_logger(), "%s", message.c_str()); },
      std::chrono::milliseconds(reconnect_ms));

    client_->start();
    parameter_callback_ = add_on_set_parameters_callback(
      [this](const std::vector<rclcpp::Parameter> & parameters) {
        rcl_interfaces::msg::SetParametersResult result;
        result.successful = true;
        auto next_host = get_parameter("tcp_host").as_string();
        auto next_port = get_parameter("tcp_port").as_int();
        bool changed = false;
        try {
          for (const auto & parameter : parameters) {
            if (parameter.get_name() == "tcp_host") {
              next_host = parameter.as_string();
              changed = true;
            } else if (parameter.get_name() == "tcp_port") {
              next_port = parameter.as_int();
              changed = true;
            } else {
              throw std::invalid_argument("Only tcp_host and tcp_port can change at runtime");
            }
          }
          if (next_host.empty() || next_port < 1 || next_port > 65535) {
            throw std::invalid_argument("TCP host must be nonempty and port must be 1..65535");
          }
          if (changed) {
            client_->set_endpoint(next_host, static_cast<std::uint16_t>(next_port));
            RCLCPP_INFO(get_logger(), "Source changed to %s:%ld", next_host.c_str(),
              static_cast<long>(next_port));
          }
        } catch (const std::exception & error) {
          result.successful = false;
          result.reason = error.what();
        }
        return result;
      });

    RCLCPP_INFO(
      get_logger(),
      "TCP receiver ready. Connecting to %s:%u and publishing %s",
      host.c_str(),
      port,
      topic_name.c_str());
  }

  ~TcpReceiverNode() override
  {
    if (client_) {
      client_->stop();
    }
  }

private:
  void publish_frame(const FbgFrameData & frame)
  {
    fbg_shape_msgs::msg::FbgFrame msg;
    msg.header.stamp = now();
    msg.header.frame_id = "needle";
    msg.fiber_index = frame.fiber_index;
    msg.error = frame.error;
    msg.line_number = frame.line_number;
    msg.source_timestamp = frame.source_timestamp;
    msg.curvature = frame.curvature;
    msg.angle = frame.angle;
    msg.temperature = frame.temperature;
    msg.shape_points = frame.shape_points;
    msg.shape_stride = frame.shape_width;
    msg.shape_height = frame.shape_height;
    msg.spectra_cores.reserve(frame.spectra_cores.size());

    for (const auto & core : frame.spectra_cores) {
      fbg_shape_msgs::msg::SpectraCore ros_core;
      ros_core.channel = core.channel;
      ros_core.timestamp = core.timestamp;
      ros_core.sample_number = core.sample_number;
      ros_core.error_status = core.error_status;
      ros_core.spectrum_wavelengths = core.spectrum_wavelengths;
      ros_core.spectrum_powers = core.spectrum_powers;
      ros_core.peaks_wavelengths = core.peaks_wavelengths;
      ros_core.peaks_powers = core.peaks_powers;
      ros_core.spectrometer_temperature = core.spectrometer_temperature;
      msg.spectra_cores.push_back(std::move(ros_core));
    }

    publisher_->publish(msg);
  }

  rclcpp::Publisher<fbg_shape_msgs::msg::FbgFrame>::SharedPtr publisher_;
  std::unique_ptr<TcpInterrogatorClient> client_;
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr parameter_callback_;
};

}  // namespace fbg_shape_pipeline_cpp

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<fbg_shape_pipeline_cpp::TcpReceiverNode>());
  rclcpp::shutdown();
  return 0;
}
