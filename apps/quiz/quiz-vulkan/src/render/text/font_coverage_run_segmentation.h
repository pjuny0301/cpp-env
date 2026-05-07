#pragma once

#include "render/text/font_glyph_atlas.h"
#include "render/text/utf8_text_run.h"

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

    for (std::size_t index = 0; index < codepoints.size(); ++index) {
        const utf8_text_codepoint& scalar = codepoints[index];
        render_text_font_coverage_run_segment segment = scalar.valid
            ? make_font_coverage_codepoint_segment(scalar, index, catalog, style)
            : make_font_coverage_invalid_utf8_segment(scalar, index, catalog, style);

        if (!segment.valid_utf8) {
            ++segmentation.invalid_utf8_count;
        } else if (!segment.glyph_supported) {
            ++segmentation.unsupported_codepoint_count;
        } else {
            ++segmentation.supported_codepoint_count;
        }
        if (segment.used_fallback && segment.glyph_supported) {
            ++segmentation.fallback_codepoint_count;
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

} // namespace quiz_vulkan::render
