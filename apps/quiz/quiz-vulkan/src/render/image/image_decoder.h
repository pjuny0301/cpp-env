#pragma once

#include "render/image/image_types.h"

#include <cstddef>
#include <string>
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

inline bool fake_image_decoder_supports_source(const render_resolved_image_source& source)
{
    return !source.cache_key().empty() && source.normalized_uri.ends_with(".fake");
}

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
