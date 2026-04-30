#pragma once

#include "render/text/font_resolver.h"
#include "render/text/glyph_run.h"
#include "render/text/text_engine.h"

#include <cstddef>
#include <string>
#include <vector>

namespace quiz_vulkan::render {

struct fake_text_engine_style_fallback {
    std::size_t run_index = 0;
    render_style_id requested_style_token;
    render_style_id fallback_style_token;
};

struct fake_text_engine_font_fallback {
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

struct fake_text_engine_diagnostics {
    std::vector<fake_text_engine_style_fallback> style_fallbacks;
    std::vector<fake_text_engine_font_fallback> font_fallbacks;
    std::vector<render_text_glyph_cluster> glyph_clusters;
    std::vector<render_text_caret_rect_snapshot> caret_rects;
    std::vector<render_text_selection_rect_snapshot> selection_rects;
    std::vector<render_text_glyph_atlas_placement_snapshot> glyph_atlas_placements;
    render_text_glyph_atlas_metrics_snapshot glyph_atlas_metrics;
    std::vector<render_text_glyph_atlas_page_snapshot> glyph_atlas_pages;
    render_text_glyph_atlas_page_policy_snapshot glyph_atlas_page_policy;
    std::vector<render_text_utf8_cluster_snapshot> utf8_clusters;
    std::vector<render_text_font_face_selection_snapshot> font_face_selections;
    std::vector<render_text_glyph_font_resolution_snapshot> glyph_font_resolutions;
    render_text_font_resolution_policy_snapshot font_resolution_policy;
    std::vector<render_text_line_break_snapshot> line_breaks;
    std::vector<render_text_line_metrics_snapshot> line_metrics;
    render_text_line_layout_metrics_snapshot line_layout_metrics;
    render_text_line_break_policy_snapshot line_break_policy;
    std::vector<render_text_glyph_cache_readiness_snapshot> glyph_cache_readiness;
    render_text_glyph_cache_readiness_policy_snapshot glyph_cache_readiness_policy;
    std::vector<render_text_glyph_cache_face_snapshot> glyph_cache_faces;
    std::vector<render_text_glyph_cache_eviction_snapshot> glyph_cache_evictions;
    render_text_glyph_cache_policy_snapshot glyph_cache_policy;
    std::size_t invalid_utf8_sequence_count = 0;

    bool used_style_fallback() const
    {
        return !style_fallbacks.empty();
    }

    bool saw_invalid_utf8() const
    {
        return invalid_utf8_sequence_count > 0;
    }

    bool used_font_fallback() const
    {
        return !font_fallbacks.empty();
    }

    bool has_glyph_clusters() const
    {
        return !glyph_clusters.empty();
    }

    bool has_caret_rects() const
    {
        return !caret_rects.empty();
    }

    bool has_selection_rects() const
    {
        return !selection_rects.empty();
    }

    bool has_glyph_atlas_placements() const
    {
        return !glyph_atlas_placements.empty();
    }

    bool has_glyph_atlas_pages() const
    {
        return !glyph_atlas_pages.empty();
    }

    bool has_clean_glyph_atlas_pages() const
    {
        return has_glyph_atlas_pages() && glyph_atlas_page_policy.repeated_layout_clean_page_count > 0;
    }

    bool has_utf8_clusters() const
    {
        return !utf8_clusters.empty();
    }

    bool has_font_face_selections() const
    {
        return !font_face_selections.empty();
    }

    bool has_glyph_font_resolutions() const
    {
        return !glyph_font_resolutions.empty();
    }

    bool has_glyph_cache_readiness() const
    {
        return !glyph_cache_readiness.empty();
    }

    bool has_line_breaks() const
    {
        return !line_breaks.empty();
    }

    bool has_line_metrics() const
    {
        return !line_metrics.empty();
    }

    bool has_line_break_policy() const
    {
        return line_break_policy.break_count > 0;
    }

    bool has_glyph_cache_faces() const
    {
        return !glyph_cache_faces.empty();
    }

    bool has_glyph_cache_evictions() const
    {
        return !glyph_cache_evictions.empty();
    }
};

struct fake_text_engine_caret {
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    render_rect bounds;
};

struct fake_text_engine_selection_range {
    std::size_t start_run_index = 0;
    std::size_t start_byte_offset = 0;
    std::size_t end_run_index = 0;
    std::size_t end_byte_offset = 0;
};

class fake_text_engine final : public text_engine_interface {
public:
    render_text_measure measure_text(const render_text_request& request) const override;
    render_text_layout layout_text(const render_text_request& request) const override;
    std::vector<render_text_atlas_update> consume_atlas_updates() override;

    const fake_text_engine_diagnostics& last_diagnostics() const;
    const font_face_descriptor& add_font_face(font_face_descriptor descriptor);
    std::vector<fake_text_engine_caret> caret_positions(const render_text_request& request) const;
    std::vector<render_rect> selection_rects(
        const render_text_request& request,
        fake_text_engine_selection_range range) const;

private:
    mutable glyph_atlas_cache glyph_atlas_cache_{glyph_atlas_page_config{
        .width = 64,
        .height = 64,
        .padding = 1,
    }};
    mutable std::vector<glyph_atlas_key> glyph_cache_policy_entries_;
    mutable std::vector<render_text_atlas_update> atlas_updates_;
    mutable fake_text_engine_diagnostics diagnostics_;
    deterministic_fake_font_resolver font_resolver_;
};

} // namespace quiz_vulkan::render
