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
using Mat3 = std::array<std::array<double, 3>, 3>;

struct PoseState
{
  Mat3 rotation {{
      {{1.0, 0.0, 0.0}},
      {{0.0, 1.0, 0.0}},
      {{0.0, 0.0, 1.0}},
    }};
  Vec3 position {{0.0, 0.0, 0.0}};
};

Mat3 identity_matrix()
{
  return {{
      {{1.0, 0.0, 0.0}},
      {{0.0, 1.0, 0.0}},
      {{0.0, 0.0, 1.0}},
    }};
}

Mat3 skew(const Vec3 & v)
{
  return {{
      {{0.0, -v[2], v[1]}},
      {{v[2], 0.0, -v[0]}},
      {{-v[1], v[0], 0.0}},
    }};
}

Mat3 add(const Mat3 & a, const Mat3 & b)
{
  Mat3 out {};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      out[r][c] = a[r][c] + b[r][c];
    }
  }
  return out;
}

Mat3 scale(const Mat3 & a, double s)
{
  Mat3 out {};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      out[r][c] = a[r][c] * s;
    }
  }
  return out;
}

Mat3 multiply(const Mat3 & a, const Mat3 & b)
{
  Mat3 out {};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      for (int k = 0; k < 3; ++k) {
        out[r][c] += a[r][k] * b[k][c];
      }
    }
  }
  return out;
}

Vec3 multiply(const Mat3 & a, const Vec3 & v)
{
  Vec3 out {{0.0, 0.0, 0.0}};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      out[r] += a[r][c] * v[c];
    }
  }
  return out;
}

Vec3 add(const Vec3 & a, const Vec3 & b)
{
  return {{a[0] + b[0], a[1] + b[1], a[2] + b[2]}};
}

Vec3 scale(const Vec3 & v, double s)
{
  return {{v[0] * s, v[1] * s, v[2] * s}};
}

geometry_msgs::msg::Quaternion quaternion_from_rotation(const Mat3 & r)
{
  geometry_msgs::msg::Quaternion q;
  const double trace = r[0][0] + r[1][1] + r[2][2];

  if (trace > 0.0) {
    const double s = std::sqrt(trace + 1.0) * 2.0;
    q.w = 0.25 * s;
    q.x = (r[2][1] - r[1][2]) / s;
    q.y = (r[0][2] - r[2][0]) / s;
    q.z = (r[1][0] - r[0][1]) / s;
  } else if (r[0][0] > r[1][1] && r[0][0] > r[2][2]) {
    const double s = std::sqrt(1.0 + r[0][0] - r[1][1] - r[2][2]) * 2.0;
    q.w = (r[2][1] - r[1][2]) / s;
    q.x = 0.25 * s;
    q.y = (r[0][1] + r[1][0]) / s;
    q.z = (r[0][2] + r[2][0]) / s;
  } else if (r[1][1] > r[2][2]) {
    const double s = std::sqrt(1.0 + r[1][1] - r[0][0] - r[2][2]) * 2.0;
    q.w = (r[0][2] - r[2][0]) / s;
    q.x = (r[0][1] + r[1][0]) / s;
    q.y = 0.25 * s;
    q.z = (r[1][2] + r[2][1]) / s;
  } else {
    const double s = std::sqrt(1.0 + r[2][2] - r[0][0] - r[1][1]) * 2.0;
    q.w = (r[1][0] - r[0][1]) / s;
    q.x = (r[0][2] + r[2][0]) / s;
    q.y = (r[1][2] + r[2][1]) / s;
    q.z = 0.25 * s;
  }

  return q;
}

std::vector<double> build_output_positions(
  const std::vector<double> & arc_lengths,
  double needle_length_m,
  double output_spacing_m)
{
  std::vector<double> positions;
  positions.push_back(0.0);

  const double measured_end = arc_lengths.empty() ? 0.0 : arc_lengths.back();
  const double end_s = std::max(needle_length_m, measured_end);
  if (output_spacing_m > 0.0) {
    for (double s = output_spacing_m; s < end_s; s += output_spacing_m) {
      positions.push_back(s);
    }
  } else {
    for (const auto value : arc_lengths) {
      if (value > positions.back()) {
        positions.push_back(value);
      }
    }
  }

  if (end_s > positions.back()) {
    positions.push_back(end_s);
  }

  return positions;
}

double interpolate_scalar(
  const std::vector<double> & arc_lengths,
  const std::vector<double> & values,
  double s)
{
  if (values.empty()) {
    return 0.0;
  }
  if (arc_lengths.empty() || values.size() != arc_lengths.size()) {
    throw std::runtime_error("Arc length and value vectors must be the same size.");
  }
  if (s <= arc_lengths.front()) {
    return values.front();
  }
  if (s >= arc_lengths.back()) {
    return values.back();
  }

  for (std::size_t i = 0; i + 1 < arc_lengths.size(); ++i) {
    if (s >= arc_lengths[i] && s <= arc_lengths[i + 1]) {
      const double ds = arc_lengths[i + 1] - arc_lengths[i];
      const double t = ds > 0.0 ? (s - arc_lengths[i]) / ds : 0.0;
      return (1.0 - t) * values[i] + t * values[i + 1];
    }
  }

  return values.back();
}

PoseState integrate_segment(
  const PoseState & start,
  const Vec3 & kappa,
  double ds)
{
  PoseState next = start;
  const Vec3 v {{0.0, 0.0, 1.0}};
  const Mat3 omega_hat = skew(kappa);
  const Mat3 omega_hat_sq = multiply(omega_hat, omega_hat);
  const double omega_norm = std::sqrt(
    kappa[0] * kappa[0] + kappa[1] * kappa[1] + kappa[2] * kappa[2]);
  const double theta = omega_norm * ds;

  Mat3 rotation_increment = identity_matrix();
  Mat3 v_matrix = scale(identity_matrix(), ds);

  if (theta < 1e-9) {
    rotation_increment = add(rotation_increment, scale(omega_hat, ds));
    rotation_increment = add(rotation_increment, scale(omega_hat_sq, 0.5 * ds * ds));

    v_matrix = add(v_matrix, scale(omega_hat, 0.5 * ds * ds));
    v_matrix = add(v_matrix, scale(omega_hat_sq, (ds * ds * ds) / 6.0));
  } else {
    rotation_increment = add(rotation_increment, scale(omega_hat, std::sin(theta) / omega_norm));
    rotation_increment = add(
      rotation_increment,
      scale(omega_hat_sq, (1.0 - std::cos(theta)) / (omega_norm * omega_norm)));

    v_matrix = add(
      v_matrix,
      scale(omega_hat, (1.0 - std::cos(theta)) / (omega_norm * omega_norm)));
    v_matrix = add(
      v_matrix,
      scale(
        omega_hat_sq,
        (theta - std::sin(theta)) / (omega_norm * omega_norm * omega_norm)));
  }

  next.rotation = multiply(start.rotation, rotation_increment);
  next.position = add(start.position, multiply(start.rotation, multiply(v_matrix, v)));
  return next;
}

}  // namespace

geometry_msgs::msg::PoseArray reconstruct_shape(
  const CurvatureFrameData & frame,
  double needle_length_m,
  double output_spacing_m,
  int segment_substeps,
  const std::string & frame_id)
{
  geometry_msgs::msg::PoseArray pose_array;
  pose_array.header.frame_id = frame_id;

  if (
    frame.arc_lengths.empty() ||
    frame.kappa_x.size() != frame.arc_lengths.size() ||
    frame.kappa_y.size() != frame.arc_lengths.size() ||
    frame.kappa_z.size() != frame.arc_lengths.size())
  {
    return pose_array;
  }

  segment_substeps = std::max(1, segment_substeps);
  const auto output_positions = build_output_positions(
    frame.arc_lengths, needle_length_m, output_spacing_m);

  PoseState state;
  double current_s = 0.0;

  for (const auto target_s : output_positions) {
    const double segment_length = target_s - current_s;
    if (segment_length > 0.0) {
      const double ds = segment_length / static_cast<double>(segment_substeps);
      for (int step = 0; step < segment_substeps; ++step) {
        const double sample_s = current_s + (step + 0.5) * ds;
        const Vec3 kappa {{
            interpolate_scalar(frame.arc_lengths, frame.kappa_x, sample_s),
            interpolate_scalar(frame.arc_lengths, frame.kappa_y, sample_s),
            interpolate_scalar(frame.arc_lengths, frame.kappa_z, sample_s),
          }};
        state = integrate_segment(state, kappa, ds);
      }
    }

    geometry_msgs::msg::Pose pose;
    pose.position.x = state.position[0];
    pose.position.y = state.position[1];
    pose.position.z = state.position[2];
    pose.orientation = quaternion_from_rotation(state.rotation);
    pose_array.poses.push_back(pose);
    current_s = target_s;
  }

  return pose_array;
}

}  // namespace fbg_shape_pipeline_cpp
