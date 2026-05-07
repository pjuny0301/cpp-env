#pragma once

#include "render/text/font_backend_capabilities.h"
#include "render/text/font_rasterizer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_backend_adapter_status {
    shaped,
    rasterized,
    backend_unavailable,
    unsupported_script,
    unsupported_glyph,
    glyph_id_mismatch,
    recoverable_backend_failure,
    fatal_backend_failure,
};

inline std::string render_text_font_backend_adapter_status_name(
    const render_text_font_backend_adapter_status status)
{
    switch (status) {
    case render_text_font_backend_adapter_status::shaped:
        return "shaped";
    case render_text_font_backend_adapter_status::rasterized:
        return "rasterized";
    case render_text_font_backend_adapter_status::backend_unavailable:
        return "backend_unavailable";
    case render_text_font_backend_adapter_status::unsupported_script:
        return "unsupported_script";
    case render_text_font_backend_adapter_status::unsupported_glyph:
        return "unsupported_glyph";
    case render_text_font_backend_adapter_status::glyph_id_mismatch:
        return "glyph_id_mismatch";
    case render_text_font_backend_adapter_status::recoverable_backend_failure:
        return "recoverable_backend_failure";
    case render_text_font_backend_adapter_status::fatal_backend_failure:
        return "fatal_backend_failure";
    }

    return "unknown";
}

struct render_text_font_backend_adapter_diagnostic {
    render_text_font_backend_adapter_status status =
        render_text_font_backend_adapter_status::backend_unavailable;
    render_text_font_backend_library library =
        render_text_font_backend_library::deterministic_fake;
    render_text_font_backend_capability_status capability_status =
        render_text_font_backend_capability_status::unavailable;
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::uint32_t codepoint = utf8_replacement_codepoint;
    std::uint32_t expected_glyph_id = 0;
    std::uint32_t actual_glyph_id = 0;
    bool recoverable = false;
    bool fatal = false;
    std::string diagnostic;
};

struct render_text_real_font_shaping_adapter_request {
    render_text_font_backend_capability_snapshot capability;
    render_text_font_backend_library library =
        render_text_font_backend_library::harfbuzz;
    std::size_t run_index = 0;
    render_style_id style_token;
    render_text_style style;
    std::vector<utf8_text_codepoint> codepoints;
    std::vector<utf8_text_cluster> clusters;
    std::vector<render_text_font_shaping_codepoint_selection> font_selections;
    std::uint32_t fallback_glyph_id = 0;
    std::string source_label;
};

struct render_text_real_font_shaping_adapter_result {
    render_text_font_backend_adapter_status status =
        render_text_font_backend_adapter_status::backend_unavailable;
    render_text_font_backend_library library =
        render_text_font_backend_library::harfbuzz;
    render_text_font_backend_capability_status capability_status =
        render_text_font_backend_capability_status::unavailable;
    std::size_t run_index = 0;
    render_style_id style_token;
    std::vector<render_text_shaped_glyph> glyphs;
    std::vector<render_text_font_backend_adapter_diagnostic> diagnostics;
    bool recoverable = false;
    bool fatal = false;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_backend_adapter_status::shaped;
    }

    bool can_fallback() const
    {
        return recoverable
            || status == render_text_font_backend_adapter_status::backend_unavailable
            || status == render_text_font_backend_adapter_status::unsupported_script
            || status == render_text_font_backend_adapter_status::unsupported_glyph
            || status == render_text_font_backend_adapter_status::glyph_id_mismatch
            || status == render_text_font_backend_adapter_status::recoverable_backend_failure;
    }

    bool has_diagnostic(const render_text_font_backend_adapter_status diagnostic_status) const
    {
        return std::any_of(
            diagnostics.begin(),
            diagnostics.end(),
            [&](const render_text_font_backend_adapter_diagnostic& item) {
                return item.status == diagnostic_status;
            });
    }
};

struct render_text_real_font_raster_adapter_request {
    render_text_font_backend_capability_snapshot capability;
    render_text_font_backend_library library =
        render_text_font_backend_library::freetype;
    render_text_font_rasterize_request rasterize;
};

struct render_text_real_font_raster_adapter_result {
    render_text_font_backend_adapter_status status =
        render_text_font_backend_adapter_status::backend_unavailable;
    render_text_font_backend_library library =
        render_text_font_backend_library::freetype;
    render_text_font_backend_capability_status capability_status =
        render_text_font_backend_capability_status::unavailable;
    render_text_font_rasterize_result rasterized;
    std::vector<render_text_font_backend_adapter_diagnostic> diagnostics;
    bool recoverable = false;
    bool fatal = false;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_backend_adapter_status::rasterized;
    }

    bool can_fallback() const
    {
        return recoverable
            || status == render_text_font_backend_adapter_status::backend_unavailable
            || status == render_text_font_backend_adapter_status::unsupported_glyph
            || status == render_text_font_backend_adapter_status::recoverable_backend_failure;
    }
};

class font_backend_adapter_interface {
public:
    virtual ~font_backend_adapter_interface() = default;

    virtual render_text_real_font_shaping_adapter_result shape(
        const render_text_real_font_shaping_adapter_request& request) const = 0;

    virtual render_text_real_font_raster_adapter_result rasterize(
        const render_text_real_font_raster_adapter_request& request) const = 0;
};

using render_text_font_backend_shape_callback =
    render_text_real_font_shaping_adapter_result (*)(const render_text_real_font_shaping_adapter_request&);

using render_text_font_backend_raster_callback =
    render_text_real_font_raster_adapter_result (*)(const render_text_real_font_raster_adapter_request&);

struct render_text_font_backend_adapter_functions {
    render_text_font_backend_shape_callback shape = nullptr;
    render_text_font_backend_raster_callback rasterize = nullptr;
    std::string label;
};

inline bool render_text_font_backend_adapter_capability_allows_shaping(
    const render_text_font_backend_capability_snapshot& capability)
{
    return capability.ok()
        && capability.supports_feature(render_text_font_backend_feature::glyph_shaping);
}

inline bool render_text_font_backend_adapter_capability_allows_rasterization(
    const render_text_font_backend_capability_snapshot& capability)
{
    return capability.ok()
        && capability.supports_feature(render_text_font_backend_feature::glyph_rasterization);
}

inline bool render_text_font_backend_adapter_request_requires_complex_shaping(
    const render_text_real_font_shaping_adapter_request& request)
{
    return std::any_of(
        request.codepoints.begin(),
        request.codepoints.end(),
        [](const utf8_text_codepoint& codepoint) {
            return codepoint.valid
                && font_shaping_backend_codepoint_requires_complex_backend(codepoint.code_point);
        });
}

inline utf8_text_cluster render_text_font_backend_adapter_cluster_for_codepoint(
    const render_text_real_font_shaping_adapter_request& request,
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

inline render_text_font_shaping_codepoint_selection render_text_font_backend_adapter_selection_for_codepoint(
    const render_text_real_font_shaping_adapter_request& request,
    const std::size_t codepoint_index)
{
    if (codepoint_index < request.font_selections.size()) {
        return request.font_selections[codepoint_index];
    }
    return {};
}

inline render_text_font_backend_adapter_diagnostic render_text_font_backend_adapter_diagnostic_for_request(
    const render_text_real_font_shaping_adapter_request& request,
    const render_text_font_backend_adapter_status status,
    std::string diagnostic)
{
    return render_text_font_backend_adapter_diagnostic{
        .status = status,
        .library = request.library,
        .capability_status = request.capability.status,
        .run_index = request.run_index,
        .recoverable = status != render_text_font_backend_adapter_status::fatal_backend_failure,
        .fatal = status == render_text_font_backend_adapter_status::fatal_backend_failure,
        .diagnostic = std::move(diagnostic),
    };
}

inline render_text_real_font_shaping_adapter_result make_render_text_font_backend_adapter_shape_failure(
    const render_text_real_font_shaping_adapter_request& request,
    const render_text_font_backend_adapter_status status,
    std::string diagnostic)
{
    render_text_real_font_shaping_adapter_result result{
        .status = status,
        .library = request.library,
        .capability_status = request.capability.status,
        .run_index = request.run_index,
        .style_token = request.style_token,
        .recoverable = status != render_text_font_backend_adapter_status::fatal_backend_failure,
        .fatal = status == render_text_font_backend_adapter_status::fatal_backend_failure,
        .diagnostic = std::move(diagnostic),
    };
    result.diagnostics.push_back(render_text_font_backend_adapter_diagnostic_for_request(
        request,
        status,
        result.diagnostic));
    return result;
}

inline render_text_real_font_raster_adapter_result make_render_text_font_backend_adapter_raster_failure(
    const render_text_real_font_raster_adapter_request& request,
    const render_text_font_backend_adapter_status status,
    std::string diagnostic)
{
    render_text_real_font_raster_adapter_result result{
        .status = status,
        .library = request.library,
        .capability_status = request.capability.status,
        .recoverable = status != render_text_font_backend_adapter_status::fatal_backend_failure,
        .fatal = status == render_text_font_backend_adapter_status::fatal_backend_failure,
        .diagnostic = std::move(diagnostic),
    };
    result.diagnostics.push_back(render_text_font_backend_adapter_diagnostic{
        .status = status,
        .library = request.library,
        .capability_status = request.capability.status,
        .codepoint = request.rasterize.codepoint,
        .expected_glyph_id = request.rasterize.key.glyph_id,
        .actual_glyph_id = request.rasterize.key.glyph_id,
        .recoverable = result.recoverable,
        .fatal = result.fatal,
        .diagnostic = result.diagnostic,
    });
    return result;
}

inline const utf8_text_codepoint* render_text_font_backend_adapter_first_complex_codepoint(
    const render_text_real_font_shaping_adapter_request& request)
{
    const auto match = std::find_if(
        request.codepoints.begin(),
        request.codepoints.end(),
        [](const utf8_text_codepoint& codepoint) {
            return codepoint.valid
                && font_shaping_backend_codepoint_requires_complex_backend(codepoint.code_point);
        });
    return match == request.codepoints.end() ? nullptr : &*match;
}

inline render_text_real_font_shaping_adapter_result make_render_text_font_backend_adapter_unsupported_script(
    const render_text_real_font_shaping_adapter_request& request)
{
    render_text_real_font_shaping_adapter_result result =
        make_render_text_font_backend_adapter_shape_failure(
            request,
            render_text_font_backend_adapter_status::unsupported_script,
            "real font backend capability does not support complex script shaping");
    if (const utf8_text_codepoint* codepoint =
            render_text_font_backend_adapter_first_complex_codepoint(request);
        codepoint != nullptr) {
        result.diagnostics.front().byte_offset = codepoint->byte_offset;
        result.diagnostics.front().byte_count = codepoint->byte_count;
        result.diagnostics.front().codepoint = codepoint->code_point;
    }
    return result;
}

inline void render_text_font_backend_adapter_normalize_shape_result(
    render_text_real_font_shaping_adapter_result& result,
    const render_text_real_font_shaping_adapter_request& request)
{
    result.library = request.library;
    result.capability_status = request.capability.status;
    result.run_index = request.run_index;
    if (result.style_token.empty()) {
        result.style_token = request.style_token;
    }
    for (render_text_font_backend_adapter_diagnostic& diagnostic : result.diagnostics) {
        diagnostic.library = request.library;
        diagnostic.capability_status = request.capability.status;
        diagnostic.run_index = request.run_index;
    }
}

inline void render_text_font_backend_adapter_validate_glyph_ids(
    render_text_real_font_shaping_adapter_result& result,
    const render_text_real_font_shaping_adapter_request& request)
{
    if (!result.ok()) {
        return;
    }

    for (const render_text_shaped_glyph& glyph : result.glyphs) {
        if (glyph.glyph_index >= request.font_selections.size()) {
            continue;
        }
        const render_text_font_shaping_codepoint_selection& selection =
            request.font_selections[glyph.glyph_index];
        if (!selection.has_glyph_id || selection.glyph_id == glyph.glyph_id) {
            continue;
        }

        result.status = render_text_font_backend_adapter_status::glyph_id_mismatch;
        result.recoverable = true;
        result.fatal = false;
        result.diagnostic = "real font backend returned a glyph id that does not match the resolved selection";
        result.diagnostics.push_back(render_text_font_backend_adapter_diagnostic{
            .status = render_text_font_backend_adapter_status::glyph_id_mismatch,
            .library = request.library,
            .capability_status = request.capability.status,
            .run_index = request.run_index,
            .byte_offset = glyph.byte_offset,
            .byte_count = glyph.byte_count,
            .codepoint = glyph.codepoint,
            .expected_glyph_id = selection.glyph_id,
            .actual_glyph_id = glyph.glyph_id,
            .recoverable = true,
            .fatal = false,
            .diagnostic = result.diagnostic,
        });
        return;
    }
}

inline render_text_real_font_shaping_adapter_result deterministic_fake_real_font_backend_shape(
    const render_text_real_font_shaping_adapter_request& request)
{
    render_text_real_font_shaping_adapter_result result{
        .status = render_text_font_backend_adapter_status::shaped,
        .library = request.library,
        .capability_status = request.capability.status,
        .run_index = request.run_index,
        .style_token = request.style_token,
        .diagnostic = "deterministic fake real-font shaping adapter shaped glyph run",
    };
    result.glyphs.reserve(request.codepoints.size());

    for (std::size_t index = 0; index < request.codepoints.size(); ++index) {
        const utf8_text_codepoint& scalar = request.codepoints[index];
        const utf8_text_cluster cluster =
            render_text_font_backend_adapter_cluster_for_codepoint(request, index);
        const render_text_font_shaping_codepoint_selection selection =
            render_text_font_backend_adapter_selection_for_codepoint(request, index);
        if (!selection.glyph_supported) {
            return make_render_text_font_backend_adapter_shape_failure(
                request,
                render_text_font_backend_adapter_status::unsupported_glyph,
                "real font backend adapter selection does not support requested glyph");
        }

        const std::uint32_t glyph_id = selection.has_glyph_id
            ? selection.glyph_id
            : scalar.code_point;
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
            .advance_x = font_shaping_backend_fake_advance_for(request.style, scalar.code_point),
            .advance_y = 0.0f,
            .offset_x = 0.0f,
            .offset_y = 0.0f,
            .valid_utf8 = scalar.valid,
            .cluster_start = scalar.cluster_start,
            .glyph_supported = true,
            .used_codepoint_fallback = selection.used_codepoint_fallback,
            .used_fallback_glyph_id = false,
            .glyph_id_from_selection = selection.has_glyph_id,
            .glyph_id_offset = selection.glyph_id_offset,
            .glyph_id_matches_codepoint = glyph_id == scalar.code_point,
            .zero_advance = font_shaping_backend_fake_advance_for(request.style, scalar.code_point) == 0.0f,
            .combining_mark = is_utf8_combining_mark(scalar.code_point),
        });
    }

    result.diagnostics.push_back(render_text_font_backend_adapter_diagnostic{
        .status = render_text_font_backend_adapter_status::shaped,
        .library = request.library,
        .capability_status = request.capability.status,
        .run_index = request.run_index,
        .byte_offset = request.codepoints.empty() ? 0U : request.codepoints.front().byte_offset,
        .byte_count = request.codepoints.empty()
            ? 0U
            : (request.codepoints.back().byte_offset + request.codepoints.back().byte_count)
                - request.codepoints.front().byte_offset,
        .diagnostic = result.diagnostic,
    });
    return result;
}

inline render_text_real_font_raster_adapter_result deterministic_fake_real_font_backend_rasterize(
    const render_text_real_font_raster_adapter_request& request)
{
    const deterministic_fake_font_rasterizer rasterizer;
    const render_text_font_rasterize_result rasterized = rasterizer.rasterize(request.rasterize);
    const render_text_font_backend_adapter_status status = rasterized.ok()
        ? render_text_font_backend_adapter_status::rasterized
        : render_text_font_backend_adapter_status::recoverable_backend_failure;

    render_text_real_font_raster_adapter_result result{
        .status = status,
        .library = request.library,
        .capability_status = request.capability.status,
        .rasterized = rasterized,
        .recoverable = !rasterized.ok(),
        .fatal = false,
        .diagnostic = rasterized.ok()
            ? "deterministic fake real-font raster adapter rasterized glyph"
            : rasterized.diagnostic,
    };
    result.diagnostics.push_back(render_text_font_backend_adapter_diagnostic{
        .status = status,
        .library = request.library,
        .capability_status = request.capability.status,
        .codepoint = request.rasterize.codepoint,
        .expected_glyph_id = request.rasterize.key.glyph_id,
        .actual_glyph_id = rasterized.glyph_id,
        .recoverable = result.recoverable,
        .fatal = false,
        .diagnostic = result.diagnostic,
    });
    return result;
}

class function_table_font_backend_adapter final : public font_backend_adapter_interface {
public:
    function_table_font_backend_adapter()
        : functions_{
              .shape = deterministic_fake_real_font_backend_shape,
              .rasterize = deterministic_fake_real_font_backend_rasterize,
              .label = "deterministic fake real-font backend adapter",
          }
    {
    }

    explicit function_table_font_backend_adapter(render_text_font_backend_adapter_functions functions)
        : functions_(std::move(functions))
    {
        if (functions_.shape == nullptr) {
            functions_.shape = deterministic_fake_real_font_backend_shape;
        }
        if (functions_.rasterize == nullptr) {
            functions_.rasterize = deterministic_fake_real_font_backend_rasterize;
        }
    }

    render_text_real_font_shaping_adapter_result shape(
        const render_text_real_font_shaping_adapter_request& request) const override
    {
        if (!render_text_font_backend_adapter_capability_allows_shaping(request.capability)) {
            return make_render_text_font_backend_adapter_shape_failure(
                request,
                render_text_font_backend_adapter_status::backend_unavailable,
                "font backend capability does not allow real shaping adapter calls");
        }
        if (render_text_font_backend_adapter_request_requires_complex_shaping(request)
            && !request.capability.supports_feature(render_text_font_backend_feature::complex_script_shaping)) {
            return make_render_text_font_backend_adapter_unsupported_script(request);
        }

        render_text_real_font_shaping_adapter_result result = functions_.shape(request);
        render_text_font_backend_adapter_normalize_shape_result(result, request);
        render_text_font_backend_adapter_validate_glyph_ids(result, request);
        return result;
    }

    render_text_real_font_raster_adapter_result rasterize(
        const render_text_real_font_raster_adapter_request& request) const override
    {
        if (!render_text_font_backend_adapter_capability_allows_rasterization(request.capability)) {
            return make_render_text_font_backend_adapter_raster_failure(
                request,
                render_text_font_backend_adapter_status::backend_unavailable,
                "font backend capability does not allow real raster adapter calls");
        }

        render_text_real_font_raster_adapter_result result = functions_.rasterize(request);
        result.library = request.library;
        result.capability_status = request.capability.status;
        for (render_text_font_backend_adapter_diagnostic& diagnostic : result.diagnostics) {
            diagnostic.library = request.library;
            diagnostic.capability_status = request.capability.status;
        }
        return result;
    }

private:
    render_text_font_backend_adapter_functions functions_;
};

} // namespace quiz_vulkan::render
