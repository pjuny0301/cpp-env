#include "render/image/image_texture_frame_resource_packet_materialization.h"

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

static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_cache_handoff_record>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_upload_handoff_record>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_sampler_handoff_record>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_materialization>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_cache_handoff_record>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_upload_handoff_record>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_sampler_handoff_record>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_materialization>);

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::render_image_texture_frame_resource_packet_plan_entry make_packet(
    std::size_t request_index,
    std::string source_key,
    quiz_vulkan::render::render_image_texture_frame_resource_packet_status status)
{
    using namespace quiz_vulkan::render;

    const render_image_sampler_policy sampler;
    const std::string sampler_key = render_image_sampler_policy_stable_fragment(sampler);
    const bool bindable = render_image_texture_frame_resource_packet_status_is_bindable(status);
    const bool placeholder = status == render_image_texture_frame_resource_packet_status::placeholder_backed;
    const bool blocked = render_image_texture_frame_resource_packet_status_is_blocked(status);

    render_image_texture_frame_resource_packet_plan_entry packet{
        .sequence = request_index + 1,
        .request_index = request_index,
        .status = status,
        .status_name = render_image_texture_frame_resource_packet_status_name(status),
        .render_image_uri = source_key,
        .normalized_uri = source_key,
        .cache_key = source_key,
        .source_kind = render_image_source_kind::asset_uri,
        .sampler = sampler,
        .sampler_key = sampler_key,
        .texture_key = render_image_texture_key{.source_key = source_key, .sampler = sampler},
        .stable_texture_cache_key = source_key + "|" + sampler_key,
        .texture_id = bindable ? 100 + request_index : 0,
        .texture_revision = bindable ? 7u : 0u,
        .texture_width = placeholder ? 2u : (bindable ? 4u : 0u),
        .texture_height = placeholder ? 2u : (bindable ? 1u : 0u),
        .upload_request_id = bindable ? 200 + request_index : 0,
        .upload_generation_id = bindable ? 5u : 0u,
        .mip_level_count = bindable ? 1u : 0u,
        .uploaded_byte_count = placeholder ? 16u : (bindable ? 8u : 0u),
        .requested = status != render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding,
        .bindable = bindable,
        .resource_packet_ready = status == render_image_texture_frame_resource_packet_status::resource_packet_ready,
        .placeholder_backed = placeholder,
        .blocked = blocked,
        .missing_upload_result =
            status == render_image_texture_frame_resource_packet_status::blocked_missing_upload_result,
        .missing_frame_binding =
            status == render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding,
        .retry_backoff_blocked =
            status == render_image_texture_frame_resource_packet_status::blocked_retry_backoff,
        .cache_reused = request_index == 1,
        .expected_cache_reuse = request_index == 1,
        .cache_key_summary = source_key,
        .sampler_summary = sampler_key,
        .blocker_summary = blocked ? "resource packet is blocked" : "",
        .diagnostic = bindable ? "resource packet can materialize" : "resource packet is blocked",
    };

    if (packet.retry_backoff_blocked) {
        packet.retry_backoff_summary = "eligible";
        packet.retry_after_queue_sequence_delta = 3;
        packet.next_retry_sequence = 11;
        packet.blocker_summary = "upload failed but can retry: invalid_image";
    }

    return packet;
}

void test_materialization_outputs_deterministic_handoff_records()
{
    using namespace quiz_vulkan::render;

    render_image_texture_frame_resource_packet_plan plan;
    plan.frame_request_count = 3;
    plan.resource_binding_ready = true;
    plan.unique_texture_cache_key_count = 3;
    plan.unique_sampler_key_count = 1;
    plan.uploaded_byte_count = 32;
    plan.entries.push_back(make_packet(
        2,
        "asset://textures/placeholder.ppm",
        render_image_texture_frame_resource_packet_status::placeholder_backed));
    plan.entries.push_back(make_packet(
        0,
        "asset://textures/card.ppm",
        render_image_texture_frame_resource_packet_status::resource_packet_ready));
    plan.entries.push_back(make_packet(
        1,
        "asset://textures/reused.ppm",
        render_image_texture_frame_resource_packet_status::resource_packet_ready));

    const render_image_texture_frame_resource_packet_materialization materialization =
        materialize_render_image_texture_frame_resource_packets(plan);

    require(materialization.ok(), "materialization is ok for ready and placeholder packets");
    require(materialization.renderer_boundary_ready, "materialization is renderer-boundary ready");
    require(materialization.frame_request_count == 3, "materialization records frame request count");
    require(materialization.planned_packet_count == 3, "materialization records planned packet count");
    require(materialization.materialized_packet_count == 3, "materialization counts materialized packets");
    require(materialization.placeholder_packet_count == 1, "materialization counts placeholder packets");
    require(materialization.blocked_packet_count == 0, "materialization records no blockers");
    require(materialization.cache_record_count == 3, "materialization creates cache records");
    require(materialization.upload_record_count == 3, "materialization creates upload records");
    require(materialization.sampler_record_count == 3, "materialization creates sampler records");
    require(materialization.has_placeholders, "materialization records placeholder aggregate");
    require(!materialization.has_blockers, "materialization records no blocker aggregate");
    require(
        materialization.cache_handoff_summary == "cache_records=3; stable_cache_keys=3",
        "cache handoff summary is stable");
    require(
        materialization.upload_handoff_summary == "upload_records=3; uploaded_bytes=32",
        "upload handoff summary is stable");
    require(
        materialization.sampler_handoff_summary == "sampler_records=3; sampler_keys=1",
        "sampler handoff summary is stable");
    require(
        materialization.diagnostic == "image frame resource packet materialization is placeholder-backed",
        "placeholder materialization diagnostic is stable");

    require(materialization.entries[0].request_index == 0, "materialization sorts first request deterministically");
    require(materialization.entries[1].request_index == 1, "materialization sorts second request deterministically");
    require(materialization.entries[2].request_index == 2, "materialization sorts third request deterministically");

    const render_image_texture_frame_resource_packet_materialization_entry& ready = materialization.entries[0];
    require(ready.ok(), "ready materialization entry is ok");
    require(
        ready.status == render_image_texture_frame_resource_packet_materialization_status::materialized,
        "ready materialization entry status is materialized");
    require(ready.cache_record_present, "ready entry has cache handoff record");
    require(ready.upload_record_present, "ready entry has upload handoff record");
    require(ready.sampler_record_present, "ready entry has sampler handoff record");
    require(ready.cache_record.texture_id == 100, "ready cache handoff preserves texture id");
    require(ready.cache_record.stable_texture_cache_key.find("asset://textures/card.ppm") != std::string::npos, "ready cache handoff preserves stable key");
    require(ready.upload_record.upload_request_id == 200, "ready upload handoff preserves upload request id");
    require(ready.sampler_record.sampler_summary.find("min=linear") != std::string::npos, "ready sampler handoff preserves sampler key");

    const render_image_texture_frame_resource_packet_materialization_entry& placeholder = materialization.entries[2];
    require(placeholder.ok(), "placeholder materialization entry is ok");
    require(
        placeholder.status
            == render_image_texture_frame_resource_packet_materialization_status::materialized_placeholder,
        "placeholder materialization entry status is stable");
    require(placeholder.placeholder_backed, "placeholder entry records placeholder-backed state");
    require(placeholder.cache_record.placeholder_backed, "placeholder cache record carries placeholder flag");
    require(placeholder.upload_record.placeholder_backed, "placeholder upload record carries placeholder flag");
    require(placeholder.sampler_record.placeholder_backed, "placeholder sampler record carries placeholder flag");
}

void test_materialization_keeps_blockers_visible_without_handoff_records()
{
    using namespace quiz_vulkan::render;

    render_image_texture_frame_resource_packet_plan plan;
    plan.frame_request_count = 4;
    plan.resource_binding_ready = false;
    plan.has_blockers = true;
    plan.unique_texture_cache_key_count = 1;
    plan.unique_sampler_key_count = 1;
    plan.uploaded_byte_count = 8;
    plan.entries.push_back(make_packet(
        0,
        "asset://textures/ready.ppm",
        render_image_texture_frame_resource_packet_status::resource_packet_ready));
    plan.entries.push_back(make_packet(
        1,
        "asset://textures/missing-upload.ppm",
        render_image_texture_frame_resource_packet_status::blocked_missing_upload_result));
    plan.entries.push_back(make_packet(
        2,
        "asset://textures/missing-binding.ppm",
        render_image_texture_frame_resource_packet_status::blocked_missing_frame_binding));
    plan.entries.push_back(make_packet(
        3,
        "asset://textures/retry.ppm",
        render_image_texture_frame_resource_packet_status::blocked_retry_backoff));

    const render_image_texture_frame_resource_packet_materialization materialization =
        materialize_render_image_texture_frame_resource_packets(plan);

    require(!materialization.ok(), "blocked materialization is not ok");
    require(!materialization.renderer_boundary_ready, "blocked materialization is not renderer-boundary ready");
    require(materialization.materialized_packet_count == 1, "blocked materialization preserves ready packet");
    require(materialization.blocked_packet_count == 3, "blocked materialization counts blocked packets");
    require(materialization.missing_upload_result_count == 1, "blocked materialization counts missing upload");
    require(materialization.missing_frame_binding_count == 1, "blocked materialization counts missing binding");
    require(materialization.retry_backoff_blocked_count == 1, "blocked materialization counts retry backoff");
    require(materialization.cache_record_count == 1, "blocked materialization emits only ready cache record");
    require(materialization.upload_record_count == 1, "blocked materialization emits only ready upload record");
    require(materialization.sampler_record_count == 1, "blocked materialization emits only ready sampler record");
    require(
        materialization.blocker_summary == "upload failed but can retry: invalid_image",
        "blocked materialization preserves retry blocker summary");
    require(
        materialization.diagnostic
            == "image frame resource packet materialization has missing upload results",
        "blocked materialization diagnostic prioritizes missing upload");

    const render_image_texture_frame_resource_packet_materialization_entry& missing_upload =
        materialization.entries[1];
    require(
        missing_upload.status
            == render_image_texture_frame_resource_packet_materialization_status::blocked_missing_upload_result,
        "missing upload entry materialization status is stable");
    require(missing_upload.blocked, "missing upload entry is blocked");
    require(!missing_upload.cache_record_present, "missing upload entry has no cache record");
    require(!missing_upload.upload_record_present, "missing upload entry has no upload record");
    require(!missing_upload.sampler_record_present, "missing upload entry has no sampler record");

    const render_image_texture_frame_resource_packet_materialization_entry& retry = materialization.entries[3];
    require(
        retry.status == render_image_texture_frame_resource_packet_materialization_status::blocked_retry_backoff,
        "retry entry materialization status is stable");
    require(retry.retry_backoff_blocked, "retry entry records retry backoff flag");
    require(
        retry.diagnostic == "image frame resource packet materialization blocked by retry backoff",
        "retry materialization diagnostic is stable");
}

void test_materialization_status_helpers_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        render_image_texture_frame_resource_packet_materialization_status_name(
            render_image_texture_frame_resource_packet_materialization_status::materialized)
            == "materialized",
        "materialized status name is stable");
    require(
        render_image_texture_frame_resource_packet_materialization_status_is_materialized(
            render_image_texture_frame_resource_packet_materialization_status::materialized_placeholder),
        "materialized placeholder status is materialized");
    require(
        render_image_texture_frame_resource_packet_materialization_status_is_blocked(
            render_image_texture_frame_resource_packet_materialization_status::blocked_retry_backoff),
        "retry backoff materialization status is blocked");
    require(
        render_image_texture_frame_resource_packet_materialization_status_for(
            render_image_texture_frame_resource_packet_status::placeholder_backed)
            == render_image_texture_frame_resource_packet_materialization_status::materialized_placeholder,
        "packet placeholder status maps to placeholder materialization");
}

} // namespace

int main()
{
    test_materialization_outputs_deterministic_handoff_records();
    test_materialization_keeps_blockers_visible_without_handoff_records();
    test_materialization_status_helpers_are_stable();
    return 0;
}
