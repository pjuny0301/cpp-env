#pragma once

#include "render/image/image_types.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class png_image_header_inspect_status {
    valid,
    missing_signature,
    truncated_ihdr,
    invalid_ihdr,
    invalid_dimensions,
    unsupported_color_type,
    unsupported_bit_depth,
};

inline std::string png_image_header_inspect_status_name(png_image_header_inspect_status status)
{
    switch (status) {
    case png_image_header_inspect_status::valid:
        return "valid";
    case png_image_header_inspect_status::missing_signature:
        return "missing_signature";
    case png_image_header_inspect_status::truncated_ihdr:
        return "truncated_ihdr";
    case png_image_header_inspect_status::invalid_ihdr:
        return "invalid_ihdr";
    case png_image_header_inspect_status::invalid_dimensions:
        return "invalid_dimensions";
    case png_image_header_inspect_status::unsupported_color_type:
        return "unsupported_color_type";
    case png_image_header_inspect_status::unsupported_bit_depth:
        return "unsupported_bit_depth";
    }

    return "unknown";
}

inline std::string png_image_color_type_name(std::uint8_t color_type)
{
    switch (color_type) {
    case 0:
        return "grayscale";
    case 2:
        return "truecolor";
    case 3:
        return "indexed_color";
    case 4:
        return "grayscale_alpha";
    case 6:
        return "truecolor_alpha";
    }

    return "unknown";
}

struct png_image_header {
    std::size_t width = 0;
    std::size_t height = 0;
    std::uint8_t bit_depth = 0;
    std::uint8_t color_type = 0;
    std::uint8_t compression_method = 0;
    std::uint8_t filter_method = 0;
    std::uint8_t interlace_method = 0;
    std::size_t pixel_count = 0;
    std::size_t decoded_rgba_byte_count = 0;
    render_image_pixel_format decoded_pixel_format = render_image_pixel_format::rgba8_srgb;
    bool rgba8_supported = false;
};

struct png_image_header_inspect_result {
    png_image_header_inspect_status status = png_image_header_inspect_status::missing_signature;
    png_image_header header;
    bool signature_valid = false;
    bool ihdr_present = false;
    std::string diagnostic;

    bool ok() const
    {
        return status == png_image_header_inspect_status::valid;
    }
};

inline bool starts_with_png_signature(const std::vector<std::byte>& bytes)
{
    constexpr unsigned char signature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    constexpr std::size_t signature_size = sizeof(signature) / sizeof(signature[0]);
    if (bytes.size() < signature_size) {
        return false;
    }
    for (std::size_t index = 0; index < signature_size; ++index) {
        if (std::to_integer<unsigned char>(bytes[index]) != signature[index]) {
            return false;
        }
    }
    return true;
}

namespace detail {

inline std::uint32_t png_read_u32_be(const std::vector<std::byte>& bytes, std::size_t offset)
{
    return (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset])) << 24)
        | (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 1])) << 16)
        | (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 2])) << 8)
        | static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 3]));
}

inline bool png_checked_mul(std::size_t left, std::size_t right, std::size_t& result)
{
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (left != 0 && right > max_size / left) {
        return false;
    }

    result = left * right;
    return true;
}

inline png_image_header_inspect_result make_png_image_header_inspect_failure(
    png_image_header_inspect_status status,
    png_image_header header,
    bool signature_valid,
    bool ihdr_present,
    std::string diagnostic)
{
    return png_image_header_inspect_result{
        .status = status,
        .header = header,
        .signature_valid = signature_valid,
        .ihdr_present = ihdr_present,
        .diagnostic = std::move(diagnostic),
    };
}

} // namespace detail

inline png_image_header_inspect_result inspect_png_image_header(const std::vector<std::byte>& bytes)
{
    constexpr std::size_t png_signature_byte_count = 8;
    constexpr std::size_t png_chunk_prefix_byte_count = 8;
    constexpr std::size_t png_ihdr_data_byte_count = 13;
    constexpr std::size_t png_chunk_crc_byte_count = 4;
    constexpr std::size_t png_complete_ihdr_byte_count =
        png_signature_byte_count + png_chunk_prefix_byte_count + png_ihdr_data_byte_count + png_chunk_crc_byte_count;

    if (!starts_with_png_signature(bytes)) {
        return detail::make_png_image_header_inspect_failure(
            png_image_header_inspect_status::missing_signature,
            {},
            false,
            false,
            "png image header inspector requires the PNG file signature");
    }

    if (bytes.size() < png_complete_ihdr_byte_count) {
        return detail::make_png_image_header_inspect_failure(
            png_image_header_inspect_status::truncated_ihdr,
            {},
            true,
            false,
            "png image header inspector requires a complete IHDR chunk");
    }

    constexpr std::uint32_t expected_ihdr_data_byte_count = 13;
    const std::uint32_t ihdr_data_byte_count = detail::png_read_u32_be(bytes, png_signature_byte_count);
    const bool ihdr_chunk_type_matches =
        std::to_integer<unsigned char>(bytes[12]) == 'I'
        && std::to_integer<unsigned char>(bytes[13]) == 'H'
        && std::to_integer<unsigned char>(bytes[14]) == 'D'
        && std::to_integer<unsigned char>(bytes[15]) == 'R';
    if (ihdr_data_byte_count != expected_ihdr_data_byte_count || !ihdr_chunk_type_matches) {
        return detail::make_png_image_header_inspect_failure(
            png_image_header_inspect_status::invalid_ihdr,
            {},
            true,
            false,
            "png image header inspector requires the first chunk to be IHDR with length 13");
    }

    png_image_header header{
        .width = static_cast<std::size_t>(detail::png_read_u32_be(bytes, 16)),
        .height = static_cast<std::size_t>(detail::png_read_u32_be(bytes, 20)),
        .bit_depth = std::to_integer<std::uint8_t>(bytes[24]),
        .color_type = std::to_integer<std::uint8_t>(bytes[25]),
        .compression_method = std::to_integer<std::uint8_t>(bytes[26]),
        .filter_method = std::to_integer<std::uint8_t>(bytes[27]),
        .interlace_method = std::to_integer<std::uint8_t>(bytes[28]),
        .pixel_count = 0,
        .decoded_rgba_byte_count = 0,
        .decoded_pixel_format = render_image_pixel_format::rgba8_srgb,
        .rgba8_supported = false,
    };

    constexpr std::size_t png_max_dimension = 0x7fffffffu;
    if (header.width == 0 || header.height == 0
        || header.width > png_max_dimension || header.height > png_max_dimension) {
        return detail::make_png_image_header_inspect_failure(
            png_image_header_inspect_status::invalid_dimensions,
            header,
            true,
            true,
            "png image header inspector requires nonzero dimensions no larger than 2^31-1");
    }

    if (!detail::png_checked_mul(header.width, header.height, header.pixel_count)
        || !detail::png_checked_mul(header.pixel_count, std::size_t{4}, header.decoded_rgba_byte_count)) {
        return detail::make_png_image_header_inspect_failure(
            png_image_header_inspect_status::invalid_dimensions,
            header,
            true,
            true,
            "png image header inspector dimensions overflow RGBA byte count");
    }

    if (header.compression_method != 0 || header.filter_method != 0 || header.interlace_method > 1) {
        return detail::make_png_image_header_inspect_failure(
            png_image_header_inspect_status::invalid_ihdr,
            header,
            true,
            true,
            "png image header inspector requires compression 0, filter 0, and interlace 0 or 1");
    }

    if (header.color_type != 6) {
        return detail::make_png_image_header_inspect_failure(
            png_image_header_inspect_status::unsupported_color_type,
            header,
            true,
            true,
            "png image header inspector currently supports only truecolor alpha PNG color type 6");
    }

    if (header.bit_depth != 8) {
        return detail::make_png_image_header_inspect_failure(
            png_image_header_inspect_status::unsupported_bit_depth,
            header,
            true,
            true,
            "png image header inspector currently supports only 8-bit RGBA PNG headers");
    }

    header.rgba8_supported = true;
    return png_image_header_inspect_result{
        .status = png_image_header_inspect_status::valid,
        .header = header,
        .signature_valid = true,
        .ihdr_present = true,
        .diagnostic = "png image header inspector parsed an 8-bit RGBA PNG IHDR",
    };
}

} // namespace quiz_vulkan::render
