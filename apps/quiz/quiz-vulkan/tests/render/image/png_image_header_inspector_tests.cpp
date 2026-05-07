#include "render/image/image_decoder.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
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

std::vector<std::byte> make_png_ihdr_bytes(
    std::uint32_t width,
    std::uint32_t height,
    unsigned char bit_depth,
    unsigned char color_type,
    unsigned char compression_method = 0,
    unsigned char filter_method = 0,
    unsigned char interlace_method = 0)
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 0x89);
    append_byte(bytes, 'P');
    append_byte(bytes, 'N');
    append_byte(bytes, 'G');
    append_byte(bytes, '\r');
    append_byte(bytes, '\n');
    append_byte(bytes, 0x1a);
    append_byte(bytes, '\n');

    append_u32_be(bytes, 13);
    append_byte(bytes, 'I');
    append_byte(bytes, 'H');
    append_byte(bytes, 'D');
    append_byte(bytes, 'R');
    append_u32_be(bytes, width);
    append_u32_be(bytes, height);
    append_byte(bytes, bit_depth);
    append_byte(bytes, color_type);
    append_byte(bytes, compression_method);
    append_byte(bytes, filter_method);
    append_byte(bytes, interlace_method);
    append_u32_be(bytes, 0);
    return bytes;
}

void test_valid_rgba_png_ihdr()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_png_ihdr_bytes(32, 16, 8, 6);
    const png_image_header_inspect_result result = inspect_png_image_header(bytes);

    require(starts_with_png_signature(bytes), "PNG signature helper validates PNG bytes");
    require(result.ok(), "valid RGBA PNG IHDR is accepted");
    require(result.status == png_image_header_inspect_status::valid, "valid IHDR reports valid status");
    require(result.signature_valid, "valid IHDR records valid signature");
    require(result.ihdr_present, "valid IHDR records IHDR presence");
    require(result.header.width == 32, "valid IHDR records width");
    require(result.header.height == 16, "valid IHDR records height");
    require(result.header.bit_depth == 8, "valid IHDR records bit depth");
    require(result.header.color_type == 6, "valid IHDR records color type");
    require(result.header.compression_method == 0, "valid IHDR records compression method");
    require(result.header.filter_method == 0, "valid IHDR records filter method");
    require(result.header.interlace_method == 0, "valid IHDR records interlace method");
    require(result.header.pixel_count == 512, "valid IHDR reports pixel count");
    require(result.header.decoded_rgba_byte_count == 2048, "valid IHDR reports future RGBA byte count");
    require(
        result.header.decoded_pixel_format == render_image_pixel_format::rgba8_srgb,
        "valid IHDR exposes RGBA8 SRGB decode target");
    require(result.header.rgba8_supported, "valid IHDR records RGBA8 support");
    require(
        result.diagnostic == "png image header inspector parsed an 8-bit RGBA PNG IHDR",
        "valid IHDR diagnostic is deterministic");
}

void test_truncated_ihdr_reports_failure()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> bytes = make_png_ihdr_bytes(32, 16, 8, 6);
    bytes.resize(20);

    const png_image_header_inspect_result result = inspect_png_image_header(bytes);

    require(!result.ok(), "truncated PNG IHDR is rejected");
    require(
        result.status == png_image_header_inspect_status::truncated_ihdr,
        "truncated PNG IHDR reports truncated status");
    require(result.signature_valid, "truncated PNG IHDR records valid signature");
    require(!result.ihdr_present, "truncated PNG IHDR does not report complete IHDR");
    require(
        result.diagnostic.find("complete IHDR") != std::string::npos,
        "truncated PNG IHDR diagnostic is placeholder friendly");
}

void test_bad_signature_reports_missing_signature()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes{
        std::byte{static_cast<unsigned char>('n')},
        std::byte{static_cast<unsigned char>('o')},
        std::byte{static_cast<unsigned char>('t')},
        std::byte{static_cast<unsigned char>('p')},
        std::byte{static_cast<unsigned char>('n')},
        std::byte{static_cast<unsigned char>('g')},
    };
    const png_image_header_inspect_result result = inspect_png_image_header(bytes);

    require(!starts_with_png_signature(bytes), "PNG signature helper rejects non-PNG bytes");
    require(!result.ok(), "bad PNG signature is rejected");
    require(
        result.status == png_image_header_inspect_status::missing_signature,
        "bad PNG signature reports missing signature status");
    require(!result.signature_valid, "bad PNG signature records invalid signature");
    require(!result.ihdr_present, "bad PNG signature records missing IHDR");
    require(
        result.diagnostic.find("PNG file signature") != std::string::npos,
        "bad PNG signature diagnostic is deterministic");
}

void test_zero_dimensions_report_invalid_dimensions()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_png_ihdr_bytes(0, 16, 8, 6);
    const png_image_header_inspect_result result = inspect_png_image_header(bytes);

    require(!result.ok(), "zero-width PNG IHDR is rejected");
    require(
        result.status == png_image_header_inspect_status::invalid_dimensions,
        "zero-width PNG IHDR reports invalid dimensions");
    require(result.signature_valid, "zero-width PNG records valid signature");
    require(result.ihdr_present, "zero-width PNG records IHDR presence");
    require(result.header.width == 0, "zero-width PNG preserves parsed width");
    require(result.header.height == 16, "zero-width PNG preserves parsed height");
    require(
        result.diagnostic.find("nonzero dimensions") != std::string::npos,
        "zero-width PNG diagnostic explains dimensions");
}

void test_unsupported_color_type_reports_failure()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_png_ihdr_bytes(32, 16, 8, 2);
    const png_image_header_inspect_result result = inspect_png_image_header(bytes);

    require(!result.ok(), "non-RGBA PNG IHDR is rejected by narrow inspector support");
    require(
        result.status == png_image_header_inspect_status::unsupported_color_type,
        "non-RGBA PNG IHDR reports unsupported color type");
    require(result.header.color_type == 2, "non-RGBA PNG preserves parsed color type");
    require(result.header.bit_depth == 8, "non-RGBA PNG preserves parsed bit depth");
    require(result.header.pixel_count == 512, "non-RGBA PNG still reports pixel count");
    require(!result.header.rgba8_supported, "non-RGBA PNG is not marked RGBA8 supported");
    require(
        result.diagnostic.find("color type 6") != std::string::npos,
        "unsupported color type diagnostic names the supported type");
}

void test_unsupported_bit_depth_reports_failure()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_png_ihdr_bytes(32, 16, 16, 6);
    const png_image_header_inspect_result result = inspect_png_image_header(bytes);

    require(!result.ok(), "16-bit RGBA PNG IHDR is rejected by narrow inspector support");
    require(
        result.status == png_image_header_inspect_status::unsupported_bit_depth,
        "16-bit RGBA PNG IHDR reports unsupported bit depth");
    require(result.header.color_type == 6, "16-bit RGBA PNG preserves parsed color type");
    require(result.header.bit_depth == 16, "16-bit RGBA PNG preserves parsed bit depth");
    require(!result.header.rgba8_supported, "16-bit RGBA PNG is not marked RGBA8 supported");
    require(
        result.diagnostic.find("8-bit RGBA") != std::string::npos,
        "unsupported bit depth diagnostic names the supported depth");
}

void test_diagnostic_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        png_image_header_inspect_status_name(png_image_header_inspect_status::valid) == "valid",
        "valid status name is stable");
    require(
        png_image_header_inspect_status_name(png_image_header_inspect_status::missing_signature)
            == "missing_signature",
        "missing signature status name is stable");
    require(
        png_image_header_inspect_status_name(png_image_header_inspect_status::truncated_ihdr)
            == "truncated_ihdr",
        "truncated IHDR status name is stable");
    require(
        png_image_header_inspect_status_name(png_image_header_inspect_status::invalid_dimensions)
            == "invalid_dimensions",
        "invalid dimensions status name is stable");
    require(
        png_image_header_inspect_status_name(png_image_header_inspect_status::unsupported_color_type)
            == "unsupported_color_type",
        "unsupported color type status name is stable");
    require(
        png_image_header_inspect_status_name(png_image_header_inspect_status::unsupported_bit_depth)
            == "unsupported_bit_depth",
        "unsupported bit depth status name is stable");
    require(png_image_color_type_name(0) == "grayscale", "grayscale color type name is stable");
    require(png_image_color_type_name(2) == "truecolor", "truecolor color type name is stable");
    require(png_image_color_type_name(3) == "indexed_color", "indexed color type name is stable");
    require(png_image_color_type_name(4) == "grayscale_alpha", "grayscale alpha color type name is stable");
    require(png_image_color_type_name(6) == "truecolor_alpha", "truecolor alpha color type name is stable");
    require(png_image_color_type_name(7) == "unknown", "unknown color type name is stable");
}

} // namespace

int main()
{
    test_valid_rgba_png_ihdr();
    test_truncated_ihdr_reports_failure();
    test_bad_signature_reports_missing_signature();
    test_zero_dimensions_report_invalid_dimensions();
    test_unsupported_color_type_reports_failure();
    test_unsupported_bit_depth_reports_failure();
    test_diagnostic_names_are_stable();
    return 0;
}
