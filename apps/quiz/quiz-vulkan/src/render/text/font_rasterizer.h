#pragma once

#include "render/text/font_backend_capabilities.h"
#include "render/text/font_glyph_atlas.h"
#include "render/text/font_source_bytes_loader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_rasterizer_status {
    rasterized,
    atlas_cache_hit,
    missing_font_source,
    missing_font_bytes,
    unsupported_glyph,
    invalid_pixel_size,
};

inline std::string render_text_font_rasterizer_status_name(
    const render_text_font_rasterizer_status status)
{
    switch (status) {
    case render_text_font_rasterizer_status::rasterized:
        return "rasterized";
    case render_text_font_rasterizer_status::atlas_cache_hit:
        return "atlas_cache_hit";
    case render_text_font_rasterizer_status::missing_font_source:
        return "missing_font_source";
    case render_text_font_rasterizer_status::missing_font_bytes:
        return "missing_font_bytes";
    case render_text_font_rasterizer_status::unsupported_glyph:
        return "unsupported_glyph";
    case render_text_font_rasterizer_status::invalid_pixel_size:
        return "invalid_pixel_size";
    }

    return "unknown";
}

struct render_text_font_glyph_bitmap {
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t row_stride = 0;
    std::vector<unsigned char> alpha;

    bool empty() const
    {
        return width == 0U || height == 0U || alpha.empty();
    }
};

struct render_text_font_glyph_metrics {
    float advance_x = 0.0f;
    float advance_y = 0.0f;
    float bearing_x = 0.0f;
    float bearing_y = 0.0f;
    float ascender = 0.0f;
    float descender = 0.0f;
    std::size_t bitmap_width = 0;
    std::size_t bitmap_height = 0;
};

struct render_text_font_rasterize_request {
    font_face_descriptor face;
    glyph_atlas_key key;
    std::uint32_t codepoint = 0;
    std::uint32_t pixel_size = 0;
    std::span<const std::byte> font_bytes;
    render_text_font_source_bytes_load_status font_bytes_status =
        render_text_font_source_bytes_load_status::loaded;
    std::string source_label;
};

struct render_text_font_rasterize_result {
    render_text_font_rasterizer_status status =
        render_text_font_rasterizer_status::missing_font_source;
    glyph_atlas_key key;
    font_face_id face_id = 0;
    std::uint32_t glyph_id = 0;
    std::uint32_t codepoint = 0;
    std::uint32_t pixel_size = 0;
    render_text_font_glyph_metrics metrics;
    render_text_font_glyph_bitmap bitmap;
    std::string source_label;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_rasterizer_status::rasterized;
    }

    bool has_bitmap() const
    {
        return ok() && !bitmap.empty();
    }
};

struct render_text_font_atlas_glyph_payload {
    glyph_atlas_key key;
    render_text_font_glyph_metrics metrics;
    std::size_t width = 0;
    std::size_t height = 0;
    std::vector<unsigned char> alpha;
    std::vector<unsigned char> rgba;
    bool upload_ready = false;
};

struct render_text_rasterized_glyph_atlas_payload_snapshot {
    std::size_t cluster_index = 0;
    std::size_t run_index = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_count = 0;
    std::uint32_t codepoint = 0;
    std::uint32_t glyph_id = 0;
    font_face_id resolved_face_id = 0;
    glyph_atlas_key cache_key;
    render_text_font_rasterizer_status status =
        render_text_font_rasterizer_status::missing_font_source;
    render_text_font_glyph_metrics metrics;
    std::size_t bitmap_width = 0;
    std::size_t bitmap_height = 0;
    std::size_t alpha_bytes = 0;
    std::size_t rgba_bytes = 0;
    std::string source_label;
    std::string diagnostic;
    bool glyph_id_from_selection = false;
    bool glyph_id_matches_codepoint = false;
    bool used_fallback_glyph_id = false;
    std::uint32_t glyph_id_offset = 0;
    render_text_font_backend_capability_status font_backend_capability_status =
        render_text_font_backend_capability_status::unavailable;
    render_text_font_backend_library font_backend_library =
        render_text_font_backend_library::deterministic_fake;
    std::string font_backend_label;
    bool font_backend_used_deterministic_fallback = false;
    bool font_backend_fallback_only = false;
    bool font_backend_supports_rasterization = false;
    render_text_font_source_bytes_load_status source_bytes_status =
        render_text_font_source_bytes_load_status::missing_source;
    bool materialized_font_bytes = false;
    bool used_freetype_rasterizer = false;
    bool uses_deterministic_rasterizer = true;
    std::string deterministic_fallback_reason;
    bool atlas_cache_hit = false;
    bool reused_atlas_payload = false;
    bool rasterization_skipped_for_atlas_cache_hit = false;
    bool cacheable = false;
    bool upload_ready = false;
    bool skipped = true;
};

struct render_text_rasterized_glyph_atlas_payload_policy_snapshot {
    std::size_t request_count = 0;
    std::size_t rasterized_count = 0;
    std::size_t skipped_count = 0;
    std::size_t upload_ready_count = 0;
    std::size_t missing_font_source_count = 0;
    std::size_t missing_font_bytes_count = 0;
    std::size_t unsupported_glyph_count = 0;
    std::size_t invalid_pixel_size_count = 0;
    std::size_t deterministic_rasterizer_count = 0;
    std::size_t freetype_rasterizer_count = 0;
    std::size_t materialized_font_bytes_count = 0;
    std::size_t deterministic_fallback_reason_count = 0;
    std::size_t atlas_cache_hit_count = 0;
    std::size_t reused_atlas_payload_count = 0;
    std::size_t rasterization_skipped_for_atlas_cache_hit_count = 0;
    std::size_t total_alpha_bytes = 0;
    std::size_t total_rgba_bytes = 0;
};

class font_rasterizer_interface {
public:
    virtual ~font_rasterizer_interface() = default;

    virtual render_text_font_rasterize_result rasterize(
        const render_text_font_rasterize_request& request) const = 0;
};

inline glyph_atlas_key font_rasterizer_atlas_key_for(
    const font_face_descriptor& face,
    const std::uint32_t codepoint,
    const std::uint32_t pixel_size)
{
    return glyph_atlas_key{
        .face_id = face.id,
        .glyph_id = codepoint,
        .pixel_size = pixel_size,
    };
}

inline std::string font_rasterizer_source_label_for(
    const font_face_descriptor& face,
    const std::string& explicit_label = {})
{
    if (!explicit_label.empty()) {
        return explicit_label;
    }
    if (!face.source_uri.empty()) {
        return face.source_uri;
    }
    return face.family;
}

inline render_text_font_rasterize_request make_font_rasterize_request(
    font_face_descriptor face,
    const glyph_atlas_key& key,
    const std::uint32_t codepoint,
    const std::span<const std::byte> font_bytes,
    std::string source_label = {})
{
    return render_text_font_rasterize_request{
        .face = std::move(face),
        .key = key,
        .codepoint = codepoint,
        .pixel_size = key.pixel_size,
        .font_bytes = font_bytes,
        .font_bytes_status = render_text_font_source_bytes_load_status::loaded,
        .source_label = std::move(source_label),
    };
}

inline render_text_font_rasterize_request make_font_rasterize_request(
    font_face_descriptor face,
    const std::uint32_t codepoint,
    const std::uint32_t pixel_size,
    const std::span<const std::byte> font_bytes,
    std::string source_label = {})
{
    const glyph_atlas_key key = font_rasterizer_atlas_key_for(face, codepoint, pixel_size);
    return make_font_rasterize_request(
        std::move(face),
        key,
        codepoint,
        font_bytes,
        std::move(source_label));
}

inline render_text_font_rasterize_request make_font_rasterize_request(
    font_face_descriptor face,
    const render_text_font_source_bytes_load_result& font_bytes,
    const std::uint32_t codepoint,
    const std::uint32_t pixel_size)
{
    const glyph_atlas_key key = font_rasterizer_atlas_key_for(face, codepoint, pixel_size);
    return render_text_font_rasterize_request{
        .face = std::move(face),
        .key = key,
        .codepoint = codepoint,
        .pixel_size = pixel_size,
        .font_bytes = std::span<const std::byte>{font_bytes.bytes.data(), font_bytes.bytes.size()},
        .font_bytes_status = font_bytes.status,
        .source_label = font_bytes.resolved_path.empty()
            ? font_bytes.cache_key
            : font_bytes.resolved_path,
    };
}

render_text_font_rasterize_request make_font_rasterize_request(
    font_face_descriptor,
    render_text_font_source_bytes_load_result&&,
    std::uint32_t,
    std::uint32_t) = delete;

inline render_text_font_rasterizer_status font_rasterizer_missing_status_for(
    const render_text_font_source_bytes_load_status status)
{
    switch (status) {
    case render_text_font_source_bytes_load_status::missing_source:
    case render_text_font_source_bytes_load_status::unsupported_source:
    case render_text_font_source_bytes_load_status::invalid_cache_key:
    case render_text_font_source_bytes_load_status::path_traversal_blocked:
        return render_text_font_rasterizer_status::missing_font_source;
    case render_text_font_source_bytes_load_status::available_virtual_fixture:
    case render_text_font_source_bytes_load_status::missing_bytes:
    case render_text_font_source_bytes_load_status::empty_bytes:
    case render_text_font_source_bytes_load_status::read_error:
        return render_text_font_rasterizer_status::missing_font_bytes;
    case render_text_font_source_bytes_load_status::loaded:
        return render_text_font_rasterizer_status::rasterized;
    }

    return render_text_font_rasterizer_status::missing_font_bytes;
}

inline render_text_font_atlas_glyph_payload make_font_rasterizer_atlas_payload(
    const render_text_font_rasterize_result& result)
{
    render_text_font_atlas_glyph_payload payload{
        .key = result.key,
        .metrics = result.metrics,
        .width = result.bitmap.width,
        .height = result.bitmap.height,
        .alpha = result.bitmap.alpha,
        .upload_ready = result.has_bitmap(),
    };

    if (!payload.upload_ready) {
        return payload;
    }

    payload.rgba.resize(payload.alpha.size() * 4U);
    for (std::size_t index = 0; index < payload.alpha.size(); ++index) {
        const unsigned char alpha = payload.alpha[index];
        const std::size_t offset = index * 4U;
        payload.rgba[offset] = alpha;
        payload.rgba[offset + 1U] = alpha;
        payload.rgba[offset + 2U] = alpha;
        payload.rgba[offset + 3U] = alpha;
    }
    return payload;
}

inline render_text_font_rasterize_result make_font_rasterizer_failure(
    const render_text_font_rasterize_request& request,
    const render_text_font_rasterizer_status status,
    std::string diagnostic)
{
    return render_text_font_rasterize_result{
        .status = status,
        .key = request.key,
        .face_id = request.face.id,
        .glyph_id = request.key.glyph_id,
        .codepoint = request.codepoint,
        .pixel_size = request.pixel_size,
        .source_label = font_rasterizer_source_label_for(request.face, request.source_label),
        .diagnostic = std::move(diagnostic),
    };
}

class deterministic_fake_font_rasterizer final : public font_rasterizer_interface {
public:
    render_text_font_rasterize_result rasterize(
        const render_text_font_rasterize_request& request) const override
    {
        if (request.pixel_size == 0U) {
            return make_font_rasterizer_failure(
                request,
                render_text_font_rasterizer_status::invalid_pixel_size,
                "font rasterization pixel size must be greater than zero");
        }

        const render_text_font_rasterizer_status missing_status =
            font_rasterizer_missing_status_for(request.font_bytes_status);
        if (missing_status != render_text_font_rasterizer_status::rasterized) {
            return make_font_rasterizer_failure(
                request,
                missing_status,
                "font bytes load status is "
                    + render_text_font_source_bytes_load_status_name(request.font_bytes_status));
        }
        if (request.face.source_uri.empty()) {
            return make_font_rasterizer_failure(
                request,
                render_text_font_rasterizer_status::missing_font_source,
                "font face has no source URI");
        }
        if (request.font_bytes.empty()) {
            return make_font_rasterizer_failure(
                request,
                render_text_font_rasterizer_status::missing_font_bytes,
                "font rasterization requires concrete font bytes");
        }
        if (!request.face.supports_codepoint(request.codepoint)) {
            return make_font_rasterizer_failure(
                request,
                render_text_font_rasterizer_status::unsupported_glyph,
                "font face coverage does not include requested glyph");
        }

        const std::size_t dimension =
            static_cast<std::size_t>(std::min<std::uint32_t>(request.pixel_size, 8U));
        render_text_font_glyph_bitmap bitmap{
            .width = std::max<std::size_t>(1U, dimension),
            .height = std::max<std::size_t>(1U, dimension),
            .row_stride = std::max<std::size_t>(1U, dimension),
        };
        bitmap.alpha.resize(bitmap.row_stride * bitmap.height);

        const std::uint32_t seed =
            request.face.id * 13U
            + request.key.glyph_id * 7U
            + request.pixel_size * 3U
            + static_cast<std::uint32_t>(request.font_bytes.size());
        for (std::size_t y = 0; y < bitmap.height; ++y) {
            for (std::size_t x = 0; x < bitmap.width; ++x) {
                const std::size_t offset = y * bitmap.row_stride + x;
                bitmap.alpha[offset] = static_cast<unsigned char>(
                    ((seed + static_cast<std::uint32_t>(x * 17U) + static_cast<std::uint32_t>(y * 31U)) % 255U) + 1U);
            }
        }

        const float pixel_size = static_cast<float>(request.pixel_size);
        render_text_font_glyph_metrics metrics{
            .advance_x = pixel_size * 0.625f,
            .advance_y = 0.0f,
            .bearing_x = pixel_size * 0.125f,
            .bearing_y = pixel_size * 0.75f,
            .ascender = pixel_size * 0.75f,
            .descender = pixel_size * 0.25f,
            .bitmap_width = bitmap.width,
            .bitmap_height = bitmap.height,
        };

        return render_text_font_rasterize_result{
            .status = render_text_font_rasterizer_status::rasterized,
            .key = request.key,
            .face_id = request.face.id,
            .glyph_id = request.key.glyph_id,
            .codepoint = request.codepoint,
            .pixel_size = request.pixel_size,
            .metrics = metrics,
            .bitmap = std::move(bitmap),
            .source_label = font_rasterizer_source_label_for(request.face, request.source_label),
            .diagnostic = "deterministic fake glyph rasterized",
        };
    }
};

} // namespace quiz_vulkan::render
