#include "render/image/image_decoder.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
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

std::vector<std::byte> make_repeated_bytes(std::size_t byte_count, unsigned char value)
{
    std::vector<std::byte> bytes;
    bytes.reserve(byte_count);
    for (std::size_t index = 0; index < byte_count; ++index) {
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

std::vector<std::byte> make_inflated_rgba_scanlines(std::size_t byte_count)
{
    std::vector<std::byte> bytes;
    bytes.reserve(byte_count);
    for (std::size_t index = 0; index < byte_count; ++index) {
        append_byte(bytes, index % 9 == 0 ? 0x00 : 0xff);
    }
    return bytes;
}

void test_decode_plan_accepts_rgba8_png()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> encoded = make_png_bytes();
    const png_image_chunk_scan_result scan = scan_png_image_chunks(encoded);
    const png_image_decode_plan_result plan = make_png_image_decode_plan(scan);

    require(scan.ok(), "valid PNG chunks scan before decode planning");
    require(plan.ok(), "RGBA8 PNG decode plan is ready");
    require(plan.status == png_image_decode_plan_status::ready, "RGBA8 PNG decode plan reports ready status");
    require(plan.plan.header.width == 2, "decode plan records width");
    require(plan.plan.header.height == 2, "decode plan records height");
    require(plan.plan.header.color_type == 6, "decode plan records color type");
    require(plan.plan.header.bit_depth == 8, "decode plan records bit depth");
    require(plan.plan.chunk_count == 3, "decode plan records chunk count");
    require(plan.plan.idat_chunk_count == 1, "decode plan records IDAT chunk count");
    require(plan.plan.idat_compressed_byte_count == 3, "decode plan records IDAT compressed bytes");
    require(plan.plan.bytes_per_pixel == 4, "decode plan records RGBA bytes per pixel");
    require(plan.plan.row_byte_count == 8, "decode plan records unfiltered row bytes");
    require(plan.plan.filtered_row_byte_count == 9, "decode plan records PNG filter byte per row");
    require(plan.plan.expected_inflated_byte_count == 18, "decode plan records expected inflated bytes");
    require(plan.plan.expected_rgba_byte_count == 16, "decode plan records expected RGBA bytes");
    require(
        plan.plan.pixel_format == render_image_pixel_format::rgba8_srgb,
        "decode plan records RGBA8 SRGB target format");
    require(
        plan.diagnostic == "png image decode boundary prepared an RGBA8 decode plan",
        "decode plan diagnostic is deterministic");
}

void test_decode_plan_reports_unsupported_color_type()
{
    using namespace quiz_vulkan::render;

    const png_image_chunk_scan_result scan = scan_png_image_chunks(make_png_bytes(make_bytes({0x01}), 8, 2));
    const png_image_decode_plan_result plan = make_png_image_decode_plan(scan);

    require(scan.ok(), "non-RGBA PNG chunks are structurally valid");
    require(!plan.ok(), "non-RGBA PNG decode plan is rejected");
    require(
        plan.status == png_image_decode_plan_status::unsupported_color_type,
        "non-RGBA PNG decode plan reports unsupported color type");
    require(plan.plan.header.color_type == 2, "unsupported color type plan preserves parsed color type");
    require(
        plan.diagnostic.find("color type 6") != std::string::npos,
        "unsupported color type diagnostic names RGBA color type");
}

void test_decode_plan_reports_unsupported_bit_depth()
{
    using namespace quiz_vulkan::render;

    const png_image_chunk_scan_result scan = scan_png_image_chunks(make_png_bytes(make_bytes({0x01}), 16, 6));
    const png_image_decode_plan_result plan = make_png_image_decode_plan(scan);

    require(scan.ok(), "16-bit RGBA PNG chunks are structurally valid");
    require(!plan.ok(), "16-bit RGBA PNG decode plan is rejected");
    require(
        plan.status == png_image_decode_plan_status::unsupported_bit_depth,
        "16-bit RGBA PNG decode plan reports unsupported bit depth");
    require(plan.plan.header.bit_depth == 16, "unsupported bit depth plan preserves parsed bit depth");
    require(
        plan.diagnostic.find("8-bit RGBA") != std::string::npos,
        "unsupported bit depth diagnostic names 8-bit RGBA support");
}

void test_decode_plan_reports_missing_idat_payload()
{
    using namespace quiz_vulkan::render;

    const png_image_chunk_scan_result scan = scan_png_image_chunks(make_png_bytes(std::vector<std::byte>{}));
    const png_image_decode_plan_result plan = make_png_image_decode_plan(scan);

    require(scan.ok(), "zero-length IDAT PNG is structurally scanned");
    require(!plan.ok(), "zero-length IDAT PNG decode plan is rejected");
    require(
        plan.status == png_image_decode_plan_status::missing_idat_payload,
        "zero-length IDAT PNG reports missing IDAT payload");
    require(plan.plan.idat_chunk_count == 1, "missing payload plan still records IDAT chunk count");
    require(plan.plan.idat_compressed_byte_count == 0, "missing payload plan records zero compressed bytes");
    require(
        plan.diagnostic.find("non-empty IDAT") != std::string::npos,
        "missing IDAT payload diagnostic is deterministic");
}

void test_decode_boundary_reports_inflater_unavailable()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> encoded = make_png_bytes();
    const png_image_chunk_scan_result scan = scan_png_image_chunks(encoded);
    const png_image_decode_boundary_result result = decode_png_image_with_inflater(encoded, scan, nullptr);

    require(!result.ok(), "PNG decode boundary fails without inflater");
    require(
        result.status == png_image_decode_boundary_status::inflater_unavailable,
        "PNG decode boundary reports unavailable inflater");
    require(result.plan.ok(), "PNG decode boundary still exposes ready plan when inflater is missing");
    require(
        result.diagnostic.find("inflater backend") != std::string::npos,
        "unavailable inflater diagnostic names backend boundary");
}

void test_decode_boundary_reports_inflater_failure()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> encoded = make_png_bytes();
    const png_image_chunk_scan_result scan = scan_png_image_chunks(encoded);
    const fake_png_image_inflater inflater(png_image_inflate_result{
        .status = png_image_inflate_status::failed,
        .inflated_bytes = {},
        .diagnostic = "synthetic inflate failure",
    });
    const png_image_decode_boundary_result result = decode_png_image_with_inflater(encoded, scan, &inflater);

    require(!result.ok(), "PNG decode boundary fails when inflater fails");
    require(
        result.status == png_image_decode_boundary_status::inflater_failed,
        "PNG decode boundary reports inflater failure");
    require(inflater.requests.size() == 1, "failing fake inflater records request");
    require(result.inflate.diagnostic == "synthetic inflate failure", "inflater failure diagnostic is preserved");
    require(result.inflate.status == png_image_inflate_status::failed, "inflater failure status is preserved");
}

void test_decode_boundary_reports_row_byte_mismatch()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> encoded = make_png_bytes();
    const png_image_chunk_scan_result scan = scan_png_image_chunks(encoded);
    const fake_png_image_inflater inflater(png_image_inflate_result{
        .status = png_image_inflate_status::inflated,
        .inflated_bytes = make_repeated_bytes(17, 0x00),
        .diagnostic = "synthetic short row data",
    });
    const png_image_decode_boundary_result result = decode_png_image_with_inflater(encoded, scan, &inflater);

    require(!result.ok(), "PNG decode boundary rejects inflated row byte mismatch");
    require(
        result.status == png_image_decode_boundary_status::row_byte_mismatch,
        "PNG decode boundary reports row byte mismatch");
    require(result.plan.plan.expected_inflated_byte_count == 18, "row mismatch exposes expected inflated bytes");
    require(result.inflated_byte_count == 17, "row mismatch exposes actual inflated bytes");
    require(
        result.diagnostic.find("row byte count") != std::string::npos,
        "row byte mismatch diagnostic is deterministic");
}

void test_decode_boundary_accepts_successful_fake_inflater()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> idat = make_bytes({0x78, 0x9c, 0x63, 0x60});
    const std::vector<std::byte> encoded = make_png_bytes(idat);
    const png_image_chunk_scan_result scan = scan_png_image_chunks(encoded);
    const fake_png_image_inflater inflater(png_image_inflate_result{
        .status = png_image_inflate_status::inflated,
        .inflated_bytes = make_inflated_rgba_scanlines(18),
        .diagnostic = "synthetic inflate success",
    });
    const png_image_decode_boundary_result result = decode_png_image_with_inflater(encoded, scan, &inflater);

    require(result.ok(), "PNG decode boundary accepts successful fake inflater");
    require(result.status == png_image_decode_boundary_status::ready, "successful fake inflater reports ready");
    require(result.plan.ok(), "successful fake inflater exposes ready plan");
    require(result.plan.plan.expected_inflated_byte_count == 18, "successful fake inflater preserves expected bytes");
    require(result.inflated_byte_count == 18, "successful fake inflater records actual inflated bytes");
    require(result.inflate.ok(), "successful fake inflater result is preserved");
    require(inflater.requests.size() == 1, "successful fake inflater records request");
    require(inflater.requests[0].compressed_byte_count == idat.size(), "inflate request records compressed byte count");
    require(inflater.requests[0].compressed_bytes == idat, "inflate request concatenates IDAT compressed bytes");
    require(inflater.requests[0].expected_inflated_byte_count == 18, "inflate request records expected row bytes");
    require(inflater.requests[0].idat_chunk_count == 1, "inflate request records IDAT chunk count");
    require(
        result.diagnostic.find("successful RGBA8 decode plan") != std::string::npos,
        "successful fake inflater diagnostic names RGBA8 decode plan");
}

void test_stable_decode_boundary_names()
{
    using namespace quiz_vulkan::render;

    require(png_image_decode_plan_status_name(png_image_decode_plan_status::ready) == "ready", "ready plan name");
    require(
        png_image_decode_plan_status_name(png_image_decode_plan_status::invalid_png_structure)
            == "invalid_png_structure",
        "invalid structure plan name");
    require(
        png_image_decode_plan_status_name(png_image_decode_plan_status::unsupported_color_type)
            == "unsupported_color_type",
        "unsupported color type plan name");
    require(
        png_image_decode_plan_status_name(png_image_decode_plan_status::unsupported_bit_depth)
            == "unsupported_bit_depth",
        "unsupported bit depth plan name");
    require(
        png_image_decode_plan_status_name(png_image_decode_plan_status::missing_idat_payload)
            == "missing_idat_payload",
        "missing IDAT payload plan name");
    require(
        png_image_inflate_status_name(png_image_inflate_status::inflated) == "inflated",
        "inflated status name");
    require(
        png_image_inflate_status_name(png_image_inflate_status::unavailable) == "unavailable",
        "unavailable status name");
    require(png_image_inflate_status_name(png_image_inflate_status::failed) == "failed", "failed status name");
    require(
        png_image_decode_boundary_status_name(png_image_decode_boundary_status::ready) == "ready",
        "ready boundary status name");
    require(
        png_image_decode_boundary_status_name(png_image_decode_boundary_status::plan_failed) == "plan_failed",
        "plan failed boundary status name");
    require(
        png_image_decode_boundary_status_name(png_image_decode_boundary_status::inflater_unavailable)
            == "inflater_unavailable",
        "inflater unavailable boundary status name");
    require(
        png_image_decode_boundary_status_name(png_image_decode_boundary_status::inflater_failed)
            == "inflater_failed",
        "inflater failed boundary status name");
    require(
        png_image_decode_boundary_status_name(png_image_decode_boundary_status::row_byte_mismatch)
            == "row_byte_mismatch",
        "row byte mismatch boundary status name");
}

} // namespace

int main()
{
    test_decode_plan_accepts_rgba8_png();
    test_decode_plan_reports_unsupported_color_type();
    test_decode_plan_reports_unsupported_bit_depth();
    test_decode_plan_reports_missing_idat_payload();
    test_decode_boundary_reports_inflater_unavailable();
    test_decode_boundary_reports_inflater_failure();
    test_decode_boundary_reports_row_byte_mismatch();
    test_decode_boundary_accepts_successful_fake_inflater();
    test_stable_decode_boundary_names();
    return 0;
}
