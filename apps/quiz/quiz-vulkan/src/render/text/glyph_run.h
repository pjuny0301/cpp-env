#pragma once

#include "render/text/font_glyph_atlas.h"
#include "render/text/text_engine.h"

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

struct render_text_caret_rect_snapshot {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    render_rect bounds;
    std::size_t line_index = 0;
    std::size_t cluster_index = 0;
    bool at_cluster_end = false;
};

struct render_text_selection_rect_snapshot {
    std::size_t start_run_index = 0;
    std::size_t start_byte_offset = 0;
    std::size_t end_run_index = 0;
    std::size_t end_byte_offset = 0;
    render_rect bounds;
    std::size_t line_index = 0;
    std::size_t cluster_offset = 0;
    std::size_t cluster_count = 0;
};

} // namespace quiz_vulkan::render
