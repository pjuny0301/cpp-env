#include "render/image/image_decoder.h"
#include "render/image/third_party_image_decoder_adapter.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

void append_byte(std::vector<std::byte>& bytes, unsigned char value)
{
    bytes.push_back(std::byte{value});
}

void append_u16_le(std::vector<std::byte>& bytes, std::uint16_t value)
{
    append_byte(bytes, static_cast<unsigned char>(value & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 8) & 0xffu));
}

void append_u32_le(std::vector<std::byte>& bytes, std::uint32_t value)
{
    append_byte(bytes, static_cast<unsigned char>(value & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 8) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 16) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 24) & 0xffu));
}

void append_i32_le(std::vector<std::byte>& bytes, std::int32_t value)
{
    append_u32_le(bytes, static_cast<std::uint32_t>(value));
}

void append_u32_be(std::vector<std::byte>& bytes, std::uint32_t value)
{
    append_byte(bytes, static_cast<unsigned char>((value >> 24) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 16) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 8) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>(value & 0xffu));
}

std::vector<std::byte> make_bytes(std::initializer_list<unsigned char> values)
{
    std::vector<std::byte> bytes;
    bytes.reserve(values.size());
    for (const unsigned char value : values) {
        append_byte(bytes, value);
    }
    return bytes;
}

std::uint32_t adler32(const std::vector<std::byte>& bytes)
{
    constexpr std::uint32_t adler_modulus = 65521u;
    std::uint32_t a = 1u;
    std::uint32_t b = 0u;
    for (std::byte value : bytes) {
        a = (a + std::to_integer<std::uint8_t>(value)) % adler_modulus;
        b = (b + a) % adler_modulus;
    }
    return (b << 16) | a;
}

std::vector<std::byte> make_bmp_bytes()
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 'B');
    append_byte(bytes, 'M');
    append_u32_le(bytes, 58);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u32_le(bytes, 54);
    append_u32_le(bytes, 40);
    append_i32_le(bytes, 1);
    append_i32_le(bytes, 1);
    append_u16_le(bytes, 1);
    append_u16_le(bytes, 24);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, 4);
    append_i32_le(bytes, 0);
    append_i32_le(bytes, 0);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, 0);
    append_byte(bytes, 3);
    append_byte(bytes, 2);
    append_byte(bytes, 1);
    append_byte(bytes, 0);
    return bytes;
}

std::vector<std::byte> make_ppm_bytes()
{
    std::vector<std::byte> bytes;
    for (const char value : std::string_view{"P6\n1 1\n255\n"}) {
        append_byte(bytes, static_cast<unsigned char>(value));
    }
    append_byte(bytes, 10);
    append_byte(bytes, 20);
    append_byte(bytes, 30);
    return bytes;
}

void append_png_signature(std::vector<std::byte>& bytes)
{
    append_byte(bytes, 0x89);
    append_byte(bytes, 'P');
    append_byte(bytes, 'N');
    append_byte(bytes, 'G');
    append_byte(bytes, '\r');
    append_byte(bytes, '\n');
    append_byte(bytes, 0x1a);
    append_byte(bytes, '\n');
}

std::vector<std::byte> make_ihdr_data()
{
    std::vector<std::byte> data;
    append_u32_be(data, 2);
    append_u32_be(data, 2);
    append_byte(data, 8);
    append_byte(data, 6);
    append_byte(data, 0);
    append_byte(data, 0);
    append_byte(data, 0);
    return data;
}

void append_png_chunk(
    std::vector<std::byte>& bytes,
    std::string_view type_code,
    const std::vector<std::byte>& data)
{
    append_u32_be(bytes, static_cast<std::uint32_t>(data.size()));
    for (char value : type_code) {
        append_byte(bytes, static_cast<unsigned char>(value));
    }
    bytes.insert(bytes.end(), data.begin(), data.end());
    append_u32_be(bytes, 0);
}

std::vector<std::byte> make_png_bytes(std::vector<std::byte> idat_bytes)
{
    std::vector<std::byte> bytes;
    append_png_signature(bytes);
    append_png_chunk(bytes, "IHDR", make_ihdr_data());
    append_png_chunk(bytes, "IDAT", idat_bytes);
    append_png_chunk(bytes, "IEND", std::vector<std::byte>{});
    return bytes;
}

std::vector<std::byte> make_filter_none_scanlines()
{
    std::vector<std::byte> bytes;
    const std::vector<std::byte> first_row = make_bytes({1, 2, 3, 4, 5, 6, 7, 8});
    const std::vector<std::byte> second_row = make_bytes({9, 10, 11, 12, 13, 14, 15, 16});
    append_byte(bytes, 0);
    bytes.insert(bytes.end(), first_row.begin(), first_row.end());
    append_byte(bytes, 0);
    bytes.insert(bytes.end(), second_row.begin(), second_row.end());
    return bytes;
}

std::vector<std::byte> make_zlib_stored_stream(const std::vector<std::byte>& payload)
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 0x78);
    append_byte(bytes, 0x01);
    append_byte(bytes, 0x01);
    const std::uint16_t len = static_cast<std::uint16_t>(payload.size());
    append_u16_le(bytes, len);
    append_u16_le(bytes, static_cast<std::uint16_t>(len ^ 0xffffu));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    append_u32_be(bytes, adler32(payload));
    return bytes;
}

std::vector<std::byte> make_zlib_huffman_stream()
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 0x78);
    append_byte(bytes, 0x01);
    append_byte(bytes, 0x03);
    append_u32_be(bytes, adler32(std::vector<std::byte>{}));
    return bytes;
}

quiz_vulkan::render::render_image_decode_request make_decode_request(
    std::string normalized_uri,
    std::vector<std::byte> encoded_bytes)
{
    return quiz_vulkan::render::render_image_decode_request{
        .source = quiz_vulkan::render::render_resolved_image_source{
            .original_uri = normalized_uri,
            .normalized_uri = std::move(normalized_uri),
            .kind = quiz_vulkan::render::render_image_source_kind::local_path,
        },
        .encoded_bytes = std::move(encoded_bytes),
    };
}

void require_candidate(
    const quiz_vulkan::render::render_image_decoder_capability_manifest& manifest,
    std::size_t index,
    quiz_vulkan::render::render_image_decoder_capability_candidate_kind kind,
    quiz_vulkan::render::render_image_decoder_capability_candidate_status status,
    std::string_view decoder_id,
    bool terminal)
{
    require(index < manifest.candidates.size(), "capability manifest candidate index exists");
    const quiz_vulkan::render::render_image_decoder_capability_candidate_snapshot& candidate =
        manifest.candidates[index];
    require(candidate.candidate_index == index, "capability manifest candidate records stable index");
    require(candidate.candidate_order == index + 1, "capability manifest candidate records stable order");
    require(candidate.kind == kind, "capability manifest candidate records kind");
    require(candidate.kind_name == quiz_vulkan::render::render_image_decoder_capability_candidate_kind_name(kind),
        "capability manifest candidate records kind name");
    require(candidate.status == status, "capability manifest candidate records status");
    require(candidate.status_name == quiz_vulkan::render::render_image_decoder_capability_candidate_status_name(status),
        "capability manifest candidate records status name");
    require(candidate.decoder_id == decoder_id, "capability manifest candidate records decoder id");
    require(candidate.terminal_candidate == terminal, "capability manifest candidate records terminal flag");
}

void test_standard_chain_decodes_bmp_first()
{
    using namespace quiz_vulkan::render;

    const standard_image_decoder_chain decoder;
    const render_image_decode_result result =
        decoder.decode(make_decode_request("textures/card.bmp", make_bmp_bytes()));

    require(decoder.decoder_count() == 3, "standard decoder chain registers three decoders");
    require(result.ok(), "standard decoder chain decodes BMP");
    require(result.metadata.decoder_id == "bmp_image_decoder", "standard decoder chain selects BMP decoder");
    require(result.decoder_diagnostics.size() == 1, "BMP decode stops at first standard candidate");
    require(result.decoder_diagnostics[0].decoder_id == "bmp_image_decoder", "BMP diagnostic records decoder id");
    require(result.decoder_diagnostics[0].supported, "BMP diagnostic records supported candidate");
    require(result.decoder_diagnostics[0].decode_attempted, "BMP diagnostic records decode attempt");
    require(
        result.image.pixels == make_bytes({1, 2, 3, 0xff}),
        "standard decoder chain preserves BMP RGBA pixels");

    const render_image_decoder_capability_manifest manifest =
        make_render_image_decoder_capability_manifest(
            make_decode_request("textures/card.bmp", make_bmp_bytes()),
            result);
    require(manifest.candidates.size() == 1, "BMP manifest records one candidate");
    require(!manifest.used_third_party_adapter, "BMP manifest does not use third-party adapter");
    require(!manifest.fallback_used, "BMP manifest does not use fallback");
    require(manifest.decoded, "BMP manifest records decoded result");
    require_candidate(
        manifest,
        0,
        render_image_decoder_capability_candidate_kind::bmp,
        render_image_decoder_capability_candidate_status::decoded,
        "bmp_image_decoder",
        true);
    require(manifest.terminal_decoder_id == "bmp_image_decoder", "BMP manifest terminal decoder is BMP");
    require(manifest.terminal_kind == render_image_decoder_capability_candidate_kind::bmp, "BMP manifest terminal kind is BMP");
}

void test_standard_chain_decodes_ppm_after_bmp_candidate()
{
    using namespace quiz_vulkan::render;

    const standard_image_decoder_chain decoder;
    const render_image_decode_result result =
        decoder.decode(make_decode_request("textures/card.ppm", make_ppm_bytes()));

    require(result.ok(), "standard decoder chain decodes PPM");
    require(result.metadata.decoder_id == "ppm_image_decoder", "standard decoder chain selects PPM decoder");
    require(result.decoder_diagnostics.size() == 2, "PPM decode records BMP and PPM candidates");
    require(result.decoder_diagnostics[0].decoder_id == "bmp_image_decoder", "PPM diagnostic records BMP first");
    require(!result.decoder_diagnostics[0].supported, "PPM diagnostic records unsupported BMP");
    require(result.decoder_diagnostics[1].decoder_id == "ppm_image_decoder", "PPM diagnostic records PPM second");
    require(result.decoder_diagnostics[1].supported, "PPM diagnostic records supported PPM");
    require(
        result.image.pixels == make_bytes({10, 20, 30, 0xff}),
        "standard decoder chain preserves PPM RGBA pixels");

    const render_image_decoder_capability_manifest manifest =
        make_render_image_decoder_capability_manifest(
            make_decode_request("textures/card.ppm", make_ppm_bytes()),
            result);
    require(manifest.candidates.size() == 2, "PPM manifest records BMP and PPM candidates");
    require_candidate(
        manifest,
        0,
        render_image_decoder_capability_candidate_kind::bmp,
        render_image_decoder_capability_candidate_status::unsupported_format,
        "bmp_image_decoder",
        false);
    require_candidate(
        manifest,
        1,
        render_image_decoder_capability_candidate_kind::ppm,
        render_image_decoder_capability_candidate_status::decoded,
        "ppm_image_decoder",
        true);
    require(manifest.terminal_decoder_id == "ppm_image_decoder", "PPM manifest terminal decoder is PPM");
    require(manifest.terminal_kind == render_image_decoder_capability_candidate_kind::ppm, "PPM manifest terminal kind is PPM");
}

void test_standard_chain_decodes_zlib_stored_png()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> scanlines = make_filter_none_scanlines();
    const standard_image_decoder_chain decoder;
    const render_image_decode_result result =
        decoder.decode(make_decode_request("textures/card.png", make_png_bytes(make_zlib_stored_stream(scanlines))));

    require(decoder.supports(make_decode_request("textures/card.png", make_png_bytes(make_zlib_stored_stream(scanlines)))),
        "standard decoder chain supports PNG sources");
    require(result.ok(), "standard decoder chain decodes zlib-stored PNG");
    require(result.metadata.decoder_id == "png_image_decoder", "standard decoder chain selects PNG decoder");
    require(result.metadata.size_validation.valid, "standard decoder chain validates PNG decoded size");
    require(result.decoder_diagnostics.size() == 3, "PNG decode records BMP, PPM, and PNG candidates");
    require(result.decoder_diagnostics[0].decoder_id == "bmp_image_decoder", "PNG diagnostic records BMP first");
    require(result.decoder_diagnostics[1].decoder_id == "ppm_image_decoder", "PNG diagnostic records PPM second");
    require(result.decoder_diagnostics[2].decoder_id == "png_image_decoder", "PNG diagnostic records PNG third");
    require(result.decoder_diagnostics[2].supported, "PNG diagnostic records supported PNG");
    require(result.decoder_diagnostics[2].decode_attempted, "PNG diagnostic records decode attempt");
    require(result.decoder_diagnostics[2].terminal_candidate, "PNG diagnostic records terminal PNG candidate");
    require(
        result.image.pixels == make_bytes({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}),
        "standard decoder chain unfilters zlib-stored PNG rows");

    const render_image_decoder_capability_manifest manifest =
        make_render_image_decoder_capability_manifest(
            make_decode_request("textures/card.png", make_png_bytes(make_zlib_stored_stream(scanlines))),
            result);
    require(manifest.candidates.size() == 3, "PNG manifest records BMP, PPM, and PNG candidates");
    require_candidate(
        manifest,
        0,
        render_image_decoder_capability_candidate_kind::bmp,
        render_image_decoder_capability_candidate_status::unsupported_format,
        "bmp_image_decoder",
        false);
    require_candidate(
        manifest,
        1,
        render_image_decoder_capability_candidate_kind::ppm,
        render_image_decoder_capability_candidate_status::unsupported_format,
        "ppm_image_decoder",
        false);
    require_candidate(
        manifest,
        2,
        render_image_decoder_capability_candidate_kind::png,
        render_image_decoder_capability_candidate_status::decoded,
        "png_image_decoder",
        true);
    require(manifest.terminal_decoder_id == "png_image_decoder", "PNG manifest terminal decoder is PNG");
    require(manifest.terminal_kind == render_image_decoder_capability_candidate_kind::png, "PNG manifest terminal kind is PNG");
}

void test_standard_chain_reports_png_inflate_failure_without_placeholder_pixels()
{
    using namespace quiz_vulkan::render;

    const standard_image_decoder_chain decoder;
    const render_image_decode_result result =
        decoder.decode(make_decode_request("textures/card.png", make_png_bytes(make_zlib_huffman_stream())));

    require(!result.ok(), "standard decoder chain rejects unsupported compressed PNG");
    require(
        result.status == render_image_decode_status::invalid_data,
        "unsupported PNG compression reports invalid data through standard chain");
    require(result.image.empty(), "unsupported PNG compression returns no placeholder image");
    require(result.decoder_diagnostics.size() == 3, "failed PNG records all standard candidates");
    require(result.decoder_diagnostics[2].decoder_id == "png_image_decoder", "failed PNG diagnostic records PNG");
    require(result.decoder_diagnostics[2].decode_attempted, "failed PNG diagnostic records PNG decode attempt");
    require(result.decoder_diagnostics[2].terminal_candidate, "failed PNG diagnostic records terminal candidate");
    require(
        result.decoder_diagnostics[2].decode_diagnostic.find("inflater_failed") != std::string::npos,
        "failed PNG diagnostic records deterministic inflater failure");
}

void test_standard_chain_reports_unsupported_source_without_placeholder_pixels()
{
    using namespace quiz_vulkan::render;

    const standard_image_decoder_chain decoder;
    const render_image_decode_result result =
        decoder.decode(make_decode_request("textures/card.jpg", make_bytes({0xff, 0xd8, 0xff, 0xd9})));

    require(!result.ok(), "standard decoder chain rejects unsupported JPEG");
    require(
        result.status == render_image_decode_status::unsupported_format,
        "unsupported standard source reports unsupported format");
    require(result.image.empty(), "unsupported standard source returns no placeholder image");
    require(result.decoder_diagnostics.size() == 3, "unsupported source records all standard candidates");
    require(result.decoder_diagnostics[2].terminal_candidate, "unsupported source terminates on final candidate");
    require(
        result.decoder_diagnostics[2].diagnostic == "decoder chain exhausted all candidates",
        "unsupported source diagnostic is deterministic");

    const render_image_decoder_capability_manifest manifest =
        make_render_image_decoder_capability_manifest(
            make_decode_request("textures/card.jpg", make_bytes({0xff, 0xd8, 0xff, 0xd9})),
            result);
    require(manifest.candidates.size() == 4, "unsupported manifest records standard candidates plus terminal");
    require_candidate(
        manifest,
        0,
        render_image_decoder_capability_candidate_kind::bmp,
        render_image_decoder_capability_candidate_status::unsupported_format,
        "bmp_image_decoder",
        false);
    require_candidate(
        manifest,
        1,
        render_image_decoder_capability_candidate_kind::ppm,
        render_image_decoder_capability_candidate_status::unsupported_format,
        "ppm_image_decoder",
        false);
    require_candidate(
        manifest,
        2,
        render_image_decoder_capability_candidate_kind::png,
        render_image_decoder_capability_candidate_status::unsupported_format,
        "png_image_decoder",
        false);
    require_candidate(
        manifest,
        3,
        render_image_decoder_capability_candidate_kind::unsupported_terminal,
        render_image_decoder_capability_candidate_status::unsupported_terminal,
        "unsupported_terminal",
        true);
    require(
        manifest.terminal_kind == render_image_decoder_capability_candidate_kind::unsupported_terminal,
        "unsupported manifest terminal kind is explicit");
    require(manifest.terminal_decoder_id == "unsupported_terminal", "unsupported manifest terminal id is explicit");
}

} // namespace

int main()
{
    test_standard_chain_decodes_bmp_first();
    test_standard_chain_decodes_ppm_after_bmp_candidate();
    test_standard_chain_decodes_zlib_stored_png();
    test_standard_chain_reports_png_inflate_failure_without_placeholder_pixels();
    test_standard_chain_reports_unsupported_source_without_placeholder_pixels();
    return 0;
}
