#pragma once

#include "render/text/font_backend_capabilities.h"
#include "render/text/font_backend_adapter.h"
#include "render/text/font_backend_dependency.h"
#include "render/text/font_backend_selection.h"
#include "render/text/font_coverage_run_segmentation.h"
#include "render/text/font_fallback_shaping_handoff.h"
#include "render/text/font_rasterizer.h"
#include "render/text/font_glyph_id_resolver.h"
#include "render/text/font_resolver.h"
#include "render/text/font_shaped_atlas_update.h"
#include "render/text/font_shaping_backend.h"
#include "render/text/glyph_run.h"
#include "render/text/text_engine.h"

#include <cstddef>
#include <optional>
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

struct fake_text_engine_font_backend_adapter_policy_snapshot {
    bool configured = false;
    bool used_for_shaping = false;
    std::size_t shaping_request_count = 0;
    std::size_t shaped_count = 0;
    std::size_t backend_unavailable_count = 0;
    std::size_t unsupported_script_count = 0;
    std::size_t unsupported_glyph_count = 0;
    std::size_t glyph_id_mismatch_count = 0;
    std::size_t recoverable_failure_count = 0;
    std::size_t fatal_failure_count = 0;
};

struct fake_text_engine_font_backend_dependency_policy_snapshot {
    bool configured = false;
    bool fake_only = true;
    bool adapter_ready = false;
    bool fallback_ready = false;
    std::size_t probe_count = 0;
    std::size_t adapter_ready_count = 0;
    std::size_t fallback_ready_count = 0;
    std::size_t missing_dependency_count = 0;
    std::size_t adapter_unavailable_count = 0;
    std::size_t version_mismatch_count = 0;
    std::size_t unsupported_feature_count = 0;
};

struct fake_text_engine_font_backend_selection_snapshot {
    render_text_font_backend_selection_purpose purpose =
        render_text_font_backend_selection_purpose::shaping;
    render_text_font_backend_library library =
        render_text_font_backend_library::deterministic_fake;
    std::string label;
    render_text_font_backend_selection_status selection_status =
        render_text_font_backend_selection_status::unavailable;
    render_text_font_backend_capability_status capability_status =
        render_text_font_backend_capability_status::unavailable;
    bool used_deterministic_fallback = false;
    bool fallback_only = false;
    bool selected_real_backend = false;
    bool dependency_probe_configured = false;
    render_text_font_backend_adapter_readiness_status dependency_status =
        render_text_font_backend_adapter_readiness_status::fallback_ready;
    render_text_font_backend_adapter_readiness_status dependency_fallback_reason =
        render_text_font_backend_adapter_readiness_status::missing_dependency;
    bool dependency_adapter_ready = false;
    bool dependency_fallback_ready = false;
    bool fake_only = false;
    std::string dependency_diagnostic;
};

struct fake_text_engine_font_backend_run_selection_snapshot {
    std::size_t run_index = 0;
    render_style_id style_token;
    fake_text_engine_font_backend_selection_snapshot shaping;
    fake_text_engine_font_backend_selection_snapshot rasterization;
    fake_text_engine_font_backend_selection_snapshot unicode_processing;
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
    render_text_font_catalog_policy_snapshot font_catalog_policy;
    std::vector<render_text_font_source_resolution_snapshot> font_source_resolutions;
    render_text_font_source_policy_snapshot font_source_policy;
    std::vector<render_text_font_source_bytes_snapshot> font_source_bytes;
    render_text_font_source_bytes_policy_snapshot font_source_bytes_policy;
    render_text_font_backend_capability_snapshot font_backend_capability;
    render_text_font_backend_selection_result font_backend_shaping_selection;
    render_text_font_backend_selection_result font_backend_rasterization_selection;
    render_text_font_backend_selection_result font_backend_unicode_selection;
    render_text_external_font_backend_probe_result font_backend_shaping_dependency;
    render_text_external_font_backend_probe_result font_backend_rasterization_dependency;
    render_text_external_font_backend_probe_result font_backend_unicode_dependency;
    std::vector<fake_text_engine_font_backend_run_selection_snapshot> font_backend_run_selections;
    std::vector<render_text_font_fallback_chain_run_snapshot> font_fallback_chain_runs;
    std::vector<render_text_font_fallback_chain_missing_glyph_snapshot> font_fallback_chain_missing_glyphs;
    std::vector<font_face_id> font_fallback_chain_selected_face_order;
    render_text_font_backend_selection_result font_fallback_chain_shaping_selection;
    render_text_font_fallback_chain_plan_policy_snapshot font_fallback_chain_policy;
    std::string font_fallback_chain_diagnostic;
    render_text_font_fallback_run_plan_snapshot font_fallback_run_plan;
    render_text_font_fallback_shaping_handoff_snapshot font_fallback_shaping_handoff;
    render_text_font_fallback_shaped_glyph_input_snapshot font_fallback_shaped_glyph_inputs;
    render_text_font_backend_shaping_capability font_backend_shaping_capability;
    bool font_backend_uses_deterministic_shaping = true;
    bool font_backend_uses_deterministic_rasterizer = true;
    bool font_backend_uses_adapter_shaping = false;
    std::vector<render_text_font_backend_adapter_diagnostic> font_backend_adapter_diagnostics;
    fake_text_engine_font_backend_adapter_policy_snapshot font_backend_adapter_policy;
    fake_text_engine_font_backend_dependency_policy_snapshot font_backend_dependency_policy;
    std::vector<render_text_shaped_glyph> shaped_glyphs;
    std::vector<render_text_font_shaping_diagnostic> font_shaping_diagnostics;
    render_text_font_shaping_policy_snapshot font_shaping_policy;
    std::vector<render_text_font_glyph_id_resolution_snapshot> glyph_id_resolutions;
    render_text_font_glyph_id_resolution_policy_snapshot glyph_id_resolution_policy;
    std::vector<render_text_glyph_font_resolution_snapshot> glyph_font_resolutions;
    render_text_font_resolution_policy_snapshot font_resolution_policy;
    std::vector<render_text_line_break_snapshot> line_breaks;
    std::vector<render_text_line_metrics_snapshot> line_metrics;
    std::vector<render_text_line_run_box_snapshot> line_run_boxes;
    render_text_line_layout_metrics_snapshot line_layout_metrics;
    render_text_line_layout_policy_snapshot line_layout_policy;
    render_text_line_break_policy_snapshot line_break_policy;
    std::vector<render_text_caret_rect_snapshot> caret_hit_tests;
    std::vector<render_text_glyph_cache_readiness_snapshot> glyph_cache_readiness;
    render_text_glyph_cache_readiness_policy_snapshot glyph_cache_readiness_policy;
    std::vector<render_text_rasterized_glyph_atlas_payload_snapshot> rasterized_glyph_atlas_payloads;
    render_text_rasterized_glyph_atlas_payload_policy_snapshot rasterized_glyph_atlas_payload_policy;
    std::vector<render_text_glyph_atlas_materialization_snapshot> glyph_atlas_materializations;
    render_text_glyph_atlas_materialization_policy_snapshot glyph_atlas_materialization_policy;
    render_text_atlas_upload_request_bridge_snapshot atlas_upload_request_bridge;
    std::vector<std::string> queued_atlas_upload_request_ids;
    std::vector<std::string> consumed_atlas_upload_request_ids;
    std::size_t consumed_atlas_update_count = 0;
    render_text_frame_snapshot text_frame_snapshot;
    std::vector<render_text_shaped_atlas_update_trace_snapshot> shaped_atlas_update_traces;
    render_text_shaped_atlas_update_trace_policy_snapshot shaped_atlas_update_trace_policy;
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

    bool has_font_catalog_policy() const
    {
        return !font_catalog_policy.style_face_mappings.empty();
    }

    bool has_font_source_resolutions() const
    {
        return !font_source_resolutions.empty();
    }

    bool has_font_source_policy() const
    {
        return font_source_policy.request_count > 0;
    }

    bool has_font_source_bytes() const
    {
        return !font_source_bytes.empty();
    }

    bool has_font_source_bytes_policy() const
    {
        return font_source_bytes_policy.request_count > 0;
    }

    bool has_font_backend_capability() const
    {
        return !font_backend_capability.components.empty()
            || !font_backend_capability.diagnostic.empty();
    }

    bool has_font_backend_selection() const
    {
        return font_backend_shaping_selection.has_selection
            || font_backend_rasterization_selection.has_selection
            || font_backend_unicode_selection.has_selection;
    }

    bool has_font_backend_run_selections() const
    {
        return !font_backend_run_selections.empty();
    }

    bool has_font_fallback_chain_runs() const
    {
        return !font_fallback_chain_runs.empty();
    }

    bool has_font_fallback_chain_missing_glyphs() const
    {
        return !font_fallback_chain_missing_glyphs.empty();
    }

    bool has_font_fallback_chain_policy() const
    {
        return font_fallback_chain_policy.run_count > 0;
    }

    bool has_font_fallback_run_plan() const
    {
        return font_fallback_run_plan.policy.fallback_run_count > 0;
    }

    bool has_font_fallback_shaping_handoff() const
    {
        return font_fallback_shaping_handoff.policy.run_count > 0;
    }

    bool has_font_fallback_shaped_glyph_inputs() const
    {
        return font_fallback_shaped_glyph_inputs.has_inputs();
    }

    bool has_font_backend_adapter_diagnostics() const
    {
        return !font_backend_adapter_diagnostics.empty();
    }

    bool has_font_backend_adapter_policy() const
    {
        return font_backend_adapter_policy.configured
            || font_backend_adapter_policy.shaping_request_count > 0;
    }

    bool has_font_backend_dependency_probe() const
    {
        return font_backend_dependency_policy.configured
            || font_backend_dependency_policy.probe_count > 0;
    }

    bool has_shaped_glyphs() const
    {
        return !shaped_glyphs.empty();
    }

    bool has_font_shaping_diagnostics() const
    {
        return !font_shaping_diagnostics.empty();
    }

    bool has_font_shaping_policy() const
    {
        return font_shaping_policy.run_count > 0;
    }

    bool has_glyph_id_resolutions() const
    {
        return !glyph_id_resolutions.empty();
    }

    bool has_glyph_id_resolution_policy() const
    {
        return glyph_id_resolution_policy.request_count > 0;
    }

    bool has_glyph_font_resolutions() const
    {
        return !glyph_font_resolutions.empty();
    }

    bool has_glyph_cache_readiness() const
    {
        return !glyph_cache_readiness.empty();
    }

    bool has_rasterized_glyph_atlas_payloads() const
    {
        return !rasterized_glyph_atlas_payloads.empty();
    }

    bool has_rasterized_glyph_atlas_payload_policy() const
    {
        return rasterized_glyph_atlas_payload_policy.request_count > 0;
    }

    bool has_glyph_atlas_materializations() const
    {
        return !glyph_atlas_materializations.empty();
    }

    bool has_glyph_atlas_materialization_policy() const
    {
        return glyph_atlas_materialization_policy.request_count > 0;
    }

    bool has_atlas_upload_request_bridge() const
    {
        return !atlas_upload_request_bridge.requests.empty();
    }

    bool has_queued_atlas_upload_request_ids() const
    {
        return !queued_atlas_upload_request_ids.empty();
    }

    bool has_consumed_atlas_upload_request_ids() const
    {
        return !consumed_atlas_upload_request_ids.empty();
    }

    bool has_text_frame_snapshot() const
    {
        return !text_frame_snapshot.frame_id.empty()
            || text_frame_snapshot.policy.layout_request_count > 0U
            || text_frame_snapshot.policy.upload_request_count > 0U;
    }

    bool has_shaped_atlas_update_traces() const
    {
        return !shaped_atlas_update_traces.empty();
    }

    bool has_shaped_atlas_update_trace_policy() const
    {
        return shaped_atlas_update_trace_policy.trace_count > 0;
    }

    bool has_line_breaks() const
    {
        return !line_breaks.empty();
    }

    bool has_line_metrics() const
    {
        return !line_metrics.empty();
    }

    bool has_line_run_boxes() const
    {
        return !line_run_boxes.empty();
    }

    bool has_line_layout_policy() const
    {
        return line_layout_policy.clipped_line_count > 0 || line_layout_policy.ellipsis_applied;
    }

    bool has_line_break_policy() const
    {
        return line_break_policy.break_count > 0;
    }

    bool has_glyph_cache_faces() const
    {
        return !glyph_cache_faces.empty();
    }

    bool has_caret_hit_tests() const
    {
        return !caret_hit_tests.empty();
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
    void set_font_backend_capability_components(
        std::vector<render_text_font_backend_component> components);
    void clear_font_backend_capability_components();
    void set_font_backend_capability_probe_request(
        render_text_font_backend_capability_probe_request request);
    void set_font_backend_adapter_functions(
        render_text_font_backend_adapter_functions functions);
    void clear_font_backend_adapter_functions();
    void set_font_backend_selection_candidates(
        std::vector<render_text_font_backend_candidate> candidates);
    void clear_font_backend_selection_candidates();
    void set_font_backend_dependency_manifest(
        render_text_external_font_backend_manifest manifest);
    void clear_font_backend_dependency_manifest();
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
    mutable std::vector<std::string> font_source_bytes_cache_entries_;
    mutable std::vector<render_text_atlas_update> atlas_updates_;
    mutable std::vector<std::string> atlas_update_request_ids_;
    mutable fake_text_engine_diagnostics diagnostics_;
    deterministic_fake_font_resolver font_resolver_;
    std::vector<render_text_font_backend_component> font_backend_capability_components_;
    std::vector<render_text_font_backend_candidate> font_backend_selection_candidates_;
    std::optional<render_text_external_font_backend_manifest> font_backend_dependency_manifest_;
    std::optional<render_text_font_backend_adapter_functions> font_backend_adapter_functions_;
    render_text_font_backend_capability_probe_request font_backend_capability_request_{
        .required_features = {
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_shaping,
            render_text_font_backend_feature::glyph_rasterization,
        },
    };
};

} // namespace quiz_vulkan::render
