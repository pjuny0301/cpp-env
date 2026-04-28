#pragma once

#include "render/image/image_decoder.h"
#include "render/image/image_types.h"

#include <string>

namespace quiz_vulkan::render {

struct render_image_texture_request {
    render_resolved_image_source source;
    render_image_sampler_policy sampler;
};

enum class render_image_texture_status {
    ready,
    missing_source,
    decode_failed,
    upload_failed,
};

struct render_image_texture_result {
    render_image_texture_status status = render_image_texture_status::missing_source;
    render_image_texture_key key;
    render_image_texture_handle texture;
    bool cache_hit = false;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_texture_status::ready && texture.valid();
    }
};

class image_texture_cache_interface {
public:
    virtual ~image_texture_cache_interface() = default;

    virtual render_image_texture_result acquire_texture(const render_image_texture_request& request) = 0;
    virtual void release_unused() = 0;
};

} // namespace quiz_vulkan::render
