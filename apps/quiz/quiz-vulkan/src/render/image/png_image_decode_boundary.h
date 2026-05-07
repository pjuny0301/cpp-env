#pragma once

#include "render/image/png_image_chunk_scanner.h"

#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class png_image_decode_plan_status {
    ready,
    invalid_png_structure,
    unsupported_color_type,
    unsupported_bit_depth,
    missing_idat_payload,
};

inline std::string png_image_decode_plan_status_name(png_image_decode_plan_status status)
{
    switch (status) {
    case png_image_decode_plan_status::ready:
        return "ready";
    case png_image_decode_plan_status::invalid_png_structure:
        return "invalid_png_structure";
    case png_image_decode_plan_status::unsupported_color_type:
        return "unsupported_color_type";
    case png_image_decode_plan_status::unsupported_bit_depth:
        return "unsupported_bit_depth";
    case png_image_decode_plan_status::missing_idat_payload:
        return "missing_idat_payload";
    }

    return "unknown";
}

struct png_image_decode_plan {
    png_image_header header;
    std::size_t chunk_count = 0;
    std::size_t idat_chunk_count = 0;
    std::size_t idat_compressed_byte_count = 0;
    std::size_t bytes_per_pixel = 0;
    std::size_t row_byte_count = 0;
    std::size_t filtered_row_byte_count = 0;
    std::size_t expected_inflated_byte_count = 0;
    std::size_t expected_rgba_byte_count = 0;
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
};

struct png_image_decode_plan_result {
    png_image_decode_plan_status status = png_image_decode_plan_status::invalid_png_structure;
    png_image_decode_plan plan;
    std::string diagnostic;

    bool ok() const
    {
        return status == png_image_decode_plan_status::ready;
    }
};

enum class png_image_inflate_status {
    inflated,
    unavailable,
    failed,
};

inline std::string png_image_inflate_status_name(png_image_inflate_status status)
{
    switch (status) {
    case png_image_inflate_status::inflated:
        return "inflated";
    case png_image_inflate_status::unavailable:
        return "unavailable";
    case png_image_inflate_status::failed:
        return "failed";
    }

    return "unknown";
}

struct png_image_inflate_request {
    std::size_t compressed_byte_count = 0;
    std::size_t expected_inflated_byte_count = 0;
    std::size_t idat_chunk_count = 0;
    std::vector<std::byte> compressed_bytes;
};

struct png_image_inflate_result {
    png_image_inflate_status status = png_image_inflate_status::unavailable;
    std::vector<std::byte> inflated_bytes;
    std::string diagnostic;

    bool ok() const
    {
        return status == png_image_inflate_status::inflated;
    }
};

class png_image_inflater_interface {
public:
    virtual ~png_image_inflater_interface() = default;

    virtual png_image_inflate_result inflate(const png_image_inflate_request& request) const = 0;
};

class fake_png_image_inflater final : public png_image_inflater_interface {
public:
    fake_png_image_inflater() = default;

    explicit fake_png_image_inflater(png_image_inflate_result result)
        : result_(std::move(result))
    {
    }

    png_image_inflate_result inflate(const png_image_inflate_request& request) const override
    {
        requests.push_back(request);
        return result_;
    }

    void set_result(png_image_inflate_result result)
    {
        result_ = std::move(result);
    }

    mutable std::vector<png_image_inflate_request> requests;

private:
    png_image_inflate_result result_{
        .status = png_image_inflate_status::unavailable,
        .inflated_bytes = {},
        .diagnostic = "fake PNG inflater has no configured inflate result",
    };
};

enum class png_image_decode_boundary_status {
    ready,
    plan_failed,
    inflater_unavailable,
    inflater_failed,
    row_byte_mismatch,
};

inline std::string png_image_decode_boundary_status_name(png_image_decode_boundary_status status)
{
    switch (status) {
    case png_image_decode_boundary_status::ready:
        return "ready";
    case png_image_decode_boundary_status::plan_failed:
        return "plan_failed";
    case png_image_decode_boundary_status::inflater_unavailable:
        return "inflater_unavailable";
    case png_image_decode_boundary_status::inflater_failed:
        return "inflater_failed";
    case png_image_decode_boundary_status::row_byte_mismatch:
        return "row_byte_mismatch";
    }

    return "unknown";
}

struct png_image_decode_boundary_result {
    png_image_decode_boundary_status status = png_image_decode_boundary_status::plan_failed;
    png_image_decode_plan_result plan;
    png_image_inflate_result inflate;
    std::size_t inflated_byte_count = 0;
    std::string diagnostic;

    bool ok() const
    {
        return status == png_image_decode_boundary_status::ready;
    }
};

namespace detail {

inline png_image_decode_plan_result make_png_image_decode_plan_failure(
    png_image_decode_plan_status status,
    png_image_decode_plan plan,
    std::string diagnostic)
{
    return png_image_decode_plan_result{
        .status = status,
        .plan = std::move(plan),
        .diagnostic = std::move(diagnostic),
    };
}

inline bool png_decode_checked_add(std::size_t left, std::size_t right, std::size_t& result)
{
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (left > max_size - right) {
        return false;
    }

    result = left + right;
    return true;
}

inline bool png_decode_checked_mul(std::size_t left, std::size_t right, std::size_t& result)
{
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (left != 0 && right > max_size / left) {
        return false;
    }

    result = left * right;
    return true;
}

} // namespace detail

inline png_image_decode_plan_result make_png_image_decode_plan(const png_image_chunk_scan_result& scan)
{
    png_image_decode_plan plan{
        .header = scan.header.header,
        .chunk_count = scan.chunk_count,
        .idat_chunk_count = scan.idat_chunk_count,
        .idat_compressed_byte_count = scan.idat_compressed_byte_count,
        .bytes_per_pixel = 4,
        .row_byte_count = 0,
        .filtered_row_byte_count = 0,
        .expected_inflated_byte_count = 0,
        .expected_rgba_byte_count = scan.header.header.decoded_rgba_byte_count,
        .pixel_format = render_image_pixel_format::rgba8_srgb,
    };

    if (!scan.ok()) {
        return detail::make_png_image_decode_plan_failure(
            png_image_decode_plan_status::invalid_png_structure,
            plan,
            "png image decode boundary requires a structurally valid PNG chunk scan: " + scan.diagnostic);
    }

    if (scan.header.header.color_type != 6) {
        return detail::make_png_image_decode_plan_failure(
            png_image_decode_plan_status::unsupported_color_type,
            plan,
            "png image decode boundary supports only truecolor alpha PNG color type 6");
    }

    if (scan.header.header.bit_depth != 8) {
        return detail::make_png_image_decode_plan_failure(
            png_image_decode_plan_status::unsupported_bit_depth,
            plan,
            "png image decode boundary supports only 8-bit RGBA PNG data");
    }

    if (scan.idat_compressed_byte_count == 0) {
        return detail::make_png_image_decode_plan_failure(
            png_image_decode_plan_status::missing_idat_payload,
            plan,
            "png image decode boundary requires a non-empty IDAT compressed payload");
    }

    if (!detail::png_decode_checked_mul(scan.header.header.width, plan.bytes_per_pixel, plan.row_byte_count)) {
        return detail::make_png_image_decode_plan_failure(
            png_image_decode_plan_status::invalid_png_structure,
            plan,
            "png image decode boundary row byte count overflows");
    }

    if (!detail::png_decode_checked_add(plan.row_byte_count, std::size_t{1}, plan.filtered_row_byte_count)
        || !detail::png_decode_checked_mul(
            plan.filtered_row_byte_count,
            scan.header.header.height,
            plan.expected_inflated_byte_count)) {
        return detail::make_png_image_decode_plan_failure(
            png_image_decode_plan_status::invalid_png_structure,
            plan,
            "png image decode boundary filtered scanline byte count overflows");
    }

    return png_image_decode_plan_result{
        .status = png_image_decode_plan_status::ready,
        .plan = plan,
        .diagnostic = "png image decode boundary prepared an RGBA8 decode plan",
    };
}

inline png_image_inflate_request make_png_image_inflate_request(
    const std::vector<std::byte>& encoded_bytes,
    const png_image_chunk_scan_result& scan,
    const png_image_decode_plan& plan)
{
    std::vector<std::byte> compressed_bytes;
    compressed_bytes.reserve(plan.idat_compressed_byte_count);
    for (const png_image_chunk_snapshot& chunk : scan.chunks) {
        if (chunk.kind != png_image_chunk_kind::idat || chunk.data_byte_count == 0) {
            continue;
        }
        compressed_bytes.insert(
            compressed_bytes.end(),
            encoded_bytes.begin() + static_cast<std::ptrdiff_t>(chunk.data_offset),
            encoded_bytes.begin() + static_cast<std::ptrdiff_t>(chunk.data_offset + chunk.data_byte_count));
    }

    return png_image_inflate_request{
        .compressed_byte_count = compressed_bytes.size(),
        .expected_inflated_byte_count = plan.expected_inflated_byte_count,
        .idat_chunk_count = plan.idat_chunk_count,
        .compressed_bytes = std::move(compressed_bytes),
    };
}

inline png_image_decode_boundary_result decode_png_image_with_inflater(
    const std::vector<std::byte>& encoded_bytes,
    const png_image_chunk_scan_result& scan,
    const png_image_inflater_interface* inflater)
{
    png_image_decode_plan_result plan = make_png_image_decode_plan(scan);
    if (!plan.ok()) {
        return png_image_decode_boundary_result{
            .status = png_image_decode_boundary_status::plan_failed,
            .plan = std::move(plan),
            .inflate = {},
            .inflated_byte_count = 0,
            .diagnostic = "png image decode boundary cannot start inflation: decode plan failed",
        };
    }

    if (inflater == nullptr) {
        return png_image_decode_boundary_result{
            .status = png_image_decode_boundary_status::inflater_unavailable,
            .plan = std::move(plan),
            .inflate = {},
            .inflated_byte_count = 0,
            .diagnostic = "png image decode boundary requires an IDAT inflater backend",
        };
    }

    png_image_inflate_request inflate_request = make_png_image_inflate_request(encoded_bytes, scan, plan.plan);
    png_image_inflate_result inflate = inflater->inflate(inflate_request);
    const std::size_t inflated_byte_count = inflate.inflated_bytes.size();
    if (inflate.status == png_image_inflate_status::unavailable) {
        return png_image_decode_boundary_result{
            .status = png_image_decode_boundary_status::inflater_unavailable,
            .plan = std::move(plan),
            .inflate = std::move(inflate),
            .inflated_byte_count = inflated_byte_count,
            .diagnostic = "png image decode boundary inflater backend is unavailable",
        };
    }

    if (!inflate.ok()) {
        return png_image_decode_boundary_result{
            .status = png_image_decode_boundary_status::inflater_failed,
            .plan = std::move(plan),
            .inflate = std::move(inflate),
            .inflated_byte_count = inflated_byte_count,
            .diagnostic = "png image decode boundary inflater backend failed",
        };
    }

    if (inflated_byte_count != plan.plan.expected_inflated_byte_count) {
        return png_image_decode_boundary_result{
            .status = png_image_decode_boundary_status::row_byte_mismatch,
            .plan = std::move(plan),
            .inflate = std::move(inflate),
            .inflated_byte_count = inflated_byte_count,
            .diagnostic = "png image decode boundary inflated row byte count does not match the RGBA8 plan",
        };
    }

    return png_image_decode_boundary_result{
        .status = png_image_decode_boundary_status::ready,
        .plan = std::move(plan),
        .inflate = std::move(inflate),
        .inflated_byte_count = inflated_byte_count,
        .diagnostic = "png image decode boundary has a successful RGBA8 decode plan and inflated rows",
    };
}

} // namespace quiz_vulkan::render
