#pragma once

#include <cstdint>
#include <vector>

#include "fbg_shape_pipeline_cpp/frame_types.hpp"

namespace fbg_shape_pipeline_cpp
{

// Parse a single interrogator payload.
//
// Input:
//   bytes after the 4-byte packet length prefix.
//
// Output:
//   a ParseResult that is either successful or contains an explanation.
ParseResult parse_frame_payload(const std::vector<std::uint8_t> & payload);

}  // namespace fbg_shape_pipeline_cpp
