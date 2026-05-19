#pragma once

#include "render/image/image_texture_diagnostics.h"
#include "render/image/image_texture_upload_retry_policy.h"
#include "render/image/image_types.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace quiz_vulkan::render {

struct render_image_texture_upload_request {
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    render_decoded_image image;
};

enum class render_image_texture_mipmap_upload_plan_status {
    ready,
    no_mipmaps_requested,
    invalid_sampler,
    invalid_image,
    unsupported_format,
    overflow,
};

inline std::string render_image_texture_mipmap_upload_plan_status_name(
    render_image_texture_mipmap_upload_plan_status status)
{
    switch (status) {
    case render_image_texture_mipmap_upload_plan_status::ready:
        return "ready";
    case render_image_texture_mipmap_upload_plan_status::no_mipmaps_requested:
        return "no_mipmaps_requested";
    case render_image_texture_mipmap_upload_plan_status::invalid_sampler:
        return "invalid_sampler";
    case render_image_texture_mipmap_upload_plan_status::invalid_image:
        return "invalid_image";
    case render_image_texture_mipmap_upload_plan_status::unsupported_format:
        return "unsupported_format";
    case render_image_texture_mipmap_upload_plan_status::overflow:
        return "overflow";
    }

    return "unknown";
}

struct render_image_texture_mipmap_level_upload_plan {
    std::size_t level = 0;
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t pixel_count = 0;
    std::size_t byte_count = 0;
    std::size_t staging_byte_offset = 0;
};

struct render_image_texture_mipmap_upload_plan {
    render_image_texture_mipmap_upload_plan_status status =
        render_image_texture_mipmap_upload_plan_status::invalid_image;
    std::string status_name = render_image_texture_mipmap_upload_plan_status_name(
        render_image_texture_mipmap_upload_plan_status::invalid_image);
    render_image_mipmap_mode mipmap_mode = render_image_mipmap_mode::none;
    std::string mipmap_mode_name = render_image_mipmap_mode_name(render_image_mipmap_mode::none);
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    std::size_t bytes_per_pixel = 0;
    std::size_t base_width = 0;
    std::size_t base_height = 0;
    bool mipmaps_requested = false;
    bool upload_plannable = false;
    std::size_t requested_mip_level_count = 0;
    std::size_t generated_mip_level_count = 0;
    std::size_t total_pixel_count = 0;
    std::size_t total_staging_byte_count = 0;
    std::size_t total_upload_byte_count = 0;
    std::vector<render_image_texture_mipmap_level_upload_plan> levels;
    std::string diagnostic;

    bool ok() const
    {
        return upload_plannable;
    }
};

inline bool checked_render_image_texture_mipmap_upload_size(
    std::size_t left,
    std::size_t right,
    std::size_t& output)
{
    if (left == 0 || right == 0) {
        output = 0;
        return true;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (left > max_size / right) {
        output = 0;
        return false;
    }
    output = left * right;
    return true;
}

inline bool append_render_image_texture_mipmap_level_upload_plan(
    render_image_texture_mipmap_upload_plan& plan,
    std::size_t level,
    std::size_t width,
    std::size_t height)
{
    std::size_t pixel_count = 0;
    if (!checked_render_image_texture_mipmap_upload_size(width, height, pixel_count)) {
        return false;
    }

    std::size_t byte_count = 0;
    if (!checked_render_image_texture_mipmap_upload_size(pixel_count, plan.bytes_per_pixel, byte_count)) {
        return false;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (plan.total_staging_byte_count > max_size - byte_count
        || plan.total_upload_byte_count > max_size - byte_count
        || plan.total_pixel_count > max_size - pixel_count) {
        return false;
    }

    plan.levels.push_back(render_image_texture_mipmap_level_upload_plan{
        .level = level,
        .width = width,
        .height = height,
        .pixel_count = pixel_count,
        .byte_count = byte_count,
        .staging_byte_offset = plan.total_staging_byte_count,
    });
    plan.total_pixel_count += pixel_count;
    plan.total_staging_byte_count += byte_count;
    plan.total_upload_byte_count += byte_count;
    return true;
}

inline render_image_texture_mipmap_upload_plan make_render_image_texture_mipmap_upload_plan(
    const render_decoded_image& image,
    const render_image_sampler_policy& sampler)
{
    render_image_texture_mipmap_upload_plan plan{
        .mipmap_mode = sampler.mipmap_mode,
        .mipmap_mode_name = render_image_mipmap_mode_name(sampler.mipmap_mode),
        .pixel_format = image.pixel_format,
        .bytes_per_pixel = render_image_pixel_format_byte_count(image.pixel_format),
        .base_width = image.width,
        .base_height = image.height,
        .mipmaps_requested = sampler.mipmap_mode != render_image_mipmap_mode::none,
    };

    if (!is_valid_render_image_mipmap_mode(sampler.mipmap_mode)) {
        plan.status = render_image_texture_mipmap_upload_plan_status::invalid_sampler;
        plan.status_name = render_image_texture_mipmap_upload_plan_status_name(plan.status);
        plan.diagnostic = "image texture mipmap upload plan has invalid mipmap sampler mode";
        return plan;
    }

    if (image.width == 0 || image.height == 0) {
        plan.status = render_image_texture_mipmap_upload_plan_status::invalid_image;
        plan.status_name = render_image_texture_mipmap_upload_plan_status_name(plan.status);
        plan.diagnostic = "image texture mipmap upload plan requires non-zero image dimensions";
        return plan;
    }

    if (plan.bytes_per_pixel == 0) {
        plan.status = render_image_texture_mipmap_upload_plan_status::unsupported_format;
        plan.status_name = render_image_texture_mipmap_upload_plan_status_name(plan.status);
        plan.diagnostic = "image texture mipmap upload plan pixel format is unsupported";
        return plan;
    }

    std::size_t width = image.width;
    std::size_t height = image.height;
    std::size_t level = 0;
    while (true) {
        if (!append_render_image_texture_mipmap_level_upload_plan(plan, level, width, height)) {
            plan.status = render_image_texture_mipmap_upload_plan_status::overflow;
            plan.status_name = render_image_texture_mipmap_upload_plan_status_name(plan.status);
            plan.upload_plannable = false;
            plan.diagnostic = "image texture mipmap upload plan byte count overflowed";
            return plan;
        }

        if (!plan.mipmaps_requested || (width == 1 && height == 1)) {
            break;
        }

        width = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
        ++level;
    }

    plan.requested_mip_level_count = plan.levels.size();
    plan.generated_mip_level_count = plan.levels.size();
    plan.status = plan.mipmaps_requested
        ? render_image_texture_mipmap_upload_plan_status::ready
        : render_image_texture_mipmap_upload_plan_status::no_mipmaps_requested;
    plan.status_name = render_image_texture_mipmap_upload_plan_status_name(plan.status);
    plan.upload_plannable = true;
    plan.diagnostic = plan.mipmaps_requested
        ? "image texture mipmap upload plan is ready"
        : "image texture mipmap upload plan uses base level only";
    return plan;
}

enum class render_image_texture_staging_payload_plan_status {
    ready,
    ready_with_mip_references,
    blocked_missing_payload,
    blocked_invalid_layout,
    blocked_invalid_mipmap_plan,
    blocked_invalid_alignment,
    blocked_overflow,
};

inline std::string render_image_texture_staging_payload_plan_status_name(
    render_image_texture_staging_payload_plan_status status)
{
    switch (status) {
    case render_image_texture_staging_payload_plan_status::ready:
        return "ready";
    case render_image_texture_staging_payload_plan_status::ready_with_mip_references:
        return "ready_with_mip_references";
    case render_image_texture_staging_payload_plan_status::blocked_missing_payload:
        return "blocked_missing_payload";
    case render_image_texture_staging_payload_plan_status::blocked_invalid_layout:
        return "blocked_invalid_layout";
    case render_image_texture_staging_payload_plan_status::blocked_invalid_mipmap_plan:
        return "blocked_invalid_mipmap_plan";
    case render_image_texture_staging_payload_plan_status::blocked_invalid_alignment:
        return "blocked_invalid_alignment";
    case render_image_texture_staging_payload_plan_status::blocked_overflow:
        return "blocked_overflow";
    }

    return "unknown";
}

struct render_image_texture_staging_row_copy_plan {
    std::size_t mip_level = 0;
    std::size_t row_index = 0;
    std::size_t mip_width = 0;
    std::size_t mip_height = 0;
    std::size_t source_byte_offset = 0;
    std::size_t source_row_stride_byte_count = 0;
    std::size_t row_payload_byte_count = 0;
    std::size_t staging_byte_offset = 0;
    std::size_t staging_row_stride_byte_count = 0;
    std::size_t row_padding_byte_count = 0;
    std::size_t alignment_byte_count = 0;
    bool row_aligned = false;
    bool decoded_payload_backed = false;
    bool generated_mip_reference = false;
};

struct render_image_texture_staging_mip_level_reference {
    std::size_t mip_level = 0;
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t pixel_count = 0;
    std::size_t byte_count = 0;
    std::size_t mipmap_staging_byte_offset = 0;
    std::size_t staging_byte_offset = 0;
    std::size_t row_copy_begin = 0;
    std::size_t row_copy_count = 0;
    bool base_level = false;
    bool decoded_payload_backed = false;
    bool generated_mip_reference = false;
};

struct render_image_texture_staging_payload_plan {
    render_image_texture_staging_payload_plan_status status =
        render_image_texture_staging_payload_plan_status::blocked_missing_payload;
    std::string status_name = render_image_texture_staging_payload_plan_status_name(
        render_image_texture_staging_payload_plan_status::blocked_missing_payload);
    render_image_texture_key texture_key;
    render_image_sampler_policy sampler;
    std::string stable_texture_cache_key;
    std::string sampler_summary;
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    std::size_t extent_width = 0;
    std::size_t extent_height = 0;
    std::size_t bytes_per_pixel = 0;
    std::size_t alignment_byte_count = 0;
    std::size_t base_row_stride_byte_count = 0;
    std::size_t base_staging_row_stride_byte_count = 0;
    std::size_t base_row_padding_byte_count = 0;
    std::size_t row_copy_count = 0;
    std::size_t mip_level_reference_count = 0;
    std::size_t total_row_payload_byte_count = 0;
    std::size_t total_row_padding_byte_count = 0;
    std::size_t total_staging_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t referenced_mipmap_byte_count = 0;
    std::size_t referenced_mipmap_level_count = 0;
    std::uint64_t decoded_payload_hash = 0;
    render_image_texture_upload_payload_layout_evidence payload_layout;
    render_image_texture_mipmap_upload_plan mipmap_upload_plan;
    std::vector<render_image_texture_staging_row_copy_plan> row_copies;
    std::vector<render_image_texture_staging_mip_level_reference> mip_level_references;
    bool layout_ready = false;
    bool mipmap_plan_ready = false;
    bool rows_aligned = false;
    bool has_row_padding = false;
    bool decoded_payload_available = false;
    bool mipmaps_referenced = false;
    bool ready = false;
    bool blocked = true;
    std::string blocker_summary;
    std::string diagnostic;

    bool ok() const
    {
        return ready && !blocked;
    }
};

inline std::size_t render_image_texture_default_staging_row_alignment_byte_count()
{
    return 4;
}

inline bool checked_render_image_texture_staging_row_stride(
    std::size_t row_payload_byte_count,
    std::size_t alignment_byte_count,
    std::size_t& aligned_byte_count)
{
    if (alignment_byte_count == 0) {
        aligned_byte_count = 0;
        return false;
    }
    if (row_payload_byte_count == 0) {
        aligned_byte_count = 0;
        return true;
    }

    const std::size_t remainder = row_payload_byte_count % alignment_byte_count;
    if (remainder == 0) {
        aligned_byte_count = row_payload_byte_count;
        return true;
    }

    const std::size_t padding = alignment_byte_count - remainder;
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (row_payload_byte_count > max_size - padding) {
        aligned_byte_count = 0;
        return false;
    }
    aligned_byte_count = row_payload_byte_count + padding;
    return true;
}

inline bool append_render_image_texture_staging_row_copy_plan(
    render_image_texture_staging_payload_plan& plan,
    const render_image_texture_mipmap_level_upload_plan& level,
    std::size_t row_index,
    std::size_t level_staging_byte_offset,
    std::size_t row_payload_byte_count,
    std::size_t staging_row_stride_byte_count)
{
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (row_payload_byte_count > staging_row_stride_byte_count) {
        return false;
    }
    if (plan.alignment_byte_count == 0) {
        return false;
    }
    if (row_payload_byte_count == 0 || staging_row_stride_byte_count == 0) {
        return false;
    }
    if (row_index != 0 && row_index > max_size / row_payload_byte_count) {
        return false;
    }
    if (row_index != 0 && row_index > max_size / staging_row_stride_byte_count) {
        return false;
    }

    const std::size_t source_row_offset = row_index * row_payload_byte_count;
    const std::size_t staging_row_offset = row_index * staging_row_stride_byte_count;
    if (level.staging_byte_offset > max_size - source_row_offset
        || level_staging_byte_offset > max_size - staging_row_offset
        || plan.total_row_payload_byte_count > max_size - row_payload_byte_count
        || plan.total_row_padding_byte_count > max_size - (staging_row_stride_byte_count - row_payload_byte_count)
        || plan.total_staging_byte_count > max_size - staging_row_stride_byte_count) {
        return false;
    }

    const std::size_t row_padding_byte_count = staging_row_stride_byte_count - row_payload_byte_count;
    plan.row_copies.push_back(render_image_texture_staging_row_copy_plan{
        .mip_level = level.level,
        .row_index = row_index,
        .mip_width = level.width,
        .mip_height = level.height,
        .source_byte_offset = level.staging_byte_offset + source_row_offset,
        .source_row_stride_byte_count = row_payload_byte_count,
        .row_payload_byte_count = row_payload_byte_count,
        .staging_byte_offset = level_staging_byte_offset + staging_row_offset,
        .staging_row_stride_byte_count = staging_row_stride_byte_count,
        .row_padding_byte_count = row_padding_byte_count,
        .alignment_byte_count = plan.alignment_byte_count,
        .row_aligned = staging_row_stride_byte_count % plan.alignment_byte_count == 0,
        .decoded_payload_backed = level.level == 0,
        .generated_mip_reference = level.level != 0,
    });
    ++plan.row_copy_count;
    plan.total_row_payload_byte_count += row_payload_byte_count;
    plan.total_row_padding_byte_count += row_padding_byte_count;
    plan.total_staging_byte_count += staging_row_stride_byte_count;
    plan.has_row_padding = plan.has_row_padding || row_padding_byte_count != 0;
    plan.rows_aligned = plan.rows_aligned && staging_row_stride_byte_count % plan.alignment_byte_count == 0;
    return true;
}

inline render_image_texture_staging_payload_plan make_render_image_texture_staging_payload_plan(
    const render_image_texture_upload_payload_layout_evidence& payload_layout,
    const render_image_texture_mipmap_upload_plan& mipmap_upload_plan,
    std::size_t alignment_byte_count = render_image_texture_default_staging_row_alignment_byte_count())
{
    render_image_texture_staging_payload_plan plan{
        .texture_key = payload_layout.texture_key,
        .sampler = payload_layout.sampler,
        .stable_texture_cache_key = payload_layout.stable_texture_cache_key,
        .sampler_summary = payload_layout.sampler_summary,
        .pixel_format = payload_layout.pixel_format,
        .extent_width = payload_layout.extent_width,
        .extent_height = payload_layout.extent_height,
        .bytes_per_pixel = payload_layout.bytes_per_pixel,
        .alignment_byte_count = alignment_byte_count,
        .base_row_stride_byte_count = payload_layout.row_stride_byte_count,
        .decoded_byte_count = payload_layout.decoded_byte_count,
        .referenced_mipmap_byte_count = mipmap_upload_plan.total_upload_byte_count,
        .referenced_mipmap_level_count = mipmap_upload_plan.generated_mip_level_count,
        .decoded_payload_hash = payload_layout.decoded_payload.stable_byte_hash,
        .payload_layout = payload_layout,
        .mipmap_upload_plan = mipmap_upload_plan,
        .layout_ready = payload_layout.ok(),
        .mipmap_plan_ready = mipmap_upload_plan.upload_plannable,
        .rows_aligned = true,
        .decoded_payload_available = payload_layout.decoded_payload.payload_valid,
        .mipmaps_referenced = mipmap_upload_plan.generated_mip_level_count > 1,
    };

    if (alignment_byte_count == 0) {
        plan.status = render_image_texture_staging_payload_plan_status::blocked_invalid_alignment;
        plan.status_name = render_image_texture_staging_payload_plan_status_name(plan.status);
        plan.blocker_summary = "staging row alignment must be non-zero";
        plan.diagnostic = "image texture staging payload plan blocked: " + plan.blocker_summary;
        return plan;
    }

    if (!plan.decoded_payload_available) {
        plan.status = render_image_texture_staging_payload_plan_status::blocked_missing_payload;
        plan.status_name = render_image_texture_staging_payload_plan_status_name(plan.status);
        plan.blocker_summary = "decoded payload bytes are missing or inconsistent";
        plan.diagnostic = "image texture staging payload plan blocked: " + plan.blocker_summary;
        return plan;
    }

    if (!payload_layout.ok()) {
        plan.status = render_image_texture_staging_payload_plan_status::blocked_invalid_layout;
        plan.status_name = render_image_texture_staging_payload_plan_status_name(plan.status);
        plan.blocker_summary = payload_layout.diagnostic;
        plan.diagnostic = "image texture staging payload plan blocked: " + plan.blocker_summary;
        return plan;
    }

    if (!mipmap_upload_plan.upload_plannable) {
        plan.status = render_image_texture_staging_payload_plan_status::blocked_invalid_mipmap_plan;
        plan.status_name = render_image_texture_staging_payload_plan_status_name(plan.status);
        plan.blocker_summary = "mipmap upload plan is not plannable: " + mipmap_upload_plan.status_name;
        plan.diagnostic = "image texture staging payload plan blocked: " + plan.blocker_summary;
        return plan;
    }
    if (mipmap_upload_plan.pixel_format != payload_layout.pixel_format
        || mipmap_upload_plan.bytes_per_pixel != payload_layout.bytes_per_pixel
        || mipmap_upload_plan.base_width != payload_layout.extent_width
        || mipmap_upload_plan.base_height != payload_layout.extent_height) {
        plan.status = render_image_texture_staging_payload_plan_status::blocked_invalid_mipmap_plan;
        plan.status_name = render_image_texture_staging_payload_plan_status_name(plan.status);
        plan.blocker_summary = "mipmap upload plan does not match payload layout";
        plan.diagnostic = "image texture staging payload plan blocked: " + plan.blocker_summary;
        return plan;
    }

    std::size_t level_staging_byte_offset = 0;
    for (const render_image_texture_mipmap_level_upload_plan& level : mipmap_upload_plan.levels) {
        std::size_t row_payload_byte_count = 0;
        if (!checked_render_image_texture_mipmap_upload_size(
                level.width,
                mipmap_upload_plan.bytes_per_pixel,
                row_payload_byte_count)) {
            plan.status = render_image_texture_staging_payload_plan_status::blocked_overflow;
            plan.status_name = render_image_texture_staging_payload_plan_status_name(plan.status);
            plan.blocker_summary = "staging row payload byte count overflowed";
            plan.diagnostic = "image texture staging payload plan blocked: " + plan.blocker_summary;
            return plan;
        }

        std::size_t staging_row_stride_byte_count = 0;
        if (!checked_render_image_texture_staging_row_stride(
                row_payload_byte_count,
                alignment_byte_count,
                staging_row_stride_byte_count)) {
            plan.status = render_image_texture_staging_payload_plan_status::blocked_overflow;
            plan.status_name = render_image_texture_staging_payload_plan_status_name(plan.status);
            plan.blocker_summary = "staging row alignment byte count overflowed";
            plan.diagnostic = "image texture staging payload plan blocked: " + plan.blocker_summary;
            return plan;
        }

        const std::size_t row_copy_begin = plan.row_copies.size();
        plan.mip_level_references.push_back(render_image_texture_staging_mip_level_reference{
            .mip_level = level.level,
            .width = level.width,
            .height = level.height,
            .pixel_count = level.pixel_count,
            .byte_count = level.byte_count,
            .mipmap_staging_byte_offset = level.staging_byte_offset,
            .staging_byte_offset = level_staging_byte_offset,
            .row_copy_begin = row_copy_begin,
            .row_copy_count = level.height,
            .base_level = level.level == 0,
            .decoded_payload_backed = level.level == 0,
            .generated_mip_reference = level.level != 0,
        });
        ++plan.mip_level_reference_count;

        if (level.level == 0) {
            plan.base_staging_row_stride_byte_count = staging_row_stride_byte_count;
            plan.base_row_padding_byte_count = staging_row_stride_byte_count - row_payload_byte_count;
        }

        for (std::size_t row_index = 0; row_index < level.height; ++row_index) {
            if (!append_render_image_texture_staging_row_copy_plan(
                    plan,
                    level,
                    row_index,
                    level_staging_byte_offset,
                    row_payload_byte_count,
                    staging_row_stride_byte_count)) {
                plan.status = render_image_texture_staging_payload_plan_status::blocked_overflow;
                plan.status_name = render_image_texture_staging_payload_plan_status_name(plan.status);
                plan.blocker_summary = "staging row copy byte count overflowed";
                plan.diagnostic = "image texture staging payload plan blocked: " + plan.blocker_summary;
                return plan;
            }
        }

        constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
        std::size_t level_staging_byte_count = 0;
        if (!checked_render_image_texture_mipmap_upload_size(
                level.height,
                staging_row_stride_byte_count,
                level_staging_byte_count)
            || level_staging_byte_offset > max_size - level_staging_byte_count) {
            plan.status = render_image_texture_staging_payload_plan_status::blocked_overflow;
            plan.status_name = render_image_texture_staging_payload_plan_status_name(plan.status);
            plan.blocker_summary = "staging mip level byte count overflowed";
            plan.diagnostic = "image texture staging payload plan blocked: " + plan.blocker_summary;
            return plan;
        }
        level_staging_byte_offset += level_staging_byte_count;
    }

    plan.ready = true;
    plan.blocked = false;
    plan.status = plan.mipmaps_referenced
        ? render_image_texture_staging_payload_plan_status::ready_with_mip_references
        : render_image_texture_staging_payload_plan_status::ready;
    plan.status_name = render_image_texture_staging_payload_plan_status_name(plan.status);
    plan.diagnostic = plan.mipmaps_referenced
        ? "image texture staging payload plan is ready with mip references"
        : "image texture staging payload plan is ready";
    return plan;
}

inline bool render_image_texture_staging_row_copy_plan_equal(
    const render_image_texture_staging_row_copy_plan& before,
    const render_image_texture_staging_row_copy_plan& after)
{
    return before.mip_level == after.mip_level
        && before.row_index == after.row_index
        && before.mip_width == after.mip_width
        && before.mip_height == after.mip_height
        && before.source_byte_offset == after.source_byte_offset
        && before.source_row_stride_byte_count == after.source_row_stride_byte_count
        && before.row_payload_byte_count == after.row_payload_byte_count
        && before.staging_byte_offset == after.staging_byte_offset
        && before.staging_row_stride_byte_count == after.staging_row_stride_byte_count
        && before.row_padding_byte_count == after.row_padding_byte_count
        && before.alignment_byte_count == after.alignment_byte_count
        && before.row_aligned == after.row_aligned
        && before.decoded_payload_backed == after.decoded_payload_backed
        && before.generated_mip_reference == after.generated_mip_reference;
}

inline bool render_image_texture_staging_mip_level_reference_equal(
    const render_image_texture_staging_mip_level_reference& before,
    const render_image_texture_staging_mip_level_reference& after)
{
    return before.mip_level == after.mip_level
        && before.width == after.width
        && before.height == after.height
        && before.pixel_count == after.pixel_count
        && before.byte_count == after.byte_count
        && before.mipmap_staging_byte_offset == after.mipmap_staging_byte_offset
        && before.staging_byte_offset == after.staging_byte_offset
        && before.row_copy_begin == after.row_copy_begin
        && before.row_copy_count == after.row_copy_count
        && before.base_level == after.base_level
        && before.decoded_payload_backed == after.decoded_payload_backed
        && before.generated_mip_reference == after.generated_mip_reference;
}

inline bool render_image_texture_staging_row_copy_plans_equal(
    const std::vector<render_image_texture_staging_row_copy_plan>& before,
    const std::vector<render_image_texture_staging_row_copy_plan>& after)
{
    if (before.size() != after.size()) {
        return false;
    }
    for (std::size_t index = 0; index < before.size(); ++index) {
        if (!render_image_texture_staging_row_copy_plan_equal(before[index], after[index])) {
            return false;
        }
    }
    return true;
}

inline bool render_image_texture_staging_mip_level_references_equal(
    const std::vector<render_image_texture_staging_mip_level_reference>& before,
    const std::vector<render_image_texture_staging_mip_level_reference>& after)
{
    if (before.size() != after.size()) {
        return false;
    }
    for (std::size_t index = 0; index < before.size(); ++index) {
        if (!render_image_texture_staging_mip_level_reference_equal(before[index], after[index])) {
            return false;
        }
    }
    return true;
}

inline bool render_image_texture_staging_payload_plan_equal(
    const render_image_texture_staging_payload_plan& before,
    const render_image_texture_staging_payload_plan& after)
{
    return before.status == after.status
        && before.texture_key == after.texture_key
        && before.sampler == after.sampler
        && before.stable_texture_cache_key == after.stable_texture_cache_key
        && before.sampler_summary == after.sampler_summary
        && before.pixel_format == after.pixel_format
        && before.extent_width == after.extent_width
        && before.extent_height == after.extent_height
        && before.bytes_per_pixel == after.bytes_per_pixel
        && before.alignment_byte_count == after.alignment_byte_count
        && before.base_row_stride_byte_count == after.base_row_stride_byte_count
        && before.base_staging_row_stride_byte_count == after.base_staging_row_stride_byte_count
        && before.base_row_padding_byte_count == after.base_row_padding_byte_count
        && before.row_copy_count == after.row_copy_count
        && before.mip_level_reference_count == after.mip_level_reference_count
        && before.total_row_payload_byte_count == after.total_row_payload_byte_count
        && before.total_row_padding_byte_count == after.total_row_padding_byte_count
        && before.total_staging_byte_count == after.total_staging_byte_count
        && before.decoded_byte_count == after.decoded_byte_count
        && before.referenced_mipmap_byte_count == after.referenced_mipmap_byte_count
        && before.referenced_mipmap_level_count == after.referenced_mipmap_level_count
        && before.decoded_payload_hash == after.decoded_payload_hash
        && render_image_texture_upload_payload_layout_evidence_equal(before.payload_layout, after.payload_layout)
        && render_image_texture_staging_row_copy_plans_equal(before.row_copies, after.row_copies)
        && render_image_texture_staging_mip_level_references_equal(
            before.mip_level_references,
            after.mip_level_references)
        && before.layout_ready == after.layout_ready
        && before.mipmap_plan_ready == after.mipmap_plan_ready
        && before.rows_aligned == after.rows_aligned
        && before.has_row_padding == after.has_row_padding
        && before.decoded_payload_available == after.decoded_payload_available
        && before.mipmaps_referenced == after.mipmaps_referenced
        && before.ready == after.ready
        && before.blocked == after.blocked
        && before.blocker_summary == after.blocker_summary;
}

enum class render_image_texture_staging_payload_plan_diff_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string render_image_texture_staging_payload_plan_diff_status_name(
    render_image_texture_staging_payload_plan_diff_status status)
{
    switch (status) {
    case render_image_texture_staging_payload_plan_diff_status::unchanged:
        return "unchanged";
    case render_image_texture_staging_payload_plan_diff_status::added:
        return "added";
    case render_image_texture_staging_payload_plan_diff_status::removed:
        return "removed";
    case render_image_texture_staging_payload_plan_diff_status::changed:
        return "changed";
    }

    return "unknown";
}

struct render_image_texture_staging_payload_plan_diff {
    render_image_texture_staging_payload_plan_diff_status status =
        render_image_texture_staging_payload_plan_diff_status::unchanged;
    std::string status_name = render_image_texture_staging_payload_plan_diff_status_name(
        render_image_texture_staging_payload_plan_diff_status::unchanged);
    bool before_present = false;
    bool after_present = false;
    render_image_texture_staging_payload_plan_status before_plan_status =
        render_image_texture_staging_payload_plan_status::blocked_missing_payload;
    render_image_texture_staging_payload_plan_status after_plan_status =
        render_image_texture_staging_payload_plan_status::blocked_missing_payload;
    std::string before_plan_status_name;
    std::string after_plan_status_name;
    std::string before_stable_texture_cache_key;
    std::string after_stable_texture_cache_key;
    std::string before_sampler_summary;
    std::string after_sampler_summary;
    std::size_t before_row_copy_count = 0;
    std::size_t after_row_copy_count = 0;
    std::int64_t row_copy_count_delta = 0;
    std::size_t before_alignment_byte_count = 0;
    std::size_t after_alignment_byte_count = 0;
    std::int64_t alignment_byte_delta = 0;
    std::size_t before_total_row_padding_byte_count = 0;
    std::size_t after_total_row_padding_byte_count = 0;
    std::int64_t row_padding_byte_delta = 0;
    std::size_t before_total_staging_byte_count = 0;
    std::size_t after_total_staging_byte_count = 0;
    std::int64_t total_staging_byte_delta = 0;
    std::size_t before_mip_level_reference_count = 0;
    std::size_t after_mip_level_reference_count = 0;
    std::int64_t mip_level_reference_delta = 0;
    bool before_ready = false;
    bool after_ready = false;
    bool before_blocked = false;
    bool after_blocked = false;
    bool before_mipmap_plan_ready = false;
    bool after_mipmap_plan_ready = false;
    bool before_mipmaps_referenced = false;
    bool after_mipmaps_referenced = false;
    std::string before_blocker_summary;
    std::string after_blocker_summary;
    bool row_copy_count_changed = false;
    bool alignment_changed = false;
    bool padding_changed = false;
    bool total_staging_byte_count_changed = false;
    bool cache_key_changed = false;
    bool sampler_changed = false;
    bool mip_level_readiness_changed = false;
    bool blocker_changed = false;
    bool ready_regressed = false;
    bool ready_recovered = false;
    bool regression = false;
    bool recovery = false;
    std::string change_summary;
    std::string diagnostic;

    bool changed() const
    {
        return status != render_image_texture_staging_payload_plan_diff_status::unchanged;
    }

    bool ok() const
    {
        return !regression;
    }
};

inline std::int64_t render_image_texture_staging_payload_plan_size_delta(
    std::size_t before_value,
    std::size_t after_value)
{
    constexpr auto max_delta = static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max());
    if (after_value >= before_value) {
        const std::size_t magnitude = after_value - before_value;
        return magnitude > max_delta ? std::numeric_limits<std::int64_t>::max()
                                     : static_cast<std::int64_t>(magnitude);
    }

    const std::size_t magnitude = before_value - after_value;
    return magnitude > max_delta ? std::numeric_limits<std::int64_t>::min()
                                 : -static_cast<std::int64_t>(magnitude);
}

inline void append_render_image_texture_staging_payload_plan_diff_summary(
    std::string& summary,
    const std::string& fragment)
{
    if (fragment.empty()) {
        return;
    }
    if (!summary.empty()) {
        summary += ",";
    }
    summary += fragment;
}

inline render_image_texture_staging_payload_plan_diff
make_render_image_texture_staging_payload_plan_diff(
    const render_image_texture_staging_payload_plan* before,
    const render_image_texture_staging_payload_plan* after)
{
    render_image_texture_staging_payload_plan_diff diff{
        .before_present = before != nullptr,
        .after_present = after != nullptr,
    };

    if (before != nullptr) {
        diff.before_plan_status = before->status;
        diff.before_plan_status_name = before->status_name;
        diff.before_stable_texture_cache_key = before->stable_texture_cache_key;
        diff.before_sampler_summary = before->sampler_summary;
        diff.before_row_copy_count = before->row_copy_count;
        diff.before_alignment_byte_count = before->alignment_byte_count;
        diff.before_total_row_padding_byte_count = before->total_row_padding_byte_count;
        diff.before_total_staging_byte_count = before->total_staging_byte_count;
        diff.before_mip_level_reference_count = before->mip_level_reference_count;
        diff.before_ready = before->ready;
        diff.before_blocked = before->blocked;
        diff.before_mipmap_plan_ready = before->mipmap_plan_ready;
        diff.before_mipmaps_referenced = before->mipmaps_referenced;
        diff.before_blocker_summary = before->blocker_summary;
    }
    if (after != nullptr) {
        diff.after_plan_status = after->status;
        diff.after_plan_status_name = after->status_name;
        diff.after_stable_texture_cache_key = after->stable_texture_cache_key;
        diff.after_sampler_summary = after->sampler_summary;
        diff.after_row_copy_count = after->row_copy_count;
        diff.after_alignment_byte_count = after->alignment_byte_count;
        diff.after_total_row_padding_byte_count = after->total_row_padding_byte_count;
        diff.after_total_staging_byte_count = after->total_staging_byte_count;
        diff.after_mip_level_reference_count = after->mip_level_reference_count;
        diff.after_ready = after->ready;
        diff.after_blocked = after->blocked;
        diff.after_mipmap_plan_ready = after->mipmap_plan_ready;
        diff.after_mipmaps_referenced = after->mipmaps_referenced;
        diff.after_blocker_summary = after->blocker_summary;
    }

    diff.row_copy_count_delta = render_image_texture_staging_payload_plan_size_delta(
        diff.before_row_copy_count,
        diff.after_row_copy_count);
    diff.alignment_byte_delta = render_image_texture_staging_payload_plan_size_delta(
        diff.before_alignment_byte_count,
        diff.after_alignment_byte_count);
    diff.row_padding_byte_delta = render_image_texture_staging_payload_plan_size_delta(
        diff.before_total_row_padding_byte_count,
        diff.after_total_row_padding_byte_count);
    diff.total_staging_byte_delta = render_image_texture_staging_payload_plan_size_delta(
        diff.before_total_staging_byte_count,
        diff.after_total_staging_byte_count);
    diff.mip_level_reference_delta = render_image_texture_staging_payload_plan_size_delta(
        diff.before_mip_level_reference_count,
        diff.after_mip_level_reference_count);
    diff.row_copy_count_changed = diff.before_row_copy_count != diff.after_row_copy_count;
    diff.alignment_changed = diff.before_alignment_byte_count != diff.after_alignment_byte_count;
    diff.padding_changed = diff.before_total_row_padding_byte_count != diff.after_total_row_padding_byte_count;
    diff.total_staging_byte_count_changed =
        diff.before_total_staging_byte_count != diff.after_total_staging_byte_count;
    diff.cache_key_changed = diff.before_stable_texture_cache_key != diff.after_stable_texture_cache_key;
    diff.sampler_changed = diff.before_sampler_summary != diff.after_sampler_summary;
    diff.mip_level_readiness_changed = diff.before_mipmap_plan_ready != diff.after_mipmap_plan_ready
        || diff.before_mipmaps_referenced != diff.after_mipmaps_referenced
        || diff.before_mip_level_reference_count != diff.after_mip_level_reference_count;
    diff.blocker_changed = diff.before_blocked != diff.after_blocked
        || diff.before_blocker_summary != diff.after_blocker_summary;
    diff.ready_regressed = before != nullptr && before->ready && (after == nullptr || !after->ready);
    diff.ready_recovered = before != nullptr && after != nullptr && !before->ready && after->ready;
    diff.regression = diff.ready_regressed
        || (before != nullptr && after != nullptr && !before->blocked && after->blocked);
    diff.recovery = diff.ready_recovered
        || (before != nullptr && after != nullptr && before->blocked && !after->blocked);

    if (before == nullptr && after != nullptr) {
        diff.status = render_image_texture_staging_payload_plan_diff_status::added;
    } else if (before != nullptr && after == nullptr) {
        diff.status = render_image_texture_staging_payload_plan_diff_status::removed;
    } else if (before != nullptr && after != nullptr
        && !render_image_texture_staging_payload_plan_equal(*before, *after)) {
        diff.status = render_image_texture_staging_payload_plan_diff_status::changed;
    } else {
        diff.status = render_image_texture_staging_payload_plan_diff_status::unchanged;
    }
    diff.status_name = render_image_texture_staging_payload_plan_diff_status_name(diff.status);

    if (diff.total_staging_byte_count_changed) {
        append_render_image_texture_staging_payload_plan_diff_summary(
            diff.change_summary,
            "staging_bytes=" + std::to_string(diff.before_total_staging_byte_count)
                + "->" + std::to_string(diff.after_total_staging_byte_count));
    }
    if (diff.row_copy_count_changed) {
        append_render_image_texture_staging_payload_plan_diff_summary(
            diff.change_summary,
            "rows=" + std::to_string(diff.before_row_copy_count)
                + "->" + std::to_string(diff.after_row_copy_count));
    }
    if (diff.alignment_changed) {
        append_render_image_texture_staging_payload_plan_diff_summary(
            diff.change_summary,
            "alignment=" + std::to_string(diff.before_alignment_byte_count)
                + "->" + std::to_string(diff.after_alignment_byte_count));
    }
    if (diff.padding_changed) {
        append_render_image_texture_staging_payload_plan_diff_summary(
            diff.change_summary,
            "row_padding=" + std::to_string(diff.before_total_row_padding_byte_count)
                + "->" + std::to_string(diff.after_total_row_padding_byte_count));
    }
    if (diff.cache_key_changed) {
        append_render_image_texture_staging_payload_plan_diff_summary(
            diff.change_summary,
            "cache_key_changed");
    }
    if (diff.sampler_changed) {
        append_render_image_texture_staging_payload_plan_diff_summary(
            diff.change_summary,
            "sampler_changed");
    }
    if (diff.mip_level_readiness_changed) {
        append_render_image_texture_staging_payload_plan_diff_summary(
            diff.change_summary,
            "mip_levels=" + std::to_string(diff.before_mip_level_reference_count)
                + "->" + std::to_string(diff.after_mip_level_reference_count));
    }
    if (diff.blocker_changed) {
        const std::string before_blocker =
            diff.before_blocked ? diff.before_plan_status_name : std::string{"ready"};
        const std::string after_blocker =
            diff.after_blocked ? diff.after_plan_status_name : std::string{"ready"};
        append_render_image_texture_staging_payload_plan_diff_summary(
            diff.change_summary,
            before_blocker == after_blocker
                ? std::string{"blocker_summary_changed"}
                : "blocker=" + before_blocker + "->" + after_blocker);
    }
    if (diff.status == render_image_texture_staging_payload_plan_diff_status::added) {
        append_render_image_texture_staging_payload_plan_diff_summary(diff.change_summary, "added");
    } else if (diff.status == render_image_texture_staging_payload_plan_diff_status::removed) {
        append_render_image_texture_staging_payload_plan_diff_summary(diff.change_summary, "removed");
    } else if (diff.change_summary.empty()) {
        diff.change_summary = "no staging payload plan changes";
    }

    if (diff.status == render_image_texture_staging_payload_plan_diff_status::unchanged) {
        diff.diagnostic = "image texture staging payload plan unchanged";
    } else if (diff.regression) {
        diff.diagnostic = "image texture staging payload plan changed with regression";
    } else if (diff.recovery) {
        diff.diagnostic = "image texture staging payload plan changed with recovery";
    } else if (diff.blocker_changed) {
        diff.diagnostic = "image texture staging payload plan changed blocker state";
    } else if (diff.total_staging_byte_count_changed) {
        diff.diagnostic = "image texture staging payload plan changed staging bytes";
    } else {
        diff.diagnostic = "image texture staging payload plan changed";
    }
    return diff;
}

inline render_image_texture_staging_payload_plan_diff
diff_render_image_texture_staging_payload_plans(
    const render_image_texture_staging_payload_plan& before,
    const render_image_texture_staging_payload_plan& after)
{
    return make_render_image_texture_staging_payload_plan_diff(&before, &after);
}

struct render_image_texture_upload_result {
    render_image_texture_upload_status status = render_image_texture_upload_status::invalid_image;
    std::uint64_t generation_id = 0;
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    render_image_texture_handle texture;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    render_image_texture_mipmap_upload_plan mipmap_upload_plan;
    render_image_texture_upload_payload_layout_evidence payload_layout;
    render_image_texture_staging_payload_plan staging_payload_plan;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_texture_upload_status::uploaded && texture.valid();
    }
};

class image_texture_uploader_interface {
public:
    virtual ~image_texture_uploader_interface() = default;

    virtual render_image_texture_upload_result upload(
        const render_image_texture_upload_request& request) = 0;
};

struct fake_image_texture_upload_request_snapshot {
    fake_image_texture_upload_generation_id generation_id = 0;
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    std::size_t width = 0;
    std::size_t height = 0;
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    render_image_decoded_payload_evidence decoded_payload;
    render_image_texture_upload_payload_layout_evidence payload_layout;
    render_image_texture_staging_payload_plan staging_payload_plan;
    render_image_texture_mipmap_upload_plan mipmap_upload_plan;
    std::size_t enqueue_sequence = 0;
    std::size_t queue_depth_before_enqueue = 0;
    std::size_t queue_depth_after_enqueue = 0;
    std::size_t attempt_count_for_key = 0;
};

struct fake_image_texture_upload_result_snapshot {
    fake_image_texture_upload_generation_id generation_id = 0;
    render_image_texture_upload_status status = render_image_texture_upload_status::invalid_image;
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    render_image_texture_handle texture;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    render_image_texture_mipmap_upload_plan mipmap_upload_plan;
    render_image_texture_upload_payload_layout_evidence payload_layout;
    render_image_texture_staging_payload_plan staging_payload_plan;
    std::string diagnostic;
    std::size_t completion_sequence = 0;
    std::size_t queue_depth_after_completion = 0;
    fake_image_texture_upload_retry_snapshot retry;
};

struct fake_image_texture_upload_snapshot_entry {
    fake_image_texture_upload_generation_id generation_id = 0;
    fake_image_texture_upload_request_snapshot request;
    fake_image_texture_upload_result_snapshot result;
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    render_image_texture_handle texture;
    render_image_texture_upload_status status = render_image_texture_upload_status::invalid_image;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    render_image_texture_mipmap_upload_plan mipmap_upload_plan;
    render_image_texture_upload_payload_layout_evidence payload_layout;
    render_image_texture_staging_payload_plan staging_payload_plan;
    std::string diagnostic;
    fake_image_texture_upload_retry_snapshot retry;
};

struct fake_image_texture_upload_queue_entry_snapshot {
    std::size_t enqueue_sequence = 0;
    std::size_t completion_sequence = 0;
    fake_image_texture_upload_generation_id generation_id = 0;
    render_image_texture_key key;
    render_image_texture_upload_status status = render_image_texture_upload_status::invalid_image;
    render_image_texture_handle texture;
    bool completed = false;
    std::size_t staging_byte_count = 0;
    render_image_texture_mipmap_upload_plan mipmap_upload_plan;
    render_image_texture_upload_payload_layout_evidence payload_layout;
    render_image_texture_staging_payload_plan staging_payload_plan;
    std::size_t queue_depth_before_enqueue = 0;
    std::size_t queue_depth_after_enqueue = 0;
    std::size_t queue_depth_after_completion = 0;
    std::string diagnostic;
    fake_image_texture_upload_retry_snapshot retry;
};

struct fake_image_texture_upload_snapshot {
    std::size_t upload_count = 0;
    std::size_t failed_upload_count = 0;
    std::size_t uploaded_pixel_count = 0;
    std::size_t uploaded_pixel_byte_count = 0;
    std::size_t uploaded_decoded_byte_count = 0;
    std::size_t staged_byte_count = 0;
    std::size_t attempted_staging_byte_count = 0;
    fake_image_texture_upload_generation_id next_generation_id = 1;
    std::vector<fake_image_texture_upload_request_snapshot> request_snapshots;
    std::vector<fake_image_texture_upload_result_snapshot> result_snapshots;
    std::vector<fake_image_texture_upload_snapshot_entry> entries;
    std::size_t enqueued_upload_count = 0;
    std::size_t completed_upload_count = 0;
    std::size_t pending_upload_count = 0;
    std::size_t next_queue_sequence = 1;
    std::vector<fake_image_texture_upload_queue_entry_snapshot> queue_entries;
    std::size_t retryable_upload_count = 0;
    std::size_t nonretryable_upload_count = 0;
    std::size_t completed_without_retry_count = 0;
    std::vector<fake_image_texture_upload_retry_snapshot> retry_snapshots;
};

} // namespace quiz_vulkan::render

#include "render/image/image_texture_upload_snapshot_diff.h"
#include "render/image/image_texture_upload_operation_plan.h"
#include "render/image/image_texture_upload_result_diagnostics.h"

namespace quiz_vulkan::render {

class fake_image_texture_uploader final : public image_texture_uploader_interface {
public:
    render_image_texture_upload_result upload(const render_image_texture_upload_request& request) override
    {
        upload_requests.push_back(request);

        const fake_image_texture_upload_generation_id generation_id = next_generation_id_++;
        const std::size_t enqueue_sequence = next_upload_queue_sequence_++;
        const std::size_t queue_depth_before_enqueue = pending_upload_count_;
        ++pending_upload_count_;
        const std::size_t queue_depth_after_enqueue = pending_upload_count_;
        const std::size_t attempt_count_for_key = ++upload_attempt_count_by_key_[request.key];
        const std::size_t pixel_byte_count = expected_render_decoded_image_byte_count(request.image);
        const std::size_t decoded_byte_count = request.image.pixels.size();
        const render_image_texture_mipmap_upload_plan mipmap_upload_plan =
            make_render_image_texture_mipmap_upload_plan(request.image, request.sampler);
        const render_image_texture_upload_payload_layout_evidence payload_layout =
            make_render_image_texture_upload_payload_layout_evidence(
                request.key,
                request.sampler,
                request.image);
        const render_image_texture_staging_payload_plan staging_payload_plan =
            make_render_image_texture_staging_payload_plan(payload_layout, mipmap_upload_plan);
        std::size_t pixel_count = 0;
        if (request.image.width != 0 && request.image.height != 0
            && request.image.width <= std::numeric_limits<std::size_t>::max() / request.image.height) {
            pixel_count = request.image.width * request.image.height;
        }
        const std::size_t staging_byte_count = has_valid_render_decoded_image_payload(request.image)
                && mipmap_upload_plan.upload_plannable
            ? mipmap_upload_plan.total_staging_byte_count
            : 0;

        upload_request_snapshots.push_back(fake_image_texture_upload_request_snapshot{
            .generation_id = generation_id,
            .key = request.key,
            .sampler = request.sampler,
            .width = request.image.width,
            .height = request.image.height,
            .pixel_format = request.image.pixel_format,
            .pixel_count = pixel_count,
            .pixel_byte_count = pixel_byte_count,
            .decoded_byte_count = decoded_byte_count,
            .staging_byte_count = staging_byte_count,
            .decoded_payload = make_render_image_decoded_payload_evidence(request.image),
            .payload_layout = payload_layout,
            .staging_payload_plan = staging_payload_plan,
            .mipmap_upload_plan = mipmap_upload_plan,
            .enqueue_sequence = enqueue_sequence,
            .queue_depth_before_enqueue = queue_depth_before_enqueue,
            .queue_depth_after_enqueue = queue_depth_after_enqueue,
            .attempt_count_for_key = attempt_count_for_key,
        });

        if (!is_valid_render_image_texture_key(request.key)) {
            return record_result(render_image_texture_upload_result{
                .status = render_image_texture_upload_status::invalid_key,
                .generation_id = generation_id,
                .key = request.key,
                .sampler = request.sampler,
                .texture = {},
                .pixel_count = pixel_count,
                .pixel_byte_count = pixel_byte_count,
                .decoded_byte_count = decoded_byte_count,
                .staging_byte_count = staging_byte_count,
                .mipmap_upload_plan = mipmap_upload_plan,
                .payload_layout = payload_layout,
                .staging_payload_plan = staging_payload_plan,
                .diagnostic = "image texture upload key is empty or contains control characters",
            },
                enqueue_sequence,
                queue_depth_after_enqueue,
                attempt_count_for_key);
        }

        if (request.key.sampler != request.sampler || !is_valid_render_image_sampler_policy(request.sampler)) {
            return record_result(render_image_texture_upload_result{
                .status = render_image_texture_upload_status::invalid_sampler,
                .generation_id = generation_id,
                .key = request.key,
                .sampler = request.sampler,
                .texture = {},
                .pixel_count = pixel_count,
                .pixel_byte_count = pixel_byte_count,
                .decoded_byte_count = decoded_byte_count,
                .staging_byte_count = staging_byte_count,
                .mipmap_upload_plan = mipmap_upload_plan,
                .payload_layout = payload_layout,
                .staging_payload_plan = staging_payload_plan,
                .diagnostic = "image texture upload sampler policy is invalid or does not match the texture key",
            },
                enqueue_sequence,
                queue_depth_after_enqueue,
                attempt_count_for_key);
        }

        if (render_image_pixel_format_byte_count(request.image.pixel_format) == 0) {
            return record_result(render_image_texture_upload_result{
                .status = render_image_texture_upload_status::unsupported_format,
                .generation_id = generation_id,
                .key = request.key,
                .sampler = request.sampler,
                .texture = {},
                .pixel_count = pixel_count,
                .pixel_byte_count = pixel_byte_count,
                .decoded_byte_count = decoded_byte_count,
                .staging_byte_count = staging_byte_count,
                .mipmap_upload_plan = mipmap_upload_plan,
                .payload_layout = payload_layout,
                .staging_payload_plan = staging_payload_plan,
                .diagnostic = "image texture upload pixel format is unsupported",
            },
                enqueue_sequence,
                queue_depth_after_enqueue,
                attempt_count_for_key);
        }

        if (!has_valid_render_decoded_image_payload(request.image)) {
            return record_result(render_image_texture_upload_result{
                .status = render_image_texture_upload_status::invalid_image,
                .generation_id = generation_id,
                .key = request.key,
                .sampler = request.sampler,
                .texture = {},
                .pixel_count = pixel_count,
                .pixel_byte_count = pixel_byte_count,
                .decoded_byte_count = decoded_byte_count,
                .staging_byte_count = staging_byte_count,
                .mipmap_upload_plan = mipmap_upload_plan,
                .payload_layout = payload_layout,
                .staging_payload_plan = staging_payload_plan,
                .diagnostic = "image texture upload payload size does not match dimensions and format",
            },
                enqueue_sequence,
                queue_depth_after_enqueue,
                attempt_count_for_key);
        }

        return record_result(render_image_texture_upload_result{
            .status = render_image_texture_upload_status::uploaded,
            .generation_id = generation_id,
            .key = request.key,
            .sampler = request.sampler,
            .texture = render_image_texture_handle{
                .id = next_id_++,
                .revision = 1,
                .width = request.image.width,
                .height = request.image.height,
            },
            .pixel_count = pixel_count,
            .pixel_byte_count = pixel_byte_count,
            .decoded_byte_count = decoded_byte_count,
            .staging_byte_count = staging_byte_count,
            .mipmap_upload_plan = mipmap_upload_plan,
            .payload_layout = payload_layout,
            .staging_payload_plan = staging_payload_plan,
            .diagnostic = {},
        },
            enqueue_sequence,
            queue_depth_after_enqueue,
            attempt_count_for_key);
    }

    fake_image_texture_upload_snapshot diagnostic_snapshot() const
    {
        fake_image_texture_upload_snapshot snapshot{
            .upload_count = upload_results.size(),
            .failed_upload_count = failed_upload_count_,
            .uploaded_pixel_count = uploaded_pixel_count_,
            .uploaded_pixel_byte_count = uploaded_pixel_byte_count_,
            .uploaded_decoded_byte_count = uploaded_decoded_byte_count_,
            .staged_byte_count = staged_byte_count_,
            .attempted_staging_byte_count = attempted_staging_byte_count_,
            .next_generation_id = next_generation_id_,
            .request_snapshots = upload_request_snapshots,
            .result_snapshots = upload_result_snapshots,
            .entries = {},
            .enqueued_upload_count = upload_requests.size(),
            .completed_upload_count = upload_results.size(),
            .pending_upload_count = pending_upload_count_,
            .next_queue_sequence = next_upload_queue_sequence_,
            .queue_entries = upload_queue_entries_,
            .retryable_upload_count = retryable_upload_count_,
            .nonretryable_upload_count = nonretryable_upload_count_,
            .completed_without_retry_count = completed_without_retry_count_,
            .retry_snapshots = upload_retry_snapshots_,
        };
        snapshot.entries.reserve(upload_results.size());
        for (std::size_t index = 0; index < upload_results.size(); ++index) {
            const render_image_texture_upload_result& result = upload_results[index];
            snapshot.entries.push_back(fake_image_texture_upload_snapshot_entry{
                .generation_id = result.generation_id,
                .request = upload_request_snapshots[index],
                .result = upload_result_snapshots[index],
                .key = result.key,
                .sampler = result.sampler,
                .texture = result.texture,
                .status = result.status,
                .pixel_count = result.pixel_count,
                .pixel_byte_count = result.pixel_byte_count,
                .decoded_byte_count = result.decoded_byte_count,
                .staging_byte_count = result.staging_byte_count,
                .mipmap_upload_plan = result.mipmap_upload_plan,
                .payload_layout = result.payload_layout,
                .staging_payload_plan = result.staging_payload_plan,
                .diagnostic = result.diagnostic,
                .retry = upload_result_snapshots[index].retry,
            });
        }
        return snapshot;
    }

    std::vector<render_image_texture_upload_request> upload_requests;
    std::vector<fake_image_texture_upload_request_snapshot> upload_request_snapshots;
    std::vector<fake_image_texture_upload_result_snapshot> upload_result_snapshots;
    std::vector<render_image_texture_upload_result> upload_results;

private:
    render_image_texture_upload_result record_result(
        render_image_texture_upload_result result,
        std::size_t enqueue_sequence,
        std::size_t queue_depth_after_enqueue,
        std::size_t attempt_count_for_key)
    {
        attempted_staging_byte_count_ += result.staging_byte_count;
        if (result.ok()) {
            uploaded_pixel_count_ += result.pixel_count;
            uploaded_pixel_byte_count_ += result.pixel_byte_count;
            uploaded_decoded_byte_count_ += result.decoded_byte_count;
            staged_byte_count_ += result.staging_byte_count;
        } else {
            ++failed_upload_count_;
        }

        const std::size_t completion_sequence = next_upload_queue_sequence_++;
        if (pending_upload_count_ != 0) {
            --pending_upload_count_;
        }
        const std::size_t queue_depth_after_completion = pending_upload_count_;
        const fake_image_texture_upload_retry_snapshot retry = make_retry_snapshot(
            result,
            attempt_count_for_key,
            completion_sequence);

        upload_result_snapshots.push_back(fake_image_texture_upload_result_snapshot{
            .generation_id = result.generation_id,
            .status = result.status,
            .key = result.key,
            .sampler = result.sampler,
            .texture = result.texture,
            .pixel_count = result.pixel_count,
            .pixel_byte_count = result.pixel_byte_count,
            .decoded_byte_count = result.decoded_byte_count,
            .staging_byte_count = result.staging_byte_count,
            .mipmap_upload_plan = result.mipmap_upload_plan,
            .payload_layout = result.payload_layout,
            .staging_payload_plan = result.staging_payload_plan,
            .diagnostic = result.diagnostic,
            .completion_sequence = completion_sequence,
            .queue_depth_after_completion = queue_depth_after_completion,
            .retry = retry,
        });
        upload_queue_entries_.push_back(fake_image_texture_upload_queue_entry_snapshot{
            .enqueue_sequence = enqueue_sequence,
            .completion_sequence = completion_sequence,
            .generation_id = result.generation_id,
            .key = result.key,
            .status = result.status,
            .texture = result.texture,
            .completed = true,
            .staging_byte_count = result.staging_byte_count,
            .mipmap_upload_plan = result.mipmap_upload_plan,
            .payload_layout = result.payload_layout,
            .staging_payload_plan = result.staging_payload_plan,
            .queue_depth_before_enqueue = queue_depth_after_enqueue == 0 ? 0 : queue_depth_after_enqueue - 1,
            .queue_depth_after_enqueue = queue_depth_after_enqueue,
            .queue_depth_after_completion = queue_depth_after_completion,
            .diagnostic = result.diagnostic,
            .retry = retry,
        });
        upload_retry_snapshots_.push_back(retry);
        upload_results.push_back(result);
        return result;
    }

    fake_image_texture_upload_retry_snapshot make_retry_snapshot(
        const render_image_texture_upload_result& result,
        std::size_t attempt_count_for_key,
        std::size_t completion_sequence)
    {
        std::size_t failed_attempt_count_for_key = failed_upload_attempt_count_by_key_[result.key];
        if (!result.ok()) {
            failed_attempt_count_for_key = ++failed_upload_attempt_count_by_key_[result.key];
        }

        if (result.ok()) {
            ++completed_without_retry_count_;
        } else if (is_retryable_render_image_texture_upload_status(result.status)) {
            ++retryable_upload_count_;
        } else {
            ++nonretryable_upload_count_;
        }

        return detail::make_fake_image_texture_upload_retry_snapshot(
            result.generation_id,
            result.key,
            result.status,
            result.ok(),
            attempt_count_for_key,
            failed_attempt_count_for_key,
            completion_sequence);
    }

    render_image_texture_id next_id_ = 1;
    fake_image_texture_upload_generation_id next_generation_id_ = 1;
    std::size_t next_upload_queue_sequence_ = 1;
    std::size_t pending_upload_count_ = 0;
    std::size_t failed_upload_count_ = 0;
    std::size_t retryable_upload_count_ = 0;
    std::size_t nonretryable_upload_count_ = 0;
    std::size_t completed_without_retry_count_ = 0;
    std::size_t uploaded_pixel_count_ = 0;
    std::size_t uploaded_pixel_byte_count_ = 0;
    std::size_t uploaded_decoded_byte_count_ = 0;
    std::size_t staged_byte_count_ = 0;
    std::size_t attempted_staging_byte_count_ = 0;
    std::map<render_image_texture_key, std::size_t, render_image_texture_key_less> upload_attempt_count_by_key_;
    std::map<render_image_texture_key, std::size_t, render_image_texture_key_less> failed_upload_attempt_count_by_key_;
    std::vector<fake_image_texture_upload_queue_entry_snapshot> upload_queue_entries_;
    std::vector<fake_image_texture_upload_retry_snapshot> upload_retry_snapshots_;
};

} // namespace quiz_vulkan::render
