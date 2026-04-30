#pragma once

#include "render/text/font_glyph_atlas.h"
#include "render/text/text_engine.h"
#include "render/text/utf8_line_break.h"

#include <cstddef>
#include <string>
#include <vector>

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

struct render_text_glyph_atlas_page_snapshot {
    render_text_atlas_page page;
    std::size_t cluster_count = 0;
    std::size_t cache_hit_count = 0;
    std::size_t new_slot_count = 0;
    std::size_t created_page_count = 0;
    std::size_t dirty_update_count = 0;
    render_rect dirty_bounds;
    bool upload_ready = false;
};

struct render_text_glyph_atlas_page_policy_snapshot {
    std::size_t page_count = 0;
    std::size_t allocated_page_count = 0;
    std::size_t created_page_count = 0;
    std::size_t dirty_page_count = 0;
    std::size_t upload_ready_page_count = 0;
    std::size_t cache_hit_page_count = 0;
    std::size_t total_cluster_count = 0;
    std::size_t total_new_slot_count = 0;
    std::size_t total_cache_hit_count = 0;
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

struct render_text_glyph_font_resolution_snapshot {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::uint32_t code_point = 0;
    font_face_id requested_face_id = 0;
    font_face_id resolved_face_id = 0;
    bool used_codepoint_fallback = false;
    bool glyph_supported = true;
    bool cacheable = true;
};

struct render_text_font_resolution_policy_snapshot {
    std::size_t run_request_count = 0;
    std::size_t exact_face_match_count = 0;
    std::size_t family_fallback_count = 0;
    std::size_t style_fallback_count = 0;
    std::size_t glyph_request_count = 0;
    std::size_t glyph_supported_count = 0;
    std::size_t codepoint_fallback_count = 0;
    std::size_t missing_glyph_count = 0;
    std::size_t cacheable_glyph_count = 0;
    std::size_t unique_resolved_face_count = 0;
};

struct render_text_line_break_snapshot {
    std::size_t line_index = 0;
    std::size_t codepoint_offset = 0;
    std::size_t codepoint_count = 0;
    std::size_t utf8_cluster_offset = 0;
    std::size_t utf8_cluster_count = 0;
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
    bool starts_at_utf8_cluster_boundary = true;
    bool ends_at_utf8_cluster_boundary = true;
    bool caret_safe = true;
    bool used_hangul_width_break = false;
    bool used_long_token_fallback = false;
};

struct render_text_line_metrics_snapshot {
    std::size_t line_index = 0;
    std::size_t start_run_index = 0;
    std::size_t start_byte_offset = 0;
    std::size_t end_run_index = 0;
    std::size_t end_byte_offset = 0;
    std::size_t caret_stop_count = 0;
    float width = 0.0f;
    float height = 0.0f;
    float max_width = 0.0f;
    float overflow_width = 0.0f;
    bool overflowed = false;
    bool truncated = false;
    bool caret_safe = true;
};

struct render_text_line_layout_metrics_snapshot {
    std::size_t produced_line_count = 0;
    std::size_t visible_line_count = 0;
    std::size_t truncated_line_count = 0;
    std::size_t overflow_line_count = 0;
    bool truncated = false;
    bool overflowed = false;
};

struct render_text_line_break_policy_snapshot {
    std::size_t break_count = 0;
    std::size_t ascii_whitespace_break_count = 0;
    std::size_t explicit_newline_break_count = 0;
    std::size_t width_pressure_break_count = 0;
    std::size_t hangul_width_break_count = 0;
    std::size_t long_token_fallback_break_count = 0;
    std::size_t caret_safe_break_count = 0;
    std::size_t unsafe_break_count = 0;
    std::size_t overflow_line_count = 0;
    std::size_t truncated_line_count = 0;
};

struct render_text_glyph_cache_face_snapshot {
    font_face_id face_id = 0;
    std::vector<std::uint32_t> cached_glyph_ids;
    std::size_t request_count = 0;
    std::size_t hit_count = 0;
    std::size_t miss_count = 0;
    std::size_t eviction_count = 0;
    std::size_t atlas_reuse_count = 0;
};

struct render_text_glyph_cache_policy_snapshot {
    std::size_t capacity = 0;
    std::size_t cached_glyph_count = 0;
    std::size_t request_count = 0;
    std::size_t hit_count = 0;
    std::size_t miss_count = 0;
    std::size_t insert_count = 0;
    std::size_t eviction_count = 0;
    std::size_t atlas_reuse_count = 0;
    std::size_t atlas_allocation_count = 0;
    std::size_t atlas_page_reuse_count = 0;
    std::size_t atlas_page_create_count = 0;
};

struct render_text_glyph_cache_readiness_snapshot {
    std::size_t cluster_index = 0;
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::uint32_t glyph_id = 0;
    font_face_id requested_face_id = 0;
    font_face_id resolved_face_id = 0;
    glyph_atlas_key cache_key;
    std::size_t atlas_width = 0;
    std::size_t atlas_height = 0;
    std::size_t estimated_rgba_bytes = 0;
    bool glyph_supported = true;
    bool used_codepoint_fallback = false;
    bool cacheable = true;
    bool has_atlas_slot = false;
};

struct render_text_glyph_cache_readiness_policy_snapshot {
    std::size_t cluster_count = 0;
    std::size_t cacheable_cluster_count = 0;
    std::size_t uncacheable_cluster_count = 0;
    std::size_t unsupported_cluster_count = 0;
    std::size_t codepoint_fallback_cluster_count = 0;
    std::size_t zero_advance_cluster_count = 0;
    std::size_t unique_cache_key_count = 0;
    std::size_t unique_face_count = 0;
    std::size_t estimated_rgba_bytes = 0;
};

} // namespace quiz_vulkan::render
