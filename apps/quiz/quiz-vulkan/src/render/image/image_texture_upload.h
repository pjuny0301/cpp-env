#pragma once

#include "render/image/image_texture_diagnostics.h"
#include "render/image/image_texture_upload_retry_policy.h"
#include "render/image/image_types.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

struct render_image_texture_upload_request {
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    render_decoded_image image;
};

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
        std::size_t pixel_count = 0;
        if (request.image.width != 0 && request.image.height != 0
            && request.image.width <= std::numeric_limits<std::size_t>::max() / request.image.height) {
            pixel_count = request.image.width * request.image.height;
        }
        const std::size_t staging_byte_count = has_valid_render_decoded_image_payload(request.image)
            ? decoded_byte_count
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
