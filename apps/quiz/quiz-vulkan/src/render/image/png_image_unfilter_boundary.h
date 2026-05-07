#pragma once

#include "render/image/png_image_decode_boundary.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class png_image_unfilter_status {
    decoded,
    decode_boundary_failed,
    unsupported_filter_type,
    truncated_scanline_data,
    row_stride_mismatch,
};

inline std::string png_image_unfilter_status_name(png_image_unfilter_status status)
{
    switch (status) {
    case png_image_unfilter_status::decoded:
        return "decoded";
    case png_image_unfilter_status::decode_boundary_failed:
        return "decode_boundary_failed";
    case png_image_unfilter_status::unsupported_filter_type:
        return "unsupported_filter_type";
    case png_image_unfilter_status::truncated_scanline_data:
        return "truncated_scanline_data";
    case png_image_unfilter_status::row_stride_mismatch:
        return "row_stride_mismatch";
    }

    return "unknown";
}

struct png_image_unfilter_result {
    png_image_unfilter_status status = png_image_unfilter_status::decode_boundary_failed;
    render_decoded_image image;
    png_image_decode_plan plan;
    std::size_t row_count = 0;
    std::size_t row_byte_count = 0;
    std::size_t filtered_row_byte_count = 0;
    std::size_t inflated_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::uint8_t failed_filter_type = 0;
    std::string diagnostic;

    bool ok() const
    {
        return status == png_image_unfilter_status::decoded;
    }
};

namespace detail {

inline png_image_unfilter_result make_png_image_unfilter_failure(
    png_image_unfilter_status status,
    png_image_decode_plan plan,
    std::size_t inflated_byte_count,
    std::uint8_t failed_filter_type,
    std::string diagnostic)
{
    const std::size_t row_count = plan.header.height;
    const std::size_t row_byte_count = plan.row_byte_count;
    const std::size_t filtered_row_byte_count = plan.filtered_row_byte_count;
    return png_image_unfilter_result{
        .status = status,
        .image = {},
        .plan = std::move(plan),
        .row_count = row_count,
        .row_byte_count = row_byte_count,
        .filtered_row_byte_count = filtered_row_byte_count,
        .inflated_byte_count = inflated_byte_count,
        .decoded_byte_count = 0,
        .failed_filter_type = failed_filter_type,
        .diagnostic = std::move(diagnostic),
    };
}

} // namespace detail

inline png_image_unfilter_result unfilter_png_image_rgba8_scanlines(
    png_image_decode_plan plan,
    const std::vector<std::byte>& inflated_bytes)
{
    constexpr std::size_t rgba8_bytes_per_pixel = 4;
    std::size_t expected_row_byte_count = 0;
    if (!detail::png_decode_checked_mul(plan.header.width, rgba8_bytes_per_pixel, expected_row_byte_count)
        || plan.header.height == 0
        || plan.bytes_per_pixel != rgba8_bytes_per_pixel
        || plan.pixel_format != render_image_pixel_format::rgba8_srgb
        || plan.row_byte_count != expected_row_byte_count) {
        return detail::make_png_image_unfilter_failure(
            png_image_unfilter_status::row_stride_mismatch,
            std::move(plan),
            inflated_bytes.size(),
            0,
            "png image unfilter boundary RGBA8 row stride does not match the decode plan");
    }

    std::size_t expected_filtered_row_byte_count = 0;
    if (!detail::png_decode_checked_add(expected_row_byte_count, std::size_t{1}, expected_filtered_row_byte_count)
        || plan.filtered_row_byte_count != expected_filtered_row_byte_count) {
        return detail::make_png_image_unfilter_failure(
            png_image_unfilter_status::row_stride_mismatch,
            std::move(plan),
            inflated_bytes.size(),
            0,
            "png image unfilter boundary filtered row stride does not match the decode plan");
    }

    std::size_t expected_inflated_byte_count = 0;
    std::size_t expected_decoded_byte_count = 0;
    if (!detail::png_decode_checked_mul(
            expected_filtered_row_byte_count,
            plan.header.height,
            expected_inflated_byte_count)
        || !detail::png_decode_checked_mul(expected_row_byte_count, plan.header.height, expected_decoded_byte_count)
        || plan.expected_inflated_byte_count != expected_inflated_byte_count
        || plan.expected_rgba_byte_count != expected_decoded_byte_count) {
        return detail::make_png_image_unfilter_failure(
            png_image_unfilter_status::row_stride_mismatch,
            std::move(plan),
            inflated_bytes.size(),
            0,
            "png image unfilter boundary expected byte counts do not match the RGBA8 row stride");
    }

    if (inflated_bytes.size() < expected_inflated_byte_count) {
        return detail::make_png_image_unfilter_failure(
            png_image_unfilter_status::truncated_scanline_data,
            std::move(plan),
            inflated_bytes.size(),
            0,
            "png image unfilter boundary inflated scanline data is truncated");
    }

    if (inflated_bytes.size() != expected_inflated_byte_count) {
        return detail::make_png_image_unfilter_failure(
            png_image_unfilter_status::row_stride_mismatch,
            std::move(plan),
            inflated_bytes.size(),
            0,
            "png image unfilter boundary inflated byte count does not match the RGBA8 row stride");
    }

    std::vector<std::byte> pixels(expected_decoded_byte_count);
    for (std::size_t row = 0; row < plan.header.height; ++row) {
        const std::size_t source_row_offset = row * expected_filtered_row_byte_count;
        const std::uint8_t filter_type = std::to_integer<std::uint8_t>(inflated_bytes[source_row_offset]);
        if (filter_type != 0) {
            return detail::make_png_image_unfilter_failure(
                png_image_unfilter_status::unsupported_filter_type,
                std::move(plan),
                inflated_bytes.size(),
                filter_type,
                "png image unfilter boundary supports only PNG filter type 0 for RGBA8 scanlines");
        }

        const std::size_t source_pixel_offset = source_row_offset + 1;
        const std::size_t destination_row_offset = row * expected_row_byte_count;
        for (std::size_t index = 0; index < expected_row_byte_count; ++index) {
            pixels[destination_row_offset + index] = inflated_bytes[source_pixel_offset + index];
        }
    }

    const std::size_t width = plan.header.width;
    const std::size_t height = plan.header.height;
    return png_image_unfilter_result{
        .status = png_image_unfilter_status::decoded,
        .image = render_decoded_image{
            .width = width,
            .height = height,
            .pixel_format = render_image_pixel_format::rgba8_srgb,
            .pixels = std::move(pixels),
        },
        .plan = std::move(plan),
        .row_count = height,
        .row_byte_count = expected_row_byte_count,
        .filtered_row_byte_count = expected_filtered_row_byte_count,
        .inflated_byte_count = inflated_bytes.size(),
        .decoded_byte_count = expected_decoded_byte_count,
        .failed_filter_type = 0,
        .diagnostic = "png image unfilter boundary produced RGBA8 decoded pixels from filter-none scanlines",
    };
}

inline png_image_unfilter_result unfilter_png_image_rgba8(
    const png_image_decode_boundary_result& decode)
{
    if (!decode.ok()) {
        return detail::make_png_image_unfilter_failure(
            png_image_unfilter_status::decode_boundary_failed,
            decode.plan.plan,
            decode.inflated_byte_count,
            0,
            "png image unfilter boundary requires a successful PNG decode/inflate boundary result");
    }

    return unfilter_png_image_rgba8_scanlines(decode.plan.plan, decode.inflate.inflated_bytes);
}

} // namespace quiz_vulkan::render
