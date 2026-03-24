#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fbg_shape_pipeline_cpp
{

// C++ representation of one parsed TCP frame.
//
// The goal here is clarity rather than maximum cleverness. Using standard
// library containers makes the code easy to debug, easy to inspect, and easy
// to convert into ROS 2 messages.
struct FbgFrameData
{
  std::uint8_t fiber_index {0U};
  std::uint16_t error {0U};
  std::uint64_t line_number {0U};
  double source_timestamp {0.0};

  std::vector<float> curvature;
  std::vector<float> angle;
  std::vector<float> temperature;

  // Some interrogator packets already include a shape vector. We preserve it
  // mainly for debugging and comparison against the ROS 2 reconstruction.
  std::vector<float> shape_points;
  std::uint32_t shape_width {0U};
  std::uint32_t shape_height {0U};

  struct SpectraCoreData
  {
    std::uint32_t channel {0U};
    double timestamp {0.0};
    std::uint64_t sample_number {0U};
    std::uint16_t error_status {0U};
    std::vector<std::uint32_t> spectrum_wavelengths;
    std::vector<std::uint16_t> spectrum_powers;
    std::vector<std::uint32_t> peaks_wavelengths;
    std::vector<std::uint16_t> peaks_powers;
    std::uint32_t spectrometer_temperature {0U};
  };

  std::vector<SpectraCoreData> spectra_cores;

  bool has_shape() const noexcept
  {
    return !shape_points.empty() && shape_width > 0U && shape_height > 0U;
  }
};

struct CurvatureFrameData
{
  std::uint64_t line_number {0U};
  double source_timestamp {0.0};
  std::vector<double> arc_lengths;
  std::vector<double> curvature;
  std::vector<double> angle;
  std::vector<double> kappa_x;
  std::vector<double> kappa_y;
  std::vector<double> kappa_z;
  std::vector<double> temperature;
};

struct ParseResult
{
  bool success {false};
  FbgFrameData frame;
  std::string error_message;
};

}  // namespace fbg_shape_pipeline_cpp
