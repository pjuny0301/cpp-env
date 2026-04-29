#pragma once

#include "render/image/image_types.h"

#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

struct render_image_decode_request {
    render_resolved_image_source source;
    std::vector<std::byte> encoded_bytes;
};

enum class render_image_decode_status {
    decoded,
    empty_input,
    unsupported_format,
    invalid_data,
};

struct render_image_decode_result {
    render_image_decode_status status = render_image_decode_status::empty_input;
    render_decoded_image image;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_decode_status::decoded;
    }
};

class image_decoder_interface {
public:
    virtual ~image_decoder_interface() = default;

    virtual bool supports(const render_image_decode_request& request) const = 0;
    virtual render_image_decode_result decode(const render_image_decode_request& request) const = 0;
};

namespace detail {

inline unsigned char ppm_byte_to_uchar(std::byte value)
{
    return std::to_integer<unsigned char>(value);
}

inline bool ppm_is_ascii_space(unsigned char value)
{
    switch (value) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
    case '\f':
    case '\v':
        return true;
    default:
        return false;
    }
}

inline bool ppm_is_ascii_digit(unsigned char value)
{
    return value >= '0' && value <= '9';
}

inline void ppm_skip_whitespace_and_comments(const std::vector<std::byte>& bytes, std::size_t& offset)
{
    while (offset < bytes.size()) {
        const unsigned char value = ppm_byte_to_uchar(bytes[offset]);
        if (ppm_is_ascii_space(value)) {
            ++offset;
            continue;
        }

        if (value != '#') {
            return;
        }

        while (offset < bytes.size() && ppm_byte_to_uchar(bytes[offset]) != '\n') {
            ++offset;
        }
    }
}

inline bool ppm_parse_positive_integer(
    const std::vector<std::byte>& bytes,
    std::size_t& offset,
    std::size_t& parsed)
{
    ppm_skip_whitespace_and_comments(bytes, offset);
    if (offset >= bytes.size() || !ppm_is_ascii_digit(ppm_byte_to_uchar(bytes[offset]))) {
        return false;
    }

    std::size_t value = 0;
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    while (offset < bytes.size()) {
        const unsigned char current = ppm_byte_to_uchar(bytes[offset]);
        if (!ppm_is_ascii_digit(current)) {
            break;
        }

        const std::size_t digit = static_cast<std::size_t>(current - '0');
        if (value > (max_size - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
        ++offset;
    }

    if (value == 0) {
        return false;
    }

    parsed = value;
    return true;
}

inline bool ppm_checked_pixel_count(std::size_t width, std::size_t height, std::size_t& pixel_count)
{
    if (width == 0 || height == 0) {
        return false;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (width > max_size / height) {
        return false;
    }

    pixel_count = width * height;
    return true;
}

} // namespace detail

inline bool fake_image_decoder_supports_source(const render_resolved_image_source& source)
{
    return !source.cache_key().empty() && source.normalized_uri.ends_with(".fake");
}

inline bool ppm_image_decoder_supports_source(const render_resolved_image_source& source)
{
    return !source.cache_key().empty()
        && (source.normalized_uri.ends_with(".ppm") || source.normalized_uri.ends_with(".pnm"));
}

class ppm_image_decoder final : public image_decoder_interface {
public:
    bool supports(const render_image_decode_request& request) const override
    {
        return ppm_image_decoder_supports_source(request.source);
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        if (request.encoded_bytes.empty()) {
            return render_image_decode_result{
                .status = render_image_decode_status::empty_input,
                .image = {},
                .diagnostic = "ppm image decoder requires encoded bytes",
            };
        }

        if (!ppm_image_decoder_supports_source(request.source)) {
            return render_image_decode_result{
                .status = render_image_decode_status::unsupported_format,
                .image = {},
                .diagnostic = "ppm image decoder only supports .ppm or .pnm image sources",
            };
        }

        const std::vector<std::byte>& bytes = request.encoded_bytes;
        if (bytes.size() < 3 || detail::ppm_byte_to_uchar(bytes[0]) != 'P'
            || detail::ppm_byte_to_uchar(bytes[1]) != '6') {
            return render_image_decode_result{
                .status = render_image_decode_status::unsupported_format,
                .image = {},
                .diagnostic = "ppm image decoder requires binary P6 magic",
            };
        }

        if (!detail::ppm_is_ascii_space(detail::ppm_byte_to_uchar(bytes[2]))) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image header must delimit the P6 magic with whitespace",
            };
        }

        std::size_t offset = 2;
        std::size_t width = 0;
        std::size_t height = 0;
        std::size_t max_value = 0;
        if (!detail::ppm_parse_positive_integer(bytes, offset, width)
            || !detail::ppm_parse_positive_integer(bytes, offset, height)
            || !detail::ppm_parse_positive_integer(bytes, offset, max_value)) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image header must contain positive width, height, and max value",
            };
        }

        if (max_value != 255) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image decoder only supports 8-bit max value 255",
            };
        }

        if (offset >= bytes.size() || !detail::ppm_is_ascii_space(detail::ppm_byte_to_uchar(bytes[offset]))) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image header must end with a whitespace byte before pixel data",
            };
        }
        ++offset;

        std::size_t pixel_count = 0;
        if (!detail::ppm_checked_pixel_count(width, height, pixel_count)) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image dimensions overflow the pixel count",
            };
        }

        constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
        if (pixel_count > max_size / 4) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image dimensions overflow the RGBA byte count",
            };
        }
        const std::size_t rgba_byte_count = pixel_count * 4;

        if (pixel_count > max_size / 3) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image dimensions overflow the RGB byte count",
            };
        }
        const std::size_t rgb_byte_count = pixel_count * 3;
        if (bytes.size() - offset != rgb_byte_count) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image pixel data size does not match dimensions",
            };
        }

        std::vector<std::byte> pixels;
        pixels.reserve(rgba_byte_count);
        for (std::size_t rgb_offset = offset; rgb_offset < bytes.size(); rgb_offset += 3) {
            pixels.push_back(bytes[rgb_offset]);
            pixels.push_back(bytes[rgb_offset + 1]);
            pixels.push_back(bytes[rgb_offset + 2]);
            pixels.push_back(std::byte{0xff});
        }

        return render_image_decode_result{
            .status = render_image_decode_status::decoded,
            .image = render_decoded_image{
                .width = width,
                .height = height,
                .pixel_format = render_image_pixel_format::rgba8_srgb,
                .pixels = std::move(pixels),
            },
            .diagnostic = {},
        };
    }
};

class fake_image_decoder final : public image_decoder_interface {
public:
    bool supports(const render_image_decode_request& request) const override
    {
        support_requests.push_back(request);
        return fake_image_decoder_supports_source(request.source);
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        decode_requests.push_back(request);
        if (request.encoded_bytes.empty()) {
            return render_image_decode_result{
                .status = render_image_decode_status::empty_input,
                .image = {},
                .diagnostic = "fake image decoder requires placeholder bytes",
            };
        }

        if (!fake_image_decoder_supports_source(request.source)) {
            return render_image_decode_result{
                .status = render_image_decode_status::unsupported_format,
                .image = {},
                .diagnostic = "fake image decoder only supports .fake image sources",
            };
        }

        return render_image_decode_result{
            .status = render_image_decode_status::decoded,
            .image = render_decoded_image{
                .width = 2,
                .height = 1,
                .pixel_format = render_image_pixel_format::rgba8_srgb,
                .pixels = {
                    std::byte{0xff},
                    std::byte{0x00},
                    std::byte{0x00},
                    std::byte{0xff},
                    std::byte{0x00},
                    std::byte{0xff},
                    std::byte{0x00},
                    std::byte{0xff},
                },
            },
            .diagnostic = {},
        };
    }

    mutable std::vector<render_image_decode_request> support_requests;
    mutable std::vector<render_image_decode_request> decode_requests;
};

} // namespace quiz_vulkan::render
