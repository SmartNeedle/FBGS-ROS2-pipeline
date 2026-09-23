#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <limits>
#include "fbg_shape_pipeline_cpp/frame_parser.hpp"
#include "fbg_shape_pipeline_cpp/shape_reconstruction.hpp"

using namespace fbg_shape_pipeline_cpp;

TEST(Parser, ShapeCoreSpectraBlockLengthsAndTruncation)
{
  // Synthetic zero-valued spectra: protocol structure only, no captured data.
  std::vector<std::uint8_t> payload(7, 0);
  payload[5] = 7;
  auto append = [&](const auto & value) {
      const auto offset = payload.size();
      payload.resize(offset + sizeof(value));
      std::memcpy(payload.data() + offset, &value, sizeof(value));
    };
  for (std::uint8_t channel = 0; channel < 4; ++channel) {
    append(std::uint32_t{3279}); // Excludes its own four-byte length prefix.
    append(channel);
    const std::uint32_t sizes[] = {8, 8, 2, 2052, 1028, 84, 44, 4};
    for (std::uint16_t id = 0; id < 8; ++id) {
      append(std::uint32_t{sizes[id] + 2});
      append(id);
      const auto offset = payload.size();
      payload.resize(offset + sizes[id], 0);
      if (id >= 3 && id <= 6) {
        const std::uint32_t count = id <= 4 ? 512 : 20;
        std::memcpy(payload.data() + offset, &count, 4);
      }
    }
  }
  const auto field_length = static_cast<std::uint32_t>(payload.size() - 5);
  std::memcpy(payload.data() + 1, &field_length, 4);
  const auto parsed = parse_frame_payload(payload);
  ASSERT_TRUE(parsed.success) << parsed.error_message;
  ASSERT_EQ(parsed.frame.spectra_cores.size(), 4U);
  for (std::size_t i = 0; i < 4; ++i) {
    const auto & core = parsed.frame.spectra_cores[i];
    EXPECT_EQ(core.channel, i);
    EXPECT_EQ(core.spectrum_wavelengths.size(), 512U);
    EXPECT_EQ(core.spectrum_powers.size(), 512U);
    EXPECT_EQ(core.peaks_wavelengths.size(), 20U);
    EXPECT_EQ(core.peaks_powers.size(), 20U);
    EXPECT_EQ(core.error_status, 0U);
  }
  // Keep outer framing intact but force a nested block to end inside a subfield.
  const std::uint32_t shortened_block = 3278U;
  std::memcpy(payload.data() + 7, &shortened_block, 4);
  EXPECT_FALSE(parse_frame_payload(payload).success);
  payload.pop_back();
  EXPECT_FALSE(parse_frame_payload(payload).success);
}

TEST(Reconstruction, BaseOriginAndFullLengthStraightTip)
{
  CurvatureFrameData frame;
  frame.arc_lengths = {11.592254970012, 21.592254970012, 181.592254970012};
  frame.kappa_x = frame.kappa_y = frame.kappa_z = {0.0, 0.0, 0.0};
  const auto shape = reconstruct_shape(frame, 196.391633064447, "needle");
  ASSERT_EQ(shape.poses.size(), 4U);
  EXPECT_DOUBLE_EQ(shape.poses.front().position.z, 0.0);
  EXPECT_NEAR(shape.poses[1].position.z, 16.592254970012, 1e-10);
  EXPECT_NEAR(shape.poses[2].position.z, 101.592254970012, 1e-10);
  EXPECT_NEAR(shape.poses.back().position.z, 196.391633064447, 1e-10);
}

TEST(Reconstruction, ConstantBendMatchesAnalyticalSE3)
{
  CurvatureFrameData frame;
  frame.arc_lengths = {10.0, 30.0, 70.0};
  frame.kappa_x = {0.003, 0.003, 0.003};
  frame.kappa_y = frame.kappa_z = {0.0, 0.0, 0.0};
  const auto shape = reconstruct_shape(frame, 100.0, "needle");
  ASSERT_EQ(shape.poses.size(), 4U);
  for (std::size_t i = 0; i < 4; ++i) {
    const double boundaries[] = {0.0, 20.0, 50.0, 100.0};
    const double s = boundaries[i];
    EXPECT_NEAR(shape.poses[i].position.x, 0.0, 1e-10);
    EXPECT_NEAR(shape.poses[i].position.y, -(1.0 - std::cos(0.003*s))/0.003, 1e-9);
    EXPECT_NEAR(shape.poses[i].position.z, std::sin(0.003*s)/0.003, 1e-9);
    EXPECT_NEAR(shape.poses[i].orientation.x, std::sin(0.0015*s), 1e-12);
    EXPECT_NEAR(shape.poses[i].orientation.w, std::cos(0.0015*s), 1e-12);
  }
}

TEST(Reconstruction, NoInterpolationAndLastMeasurementAtTip)
{
  CurvatureFrameData frame;
  frame.arc_lengths = {10.0, 30.0};
  frame.kappa_x = {0.0, 0.004};
  frame.kappa_y = frame.kappa_z = {0.0, 0.0};
  const auto shape = reconstruct_shape(frame, 30.0, "needle");
  ASSERT_EQ(shape.poses.size(), 3U);
  EXPECT_DOUBLE_EQ(shape.poses[1].position.y, 0.0);
  EXPECT_DOUBLE_EQ(shape.poses[1].position.z, 20.0);
  EXPECT_NEAR(shape.poses[2].position.y, -(1.0 - std::cos(0.04))/0.004, 1e-10);
  EXPECT_NEAR(shape.poses[2].position.z, 20.0 + std::sin(0.04)/0.004, 1e-10);
  frame.arc_lengths = {30.0, 10.0};
  EXPECT_TRUE(reconstruct_shape(frame, 40.0, "needle").poses.empty());
}

TEST(Parser, RejectsMismatchedFieldLength)
{
  // Fiber=0, field length=2, error ID=0, but error uint16 follows.
  EXPECT_FALSE(parse_frame_payload({0,2,0,0,0,0,0,0,0}).success);
  EXPECT_FALSE(parse_frame_payload({0,255,255,255,127,0,0}).success);
  EXPECT_FALSE(parse_frame_payload({0,3,0,0,0,7,0,0}).success);
}

TEST(Reconstruction, VaryingBendsMatchIndependentMatrixExponential)
{
  CurvatureFrameData frame;
  frame.arc_lengths = {11.592254970012,21.592254970012,61.592254970012,181.592254970012};
  frame.kappa_x = {0.003,0.0,-0.001,0.002};
  frame.kappa_y = {0.0,0.002,0.001,-0.002};
  frame.kappa_z = {0.0,0.0,0.0,0.0};
  const auto shape = reconstruct_shape(frame, 196.391633064447, "needle");
  const auto direct_shape = reconstruct_shape(
    frame.arc_lengths, frame.kappa_x, frame.kappa_y, frame.kappa_z,
    196.391633064447, "needle");
  ASSERT_EQ(shape.poses.size(), 5U);
  ASSERT_EQ(direct_shape.poses.size(), shape.poses.size());
  // Independent scipy.linalg.expm oracle with midpoint-bounded intervals.
  const double expected[4][3] = {
    {0.0, -0.41286912886361476, 16.585403974323565},
    {0.6248698025168766, -1.6562561978465626, 41.54403632554556},
    {7.807272243255075, -2.422724881163089, 121.17444425063125},
    {11.926403393277996, -5.744019292731825, 195.64665975599456}};
  const double expected_orientation[4][4] = {
    {0.0248858130929134, 0.0, 0.0, 0.9996903001963682},
    {0.0248780366813560, 0.0249896542261062, 0.0006220805225431, 0.9993779132482188},
    {-0.0151404273945406, 0.0648786113672154, 0.0026147294215276, 0.9977748726217877},
    {0.0596332478087249, -0.0097826557679372, -0.0011133368713461, 0.9981717967779494}};
  for (std::size_t i=0; i<4; ++i) {
    EXPECT_NEAR(shape.poses[i+1].position.x, expected[i][0], 1e-9);
    EXPECT_NEAR(shape.poses[i+1].position.y, expected[i][1], 1e-9);
    EXPECT_NEAR(shape.poses[i+1].position.z, expected[i][2], 1e-9);
    EXPECT_DOUBLE_EQ(direct_shape.poses[i+1].position.x, shape.poses[i+1].position.x);
    EXPECT_DOUBLE_EQ(direct_shape.poses[i+1].position.y, shape.poses[i+1].position.y);
    EXPECT_DOUBLE_EQ(direct_shape.poses[i+1].position.z, shape.poses[i+1].position.z);
    EXPECT_NEAR(shape.poses[i+1].orientation.x, expected_orientation[i][0], 1e-12);
    EXPECT_NEAR(shape.poses[i+1].orientation.y, expected_orientation[i][1], 1e-12);
    EXPECT_NEAR(shape.poses[i+1].orientation.z, expected_orientation[i][2], 1e-12);
    EXPECT_NEAR(shape.poses[i+1].orientation.w, expected_orientation[i][3], 1e-12);
  }
}

TEST(Reconstruction, SingleMeasurementCoversWholeNeedle)
{
  CurvatureFrameData frame;
  frame.arc_lengths = {40.0};
  frame.kappa_x = {0.003};
  frame.kappa_y = frame.kappa_z = {0.0};
  const auto shape = reconstruct_shape(frame, 100.0, "needle");
  ASSERT_EQ(shape.poses.size(), 2U);
  EXPECT_NEAR(shape.poses.back().position.y, -(1.0 - std::cos(0.3))/0.003, 1e-9);
  EXPECT_NEAR(shape.poses.back().position.z, std::sin(0.3)/0.003, 1e-9);
  EXPECT_TRUE(reconstruct_shape(frame, 0.0, "needle").poses.empty());
}

TEST(Reconstruction, SmallAngleLimitIsStable)
{
  CurvatureFrameData frame;
  frame.arc_lengths = {50.0};
  frame.kappa_x = {1e-12};
  frame.kappa_y = frame.kappa_z = {0.0};
  const auto shape = reconstruct_shape(frame, 100.0, "needle");
  ASSERT_EQ(shape.poses.size(), 2U);
  EXPECT_NEAR(shape.poses.back().position.y, -5e-9, 1e-18);
  EXPECT_NEAR(shape.poses.back().position.z, 100.0, 1e-10);
  EXPECT_NEAR(shape.poses.back().orientation.x, 5e-11, 1e-18);
  EXPECT_NEAR(shape.poses.back().orientation.w, 1.0, 1e-15);
}

TEST(Parser, VariableLengthCurvature)
{
  for (std::uint32_t count : {1U,20U,26U}) {
    std::vector<std::uint8_t> packet(11 + 4*count, 0);
    const std::uint32_t length = 6 + 4*count;
    std::memcpy(packet.data()+1, &length, 4);
    packet[5] = 3;
    std::memcpy(packet.data()+7, &count, 4);
    const auto result = parse_frame_payload(packet);
    ASSERT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(result.frame.curvature.size(), count);
  }
}
