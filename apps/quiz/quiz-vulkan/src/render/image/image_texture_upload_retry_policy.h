#pragma once

#include "render/image/image_types.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace quiz_vulkan::render {

enum class render_image_texture_upload_status {
    uploaded,
    invalid_key,
    invalid_sampler,
    unsupported_format,
    invalid_image,
};

using fake_image_texture_upload_generation_id = std::uint64_t;

enum class fake_image_texture_upload_retry_eligibility {
    not_needed,
    eligible,
    ineligible,
};

inline std::string fake_image_texture_upload_retry_eligibility_name(
    fake_image_texture_upload_retry_eligibility eligibility)
{
    switch (eligibility) {
    case fake_image_texture_upload_retry_eligibility::not_needed:
        return "not_needed";
    case fake_image_texture_upload_retry_eligibility::eligible:
        return "eligible";
    case fake_image_texture_upload_retry_eligibility::ineligible:
        return "ineligible";
    }

    return "unknown";
}

inline bool is_retryable_render_image_texture_upload_status(
    render_image_texture_upload_status status)
{
    return status == render_image_texture_upload_status::invalid_image;
}

inline std::size_t fake_image_texture_upload_retry_backoff_sequence_delta(
    std::size_t failed_attempt_count_for_key)
{
    if (failed_attempt_count_for_key == 0) {
        return 0;
    }

    constexpr std::size_t max_backoff = 16;
    if (failed_attempt_count_for_key > 5) {
        return max_backoff;
    }

    const std::size_t backoff = std::size_t{1} << (failed_attempt_count_for_key - 1);
    return backoff < max_backoff ? backoff : max_backoff;
}

struct fake_image_texture_upload_retry_snapshot {
    fake_image_texture_upload_generation_id generation_id = 0;
    render_image_texture_key key;
    render_image_texture_upload_status status = render_image_texture_upload_status::invalid_image;
    fake_image_texture_upload_retry_eligibility eligibility =
        fake_image_texture_upload_retry_eligibility::ineligible;
    std::size_t attempt_count_for_key = 0;
    std::size_t failed_attempt_count_for_key = 0;
    std::size_t retry_after_queue_sequence_delta = 0;
    std::size_t next_retry_sequence = 0;
    std::string diagnostic;
};

namespace detail {

inline fake_image_texture_upload_retry_snapshot make_fake_image_texture_upload_retry_snapshot(
    fake_image_texture_upload_generation_id generation_id,
    const render_image_texture_key& key,
    render_image_texture_upload_status status,
    bool upload_succeeded,
    std::size_t attempt_count_for_key,
    std::size_t failed_attempt_count_for_key,
    std::size_t completion_sequence)
{
    if (upload_succeeded) {
        return fake_image_texture_upload_retry_snapshot{
            .generation_id = generation_id,
            .key = key,
            .status = status,
            .eligibility = fake_image_texture_upload_retry_eligibility::not_needed,
            .attempt_count_for_key = attempt_count_for_key,
            .failed_attempt_count_for_key = failed_attempt_count_for_key,
            .retry_after_queue_sequence_delta = 0,
            .next_retry_sequence = 0,
            .diagnostic = "image texture upload succeeded; retry is not needed",
        };
    }

    if (is_retryable_render_image_texture_upload_status(status)) {
        const std::size_t backoff = fake_image_texture_upload_retry_backoff_sequence_delta(
            failed_attempt_count_for_key);
        return fake_image_texture_upload_retry_snapshot{
            .generation_id = generation_id,
            .key = key,
            .status = status,
            .eligibility = fake_image_texture_upload_retry_eligibility::eligible,
            .attempt_count_for_key = attempt_count_for_key,
            .failed_attempt_count_for_key = failed_attempt_count_for_key,
            .retry_after_queue_sequence_delta = backoff,
            .next_retry_sequence = completion_sequence + backoff,
            .diagnostic = "image texture upload can retry after decoded payload changes",
        };
    }

    return fake_image_texture_upload_retry_snapshot{
        .generation_id = generation_id,
        .key = key,
        .status = status,
        .eligibility = fake_image_texture_upload_retry_eligibility::ineligible,
        .attempt_count_for_key = attempt_count_for_key,
        .failed_attempt_count_for_key = failed_attempt_count_for_key,
        .retry_after_queue_sequence_delta = 0,
        .next_retry_sequence = 0,
        .diagnostic = "image texture upload failure is structural and is not retryable",
    };
}

} // namespace detail

} // namespace quiz_vulkan::render
