#pragma once

#include "render/text/font_shaping_backend.h"
#include "render/text/font_unicode_coverage.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_glyph_id_resolution_status {
    resolved,
    invalid_utf8,
    missing_face,
    coverage_invalid,
    unsupported_codepoint,
};

inline std::string render_text_font_glyph_id_resolution_status_name(
    const render_text_font_glyph_id_resolution_status status)
{
    switch (status) {
    case render_text_font_glyph_id_resolution_status::resolved:
        return "resolved";
    case render_text_font_glyph_id_resolution_status::invalid_utf8:
        return "invalid_utf8";
    case render_text_font_glyph_id_resolution_status::missing_face:
        return "missing_face";
    case render_text_font_glyph_id_resolution_status::coverage_invalid:
        return "coverage_invalid";
    case render_text_font_glyph_id_resolution_status::unsupported_codepoint:
        return "unsupported_codepoint";
    }

    return "unknown";
}

struct render_text_font_glyph_id_resolution_request {
    std::size_t run_index = 0;
    std::size_t codepoint_index = 0;
    utf8_text_codepoint codepoint;
    font_face_id requested_face_id = 0;
    font_face_descriptor resolved_face;
    bool has_resolved_face = false;
    bool used_codepoint_fallback = false;
    render_text_font_unicode_coverage_snapshot coverage;
    bool has_coverage = false;
    std::uint32_t fallback_glyph_id = 0;
};

struct render_text_font_glyph_id_resolution_snapshot {
    render_text_font_glyph_id_resolution_status status =
        render_text_font_glyph_id_resolution_status::missing_face;
    std::size_t run_index = 0;
    std::size_t codepoint_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::uint32_t codepoint = utf8_replacement_codepoint;
    std::uint32_t glyph_id = 0;
    font_face_id requested_face_id = 0;
    font_face_id resolved_face_id = 0;
    bool valid_utf8 = true;
    bool glyph_supported = false;
    bool used_codepoint_fallback = false;
    bool used_fallback_glyph_id = true;
    bool has_glyph_id = true;
    std::uint32_t glyph_id_offset = 0;
    bool glyph_id_matches_codepoint = false;
    render_text_font_unicode_coverage_status coverage_status =
        render_text_font_unicode_coverage_status::missing_bytes;
    render_text_font_cmap_inspect_status cmap_status =
        render_text_font_cmap_inspect_status::malformed_table;
    std::uint16_t selected_cmap_format = 0;
    std::string source_label;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_glyph_id_resolution_status::resolved
            && glyph_supported
            && !used_fallback_glyph_id;
    }
};

struct render_text_font_glyph_id_resolution_policy_snapshot {
    std::size_t request_count = 0;
    std::size_t resolved_count = 0;
    std::size_t invalid_utf8_count = 0;
    std::size_t missing_face_count = 0;
    std::size_t coverage_invalid_count = 0;
    std::size_t unsupported_codepoint_count = 0;
    std::size_t fallback_glyph_id_count = 0;
    std::size_t supported_glyph_count = 0;
};

struct render_text_font_glyph_id_resolution_run {
    std::vector<render_text_font_glyph_id_resolution_snapshot> resolutions;
    render_text_font_glyph_id_resolution_policy_snapshot policy;
    std::string diagnostic;

    bool ok() const
    {
        return policy.request_count == policy.resolved_count;
    }
};

struct render_text_font_glyph_id_resolution_run_request {
    std::vector<render_text_font_glyph_id_resolution_request> requests;
};

class font_glyph_id_resolver_interface {
public:
    virtual ~font_glyph_id_resolver_interface() = default;

    virtual render_text_font_glyph_id_resolution_snapshot resolve(
        const render_text_font_glyph_id_resolution_request& request) const = 0;

    virtual render_text_font_glyph_id_resolution_run resolve_run(
        const render_text_font_glyph_id_resolution_run_request& request) const = 0;
};

inline std::string font_glyph_id_hex_codepoint_label(const std::uint32_t codepoint)
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

inline void font_glyph_id_append_descriptor_coverage_range(
    std::vector<render_text_font_cmap_range>& ranges,
    std::uint32_t first,
    std::uint32_t last)
{
    if (first > last || first > 0x10ffffU) {
        return;
    }

    last = std::min(last, 0x10ffffU);
    if (first <= 0xd7ffU) {
        ranges.push_back(render_text_font_cmap_range{
            .first_codepoint = static_cast<char32_t>(first),
            .last_codepoint = static_cast<char32_t>(std::min(last, 0xd7ffU)),
        });
    }
    if (last >= 0xe000U) {
        ranges.push_back(render_text_font_cmap_range{
            .first_codepoint = static_cast<char32_t>(std::max(first, 0xe000U)),
            .last_codepoint = static_cast<char32_t>(last),
        });
    }
}

inline std::string font_glyph_id_source_label_for(const font_face_descriptor& face)
{
    if (!face.source_uri.empty()) {
        return face.source_uri;
    }
    if (!face.family.empty()) {
        return face.family;
    }
    return "unlabeled-font-face";
}

inline std::uint32_t font_glyph_id_apply_descriptor_mapping(
    const font_face_descriptor& face,
    const std::uint32_t codepoint)
{
    if (face.glyph_id_offset == 0U) {
        return codepoint;
    }

    const std::uint64_t mapped =
        static_cast<std::uint64_t>(face.glyph_id_offset) + static_cast<std::uint64_t>(codepoint);
    if (mapped > std::numeric_limits<std::uint32_t>::max()) {
        return codepoint;
    }
    return static_cast<std::uint32_t>(mapped);
}

inline render_text_font_unicode_coverage_snapshot font_glyph_id_coverage_snapshot_for_descriptor(
    const font_face_descriptor& face)
{
    std::vector<render_text_font_cmap_range> ranges;
    if (face.coverage.empty()) {
        font_glyph_id_append_descriptor_coverage_range(ranges, 0U, 0x10ffffU);
    } else {
        ranges.reserve(face.coverage.size());
        for (const font_codepoint_range& range : face.coverage) {
            if (font_unicode_coverage_codepoint_range_is_known_empty(range)) {
                continue;
            }
            font_glyph_id_append_descriptor_coverage_range(ranges, range.first, range.last);
        }
    }

    render_text_font_unicode_coverage_snapshot snapshot{
        .source_label = font_glyph_id_source_label_for(face),
        .status = render_text_font_unicode_coverage_status::valid,
        .ranges = ranges,
        .diagnostic = "font face descriptor coverage converted to deterministic glyph-id coverage",
    };
    snapshot.cmap = render_text_font_cmap_inspection{
        .status = render_text_font_cmap_inspect_status::valid,
        .selected_format = 0,
        .ranges = std::move(ranges),
        .diagnostic = "descriptor coverage used as cmap-like coverage diagnostics",
    };
    return snapshot;
}

inline bool font_glyph_id_request_supports_codepoint(
    const render_text_font_glyph_id_resolution_request& request)
{
    if (request.has_coverage) {
        return request.coverage.supports_codepoint(static_cast<char32_t>(request.codepoint.code_point));
    }

    return request.has_resolved_face
        && request.resolved_face.supports_codepoint(request.codepoint.code_point);
}

inline render_text_font_glyph_id_resolution_snapshot make_font_glyph_id_resolution_failure(
    const render_text_font_glyph_id_resolution_request& request,
    const render_text_font_glyph_id_resolution_status status,
    std::string diagnostic)
{
    return render_text_font_glyph_id_resolution_snapshot{
        .status = status,
        .run_index = request.run_index,
        .codepoint_index = request.codepoint_index,
        .byte_offset = request.codepoint.byte_offset,
        .byte_count = request.codepoint.byte_count,
        .codepoint = request.codepoint.code_point,
        .glyph_id = request.fallback_glyph_id,
        .requested_face_id = request.requested_face_id,
        .resolved_face_id = request.has_resolved_face ? request.resolved_face.id : 0,
        .valid_utf8 = request.codepoint.valid,
        .glyph_supported = false,
        .used_codepoint_fallback = request.used_codepoint_fallback,
        .used_fallback_glyph_id = true,
        .has_glyph_id = true,
        .glyph_id_offset = request.has_resolved_face ? request.resolved_face.glyph_id_offset : 0U,
        .glyph_id_matches_codepoint = request.fallback_glyph_id == request.codepoint.code_point,
        .coverage_status = request.has_coverage
            ? request.coverage.status
            : render_text_font_unicode_coverage_status::valid,
        .cmap_status = request.has_coverage
            ? request.coverage.cmap.status
            : render_text_font_cmap_inspect_status::valid,
        .selected_cmap_format = request.has_coverage ? request.coverage.cmap.selected_format : std::uint16_t{0},
        .source_label = request.has_coverage
            ? request.coverage.source_label
            : font_glyph_id_source_label_for(request.resolved_face),
        .diagnostic = std::move(diagnostic),
    };
}

inline render_text_font_glyph_id_resolution_snapshot resolve_font_glyph_id(
    const render_text_font_glyph_id_resolution_request& request)
{
    if (!request.codepoint.valid || !font_unicode_coverage_codepoint_is_unicode_scalar(
            static_cast<char32_t>(request.codepoint.code_point))) {
        return make_font_glyph_id_resolution_failure(
            request,
            render_text_font_glyph_id_resolution_status::invalid_utf8,
            "glyph id resolution requires a valid Unicode scalar");
    }

    if (!request.has_resolved_face || request.resolved_face.id == 0U) {
        return make_font_glyph_id_resolution_failure(
            request,
            render_text_font_glyph_id_resolution_status::missing_face,
            "glyph id resolution requires a resolved font face");
    }

    if (request.has_coverage && !request.coverage.ok()) {
        return make_font_glyph_id_resolution_failure(
            request,
            render_text_font_glyph_id_resolution_status::coverage_invalid,
            "font coverage is "
                + render_text_font_unicode_coverage_status_name(request.coverage.status)
                + " and cmap inspection is "
                + render_text_font_cmap_inspect_status_name(request.coverage.cmap.status)
                + ": " + request.coverage.diagnostic);
    }

    if (!font_glyph_id_request_supports_codepoint(request)) {
        return make_font_glyph_id_resolution_failure(
            request,
            render_text_font_glyph_id_resolution_status::unsupported_codepoint,
            "resolved font face "
                + std::to_string(request.resolved_face.id)
                + " does not cover "
                + font_glyph_id_hex_codepoint_label(request.codepoint.code_point));
    }

    const std::uint32_t glyph_id =
        font_glyph_id_apply_descriptor_mapping(request.resolved_face, request.codepoint.code_point);
    return render_text_font_glyph_id_resolution_snapshot{
        .status = render_text_font_glyph_id_resolution_status::resolved,
        .run_index = request.run_index,
        .codepoint_index = request.codepoint_index,
        .byte_offset = request.codepoint.byte_offset,
        .byte_count = request.codepoint.byte_count,
        .codepoint = request.codepoint.code_point,
        .glyph_id = glyph_id,
        .requested_face_id = request.requested_face_id,
        .resolved_face_id = request.resolved_face.id,
        .valid_utf8 = true,
        .glyph_supported = true,
        .used_codepoint_fallback = request.used_codepoint_fallback,
        .used_fallback_glyph_id = false,
        .has_glyph_id = true,
        .glyph_id_offset = request.resolved_face.glyph_id_offset,
        .glyph_id_matches_codepoint = glyph_id == request.codepoint.code_point,
        .coverage_status = request.has_coverage
            ? request.coverage.status
            : render_text_font_unicode_coverage_status::valid,
        .cmap_status = request.has_coverage
            ? request.coverage.cmap.status
            : render_text_font_cmap_inspect_status::valid,
        .selected_cmap_format = request.has_coverage ? request.coverage.cmap.selected_format : std::uint16_t{0},
        .source_label = request.has_coverage
            ? request.coverage.source_label
            : font_glyph_id_source_label_for(request.resolved_face),
        .diagnostic = "deterministic glyph id resolved from Unicode codepoint coverage",
    };
}

inline void append_font_glyph_id_resolution(
    std::vector<render_text_font_glyph_id_resolution_snapshot>& resolutions,
    render_text_font_glyph_id_resolution_policy_snapshot& policy,
    render_text_font_glyph_id_resolution_snapshot snapshot)
{
    ++policy.request_count;
    if (snapshot.glyph_supported) {
        ++policy.supported_glyph_count;
    }
    if (snapshot.used_fallback_glyph_id) {
        ++policy.fallback_glyph_id_count;
    }

    switch (snapshot.status) {
    case render_text_font_glyph_id_resolution_status::resolved:
        ++policy.resolved_count;
        break;
    case render_text_font_glyph_id_resolution_status::invalid_utf8:
        ++policy.invalid_utf8_count;
        break;
    case render_text_font_glyph_id_resolution_status::missing_face:
        ++policy.missing_face_count;
        break;
    case render_text_font_glyph_id_resolution_status::coverage_invalid:
        ++policy.coverage_invalid_count;
        break;
    case render_text_font_glyph_id_resolution_status::unsupported_codepoint:
        ++policy.unsupported_codepoint_count;
        break;
    }

    resolutions.push_back(std::move(snapshot));
}

inline void append_font_glyph_id_resolution(
    render_text_font_glyph_id_resolution_run& run,
    render_text_font_glyph_id_resolution_snapshot snapshot)
{
    append_font_glyph_id_resolution(
        run.resolutions,
        run.policy,
        std::move(snapshot));
}

inline render_text_font_glyph_id_resolution_run resolve_font_glyph_ids(
    const render_text_font_glyph_id_resolution_run_request& request)
{
    render_text_font_glyph_id_resolution_run run;
    run.resolutions.reserve(request.requests.size());
    for (const render_text_font_glyph_id_resolution_request& item : request.requests) {
        append_font_glyph_id_resolution(run, resolve_font_glyph_id(item));
    }

    run.diagnostic = run.ok()
        ? "all UTF-8 codepoints resolved to deterministic glyph ids"
        : "glyph id resolution produced diagnostics for "
            + std::to_string(run.policy.request_count - run.policy.resolved_count)
            + " codepoint(s)";
    return run;
}

inline render_text_font_shaping_codepoint_selection font_glyph_id_resolution_to_shaping_selection(
    const render_text_font_glyph_id_resolution_snapshot& resolution)
{
    return render_text_font_shaping_codepoint_selection{
        .requested_face_id = resolution.requested_face_id,
        .resolved_face_id = resolution.resolved_face_id,
        .glyph_id = resolution.glyph_id,
        .has_glyph_id = resolution.has_glyph_id,
        .glyph_id_offset = resolution.glyph_id_offset,
        .glyph_id_matches_codepoint = resolution.glyph_id_matches_codepoint,
        .glyph_supported = resolution.glyph_supported,
        .used_codepoint_fallback = resolution.used_codepoint_fallback,
        .used_fallback_glyph_id = resolution.used_fallback_glyph_id,
    };
}

class deterministic_font_glyph_id_resolver final
    : public font_glyph_id_resolver_interface {
public:
    render_text_font_glyph_id_resolution_snapshot resolve(
        const render_text_font_glyph_id_resolution_request& request) const override
    {
        return resolve_font_glyph_id(request);
    }

    render_text_font_glyph_id_resolution_run resolve_run(
        const render_text_font_glyph_id_resolution_run_request& request) const override
    {
        return resolve_font_glyph_ids(request);
    }
};

} // namespace quiz_vulkan::render
