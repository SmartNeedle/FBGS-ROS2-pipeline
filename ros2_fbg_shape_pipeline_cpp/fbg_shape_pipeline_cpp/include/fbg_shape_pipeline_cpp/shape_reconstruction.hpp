#pragma once

#include <string>

#include "geometry_msgs/msg/pose_array.hpp"

#include "fbg_shape_pipeline_cpp/frame_types.hpp"

namespace fbg_shape_pipeline_cpp
{

// Reconstruct the centerline using piecewise-constant curvature and an exact
// SE(3) exponential for each interval.
geometry_msgs::msg::PoseArray reconstruct_shape(
  const CurvatureFrameData & frame,
  double needle_length_m,
  const std::string & frame_id);

}  // namespace fbg_shape_pipeline_cpp
