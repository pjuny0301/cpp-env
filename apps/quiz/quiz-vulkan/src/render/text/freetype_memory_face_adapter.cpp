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

} // namespace

bool render_text_freetype_memory_face_adapter_available()
{
#if QUIZ_VULKAN_HAS_FREETYPE_EXTERNAL_LIBRARY && QUIZ_VULKAN_HAS_FREETYPE_HEADERS
    return true;
#else
    return false;
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
