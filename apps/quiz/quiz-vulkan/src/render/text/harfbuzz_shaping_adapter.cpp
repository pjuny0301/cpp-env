#include "render/text/font_backend_adapter.h"

#ifndef QUIZ_VULKAN_HAS_HARFBUZZ_EXTERNAL_LIBRARY
#define QUIZ_VULKAN_HAS_HARFBUZZ_EXTERNAL_LIBRARY 0
#endif

#ifndef QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS
#define QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS 0
#endif

#if QUIZ_VULKAN_HAS_HARFBUZZ_EXTERNAL_LIBRARY && QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS
#include <hb.h>
#include <hb-ot.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace quiz_vulkan::render {

namespace {

#if QUIZ_VULKAN_HAS_HARFBUZZ_EXTERNAL_LIBRARY && QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS
class harfbuzz_blob_owner final {
public:
    explicit harfbuzz_blob_owner(hb_blob_t* blob)
        : blob_(blob)
    {
    }

    harfbuzz_blob_owner(const harfbuzz_blob_owner&) = delete;
    harfbuzz_blob_owner& operator=(const harfbuzz_blob_owner&) = delete;

    ~harfbuzz_blob_owner()
    {
        if (blob_ != nullptr) {
            hb_blob_destroy(blob_);
        }
    }

    hb_blob_t* get() const
    {
        return blob_;
    }

private:
    hb_blob_t* blob_ = nullptr;
};

class harfbuzz_face_owner final {
public:
    explicit harfbuzz_face_owner(hb_face_t* face)
        : face_(face)
    {
    }

    harfbuzz_face_owner(const harfbuzz_face_owner&) = delete;
    harfbuzz_face_owner& operator=(const harfbuzz_face_owner&) = delete;

    ~harfbuzz_face_owner()
    {
        if (face_ != nullptr) {
            hb_face_destroy(face_);
        }
    }

    hb_face_t* get() const
    {
        return face_;
    }

private:
    hb_face_t* face_ = nullptr;
};

class harfbuzz_font_owner final {
public:
    explicit harfbuzz_font_owner(hb_font_t* font)
        : font_(font)
    {
    }

    harfbuzz_font_owner(const harfbuzz_font_owner&) = delete;
    harfbuzz_font_owner& operator=(const harfbuzz_font_owner&) = delete;

    ~harfbuzz_font_owner()
    {
        if (font_ != nullptr) {
            hb_font_destroy(font_);
        }
    }

    hb_font_t* get() const
    {
        return font_;
    }

private:
    hb_font_t* font_ = nullptr;
};

class harfbuzz_buffer_owner final {
public:
    explicit harfbuzz_buffer_owner(hb_buffer_t* buffer)
        : buffer_(buffer)
    {
    }

    harfbuzz_buffer_owner(const harfbuzz_buffer_owner&) = delete;
    harfbuzz_buffer_owner& operator=(const harfbuzz_buffer_owner&) = delete;

    ~harfbuzz_buffer_owner()
    {
        if (buffer_ != nullptr) {
            hb_buffer_destroy(buffer_);
        }
    }

    hb_buffer_t* get() const
    {
        return buffer_;
    }

private:
    hb_buffer_t* buffer_ = nullptr;
};

float harfbuzz_position_to_float(const hb_position_t value)
{
    return static_cast<float>(value) / 64.0f;
}
#endif

render_text_real_font_shaping_adapter_result make_harfbuzz_shape_failure(
    const render_text_real_font_shaping_adapter_request& request,
    const render_text_font_backend_adapter_status status,
    std::string diagnostic)
{
    return make_render_text_font_backend_adapter_shape_failure(
        request,
        status,
        std::move(diagnostic));
}

utf8_text_cluster harfbuzz_cluster_for_byte_offset(
    const render_text_real_font_shaping_adapter_request& request,
    const std::size_t byte_offset)
{
    for (const utf8_text_cluster& cluster : request.clusters) {
        const std::size_t cluster_end = cluster.byte_offset + cluster.byte_count;
        if (byte_offset >= cluster.byte_offset && byte_offset < cluster_end) {
            return cluster;
        }
    }

    for (std::size_t index = 0; index < request.codepoints.size(); ++index) {
        const utf8_text_codepoint& scalar = request.codepoints[index];
        const std::size_t scalar_end = scalar.byte_offset + scalar.byte_count;
        if (byte_offset >= scalar.byte_offset && byte_offset < scalar_end) {
            return utf8_text_cluster{
                .byte_offset = scalar.byte_offset,
                .byte_count = scalar.byte_count,
                .codepoint_offset = index,
                .codepoint_count = 1,
                .valid = scalar.valid,
            };
        }
    }

    if (!request.codepoints.empty()) {
        const utf8_text_codepoint& scalar = request.codepoints.front();
        return utf8_text_cluster{
            .byte_offset = scalar.byte_offset,
            .byte_count = scalar.byte_count,
            .codepoint_offset = 0,
            .codepoint_count = 1,
            .valid = scalar.valid,
        };
    }

    return {};
}

std::uint32_t harfbuzz_codepoint_for_cluster(
    const render_text_real_font_shaping_adapter_request& request,
    const utf8_text_cluster& cluster)
{
    if (cluster.codepoint_offset < request.codepoints.size()) {
        return request.codepoints[cluster.codepoint_offset].code_point;
    }
    return utf8_replacement_codepoint;
}

render_text_font_shaping_codepoint_selection harfbuzz_selection_for_cluster(
    const render_text_real_font_shaping_adapter_request& request,
    const utf8_text_cluster& cluster)
{
    return render_text_font_backend_adapter_selection_for_codepoint(
        request,
        cluster.codepoint_offset);
}

} // namespace

bool render_text_harfbuzz_shaping_adapter_available()
{
#if QUIZ_VULKAN_HAS_HARFBUZZ_EXTERNAL_LIBRARY && QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS
    return true;
#else
    return false;
#endif
}

render_text_real_font_shaping_adapter_result harfbuzz_real_font_backend_shape(
    const render_text_real_font_shaping_adapter_request& request)
{
    if (request.source_bytes.status != render_text_font_source_bytes_load_status::loaded) {
        return make_harfbuzz_shape_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            "HarfBuzz shaping requires loaded materialized font bytes; source byte status is "
                + render_text_font_source_bytes_load_status_name(request.source_bytes.status));
    }
    if (request.source_bytes.bytes.empty()) {
        return make_harfbuzz_shape_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            "HarfBuzz shaping requires non-empty materialized font bytes");
    }

#if QUIZ_VULKAN_HAS_HARFBUZZ_EXTERNAL_LIBRARY && QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS
    constexpr std::size_t max_harfbuzz_uint =
        static_cast<std::size_t>(std::numeric_limits<unsigned int>::max());
    if (request.source_bytes.bytes.size() > max_harfbuzz_uint) {
        return make_harfbuzz_shape_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            "font byte buffer exceeds HarfBuzz blob size limit");
    }

    for (const utf8_text_codepoint& scalar : request.codepoints) {
        if (scalar.byte_offset > max_harfbuzz_uint) {
            return make_harfbuzz_shape_failure(
                request,
                render_text_font_backend_adapter_status::recoverable_backend_failure,
                "UTF-8 byte offset exceeds HarfBuzz cluster size limit");
        }
    }

    harfbuzz_blob_owner blob(hb_blob_create(
        reinterpret_cast<const char*>(request.source_bytes.bytes.data()),
        static_cast<unsigned int>(request.source_bytes.bytes.size()),
        HB_MEMORY_MODE_READONLY,
        nullptr,
        nullptr));
    if (blob.get() == nullptr || hb_blob_get_length(blob.get()) == 0U) {
        return make_harfbuzz_shape_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            "hb_blob_create failed to retain materialized font bytes");
    }

    harfbuzz_face_owner face(hb_face_create(blob.get(), 0));
    if (face.get() == nullptr || hb_face_get_glyph_count(face.get()) == 0U) {
        return make_harfbuzz_shape_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            "hb_face_create could not create a glyph-bearing face from materialized font bytes");
    }

    harfbuzz_font_owner font(hb_font_create(face.get()));
    if (font.get() == nullptr) {
        return make_harfbuzz_shape_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            "hb_font_create failed for HarfBuzz memory face");
    }

    const int font_scale = std::max(1, static_cast<int>(request.style.font_size * 64.0f));
    hb_font_set_scale(font.get(), font_scale, font_scale);
    hb_ot_font_set_funcs(font.get());

    harfbuzz_buffer_owner buffer(hb_buffer_create());
    if (buffer.get() == nullptr) {
        return make_harfbuzz_shape_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            "hb_buffer_create failed for HarfBuzz shaping");
    }

    hb_buffer_set_content_type(buffer.get(), HB_BUFFER_CONTENT_TYPE_UNICODE);
    for (const utf8_text_codepoint& scalar : request.codepoints) {
        hb_buffer_add(
            buffer.get(),
            static_cast<hb_codepoint_t>(scalar.valid ? scalar.code_point : utf8_replacement_codepoint),
            static_cast<unsigned int>(scalar.byte_offset));
    }
    hb_buffer_guess_segment_properties(buffer.get());
    hb_shape(font.get(), buffer.get(), nullptr, 0);

    unsigned int shaped_count = 0;
    const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer.get(), &shaped_count);
    const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer.get(), nullptr);
    if ((infos == nullptr || positions == nullptr) && shaped_count != 0U) {
        return make_harfbuzz_shape_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            "HarfBuzz did not expose shaped glyph info and position arrays");
    }

    render_text_real_font_shaping_adapter_result result{
        .status = render_text_font_backend_adapter_status::shaped,
        .library = request.library,
        .capability_status = request.capability.status,
        .run_index = request.run_index,
        .style_token = request.style_token,
        .diagnostic = "HarfBuzz shaped glyph run from materialized font bytes",
    };
    result.glyphs.reserve(shaped_count);

    bool unsupported_glyph = false;
    for (unsigned int shaped_index = 0; shaped_index < shaped_count; ++shaped_index) {
        const std::size_t cluster_byte_offset = static_cast<std::size_t>(infos[shaped_index].cluster);
        const utf8_text_cluster cluster =
            harfbuzz_cluster_for_byte_offset(request, cluster_byte_offset);
        const render_text_font_shaping_codepoint_selection selection =
            harfbuzz_selection_for_cluster(request, cluster);
        const std::uint32_t glyph_id = static_cast<std::uint32_t>(infos[shaped_index].codepoint);
        const bool glyph_supported = glyph_id != 0U;
        unsupported_glyph = unsupported_glyph || !glyph_supported;

        result.glyphs.push_back(render_text_shaped_glyph{
            .run_index = request.run_index,
            .glyph_index = shaped_index,
            .byte_offset = cluster.byte_offset,
            .byte_count = cluster.byte_count,
            .cluster_byte_offset = cluster.byte_offset,
            .cluster_byte_count = cluster.byte_count,
            .cluster_codepoint_offset = cluster.codepoint_offset,
            .cluster_codepoint_count = cluster.codepoint_count,
            .codepoint = harfbuzz_codepoint_for_cluster(request, cluster),
            .glyph_id = glyph_id,
            .requested_face_id = selection.requested_face_id,
            .resolved_face_id = selection.resolved_face_id,
            .advance_x = harfbuzz_position_to_float(positions[shaped_index].x_advance),
            .advance_y = harfbuzz_position_to_float(positions[shaped_index].y_advance),
            .offset_x = harfbuzz_position_to_float(positions[shaped_index].x_offset),
            .offset_y = harfbuzz_position_to_float(positions[shaped_index].y_offset),
            .valid_utf8 = cluster.valid,
            .cluster_start = shaped_index == 0U
                || infos[shaped_index].cluster != infos[shaped_index - 1U].cluster,
            .glyph_supported = glyph_supported,
            .used_codepoint_fallback = selection.used_codepoint_fallback,
            .used_fallback_glyph_id = false,
            .glyph_id_from_selection = false,
            .glyph_id_offset = selection.glyph_id_offset,
            .glyph_id_matches_codepoint = glyph_id == harfbuzz_codepoint_for_cluster(request, cluster),
            .zero_advance = positions[shaped_index].x_advance == 0
                && positions[shaped_index].y_advance == 0,
            .combining_mark = is_utf8_combining_mark(harfbuzz_codepoint_for_cluster(request, cluster)),
        });
    }

    const std::size_t byte_count = request.codepoints.empty()
        ? 0U
        : (request.codepoints.back().byte_offset + request.codepoints.back().byte_count)
            - request.codepoints.front().byte_offset;
    result.diagnostics.push_back(render_text_font_backend_adapter_diagnostic{
        .status = unsupported_glyph
            ? render_text_font_backend_adapter_status::unsupported_glyph
            : render_text_font_backend_adapter_status::shaped,
        .library = request.library,
        .capability_status = request.capability.status,
        .run_index = request.run_index,
        .byte_offset = request.codepoints.empty() ? 0U : request.codepoints.front().byte_offset,
        .byte_count = byte_count,
        .recoverable = unsupported_glyph,
        .fatal = false,
        .diagnostic = unsupported_glyph
            ? "HarfBuzz shaped at least one .notdef glyph from materialized font bytes"
            : result.diagnostic,
    });

    if (unsupported_glyph) {
        result.status = render_text_font_backend_adapter_status::unsupported_glyph;
        result.recoverable = true;
        result.fatal = false;
        result.diagnostic = "HarfBuzz shaped at least one .notdef glyph from materialized font bytes";
    }
    return result;
#else
    return make_harfbuzz_shape_failure(
        request,
        render_text_font_backend_adapter_status::backend_unavailable,
        "HarfBuzz shaping adapter is not compiled; deterministic fallback remains active");
#endif
}

} // namespace quiz_vulkan::render
