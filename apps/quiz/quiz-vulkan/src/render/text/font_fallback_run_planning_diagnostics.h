#pragma once

#include "render/text/font_coverage_run_segmentation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_fallback_run_status {
    covered,
    missing_glyph,
    invalid_utf8,
};

inline std::string render_text_font_fallback_run_status_name(
    const render_text_font_fallback_run_status status)
{
    switch (status) {
    case render_text_font_fallback_run_status::covered:
        return "covered";
    case render_text_font_fallback_run_status::missing_glyph:
        return "missing_glyph";
    case render_text_font_fallback_run_status::invalid_utf8:
        return "invalid_utf8";
    }

    return "unknown";
}

struct render_text_font_fallback_run_snapshot {
    std::string stable_run_key;
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
    render_text_font_fallback_run_status status =
        render_text_font_fallback_run_status::missing_glyph;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_fallback_run_status::covered
            && valid_utf8
            && glyph_supported
            && selected_face_id != 0U;
    }

    bool missing() const
    {
        return status == render_text_font_fallback_run_status::missing_glyph
            || status == render_text_font_fallback_run_status::invalid_utf8;
    }
};

struct render_text_font_fallback_run_plan_policy_snapshot {
    std::size_t item_count = 0;
    std::size_t source_run_count = 0;
    std::size_t fallback_run_count = 0;
    std::size_t codepoint_count = 0;
    std::size_t covered_codepoint_count = 0;
    std::size_t fallback_codepoint_count = 0;
    std::size_t missing_glyph_count = 0;
    std::size_t invalid_utf8_count = 0;
    std::size_t missing_run_count = 0;
    std::size_t invalid_run_count = 0;
    std::size_t unique_selected_face_count = 0;
    bool has_missing_ranges = false;
    bool has_invalid_utf8 = false;
};

struct render_text_font_fallback_run_plan_request {
    std::vector<render_text_font_fallback_chain_plan_item> items;
};

struct render_text_font_fallback_run_plan_snapshot {
    std::vector<render_text_font_fallback_run_snapshot> runs;
    std::vector<render_text_font_fallback_run_snapshot> missing_runs;
    std::vector<font_face_id> selected_face_order;
    render_text_font_fallback_run_plan_policy_snapshot policy;
    std::string diagnostic;

    bool ok() const
    {
        return policy.missing_glyph_count == 0U && policy.invalid_utf8_count == 0U;
    }

    bool has_missing_ranges() const
    {
        return !missing_runs.empty();
    }
};

struct render_text_font_fallback_run_diff_policy_snapshot {
    std::ptrdiff_t fallback_run_count_delta = 0;
    std::ptrdiff_t covered_codepoint_count_delta = 0;
    std::ptrdiff_t fallback_codepoint_count_delta = 0;
    std::ptrdiff_t missing_glyph_count_delta = 0;
    std::ptrdiff_t invalid_utf8_count_delta = 0;
    std::ptrdiff_t unique_selected_face_count_delta = 0;
    bool selected_face_order_changed = false;
    bool missing_state_changed = false;
};

struct render_text_font_fallback_run_diff_snapshot {
    std::string stable_run_key;
    bool added = false;
    bool removed = false;
    bool changed = false;
    bool status_changed = false;
    bool selected_face_changed = false;
    bool fallback_changed = false;
    bool missing_changed = false;
    render_text_font_fallback_run_status previous_status =
        render_text_font_fallback_run_status::missing_glyph;
    render_text_font_fallback_run_status current_status =
        render_text_font_fallback_run_status::missing_glyph;
    font_face_id previous_selected_face_id = 0;
    font_face_id current_selected_face_id = 0;
};

struct render_text_font_fallback_run_plan_diff_snapshot {
    render_text_font_fallback_run_diff_policy_snapshot policy;
    std::vector<render_text_font_fallback_run_diff_snapshot> run_diffs;
    std::vector<std::string> added_run_keys;
    std::vector<std::string> removed_run_keys;
    std::vector<std::string> changed_run_keys;
    std::size_t added_run_count = 0;
    std::size_t removed_run_count = 0;
    std::size_t changed_run_count = 0;
    std::size_t unchanged_run_count = 0;
    std::string diagnostic;

    bool has_changes() const
    {
        return added_run_count != 0U
            || removed_run_count != 0U
            || changed_run_count != 0U
            || policy.selected_face_order_changed
            || policy.missing_state_changed;
    }
};

inline std::string font_fallback_run_stable_key_for(
    const render_text_font_fallback_run_snapshot& run)
{
    return "font-fallback-run:v1"
        + std::string{":item="} + std::to_string(run.item_index)
        + ":source-run=" + std::to_string(run.source_run_index)
        + ":style=" + run.style_token
        + ":bytes=" + std::to_string(run.byte_offset)
        + "+" + std::to_string(run.byte_count)
        + ":codepoints=" + std::to_string(run.codepoint_offset)
        + "+" + std::to_string(run.codepoint_count);
}

inline bool font_fallback_run_attempted_faces_equal(
    const std::vector<font_face_id>& lhs,
    const std::vector<font_face_id>& rhs)
{
    return lhs == rhs;
}

inline bool font_fallback_runs_can_merge(
    const render_text_font_fallback_run_snapshot& lhs,
    const render_text_font_fallback_run_snapshot& rhs)
{
    return lhs.item_index == rhs.item_index
        && lhs.source_run_index == rhs.source_run_index
        && lhs.style_token == rhs.style_token
        && lhs.byte_offset + lhs.byte_count == rhs.byte_offset
        && lhs.codepoint_offset + lhs.codepoint_count == rhs.codepoint_offset
        && lhs.requested_face_id == rhs.requested_face_id
        && lhs.selected_face_id == rhs.selected_face_id
        && lhs.fallback_order == rhs.fallback_order
        && lhs.valid_utf8 == rhs.valid_utf8
        && lhs.glyph_supported == rhs.glyph_supported
        && lhs.used_fallback == rhs.used_fallback
        && lhs.status == rhs.status
        && font_fallback_run_attempted_faces_equal(lhs.attempted_face_ids, rhs.attempted_face_ids);
}

inline void font_fallback_run_refresh_stable_key(
    render_text_font_fallback_run_snapshot& run)
{
    run.stable_run_key = font_fallback_run_stable_key_for(run);
}

inline void append_render_text_font_fallback_run(
    render_text_font_fallback_run_plan_snapshot& plan,
    render_text_font_fallback_run_snapshot run)
{
    if (!plan.runs.empty() && font_fallback_runs_can_merge(plan.runs.back(), run)) {
        render_text_font_fallback_run_snapshot& previous = plan.runs.back();
        previous.byte_count = (run.byte_offset + run.byte_count) - previous.byte_offset;
        previous.codepoint_count += run.codepoint_count;
        previous.last_codepoint = run.last_codepoint;
        previous.fallback_run_index = plan.runs.size() - 1U;
        font_fallback_run_refresh_stable_key(previous);
        return;
    }

    run.fallback_run_index = plan.runs.size();
    font_fallback_run_refresh_stable_key(run);
    plan.runs.push_back(std::move(run));
}

inline const font_face_descriptor* font_fallback_run_select_face_for_codepoint(
    const std::vector<const font_face_descriptor*>& candidate_faces,
    const std::uint32_t codepoint,
    std::size_t& selected_order)
{
    for (std::size_t index = 0; index < candidate_faces.size(); ++index) {
        const font_face_descriptor* face = candidate_faces[index];
        if (face != nullptr && face->supports_codepoint(codepoint)) {
            selected_order = index;
            return face;
        }
    }

    selected_order = candidate_faces.size();
    return nullptr;
}

inline const font_face_descriptor* font_fallback_run_select_face_for_cluster(
    const std::vector<const font_face_descriptor*>& candidate_faces,
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster,
    std::size_t& selected_order)
{
    for (std::size_t index = 0; index < candidate_faces.size(); ++index) {
        const font_face_descriptor* face = candidate_faces[index];
        if (face != nullptr && font_coverage_face_supports_utf8_cluster(*face, codepoints, cluster)) {
            selected_order = index;
            return face;
        }
    }

    selected_order = candidate_faces.size();
    return nullptr;
}

inline std::uint32_t font_fallback_run_cluster_last_codepoint(
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster)
{
    const std::size_t cluster_end = cluster.codepoint_offset + cluster.codepoint_count;
    if (cluster_end == 0U || cluster_end > codepoints.size()) {
        return utf8_replacement_codepoint;
    }
    const utf8_text_codepoint& scalar = codepoints[cluster_end - 1U];
    return scalar.valid ? scalar.code_point : utf8_replacement_codepoint;
}

inline render_text_font_fallback_run_snapshot make_render_text_font_fallback_run_for_scalar(
    const utf8_text_codepoint& scalar,
    const std::size_t item_index,
    const std::size_t source_run_index,
    const render_text_run& source_run,
    const render_text_style& style,
    const std::string& source_label,
    const font_face_descriptor* requested_face,
    const std::vector<const font_face_descriptor*>& candidate_faces,
    const std::vector<font_face_id>& attempted_face_ids,
    const std::size_t codepoint_offset)
{
    if (!scalar.valid) {
        return render_text_font_fallback_run_snapshot{
            .item_index = item_index,
            .source_run_index = source_run_index,
            .style_token = source_run.style_token,
            .source_label = source_label,
            .byte_offset = scalar.byte_offset,
            .byte_count = scalar.byte_count,
            .codepoint_offset = codepoint_offset,
            .codepoint_count = 1,
            .first_codepoint = utf8_replacement_codepoint,
            .last_codepoint = utf8_replacement_codepoint,
            .requested_face_id = requested_face == nullptr ? 0U : requested_face->id,
            .requested_family = style.font_family,
            .fallback_order = attempted_face_ids.size(),
            .attempted_face_ids = attempted_face_ids,
            .valid_utf8 = false,
            .glyph_supported = false,
            .status = render_text_font_fallback_run_status::invalid_utf8,
            .diagnostic = "invalid UTF-8 sequence at byte " + std::to_string(scalar.byte_offset),
        };
    }

    std::size_t selected_order = candidate_faces.size();
    const font_face_descriptor* selected =
        font_fallback_run_select_face_for_codepoint(candidate_faces, scalar.code_point, selected_order);
    const bool supported = selected != nullptr;
    const bool used_fallback =
        supported && (requested_face == nullptr || selected->id != requested_face->id);

    return render_text_font_fallback_run_snapshot{
        .item_index = item_index,
        .source_run_index = source_run_index,
        .style_token = source_run.style_token,
        .source_label = source_label,
        .byte_offset = scalar.byte_offset,
        .byte_count = scalar.byte_count,
        .codepoint_offset = codepoint_offset,
        .codepoint_count = 1,
        .first_codepoint = scalar.code_point,
        .last_codepoint = scalar.code_point,
        .requested_face_id = requested_face == nullptr ? 0U : requested_face->id,
        .selected_face_id = selected == nullptr ? 0U : selected->id,
        .requested_family = style.font_family,
        .selected_family = selected == nullptr ? std::string{} : selected->family,
        .selected_source_uri = selected == nullptr ? std::string{} : selected->source_uri,
        .fallback_order = selected_order,
        .attempted_face_ids = attempted_face_ids,
        .valid_utf8 = true,
        .glyph_supported = supported,
        .used_fallback = used_fallback,
        .status = supported
            ? render_text_font_fallback_run_status::covered
            : render_text_font_fallback_run_status::missing_glyph,
        .diagnostic = supported
            ? std::string{"font fallback run selected face before shaping"}
            : "no fallback run face covers " + font_coverage_run_hex_codepoint_label(scalar.code_point),
    };
}

inline render_text_font_fallback_run_snapshot make_render_text_font_fallback_run_for_cluster(
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster,
    const std::size_t item_index,
    const std::size_t source_run_index,
    const render_text_run& source_run,
    const render_text_style& style,
    const std::string& source_label,
    const font_face_descriptor* requested_face,
    const std::vector<const font_face_descriptor*>& candidate_faces,
    const std::vector<font_face_id>& attempted_face_ids)
{
    const std::uint32_t first_codepoint =
        font_coverage_run_cluster_first_codepoint(codepoints, cluster);
    const std::uint32_t last_codepoint =
        font_fallback_run_cluster_last_codepoint(codepoints, cluster);

    if (!cluster.valid) {
        return render_text_font_fallback_run_snapshot{
            .item_index = item_index,
            .source_run_index = source_run_index,
            .style_token = source_run.style_token,
            .source_label = source_label,
            .byte_offset = cluster.byte_offset,
            .byte_count = cluster.byte_count,
            .codepoint_offset = cluster.codepoint_offset,
            .codepoint_count = cluster.codepoint_count,
            .first_codepoint = first_codepoint,
            .last_codepoint = last_codepoint,
            .requested_face_id = requested_face == nullptr ? 0U : requested_face->id,
            .requested_family = style.font_family,
            .fallback_order = attempted_face_ids.size(),
            .attempted_face_ids = attempted_face_ids,
            .valid_utf8 = false,
            .glyph_supported = false,
            .status = render_text_font_fallback_run_status::invalid_utf8,
            .diagnostic = "invalid UTF-8 cluster at byte " + std::to_string(cluster.byte_offset),
        };
    }

    std::size_t selected_order = candidate_faces.size();
    const font_face_descriptor* selected =
        font_fallback_run_select_face_for_cluster(candidate_faces, codepoints, cluster, selected_order);
    const bool supported = selected != nullptr;
    const bool used_fallback =
        supported && (requested_face == nullptr || selected->id != requested_face->id);

    return render_text_font_fallback_run_snapshot{
        .item_index = item_index,
        .source_run_index = source_run_index,
        .style_token = source_run.style_token,
        .source_label = source_label,
        .byte_offset = cluster.byte_offset,
        .byte_count = cluster.byte_count,
        .codepoint_offset = cluster.codepoint_offset,
        .codepoint_count = cluster.codepoint_count,
        .first_codepoint = first_codepoint,
        .last_codepoint = last_codepoint,
        .requested_face_id = requested_face == nullptr ? 0U : requested_face->id,
        .selected_face_id = selected == nullptr ? 0U : selected->id,
        .requested_family = style.font_family,
        .selected_family = selected == nullptr ? std::string{} : selected->family,
        .selected_source_uri = selected == nullptr ? std::string{} : selected->source_uri,
        .fallback_order = selected_order,
        .attempted_face_ids = attempted_face_ids,
        .valid_utf8 = true,
        .glyph_supported = supported,
        .used_fallback = used_fallback,
        .status = supported
            ? render_text_font_fallback_run_status::covered
            : render_text_font_fallback_run_status::missing_glyph,
        .diagnostic = supported
            ? std::string{"font fallback run selected one face for a UTF-8 cluster before shaping"}
            : "no fallback run face covers UTF-8 cluster starting at "
                + font_coverage_run_hex_codepoint_label(first_codepoint),
    };
}

inline void summarize_render_text_font_fallback_run_policy(
    render_text_font_fallback_run_plan_snapshot& plan)
{
    plan.policy.fallback_run_count = plan.runs.size();
    plan.missing_runs.clear();
    plan.selected_face_order.clear();

    for (const render_text_font_fallback_run_snapshot& run : plan.runs) {
        plan.policy.codepoint_count += run.codepoint_count;
        if (run.ok()) {
            plan.policy.covered_codepoint_count += run.codepoint_count;
            if (run.used_fallback) {
                plan.policy.fallback_codepoint_count += run.codepoint_count;
            }
            font_fallback_chain_append_unique_selected_face(plan.selected_face_order, run.selected_face_id);
            continue;
        }

        plan.missing_runs.push_back(run);
        if (run.status == render_text_font_fallback_run_status::invalid_utf8) {
            plan.policy.invalid_utf8_count += run.codepoint_count;
            ++plan.policy.invalid_run_count;
        } else {
            plan.policy.missing_glyph_count += run.codepoint_count;
        }
        ++plan.policy.missing_run_count;
    }

    plan.policy.unique_selected_face_count = plan.selected_face_order.size();
    plan.policy.has_missing_ranges = !plan.missing_runs.empty();
    plan.policy.has_invalid_utf8 = plan.policy.invalid_utf8_count > 0U;
}

inline render_text_font_fallback_run_plan_snapshot plan_render_text_font_fallback_runs(
    const render_text_font_fallback_run_plan_request& request,
    const font_face_catalog& font_catalog)
{
    render_text_font_fallback_run_plan_snapshot plan;
    plan.policy.item_count = request.items.size();

    for (const render_text_font_fallback_chain_plan_item& item : request.items) {
        plan.policy.source_run_count += item.text_runs.size();
        for (std::size_t source_run_index = 0; source_run_index < item.text_runs.size(); ++source_run_index) {
            const render_text_run& source_run = item.text_runs[source_run_index];
            const render_text_style& style = item.style_catalog.resolve(source_run.style_token);
            const font_face_descriptor* requested_face =
                font_catalog.find_exact(style.font_family, style.font_weight, style.italic);
            const std::vector<const font_face_descriptor*> candidate_faces =
                font_fallback_chain_candidate_faces_for(font_catalog, style);
            const std::vector<font_face_id> attempted_face_ids =
                font_fallback_chain_attempted_face_ids(candidate_faces);
            const std::vector<utf8_text_codepoint> codepoints =
                iterate_utf8_text_run(source_run.text);
            const std::vector<utf8_text_cluster> clusters = cluster_utf8_text_run(codepoints);

            for (const utf8_text_cluster& cluster : clusters) {
                append_render_text_font_fallback_run(
                    plan,
                    make_render_text_font_fallback_run_for_cluster(
                        codepoints,
                        cluster,
                        item.item_index,
                        source_run_index,
                        source_run,
                        style,
                        item.source_label,
                        requested_face,
                        candidate_faces,
                        attempted_face_ids));
            }
        }
    }

    summarize_render_text_font_fallback_run_policy(plan);
    plan.diagnostic = plan.ok()
        ? "font fallback run planner resolved all UTF-8 ranges before shaping"
        : "font fallback run planner found "
            + std::to_string(plan.policy.missing_glyph_count)
            + " missing glyph(s) and "
            + std::to_string(plan.policy.invalid_utf8_count)
            + " invalid UTF-8 sequence(s)";
    return plan;
}

inline render_text_font_fallback_run_plan_snapshot plan_render_text_font_fallback_runs(
    const std::string_view text,
    const font_face_catalog& font_catalog,
    render_text_style style)
{
    if (style.id.empty()) {
        style.id = "default";
    }

    render_text_style_catalog style_catalog;
    style_catalog.fallback_style = style;
    style_catalog.styles.push_back(style);

    return plan_render_text_font_fallback_runs(
        render_text_font_fallback_run_plan_request{
            .items = {
                render_text_font_fallback_chain_plan_item{
                    .text_runs = {
                        render_text_run{
                            .text = std::string{text},
                            .style_token = style.id,
                        },
                    },
                    .style_catalog = std::move(style_catalog),
                },
            },
        },
        font_catalog);
}

inline std::ptrdiff_t font_fallback_run_count_delta(
    const std::size_t before,
    const std::size_t after)
{
    return static_cast<std::ptrdiff_t>(after) - static_cast<std::ptrdiff_t>(before);
}

inline bool render_text_font_fallback_run_snapshots_equal(
    const render_text_font_fallback_run_snapshot& lhs,
    const render_text_font_fallback_run_snapshot& rhs)
{
    return lhs.stable_run_key == rhs.stable_run_key
        && lhs.status == rhs.status
        && lhs.selected_face_id == rhs.selected_face_id
        && lhs.requested_face_id == rhs.requested_face_id
        && lhs.fallback_order == rhs.fallback_order
        && lhs.valid_utf8 == rhs.valid_utf8
        && lhs.glyph_supported == rhs.glyph_supported
        && lhs.used_fallback == rhs.used_fallback
        && lhs.attempted_face_ids == rhs.attempted_face_ids;
}

inline const render_text_font_fallback_run_snapshot* find_render_text_font_fallback_run_by_key(
    const std::vector<render_text_font_fallback_run_snapshot>& runs,
    const std::string& stable_run_key)
{
    const auto match = std::find_if(
        runs.begin(),
        runs.end(),
        [&](const render_text_font_fallback_run_snapshot& run) {
            return run.stable_run_key == stable_run_key;
        });
    return match == runs.end() ? nullptr : &*match;
}

inline void font_fallback_run_append_unique_key(
    std::vector<std::string>& keys,
    const std::string& key)
{
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

inline bool font_fallback_run_selected_face_order_equal(
    const std::vector<font_face_id>& lhs,
    const std::vector<font_face_id>& rhs)
{
    return lhs == rhs;
}

inline render_text_font_fallback_run_diff_snapshot diff_render_text_font_fallback_runs(
    const render_text_font_fallback_run_snapshot* before,
    const render_text_font_fallback_run_snapshot* after,
    const std::string& stable_run_key)
{
    if (before == nullptr && after != nullptr) {
        return render_text_font_fallback_run_diff_snapshot{
            .stable_run_key = stable_run_key,
            .added = true,
            .changed = true,
            .current_status = after->status,
            .current_selected_face_id = after->selected_face_id,
        };
    }
    if (before != nullptr && after == nullptr) {
        return render_text_font_fallback_run_diff_snapshot{
            .stable_run_key = stable_run_key,
            .removed = true,
            .changed = true,
            .previous_status = before->status,
            .previous_selected_face_id = before->selected_face_id,
        };
    }
    if (before == nullptr || after == nullptr) {
        return render_text_font_fallback_run_diff_snapshot{.stable_run_key = stable_run_key};
    }

    const bool status_changed = before->status != after->status;
    const bool selected_face_changed = before->selected_face_id != after->selected_face_id;
    const bool fallback_changed = before->used_fallback != after->used_fallback
        || before->fallback_order != after->fallback_order;
    const bool missing_changed = before->missing() != after->missing();
    return render_text_font_fallback_run_diff_snapshot{
        .stable_run_key = stable_run_key,
        .changed = !render_text_font_fallback_run_snapshots_equal(*before, *after),
        .status_changed = status_changed,
        .selected_face_changed = selected_face_changed,
        .fallback_changed = fallback_changed,
        .missing_changed = missing_changed,
        .previous_status = before->status,
        .current_status = after->status,
        .previous_selected_face_id = before->selected_face_id,
        .current_selected_face_id = after->selected_face_id,
    };
}

inline render_text_font_fallback_run_plan_diff_snapshot diff_render_text_font_fallback_run_plans(
    const render_text_font_fallback_run_plan_snapshot& before,
    const render_text_font_fallback_run_plan_snapshot& after)
{
    render_text_font_fallback_run_plan_diff_snapshot diff{
        .policy = render_text_font_fallback_run_diff_policy_snapshot{
            .fallback_run_count_delta = font_fallback_run_count_delta(
                before.policy.fallback_run_count,
                after.policy.fallback_run_count),
            .covered_codepoint_count_delta = font_fallback_run_count_delta(
                before.policy.covered_codepoint_count,
                after.policy.covered_codepoint_count),
            .fallback_codepoint_count_delta = font_fallback_run_count_delta(
                before.policy.fallback_codepoint_count,
                after.policy.fallback_codepoint_count),
            .missing_glyph_count_delta = font_fallback_run_count_delta(
                before.policy.missing_glyph_count,
                after.policy.missing_glyph_count),
            .invalid_utf8_count_delta = font_fallback_run_count_delta(
                before.policy.invalid_utf8_count,
                after.policy.invalid_utf8_count),
            .unique_selected_face_count_delta = font_fallback_run_count_delta(
                before.policy.unique_selected_face_count,
                after.policy.unique_selected_face_count),
            .selected_face_order_changed = !font_fallback_run_selected_face_order_equal(
                before.selected_face_order,
                after.selected_face_order),
            .missing_state_changed = before.policy.has_missing_ranges != after.policy.has_missing_ranges,
        },
    };

    std::vector<std::string> keys;
    for (const render_text_font_fallback_run_snapshot& run : before.runs) {
        font_fallback_run_append_unique_key(keys, run.stable_run_key);
    }
    for (const render_text_font_fallback_run_snapshot& run : after.runs) {
        font_fallback_run_append_unique_key(keys, run.stable_run_key);
    }

    for (const std::string& key : keys) {
        const render_text_font_fallback_run_snapshot* before_run =
            find_render_text_font_fallback_run_by_key(before.runs, key);
        const render_text_font_fallback_run_snapshot* after_run =
            find_render_text_font_fallback_run_by_key(after.runs, key);
        render_text_font_fallback_run_diff_snapshot run_diff =
            diff_render_text_font_fallback_runs(before_run, after_run, key);
        if (run_diff.added) {
            ++diff.added_run_count;
            diff.added_run_keys.push_back(key);
        } else if (run_diff.removed) {
            ++diff.removed_run_count;
            diff.removed_run_keys.push_back(key);
        } else if (run_diff.changed) {
            ++diff.changed_run_count;
            diff.changed_run_keys.push_back(key);
        } else {
            ++diff.unchanged_run_count;
        }
        diff.run_diffs.push_back(std::move(run_diff));
    }

    diff.diagnostic = diff.has_changes()
        ? "font fallback run plan changed"
        : "font fallback run plans match";
    return diff;
}

} // namespace quiz_vulkan::render
