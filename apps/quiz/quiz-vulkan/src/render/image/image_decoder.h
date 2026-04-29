#pragma once

#include "render/image/image_types.h"

#include <cstddef>
#include <functional>
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

struct render_image_decode_metadata {
    std::string decoder_id;
    std::size_t encoded_byte_count = 0;
    std::size_t width = 0;
    std::size_t height = 0;
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    std::size_t decoded_byte_count = 0;

    bool has_image() const
    {
        return width != 0 && height != 0 && decoded_byte_count != 0;
    }
};

struct render_image_decoder_diagnostic {
    std::string decoder_id;
    bool supported = false;
    render_image_decode_status status = render_image_decode_status::unsupported_format;
    std::string diagnostic;
};

struct render_image_decode_result {
    render_image_decode_status status = render_image_decode_status::empty_input;
    render_decoded_image image;
    std::string diagnostic;
    render_image_decode_metadata metadata;
    std::vector<render_image_decoder_diagnostic> decoder_diagnostics;

    bool ok() const
    {
        return status == render_image_decode_status::decoded;
    }
};

inline render_image_decode_metadata make_render_image_decode_metadata(
    std::string decoder_id,
    const render_image_decode_request& request,
    const render_decoded_image& image)
{
    return render_image_decode_metadata{
        .decoder_id = std::move(decoder_id),
        .encoded_byte_count = request.encoded_bytes.size(),
        .width = image.width,
        .height = image.height,
        .pixel_format = image.pixel_format,
        .decoded_byte_count = image.pixels.size(),
    };
}

inline render_image_decode_metadata make_render_image_decode_metadata(
    std::string decoder_id,
    const render_image_decode_request& request)
{
    return make_render_image_decode_metadata(std::move(decoder_id), request, render_decoded_image{});
}

class image_decoder_interface {
public:
    virtual ~image_decoder_interface() = default;

    virtual bool supports(const render_image_decode_request& request) const = 0;
    virtual render_image_decode_result decode(const render_image_decode_request& request) const = 0;
};

struct image_decoder_chain_entry {
    std::string decoder_id;
    std::reference_wrapper<const image_decoder_interface> decoder;
};

class image_decoder_chain final : public image_decoder_interface {
public:
    image_decoder_chain() = default;

    explicit image_decoder_chain(std::vector<std::reference_wrapper<const image_decoder_interface>> decoders)
    {
        decoders_.reserve(decoders.size());
        for (const std::reference_wrapper<const image_decoder_interface> decoder : decoders) {
            add_decoder(decoder.get());
        }
    }

    explicit image_decoder_chain(std::vector<image_decoder_chain_entry> decoders)
        : decoders_(std::move(decoders))
    {
    }

    void add_decoder(const image_decoder_interface& decoder)
    {
        add_decoder(make_default_decoder_id(decoders_.size()), decoder);
    }

    void add_decoder(std::string decoder_id, const image_decoder_interface& decoder)
    {
        decoders_.push_back(image_decoder_chain_entry{
            .decoder_id = std::move(decoder_id),
            .decoder = std::cref(decoder),
        });
    }

    std::size_t decoder_count() const
    {
        return decoders_.size();
    }

    bool supports(const render_image_decode_request& request) const override
    {
        for (const image_decoder_chain_entry& entry : decoders_) {
            if (entry.decoder.get().supports(request)) {
                return true;
            }
        }
        return false;
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        std::vector<render_image_decoder_diagnostic> diagnostics;
        diagnostics.reserve(decoders_.size());
        for (const image_decoder_chain_entry& entry : decoders_) {
            if (!entry.decoder.get().supports(request)) {
                diagnostics.push_back(render_image_decoder_diagnostic{
                    .decoder_id = entry.decoder_id,
                    .supported = false,
                    .status = render_image_decode_status::unsupported_format,
                    .diagnostic = "decoder did not support source",
                });
                continue;
            }

            render_image_decode_result result = entry.decoder.get().decode(request);
            if (result.metadata.decoder_id.empty()) {
                result.metadata.decoder_id = entry.decoder_id;
            }
            diagnostics.push_back(render_image_decoder_diagnostic{
                .decoder_id = entry.decoder_id,
                .supported = true,
                .status = result.status,
                .diagnostic = result.diagnostic,
            });
            result.decoder_diagnostics.insert(
                result.decoder_diagnostics.begin(),
                diagnostics.begin(),
                diagnostics.end());
            return result;
        }

        return render_image_decode_result{
            .status = render_image_decode_status::unsupported_format,
            .image = {},
            .diagnostic = "no image decoder in the chain supports the source",
            .metadata = make_render_image_decode_metadata({}, request),
            .decoder_diagnostics = std::move(diagnostics),
        };
    }

private:
    static std::string make_default_decoder_id(std::size_t index)
    {
        return "decoder[" + std::to_string(index) + "]";
    }

    std::vector<image_decoder_chain_entry> decoders_;
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
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        if (!ppm_image_decoder_supports_source(request.source)) {
            return render_image_decode_result{
                .status = render_image_decode_status::unsupported_format,
                .image = {},
                .diagnostic = "ppm image decoder only supports .ppm or .pnm image sources",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        const std::vector<std::byte>& bytes = request.encoded_bytes;
        if (bytes.size() < 3 || detail::ppm_byte_to_uchar(bytes[0]) != 'P'
            || detail::ppm_byte_to_uchar(bytes[1]) != '6') {
            return render_image_decode_result{
                .status = render_image_decode_status::unsupported_format,
                .image = {},
                .diagnostic = "ppm image decoder requires binary P6 magic",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        if (!detail::ppm_is_ascii_space(detail::ppm_byte_to_uchar(bytes[2]))) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image header must delimit the P6 magic with whitespace",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
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
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        if (max_value != 255) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image decoder only supports 8-bit max value 255",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        if (offset >= bytes.size() || !detail::ppm_is_ascii_space(detail::ppm_byte_to_uchar(bytes[offset]))) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image header must end with a whitespace byte before pixel data",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }
        ++offset;

        std::size_t pixel_count = 0;
        if (!detail::ppm_checked_pixel_count(width, height, pixel_count)) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image dimensions overflow the pixel count",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
        if (pixel_count > max_size / 4) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image dimensions overflow the RGBA byte count",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }
        const std::size_t rgba_byte_count = pixel_count * 4;

        if (pixel_count > max_size / 3) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image dimensions overflow the RGB byte count",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }
        const std::size_t rgb_byte_count = pixel_count * 3;
        if (bytes.size() - offset != rgb_byte_count) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image pixel data size does not match dimensions",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
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

        render_decoded_image image{
            .width = width,
            .height = height,
            .pixel_format = render_image_pixel_format::rgba8_srgb,
            .pixels = std::move(pixels),
        };
        render_image_decode_metadata metadata = make_render_image_decode_metadata(
            "ppm_image_decoder",
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
                .metadata = make_render_image_decode_metadata("fake_image_decoder", request),
            };
        }

        if (!fake_image_decoder_supports_source(request.source)) {
            return render_image_decode_result{
                .status = render_image_decode_status::unsupported_format,
                .image = {},
                .diagnostic = "fake image decoder only supports .fake image sources",
                .metadata = make_render_image_decode_metadata("fake_image_decoder", request),
            };
        }

        render_decoded_image image{
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
        };
        render_image_decode_metadata metadata = make_render_image_decode_metadata(
            "fake_image_decoder",
            request,
            image);

        return render_image_decode_result{
            .status = render_image_decode_status::decoded,
            .image = std::move(image),
            .diagnostic = {},
            .metadata = std::move(metadata),
        };
    }

    mutable std::vector<render_image_decode_request> support_requests;
    mutable std::vector<render_image_decode_request> decode_requests;
};

} // namespace quiz_vulkan::render
