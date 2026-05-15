#include "render/text/font_backend_adapter.h"

#ifndef QUIZ_VULKAN_HAS_FREETYPE_EXTERNAL_LIBRARY
#define QUIZ_VULKAN_HAS_FREETYPE_EXTERNAL_LIBRARY 0
#endif

#ifndef QUIZ_VULKAN_HAS_FREETYPE_HEADERS
#define QUIZ_VULKAN_HAS_FREETYPE_HEADERS 0
#endif

#if QUIZ_VULKAN_HAS_FREETYPE_EXTERNAL_LIBRARY && QUIZ_VULKAN_HAS_FREETYPE_HEADERS
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>

namespace quiz_vulkan::render {

namespace {

std::string freetype_memory_face_source_label_for(
    const render_text_freetype_memory_face_load_request& request)
{
    if (!request.source_label.empty()) {
        return request.source_label;
    }
    return font_unicode_coverage_source_label_for(request.source_bytes);
}

render_text_freetype_memory_face_load_status freetype_memory_face_load_status_for(
    const render_text_font_face_byte_readiness_status status)
{
    switch (status) {
    case render_text_font_face_byte_readiness_status::missing_bytes:
        return render_text_freetype_memory_face_load_status::missing_bytes;
    case render_text_font_face_byte_readiness_status::empty_bytes:
        return render_text_freetype_memory_face_load_status::empty_bytes;
    case render_text_font_face_byte_readiness_status::invalid_sfnt:
        return render_text_freetype_memory_face_load_status::invalid_sfnt;
    case render_text_font_face_byte_readiness_status::missing_cmap:
        return render_text_freetype_memory_face_load_status::missing_cmap;
    case render_text_font_face_byte_readiness_status::invalid_cmap:
        return render_text_freetype_memory_face_load_status::invalid_cmap;
    case render_text_font_face_byte_readiness_status::fallback_required:
        return render_text_freetype_memory_face_load_status::fallback_required;
    case render_text_font_face_byte_readiness_status::coverage_ready:
        return render_text_freetype_memory_face_load_status::loaded;
    }

    return render_text_freetype_memory_face_load_status::fallback_required;
}

render_text_freetype_memory_face_load_result make_freetype_memory_face_load_result(
    const render_text_freetype_memory_face_load_request& request,
    const render_text_font_face_byte_readiness& face_bytes,
    const render_text_freetype_memory_face_load_status status,
    std::string diagnostic)
{
    return render_text_freetype_memory_face_load_result{
        .status = status,
        .face_id = face_bytes.face_id,
        .source_label = freetype_memory_face_source_label_for(request),
        .face_index = request.face_index,
        .source_bytes_status = face_bytes.source_bytes_status,
        .face_byte_status = face_bytes.status,
        .sfnt_status = face_bytes.sfnt_status,
        .cmap_status = face_bytes.cmap_status,
        .byte_count = face_bytes.byte_count,
        .coverage_range_count = face_bytes.coverage_range_count,
        .backend_available = render_text_freetype_memory_face_adapter_available(),
        .materialized_bytes_ready = face_bytes.source_loaded,
        .coverage_ready = face_bytes.coverage_ready,
        .deterministic_fallback_required =
            status != render_text_freetype_memory_face_load_status::loaded,
        .diagnostic = std::move(diagnostic),
    };
}

#if QUIZ_VULKAN_HAS_FREETYPE_EXTERNAL_LIBRARY && QUIZ_VULKAN_HAS_FREETYPE_HEADERS
float freetype_26_6_to_float(const FT_Pos value)
{
    return static_cast<float>(value) / 64.0f;
}

class freetype_library_owner final {
public:
    freetype_library_owner() = default;

    freetype_library_owner(const freetype_library_owner&) = delete;
    freetype_library_owner& operator=(const freetype_library_owner&) = delete;

    ~freetype_library_owner()
    {
        if (library_ != nullptr) {
            FT_Done_FreeType(library_);
        }
    }

    FT_Error init()
    {
        return FT_Init_FreeType(&library_);
    }

    FT_Library get() const
    {
        return library_;
    }

private:
    FT_Library library_ = nullptr;
};

class freetype_face_owner final {
public:
    freetype_face_owner() = default;

    freetype_face_owner(const freetype_face_owner&) = delete;
    freetype_face_owner& operator=(const freetype_face_owner&) = delete;

    ~freetype_face_owner()
    {
        if (face_ != nullptr) {
            FT_Done_Face(face_);
        }
    }

    FT_Face* out()
    {
        return &face_;
    }

    FT_Face get() const
    {
        return face_;
    }

private:
    FT_Face face_ = nullptr;
};
#endif

render_text_real_font_raster_adapter_result make_freetype_real_font_backend_raster_result(
    const render_text_real_font_raster_adapter_request& request,
    const render_text_font_backend_adapter_status status,
    render_text_font_rasterize_result rasterized,
    std::string diagnostic)
{
    render_text_real_font_raster_adapter_result result{
        .status = status,
        .library = request.library,
        .capability_status = request.capability.status,
        .rasterized = std::move(rasterized),
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
        .actual_glyph_id = result.rasterized.glyph_id,
        .recoverable = result.recoverable,
        .fatal = result.fatal,
        .diagnostic = result.diagnostic,
    });
    return result;
}

render_text_real_font_raster_adapter_result make_freetype_real_font_backend_raster_failure(
    const render_text_real_font_raster_adapter_request& request,
    const render_text_font_backend_adapter_status adapter_status,
    const render_text_font_rasterizer_status rasterizer_status,
    std::string diagnostic)
{
    render_text_font_rasterize_result rasterized =
        make_font_rasterizer_failure(request.rasterize, rasterizer_status, diagnostic);
    return make_freetype_real_font_backend_raster_result(
        request,
        adapter_status,
        std::move(rasterized),
        std::move(diagnostic));
}

} // namespace

bool render_text_freetype_memory_face_adapter_available()
{
#if QUIZ_VULKAN_HAS_FREETYPE_EXTERNAL_LIBRARY && QUIZ_VULKAN_HAS_FREETYPE_HEADERS
    return true;
#else
    return false;
#endif
}

render_text_real_font_raster_adapter_result freetype_real_font_backend_rasterize(
    const render_text_real_font_raster_adapter_request& request)
{
    if (request.rasterize.pixel_size == 0U) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            render_text_font_rasterizer_status::invalid_pixel_size,
            "FreeType glyph rasterization pixel size must be greater than zero");
    }

    const render_text_font_rasterizer_status missing_status =
        font_rasterizer_missing_status_for(request.rasterize.font_bytes_status);
    if (missing_status != render_text_font_rasterizer_status::rasterized) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            missing_status,
            "FreeType glyph rasterization font bytes load status is "
                + render_text_font_source_bytes_load_status_name(request.rasterize.font_bytes_status));
    }
    if (request.rasterize.font_bytes.empty()) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            render_text_font_rasterizer_status::missing_font_bytes,
            "FreeType glyph rasterization requires materialized font bytes");
    }

#if QUIZ_VULKAN_HAS_FREETYPE_EXTERNAL_LIBRARY && QUIZ_VULKAN_HAS_FREETYPE_HEADERS
    if (request.rasterize.font_bytes.size()
        > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max())) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            render_text_font_rasterizer_status::missing_font_bytes,
            "font byte buffer exceeds FreeType FT_Long size limit");
    }

    freetype_library_owner library;
    const FT_Error init_error = library.init();
    if (init_error != 0) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            render_text_font_rasterizer_status::missing_font_source,
            "FT_Init_FreeType failed with error " + std::to_string(init_error));
    }

    freetype_face_owner face;
    const FT_Error face_error = FT_New_Memory_Face(
        library.get(),
        reinterpret_cast<const FT_Byte*>(request.rasterize.font_bytes.data()),
        static_cast<FT_Long>(request.rasterize.font_bytes.size()),
        0,
        face.out());
    if (face_error != 0) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            render_text_font_rasterizer_status::missing_font_bytes,
            "FT_New_Memory_Face failed with error " + std::to_string(face_error));
    }

    FT_Face ft_face = face.get();
    const FT_Error size_error = FT_Set_Pixel_Sizes(ft_face, 0, request.rasterize.pixel_size);
    if (size_error != 0) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            render_text_font_rasterizer_status::invalid_pixel_size,
            "FT_Set_Pixel_Sizes failed with error " + std::to_string(size_error));
    }

    if (FT_Select_Charmap(ft_face, FT_ENCODING_UNICODE) != 0 && ft_face->charmap == nullptr) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::unsupported_glyph,
            render_text_font_rasterizer_status::unsupported_glyph,
            "FreeType memory face does not expose a Unicode charmap for glyph lookup");
    }

    const FT_UInt glyph_index = FT_Get_Char_Index(
        ft_face,
        static_cast<FT_ULong>(request.rasterize.codepoint));
    if (glyph_index == 0U) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::unsupported_glyph,
            render_text_font_rasterizer_status::unsupported_glyph,
            "FreeType Unicode charmap does not contain the requested codepoint");
    }

    const FT_Error load_error = FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
    if (load_error != 0) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            render_text_font_rasterizer_status::unsupported_glyph,
            "FT_Load_Glyph failed with error " + std::to_string(load_error));
    }

    FT_GlyphSlot glyph = ft_face->glyph;
    const FT_Error render_error = FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);
    if (render_error != 0) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            render_text_font_rasterizer_status::unsupported_glyph,
            "FT_Render_Glyph failed with error " + std::to_string(render_error));
    }

    const FT_Bitmap& ft_bitmap = glyph->bitmap;
    if (ft_bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
        return make_freetype_real_font_backend_raster_failure(
            request,
            render_text_font_backend_adapter_status::recoverable_backend_failure,
            render_text_font_rasterizer_status::unsupported_glyph,
            "FreeType rendered bitmap is not an 8-bit grayscale glyph bitmap");
    }

    render_text_font_glyph_bitmap bitmap{
        .width = static_cast<std::size_t>(ft_bitmap.width),
        .height = static_cast<std::size_t>(ft_bitmap.rows),
        .row_stride = static_cast<std::size_t>(ft_bitmap.width),
    };
    bitmap.alpha.resize(bitmap.row_stride * bitmap.height);
    const int pitch = ft_bitmap.pitch;
    const std::size_t source_row_stride =
        static_cast<std::size_t>(pitch < 0 ? -pitch : pitch);
    for (std::size_t y = 0; y < bitmap.height; ++y) {
        const std::size_t source_y = pitch < 0 ? bitmap.height - 1U - y : y;
        const unsigned char* source_row =
            ft_bitmap.buffer + (source_y * source_row_stride);
        const std::size_t copy_count = std::min(bitmap.width, source_row_stride);
        std::copy_n(
            source_row,
            copy_count,
            bitmap.alpha.begin() + static_cast<std::ptrdiff_t>(y * bitmap.row_stride));
    }

    render_text_font_glyph_metrics metrics{
        .advance_x = freetype_26_6_to_float(glyph->metrics.horiAdvance),
        .advance_y = freetype_26_6_to_float(glyph->metrics.vertAdvance),
        .bearing_x = freetype_26_6_to_float(glyph->metrics.horiBearingX),
        .bearing_y = freetype_26_6_to_float(glyph->metrics.horiBearingY),
        .ascender = ft_face->size != nullptr
            ? freetype_26_6_to_float(ft_face->size->metrics.ascender)
            : 0.0f,
        .descender = ft_face->size != nullptr
            ? freetype_26_6_to_float(ft_face->size->metrics.descender)
            : 0.0f,
        .bitmap_width = bitmap.width,
        .bitmap_height = bitmap.height,
    };

    glyph_atlas_key resolved_key = request.rasterize.key;
    resolved_key.glyph_id = static_cast<std::uint32_t>(glyph_index);

    render_text_font_rasterize_result rasterized{
        .status = render_text_font_rasterizer_status::rasterized,
        .key = resolved_key,
        .face_id = request.rasterize.face.id,
        .glyph_id = static_cast<std::uint32_t>(glyph_index),
        .codepoint = request.rasterize.codepoint,
        .pixel_size = request.rasterize.pixel_size,
        .metrics = metrics,
        .bitmap = std::move(bitmap),
        .source_label = font_rasterizer_source_label_for(
            request.rasterize.face,
            request.rasterize.source_label),
        .diagnostic = "FreeType glyph metrics and grayscale bitmap rasterized from memory face",
    };

    return make_freetype_real_font_backend_raster_result(
        request,
        render_text_font_backend_adapter_status::rasterized,
        std::move(rasterized),
        "FreeType glyph metrics and grayscale bitmap rasterized from memory face");
#else
    return make_freetype_real_font_backend_raster_failure(
        request,
        render_text_font_backend_adapter_status::backend_unavailable,
        render_text_font_rasterizer_status::missing_font_source,
        "FreeType glyph rasterizer adapter is not compiled; deterministic fallback remains active");
#endif
}

render_text_freetype_memory_face_load_result load_render_text_freetype_memory_face(
    const render_text_freetype_memory_face_load_request& request)
{
    const render_text_font_face_byte_readiness face_bytes =
        inspect_render_text_font_face_byte_readiness(request.source_bytes);
    const render_text_freetype_memory_face_load_status byte_status =
        freetype_memory_face_load_status_for(face_bytes.status);

    if (byte_status != render_text_freetype_memory_face_load_status::loaded) {
        return make_freetype_memory_face_load_result(
            request,
            face_bytes,
            byte_status,
            "FreeType memory-face load blocked by font bytes: " + face_bytes.diagnostic);
    }

#if QUIZ_VULKAN_HAS_FREETYPE_EXTERNAL_LIBRARY && QUIZ_VULKAN_HAS_FREETYPE_HEADERS
    if (request.source_bytes.bytes.size()
        > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max())) {
        return make_freetype_memory_face_load_result(
            request,
            face_bytes,
            render_text_freetype_memory_face_load_status::byte_count_overflow,
            "font byte buffer exceeds FreeType FT_Long size limit");
    }

    freetype_library_owner library;
    const FT_Error init_error = library.init();
    if (init_error != 0) {
        render_text_freetype_memory_face_load_result result =
            make_freetype_memory_face_load_result(
                request,
                face_bytes,
                render_text_freetype_memory_face_load_status::freetype_init_failed,
                "FT_Init_FreeType failed with error " + std::to_string(init_error));
        result.freetype_error = static_cast<int>(init_error);
        return result;
    }

    freetype_face_owner face;
    const FT_Error face_error = FT_New_Memory_Face(
        library.get(),
        reinterpret_cast<const FT_Byte*>(request.source_bytes.bytes.data()),
        static_cast<FT_Long>(request.source_bytes.bytes.size()),
        static_cast<FT_Long>(request.face_index),
        face.out());
    if (face_error != 0) {
        render_text_freetype_memory_face_load_result result =
            make_freetype_memory_face_load_result(
                request,
                face_bytes,
                render_text_freetype_memory_face_load_status::freetype_new_memory_face_failed,
                "FT_New_Memory_Face failed with error " + std::to_string(face_error));
        result.freetype_error = static_cast<int>(face_error);
        return result;
    }

    const FT_Face ft_face = face.get();
    render_text_freetype_memory_face_load_result result =
        make_freetype_memory_face_load_result(
            request,
            face_bytes,
            render_text_freetype_memory_face_load_status::loaded,
            "FreeType created a memory face from materialized font bytes");
    result.face_created = true;
    result.face_count = static_cast<long>(ft_face->num_faces);
    result.glyph_count = static_cast<long>(ft_face->num_glyphs);
    result.family_name = ft_face->family_name != nullptr ? ft_face->family_name : "";
    result.style_name = ft_face->style_name != nullptr ? ft_face->style_name : "";
    result.scalable = FT_IS_SCALABLE(ft_face) != 0;
    result.fixed_size = FT_HAS_FIXED_SIZES(ft_face) != 0;
    result.deterministic_fallback_required = false;
    return result;
#else
    return make_freetype_memory_face_load_result(
        request,
        face_bytes,
        render_text_freetype_memory_face_load_status::backend_unavailable,
        "FreeType memory-face adapter is not compiled; deterministic fallback remains active");
#endif
}

} // namespace quiz_vulkan::render
