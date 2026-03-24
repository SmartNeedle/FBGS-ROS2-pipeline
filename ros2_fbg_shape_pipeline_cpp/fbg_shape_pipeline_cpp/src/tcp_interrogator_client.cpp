#include "fbg_shape_pipeline_cpp/tcp_interrogator_client.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "fbg_shape_pipeline_cpp/frame_parser.hpp"

namespace fbg_shape_pipeline_cpp
{

TcpInterrogatorClient::TcpInterrogatorClient(
  std::string host,
  std::uint16_t port,
  FrameCallback frame_callback,
  ErrorCallback error_callback,
  std::chrono::milliseconds reconnect_delay)
: host_(std::move(host)),
  port_(port),
  frame_callback_(std::move(frame_callback)),
  error_callback_(std::move(error_callback)),
  reconnect_delay_(reconnect_delay)
{
}

TcpInterrogatorClient::~TcpInterrogatorClient()
{
  stop();
}

void TcpInterrogatorClient::start()
{
  if (running_.exchange(true)) {
    return;
  }

  worker_ = std::thread(&TcpInterrogatorClient::run, this);
}

void TcpInterrogatorClient::stop()
{
  if (!running_.exchange(false)) {
    return;
  }

  if (worker_.joinable()) {
    worker_.join();
  }
}

void TcpInterrogatorClient::run()
{
  using boost::asio::ip::tcp;

  while (running_) {
    try {
      boost::asio::io_context io_context;
      tcp::resolver resolver(io_context);
      tcp::socket socket(io_context);

      const auto endpoints = resolver.resolve(host_, std::to_string(port_));
      boost::asio::connect(socket, endpoints);

      if (error_callback_) {
        error_callback_("Connected to interrogator stream.");
      }

      while (running_) {
        std::array<std::uint8_t, 4> length_buffer {};
        boost::asio::read(socket, boost::asio::buffer(length_buffer));

        std::uint32_t payload_length {0U};
        std::memcpy(&payload_length, length_buffer.data(), sizeof(payload_length));

        if (payload_length == 0U) {
          throw std::runtime_error("Received zero-length payload.");
        }

        std::vector<std::uint8_t> payload(payload_length);
        boost::asio::read(socket, boost::asio::buffer(payload));

        const auto parsed = parse_frame_payload(payload);
        if (!parsed.success) {
          std::ostringstream stream;
          stream << "Failed to parse frame: " << parsed.error_message;
          throw std::runtime_error(stream.str());
        }

        if (frame_callback_) {
          frame_callback_(parsed.frame);
        }
      }
    } catch (const std::exception & ex) {
      if (error_callback_) {
        error_callback_(std::string("TCP receiver error: ") + ex.what());
      }

      if (running_) {
        std::this_thread::sleep_for(reconnect_delay_);
      }
    }
  }
}

}  // namespace fbg_shape_pipeline_cpp
