#pragma once

#include "render/text/font_glyph_atlas.h"

#include <cstddef>

namespace quiz_vulkan::render {

struct render_text_glyph_cluster {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::size_t glyph_offset = 0;
    std::size_t glyph_count = 0;
    float advance = 0.0f;
    float baseline = 0.0f;
    std::size_t line_index = 0;
    font_face_id resolved_face_id = 0;
};

} // namespace quiz_vulkan::render
