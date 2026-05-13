#pragma once

#include "render/text/font_fallback_run_planning_diagnostics.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
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

} // namespace quiz_vulkan::render
