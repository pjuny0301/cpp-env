#pragma once

#include "render/image/png_image_decode_boundary.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {
namespace detail {

inline std::uint8_t png_zlib_byte_to_u8(std::byte value)
{
    return std::to_integer<std::uint8_t>(value);
}

inline std::uint16_t png_zlib_read_u16_le(const std::vector<std::byte>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(png_zlib_byte_to_u8(bytes[offset]))
        | (static_cast<std::uint16_t>(png_zlib_byte_to_u8(bytes[offset + 1])) << 8));
}

inline std::uint32_t png_zlib_read_u32_be(const std::vector<std::byte>& bytes, std::size_t offset)
{
    return (static_cast<std::uint32_t>(png_zlib_byte_to_u8(bytes[offset])) << 24)
        | (static_cast<std::uint32_t>(png_zlib_byte_to_u8(bytes[offset + 1])) << 16)
        | (static_cast<std::uint32_t>(png_zlib_byte_to_u8(bytes[offset + 2])) << 8)
        | static_cast<std::uint32_t>(png_zlib_byte_to_u8(bytes[offset + 3]));
}

inline png_image_inflate_result make_png_zlib_inflate_failure(std::string diagnostic)
{
    return png_image_inflate_result{
        .status = png_image_inflate_status::failed,
        .inflated_bytes = {},
        .diagnostic = std::move(diagnostic),
    };
}

inline std::uint32_t png_zlib_adler32(const std::vector<std::byte>& bytes)
{
    constexpr std::uint32_t adler_modulus = 65521u;
    std::uint32_t a = 1u;
    std::uint32_t b = 0u;
    for (std::byte value : bytes) {
        a = (a + png_zlib_byte_to_u8(value)) % adler_modulus;
        b = (b + a) % adler_modulus;
    }
    return (b << 16) | a;
}

struct png_zlib_bit_reader {
    const std::vector<std::byte>& bytes;
    std::size_t limit = 0;
    std::size_t byte_offset = 0;
    std::uint8_t bit_offset = 0;

    bool read_bits(std::uint8_t bit_count, std::uint8_t& value)
    {
        value = 0;
        for (std::uint8_t bit_index = 0; bit_index < bit_count; ++bit_index) {
            if (byte_offset >= limit) {
                return false;
            }

            const std::uint8_t current = png_zlib_byte_to_u8(bytes[byte_offset]);
            const std::uint8_t bit = static_cast<std::uint8_t>((current >> bit_offset) & 0x01u);
            value = static_cast<std::uint8_t>(value | static_cast<std::uint8_t>(bit << bit_index));

            ++bit_offset;
            if (bit_offset == 8) {
                bit_offset = 0;
                ++byte_offset;
            }
        }
        return true;
    }

    void align_to_byte()
    {
        if (bit_offset == 0) {
            return;
        }

        bit_offset = 0;
        ++byte_offset;
    }
};

inline bool png_zlib_has_valid_header(std::uint8_t cmf, std::uint8_t flg, std::string& diagnostic)
{
    constexpr std::uint8_t deflate_compression_method = 8u;
    const std::uint8_t compression_method = static_cast<std::uint8_t>(cmf & 0x0fu);
    if (compression_method != deflate_compression_method) {
        diagnostic = "png zlib inflater bad_header: zlib CMF must use deflate compression method 8";
        return false;
    }

    const std::uint8_t compression_info = static_cast<std::uint8_t>((cmf >> 4) & 0x0fu);
    if (compression_info > 7u) {
        diagnostic = "png zlib inflater bad_header: zlib CINFO window size is invalid";
        return false;
    }

    const std::uint16_t header = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(cmf) << 8) | static_cast<std::uint16_t>(flg));
    if ((header % 31u) != 0u) {
        diagnostic = "png zlib inflater bad_header: zlib CMF/FLG check bits are invalid";
        return false;
    }

    constexpr std::uint8_t preset_dictionary_flag = 0x20u;
    if ((flg & preset_dictionary_flag) != 0u) {
        diagnostic = "png zlib inflater unsupported_dictionary: preset dictionaries are not supported";
        return false;
    }

    return true;
}

} // namespace detail

inline png_image_inflate_result inflate_png_zlib_stored_blocks(const png_image_inflate_request& request)
{
    const std::vector<std::byte>& bytes = request.compressed_bytes;
    if (bytes.size() != request.compressed_byte_count) {
        return detail::make_png_zlib_inflate_failure(
            "png zlib inflater bad_request: compressed byte count does not match the payload size");
    }

    if (bytes.size() < 2) {
        return detail::make_png_zlib_inflate_failure(
            "png zlib inflater bad_header: zlib stream requires CMF and FLG bytes");
    }

    std::string header_diagnostic;
    if (!detail::png_zlib_has_valid_header(
            detail::png_zlib_byte_to_u8(bytes[0]),
            detail::png_zlib_byte_to_u8(bytes[1]),
            header_diagnostic)) {
        return detail::make_png_zlib_inflate_failure(std::move(header_diagnostic));
    }

    constexpr std::size_t zlib_header_byte_count = 2;
    constexpr std::size_t adler32_byte_count = 4;
    if (bytes.size() < zlib_header_byte_count + adler32_byte_count + 1) {
        return detail::make_png_zlib_inflate_failure(
            "png zlib inflater truncated_block: zlib stream requires deflate bytes and Adler32 checksum");
    }

    const std::size_t checksum_offset = bytes.size() - adler32_byte_count;
    detail::png_zlib_bit_reader reader{
        .bytes = bytes,
        .limit = checksum_offset,
        .byte_offset = zlib_header_byte_count,
        .bit_offset = 0,
    };

    bool final_block_seen = false;
    std::vector<std::byte> inflated_bytes;
    while (!final_block_seen) {
        std::uint8_t final_block = 0;
        std::uint8_t block_type = 0;
        if (!reader.read_bits(1, final_block) || !reader.read_bits(2, block_type)) {
            return detail::make_png_zlib_inflate_failure(
                "png zlib inflater truncated_block: deflate block header is truncated");
        }

        final_block_seen = final_block != 0u;
        if (block_type == 1u || block_type == 2u) {
            return detail::make_png_zlib_inflate_failure(
                "png zlib inflater unsupported_huffman_block: compressed Huffman deflate blocks are not supported");
        }
        if (block_type == 3u) {
            return detail::make_png_zlib_inflate_failure(
                "png zlib inflater bad_block_type: reserved deflate block type 3 is invalid");
        }

        reader.align_to_byte();
        if (reader.byte_offset > checksum_offset || checksum_offset - reader.byte_offset < 4) {
            return detail::make_png_zlib_inflate_failure(
                "png zlib inflater truncated_block: stored block LEN/NLEN bytes are truncated");
        }

        const std::uint16_t len = detail::png_zlib_read_u16_le(bytes, reader.byte_offset);
        const std::uint16_t nlen = detail::png_zlib_read_u16_le(bytes, reader.byte_offset + 2);
        reader.byte_offset += 4;
        if (static_cast<std::uint16_t>(len ^ 0xffffu) != nlen) {
            return detail::make_png_zlib_inflate_failure(
                "png zlib inflater bad_stored_block_length: stored block LEN and NLEN are not complements");
        }

        if (reader.byte_offset > checksum_offset
            || static_cast<std::size_t>(len) > checksum_offset - reader.byte_offset) {
            return detail::make_png_zlib_inflate_failure(
                "png zlib inflater truncated_block: stored block payload is truncated");
        }

        inflated_bytes.insert(
            inflated_bytes.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(reader.byte_offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(reader.byte_offset + len));
        reader.byte_offset += len;
    }

    if (reader.bit_offset != 0 || reader.byte_offset != checksum_offset) {
        return detail::make_png_zlib_inflate_failure(
            "png zlib inflater trailing_bytes: zlib stream has bytes between final deflate block and Adler32 checksum");
    }

    const std::uint32_t expected_adler32 = detail::png_zlib_read_u32_be(bytes, checksum_offset);
    const std::uint32_t actual_adler32 = detail::png_zlib_adler32(inflated_bytes);
    if (expected_adler32 != actual_adler32) {
        return detail::make_png_zlib_inflate_failure(
            "png zlib inflater checksum_mismatch: Adler32 checksum does not match inflated bytes");
    }

    return png_image_inflate_result{
        .status = png_image_inflate_status::inflated,
        .inflated_bytes = std::move(inflated_bytes),
        .diagnostic = "png zlib inflater inflated stored/no-compression deflate blocks and validated Adler32",
    };
}

class png_image_zlib_inflater final : public png_image_inflater_interface {
public:
    png_image_inflate_result inflate(const png_image_inflate_request& request) const override
    {
        return inflate_png_zlib_stored_blocks(request);
    }
};

} // namespace quiz_vulkan::render
