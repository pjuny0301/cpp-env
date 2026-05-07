#pragma once

#include "render/image/image_decoder.h"

#include <cstddef>

namespace quiz_vulkan::render {

class standard_image_decoder_chain final : public image_decoder_interface {
public:
    standard_image_decoder_chain()
        : png_decoder_(png_inflater_)
    {
        chain_.add_decoder("bmp_image_decoder", bmp_decoder_);
        chain_.add_decoder("ppm_image_decoder", ppm_decoder_);
        chain_.add_decoder("png_image_decoder", png_decoder_);
    }

    standard_image_decoder_chain(const standard_image_decoder_chain&) = delete;
    standard_image_decoder_chain& operator=(const standard_image_decoder_chain&) = delete;
    standard_image_decoder_chain(standard_image_decoder_chain&&) = delete;
    standard_image_decoder_chain& operator=(standard_image_decoder_chain&&) = delete;

    bool supports(const render_image_decode_request& request) const override
    {
        return chain_.supports(request);
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        return chain_.decode(request);
    }

    std::size_t decoder_count() const
    {
        return chain_.decoder_count();
    }

private:
    bmp_image_decoder bmp_decoder_;
    ppm_image_decoder ppm_decoder_;
    png_image_zlib_inflater png_inflater_;
    png_image_decoder png_decoder_;
    image_decoder_chain chain_;
};

inline standard_image_decoder_chain make_standard_image_decoder_chain()
{
    return standard_image_decoder_chain{};
}

} // namespace quiz_vulkan::render
