#pragma once

#include "render/text/font_backend_selection.h"
#include "render/text/font_glyph_atlas.h"
#include "render/text/utf8_text_run.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_coverage_run_segment_status {
    supported,
    invalid_utf8,
    unsupported_codepoint,
};

inline std::string render_text_font_coverage_run_segment_status_name(
    const render_text_font_coverage_run_segment_status status)
{
    switch (status) {
    case render_text_font_coverage_run_segment_status::supported:
        return "supported";
    case render_text_font_coverage_run_segment_status::invalid_utf8:
        return "invalid_utf8";
    case render_text_font_coverage_run_segment_status::unsupported_codepoint:
        return "unsupported_codepoint";
    }

    return "unknown";
}

struct render_text_font_coverage_run_segment {
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::size_t codepoint_offset = 0;
    std::size_t codepoint_count = 0;
    std::uint32_t first_codepoint = utf8_replacement_codepoint;
    font_face_id requested_face_id = 0;
    font_face_id resolved_face_id = 0;
    std::string requested_family;
    std::string resolved_family;
    bool used_fallback = false;
    bool glyph_supported = false;
    bool valid_utf8 = true;
    render_text_font_coverage_run_segment_status status =
        render_text_font_coverage_run_segment_status::unsupported_codepoint;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_coverage_run_segment_status::supported
            && glyph_supported
            && valid_utf8;
    }
};

struct render_text_font_coverage_run_segmentation {
    std::vector<render_text_font_coverage_run_segment> segments;
    std::size_t codepoint_count = 0;
    std::size_t supported_codepoint_count = 0;
    std::size_t fallback_codepoint_count = 0;
    std::size_t invalid_utf8_count = 0;
    std::size_t unsupported_codepoint_count = 0;
    std::string diagnostic;

    bool ok() const
    {
        return invalid_utf8_count == 0U && unsupported_codepoint_count == 0U;
    }
};

struct render_text_font_coverage_run_segmentation_request {
    std::string_view text;
    render_text_style style;
};

struct render_text_font_fallback_chain_entry_snapshot {
    std::size_t order = 0;
    font_face_id face_id = 0;
    std::string family;
    std::string source_uri;
    int weight = 400;
    bool italic = false;
    bool fallback_face = false;
    bool requested_face = false;
    std::size_t covered_codepoint_count = 0;
    std::size_t selected_codepoint_count = 0;

    bool covers_run() const
    {
        return covered_codepoint_count > 0U;
    }

    bool selected() const
    {
        return selected_codepoint_count > 0U;
    }
};

struct render_text_font_fallback_chain_missing_glyph_snapshot {
    std::size_t item_index = 0;
    std::size_t run_index = 0;
    render_style_id style_token;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::size_t codepoint_offset = 0;
    std::uint32_t codepoint = utf8_replacement_codepoint;
    bool valid_utf8 = true;
    font_face_id requested_face_id = 0;
    std::string requested_family;
    std::vector<font_face_id> attempted_face_ids;
    std::string diagnostic;
};

struct render_text_font_fallback_chain_run_snapshot {
    std::size_t item_index = 0;
    std::size_t run_index = 0;
    render_style_id style_token;
    std::string source_label;
    std::size_t byte_count = 0;
    std::size_t codepoint_count = 0;
    font_face_id requested_face_id = 0;
    std::string requested_family;
    std::vector<render_text_font_fallback_chain_entry_snapshot> entries;
    std::vector<font_face_id> selected_face_ids;
    render_text_font_coverage_run_segmentation coverage;
    std::size_t supported_codepoint_count = 0;
    std::size_t fallback_codepoint_count = 0;
    std::size_t missing_glyph_count = 0;
    std::size_t invalid_utf8_count = 0;
    render_text_font_backend_library shaping_backend =
        render_text_font_backend_library::deterministic_fake;
    std::string shaping_backend_label;
    render_text_font_backend_selection_status shaping_selection_status =
        render_text_font_backend_selection_status::unavailable;
    render_text_font_backend_capability_status shaping_capability_status =
        render_text_font_backend_capability_status::unavailable;
    bool shaping_used_deterministic_fallback = true;
    std::string diagnostic;

    bool ok() const
    {
        return missing_glyph_count == 0U && invalid_utf8_count == 0U;
    }

    bool used_font_fallback() const
    {
        return fallback_codepoint_count > 0U;
    }
};

struct render_text_font_fallback_chain_plan_item {
    std::size_t item_index = 0;
    std::vector<render_text_run> text_runs;
    render_text_style_catalog style_catalog;
    std::string source_label;
};

struct render_text_font_fallback_chain_plan_request {
    std::vector<render_text_font_fallback_chain_plan_item> items;
    render_text_font_backend_selection_result shaping_selection;
};

struct render_text_font_fallback_chain_plan_policy_snapshot {
    std::size_t item_count = 0;
    std::size_t run_count = 0;
    std::size_t codepoint_count = 0;
    std::size_t supported_codepoint_count = 0;
    std::size_t fallback_codepoint_count = 0;
    std::size_t missing_glyph_count = 0;
    std::size_t invalid_utf8_count = 0;
    std::size_t fallback_run_count = 0;
    std::size_t missing_run_count = 0;
    std::size_t invalid_run_count = 0;
    std::size_t unique_selected_face_count = 0;
    bool deterministic_backend_selected = true;
};

struct render_text_font_fallback_chain_plan_snapshot {
    std::vector<render_text_font_fallback_chain_run_snapshot> runs;
    std::vector<render_text_font_fallback_chain_missing_glyph_snapshot> missing_glyphs;
    std::vector<font_face_id> deterministic_selected_face_order;
    render_text_font_backend_selection_result shaping_selection;
    render_text_font_fallback_chain_plan_policy_snapshot policy;
    std::string diagnostic;

    bool ok() const
    {
        return policy.missing_glyph_count == 0U && policy.invalid_utf8_count == 0U;
    }

    bool has_missing_glyphs() const
    {
        return !missing_glyphs.empty();
    }
};

inline std::string font_coverage_run_hex_codepoint_label(const std::uint32_t codepoint)
{
    constexpr char digits[] = "0123456789ABCDEF";
    std::string label = "U+";
    bool emitted = false;
    for (int shift = 20; shift >= 0; shift -= 4) {
        const auto digit = static_cast<unsigned>((codepoint >> static_cast<unsigned>(shift)) & 0x0fU);
        if (digit != 0U || emitted || shift <= 12) {
            label.push_back(digits[digit]);
            emitted = true;
        }
    }
    return label;
}

inline bool font_coverage_face_supports_utf8_cluster(
    const font_face_descriptor& face,
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster)
{
    const std::size_t cluster_end = cluster.codepoint_offset + cluster.codepoint_count;
    if (!cluster.valid || cluster.codepoint_count == 0U || cluster_end > codepoints.size()) {
        return false;
    }

    for (std::size_t index = cluster.codepoint_offset; index < cluster_end; ++index) {
        const utf8_text_codepoint& scalar = codepoints[index];
        if (!scalar.valid || !face.supports_codepoint(scalar.code_point)) {
            return false;
        }
    }
    return true;
}

inline const font_face_descriptor* font_coverage_find_covering_fallback_for_cluster(
    const font_face_catalog& catalog,
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster)
{
    const auto known_coverage_match = std::find_if(
        catalog.faces().begin(),
        catalog.faces().end(),
        [&](const font_face_descriptor& face) {
            return face.fallback
                && !face.coverage.empty()
                && font_coverage_face_supports_utf8_cluster(face, codepoints, cluster);
        });
    if (known_coverage_match != catalog.faces().end()) {
        return &*known_coverage_match;
    }

    const auto match = std::find_if(
        catalog.faces().begin(),
        catalog.faces().end(),
        [&](const font_face_descriptor& face) {
            return face.fallback
                && font_coverage_face_supports_utf8_cluster(face, codepoints, cluster);
        });
    return match == catalog.faces().end() ? nullptr : &*match;
}

inline const font_face_descriptor* font_coverage_find_covering_face_for_cluster(
    const font_face_catalog& catalog,
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster)
{
    const auto known_coverage_match = std::find_if(
        catalog.faces().begin(),
        catalog.faces().end(),
        [&](const font_face_descriptor& face) {
            return !face.coverage.empty()
                && font_coverage_face_supports_utf8_cluster(face, codepoints, cluster);
        });
    if (known_coverage_match != catalog.faces().end()) {
        return &*known_coverage_match;
    }

    const auto match = std::find_if(
        catalog.faces().begin(),
        catalog.faces().end(),
        [&](const font_face_descriptor& face) {
            return font_coverage_face_supports_utf8_cluster(face, codepoints, cluster);
        });
    return match == catalog.faces().end() ? nullptr : &*match;
}

inline font_face_resolution resolve_font_face_for_utf8_cluster(
    const font_face_catalog& catalog,
    const font_face_descriptor* requested,
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster)
{
    if (requested != nullptr && font_coverage_face_supports_utf8_cluster(*requested, codepoints, cluster)) {
        return font_face_resolution{
            .requested_face = requested,
            .resolved_face = requested,
            .used_fallback = false,
            .glyph_supported = true,
        };
    }

    if (const font_face_descriptor* fallback =
            font_coverage_find_covering_fallback_for_cluster(catalog, codepoints, cluster);
        fallback != nullptr) {
        return font_face_resolution{
            .requested_face = requested,
            .resolved_face = fallback,
            .used_fallback = requested == nullptr || fallback->id != requested->id,
            .glyph_supported = true,
        };
    }

    if (const font_face_descriptor* covering =
            font_coverage_find_covering_face_for_cluster(catalog, codepoints, cluster);
        covering != nullptr) {
        return font_face_resolution{
            .requested_face = requested,
            .resolved_face = covering,
            .used_fallback = requested == nullptr || covering->id != requested->id,
            .glyph_supported = true,
        };
    }

    const font_face_descriptor* unresolved = requested == nullptr ? catalog.fallback_face() : requested;
    return font_face_resolution{
        .requested_face = requested,
        .resolved_face = unresolved,
        .used_fallback = requested == nullptr && unresolved != nullptr,
        .glyph_supported = false,
    };
}

inline font_face_resolution resolve_font_face_for_utf8_cluster(
    const font_face_catalog& catalog,
    const render_text_style& style,
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster)
{
    return resolve_font_face_for_utf8_cluster(
        catalog,
        catalog.find_exact(style.font_family, style.font_weight, style.italic),
        codepoints,
        cluster);
}

inline font_face_resolution resolve_font_face_for_utf8_cluster(
    const font_face_catalog& catalog,
    const font_face_id requested_face_id,
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster)
{
    return resolve_font_face_for_utf8_cluster(
        catalog,
        catalog.find_by_id(requested_face_id),
        codepoints,
        cluster);
}

inline std::uint32_t font_coverage_run_cluster_first_codepoint(
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster)
{
    if (cluster.codepoint_offset >= codepoints.size()) {
        return utf8_replacement_codepoint;
    }
    return codepoints[cluster.codepoint_offset].valid
        ? codepoints[cluster.codepoint_offset].code_point
        : utf8_replacement_codepoint;
}

inline bool font_coverage_run_segments_can_merge(
    const render_text_font_coverage_run_segment& lhs,
    const render_text_font_coverage_run_segment& rhs)
{
    return lhs.status == rhs.status
        && lhs.requested_face_id == rhs.requested_face_id
        && lhs.resolved_face_id == rhs.resolved_face_id
        && lhs.used_fallback == rhs.used_fallback
        && lhs.glyph_supported == rhs.glyph_supported
        && lhs.valid_utf8 == rhs.valid_utf8
        && lhs.diagnostic == rhs.diagnostic;
}

inline render_text_font_coverage_run_segment make_font_coverage_invalid_utf8_segment(
    const utf8_text_codepoint& scalar,
    const std::size_t codepoint_offset,
    const font_face_catalog& catalog,
    const render_text_style& style)
{
    const font_face_descriptor* requested =
        catalog.find_exact(style.font_family, style.font_weight, style.italic);
    return render_text_font_coverage_run_segment{
        .byte_offset = scalar.byte_offset,
        .byte_count = scalar.byte_count,
        .codepoint_offset = codepoint_offset,
        .codepoint_count = 1,
        .first_codepoint = utf8_replacement_codepoint,
        .requested_face_id = requested == nullptr ? 0 : requested->id,
        .resolved_face_id = requested == nullptr ? 0 : requested->id,
        .requested_family = style.font_family,
        .resolved_family = requested == nullptr ? std::string{} : requested->family,
        .used_fallback = false,
        .glyph_supported = false,
        .valid_utf8 = false,
        .status = render_text_font_coverage_run_segment_status::invalid_utf8,
        .diagnostic = "invalid UTF-8 sequence at byte "
            + std::to_string(scalar.byte_offset),
    };
}

inline render_text_font_coverage_run_segment make_font_coverage_codepoint_segment(
    const utf8_text_codepoint& scalar,
    const std::size_t codepoint_offset,
    const font_face_catalog& catalog,
    const render_text_style& style)
{
    const font_face_resolution resolution =
        catalog.resolve_for_codepoint(style, scalar.code_point);
    const font_face_descriptor* requested = resolution.requested_face;
    const font_face_descriptor* resolved = resolution.resolved_face;
    const bool supported = resolution.glyph_supported;

    return render_text_font_coverage_run_segment{
        .byte_offset = scalar.byte_offset,
        .byte_count = scalar.byte_count,
        .codepoint_offset = codepoint_offset,
        .codepoint_count = 1,
        .first_codepoint = scalar.code_point,
        .requested_face_id = requested == nullptr ? 0 : requested->id,
        .resolved_face_id = resolved == nullptr ? 0 : resolved->id,
        .requested_family = style.font_family,
        .resolved_family = resolved == nullptr ? std::string{} : resolved->family,
        .used_fallback = resolution.used_fallback,
        .glyph_supported = supported,
        .valid_utf8 = true,
        .status = supported
            ? render_text_font_coverage_run_segment_status::supported
            : render_text_font_coverage_run_segment_status::unsupported_codepoint,
        .diagnostic = supported
            ? std::string{}
            : "no font face covers " + font_coverage_run_hex_codepoint_label(scalar.code_point),
    };
}

inline render_text_font_coverage_run_segment make_font_coverage_cluster_segment(
    const std::vector<utf8_text_codepoint>& codepoints,
    const utf8_text_cluster& cluster,
    const font_face_catalog& catalog,
    const render_text_style& style)
{
    const font_face_resolution resolution =
        resolve_font_face_for_utf8_cluster(catalog, style, codepoints, cluster);
    const font_face_descriptor* requested = resolution.requested_face;
    const font_face_descriptor* resolved = resolution.resolved_face;
    const bool supported = resolution.glyph_supported;
    const std::uint32_t first_codepoint =
        font_coverage_run_cluster_first_codepoint(codepoints, cluster);

    return render_text_font_coverage_run_segment{
        .byte_offset = cluster.byte_offset,
        .byte_count = cluster.byte_count,
        .codepoint_offset = cluster.codepoint_offset,
        .codepoint_count = cluster.codepoint_count,
        .first_codepoint = first_codepoint,
        .requested_face_id = requested == nullptr ? 0 : requested->id,
        .resolved_face_id = resolved == nullptr ? 0 : resolved->id,
        .requested_family = style.font_family,
        .resolved_family = resolved == nullptr ? std::string{} : resolved->family,
        .used_fallback = resolution.used_fallback,
        .glyph_supported = supported,
        .valid_utf8 = true,
        .status = supported
            ? render_text_font_coverage_run_segment_status::supported
            : render_text_font_coverage_run_segment_status::unsupported_codepoint,
        .diagnostic = supported
            ? std::string{}
            : "no font face covers UTF-8 cluster starting at "
                + font_coverage_run_hex_codepoint_label(first_codepoint),
    };
}

inline void font_coverage_run_append_segment(
    std::vector<render_text_font_coverage_run_segment>& segments,
    render_text_font_coverage_run_segment segment)
{
    if (segments.empty() || !font_coverage_run_segments_can_merge(segments.back(), segment)) {
        segments.push_back(std::move(segment));
        return;
    }

    render_text_font_coverage_run_segment& previous = segments.back();
    previous.byte_count = (segment.byte_offset + segment.byte_count) - previous.byte_offset;
    previous.codepoint_count += segment.codepoint_count;
}

inline render_text_font_coverage_run_segmentation segment_font_coverage_runs(
    const std::vector<utf8_text_codepoint>& codepoints,
    const font_face_catalog& catalog,
    const render_text_style& style)
{
    render_text_font_coverage_run_segmentation segmentation{
        .codepoint_count = codepoints.size(),
    };

    const std::vector<utf8_text_cluster> clusters = cluster_utf8_text_run(codepoints);
    for (const utf8_text_cluster& cluster : clusters) {
        if (!cluster.valid) {
            const std::size_t cluster_end = cluster.codepoint_offset + cluster.codepoint_count;
            for (std::size_t index = cluster.codepoint_offset;
                 index < cluster_end && index < codepoints.size();
                 ++index) {
                render_text_font_coverage_run_segment segment =
                    make_font_coverage_invalid_utf8_segment(codepoints[index], index, catalog, style);
                ++segmentation.invalid_utf8_count;
                font_coverage_run_append_segment(segmentation.segments, std::move(segment));
            }
            continue;
        }

        render_text_font_coverage_run_segment segment =
            make_font_coverage_cluster_segment(codepoints, cluster, catalog, style);
        if (!segment.glyph_supported) {
            segmentation.unsupported_codepoint_count += segment.codepoint_count;
        } else {
            segmentation.supported_codepoint_count += segment.codepoint_count;
        }
        if (segment.used_fallback && segment.glyph_supported) {
            segmentation.fallback_codepoint_count += segment.codepoint_count;
        }

        font_coverage_run_append_segment(segmentation.segments, std::move(segment));
    }

    if (segmentation.ok()) {
        segmentation.diagnostic = "all UTF-8 codepoints resolved to font coverage";
    } else {
        segmentation.diagnostic = "font coverage segmentation found "
            + std::to_string(segmentation.invalid_utf8_count)
            + " invalid UTF-8 sequence(s) and "
            + std::to_string(segmentation.unsupported_codepoint_count)
            + " unsupported codepoint(s)";
    }

    return segmentation;
}

inline render_text_font_coverage_run_segmentation segment_font_coverage_runs(
    const std::string_view text,
    const font_face_catalog& catalog,
    const render_text_style& style)
{
    return segment_font_coverage_runs(iterate_utf8_text_run(text), catalog, style);
}

inline render_text_font_coverage_run_segmentation segment_font_coverage_runs(
    const render_text_font_coverage_run_segmentation_request& request,
    const font_face_catalog& catalog)
{
    return segment_font_coverage_runs(request.text, catalog, request.style);
}

class font_coverage_run_segmenter final {
public:
    render_text_font_coverage_run_segmentation segment(
        const render_text_font_coverage_run_segmentation_request& request,
        const font_face_catalog& catalog) const
    {
        return segment_font_coverage_runs(request, catalog);
    }
};

inline render_text_font_fallback_chain_plan_item make_render_text_font_fallback_chain_plan_item(
    render_text_request request,
    std::string source_label = {},
    const std::size_t item_index = 0)
{
    return render_text_font_fallback_chain_plan_item{
        .item_index = item_index,
        .text_runs = std::move(request.text_runs),
        .style_catalog = std::move(request.style_catalog),
        .source_label = std::move(source_label),
    };
}

inline render_text_font_fallback_chain_plan_item make_render_text_font_fallback_chain_plan_item(
    std::vector<render_text_run> text_runs,
    render_text_style_catalog style_catalog,
    std::string source_label = {},
    const std::size_t item_index = 0)
{
    return render_text_font_fallback_chain_plan_item{
        .item_index = item_index,
        .text_runs = std::move(text_runs),
        .style_catalog = std::move(style_catalog),
        .source_label = std::move(source_label),
    };
}

inline render_text_font_backend_selection_result render_text_font_fallback_chain_effective_shaping_selection(
    const render_text_font_backend_selection_result& selection)
{
    if (selection.has_selection) {
        return selection;
    }

    return make_render_text_font_backend_selection_result(
        render_text_font_backend_selection_status::fallback_selected,
        render_text_font_backend_selection_purpose::shaping,
        make_render_text_deterministic_fake_backend_candidate(),
        render_text_font_backend_required_features_for(render_text_font_backend_selection_purpose::shaping),
        {"fallback chain planner used deterministic fake shaping metadata"});
}

inline bool font_fallback_chain_contains_face(
    const std::vector<const font_face_descriptor*>& faces,
    const font_face_id face_id)
{
    for (const font_face_descriptor* face : faces) {
        if (face != nullptr && face->id == face_id) {
            return true;
        }
    }
    return false;
}

inline bool font_fallback_chain_contains_face_id(
    const std::vector<font_face_id>& faces,
    const font_face_id face_id)
{
    for (const font_face_id existing : faces) {
        if (existing == face_id) {
            return true;
        }
    }
    return false;
}

inline void font_fallback_chain_append_candidate_face(
    std::vector<const font_face_descriptor*>& faces,
    const font_face_descriptor* face)
{
    if (face != nullptr && !font_fallback_chain_contains_face(faces, face->id)) {
        faces.push_back(face);
    }
}

inline std::vector<const font_face_descriptor*> font_fallback_chain_candidate_faces_for(
    const font_face_catalog& catalog,
    const render_text_style& style)
{
    std::vector<const font_face_descriptor*> candidates;
    font_fallback_chain_append_candidate_face(
        candidates,
        catalog.find_exact(style.font_family, style.font_weight, style.italic));

    for (const font_face_descriptor& face : catalog.faces()) {
        if (face.fallback && !face.coverage.empty()) {
            font_fallback_chain_append_candidate_face(candidates, &face);
        }
    }
    for (const font_face_descriptor& face : catalog.faces()) {
        if (face.fallback && face.coverage.empty()) {
            font_fallback_chain_append_candidate_face(candidates, &face);
        }
    }
    for (const font_face_descriptor& face : catalog.faces()) {
        font_fallback_chain_append_candidate_face(candidates, &face);
    }

    return candidates;
}

inline std::vector<font_face_id> font_fallback_chain_attempted_face_ids(
    const std::vector<const font_face_descriptor*>& candidates)
{
    std::vector<font_face_id> face_ids;
    face_ids.reserve(candidates.size());
    for (const font_face_descriptor* face : candidates) {
        if (face != nullptr) {
            face_ids.push_back(face->id);
        }
    }
    return face_ids;
}

inline render_text_font_fallback_chain_entry_snapshot make_font_fallback_chain_entry_snapshot(
    const font_face_descriptor& face,
    const std::size_t order,
    const bool requested_face)
{
    return render_text_font_fallback_chain_entry_snapshot{
        .order = order,
        .face_id = face.id,
        .family = face.family,
        .source_uri = face.source_uri,
        .weight = face.weight,
        .italic = face.italic,
        .fallback_face = face.fallback,
        .requested_face = requested_face,
    };
}

inline render_text_font_fallback_chain_entry_snapshot* font_fallback_chain_find_entry(
    std::vector<render_text_font_fallback_chain_entry_snapshot>& entries,
    const font_face_id face_id)
{
    for (render_text_font_fallback_chain_entry_snapshot& entry : entries) {
        if (entry.face_id == face_id) {
            return &entry;
        }
    }
    return nullptr;
}

inline void font_fallback_chain_append_unique_selected_face(
    std::vector<font_face_id>& face_ids,
    const font_face_id face_id)
{
    if (face_id != 0U && !font_fallback_chain_contains_face_id(face_ids, face_id)) {
        face_ids.push_back(face_id);
    }
}

inline render_text_font_fallback_chain_missing_glyph_snapshot make_font_fallback_chain_missing_glyph(
    const std::size_t item_index,
    const std::size_t run_index,
    const render_text_run& run,
    const render_text_style& style,
    const font_face_descriptor* requested_face,
    const utf8_text_codepoint& scalar,
    const std::size_t codepoint_offset,
    std::vector<font_face_id> attempted_face_ids)
{
    const bool valid = scalar.valid;
    return render_text_font_fallback_chain_missing_glyph_snapshot{
        .item_index = item_index,
        .run_index = run_index,
        .style_token = run.style_token,
        .byte_offset = scalar.byte_offset,
        .byte_count = scalar.byte_count,
        .codepoint_offset = codepoint_offset,
        .codepoint = valid ? scalar.code_point : utf8_replacement_codepoint,
        .valid_utf8 = valid,
        .requested_face_id = requested_face == nullptr ? 0U : requested_face->id,
        .requested_family = style.font_family,
        .attempted_face_ids = std::move(attempted_face_ids),
        .diagnostic = valid
            ? "no fallback chain face covers " + font_coverage_run_hex_codepoint_label(scalar.code_point)
            : "invalid UTF-8 sequence at byte " + std::to_string(scalar.byte_offset),
    };
}

inline render_text_font_fallback_chain_run_snapshot plan_render_text_font_fallback_chain_run(
    const std::size_t item_index,
    const std::size_t run_index,
    const render_text_run& run,
    const render_text_style_catalog& style_catalog,
    const std::string& source_label,
    const font_face_catalog& font_catalog,
    const render_text_font_backend_selection_result& shaping_selection,
    std::vector<render_text_font_fallback_chain_missing_glyph_snapshot>& missing_glyphs,
    std::vector<font_face_id>& deterministic_selected_face_order)
{
    const render_text_style& style = style_catalog.resolve(run.style_token);
    const font_face_descriptor* requested_face =
        font_catalog.find_exact(style.font_family, style.font_weight, style.italic);
    const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(run.text);
    const std::vector<utf8_text_cluster> clusters = cluster_utf8_text_run(codepoints);
    const std::vector<const font_face_descriptor*> candidate_faces =
        font_fallback_chain_candidate_faces_for(font_catalog, style);

    render_text_font_fallback_chain_run_snapshot snapshot{
        .item_index = item_index,
        .run_index = run_index,
        .style_token = run.style_token,
        .source_label = source_label,
        .byte_count = run.text.size(),
        .codepoint_count = codepoints.size(),
        .requested_face_id = requested_face == nullptr ? 0U : requested_face->id,
        .requested_family = style.font_family,
        .coverage = segment_font_coverage_runs(codepoints, font_catalog, style),
        .shaping_backend = shaping_selection.selected.library,
        .shaping_backend_label = shaping_selection.selected.label,
        .shaping_selection_status = shaping_selection.status,
        .shaping_capability_status = shaping_selection.capability.status,
        .shaping_used_deterministic_fallback = shaping_selection.used_deterministic_fallback,
    };

    snapshot.entries.reserve(candidate_faces.size());
    for (std::size_t order = 0; order < candidate_faces.size(); ++order) {
        if (candidate_faces[order] != nullptr) {
            snapshot.entries.push_back(make_font_fallback_chain_entry_snapshot(
                *candidate_faces[order],
                order,
                requested_face != nullptr && candidate_faces[order]->id == requested_face->id));
        }
    }

    const std::vector<font_face_id> attempted_face_ids =
        font_fallback_chain_attempted_face_ids(candidate_faces);
    for (const utf8_text_cluster& cluster : clusters) {
        const std::size_t cluster_end = cluster.codepoint_offset + cluster.codepoint_count;
        if (!cluster.valid) {
            for (std::size_t codepoint_index = cluster.codepoint_offset;
                 codepoint_index < cluster_end && codepoint_index < codepoints.size();
                 ++codepoint_index) {
                const utf8_text_codepoint& scalar = codepoints[codepoint_index];
                ++snapshot.invalid_utf8_count;
                ++snapshot.missing_glyph_count;
                missing_glyphs.push_back(make_font_fallback_chain_missing_glyph(
                    item_index,
                    run_index,
                    run,
                    style,
                    requested_face,
                    scalar,
                    codepoint_index,
                    attempted_face_ids));
            }
            continue;
        }

        for (render_text_font_fallback_chain_entry_snapshot& entry : snapshot.entries) {
            if (const font_face_descriptor* face = font_catalog.find_by_id(entry.face_id);
                face != nullptr && font_coverage_face_supports_utf8_cluster(*face, codepoints, cluster)) {
                entry.covered_codepoint_count += cluster.codepoint_count;
            }
        }

        const render_text_font_coverage_run_segment segment =
            make_font_coverage_cluster_segment(codepoints, cluster, font_catalog, style);
        if (!segment.glyph_supported) {
            snapshot.missing_glyph_count += segment.codepoint_count;
            for (std::size_t codepoint_index = cluster.codepoint_offset;
                 codepoint_index < cluster_end && codepoint_index < codepoints.size();
                 ++codepoint_index) {
                missing_glyphs.push_back(make_font_fallback_chain_missing_glyph(
                    item_index,
                    run_index,
                    run,
                    style,
                    requested_face,
                    codepoints[codepoint_index],
                    codepoint_index,
                    attempted_face_ids));
            }
            continue;
        }

        snapshot.supported_codepoint_count += segment.codepoint_count;
        if (segment.used_fallback) {
            snapshot.fallback_codepoint_count += segment.codepoint_count;
        }
        if (render_text_font_fallback_chain_entry_snapshot* entry =
                font_fallback_chain_find_entry(snapshot.entries, segment.resolved_face_id);
            entry != nullptr) {
            entry->selected_codepoint_count += segment.codepoint_count;
        }
        font_fallback_chain_append_unique_selected_face(snapshot.selected_face_ids, segment.resolved_face_id);
        font_fallback_chain_append_unique_selected_face(deterministic_selected_face_order, segment.resolved_face_id);
    }

    if (snapshot.ok()) {
        snapshot.diagnostic = "fallback chain resolved all run codepoints before shaping";
    } else {
        snapshot.diagnostic = "fallback chain found "
            + std::to_string(snapshot.missing_glyph_count)
            + " missing glyph(s) and "
            + std::to_string(snapshot.invalid_utf8_count)
            + " invalid UTF-8 sequence(s) before shaping";
    }

    return snapshot;
}

inline render_text_font_fallback_chain_plan_snapshot plan_render_text_font_fallback_chains(
    const render_text_font_fallback_chain_plan_request& request,
    const font_face_catalog& font_catalog)
{
    render_text_font_fallback_chain_plan_snapshot plan{
        .shaping_selection = render_text_font_fallback_chain_effective_shaping_selection(
            request.shaping_selection),
    };
    plan.policy.item_count = request.items.size();
    plan.policy.deterministic_backend_selected = plan.shaping_selection.used_deterministic_fallback;

    for (const render_text_font_fallback_chain_plan_item& item : request.items) {
        const std::size_t item_index = item.item_index;
        for (std::size_t run_index = 0; run_index < item.text_runs.size(); ++run_index) {
            render_text_font_fallback_chain_run_snapshot run = plan_render_text_font_fallback_chain_run(
                item_index,
                run_index,
                item.text_runs[run_index],
                item.style_catalog,
                item.source_label,
                font_catalog,
                plan.shaping_selection,
                plan.missing_glyphs,
                plan.deterministic_selected_face_order);

            ++plan.policy.run_count;
            plan.policy.codepoint_count += run.codepoint_count;
            plan.policy.supported_codepoint_count += run.supported_codepoint_count;
            plan.policy.fallback_codepoint_count += run.fallback_codepoint_count;
            plan.policy.missing_glyph_count += run.missing_glyph_count;
            plan.policy.invalid_utf8_count += run.invalid_utf8_count;
            if (run.used_font_fallback()) {
                ++plan.policy.fallback_run_count;
            }
            if (run.missing_glyph_count > 0U) {
                ++plan.policy.missing_run_count;
            }
            if (run.invalid_utf8_count > 0U) {
                ++plan.policy.invalid_run_count;
            }
            plan.runs.push_back(std::move(run));
        }
    }

    plan.policy.unique_selected_face_count = plan.deterministic_selected_face_order.size();
    if (plan.ok()) {
        plan.diagnostic = "font fallback chain resolved all batch glyphs before shaping";
    } else {
        plan.diagnostic = "font fallback chain found "
            + std::to_string(plan.policy.missing_glyph_count)
            + " missing glyph(s) and "
            + std::to_string(plan.policy.invalid_utf8_count)
            + " invalid UTF-8 sequence(s) before shaping";
    }

    return plan;
}

} // namespace quiz_vulkan::render

#include "render/text/font_fallback_run_planning_diagnostics.h"
