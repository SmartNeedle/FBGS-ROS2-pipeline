#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <boost/asio.hpp>
#include "fbg_shape_pipeline_cpp/tcp_interrogator_client.hpp"

using namespace fbg_shape_pipeline_cpp;
using namespace std::chrono_literals;

TEST(TcpClient, FragmentationSwitchAndIdleShutdown)
{
  using boost::asio::ip::tcp;
  boost::asio::io_context io;
  tcp::acceptor first(io, tcp::endpoint(tcp::v4(), 0));
  tcp::acceptor second(io, tcp::endpoint(tcp::v4(), 0));
  first.non_blocking(true);
  second.non_blocking(true);
  std::atomic<int> received{0};
  TcpInterrogatorClient client("127.0.0.1", first.local_endpoint().port(),
    [&](const FbgFrameData & frame) { received = frame.fiber_index; },
    [](const std::string &) {}, 10ms);
  auto accept = [&](tcp::acceptor & acceptor, tcp::socket & socket) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    boost::system::error_code ec;
    do {
      acceptor.accept(socket, ec);
      if (!ec) { return true; }
      std::this_thread::sleep_for(5ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
  };
  auto wait = [&](int expected) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (received != expected && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(5ms);
    }
    return received == expected;
  };
  client.start();
  tcp::socket source1(io);
  ASSERT_TRUE(accept(first, source1));
  // Fragment the uint32 length prefix across TCP writes.
  std::array<unsigned char, 5> packet{{1,0,0,0,1}};
  boost::asio::write(source1, boost::asio::buffer(packet.data(), 2));
  std::this_thread::sleep_for(10ms);
  boost::asio::write(source1, boost::asio::buffer(packet.data()+2, 3));
  ASSERT_TRUE(wait(1));
  // First socket remains open and idle: switching must interrupt its read.
  client.set_endpoint("127.0.0.1", second.local_endpoint().port());
  tcp::socket source2(io);
  ASSERT_TRUE(accept(second, source2));
  packet[4] = 2;
  boost::asio::write(source2, boost::asio::buffer(packet));
  ASSERT_TRUE(wait(2));
  const auto start = std::chrono::steady_clock::now();
  client.stop();
  EXPECT_LT(std::chrono::steady_clock::now() - start, 500ms);
}
