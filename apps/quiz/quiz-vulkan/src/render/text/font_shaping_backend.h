#pragma once

#include "render/text/font_glyph_atlas.h"
#include "render/text/utf8_text_run.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_shaping_backend_status {
    shaped,
    backend_unavailable,
    unsupported_script,
    unsupported_glyph,
    fallback_glyph_id,
    zero_advance_combining_mark,
};

inline std::string render_text_font_shaping_backend_status_name(
    const render_text_font_shaping_backend_status status)
{
    switch (status) {
    case render_text_font_shaping_backend_status::shaped:
        return "shaped";
    case render_text_font_shaping_backend_status::backend_unavailable:
        return "backend_unavailable";
    case render_text_font_shaping_backend_status::unsupported_script:
        return "unsupported_script";
    case render_text_font_shaping_backend_status::unsupported_glyph:
        return "unsupported_glyph";
    case render_text_font_shaping_backend_status::fallback_glyph_id:
        return "fallback_glyph_id";
    case render_text_font_shaping_backend_status::zero_advance_combining_mark:
        return "zero_advance_combining_mark";
    }

    return "unknown";
}

struct render_text_font_shaping_codepoint_selection {
    font_face_id requested_face_id = 0;
    font_face_id resolved_face_id = 0;
    bool glyph_supported = true;
    bool used_codepoint_fallback = false;
};

struct render_text_font_shaping_request {
    std::size_t run_index = 0;
    render_style_id style_token;
    render_text_style style;
    std::vector<utf8_text_codepoint> codepoints;
    std::vector<utf8_text_cluster> clusters;
    std::vector<render_text_font_shaping_codepoint_selection> font_selections;
    bool backend_available = true;
    bool support_complex_scripts = true;
    std::uint32_t fallback_glyph_id = 0;
};

struct render_text_shaped_glyph {
    std::size_t run_index = 0;
    std::size_t glyph_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::size_t cluster_byte_offset = 0;
    std::size_t cluster_byte_count = 0;
    std::size_t cluster_codepoint_offset = 0;
    std::size_t cluster_codepoint_count = 0;
    std::uint32_t codepoint = utf8_replacement_codepoint;
    std::uint32_t glyph_id = 0;
    font_face_id requested_face_id = 0;
    font_face_id resolved_face_id = 0;
    float advance_x = 0.0f;
    float advance_y = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    bool valid_utf8 = true;
    bool cluster_start = true;
    bool glyph_supported = true;
    bool used_codepoint_fallback = false;
    bool used_fallback_glyph_id = false;
    bool zero_advance = false;
    bool combining_mark = false;
};

struct render_text_font_shaping_diagnostic {
    render_text_font_shaping_backend_status status =
        render_text_font_shaping_backend_status::shaped;
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::uint32_t codepoint = utf8_replacement_codepoint;
    std::uint32_t glyph_id = 0;
    font_face_id resolved_face_id = 0;
    std::string diagnostic;
};

struct render_text_font_shaping_policy_snapshot {
    std::size_t run_count = 0;
    std::size_t shaped_run_count = 0;
    std::size_t codepoint_count = 0;
    std::size_t glyph_count = 0;
    std::size_t supported_glyph_count = 0;
    std::size_t backend_unavailable_count = 0;
    std::size_t unsupported_script_count = 0;
    std::size_t unsupported_glyph_count = 0;
    std::size_t fallback_glyph_id_count = 0;
    std::size_t zero_advance_combining_mark_count = 0;
};

struct render_text_font_shaping_result {
    render_text_font_shaping_backend_status status =
        render_text_font_shaping_backend_status::backend_unavailable;
    std::size_t run_index = 0;
    render_style_id style_token;
    std::vector<render_text_shaped_glyph> glyphs;
    std::vector<render_text_font_shaping_diagnostic> diagnostics;
    render_text_font_shaping_policy_snapshot policy;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_shaping_backend_status::shaped;
    }

    bool has_diagnostic(const render_text_font_shaping_backend_status diagnostic_status) const
    {
        return std::any_of(
            diagnostics.begin(),
            diagnostics.end(),
            [&](const render_text_font_shaping_diagnostic& item) {
                return item.status == diagnostic_status;
            });
    }
};

class font_shaping_backend_interface {
public:
    virtual ~font_shaping_backend_interface() = default;

    virtual render_text_font_shaping_result shape(
        const render_text_font_shaping_request& request) const = 0;
};

inline bool font_shaping_backend_codepoint_is_hangul_or_cjk(const std::uint32_t codepoint)
{
    return (codepoint >= 0x1100U && codepoint <= 0x11ffU)
        || (codepoint >= 0x3130U && codepoint <= 0x318fU)
        || (codepoint >= 0x3400U && codepoint <= 0x4dbfU)
        || (codepoint >= 0x4e00U && codepoint <= 0x9fffU)
        || (codepoint >= 0xac00U && codepoint <= 0xd7afU);
}

inline bool font_shaping_backend_codepoint_is_wide_symbol(const std::uint32_t codepoint)
{
    return (codepoint >= 0x1f000U && codepoint <= 0x1faffU)
        || (codepoint >= 0xff01U && codepoint <= 0xff60U);
}

inline bool font_shaping_backend_codepoint_requires_complex_backend(const std::uint32_t codepoint)
{
    return (codepoint >= 0x0590U && codepoint <= 0x08ffU)
        || (codepoint >= 0x0900U && codepoint <= 0x0dffU)
        || (codepoint >= 0xfb1dU && codepoint <= 0xfdffU)
        || (codepoint >= 0xfe70U && codepoint <= 0xfeffU);
}

inline float font_shaping_backend_fake_advance_for(
    const render_text_style& style,
    const std::uint32_t codepoint)
{
    if (codepoint == '\n' || codepoint == '\r' || is_utf8_combining_mark(codepoint)) {
        return 0.0f;
    }
    if (codepoint == '\t') {
        return (style.font_size * 2.0f) + style.letter_spacing;
    }
    if (font_shaping_backend_codepoint_is_hangul_or_cjk(codepoint)
        || font_shaping_backend_codepoint_is_wide_symbol(codepoint)) {
        return style.font_size + style.letter_spacing;
    }
    return (style.font_size * 0.5f) + style.letter_spacing;
}

inline utf8_text_cluster font_shaping_backend_cluster_for_codepoint(
    const render_text_font_shaping_request& request,
    const std::size_t codepoint_index)
{
    for (const utf8_text_cluster& cluster : request.clusters) {
        if (codepoint_index >= cluster.codepoint_offset
            && codepoint_index < cluster.codepoint_offset + cluster.codepoint_count) {
            return cluster;
        }
    }

    if (codepoint_index < request.codepoints.size()) {
        const utf8_text_codepoint& scalar = request.codepoints[codepoint_index];
        return utf8_text_cluster{
            .byte_offset = scalar.byte_offset,
            .byte_count = scalar.byte_count,
            .codepoint_offset = codepoint_index,
            .codepoint_count = 1,
            .valid = scalar.valid,
        };
    }

    return {};
}

inline render_text_font_shaping_codepoint_selection font_shaping_backend_selection_for_codepoint(
    const render_text_font_shaping_request& request,
    const std::size_t codepoint_index)
{
    if (codepoint_index < request.font_selections.size()) {
        return request.font_selections[codepoint_index];
    }
    return {};
}

inline void font_shaping_backend_count_diagnostic(
    render_text_font_shaping_policy_snapshot& policy,
    const render_text_font_shaping_backend_status status)
{
    switch (status) {
    case render_text_font_shaping_backend_status::shaped:
        ++policy.shaped_run_count;
        return;
    case render_text_font_shaping_backend_status::backend_unavailable:
        ++policy.backend_unavailable_count;
        return;
    case render_text_font_shaping_backend_status::unsupported_script:
        ++policy.unsupported_script_count;
        return;
    case render_text_font_shaping_backend_status::unsupported_glyph:
        ++policy.unsupported_glyph_count;
        return;
    case render_text_font_shaping_backend_status::fallback_glyph_id:
        ++policy.fallback_glyph_id_count;
        return;
    case render_text_font_shaping_backend_status::zero_advance_combining_mark:
        ++policy.zero_advance_combining_mark_count;
        return;
    }
}

inline void font_shaping_backend_append_diagnostic(
    render_text_font_shaping_result& result,
    render_text_font_shaping_diagnostic diagnostic)
{
    font_shaping_backend_count_diagnostic(result.policy, diagnostic.status);
    result.diagnostics.push_back(std::move(diagnostic));
}

class deterministic_fake_font_shaping_backend final : public font_shaping_backend_interface {
public:
    render_text_font_shaping_result shape(
        const render_text_font_shaping_request& request) const override
    {
        render_text_font_shaping_result result{
            .status = render_text_font_shaping_backend_status::shaped,
            .run_index = request.run_index,
            .style_token = request.style_token,
            .policy = render_text_font_shaping_policy_snapshot{
                .run_count = 1,
                .codepoint_count = request.codepoints.size(),
            },
            .diagnostic = "deterministic fake shaping backend shaped glyph run",
        };

        if (!request.backend_available) {
            result.status = render_text_font_shaping_backend_status::backend_unavailable;
            result.diagnostic = "font shaping backend is unavailable";
            font_shaping_backend_append_diagnostic(
                result,
                render_text_font_shaping_diagnostic{
                    .status = render_text_font_shaping_backend_status::backend_unavailable,
                    .run_index = request.run_index,
                    .diagnostic = result.diagnostic,
                });
            return result;
        }

        result.glyphs.reserve(request.codepoints.size());
        for (std::size_t index = 0; index < request.codepoints.size(); ++index) {
            const utf8_text_codepoint& scalar = request.codepoints[index];
            const utf8_text_cluster cluster = font_shaping_backend_cluster_for_codepoint(request, index);
            const render_text_font_shaping_codepoint_selection selection =
                font_shaping_backend_selection_for_codepoint(request, index);
            const bool unsupported_script =
                !request.support_complex_scripts
                && scalar.valid
                && font_shaping_backend_codepoint_requires_complex_backend(scalar.code_point);
            const bool unsupported_glyph = !selection.glyph_supported;
            const bool combining_mark = is_utf8_combining_mark(scalar.code_point);
            const float advance = font_shaping_backend_fake_advance_for(request.style, scalar.code_point);
            const bool zero_advance = advance == 0.0f;
            const bool glyph_supported = !unsupported_script && !unsupported_glyph;
            const bool uses_fallback_glyph_id = !glyph_supported;
            const std::uint32_t glyph_id =
                uses_fallback_glyph_id ? request.fallback_glyph_id : scalar.code_point;

            result.glyphs.push_back(render_text_shaped_glyph{
                .run_index = request.run_index,
                .glyph_index = index,
                .byte_offset = scalar.byte_offset,
                .byte_count = scalar.byte_count,
                .cluster_byte_offset = cluster.byte_offset,
                .cluster_byte_count = cluster.byte_count,
                .cluster_codepoint_offset = cluster.codepoint_offset,
                .cluster_codepoint_count = cluster.codepoint_count,
                .codepoint = scalar.code_point,
                .glyph_id = glyph_id,
                .requested_face_id = selection.requested_face_id,
                .resolved_face_id = selection.resolved_face_id,
                .advance_x = advance,
                .advance_y = 0.0f,
                .offset_x = 0.0f,
                .offset_y = 0.0f,
                .valid_utf8 = scalar.valid,
                .cluster_start = scalar.cluster_start,
                .glyph_supported = glyph_supported,
                .used_codepoint_fallback = selection.used_codepoint_fallback,
                .used_fallback_glyph_id = uses_fallback_glyph_id,
                .zero_advance = zero_advance,
                .combining_mark = combining_mark,
            });
            ++result.policy.glyph_count;
            if (glyph_supported) {
                ++result.policy.supported_glyph_count;
            }

            if (unsupported_script) {
                result.status = render_text_font_shaping_backend_status::unsupported_script;
                font_shaping_backend_append_diagnostic(
                    result,
                    render_text_font_shaping_diagnostic{
                        .status = render_text_font_shaping_backend_status::unsupported_script,
                        .run_index = request.run_index,
                        .byte_offset = scalar.byte_offset,
                        .byte_count = scalar.byte_count,
                        .codepoint = scalar.code_point,
                        .glyph_id = glyph_id,
                        .resolved_face_id = selection.resolved_face_id,
                        .diagnostic = "fake shaping backend does not support complex script shaping",
                    });
            } else if (unsupported_glyph) {
                result.status = render_text_font_shaping_backend_status::unsupported_glyph;
                font_shaping_backend_append_diagnostic(
                    result,
                    render_text_font_shaping_diagnostic{
                        .status = render_text_font_shaping_backend_status::unsupported_glyph,
                        .run_index = request.run_index,
                        .byte_offset = scalar.byte_offset,
                        .byte_count = scalar.byte_count,
                        .codepoint = scalar.code_point,
                        .glyph_id = glyph_id,
                        .resolved_face_id = selection.resolved_face_id,
                        .diagnostic = "font face selection does not support requested glyph",
                    });
            }

            if (uses_fallback_glyph_id) {
                font_shaping_backend_append_diagnostic(
                    result,
                    render_text_font_shaping_diagnostic{
                        .status = render_text_font_shaping_backend_status::fallback_glyph_id,
                        .run_index = request.run_index,
                        .byte_offset = scalar.byte_offset,
                        .byte_count = scalar.byte_count,
                        .codepoint = scalar.code_point,
                        .glyph_id = glyph_id,
                        .resolved_face_id = selection.resolved_face_id,
                        .diagnostic = "fallback glyph id substituted for unshapeable scalar",
                    });
            }

            if (zero_advance && combining_mark) {
                font_shaping_backend_append_diagnostic(
                    result,
                    render_text_font_shaping_diagnostic{
                        .status = render_text_font_shaping_backend_status::zero_advance_combining_mark,
                        .run_index = request.run_index,
                        .byte_offset = scalar.byte_offset,
                        .byte_count = scalar.byte_count,
                        .codepoint = scalar.code_point,
                        .glyph_id = glyph_id,
                        .resolved_face_id = selection.resolved_face_id,
                        .diagnostic = "combining mark shaped with zero advance",
                    });
            }
        }

        if (result.status == render_text_font_shaping_backend_status::shaped) {
            font_shaping_backend_append_diagnostic(
                result,
                render_text_font_shaping_diagnostic{
                    .status = render_text_font_shaping_backend_status::shaped,
                    .run_index = request.run_index,
                    .byte_offset = request.codepoints.empty() ? 0U : request.codepoints.front().byte_offset,
                    .byte_count = request.codepoints.empty()
                        ? 0U
                        : (request.codepoints.back().byte_offset + request.codepoints.back().byte_count)
                            - request.codepoints.front().byte_offset,
                    .diagnostic = "successfully shaped glyph run",
                });
        } else {
            result.diagnostic = "deterministic fake shaping backend produced shaping diagnostics";
        }

        return result;
    }
};

} // namespace quiz_vulkan::render
