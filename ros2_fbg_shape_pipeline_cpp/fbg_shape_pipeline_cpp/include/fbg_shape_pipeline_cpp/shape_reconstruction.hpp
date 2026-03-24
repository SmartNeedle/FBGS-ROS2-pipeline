#pragma once

#include <string>

#include "geometry_msgs/msg/pose_array.hpp"

#include "fbg_shape_pipeline_cpp/frame_types.hpp"

namespace fbg_shape_pipeline_cpp
{

// Baseline shape reconstruction.
//
// This reference version intentionally uses a simple constant-curvature-style
// integration so the pipeline can be tested immediately. It is the right place
// to substitute your validated needle model later.
geometry_msgs::msg::PoseArray reconstruct_shape(
  const CurvatureFrameData & frame,
  double needle_length_m,
  double output_spacing_m,
  int segment_substeps,
  const std::string & frame_id);

}  // namespace fbg_shape_pipeline_cpp
