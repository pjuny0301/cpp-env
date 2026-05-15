#pragma once

#include "render/text/font_fallback_run_planning_diagnostics.h"
#include "render/text/font_glyph_id_resolver.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_fallback_shaping_handoff_status {
    ready,
    missing_glyph,
    invalid_utf8,
    no_selected_face,
};

enum class render_text_font_fallback_shaped_glyph_execution_status {
    shaped,
    invalid_utf8,
    unsupported_glyph,
    no_selected_face,
    missing_glyph_id,
};

[[nodiscard]] inline std::string render_text_font_fallback_shaping_handoff_status_name(
    const render_text_font_fallback_shaping_handoff_status status)
{
    switch (status) {
    case render_text_font_fallback_shaping_handoff_status::ready:
        return "ready";
    case render_text_font_fallback_shaping_handoff_status::missing_glyph:
        return "missing_glyph";
    case render_text_font_fallback_shaping_handoff_status::invalid_utf8:
        return "invalid_utf8";
    case render_text_font_fallback_shaping_handoff_status::no_selected_face:
        return "no_selected_face";
    }

    return "unknown";
}

[[nodiscard]] inline std::string render_text_font_fallback_shaped_glyph_execution_status_name(
    const render_text_font_fallback_shaped_glyph_execution_status status)
{
    switch (status) {
    case render_text_font_fallback_shaped_glyph_execution_status::shaped:
        return "shaped";
    case render_text_font_fallback_shaped_glyph_execution_status::invalid_utf8:
        return "invalid_utf8";
    case render_text_font_fallback_shaped_glyph_execution_status::unsupported_glyph:
        return "unsupported_glyph";
    case render_text_font_fallback_shaped_glyph_execution_status::no_selected_face:
        return "no_selected_face";
    case render_text_font_fallback_shaped_glyph_execution_status::missing_glyph_id:
        return "missing_glyph_id";
    }

    return "unknown";
}

struct render_text_font_fallback_shaping_handoff_run_snapshot {
    std::string stable_run_key;
    std::string stable_page_key;
    std::size_t item_index = 0;
    std::size_t source_run_index = 0;
    std::size_t fallback_run_index = 0;
    render_style_id style_token;
    std::string source_label;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::size_t codepoint_offset = 0;
    std::size_t codepoint_count = 0;
    std::uint32_t first_codepoint = utf8_replacement_codepoint;
    std::uint32_t last_codepoint = utf8_replacement_codepoint;
    font_face_id requested_face_id = 0;
    font_face_id selected_face_id = 0;
    std::string requested_family;
    std::string selected_family;
    std::string selected_source_uri;
    std::size_t fallback_order = 0;
    std::vector<font_face_id> attempted_face_ids;
    bool valid_utf8 = true;
    bool glyph_supported = false;
    bool used_fallback = false;
    render_text_font_fallback_run_status fallback_run_status =
        render_text_font_fallback_run_status::missing_glyph;
    render_text_font_fallback_shaping_handoff_status handoff_status =
        render_text_font_fallback_shaping_handoff_status::missing_glyph;
    std::string diagnostic;

    [[nodiscard]] bool ready_to_shape() const
    {
        return handoff_status == render_text_font_fallback_shaping_handoff_status::ready
            && !stable_run_key.empty()
            && !stable_page_key.empty()
            && selected_face_id != 0U;
    }

    [[nodiscard]] bool blocked() const
    {
        return !ready_to_shape();
    }
};

struct render_text_font_fallback_shaping_handoff_policy_snapshot {
    std::size_t run_count = 0;
    std::size_t ready_run_count = 0;
    std::size_t blocked_run_count = 0;
    std::size_t missing_glyph_run_count = 0;
    std::size_t invalid_utf8_run_count = 0;
    std::size_t no_selected_face_run_count = 0;
    std::size_t codepoint_count = 0;
    std::size_t ready_codepoint_count = 0;
    std::size_t blocked_codepoint_count = 0;
    std::size_t fallback_ready_run_count = 0;
    std::size_t unique_page_key_count = 0;
    std::size_t unique_style_token_count = 0;
};

struct render_text_font_fallback_shaping_handoff_request {
    render_text_font_fallback_run_plan_snapshot fallback_run_plan;
};

struct render_text_font_fallback_shaping_handoff_snapshot {
    std::vector<render_text_font_fallback_shaping_handoff_run_snapshot> runs;
    std::vector<render_text_font_fallback_shaping_handoff_run_snapshot> ready_runs;
    std::vector<render_text_font_fallback_shaping_handoff_run_snapshot> blocked_runs;
    std::vector<std::string> stable_run_keys;
    std::vector<std::string> stable_page_keys;
    std::vector<render_style_id> style_tokens;
    render_text_font_fallback_shaping_handoff_policy_snapshot policy;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return policy.blocked_run_count == 0U;
    }

    [[nodiscard]] bool has_blocked_runs() const
    {
        return !blocked_runs.empty();
    }
};

struct render_text_font_fallback_shaped_glyph_input_record {
    std::string stable_input_key;
    std::string stable_run_key;
    std::string stable_page_key;
    std::size_t item_index = 0;
    std::size_t source_run_index = 0;
    std::size_t fallback_run_index = 0;
    std::size_t glyph_index = 0;
    std::size_t source_codepoint_index = 0;
    render_style_id style_token;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::uint32_t codepoint = utf8_replacement_codepoint;
    std::uint32_t glyph_id = 0;
    font_face_id requested_face_id = 0;
    font_face_id selected_face_id = 0;
    std::string selected_family;
    std::string selected_source_uri;
    float advance_x = 0.0f;
    float line_height = 0.0f;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    bool cacheable = false;
    bool valid_utf8 = true;
    bool glyph_supported = true;
    bool cluster_start = true;
    bool used_fallback = false;
    bool glyph_id_from_selection = false;
    bool glyph_id_matches_codepoint = false;
    std::uint32_t glyph_id_offset = 0;
    render_text_font_shaping_codepoint_selection font_selection;
    std::string diagnostic;

    [[nodiscard]] bool ready_for_shaping() const
    {
        return valid_utf8
            && glyph_supported
            && selected_face_id != 0U
            && font_selection.has_glyph_id;
    }

    [[nodiscard]] bool ready_for_glyph_atlas() const
    {
        return ready_for_shaping() && has_cache_key && cacheable;
    }
};

struct render_text_font_fallback_shaped_glyph_input_policy_snapshot {
    std::size_t handoff_run_count = 0;
    std::size_t ready_run_count = 0;
    std::size_t blocked_run_count = 0;
    std::size_t input_count = 0;
    std::size_t cacheable_input_count = 0;
    std::size_t uncacheable_input_count = 0;
    std::size_t fallback_input_count = 0;
    std::size_t glyph_id_offset_input_count = 0;
    std::size_t missing_source_run_count = 0;
    std::size_t missing_codepoint_count = 0;
    std::size_t fallback_style_count = 0;
    std::size_t no_selected_face_count = 0;
    std::size_t unique_page_key_count = 0;
    std::size_t unique_style_token_count = 0;
};

struct render_text_font_fallback_shaped_glyph_input_request {
    render_text_font_fallback_shaping_handoff_snapshot handoff;
    std::vector<render_text_font_fallback_chain_plan_item> items;
    font_face_catalog font_catalog;
};

struct render_text_font_fallback_shaped_glyph_input_snapshot {
    std::vector<render_text_font_fallback_shaped_glyph_input_record> inputs;
    std::vector<render_text_font_fallback_shaping_handoff_run_snapshot> blocked_runs;
    std::vector<std::string> stable_input_keys;
    std::vector<std::string> stable_page_keys;
    std::vector<render_style_id> style_tokens;
    render_text_font_fallback_shaped_glyph_input_policy_snapshot policy;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return policy.blocked_run_count == 0U
            && policy.missing_source_run_count == 0U
            && policy.missing_codepoint_count == 0U
            && policy.no_selected_face_count == 0U;
    }

    [[nodiscard]] bool has_inputs() const
    {
        return !inputs.empty();
    }
};

struct render_text_font_fallback_shaped_glyph_execution_record {
    std::string stable_execution_key;
    std::string stable_input_key;
    std::string stable_run_key;
    std::string stable_page_key;
    std::size_t item_index = 0;
    std::size_t source_run_index = 0;
    std::size_t fallback_run_index = 0;
    std::size_t glyph_index = 0;
    std::size_t source_codepoint_index = 0;
    render_style_id style_token;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::uint32_t codepoint = utf8_replacement_codepoint;
    std::uint32_t glyph_id = 0;
    font_face_id requested_face_id = 0;
    font_face_id selected_face_id = 0;
    float advance_x = 0.0f;
    float line_height = 0.0f;
    glyph_atlas_key cache_key;
    bool has_cache_key = false;
    bool cacheable = false;
    bool valid_utf8 = true;
    bool glyph_supported = true;
    bool cluster_start = true;
    bool used_fallback = false;
    bool glyph_id_from_selection = false;
    bool glyph_id_matches_codepoint = false;
    std::uint32_t glyph_id_offset = 0;
    render_text_font_fallback_shaped_glyph_execution_status status =
        render_text_font_fallback_shaped_glyph_execution_status::missing_glyph_id;
    render_text_shaped_glyph shaped_glyph;
    std::string diagnostic;

    [[nodiscard]] bool executed() const
    {
        return status == render_text_font_fallback_shaped_glyph_execution_status::shaped;
    }

    [[nodiscard]] bool ready_for_glyph_atlas() const
    {
        return executed() && has_cache_key && cacheable;
    }
};

struct render_text_font_fallback_shaped_glyph_execution_policy_snapshot {
    std::size_t input_count = 0;
    std::size_t execution_count = 0;
    std::size_t shaped_count = 0;
    std::size_t blocked_input_count = 0;
    std::size_t atlas_ready_count = 0;
    std::size_t missing_cache_key_count = 0;
    std::size_t invalid_utf8_count = 0;
    std::size_t unsupported_glyph_count = 0;
    std::size_t no_selected_face_count = 0;
    std::size_t missing_glyph_id_count = 0;
    std::size_t fallback_execution_count = 0;
    std::size_t glyph_id_offset_execution_count = 0;
    std::size_t unique_input_key_count = 0;
    std::size_t unique_page_key_count = 0;
    std::size_t unique_cache_key_count = 0;
    std::size_t unique_selected_face_count = 0;
    std::size_t unique_style_token_count = 0;
};

struct render_text_font_fallback_shaped_glyph_execution_request {
    render_text_font_fallback_shaped_glyph_input_snapshot inputs;
};

struct render_text_font_fallback_shaped_glyph_execution_snapshot {
    std::vector<render_text_font_fallback_shaped_glyph_execution_record> executions;
    std::vector<render_text_font_fallback_shaping_handoff_run_snapshot> blocked_runs;
    std::vector<std::string> stable_execution_keys;
    std::vector<std::string> stable_input_keys;
    std::vector<std::string> stable_page_keys;
    std::vector<glyph_atlas_key> cache_keys;
    std::vector<font_face_id> selected_face_ids;
    std::vector<render_style_id> style_tokens;
    render_text_font_fallback_shaped_glyph_execution_policy_snapshot policy;
    std::string diagnostic;

    [[nodiscard]] bool ok() const
    {
        return policy.blocked_input_count == 0U
            && policy.invalid_utf8_count == 0U
            && policy.unsupported_glyph_count == 0U
            && policy.no_selected_face_count == 0U
            && policy.missing_glyph_id_count == 0U;
    }

    [[nodiscard]] bool has_executions() const
    {
        return !executions.empty();
    }
};

struct render_text_font_fallback_shaped_glyph_execution_record_diff {
    std::string stable_execution_key;
    bool added = false;
    bool removed = false;
    bool changed = false;
    bool status_changed = false;
    bool selected_face_changed = false;
    bool cache_key_changed = false;
    bool page_key_changed = false;
    bool style_token_changed = false;
    bool glyph_id_changed = false;
    bool diagnostic_changed = false;
    render_text_font_fallback_shaped_glyph_execution_status previous_status =
        render_text_font_fallback_shaped_glyph_execution_status::missing_glyph_id;
    render_text_font_fallback_shaped_glyph_execution_status current_status =
        render_text_font_fallback_shaped_glyph_execution_status::missing_glyph_id;
    font_face_id previous_selected_face_id = 0;
    font_face_id current_selected_face_id = 0;
    glyph_atlas_key previous_cache_key;
    glyph_atlas_key current_cache_key;
    std::string previous_page_key;
    std::string current_page_key;
    render_style_id previous_style_token;
    render_style_id current_style_token;
    std::uint32_t previous_glyph_id = 0;
    std::uint32_t current_glyph_id = 0;
    std::string previous_diagnostic;
    std::string current_diagnostic;
    std::string diagnostic_reason;
};

struct render_text_font_fallback_shaped_glyph_execution_diff_policy {
    std::ptrdiff_t input_count_delta = 0;
    std::ptrdiff_t execution_count_delta = 0;
    std::ptrdiff_t shaped_count_delta = 0;
    std::ptrdiff_t glyph_count_delta = 0;
    std::ptrdiff_t blocked_input_count_delta = 0;
    std::ptrdiff_t blocked_run_count_delta = 0;
    std::ptrdiff_t atlas_ready_count_delta = 0;
    std::ptrdiff_t missing_cache_key_count_delta = 0;
    std::ptrdiff_t invalid_utf8_count_delta = 0;
    std::ptrdiff_t unsupported_glyph_count_delta = 0;
    std::ptrdiff_t no_selected_face_count_delta = 0;
    std::ptrdiff_t missing_glyph_id_count_delta = 0;
    std::ptrdiff_t fallback_execution_count_delta = 0;
    std::ptrdiff_t glyph_id_offset_execution_count_delta = 0;
    std::ptrdiff_t unique_page_key_count_delta = 0;
    std::ptrdiff_t unique_cache_key_count_delta = 0;
    std::ptrdiff_t unique_selected_face_count_delta = 0;
    std::ptrdiff_t unique_style_token_count_delta = 0;
    std::size_t added_execution_count = 0;
    std::size_t removed_execution_count = 0;
    std::size_t changed_execution_count = 0;
    std::size_t unchanged_execution_count = 0;
    std::size_t status_changed_count = 0;
    std::size_t selected_face_changed_count = 0;
    std::size_t cache_key_changed_count = 0;
    std::size_t page_key_changed_count = 0;
    std::size_t style_token_changed_count = 0;
    std::size_t glyph_id_changed_count = 0;
    std::size_t diagnostic_changed_count = 0;
    std::size_t added_blocked_run_count = 0;
    std::size_t removed_blocked_run_count = 0;
    bool status_counts_changed = false;
    bool selected_face_set_changed = false;
    bool cache_key_set_changed = false;
    bool page_key_set_changed = false;
    bool style_token_set_changed = false;
    bool blocked_runs_changed = false;
    bool glyph_count_changed = false;
};

struct render_text_font_fallback_shaped_glyph_execution_diff_snapshot {
    render_text_font_fallback_shaped_glyph_execution_diff_policy policy;
    std::vector<render_text_font_fallback_shaped_glyph_execution_record_diff> execution_diffs;
    std::vector<std::string> added_execution_keys;
    std::vector<std::string> removed_execution_keys;
    std::vector<std::string> changed_execution_keys;
    std::vector<std::string> added_blocked_run_keys;
    std::vector<std::string> removed_blocked_run_keys;
    std::vector<std::string> diagnostic_reasons;
    std::string diagnostic;

    [[nodiscard]] bool has_changes() const
    {
        return policy.added_execution_count != 0U
            || policy.removed_execution_count != 0U
            || policy.changed_execution_count != 0U
            || policy.added_blocked_run_count != 0U
            || policy.removed_blocked_run_count != 0U
            || policy.status_counts_changed
            || policy.selected_face_set_changed
            || policy.cache_key_set_changed
            || policy.page_key_set_changed
            || policy.style_token_set_changed
            || policy.glyph_count_changed;
    }
};

[[nodiscard]] inline std::string font_fallback_shaping_handoff_stable_page_key_for(
    const render_text_font_fallback_run_snapshot& run)
{
    if (run.selected_face_id == 0U) {
        return {};
    }

    return "font-fallback-shaping-page:v1"
        + std::string{":face="} + std::to_string(run.selected_face_id)
        + ":style=" + run.style_token;
}

[[nodiscard]] inline render_text_font_fallback_shaping_handoff_status
font_fallback_shaping_handoff_status_for(
    const render_text_font_fallback_run_snapshot& run)
{
    if (run.status == render_text_font_fallback_run_status::invalid_utf8 || !run.valid_utf8) {
        return render_text_font_fallback_shaping_handoff_status::invalid_utf8;
    }
    if (run.status == render_text_font_fallback_run_status::missing_glyph || !run.glyph_supported) {
        return render_text_font_fallback_shaping_handoff_status::missing_glyph;
    }
    if (run.selected_face_id == 0U) {
        return render_text_font_fallback_shaping_handoff_status::no_selected_face;
    }

    return render_text_font_fallback_shaping_handoff_status::ready;
}

inline void font_fallback_shaping_handoff_append_unique_key(
    std::vector<std::string>& keys,
    const std::string& key)
{
    if (!key.empty() && std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

inline void font_fallback_shaping_handoff_append_unique_style(
    std::vector<render_style_id>& style_tokens,
    const render_style_id& style_token)
{
    if (!style_token.empty()
        && std::find(style_tokens.begin(), style_tokens.end(), style_token) == style_tokens.end()) {
        style_tokens.push_back(style_token);
    }
}

[[nodiscard]] inline float font_fallback_shaped_glyph_input_line_height_for(
    const render_text_style& style)
{
    return style.line_height > 0.0f ? style.line_height : style.font_size;
}

[[nodiscard]] inline std::size_t font_fallback_shaped_glyph_input_atlas_dimension_for(
    const float value)
{
    if (value <= 1.0f) {
        return 1U;
    }
    return static_cast<std::size_t>(value);
}

[[nodiscard]] inline std::string font_fallback_shaped_glyph_input_stable_key_for(
    const render_text_font_fallback_shaping_handoff_run_snapshot& run,
    const utf8_text_codepoint& scalar,
    const std::size_t glyph_index)
{
    return "font-fallback-shaped-glyph-input:v1"
        + std::string{":run="} + run.stable_run_key
        + ":glyph=" + std::to_string(glyph_index)
        + ":byte=" + std::to_string(scalar.byte_offset)
        + "+" + std::to_string(scalar.byte_count);
}

[[nodiscard]] inline const render_text_font_fallback_chain_plan_item*
find_font_fallback_shaped_glyph_input_item(
    const std::vector<render_text_font_fallback_chain_plan_item>& items,
    const std::size_t item_index)
{
    const auto match = std::find_if(
        items.begin(),
        items.end(),
        [&](const render_text_font_fallback_chain_plan_item& item) {
            return item.item_index == item_index;
        });
    return match == items.end() ? nullptr : &*match;
}

[[nodiscard]] inline render_text_font_fallback_shaped_glyph_input_record
make_render_text_font_fallback_shaped_glyph_input(
    const render_text_font_fallback_shaping_handoff_run_snapshot& run,
    const utf8_text_codepoint& scalar,
    const std::size_t source_codepoint_index,
    const std::size_t glyph_index,
    const render_text_style& style,
    const font_face_catalog& font_catalog)
{
    const font_face_descriptor* selected_face = font_catalog.find_by_id(run.selected_face_id);
    const std::uint32_t glyph_id = selected_face == nullptr
        ? scalar.code_point
        : font_glyph_id_apply_descriptor_mapping(*selected_face, scalar.code_point);
    const std::uint32_t glyph_id_offset =
        selected_face == nullptr ? std::uint32_t{0} : selected_face->glyph_id_offset;
    const float advance = font_shaping_backend_fake_advance_for(style, scalar.code_point);
    const float line_height = font_fallback_shaped_glyph_input_line_height_for(style);
    const bool has_selected_face = selected_face != nullptr && selected_face->id != 0U;
    const bool cacheable = scalar.valid
        && run.glyph_supported
        && has_selected_face
        && advance > 0.0f
        && line_height > 0.0f;
    const glyph_atlas_key cache_key = cacheable
        ? glyph_atlas_key{
            .face_id = run.selected_face_id,
            .glyph_id = glyph_id,
            .pixel_size = static_cast<std::uint32_t>(
                font_fallback_shaped_glyph_input_atlas_dimension_for(line_height)),
        }
        : glyph_atlas_key{};
    const render_text_font_shaping_codepoint_selection font_selection{
        .requested_face_id = run.requested_face_id,
        .resolved_face_id = run.selected_face_id,
        .glyph_id = glyph_id,
        .has_glyph_id = has_selected_face,
        .glyph_id_offset = glyph_id_offset,
        .glyph_id_matches_codepoint = glyph_id == scalar.code_point,
        .glyph_supported = run.glyph_supported && scalar.valid && has_selected_face,
        .used_codepoint_fallback = run.used_fallback,
        .used_fallback_glyph_id = false,
    };

    return render_text_font_fallback_shaped_glyph_input_record{
        .stable_input_key = font_fallback_shaped_glyph_input_stable_key_for(run, scalar, glyph_index),
        .stable_run_key = run.stable_run_key,
        .stable_page_key = run.stable_page_key,
        .item_index = run.item_index,
        .source_run_index = run.source_run_index,
        .fallback_run_index = run.fallback_run_index,
        .glyph_index = glyph_index,
        .source_codepoint_index = source_codepoint_index,
        .style_token = run.style_token,
        .byte_offset = scalar.byte_offset,
        .byte_count = scalar.byte_count,
        .codepoint = scalar.code_point,
        .glyph_id = glyph_id,
        .requested_face_id = run.requested_face_id,
        .selected_face_id = run.selected_face_id,
        .selected_family = selected_face == nullptr ? run.selected_family : selected_face->family,
        .selected_source_uri = selected_face == nullptr ? run.selected_source_uri : selected_face->source_uri,
        .advance_x = advance,
        .line_height = line_height,
        .cache_key = cache_key,
        .has_cache_key = cacheable,
        .cacheable = cacheable,
        .valid_utf8 = scalar.valid,
        .glyph_supported = run.glyph_supported && has_selected_face,
        .cluster_start = scalar.cluster_start,
        .used_fallback = run.used_fallback,
        .glyph_id_from_selection = has_selected_face,
        .glyph_id_matches_codepoint = glyph_id == scalar.code_point,
        .glyph_id_offset = glyph_id_offset,
        .font_selection = font_selection,
        .diagnostic = cacheable
            ? "fallback shaped glyph input is cacheable for fake shaping and atlas handoff"
            : "fallback shaped glyph input is ready for shaping but not cacheable for atlas handoff",
    };
}

[[nodiscard]] inline render_text_font_fallback_shaping_handoff_run_snapshot
make_render_text_font_fallback_shaping_handoff_run(
    const render_text_font_fallback_run_snapshot& run)
{
    const render_text_font_fallback_shaping_handoff_status status =
        font_fallback_shaping_handoff_status_for(run);
    const std::string stable_page_key =
        status == render_text_font_fallback_shaping_handoff_status::ready
            ? font_fallback_shaping_handoff_stable_page_key_for(run)
            : std::string{};

    return render_text_font_fallback_shaping_handoff_run_snapshot{
        .stable_run_key = run.stable_run_key,
        .stable_page_key = stable_page_key,
        .item_index = run.item_index,
        .source_run_index = run.source_run_index,
        .fallback_run_index = run.fallback_run_index,
        .style_token = run.style_token,
        .source_label = run.source_label,
        .byte_offset = run.byte_offset,
        .byte_count = run.byte_count,
        .codepoint_offset = run.codepoint_offset,
        .codepoint_count = run.codepoint_count,
        .first_codepoint = run.first_codepoint,
        .last_codepoint = run.last_codepoint,
        .requested_face_id = run.requested_face_id,
        .selected_face_id = run.selected_face_id,
        .requested_family = run.requested_family,
        .selected_family = run.selected_family,
        .selected_source_uri = run.selected_source_uri,
        .fallback_order = run.fallback_order,
        .attempted_face_ids = run.attempted_face_ids,
        .valid_utf8 = run.valid_utf8,
        .glyph_supported = run.glyph_supported,
        .used_fallback = run.used_fallback,
        .fallback_run_status = run.status,
        .handoff_status = status,
        .diagnostic = status == render_text_font_fallback_shaping_handoff_status::ready
            ? "fallback run is ready for shaping handoff"
            : "fallback run is blocked from shaping handoff: "
                + render_text_font_fallback_shaping_handoff_status_name(status),
    };
}

inline void summarize_render_text_font_fallback_shaping_handoff_policy(
    render_text_font_fallback_shaping_handoff_snapshot& handoff)
{
    handoff.ready_runs.clear();
    handoff.blocked_runs.clear();
    handoff.stable_run_keys.clear();
    handoff.stable_page_keys.clear();
    handoff.style_tokens.clear();
    handoff.policy = render_text_font_fallback_shaping_handoff_policy_snapshot{
        .run_count = handoff.runs.size(),
    };

    for (const render_text_font_fallback_shaping_handoff_run_snapshot& run : handoff.runs) {
        handoff.policy.codepoint_count += run.codepoint_count;
        font_fallback_shaping_handoff_append_unique_key(handoff.stable_run_keys, run.stable_run_key);
        font_fallback_shaping_handoff_append_unique_style(handoff.style_tokens, run.style_token);

        if (run.ready_to_shape()) {
            ++handoff.policy.ready_run_count;
            handoff.policy.ready_codepoint_count += run.codepoint_count;
            if (run.used_fallback) {
                ++handoff.policy.fallback_ready_run_count;
            }
            font_fallback_shaping_handoff_append_unique_key(handoff.stable_page_keys, run.stable_page_key);
            handoff.ready_runs.push_back(run);
            continue;
        }

        ++handoff.policy.blocked_run_count;
        handoff.policy.blocked_codepoint_count += run.codepoint_count;
        switch (run.handoff_status) {
        case render_text_font_fallback_shaping_handoff_status::ready:
            ++handoff.policy.no_selected_face_run_count;
            break;
        case render_text_font_fallback_shaping_handoff_status::missing_glyph:
            ++handoff.policy.missing_glyph_run_count;
            break;
        case render_text_font_fallback_shaping_handoff_status::invalid_utf8:
            ++handoff.policy.invalid_utf8_run_count;
            break;
        case render_text_font_fallback_shaping_handoff_status::no_selected_face:
            ++handoff.policy.no_selected_face_run_count;
            break;
        }
        handoff.blocked_runs.push_back(run);
    }

    handoff.policy.unique_page_key_count = handoff.stable_page_keys.size();
    handoff.policy.unique_style_token_count = handoff.style_tokens.size();
}

[[nodiscard]] inline render_text_font_fallback_shaping_handoff_snapshot
make_render_text_font_fallback_shaping_handoff(
    const render_text_font_fallback_run_plan_snapshot& fallback_run_plan)
{
    render_text_font_fallback_shaping_handoff_snapshot handoff;
    handoff.runs.reserve(fallback_run_plan.runs.size());
    for (const render_text_font_fallback_run_snapshot& run : fallback_run_plan.runs) {
        handoff.runs.push_back(make_render_text_font_fallback_shaping_handoff_run(run));
    }

    summarize_render_text_font_fallback_shaping_handoff_policy(handoff);
    handoff.diagnostic = handoff.ok()
        ? "font fallback shaping handoff has no blocked fallback runs"
        : "font fallback shaping handoff blocked "
            + std::to_string(handoff.policy.blocked_run_count)
            + " fallback run(s) before shaping";
    return handoff;
}

[[nodiscard]] inline render_text_font_fallback_shaping_handoff_snapshot
make_render_text_font_fallback_shaping_handoff(
    const render_text_font_fallback_shaping_handoff_request& request)
{
    return make_render_text_font_fallback_shaping_handoff(request.fallback_run_plan);
}

inline void append_render_text_font_fallback_shaped_glyph_input(
    render_text_font_fallback_shaped_glyph_input_snapshot& snapshot,
    render_text_font_fallback_shaped_glyph_input_record input)
{
    if (input.cacheable) {
        ++snapshot.policy.cacheable_input_count;
    } else {
        ++snapshot.policy.uncacheable_input_count;
    }
    if (input.used_fallback) {
        ++snapshot.policy.fallback_input_count;
    }
    if (input.glyph_id_offset != 0U) {
        ++snapshot.policy.glyph_id_offset_input_count;
    }
    if (input.selected_face_id == 0U || !input.font_selection.has_glyph_id) {
        ++snapshot.policy.no_selected_face_count;
    }
    font_fallback_shaping_handoff_append_unique_key(snapshot.stable_input_keys, input.stable_input_key);
    font_fallback_shaping_handoff_append_unique_key(snapshot.stable_page_keys, input.stable_page_key);
    font_fallback_shaping_handoff_append_unique_style(snapshot.style_tokens, input.style_token);
    snapshot.inputs.push_back(std::move(input));
    snapshot.policy.input_count = snapshot.inputs.size();
    snapshot.policy.unique_page_key_count = snapshot.stable_page_keys.size();
    snapshot.policy.unique_style_token_count = snapshot.style_tokens.size();
}

[[nodiscard]] inline render_text_font_fallback_shaped_glyph_input_snapshot
make_render_text_font_fallback_shaped_glyph_inputs(
    const render_text_font_fallback_shaped_glyph_input_request& request)
{
    render_text_font_fallback_shaped_glyph_input_snapshot snapshot;
    snapshot.policy.handoff_run_count = request.handoff.policy.run_count;
    snapshot.policy.ready_run_count = request.handoff.policy.ready_run_count;
    snapshot.policy.blocked_run_count = request.handoff.policy.blocked_run_count;
    snapshot.blocked_runs = request.handoff.blocked_runs;

    for (const render_text_font_fallback_shaping_handoff_run_snapshot& run : request.handoff.runs) {
        if (!run.ready_to_shape()) {
            if (run.handoff_status == render_text_font_fallback_shaping_handoff_status::no_selected_face) {
                ++snapshot.policy.no_selected_face_count;
            }
            continue;
        }

        const render_text_font_fallback_chain_plan_item* item =
            find_font_fallback_shaped_glyph_input_item(request.items, run.item_index);
        if (item == nullptr || run.source_run_index >= item->text_runs.size()) {
            ++snapshot.policy.missing_source_run_count;
            continue;
        }

        if (item->style_catalog.find(run.style_token) == nullptr) {
            ++snapshot.policy.fallback_style_count;
        }
        const render_text_style& style = item->style_catalog.resolve(run.style_token);
        const render_text_run& source_run = item->text_runs[run.source_run_index];
        const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(source_run.text);
        const std::size_t end_codepoint = run.codepoint_offset + run.codepoint_count;
        if (end_codepoint > codepoints.size()) {
            snapshot.policy.missing_codepoint_count += end_codepoint - codepoints.size();
        }

        std::size_t glyph_index = 0;
        for (std::size_t codepoint_index = run.codepoint_offset;
             codepoint_index < end_codepoint && codepoint_index < codepoints.size();
             ++codepoint_index) {
            append_render_text_font_fallback_shaped_glyph_input(
                snapshot,
                make_render_text_font_fallback_shaped_glyph_input(
                    run,
                    codepoints[codepoint_index],
                    codepoint_index,
                    glyph_index,
                    style,
                    request.font_catalog));
            ++glyph_index;
        }
    }

    snapshot.policy.input_count = snapshot.inputs.size();
    snapshot.policy.unique_page_key_count = snapshot.stable_page_keys.size();
    snapshot.policy.unique_style_token_count = snapshot.style_tokens.size();
    snapshot.diagnostic = snapshot.ok()
        ? "fallback shaped glyph inputs are ready for deterministic fake shaping handoff"
        : "fallback shaped glyph input handoff has "
            + std::to_string(snapshot.policy.blocked_run_count)
            + " blocked run(s), "
            + std::to_string(snapshot.policy.missing_source_run_count)
            + " missing source run(s), and "
            + std::to_string(snapshot.policy.missing_codepoint_count)
            + " missing source codepoint(s)";
    return snapshot;
}

[[nodiscard]] inline render_text_font_fallback_shaped_glyph_input_snapshot
make_render_text_font_fallback_shaped_glyph_inputs(
    const render_text_font_fallback_shaping_handoff_snapshot& handoff,
    std::vector<render_text_font_fallback_chain_plan_item> items,
    font_face_catalog font_catalog)
{
    return make_render_text_font_fallback_shaped_glyph_inputs(
        render_text_font_fallback_shaped_glyph_input_request{
            .handoff = handoff,
            .items = std::move(items),
            .font_catalog = std::move(font_catalog),
        });
}

[[nodiscard]] inline render_text_font_fallback_shaped_glyph_input_snapshot
make_render_text_font_fallback_shaped_glyph_inputs(
    const render_text_font_fallback_run_plan_snapshot& fallback_run_plan,
    std::vector<render_text_font_fallback_chain_plan_item> items,
    font_face_catalog font_catalog)
{
    return make_render_text_font_fallback_shaped_glyph_inputs(
        make_render_text_font_fallback_shaping_handoff(fallback_run_plan),
        std::move(items),
        std::move(font_catalog));
}

[[nodiscard]] inline std::string font_fallback_shaped_glyph_execution_stable_key_for(
    const render_text_font_fallback_shaped_glyph_input_record& input)
{
    return "font-fallback-shaped-glyph-execution:v1"
        + std::string{":input="} + input.stable_input_key;
}

[[nodiscard]] inline render_text_font_fallback_shaped_glyph_execution_status
font_fallback_shaped_glyph_execution_status_for(
    const render_text_font_fallback_shaped_glyph_input_record& input)
{
    if (!input.valid_utf8) {
        return render_text_font_fallback_shaped_glyph_execution_status::invalid_utf8;
    }
    if (!input.glyph_supported) {
        return render_text_font_fallback_shaped_glyph_execution_status::unsupported_glyph;
    }
    if (input.selected_face_id == 0U || input.font_selection.resolved_face_id == 0U) {
        return render_text_font_fallback_shaped_glyph_execution_status::no_selected_face;
    }
    if (!input.font_selection.has_glyph_id || input.glyph_id == 0U) {
        return render_text_font_fallback_shaped_glyph_execution_status::missing_glyph_id;
    }

    return render_text_font_fallback_shaped_glyph_execution_status::shaped;
}

[[nodiscard]] inline render_text_shaped_glyph
make_render_text_font_fallback_shaped_glyph_for_execution(
    const render_text_font_fallback_shaped_glyph_input_record& input)
{
    return render_text_shaped_glyph{
        .run_index = input.source_run_index,
        .glyph_index = input.source_codepoint_index,
        .byte_offset = input.byte_offset,
        .byte_count = input.byte_count,
        .cluster_byte_offset = input.byte_offset,
        .cluster_byte_count = input.byte_count,
        .cluster_codepoint_offset = input.source_codepoint_index,
        .cluster_codepoint_count = 1,
        .codepoint = input.codepoint,
        .glyph_id = input.glyph_id,
        .requested_face_id = input.requested_face_id,
        .resolved_face_id = input.selected_face_id,
        .advance_x = input.advance_x,
        .valid_utf8 = input.valid_utf8,
        .cluster_start = input.cluster_start,
        .glyph_supported = input.glyph_supported,
        .used_codepoint_fallback = input.used_fallback,
        .used_fallback_glyph_id = false,
        .glyph_id_from_selection = input.glyph_id_from_selection,
        .glyph_id_offset = input.glyph_id_offset,
        .glyph_id_matches_codepoint = input.glyph_id_matches_codepoint,
        .zero_advance = input.advance_x <= 0.0f,
        .combining_mark = is_utf8_combining_mark(input.codepoint),
    };
}

[[nodiscard]] inline render_text_font_fallback_shaped_glyph_execution_record
execute_render_text_font_fallback_shaped_glyph_input(
    const render_text_font_fallback_shaped_glyph_input_record& input)
{
    const render_text_font_fallback_shaped_glyph_execution_status status =
        font_fallback_shaped_glyph_execution_status_for(input);

    return render_text_font_fallback_shaped_glyph_execution_record{
        .stable_execution_key = font_fallback_shaped_glyph_execution_stable_key_for(input),
        .stable_input_key = input.stable_input_key,
        .stable_run_key = input.stable_run_key,
        .stable_page_key = input.stable_page_key,
        .item_index = input.item_index,
        .source_run_index = input.source_run_index,
        .fallback_run_index = input.fallback_run_index,
        .glyph_index = input.glyph_index,
        .source_codepoint_index = input.source_codepoint_index,
        .style_token = input.style_token,
        .byte_offset = input.byte_offset,
        .byte_count = input.byte_count,
        .codepoint = input.codepoint,
        .glyph_id = input.glyph_id,
        .requested_face_id = input.requested_face_id,
        .selected_face_id = input.selected_face_id,
        .advance_x = input.advance_x,
        .line_height = input.line_height,
        .cache_key = input.cache_key,
        .has_cache_key = input.has_cache_key,
        .cacheable = input.cacheable,
        .valid_utf8 = input.valid_utf8,
        .glyph_supported = input.glyph_supported,
        .cluster_start = input.cluster_start,
        .used_fallback = input.used_fallback,
        .glyph_id_from_selection = input.glyph_id_from_selection,
        .glyph_id_matches_codepoint = input.glyph_id_matches_codepoint,
        .glyph_id_offset = input.glyph_id_offset,
        .status = status,
        .shaped_glyph = make_render_text_font_fallback_shaped_glyph_for_execution(input),
        .diagnostic = status == render_text_font_fallback_shaped_glyph_execution_status::shaped
            ? "fallback shaped glyph input executed deterministically"
            : "fallback shaped glyph input execution blocked: "
                + render_text_font_fallback_shaped_glyph_execution_status_name(status),
    };
}

inline void font_fallback_shaped_glyph_execution_append_unique_face(
    std::vector<font_face_id>& face_ids,
    const font_face_id face_id)
{
    if (face_id != 0U && std::find(face_ids.begin(), face_ids.end(), face_id) == face_ids.end()) {
        face_ids.push_back(face_id);
    }
}

inline void font_fallback_shaped_glyph_execution_append_unique_cache_key(
    std::vector<glyph_atlas_key>& cache_keys,
    const glyph_atlas_key& cache_key)
{
    if (cache_key.face_id != 0U
        && std::find(cache_keys.begin(), cache_keys.end(), cache_key) == cache_keys.end()) {
        cache_keys.push_back(cache_key);
    }
}

inline void count_render_text_font_fallback_shaped_glyph_execution_status(
    render_text_font_fallback_shaped_glyph_execution_policy_snapshot& policy,
    const render_text_font_fallback_shaped_glyph_execution_status status)
{
    switch (status) {
    case render_text_font_fallback_shaped_glyph_execution_status::shaped:
        ++policy.shaped_count;
        return;
    case render_text_font_fallback_shaped_glyph_execution_status::invalid_utf8:
        ++policy.invalid_utf8_count;
        return;
    case render_text_font_fallback_shaped_glyph_execution_status::unsupported_glyph:
        ++policy.unsupported_glyph_count;
        return;
    case render_text_font_fallback_shaped_glyph_execution_status::no_selected_face:
        ++policy.no_selected_face_count;
        return;
    case render_text_font_fallback_shaped_glyph_execution_status::missing_glyph_id:
        ++policy.missing_glyph_id_count;
        return;
    }
}

inline void append_render_text_font_fallback_shaped_glyph_execution(
    render_text_font_fallback_shaped_glyph_execution_snapshot& snapshot,
    render_text_font_fallback_shaped_glyph_execution_record execution)
{
    count_render_text_font_fallback_shaped_glyph_execution_status(snapshot.policy, execution.status);
    if (execution.ready_for_glyph_atlas()) {
        ++snapshot.policy.atlas_ready_count;
        font_fallback_shaped_glyph_execution_append_unique_cache_key(
            snapshot.cache_keys,
            execution.cache_key);
    } else if (execution.executed()) {
        ++snapshot.policy.missing_cache_key_count;
    }
    if (execution.used_fallback) {
        ++snapshot.policy.fallback_execution_count;
    }
    if (execution.glyph_id_offset != 0U) {
        ++snapshot.policy.glyph_id_offset_execution_count;
    }

    font_fallback_shaping_handoff_append_unique_key(
        snapshot.stable_execution_keys,
        execution.stable_execution_key);
    font_fallback_shaping_handoff_append_unique_key(snapshot.stable_input_keys, execution.stable_input_key);
    font_fallback_shaping_handoff_append_unique_key(snapshot.stable_page_keys, execution.stable_page_key);
    font_fallback_shaped_glyph_execution_append_unique_face(
        snapshot.selected_face_ids,
        execution.selected_face_id);
    font_fallback_shaping_handoff_append_unique_style(snapshot.style_tokens, execution.style_token);
    snapshot.executions.push_back(std::move(execution));
    snapshot.policy.execution_count = snapshot.executions.size();
    snapshot.policy.unique_input_key_count = snapshot.stable_input_keys.size();
    snapshot.policy.unique_page_key_count = snapshot.stable_page_keys.size();
    snapshot.policy.unique_cache_key_count = snapshot.cache_keys.size();
    snapshot.policy.unique_selected_face_count = snapshot.selected_face_ids.size();
    snapshot.policy.unique_style_token_count = snapshot.style_tokens.size();
}

[[nodiscard]] inline render_text_font_fallback_shaped_glyph_execution_snapshot
execute_render_text_font_fallback_shaped_glyph_inputs(
    const render_text_font_fallback_shaped_glyph_execution_request& request)
{
    render_text_font_fallback_shaped_glyph_execution_snapshot snapshot;
    snapshot.policy.input_count = request.inputs.policy.input_count;
    snapshot.policy.blocked_input_count = request.inputs.policy.blocked_run_count
        + request.inputs.policy.missing_source_run_count
        + request.inputs.policy.missing_codepoint_count
        + request.inputs.policy.no_selected_face_count;
    snapshot.blocked_runs = request.inputs.blocked_runs;

    snapshot.executions.reserve(request.inputs.inputs.size());
    for (const render_text_font_fallback_shaped_glyph_input_record& input : request.inputs.inputs) {
        append_render_text_font_fallback_shaped_glyph_execution(
            snapshot,
            execute_render_text_font_fallback_shaped_glyph_input(input));
    }

    snapshot.policy.execution_count = snapshot.executions.size();
    snapshot.policy.unique_input_key_count = snapshot.stable_input_keys.size();
    snapshot.policy.unique_page_key_count = snapshot.stable_page_keys.size();
    snapshot.policy.unique_cache_key_count = snapshot.cache_keys.size();
    snapshot.policy.unique_selected_face_count = snapshot.selected_face_ids.size();
    snapshot.policy.unique_style_token_count = snapshot.style_tokens.size();
    snapshot.diagnostic = snapshot.ok()
        ? "fallback shaped glyph inputs executed deterministically"
        : "fallback shaped glyph execution has "
            + std::to_string(snapshot.policy.blocked_input_count)
            + " blocked input(s) and "
            + std::to_string(snapshot.policy.execution_count - snapshot.policy.shaped_count)
            + " unshaped execution record(s)";
    return snapshot;
}

[[nodiscard]] inline render_text_font_fallback_shaped_glyph_execution_snapshot
execute_render_text_font_fallback_shaped_glyph_inputs(
    const render_text_font_fallback_shaped_glyph_input_snapshot& inputs)
{
    return execute_render_text_font_fallback_shaped_glyph_inputs(
        render_text_font_fallback_shaped_glyph_execution_request{
            .inputs = inputs,
        });
}

[[nodiscard]] inline std::ptrdiff_t font_fallback_shaped_glyph_execution_count_delta(
    const std::size_t before,
    const std::size_t after)
{
    return static_cast<std::ptrdiff_t>(after) - static_cast<std::ptrdiff_t>(before);
}

inline void font_fallback_shaped_glyph_execution_append_unique_reason(
    std::vector<std::string>& reasons,
    const std::string& reason)
{
    if (!reason.empty() && std::find(reasons.begin(), reasons.end(), reason) == reasons.end()) {
        reasons.push_back(reason);
    }
}

[[nodiscard]] inline const render_text_font_fallback_shaped_glyph_execution_record*
find_render_text_font_fallback_shaped_glyph_execution_by_key(
    const std::vector<render_text_font_fallback_shaped_glyph_execution_record>& executions,
    const std::string& stable_execution_key)
{
    const auto match = std::find_if(
        executions.begin(),
        executions.end(),
        [&](const render_text_font_fallback_shaped_glyph_execution_record& execution) {
            return execution.stable_execution_key == stable_execution_key;
        });
    return match == executions.end() ? nullptr : &*match;
}

inline void font_fallback_shaped_glyph_execution_append_unique_execution_key(
    std::vector<std::string>& keys,
    const std::string& key)
{
    font_fallback_shaping_handoff_append_unique_key(keys, key);
}

[[nodiscard]] inline std::vector<std::string> font_fallback_shaped_glyph_execution_blocked_run_keys(
    const render_text_font_fallback_shaped_glyph_execution_snapshot& snapshot)
{
    std::vector<std::string> keys;
    keys.reserve(snapshot.blocked_runs.size());
    for (const render_text_font_fallback_shaping_handoff_run_snapshot& run : snapshot.blocked_runs) {
        font_fallback_shaping_handoff_append_unique_key(keys, run.stable_run_key);
    }
    return keys;
}

[[nodiscard]] inline bool font_fallback_shaped_glyph_execution_key_list_contains(
    const std::vector<std::string>& keys,
    const std::string& key)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

[[nodiscard]] inline bool render_text_font_fallback_shaped_glyph_execution_records_equal(
    const render_text_font_fallback_shaped_glyph_execution_record& lhs,
    const render_text_font_fallback_shaped_glyph_execution_record& rhs)
{
    return lhs.stable_execution_key == rhs.stable_execution_key
        && lhs.status == rhs.status
        && lhs.selected_face_id == rhs.selected_face_id
        && lhs.cache_key == rhs.cache_key
        && lhs.stable_page_key == rhs.stable_page_key
        && lhs.style_token == rhs.style_token
        && lhs.glyph_id == rhs.glyph_id
        && lhs.codepoint == rhs.codepoint
        && lhs.byte_offset == rhs.byte_offset
        && lhs.byte_count == rhs.byte_count
        && lhs.has_cache_key == rhs.has_cache_key
        && lhs.cacheable == rhs.cacheable
        && lhs.glyph_supported == rhs.glyph_supported
        && lhs.used_fallback == rhs.used_fallback
        && lhs.diagnostic == rhs.diagnostic;
}

[[nodiscard]] inline std::string font_fallback_shaped_glyph_execution_diff_reason_for(
    const render_text_font_fallback_shaped_glyph_execution_record_diff& diff)
{
    if (diff.added) {
        return "added execution " + diff.stable_execution_key
            + " with status "
            + render_text_font_fallback_shaped_glyph_execution_status_name(diff.current_status);
    }
    if (diff.removed) {
        return "removed execution " + diff.stable_execution_key
            + " with status "
            + render_text_font_fallback_shaped_glyph_execution_status_name(diff.previous_status);
    }
    if (diff.status_changed) {
        return "execution " + diff.stable_execution_key
            + " status changed from "
            + render_text_font_fallback_shaped_glyph_execution_status_name(diff.previous_status)
            + " to "
            + render_text_font_fallback_shaped_glyph_execution_status_name(diff.current_status);
    }
    if (diff.selected_face_changed) {
        return "execution " + diff.stable_execution_key
            + " selected face changed from "
            + std::to_string(diff.previous_selected_face_id)
            + " to "
            + std::to_string(diff.current_selected_face_id);
    }
    if (diff.cache_key_changed) {
        return "execution " + diff.stable_execution_key + " cache key changed";
    }
    if (diff.page_key_changed) {
        return "execution " + diff.stable_execution_key + " page key changed";
    }
    if (diff.style_token_changed) {
        return "execution " + diff.stable_execution_key + " style token changed";
    }
    if (diff.glyph_id_changed) {
        return "execution " + diff.stable_execution_key
            + " glyph id changed from "
            + std::to_string(diff.previous_glyph_id)
            + " to "
            + std::to_string(diff.current_glyph_id);
    }
    if (diff.diagnostic_changed) {
        return "execution " + diff.stable_execution_key + " diagnostic changed";
    }
    return {};
}

[[nodiscard]] inline render_text_font_fallback_shaped_glyph_execution_record_diff
diff_render_text_font_fallback_shaped_glyph_execution_records(
    const render_text_font_fallback_shaped_glyph_execution_record* before,
    const render_text_font_fallback_shaped_glyph_execution_record* after,
    const std::string& stable_execution_key)
{
    if (before == nullptr && after != nullptr) {
        render_text_font_fallback_shaped_glyph_execution_record_diff diff{
            .stable_execution_key = stable_execution_key,
            .added = true,
            .changed = true,
            .current_status = after->status,
            .current_selected_face_id = after->selected_face_id,
            .current_cache_key = after->cache_key,
            .current_page_key = after->stable_page_key,
            .current_style_token = after->style_token,
            .current_glyph_id = after->glyph_id,
            .current_diagnostic = after->diagnostic,
        };
        diff.diagnostic_reason = font_fallback_shaped_glyph_execution_diff_reason_for(diff);
        return diff;
    }
    if (before != nullptr && after == nullptr) {
        render_text_font_fallback_shaped_glyph_execution_record_diff diff{
            .stable_execution_key = stable_execution_key,
            .removed = true,
            .changed = true,
            .previous_status = before->status,
            .previous_selected_face_id = before->selected_face_id,
            .previous_cache_key = before->cache_key,
            .previous_page_key = before->stable_page_key,
            .previous_style_token = before->style_token,
            .previous_glyph_id = before->glyph_id,
            .previous_diagnostic = before->diagnostic,
        };
        diff.diagnostic_reason = font_fallback_shaped_glyph_execution_diff_reason_for(diff);
        return diff;
    }
    if (before == nullptr || after == nullptr) {
        return render_text_font_fallback_shaped_glyph_execution_record_diff{
            .stable_execution_key = stable_execution_key,
        };
    }

    render_text_font_fallback_shaped_glyph_execution_record_diff diff{
        .stable_execution_key = stable_execution_key,
        .status_changed = before->status != after->status,
        .selected_face_changed = before->selected_face_id != after->selected_face_id,
        .cache_key_changed = !(before->cache_key == after->cache_key),
        .page_key_changed = before->stable_page_key != after->stable_page_key,
        .style_token_changed = before->style_token != after->style_token,
        .glyph_id_changed = before->glyph_id != after->glyph_id,
        .diagnostic_changed = before->diagnostic != after->diagnostic,
        .previous_status = before->status,
        .current_status = after->status,
        .previous_selected_face_id = before->selected_face_id,
        .current_selected_face_id = after->selected_face_id,
        .previous_cache_key = before->cache_key,
        .current_cache_key = after->cache_key,
        .previous_page_key = before->stable_page_key,
        .current_page_key = after->stable_page_key,
        .previous_style_token = before->style_token,
        .current_style_token = after->style_token,
        .previous_glyph_id = before->glyph_id,
        .current_glyph_id = after->glyph_id,
        .previous_diagnostic = before->diagnostic,
        .current_diagnostic = after->diagnostic,
    };
    diff.changed = !render_text_font_fallback_shaped_glyph_execution_records_equal(*before, *after);
    diff.diagnostic_reason = font_fallback_shaped_glyph_execution_diff_reason_for(diff);
    return diff;
}

inline void summarize_render_text_font_fallback_shaped_glyph_execution_diff_policy(
    render_text_font_fallback_shaped_glyph_execution_diff_snapshot& diff,
    const render_text_font_fallback_shaped_glyph_execution_snapshot& before,
    const render_text_font_fallback_shaped_glyph_execution_snapshot& after)
{
    diff.policy.input_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.input_count,
        after.policy.input_count);
    diff.policy.execution_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.execution_count,
        after.policy.execution_count);
    diff.policy.shaped_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.shaped_count,
        after.policy.shaped_count);
    diff.policy.glyph_count_delta = diff.policy.shaped_count_delta;
    diff.policy.blocked_input_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.blocked_input_count,
        after.policy.blocked_input_count);
    diff.policy.blocked_run_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.blocked_runs.size(),
        after.blocked_runs.size());
    diff.policy.atlas_ready_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.atlas_ready_count,
        after.policy.atlas_ready_count);
    diff.policy.missing_cache_key_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.missing_cache_key_count,
        after.policy.missing_cache_key_count);
    diff.policy.invalid_utf8_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.invalid_utf8_count,
        after.policy.invalid_utf8_count);
    diff.policy.unsupported_glyph_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.unsupported_glyph_count,
        after.policy.unsupported_glyph_count);
    diff.policy.no_selected_face_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.no_selected_face_count,
        after.policy.no_selected_face_count);
    diff.policy.missing_glyph_id_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.missing_glyph_id_count,
        after.policy.missing_glyph_id_count);
    diff.policy.fallback_execution_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.fallback_execution_count,
        after.policy.fallback_execution_count);
    diff.policy.glyph_id_offset_execution_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.glyph_id_offset_execution_count,
        after.policy.glyph_id_offset_execution_count);
    diff.policy.unique_page_key_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.unique_page_key_count,
        after.policy.unique_page_key_count);
    diff.policy.unique_cache_key_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.unique_cache_key_count,
        after.policy.unique_cache_key_count);
    diff.policy.unique_selected_face_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.unique_selected_face_count,
        after.policy.unique_selected_face_count);
    diff.policy.unique_style_token_count_delta = font_fallback_shaped_glyph_execution_count_delta(
        before.policy.unique_style_token_count,
        after.policy.unique_style_token_count);

    diff.policy.status_counts_changed = diff.policy.shaped_count_delta != 0
        || diff.policy.invalid_utf8_count_delta != 0
        || diff.policy.unsupported_glyph_count_delta != 0
        || diff.policy.no_selected_face_count_delta != 0
        || diff.policy.missing_glyph_id_count_delta != 0;
    diff.policy.selected_face_set_changed = before.selected_face_ids != after.selected_face_ids;
    diff.policy.cache_key_set_changed = before.cache_keys != after.cache_keys;
    diff.policy.page_key_set_changed = before.stable_page_keys != after.stable_page_keys;
    diff.policy.style_token_set_changed = before.style_tokens != after.style_tokens;
    diff.policy.glyph_count_changed = diff.policy.glyph_count_delta != 0;
}

[[nodiscard]] inline render_text_font_fallback_shaped_glyph_execution_diff_snapshot
diff_render_text_font_fallback_shaped_glyph_execution_snapshots(
    const render_text_font_fallback_shaped_glyph_execution_snapshot& before,
    const render_text_font_fallback_shaped_glyph_execution_snapshot& after)
{
    render_text_font_fallback_shaped_glyph_execution_diff_snapshot diff;
    summarize_render_text_font_fallback_shaped_glyph_execution_diff_policy(diff, before, after);

    std::vector<std::string> execution_keys;
    for (const render_text_font_fallback_shaped_glyph_execution_record& execution : before.executions) {
        font_fallback_shaped_glyph_execution_append_unique_execution_key(
            execution_keys,
            execution.stable_execution_key);
    }
    for (const render_text_font_fallback_shaped_glyph_execution_record& execution : after.executions) {
        font_fallback_shaped_glyph_execution_append_unique_execution_key(
            execution_keys,
            execution.stable_execution_key);
    }

    for (const std::string& key : execution_keys) {
        const render_text_font_fallback_shaped_glyph_execution_record* before_execution =
            find_render_text_font_fallback_shaped_glyph_execution_by_key(before.executions, key);
        const render_text_font_fallback_shaped_glyph_execution_record* after_execution =
            find_render_text_font_fallback_shaped_glyph_execution_by_key(after.executions, key);
        render_text_font_fallback_shaped_glyph_execution_record_diff execution_diff =
            diff_render_text_font_fallback_shaped_glyph_execution_records(
                before_execution,
                after_execution,
                key);

        if (execution_diff.added) {
            ++diff.policy.added_execution_count;
            diff.added_execution_keys.push_back(key);
        } else if (execution_diff.removed) {
            ++diff.policy.removed_execution_count;
            diff.removed_execution_keys.push_back(key);
        } else if (execution_diff.changed) {
            ++diff.policy.changed_execution_count;
            diff.changed_execution_keys.push_back(key);
        } else {
            ++diff.policy.unchanged_execution_count;
        }

        if (execution_diff.status_changed) {
            ++diff.policy.status_changed_count;
        }
        if (execution_diff.selected_face_changed) {
            ++diff.policy.selected_face_changed_count;
        }
        if (execution_diff.cache_key_changed) {
            ++diff.policy.cache_key_changed_count;
        }
        if (execution_diff.page_key_changed) {
            ++diff.policy.page_key_changed_count;
        }
        if (execution_diff.style_token_changed) {
            ++diff.policy.style_token_changed_count;
        }
        if (execution_diff.glyph_id_changed) {
            ++diff.policy.glyph_id_changed_count;
        }
        if (execution_diff.diagnostic_changed) {
            ++diff.policy.diagnostic_changed_count;
        }
        font_fallback_shaped_glyph_execution_append_unique_reason(
            diff.diagnostic_reasons,
            execution_diff.diagnostic_reason);
        diff.execution_diffs.push_back(std::move(execution_diff));
    }

    const std::vector<std::string> before_blocked =
        font_fallback_shaped_glyph_execution_blocked_run_keys(before);
    const std::vector<std::string> after_blocked =
        font_fallback_shaped_glyph_execution_blocked_run_keys(after);
    for (const std::string& key : before_blocked) {
        if (!font_fallback_shaped_glyph_execution_key_list_contains(after_blocked, key)) {
            diff.removed_blocked_run_keys.push_back(key);
        }
    }
    for (const std::string& key : after_blocked) {
        if (!font_fallback_shaped_glyph_execution_key_list_contains(before_blocked, key)) {
            diff.added_blocked_run_keys.push_back(key);
        }
    }
    diff.policy.added_blocked_run_count = diff.added_blocked_run_keys.size();
    diff.policy.removed_blocked_run_count = diff.removed_blocked_run_keys.size();
    diff.policy.blocked_runs_changed =
        diff.policy.added_blocked_run_count != 0U || diff.policy.removed_blocked_run_count != 0U;
    for (const std::string& key : diff.added_blocked_run_keys) {
        font_fallback_shaped_glyph_execution_append_unique_reason(
            diff.diagnostic_reasons,
            "added blocked run " + key);
    }
    for (const std::string& key : diff.removed_blocked_run_keys) {
        font_fallback_shaped_glyph_execution_append_unique_reason(
            diff.diagnostic_reasons,
            "removed blocked run " + key);
    }

    diff.diagnostic = diff.has_changes()
        ? "fallback shaped glyph execution snapshots changed"
        : "fallback shaped glyph execution snapshots match";
    return diff;
}

} // namespace quiz_vulkan::render
