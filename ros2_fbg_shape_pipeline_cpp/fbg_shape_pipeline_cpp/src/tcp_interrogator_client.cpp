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
namespace
{
// Run asynchronous I/O in the acquisition thread so shutdown/source changes
// can interrupt an idle socket, a pending connection, or DNS resolution.
template<typename Start>
void interruptible_io(boost::asio::io_context & io, const std::atomic<bool> & running, Start start)
{
  bool complete = false;
  boost::system::error_code result;
  io.restart();
  start([&](const boost::system::error_code & error, auto...) {
    result = error;
    complete = true;
  });
  while (running && !complete) {
    io.run_for(std::chrono::milliseconds(10));
  }
  if (!running) {
    throw std::runtime_error("Acquisition interrupted.");
  }
  if (result) {
    throw boost::system::system_error(result);
  }
}
}  // namespace

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

      tcp::resolver::results_type endpoints;
      interruptible_io(io_context, running_, [&](auto done) {
        resolver.async_resolve(host_, std::to_string(port_),
          [&, done](const boost::system::error_code & error, tcp::resolver::results_type found) {
            endpoints = std::move(found);
            done(error);
          });
      });
      interruptible_io(io_context, running_, [&](auto done) {
        boost::asio::async_connect(socket, endpoints, done);
      });
      socket.set_option(tcp::no_delay(true));

      if (error_callback_) {
        error_callback_("Connected to interrogator stream.");
      }

      while (running_) {
        std::array<std::uint8_t, 4> length_buffer {};
        interruptible_io(io_context, running_, [&](auto done) {
          boost::asio::async_read(socket, boost::asio::buffer(length_buffer), done);
        });

        std::uint32_t payload_length {0U};
        std::memcpy(&payload_length, length_buffer.data(), sizeof(payload_length));

        if (payload_length == 0U || payload_length > 64U * 1024U * 1024U) {
          throw std::runtime_error("Payload length outside supported 1..64 MiB range.");
        }

        std::vector<std::uint8_t> payload(payload_length);
        interruptible_io(io_context, running_, [&](auto done) {
          boost::asio::async_read(socket, boost::asio::buffer(payload), done);
        });

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
      if (running_ && error_callback_) {
        error_callback_(std::string("TCP receiver error: ") + ex.what());
      }

      if (running_) {
        const auto deadline = std::chrono::steady_clock::now() + reconnect_delay_;
        while (running_ && std::chrono::steady_clock::now() < deadline) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }
    }
  }
}

void TcpInterrogatorClient::set_endpoint(std::string host, std::uint16_t port)
{
  stop();
  host_ = std::move(host);
  port_ = port;
  start();
}

}  // namespace fbg_shape_pipeline_cpp
