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
