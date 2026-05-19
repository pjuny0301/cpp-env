#include "render/image/image_texture_upload_result_diagnostics.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>

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

void set_upload_snapshot_staging_plan_for_generation(
    quiz_vulkan::render::fake_image_texture_upload_snapshot& snapshot,
    quiz_vulkan::render::fake_image_texture_upload_generation_id generation_id,
    const quiz_vulkan::render::render_image_texture_staging_payload_plan& plan)
{
    for (quiz_vulkan::render::fake_image_texture_upload_request_snapshot& request : snapshot.request_snapshots) {
        if (request.generation_id == generation_id) {
            request.staging_payload_plan = plan;
            request.staging_byte_count = plan.total_staging_byte_count;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_result_snapshot& result : snapshot.result_snapshots) {
        if (result.generation_id == generation_id) {
            result.staging_payload_plan = plan;
            result.staging_byte_count = plan.total_staging_byte_count;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_queue_entry_snapshot& queue_entry : snapshot.queue_entries) {
        if (queue_entry.generation_id == generation_id) {
            queue_entry.staging_payload_plan = plan;
            queue_entry.staging_byte_count = plan.total_staging_byte_count;
        }
    }
    for (quiz_vulkan::render::fake_image_texture_upload_snapshot_entry& entry : snapshot.entries) {
        if (entry.generation_id == generation_id) {
            entry.staging_payload_plan = plan;
            entry.staging_byte_count = plan.total_staging_byte_count;
            entry.request.staging_payload_plan = plan;
            entry.request.staging_byte_count = plan.total_staging_byte_count;
            entry.result.staging_payload_plan = plan;
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

void remove_upload_result_and_queue_for_generation(
    quiz_vulkan::render::fake_image_texture_upload_snapshot& snapshot,
    quiz_vulkan::render::fake_image_texture_upload_generation_id generation_id)
{
    const auto matches_generation = [generation_id](const auto& value) {
        return value.generation_id == generation_id;
    };
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

void test_upload_payload_layout_evidence_records_extent_stride_and_identity()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy sampler;
    sampler.min_filter = render_image_filter::nearest;
    sampler.mag_filter = render_image_filter::nearest;
    const render_image_texture_key key{
        .source_key = "textures/layout.ppm",
        .sampler = sampler,
    };

    const render_image_texture_upload_payload_layout_evidence layout =
        make_render_image_texture_upload_payload_layout_evidence(
            key,
            sampler,
            make_rgba_2x1_decoded_image());

    require(layout.ok(), "upload payload layout evidence is ready for valid RGBA8 image");
    require(layout.extent_width == 2, "upload payload layout records extent width");
    require(layout.extent_height == 1, "upload payload layout records extent height");
    require(layout.bytes_per_pixel == 4, "upload payload layout records RGBA8 byte size");
    require(layout.row_stride_byte_count == 8, "upload payload layout records row stride");
    require(layout.pixel_count == 2, "upload payload layout records pixel count");
    require(layout.expected_byte_count == 8, "upload payload layout records expected byte count");
    require(layout.decoded_byte_count == 8, "upload payload layout records decoded byte count");
    require(layout.staging_byte_count == 8, "upload payload layout records staging bytes");
    require(layout.cache_key_valid, "upload payload layout records valid cache identity");
    require(layout.sampler_valid, "upload payload layout records valid sampler");
    require(layout.sampler_matches_texture_key, "upload payload layout records sampler identity match");
    require(layout.rgba8_format, "upload payload layout records RGBA8 format");
    require(layout.nonzero_extent, "upload payload layout records non-zero extent");
    require(layout.row_stride_consistent, "upload payload layout records row-stride consistency");
    require(layout.byte_count_consistent, "upload payload layout records byte-count consistency");
    require(layout.staging_byte_count_consistent, "upload payload layout records staging consistency");
    require(
        layout.stable_texture_cache_key.find("textures/layout.ppm") != std::string::npos,
        "upload payload layout preserves stable cache key");
    require(
        layout.sampler_summary == render_image_sampler_policy_stable_fragment(sampler),
        "upload payload layout preserves sampler summary");
    require(
        layout.decoded_payload.stable_byte_hash
            == make_render_image_decoded_payload_evidence(make_rgba_2x1_decoded_image()).stable_byte_hash,
        "upload payload layout preserves decoded payload hash");
    require(
        layout.diagnostic == "image texture upload payload layout is ready",
        "upload payload layout ready diagnostic is stable");

    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    const render_image_texture_upload_payload_layout_evidence invalid_layout =
        make_render_image_texture_upload_payload_layout_evidence(key, sampler, invalid_payload);
    require(!invalid_layout.ok(), "upload payload layout rejects inconsistent byte count");
    require(invalid_layout.row_stride_byte_count == 8, "invalid layout still records deterministic row stride");
    require(invalid_layout.expected_byte_count == 8, "invalid layout still records expected bytes");
    require(invalid_layout.decoded_byte_count == 7, "invalid layout records actual decoded bytes");
    require(!invalid_layout.byte_count_consistent, "invalid layout records byte-count mismatch");
    require(
        invalid_layout.diagnostic == "image texture upload payload layout byte count does not match extent",
        "invalid layout byte-count diagnostic is stable");

    render_image_sampler_policy mismatched_sampler = sampler;
    mismatched_sampler.wrap_u = render_image_wrap_mode::repeat;
    const render_image_texture_upload_payload_layout_evidence mismatched_layout =
        make_render_image_texture_upload_payload_layout_evidence(
            key,
            mismatched_sampler,
            make_rgba_2x1_decoded_image());
    require(!mismatched_layout.ok(), "upload payload layout rejects sampler mismatch");
    require(!mismatched_layout.sampler_matches_texture_key, "sampler mismatch is recorded");
    require(
        mismatched_layout.diagnostic == "image texture upload payload layout sampler does not match texture key",
        "sampler mismatch diagnostic is stable");
}

void test_staging_payload_plan_records_rows_alignment_and_blockers()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy sampler;
    sampler.min_filter = render_image_filter::nearest;
    sampler.mag_filter = render_image_filter::nearest;
    const render_image_texture_key key{
        .source_key = "textures/staging.ppm",
        .sampler = sampler,
    };
    const render_decoded_image base_image = make_rgba_2x1_decoded_image();
    const render_image_texture_upload_payload_layout_evidence layout =
        make_render_image_texture_upload_payload_layout_evidence(key, sampler, base_image);
    const render_image_texture_mipmap_upload_plan base_mipmap_plan =
        make_render_image_texture_mipmap_upload_plan(base_image, sampler);

    const render_image_texture_staging_payload_plan base_plan =
        make_render_image_texture_staging_payload_plan(layout, base_mipmap_plan);
    require(base_plan.ok(), "staging payload plan accepts valid base decoded bytes");
    require(
        base_plan.status == render_image_texture_staging_payload_plan_status::ready,
        "base staging payload plan reports ready status");
    require(base_plan.status_name == "ready", "base staging payload plan status name is stable");
    require(base_plan.stable_texture_cache_key == layout.stable_texture_cache_key, "staging plan preserves cache identity");
    require(base_plan.sampler_summary == layout.sampler_summary, "staging plan preserves sampler identity");
    require(base_plan.alignment_byte_count == 4, "staging plan records default row alignment");
    require(base_plan.base_row_stride_byte_count == 8, "staging plan records base row payload stride");
    require(base_plan.base_staging_row_stride_byte_count == 8, "staging plan records aligned base row stride");
    require(base_plan.base_row_padding_byte_count == 0, "staging plan records base row padding");
    require(base_plan.row_copy_count == 1, "staging plan records row copy count");
    require(base_plan.mip_level_reference_count == 1, "staging plan records base mip reference");
    require(base_plan.total_row_payload_byte_count == 8, "staging plan records row payload bytes");
    require(base_plan.total_row_padding_byte_count == 0, "staging plan records row padding bytes");
    require(base_plan.total_staging_byte_count == 8, "staging plan records total staging bytes");
    require(base_plan.decoded_byte_count == 8, "staging plan records decoded byte count");
    require(base_plan.referenced_mipmap_byte_count == 8, "staging plan records referenced mip bytes");
    require(base_plan.decoded_payload_hash == layout.decoded_payload.stable_byte_hash, "staging plan preserves payload hash");
    require(base_plan.layout_ready, "staging plan records layout readiness");
    require(base_plan.mipmap_plan_ready, "staging plan records mipmap readiness");
    require(base_plan.rows_aligned, "staging plan records row alignment success");
    require(!base_plan.has_row_padding, "staging plan records no padding for aligned RGBA row");
    require(base_plan.decoded_payload_available, "staging plan records decoded payload availability");
    require(!base_plan.mipmaps_referenced, "base staging plan records no generated mip references");
    require(base_plan.row_copies.size() == 1, "staging plan exposes row copy diagnostics");
    require(base_plan.row_copies[0].mip_level == 0, "staging row copy records mip level");
    require(base_plan.row_copies[0].row_index == 0, "staging row copy records row index");
    require(base_plan.row_copies[0].source_byte_offset == 0, "staging row copy records source offset");
    require(base_plan.row_copies[0].staging_byte_offset == 0, "staging row copy records staging offset");
    require(base_plan.row_copies[0].row_payload_byte_count == 8, "staging row copy records payload bytes");
    require(base_plan.row_copies[0].staging_row_stride_byte_count == 8, "staging row copy records aligned stride");
    require(base_plan.row_copies[0].row_padding_byte_count == 0, "staging row copy records padding");
    require(base_plan.row_copies[0].decoded_payload_backed, "base row copy is decoded-payload backed");
    require(!base_plan.row_copies[0].generated_mip_reference, "base row copy is not generated mip reference");
    require(base_plan.mip_level_references[0].base_level, "base mip reference records base level");
    require(
        base_plan.diagnostic == "image texture staging payload plan is ready",
        "base staging plan diagnostic is stable");

    const render_image_texture_staging_payload_plan aligned_plan =
        make_render_image_texture_staging_payload_plan(layout, base_mipmap_plan, 16);
    require(aligned_plan.ok(), "staging payload plan accepts wider row alignment");
    require(aligned_plan.alignment_byte_count == 16, "aligned staging plan records requested alignment");
    require(aligned_plan.base_staging_row_stride_byte_count == 16, "aligned staging plan pads base row stride");
    require(aligned_plan.base_row_padding_byte_count == 8, "aligned staging plan records base padding");
    require(aligned_plan.total_staging_byte_count == 16, "aligned staging plan records padded total bytes");
    require(aligned_plan.total_row_padding_byte_count == 8, "aligned staging plan records row padding bytes");
    require(aligned_plan.has_row_padding, "aligned staging plan records padding evidence");
    const render_image_texture_staging_payload_plan_diff aligned_diff =
        diff_render_image_texture_staging_payload_plans(base_plan, aligned_plan);
    require(aligned_diff.ok(), "aligned staging diff is not a readiness regression");
    require(aligned_diff.changed(), "aligned staging diff reports a compact change");
    require(
        aligned_diff.status == render_image_texture_staging_payload_plan_diff_status::changed,
        "aligned staging diff status is changed");
    require(aligned_diff.before_row_copy_count == 1, "aligned staging diff records before row copy count");
    require(aligned_diff.after_row_copy_count == 1, "aligned staging diff records after row copy count");
    require(aligned_diff.alignment_changed, "aligned staging diff records alignment changes");
    require(aligned_diff.padding_changed, "aligned staging diff records padding changes");
    require(aligned_diff.total_staging_byte_count_changed, "aligned staging diff records staging byte changes");
    require(aligned_diff.alignment_byte_delta == 12, "aligned staging diff records alignment delta");
    require(aligned_diff.row_padding_byte_delta == 8, "aligned staging diff records padding delta");
    require(aligned_diff.total_staging_byte_delta == 8, "aligned staging diff records staging byte delta");
    require(
        aligned_diff.change_summary == "staging_bytes=8->16,alignment=4->16,row_padding=0->8",
        "aligned staging diff emits compact layout summary");
    require(
        aligned_diff.diagnostic == "image texture staging payload plan changed staging bytes",
        "aligned staging diff diagnostic is stable");
    const render_image_texture_staging_payload_plan_diff unchanged_diff =
        diff_render_image_texture_staging_payload_plans(base_plan, base_plan);
    require(!unchanged_diff.changed(), "unchanged staging diff reports stable no-op");
    require(
        unchanged_diff.change_summary == "no staging payload plan changes",
        "unchanged staging diff summary is stable");

    const render_image_texture_staging_payload_plan invalid_alignment_plan =
        make_render_image_texture_staging_payload_plan(layout, base_mipmap_plan, 0);
    require(!invalid_alignment_plan.ok(), "staging payload plan rejects zero row alignment");
    require(
        invalid_alignment_plan.status == render_image_texture_staging_payload_plan_status::blocked_invalid_alignment,
        "zero-alignment staging plan reports invalid-alignment blocker");
    require(invalid_alignment_plan.alignment_byte_count == 0, "zero-alignment staging plan records requested alignment");
    require(invalid_alignment_plan.row_copy_count == 0, "zero-alignment staging plan records no row copies");
    require(invalid_alignment_plan.total_staging_byte_count == 0, "zero-alignment staging plan records no staging bytes");
    require(invalid_alignment_plan.layout_ready, "zero-alignment staging plan preserves layout readiness");
    require(invalid_alignment_plan.decoded_payload_available, "zero-alignment staging plan preserves payload availability");
    require(
        invalid_alignment_plan.blocker_summary == "staging row alignment must be non-zero",
        "zero-alignment staging plan blocker is stable");

    render_image_sampler_policy mipmapped_sampler = sampler;
    mipmapped_sampler.mipmap_mode = render_image_mipmap_mode::linear;
    const render_image_texture_key mipmapped_key{
        .source_key = "textures/staging-mips.ppm",
        .sampler = mipmapped_sampler,
    };
    const render_decoded_image mipmapped_image = make_rgba_4x4_decoded_image();
    const render_image_texture_upload_payload_layout_evidence mipmapped_layout =
        make_render_image_texture_upload_payload_layout_evidence(
            mipmapped_key,
            mipmapped_sampler,
            mipmapped_image);
    const render_image_texture_mipmap_upload_plan mipmapped_upload_plan =
        make_render_image_texture_mipmap_upload_plan(mipmapped_image, mipmapped_sampler);
    const render_image_texture_staging_payload_plan mipmapped_plan =
        make_render_image_texture_staging_payload_plan(mipmapped_layout, mipmapped_upload_plan, 16);
    require(mipmapped_plan.ok(), "staging payload plan accepts mipmapped decoded texture");
    require(
        mipmapped_plan.status == render_image_texture_staging_payload_plan_status::ready_with_mip_references,
        "mipmapped staging plan reports mip-reference readiness");
    require(mipmapped_plan.row_copy_count == 7, "mipmapped staging plan records rows across levels");
    require(mipmapped_plan.mip_level_reference_count == 3, "mipmapped staging plan records mip references");
    require(mipmapped_plan.total_row_payload_byte_count == 84, "mipmapped staging plan records unpadded payload bytes");
    require(mipmapped_plan.total_row_padding_byte_count == 28, "mipmapped staging plan records padded bytes");
    require(mipmapped_plan.total_staging_byte_count == 112, "mipmapped staging plan records aligned staging bytes");
    require(mipmapped_plan.referenced_mipmap_byte_count == 84, "mipmapped staging plan preserves mipmap byte references");
    require(mipmapped_plan.mipmaps_referenced, "mipmapped staging plan records generated mip references");
    require(mipmapped_plan.mip_level_references[1].mip_level == 1, "mipmapped staging plan records level one");
    require(!mipmapped_plan.mip_level_references[1].decoded_payload_backed, "generated mip reference is not decoded-backed");
    require(mipmapped_plan.mip_level_references[1].generated_mip_reference, "generated mip reference is explicit");
    require(mipmapped_plan.row_copies[4].mip_level == 1, "mipmapped row copy begins level one after base rows");
    require(mipmapped_plan.row_copies[6].mip_level == 2, "mipmapped row copy records terminal level");

    render_image_sampler_policy mismatched_sampler = sampler;
    mismatched_sampler.wrap_u = render_image_wrap_mode::repeat;
    const render_image_texture_upload_payload_layout_evidence mismatched_layout =
        make_render_image_texture_upload_payload_layout_evidence(key, mismatched_sampler, base_image);
    const render_image_texture_mipmap_upload_plan mismatched_mipmap_plan =
        make_render_image_texture_mipmap_upload_plan(base_image, mismatched_sampler);
    const render_image_texture_staging_payload_plan invalid_layout_plan =
        make_render_image_texture_staging_payload_plan(mismatched_layout, mismatched_mipmap_plan);
    require(!invalid_layout_plan.ok(), "staging payload plan blocks invalid layout evidence");
    require(
        invalid_layout_plan.status == render_image_texture_staging_payload_plan_status::blocked_invalid_layout,
        "invalid-layout staging plan reports layout blocker");
    require(invalid_layout_plan.decoded_payload_available, "invalid-layout staging plan preserves payload availability");
    require(!invalid_layout_plan.layout_ready, "invalid-layout staging plan records layout blocker");
    require(invalid_layout_plan.mipmap_plan_ready, "invalid-layout staging plan preserves mipmap readiness");
    require(invalid_layout_plan.row_copy_count == 0, "invalid-layout staging plan records no row copies");
    require(invalid_layout_plan.total_staging_byte_count == 0, "invalid-layout staging plan records no staging bytes");
    require(
        invalid_layout_plan.blocker_summary
            == "image texture upload payload layout sampler does not match texture key",
        "invalid-layout staging plan preserves layout diagnostic");

    render_decoded_image invalid_payload = base_image;
    invalid_payload.pixels.pop_back();
    const render_image_texture_upload_payload_layout_evidence invalid_layout =
        make_render_image_texture_upload_payload_layout_evidence(key, sampler, invalid_payload);
    const render_image_texture_mipmap_upload_plan invalid_mipmap_plan =
        make_render_image_texture_mipmap_upload_plan(invalid_payload, sampler);
    const render_image_texture_staging_payload_plan blocked_plan =
        make_render_image_texture_staging_payload_plan(invalid_layout, invalid_mipmap_plan);
    require(!blocked_plan.ok(), "staging payload plan preserves blocked state for missing decoded bytes");
    require(
        blocked_plan.status == render_image_texture_staging_payload_plan_status::blocked_missing_payload,
        "staging payload plan reports missing payload blocker");
    require(blocked_plan.total_staging_byte_count == 0, "blocked staging plan records no staging bytes");
    require(blocked_plan.row_copy_count == 0, "blocked staging plan records no row copies");
    require(
        blocked_plan.diagnostic
            == "image texture staging payload plan blocked: decoded payload bytes are missing or inconsistent",
        "blocked staging plan diagnostic is stable");
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
    require(uploaded.staging_payload_plan.ok(), "texture uploader result records ready staging payload plan");
    require(uploaded.staging_payload_plan.total_staging_byte_count == 8, "texture uploader result records staging plan bytes");
    require(uploaded.staging_payload_plan.row_copy_count == 1, "texture uploader result records staging row copy");
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
    require(uploader.upload_request_snapshots[0].payload_layout.ok(), "request snapshot records ready payload layout");
    require(uploader.upload_request_snapshots[0].payload_layout.extent_width == 2, "request payload layout records width");
    require(uploader.upload_request_snapshots[0].payload_layout.extent_height == 1, "request payload layout records height");
    require(uploader.upload_request_snapshots[0].payload_layout.row_stride_byte_count == 8, "request payload layout records row stride");
    require(uploader.upload_request_snapshots[0].payload_layout.byte_count_consistent, "request payload layout records byte consistency");
    require(uploader.upload_request_snapshots[0].staging_payload_plan.ok(), "request snapshot records ready staging plan");
    require(
        uploader.upload_request_snapshots[0].staging_payload_plan.total_staging_byte_count == 8,
        "request staging plan records total bytes");
    require(
        uploader.upload_request_snapshots[0].payload_layout.stable_texture_cache_key
            == make_render_image_texture_key_diagnostic(key).stable_cache_key,
        "request payload layout records stable cache key");
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
    require(uploader.upload_result_snapshots[0].payload_layout.ok(), "result snapshot records ready payload layout");
    require(uploader.upload_result_snapshots[0].payload_layout.row_stride_byte_count == 8, "result payload layout records row stride");
    require(uploader.upload_result_snapshots[0].staging_payload_plan.ok(), "result snapshot records ready staging plan");
    require(uploader.upload_result_snapshots[0].staging_payload_plan.row_copies.size() == 1, "result staging plan records row copy");
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
    require(snapshot.queue_entries[0].payload_layout.ok(), "queue entry records ready payload layout");
    require(snapshot.queue_entries[0].payload_layout.staging_byte_count == 8, "queue payload layout records staging bytes");
    require(snapshot.queue_entries[0].staging_payload_plan.ok(), "queue entry records ready staging plan");
    require(snapshot.queue_entries[0].staging_payload_plan.row_copies[0].row_payload_byte_count == 8, "queue staging plan records row bytes");
    require(snapshot.entries[0].generation_id == 1, "texture uploader snapshot records generation id");
    require(snapshot.entries[0].key == key, "texture uploader snapshot records key");
    require(snapshot.entries[0].sampler == render_image_sampler_policy{}, "texture uploader snapshot records sampler");
    require(snapshot.entries[0].texture.id == uploaded.texture.id, "texture uploader snapshot records handle");
    require(snapshot.entries[0].status == render_image_texture_upload_status::uploaded, "snapshot records status");
    require(snapshot.entries[0].staging_byte_count == 8, "texture uploader snapshot records staging bytes");
    require(snapshot.entries[0].payload_layout.ok(), "texture uploader snapshot records ready payload layout");
    require(snapshot.entries[0].payload_layout.row_stride_byte_count == 8, "snapshot entry payload layout records row stride");
    require(snapshot.entries[0].staging_payload_plan.ok(), "texture uploader snapshot records ready staging plan");
    require(snapshot.entries[0].staging_payload_plan.total_staging_byte_count == 8, "snapshot entry staging plan records bytes");
    require(
        snapshot.entries[0].mipmap_upload_plan.total_staging_byte_count == 8,
        "texture uploader snapshot entry records mipmap staging bytes");
    require(snapshot.entries[0].request.generation_id == 1, "entry request snapshot records generation id");
    require(snapshot.entries[0].request.payload_layout.ok(), "entry request snapshot preserves payload layout");
    require(snapshot.entries[0].request.staging_payload_plan.ok(), "entry request snapshot preserves staging plan");
    require(snapshot.entries[0].result.texture.id == uploaded.texture.id, "entry result snapshot records handle");
    require(snapshot.entries[0].result.payload_layout.ok(), "entry result snapshot preserves payload layout");
    require(snapshot.entries[0].result.staging_payload_plan.ok(), "entry result snapshot preserves staging plan");
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
    require(uploaded.staging_payload_plan.ok(), "mipmapped upload result records ready staging plan");
    require(uploaded.staging_payload_plan.status == render_image_texture_staging_payload_plan_status::ready_with_mip_references, "mipmapped upload result records staging mip references");
    require(uploaded.staging_payload_plan.total_staging_byte_count == 84, "mipmapped upload result records unpadded staging plan bytes");
    require(uploaded.staging_payload_plan.row_copy_count == 7, "mipmapped upload result records staging rows");
    require(uploaded.staging_payload_plan.mip_level_reference_count == 3, "mipmapped upload result records staging mip references");
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
    require(snapshot.entries[0].staging_payload_plan.mipmaps_referenced, "entry snapshot records staging mip references");
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

void test_texture_upload_operation_plan_reports_ready_placeholder_and_retryable_packets()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key base_key{
        .source_key = "textures/operation-base.ppm",
        .sampler = render_image_sampler_policy{},
    };
    const render_image_sampler_policy mipmapped_sampler = make_mipmapped_sampler();
    const render_image_texture_key mipmapped_key{
        .source_key = "textures/operation-mips.ppm",
        .sampler = mipmapped_sampler,
    };
    const render_image_texture_key placeholder_key = make_fake_image_texture_placeholder_key(
        fake_image_texture_placeholder_policy{},
        fake_image_texture_placeholder_reason::decode_failed,
        mipmapped_key);

    fake_image_texture_uploader uploader;
    const render_image_texture_upload_result uploaded = uploader.upload(render_image_texture_upload_request{
        .key = base_key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });
    const render_image_texture_upload_result placeholder = uploader.upload(render_image_texture_upload_request{
        .key = placeholder_key,
        .sampler = mipmapped_sampler,
        .image = make_rgba_4x4_decoded_image(),
    });

    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    const render_image_texture_upload_result failed = uploader.upload(render_image_texture_upload_request{
        .key = base_key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });

    require(uploaded.ok(), "operation plan starts from successful base upload");
    require(placeholder.ok(), "operation plan includes successful placeholder upload");
    require(!failed.ok(), "operation plan includes retryable failure");

    const render_image_texture_upload_operation_plan plan =
        plan_render_image_texture_upload_operations(uploader.diagnostic_snapshot());

    require(!plan.ok(), "operation plan is blocked when any upload packet failed");
    require(plan.upload_count == 3, "operation plan records source upload count");
    require(plan.packet_count == 3, "operation plan emits one packet per generation");
    require(plan.ready_packet_count == 2, "operation plan counts ready packets");
    require(plan.placeholder_packet_count == 1, "operation plan counts placeholder packets");
    require(plan.fallback_packet_count == 1, "operation plan counts fallback packets");
    require(plan.blocked_packet_count == 1, "operation plan counts blocked packets");
    require(plan.retryable_blocked_packet_count == 1, "operation plan counts retryable blockers");
    require(plan.nonretryable_blocked_packet_count == 0, "operation plan reports no nonretryable blockers");
    require(plan.total_staging_byte_count == 92, "operation plan sums successful staging bytes");
    require(plan.total_mip_level_count == 5, "operation plan sums planned mip levels");
    require(plan.total_mipmap_byte_count == 100, "operation plan preserves planned mip bytes");
    require(plan.has_blockers, "operation plan exposes blocker flag");
    require(plan.blocker_summary.find("invalid_image") != std::string::npos, "operation plan carries blocker reason");

    const render_image_texture_upload_operation_packet& base_packet = plan.packets[0];
    require(base_packet.ok(), "base upload packet is ready");
    require(base_packet.status == render_image_texture_upload_operation_packet_status::ready, "base packet status is ready");
    require(base_packet.status_name == "ready", "operation packet ready status name is stable");
    require(base_packet.upload_status_name == "uploaded", "operation packet upload status name is stable");
    require(base_packet.texture_key == base_key, "operation packet records texture key");
    require(base_packet.texture.id == uploaded.texture.id, "operation packet records texture handle id");
    require(base_packet.generation_id == 1, "operation packet records generation id");
    require(base_packet.staging_byte_count == 8, "operation packet records base staging bytes");
    require(base_packet.staging_payload_plan.ok(), "operation packet records ready staging plan");
    require(base_packet.staging_payload_plan.row_copy_count == 1, "operation packet records staging row copies");
    require(
        base_packet.staging_payload_plan.stable_texture_cache_key == base_packet.payload_layout.stable_texture_cache_key,
        "operation packet staging plan preserves cache identity");
    require(base_packet.mip_level_count == 1, "operation packet records base mip level count");
    require(!base_packet.placeholder_texture, "base upload packet is not placeholder");
    require(base_packet.ready_for_upload, "base upload packet is ready for backend upload");
    require(!base_packet.blocked, "base upload packet is not blocked");

    const render_image_texture_upload_operation_packet& placeholder_packet = plan.packets[1];
    require(placeholder_packet.ok(), "placeholder upload packet is backend-ready");
    require(
        placeholder_packet.status == render_image_texture_upload_operation_packet_status::placeholder_ready,
        "placeholder packet reports placeholder-ready status");
    require(placeholder_packet.status_name == "placeholder_ready", "placeholder status name is stable");
    require(placeholder_packet.placeholder_texture, "placeholder packet is flagged as placeholder");
    require(placeholder_packet.fallback_texture, "placeholder packet is flagged as fallback");
    require(placeholder_packet.texture.id == placeholder.texture.id, "placeholder packet records texture handle");
    require(placeholder_packet.sampler == mipmapped_sampler, "placeholder packet preserves sampler policy");
    require(placeholder_packet.mip_level_count == 3, "placeholder packet records mipmapped level count");
    require(placeholder_packet.staging_byte_count == 84, "placeholder packet records mipmapped staging bytes");
    require(placeholder_packet.staging_payload_plan.ok(), "placeholder packet records ready staging plan");
    require(placeholder_packet.staging_payload_plan.mipmaps_referenced, "placeholder staging plan records mip references");
    require(placeholder_packet.staging_payload_plan.total_staging_byte_count == 84, "placeholder staging plan records staging bytes");
    require(
        placeholder_packet.readiness_summary == "placeholder texture upload packet is ready",
        "placeholder packet has stable readiness summary");

    const render_image_texture_upload_operation_packet& failed_packet = plan.packets[2];
    require(!failed_packet.ok(), "failed upload packet is blocked");
    require(
        failed_packet.status == render_image_texture_upload_operation_packet_status::blocked_retryable,
        "failed upload packet reports retryable blocker");
    require(failed_packet.status_name == "blocked_retryable", "retryable blocker status name is stable");
    require(failed_packet.retryable, "failed upload packet carries retryable flag");
    require(failed_packet.retry_eligibility_name == "eligible", "failed upload packet carries retry eligibility name");
    require(failed_packet.attempt_count_for_key == 2, "failed upload packet carries key attempt count");
    require(failed_packet.failed_attempt_count_for_key == 1, "failed upload packet carries failed attempt count");
    require(
        failed_packet.retry_after_queue_sequence_delta == 1,
        "failed upload packet carries retry backoff sequence delta");
    require(failed_packet.blocked, "failed upload packet is blocked");
    require(!failed_packet.has_texture_handle, "failed upload packet has no texture handle");
    require(!failed_packet.staging_payload_plan.ok(), "failed upload packet records blocked staging plan");
    require(
        failed_packet.staging_payload_plan.status
            == render_image_texture_staging_payload_plan_status::blocked_missing_payload,
        "failed upload packet preserves staging blocker");
    require(
        failed_packet.blocker_summary == "upload failed but can retry: invalid_image",
        "failed upload packet has stable blocker summary");
}

void test_texture_upload_operation_plan_reports_invalid_mipmap_and_missing_snapshot_blockers()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key valid_key{
        .source_key = "textures/operation-invalid-plan.ppm",
        .sampler = render_image_sampler_policy{},
    };
    fake_image_texture_uploader uploader;
    const render_image_texture_upload_result uploaded = uploader.upload(render_image_texture_upload_request{
        .key = valid_key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });
    require(uploaded.ok(), "invalid mipmap operation test starts with successful upload");

    fake_image_texture_upload_snapshot snapshot = uploader.diagnostic_snapshot();
    set_upload_snapshot_plan_for_generation(
        snapshot,
        uploaded.generation_id,
        make_mipmap_plan_with_status(render_image_texture_mipmap_upload_plan_status::overflow, 1, 4));

    const render_image_texture_upload_operation_plan invalid_plan =
        plan_render_image_texture_upload_operations(snapshot);

    require(!invalid_plan.ok(), "operation plan blocks uploaded packet with invalid mipmap plan");
    require(invalid_plan.packet_count == 1, "invalid mipmap operation plan has one packet");
    require(invalid_plan.blocked_packet_count == 1, "invalid mipmap operation plan counts blocker");
    require(invalid_plan.invalid_mipmap_plan_packet_count == 1, "invalid mipmap operation plan counts invalid plan");
    require(invalid_plan.overflow_mipmap_plan_packet_count == 1, "invalid mipmap operation plan counts overflow");
    require(
        invalid_plan.packets[0].status
            == render_image_texture_upload_operation_packet_status::blocked_invalid_mipmap_plan,
        "invalid mipmap packet reports mipmap blocker status");
    require(
        invalid_plan.packets[0].blocker_summary == "mipmap upload plan is not plannable: overflow",
        "invalid mipmap packet has stable blocker summary");

    remove_upload_result_and_queue_for_generation(snapshot, uploaded.generation_id);
    const render_image_texture_upload_operation_plan missing_snapshot_plan =
        plan_render_image_texture_upload_operations(snapshot);

    require(!missing_snapshot_plan.ok(), "operation plan blocks request with missing result and queue snapshots");
    require(missing_snapshot_plan.packet_count == 1, "missing snapshot operation plan keeps request generation");
    require(missing_snapshot_plan.missing_snapshot_packet_count == 1, "missing snapshot operation plan counts missing snapshots");
    require(
        missing_snapshot_plan.packets[0].status
            == render_image_texture_upload_operation_packet_status::blocked_missing_snapshot,
        "missing snapshot packet status is stable");
    require(
        missing_snapshot_plan.packets[0].blocker_summary == "upload result and queue snapshots are missing",
        "missing snapshot packet has stable blocker summary");
    require(
        render_image_texture_upload_operation_packet_status_name(
            render_image_texture_upload_operation_packet_status::blocked_missing_texture_handle)
            == "blocked_missing_texture_handle",
        "operation packet status helper names missing handle status");
}

void test_texture_upload_result_snapshot_reports_accepted_rejected_and_placeholder_packets()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key base_key{
        .source_key = "textures/result-base.ppm",
        .sampler = render_image_sampler_policy{},
    };
    const render_image_sampler_policy mipmapped_sampler = make_mipmapped_sampler();
    const render_image_texture_key mipmapped_key{
        .source_key = "textures/result-mips.ppm",
        .sampler = mipmapped_sampler,
    };
    const render_image_texture_key placeholder_key = make_fake_image_texture_placeholder_key(
        fake_image_texture_placeholder_policy{},
        fake_image_texture_placeholder_reason::upload_failed,
        mipmapped_key);

    fake_image_texture_uploader uploader;
    const render_image_texture_upload_result uploaded = uploader.upload(render_image_texture_upload_request{
        .key = base_key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });
    const render_image_texture_upload_result placeholder = uploader.upload(render_image_texture_upload_request{
        .key = placeholder_key,
        .sampler = mipmapped_sampler,
        .image = make_rgba_4x4_decoded_image(),
    });
    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    const render_image_texture_upload_result failed = uploader.upload(render_image_texture_upload_request{
        .key = base_key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });

    require(uploaded.ok(), "upload result snapshot starts from accepted base upload");
    require(placeholder.ok(), "upload result snapshot includes accepted placeholder upload");
    require(!failed.ok(), "upload result snapshot includes rejected upload");

    const render_image_texture_upload_result_snapshot result_snapshot =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
            uploader.diagnostic_snapshot());

    require(!result_snapshot.ok(), "upload result snapshot reports rejected packet");
    require(result_snapshot.source_upload_count == 3, "upload result snapshot records source upload count");
    require(result_snapshot.operation_packet_count == 3, "upload result snapshot records operation packet count");
    require(result_snapshot.packet_count == 3, "upload result snapshot records packet count");
    require(result_snapshot.accepted_packet_count == 2, "upload result snapshot counts accepted packets");
    require(result_snapshot.rejected_packet_count == 1, "upload result snapshot counts rejected packets");
    require(result_snapshot.placeholder_packet_count == 1, "upload result snapshot counts placeholders");
    require(result_snapshot.fallback_packet_count == 1, "upload result snapshot counts fallback packets");
    require(result_snapshot.retryable_rejected_packet_count == 1, "upload result snapshot counts retryable rejections");
    require(result_snapshot.nonretryable_rejected_packet_count == 0, "upload result snapshot reports no nonretryable rejections");
    require(result_snapshot.blocker_count == 1, "upload result snapshot counts blockers");
    require(result_snapshot.texture_count == 2, "upload result snapshot counts accepted texture handles");
    require(result_snapshot.request_id_count == 3, "upload result snapshot counts request ids");
    require(result_snapshot.total_mip_level_count == 5, "upload result snapshot sums all planned mip levels");
    require(result_snapshot.accepted_mip_level_count == 4, "upload result snapshot sums accepted mip levels");
    require(result_snapshot.rejected_mip_level_count == 1, "upload result snapshot sums rejected mip levels");
    require(result_snapshot.total_uploaded_byte_count == 92, "upload result snapshot sums accepted upload bytes");
    require(result_snapshot.total_planned_staging_byte_count == 92, "upload result snapshot sums planned staging bytes");
    require(result_snapshot.total_planned_mipmap_byte_count == 100, "upload result snapshot sums planned mipmap bytes");
    require(result_snapshot.request_ids.size() == 3, "upload result snapshot exposes request ids");
    require(result_snapshot.texture_ids.size() == 2, "upload result snapshot exposes texture ids");
    require(result_snapshot.has_rejections, "upload result snapshot exposes rejection flag");
    require(result_snapshot.has_placeholders, "upload result snapshot exposes placeholder flag");
    require(result_snapshot.has_retryable_rejections, "upload result snapshot exposes retryable rejection flag");
    require(
        result_snapshot.rejected_summary.find("invalid_image") != std::string::npos,
        "upload result snapshot preserves rejection summary");
    require(
        result_snapshot.texture_summary.find("#texture=1") != std::string::npos,
        "upload result snapshot preserves texture id summary");

    const render_image_texture_upload_result_packet_snapshot& base_packet = result_snapshot.packets[0];
    require(base_packet.ok(), "base upload result packet is accepted");
    require(base_packet.status == render_image_texture_upload_result_packet_status::accepted, "base result packet is accepted");
    require(base_packet.status_name == "accepted", "base result packet status name is stable");
    require(base_packet.request_id == 1, "base result packet records request id");
    require(base_packet.generation_id == 1, "base result packet records generation id");
    require(base_packet.texture_id == uploaded.texture.id, "base result packet records texture id");
    require(base_packet.texture_revision == 1, "base result packet records texture revision");
    require(base_packet.stable_cache_key.find("textures/result-base.ppm") != std::string::npos, "base result packet records stable cache key");
    require(base_packet.sampler_summary == render_image_sampler_policy_stable_fragment(render_image_sampler_policy{}), "base result packet records sampler summary");
    require(base_packet.uploaded_byte_count == 8, "base result packet records uploaded bytes");
    require(base_packet.staging_payload_plan.ok(), "base result packet preserves staging plan");
    require(base_packet.staging_payload_plan.total_staging_byte_count == 8, "base result packet staging plan records bytes");
    require(base_packet.accepted_mip_level_count == 1, "base result packet records accepted mip count");
    require(!base_packet.placeholder_texture, "base result packet is not placeholder");

    const render_image_texture_upload_result_packet_snapshot& placeholder_packet = result_snapshot.packets[1];
    require(placeholder_packet.ok(), "placeholder result packet is accepted");
    require(
        placeholder_packet.status == render_image_texture_upload_result_packet_status::accepted_placeholder,
        "placeholder result packet reports placeholder accepted status");
    require(placeholder_packet.status_name == "accepted_placeholder", "placeholder result packet status name is stable");
    require(placeholder_packet.placeholder_texture, "placeholder result packet keeps placeholder flag");
    require(placeholder_packet.fallback_texture, "placeholder result packet keeps fallback flag");
    require(placeholder_packet.texture_id == placeholder.texture.id, "placeholder result packet records texture id");
    require(placeholder_packet.mip_level_count == 3, "placeholder result packet records mip levels");
    require(placeholder_packet.uploaded_byte_count == 84, "placeholder result packet records uploaded bytes");
    require(placeholder_packet.staging_payload_plan.ok(), "placeholder result packet preserves staging plan");
    require(placeholder_packet.staging_payload_plan.mip_level_reference_count == 3, "placeholder result packet records staging mip references");

    const render_image_texture_upload_result_packet_snapshot& failed_packet = result_snapshot.packets[2];
    require(!failed_packet.ok(), "failed result packet is rejected");
    require(
        failed_packet.status == render_image_texture_upload_result_packet_status::rejected_retryable,
        "failed result packet reports retryable rejection");
    require(failed_packet.status_name == "rejected_retryable", "failed result packet status name is stable");
    require(failed_packet.retryable, "failed result packet keeps retryable flag");
    require(failed_packet.uploaded_byte_count == 0, "failed result packet has no uploaded bytes");
    require(failed_packet.planned_mipmap_byte_count == 8, "failed result packet preserves planned mipmap bytes");
    require(!failed_packet.staging_payload_plan.ok(), "failed result packet preserves blocked staging plan");
    require(failed_packet.blocker_summary == "upload failed but can retry: invalid_image", "failed result packet keeps blocker summary");
    require(
        render_image_texture_upload_result_packet_status_name(
            render_image_texture_upload_result_packet_status::rejected_missing_texture_handle)
            == "rejected_missing_texture_handle",
        "upload result packet status helper names missing texture handle");
}

void test_texture_upload_result_diff_reports_added_changed_and_regressed_packets()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key base_key{
        .source_key = "textures/result-diff-base.ppm",
        .sampler = render_image_sampler_policy{},
    };
    const render_image_sampler_policy mipmapped_sampler = make_mipmapped_sampler();
    const render_image_texture_key mipmapped_key{
        .source_key = "textures/result-diff-mips.ppm",
        .sampler = mipmapped_sampler,
    };
    const render_image_texture_key placeholder_key = make_fake_image_texture_placeholder_key(
        fake_image_texture_placeholder_policy{},
        fake_image_texture_placeholder_reason::decode_failed,
        mipmapped_key);

    fake_image_texture_uploader uploader;
    const render_image_texture_upload_result base_upload = uploader.upload(render_image_texture_upload_request{
        .key = base_key,
        .sampler = render_image_sampler_policy{},
        .image = make_rgba_2x1_decoded_image(),
    });
    require(base_upload.ok(), "upload result diff starts from base accepted upload");

    const fake_image_texture_upload_snapshot before_fake_snapshot = uploader.diagnostic_snapshot();
    const render_image_texture_upload_result_snapshot before =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(before_fake_snapshot);

    uploader.upload(render_image_texture_upload_request{
        .key = placeholder_key,
        .sampler = mipmapped_sampler,
        .image = make_rgba_4x4_decoded_image(),
    });
    render_decoded_image invalid_payload = make_rgba_2x1_decoded_image();
    invalid_payload.pixels.pop_back();
    uploader.upload(render_image_texture_upload_request{
        .key = base_key,
        .sampler = render_image_sampler_policy{},
        .image = invalid_payload,
    });

    const render_image_texture_upload_result_snapshot after_added =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
            uploader.diagnostic_snapshot());
    const render_image_texture_upload_result_snapshot_diff added_diff =
        diff_render_image_texture_upload_result_snapshots(before, after_added);

    require(added_diff.ok(), "added upload result diff is not a transition regression");
    require(added_diff.has_changes, "added upload result diff reports changes");
    require(added_diff.before_packet_count == 1, "added upload result diff records before packet count");
    require(added_diff.after_packet_count == 3, "added upload result diff records after packet count");
    require(added_diff.added_packet_count == 2, "added upload result diff counts added packets");
    require(added_diff.changed_packet_count == 0, "added upload result diff reports no changed existing packets");
    require(added_diff.accepted_packet_delta == 1, "added upload result diff records accepted packet delta");
    require(added_diff.rejected_packet_delta == 1, "added upload result diff records rejected packet delta");
    require(added_diff.texture_count_delta == 1, "added upload result diff records texture count delta");
    require(added_diff.placeholder_packet_delta == 1, "added upload result diff records placeholder delta");
    require(added_diff.retryable_rejected_packet_delta == 1, "added upload result diff records retryable rejection delta");
    require(added_diff.blocker_count_delta == 1, "added upload result diff records blocker delta");
    require(added_diff.mip_level_count_delta == 4, "added upload result diff records mip level delta");
    require(added_diff.uploaded_byte_delta == 84, "added upload result diff records uploaded byte delta");
    require(added_diff.planned_mipmap_byte_delta == 92, "added upload result diff records planned mipmap byte delta");
    require(added_diff.before_staging_payload_byte_count == 8, "added upload result diff records before staging plan bytes");
    require(added_diff.after_staging_payload_byte_count == 92, "added upload result diff records after staging plan bytes");
    require(added_diff.staging_payload_byte_delta == 84, "added upload result diff records staging plan byte delta");
    require(added_diff.staging_payload_plan_changed_count == 2, "added upload result diff counts added staging plans");
    require(
        added_diff.staging_payload_summary.find("request=2:staging_bytes=0->84") != std::string::npos,
        "added upload result diff summarizes added staging payload bytes");
    require(
        added_diff.changed_packet_summary.find("request=2:added") != std::string::npos,
        "added upload result diff summarizes added packet");
    require(
        render_image_texture_upload_result_diff_entry_status_name(
            render_image_texture_upload_result_diff_entry_status::changed)
            == "changed",
        "upload result diff status name is stable");

    fake_image_texture_upload_snapshot texture_changed_fake_snapshot = before_fake_snapshot;
    set_upload_snapshot_texture_for_generation(
        texture_changed_fake_snapshot,
        base_upload.generation_id,
        render_image_texture_handle{.id = 42, .revision = 7, .width = 2, .height = 1});
    const render_image_texture_upload_result_snapshot texture_changed =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(texture_changed_fake_snapshot);
    const render_image_texture_upload_result_snapshot_diff texture_diff =
        diff_render_image_texture_upload_result_snapshots(before, texture_changed);

    require(texture_diff.ok(), "texture handle change is not a rejection regression");
    require(texture_diff.changed_packet_count == 1, "texture handle diff counts changed packet");
    require(texture_diff.texture_changed_count == 1, "texture handle diff counts changed texture");
    require(
        texture_diff.changed_texture_summary == "request=1:texture=1->42",
        "texture handle diff emits stable texture summary");

    fake_image_texture_upload_snapshot staging_changed_fake_snapshot = before_fake_snapshot;
    render_image_texture_staging_payload_plan changed_staging =
        make_render_image_texture_staging_payload_plan(
            before_fake_snapshot.request_snapshots[0].payload_layout,
            before_fake_snapshot.request_snapshots[0].mipmap_upload_plan,
            16);
    changed_staging.stable_texture_cache_key = "mutated-cache-key";
    changed_staging.sampler_summary = "mutated-sampler";
    set_upload_snapshot_staging_plan_for_generation(
        staging_changed_fake_snapshot,
        base_upload.generation_id,
        changed_staging);
    const render_image_texture_upload_result_snapshot staging_changed =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(staging_changed_fake_snapshot);
    const render_image_texture_upload_result_snapshot_diff staging_diff =
        diff_render_image_texture_upload_result_snapshots(before, staging_changed);

    require(staging_diff.ok(), "staging payload diff keeps ready aligned plan non-regressive");
    require(staging_diff.changed_packet_count == 1, "staging payload diff counts changed packet");
    require(staging_diff.staging_payload_plan_changed_count == 1, "staging payload diff counts plan change");
    require(staging_diff.staging_alignment_changed_count == 1, "staging payload diff counts alignment change");
    require(staging_diff.staging_padding_changed_count == 1, "staging payload diff counts padding change");
    require(staging_diff.staging_byte_count_changed_count == 1, "staging payload diff counts byte change");
    require(staging_diff.staging_cache_key_changed_count == 1, "staging payload diff counts cache-key change");
    require(staging_diff.staging_sampler_changed_count == 1, "staging payload diff counts sampler change");
    require(staging_diff.before_staging_payload_byte_count == 8, "staging payload diff records before aggregate bytes");
    require(staging_diff.after_staging_payload_byte_count == 16, "staging payload diff records after aggregate bytes");
    require(staging_diff.staging_payload_byte_delta == 8, "staging payload diff records aggregate byte delta");
    require(
        staging_diff.staging_payload_summary
            == "request=1:staging_bytes=8->16,alignment=4->16,row_padding=0->8,cache_key_changed,sampler_changed",
        "staging payload diff emits compact layout and identity summary");
    require(
        staging_diff.entries[0].staging_payload_plan_diff.change_summary
            == "staging_bytes=8->16,alignment=4->16,row_padding=0->8,cache_key_changed,sampler_changed",
        "staging payload packet diff records compact change summary");
    require(
        staging_diff.entries[0].staging_payload_plan_diff.alignment_changed,
        "staging payload packet diff records alignment evidence");
    require(
        staging_diff.entries[0].staging_payload_plan_diff.padding_changed,
        "staging payload packet diff records padding evidence");
    require(
        staging_diff.entries[0].staging_payload_plan_diff.total_staging_byte_delta == 8,
        "staging payload packet diff records staging byte delta");
    require(
        staging_diff.entries[0].staging_payload_plan_diff.cache_key_changed,
        "staging payload packet diff records cache identity change");
    require(
        staging_diff.entries[0].staging_payload_plan_diff.sampler_changed,
        "staging payload packet diff records sampler identity change");

    fake_image_texture_upload_snapshot regressed_fake_snapshot = before_fake_snapshot;
    set_upload_snapshot_status_for_generation(
        regressed_fake_snapshot,
        base_upload.generation_id,
        render_image_texture_upload_status::invalid_image,
        fake_image_texture_upload_retry_eligibility::eligible);
    const render_image_texture_upload_result_snapshot regressed =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(regressed_fake_snapshot);
    const render_image_texture_upload_result_snapshot_diff regression_diff =
        diff_render_image_texture_upload_result_snapshots(before, regressed);

    require(!regression_diff.ok(), "accepted-to-rejected upload result diff is a regression");
    require(regression_diff.has_regression, "upload result diff exposes regression flag");
    require(regression_diff.accepted_to_rejected_count == 1, "upload result diff counts accepted-to-rejected transition");
    require(regression_diff.retryability_changed_count == 1, "upload result diff counts retryability change");
    require(regression_diff.blocker_changed_count == 1, "upload result diff counts blocker change");
    require(regression_diff.uploaded_byte_delta == -8, "upload result diff records uploaded byte regression");
    require(
        regression_diff.regression_summary.find("changed") != std::string::npos,
        "upload result diff exposes regression summary");

    fake_image_texture_upload_snapshot staging_regressed_fake_snapshot = before_fake_snapshot;
    render_image_texture_staging_payload_plan blocked_staging =
        before_fake_snapshot.request_snapshots[0].staging_payload_plan;
    blocked_staging.status = render_image_texture_staging_payload_plan_status::blocked_invalid_layout;
    blocked_staging.status_name = render_image_texture_staging_payload_plan_status_name(blocked_staging.status);
    blocked_staging.ready = false;
    blocked_staging.blocked = true;
    blocked_staging.blocker_summary = "mutated staging blocker";
    blocked_staging.diagnostic = "image texture staging payload plan blocked: mutated staging blocker";
    set_upload_snapshot_staging_plan_for_generation(
        staging_regressed_fake_snapshot,
        base_upload.generation_id,
        blocked_staging);
    const render_image_texture_upload_result_snapshot staging_regressed =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(staging_regressed_fake_snapshot);
    const render_image_texture_upload_result_snapshot_diff staging_regression_diff =
        diff_render_image_texture_upload_result_snapshots(before, staging_regressed);

    require(!staging_regression_diff.ok(), "ready-to-blocked staging plan diff is a regression");
    require(staging_regression_diff.has_regression, "staging regression diff exposes regression flag");
    require(
        staging_regression_diff.staging_payload_plan_changed_count == 1,
        "staging regression diff counts changed staging plan");
    require(
        staging_regression_diff.staging_blocker_changed_count == 1,
        "staging regression diff counts blocker transition");
    require(
        staging_regression_diff.staging_regression_count == 1,
        "staging regression diff counts staging regression");
    require(
        staging_regression_diff.entries[0].staging_payload_plan_diff.ready_regressed,
        "staging regression packet diff records ready regression");
    require(
        staging_regression_diff.entries[0].staging_payload_plan_diff.after_blocker_summary
            == "mutated staging blocker",
        "staging regression packet diff preserves blocker summary");
    require(
        staging_regression_diff.staging_payload_summary == "request=1:blocker=ready->blocked_invalid_layout",
        "staging regression diff summarizes blocker transition");

    const render_image_texture_upload_result_snapshot_diff staging_recovery_diff =
        diff_render_image_texture_upload_result_snapshots(staging_regressed, before);
    require(staging_recovery_diff.ok(), "blocked-to-ready staging plan diff is a recovery");
    require(staging_recovery_diff.has_recovery, "staging recovery diff exposes recovery flag");
    require(
        staging_recovery_diff.staging_recovery_count == 1,
        "staging recovery diff counts staging recovery");
    require(
        staging_recovery_diff.entries[0].staging_payload_plan_diff.ready_recovered,
        "staging recovery packet diff records ready recovery");
    require(
        staging_recovery_diff.staging_payload_summary == "request=1:blocker=blocked_invalid_layout->ready",
        "staging recovery diff summarizes blocker recovery");
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
    test_upload_payload_layout_evidence_records_extent_stride_and_identity();
    test_staging_payload_plan_records_rows_alignment_and_blockers();
    test_texture_uploader_uploads_valid_decoded_image();
    test_texture_uploader_records_mipmap_upload_plan();
    test_texture_uploader_reports_deterministic_queue_lifecycle();
    test_texture_uploader_reports_retry_eligibility_and_backoff();
    test_texture_upload_operation_plan_reports_ready_placeholder_and_retryable_packets();
    test_texture_upload_operation_plan_reports_invalid_mipmap_and_missing_snapshot_blockers();
    test_texture_upload_result_snapshot_reports_accepted_rejected_and_placeholder_packets();
    test_texture_upload_result_diff_reports_added_changed_and_regressed_packets();
    test_texture_upload_snapshot_diff_reports_added_uploads_and_byte_deltas();
    test_texture_upload_snapshot_diff_reports_changed_transitions_and_regressions();
    test_texture_uploader_rejects_invalid_inputs();
    return 0;
}
