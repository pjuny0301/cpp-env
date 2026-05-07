#pragma once

#include "render/image/png_image_header_inspector.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class png_image_chunk_kind {
    ihdr,
    idat,
    iend,
    unknown,
};

inline std::string png_image_chunk_kind_name(png_image_chunk_kind kind)
{
    switch (kind) {
    case png_image_chunk_kind::ihdr:
        return "IHDR";
    case png_image_chunk_kind::idat:
        return "IDAT";
    case png_image_chunk_kind::iend:
        return "IEND";
    case png_image_chunk_kind::unknown:
        return "unknown";
    }

    return "unknown";
}

enum class png_image_chunk_scan_status {
    valid,
    missing_signature,
    missing_ihdr,
    missing_idat,
    missing_iend,
    duplicate_ihdr,
    invalid_first_chunk,
    truncated_chunk,
};

inline std::string png_image_chunk_scan_status_name(png_image_chunk_scan_status status)
{
    switch (status) {
    case png_image_chunk_scan_status::valid:
        return "valid";
    case png_image_chunk_scan_status::missing_signature:
        return "missing_signature";
    case png_image_chunk_scan_status::missing_ihdr:
        return "missing_ihdr";
    case png_image_chunk_scan_status::missing_idat:
        return "missing_idat";
    case png_image_chunk_scan_status::missing_iend:
        return "missing_iend";
    case png_image_chunk_scan_status::duplicate_ihdr:
        return "duplicate_ihdr";
    case png_image_chunk_scan_status::invalid_first_chunk:
        return "invalid_first_chunk";
    case png_image_chunk_scan_status::truncated_chunk:
        return "truncated_chunk";
    }

    return "unknown";
}

struct png_image_chunk_snapshot {
    std::size_t index = 0;
    png_image_chunk_kind kind = png_image_chunk_kind::unknown;
    std::string type_code;
    std::size_t chunk_offset = 0;
    std::size_t data_offset = 0;
    std::size_t crc_offset = 0;
    std::size_t next_chunk_offset = 0;
    std::size_t data_byte_count = 0;
};

struct png_image_chunk_scan_result {
    png_image_chunk_scan_status status = png_image_chunk_scan_status::missing_signature;
    png_image_header_inspect_result header;
    bool signature_valid = false;
    bool ihdr_seen = false;
    bool idat_seen = false;
    bool iend_seen = false;
    std::size_t chunk_count = 0;
    std::size_t unknown_chunk_count = 0;
    std::size_t idat_chunk_count = 0;
    std::size_t idat_compressed_byte_count = 0;
    std::vector<png_image_chunk_snapshot> chunks;
    std::string diagnostic;

    bool ok() const
    {
        return status == png_image_chunk_scan_status::valid;
    }
};

namespace detail {

inline std::string png_chunk_type_code(const std::vector<std::byte>& bytes, std::size_t offset)
{
    std::string type_code;
    type_code.reserve(4);
    for (std::size_t index = 0; index < 4; ++index) {
        type_code.push_back(static_cast<char>(std::to_integer<unsigned char>(bytes[offset + index])));
    }
    return type_code;
}

inline png_image_chunk_kind png_chunk_kind_for_type(std::string_view type_code)
{
    if (type_code == "IHDR") {
        return png_image_chunk_kind::ihdr;
    }
    if (type_code == "IDAT") {
        return png_image_chunk_kind::idat;
    }
    if (type_code == "IEND") {
        return png_image_chunk_kind::iend;
    }
    return png_image_chunk_kind::unknown;
}

inline png_image_chunk_scan_result make_png_image_chunk_scan_failure(
    png_image_chunk_scan_status status,
    png_image_header_inspect_result header,
    bool signature_valid,
    bool ihdr_seen,
    bool idat_seen,
    bool iend_seen,
    std::size_t unknown_chunk_count,
    std::size_t idat_chunk_count,
    std::size_t idat_compressed_byte_count,
    std::vector<png_image_chunk_snapshot> chunks,
    std::string diagnostic)
{
    return png_image_chunk_scan_result{
        .status = status,
        .header = std::move(header),
        .signature_valid = signature_valid,
        .ihdr_seen = ihdr_seen,
        .idat_seen = idat_seen,
        .iend_seen = iend_seen,
        .chunk_count = chunks.size(),
        .unknown_chunk_count = unknown_chunk_count,
        .idat_chunk_count = idat_chunk_count,
        .idat_compressed_byte_count = idat_compressed_byte_count,
        .chunks = std::move(chunks),
        .diagnostic = std::move(diagnostic),
    };
}

} // namespace detail

inline png_image_chunk_scan_result scan_png_image_chunks(const std::vector<std::byte>& bytes)
{
    constexpr std::size_t png_signature_byte_count = 8;
    constexpr std::size_t png_chunk_prefix_byte_count = 8;
    constexpr std::size_t png_chunk_crc_byte_count = 4;
    constexpr std::size_t png_ihdr_data_byte_count = 13;
    constexpr std::size_t png_ihdr_total_byte_count =
        png_chunk_prefix_byte_count + png_ihdr_data_byte_count + png_chunk_crc_byte_count;

    png_image_header_inspect_result header = inspect_png_image_header(bytes);
    if (!starts_with_png_signature(bytes)) {
        return detail::make_png_image_chunk_scan_failure(
            png_image_chunk_scan_status::missing_signature,
            std::move(header),
            false,
            false,
            false,
            false,
            0,
            0,
            0,
            {},
            "png image chunk scanner requires the PNG file signature");
    }

    if (bytes.size() == png_signature_byte_count) {
        return detail::make_png_image_chunk_scan_failure(
            png_image_chunk_scan_status::missing_ihdr,
            std::move(header),
            true,
            false,
            false,
            false,
            0,
            0,
            0,
            {},
            "png image chunk scanner requires an IHDR chunk after the PNG signature");
    }

    std::size_t offset = png_signature_byte_count;
    bool ihdr_seen = false;
    bool idat_seen = false;
    bool iend_seen = false;
    std::size_t unknown_chunk_count = 0;
    std::size_t idat_chunk_count = 0;
    std::size_t idat_compressed_byte_count = 0;
    std::vector<png_image_chunk_snapshot> chunks;

    while (offset < bytes.size()) {
        if (bytes.size() - offset < png_chunk_prefix_byte_count) {
            return detail::make_png_image_chunk_scan_failure(
                png_image_chunk_scan_status::truncated_chunk,
                std::move(header),
                true,
                ihdr_seen,
                idat_seen,
                iend_seen,
                unknown_chunk_count,
                idat_chunk_count,
                idat_compressed_byte_count,
                std::move(chunks),
                "png image chunk scanner found a truncated chunk length/type header");
        }

        const std::size_t data_byte_count = static_cast<std::size_t>(detail::png_read_u32_be(bytes, offset));
        const std::string type_code = detail::png_chunk_type_code(bytes, offset + 4);
        const png_image_chunk_kind kind = detail::png_chunk_kind_for_type(type_code);
        const std::size_t data_offset = offset + png_chunk_prefix_byte_count;

        constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
        if (data_byte_count > max_size - data_offset
            || data_offset + data_byte_count > max_size - png_chunk_crc_byte_count) {
            return detail::make_png_image_chunk_scan_failure(
                png_image_chunk_scan_status::truncated_chunk,
                std::move(header),
                true,
                ihdr_seen,
                idat_seen,
                iend_seen,
                unknown_chunk_count,
                idat_chunk_count,
                idat_compressed_byte_count,
                std::move(chunks),
                "png image chunk scanner chunk byte span overflows");
        }

        const std::size_t crc_offset = data_offset + data_byte_count;
        const std::size_t next_offset = crc_offset + png_chunk_crc_byte_count;
        if (next_offset > bytes.size()) {
            return detail::make_png_image_chunk_scan_failure(
                png_image_chunk_scan_status::truncated_chunk,
                std::move(header),
                true,
                ihdr_seen,
                idat_seen,
                iend_seen,
                unknown_chunk_count,
                idat_chunk_count,
                idat_compressed_byte_count,
                std::move(chunks),
                "png image chunk scanner found truncated chunk data or CRC bytes");
        }

        png_image_chunk_snapshot chunk{
            .index = chunks.size(),
            .kind = kind,
            .type_code = type_code,
            .chunk_offset = offset,
            .data_offset = data_offset,
            .crc_offset = crc_offset,
            .next_chunk_offset = next_offset,
            .data_byte_count = data_byte_count,
        };
        chunks.push_back(std::move(chunk));

        if (chunks.size() == 1
            && (kind != png_image_chunk_kind::ihdr || data_byte_count != png_ihdr_data_byte_count)) {
            return detail::make_png_image_chunk_scan_failure(
                png_image_chunk_scan_status::invalid_first_chunk,
                std::move(header),
                true,
                false,
                kind == png_image_chunk_kind::idat,
                kind == png_image_chunk_kind::iend,
                kind == png_image_chunk_kind::unknown ? 1 : 0,
                kind == png_image_chunk_kind::idat ? 1 : 0,
                kind == png_image_chunk_kind::idat ? data_byte_count : 0,
                std::move(chunks),
                "png image chunk scanner requires the first chunk to be IHDR with length 13");
        }

        switch (kind) {
        case png_image_chunk_kind::ihdr:
            if (ihdr_seen) {
                return detail::make_png_image_chunk_scan_failure(
                    png_image_chunk_scan_status::duplicate_ihdr,
                    std::move(header),
                    true,
                    true,
                    idat_seen,
                    iend_seen,
                    unknown_chunk_count,
                    idat_chunk_count,
                    idat_compressed_byte_count,
                    std::move(chunks),
                    "png image chunk scanner found a duplicate IHDR chunk");
            }
            if (data_byte_count != png_ihdr_data_byte_count) {
                return detail::make_png_image_chunk_scan_failure(
                    png_image_chunk_scan_status::invalid_first_chunk,
                    std::move(header),
                    true,
                    false,
                    idat_seen,
                    iend_seen,
                    unknown_chunk_count,
                    idat_chunk_count,
                    idat_compressed_byte_count,
                    std::move(chunks),
                    "png image chunk scanner requires IHDR length 13");
            }
            ihdr_seen = true;
            break;
        case png_image_chunk_kind::idat:
            idat_seen = true;
            ++idat_chunk_count;
            idat_compressed_byte_count += data_byte_count;
            break;
        case png_image_chunk_kind::iend:
            iend_seen = true;
            break;
        case png_image_chunk_kind::unknown:
            ++unknown_chunk_count;
            break;
        }

        offset = next_offset;
        if (iend_seen) {
            break;
        }
    }

    if (!ihdr_seen) {
        return detail::make_png_image_chunk_scan_failure(
            png_image_chunk_scan_status::missing_ihdr,
            std::move(header),
            true,
            false,
            idat_seen,
            iend_seen,
            unknown_chunk_count,
            idat_chunk_count,
            idat_compressed_byte_count,
            std::move(chunks),
            "png image chunk scanner did not find an IHDR chunk");
    }

    if (!idat_seen) {
        return detail::make_png_image_chunk_scan_failure(
            png_image_chunk_scan_status::missing_idat,
            std::move(header),
            true,
            true,
            false,
            iend_seen,
            unknown_chunk_count,
            idat_chunk_count,
            idat_compressed_byte_count,
            std::move(chunks),
            "png image chunk scanner did not find any IDAT compressed data chunks");
    }

    if (!iend_seen) {
        return detail::make_png_image_chunk_scan_failure(
            png_image_chunk_scan_status::missing_iend,
            std::move(header),
            true,
            true,
            true,
            false,
            unknown_chunk_count,
            idat_chunk_count,
            idat_compressed_byte_count,
            std::move(chunks),
            "png image chunk scanner did not find an IEND chunk");
    }

    return png_image_chunk_scan_result{
        .status = png_image_chunk_scan_status::valid,
        .header = std::move(header),
        .signature_valid = true,
        .ihdr_seen = true,
        .idat_seen = true,
        .iend_seen = true,
        .chunk_count = chunks.size(),
        .unknown_chunk_count = unknown_chunk_count,
        .idat_chunk_count = idat_chunk_count,
        .idat_compressed_byte_count = idat_compressed_byte_count,
        .chunks = std::move(chunks),
        .diagnostic = "png image chunk scanner found IHDR, IDAT, and IEND chunks",
    };
}

} // namespace quiz_vulkan::render
