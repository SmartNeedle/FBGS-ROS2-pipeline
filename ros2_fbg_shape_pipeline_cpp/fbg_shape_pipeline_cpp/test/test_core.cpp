#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <limits>
#include "fbg_shape_pipeline_cpp/frame_parser.hpp"
#include "fbg_shape_pipeline_cpp/shape_reconstruction.hpp"

using namespace fbg_shape_pipeline_cpp;

TEST(Reconstruction, FirstFbgOriginAndStraightTip)
{
  CurvatureFrameData frame;
  frame.arc_lengths = {11.592254970012, 21.592254970012, 181.592254970012};
  frame.kappa_x = frame.kappa_y = frame.kappa_z = {0.0, 0.0, 0.0};
  const auto shape = reconstruct_shape(frame, 196.391633064447, "needle");
  ASSERT_EQ(shape.poses.size(), 4U);
  EXPECT_DOUBLE_EQ(shape.poses.front().position.z, 0.0);
  EXPECT_NEAR(shape.poses.back().position.z, 184.799378094435, 1e-10);
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
    const double s = (i < 3 ? frame.arc_lengths[i] : 100.0) - 10.0;
    EXPECT_NEAR(shape.poses[i].position.x, 0.0, 1e-10);
    EXPECT_NEAR(shape.poses[i].position.y, -(1.0 - std::cos(0.003*s))/0.003, 1e-9);
    EXPECT_NEAR(shape.poses[i].position.z, std::sin(0.003*s)/0.003, 1e-9);
  }
}

TEST(Reconstruction, NoInterpolationAndZeroLengthTipInterval)
{
  CurvatureFrameData frame;
  frame.arc_lengths = {10.0, 30.0};
  frame.kappa_x = {0.0, 0.004};
  frame.kappa_y = frame.kappa_z = {0.0, 0.0};
  const auto shape = reconstruct_shape(frame, 30.0, "needle");
  ASSERT_EQ(shape.poses.size(), 3U);
  EXPECT_DOUBLE_EQ(shape.poses[1].position.y, 0.0);
  EXPECT_DOUBLE_EQ(shape.poses[1].position.z, 20.0);
  EXPECT_DOUBLE_EQ(shape.poses[2].position.z, 20.0);
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
  ASSERT_EQ(shape.poses.size(), 5U);
  // Independent scipy.linalg.expm oracle using the supplied MATLAB loop.
  const double expected[4][3] = {
    {0, -0.14998875033749459, 9.9985000674985542},
    {1.5991468486903073, -1.3485293599048433, 49.937867594461835},
    {18.302709579411257, 2.2774423072783896, 168.56887737736957},
    {21.0183397369854, 3.3939408785095679, 183.07295946940013}};
  for (std::size_t i=0; i<4; ++i) {
    EXPECT_NEAR(shape.poses[i+1].position.x, expected[i][0], 1e-9);
    EXPECT_NEAR(shape.poses[i+1].position.y, expected[i][1], 1e-9);
    EXPECT_NEAR(shape.poses[i+1].position.z, expected[i][2], 1e-9);
  }
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
