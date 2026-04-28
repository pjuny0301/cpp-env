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

} // namespace quiz_vulkan::render
