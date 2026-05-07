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

std::vector<std::byte> make_png_bytes()
{
    std::vector<std::byte> bytes;
    append_png_signature(bytes);
    append_png_chunk(bytes, "IHDR", make_ihdr_data());
    append_png_chunk(bytes, "IDAT", make_bytes({0x78, 0x9c, 0x03}));
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

quiz_vulkan::render::png_image_decode_plan make_ready_plan()
{
    const std::vector<std::byte> encoded = make_png_bytes();
    const quiz_vulkan::render::png_image_chunk_scan_result scan =
        quiz_vulkan::render::scan_png_image_chunks(encoded);
    const quiz_vulkan::render::png_image_decode_plan_result plan =
        quiz_vulkan::render::make_png_image_decode_plan(scan);
    require(scan.ok(), "test PNG chunk scan succeeds");
    require(plan.ok(), "test PNG decode plan succeeds");
    return plan.plan;
}

void test_filter_none_scanlines_decode_to_rgba8()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> encoded = make_png_bytes();
    const png_image_chunk_scan_result scan = scan_png_image_chunks(encoded);
    const fake_png_image_inflater inflater(png_image_inflate_result{
        .status = png_image_inflate_status::inflated,
        .inflated_bytes = make_filter_none_scanlines(),
        .diagnostic = "synthetic filter-none rows",
    });
    const png_image_decode_boundary_result decode = decode_png_image_with_inflater(encoded, scan, &inflater);
    const png_image_unfilter_result result = unfilter_png_image_rgba8(decode);

    require(decode.ok(), "filter-none test has a ready decode boundary");
    require(result.ok(), "filter-none PNG scanlines decode to RGBA8");
    require(result.status == png_image_unfilter_status::decoded, "filter-none PNG reports decoded status");
    require(result.image.width == 2, "filter-none PNG output width is preserved");
    require(result.image.height == 2, "filter-none PNG output height is preserved");
    require(result.image.pixel_format == render_image_pixel_format::rgba8_srgb, "filter-none output is RGBA8 SRGB");
    require(result.row_count == 2, "filter-none result records row count");
    require(result.row_byte_count == 8, "filter-none result records row byte count");
    require(result.filtered_row_byte_count == 9, "filter-none result records filtered row byte count");
    require(result.inflated_byte_count == 18, "filter-none result records inflated bytes");
    require(result.decoded_byte_count == 16, "filter-none result records decoded bytes");
    require(result.failed_filter_type == 0, "filter-none result records no failed filter");
    require(
        result.image.pixels == make_bytes({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}),
        "filter-none scanlines drop filter bytes and preserve RGBA payload");
    require(
        result.diagnostic.find("RGBA8 decoded pixels") != std::string::npos,
        "filter-none success diagnostic names RGBA8 payload");
}

void test_unsupported_filters_report_explicit_status()
{
    using namespace quiz_vulkan::render;

    const png_image_decode_plan plan = make_ready_plan();
    for (unsigned char filter_type = 1; filter_type <= 4; ++filter_type) {
        const png_image_unfilter_result result =
            unfilter_png_image_rgba8_scanlines(plan, make_scanlines_with_filter(filter_type));

        require(!result.ok(), "unsupported PNG filter does not decode");
        require(
            result.status == png_image_unfilter_status::unsupported_filter_type,
            "unsupported PNG filter reports unsupported filter status");
        require(result.failed_filter_type == filter_type, "unsupported PNG filter records failed filter type");
        require(result.image.empty(), "unsupported PNG filter returns no decoded image");
        require(
            result.diagnostic.find("filter type 0") != std::string::npos,
            "unsupported PNG filter diagnostic names supported filter");
    }
}

void test_truncated_scanline_data_reports_failure()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> inflated = make_filter_none_scanlines();
    inflated.resize(inflated.size() - 1);
    const png_image_unfilter_result result = unfilter_png_image_rgba8_scanlines(make_ready_plan(), inflated);

    require(!result.ok(), "truncated PNG scanlines do not decode");
    require(
        result.status == png_image_unfilter_status::truncated_scanline_data,
        "truncated PNG scanlines report truncated scanline status");
    require(result.inflated_byte_count == 17, "truncated PNG scanlines record actual inflated bytes");
    require(result.decoded_byte_count == 0, "truncated PNG scanlines return no decoded bytes");
    require(
        result.diagnostic.find("truncated") != std::string::npos,
        "truncated PNG scanline diagnostic is deterministic");
}

void test_row_stride_mismatch_reports_failure()
{
    using namespace quiz_vulkan::render;

    png_image_decode_plan plan = make_ready_plan();
    plan.row_byte_count = 7;
    const png_image_unfilter_result result = unfilter_png_image_rgba8_scanlines(plan, make_filter_none_scanlines());

    require(!result.ok(), "row-stride mismatch does not decode");
    require(
        result.status == png_image_unfilter_status::row_stride_mismatch,
        "row-stride mismatch reports row stride mismatch");
    require(result.row_byte_count == 7, "row-stride mismatch preserves bad row byte count");
    require(result.image.empty(), "row-stride mismatch returns no decoded image");
    require(
        result.diagnostic.find("row stride") != std::string::npos,
        "row-stride mismatch diagnostic names row stride");
}

void test_decode_boundary_failure_reports_failure()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> encoded = make_png_bytes();
    const png_image_chunk_scan_result scan = scan_png_image_chunks(encoded);
    const png_image_decode_boundary_result decode = decode_png_image_with_inflater(encoded, scan, nullptr);
    const png_image_unfilter_result result = unfilter_png_image_rgba8(decode);

    require(!decode.ok(), "decode boundary without inflater fails before unfilter");
    require(!result.ok(), "unfilter rejects failed decode boundary");
    require(
        result.status == png_image_unfilter_status::decode_boundary_failed,
        "failed decode boundary reports decode boundary failed status");
    require(
        result.diagnostic.find("decode/inflate boundary") != std::string::npos,
        "failed decode boundary diagnostic names decode/inflate boundary");
}

void test_stable_unfilter_status_names()
{
    using namespace quiz_vulkan::render;

    require(png_image_unfilter_status_name(png_image_unfilter_status::decoded) == "decoded", "decoded name");
    require(
        png_image_unfilter_status_name(png_image_unfilter_status::decode_boundary_failed)
            == "decode_boundary_failed",
        "decode boundary failed name");
    require(
        png_image_unfilter_status_name(png_image_unfilter_status::unsupported_filter_type)
            == "unsupported_filter_type",
        "unsupported filter name");
    require(
        png_image_unfilter_status_name(png_image_unfilter_status::truncated_scanline_data)
            == "truncated_scanline_data",
        "truncated scanline data name");
    require(
        png_image_unfilter_status_name(png_image_unfilter_status::row_stride_mismatch)
            == "row_stride_mismatch",
        "row stride mismatch name");
}

} // namespace

int main()
{
    test_filter_none_scanlines_decode_to_rgba8();
    test_unsupported_filters_report_explicit_status();
    test_truncated_scanline_data_reports_failure();
    test_row_stride_mismatch_reports_failure();
    test_decode_boundary_failure_reports_failure();
    test_stable_unfilter_status_names();
    return 0;
}
