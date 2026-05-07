#include "render/image/image_decoder.h"

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

void append_u32_be(std::vector<std::byte>& bytes, std::uint32_t value)
{
    append_byte(bytes, static_cast<unsigned char>((value >> 24) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 16) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 8) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>(value & 0xffu));
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

std::vector<std::byte> make_bytes(std::initializer_list<unsigned char> values)
{
    std::vector<std::byte> bytes;
    bytes.reserve(values.size());
    for (const unsigned char value : values) {
        append_byte(bytes, value);
    }
    return bytes;
}

std::vector<std::byte> make_ihdr_data(
    std::uint32_t width = 2,
    std::uint32_t height = 2,
    unsigned char bit_depth = 8,
    unsigned char color_type = 6)
{
    std::vector<std::byte> data;
    append_u32_be(data, width);
    append_u32_be(data, height);
    append_byte(data, bit_depth);
    append_byte(data, color_type);
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

std::vector<std::byte> make_png_bytes(
    std::vector<std::byte> idat_bytes = make_bytes({0x78, 0x9c, 0x03}),
    unsigned char bit_depth = 8,
    unsigned char color_type = 6)
{
    std::vector<std::byte> bytes;
    append_png_signature(bytes);
    append_png_chunk(bytes, "IHDR", make_ihdr_data(2, 2, bit_depth, color_type));
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

std::vector<std::byte> make_scanlines_with_filter(unsigned char filter_type)
{
    std::vector<std::byte> bytes = make_filter_none_scanlines();
    bytes[0] = std::byte{filter_type};
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

quiz_vulkan::render::fake_png_image_inflater make_successful_inflater()
{
    return quiz_vulkan::render::fake_png_image_inflater(quiz_vulkan::render::png_image_inflate_result{
        .status = quiz_vulkan::render::png_image_inflate_status::inflated,
        .inflated_bytes = make_filter_none_scanlines(),
        .diagnostic = "synthetic filter-none rows",
    });
}

void test_png_decoder_decodes_rgba8_with_fake_inflater()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> idat = make_bytes({0x78, 0x9c, 0x63, 0x60});
    const render_image_decode_request request = make_decode_request("textures/card.png", make_png_bytes(idat));
    const fake_png_image_inflater inflater(png_image_inflate_result{
        .status = png_image_inflate_status::inflated,
        .inflated_bytes = make_filter_none_scanlines(),
        .diagnostic = "synthetic filter-none rows",
    });
    const png_image_decoder decoder(inflater);
    const render_image_decode_result result = decoder.decode(request);

    require(decoder.supports(request), "PNG decoder supports .png requests");
    require(result.ok(), "PNG decoder decodes RGBA8 bytes with fake inflater");
    require(result.status == render_image_decode_status::decoded, "PNG decoder reports decoded status");
    require(result.metadata.decoder_id == "png_image_decoder", "PNG decoder metadata records decoder id");
    require(result.metadata.width == 2, "PNG decoder metadata records width");
    require(result.metadata.height == 2, "PNG decoder metadata records height");
    require(result.metadata.decoded_byte_count == 16, "PNG decoder metadata records decoded bytes");
    require(result.metadata.size_validation.valid, "PNG decoder metadata validates decoded size");
    require(
        result.metadata.format_detection.detected_format == render_image_encoded_format::png,
        "PNG decoder metadata preserves PNG format detection");
    require(result.image.width == 2, "PNG decoder output width is preserved");
    require(result.image.height == 2, "PNG decoder output height is preserved");
    require(result.image.pixel_format == render_image_pixel_format::rgba8_srgb, "PNG decoder emits RGBA8 SRGB");
    require(
        result.image.pixels == make_bytes({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}),
        "PNG decoder drops filter bytes and preserves RGBA payload");
    require(inflater.requests.size() == 1, "PNG decoder asks fake inflater once");
    require(inflater.requests[0].compressed_bytes == idat, "PNG decoder forwards concatenated IDAT bytes");
    require(inflater.requests[0].expected_inflated_byte_count == 18, "PNG decoder forwards expected row bytes");
}

void test_png_decoder_supports_png_signature_without_extension()
{
    using namespace quiz_vulkan::render;

    const render_image_decode_request request = make_decode_request("textures/card.bin", make_png_bytes());
    const fake_png_image_inflater inflater = make_successful_inflater();
    const png_image_decoder decoder(inflater);
    const render_image_decode_result result = decoder.decode(request);

    require(decoder.supports(request), "PNG decoder supports PNG signature bytes without extension");
    require(result.ok(), "PNG decoder decodes signature-detected PNG bytes");
    require(
        result.metadata.format_detection.recognized_signature,
        "PNG decoder metadata records recognized signature");
}

void test_png_decoder_reports_inflater_unavailable()
{
    using namespace quiz_vulkan::render;

    const render_image_decode_request request = make_decode_request("textures/card.png", make_png_bytes());
    const png_image_decoder decoder;
    const render_image_decode_result result = decoder.decode(request);

    require(!result.ok(), "PNG decoder without inflater does not decode");
    require(
        result.status == render_image_decode_status::unsupported_format,
        "PNG decoder without inflater reports unsupported format");
    require(result.image.empty(), "PNG decoder without inflater returns no image");
    require(
        result.diagnostic.find("inflater_unavailable") != std::string::npos,
        "PNG decoder unavailable inflater diagnostic includes boundary status");
    require(
        result.diagnostic.find("IDAT inflater backend") != std::string::npos,
        "PNG decoder unavailable inflater diagnostic names inflater backend");
}

void test_png_decoder_preserves_inflater_failure()
{
    using namespace quiz_vulkan::render;

    const render_image_decode_request request = make_decode_request("textures/card.png", make_png_bytes());
    const fake_png_image_inflater inflater(png_image_inflate_result{
        .status = png_image_inflate_status::failed,
        .inflated_bytes = {},
        .diagnostic = "synthetic inflate failure",
    });
    const png_image_decoder decoder(inflater);
    const render_image_decode_result result = decoder.decode(request);

    require(!result.ok(), "PNG decoder reports fake inflater failure");
    require(
        result.status == render_image_decode_status::invalid_data,
        "PNG decoder maps inflater failure to invalid data");
    require(
        result.diagnostic.find("inflater_failed") != std::string::npos,
        "PNG decoder inflater failure diagnostic includes boundary status");
}

void test_png_decoder_reports_unsupported_color_type_and_bit_depth()
{
    using namespace quiz_vulkan::render;

    const render_image_decode_request truecolor =
        make_decode_request("textures/card.png", make_png_bytes(make_bytes({0x01}), 8, 2));
    const render_image_decode_request rgba16 =
        make_decode_request("textures/card.png", make_png_bytes(make_bytes({0x01}), 16, 6));
    const fake_png_image_inflater inflater = make_successful_inflater();
    const png_image_decoder decoder(inflater);
    const render_image_decode_result truecolor_result = decoder.decode(truecolor);
    const render_image_decode_result rgba16_result = decoder.decode(rgba16);

    require(!truecolor_result.ok(), "PNG decoder rejects non-RGBA color type");
    require(
        truecolor_result.status == render_image_decode_status::unsupported_format,
        "PNG decoder maps non-RGBA color type to unsupported format");
    require(
        truecolor_result.diagnostic.find("unsupported_color_type") != std::string::npos,
        "PNG decoder non-RGBA diagnostic includes plan status");
    require(!rgba16_result.ok(), "PNG decoder rejects non-8-bit RGBA");
    require(
        rgba16_result.status == render_image_decode_status::unsupported_format,
        "PNG decoder maps non-8-bit RGBA to unsupported format");
    require(
        rgba16_result.diagnostic.find("unsupported_bit_depth") != std::string::npos,
        "PNG decoder non-8-bit diagnostic includes plan status");
    require(inflater.requests.empty(), "PNG decoder does not call inflater for unsupported decode plans");
}

void test_png_decoder_reports_unsupported_filter_type()
{
    using namespace quiz_vulkan::render;

    const render_image_decode_request request = make_decode_request("textures/card.png", make_png_bytes());
    const fake_png_image_inflater inflater(png_image_inflate_result{
        .status = png_image_inflate_status::inflated,
        .inflated_bytes = make_scanlines_with_filter(2),
        .diagnostic = "synthetic unsupported filter",
    });
    const png_image_decoder decoder(inflater);
    const render_image_decode_result result = decoder.decode(request);

    require(!result.ok(), "PNG decoder rejects unsupported filter type");
    require(
        result.status == render_image_decode_status::unsupported_format,
        "PNG decoder maps unsupported filter to unsupported format");
    require(
        result.diagnostic.find("unsupported_filter_type") != std::string::npos,
        "PNG decoder unsupported filter diagnostic includes unfilter status");
    require(
        result.diagnostic.find("filter type 0") != std::string::npos,
        "PNG decoder unsupported filter diagnostic names supported filter");
}

void test_png_decoder_reports_truncated_scanline_data()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> inflated = make_filter_none_scanlines();
    inflated.resize(inflated.size() - 1);
    const render_image_decode_request request = make_decode_request("textures/card.png", make_png_bytes());
    const fake_png_image_inflater inflater(png_image_inflate_result{
        .status = png_image_inflate_status::inflated,
        .inflated_bytes = std::move(inflated),
        .diagnostic = "synthetic truncated rows",
    });
    const png_image_decoder decoder(inflater);
    const render_image_decode_result result = decoder.decode(request);

    require(!result.ok(), "PNG decoder rejects truncated scanline rows");
    require(
        result.status == render_image_decode_status::invalid_data,
        "PNG decoder maps truncated rows to invalid data");
    require(
        result.diagnostic.find("truncated_scanline_data") != std::string::npos,
        "PNG decoder truncated row diagnostic includes unfilter status");
}

void test_png_decoder_reports_row_stride_mismatch()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> inflated = make_filter_none_scanlines();
    append_byte(inflated, 0xff);
    const render_image_decode_request request = make_decode_request("textures/card.png", make_png_bytes());
    const fake_png_image_inflater inflater(png_image_inflate_result{
        .status = png_image_inflate_status::inflated,
        .inflated_bytes = std::move(inflated),
        .diagnostic = "synthetic extra row byte",
    });
    const png_image_decoder decoder(inflater);
    const render_image_decode_result result = decoder.decode(request);

    require(!result.ok(), "PNG decoder rejects row byte mismatch");
    require(
        result.status == render_image_decode_status::invalid_data,
        "PNG decoder maps row mismatch to invalid data");
    require(
        result.diagnostic.find("row_stride_mismatch") != std::string::npos,
        "PNG decoder row mismatch diagnostic includes unfilter status");
}

void test_png_decoder_reports_bad_signature_for_png_source()
{
    using namespace quiz_vulkan::render;

    const render_image_decode_request request =
        make_decode_request("textures/card.png", make_bytes({'n', 'o', 't', 'p', 'n', 'g'}));
    const fake_png_image_inflater inflater = make_successful_inflater();
    const png_image_decoder decoder(inflater);
    const render_image_decode_result result = decoder.decode(request);

    require(decoder.supports(request), "PNG decoder supports .png request before validating bytes");
    require(!result.ok(), "PNG decoder rejects invalid PNG signature bytes");
    require(
        result.status == render_image_decode_status::invalid_data,
        "PNG decoder maps bad PNG signature to invalid data");
    require(
        result.diagnostic.find("missing_signature") != std::string::npos,
        "PNG decoder bad signature diagnostic includes chunk scan status");
}

void test_png_decoder_chain_selects_png_decoder()
{
    using namespace quiz_vulkan::render;

    const render_image_decode_request request = make_decode_request("textures/card.png", make_png_bytes());
    const ppm_image_decoder ppm_decoder;
    const fake_png_image_inflater inflater = make_successful_inflater();
    const png_image_decoder png_decoder(inflater);
    image_decoder_chain chain;
    chain.add_decoder("ppm_image_decoder", ppm_decoder);
    chain.add_decoder("png_image_decoder", png_decoder);

    const render_image_decode_result result = chain.decode(request);

    require(result.ok(), "decoder chain decodes PNG when PNG decoder is registered");
    require(result.metadata.decoder_id == "png_image_decoder", "decoder chain selects PNG decoder");
    require(result.decoder_diagnostics.size() == 2, "decoder chain records PPM and PNG candidates");
    require(
        result.decoder_diagnostics[0].decoder_id == "ppm_image_decoder",
        "decoder chain records PPM candidate first");
    require(!result.decoder_diagnostics[0].supported, "decoder chain skips unsupported PPM candidate");
    require(
        result.decoder_diagnostics[1].decoder_id == "png_image_decoder",
        "decoder chain records PNG candidate second");
    require(result.decoder_diagnostics[1].supported, "decoder chain marks PNG candidate supported");
    require(result.decoder_diagnostics[1].decode_attempted, "decoder chain attempts PNG candidate");
    require(result.decoder_diagnostics[1].terminal_candidate, "decoder chain terminates at PNG candidate");
}

void test_png_decoder_reports_empty_and_unsupported_requests()
{
    using namespace quiz_vulkan::render;

    const fake_png_image_inflater inflater = make_successful_inflater();
    const png_image_decoder decoder(inflater);
    const render_image_decode_result empty =
        decoder.decode(make_decode_request("textures/card.png", std::vector<std::byte>{}));
    const render_image_decode_result unsupported =
        decoder.decode(make_decode_request("textures/card.bin", make_bytes({'n', 'o', 'p', 'e'})));

    require(empty.status == render_image_decode_status::empty_input, "PNG decoder reports empty input");
    require(
        empty.diagnostic.find("encoded bytes") != std::string::npos,
        "PNG decoder empty input diagnostic is deterministic");
    require(
        unsupported.status == render_image_decode_status::unsupported_format,
        "PNG decoder rejects non-PNG requests");
    require(
        unsupported.diagnostic.find(".png sources or PNG signature") != std::string::npos,
        "PNG decoder unsupported request diagnostic is deterministic");
}

} // namespace

int main()
{
    test_png_decoder_decodes_rgba8_with_fake_inflater();
    test_png_decoder_supports_png_signature_without_extension();
    test_png_decoder_reports_inflater_unavailable();
    test_png_decoder_preserves_inflater_failure();
    test_png_decoder_reports_unsupported_color_type_and_bit_depth();
    test_png_decoder_reports_unsupported_filter_type();
    test_png_decoder_reports_truncated_scanline_data();
    test_png_decoder_reports_row_stride_mismatch();
    test_png_decoder_reports_bad_signature_for_png_source();
    test_png_decoder_chain_selects_png_decoder();
    test_png_decoder_reports_empty_and_unsupported_requests();
    return 0;
}
