#pragma once

#include "render/image/image_decoder.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {
namespace detail {

inline unsigned char bmp_byte_to_uchar(std::byte value)
{
    return std::to_integer<unsigned char>(value);
}

inline std::uint16_t bmp_read_u16_le(const std::vector<std::byte>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bmp_byte_to_uchar(bytes[offset]))
        | (static_cast<std::uint16_t>(bmp_byte_to_uchar(bytes[offset + 1])) << 8));
}

inline std::uint32_t bmp_read_u32_le(const std::vector<std::byte>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bmp_byte_to_uchar(bytes[offset]))
        | (static_cast<std::uint32_t>(bmp_byte_to_uchar(bytes[offset + 1])) << 8)
        | (static_cast<std::uint32_t>(bmp_byte_to_uchar(bytes[offset + 2])) << 16)
        | (static_cast<std::uint32_t>(bmp_byte_to_uchar(bytes[offset + 3])) << 24);
}

inline std::int32_t bmp_read_i32_le(const std::vector<std::byte>& bytes, std::size_t offset)
{
    const std::uint32_t value = bmp_read_u32_le(bytes, offset);
    if (value <= 0x7fffffffu) {
        return static_cast<std::int32_t>(value);
    }
    if (value == 0x80000000u) {
        return std::numeric_limits<std::int32_t>::min();
    }

    const std::uint32_t magnitude = (~value + 1u) & 0x7fffffffu;
    return -static_cast<std::int32_t>(magnitude);
}

inline bool bmp_checked_mul(std::size_t left, std::size_t right, std::size_t& result)
{
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (left != 0 && right > max_size / left) {
        return false;
    }

    result = left * right;
    return true;
}

struct bmp_info_header {
    std::size_t pixel_data_offset = 0;
    std::size_t width = 0;
    std::size_t height = 0;
    bool top_down = false;
    std::uint16_t bits_per_pixel = 0;
    std::size_t source_bytes_per_pixel = 0;
    std::size_t row_stride_byte_count = 0;
    std::size_t required_pixel_byte_count = 0;
    std::size_t rgba_byte_count = 0;
};

struct bmp_info_header_parse_result {
    bool ok = false;
    render_image_decode_status status = render_image_decode_status::invalid_data;
    bmp_info_header header;
    std::string diagnostic;
};

inline bmp_info_header_parse_result parse_bmp_info_header(const std::vector<std::byte>& bytes)
{
    constexpr std::size_t file_header_byte_count = 14;
    constexpr std::size_t bitmap_info_header_byte_count = 40;
    constexpr std::uint32_t bi_rgb_compression = 0;

    if (bytes.size() < 2 || bmp_byte_to_uchar(bytes[0]) != 'B' || bmp_byte_to_uchar(bytes[1]) != 'M') {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::unsupported_format,
            .header = {},
            .diagnostic = "bmp image decoder requires BM file signature",
        };
    }

    if (bytes.size() < file_header_byte_count + bitmap_info_header_byte_count) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder requires a complete BMP file and BITMAPINFOHEADER",
        };
    }

    const std::size_t pixel_data_offset = static_cast<std::size_t>(bmp_read_u32_le(bytes, 10));
    const std::uint32_t dib_header_byte_count = bmp_read_u32_le(bytes, file_header_byte_count);
    if (dib_header_byte_count != bitmap_info_header_byte_count) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::unsupported_format,
            .header = {},
            .diagnostic = "bmp image decoder only supports BITMAPINFOHEADER size 40",
        };
    }

    if (pixel_data_offset < file_header_byte_count + bitmap_info_header_byte_count
        || pixel_data_offset > bytes.size()) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder pixel data offset is outside encoded bytes",
        };
    }

    const std::int32_t signed_width = bmp_read_i32_le(bytes, 18);
    const std::int32_t signed_height = bmp_read_i32_le(bytes, 22);
    if (signed_width <= 0) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder requires a positive image width",
        };
    }
    if (signed_height == 0 || signed_height == std::numeric_limits<std::int32_t>::min()) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder requires a nonzero representable image height",
        };
    }

    const bool top_down = signed_height < 0;
    const std::size_t width = static_cast<std::size_t>(signed_width);
    const std::size_t height = top_down
        ? static_cast<std::size_t>(-signed_height)
        : static_cast<std::size_t>(signed_height);

    const std::uint16_t planes = bmp_read_u16_le(bytes, 26);
    if (planes != 1) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder requires one color plane",
        };
    }

    const std::uint16_t bits_per_pixel = bmp_read_u16_le(bytes, 28);
    if (bits_per_pixel != 24 && bits_per_pixel != 32) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::unsupported_format,
            .header = {},
            .diagnostic = "bmp image decoder only supports 24-bit BGR and 32-bit BGRA pixels",
        };
    }

    const std::uint32_t compression = bmp_read_u32_le(bytes, 30);
    if (compression != bi_rgb_compression) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::unsupported_format,
            .header = {},
            .diagnostic = "bmp image decoder only supports uncompressed BI_RGB pixel data",
        };
    }

    const std::size_t source_bytes_per_pixel = static_cast<std::size_t>(bits_per_pixel / 8);
    std::size_t unpadded_row_byte_count = 0;
    if (!bmp_checked_mul(width, source_bytes_per_pixel, unpadded_row_byte_count)) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder row byte count overflows",
        };
    }

    const std::size_t row_padding_byte_count = (4 - (unpadded_row_byte_count % 4)) % 4;
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (unpadded_row_byte_count > max_size - row_padding_byte_count) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder padded row byte count overflows",
        };
    }
    const std::size_t row_stride_byte_count = unpadded_row_byte_count + row_padding_byte_count;

    std::size_t required_pixel_byte_count = 0;
    if (!bmp_checked_mul(row_stride_byte_count, height, required_pixel_byte_count)) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder pixel buffer byte count overflows",
        };
    }

    if (bytes.size() - pixel_data_offset < required_pixel_byte_count) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder pixel data is truncated for the declared dimensions",
        };
    }

    std::size_t pixel_count = 0;
    if (!bmp_checked_mul(width, height, pixel_count)) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder dimensions overflow the pixel count",
        };
    }

    std::size_t rgba_byte_count = 0;
    if (!bmp_checked_mul(pixel_count, std::size_t{4}, rgba_byte_count)) {
        return bmp_info_header_parse_result{
            .ok = false,
            .status = render_image_decode_status::invalid_data,
            .header = {},
            .diagnostic = "bmp image decoder dimensions overflow the RGBA byte count",
        };
    }

    return bmp_info_header_parse_result{
        .ok = true,
        .status = render_image_decode_status::decoded,
        .header = bmp_info_header{
            .pixel_data_offset = pixel_data_offset,
            .width = width,
            .height = height,
            .top_down = top_down,
            .bits_per_pixel = bits_per_pixel,
            .source_bytes_per_pixel = source_bytes_per_pixel,
            .row_stride_byte_count = row_stride_byte_count,
            .required_pixel_byte_count = required_pixel_byte_count,
            .rgba_byte_count = rgba_byte_count,
        },
        .diagnostic = {},
    };
}

} // namespace detail

inline bool bmp_image_decoder_supports_source(const render_resolved_image_source& source)
{
    return !source.cache_key().empty() && image_decode_extension_hint(source.normalized_uri) == "bmp";
}

inline bool bmp_image_decoder_supports_request(const render_image_decode_request& request)
{
    return bmp_image_decoder_supports_source(request.source)
        || detect_render_image_format(request).detected_format == render_image_encoded_format::bmp;
}

class bmp_image_decoder final : public image_decoder_interface {
public:
    bool supports(const render_image_decode_request& request) const override
    {
        return bmp_image_decoder_supports_request(request);
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        if (request.encoded_bytes.empty()) {
            return render_image_decode_result{
                .status = render_image_decode_status::empty_input,
                .image = {},
                .diagnostic = "bmp image decoder requires encoded bytes",
                .metadata = make_render_image_decode_metadata("bmp_image_decoder", request),
            };
        }

        if (!bmp_image_decoder_supports_request(request)) {
            return render_image_decode_result{
                .status = render_image_decode_status::unsupported_format,
                .image = {},
                .diagnostic = "bmp image decoder only supports .bmp sources or BMP signature bytes",
                .metadata = make_render_image_decode_metadata("bmp_image_decoder", request),
            };
        }

        const detail::bmp_info_header_parse_result parsed =
            detail::parse_bmp_info_header(request.encoded_bytes);
        if (!parsed.ok) {
            return render_image_decode_result{
                .status = parsed.status,
                .image = {},
                .diagnostic = parsed.diagnostic,
                .metadata = make_render_image_decode_metadata("bmp_image_decoder", request),
            };
        }

        const detail::bmp_info_header& header = parsed.header;
        std::vector<std::byte> pixels(header.rgba_byte_count);
        for (std::size_t output_y = 0; output_y < header.height; ++output_y) {
            const std::size_t source_y = header.top_down ? output_y : header.height - 1 - output_y;
            const std::size_t source_row_offset =
                header.pixel_data_offset + source_y * header.row_stride_byte_count;
            const std::size_t output_row_offset = output_y * header.width * 4;

            for (std::size_t x = 0; x < header.width; ++x) {
                const std::size_t source_pixel_offset =
                    source_row_offset + x * header.source_bytes_per_pixel;
                const std::size_t output_pixel_offset = output_row_offset + x * 4;

                pixels[output_pixel_offset] = request.encoded_bytes[source_pixel_offset + 2];
                pixels[output_pixel_offset + 1] = request.encoded_bytes[source_pixel_offset + 1];
                pixels[output_pixel_offset + 2] = request.encoded_bytes[source_pixel_offset];
                pixels[output_pixel_offset + 3] = header.bits_per_pixel == 32
                    ? request.encoded_bytes[source_pixel_offset + 3]
                    : std::byte{0xff};
            }
        }

        render_decoded_image image{
            .width = header.width,
            .height = header.height,
            .pixel_format = render_image_pixel_format::rgba8_srgb,
            .pixels = std::move(pixels),
        };
        render_image_decode_metadata metadata = make_render_image_decode_metadata(
            "bmp_image_decoder",
            request,
            image);

        return render_image_decode_result{
            .status = render_image_decode_status::decoded,
            .image = std::move(image),
            .diagnostic = {},
            .metadata = std::move(metadata),
        };
    }
};

} // namespace quiz_vulkan::render
