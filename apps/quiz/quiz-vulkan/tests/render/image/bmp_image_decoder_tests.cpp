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

std::vector<std::byte> make_bmp_bytes(
    std::int32_t width,
    std::int32_t height,
    std::uint16_t bits_per_pixel,
    const std::vector<std::byte>& pixel_data)
{
    constexpr std::uint32_t file_header_byte_count = 14;
    constexpr std::uint32_t info_header_byte_count = 40;
    constexpr std::uint32_t pixel_data_offset = file_header_byte_count + info_header_byte_count;
    const std::uint32_t file_size = pixel_data_offset + static_cast<std::uint32_t>(pixel_data.size());

    std::vector<std::byte> bytes;
    append_byte(bytes, 'B');
    append_byte(bytes, 'M');
    append_u32_le(bytes, file_size);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u32_le(bytes, pixel_data_offset);

    append_u32_le(bytes, info_header_byte_count);
    append_i32_le(bytes, width);
    append_i32_le(bytes, height);
    append_u16_le(bytes, 1);
    append_u16_le(bytes, bits_per_pixel);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, static_cast<std::uint32_t>(pixel_data.size()));
    append_i32_le(bytes, 0);
    append_i32_le(bytes, 0);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, 0);

    bytes.insert(bytes.end(), pixel_data.begin(), pixel_data.end());
    return bytes;
}

quiz_vulkan::render::render_image_decode_request make_decode_request(
    std::string_view uri,
    std::vector<std::byte> bytes)
{
    return quiz_vulkan::render::render_image_decode_request{
        .source = quiz_vulkan::render::render_resolved_image_source{
            .original_uri = std::string(uri),
            .normalized_uri = std::string(uri),
            .kind = quiz_vulkan::render::render_image_source_kind::local_path,
        },
        .encoded_bytes = std::move(bytes),
    };
}

std::vector<std::byte> make_expected_rgba(std::initializer_list<unsigned char> values)
{
    std::vector<std::byte> bytes;
    bytes.reserve(values.size());
    for (const unsigned char value : values) {
        append_byte(bytes, value);
    }
    return bytes;
}

std::vector<std::byte> make_bmp_24_bit_bottom_up_2x2()
{
    std::vector<std::byte> pixels;
    append_byte(pixels, 0xff);
    append_byte(pixels, 0x00);
    append_byte(pixels, 0x00);
    append_byte(pixels, 0xff);
    append_byte(pixels, 0xff);
    append_byte(pixels, 0xff);
    append_byte(pixels, 0x00);
    append_byte(pixels, 0x00);

    append_byte(pixels, 0x00);
    append_byte(pixels, 0x00);
    append_byte(pixels, 0xff);
    append_byte(pixels, 0x00);
    append_byte(pixels, 0xff);
    append_byte(pixels, 0x00);
    append_byte(pixels, 0x00);
    append_byte(pixels, 0x00);

    return make_bmp_bytes(2, 2, 24, pixels);
}

std::vector<std::byte> make_bmp_32_bit_top_down_2x2()
{
    std::vector<std::byte> pixels;
    append_byte(pixels, 0x03);
    append_byte(pixels, 0x02);
    append_byte(pixels, 0x01);
    append_byte(pixels, 0x04);
    append_byte(pixels, 0x30);
    append_byte(pixels, 0x20);
    append_byte(pixels, 0x10);
    append_byte(pixels, 0x40);

    append_byte(pixels, 0x60);
    append_byte(pixels, 0x50);
    append_byte(pixels, 0x40);
    append_byte(pixels, 0x70);
    append_byte(pixels, 0x90);
    append_byte(pixels, 0x80);
    append_byte(pixels, 0x70);
    append_byte(pixels, 0xa0);

    return make_bmp_bytes(2, -2, 32, pixels);
}

void test_bmp_decoder_decodes_24_bit_bottom_up()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> encoded = make_bmp_24_bit_bottom_up_2x2();
    const render_image_decode_request request = make_decode_request("textures/bottom-up.bmp", encoded);
    const bmp_image_decoder decoder;
    const render_image_decode_result result = decoder.decode(request);

    require(decoder.supports(request), "bmp decoder supports .bmp source");
    require(result.ok(), "bmp decoder decodes 24-bit bottom-up BMP");
    require(result.metadata.decoder_id == "bmp_image_decoder", "bmp metadata records decoder id");
    require(result.metadata.encoded_byte_count == encoded.size(), "bmp metadata records encoded byte count");
    require(result.metadata.width == 2, "bmp metadata records width");
    require(result.metadata.height == 2, "bmp metadata records height");
    require(result.metadata.decoded_byte_count == 16, "bmp metadata records RGBA byte count");
    require(result.metadata.size_validation.valid, "bmp metadata validates decoded payload size");
    require(
        result.metadata.format_detection.detected_format == render_image_encoded_format::bmp,
        "bmp metadata records detected BMP format");
    require(
        result.metadata.format_detection.recognized_signature,
        "bmp metadata records recognized BMP signature");
    require(result.image.width == 2, "decoded 24-bit BMP width is preserved");
    require(result.image.height == 2, "decoded 24-bit BMP height is preserved");
    require(result.image.pixel_format == render_image_pixel_format::rgba8_srgb, "BMP decodes to RGBA8 SRGB");
    require(
        result.image.pixels
            == make_expected_rgba({
                0xff, 0x00, 0x00, 0xff,
                0x00, 0xff, 0x00, 0xff,
                0x00, 0x00, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff,
            }),
        "24-bit bottom-up BMP pixels are flipped to top-first RGBA");
}

void test_bmp_decoder_decodes_32_bit_top_down_with_alpha()
{
    using namespace quiz_vulkan::render;

    const render_image_decode_request request =
        make_decode_request("textures/top-down.bmp", make_bmp_32_bit_top_down_2x2());
    const bmp_image_decoder decoder;
    const render_image_decode_result result = decoder.decode(request);

    require(result.ok(), "bmp decoder decodes 32-bit top-down BMP");
    require(result.image.width == 2, "decoded 32-bit BMP width is preserved");
    require(result.image.height == 2, "decoded 32-bit BMP height is absolute header height");
    require(
        result.image.pixels
            == make_expected_rgba({
                0x01, 0x02, 0x03, 0x04,
                0x10, 0x20, 0x30, 0x40,
                0x40, 0x50, 0x60, 0x70,
                0x70, 0x80, 0x90, 0xa0,
            }),
        "32-bit top-down BMP preserves row order and alpha");
}

void test_bmp_decoder_reports_truncated_pixel_data()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> encoded = make_bmp_24_bit_bottom_up_2x2();
    encoded.resize(encoded.size() - 5);

    const render_image_decode_request request = make_decode_request("textures/truncated.bmp", encoded);
    const bmp_image_decoder decoder;
    const render_image_decode_result result = decoder.decode(request);

    require(!result.ok(), "truncated BMP does not decode");
    require(result.status == render_image_decode_status::invalid_data, "truncated BMP reports invalid data");
    require(result.image.empty(), "truncated BMP returns no decoded image");
    require(!result.metadata.has_image(), "truncated BMP metadata records no decoded image");
    require(
        result.metadata.format_detection.detected_format == render_image_encoded_format::bmp,
        "truncated BMP still records BMP format from signature");
    require(
        result.diagnostic.find("truncated") != std::string::npos,
        "truncated BMP diagnostic is placeholder friendly");
}

void test_decoder_chain_selects_bmp_alongside_ppm()
{
    using namespace quiz_vulkan::render;

    const render_image_decode_request request =
        make_decode_request("textures/chain.bmp", make_bmp_24_bit_bottom_up_2x2());
    const render_image_format_detection_summary detection = detect_render_image_format(request);
    require(detection.extension_hint == "bmp", "format detection records BMP extension hint");
    require(detection.detected_format == render_image_encoded_format::bmp, "format detection recognizes BMP signature");
    require(detection.recognized_signature, "format detection marks BMP signature as recognized");
    require(render_image_encoded_format_name(render_image_encoded_format::bmp) == "bmp", "BMP format has a stable name");
    require(
        render_image_encoded_format_mime_type(render_image_encoded_format::bmp) == "image/bmp",
        "BMP format has a MIME type");
    require(
        render_image_extension_decode_order("bmp") == render_image_extension_decode_order("ppm"),
        "BMP extension receives native image decoder priority alongside PPM");
    require(
        render_image_encoded_format_decode_order(render_image_encoded_format::bmp)
            == render_image_encoded_format_decode_order(render_image_encoded_format::ppm),
        "BMP signature receives native image decoder priority alongside PPM");

    const ppm_image_decoder ppm_decoder;
    const bmp_image_decoder bmp_decoder;
    image_decoder_chain chain;
    chain.add_decoder("ppm_image_decoder", ppm_decoder);
    chain.add_decoder("bmp_image_decoder", bmp_decoder);

    require(chain.supports(request), "decoder chain supports BMP when BMP decoder is present");
    const render_image_decode_result result = chain.decode(request);
    require(result.ok(), "decoder chain decodes BMP after skipping PPM");
    require(result.metadata.decoder_id == "bmp_image_decoder", "decoder chain selects the BMP decoder");
    require(result.decoder_diagnostics.size() == 2, "decoder chain records PPM and BMP candidates");
    require(
        result.decoder_diagnostics[0].decoder_id == "ppm_image_decoder",
        "decoder chain checks PPM candidate first");
    require(!result.decoder_diagnostics[0].supported, "PPM candidate does not support BMP source");
    require(
        result.decoder_diagnostics[1].decoder_id == "bmp_image_decoder",
        "decoder chain checks BMP candidate alongside PPM");
    require(result.decoder_diagnostics[1].supported, "BMP candidate supports BMP source");
    require(result.decoder_diagnostics[1].decode_attempted, "BMP candidate is decoded");
    require(result.decoder_diagnostics[1].terminal_candidate, "BMP candidate terminates the chain");
}

} // namespace

int main()
{
    test_bmp_decoder_decodes_24_bit_bottom_up();
    test_bmp_decoder_decodes_32_bit_top_down_with_alpha();
    test_bmp_decoder_reports_truncated_pixel_data();
    test_decoder_chain_selects_bmp_alongside_ppm();
    return 0;
}
