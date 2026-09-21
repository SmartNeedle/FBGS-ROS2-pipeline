#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include "fbg_shape_pipeline_cpp/frame_types.hpp"

namespace fbg_shape_pipeline_cpp
{

// Dedicated TCP client for the FBG interrogator stream.
//
// Design:
// - one background thread
// - interruptible exact-length reads
// - parse and callback immediately
// - reconnect automatically
//
// This keeps the hottest latency path small and predictable.
class TcpInterrogatorClient
{
public:
  using FrameCallback = std::function<void(const FbgFrameData &)>;
  using ErrorCallback = std::function<void(const std::string &)>;

  TcpInterrogatorClient(
    std::string host,
    std::uint16_t port,
    FrameCallback frame_callback,
    ErrorCallback error_callback,
    std::chrono::milliseconds reconnect_delay = std::chrono::milliseconds(1000));

  ~TcpInterrogatorClient();

  void start();
  void stop();
  void set_endpoint(std::string host, std::uint16_t port);

private:
  void run();

  std::string host_;
  std::uint16_t port_ {0U};
  FrameCallback frame_callback_;
  ErrorCallback error_callback_;
  std::chrono::milliseconds reconnect_delay_ {1000};

  std::atomic<bool> running_ {false};
  std::thread worker_;
};

}  // namespace fbg_shape_pipeline_cpp
