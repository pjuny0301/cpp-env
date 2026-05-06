#pragma once

#include "render/image/image_decoder.h"
#include "render/image/image_texture_diagnostics.h"
#include "render/image/image_types.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace quiz_vulkan::render {

enum class fake_image_texture_placeholder_reason {
    none,
    resolve_failed,
    source_load_failed,
    decode_failed,
    upload_failed,
};

inline std::string fake_image_texture_placeholder_reason_name(
    fake_image_texture_placeholder_reason reason)
{
    switch (reason) {
    case fake_image_texture_placeholder_reason::none:
        return "none";
    case fake_image_texture_placeholder_reason::resolve_failed:
        return "resolve_failed";
    case fake_image_texture_placeholder_reason::source_load_failed:
        return "source_load_failed";
    case fake_image_texture_placeholder_reason::decode_failed:
        return "decode_failed";
    case fake_image_texture_placeholder_reason::upload_failed:
        return "upload_failed";
    }

    return "unknown";
}

struct fake_image_texture_placeholder_policy {
    bool enabled = false;
    std::size_t width = 2;
    std::size_t height = 2;
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    std::string source_key_prefix = "placeholder://image/";
};

inline std::string fake_image_texture_placeholder_source_fragment(
    std::string_view source_key)
{
    if (source_key.empty()) {
        return "empty";
    }

    std::string fragment;
    fragment.reserve(source_key.size());
    for (const char value : source_key) {
        fragment.push_back(
            std::iscntrl(static_cast<unsigned char>(value)) != 0 ? '_' : value);
    }
    return fragment;
}

inline render_image_cache_key make_fake_image_texture_placeholder_source_key(
    const fake_image_texture_placeholder_policy& policy,
    fake_image_texture_placeholder_reason reason,
    const render_image_cache_key& source_key)
{
    return policy.source_key_prefix + fake_image_texture_placeholder_reason_name(reason)
        + "/" + fake_image_texture_placeholder_source_fragment(source_key);
}

inline render_image_texture_key make_fake_image_texture_placeholder_key(
    const fake_image_texture_placeholder_policy& policy,
    fake_image_texture_placeholder_reason reason,
    const render_image_texture_key& requested_key)
{
    return render_image_texture_key{
        .source_key = make_fake_image_texture_placeholder_source_key(
            policy,
            reason,
            requested_key.source_key),
        .sampler = requested_key.sampler,
    };
}

inline bool is_fake_image_texture_placeholder_key(const render_image_texture_key& key)
{
    return key.source_key.starts_with("placeholder://image/");
}

inline render_decoded_image make_fake_image_texture_placeholder_decoded_image(
    const fake_image_texture_placeholder_policy& policy)
{
    render_decoded_image image{
        .width = policy.width,
        .height = policy.height,
        .pixel_format = policy.pixel_format,
        .pixels = {},
    };
    const std::size_t byte_count = expected_render_decoded_image_byte_count(image);
    if (byte_count == 0) {
        return {};
    }

    image.pixels.resize(byte_count);
    for (std::size_t pixel = 0; pixel < policy.width * policy.height; ++pixel) {
        const std::size_t offset = pixel * 4;
        const bool bright = pixel % 2 == 0;
        image.pixels[offset] = bright ? std::byte{0xff} : std::byte{0x40};
        image.pixels[offset + 1] = std::byte{0x00};
        image.pixels[offset + 2] = bright ? std::byte{0xff} : std::byte{0x40};
        image.pixels[offset + 3] = std::byte{0xff};
    }
    return image;
}

inline std::string make_fake_image_texture_placeholder_diagnostic(
    fake_image_texture_placeholder_reason reason,
    const std::string& cause)
{
    std::string diagnostic = "using deterministic placeholder texture for "
        + fake_image_texture_placeholder_reason_name(reason);
    if (!cause.empty()) {
        diagnostic += ": " + cause;
    }
    return diagnostic;
}

inline render_image_decode_metadata make_fake_image_texture_placeholder_decode_metadata(
    const render_image_texture_key& placeholder_key,
    fake_image_texture_placeholder_reason reason,
    const render_decoded_image& image)
{
    render_image_decode_request request{
        .source = render_resolved_image_source{
            .original_uri = placeholder_key.source_key,
            .normalized_uri = placeholder_key.source_key,
            .kind = render_image_source_kind::asset_uri,
        },
        .encoded_bytes = {},
    };
    render_image_decode_metadata metadata = make_render_image_decode_metadata(
        "fake_image_texture_placeholder_policy",
        request,
        image);
    metadata.format_detection.placeholder_fallback = true;
    metadata.format_detection.diagnostic = "placeholder texture policy synthesized "
        + fake_image_texture_placeholder_reason_name(reason)
        + " pixels";
    return metadata;
}

struct fake_image_texture_placeholder_snapshot {
    std::size_t sequence = 0;
    fake_image_texture_placeholder_reason reason = fake_image_texture_placeholder_reason::none;
    render_image_texture_key requested_key;
    render_image_texture_key placeholder_key;
    render_image_sampler_policy sampler;
    render_image_texture_handle texture;
    std::uint64_t upload_generation_id = 0;
    bool cache_hit = false;
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::string diagnostic;
};

} // namespace quiz_vulkan::render
