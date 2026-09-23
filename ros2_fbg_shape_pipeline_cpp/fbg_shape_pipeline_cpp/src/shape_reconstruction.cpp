#include "fbg_shape_pipeline_cpp/shape_reconstruction.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace fbg_shape_pipeline_cpp
{

namespace
{

using Vec3 = std::array<double, 3>;
using Quaternion = std::array<double, 4>;  // x, y, z, w

struct PoseState
{
  Quaternion rotation {{0.0, 0.0, 0.0, 1.0}};
  Vec3 position {{0.0, 0.0, 0.0}};
};

Vec3 cross(const Vec3 & a, const Vec3 & b)
{
  return {{a[1] * b[2] - a[2] * b[1],
      a[2] * b[0] - a[0] * b[2],
      a[0] * b[1] - a[1] * b[0]}};
}

Quaternion multiply(const Quaternion & a, const Quaternion & b)
{
  return {{
      a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
      a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
      a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
      a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    }};
}

Vec3 rotate(const Quaternion & q, const Vec3 & v)
{
  const Vec3 u {{q[0], q[1], q[2]}};
  const Vec3 uv = cross(u, v);
  const Vec3 uuv = cross(u, uv);
  return {{v[0] + 2.0 * (q[3] * uv[0] + uuv[0]),
      v[1] + 2.0 * (q[3] * uv[1] + uuv[1]),
      v[2] + 2.0 * (q[3] * uv[2] + uuv[2])}};
}

PoseState integrate_segment(
  const PoseState & start,
  const Vec3 & kappa,
  double ds)
{
  PoseState next = start;
  const double omega_norm = std::sqrt(
    kappa[0] * kappa[0] + kappa[1] * kappa[1] + kappa[2] * kappa[2]);
  const double theta = omega_norm * ds;
  double half_sine_scale;
  double half_cosine;
  double a;
  double b;
  if (std::abs(theta) < 1e-4) {
    const double theta2 = theta * theta;
    const double theta4 = theta2 * theta2;
    half_sine_scale = ds * (0.5 - theta2 / 48.0 + theta4 / 3840.0);
    half_cosine = 1.0 - theta2 / 8.0 + theta4 / 384.0;
    a = ds * ds * (0.5 - theta2 / 24.0 + theta4 / 720.0);
    b = ds * ds * ds * (1.0 / 6.0 - theta2 / 120.0 + theta4 / 5040.0);
  } else {
    const double half_sine = std::sin(0.5 * theta);
    const double half_cosine_value = std::cos(0.5 * theta);
    half_sine_scale = half_sine / omega_norm;
    half_cosine = half_cosine_value;
    a = 2.0 * half_sine * half_sine / (omega_norm * omega_norm);
    b = (theta - 2.0 * half_sine * half_cosine_value) /
      (omega_norm * omega_norm * omega_norm);
  }

  const Quaternion delta_rotation {{
      kappa[0] * half_sine_scale,
      kappa[1] * half_sine_scale,
      kappa[2] * half_sine_scale,
      half_cosine,
    }};
  const Vec3 omega_cross_e3 {{kappa[1], -kappa[0], 0.0}};
  const Vec3 omega_cross_omega_cross_e3 {{
      kappa[0] * kappa[2], kappa[1] * kappa[2],
      -(kappa[0] * kappa[0] + kappa[1] * kappa[1])}};
  const Vec3 local_translation {{
      b * omega_cross_omega_cross_e3[0] + a * omega_cross_e3[0],
      b * omega_cross_omega_cross_e3[1] + a * omega_cross_e3[1],
      ds + b * omega_cross_omega_cross_e3[2],
    }};
  const Vec3 world_translation = rotate(start.rotation, local_translation);
  next.position = {{
      start.position[0] + world_translation[0],
      start.position[1] + world_translation[1],
      start.position[2] + world_translation[2],
    }};
  next.rotation = multiply(start.rotation, delta_rotation);
  return next;
}

geometry_msgs::msg::Quaternion to_message(const Quaternion & rotation)
{
  geometry_msgs::msg::Quaternion result;
  result.x = rotation[0];
  result.y = rotation[1];
  result.z = rotation[2];
  result.w = rotation[3];
  return result;
}

}  // namespace

geometry_msgs::msg::PoseArray reconstruct_shape(
  const std::vector<double> & arc_lengths,
  const std::vector<double> & kappa_x,
  const std::vector<double> & kappa_y,
  const std::vector<double> & kappa_z,
  double needle_length_mm,
  const std::string & frame_id)
{
  geometry_msgs::msg::PoseArray pose_array;
  pose_array.header.frame_id = frame_id;

  if (
    arc_lengths.empty() ||
    kappa_x.size() != arc_lengths.size() ||
    kappa_y.size() != arc_lengths.size() ||
    kappa_z.size() != arc_lengths.size())
  {
    return pose_array;
  }
  if (!std::isfinite(needle_length_mm) || needle_length_mm <= 0.0 ||
    needle_length_mm < arc_lengths.back()) {
    return pose_array;
  }
  for (std::size_t i = 0; i < arc_lengths.size(); ++i) {
    if (!std::isfinite(arc_lengths[i]) || arc_lengths[i] < 0.0 ||
      (i > 0 && arc_lengths[i] <= arc_lengths[i - 1]) ||
      !std::isfinite(kappa_x[i]) || !std::isfinite(kappa_y[i]) ||
      !std::isfinite(kappa_z[i]))
    {
      return pose_array;
    }
  }

  PoseState state;
  double current_s = 0.0;
  pose_array.poses.reserve(arc_lengths.size() + 1U);
  geometry_msgs::msg::Pose base_pose;
  base_pose.orientation = to_message(state.rotation);
  pose_array.poses.push_back(base_pose);

  for (std::size_t i = 0; i < arc_lengths.size(); ++i) {
    const double target_s = i + 1U == arc_lengths.size() ? needle_length_mm :
      arc_lengths[i] + 0.5 * (arc_lengths[i + 1U] - arc_lengths[i]);
    const double segment_length = target_s - current_s;
    const Vec3 kappa {{kappa_x[i], kappa_y[i], kappa_z[i]}};
    state = integrate_segment(state, kappa, segment_length);
    geometry_msgs::msg::Pose pose;
    pose.position.x = state.position[0];
    pose.position.y = state.position[1];
    pose.position.z = state.position[2];
    pose.orientation = to_message(state.rotation);
    pose_array.poses.push_back(pose);
    current_s = target_s;
  }

  return pose_array;
}

geometry_msgs::msg::PoseArray reconstruct_shape(
  const CurvatureFrameData & frame,
  double needle_length_mm,
  const std::string & frame_id)
{
  return reconstruct_shape(
    frame.arc_lengths, frame.kappa_x, frame.kappa_y, frame.kappa_z,
    needle_length_mm, frame_id);
}

}  // namespace fbg_shape_pipeline_cpp
