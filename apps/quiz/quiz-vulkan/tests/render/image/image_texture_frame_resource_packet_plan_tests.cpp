#include "render/image/image_texture_frame_resource_packet_plan.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <string>

namespace {

template <typename T>
concept HasFakeCacheSnapshotField = requires(T value) {
    { value.cache_snapshot } -> std::same_as<quiz_vulkan::render::fake_image_texture_cache_snapshot&>;
};

template <typename T>
concept HasFakeUploadSnapshotField = requires(T value) {
    { value.upload_snapshot } -> std::same_as<quiz_vulkan::render::fake_image_texture_upload_snapshot&>;
};

static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan_entry>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_draw_list_texture_frame_composition_entry>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_draw_list_texture_frame_composition>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_draw_list_texture_frame_composition_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_draw_list_texture_frame_composition>);

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::render_image_texture_frame_upload_handoff_entry make_ready_handoff_entry(
    std::size_t request_index,
    std::string stable_cache_key,
    bool placeholder_backed)
{
    using namespace quiz_vulkan::render;

    const render_image_sampler_policy sampler;
    const std::string sampler_key = render_image_sampler_policy_stable_fragment(sampler);
    render_image_texture_frame_upload_handoff_entry entry;
    entry.sequence = request_index + 1;
    entry.request_index = request_index;
    entry.status = placeholder_backed
        ? render_image_texture_frame_upload_handoff_entry_status::placeholder
        : render_image_texture_frame_upload_handoff_entry_status::ready;
    entry.status_name = render_image_texture_frame_upload_handoff_entry_status_name(entry.status);
    entry.render_image_uri = stable_cache_key;
    entry.normalized_uri = stable_cache_key;
    entry.cache_key = stable_cache_key;
    entry.source_kind = render_image_source_kind::asset_uri;
    entry.sampler = sampler;
    entry.sampler_key = sampler_key;
    entry.texture_key = render_image_texture_key{.source_key = stable_cache_key, .sampler = sampler};
    entry.stable_texture_cache_key = stable_cache_key + "|" + sampler_key;
    entry.texture_id = 100 + request_index;
    entry.texture_revision = 7;
    entry.texture_width = placeholder_backed ? 2 : 4;
    entry.texture_height = placeholder_backed ? 2 : 1;
    entry.upload_request_id = 200 + request_index;
    entry.upload_generation_id = 5;
    entry.mip_level_count = 1;
    entry.uploaded_byte_count = placeholder_backed ? 16 : 8;
    entry.requested = true;
    entry.upload_result_present = true;
    entry.ready = !placeholder_backed;
    entry.placeholder_texture = placeholder_backed;
    entry.blocked = false;
    entry.missing_upload_result = false;
    entry.missing_frame_binding = false;
    entry.renderer_handoff_ready = true;
    entry.cache_reused = request_index != 0;
    entry.expected_cache_reuse = request_index != 0;
    entry.cache_key_summary = entry.stable_texture_cache_key;
    entry.sampler_summary = sampler_key;
    entry.diagnostic = placeholder_backed
        ? "image frame upload handoff entry uses placeholder texture"
        : "image frame upload handoff entry is ready";
    return entry;
}

quiz_vulkan::render::render_image_texture_frame_upload_handoff_entry make_blocked_handoff_entry(
    std::size_t request_index,
    quiz_vulkan::render::render_image_texture_frame_upload_handoff_entry_status status)
{
    using namespace quiz_vulkan::render;

    const render_image_sampler_policy sampler;
    const std::string stable_cache_key = "asset://textures/blocked-" + std::to_string(request_index);
    const std::string sampler_key = render_image_sampler_policy_stable_fragment(sampler);
    render_image_texture_frame_upload_handoff_entry entry;
    entry.sequence = request_index + 1;
    entry.request_index = request_index;
    entry.status = status;
    entry.status_name = render_image_texture_frame_upload_handoff_entry_status_name(status);
    entry.render_image_uri = stable_cache_key;
    entry.normalized_uri = stable_cache_key;
    entry.cache_key = stable_cache_key;
    entry.source_kind = render_image_source_kind::asset_uri;
    entry.sampler = sampler;
    entry.sampler_key = sampler_key;
    entry.texture_key = render_image_texture_key{.source_key = stable_cache_key, .sampler = sampler};
    entry.stable_texture_cache_key = stable_cache_key + "|" + sampler_key;
    entry.requested = status != render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding;
    entry.upload_result_present = status != render_image_texture_frame_upload_handoff_entry_status::missing_upload_result;
    entry.blocked = status != render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding;
    entry.missing_upload_result =
        status == render_image_texture_frame_upload_handoff_entry_status::missing_upload_result;
    entry.missing_frame_binding =
        status == render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding;
    entry.cache_key_summary = entry.stable_texture_cache_key;
    entry.sampler_summary = sampler_key;
    entry.blocker_summary = "blocked image ref";
    entry.diagnostic = "image frame upload handoff entry is blocked";
    return entry;
}

quiz_vulkan::render::render_image_texture_batch_execution_diagnostics make_ready_batch_execution(
    const quiz_vulkan::render::render_image_texture_batch_plan& batch_plan)
{
    using namespace quiz_vulkan::render;

    render_image_texture_batch_execution_diagnostics execution;
    execution.request_count = batch_plan.request_count;
    execution.planned_request_count = batch_plan.planned_request_count;
    execution.invalid_request_count = batch_plan.invalid_request_count;
    execution.all_planned_requests_executed = batch_plan.invalid_request_count == 0;

    for (const render_image_texture_batch_plan_entry& plan_entry : batch_plan.entries) {
        if (!plan_entry.ok()) {
            continue;
        }
        render_image_texture_batch_execution_entry entry{
            .sequence = plan_entry.request_index + 1,
            .request_index = plan_entry.request_index,
            .plan_status = plan_entry.status,
            .status = render_image_texture_batch_execution_entry_status::ready,
            .request = plan_entry.pipeline_request,
            .pipeline_status = render_image_texture_pipeline_status::ready,
            .source_bytes_status = render_image_source_bytes_load_status::loaded,
            .texture_status = render_image_texture_status::ready,
            .texture_key = plan_entry.texture_key,
            .texture = render_image_texture_handle{
                .id = 700 + plan_entry.request_index,
                .revision = 3,
                .width = 64 + plan_entry.request_index,
                .height = 32,
            },
            .executed = true,
            .ready = true,
            .expected_cache_reuse = plan_entry.expects_cache_reuse,
            .cache_reuse_expectation_matched = true,
            .diagnostic = "test frame execution entry is ready",
        };
        execution.entries.push_back(entry);
        ++execution.executed_request_count;
        ++execution.ready_count;
    }

    execution.diagnostic = "test frame execution is data-only ready";
    return execution;
}

quiz_vulkan::render::render_image_texture_frame_resource_packet_plan make_ready_resource_packet_plan(
    const quiz_vulkan::render::render_image_texture_batch_plan& batch_plan)
{
    using namespace quiz_vulkan::render;

    render_image_texture_frame_upload_handoff_summary handoff;
    handoff.frame_request_count = batch_plan.request_count;
    handoff.binding_packet_count = batch_plan.planned_request_count;
    handoff.current_binding_packet_count = batch_plan.planned_request_count;
    handoff.upload_packet_count = batch_plan.planned_request_count;
    handoff.requested_texture_count = batch_plan.planned_request_count;
    handoff.ready_texture_count = batch_plan.planned_request_count;
    handoff.renderer_handoff_ready = true;

    for (const render_image_texture_batch_plan_entry& plan_entry : batch_plan.entries) {
        if (!plan_entry.ok()) {
            continue;
        }
        const std::string sampler_key = render_image_sampler_policy_stable_fragment(plan_entry.sampler);
        render_image_texture_frame_upload_handoff_entry entry;
        entry.sequence = plan_entry.request_index + 1;
        entry.request_index = plan_entry.request_index;
        entry.status = render_image_texture_frame_upload_handoff_entry_status::ready;
        entry.status_name = render_image_texture_frame_upload_handoff_entry_status_name(entry.status);
        entry.render_image_uri = plan_entry.image.uri;
        entry.normalized_uri = plan_entry.pipeline_request.uri;
        entry.cache_key = plan_entry.normalized_source_key;
        entry.source_kind = plan_entry.source_kind;
        entry.sampler = plan_entry.sampler;
        entry.sampler_key = sampler_key;
        entry.texture_key = plan_entry.texture_key;
        entry.stable_texture_cache_key = plan_entry.stable_texture_cache_key;
        entry.texture_id = 700 + plan_entry.request_index;
        entry.texture_revision = 3;
        entry.texture_width = 64 + plan_entry.request_index;
        entry.texture_height = 32;
        entry.upload_request_id = 900 + plan_entry.request_index;
        entry.upload_generation_id = 11;
        entry.upload_result_status = render_image_texture_upload_result_packet_status::accepted;
        entry.upload_result_status_name = render_image_texture_upload_result_packet_status_name(
            entry.upload_result_status);
        entry.upload_status = render_image_texture_upload_status::uploaded;
        entry.upload_status_name = render_image_texture_upload_operation_upload_status_name(entry.upload_status);
        entry.mip_level_count = 1;
        entry.accepted_mip_level_count = 1;
        entry.uploaded_byte_count = 64;
        entry.requested = true;
        entry.upload_result_present = true;
        entry.ready = true;
        entry.blocked = false;
        entry.missing_upload_result = false;
        entry.renderer_handoff_ready = true;
        entry.expected_cache_reuse = plan_entry.expects_cache_reuse;
        entry.cache_key_summary = plan_entry.stable_texture_cache_key;
        entry.sampler_summary = sampler_key;
        entry.diagnostic = "test upload handoff entry is ready";
        handoff.entries.push_back(entry);
        handoff.uploaded_byte_count += entry.uploaded_byte_count;
        handoff.total_mip_level_count += entry.mip_level_count;
        handoff.accepted_mip_level_count += entry.accepted_mip_level_count;
    }

    return make_render_image_texture_frame_resource_packet_plan(handoff);
}

void test_resource_packet_plan_reports_ready_and_placeholder_entries()
{
    using namespace quiz_vulkan::render;

    render_image_texture_frame_upload_handoff_summary handoff;
    handoff.frame_request_count = 2;
    handoff.binding_packet_count = 2;
    handoff.current_binding_packet_count = 2;
    handoff.upload_packet_count = 2;
    handoff.requested_texture_count = 2;
    handoff.ready_texture_count = 1;
    handoff.placeholder_texture_count = 1;
    handoff.cache_reused_count = 1;
    handoff.uploaded_byte_count = 24;
    handoff.total_mip_level_count = 2;
    handoff.renderer_handoff_ready = true;
    handoff.entries.push_back(make_ready_handoff_entry(0, "asset://textures/card.ppm", false));
    handoff.entries.push_back(make_ready_handoff_entry(1, "asset://textures/missing.ppm", true));

    const render_image_texture_frame_resource_packet_plan plan =
        make_render_image_texture_frame_resource_packet_plan(handoff);

    require(plan.ok(), "resource packet plan accepts ready and placeholder-backed refs");
    require(plan.resource_binding_ready, "resource packet plan is resource-binding ready");
    require(
        plan.binding_status == render_image_texture_frame_binding_summary_status::placeholder_backed,
        "resource packet plan uses binding summary placeholder status");
    require(plan.binding_status_name == "placeholder_backed", "resource packet plan records binding status name");
    require(plan.frame_request_count == 2, "resource packet plan records frame request count");
    require(plan.handoff_entry_count == 2, "resource packet plan records handoff entry count");
    require(plan.requested_texture_count == 2, "resource packet plan records requested texture count");
    require(plan.bindable_packet_count == 2, "resource packet plan counts bindable packets");
    require(plan.resource_packet_count == 1, "resource packet plan counts real resource packets");
    require(plan.placeholder_backed_packet_count == 1, "resource packet plan counts placeholder-backed packets");
    require(plan.blocked_packet_count == 0, "resource packet plan records no blockers");
    require(plan.unique_texture_cache_key_count == 2, "resource packet plan deduplicates stable cache keys");
    require(plan.unique_sampler_key_count == 1, "resource packet plan deduplicates stable sampler keys");
    require(plan.cache_reused, "resource packet plan carries cache reuse summary without cache ownership");
    require(plan.placeholder_backed, "resource packet plan records placeholder-backed aggregate");
    require(
        plan.cache_key_summary.find("asset://textures/card.ppm") != std::string::npos,
        "resource packet plan summarizes stable cache keys");
    require(
        plan.sampler_summary.find("min=linear") != std::string::npos,
        "resource packet plan summarizes stable sampler keys");
    require(
        plan.diagnostic == "image frame resource packet plan is placeholder-backed",
        "resource packet plan placeholder diagnostic is stable");

    const render_image_texture_frame_resource_packet_plan_entry& ready = plan.entries[0];
    require(ready.ok(), "ready resource packet entry is ok");
    require(
        ready.status == render_image_texture_frame_resource_packet_status::resource_packet_ready,
        "ready resource packet entry status is ready");
    require(ready.status_name == "resource_packet_ready", "ready resource packet status name is stable");
    require(ready.resource_packet_ready, "ready entry records real resource packet readiness");
    require(ready.bindable, "ready entry is bindable");
    require(!ready.placeholder_backed, "ready entry is not placeholder-backed");
    require(ready.stable_texture_cache_key.find("asset://textures/card.ppm") != std::string::npos, "ready entry carries stable cache key");
    require(ready.sampler_summary.find("min=linear") != std::string::npos, "ready entry carries stable sampler key");

    const render_image_texture_frame_resource_packet_plan_entry& placeholder = plan.entries[1];
    require(placeholder.ok(), "placeholder resource packet entry remains bindable");
    require(
        placeholder.status == render_image_texture_frame_resource_packet_status::placeholder_backed,
        "placeholder resource packet entry status is placeholder-backed");
    require(placeholder.placeholder_backed, "placeholder entry records placeholder-backed state");
    require(!placeholder.resource_packet_ready, "placeholder entry is not a real resource packet");
    require(
        placeholder.diagnostic == "image frame resource packet stays placeholder-backed",
        "placeholder resource packet diagnostic is stable");
}

void test_resource_packet_plan_reports_missing_binding_upload_and_retry_blockers()
{
    using namespace quiz_vulkan::render;

    render_image_texture_frame_upload_handoff_entry missing_upload =
        make_blocked_handoff_entry(0, render_image_texture_frame_upload_handoff_entry_status::missing_upload_result);
    render_image_texture_frame_upload_handoff_entry missing_binding =
        make_blocked_handoff_entry(1, render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding);
    render_image_texture_frame_upload_handoff_entry retry =
        make_blocked_handoff_entry(2, render_image_texture_frame_upload_handoff_entry_status::blocked);
    retry.retryable_blocker = true;
    retry.placeholder_texture = true;
    retry.retry_eligibility_name = "eligible";
    retry.retry_after_queue_sequence_delta = 3;
    retry.next_retry_sequence = 11;
    retry.blocker_summary = "upload failed but can retry: invalid_image";

    render_image_texture_frame_upload_handoff_summary handoff;
    handoff.frame_request_count = 2;
    handoff.binding_packet_count = 2;
    handoff.current_binding_packet_count = 2;
    handoff.upload_packet_count = 2;
    handoff.requested_texture_count = 2;
    handoff.placeholder_texture_count = 1;
    handoff.blocked_texture_count = 2;
    handoff.missing_upload_result_count = 1;
    handoff.missing_frame_binding_count = 1;
    handoff.retry_blocker_count = 1;
    handoff.has_blockers = true;
    handoff.has_retry_blockers = true;
    handoff.entries.push_back(missing_upload);
    handoff.entries.push_back(missing_binding);
    handoff.entries.push_back(retry);

    const render_image_texture_frame_resource_packet_plan plan =
        make_render_image_texture_frame_resource_packet_plan(handoff);

    require(!plan.ok(), "blocked resource packet plan is not ok");
    require(!plan.resource_binding_ready, "blocked resource packet plan is not binding-ready");
    require(plan.has_blockers, "blocked resource packet plan records blockers");
    require(plan.has_retry_backoff, "blocked resource packet plan records retry backoff");
    require(
        plan.binding_status == render_image_texture_frame_binding_summary_status::missing_upload_result,
        "blocked resource packet plan carries binding summary priority status");
    require(plan.bindable_packet_count == 0, "blocked resource packet plan has no bindable packets");
    require(plan.blocked_packet_count == 3, "blocked resource packet plan counts every blocked entry");
    require(plan.missing_upload_result_packet_count == 1, "blocked plan counts missing upload result");
    require(plan.missing_frame_binding_packet_count == 1, "blocked plan counts missing frame binding");
    require(plan.retry_backoff_blocked_packet_count == 1, "blocked plan counts retry backoff");
    require(
        plan.retry_backoff_summary == "upload failed but can retry: invalid_image",
        "blocked plan preserves retry backoff summary");
    require(
        plan.diagnostic == "image frame resource packet plan has missing upload results",
        "blocked resource packet plan diagnostic is stable");

    require(
        plan.entries[0].status
            == render_image_texture_frame_resource_packet_status::blocked_missing_upload_result,
        "missing upload entry reports missing upload blocker");
    require(plan.entries[0].missing_upload_result, "missing upload entry records missing upload flag");
    require(plan.entries[0].blocked, "missing upload entry is blocked");
    require(!plan.entries[0].ok(), "missing upload entry is not ok");

    require(
        plan.entries[1].status
            == render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding,
        "missing binding entry reports missing frame binding blocker");
    require(plan.entries[1].missing_frame_binding, "missing binding entry records missing frame binding flag");
    require(plan.entries[1].blocked, "missing binding entry is treated as blocked for resource binding");

    require(
        plan.entries[2].status == render_image_texture_frame_resource_packet_status::blocked_retry_backoff,
        "retry entry reports retry backoff blocker");
    require(plan.entries[2].retry_backoff_blocked, "retry entry records retry backoff flag");
    require(plan.entries[2].retry_after_queue_sequence_delta == 3, "retry entry preserves retry delta");
    require(plan.entries[2].next_retry_sequence == 11, "retry entry preserves next retry sequence");
}

void test_draw_list_texture_frame_composition_links_ready_commands_to_resources()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.wrap_u = render_image_wrap_mode::repeat;

    const render_rect first_bounds{.x = 10.0f, .y = 20.0f, .width = 64.0f, .height = 32.0f};
    const render_rect first_content{.x = 12.0f, .y = 22.0f, .width = 60.0f, .height = 28.0f};
    const render_rect second_bounds{.x = 90.0f, .y = 20.0f, .width = 40.0f, .height = 40.0f};
    render_draw_list draw_list;
    draw_list.commands = {
        render_draw_command{
            .type = render_draw_command_type::quad,
            .node_id = "background",
            .bounds = render_rect{.x = 0.0f, .y = 0.0f, .width = 160.0f, .height = 90.0f},
            .content_bounds = render_rect{.x = 0.0f, .y = 0.0f, .width = 160.0f, .height = 90.0f},
        },
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "card-image",
            .parent_node_id = "card",
            .bounds = first_bounds,
            .content_bounds = first_content,
            .image = render_image_ref{
                .uri = "asset://textures/card-front.ppm",
                .alt_text = "front",
                .aspect_ratio = 2.0f,
                .sampler = nearest_sampler,
            },
        },
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "badge-image",
            .parent_node_id = "card",
            .bounds = second_bounds,
            .content_bounds = second_bounds,
            .image = render_image_ref{
                .uri = "asset://textures/badge.ppm",
                .alt_text = "badge",
                .aspect_ratio = 1.0f,
            },
        },
    };

    const render_image_draw_list_frame_handoff_snapshot handoff =
        make_render_image_draw_list_frame_handoff_snapshot(draw_list, "composition-frame");
    const render_image_texture_batch_plan batch_plan = plan_render_image_texture_batch(handoff);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(batch_plan, make_ready_batch_execution(batch_plan));
    const render_image_texture_frame_resource_packet_plan resources =
        make_ready_resource_packet_plan(batch_plan);

    const render_image_draw_list_texture_frame_composition composition =
        make_render_image_draw_list_texture_frame_composition(handoff, batch_plan, frame, resources);

    require(composition.ok(), "draw-list texture frame composition is ready");
    require(
        composition.status == render_image_draw_list_texture_frame_composition_status::ready,
        "composition records ready status");
    require(composition.status_name == "ready", "composition ready status name is stable");
    require(composition.frame_label == "composition-frame", "composition preserves frame label");
    require(composition.draw_command_count == 3, "composition records draw command count");
    require(composition.non_image_command_count == 1, "composition records skipped non-image commands");
    require(composition.image_command_count == 2, "composition records image command count");
    require(composition.handoff_entry_count == 2, "composition records image handoff entries");
    require(composition.texture_batch_request_count == 2, "composition records texture batch requests");
    require(composition.batch_planned_request_count == 2, "composition records planned batch requests");
    require(composition.frame_entry_count == 2, "composition records frame entries");
    require(composition.resource_packet_count == 2, "composition records resource packet entries");
    require(composition.ready_entry_count == 2, "composition records ready composed entries");
    require(composition.blocked_entry_count == 0, "composition has no blockers");
    require(
        composition.skipped_command_summary == "skipped non-image draw commands=1",
        "composition keeps non-image skip evidence");
    require(
        composition.diagnostic == "image draw-list texture frame composition ready",
        "composition ready diagnostic is stable");

    const render_image_draw_list_texture_frame_composition_entry& first = composition.entries[0];
    require(first.ok(), "first composed image entry is ready");
    require(first.draw_command_index == 1, "first composed entry preserves draw command index");
    require(first.image_command_index == 0, "first composed entry preserves image command order");
    require(first.texture_request_index == 0, "first composed entry preserves texture request index");
    require(first.node_id == "card-image", "first composed entry preserves node id");
    require(first.parent_node_id == "card", "first composed entry preserves parent node id");
    require(first.bounds.x == first_bounds.x && first.bounds.width == first_bounds.width, "first entry preserves bounds");
    require(
        first.content_bounds.x == first_content.x && first.content_bounds.width == first_content.width,
        "first entry preserves content bounds");
    require(first.uri == "asset://textures/card-front.ppm", "first entry preserves image uri");
    require(first.alt_text == "front", "first entry preserves alt text");
    require(first.aspect_ratio == 2.0f, "first entry preserves aspect ratio");
    require(first.sampler.min_filter == render_image_filter::nearest, "first entry preserves sampler filter");
    require(first.sampler.wrap_u == render_image_wrap_mode::repeat, "first entry preserves sampler wrap");
    require(first.texture_request.uri == "asset://textures/card-front.ppm", "first entry carries batch request uri");
    require(first.batch_entry_present, "first entry links batch plan evidence");
    require(first.batch_request_planned, "first entry records planned batch request");
    require(first.frame_entry_present, "first entry links frame snapshot evidence");
    require(first.frame_renderer_handoff_ready, "first entry records frame handoff readiness");
    require(first.resource_packet_present, "first entry links resource packet evidence");
    require(first.resource_packet_ready, "first entry records resource packet readiness");
    require(first.texture_id == 700, "first entry carries resource texture id");
    require(first.upload_request_id == 900, "first entry carries upload request id evidence");
    require(first.uploaded_byte_count == 64, "first entry carries uploaded byte count evidence");
    require(
        first.stable_draw_command_identity == "frame=composition-frame|node=card-image|parent=card",
        "first entry preserves stable draw command identity");
    require(
        first.stable_texture_cache_key.find("card-front.ppm") != std::string::npos,
        "first entry preserves stable texture cache key");

    const render_image_draw_list_texture_frame_composition_entry& second = composition.entries[1];
    require(second.ok(), "second composed image entry is ready");
    require(second.draw_command_index == 2, "second composed entry preserves draw command index");
    require(second.image_command_index == 1, "second composed entry preserves image command order");
    require(second.texture_request_index == 1, "second composed entry preserves texture request index");
    require(second.node_id == "badge-image", "second composed entry preserves node id");
    require(second.resource_packet_present, "second entry links resource packet evidence");
    require(second.texture_id == 701, "second entry carries second texture id");
}

void test_draw_list_texture_frame_composition_keeps_blocked_handoff_out_of_batch()
{
    using namespace quiz_vulkan::render;

    const render_rect bounds{.x = 0.0f, .y = 0.0f, .width = 16.0f, .height = 16.0f};
    render_draw_list draw_list;
    draw_list.commands = {
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "ready-image",
            .parent_node_id = "root",
            .bounds = bounds,
            .content_bounds = bounds,
            .image = render_image_ref{.uri = "asset://textures/ready.ppm"},
        },
        render_draw_command{
            .type = render_draw_command_type::text,
            .node_id = "caption",
            .parent_node_id = "root",
            .bounds = bounds,
            .content_bounds = bounds,
        },
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "blocked-image",
            .parent_node_id = "root",
            .bounds = bounds,
            .content_bounds = bounds,
            .image = render_image_ref{.uri = "   "},
        },
    };

    const render_image_draw_list_frame_handoff_snapshot handoff =
        make_render_image_draw_list_frame_handoff_snapshot(draw_list, "blocked-composition");
    const render_image_texture_batch_plan batch_plan = plan_render_image_texture_batch(handoff);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(batch_plan, make_ready_batch_execution(batch_plan));
    const render_image_texture_frame_resource_packet_plan resources =
        make_ready_resource_packet_plan(batch_plan);

    const render_image_draw_list_texture_frame_composition composition =
        make_render_image_draw_list_texture_frame_composition(handoff, batch_plan, frame, resources);

    require(!composition.ok(), "composition with blocked handoff entry is blocked");
    require(
        composition.status == render_image_draw_list_texture_frame_composition_status::blocked,
        "blocked composition records blocked status");
    require(composition.image_command_count == 2, "blocked composition sees two image commands");
    require(composition.non_image_command_count == 1, "blocked composition skips non-image command");
    require(composition.handoff_entry_count == 2, "blocked composition has image-only entries");
    require(composition.texture_batch_request_count == 1, "blocked handoff entry does not enter batch plan");
    require(composition.batch_planned_request_count == 1, "only ready handoff entry becomes planned batch request");
    require(composition.ready_entry_count == 1, "blocked composition keeps ready entry ready");
    require(composition.blocked_entry_count == 1, "blocked composition counts blocked handoff entry");
    require(composition.handoff_blocked_count == 1, "blocked composition records handoff blocker");
    require(composition.missing_batch_request_count == 0, "blocked handoff is not reported as missing batch request");
    require(
        composition.diagnostic == "image draw-list texture frame composition has blocked image commands",
        "blocked composition diagnostic is stable");

    const render_image_draw_list_texture_frame_composition_entry& ready = composition.entries[0];
    require(ready.ok(), "ready entry still reaches texture frame resources");
    require(ready.batch_entry_present, "ready entry links batch plan");
    require(ready.frame_entry_present, "ready entry links frame snapshot");
    require(ready.resource_packet_present, "ready entry links resource packet");

    const render_image_draw_list_texture_frame_composition_entry& blocked = composition.entries[1];
    require(
        blocked.status == render_image_draw_list_texture_frame_composition_entry_status::handoff_blocked,
        "blocked handoff entry keeps handoff blocked status");
    require(blocked.handoff_blocked, "blocked entry records handoff blocker");
    require(!blocked.batch_entry_present, "blocked handoff entry does not link a batch entry");
    require(!blocked.entered_texture_batch, "blocked handoff entry does not enter texture batch");
    require(!blocked.frame_entry_present, "blocked handoff entry does not link frame evidence");
    require(!blocked.resource_packet_present, "blocked handoff entry does not link resource packet evidence");
    require(blocked.draw_command_index == 2, "blocked entry preserves draw command index");
    require(blocked.node_id == "blocked-image", "blocked entry preserves node id");
    require(blocked.handoff_blocker_summary == "image draw command uri is empty", "blocked entry keeps blocker summary");
    require(
        blocked.diagnostic
            == "image draw command texture frame composition blocked by draw-list handoff: "
               "image draw command uri is empty",
        "blocked entry diagnostic is stable");
}

void test_resource_packet_status_helpers_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        render_image_texture_frame_resource_packet_status_name(
            render_image_texture_frame_resource_packet_status::resource_packet_ready)
            == "resource_packet_ready",
        "resource packet ready status name is stable");
    require(
        render_image_texture_frame_resource_packet_status_is_bindable(
            render_image_texture_frame_resource_packet_status::placeholder_backed),
        "placeholder-backed status is bindable");
    require(
        render_image_texture_frame_resource_packet_status_is_blocked(
            render_image_texture_frame_resource_packet_status::blocked_retry_backoff),
        "retry backoff status is blocked");
}

} // namespace

int main()
{
    test_resource_packet_plan_reports_ready_and_placeholder_entries();
    test_resource_packet_plan_reports_missing_binding_upload_and_retry_blockers();
    test_draw_list_texture_frame_composition_links_ready_commands_to_resources();
    test_draw_list_texture_frame_composition_keeps_blocked_handoff_out_of_batch();
    test_resource_packet_status_helpers_are_stable();
    return 0;
}
