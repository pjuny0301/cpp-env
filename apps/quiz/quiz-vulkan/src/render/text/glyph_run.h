#pragma once

#include "render/text/font_glyph_atlas.h"
#include "render/text/text_engine.h"
#include "render/text/utf8_line_break.h"

#include <cstddef>
#include <string>

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

struct render_text_utf8_cluster_snapshot {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::size_t codepoint_offset = 0;
    std::size_t codepoint_count = 0;
    bool valid = true;
};

struct render_text_font_face_selection_snapshot {
    std::size_t run_index = 0;
    render_style_id style_token;
    std::string requested_family;
    std::string resolved_family;
    int requested_weight = 400;
    int resolved_weight = 400;
    bool requested_italic = false;
    bool resolved_italic = false;
    font_face_id resolved_face_id = 0;
    bool used_family_fallback = false;
    bool used_style_fallback = false;
};

struct render_text_line_break_snapshot {
    std::size_t line_index = 0;
    std::size_t codepoint_offset = 0;
    std::size_t codepoint_count = 0;
    std::size_t start_run_index = 0;
    std::size_t start_byte_offset = 0;
    std::size_t end_run_index = 0;
    std::size_t end_byte_offset = 0;
    std::size_t separator_run_index = 0;
    std::size_t separator_byte_offset = 0;
    std::size_t separator_byte_count = 0;
    utf8_line_break_reason break_reason = utf8_line_break_reason::end_of_text;
    float line_width = 0.0f;
    float max_width = 0.0f;
    bool wrapped = false;
};

struct render_text_line_metrics_snapshot {
    std::size_t line_index = 0;
    float width = 0.0f;
    float height = 0.0f;
    float max_width = 0.0f;
    float overflow_width = 0.0f;
    bool overflowed = false;
    bool truncated = false;
};

struct render_text_line_layout_metrics_snapshot {
    std::size_t produced_line_count = 0;
    std::size_t visible_line_count = 0;
    std::size_t truncated_line_count = 0;
    std::size_t overflow_line_count = 0;
    bool truncated = false;
    bool overflowed = false;
};

} // namespace quiz_vulkan::render
