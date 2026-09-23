#pragma once

#include <string>
#include <vector>

#include "geometry_msgs/msg/pose_array.hpp"

#include "fbg_shape_pipeline_cpp/frame_types.hpp"

namespace fbg_shape_pipeline_cpp
{

// Reconstruct the centerline using piecewise-constant curvature and an exact
// SE(3) exponential for each interval.
geometry_msgs::msg::PoseArray reconstruct_shape(
  const CurvatureFrameData & frame,
  double needle_length_mm,
  const std::string & frame_id);

geometry_msgs::msg::PoseArray reconstruct_shape(
  const std::vector<double> & arc_lengths,
  const std::vector<double> & kappa_x,
  const std::vector<double> & kappa_y,
  const std::vector<double> & kappa_z,
  double needle_length_mm,
  const std::string & frame_id);

}  // namespace fbg_shape_pipeline_cpp
