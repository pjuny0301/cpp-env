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

void append_stored_block(
    std::vector<std::byte>& bytes,
    const std::vector<std::byte>& payload,
    bool final_block)
{
    append_byte(bytes, final_block ? 0x01 : 0x00);
    const std::uint16_t len = static_cast<std::uint16_t>(payload.size());
    append_u16_le(bytes, len);
    append_u16_le(bytes, static_cast<std::uint16_t>(len ^ 0xffffu));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
}

std::vector<std::byte> concatenate_blocks(const std::vector<std::vector<std::byte>>& blocks)
{
    std::vector<std::byte> payload;
    for (const std::vector<std::byte>& block : blocks) {
        payload.insert(payload.end(), block.begin(), block.end());
    }
    return payload;
}

std::vector<std::byte> make_zlib_stored_stream(const std::vector<std::vector<std::byte>>& blocks)
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 0x78);
    append_byte(bytes, 0x01);
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        append_stored_block(bytes, blocks[index], index + 1 == blocks.size());
    }
    append_u32_be(bytes, adler32(concatenate_blocks(blocks)));
    return bytes;
}

std::vector<std::byte> make_zlib_stored_stream(const std::vector<std::byte>& payload)
{
    return make_zlib_stored_stream(std::vector<std::vector<std::byte>>{payload});
}

quiz_vulkan::render::png_image_inflate_request make_inflate_request(std::vector<std::byte> compressed)
{
    return quiz_vulkan::render::png_image_inflate_request{
        .compressed_byte_count = compressed.size(),
        .expected_inflated_byte_count = 0,
        .idat_chunk_count = 1,
        .compressed_bytes = std::move(compressed),
    };
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

quiz_vulkan::render::render_image_decode_request make_decode_request(std::vector<std::byte> encoded_bytes)
{
    return quiz_vulkan::render::render_image_decode_request{
        .source = quiz_vulkan::render::render_resolved_image_source{
            .original_uri = "textures/card.png",
            .normalized_uri = "textures/card.png",
            .kind = quiz_vulkan::render::render_image_source_kind::local_path,
        },
        .encoded_bytes = std::move(encoded_bytes),
    };
}

void test_zlib_inflater_inflates_single_stored_block()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> payload = make_bytes({1, 2, 3, 4, 5});
    const png_image_zlib_inflater inflater;
    const png_image_inflate_result result =
        inflater.inflate(make_inflate_request(make_zlib_stored_stream(payload)));

    require(result.ok(), "zlib inflater accepts one stored deflate block");
    require(result.status == png_image_inflate_status::inflated, "zlib inflater reports inflated status");
    require(result.inflated_bytes == payload, "zlib inflater preserves stored block payload bytes");
    require(
        result.diagnostic.find("stored/no-compression") != std::string::npos,
        "zlib inflater success diagnostic names stored blocks");
    require(
        result.diagnostic.find("Adler32") != std::string::npos,
        "zlib inflater success diagnostic names Adler32 validation");
}

void test_zlib_inflater_inflates_multiple_stored_blocks()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> first = make_bytes({1, 2});
    const std::vector<std::byte> second = make_bytes({3, 4, 5});
    const std::vector<std::byte> expected = make_bytes({1, 2, 3, 4, 5});
    const png_image_inflate_result result =
        inflate_png_zlib_stored_blocks(make_inflate_request(make_zlib_stored_stream({first, second})));

    require(result.ok(), "zlib inflater accepts multiple stored deflate blocks");
    require(result.inflated_bytes == expected, "zlib inflater concatenates stored block payloads");
}

void test_zlib_inflater_reports_bad_header()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> compressed = make_zlib_stored_stream(make_bytes({1}));
    compressed[1] = std::byte{0x00};
    const png_image_zlib_inflater inflater;
    const png_image_inflate_result result = inflater.inflate(make_inflate_request(std::move(compressed)));

    require(!result.ok(), "zlib inflater rejects invalid CMF/FLG check bits");
    require(result.status == png_image_inflate_status::failed, "bad zlib header reports failed status");
    require(
        result.diagnostic.find("bad_header") != std::string::npos,
        "bad zlib header diagnostic is deterministic");
}

void test_zlib_inflater_reports_unsupported_huffman_block()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> compressed;
    append_byte(compressed, 0x78);
    append_byte(compressed, 0x01);
    append_byte(compressed, 0x03);
    append_u32_be(compressed, adler32(std::vector<std::byte>{}));

    const png_image_zlib_inflater inflater;
    const png_image_inflate_result result = inflater.inflate(make_inflate_request(std::move(compressed)));

    require(!result.ok(), "zlib inflater rejects fixed Huffman deflate blocks");
    require(
        result.diagnostic.find("unsupported_huffman_block") != std::string::npos,
        "unsupported Huffman diagnostic is deterministic");
}

void test_zlib_inflater_reports_truncated_stored_block_header()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> compressed;
    append_byte(compressed, 0x78);
    append_byte(compressed, 0x01);
    append_byte(compressed, 0x01);
    append_u32_be(compressed, adler32(std::vector<std::byte>{}));

    const png_image_zlib_inflater inflater;
    const png_image_inflate_result result = inflater.inflate(make_inflate_request(std::move(compressed)));

    require(!result.ok(), "zlib inflater rejects truncated stored block header");
    require(
        result.diagnostic.find("truncated_block") != std::string::npos,
        "truncated stored block header diagnostic is deterministic");
}

void test_zlib_inflater_reports_truncated_stored_block_payload()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> compressed = make_zlib_stored_stream(make_bytes({1, 2, 3}));
    compressed.erase(compressed.end() - 5);
    const png_image_zlib_inflater inflater;
    const png_image_inflate_result result = inflater.inflate(make_inflate_request(std::move(compressed)));

    require(!result.ok(), "zlib inflater rejects truncated stored block payload");
    require(
        result.diagnostic.find("truncated_block") != std::string::npos,
        "truncated stored block payload diagnostic is deterministic");
    require(
        result.diagnostic.find("payload") != std::string::npos,
        "truncated stored block payload diagnostic names payload");
}

void test_zlib_inflater_reports_bad_stored_block_length()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> compressed = make_zlib_stored_stream(make_bytes({1, 2, 3}));
    compressed[5] = std::byte{0x00};
    const png_image_zlib_inflater inflater;
    const png_image_inflate_result result = inflater.inflate(make_inflate_request(std::move(compressed)));

    require(!result.ok(), "zlib inflater rejects stored block LEN/NLEN mismatch");
    require(
        result.diagnostic.find("bad_stored_block_length") != std::string::npos,
        "bad stored block length diagnostic is deterministic");
}

void test_zlib_inflater_reports_checksum_mismatch()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> compressed = make_zlib_stored_stream(make_bytes({1, 2, 3}));
    compressed.back() = std::byte{0x00};
    const png_image_zlib_inflater inflater;
    const png_image_inflate_result result = inflater.inflate(make_inflate_request(std::move(compressed)));

    require(!result.ok(), "zlib inflater rejects Adler32 checksum mismatch");
    require(
        result.diagnostic.find("checksum_mismatch") != std::string::npos,
        "checksum mismatch diagnostic is deterministic");
}

void test_png_decoder_uses_zlib_inflater_backend_for_stored_idat()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> scanlines = make_filter_none_scanlines();
    const render_image_decode_request request =
        make_decode_request(make_png_bytes(make_zlib_stored_stream(scanlines)));
    const png_image_zlib_inflater inflater;
    const png_image_decoder decoder(inflater);
    const render_image_decode_result result = decoder.decode(request);

    require(result.ok(), "PNG decoder accepts zlib stored IDAT through inflater interface");
    require(result.metadata.decoder_id == "png_image_decoder", "PNG decoder records decoder id");
    require(result.metadata.size_validation.valid, "PNG decoder validates zlib-inflated decoded image");
    require(
        result.image.pixels == make_bytes({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}),
        "PNG decoder unfilters zlib stored filter-none rows");
}

} // namespace

int main()
{
    test_zlib_inflater_inflates_single_stored_block();
    test_zlib_inflater_inflates_multiple_stored_blocks();
    test_zlib_inflater_reports_bad_header();
    test_zlib_inflater_reports_unsupported_huffman_block();
    test_zlib_inflater_reports_truncated_stored_block_header();
    test_zlib_inflater_reports_truncated_stored_block_payload();
    test_zlib_inflater_reports_bad_stored_block_length();
    test_zlib_inflater_reports_checksum_mismatch();
    test_png_decoder_uses_zlib_inflater_backend_for_stored_idat();
    return 0;
}
