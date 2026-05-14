#if defined(QUIZ_VULKAN_HAS_STB_HEADERS) && QUIZ_VULKAN_HAS_STB_HEADERS
#ifndef STBI_NO_STDIO
#define STBI_NO_STDIO
#endif
#ifndef STB_IMAGE_STATIC
#define STB_IMAGE_STATIC
#endif
#define STB_IMAGE_IMPLEMENTATION
#endif

#include "render/image/third_party_image_decoder_adapter.h"

#if defined(QUIZ_VULKAN_HAS_STB_HEADERS) && QUIZ_VULKAN_HAS_STB_HEADERS
#undef STB_IMAGE_IMPLEMENTATION
#endif

#include <limits>

namespace quiz_vulkan::render {

stb_image_decoder_backend::stb_image_decoder_backend(std::string decoder_id)
    : decoder_id_(std::move(decoder_id))
{
}

third_party_image_decoder_capability stb_image_decoder_backend::inspect(
    const render_image_decode_request& request) const
{
    if (!stb_image_decoder_headers_available()) {
        return make_unavailable_third_party_image_decoder_capability(
            request,
            decoder_id_,
            "stb_image headers are unavailable; falling back to standard image decoders");
    }

    const render_image_format_detection_summary format_detection =
        detect_render_image_format(request);
    if (request.encoded_bytes.empty()) {
        return make_unsupported_third_party_image_decoder_capability(
            request,
            decoder_id_,
            "stb_image decoder requires encoded bytes");
    }
    if (!stb_image_decoder_format_supported(format_detection.detected_format)) {
        return make_unsupported_third_party_image_decoder_capability(
            request,
            decoder_id_,
            "stb_image decoder does not support detected image format");
    }

    return make_supported_third_party_image_decoder_capability(
        request,
        decoder_id_,
        "stb_image decoder supports detected image bytes");
}

render_image_decode_result stb_image_decoder_backend::decode(
    const render_image_decode_request& request) const
{
    if (!stb_image_decoder_headers_available()) {
        return make_failure(
            render_image_decode_status::unsupported_format,
            request,
            "stb_image headers are unavailable; falling back to standard image decoders");
    }
    if (request.encoded_bytes.empty()) {
        return make_failure(
            render_image_decode_status::empty_input,
            request,
            "stb_image decoder requires encoded bytes");
    }

    const render_image_format_detection_summary format_detection =
        detect_render_image_format(request);
    if (!stb_image_decoder_format_supported(format_detection.detected_format)) {
        return make_failure(
            render_image_decode_status::unsupported_format,
            request,
            "stb_image decoder does not support detected image format");
    }
    if (request.encoded_bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return make_failure(
            render_image_decode_status::invalid_data,
            request,
            "stb_image decoder source byte span exceeds supported in-memory size");
    }

#if defined(QUIZ_VULKAN_HAS_STB_HEADERS) && QUIZ_VULKAN_HAS_STB_HEADERS
    constexpr int requested_component_count = 4;
    int width = 0;
    int height = 0;
    int source_component_count = 0;
    stbi_uc* decoded = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(request.encoded_bytes.data()),
        static_cast<int>(request.encoded_bytes.size()),
        &width,
        &height,
        &source_component_count,
        requested_component_count);
    if (decoded == nullptr) {
        return make_failure(
            render_image_decode_status::invalid_data,
            request,
            "stb_image decoder failed to decode image bytes");
    }
    if (width <= 0 || height <= 0) {
        stbi_image_free(decoded);
        return make_failure(
            render_image_decode_status::invalid_data,
            request,
            "stb_image decoder returned invalid image dimensions");
    }

    const std::size_t pixel_byte_count =
        static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height)
        * static_cast<std::size_t>(requested_component_count);
    std::vector<std::byte> pixels;
    pixels.reserve(pixel_byte_count);
    for (std::size_t index = 0; index < pixel_byte_count; ++index) {
        pixels.push_back(std::byte{decoded[index]});
    }
    stbi_image_free(decoded);

    render_decoded_image image{
        .width = static_cast<std::size_t>(width),
        .height = static_cast<std::size_t>(height),
        .pixel_format = render_image_pixel_format::rgba8_srgb,
        .pixels = std::move(pixels),
    };
    render_image_decode_metadata metadata =
        make_render_image_decode_metadata(decoder_id_, request, image);
    if (!metadata.size_validation.valid) {
        return make_failure(
            render_image_decode_status::invalid_data,
            request,
            "stb_image decoder produced invalid image payload: "
                + metadata.size_validation.diagnostic);
    }

    return render_image_decode_result{
        .status = render_image_decode_status::decoded,
        .image = std::move(image),
        .diagnostic = "stb_image decoder decoded image bytes",
        .metadata = std::move(metadata),
    };
#else
    return make_failure(
        render_image_decode_status::unsupported_format,
        request,
        "stb_image headers are unavailable; falling back to standard image decoders");
#endif
}

render_image_decode_result stb_image_decoder_backend::make_failure(
    render_image_decode_status status,
    const render_image_decode_request& request,
    std::string diagnostic) const
{
    return render_image_decode_result{
        .status = status,
        .image = {},
        .diagnostic = std::move(diagnostic),
        .metadata = make_render_image_decode_metadata(decoder_id_, request),
    };
}

const third_party_image_decoder_backend_interface* default_stb_image_decoder_backend()
{
#if defined(QUIZ_VULKAN_HAS_STB_HEADERS) && QUIZ_VULKAN_HAS_STB_HEADERS
    static const stb_image_decoder_backend backend{"stb_image_decoder"};
    return &backend;
#else
    return nullptr;
#endif
}

third_party_image_decoder_adapter::third_party_image_decoder_adapter()
    : backend_(default_stb_image_decoder_backend())
{
}

} // namespace quiz_vulkan::render
