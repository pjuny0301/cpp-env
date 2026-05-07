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

std::vector<std::byte> make_ihdr_data(std::uint32_t width = 2, std::uint32_t height = 2)
{
    std::vector<std::byte> data;
    append_u32_be(data, width);
    append_u32_be(data, height);
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

std::vector<std::byte> make_png_bytes(
    bool include_idat = true,
    bool include_iend = true,
    bool include_unknown = false)
{
    std::vector<std::byte> bytes;
    append_png_signature(bytes);
    append_png_chunk(bytes, "IHDR", make_ihdr_data());
    if (include_unknown) {
        append_png_chunk(bytes, "tEXt", make_bytes({'k', 'e', 'y'}));
    }
    if (include_idat) {
        append_png_chunk(bytes, "IDAT", make_bytes({0x78, 0x9c, 0x03}));
    }
    if (include_iend) {
        append_png_chunk(bytes, "IEND", std::vector<std::byte>{});
    }
    return bytes;
}

void test_valid_minimal_png_chunk_scan()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_png_bytes();
    const png_image_chunk_scan_result result = scan_png_image_chunks(bytes);

    require(result.ok(), "minimal PNG chunk scan succeeds");
    require(result.status == png_image_chunk_scan_status::valid, "minimal PNG reports valid status");
    require(result.signature_valid, "minimal PNG records valid signature");
    require(result.header.ok(), "minimal PNG scan carries valid IHDR metadata");
    require(result.header.header.width == 2, "minimal PNG scan carries IHDR width");
    require(result.header.header.height == 2, "minimal PNG scan carries IHDR height");
    require(result.ihdr_seen, "minimal PNG records IHDR presence");
    require(result.idat_seen, "minimal PNG records IDAT presence");
    require(result.iend_seen, "minimal PNG records IEND presence");
    require(result.chunk_count == 3, "minimal PNG reports chunk count");
    require(result.chunks.size() == 3, "minimal PNG exposes chunk snapshots");
    require(result.unknown_chunk_count == 0, "minimal PNG has no unknown chunks");
    require(result.idat_chunk_count == 1, "minimal PNG reports IDAT chunk count");
    require(result.idat_compressed_byte_count == 3, "minimal PNG reports IDAT compressed byte count");
    require(result.chunks[0].kind == png_image_chunk_kind::ihdr, "first PNG chunk is IHDR");
    require(result.chunks[0].type_code == "IHDR", "IHDR chunk type code is recorded");
    require(result.chunks[0].chunk_offset == 8, "IHDR chunk offset is recorded");
    require(result.chunks[0].data_offset == 16, "IHDR data offset is recorded");
    require(result.chunks[0].crc_offset == 29, "IHDR CRC offset is recorded");
    require(result.chunks[0].next_chunk_offset == 33, "IHDR next chunk offset is recorded");
    require(result.chunks[1].kind == png_image_chunk_kind::idat, "second PNG chunk is IDAT");
    require(result.chunks[1].data_byte_count == 3, "IDAT data byte count is recorded");
    require(result.chunks[2].kind == png_image_chunk_kind::iend, "third PNG chunk is IEND");
    require(result.chunks[2].data_byte_count == 0, "IEND data byte count is recorded");
    require(
        result.diagnostic == "png image chunk scanner found IHDR, IDAT, and IEND chunks",
        "valid PNG scan diagnostic is deterministic");
}

void test_unknown_chunk_is_recorded()
{
    using namespace quiz_vulkan::render;

    const png_image_chunk_scan_result result = scan_png_image_chunks(make_png_bytes(true, true, true));

    require(result.ok(), "PNG with unknown ancillary chunk still scans");
    require(result.chunk_count == 4, "PNG with unknown chunk reports all chunks through IEND");
    require(result.unknown_chunk_count == 1, "PNG with unknown chunk counts unknown chunk");
    require(result.chunks[1].kind == png_image_chunk_kind::unknown, "unknown PNG chunk kind is recorded");
    require(result.chunks[1].type_code == "tEXt", "unknown PNG chunk type code is preserved");
    require(result.chunks[1].data_byte_count == 3, "unknown PNG chunk length is recorded");
}

void test_missing_idat_reports_failure()
{
    using namespace quiz_vulkan::render;

    const png_image_chunk_scan_result result = scan_png_image_chunks(make_png_bytes(false, true));

    require(!result.ok(), "PNG without IDAT is rejected");
    require(result.status == png_image_chunk_scan_status::missing_idat, "PNG without IDAT reports missing IDAT");
    require(result.ihdr_seen, "PNG without IDAT records IHDR presence");
    require(!result.idat_seen, "PNG without IDAT records missing IDAT");
    require(result.iend_seen, "PNG without IDAT records IEND presence");
    require(result.idat_compressed_byte_count == 0, "PNG without IDAT reports zero compressed bytes");
    require(
        result.diagnostic.find("IDAT") != std::string::npos,
        "PNG without IDAT diagnostic mentions IDAT");
}

void test_missing_iend_reports_failure()
{
    using namespace quiz_vulkan::render;

    const png_image_chunk_scan_result result = scan_png_image_chunks(make_png_bytes(true, false));

    require(!result.ok(), "PNG without IEND is rejected");
    require(result.status == png_image_chunk_scan_status::missing_iend, "PNG without IEND reports missing IEND");
    require(result.ihdr_seen, "PNG without IEND records IHDR presence");
    require(result.idat_seen, "PNG without IEND records IDAT presence");
    require(!result.iend_seen, "PNG without IEND records missing IEND");
    require(result.idat_compressed_byte_count == 3, "PNG without IEND preserves IDAT byte count");
    require(
        result.diagnostic.find("IEND") != std::string::npos,
        "PNG without IEND diagnostic mentions IEND");
}

void test_duplicate_ihdr_reports_failure()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> bytes;
    append_png_signature(bytes);
    append_png_chunk(bytes, "IHDR", make_ihdr_data());
    append_png_chunk(bytes, "IHDR", make_ihdr_data());
    append_png_chunk(bytes, "IDAT", make_bytes({0x78}));
    append_png_chunk(bytes, "IEND", std::vector<std::byte>{});

    const png_image_chunk_scan_result result = scan_png_image_chunks(bytes);

    require(!result.ok(), "PNG with duplicate IHDR is rejected");
    require(
        result.status == png_image_chunk_scan_status::duplicate_ihdr,
        "PNG with duplicate IHDR reports duplicate IHDR");
    require(result.ihdr_seen, "PNG with duplicate IHDR records initial IHDR");
    require(result.chunk_count == 2, "PNG with duplicate IHDR reports chunks scanned through duplicate");
    require(result.chunks[1].kind == png_image_chunk_kind::ihdr, "duplicate chunk is identified as IHDR");
    require(
        result.diagnostic.find("duplicate IHDR") != std::string::npos,
        "PNG with duplicate IHDR diagnostic mentions duplicate IHDR");
}

void test_truncated_chunk_reports_failure()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> bytes;
    append_png_signature(bytes);
    append_png_chunk(bytes, "IHDR", make_ihdr_data());
    append_u32_be(bytes, 4);
    append_byte(bytes, 'I');
    append_byte(bytes, 'D');
    append_byte(bytes, 'A');
    append_byte(bytes, 'T');
    append_byte(bytes, 0x78);
    append_byte(bytes, 0x9c);

    const png_image_chunk_scan_result result = scan_png_image_chunks(bytes);

    require(!result.ok(), "PNG with truncated chunk is rejected");
    require(
        result.status == png_image_chunk_scan_status::truncated_chunk,
        "PNG with truncated chunk reports truncated chunk");
    require(result.ihdr_seen, "PNG with truncated later chunk records IHDR presence");
    require(result.chunk_count == 1, "PNG with truncated later chunk keeps complete prior chunks");
    require(
        result.diagnostic.find("truncated chunk data or CRC") != std::string::npos,
        "PNG with truncated chunk diagnostic mentions data or CRC span");
}

void test_invalid_first_chunk_reports_failure()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> bytes;
    append_png_signature(bytes);
    append_png_chunk(bytes, "IDAT", make_bytes({0x78, 0x9c}));
    append_png_chunk(bytes, "IEND", std::vector<std::byte>{});

    const png_image_chunk_scan_result result = scan_png_image_chunks(bytes);

    require(!result.ok(), "PNG with non-IHDR first chunk is rejected");
    require(
        result.status == png_image_chunk_scan_status::invalid_first_chunk,
        "PNG with non-IHDR first chunk reports invalid first chunk");
    require(!result.ihdr_seen, "PNG with non-IHDR first chunk records missing IHDR");
    require(result.idat_seen, "PNG with IDAT first chunk records IDAT type");
    require(result.chunk_count == 1, "PNG with invalid first chunk exposes first chunk snapshot");
    require(result.chunks[0].kind == png_image_chunk_kind::idat, "invalid first chunk identifies IDAT");
}

void test_missing_ihdr_reports_failure()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> bytes;
    append_png_signature(bytes);

    const png_image_chunk_scan_result result = scan_png_image_chunks(bytes);

    require(!result.ok(), "PNG signature without chunks is rejected");
    require(result.status == png_image_chunk_scan_status::missing_ihdr, "PNG without IHDR reports missing IHDR");
    require(result.signature_valid, "PNG without IHDR records valid signature");
    require(!result.ihdr_seen, "PNG without IHDR records missing IHDR");
    require(result.chunk_count == 0, "PNG without IHDR has no chunk snapshots");
}

void test_stable_status_and_chunk_names()
{
    using namespace quiz_vulkan::render;

    require(png_image_chunk_scan_status_name(png_image_chunk_scan_status::valid) == "valid", "valid status name");
    require(
        png_image_chunk_scan_status_name(png_image_chunk_scan_status::missing_signature) == "missing_signature",
        "missing signature status name");
    require(
        png_image_chunk_scan_status_name(png_image_chunk_scan_status::missing_ihdr) == "missing_ihdr",
        "missing IHDR status name");
    require(
        png_image_chunk_scan_status_name(png_image_chunk_scan_status::missing_idat) == "missing_idat",
        "missing IDAT status name");
    require(
        png_image_chunk_scan_status_name(png_image_chunk_scan_status::missing_iend) == "missing_iend",
        "missing IEND status name");
    require(
        png_image_chunk_scan_status_name(png_image_chunk_scan_status::duplicate_ihdr) == "duplicate_ihdr",
        "duplicate IHDR status name");
    require(
        png_image_chunk_scan_status_name(png_image_chunk_scan_status::invalid_first_chunk)
            == "invalid_first_chunk",
        "invalid first chunk status name");
    require(
        png_image_chunk_scan_status_name(png_image_chunk_scan_status::truncated_chunk) == "truncated_chunk",
        "truncated chunk status name");
    require(png_image_chunk_kind_name(png_image_chunk_kind::ihdr) == "IHDR", "IHDR chunk kind name");
    require(png_image_chunk_kind_name(png_image_chunk_kind::idat) == "IDAT", "IDAT chunk kind name");
    require(png_image_chunk_kind_name(png_image_chunk_kind::iend) == "IEND", "IEND chunk kind name");
    require(png_image_chunk_kind_name(png_image_chunk_kind::unknown) == "unknown", "unknown chunk kind name");
}

} // namespace

int main()
{
    test_valid_minimal_png_chunk_scan();
    test_unknown_chunk_is_recorded();
    test_missing_idat_reports_failure();
    test_missing_iend_reports_failure();
    test_duplicate_ihdr_reports_failure();
    test_truncated_chunk_reports_failure();
    test_invalid_first_chunk_reports_failure();
    test_missing_ihdr_reports_failure();
    test_stable_status_and_chunk_names();
    return 0;
}
