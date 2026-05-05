#include "render/image/image_decoder.h"
#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_cache.h"
#include "render/image/image_texture_pipeline.h"
#include "render/render_draw_list.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

void append_ascii(std::vector<std::byte>& bytes, std::string_view text)
{
    for (const char value : text) {
        bytes.push_back(std::byte{static_cast<unsigned char>(value)});
    }
}

void append_byte(std::vector<std::byte>& bytes, unsigned char value)
{
    bytes.push_back(std::byte{value});
}

std::vector<std::byte> make_ppm_2x1_encoded_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n# deterministic image test fixture\n2 1\n255\n");
    append_byte(bytes, 0xff);
    append_byte(bytes, 0x00);
    append_byte(bytes, 0x00);
    append_byte(bytes, 0x00);
    append_byte(bytes, 0xff);
    append_byte(bytes, 0x00);
    return bytes;
}

std::vector<std::byte> make_short_ppm_2x1_encoded_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n2 1\n255\n");
    append_byte(bytes, 0xff);
    append_byte(bytes, 0x00);
    append_byte(bytes, 0x00);
    return bytes;
}

std::vector<std::byte> make_ppm_1x1_encoded_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n1 1\n255\n");
    append_byte(bytes, 0x00);
    append_byte(bytes, 0x00);
    append_byte(bytes, 0xff);
    return bytes;
}

std::vector<std::byte> make_png_signature_fixture_bytes()
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 0x89);
    append_ascii(bytes, "PNG\r\n");
    append_byte(bytes, 0x1a);
    append_byte(bytes, '\n');
    append_ascii(bytes, "fake-png-payload");
    return bytes;
}

std::vector<std::byte> make_jpeg_signature_fixture_bytes()
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 0xff);
    append_byte(bytes, 0xd8);
    append_byte(bytes, 0xff);
    append_ascii(bytes, "fake-jpeg-payload");
    return bytes;
}

std::vector<std::byte> make_unknown_image_fixture_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "not-a-known-image-format");
    return bytes;
}

quiz_vulkan::render::render_decoded_image make_rgba_2x1_decoded_image()
{
    return quiz_vulkan::render::render_decoded_image{
        .width = 2,
        .height = 1,
        .pixel_format = quiz_vulkan::render::render_image_pixel_format::rgba8_srgb,
        .pixels = {
            std::byte{0xff},
            std::byte{0x00},
            std::byte{0x00},
            std::byte{0xff},
            std::byte{0x00},
            std::byte{0xff},
            std::byte{0x00},
            std::byte{0xff},
        },
    };
}

class malformed_payload_decoder final : public quiz_vulkan::render::image_decoder_interface {
public:
    bool supports(const quiz_vulkan::render::render_image_decode_request&) const override
    {
        ++support_request_count;
        return true;
    }

    quiz_vulkan::render::render_image_decode_result decode(
        const quiz_vulkan::render::render_image_decode_request&) const override
    {
        ++decode_request_count;
        return quiz_vulkan::render::render_image_decode_result{
            .status = quiz_vulkan::render::render_image_decode_status::decoded,
            .image = quiz_vulkan::render::render_decoded_image{
                .width = 2,
                .height = 2,
                .pixel_format = quiz_vulkan::render::render_image_pixel_format::rgba8_srgb,
                .pixels = {
                    std::byte{0xff},
                    std::byte{0x00},
                    std::byte{0x00},
                    std::byte{0xff},
                },
            },
            .diagnostic = {},
        };
    }

    mutable int support_request_count = 0;
    mutable int decode_request_count = 0;
};

class unknown_pixel_format_decoder final : public quiz_vulkan::render::image_decoder_interface {
public:
    bool supports(const quiz_vulkan::render::render_image_decode_request&) const override
    {
        ++support_request_count;
        return true;
    }

    quiz_vulkan::render::render_image_decode_result decode(
        const quiz_vulkan::render::render_image_decode_request&) const override
    {
        ++decode_request_count;
        return quiz_vulkan::render::render_image_decode_result{
            .status = quiz_vulkan::render::render_image_decode_status::decoded,
            .image = quiz_vulkan::render::render_decoded_image{
                .width = 1,
                .height = 1,
                .pixel_format = static_cast<quiz_vulkan::render::render_image_pixel_format>(99),
                .pixels = {
                    std::byte{0xff},
                    std::byte{0x00},
                    std::byte{0x00},
                    std::byte{0xff},
                },
            },
            .diagnostic = {},
        };
    }

    mutable int support_request_count = 0;
    mutable int decode_request_count = 0;
};

class linear_pixel_format_decoder final : public quiz_vulkan::render::image_decoder_interface {
public:
    bool supports(const quiz_vulkan::render::render_image_decode_request&) const override
    {
        ++support_request_count;
        return true;
    }

    quiz_vulkan::render::render_image_decode_result decode(
        const quiz_vulkan::render::render_image_decode_request& request) const override
    {
        ++decode_request_count;
        quiz_vulkan::render::render_decoded_image image{
            .width = 1,
            .height = 1,
            .pixel_format = quiz_vulkan::render::render_image_pixel_format::rgba8_unorm,
            .pixels = {
                std::byte{0x10},
                std::byte{0x20},
                std::byte{0x30},
                std::byte{0xff},
            },
        };
        return quiz_vulkan::render::render_image_decode_result{
            .status = quiz_vulkan::render::render_image_decode_status::decoded,
            .image = image,
            .diagnostic = {},
            .metadata = quiz_vulkan::render::make_render_image_decode_metadata(
                "linear_pixel_format_decoder",
                request,
                image),
        };
    }

    mutable int support_request_count = 0;
    mutable int decode_request_count = 0;
};

class source_upload_failure_then_placeholder_uploader final
    : public quiz_vulkan::render::image_texture_uploader_interface {
public:
    quiz_vulkan::render::render_image_texture_upload_result upload(
        const quiz_vulkan::render::render_image_texture_upload_request& request) override
    {
        ++upload_request_count;
        if (quiz_vulkan::render::is_fake_image_texture_placeholder_key(request.key)) {
            return placeholder_uploader.upload(request);
        }

        ++rejected_source_upload_count;
        return quiz_vulkan::render::render_image_texture_upload_result{
            .status = quiz_vulkan::render::render_image_texture_upload_status::invalid_image,
            .generation_id = 100 + static_cast<std::uint64_t>(rejected_source_upload_count),
            .key = request.key,
            .sampler = request.sampler,
            .texture = {},
            .pixel_count = quiz_vulkan::render::render_image_checked_pixel_count(request.image),
            .pixel_byte_count = quiz_vulkan::render::expected_render_decoded_image_byte_count(request.image),
            .decoded_byte_count = request.image.pixels.size(),
            .staging_byte_count = 0,
            .diagnostic = "forced source texture upload failure",
        };
    }

    int upload_request_count = 0;
    int rejected_source_upload_count = 0;
    quiz_vulkan::render::fake_image_texture_uploader placeholder_uploader;
};

const quiz_vulkan::render::fake_image_texture_cache_entry_snapshot* find_snapshot_entry(
    const quiz_vulkan::render::fake_image_texture_cache_snapshot& snapshot,
    std::string_view source_key)
{
    for (const quiz_vulkan::render::fake_image_texture_cache_entry_snapshot& entry : snapshot.entries) {
        if (entry.key.source_key == source_key) {
            return &entry;
        }
    }

    return nullptr;
}

void test_sampler_defaults_are_appended_to_render_image_ref()
{
    using namespace quiz_vulkan::render;

    render_image_ref image;
    image.uri = "assets/card.fake";
    image.alt_text = "card";
    image.aspect_ratio = 1.6f;
    require(image.sampler.min_filter == render_image_filter::linear, "default min filter is linear");
    require(image.sampler.mag_filter == render_image_filter::linear, "default mag filter is linear");
    require(image.sampler.mipmap_mode == render_image_mipmap_mode::none, "default mipmap mode is none");
    require(image.sampler.wrap_u == render_image_wrap_mode::clamp_to_edge, "default wrap_u clamps");
    require(image.sampler.wrap_v == render_image_wrap_mode::clamp_to_edge, "default wrap_v clamps");
    require(is_valid_render_image_sampler_policy(image.sampler), "default sampler policy is valid");
}

void test_resolver_normalizes_without_fetching()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_image_resolve_result local = resolver.resolve(
        render_image_resolve_request{.uri = "  ./textures\\cards\\front.fake  "});
    require(local.ok(), "local path resolves");
    require(local.source.original_uri == "  ./textures\\cards\\front.fake  ", "resolver preserves original uri");
    require(local.source.normalized_uri == "textures/cards/front.fake", "resolver trims and normalizes local path");
    require(local.source.kind == render_image_source_kind::local_path, "resolver classifies local paths");
    require(!local.source.is_remote(), "local path is not remote");

    const render_image_resolve_result windows_local = resolver.resolve(
        render_image_resolve_request{.uri = "C:\\quiz\\cards\\front.fake"});
    require(windows_local.ok(), "windows local path resolves");
    require(windows_local.source.normalized_uri == "C:/quiz/cards/front.fake", "resolver preserves drive paths");
    require(windows_local.source.kind == render_image_source_kind::local_path, "resolver classifies drive paths");

    const render_image_resolve_result asset = resolver.resolve(
        render_image_resolve_request{.uri = "  ASSET://./Deck\\cards\\front.fake  "});
    require(asset.ok(), "asset uri resolves");
    require(asset.source.normalized_uri == "asset://Deck/cards/front.fake", "resolver canonicalizes asset uri");
    require(asset.source.kind == render_image_source_kind::asset_uri, "resolver classifies asset uris");
    require(!asset.source.is_remote(), "asset uri is not remote");

    const render_image_resolve_result remote = resolver.resolve(
        render_image_resolve_request{.uri = "HTTPS://example.test/image.fake"});
    require(remote.ok(), "remote uri resolves as a source contract");
    require(remote.source.normalized_uri == "https://example.test/image.fake", "resolver lowercases scheme");
    require(remote.source.kind == render_image_source_kind::https_uri, "resolver classifies https uris");
    require(remote.source.is_remote(), "https uri is remote");

    const render_image_resolve_result empty = resolver.resolve(render_image_resolve_request{.uri = "  "});
    require(!empty.ok(), "empty uri does not resolve");
    require(empty.status == render_image_resolve_status::empty_uri, "empty uri reports empty status");

    const render_image_resolve_result unsupported = resolver.resolve(
        render_image_resolve_request{.uri = "ftp://example.test/image.fake"});
    require(!unsupported.ok(), "unsupported uri scheme does not resolve");
    require(
        unsupported.status == render_image_resolve_status::unsupported_scheme,
        "unsupported uri reports unsupported scheme");
    require(!unsupported.diagnostic.empty(), "unsupported uri includes diagnostic");
}

void test_decoder_interface_shape()
{
    using namespace quiz_vulkan::render;

    fake_image_decoder decoder;
    const render_image_decode_request request{
        .source = render_resolved_image_source{
            .original_uri = "asset://card.fake",
            .normalized_uri = "asset://card.fake",
            .kind = render_image_source_kind::asset_uri,
        },
        .encoded_bytes = {std::byte{0x01}, std::byte{0x02}},
    };

    require(decoder.supports(request), "fake decoder support check uses request shape");
    const render_image_decode_result decoded = decoder.decode(request);
    require(decoded.ok(), "fake decoder returns decoded result");
    require(decoded.image.width == 2, "decoded image carries width");
    require(decoded.image.height == 1, "decoded image carries height");
    require(decoded.image.pixels.size() == 8, "decoded image carries rgba bytes");
    require(expected_render_decoded_image_byte_count(decoded.image) == 8, "decoded image byte count is derived");
    require(has_valid_render_decoded_image_payload(decoded.image), "decoded image payload matches dimensions");
    require(decoded.metadata.decoder_id == "fake_image_decoder", "fake decoder reports decoder id");
    require(decoded.metadata.encoded_byte_count == 2, "fake decoder reports encoded byte count");
    require(decoded.metadata.width == 2, "fake decoder reports metadata width");
    require(decoded.metadata.height == 1, "fake decoder reports metadata height");
    require(decoded.metadata.decoded_byte_count == 8, "fake decoder reports decoded byte count");
    require(decoded.metadata.has_image(), "fake decoder metadata reports image");
    require(
        decoded.metadata.format_detection.detected_format == render_image_encoded_format::unknown,
        "fake decoder reports unknown fixture format by default");
    require(decoded.metadata.format_detection.placeholder_fallback, "fake decoder reports placeholder fallback");
    require(decoded.metadata.size_validation.valid, "fake decoder validates decoded size");
    require(decoded.metadata.size_validation.row_stride_byte_count == 8, "fake decoder reports row stride");
    require(decoded.metadata.size_validation.expected_decoded_byte_count == 8, "fake decoder reports expected byte count");
    require(decoded.decoder_diagnostics.empty(), "direct fake decoder has no chain diagnostics");
    require(decoder.support_requests.size() == 1, "support request was recorded");
    require(decoder.decode_requests.size() == 1, "decode request was recorded");
}

void test_fake_decoder_reports_format_detection_and_placeholder_fallbacks()
{
    using namespace quiz_vulkan::render;

    const render_resolved_image_source source{
        .original_uri = "asset://card.fake",
        .normalized_uri = "asset://card.fake",
        .kind = render_image_source_kind::asset_uri,
    };
    fake_image_decoder decoder;

    const render_image_decode_result png = decoder.decode(render_image_decode_request{
        .source = source,
        .encoded_bytes = make_png_signature_fixture_bytes(),
    });
    require(png.ok(), "fake decoder decodes PNG fixture through placeholder fallback");
    require(
        png.metadata.format_detection.detected_format == render_image_encoded_format::png,
        "fake decoder detects PNG signature");
    require(png.metadata.format_detection.extension_hint == "fake", "fake decoder reports extension hint");
    require(png.metadata.format_detection.recognized_signature, "PNG fixture has recognized signature");
    require(png.metadata.format_detection.placeholder_fallback, "PNG fixture reports placeholder fallback");
    require(!png.metadata.format_detection.diagnostic.empty(), "PNG fallback includes diagnostic");
    require(png.metadata.size_validation.valid, "PNG placeholder validates decoded payload");
    require(png.metadata.size_validation.row_stride_byte_count == 8, "PNG placeholder reports row stride");

    const render_image_decode_result jpeg = decoder.decode(render_image_decode_request{
        .source = source,
        .encoded_bytes = make_jpeg_signature_fixture_bytes(),
    });
    require(jpeg.ok(), "fake decoder decodes JPEG fixture through placeholder fallback");
    require(
        jpeg.metadata.format_detection.detected_format == render_image_encoded_format::jpeg,
        "fake decoder detects JPEG signature");
    require(jpeg.metadata.format_detection.recognized_signature, "JPEG fixture has recognized signature");
    require(jpeg.metadata.format_detection.placeholder_fallback, "JPEG fixture reports placeholder fallback");
    require(jpeg.metadata.size_validation.expected_decoded_byte_count == 8, "JPEG placeholder reports byte size");
    require(jpeg.metadata.size_validation.actual_decoded_byte_count == 8, "JPEG placeholder reports actual bytes");

    const render_image_decode_result unknown = decoder.decode(render_image_decode_request{
        .source = source,
        .encoded_bytes = make_unknown_image_fixture_bytes(),
    });
    require(unknown.ok(), "fake decoder decodes unknown fixture through placeholder fallback");
    require(
        unknown.metadata.format_detection.detected_format == render_image_encoded_format::unknown,
        "fake decoder reports unknown format");
    require(!unknown.metadata.format_detection.recognized_signature, "unknown fixture has no recognized signature");
    require(unknown.metadata.format_detection.placeholder_fallback, "unknown fixture reports placeholder fallback");
    require(!unknown.metadata.format_detection.diagnostic.empty(), "unknown fallback includes diagnostic");
    require(unknown.metadata.size_validation.valid, "unknown placeholder validates decoded payload");
    require(decoder.decode_requests.size() == 3, "fake decoder records format diagnostic decode requests");
}

void test_decoder_size_validation_reports_stride_and_invalid_payloads()
{
    using namespace quiz_vulkan::render;

    const render_decoded_image valid = make_rgba_2x1_decoded_image();
    const render_image_decode_size_validation valid_size = validate_render_image_decode_size(valid);
    require(valid_size.valid, "decoded size validation accepts valid RGBA payload");
    require(valid_size.row_stride_byte_count == 8, "decoded size validation reports row stride");
    require(valid_size.expected_decoded_byte_count == 8, "decoded size validation reports expected bytes");
    require(valid_size.actual_decoded_byte_count == 8, "decoded size validation reports actual bytes");

    render_decoded_image short_payload = make_rgba_2x1_decoded_image();
    short_payload.pixels.pop_back();
    const render_image_decode_size_validation invalid_size = validate_render_image_decode_size(short_payload);
    require(!invalid_size.valid, "decoded size validation rejects short payload");
    require(invalid_size.row_stride_byte_count == 8, "invalid payload still reports row stride");
    require(invalid_size.expected_decoded_byte_count == 8, "invalid payload reports expected bytes");
    require(invalid_size.actual_decoded_byte_count == 7, "invalid payload reports actual bytes");
    require(!invalid_size.diagnostic.empty(), "invalid payload includes size diagnostic");

    render_decoded_image unknown_format = make_rgba_2x1_decoded_image();
    unknown_format.pixel_format = static_cast<render_image_pixel_format>(99);
    const render_image_decode_size_validation invalid_format = validate_render_image_decode_size(unknown_format);
    require(!invalid_format.valid, "decoded size validation rejects unknown pixel format");
    require(invalid_format.expected_decoded_byte_count == 0, "unknown pixel format has no expected byte count");
    require(!invalid_format.diagnostic.empty(), "unknown pixel format includes size diagnostic");
}

void test_decoder_reports_explicit_failures()
{
    using namespace quiz_vulkan::render;

    fake_image_decoder decoder;
    const render_image_decode_request unsupported_request{
        .source = render_resolved_image_source{
            .original_uri = "asset://card.png",
            .normalized_uri = "asset://card.png",
            .kind = render_image_source_kind::asset_uri,
        },
        .encoded_bytes = {std::byte{0x01}},
    };
    require(!decoder.supports(unsupported_request), "fake decoder rejects unsupported extension");

    const render_image_decode_result unsupported = decoder.decode(unsupported_request);
    require(!unsupported.ok(), "unsupported decode does not return an image");
    require(
        unsupported.status == render_image_decode_status::unsupported_format,
        "unsupported decode reports unsupported format");
    require(!unsupported.diagnostic.empty(), "unsupported decode includes diagnostic");
    require(unsupported.metadata.decoder_id == "fake_image_decoder", "unsupported fake decode reports decoder id");
    require(unsupported.metadata.encoded_byte_count == 1, "unsupported fake decode reports encoded byte count");
    require(!unsupported.metadata.has_image(), "unsupported fake decode reports no image metadata");

    const render_image_decode_request empty_request{
        .source = render_resolved_image_source{
            .original_uri = "asset://card.fake",
            .normalized_uri = "asset://card.fake",
            .kind = render_image_source_kind::asset_uri,
        },
        .encoded_bytes = {},
    };
    require(decoder.supports(empty_request), "fake decoder support is independent of byte availability");

    const render_image_decode_result empty = decoder.decode(empty_request);
    require(!empty.ok(), "empty decode does not return an image");
    require(empty.status == render_image_decode_status::empty_input, "empty decode reports empty input");
    require(!empty.diagnostic.empty(), "empty decode includes diagnostic");
    require(empty.metadata.decoder_id == "fake_image_decoder", "empty fake decode reports decoder id");
    require(empty.metadata.encoded_byte_count == 0, "empty fake decode reports empty byte count");
    require(!empty.metadata.has_image(), "empty fake decode reports no image metadata");

    require(decoder.support_requests.size() == 2, "failure support requests were recorded");
    require(decoder.decode_requests.size() == 2, "failure decode requests were recorded");
}

void test_ppm_decoder_decodes_binary_rgb_to_rgba()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.ppm"})
                                                    .source;
    const render_image_decode_request request{
        .source = source,
        .encoded_bytes = make_ppm_2x1_encoded_bytes(),
    };

    ppm_image_decoder decoder;
    require(decoder.supports(request), "ppm decoder supports ppm source");
    const render_image_decode_result decoded = decoder.decode(request);
    require(decoded.ok(), "ppm decoder decodes P6 bytes");
    require(decoded.image.width == 2, "ppm decoder preserves width");
    require(decoded.image.height == 1, "ppm decoder preserves height");
    require(decoded.image.pixel_format == render_image_pixel_format::rgba8_srgb, "ppm decoder emits srgb rgba");
    require(decoded.image.pixels.size() == 8, "ppm decoder expands rgb bytes to rgba bytes");
    require(has_valid_render_decoded_image_payload(decoded.image), "ppm decoded payload matches image contract");
    require(decoded.image.pixels[0] == std::byte{0xff}, "ppm decoder copies first pixel red channel");
    require(decoded.image.pixels[1] == std::byte{0x00}, "ppm decoder copies first pixel green channel");
    require(decoded.image.pixels[2] == std::byte{0x00}, "ppm decoder copies first pixel blue channel");
    require(decoded.image.pixels[3] == std::byte{0xff}, "ppm decoder adds opaque alpha");
    require(decoded.image.pixels[4] == std::byte{0x00}, "ppm decoder copies second pixel red channel");
    require(decoded.image.pixels[5] == std::byte{0xff}, "ppm decoder copies second pixel green channel");
    require(decoded.image.pixels[6] == std::byte{0x00}, "ppm decoder copies second pixel blue channel");
    require(decoded.image.pixels[7] == std::byte{0xff}, "ppm decoder adds second opaque alpha");
    require(decoded.metadata.decoder_id == "ppm_image_decoder", "ppm decoder reports decoder id");
    require(decoded.metadata.encoded_byte_count == request.encoded_bytes.size(), "ppm decoder reports encoded byte count");
    require(decoded.metadata.width == 2, "ppm decoder metadata carries width");
    require(decoded.metadata.height == 1, "ppm decoder metadata carries height");
    require(decoded.metadata.decoded_byte_count == 8, "ppm decoder metadata carries decoded byte count");
    require(decoded.metadata.has_image(), "ppm decoder metadata reports image");
    require(
        decoded.metadata.format_detection.detected_format == render_image_encoded_format::ppm,
        "ppm decoder reports detected PPM format");
    require(decoded.metadata.format_detection.recognized_signature, "ppm decoder reports recognized signature");
    require(!decoded.metadata.format_detection.placeholder_fallback, "ppm decoder does not use placeholder fallback");
    require(decoded.metadata.size_validation.valid, "ppm decoder validates decoded size");
    require(decoded.metadata.size_validation.row_stride_byte_count == 8, "ppm decoder reports row stride");
}

void test_ppm_decoder_reports_invalid_payload_size()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "asset://card.ppm"})
                                                    .source;
    const render_image_decode_request request{
        .source = source,
        .encoded_bytes = make_short_ppm_2x1_encoded_bytes(),
    };

    ppm_image_decoder decoder;
    const render_image_decode_result decoded = decoder.decode(request);
    require(!decoded.ok(), "short ppm payload does not decode");
    require(decoded.status == render_image_decode_status::invalid_data, "short ppm payload reports invalid data");
    require(!decoded.diagnostic.empty(), "short ppm payload includes diagnostic");
    require(decoded.metadata.decoder_id == "ppm_image_decoder", "short ppm payload reports decoder id");
    require(decoded.metadata.encoded_byte_count == request.encoded_bytes.size(), "short ppm reports encoded byte count");
    require(!decoded.metadata.has_image(), "short ppm payload reports no decoded image metadata");

    const render_image_decode_request unsupported_request{
        .source = render_resolved_image_source{
            .original_uri = "asset://card.png",
            .normalized_uri = "asset://card.png",
            .kind = render_image_source_kind::asset_uri,
        },
        .encoded_bytes = make_ppm_2x1_encoded_bytes(),
    };
    require(!decoder.supports(unsupported_request), "ppm decoder rejects non-ppm source");
    const render_image_decode_result unsupported = decoder.decode(unsupported_request);
    require(!unsupported.ok(), "non-ppm source does not decode through ppm decoder");
    require(
        unsupported.status == render_image_decode_status::unsupported_format,
        "non-ppm source reports unsupported format");
    require(unsupported.metadata.decoder_id == "ppm_image_decoder", "unsupported ppm reports decoder id");
    require(unsupported.metadata.encoded_byte_count == unsupported_request.encoded_bytes.size(), "unsupported ppm reports encoded byte count");
}

void test_decoder_chain_routes_supported_formats()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source fake_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.fake"})
                                                     .source;
    const render_resolved_image_source ppm_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.ppm"})
                                                    .source;

    fake_image_decoder fake_decoder;
    ppm_image_decoder ppm_decoder;
    image_decoder_chain decoder_chain({std::cref(fake_decoder), std::cref(ppm_decoder)});
    require(decoder_chain.decoder_count() == 2, "decoder chain stores both decoders");

    const render_image_decode_request fake_request{
        .source = fake_source,
        .encoded_bytes = {std::byte{0x01}},
    };
    require(decoder_chain.supports(fake_request), "decoder chain supports fake decoder source");
    const render_image_decode_result fake_decoded = decoder_chain.decode(fake_request);
    require(fake_decoded.ok(), "decoder chain routes fake source to fake decoder");
    require(fake_decoded.image.width == 2, "decoder chain returns fake decoded width");
    require(fake_decoded.metadata.decoder_id == "fake_image_decoder", "decoder chain preserves fake decoder id");
    require(fake_decoded.decoder_diagnostics.size() == 1, "decoder chain reports selected fake diagnostic");
    require(fake_decoded.decoder_diagnostics[0].decoder_id == "decoder[0]", "decoder chain names first default decoder");
    require(fake_decoded.decoder_diagnostics[0].candidate_index == 0, "fake candidate records index");
    require(fake_decoded.decoder_diagnostics[0].candidate_order == 1, "fake candidate records stable order");
    require(fake_decoded.decoder_diagnostics[0].extension_hint == "fake", "fake candidate records extension hint");
    require(
        fake_decoded.decoder_diagnostics[0].detected_format == render_image_encoded_format::unknown,
        "fake candidate records unknown detected format");
    require(
        fake_decoded.decoder_diagnostics[0].detected_format_name == "unknown",
        "fake candidate records detected format name");
    require(
        fake_decoded.decoder_diagnostics[0].detected_mime_type == "application/octet-stream",
        "fake candidate records fallback mime type");
    require(
        fake_decoded.decoder_diagnostics[0].source_kind == render_image_source_kind::local_path,
        "fake candidate records source kind");
    require(fake_decoded.decoder_diagnostics[0].source_kind_name == "local_path", "fake candidate records source kind name");
    require(fake_decoded.decoder_diagnostics[0].support_checked, "fake candidate records support check");
    require(fake_decoded.decoder_diagnostics[0].supported, "decoder chain marks selected fake decoder supported");
    require(fake_decoded.decoder_diagnostics[0].decode_attempted, "fake candidate records decode attempt");
    require(fake_decoded.decoder_diagnostics[0].terminal_candidate, "fake candidate terminates chain");
    require(
        fake_decoded.decoder_diagnostics[0].status == render_image_decode_status::decoded,
        "decoder chain records fake decode status");
    require(!fake_decoded.decoder_diagnostics[0].support_diagnostic.empty(), "fake candidate records support summary");
    require(!fake_decoded.decoder_diagnostics[0].decode_diagnostic.empty(), "fake candidate records decode summary");

    const render_image_decode_request ppm_request{
        .source = ppm_source,
        .encoded_bytes = make_ppm_1x1_encoded_bytes(),
    };
    require(decoder_chain.supports(ppm_request), "decoder chain supports ppm decoder source");
    const render_image_decode_result ppm_decoded = decoder_chain.decode(ppm_request);
    require(ppm_decoded.ok(), "decoder chain routes ppm source to ppm decoder");
    require(ppm_decoded.image.width == 1, "decoder chain returns ppm decoded width");
    require(ppm_decoded.metadata.decoder_id == "ppm_image_decoder", "decoder chain preserves ppm decoder id");
    require(ppm_decoded.decoder_diagnostics.size() == 2, "decoder chain reports skipped and selected decoders");
    require(!ppm_decoded.decoder_diagnostics[0].supported, "decoder chain marks skipped fake decoder unsupported");
    require(!ppm_decoded.decoder_diagnostics[0].decode_attempted, "skipped fake decoder records no decode attempt");
    require(!ppm_decoded.decoder_diagnostics[0].terminal_candidate, "skipped fake decoder does not terminate chain");
    require(ppm_decoded.decoder_diagnostics[0].extension_hint == "ppm", "skipped fake decoder records ppm extension");
    require(
        ppm_decoded.decoder_diagnostics[0].detected_format == render_image_encoded_format::ppm,
        "skipped fake decoder records detected ppm format");
    require(
        ppm_decoded.decoder_diagnostics[0].detected_mime_type == "image/x-portable-pixmap",
        "skipped fake decoder records detected ppm mime");
    require(ppm_decoded.decoder_diagnostics[0].recognized_signature, "skipped fake decoder records recognized signature");
    require(ppm_decoded.decoder_diagnostics[0].candidate_priority == 0, "first ppm candidate records priority");
    require(ppm_decoded.decoder_diagnostics[1].supported, "decoder chain marks selected ppm decoder supported");
    require(ppm_decoded.decoder_diagnostics[1].decoder_id == "decoder[1]", "decoder chain names second default decoder");
    require(ppm_decoded.decoder_diagnostics[1].candidate_index == 1, "selected ppm decoder records index");
    require(ppm_decoded.decoder_diagnostics[1].candidate_order == 2, "selected ppm decoder records order");
    require(ppm_decoded.decoder_diagnostics[1].candidate_priority == 1, "second ppm candidate records priority");
    require(ppm_decoded.decoder_diagnostics[1].decode_attempted, "selected ppm decoder records decode attempt");
    require(ppm_decoded.decoder_diagnostics[1].terminal_candidate, "selected ppm decoder terminates chain");
    require(fake_decoder.decode_requests.size() == 1, "decoder chain decodes fake source only once");
}

void test_decoder_chain_reports_unsupported_sources()
{
    using namespace quiz_vulkan::render;

    ppm_image_decoder ppm_decoder;
    image_decoder_chain decoder_chain;
    decoder_chain.add_decoder("ppm-fixture", ppm_decoder);
    const render_image_decode_request request{
        .source = render_resolved_image_source{
            .original_uri = "asset://card.png",
            .normalized_uri = "asset://card.png",
            .kind = render_image_source_kind::asset_uri,
        },
        .encoded_bytes = make_ppm_1x1_encoded_bytes(),
    };

    require(!decoder_chain.supports(request), "decoder chain rejects unsupported source");
    const render_image_decode_result decoded = decoder_chain.decode(request);
    require(!decoded.ok(), "decoder chain unsupported source does not decode");
    require(
        decoded.status == render_image_decode_status::unsupported_format,
        "decoder chain unsupported source reports unsupported format");
    require(!decoded.diagnostic.empty(), "decoder chain unsupported source includes diagnostic");
    require(decoded.metadata.decoder_id == "image_decoder_chain", "decoder chain fallback reports decoder id");
    require(decoded.metadata.encoded_byte_count == request.encoded_bytes.size(), "decoder chain failure reports encoded byte count");
    require(decoded.decoder_diagnostics.size() == 1, "decoder chain failure reports checked decoder");
    require(decoded.decoder_diagnostics[0].decoder_id == "ppm-fixture", "decoder chain failure uses explicit decoder id");
    require(decoded.decoder_diagnostics[0].candidate_index == 0, "unsupported candidate records index");
    require(decoded.decoder_diagnostics[0].candidate_order == 1, "unsupported candidate records order");
    require(decoded.decoder_diagnostics[0].extension_hint == "png", "unsupported candidate records extension");
    require(
        decoded.decoder_diagnostics[0].detected_format == render_image_encoded_format::ppm,
        "unsupported candidate records detected bytes separately from extension");
    require(
        decoded.decoder_diagnostics[0].detected_mime_type == "image/x-portable-pixmap",
        "unsupported candidate records detected mime");
    require(
        decoded.decoder_diagnostics[0].source_kind == render_image_source_kind::asset_uri,
        "unsupported candidate records asset source kind");
    require(decoded.decoder_diagnostics[0].source_kind_name == "asset_uri", "unsupported candidate records source kind name");
    require(decoded.decoder_diagnostics[0].support_checked, "unsupported candidate records support check");
    require(!decoded.decoder_diagnostics[0].supported, "decoder chain failure marks decoder unsupported");
    require(!decoded.decoder_diagnostics[0].decode_attempted, "unsupported candidate records no decode attempt");
    require(decoded.decoder_diagnostics[0].terminal_candidate, "unsupported candidate terminates fallback chain");
    require(
        decoded.decoder_diagnostics[0].status == render_image_decode_status::unsupported_format,
        "decoder chain failure records unsupported status");
    require(!decoded.decoder_diagnostics[0].support_diagnostic.empty(), "unsupported candidate records support summary");
    require(!decoded.decoder_diagnostics[0].diagnostic.empty(), "decoder chain failure records support diagnostic");
}

void test_decoder_chain_reports_candidate_decode_failures()
{
    using namespace quiz_vulkan::render;

    ppm_image_decoder ppm_decoder;
    image_decoder_chain decoder_chain;
    decoder_chain.add_decoder("ppm-fixture", ppm_decoder);
    const render_image_decode_request request{
        .source = render_resolved_image_source{
            .original_uri = "asset://broken.ppm",
            .normalized_uri = "asset://broken.ppm",
            .kind = render_image_source_kind::asset_uri,
        },
        .encoded_bytes = make_png_signature_fixture_bytes(),
    };

    require(decoder_chain.supports(request), "decoder chain supports ppm extension before decode failure");
    const render_image_decode_result decoded = decoder_chain.decode(request);
    require(!decoded.ok(), "decoder chain returns deterministic decode failure result");
    require(
        decoded.status == render_image_decode_status::unsupported_format,
        "decoder chain preserves candidate decode failure status");
    require(decoded.decoder_diagnostics.size() == 1, "decode failure records one candidate");

    const render_image_decoder_diagnostic& candidate = decoded.decoder_diagnostics[0];
    require(candidate.decoder_id == "ppm-fixture", "decode failure records candidate id");
    require(candidate.candidate_index == 0, "decode failure records candidate index");
    require(candidate.candidate_order == 1, "decode failure records candidate order");
    require(candidate.candidate_priority == 100, "decode failure combines extension and mime ordering");
    require(candidate.extension_hint == "ppm", "decode failure records extension hint");
    require(candidate.detected_format == render_image_encoded_format::png, "decode failure records detected PNG bytes");
    require(candidate.detected_format_name == "png", "decode failure records detected format name");
    require(candidate.detected_mime_type == "image/png", "decode failure records detected mime");
    require(candidate.source_kind == render_image_source_kind::asset_uri, "decode failure records source kind");
    require(candidate.source_kind_name == "asset_uri", "decode failure records source kind name");
    require(candidate.recognized_signature, "decode failure records recognized signature");
    require(candidate.support_checked, "decode failure records support check");
    require(candidate.supported, "decode failure records supported candidate");
    require(candidate.decode_attempted, "decode failure records decode attempt");
    require(candidate.terminal_candidate, "decode failure marks terminal candidate");
    require(!candidate.support_diagnostic.empty(), "decode failure records support summary");
    require(!candidate.decode_diagnostic.empty(), "decode failure records decode summary");
    require(!candidate.diagnostic.empty(), "decode failure records candidate summary");
}

void test_source_bytes_loader_returns_registered_local_bytes()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/loader.ppm"})
                                                .source;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes(source, make_ppm_1x1_encoded_bytes());

    const render_image_source_bytes_load_result loaded = loader.load(
        render_image_source_bytes_load_request{.source = source});
    require(loaded.ok(), "source bytes loader loads registered local bytes");
    require(loaded.status == render_image_source_bytes_load_status::loaded, "source bytes loader reports loaded status");
    require(loaded.source.cache_key() == source.cache_key(), "source bytes loader preserves source");
    require(loaded.encoded_bytes == make_ppm_1x1_encoded_bytes(), "source bytes loader returns deterministic bytes");
    require(loader.has_source_bytes(source), "source bytes loader reports registered source bytes");
    require(loader.load_requests.size() == 1, "source bytes loader records load request");
}

void test_source_bytes_loader_reports_explicit_failures()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source missing_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/missing.ppm"})
                                                        .source;
    const render_resolved_image_source empty_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/empty.ppm"})
                                                      .source;
    const render_resolved_image_source remote_source = resolver.resolve(
        render_image_resolve_request{.uri = "https://example.test/image.ppm"})
                                                       .source;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes(empty_source, {});

    const render_image_source_bytes_load_result missing = loader.load(
        render_image_source_bytes_load_request{.source = missing_source});
    require(!missing.ok(), "missing source bytes do not load");
    require(
        missing.status == render_image_source_bytes_load_status::missing_bytes,
        "missing source bytes report missing bytes");
    require(!missing.diagnostic.empty(), "missing source bytes include diagnostic");

    const render_image_source_bytes_load_result empty = loader.load(
        render_image_source_bytes_load_request{.source = empty_source});
    require(!empty.ok(), "empty source bytes do not load");
    require(empty.status == render_image_source_bytes_load_status::empty_bytes, "empty source bytes report empty bytes");
    require(!empty.diagnostic.empty(), "empty source bytes include diagnostic");

    const render_image_source_bytes_load_result remote = loader.load(
        render_image_source_bytes_load_request{.source = remote_source});
    require(!remote.ok(), "remote source bytes do not load through fake loader");
    require(
        remote.status == render_image_source_bytes_load_status::unsupported_source,
        "remote source bytes report unsupported source");
    require(!remote.diagnostic.empty(), "remote source bytes include diagnostic");

    const render_resolved_image_source invalid_source{
        .original_uri = "bad\n.ppm",
        .normalized_uri = std::string("bad\n.ppm"),
        .kind = render_image_source_kind::local_path,
    };
    const render_image_source_bytes_load_result invalid = loader.load(
        render_image_source_bytes_load_request{.source = invalid_source});
    require(!invalid.ok(), "invalid source key does not load");
    require(
        invalid.status == render_image_source_bytes_load_status::missing_source,
        "invalid source key reports missing source");
    require(!invalid.diagnostic.empty(), "invalid source key includes diagnostic");
    require(loader.load_requests.size() == 4, "source bytes loader records failure requests");
}

void test_source_bytes_loader_loads_data_uri_bytes()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;

    const render_image_resolve_result base64_resolved = resolver.resolve(
        render_image_resolve_request{.uri = "DATA:image/png;base64,iVBORw0KGgo="});
    require(base64_resolved.ok(), "data URI source resolves");
    require(base64_resolved.source.kind == render_image_source_kind::data_uri, "data URI source kind is preserved");
    require(base64_resolved.source.normalized_uri.starts_with("data:"), "data URI scheme is normalized");
    require(
        base64_resolved.source.cache_key() == base64_resolved.source.normalized_uri,
        "data URI cache key stays source normalized URI");
    require(!loader.has_source_bytes(base64_resolved.source), "data URI bytes do not require registration");

    const render_image_data_uri_decode_result base64_decoded = decode_render_image_data_uri(
        base64_resolved.source.cache_key());
    require(base64_decoded.ok(), "data URI decoder accepts base64 payload");
    require(base64_decoded.status == render_image_data_uri_decode_status::decoded, "base64 data URI reports decoded");
    require(base64_decoded.media_type == "image/png", "base64 data URI preserves media type");
    require(base64_decoded.base64_encoded, "base64 data URI records base64 metadata");
    require(render_image_data_uri_decode_status_name(base64_decoded.status) == "decoded", "data URI status is named");

    const render_image_source_bytes_load_result base64_loaded = loader.load(
        render_image_source_bytes_load_request{.source = base64_resolved.source});
    require(base64_loaded.ok(), "source bytes loader loads base64 data URI bytes");
    require(
        base64_loaded.status == render_image_source_bytes_load_status::loaded,
        "base64 data URI reports loaded source bytes");
    require(
        base64_loaded.source.cache_key() == base64_resolved.source.cache_key(),
        "base64 data URI load preserves cache key");
    require(base64_loaded.encoded_bytes.size() == 8, "base64 data URI decodes PNG signature byte count");
    require(base64_loaded.encoded_bytes[0] == std::byte{0x89}, "base64 data URI decodes PNG signature byte 0");
    require(base64_loaded.encoded_bytes[1] == std::byte{0x50}, "base64 data URI decodes PNG signature byte 1");
    require(base64_loaded.encoded_bytes[2] == std::byte{0x4e}, "base64 data URI decodes PNG signature byte 2");
    require(base64_loaded.encoded_bytes[3] == std::byte{0x47}, "base64 data URI decodes PNG signature byte 3");
    require(base64_loaded.encoded_bytes[4] == std::byte{0x0d}, "base64 data URI decodes PNG signature byte 4");
    require(base64_loaded.encoded_bytes[5] == std::byte{0x0a}, "base64 data URI decodes PNG signature byte 5");
    require(base64_loaded.encoded_bytes[6] == std::byte{0x1a}, "base64 data URI decodes PNG signature byte 6");
    require(base64_loaded.encoded_bytes[7] == std::byte{0x0a}, "base64 data URI decodes PNG signature byte 7");

    const render_image_resolve_result percent_resolved = resolver.resolve(
        render_image_resolve_request{.uri = "data:image/x-portable-pixmap,P6%0A1%201%0A255%0A%00%00%FF"});
    require(percent_resolved.ok(), "percent encoded data URI source resolves");
    const render_image_data_uri_decode_result percent_decoded = decode_render_image_data_uri(
        percent_resolved.source.cache_key());
    require(percent_decoded.ok(), "data URI decoder accepts percent encoded payload");
    require(!percent_decoded.base64_encoded, "percent data URI records non-base64 payload");
    require(
        percent_decoded.media_type == "image/x-portable-pixmap",
        "percent data URI preserves media type");

    const render_image_source_bytes_load_result percent_loaded = loader.load(
        render_image_source_bytes_load_request{.source = percent_resolved.source});
    require(percent_loaded.ok(), "source bytes loader loads percent encoded data URI bytes");
    require(
        percent_loaded.encoded_bytes == make_ppm_1x1_encoded_bytes(),
        "percent encoded data URI decodes deterministic PPM bytes");

    const render_image_resolve_result raw_resolved = resolver.resolve(
        render_image_resolve_request{.uri = "data:image/test,raw-image-bytes"});
    require(raw_resolved.ok(), "raw data URI source resolves");
    std::vector<std::byte> expected_raw;
    append_ascii(expected_raw, "raw-image-bytes");
    const render_image_source_bytes_load_result raw_loaded = loader.load(
        render_image_source_bytes_load_request{.source = raw_resolved.source});
    require(raw_loaded.ok(), "source bytes loader loads raw data URI bytes");
    require(raw_loaded.encoded_bytes == expected_raw, "raw data URI payload is copied as bytes");
    require(loader.load_requests.size() == 3, "data URI loads are recorded");
}

void test_source_bytes_loader_rejects_malformed_data_uris()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;

    const render_image_resolve_result bad_metadata_resolved = resolver.resolve(
        render_image_resolve_request{.uri = "data:image/png;badflag,AAAA"});
    require(bad_metadata_resolved.ok(), "bad metadata data URI still resolves by source kind");
    const render_image_data_uri_decode_result bad_metadata_decoded = decode_render_image_data_uri(
        bad_metadata_resolved.source.cache_key());
    require(!bad_metadata_decoded.ok(), "bad metadata data URI does not decode");
    require(
        bad_metadata_decoded.status == render_image_data_uri_decode_status::invalid_metadata,
        "bad metadata data URI reports metadata failure");
    require(!bad_metadata_decoded.diagnostic.empty(), "bad metadata data URI includes decode diagnostic");

    const render_image_source_bytes_load_result bad_metadata_loaded = loader.load(
        render_image_source_bytes_load_request{.source = bad_metadata_resolved.source});
    require(!bad_metadata_loaded.ok(), "source bytes loader rejects bad metadata data URI");
    require(
        bad_metadata_loaded.status == render_image_source_bytes_load_status::malformed_data_uri,
        "bad metadata data URI reports malformed load status");
    require(!bad_metadata_loaded.diagnostic.empty(), "bad metadata load includes diagnostic");

    const render_image_resolve_result bad_base64_resolved = resolver.resolve(
        render_image_resolve_request{.uri = "data:image/png;base64,@@@@"});
    require(bad_base64_resolved.ok(), "bad base64 data URI still resolves by source kind");
    const render_image_data_uri_decode_result bad_base64_decoded = decode_render_image_data_uri(
        bad_base64_resolved.source.cache_key());
    require(!bad_base64_decoded.ok(), "bad base64 data URI does not decode");
    require(
        bad_base64_decoded.status == render_image_data_uri_decode_status::invalid_base64,
        "bad base64 data URI reports base64 failure");

    const render_image_source_bytes_load_result bad_base64_loaded = loader.load(
        render_image_source_bytes_load_request{.source = bad_base64_resolved.source});
    require(!bad_base64_loaded.ok(), "source bytes loader rejects bad base64 data URI");
    require(
        bad_base64_loaded.status == render_image_source_bytes_load_status::malformed_data_uri,
        "bad base64 data URI reports malformed load status");
    require(!bad_base64_loaded.diagnostic.empty(), "bad base64 load includes diagnostic");

    const render_image_resolve_result bad_percent_resolved = resolver.resolve(
        render_image_resolve_request{.uri = "data:image/png,%zz"});
    require(bad_percent_resolved.ok(), "bad percent data URI still resolves by source kind");
    const render_image_source_bytes_load_result bad_percent_loaded = loader.load(
        render_image_source_bytes_load_request{.source = bad_percent_resolved.source});
    require(!bad_percent_loaded.ok(), "source bytes loader rejects bad percent data URI");
    require(
        bad_percent_loaded.status == render_image_source_bytes_load_status::malformed_data_uri,
        "bad percent data URI reports malformed load status");
    require(!bad_percent_loaded.diagnostic.empty(), "bad percent load includes diagnostic");

    const render_image_resolve_result empty_payload_resolved = resolver.resolve(
        render_image_resolve_request{.uri = "data:image/png;base64,"});
    require(empty_payload_resolved.ok(), "empty data URI still resolves by source kind");
    const render_image_source_bytes_load_result empty_payload_loaded = loader.load(
        render_image_source_bytes_load_request{.source = empty_payload_resolved.source});
    require(!empty_payload_loaded.ok(), "source bytes loader rejects empty data URI payload");
    require(
        empty_payload_loaded.status == render_image_source_bytes_load_status::empty_bytes,
        "empty data URI payload reports empty bytes");
    require(!empty_payload_loaded.diagnostic.empty(), "empty data URI payload includes diagnostic");

    const render_resolved_image_source missing_comma_source{
        .original_uri = "data:image/png;base64AAAA",
        .normalized_uri = "data:image/png;base64AAAA",
        .kind = render_image_source_kind::data_uri,
    };
    const render_image_source_bytes_load_result missing_comma_loaded = loader.load(
        render_image_source_bytes_load_request{.source = missing_comma_source});
    require(!missing_comma_loaded.ok(), "source bytes loader rejects comma-less data URI");
    require(
        missing_comma_loaded.status == render_image_source_bytes_load_status::malformed_data_uri,
        "comma-less data URI reports malformed load status");
    require(loader.load_requests.size() == 5, "malformed data URI loads are recorded");
}

void test_texture_cache_can_use_loaded_source_bytes()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/cache-loader.ppm"})
                                                .source;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes(source, make_ppm_2x1_encoded_bytes());

    const render_image_source_bytes_load_result loaded = loader.load(
        render_image_source_bytes_load_request{.source = source});
    require(loaded.ok(), "source bytes loader provides bytes for texture cache");

    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_placeholder_encoded_bytes_for_source(source.cache_key(), loaded.encoded_bytes);
    const render_image_texture_result texture = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = render_image_sampler_policy{}});
    require(texture.ok(), "texture cache can consume source loader bytes");
    require(!texture.cache_hit, "source loader bytes create first texture as cache miss");
    require(texture.texture.width == 2, "texture cache preserves source loader decoded width");
    require(texture.texture.height == 1, "texture cache preserves source loader decoded height");
    require(cache.cached_texture_count() == 1, "texture cache stores texture created from source loader bytes");
    require(cache.cached_decoded_byte_count() == 8, "source loader fixture decodes to rgba bytes");
}

void test_texture_uploader_uploads_valid_decoded_image()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key key{
        .source_key = "textures/upload.ppm",
        .sampler = render_image_sampler_policy{},
    };
    fake_image_texture_uploader uploader;
    const render_image_texture_upload_result uploaded = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });

    require(uploaded.ok(), "texture uploader uploads valid decoded image");
    require(uploaded.status == render_image_texture_upload_status::uploaded, "texture uploader reports uploaded");
    require(uploaded.generation_id == 1, "texture uploader reports deterministic generation id");
    require(uploaded.sampler == render_image_sampler_policy{}, "texture uploader result preserves sampler policy");
    require(uploaded.texture.valid(), "texture uploader returns valid handle");
    require(uploaded.texture.id == 1, "texture uploader handle id is deterministic");
    require(uploaded.texture.width == 2, "texture uploader handle carries width");
    require(uploaded.texture.height == 1, "texture uploader handle carries height");
    require(uploaded.pixel_count == 2, "texture uploader reports pixel count");
    require(uploaded.pixel_byte_count == 8, "texture uploader reports expected pixel bytes");
    require(uploaded.decoded_byte_count == 8, "texture uploader reports decoded byte count");
    require(uploaded.staging_byte_count == 8, "texture uploader reports staging byte count");
    require(uploader.upload_requests.size() == 1, "texture uploader records upload request");
    require(uploader.upload_request_snapshots.size() == 1, "texture uploader records request snapshot");
    require(uploader.upload_result_snapshots.size() == 1, "texture uploader records result snapshot");
    require(uploader.upload_request_snapshots[0].generation_id == 1, "request snapshot records generation id");
    require(uploader.upload_request_snapshots[0].sampler == render_image_sampler_policy{}, "request snapshot records sampler");
    require(uploader.upload_request_snapshots[0].staging_byte_count == 8, "request snapshot records staging bytes");
    require(uploader.upload_request_snapshots[0].enqueue_sequence == 1, "request snapshot records enqueue sequence");
    require(uploader.upload_request_snapshots[0].queue_depth_before_enqueue == 0, "request snapshot records queue depth before enqueue");
    require(uploader.upload_request_snapshots[0].queue_depth_after_enqueue == 1, "request snapshot records queue depth after enqueue");
    require(uploader.upload_result_snapshots[0].generation_id == 1, "result snapshot records generation id");
    require(uploader.upload_result_snapshots[0].status == render_image_texture_upload_status::uploaded, "result snapshot records status");
    require(uploader.upload_result_snapshots[0].staging_byte_count == 8, "result snapshot records staging bytes");
    require(uploader.upload_result_snapshots[0].completion_sequence == 2, "result snapshot records completion sequence");
    require(uploader.upload_result_snapshots[0].queue_depth_after_completion == 0, "result snapshot records queue depth after completion");

    const fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    require(snapshot.upload_count == 1, "texture uploader snapshot reports upload count");
    require(snapshot.failed_upload_count == 0, "texture uploader snapshot reports no failures");
    require(snapshot.uploaded_pixel_count == 2, "texture uploader snapshot reports uploaded pixels");
    require(snapshot.uploaded_pixel_byte_count == 8, "texture uploader snapshot reports uploaded pixel bytes");
    require(snapshot.uploaded_decoded_byte_count == 8, "texture uploader snapshot reports uploaded decoded bytes");
    require(snapshot.staged_byte_count == 8, "texture uploader snapshot reports successful staging bytes");
    require(snapshot.attempted_staging_byte_count == 8, "texture uploader snapshot reports attempted staging bytes");
    require(snapshot.next_generation_id == 2, "texture uploader snapshot reports next generation id");
    require(snapshot.request_snapshots.size() == 1, "texture uploader snapshot carries request snapshot");
    require(snapshot.result_snapshots.size() == 1, "texture uploader snapshot carries result snapshot");
    require(snapshot.entries.size() == 1, "texture uploader snapshot records entry");
    require(snapshot.enqueued_upload_count == 1, "texture uploader snapshot reports enqueued uploads");
    require(snapshot.completed_upload_count == 1, "texture uploader snapshot reports completed uploads");
    require(snapshot.pending_upload_count == 0, "texture uploader snapshot reports no pending uploads");
    require(snapshot.next_queue_sequence == 3, "texture uploader snapshot reports next queue sequence");
    require(snapshot.queue_entries.size() == 1, "texture uploader snapshot records queue entry");
    require(snapshot.queue_entries[0].enqueue_sequence == 1, "texture uploader queue entry records enqueue sequence");
    require(snapshot.queue_entries[0].completion_sequence == 2, "texture uploader queue entry records completion sequence");
    require(snapshot.queue_entries[0].completed, "texture uploader queue entry records completion");
    require(snapshot.queue_entries[0].status == render_image_texture_upload_status::uploaded, "texture uploader queue entry records status");
    require(snapshot.queue_entries[0].queue_depth_after_enqueue == 1, "texture uploader queue entry records enqueue depth");
    require(snapshot.queue_entries[0].queue_depth_after_completion == 0, "texture uploader queue entry records completion depth");
    require(snapshot.entries[0].generation_id == 1, "texture uploader snapshot records generation id");
    require(snapshot.entries[0].key == key, "texture uploader snapshot records key");
    require(snapshot.entries[0].sampler == render_image_sampler_policy{}, "texture uploader snapshot records sampler");
    require(snapshot.entries[0].texture.id == uploaded.texture.id, "texture uploader snapshot records handle");
    require(snapshot.entries[0].status == render_image_texture_upload_status::uploaded, "snapshot records status");
    require(snapshot.entries[0].staging_byte_count == 8, "texture uploader snapshot records staging bytes");
    require(snapshot.entries[0].request.generation_id == 1, "entry request snapshot records generation id");
    require(snapshot.entries[0].result.texture.id == uploaded.texture.id, "entry result snapshot records handle");
}

void test_texture_uploader_reports_deterministic_queue_lifecycle()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key key{
        .source_key = "textures/upload-queue.ppm",
        .sampler = render_image_sampler_policy{},
    };
    fake_image_texture_uploader uploader;

    const render_image_texture_upload_result uploaded = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });
    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    const render_image_texture_upload_result failed = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });
    require(uploaded.ok(), "queued upload lifecycle starts with successful upload");
    require(!failed.ok(), "queued upload lifecycle records failed upload");

    const fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    require(snapshot.upload_count == 2, "queued upload snapshot records all attempts");
    require(snapshot.enqueued_upload_count == 2, "queued upload snapshot records enqueued count");
    require(snapshot.completed_upload_count == 2, "queued upload snapshot records completed count");
    require(snapshot.pending_upload_count == 0, "queued upload snapshot has no pending work");
    require(snapshot.failed_upload_count == 1, "queued upload snapshot records one failure");
    require(snapshot.next_queue_sequence == 5, "queued upload snapshot advances enqueue/completion sequence");
    require(snapshot.request_snapshots.size() == 2, "queued upload snapshot records request snapshots");
    require(snapshot.result_snapshots.size() == 2, "queued upload snapshot records result snapshots");
    require(snapshot.queue_entries.size() == 2, "queued upload snapshot records queue entries");

    require(snapshot.request_snapshots[0].enqueue_sequence == 1, "first queued request has enqueue sequence");
    require(snapshot.result_snapshots[0].completion_sequence == 2, "first queued result has completion sequence");
    require(snapshot.queue_entries[0].generation_id == 1, "first queue entry records generation");
    require(snapshot.queue_entries[0].completed, "first queue entry is completed");
    require(snapshot.queue_entries[0].status == render_image_texture_upload_status::uploaded, "first queue entry records uploaded status");
    require(snapshot.queue_entries[0].texture.id == uploaded.texture.id, "first queue entry records texture");
    require(snapshot.queue_entries[0].staging_byte_count == 8, "first queue entry records staged bytes");
    require(snapshot.queue_entries[0].queue_depth_after_enqueue == 1, "first queue entry records enqueue depth");
    require(snapshot.queue_entries[0].queue_depth_after_completion == 0, "first queue entry records completion depth");

    require(snapshot.request_snapshots[1].enqueue_sequence == 3, "second queued request has enqueue sequence");
    require(snapshot.result_snapshots[1].completion_sequence == 4, "second queued result has completion sequence");
    require(snapshot.queue_entries[1].generation_id == 2, "second queue entry records generation");
    require(snapshot.queue_entries[1].completed, "second queue entry is completed");
    require(
        snapshot.queue_entries[1].status == render_image_texture_upload_status::invalid_image,
        "second queue entry records invalid image status");
    require(!snapshot.queue_entries[1].texture.valid(), "failed queue entry records no texture handle");
    require(snapshot.queue_entries[1].staging_byte_count == 0, "failed queue entry records no staged bytes");
    require(snapshot.queue_entries[1].queue_depth_after_enqueue == 1, "failed queue entry records enqueue depth");
    require(snapshot.queue_entries[1].queue_depth_after_completion == 0, "failed queue entry records completion depth");
    require(!snapshot.queue_entries[1].diagnostic.empty(), "failed queue entry preserves diagnostic");
}

void test_texture_uploader_reports_retry_eligibility_and_backoff()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key key{
        .source_key = "textures/upload-retry.ppm",
        .sampler = render_image_sampler_policy{},
    };
    fake_image_texture_uploader uploader;

    const render_image_texture_upload_result uploaded = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });

    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    const render_image_texture_upload_result first_retryable_failure = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });
    const render_image_texture_upload_result second_retryable_failure = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });
    const render_image_texture_upload_result invalid_key_failure = uploader.upload(render_image_texture_upload_request{
        .key = render_image_texture_key{.source_key = {}, .sampler = render_image_sampler_policy{}},
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });

    require(uploaded.ok(), "retry policy starts from successful upload");
    require(!first_retryable_failure.ok(), "retry policy records first retryable failure");
    require(!second_retryable_failure.ok(), "retry policy records second retryable failure");
    require(!invalid_key_failure.ok(), "retry policy records nonretryable failure");
    require(is_retryable_render_image_texture_upload_status(render_image_texture_upload_status::invalid_image), "invalid image status is retryable");
    require(
        !is_retryable_render_image_texture_upload_status(render_image_texture_upload_status::invalid_key),
        "invalid key status is not retryable");
    require(fake_image_texture_upload_retry_backoff_sequence_delta(1) == 1, "first retry backoff is one sequence");
    require(fake_image_texture_upload_retry_backoff_sequence_delta(2) == 2, "second retry backoff doubles");
    require(fake_image_texture_upload_retry_backoff_sequence_delta(6) == 16, "retry backoff is capped");

    const fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    require(snapshot.upload_count == 4, "retry policy snapshot records all uploads");
    require(snapshot.failed_upload_count == 3, "retry policy snapshot records failed uploads");
    require(snapshot.retryable_upload_count == 2, "retry policy snapshot counts retryable failures");
    require(snapshot.nonretryable_upload_count == 1, "retry policy snapshot counts nonretryable failures");
    require(snapshot.completed_without_retry_count == 1, "retry policy snapshot counts completed upload");
    require(snapshot.retry_snapshots.size() == 4, "retry policy snapshot records retry decisions");
    require(snapshot.queue_entries.size() == 4, "retry policy snapshot records queue decisions");
    require(snapshot.next_queue_sequence == 9, "retry policy snapshot advances deterministic queue sequence");

    const fake_image_texture_upload_retry_snapshot& success_retry = snapshot.retry_snapshots[0];
    require(success_retry.generation_id == 1, "successful retry snapshot records generation");
    require(success_retry.status == render_image_texture_upload_status::uploaded, "successful retry snapshot records uploaded status");
    require(
        success_retry.eligibility == fake_image_texture_upload_retry_eligibility::not_needed,
        "successful retry snapshot reports no retry needed");
    require(success_retry.attempt_count_for_key == 1, "successful retry snapshot records attempt count");
    require(success_retry.failed_attempt_count_for_key == 0, "successful retry snapshot records no failures");
    require(success_retry.next_retry_sequence == 0, "successful retry snapshot records no retry sequence");
    require(
        fake_image_texture_upload_retry_eligibility_name(success_retry.eligibility) == "not_needed",
        "successful retry snapshot reports eligibility name");

    const fake_image_texture_upload_retry_snapshot& first_retry = snapshot.retry_snapshots[1];
    require(first_retry.generation_id == 2, "first retryable snapshot records generation");
    require(
        first_retry.eligibility == fake_image_texture_upload_retry_eligibility::eligible,
        "first retryable snapshot is eligible");
    require(first_retry.attempt_count_for_key == 2, "first retryable snapshot records attempt count");
    require(first_retry.failed_attempt_count_for_key == 1, "first retryable snapshot records failed count");
    require(first_retry.retry_after_queue_sequence_delta == 1, "first retryable snapshot records backoff");
    require(first_retry.next_retry_sequence == 5, "first retryable snapshot records next retry sequence");
    require(!first_retry.diagnostic.empty(), "first retryable snapshot includes diagnostic");

    const fake_image_texture_upload_retry_snapshot& second_retry = snapshot.retry_snapshots[2];
    require(second_retry.generation_id == 3, "second retryable snapshot records generation");
    require(
        second_retry.eligibility == fake_image_texture_upload_retry_eligibility::eligible,
        "second retryable snapshot is eligible");
    require(second_retry.attempt_count_for_key == 3, "second retryable snapshot records attempt count");
    require(second_retry.failed_attempt_count_for_key == 2, "second retryable snapshot records failed count");
    require(second_retry.retry_after_queue_sequence_delta == 2, "second retryable snapshot records doubled backoff");
    require(second_retry.next_retry_sequence == 8, "second retryable snapshot records next retry sequence");

    const fake_image_texture_upload_retry_snapshot& nonretryable = snapshot.retry_snapshots[3];
    require(nonretryable.generation_id == 4, "nonretryable snapshot records generation");
    require(
        nonretryable.eligibility == fake_image_texture_upload_retry_eligibility::ineligible,
        "nonretryable snapshot is ineligible");
    require(nonretryable.status == render_image_texture_upload_status::invalid_key, "nonretryable snapshot records invalid key");
    require(nonretryable.attempt_count_for_key == 1, "nonretryable snapshot records separate key attempts");
    require(nonretryable.failed_attempt_count_for_key == 1, "nonretryable snapshot records failed count");
    require(nonretryable.retry_after_queue_sequence_delta == 0, "nonretryable snapshot records no backoff");
    require(nonretryable.next_retry_sequence == 0, "nonretryable snapshot records no retry sequence");
    require(
        fake_image_texture_upload_retry_eligibility_name(nonretryable.eligibility) == "ineligible",
        "nonretryable snapshot reports eligibility name");

    require(
        snapshot.queue_entries[1].retry.next_retry_sequence == first_retry.next_retry_sequence,
        "queue entry carries retry summary");
    require(
        snapshot.entries[2].retry.retry_after_queue_sequence_delta
            == second_retry.retry_after_queue_sequence_delta,
        "upload entry carries retry summary");
}

void test_texture_uploader_rejects_invalid_inputs()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key valid_key{
        .source_key = "textures/upload-invalid.ppm",
        .sampler = render_image_sampler_policy{},
    };
    fake_image_texture_uploader uploader;

    const render_image_texture_upload_result invalid_key = uploader.upload(render_image_texture_upload_request{
        .key = render_image_texture_key{.source_key = {}, .sampler = render_image_sampler_policy{}},
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });
    require(!invalid_key.ok(), "texture uploader rejects invalid key");
    require(invalid_key.status == render_image_texture_upload_status::invalid_key, "invalid key status is reported");
    require(invalid_key.generation_id == 1, "invalid key reports generation id");
    require(invalid_key.staging_byte_count == 8, "invalid key preserves staging diagnostics for valid payload");
    require(!invalid_key.diagnostic.empty(), "invalid key includes diagnostic");

    render_image_sampler_policy invalid_sampler;
    invalid_sampler.wrap_u = static_cast<render_image_wrap_mode>(99);
    const render_image_texture_upload_result invalid_sampler_result = uploader.upload(
        render_image_texture_upload_request{
            .key = valid_key,
            .sampler = invalid_sampler,
            .image = make_rgba_2x1_decoded_image(),
        });
    require(!invalid_sampler_result.ok(), "texture uploader rejects invalid sampler");
    require(
        invalid_sampler_result.status == render_image_texture_upload_status::invalid_sampler,
        "invalid sampler status is reported");
    require(invalid_sampler_result.generation_id == 2, "invalid sampler reports generation id");
    require(invalid_sampler_result.sampler == invalid_sampler, "invalid sampler result records attempted sampler");
    require(invalid_sampler_result.staging_byte_count == 8, "invalid sampler preserves staging diagnostics");
    require(!invalid_sampler_result.diagnostic.empty(), "invalid sampler includes diagnostic");

    render_decoded_image unsupported_format = make_rgba_2x1_decoded_image();
    unsupported_format.pixel_format = static_cast<render_image_pixel_format>(99);
    const render_image_texture_upload_result unsupported = uploader.upload(render_image_texture_upload_request{
        .key = valid_key,
        .sampler = render_image_sampler_policy{},
        .image = unsupported_format,
    });
    require(!unsupported.ok(), "texture uploader rejects unsupported format");
    require(
        unsupported.status == render_image_texture_upload_status::unsupported_format,
        "unsupported format status is reported");
    require(unsupported.generation_id == 3, "unsupported format reports generation id");
    require(unsupported.staging_byte_count == 0, "unsupported format reports no staged bytes");
    require(!unsupported.diagnostic.empty(), "unsupported format includes diagnostic");

    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    const render_image_texture_upload_result invalid_image = uploader.upload(render_image_texture_upload_request{
        .key = valid_key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });
    require(!invalid_image.ok(), "texture uploader rejects invalid payload");
    require(
        invalid_image.status == render_image_texture_upload_status::invalid_image,
        "invalid payload status is reported");
    require(invalid_image.generation_id == 4, "invalid payload reports generation id");
    require(invalid_image.staging_byte_count == 0, "invalid payload reports no staged bytes");
    require(!invalid_image.diagnostic.empty(), "invalid payload includes diagnostic");

    const fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    require(snapshot.upload_count == 4, "texture uploader snapshot records failed attempts");
    require(snapshot.failed_upload_count == 4, "texture uploader snapshot counts failures");
    require(snapshot.next_generation_id == 5, "texture uploader failure snapshot reports next generation id");
    require(snapshot.request_snapshots.size() == 4, "texture uploader failure snapshot records request snapshots");
    require(snapshot.result_snapshots.size() == 4, "texture uploader failure snapshot records result snapshots");
    require(snapshot.entries.size() == 4, "texture uploader snapshot records failure entries");
    require(snapshot.uploaded_pixel_count == 0, "texture uploader snapshot excludes failed pixels");
    require(snapshot.staged_byte_count == 0, "texture uploader failure snapshot reports no successful staging");
    require(
        snapshot.attempted_staging_byte_count == 16,
        "texture uploader failure snapshot reports attempted staging bytes");
    require(snapshot.entries[0].generation_id == 1, "snapshot records invalid key generation");
    require(snapshot.entries[0].status == render_image_texture_upload_status::invalid_key, "snapshot records invalid key");
    require(snapshot.entries[0].staging_byte_count == 8, "snapshot records invalid key staging bytes");
    require(
        snapshot.entries[1].status == render_image_texture_upload_status::invalid_sampler,
        "snapshot records invalid sampler");
    require(snapshot.entries[1].generation_id == 2, "snapshot records invalid sampler generation");
    require(snapshot.entries[1].sampler == invalid_sampler, "snapshot records invalid sampler policy");
    require(
        snapshot.entries[2].status == render_image_texture_upload_status::unsupported_format,
        "snapshot records unsupported format");
    require(snapshot.entries[2].generation_id == 3, "snapshot records unsupported format generation");
    require(
        snapshot.entries[3].status == render_image_texture_upload_status::invalid_image,
        "snapshot records invalid image");
    require(snapshot.entries[3].generation_id == 4, "snapshot records invalid image generation");
    require(!snapshot.entries[3].texture.valid(), "failed upload snapshot records no handle");
    require(!snapshot.entries[3].result.diagnostic.empty(), "failed upload result snapshot keeps diagnostic");
}

void test_texture_cache_uses_injected_texture_uploader()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/cache-uploader.ppm"})
                                                .source;
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    cache.set_placeholder_encoded_bytes_for_source(source.cache_key(), make_ppm_1x1_encoded_bytes());

    const render_image_texture_request request{.source = source, .sampler = render_image_sampler_policy{}};
    const render_image_texture_result first = cache.acquire_texture(request);
    require(first.ok(), "texture cache uses injected uploader to create texture");
    require(!first.cache_hit, "first uploader-backed cache request is a miss");
    require(first.texture.id == 1, "injected uploader returns deterministic texture id");
    require(uploader.upload_requests.size() == 1, "injected uploader records first upload");
    require(uploader.upload_requests[0].key == first.key, "injected uploader receives texture key");
    require(uploader.upload_requests[0].sampler == request.sampler, "injected uploader receives sampler");
    require(uploader.upload_requests[0].image.width == 1, "injected uploader receives decoded image");

    const render_image_texture_result second = cache.acquire_texture(request);
    require(second.ok(), "uploader-backed cache request can be reacquired");
    require(second.cache_hit, "uploader-backed cache hit avoids upload");
    require(second.texture.id == first.texture.id, "uploader-backed cache hit reuses handle");
    require(uploader.upload_requests.size() == 1, "cache hit does not call uploader again");

    const fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    require(snapshot.upload_count == 1, "injected uploader snapshot records one upload");
    require(snapshot.entries[0].generation_id == 1, "injected uploader snapshot records generation id");
    require(snapshot.entries[0].sampler == request.sampler, "injected uploader snapshot records sampler");
    require(snapshot.entries[0].staging_byte_count == 4, "injected uploader snapshot records staging bytes");
    require(snapshot.entries[0].texture.id == first.texture.id, "injected uploader snapshot matches cache handle");
}

void test_texture_cache_placeholder_policy_reuses_decode_failure_texture()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/placeholder-decode.png"})
                                                    .source;
    render_image_sampler_policy sampler;
    sampler.min_filter = render_image_filter::nearest;
    sampler.wrap_u = render_image_wrap_mode::repeat;

    fake_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_placeholder_policy policy{
        .enabled = true,
        .width = 3,
        .height = 2,
    };
    cache.set_placeholder_texture_policy(policy);

    const render_image_texture_request request{.source = source, .sampler = sampler};
    const render_image_texture_key requested_key = make_render_image_texture_key(request);
    const render_image_texture_key placeholder_key = make_fake_image_texture_placeholder_key(
        policy,
        fake_image_texture_placeholder_reason::decode_failed,
        requested_key);

    const render_image_texture_result first = cache.acquire_texture(request);
    const render_image_texture_result second = cache.acquire_texture(request);
    require(first.ok(), "enabled placeholder policy creates texture for decode failure");
    require(second.ok(), "enabled placeholder policy reacquires decode failure texture");
    require(!first.cache_hit, "first placeholder decode failure is a cache miss");
    require(second.cache_hit, "second placeholder decode failure is a cache hit");
    require(first.key == placeholder_key, "placeholder decode failure uses deterministic placeholder key");
    require(second.key == placeholder_key, "placeholder decode failure reuses deterministic placeholder key");
    require(is_fake_image_texture_placeholder_key(first.key), "placeholder decode failure key is identifiable");
    require(first.key.sampler == sampler, "placeholder decode failure preserves sampler policy");
    require(first.texture.width == 3, "placeholder decode failure uses configured width");
    require(first.texture.height == 2, "placeholder decode failure uses configured height");
    require(second.texture.id == first.texture.id, "placeholder decode failure cache hit reuses handle");
    require(!first.diagnostic.empty(), "placeholder decode failure includes diagnostic");
    require(decoder.support_requests.size() == 2, "decode failure still checks source support on retries");
    require(decoder.decode_requests.empty(), "unsupported decode failure does not call decode");
    require(uploader.upload_requests.size() == 1, "placeholder decode failure uploads once");
    require(uploader.upload_requests[0].key == placeholder_key, "placeholder upload uses placeholder key");
    require(uploader.upload_requests[0].sampler == sampler, "placeholder upload preserves sampler");
    require(uploader.upload_requests[0].image.width == 3, "placeholder upload uses configured image width");
    require(uploader.upload_requests[0].image.height == 2, "placeholder upload uses configured image height");

    const fake_image_texture_cache_snapshot snapshot = cache.diagnostic_snapshot();
    require(snapshot.placeholder_policy_enabled, "placeholder snapshot reports enabled policy");
    require(snapshot.placeholder_policy.width == 3, "placeholder snapshot records policy width");
    require(snapshot.placeholder_policy.height == 2, "placeholder snapshot records policy height");
    require(snapshot.placeholder_policy_request_count == 2, "placeholder snapshot counts fallback requests");
    require(snapshot.placeholder_policy_cache_hit_count == 1, "placeholder snapshot counts fallback cache hit");
    require(snapshot.placeholder_policy_upload_count == 1, "placeholder snapshot counts placeholder upload");
    require(snapshot.placeholder_policy_texture_count == 1, "placeholder snapshot counts resident placeholder texture");
    require(snapshot.placeholder_fallback_texture_count == 1, "placeholder upload readiness reports fallback texture");
    require(snapshot.cached_pixel_count == 6, "placeholder snapshot tracks configured pixel count");
    require(snapshot.cached_pixel_byte_count == 24, "placeholder snapshot tracks configured pixel bytes");
    require(snapshot.placeholder_snapshots.size() == 2, "placeholder snapshot records fallback attempts");
    require(snapshot.placeholder_snapshots[0].sequence == 1, "first placeholder attempt has deterministic sequence");
    require(!snapshot.placeholder_snapshots[0].cache_hit, "first placeholder snapshot records cache miss");
    require(snapshot.placeholder_snapshots[1].cache_hit, "second placeholder snapshot records cache hit");
    require(
        snapshot.placeholder_snapshots[1].texture.id == first.texture.id,
        "placeholder cache-hit snapshot records reused handle");

    const fake_image_texture_cache_entry_snapshot* entry = find_snapshot_entry(
        snapshot,
        placeholder_key.source_key);
    require(entry != nullptr, "placeholder texture appears in cache entries");
    require(entry->placeholder_texture, "placeholder cache entry is marked as placeholder");
    require(
        entry->placeholder_reason == fake_image_texture_placeholder_reason::decode_failed,
        "placeholder cache entry records decode failure reason");
    require(entry->requested_key == requested_key, "placeholder cache entry records requested key");
    require(entry->upload_readiness.placeholder_fallback, "placeholder cache entry records fallback readiness");
    require(!entry->placeholder_diagnostic.empty(), "placeholder cache entry records diagnostic");
}

void test_texture_cache_placeholder_policy_covers_failure_reasons()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    const render_image_sampler_policy sampler;

    fake_image_decoder unsupported_decoder;
    fake_image_texture_cache unsupported_cache(unsupported_decoder);
    unsupported_cache.set_placeholder_texture_policy(policy);
    const render_resolved_image_source unsupported_source{
        .original_uri = "zip://deck/card.fake",
        .normalized_uri = "zip://deck/card.fake",
        .kind = render_image_source_kind::unsupported,
    };
    const render_image_texture_result unsupported = unsupported_cache.acquire_texture(
        render_image_texture_request{.source = unsupported_source, .sampler = sampler});
    require(unsupported.ok(), "placeholder policy handles unsupported source kind");
    require(
        unsupported.key.source_key
            == "placeholder://image/resolve_failed/zip://deck/card.fake",
        "unsupported source uses resolve-failed placeholder key");
    require(unsupported.texture.width == 2, "unsupported source placeholder uses policy width");
    require(unsupported.texture.height == 2, "unsupported source placeholder uses policy height");

    const normalizing_image_resolver resolver;
    const render_resolved_image_source remote_source = resolver.resolve(
        render_image_resolve_request{.uri = "https://example.test/missing.fake"})
                                                       .source;
    fake_image_decoder remote_decoder;
    fake_image_texture_cache remote_cache(remote_decoder);
    remote_cache.set_placeholder_texture_policy(policy);
    const render_image_texture_result remote = remote_cache.acquire_texture(
        render_image_texture_request{.source = remote_source, .sampler = sampler});
    require(remote.ok(), "placeholder policy handles remote source failure");
    require(
        remote.key.source_key.starts_with("placeholder://image/source_load_failed/"),
        "remote source uses source-load placeholder key");
    require(remote.texture.width == 2, "remote placeholder uses policy width");
    require(remote_decoder.support_requests.empty(), "remote placeholder does not fetch or decode");

    const render_resolved_image_source malformed_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/placeholder-malformed.fake"})
                                                          .source;
    malformed_payload_decoder malformed_decoder;
    fake_image_texture_cache malformed_cache(malformed_decoder);
    malformed_cache.set_placeholder_texture_policy(policy);
    const render_image_texture_result malformed = malformed_cache.acquire_texture(
        render_image_texture_request{.source = malformed_source, .sampler = sampler});
    require(malformed.ok(), "placeholder policy handles invalid decoded payload");
    require(
        malformed.key.source_key.starts_with("placeholder://image/upload_failed/"),
        "malformed payload uses upload-failed placeholder key");
    require(malformed.texture.width == 2, "malformed payload placeholder uses policy width");
    require(malformed_decoder.decode_request_count == 1, "malformed payload still decodes once");

    const render_resolved_image_source upload_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/placeholder-upload.fake"})
                                                       .source;
    fake_image_decoder upload_decoder;
    source_upload_failure_then_placeholder_uploader upload_failure;
    fake_image_texture_cache upload_cache(upload_decoder, upload_failure);
    upload_cache.set_placeholder_texture_policy(policy);
    const render_image_texture_result uploaded = upload_cache.acquire_texture(
        render_image_texture_request{.source = upload_source, .sampler = sampler});
    require(uploaded.ok(), "placeholder policy handles uploader failure");
    require(
        uploaded.key.source_key.starts_with("placeholder://image/upload_failed/"),
        "uploader failure uses upload-failed placeholder key");
    require(upload_failure.rejected_source_upload_count == 1, "source uploader failure is exercised");
    require(upload_failure.upload_request_count == 2, "placeholder upload retries through uploader boundary");
    require(
        upload_failure.placeholder_uploader.upload_requests.size() == 1,
        "placeholder texture upload is delegated once");

    const fake_image_texture_cache_snapshot upload_snapshot = upload_cache.diagnostic_snapshot();
    require(upload_snapshot.placeholder_policy_request_count == 1, "upload failure snapshot counts placeholder request");
    require(upload_snapshot.placeholder_policy_upload_count == 1, "upload failure snapshot counts placeholder upload");
    require(
        upload_snapshot.placeholder_snapshots[0].reason == fake_image_texture_placeholder_reason::upload_failed,
        "upload failure snapshot records upload-failed reason");
}

void test_texture_cache_placeholder_policy_disabled_preserves_failures()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_resolved_image_source remote_source = resolver.resolve(
        render_image_resolve_request{.uri = "https://example.test/disabled.fake"})
                                                       .source;
    const render_image_texture_result remote = cache.acquire_texture(
        render_image_texture_request{.source = remote_source, .sampler = render_image_sampler_policy{}});
    require(!remote.ok(), "disabled placeholder policy preserves remote failure");
    require(remote.status == render_image_texture_status::missing_source, "remote failure status is unchanged");
    require(!remote.texture.valid(), "disabled placeholder policy returns no remote texture");

    const render_resolved_image_source decode_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/disabled.png"})
                                                       .source;
    const render_image_texture_result decode = cache.acquire_texture(
        render_image_texture_request{.source = decode_source, .sampler = render_image_sampler_policy{}});
    require(!decode.ok(), "disabled placeholder policy preserves decode failure");
    require(decode.status == render_image_texture_status::decode_failed, "decode failure status is unchanged");
    require(!decode.texture.valid(), "disabled placeholder policy returns no decode texture");

    const fake_image_texture_cache_snapshot snapshot = cache.diagnostic_snapshot();
    require(!snapshot.placeholder_policy_enabled, "disabled placeholder snapshot reports disabled policy");
    require(snapshot.placeholder_policy_request_count == 0, "disabled placeholder policy records no fallback requests");
    require(snapshot.placeholder_policy_upload_count == 0, "disabled placeholder policy records no fallback uploads");
    require(snapshot.placeholder_snapshots.empty(), "disabled placeholder policy records no fallback snapshots");
}

void test_texture_pipeline_resolves_loads_decodes_and_uploads_image_ref()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_image_resolve_result setup_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/pipeline-success.ppm"});
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes(setup_source.source, make_ppm_2x1_encoded_bytes());

    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    render_image_ref image;
    image.uri = "  ./textures\\pipeline-success.ppm ";
    image.sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(image);
    require(result.ok(), "texture pipeline creates a texture for an image ref");
    require(result.status == render_image_texture_pipeline_status::ready, "texture pipeline reports ready");
    require(result.resolve.ok(), "texture pipeline resolves image ref uri");
    require(
        result.resolve.source.cache_key() == setup_source.source.cache_key(),
        "texture pipeline uses normalized resolver key");
    require(result.source_bytes.ok(), "texture pipeline loads encoded source bytes");
    require(result.texture.ok(), "texture pipeline returns cache texture result");
    require(!result.texture.cache_hit, "first texture pipeline acquire is a cache miss");
    require(result.texture.texture.width == 2, "texture pipeline preserves decoded width");
    require(result.texture.texture.height == 1, "texture pipeline preserves decoded height");
    require(result.texture.key.sampler == image.sampler, "texture pipeline preserves image sampler policy");
    require(uploader.upload_requests.size() == 1, "texture pipeline reaches uploader once");
    require(uploader.upload_requests[0].sampler == image.sampler, "texture pipeline sends sampler to uploader");
    require(cache.cached_texture_count() == 1, "texture pipeline stores uploaded texture in cache");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.acquire_count == 1, "texture pipeline snapshot records acquire count");
    require(snapshot.ready_count == 1, "texture pipeline snapshot records ready count");
    require(snapshot.failure_count == 0, "texture pipeline snapshot reports no failures");
    require(!snapshot.entries[0].cache_hit, "texture pipeline snapshot records first miss");
    require(snapshot.entries[0].encoded_byte_count == make_ppm_2x1_encoded_bytes().size(), "snapshot records encoded bytes");
    require(snapshot.entries[0].decode_metadata.decoder_id == "ppm_image_decoder", "snapshot records decoder id");
    require(snapshot.entries[0].decode_metadata.width == 2, "snapshot records decoded width");
    require(snapshot.entries[0].decode_metadata.decoded_byte_count == 8, "snapshot records decoded byte count");
    require(snapshot.entries[0].upload_count_before == 0, "snapshot records upload count before acquire");
    require(snapshot.entries[0].upload_count_after == 1, "snapshot records upload count after acquire");
    require(snapshot.cache_snapshot.texture_count == 1, "snapshot includes cache state");
    require(snapshot.upload_diagnostics_available, "snapshot reports upload diagnostics are available");
    require(snapshot.upload_snapshot.upload_count == 1, "snapshot includes upload queue state");
}

void test_texture_pipeline_reports_source_load_failure()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "textures/pipeline-missing.ppm"});
    require(!result.ok(), "texture pipeline does not create texture without source bytes");
    require(
        result.status == render_image_texture_pipeline_status::source_load_failed,
        "texture pipeline reports source load failure");
    require(result.resolve.ok(), "texture pipeline resolves source before load failure");
    require(
        result.source_bytes.status == render_image_source_bytes_load_status::missing_bytes,
        "texture pipeline preserves source loader failure status");
    require(!result.diagnostic.empty(), "texture pipeline source failure includes diagnostic");
    require(cache.cached_texture_count() == 0, "texture pipeline source failure does not cache texture");
    require(uploader.upload_requests.empty(), "texture pipeline source failure does not upload");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.acquire_count == 1, "source failure snapshot records acquire count");
    require(snapshot.failure_count == 1, "source failure snapshot records failure count");
    require(snapshot.source_load_failure_count == 1, "source failure snapshot counts loader failures");
    require(snapshot.entries[0].status == render_image_texture_pipeline_status::source_load_failed, "snapshot records source load status");
    require(snapshot.entries[0].source_bytes_status == render_image_source_bytes_load_status::missing_bytes, "snapshot records loader status");
    require(snapshot.entries[0].encoded_byte_count == 0, "source failure snapshot records no encoded bytes");
    require(!snapshot.entries[0].decode_metadata.has_image(), "source failure snapshot records no decoded metadata");
    require(snapshot.entries[0].upload_count_before == 0, "source failure snapshot records upload count before");
    require(snapshot.entries[0].upload_count_after == 0, "source failure snapshot records unchanged upload count");
}

void test_texture_pipeline_reports_decode_failure()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_image_resolve_result source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/pipeline-decode.ppm"});
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes(source.source, make_short_ppm_2x1_encoded_bytes());

    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "textures/pipeline-decode.ppm"});
    require(!result.ok(), "texture pipeline does not create texture when decode fails");
    require(
        result.status == render_image_texture_pipeline_status::decode_failed,
        "texture pipeline maps cache decode failure");
    require(result.source_bytes.ok(), "texture pipeline loaded bytes before decode failure");
    require(result.texture.status == render_image_texture_status::decode_failed, "texture result preserves decode failure");
    require(!result.diagnostic.empty(), "texture pipeline decode failure includes diagnostic");
    require(result.texture.decode_metadata.decoder_id == "ppm_image_decoder", "decode failure preserves decoder id");
    require(uploader.upload_requests.empty(), "decode failure does not reach uploader");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.acquire_count == 1, "decode failure snapshot records acquire count");
    require(snapshot.decode_failure_count == 1, "decode failure snapshot counts decode failures");
    require(snapshot.upload_snapshot.upload_count == 0, "decode failure snapshot records no uploads");
    require(snapshot.entries[0].texture_status == render_image_texture_status::decode_failed, "snapshot records texture decode status");
    require(snapshot.entries[0].decode_metadata.decoder_id == "ppm_image_decoder", "snapshot records failed decoder id");
    require(
        snapshot.entries[0].decode_metadata.encoded_byte_count == make_short_ppm_2x1_encoded_bytes().size(),
        "snapshot records failed decode encoded byte count");
    require(snapshot.entries[0].upload_count_before == 0, "decode failure snapshot records upload count before");
    require(snapshot.entries[0].upload_count_after == 0, "decode failure snapshot records upload count after");
}

void test_texture_pipeline_reuses_cache_hits()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_image_resolve_result source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/pipeline-cache.fake"});
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes(source.source, {std::byte{0x01}});

    fake_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_request request{
        .uri = "textures/pipeline-cache.fake",
        .sampler = render_image_sampler_policy{},
    };
    const render_image_texture_pipeline_result first = pipeline.acquire_texture(request);
    const render_image_texture_pipeline_result second = pipeline.acquire_texture(request);
    require(first.ok(), "first texture pipeline cache request succeeds");
    require(second.ok(), "second texture pipeline cache request succeeds");
    require(!first.texture.cache_hit, "first texture pipeline cache request is a miss");
    require(second.texture.cache_hit, "second texture pipeline cache request is a cache hit");
    require(second.texture.texture.id == first.texture.texture.id, "texture pipeline reuses cached handle");
    require(loader.load_requests.size() == 2, "texture pipeline loads bytes deterministically before cache access");
    require(decoder.decode_requests.size() == 1, "texture pipeline cache hit avoids second decode");
    require(uploader.upload_requests.size() == 1, "texture pipeline cache hit avoids second upload");

    fake_image_texture_pipeline_snapshot after_hit = pipeline.diagnostic_snapshot();
    require(after_hit.acquire_count == 2, "pipeline snapshot records cache-hit acquire count");
    require(after_hit.ready_count == 2, "pipeline snapshot records cache-hit ready count");
    require(after_hit.cache_hit_count == 1, "pipeline snapshot counts cache hits");
    require(after_hit.entries[0].upload_count_before == 0, "first pipeline entry starts with no uploads");
    require(after_hit.entries[0].upload_count_after == 1, "first pipeline entry uploads once");
    require(after_hit.entries[1].cache_hit, "second pipeline entry records cache hit");
    require(after_hit.entries[1].upload_count_before == 1, "cache-hit pipeline entry sees previous upload");
    require(after_hit.entries[1].upload_count_after == 1, "cache-hit pipeline entry does not upload");
    require(after_hit.upload_snapshot.upload_count == 1, "pipeline snapshot records one upload after cache hit");

    pipeline.invalidate_source(source.source.cache_key());
    const render_image_texture_pipeline_result third = pipeline.acquire_texture(request);
    require(third.ok(), "texture pipeline reacquires after source invalidation");
    require(!third.texture.cache_hit, "texture pipeline invalidation forces cache miss");
    require(third.texture.texture.id != first.texture.texture.id, "texture pipeline invalidation creates new handle");
    require(decoder.decode_requests.size() == 2, "texture pipeline invalidation forces second decode");
    require(uploader.upload_requests.size() == 2, "texture pipeline invalidation forces second upload");

    const fake_image_texture_pipeline_snapshot after_invalidation = pipeline.diagnostic_snapshot();
    require(after_invalidation.acquire_count == 3, "pipeline snapshot records post-invalidation acquire");
    require(after_invalidation.invalidation_count == 1, "pipeline snapshot records invalidation count");
    require(after_invalidation.cache_hit_count == 1, "pipeline snapshot preserves cache-hit count after invalidation");
    require(after_invalidation.entries[2].sequence == 3, "pipeline snapshot keeps deterministic entry sequence");
    require(!after_invalidation.entries[2].cache_hit, "post-invalidation entry records cache miss");
    require(after_invalidation.entries[2].upload_count_before == 1, "post-invalidation entry records upload count before");
    require(after_invalidation.entries[2].upload_count_after == 2, "post-invalidation entry records upload count after");
    require(after_invalidation.cache_snapshot.texture_count == 1, "post-invalidation snapshot includes current cache state");
    require(after_invalidation.upload_snapshot.upload_count == 2, "post-invalidation snapshot includes upload state");
}

void test_sampler_policy_validation_rejects_unknown_enum_values()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy sampler;
    require(is_valid_render_image_sampler_policy(sampler), "baseline sampler policy is valid");

    sampler.min_filter = static_cast<render_image_filter>(99);
    require(!is_valid_render_image_sampler_policy(sampler), "unknown min filter is invalid");
    sampler.min_filter = render_image_filter::linear;

    sampler.mag_filter = static_cast<render_image_filter>(99);
    require(!is_valid_render_image_sampler_policy(sampler), "unknown mag filter is invalid");
    sampler.mag_filter = render_image_filter::linear;

    sampler.mipmap_mode = static_cast<render_image_mipmap_mode>(99);
    require(!is_valid_render_image_sampler_policy(sampler), "unknown mipmap mode is invalid");
    sampler.mipmap_mode = render_image_mipmap_mode::none;

    sampler.wrap_u = static_cast<render_image_wrap_mode>(99);
    require(!is_valid_render_image_sampler_policy(sampler), "unknown wrap_u mode is invalid");
    sampler.wrap_u = render_image_wrap_mode::clamp_to_edge;

    sampler.wrap_v = static_cast<render_image_wrap_mode>(99);
    require(!is_valid_render_image_sampler_policy(sampler), "unknown wrap_v mode is invalid");
}

void test_texture_cache_reuses_matching_key_and_misses_on_sampler_change()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.fake"})
                                                    .source;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_texture_request first_request{.source = source, .sampler = render_image_sampler_policy{}};
    const render_image_texture_result first = cache.acquire_texture(first_request);
    require(first.ok(), "first cache request creates a texture");
    require(!first.cache_hit, "first request is a cache miss");
    require(first.texture.id == 1, "first fake texture id is stable");

    const render_image_texture_result second = cache.acquire_texture(first_request);
    require(second.ok(), "second cache request returns a texture");
    require(second.cache_hit, "second matching request is a cache hit");
    require(second.texture.id == first.texture.id, "cache hit reuses the same fake texture");

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    const render_image_texture_result sampler_miss = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = nearest_sampler});
    require(sampler_miss.ok(), "sampler variant creates a texture");
    require(!sampler_miss.cache_hit, "different sampler policy is a cache miss");
    require(sampler_miss.texture.id != first.texture.id, "sampler variant receives a different texture");

    cache.release_unused();
    require(cache.release_unused_count() == 1, "cache release hook is callable");
}

void test_texture_cache_keys_include_all_sampler_policy_fields()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/sampler-fields.fake"})
                                                    .source;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_sampler_policy base_sampler;
    const render_image_texture_result base = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = base_sampler});
    require(base.ok(), "base sampler creates a texture");

    auto require_sampler_miss = [&](render_image_sampler_policy sampler, const char* message) {
        const render_image_texture_result result = cache.acquire_texture(
            render_image_texture_request{.source = source, .sampler = sampler});
        require(result.ok(), message);
        require(!result.cache_hit, "sampler field variant is a cache miss");
        require(result.texture.id != base.texture.id, "sampler field variant receives a distinct texture id");
        require(result.key.source_key == base.key.source_key, "sampler field variant preserves source key");
    };

    render_image_sampler_policy mag_filter_sampler = base_sampler;
    mag_filter_sampler.mag_filter = render_image_filter::nearest;
    require_sampler_miss(mag_filter_sampler, "mag filter variant creates a texture");

    render_image_sampler_policy mipmap_sampler = base_sampler;
    mipmap_sampler.mipmap_mode = render_image_mipmap_mode::nearest;
    require_sampler_miss(mipmap_sampler, "mipmap variant creates a texture");

    render_image_sampler_policy wrap_u_sampler = base_sampler;
    wrap_u_sampler.wrap_u = render_image_wrap_mode::repeat;
    require_sampler_miss(wrap_u_sampler, "wrap_u variant creates a texture");

    render_image_sampler_policy wrap_v_sampler = base_sampler;
    wrap_v_sampler.wrap_v = render_image_wrap_mode::mirrored_repeat;
    require_sampler_miss(wrap_v_sampler, "wrap_v variant creates a texture");

    const render_image_texture_result base_again = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = base_sampler});
    require(base_again.ok(), "base sampler can still be reacquired");
    require(base_again.cache_hit, "base sampler remains cached after sampler variants");
    require(base_again.texture.id == base.texture.id, "base sampler cache entry is unchanged");
}

void test_sampler_cache_policy_diagnostics_report_choices_and_stable_keys()
{
    using namespace quiz_vulkan::render;

    const render_image_sampler_policy base_sampler;
    const render_image_sampler_policy_diagnostic base_policy =
        make_render_image_sampler_policy_diagnostic(base_sampler);
    require(base_policy.valid, "default sampler diagnostic reports valid policy");
    require(base_policy.min_filter == "linear", "default sampler diagnostic reports min filter");
    require(base_policy.mag_filter == "linear", "default sampler diagnostic reports mag filter");
    require(base_policy.mipmap_mode == "none", "default sampler diagnostic reports mipmap mode");
    require(base_policy.wrap_u == "clamp_to_edge", "default sampler diagnostic reports wrap_u");
    require(base_policy.wrap_v == "clamp_to_edge", "default sampler diagnostic reports wrap_v");
    require(base_policy.uses_linear_filtering, "default sampler diagnostic reports linear filtering");
    require(!base_policy.uses_nearest_filtering, "default sampler diagnostic reports no nearest filtering");
    require(!base_policy.uses_mipmaps, "default sampler diagnostic reports no mipmaps");
    require(base_policy.clamps_u && base_policy.clamps_v, "default sampler diagnostic reports clamp wraps");
    require(!base_policy.repeats_u && !base_policy.repeats_v, "default sampler diagnostic reports no repeats");
    require(
        base_policy.stable_key_fragment
            == "min=linear;mag=linear;mipmap=none;wrap_u=clamp_to_edge;wrap_v=clamp_to_edge",
        "default sampler diagnostic reports stable key fragment");

    render_image_sampler_policy variant_sampler;
    variant_sampler.min_filter = render_image_filter::nearest;
    variant_sampler.mipmap_mode = render_image_mipmap_mode::linear;
    variant_sampler.wrap_u = render_image_wrap_mode::repeat;
    variant_sampler.wrap_v = render_image_wrap_mode::mirrored_repeat;
    const render_image_sampler_policy_diagnostic variant_policy =
        make_render_image_sampler_policy_diagnostic(variant_sampler);
    require(variant_policy.valid, "variant sampler diagnostic reports valid policy");
    require(variant_policy.uses_nearest_filtering, "variant sampler diagnostic reports nearest filtering");
    require(variant_policy.uses_linear_filtering, "variant sampler diagnostic reports linear filtering");
    require(variant_policy.uses_mipmaps, "variant sampler diagnostic reports mipmaps");
    require(variant_policy.repeats_u && variant_policy.repeats_v, "variant sampler diagnostic reports repeat wraps");
    require(!variant_policy.clamps_u && !variant_policy.clamps_v, "variant sampler diagnostic reports no clamps");
    require(
        variant_policy.stable_key_fragment
            == "min=nearest;mag=linear;mipmap=linear;wrap_u=repeat;wrap_v=mirrored_repeat",
        "variant sampler diagnostic reports stable key fragment");

    const render_image_texture_key base_key{
        .source_key = "textures/policy.fake",
        .sampler = base_sampler,
    };
    const render_image_texture_key variant_key{
        .source_key = "textures/policy.fake",
        .sampler = variant_sampler,
    };
    const render_image_texture_key_diagnostic base_key_diagnostic =
        make_render_image_texture_key_diagnostic(base_key);
    const render_image_texture_key_diagnostic variant_key_diagnostic =
        make_render_image_texture_key_diagnostic(variant_key);
    require(base_key_diagnostic.valid, "base texture key diagnostic reports valid key");
    require(variant_key_diagnostic.valid, "variant texture key diagnostic reports valid key");
    require(
        base_key_diagnostic.stable_cache_key
            == "source=textures/policy.fake|min=linear;mag=linear;mipmap=none;wrap_u=clamp_to_edge;wrap_v=clamp_to_edge",
        "base texture key diagnostic reports stable cache key");
    require(
        variant_key_diagnostic.stable_cache_key
            == "source=textures/policy.fake|min=nearest;mag=linear;mipmap=linear;wrap_u=repeat;wrap_v=mirrored_repeat",
        "variant texture key diagnostic reports stable cache key");
    require(
        base_key_diagnostic.stable_cache_key != variant_key_diagnostic.stable_cache_key,
        "stable texture key changes when sampler policy changes");

    render_image_sampler_policy invalid_sampler;
    invalid_sampler.wrap_u = static_cast<render_image_wrap_mode>(99);
    const render_image_sampler_policy_diagnostic invalid_policy =
        make_render_image_sampler_policy_diagnostic(invalid_sampler);
    require(!invalid_policy.valid, "invalid sampler diagnostic reports invalid policy");
    require(invalid_policy.wrap_u == "unknown", "invalid sampler diagnostic reports unknown wrap");
    require(!invalid_policy.diagnostic.empty(), "invalid sampler diagnostic includes text");

    require(
        render_image_texture_color_space_for(render_image_pixel_format::rgba8_srgb)
            == render_image_texture_color_space::srgb,
        "srgb pixel format maps to srgb texture color space");
    require(
        render_image_texture_color_space_for(render_image_pixel_format::rgba8_unorm)
            == render_image_texture_color_space::linear,
        "unorm pixel format maps to linear texture color space");
    require(
        render_image_texture_color_space_name(render_image_texture_color_space::srgb) == "srgb",
        "color-space diagnostic reports srgb name");
}

void test_texture_cache_snapshot_reports_policy_readiness_and_placeholder_fallback()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/readiness.fake"})
                                                    .source;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    render_image_sampler_policy sampler;
    sampler.min_filter = render_image_filter::nearest;
    sampler.mipmap_mode = render_image_mipmap_mode::linear;
    sampler.wrap_u = render_image_wrap_mode::repeat;
    sampler.wrap_v = render_image_wrap_mode::mirrored_repeat;

    const render_image_texture_result result = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = sampler});
    require(result.ok(), "readiness texture cache request succeeds");
    require(result.decode_metadata.format_detection.placeholder_fallback, "fake decode reports placeholder fallback");

    const fake_image_texture_cache_snapshot snapshot = cache.diagnostic_snapshot();
    require(snapshot.texture_count == 1, "readiness snapshot reports one texture");
    require(snapshot.upload_ready_texture_count == 1, "readiness snapshot reports upload-ready texture");
    require(
        snapshot.placeholder_fallback_texture_count == 1,
        "readiness snapshot reports placeholder fallback texture");

    const fake_image_texture_cache_entry_snapshot* entry = find_snapshot_entry(snapshot, source.cache_key());
    require(entry != nullptr, "readiness snapshot includes texture entry");
    require(entry->key == result.key, "readiness entry reports texture key");
    require(entry->key_diagnostic.valid, "readiness entry reports valid texture key");
    require(
        entry->key_diagnostic.stable_cache_key
            == make_render_image_texture_key_diagnostic(result.key).stable_cache_key,
        "readiness entry reports deterministic stable cache key");
    require(entry->sampler_policy.sampler == sampler, "readiness entry reports sampler association");
    require(entry->sampler_policy.uses_nearest_filtering, "readiness entry reports nearest filtering");
    require(entry->sampler_policy.uses_mipmaps, "readiness entry reports mipmap policy");
    require(entry->sampler_policy.repeats_u && entry->sampler_policy.repeats_v, "readiness entry reports wrap repeats");

    require(entry->upload_readiness.key == result.key, "upload readiness records texture key");
    require(entry->upload_readiness.upload_ready, "upload readiness reports ready payload");
    require(entry->upload_readiness.payload_valid, "upload readiness reports valid decoded payload");
    require(entry->upload_readiness.placeholder_fallback, "upload readiness reports placeholder fallback");
    require(
        entry->upload_readiness.color_space == render_image_texture_color_space::srgb,
        "upload readiness reports srgb color space");
    require(entry->upload_readiness.color_space_name == "srgb", "upload readiness reports color space name");
    require(entry->upload_readiness.pixel_count == 2, "upload readiness reports pixel count");
    require(entry->upload_readiness.pixel_byte_count == 8, "upload readiness reports pixel bytes");
    require(entry->upload_readiness.decoded_byte_count == 8, "upload readiness reports decoded bytes");
    require(entry->upload_readiness.staging_byte_count == 8, "upload readiness reports staging bytes");
    require(!entry->upload_readiness.diagnostic.empty(), "upload readiness includes diagnostic text");

    const render_image_upload_readiness_snapshot invalid_key_readiness =
        make_render_image_upload_readiness_snapshot(
            render_image_texture_key{.source_key = {}, .sampler = render_image_sampler_policy{}},
            result.decode_metadata,
            make_rgba_2x1_decoded_image());
    require(!invalid_key_readiness.upload_ready, "upload readiness rejects invalid cache key");
    require(!invalid_key_readiness.key_diagnostic.valid, "upload readiness reports invalid key diagnostic");

    linear_pixel_format_decoder linear_decoder;
    fake_image_texture_cache linear_cache(linear_decoder);
    const render_resolved_image_source linear_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/readiness-linear.fake"})
                                                           .source;
    const render_image_texture_result linear_result = linear_cache.acquire_texture(
        render_image_texture_request{.source = linear_source, .sampler = render_image_sampler_policy{}});
    require(linear_result.ok(), "linear readiness texture cache request succeeds");
    const fake_image_texture_cache_entry_snapshot* linear_entry = find_snapshot_entry(
        linear_cache.diagnostic_snapshot(),
        linear_source.cache_key());
    require(linear_entry != nullptr, "linear readiness snapshot includes texture entry");
    require(
        linear_entry->upload_readiness.color_space == render_image_texture_color_space::linear,
        "upload readiness reports linear color space");
    require(!linear_entry->upload_readiness.placeholder_fallback, "linear decoder does not report placeholder fallback");
}

void test_texture_cache_reuses_normalized_equivalent_cache_keys()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    const render_image_sampler_policy sampler;

    const render_resolved_image_source local_slashes = resolver.resolve(
        render_image_resolve_request{.uri = "  ./textures\\cards\\front.fake  "})
                                                            .source;
    const render_resolved_image_source local_canonical = resolver.resolve(
        render_image_resolve_request{.uri = "textures/cards/front.fake"})
                                                               .source;
    require(
        local_slashes.cache_key() == "textures/cards/front.fake",
        "local path cache key strips current directory and normalizes slashes");
    require(
        local_slashes.cache_key() == local_canonical.cache_key(),
        "equivalent local image refs resolve to one cache key");

    const render_image_texture_result local_first = cache.acquire_texture(
        render_image_texture_request{.source = local_slashes, .sampler = sampler});
    const render_image_texture_result local_second = cache.acquire_texture(
        render_image_texture_request{.source = local_canonical, .sampler = sampler});
    require(local_first.ok(), "first normalized local source creates a texture");
    require(local_second.ok(), "second equivalent local source returns a texture");
    require(local_second.cache_hit, "equivalent local source is a cache hit");
    require(local_second.texture.id == local_first.texture.id, "equivalent local source reuses texture id");
    require(local_second.key.source_key == local_first.key.source_key, "equivalent local source reuses texture key");

    const render_resolved_image_source asset_slashes = resolver.resolve(
        render_image_resolve_request{.uri = "  ASSET://./Deck\\cards\\front.fake  "})
                                                            .source;
    const render_resolved_image_source asset_canonical = resolver.resolve(
        render_image_resolve_request{.uri = "asset:////Deck/cards/front.fake"})
                                                               .source;
    require(
        asset_slashes.cache_key() == "asset://Deck/cards/front.fake",
        "asset uri cache key strips redundant asset slashes");
    require(
        asset_slashes.cache_key() == asset_canonical.cache_key(),
        "equivalent asset image refs resolve to one cache key");

    const render_image_texture_result asset_first = cache.acquire_texture(
        render_image_texture_request{.source = asset_slashes, .sampler = sampler});
    const render_image_texture_result asset_second = cache.acquire_texture(
        render_image_texture_request{.source = asset_canonical, .sampler = sampler});
    require(asset_first.ok(), "first normalized asset source creates a texture");
    require(asset_second.ok(), "second equivalent asset source returns a texture");
    require(asset_second.cache_hit, "equivalent asset source is a cache hit");
    require(asset_second.texture.id == asset_first.texture.id, "equivalent asset source reuses texture id");
    require(asset_second.key.source_key == asset_first.key.source_key, "equivalent asset source reuses texture key");
}

void test_texture_cache_rejects_invalid_sampler_policy_before_decode()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/invalid-sampler.fake"})
                                                    .source;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    render_image_sampler_policy invalid_sampler;
    invalid_sampler.wrap_v = static_cast<render_image_wrap_mode>(99);

    const render_image_texture_result result = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = invalid_sampler});
    require(!result.ok(), "invalid sampler policy does not create a texture");
    require(result.status == render_image_texture_status::upload_failed, "invalid sampler reports upload failure");
    require(!result.texture.valid(), "invalid sampler returns no texture handle");
    require(!result.diagnostic.empty(), "invalid sampler includes diagnostic");
    require(decoder.support_requests.empty(), "invalid sampler fails before decoder support check");
    require(decoder.decode_requests.empty(), "invalid sampler fails before decode");
}

void test_texture_cache_diagnostic_snapshot_reports_entries_and_recency()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source one_pixel_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/snapshot-one.ppm"})
                                                        .source;
    const render_resolved_image_source two_pixel_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/snapshot-two.ppm"})
                                                        .source;
    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_max_cached_pixel_count(4);
    cache.set_placeholder_encoded_bytes_for_source(one_pixel_source.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(two_pixel_source.cache_key(), make_ppm_2x1_encoded_bytes());

    const render_image_texture_request one_pixel_request{
        .source = one_pixel_source,
        .sampler = render_image_sampler_policy{},
    };
    const render_image_texture_request two_pixel_request{
        .source = two_pixel_source,
        .sampler = render_image_sampler_policy{},
    };

    const render_image_texture_result one_pixel_first = cache.acquire_texture(one_pixel_request);
    const render_image_texture_result two_pixel_first = cache.acquire_texture(two_pixel_request);
    const render_image_texture_result one_pixel_hit = cache.acquire_texture(one_pixel_request);
    require(one_pixel_first.ok(), "snapshot one-pixel texture is created");
    require(two_pixel_first.ok(), "snapshot two-pixel texture is created");
    require(one_pixel_hit.cache_hit, "snapshot one-pixel texture can refresh recency");

    const fake_image_texture_cache_snapshot snapshot = cache.diagnostic_snapshot();
    require(snapshot.texture_count == 2, "snapshot reports cached texture count");
    require(snapshot.entries.size() == 2, "snapshot carries one entry per cached texture");
    require(snapshot.max_cached_pixel_count == 4, "snapshot reports configured pixel budget");
    require(snapshot.cached_pixel_count == 3, "snapshot reports aggregate pixel count");
    require(snapshot.cached_pixel_byte_count == 12, "snapshot reports aggregate pixel bytes");
    require(snapshot.cached_decoded_byte_count == 12, "snapshot reports aggregate decoded bytes");

    const fake_image_texture_cache_entry_snapshot* one_entry = find_snapshot_entry(
        snapshot,
        one_pixel_source.cache_key());
    const fake_image_texture_cache_entry_snapshot* two_entry = find_snapshot_entry(
        snapshot,
        two_pixel_source.cache_key());
    require(one_entry != nullptr, "snapshot includes one-pixel source entry");
    require(two_entry != nullptr, "snapshot includes two-pixel source entry");

    require(one_entry->key == one_pixel_first.key, "snapshot one-pixel entry reports texture key");
    require(one_entry->texture.id == one_pixel_first.texture.id, "snapshot one-pixel entry reports handle id");
    require(one_entry->texture.width == 1, "snapshot one-pixel entry reports handle width");
    require(one_entry->texture.height == 1, "snapshot one-pixel entry reports handle height");
    require(one_entry->pixel_count == 1, "snapshot one-pixel entry reports pixel count");
    require(one_entry->pixel_byte_count == 4, "snapshot one-pixel entry reports pixel bytes");
    require(one_entry->decoded_byte_count == 4, "snapshot one-pixel entry reports decoded bytes");

    require(two_entry->key == two_pixel_first.key, "snapshot two-pixel entry reports texture key");
    require(two_entry->texture.id == two_pixel_first.texture.id, "snapshot two-pixel entry reports handle id");
    require(two_entry->texture.width == 2, "snapshot two-pixel entry reports handle width");
    require(two_entry->texture.height == 1, "snapshot two-pixel entry reports handle height");
    require(two_entry->pixel_count == 2, "snapshot two-pixel entry reports pixel count");
    require(two_entry->pixel_byte_count == 8, "snapshot two-pixel entry reports pixel bytes");
    require(two_entry->decoded_byte_count == 8, "snapshot two-pixel entry reports decoded bytes");
    require(
        one_entry->last_used_sequence > two_entry->last_used_sequence,
        "snapshot last-used sequence reflects most recent cache hit");

    cache.invalidate_source(one_pixel_source.cache_key());
    const fake_image_texture_cache_snapshot after_invalidation = cache.diagnostic_snapshot();
    require(after_invalidation.texture_count == 1, "snapshot updates texture count after source invalidation");
    require(after_invalidation.cached_pixel_count == 2, "snapshot updates pixel count after source invalidation");
    require(
        after_invalidation.cached_pixel_byte_count == 8,
        "snapshot updates pixel bytes after source invalidation");
    require(
        after_invalidation.cached_decoded_byte_count == 8,
        "snapshot updates decoded bytes after source invalidation");
    require(
        find_snapshot_entry(after_invalidation, one_pixel_source.cache_key()) == nullptr,
        "snapshot drops invalidated source entry");
    require(
        find_snapshot_entry(after_invalidation, two_pixel_source.cache_key()) != nullptr,
        "snapshot preserves unrelated source entry");
}

void test_texture_cache_invalidates_by_texture_key_and_source()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source_a = resolver.resolve(
        render_image_resolve_request{.uri = "textures/invalidate-a.ppm"})
                                                     .source;
    const render_resolved_image_source source_b = resolver.resolve(
        render_image_resolve_request{.uri = "textures/invalidate-b.ppm"})
                                                     .source;
    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_placeholder_encoded_bytes_for_source(source_a.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(source_b.cache_key(), make_ppm_1x1_encoded_bytes());

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;

    const render_image_texture_request source_a_default_request{
        .source = source_a,
        .sampler = render_image_sampler_policy{},
    };
    const render_image_texture_request source_a_nearest_request{
        .source = source_a,
        .sampler = nearest_sampler,
    };
    const render_image_texture_request source_b_request{
        .source = source_b,
        .sampler = render_image_sampler_policy{},
    };

    const render_image_texture_result source_a_default = cache.acquire_texture(source_a_default_request);
    const render_image_texture_result source_a_nearest = cache.acquire_texture(source_a_nearest_request);
    const render_image_texture_result source_b_first = cache.acquire_texture(source_b_request);
    require(source_a_default.ok(), "source invalidation default sampler creates a texture");
    require(source_a_nearest.ok(), "source invalidation nearest sampler creates a texture");
    require(source_b_first.ok(), "source invalidation control source creates a texture");
    require(cache.cached_texture_count() == 3, "source invalidation starts with three cached textures");
    require(cache.cached_pixel_count() == 3, "source invalidation starts with three cached pixels");

    cache.invalidate_texture(make_render_image_texture_key(source_a_nearest_request));
    require(cache.cached_texture_count() == 2, "texture-key invalidation removes one sampler variant");
    require(cache.cached_pixel_count() == 2, "texture-key invalidation subtracts one pixel");

    const render_image_texture_result source_a_nearest_second = cache.acquire_texture(source_a_nearest_request);
    require(source_a_nearest_second.ok(), "invalidated sampler variant can be reacquired");
    require(!source_a_nearest_second.cache_hit, "invalidated sampler variant is a cache miss");
    require(
        source_a_nearest_second.texture.id != source_a_nearest.texture.id,
        "invalidated sampler variant receives a new texture id");

    cache.invalidate_source(source_a.cache_key());
    require(cache.cached_texture_count() == 1, "source invalidation removes all source variants");
    require(cache.cached_pixel_count() == 1, "source invalidation leaves only control source pixels");

    const render_image_texture_result source_b_second = cache.acquire_texture(source_b_request);
    require(source_b_second.ok(), "source invalidation preserves unrelated source");
    require(source_b_second.cache_hit, "unrelated source remains cached");
    require(source_b_second.texture.id == source_b_first.texture.id, "unrelated source keeps texture id");

    const render_image_texture_result source_a_default_second = cache.acquire_texture(source_a_default_request);
    require(source_a_default_second.ok(), "source-invalidated default texture can be reacquired");
    require(!source_a_default_second.cache_hit, "source-invalidated default texture is a cache miss");
    require(
        source_a_default_second.texture.id != source_a_default.texture.id,
        "source-invalidated default texture receives a new id");
}

void test_texture_cache_reports_explicit_failures()
{
    using namespace quiz_vulkan::render;

    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_texture_result missing = cache.acquire_texture(
        render_image_texture_request{.source = {}, .sampler = render_image_sampler_policy{}});
    require(!missing.ok(), "empty source does not create a texture");
    require(missing.status == render_image_texture_status::missing_source, "empty source reports missing source");
    require(!missing.diagnostic.empty(), "empty source includes diagnostic");

    render_resolved_image_source control_character_source{
        .original_uri = "bad\n.fake",
        .normalized_uri = std::string("bad\n.fake"),
        .kind = render_image_source_kind::local_path,
    };
    const render_image_texture_result invalid_key = cache.acquire_texture(
        render_image_texture_request{.source = control_character_source, .sampler = render_image_sampler_policy{}});
    require(!invalid_key.ok(), "control character cache key does not create a texture");
    require(invalid_key.status == render_image_texture_status::missing_source, "invalid cache key reports missing source");

    const normalizing_image_resolver resolver;
    const render_resolved_image_source remote = resolver.resolve(
        render_image_resolve_request{.uri = "https://example.test/card.fake"})
                                                    .source;
    const render_image_texture_result remote_result = cache.acquire_texture(
        render_image_texture_request{.source = remote, .sampler = render_image_sampler_policy{}});
    require(!remote_result.ok(), "fake texture cache does not fetch remote image bytes");
    require(remote_result.status == render_image_texture_status::missing_source, "remote image reports missing source");
    require(decoder.support_requests.empty(), "early source failures do not reach the decoder");
}

void test_texture_cache_propagates_decoder_failures()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source unsupported_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.png"})
                                                              .source;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_texture_result unsupported = cache.acquire_texture(
        render_image_texture_request{.source = unsupported_source, .sampler = render_image_sampler_policy{}});
    require(!unsupported.ok(), "unsupported decoder source does not create a texture");
    require(
        unsupported.status == render_image_texture_status::decode_failed,
        "unsupported decoder source reports decode failure");
    require(!unsupported.diagnostic.empty(), "unsupported decoder source includes diagnostic");
    require(decoder.support_requests.size() == 1, "unsupported source reached decoder support check");
    require(decoder.decode_requests.empty(), "unsupported source does not decode");

    const render_resolved_image_source fake_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.fake"})
                                                         .source;
    cache.set_placeholder_encoded_bytes({});
    const render_image_texture_result empty = cache.acquire_texture(
        render_image_texture_request{.source = fake_source, .sampler = render_image_sampler_policy{}});
    require(!empty.ok(), "decoder empty input does not create a texture");
    require(empty.status == render_image_texture_status::decode_failed, "decoder empty input reports decode failure");
    require(!empty.diagnostic.empty(), "decoder empty input diagnostic is propagated");
    require(empty.decode_metadata.decoder_id == "fake_image_decoder", "empty cache decode propagates decoder id");
    require(empty.decode_metadata.encoded_byte_count == 0, "empty cache decode propagates encoded byte count");
    require(!empty.decode_metadata.has_image(), "empty cache decode propagates no image metadata");
    require(decoder.support_requests.size() == 2, "empty input reached decoder support check");
    require(decoder.decode_requests.size() == 1, "empty input reached decoder decode");
}

void test_texture_cache_uses_ppm_decoder_placeholder_bytes()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.ppm"})
                                                    .source;
    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_placeholder_encoded_bytes(make_ppm_2x1_encoded_bytes());

    const render_image_texture_request request{.source = source, .sampler = render_image_sampler_policy{}};
    const render_image_texture_result first = cache.acquire_texture(request);
    require(first.ok(), "ppm decoder creates a texture through the fake cache");
    require(!first.cache_hit, "first ppm texture request is a cache miss");
    require(first.texture.width == 2, "ppm texture preserves decoded width");
    require(first.texture.height == 1, "ppm texture preserves decoded height");
    require(first.decode_metadata.decoder_id == "ppm_image_decoder", "ppm texture reports decoder id");
    require(first.decode_metadata.width == 2, "ppm texture reports decoded metadata width");
    require(first.decode_metadata.height == 1, "ppm texture reports decoded metadata height");
    require(first.decode_metadata.has_image(), "ppm texture reports decoded image metadata");

    const render_image_texture_result second = cache.acquire_texture(request);
    require(second.ok(), "ppm texture can be reacquired");
    require(second.cache_hit, "second ppm texture request is a cache hit");
    require(second.texture.id == first.texture.id, "ppm texture cache reuses the handle");
    require(second.decode_metadata.decoder_id == "ppm_image_decoder", "cached ppm texture preserves decoder id");
    require(second.decode_metadata.decoded_byte_count == first.decode_metadata.decoded_byte_count, "cached ppm texture preserves decoded byte count");
}

void test_texture_cache_propagates_ppm_decoder_payload_failure()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.ppm"})
                                                    .source;
    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_placeholder_encoded_bytes(make_short_ppm_2x1_encoded_bytes());

    const render_image_texture_result result = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = render_image_sampler_policy{}});
    require(!result.ok(), "short ppm payload does not create a texture");
    require(result.status == render_image_texture_status::decode_failed, "short ppm payload reports decode failure");
    require(!result.texture.valid(), "short ppm payload returns no texture handle");
    require(!result.diagnostic.empty(), "short ppm payload propagates decoder diagnostic");
    require(result.decode_metadata.decoder_id == "ppm_image_decoder", "short ppm texture failure reports decoder id");
    require(
        result.decode_metadata.encoded_byte_count == make_short_ppm_2x1_encoded_bytes().size(),
        "short ppm texture failure reports encoded byte count");
    require(!result.decode_metadata.has_image(), "short ppm texture failure reports no decoded image metadata");
}

void test_texture_cache_uses_source_specific_placeholder_bytes()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source one_pixel_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/one.ppm"})
                                                        .source;
    const render_resolved_image_source two_pixel_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/two.ppm"})
                                                        .source;
    ppm_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    cache.set_placeholder_encoded_bytes({});
    cache.set_placeholder_encoded_bytes_for_source(one_pixel_source.cache_key(), make_ppm_1x1_encoded_bytes());
    cache.set_placeholder_encoded_bytes_for_source(two_pixel_source.cache_key(), make_ppm_2x1_encoded_bytes());

    const render_image_texture_result one_pixel = cache.acquire_texture(
        render_image_texture_request{.source = one_pixel_source, .sampler = render_image_sampler_policy{}});
    require(one_pixel.ok(), "source-specific one-pixel ppm creates a texture");
    require(one_pixel.texture.width == 1, "source-specific one-pixel ppm preserves width");
    require(one_pixel.texture.height == 1, "source-specific one-pixel ppm preserves height");
    require(one_pixel.decode_metadata.width == 1, "source-specific one-pixel ppm reports metadata width");
    require(one_pixel.decode_metadata.decoder_id == "ppm_image_decoder", "source-specific one-pixel ppm reports decoder id");

    const render_image_texture_result two_pixel = cache.acquire_texture(
        render_image_texture_request{.source = two_pixel_source, .sampler = render_image_sampler_policy{}});
    require(two_pixel.ok(), "source-specific two-pixel ppm creates a texture");
    require(two_pixel.texture.width == 2, "source-specific two-pixel ppm preserves width");
    require(two_pixel.texture.height == 1, "source-specific two-pixel ppm preserves height");
    require(two_pixel.decode_metadata.width == 2, "source-specific two-pixel ppm reports metadata width");
    require(two_pixel.decode_metadata.decoder_id == "ppm_image_decoder", "source-specific two-pixel ppm reports decoder id");
    require(two_pixel.texture.id != one_pixel.texture.id, "source-specific byte fixtures keep cache entries separate");
}

void test_texture_cache_uses_decoder_chain_for_source_specific_formats()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source fake_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/chain.fake"})
                                                     .source;
    const render_resolved_image_source ppm_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/chain.ppm"})
                                                    .source;

    fake_image_decoder fake_decoder;
    ppm_image_decoder ppm_decoder;
    image_decoder_chain decoder_chain({std::cref(fake_decoder), std::cref(ppm_decoder)});
    fake_image_texture_cache cache(decoder_chain);
    cache.set_placeholder_encoded_bytes_for_source(fake_source.cache_key(), {std::byte{0x01}});
    cache.set_placeholder_encoded_bytes_for_source(ppm_source.cache_key(), make_ppm_1x1_encoded_bytes());

    const render_image_texture_result fake_texture = cache.acquire_texture(
        render_image_texture_request{.source = fake_source, .sampler = render_image_sampler_policy{}});
    require(fake_texture.ok(), "decoder chain fake source creates a texture");
    require(fake_texture.texture.width == 2, "decoder chain fake source uses fake decoder dimensions");
    require(fake_texture.decode_metadata.decoder_id == "fake_image_decoder", "decoder chain fake texture reports decoder id");
    require(fake_texture.decoder_diagnostics.size() == 1, "decoder chain fake texture propagates chain diagnostic");
    require(fake_texture.decoder_diagnostics[0].supported, "decoder chain fake texture records selected decoder");

    const render_image_texture_result ppm_texture = cache.acquire_texture(
        render_image_texture_request{.source = ppm_source, .sampler = render_image_sampler_policy{}});
    require(ppm_texture.ok(), "decoder chain ppm source creates a texture");
    require(ppm_texture.texture.width == 1, "decoder chain ppm source uses ppm decoder dimensions");
    require(ppm_texture.decode_metadata.decoder_id == "ppm_image_decoder", "decoder chain ppm texture reports decoder id");
    require(ppm_texture.decoder_diagnostics.size() == 2, "decoder chain ppm texture propagates chain diagnostics");
    require(!ppm_texture.decoder_diagnostics[0].supported, "decoder chain ppm texture records skipped fake decoder");
    require(ppm_texture.decoder_diagnostics[1].supported, "decoder chain ppm texture records selected ppm decoder");
    require(ppm_texture.texture.id != fake_texture.texture.id, "decoder chain textures remain keyed by source");
}

void test_texture_cache_rejects_malformed_decoded_payload()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/malformed.fake"})
                                                    .source;
    malformed_payload_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_texture_request request{.source = source, .sampler = render_image_sampler_policy{}};
    const render_image_texture_result first = cache.acquire_texture(request);
    require(!first.ok(), "malformed decoded payload does not create a texture");
    require(
        first.status == render_image_texture_status::upload_failed,
        "malformed decoded payload reports upload failure");
    require(!first.texture.valid(), "malformed decoded payload returns no texture handle");
    require(!first.diagnostic.empty(), "malformed decoded payload includes diagnostic");
    require(decoder.support_request_count == 1, "malformed payload reached support check");
    require(decoder.decode_request_count == 1, "malformed payload reached decode");

    const render_decoded_image malformed_image{
        .width = 2,
        .height = 2,
        .pixel_format = render_image_pixel_format::rgba8_srgb,
        .pixels = {std::byte{0xff}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff}},
    };
    require(expected_render_decoded_image_byte_count(malformed_image) == 16, "expected byte count includes all pixels");
    require(!has_valid_render_decoded_image_payload(malformed_image), "malformed payload helper rejects short bytes");

    const render_image_texture_result second = cache.acquire_texture(request);
    require(!second.ok(), "malformed decoded payload remains uncached");
    require(!second.cache_hit, "malformed decoded payload is not a cache hit");
    require(decoder.support_request_count == 2, "malformed payload retry reaches support check");
    require(decoder.decode_request_count == 2, "malformed payload retry decodes again");
}

void test_texture_cache_rejects_unknown_pixel_format()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/unknown-format.fake"})
                                                    .source;
    unknown_pixel_format_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_decoded_image unknown_format_image{
        .width = 1,
        .height = 1,
        .pixel_format = static_cast<render_image_pixel_format>(99),
        .pixels = {std::byte{0xff}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff}},
    };
    require(
        render_image_pixel_format_byte_count(unknown_format_image.pixel_format) == 0,
        "unknown pixel format has no byte count");
    require(
        expected_render_decoded_image_byte_count(unknown_format_image) == 0,
        "unknown pixel format has no expected payload size");
    require(!has_valid_render_decoded_image_payload(unknown_format_image), "unknown pixel format is invalid");

    const render_image_texture_result result = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = render_image_sampler_policy{}});
    require(!result.ok(), "unknown pixel format does not create a texture");
    require(result.status == render_image_texture_status::upload_failed, "unknown pixel format reports upload failure");
    require(!result.texture.valid(), "unknown pixel format returns no texture handle");
    require(!result.diagnostic.empty(), "unknown pixel format includes diagnostic");
    require(decoder.support_request_count == 1, "unknown pixel format reached support check");
    require(decoder.decode_request_count == 1, "unknown pixel format reached decode");
}

} // namespace

int main()
{
    test_sampler_defaults_are_appended_to_render_image_ref();
    test_resolver_normalizes_without_fetching();
    test_decoder_interface_shape();
    test_fake_decoder_reports_format_detection_and_placeholder_fallbacks();
    test_decoder_size_validation_reports_stride_and_invalid_payloads();
    test_decoder_reports_explicit_failures();
    test_ppm_decoder_decodes_binary_rgb_to_rgba();
    test_ppm_decoder_reports_invalid_payload_size();
    test_decoder_chain_routes_supported_formats();
    test_decoder_chain_reports_unsupported_sources();
    test_decoder_chain_reports_candidate_decode_failures();
    test_source_bytes_loader_returns_registered_local_bytes();
    test_source_bytes_loader_reports_explicit_failures();
    test_source_bytes_loader_loads_data_uri_bytes();
    test_source_bytes_loader_rejects_malformed_data_uris();
    test_texture_cache_can_use_loaded_source_bytes();
    test_texture_uploader_uploads_valid_decoded_image();
    test_texture_uploader_reports_deterministic_queue_lifecycle();
    test_texture_uploader_reports_retry_eligibility_and_backoff();
    test_texture_uploader_rejects_invalid_inputs();
    test_texture_cache_uses_injected_texture_uploader();
    test_texture_cache_placeholder_policy_reuses_decode_failure_texture();
    test_texture_cache_placeholder_policy_covers_failure_reasons();
    test_texture_cache_placeholder_policy_disabled_preserves_failures();
    test_texture_pipeline_resolves_loads_decodes_and_uploads_image_ref();
    test_texture_pipeline_reports_source_load_failure();
    test_texture_pipeline_reports_decode_failure();
    test_texture_pipeline_reuses_cache_hits();
    test_sampler_policy_validation_rejects_unknown_enum_values();
    test_texture_cache_reuses_matching_key_and_misses_on_sampler_change();
    test_texture_cache_keys_include_all_sampler_policy_fields();
    test_sampler_cache_policy_diagnostics_report_choices_and_stable_keys();
    test_texture_cache_snapshot_reports_policy_readiness_and_placeholder_fallback();
    test_texture_cache_reuses_normalized_equivalent_cache_keys();
    test_texture_cache_rejects_invalid_sampler_policy_before_decode();
    test_texture_cache_diagnostic_snapshot_reports_entries_and_recency();
    test_texture_cache_invalidates_by_texture_key_and_source();
    test_texture_cache_reports_explicit_failures();
    test_texture_cache_propagates_decoder_failures();
    test_texture_cache_uses_ppm_decoder_placeholder_bytes();
    test_texture_cache_propagates_ppm_decoder_payload_failure();
    test_texture_cache_uses_source_specific_placeholder_bytes();
    test_texture_cache_uses_decoder_chain_for_source_specific_formats();
    test_texture_cache_rejects_malformed_decoded_payload();
    test_texture_cache_rejects_unknown_pixel_format();
    return 0;
}
