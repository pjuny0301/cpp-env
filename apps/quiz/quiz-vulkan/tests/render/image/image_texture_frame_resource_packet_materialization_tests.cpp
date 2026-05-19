#include "render/image/image_texture_frame_resource_packet_materialization.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

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
static_assert(!HasFakeCacheSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_packet_consumption_summary>);
static_assert(!HasFakeCacheSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_packet_consumption_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_cache_handoff_record>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_upload_handoff_record>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_sampler_handoff_record>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_materialization>);
static_assert(!HasFakeUploadSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_packet_consumption_summary>);
static_assert(!HasFakeUploadSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_packet_consumption_diff>);

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
    if (bindable) {
        render_decoded_image decoded{
            .width = packet.texture_width,
            .height = packet.texture_height,
            .pixel_format = render_image_pixel_format::rgba8_srgb,
            .pixels = {},
        };
        decoded.pixels.resize(
            expected_render_decoded_image_byte_count(decoded),
            placeholder ? std::byte{0x7f} : std::byte{0xff});
        packet.decoded_payload = make_render_image_decoded_payload_evidence(decoded);
        packet.payload_layout =
            make_render_image_texture_upload_payload_layout_evidence(packet.texture_key, packet.sampler, decoded);
        packet.staging_payload_plan = make_render_image_texture_staging_payload_plan(
            packet.payload_layout,
            make_render_image_texture_mipmap_upload_plan(decoded, packet.sampler));
    }

    return packet;
}

quiz_vulkan::render::render_image_texture_frame_resource_packet_materialization materialize_packets(
    std::vector<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan_entry> entries)
{
    using namespace quiz_vulkan::render;

    render_image_texture_frame_resource_packet_plan plan;
    plan.frame_request_count = entries.size();
    plan.handoff_entry_count = entries.size();

    std::map<std::string, bool> unique_cache_keys;
    std::map<std::string, bool> unique_sampler_keys;
    for (const render_image_texture_frame_resource_packet_plan_entry& entry : entries) {
        count_render_image_texture_frame_resource_packet_plan_entry(plan, entry);
        if (!entry.removed) {
            unique_cache_keys.emplace(entry.stable_texture_cache_key, true);
            unique_sampler_keys.emplace(entry.sampler_summary, true);
        }
        if (entry.bindable) {
            plan.uploaded_byte_count += entry.uploaded_byte_count;
            plan.mip_level_count += entry.mip_level_count;
        }
    }

    plan.resource_binding_ready = !plan.has_blockers;
    plan.renderer_handoff_ready = plan.resource_binding_ready;
    plan.unique_texture_cache_key_count = unique_cache_keys.size();
    plan.unique_sampler_key_count = unique_sampler_keys.size();
    plan.entries = std::move(entries);
    return materialize_render_image_texture_frame_resource_packets(plan);
}

void retarget_packet_sampler(
    quiz_vulkan::render::render_image_texture_frame_resource_packet_plan_entry& packet,
    quiz_vulkan::render::render_image_sampler_policy sampler)
{
    packet.sampler = sampler;
    packet.sampler_key = quiz_vulkan::render::render_image_sampler_policy_stable_fragment(packet.sampler);
    packet.sampler_summary = packet.sampler_key;
    packet.texture_key = quiz_vulkan::render::render_image_texture_key{
        .source_key = packet.render_image_uri,
        .sampler = packet.sampler,
    };
    packet.stable_texture_cache_key = packet.render_image_uri + "|" + packet.sampler_key;
}

const quiz_vulkan::render::render_image_texture_frame_resource_packet_consumption_entry_diff*
find_consumption_diff_entry(
    const quiz_vulkan::render::render_image_texture_frame_resource_packet_consumption_diff& diff,
    std::size_t request_index)
{
    for (const quiz_vulkan::render::render_image_texture_frame_resource_packet_consumption_entry_diff& entry :
         diff.entries) {
        if (entry.request_index == request_index) {
            return &entry;
        }
    }
    return nullptr;
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

void test_consumption_summary_reports_compact_packet_identities()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_frame_resource_packet_materialization materialization = materialize_packets({
        make_packet(
            0,
            "asset://textures/card.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(
            1,
            "asset://textures/placeholder.ppm",
            render_image_texture_frame_resource_packet_status::placeholder_backed),
    });

    const render_image_texture_frame_resource_packet_consumption_summary summary =
        make_render_image_texture_frame_resource_packet_consumption_summary(materialization);

    require(summary.ok(), "consumption summary is ok for ready materialized packets");
    require(summary.packet_count == 2, "consumption summary records packet count");
    require(summary.materialized_packet_count == 2, "consumption summary records materialized count");
    require(summary.placeholder_packet_count == 1, "consumption summary records placeholder count");
    require(summary.stable_packet_identity_count == 2, "consumption summary records stable identities");
    require(
        summary.duplicate_stable_packet_identity_count == 0,
        "consumption summary reports no duplicate identities");
    require(
        summary.missing_stable_packet_identity_count == 0,
        "consumption summary reports no missing identities");
    require(
        summary.identity_summary == "packets=2; identities=2; duplicate_identities=0; missing_identities=0",
        "consumption summary identity summary is stable");
    require(summary.entries[0].request_index == 0, "consumption summary keeps first request index");
    require(
        summary.entries[0].stable_packet_identity.find("texture=100:7") != std::string::npos,
        "consumption identity includes texture id and revision");
    require(summary.entries[0].texture_width == 4, "consumption entry preserves texture width");
    require(summary.entries[0].texture_height == 1, "consumption entry preserves texture height");
    require(summary.entries[0].upload_request_id == 200, "consumption entry preserves upload request");
    require(summary.entries[0].upload_generation_id == 5, "consumption entry preserves upload generation");
    require(summary.entries[0].uploaded_byte_count == 8, "consumption entry preserves uploaded bytes");
    require(summary.entries[0].decoded_payload_valid, "consumption entry preserves decoded payload validity");
    require(summary.entries[0].decoded_byte_count == 16, "consumption entry preserves decoded byte count");
    require(summary.entries[0].decoded_payload_hash != 0, "consumption entry preserves decoded payload hash");
    require(summary.entries[0].upload_payload_layout_ready, "consumption entry preserves upload layout readiness");
    require(summary.entries[0].upload_layout_row_stride_byte_count == 16, "consumption entry records row stride");
    require(summary.entries[0].staging_payload_ready, "consumption entry preserves staging readiness");
    require(summary.entries[0].staging_payload_byte_count == 16, "consumption entry records staging bytes");
    require(summary.entries[0].staging_row_copy_count == 1, "consumption entry records staging row copies");
    require(summary.entries[0].decoded_resource_ready, "consumption entry records decoded resource readiness");
    require(
        summary.decoded_resource_summary == "decoded_resources=2; payload_hashes=2; decoded_bytes=32; staging_bytes=32",
        "consumption summary records decoded resource byte facts");
    require(
        summary.decoded_resource_blocker_summary == "no decoded resource blockers",
        "consumption summary records no decoded resource blockers");
    require(summary.entries[1].placeholder_backed, "consumption entry preserves placeholder flag");
}

void test_consumption_summary_reports_duplicate_and_missing_stable_identities()
{
    using namespace quiz_vulkan::render;

    render_image_texture_frame_resource_packet_plan_entry duplicate =
        make_packet(
            1,
            "asset://textures/card.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready);
    duplicate.texture_id = 100;
    duplicate.texture_revision = 7;
    duplicate.upload_request_id = 200;
    duplicate.upload_generation_id = 5;
    duplicate.uploaded_byte_count = 8;

    const render_image_texture_frame_resource_packet_materialization materialization = materialize_packets({
        make_packet(
            0,
            "asset://textures/card.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready),
        duplicate,
        make_packet(
            2,
            "asset://textures/missing.ppm",
            render_image_texture_frame_resource_packet_status::blocked_missing_upload_result),
    });

    const render_image_texture_frame_resource_packet_consumption_summary summary =
        make_render_image_texture_frame_resource_packet_consumption_summary(materialization);

    require(!summary.ok(), "consumption summary is blocked when materialization has blockers");
    require(summary.stable_packet_identity_count == 1, "consumption summary deduplicates stable identities");
    require(
        summary.duplicate_stable_packet_identity_count == 2,
        "consumption summary counts duplicate identity entries");
    require(
        summary.missing_stable_packet_identity_count == 1,
        "consumption summary counts missing identity entries");
    require(summary.entries[0].duplicate_stable_packet_identity, "first duplicate entry is marked");
    require(summary.entries[1].duplicate_stable_packet_identity, "second duplicate entry is marked");
    require(!summary.entries[2].stable_packet_identity_present, "blocked entry records missing identity");
    require(
        summary.diagnostic == "image frame resource packet consumption summary has missing packet identities",
        "missing identity diagnostic is stable");
}

void test_consumption_summary_reports_decoded_resource_blockers()
{
    using namespace quiz_vulkan::render;

    render_image_texture_frame_resource_packet_plan_entry invalid_layout =
        make_packet(
            0,
            "asset://textures/layout-mismatch.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready);
    render_decoded_image decoded{
        .width = invalid_layout.texture_width,
        .height = invalid_layout.texture_height,
        .pixel_format = render_image_pixel_format::rgba8_srgb,
        .pixels = {},
    };
    decoded.pixels.resize(expected_render_decoded_image_byte_count(decoded), std::byte{0xff});
    render_image_sampler_policy mismatched_sampler = invalid_layout.sampler;
    mismatched_sampler.wrap_u = render_image_wrap_mode::repeat;
    invalid_layout.payload_layout =
        make_render_image_texture_upload_payload_layout_evidence(
            invalid_layout.texture_key,
            mismatched_sampler,
            decoded);
    invalid_layout.staging_payload_plan = make_render_image_texture_staging_payload_plan(
        invalid_layout.payload_layout,
        make_render_image_texture_mipmap_upload_plan(decoded, mismatched_sampler));

    render_image_texture_frame_resource_packet_plan_entry invalid_staging =
        make_packet(
            1,
            "asset://textures/staging-mismatch.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready);
    invalid_staging.staging_payload_plan = make_render_image_texture_staging_payload_plan(
        invalid_staging.payload_layout,
        invalid_staging.staging_payload_plan.mipmap_upload_plan,
        0);

    const render_image_texture_frame_resource_packet_materialization materialization =
        materialize_packets({invalid_layout, invalid_staging});
    const render_image_texture_frame_resource_packet_consumption_summary summary =
        make_render_image_texture_frame_resource_packet_consumption_summary(materialization);

    require(!summary.ok(), "decoded resource blockers make consumption summary not ok");
    require(summary.renderer_boundary_ready, "base materialization can be renderer-boundary ready before decoded checks");
    require(summary.has_decoded_resource_blockers, "consumption summary records decoded resource blockers");
    require(summary.decoded_resource_ready_count == 0, "decoded blocker summary records no ready decoded resources");
    require(summary.decoded_resource_blocked_count == 2, "decoded blocker summary counts blocked resources");
    require(summary.entries[0].decoded_resource_blocked, "invalid layout entry is decoded-resource blocked");
    require(
        summary.entries[0].decoded_resource_blocker_summary
            == "image texture upload payload layout sampler does not match texture key",
        "invalid layout blocker is preserved");
    require(summary.entries[1].decoded_resource_blocked, "invalid staging entry is decoded-resource blocked");
    require(
        summary.entries[1].decoded_resource_blocker_summary == "staging row alignment must be non-zero",
        "invalid staging blocker is preserved");
    require(
        summary.decoded_resource_blocker_summary.find("sampler does not match") != std::string::npos,
        "decoded blocker aggregate includes layout mismatch");
    require(
        summary.decoded_resource_blocker_summary.find("alignment must be non-zero") != std::string::npos,
        "decoded blocker aggregate includes staging mismatch");
    require(
        summary.diagnostic == "image frame resource packet consumption summary has decoded resource blockers",
        "decoded blocker diagnostic is stable");
}

void test_consumption_diff_reports_stable_no_change_frame()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_frame_resource_packet_materialization materialization = materialize_packets({
        make_packet(
            0,
            "asset://textures/card.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(
            1,
            "asset://textures/placeholder.ppm",
            render_image_texture_frame_resource_packet_status::placeholder_backed),
    });

    const render_image_texture_frame_resource_packet_consumption_summary summary =
        make_render_image_texture_frame_resource_packet_consumption_summary(materialization);
    const render_image_texture_frame_resource_packet_consumption_diff diff =
        diff_render_image_texture_frame_resource_packet_consumption_summaries(summary, summary);

    require(diff.ok(), "unchanged consumption diff is ok");
    require(diff.stable_no_change_frame, "unchanged consumption diff marks stable no-change frame");
    require(!diff.has_changes, "unchanged consumption diff has no changes");
    require(diff.unchanged_packet_count == 2, "unchanged consumption diff counts unchanged packets");
    require(diff.changed_summary == "no resource packet consumption changes", "unchanged summary is stable");
    require(
        diff.diagnostic == "image frame resource packet consumption diff is unchanged",
        "unchanged consumption diff diagnostic is stable");
}

void test_consumption_diff_reports_identity_changes_and_readiness_transitions()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    render_image_texture_frame_resource_packet_plan_entry changed =
        make_packet(
            0,
            "asset://textures/card-hd.ppm",
            render_image_texture_frame_resource_packet_status::placeholder_backed);
    retarget_packet_sampler(changed, nearest_sampler);
    changed.texture_id = 400;
    changed.texture_revision = 9;
    changed.texture_width = 8;
    changed.texture_height = 4;
    changed.upload_request_id = 500;
    changed.upload_generation_id = 15;
    changed.uploaded_byte_count = 128;

    const render_image_texture_frame_resource_packet_materialization before = materialize_packets({
        make_packet(
            0,
            "asset://textures/card.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(
            1,
            "asset://textures/regress.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(
            2,
            "asset://textures/recover.ppm",
            render_image_texture_frame_resource_packet_status::blocked_missing_upload_result),
        make_packet(
            4,
            "asset://textures/removed.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready),
    });
    const render_image_texture_frame_resource_packet_materialization after = materialize_packets({
        changed,
        make_packet(
            1,
            "asset://textures/regress.ppm",
            render_image_texture_frame_resource_packet_status::blocked_missing_upload_result),
        make_packet(
            2,
            "asset://textures/recover.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(
            3,
            "asset://textures/added.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready),
    });

    const render_image_texture_frame_resource_packet_consumption_diff diff =
        diff_render_image_texture_frame_resource_packet_consumption(before, after);

    require(!diff.ok(), "consumption diff reports ready-to-blocked regression");
    require(diff.has_changes, "consumption diff reports changes");
    require(diff.has_regression, "consumption diff exposes regression flag");
    require(diff.has_improvement, "consumption diff exposes improvement flag");
    require(diff.added_packet_count == 1, "consumption diff counts added packet");
    require(diff.removed_packet_count == 1, "consumption diff counts removed packet");
    require(diff.changed_packet_count == 3, "consumption diff counts changed packets");
    require(diff.stable_packet_identity_changed_count == 3, "consumption diff counts identity changes");
    require(
        diff.stable_texture_cache_key_changed_count == 3,
        "consumption diff counts stable cache key changes");
    require(diff.texture_handle_changed_count == 3, "consumption diff counts texture handle changes");
    require(diff.texture_extent_changed_count == 3, "consumption diff counts texture extent changes");
    require(diff.sampler_key_changed_count == 3, "consumption diff counts sampler key changes");
    require(diff.upload_request_changed_count == 3, "consumption diff counts upload request changes");
    require(diff.upload_generation_changed_count == 3, "consumption diff counts upload generation changes");
    require(diff.uploaded_byte_count_changed_count == 3, "consumption diff counts uploaded byte changes");
    require(diff.placeholder_changed_count == 1, "consumption diff counts placeholder flag change");
    require(diff.readiness_changed_count == 2, "consumption diff counts readiness transitions");
    require(diff.blocker_changed_count == 2, "consumption diff counts blocker transitions");
    require(diff.ready_regression_count == 1, "consumption diff counts ready-to-blocked regression");
    require(diff.ready_recovery_count == 1, "consumption diff counts blocked-to-ready improvement");
    require(
        diff.entries[0].after_texture_width == 8 && diff.entries[0].after_texture_height == 4,
        "consumption diff preserves changed texture extent");
    require(diff.entries[0].after_uploaded_byte_count == 128, "consumption diff preserves uploaded byte evidence");
    require(
        diff.regression_summary == "request=1:ready->blocked",
        "consumption diff regression summary is compact");
    require(
        diff.improvement_summary == "request=2:blocked->ready",
        "consumption diff improvement summary is compact");
    require(
        diff.diagnostic == "image frame resource packet consumption diff has regressions",
        "consumption diff diagnostic is stable");

    const render_image_texture_frame_resource_packet_consumption_entry_diff* added =
        find_consumption_diff_entry(diff, 3);
    require(added != nullptr, "consumption diff exposes added packet entry");
    require(
        added->status == render_image_texture_frame_resource_packet_consumption_diff_entry_status::added,
        "consumption diff classifies added materialized packet");
    require(!added->before_present, "added consumption diff has no before packet");
    require(added->after_present, "added consumption diff has after packet");
    require(added->after_ready, "added consumption diff records ready after packet");
    require(added->after_stable_packet_identity_present, "added consumption diff records after identity");
    require(
        added->after_stable_packet_identity.find("asset://textures/added.ppm") != std::string::npos,
        "added consumption diff preserves after packet identity");
    require(added->after_texture_id == 103, "added consumption diff preserves after texture id");
    require(added->after_upload_request_id == 203, "added consumption diff preserves after upload request");

    const render_image_texture_frame_resource_packet_consumption_entry_diff* removed =
        find_consumption_diff_entry(diff, 4);
    require(removed != nullptr, "consumption diff exposes removed packet entry");
    require(
        removed->status == render_image_texture_frame_resource_packet_consumption_diff_entry_status::removed,
        "consumption diff classifies removed materialized packet");
    require(removed->before_present, "removed consumption diff has before packet");
    require(!removed->after_present, "removed consumption diff has no after packet");
    require(removed->before_ready, "removed consumption diff records ready before packet");
    require(removed->before_stable_packet_identity_present, "removed consumption diff records before identity");
    require(
        removed->before_stable_packet_identity.find("asset://textures/removed.ppm") != std::string::npos,
        "removed consumption diff preserves before packet identity");
    require(removed->before_texture_id == 104, "removed consumption diff preserves before texture id");
    require(removed->before_upload_request_id == 204, "removed consumption diff preserves before upload request");
}

} // namespace

int main()
{
    test_materialization_outputs_deterministic_handoff_records();
    test_materialization_keeps_blockers_visible_without_handoff_records();
    test_materialization_status_helpers_are_stable();
    test_consumption_summary_reports_compact_packet_identities();
    test_consumption_summary_reports_duplicate_and_missing_stable_identities();
    test_consumption_summary_reports_decoded_resource_blockers();
    test_consumption_diff_reports_stable_no_change_frame();
    test_consumption_diff_reports_identity_changes_and_readiness_transitions();
    return 0;
}
