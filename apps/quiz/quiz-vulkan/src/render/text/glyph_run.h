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

struct render_text_glyph_atlas_placement_snapshot {
    std::size_t cluster_index = 0;
    glyph_atlas_key key;
    render_text_atlas_page page;
    render_rect atlas_bounds;
    bool cache_hit = false;
    bool newly_allocated = false;
    bool created_page = false;
    bool reused_existing_page = false;
};

struct render_text_glyph_atlas_metrics_snapshot {
    std::size_t requested_cluster_count = 0;
    std::size_t placed_cluster_count = 0;
    std::size_t cache_hit_count = 0;
    std::size_t new_slot_count = 0;
    std::size_t reused_page_slot_count = 0;
    std::size_t new_page_count = 0;
    std::size_t dirty_page_count = 0;
    std::size_t page_count_after = 0;
    render_text_revision latest_page_revision = 0;
};

} // namespace quiz_vulkan::render
