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

} // namespace quiz_vulkan::render
