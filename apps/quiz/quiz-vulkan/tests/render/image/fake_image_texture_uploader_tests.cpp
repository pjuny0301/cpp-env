#include "render/image/image_texture_cache.h"

#include <cassert>
#include <cstddef>
#include <cstdio>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::render_decoded_image make_rgba_2x1_decoded_image()
{
    return quiz_vulkan::render::render_decoded_image{
        .width = 2,
        .height = 1,
        .pixel_format = quiz_vulkan::render::render_image_pixel_format::rgba8_srgb,
        .pixels = {
            std::byte{0xff},
            std::byte{0x00},
            std::byte{0x00},
            std::byte{0xff},
            std::byte{0x00},
            std::byte{0xff},
            std::byte{0x00},
            std::byte{0xff},
        },
    };
}

void test_texture_uploader_uploads_valid_decoded_image()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key key{
        .source_key = "textures/upload.ppm",
        .sampler = render_image_sampler_policy{},
    };
    fake_image_texture_uploader uploader;
    const render_image_texture_upload_result uploaded = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });

    require(uploaded.ok(), "texture uploader uploads valid decoded image");
    require(uploaded.status == render_image_texture_upload_status::uploaded, "texture uploader reports uploaded");
    require(uploaded.generation_id == 1, "texture uploader reports deterministic generation id");
    require(uploaded.sampler == render_image_sampler_policy{}, "texture uploader result preserves sampler policy");
    require(uploaded.texture.valid(), "texture uploader returns valid handle");
    require(uploaded.texture.id == 1, "texture uploader handle id is deterministic");
    require(uploaded.texture.width == 2, "texture uploader handle carries width");
    require(uploaded.texture.height == 1, "texture uploader handle carries height");
    require(uploaded.pixel_count == 2, "texture uploader reports pixel count");
    require(uploaded.pixel_byte_count == 8, "texture uploader reports expected pixel bytes");
    require(uploaded.decoded_byte_count == 8, "texture uploader reports decoded byte count");
    require(uploaded.staging_byte_count == 8, "texture uploader reports staging byte count");
    require(uploader.upload_requests.size() == 1, "texture uploader records upload request");
    require(uploader.upload_request_snapshots.size() == 1, "texture uploader records request snapshot");
    require(uploader.upload_result_snapshots.size() == 1, "texture uploader records result snapshot");
    require(uploader.upload_request_snapshots[0].generation_id == 1, "request snapshot records generation id");
    require(uploader.upload_request_snapshots[0].sampler == render_image_sampler_policy{}, "request snapshot records sampler");
    require(uploader.upload_request_snapshots[0].staging_byte_count == 8, "request snapshot records staging bytes");
    require(uploader.upload_request_snapshots[0].enqueue_sequence == 1, "request snapshot records enqueue sequence");
    require(uploader.upload_request_snapshots[0].queue_depth_before_enqueue == 0, "request snapshot records queue depth before enqueue");
    require(uploader.upload_request_snapshots[0].queue_depth_after_enqueue == 1, "request snapshot records queue depth after enqueue");
    require(uploader.upload_result_snapshots[0].generation_id == 1, "result snapshot records generation id");
    require(uploader.upload_result_snapshots[0].status == render_image_texture_upload_status::uploaded, "result snapshot records status");
    require(uploader.upload_result_snapshots[0].staging_byte_count == 8, "result snapshot records staging bytes");
    require(uploader.upload_result_snapshots[0].completion_sequence == 2, "result snapshot records completion sequence");
    require(uploader.upload_result_snapshots[0].queue_depth_after_completion == 0, "result snapshot records queue depth after completion");

    const fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    require(snapshot.upload_count == 1, "texture uploader snapshot reports upload count");
    require(snapshot.failed_upload_count == 0, "texture uploader snapshot reports no failures");
    require(snapshot.uploaded_pixel_count == 2, "texture uploader snapshot reports uploaded pixels");
    require(snapshot.uploaded_pixel_byte_count == 8, "texture uploader snapshot reports uploaded pixel bytes");
    require(snapshot.uploaded_decoded_byte_count == 8, "texture uploader snapshot reports uploaded decoded bytes");
    require(snapshot.staged_byte_count == 8, "texture uploader snapshot reports successful staging bytes");
    require(snapshot.attempted_staging_byte_count == 8, "texture uploader snapshot reports attempted staging bytes");
    require(snapshot.next_generation_id == 2, "texture uploader snapshot reports next generation id");
    require(snapshot.request_snapshots.size() == 1, "texture uploader snapshot carries request snapshot");
    require(snapshot.result_snapshots.size() == 1, "texture uploader snapshot carries result snapshot");
    require(snapshot.entries.size() == 1, "texture uploader snapshot records entry");
    require(snapshot.enqueued_upload_count == 1, "texture uploader snapshot reports enqueued uploads");
    require(snapshot.completed_upload_count == 1, "texture uploader snapshot reports completed uploads");
    require(snapshot.pending_upload_count == 0, "texture uploader snapshot reports no pending uploads");
    require(snapshot.next_queue_sequence == 3, "texture uploader snapshot reports next queue sequence");
    require(snapshot.queue_entries.size() == 1, "texture uploader snapshot records queue entry");
    require(snapshot.queue_entries[0].enqueue_sequence == 1, "texture uploader queue entry records enqueue sequence");
    require(snapshot.queue_entries[0].completion_sequence == 2, "texture uploader queue entry records completion sequence");
    require(snapshot.queue_entries[0].completed, "texture uploader queue entry records completion");
    require(snapshot.queue_entries[0].status == render_image_texture_upload_status::uploaded, "texture uploader queue entry records status");
    require(snapshot.queue_entries[0].queue_depth_after_enqueue == 1, "texture uploader queue entry records enqueue depth");
    require(snapshot.queue_entries[0].queue_depth_after_completion == 0, "texture uploader queue entry records completion depth");
    require(snapshot.entries[0].generation_id == 1, "texture uploader snapshot records generation id");
    require(snapshot.entries[0].key == key, "texture uploader snapshot records key");
    require(snapshot.entries[0].sampler == render_image_sampler_policy{}, "texture uploader snapshot records sampler");
    require(snapshot.entries[0].texture.id == uploaded.texture.id, "texture uploader snapshot records handle");
    require(snapshot.entries[0].status == render_image_texture_upload_status::uploaded, "snapshot records status");
    require(snapshot.entries[0].staging_byte_count == 8, "texture uploader snapshot records staging bytes");
    require(snapshot.entries[0].request.generation_id == 1, "entry request snapshot records generation id");
    require(snapshot.entries[0].result.texture.id == uploaded.texture.id, "entry result snapshot records handle");
}

void test_texture_uploader_reports_deterministic_queue_lifecycle()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key key{
        .source_key = "textures/upload-queue.ppm",
        .sampler = render_image_sampler_policy{},
    };
    fake_image_texture_uploader uploader;

    const render_image_texture_upload_result uploaded = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });
    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    const render_image_texture_upload_result failed = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });
    require(uploaded.ok(), "queued upload lifecycle starts with successful upload");
    require(!failed.ok(), "queued upload lifecycle records failed upload");

    const fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    require(snapshot.upload_count == 2, "queued upload snapshot records all attempts");
    require(snapshot.enqueued_upload_count == 2, "queued upload snapshot records enqueued count");
    require(snapshot.completed_upload_count == 2, "queued upload snapshot records completed count");
    require(snapshot.pending_upload_count == 0, "queued upload snapshot has no pending work");
    require(snapshot.failed_upload_count == 1, "queued upload snapshot records one failure");
    require(snapshot.next_queue_sequence == 5, "queued upload snapshot advances enqueue/completion sequence");
    require(snapshot.request_snapshots.size() == 2, "queued upload snapshot records request snapshots");
    require(snapshot.result_snapshots.size() == 2, "queued upload snapshot records result snapshots");
    require(snapshot.queue_entries.size() == 2, "queued upload snapshot records queue entries");

    require(snapshot.request_snapshots[0].enqueue_sequence == 1, "first queued request has enqueue sequence");
    require(snapshot.result_snapshots[0].completion_sequence == 2, "first queued result has completion sequence");
    require(snapshot.queue_entries[0].generation_id == 1, "first queue entry records generation");
    require(snapshot.queue_entries[0].completed, "first queue entry is completed");
    require(snapshot.queue_entries[0].status == render_image_texture_upload_status::uploaded, "first queue entry records uploaded status");
    require(snapshot.queue_entries[0].texture.id == uploaded.texture.id, "first queue entry records texture");
    require(snapshot.queue_entries[0].staging_byte_count == 8, "first queue entry records staged bytes");
    require(snapshot.queue_entries[0].queue_depth_after_enqueue == 1, "first queue entry records enqueue depth");
    require(snapshot.queue_entries[0].queue_depth_after_completion == 0, "first queue entry records completion depth");

    require(snapshot.request_snapshots[1].enqueue_sequence == 3, "second queued request has enqueue sequence");
    require(snapshot.result_snapshots[1].completion_sequence == 4, "second queued result has completion sequence");
    require(snapshot.queue_entries[1].generation_id == 2, "second queue entry records generation");
    require(snapshot.queue_entries[1].completed, "second queue entry is completed");
    require(
        snapshot.queue_entries[1].status == render_image_texture_upload_status::invalid_image,
        "second queue entry records invalid image status");
    require(!snapshot.queue_entries[1].texture.valid(), "failed queue entry records no texture handle");
    require(snapshot.queue_entries[1].staging_byte_count == 0, "failed queue entry records no staged bytes");
    require(snapshot.queue_entries[1].queue_depth_after_enqueue == 1, "failed queue entry records enqueue depth");
    require(snapshot.queue_entries[1].queue_depth_after_completion == 0, "failed queue entry records completion depth");
    require(!snapshot.queue_entries[1].diagnostic.empty(), "failed queue entry preserves diagnostic");
}

void test_texture_uploader_reports_retry_eligibility_and_backoff()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key key{
        .source_key = "textures/upload-retry.ppm",
        .sampler = render_image_sampler_policy{},
    };
    fake_image_texture_uploader uploader;

    const render_image_texture_upload_result uploaded = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });

    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    const render_image_texture_upload_result first_retryable_failure = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });
    const render_image_texture_upload_result second_retryable_failure = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });
    const render_image_texture_upload_result invalid_key_failure = uploader.upload(render_image_texture_upload_request{
        .key = render_image_texture_key{.source_key = {}, .sampler = render_image_sampler_policy{}},
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });

    require(uploaded.ok(), "retry policy starts from successful upload");
    require(!first_retryable_failure.ok(), "retry policy records first retryable failure");
    require(!second_retryable_failure.ok(), "retry policy records second retryable failure");
    require(!invalid_key_failure.ok(), "retry policy records nonretryable failure");
    require(is_retryable_render_image_texture_upload_status(render_image_texture_upload_status::invalid_image), "invalid image status is retryable");
    require(
        !is_retryable_render_image_texture_upload_status(render_image_texture_upload_status::invalid_key),
        "invalid key status is not retryable");
    require(fake_image_texture_upload_retry_backoff_sequence_delta(1) == 1, "first retry backoff is one sequence");
    require(fake_image_texture_upload_retry_backoff_sequence_delta(2) == 2, "second retry backoff doubles");
    require(fake_image_texture_upload_retry_backoff_sequence_delta(6) == 16, "retry backoff is capped");

    const fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    require(snapshot.upload_count == 4, "retry policy snapshot records all uploads");
    require(snapshot.failed_upload_count == 3, "retry policy snapshot records failed uploads");
    require(snapshot.retryable_upload_count == 2, "retry policy snapshot counts retryable failures");
    require(snapshot.nonretryable_upload_count == 1, "retry policy snapshot counts nonretryable failures");
    require(snapshot.completed_without_retry_count == 1, "retry policy snapshot counts completed upload");
    require(snapshot.retry_snapshots.size() == 4, "retry policy snapshot records retry decisions");
    require(snapshot.queue_entries.size() == 4, "retry policy snapshot records queue decisions");
    require(snapshot.next_queue_sequence == 9, "retry policy snapshot advances deterministic queue sequence");

    const fake_image_texture_upload_retry_snapshot& success_retry = snapshot.retry_snapshots[0];
    require(success_retry.generation_id == 1, "successful retry snapshot records generation");
    require(success_retry.status == render_image_texture_upload_status::uploaded, "successful retry snapshot records uploaded status");
    require(
        success_retry.eligibility == fake_image_texture_upload_retry_eligibility::not_needed,
        "successful retry snapshot reports no retry needed");
    require(success_retry.attempt_count_for_key == 1, "successful retry snapshot records attempt count");
    require(success_retry.failed_attempt_count_for_key == 0, "successful retry snapshot records no failures");
    require(success_retry.next_retry_sequence == 0, "successful retry snapshot records no retry sequence");
    require(
        fake_image_texture_upload_retry_eligibility_name(success_retry.eligibility) == "not_needed",
        "successful retry snapshot reports eligibility name");

    const fake_image_texture_upload_retry_snapshot& first_retry = snapshot.retry_snapshots[1];
    require(first_retry.generation_id == 2, "first retryable snapshot records generation");
    require(
        first_retry.eligibility == fake_image_texture_upload_retry_eligibility::eligible,
        "first retryable snapshot is eligible");
    require(first_retry.attempt_count_for_key == 2, "first retryable snapshot records attempt count");
    require(first_retry.failed_attempt_count_for_key == 1, "first retryable snapshot records failed count");
    require(first_retry.retry_after_queue_sequence_delta == 1, "first retryable snapshot records backoff");
    require(first_retry.next_retry_sequence == 5, "first retryable snapshot records next retry sequence");
    require(!first_retry.diagnostic.empty(), "first retryable snapshot includes diagnostic");

    const fake_image_texture_upload_retry_snapshot& second_retry = snapshot.retry_snapshots[2];
    require(second_retry.generation_id == 3, "second retryable snapshot records generation");
    require(
        second_retry.eligibility == fake_image_texture_upload_retry_eligibility::eligible,
        "second retryable snapshot is eligible");
    require(second_retry.attempt_count_for_key == 3, "second retryable snapshot records attempt count");
    require(second_retry.failed_attempt_count_for_key == 2, "second retryable snapshot records failed count");
    require(second_retry.retry_after_queue_sequence_delta == 2, "second retryable snapshot records doubled backoff");
    require(second_retry.next_retry_sequence == 8, "second retryable snapshot records next retry sequence");

    const fake_image_texture_upload_retry_snapshot& nonretryable = snapshot.retry_snapshots[3];
    require(nonretryable.generation_id == 4, "nonretryable snapshot records generation");
    require(
        nonretryable.eligibility == fake_image_texture_upload_retry_eligibility::ineligible,
        "nonretryable snapshot is ineligible");
    require(nonretryable.status == render_image_texture_upload_status::invalid_key, "nonretryable snapshot records invalid key");
    require(nonretryable.attempt_count_for_key == 1, "nonretryable snapshot records separate key attempts");
    require(nonretryable.failed_attempt_count_for_key == 1, "nonretryable snapshot records failed count");
    require(nonretryable.retry_after_queue_sequence_delta == 0, "nonretryable snapshot records no backoff");
    require(nonretryable.next_retry_sequence == 0, "nonretryable snapshot records no retry sequence");
    require(
        fake_image_texture_upload_retry_eligibility_name(nonretryable.eligibility) == "ineligible",
        "nonretryable snapshot reports eligibility name");

    require(
        snapshot.queue_entries[1].retry.next_retry_sequence == first_retry.next_retry_sequence,
        "queue entry carries retry summary");
    require(
        snapshot.entries[2].retry.retry_after_queue_sequence_delta
            == second_retry.retry_after_queue_sequence_delta,
        "upload entry carries retry summary");
}

void test_texture_uploader_rejects_invalid_inputs()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key valid_key{
        .source_key = "textures/upload-invalid.ppm",
        .sampler = render_image_sampler_policy{},
    };
    fake_image_texture_uploader uploader;

    const render_image_texture_upload_result invalid_key = uploader.upload(render_image_texture_upload_request{
        .key = render_image_texture_key{.source_key = {}, .sampler = render_image_sampler_policy{}},
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });
    require(!invalid_key.ok(), "texture uploader rejects invalid key");
    require(invalid_key.status == render_image_texture_upload_status::invalid_key, "invalid key status is reported");
    require(invalid_key.generation_id == 1, "invalid key reports generation id");
    require(invalid_key.staging_byte_count == 8, "invalid key preserves staging diagnostics for valid payload");
    require(!invalid_key.diagnostic.empty(), "invalid key includes diagnostic");

    render_image_sampler_policy invalid_sampler;
    invalid_sampler.wrap_u = static_cast<render_image_wrap_mode>(99);
    const render_image_texture_upload_result invalid_sampler_result = uploader.upload(
        render_image_texture_upload_request{
            .key = valid_key,
            .sampler = invalid_sampler,
            .image = make_rgba_2x1_decoded_image(),
        });
    require(!invalid_sampler_result.ok(), "texture uploader rejects invalid sampler");
    require(
        invalid_sampler_result.status == render_image_texture_upload_status::invalid_sampler,
        "invalid sampler status is reported");
    require(invalid_sampler_result.generation_id == 2, "invalid sampler reports generation id");
    require(invalid_sampler_result.sampler == invalid_sampler, "invalid sampler result records attempted sampler");
    require(invalid_sampler_result.staging_byte_count == 8, "invalid sampler preserves staging diagnostics");
    require(!invalid_sampler_result.diagnostic.empty(), "invalid sampler includes diagnostic");

    render_decoded_image unsupported_format = make_rgba_2x1_decoded_image();
    unsupported_format.pixel_format = static_cast<render_image_pixel_format>(99);
    const render_image_texture_upload_result unsupported = uploader.upload(render_image_texture_upload_request{
        .key = valid_key,
        .sampler = render_image_sampler_policy{},
        .image = unsupported_format,
    });
    require(!unsupported.ok(), "texture uploader rejects unsupported format");
    require(
        unsupported.status == render_image_texture_upload_status::unsupported_format,
        "unsupported format status is reported");
    require(unsupported.generation_id == 3, "unsupported format reports generation id");
    require(unsupported.staging_byte_count == 0, "unsupported format reports no staged bytes");
    require(!unsupported.diagnostic.empty(), "unsupported format includes diagnostic");

    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    const render_image_texture_upload_result invalid_image = uploader.upload(render_image_texture_upload_request{
        .key = valid_key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });
    require(!invalid_image.ok(), "texture uploader rejects invalid payload");
    require(
        invalid_image.status == render_image_texture_upload_status::invalid_image,
        "invalid payload status is reported");
    require(invalid_image.generation_id == 4, "invalid payload reports generation id");
    require(invalid_image.staging_byte_count == 0, "invalid payload reports no staged bytes");
    require(!invalid_image.diagnostic.empty(), "invalid payload includes diagnostic");

    const fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    require(snapshot.upload_count == 4, "texture uploader snapshot records failed attempts");
    require(snapshot.failed_upload_count == 4, "texture uploader snapshot counts failures");
    require(snapshot.next_generation_id == 5, "texture uploader failure snapshot reports next generation id");
    require(snapshot.request_snapshots.size() == 4, "texture uploader failure snapshot records request snapshots");
    require(snapshot.result_snapshots.size() == 4, "texture uploader failure snapshot records result snapshots");
    require(snapshot.entries.size() == 4, "texture uploader snapshot records failure entries");
    require(snapshot.uploaded_pixel_count == 0, "texture uploader snapshot excludes failed pixels");
    require(snapshot.staged_byte_count == 0, "texture uploader failure snapshot reports no successful staging");
    require(
        snapshot.attempted_staging_byte_count == 16,
        "texture uploader failure snapshot reports attempted staging bytes");
    require(snapshot.entries[0].generation_id == 1, "snapshot records invalid key generation");
    require(snapshot.entries[0].status == render_image_texture_upload_status::invalid_key, "snapshot records invalid key");
    require(snapshot.entries[0].staging_byte_count == 8, "snapshot records invalid key staging bytes");
    require(
        snapshot.entries[1].status == render_image_texture_upload_status::invalid_sampler,
        "snapshot records invalid sampler");
    require(snapshot.entries[1].generation_id == 2, "snapshot records invalid sampler generation");
    require(snapshot.entries[1].sampler == invalid_sampler, "snapshot records invalid sampler policy");
    require(
        snapshot.entries[2].status == render_image_texture_upload_status::unsupported_format,
        "snapshot records unsupported format");
    require(snapshot.entries[2].generation_id == 3, "snapshot records unsupported format generation");
    require(
        snapshot.entries[3].status == render_image_texture_upload_status::invalid_image,
        "snapshot records invalid image");
    require(snapshot.entries[3].generation_id == 4, "snapshot records invalid image generation");
    require(!snapshot.entries[3].texture.valid(), "failed upload snapshot records no handle");
    require(!snapshot.entries[3].result.diagnostic.empty(), "failed upload result snapshot keeps diagnostic");
}

} // namespace

int main()
{
    test_texture_uploader_uploads_valid_decoded_image();
    test_texture_uploader_reports_deterministic_queue_lifecycle();
    test_texture_uploader_reports_retry_eligibility_and_backoff();
    test_texture_uploader_rejects_invalid_inputs();
    return 0;
}
