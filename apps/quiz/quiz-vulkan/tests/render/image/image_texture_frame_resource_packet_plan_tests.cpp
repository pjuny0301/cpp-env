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
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan>);

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
    test_resource_packet_status_helpers_are_stable();
    return 0;
}
