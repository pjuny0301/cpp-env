#include "render/image/image_texture_upload_snapshot_diff.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <limits>

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

quiz_vulkan::render::render_decoded_image make_rgba_4x4_decoded_image()
{
    quiz_vulkan::render::render_decoded_image image{
        .width = 4,
        .height = 4,
        .pixel_format = quiz_vulkan::render::render_image_pixel_format::rgba8_srgb,
        .pixels = {},
    };
    image.pixels.resize(64, std::byte{0xff});
    return image;
}

quiz_vulkan::render::render_image_sampler_policy make_mipmapped_sampler()
{
    quiz_vulkan::render::render_image_sampler_policy sampler;
    sampler.mipmap_mode = quiz_vulkan::render::render_image_mipmap_mode::linear;
    return sampler;
}

quiz_vulkan::render::render_image_texture_mipmap_upload_plan make_mipmap_plan_with_status(
    quiz_vulkan::render::render_image_texture_mipmap_upload_plan_status status,
    std::size_t level_count = 0,
    std::size_t byte_count = 0)
{
    using namespace quiz_vulkan::render;

    render_image_texture_mipmap_upload_plan plan{
        .status = status,
        .status_name = render_image_texture_mipmap_upload_plan_status_name(status),
        .mipmap_mode = render_image_mipmap_mode::linear,
        .mipmap_mode_name = render_image_mipmap_mode_name(render_image_mipmap_mode::linear),
        .pixel_format = render_image_pixel_format::rgba8_srgb,
        .bytes_per_pixel = 4,
        .base_width = 1,
        .base_height = 1,
        .mipmaps_requested = true,
        .upload_plannable = status == render_image_texture_mipmap_upload_plan_status::ready,
        .requested_mip_level_count = level_count,
        .generated_mip_level_count = level_count,
        .total_pixel_count = level_count,
        .total_staging_byte_count = byte_count,
        .total_upload_byte_count = byte_count,
        .levels = {},
        .diagnostic = render_image_texture_mipmap_upload_plan_status_name(status),
    };

    std::size_t offset = 0;
    const std::size_t per_level_byte_count = level_count == 0 ? 0 : byte_count / level_count;
    for (std::size_t level = 0; level < level_count; ++level) {
        plan.levels.push_back(render_image_texture_mipmap_level_upload_plan{
            .level = level,
            .width = 1,
            .height = 1,
            .pixel_count = 1,
            .byte_count = per_level_byte_count,
            .staging_byte_offset = offset,
        });
        offset += per_level_byte_count;
    }
    return plan;
}

void set_upload_snapshot_plan_for_generation(
    quiz_vulkan::render::fake_image_texture_upload_snapshot& snapshot,
    quiz_vulkan::render::fake_image_texture_upload_generation_id generation_id,
    const quiz_vulkan::render::render_image_texture_mipmap_upload_plan& plan)
{
    for (quiz_vulkan::render::fake_image_texture_upload_request_snapshot& request : snapshot.request_snapshots) {
        if (request.generation_id == generation_id) {
            request.mipmap_upload_plan = plan;
            request.staging_byte_count = plan.total_staging_byte_count;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_result_snapshot& result : snapshot.result_snapshots) {
        if (result.generation_id == generation_id) {
            result.mipmap_upload_plan = plan;
            result.staging_byte_count = plan.total_staging_byte_count;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_queue_entry_snapshot& queue_entry : snapshot.queue_entries) {
        if (queue_entry.generation_id == generation_id) {
            queue_entry.mipmap_upload_plan = plan;
            queue_entry.staging_byte_count = plan.total_staging_byte_count;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_snapshot_entry& entry : snapshot.entries) {
        if (entry.generation_id == generation_id) {
            entry.mipmap_upload_plan = plan;
            entry.staging_byte_count = plan.total_staging_byte_count;
            entry.request.mipmap_upload_plan = plan;
            entry.request.staging_byte_count = plan.total_staging_byte_count;
            entry.result.mipmap_upload_plan = plan;
            entry.result.staging_byte_count = plan.total_staging_byte_count;
        }
    }
}

void set_upload_snapshot_status_for_generation(
    quiz_vulkan::render::fake_image_texture_upload_snapshot& snapshot,
    quiz_vulkan::render::fake_image_texture_upload_generation_id generation_id,
    quiz_vulkan::render::render_image_texture_upload_status status,
    quiz_vulkan::render::fake_image_texture_upload_retry_eligibility eligibility)
{
    for (quiz_vulkan::render::fake_image_texture_upload_result_snapshot& result : snapshot.result_snapshots) {
        if (result.generation_id == generation_id) {
            result.status = status;
            result.retry.status = status;
            result.retry.eligibility = eligibility;
            result.diagnostic = "mutated upload status";
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_queue_entry_snapshot& queue_entry : snapshot.queue_entries) {
        if (queue_entry.generation_id == generation_id) {
            queue_entry.status = status;
            queue_entry.retry.status = status;
            queue_entry.retry.eligibility = eligibility;
            queue_entry.diagnostic = "mutated upload status";
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_retry_snapshot& retry : snapshot.retry_snapshots) {
        if (retry.generation_id == generation_id) {
            retry.status = status;
            retry.eligibility = eligibility;
            retry.diagnostic = "mutated retry status";
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_snapshot_entry& entry : snapshot.entries) {
        if (entry.generation_id == generation_id) {
            entry.status = status;
            entry.retry.status = status;
            entry.retry.eligibility = eligibility;
            entry.result.status = status;
            entry.result.retry.status = status;
            entry.result.retry.eligibility = eligibility;
            entry.diagnostic = "mutated upload status";
            entry.result.diagnostic = "mutated upload status";
        }
    }
}

void set_upload_snapshot_texture_for_generation(
    quiz_vulkan::render::fake_image_texture_upload_snapshot& snapshot,
    quiz_vulkan::render::fake_image_texture_upload_generation_id generation_id,
    quiz_vulkan::render::render_image_texture_handle texture)
{
    for (quiz_vulkan::render::fake_image_texture_upload_result_snapshot& result : snapshot.result_snapshots) {
        if (result.generation_id == generation_id) {
            result.texture = texture;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_queue_entry_snapshot& queue_entry : snapshot.queue_entries) {
        if (queue_entry.generation_id == generation_id) {
            queue_entry.texture = texture;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_snapshot_entry& entry : snapshot.entries) {
        if (entry.generation_id == generation_id) {
            entry.texture = texture;
            entry.result.texture = texture;
        }
    }
}

void set_upload_snapshot_queue_depth_for_generation(
    quiz_vulkan::render::fake_image_texture_upload_snapshot& snapshot,
    quiz_vulkan::render::fake_image_texture_upload_generation_id generation_id,
    std::size_t queue_depth_after_enqueue,
    std::size_t queue_depth_after_completion)
{
    for (quiz_vulkan::render::fake_image_texture_upload_request_snapshot& request : snapshot.request_snapshots) {
        if (request.generation_id == generation_id) {
            request.queue_depth_before_enqueue = queue_depth_after_enqueue == 0 ? 0 : queue_depth_after_enqueue - 1;
            request.queue_depth_after_enqueue = queue_depth_after_enqueue;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_result_snapshot& result : snapshot.result_snapshots) {
        if (result.generation_id == generation_id) {
            result.queue_depth_after_completion = queue_depth_after_completion;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_queue_entry_snapshot& queue_entry : snapshot.queue_entries) {
        if (queue_entry.generation_id == generation_id) {
            queue_entry.queue_depth_before_enqueue = queue_depth_after_enqueue == 0 ? 0 : queue_depth_after_enqueue - 1;
            queue_entry.queue_depth_after_enqueue = queue_depth_after_enqueue;
            queue_entry.queue_depth_after_completion = queue_depth_after_completion;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_snapshot_entry& entry : snapshot.entries) {
        if (entry.generation_id == generation_id) {
            entry.request.queue_depth_before_enqueue = queue_depth_after_enqueue == 0 ? 0 : queue_depth_after_enqueue - 1;
            entry.request.queue_depth_after_enqueue = queue_depth_after_enqueue;
            entry.result.queue_depth_after_completion = queue_depth_after_completion;
        }
    }
}

void remove_upload_snapshot_generation(
    quiz_vulkan::render::fake_image_texture_upload_snapshot& snapshot,
    quiz_vulkan::render::fake_image_texture_upload_generation_id generation_id)
{
    const auto matches_generation = [generation_id](const auto& value) {
        return value.generation_id == generation_id;
    };
    snapshot.request_snapshots.erase(
        std::remove_if(snapshot.request_snapshots.begin(), snapshot.request_snapshots.end(), matches_generation),
        snapshot.request_snapshots.end());
    snapshot.result_snapshots.erase(
        std::remove_if(snapshot.result_snapshots.begin(), snapshot.result_snapshots.end(), matches_generation),
        snapshot.result_snapshots.end());
    snapshot.queue_entries.erase(
        std::remove_if(snapshot.queue_entries.begin(), snapshot.queue_entries.end(), matches_generation),
        snapshot.queue_entries.end());
    snapshot.retry_snapshots.erase(
        std::remove_if(snapshot.retry_snapshots.begin(), snapshot.retry_snapshots.end(), matches_generation),
        snapshot.retry_snapshots.end());
    snapshot.entries.erase(
        std::remove_if(snapshot.entries.begin(), snapshot.entries.end(), matches_generation),
        snapshot.entries.end());
}

void test_mipmap_upload_plan_calculates_levels_and_failure_states()
{
    using namespace quiz_vulkan::render;

    const render_image_sampler_policy mipmapped_sampler = make_mipmapped_sampler();

    const render_image_texture_mipmap_upload_plan mipmapped_plan =
        make_render_image_texture_mipmap_upload_plan(make_rgba_4x4_decoded_image(), mipmapped_sampler);

    require(mipmapped_plan.ok(), "mipmap upload plan accepts valid mipmapped image");
    require(
        mipmapped_plan.status == render_image_texture_mipmap_upload_plan_status::ready,
        "mipmap upload plan reports ready status");
    require(mipmapped_plan.status_name == "ready", "mipmap upload plan ready status name is stable");
    require(mipmapped_plan.mipmap_mode == render_image_mipmap_mode::linear, "mipmap upload plan records mode");
    require(mipmapped_plan.mipmap_mode_name == "linear", "mipmap upload plan records mode name");
    require(mipmapped_plan.mipmaps_requested, "mipmap upload plan records requested mipmaps");
    require(mipmapped_plan.bytes_per_pixel == 4, "mipmap upload plan records RGBA8 byte size");
    require(mipmapped_plan.base_width == 4, "mipmap upload plan records base width");
    require(mipmapped_plan.base_height == 4, "mipmap upload plan records base height");
    require(mipmapped_plan.requested_mip_level_count == 3, "mipmap upload plan requests full chain");
    require(mipmapped_plan.generated_mip_level_count == 3, "mipmap upload plan generates full chain");
    require(mipmapped_plan.total_pixel_count == 21, "mipmap upload plan sums all level pixels");
    require(mipmapped_plan.total_staging_byte_count == 84, "mipmap upload plan sums staging bytes");
    require(mipmapped_plan.total_upload_byte_count == 84, "mipmap upload plan sums upload bytes");
    require(mipmapped_plan.levels.size() == 3, "mipmap upload plan exposes per-level diagnostics");
    require(mipmapped_plan.levels[0].level == 0, "mipmap upload plan level zero index is stable");
    require(mipmapped_plan.levels[0].width == 4, "mipmap upload plan level zero width");
    require(mipmapped_plan.levels[0].height == 4, "mipmap upload plan level zero height");
    require(mipmapped_plan.levels[0].pixel_count == 16, "mipmap upload plan level zero pixels");
    require(mipmapped_plan.levels[0].byte_count == 64, "mipmap upload plan level zero bytes");
    require(mipmapped_plan.levels[0].staging_byte_offset == 0, "mipmap upload plan level zero offset");
    require(mipmapped_plan.levels[1].width == 2, "mipmap upload plan halves level width");
    require(mipmapped_plan.levels[1].height == 2, "mipmap upload plan halves level height");
    require(mipmapped_plan.levels[1].staging_byte_offset == 64, "mipmap upload plan level one offset");
    require(mipmapped_plan.levels[2].width == 1, "mipmap upload plan terminal width is one");
    require(mipmapped_plan.levels[2].height == 1, "mipmap upload plan terminal height is one");
    require(mipmapped_plan.levels[2].staging_byte_offset == 80, "mipmap upload plan terminal offset");

    const render_image_texture_mipmap_upload_plan base_plan =
        make_render_image_texture_mipmap_upload_plan(make_rgba_2x1_decoded_image(), render_image_sampler_policy{});
    require(base_plan.ok(), "base-only mipmap upload plan is plannable");
    require(
        base_plan.status == render_image_texture_mipmap_upload_plan_status::no_mipmaps_requested,
        "base-only mipmap upload plan reports no-mipmap state");
    require(base_plan.status_name == "no_mipmaps_requested", "base-only mipmap status name is stable");
    require(!base_plan.mipmaps_requested, "base-only mipmap upload plan records no mipmaps requested");
    require(base_plan.levels.size() == 1, "base-only mipmap upload plan exposes one level");
    require(base_plan.total_staging_byte_count == 8, "base-only mipmap upload plan estimates base bytes");

    render_image_sampler_policy invalid_mipmap_sampler;
    invalid_mipmap_sampler.mipmap_mode = static_cast<render_image_mipmap_mode>(99);
    const render_image_texture_mipmap_upload_plan invalid_sampler_plan =
        make_render_image_texture_mipmap_upload_plan(make_rgba_2x1_decoded_image(), invalid_mipmap_sampler);
    require(!invalid_sampler_plan.ok(), "mipmap upload plan rejects invalid mipmap mode");
    require(
        invalid_sampler_plan.status == render_image_texture_mipmap_upload_plan_status::invalid_sampler,
        "mipmap upload plan reports invalid sampler status");

    render_decoded_image empty_image = make_rgba_2x1_decoded_image();
    empty_image.width = 0;
    const render_image_texture_mipmap_upload_plan invalid_image_plan =
        make_render_image_texture_mipmap_upload_plan(empty_image, render_image_sampler_policy{});
    require(!invalid_image_plan.ok(), "mipmap upload plan rejects invalid dimensions");
    require(
        invalid_image_plan.status == render_image_texture_mipmap_upload_plan_status::invalid_image,
        "mipmap upload plan reports invalid image status");

    render_decoded_image unsupported_format = make_rgba_2x1_decoded_image();
    unsupported_format.pixel_format = static_cast<render_image_pixel_format>(99);
    const render_image_texture_mipmap_upload_plan unsupported_plan =
        make_render_image_texture_mipmap_upload_plan(unsupported_format, render_image_sampler_policy{});
    require(!unsupported_plan.ok(), "mipmap upload plan rejects unsupported pixel formats");
    require(
        unsupported_plan.status == render_image_texture_mipmap_upload_plan_status::unsupported_format,
        "mipmap upload plan reports unsupported format status");

    render_decoded_image overflow_image{
        .width = std::numeric_limits<std::size_t>::max() / 4 + 1,
        .height = 2,
        .pixel_format = render_image_pixel_format::rgba8_srgb,
        .pixels = {},
    };
    const render_image_texture_mipmap_upload_plan overflow_plan =
        make_render_image_texture_mipmap_upload_plan(overflow_image, render_image_sampler_policy{});
    require(!overflow_plan.ok(), "mipmap upload plan rejects byte count overflow");
    require(
        overflow_plan.status == render_image_texture_mipmap_upload_plan_status::overflow,
        "mipmap upload plan reports overflow status");
    require(overflow_plan.status_name == "overflow", "mipmap upload plan overflow status name is stable");
    require(
        render_image_texture_mipmap_upload_plan_status_name(
            render_image_texture_mipmap_upload_plan_status::ready) == "ready",
        "mipmap upload plan status helper is stable");
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
    require(
        uploaded.mipmap_upload_plan.status == render_image_texture_mipmap_upload_plan_status::no_mipmaps_requested,
        "texture uploader result records no-mipmap plan");
    require(uploaded.mipmap_upload_plan.levels.size() == 1, "texture uploader result records base mip level");
    require(uploaded.mipmap_upload_plan.total_staging_byte_count == 8, "texture uploader result records planned staging bytes");
    require(uploader.upload_requests.size() == 1, "texture uploader records upload request");
    require(uploader.upload_request_snapshots.size() == 1, "texture uploader records request snapshot");
    require(uploader.upload_result_snapshots.size() == 1, "texture uploader records result snapshot");
    require(uploader.upload_request_snapshots[0].generation_id == 1, "request snapshot records generation id");
    require(uploader.upload_request_snapshots[0].sampler == render_image_sampler_policy{}, "request snapshot records sampler");
    require(uploader.upload_request_snapshots[0].staging_byte_count == 8, "request snapshot records staging bytes");
    require(
        uploader.upload_request_snapshots[0].mipmap_upload_plan.status
            == render_image_texture_mipmap_upload_plan_status::no_mipmaps_requested,
        "request snapshot records mipmap plan");
    require(uploader.upload_request_snapshots[0].enqueue_sequence == 1, "request snapshot records enqueue sequence");
    require(uploader.upload_request_snapshots[0].queue_depth_before_enqueue == 0, "request snapshot records queue depth before enqueue");
    require(uploader.upload_request_snapshots[0].queue_depth_after_enqueue == 1, "request snapshot records queue depth after enqueue");
    require(uploader.upload_result_snapshots[0].generation_id == 1, "result snapshot records generation id");
    require(uploader.upload_result_snapshots[0].status == render_image_texture_upload_status::uploaded, "result snapshot records status");
    require(uploader.upload_result_snapshots[0].staging_byte_count == 8, "result snapshot records staging bytes");
    require(
        uploader.upload_result_snapshots[0].mipmap_upload_plan.total_upload_byte_count == 8,
        "result snapshot records mipmap upload bytes");
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
    require(
        snapshot.queue_entries[0].mipmap_upload_plan.generated_mip_level_count == 1,
        "queue entry records mipmap plan");
    require(snapshot.entries[0].generation_id == 1, "texture uploader snapshot records generation id");
    require(snapshot.entries[0].key == key, "texture uploader snapshot records key");
    require(snapshot.entries[0].sampler == render_image_sampler_policy{}, "texture uploader snapshot records sampler");
    require(snapshot.entries[0].texture.id == uploaded.texture.id, "texture uploader snapshot records handle");
    require(snapshot.entries[0].status == render_image_texture_upload_status::uploaded, "snapshot records status");
    require(snapshot.entries[0].staging_byte_count == 8, "texture uploader snapshot records staging bytes");
    require(
        snapshot.entries[0].mipmap_upload_plan.total_staging_byte_count == 8,
        "texture uploader snapshot entry records mipmap staging bytes");
    require(snapshot.entries[0].request.generation_id == 1, "entry request snapshot records generation id");
    require(snapshot.entries[0].result.texture.id == uploaded.texture.id, "entry result snapshot records handle");
}

void test_texture_uploader_records_mipmap_upload_plan()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy mipmapped_sampler;
    mipmapped_sampler.mipmap_mode = render_image_mipmap_mode::linear;
    const render_image_texture_key key{
        .source_key = "textures/upload-mips.ppm",
        .sampler = mipmapped_sampler,
    };
    fake_image_texture_uploader uploader;
    const render_image_texture_upload_result uploaded = uploader.upload(render_image_texture_upload_request{
        .key = key,
        .sampler = mipmapped_sampler,
        .image = make_rgba_4x4_decoded_image(),
    });

    require(uploaded.ok(), "texture uploader accepts mipmapped sampler upload");
    require(uploaded.pixel_count == 16, "mipmapped upload result records base pixel count");
    require(uploaded.pixel_byte_count == 64, "mipmapped upload result records base pixel bytes");
    require(uploaded.decoded_byte_count == 64, "mipmapped upload result records decoded source bytes");
    require(uploaded.staging_byte_count == 84, "mipmapped upload result records planned staging bytes");
    require(
        uploaded.mipmap_upload_plan.status == render_image_texture_mipmap_upload_plan_status::ready,
        "mipmapped upload result records ready plan");
    require(uploaded.mipmap_upload_plan.requested_mip_level_count == 3, "mipmapped upload result records level count");
    require(uploaded.mipmap_upload_plan.total_staging_byte_count == 84, "mipmapped upload result records total staging bytes");
    require(uploaded.mipmap_upload_plan.levels[2].byte_count == 4, "mipmapped upload result records terminal level bytes");

    const fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    require(snapshot.staged_byte_count == 84, "mipmapped upload snapshot records staged bytes");
    require(snapshot.attempted_staging_byte_count == 84, "mipmapped upload snapshot records attempted staging bytes");
    require(snapshot.request_snapshots[0].mipmap_upload_plan.generated_mip_level_count == 3, "request snapshot records mip levels");
    require(snapshot.result_snapshots[0].mipmap_upload_plan.total_upload_byte_count == 84, "result snapshot records mip upload bytes");
    require(snapshot.queue_entries[0].mipmap_upload_plan.levels[1].byte_count == 16, "queue snapshot records mip level bytes");
    require(snapshot.entries[0].mipmap_upload_plan.mipmap_mode == render_image_mipmap_mode::linear, "entry snapshot records mipmap mode");
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

void test_texture_upload_snapshot_diff_reports_added_uploads_and_byte_deltas()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_uploader uploader;
    const fake_image_texture_upload_snapshot before = uploader.diagnostic_snapshot();

    const render_image_texture_key base_key{
        .source_key = "textures/diff-base.ppm",
        .sampler = render_image_sampler_policy{},
    };
    const render_image_sampler_policy mipmapped_sampler = make_mipmapped_sampler();
    const render_image_texture_key mipmapped_key{
        .source_key = "textures/diff-mips.ppm",
        .sampler = mipmapped_sampler,
    };
    uploader.upload(render_image_texture_upload_request{
        .key = base_key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });
    uploader.upload(render_image_texture_upload_request{
        .key = mipmapped_key,
        .sampler = mipmapped_sampler,
        .image = make_rgba_4x4_decoded_image(),
    });

    const fake_image_texture_upload_snapshot after = uploader.diagnostic_snapshot();
    const fake_image_texture_upload_snapshot_diff diff =
        diff_fake_image_texture_upload_snapshots(before, after);

    require(diff.ok(), "upload snapshot diff treats added successful uploads as non-regressive");
    require(diff.has_changes, "upload snapshot diff reports added uploads as changes");
    require(!diff.has_regression, "upload snapshot diff has no regression for successful added uploads");
    require(diff.before_upload_count == 0, "upload snapshot diff records before upload count");
    require(diff.after_upload_count == 2, "upload snapshot diff records after upload count");
    require(diff.added_upload_count == 2, "upload snapshot diff counts added uploads");
    require(diff.removed_upload_count == 0, "upload snapshot diff reports no removed uploads");
    require(diff.changed_upload_count == 0, "upload snapshot diff reports no changed uploads");
    require(diff.request_snapshot_added_count == 2, "upload snapshot diff counts added request snapshots");
    require(diff.result_snapshot_added_count == 2, "upload snapshot diff counts added result snapshots");
    require(diff.queue_snapshot_added_count == 2, "upload snapshot diff counts added queue snapshots");
    require(diff.staged_byte_delta == 92, "upload snapshot diff reports staged byte delta");
    require(diff.attempted_staging_byte_delta == 92, "upload snapshot diff reports attempted staging byte delta");
    require(diff.before_mip_level_count == 0, "upload snapshot diff records before mip levels");
    require(diff.after_mip_level_count == 4, "upload snapshot diff records after mip levels");
    require(diff.mip_level_count_delta == 4, "upload snapshot diff reports mip level delta");
    require(diff.mipmap_byte_delta == 92, "upload snapshot diff reports mipmap byte delta");
    require(diff.texture_handle_added_count == 2, "upload snapshot diff counts added texture handles");
    require(diff.entries.size() == 2, "upload snapshot diff emits one entry per added generation");
    require(
        diff.entries[0].status == fake_image_texture_upload_snapshot_diff_entry_status::added,
        "upload snapshot diff entry reports added status");
    require(diff.entries[1].mip_level_count_delta == 3, "upload snapshot diff entry reports mip level delta");
    require(
        fake_image_texture_upload_snapshot_diff_entry_status_name(
            fake_image_texture_upload_snapshot_diff_entry_status::changed)
            == "changed",
        "upload snapshot diff status name is stable");
}

void test_texture_upload_snapshot_diff_reports_changed_transitions_and_regressions()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key valid_key{
        .source_key = "textures/diff-transition.ppm",
        .sampler = render_image_sampler_policy{},
    };
    const render_image_sampler_policy mipmapped_sampler = make_mipmapped_sampler();
    const render_image_texture_key mipmapped_key{
        .source_key = "textures/diff-transition-mips.ppm",
        .sampler = mipmapped_sampler,
    };

    fake_image_texture_uploader uploader;
    uploader.upload(render_image_texture_upload_request{
        .key = valid_key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });

    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    uploader.upload(render_image_texture_upload_request{
        .key = valid_key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });
    uploader.upload(render_image_texture_upload_request{
        .key = render_image_texture_key{.source_key = {}, .sampler = render_image_sampler_policy{}},
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });
    uploader.upload(render_image_texture_upload_request{
        .key = mipmapped_key,
        .sampler = mipmapped_sampler,
        .image = make_rgba_4x4_decoded_image(),
    });

    const fake_image_texture_upload_snapshot before = uploader.diagnostic_snapshot();
    fake_image_texture_upload_snapshot after = before;

    set_upload_snapshot_plan_for_generation(
        after,
        1,
        make_mipmap_plan_with_status(render_image_texture_mipmap_upload_plan_status::overflow));
    set_upload_snapshot_texture_for_generation(
        after,
        1,
        render_image_texture_handle{.id = 99, .revision = 2, .width = 2, .height = 1});
    set_upload_snapshot_queue_depth_for_generation(after, 1, 4, 1);

    set_upload_snapshot_plan_for_generation(
        after,
        2,
        make_mipmap_plan_with_status(render_image_texture_mipmap_upload_plan_status::unsupported_format));
    set_upload_snapshot_status_for_generation(
        after,
        2,
        render_image_texture_upload_status::unsupported_format,
        fake_image_texture_upload_retry_eligibility::ineligible);

    set_upload_snapshot_plan_for_generation(
        after,
        3,
        make_mipmap_plan_with_status(render_image_texture_mipmap_upload_plan_status::invalid_image));
    set_upload_snapshot_status_for_generation(
        after,
        3,
        render_image_texture_upload_status::invalid_image,
        fake_image_texture_upload_retry_eligibility::eligible);
    set_upload_snapshot_queue_depth_for_generation(after, 3, 0, 0);

    remove_upload_snapshot_generation(after, 4);
    after.upload_count = 3;
    after.enqueued_upload_count = 3;
    after.completed_upload_count = 3;
    after.staged_byte_count = 8;
    after.attempted_staging_byte_count = 8;

    const fake_image_texture_upload_snapshot_diff diff =
        diff_fake_image_texture_upload_snapshots(before, after);

    require(!diff.ok(), "upload snapshot diff reports transition regressions");
    require(diff.has_changes, "upload snapshot diff reports changed transitions");
    require(diff.has_regression, "upload snapshot diff reports regressions");
    require(diff.has_recovery, "upload snapshot diff reports recoveries");
    require(diff.before_upload_count == 4, "upload snapshot diff records before count for transitions");
    require(diff.after_upload_count == 3, "upload snapshot diff records after count for transitions");
    require(diff.changed_upload_count == 3, "upload snapshot diff counts changed uploads");
    require(diff.removed_upload_count == 1, "upload snapshot diff counts removed uploads");
    require(diff.request_snapshot_changed_count == 3, "upload snapshot diff counts changed request snapshots");
    require(diff.request_snapshot_removed_count == 1, "upload snapshot diff counts removed request snapshots");
    require(diff.result_snapshot_changed_count == 3, "upload snapshot diff counts changed result snapshots");
    require(diff.result_snapshot_removed_count == 1, "upload snapshot diff counts removed result snapshots");
    require(diff.queue_snapshot_changed_count == 3, "upload snapshot diff counts changed queue snapshots");
    require(diff.queue_snapshot_removed_count == 1, "upload snapshot diff counts removed queue snapshots");
    require(diff.staged_byte_delta == -84, "upload snapshot diff reports staged byte reduction");
    require(diff.mipmap_byte_delta == -108, "upload snapshot diff reports mipmap byte reduction");
    require(diff.retryability_changed_count == 2, "upload snapshot diff counts retryability changes");
    require(
        diff.retryable_to_nonretryable_count == 1,
        "upload snapshot diff counts retryable to nonretryable transition");
    require(
        diff.nonretryable_to_retryable_count == 1,
        "upload snapshot diff counts nonretryable to retryable transition");
    require(diff.queue_depth_regression_count == 1, "upload snapshot diff counts queue depth regression");
    require(diff.queue_depth_recovery_count == 1, "upload snapshot diff counts queue depth recoveries");
    require(diff.mipmap_plan_status_changed_count == 3, "upload snapshot diff counts mipmap plan changes");
    require(diff.invalid_plan_transition_count == 1, "upload snapshot diff counts invalid plan transition");
    require(diff.invalid_plan_regression_count == 1, "upload snapshot diff counts invalid plan regression");
    require(diff.overflow_plan_transition_count == 1, "upload snapshot diff counts overflow plan transition");
    require(diff.overflow_plan_regression_count == 1, "upload snapshot diff counts overflow plan regression");
    require(diff.unsupported_plan_transition_count == 1, "upload snapshot diff counts unsupported plan transition");
    require(
        diff.unsupported_plan_regression_count == 1,
        "upload snapshot diff counts unsupported plan regression");
    require(diff.texture_handle_changed_count == 1, "upload snapshot diff counts texture handle changes");
    require(diff.texture_handle_removed_count == 1, "upload snapshot diff counts texture handle removals");
    require(!diff.regression_summary.empty(), "upload snapshot diff includes regression summary");

    const fake_image_texture_upload_snapshot_entry_diff* generation_one = nullptr;
    for (const fake_image_texture_upload_snapshot_entry_diff& entry : diff.entries) {
        if (entry.generation_id == 1) {
            generation_one = &entry;
        }
    }
    require(generation_one != nullptr, "upload snapshot diff exposes changed entry by generation");
    require(generation_one->texture_handle_changed, "upload snapshot diff entry records texture handle change");
    require(generation_one->queue_depth_regressed, "upload snapshot diff entry records queue depth regression");
    require(generation_one->overflow_plan_regression, "upload snapshot diff entry records overflow plan regression");
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
    test_mipmap_upload_plan_calculates_levels_and_failure_states();
    test_texture_uploader_uploads_valid_decoded_image();
    test_texture_uploader_records_mipmap_upload_plan();
    test_texture_uploader_reports_deterministic_queue_lifecycle();
    test_texture_uploader_reports_retry_eligibility_and_backoff();
    test_texture_upload_snapshot_diff_reports_added_uploads_and_byte_deltas();
    test_texture_upload_snapshot_diff_reports_changed_transitions_and_regressions();
    test_texture_uploader_rejects_invalid_inputs();
    return 0;
}
