#include "fbg_shape_pipeline_cpp/frame_parser.hpp"

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace fbg_shape_pipeline_cpp
{

namespace
{

template<typename T>
T read_scalar(const std::vector<std::uint8_t> & bytes, std::size_t offset)
{
  if (offset + sizeof(T) > bytes.size()) {
    throw std::runtime_error("Packet ended before expected scalar field.");
  }

  T value {};
  std::memcpy(&value, bytes.data() + offset, sizeof(T));
  return value;
}

template<typename T>
std::vector<T> read_vector(
  const std::vector<std::uint8_t> & bytes,
  std::size_t offset,
  std::size_t count)
{
  if (offset > bytes.size() || count > (bytes.size() - offset) / sizeof(T)) {
    throw std::runtime_error("Packet ended before expected array field.");
  }
  const std::size_t byte_count = sizeof(T) * count;

  std::vector<T> values(count);
  if (count > 0U) {
    std::memcpy(values.data(), bytes.data() + offset, byte_count);
  }
  return values;
}

void skip_bytes(
  const std::vector<std::uint8_t> & bytes,
  std::size_t & offset,
  std::size_t count)
{
  if (offset + count > bytes.size()) {
    throw std::runtime_error("Packet ended before expected skip.");
  }
  offset += count;
}

}  // namespace

ParseResult parse_frame_payload(const std::vector<std::uint8_t> & payload)
{
  ParseResult result;

  try {
    if (payload.empty()) {
      result.error_message = "Empty payload.";
      return result;
    }

    FbgFrameData frame;
    std::size_t offset = 0U;

    frame.fiber_index = read_scalar<std::uint8_t>(payload, offset);
    offset += sizeof(std::uint8_t);

    while (offset < payload.size()) {
      const auto field_length = read_scalar<std::int32_t>(payload, offset);
      offset += sizeof(std::int32_t);

      const auto field_id = read_scalar<std::uint16_t>(payload, offset);
      offset += sizeof(std::uint16_t);

      if (field_length < 2) {
        throw std::runtime_error("Invalid field length in packet.");
      }

      const std::size_t field_payload_length =
        static_cast<std::size_t>(field_length - 2);
      if (field_payload_length > payload.size() - offset) {
        throw std::runtime_error("Field exceeds packet boundary.");
      }
      const auto field_end = offset + field_payload_length;

      switch (field_id) {
        case 0:
          frame.error = read_scalar<std::uint16_t>(payload, offset);
          offset += sizeof(std::uint16_t);
          break;

        case 1:
          frame.line_number = read_scalar<std::uint64_t>(payload, offset);
          offset += sizeof(std::uint64_t);
          break;

        case 2:
          frame.source_timestamp = read_scalar<double>(payload, offset);
          offset += sizeof(double);
          break;

        case 3:
        {
          const auto array_length = read_scalar<std::uint32_t>(payload, offset);
          offset += sizeof(std::uint32_t);
          frame.curvature = read_vector<float>(payload, offset, array_length);
          offset += sizeof(float) * array_length;
          break;
        }

        case 4:
        {
          const auto array_length = read_scalar<std::uint32_t>(payload, offset);
          offset += sizeof(std::uint32_t);
          frame.angle = read_vector<float>(payload, offset, array_length);
          offset += sizeof(float) * array_length;
          break;
        }

        case 5:
        {
          frame.shape_width = read_scalar<std::uint32_t>(payload, offset);
          offset += sizeof(std::uint32_t);
          frame.shape_height = read_scalar<std::uint32_t>(payload, offset);
          offset += sizeof(std::uint32_t);

          const std::size_t point_count =
            static_cast<std::size_t>(frame.shape_width) *
            static_cast<std::size_t>(frame.shape_height);

          frame.shape_points = read_vector<float>(payload, offset, point_count);
          offset += sizeof(float) * point_count;
          break;
        }

        case 6:
        {
          const auto array_length = read_scalar<std::uint32_t>(payload, offset);
          offset += sizeof(std::uint32_t);
          frame.temperature = read_vector<float>(payload, offset, array_length);
          offset += sizeof(float) * array_length;
          break;
        }

        case 7:
        {
          const std::size_t spectra_field_end = offset + field_payload_length;
          frame.spectra_cores.clear();

          while (offset < spectra_field_end) {
            FbgFrameData::SpectraCoreData core_data;

            const auto block_length = read_scalar<std::uint32_t>(payload, offset);
            if (block_length < 5U || block_length > spectra_field_end - offset) {
              throw std::runtime_error("Invalid spectra block boundary.");
            }
            const std::size_t block_end = offset + block_length;
            offset += sizeof(std::uint32_t);

            core_data.channel = read_scalar<std::uint8_t>(payload, offset);
            offset += sizeof(std::uint8_t);

            while (offset < block_end) {
              const auto subfield_length = read_scalar<std::int32_t>(payload, offset);
              offset += sizeof(std::int32_t);

              const auto subfield_id = read_scalar<std::uint16_t>(payload, offset);
              offset += sizeof(std::uint16_t);

              if (subfield_length < 2) {
                throw std::runtime_error("Invalid spectra subfield length in packet.");
              }

              const std::size_t subfield_payload_length =
                static_cast<std::size_t>(subfield_length - 2);
              if (offset > block_end || subfield_payload_length > block_end - offset) {
                throw std::runtime_error("Spectra subfield exceeds block boundary.");
              }
              const auto subfield_end = offset + subfield_payload_length;

              switch (subfield_id) {
                case 0:
                  core_data.timestamp = read_scalar<double>(payload, offset);
                  offset += sizeof(double);
                  break;

                case 1:
                  core_data.sample_number = read_scalar<std::uint64_t>(payload, offset);
                  offset += sizeof(std::uint64_t);
                  break;

                case 2:
                  core_data.error_status = read_scalar<std::uint16_t>(payload, offset);
                  offset += sizeof(std::uint16_t);
                  break;

                case 3:
                {
                  const auto count = read_scalar<std::uint32_t>(payload, offset);
                  offset += sizeof(std::uint32_t);
                  core_data.spectrum_wavelengths =
                    read_vector<std::uint32_t>(payload, offset, count);
                  offset += sizeof(std::uint32_t) * count;
                  break;
                }

                case 4:
                {
                  const auto count = read_scalar<std::uint32_t>(payload, offset);
                  offset += sizeof(std::uint32_t);
                  core_data.spectrum_powers =
                    read_vector<std::uint16_t>(payload, offset, count);
                  offset += sizeof(std::uint16_t) * count;
                  break;
                }

                case 5:
                {
                  const auto count = read_scalar<std::uint32_t>(payload, offset);
                  offset += sizeof(std::uint32_t);
                  core_data.peaks_wavelengths =
                    read_vector<std::uint32_t>(payload, offset, count);
                  offset += sizeof(std::uint32_t) * count;
                  break;
                }

                case 6:
                {
                  const auto count = read_scalar<std::uint32_t>(payload, offset);
                  offset += sizeof(std::uint32_t);
                  core_data.peaks_powers =
                    read_vector<std::uint16_t>(payload, offset, count);
                  offset += sizeof(std::uint16_t) * count;
                  break;
                }

                case 7:
                  core_data.spectrometer_temperature =
                    read_scalar<std::uint32_t>(payload, offset);
                  offset += sizeof(std::uint32_t);
                  break;

                default:
                  skip_bytes(payload, offset, subfield_payload_length);
                  break;
              }
              if (offset != subfield_end) {
                throw std::runtime_error("Spectra subfield length does not match content.");
              }
            }

            frame.spectra_cores.push_back(std::move(core_data));
          }

          if (offset != spectra_field_end) {
            throw std::runtime_error("Spectra field parsing ended at unexpected offset.");
          }
          break;
        }

        default:
        {
          std::ostringstream stream;
          stream << "Unknown field id: " << field_id;
          throw std::runtime_error(stream.str());
        }
      }
      if (offset != field_end) {
        throw std::runtime_error("Field length does not match content.");
      }
    }

    result.success = true;
    result.frame = std::move(frame);
    return result;
  } catch (const std::exception & ex) {
    result.error_message = ex.what();
    return result;
  }
}

}  // namespace fbg_shape_pipeline_cpp
