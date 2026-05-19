#include "render/image/image_texture_frame_resource_packet_plan.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
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

static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan_entry>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_draw_list_texture_frame_composition_entry>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_draw_list_texture_frame_composition>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_packet>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_packet_summary>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_packet_diff_entry>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_packet_summary_diff>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_draw_payload>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_draw_payload_frame>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_resource_packet_plan>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_draw_list_texture_frame_composition_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_draw_list_texture_frame_composition>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_packet>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_packet_summary>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_packet_diff_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_packet_summary_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_draw_payload>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_renderer_texture_quad_draw_payload_frame>);

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

void attach_decoded_upload_evidence(
    quiz_vulkan::render::render_image_texture_frame_upload_handoff_entry& entry,
    std::size_t width,
    std::size_t height,
    std::byte fill)
{
    using namespace quiz_vulkan::render;

    render_decoded_image decoded{
        .width = width,
        .height = height,
        .pixel_format = render_image_pixel_format::rgba8_srgb,
        .pixels = {},
    };
    decoded.pixels.resize(expected_render_decoded_image_byte_count(decoded), fill);
    entry.decoded_payload = make_render_image_decoded_payload_evidence(decoded);
    entry.payload_layout = make_render_image_texture_upload_payload_layout_evidence(
        entry.texture_key,
        entry.sampler,
        decoded);
    entry.staging_payload_plan = make_render_image_texture_staging_payload_plan(
        entry.payload_layout,
        make_render_image_texture_mipmap_upload_plan(decoded, entry.sampler));
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
    attach_decoded_upload_evidence(
        entry,
        entry.texture_width,
        entry.texture_height,
        placeholder_backed ? std::byte{0x7f} : std::byte{0xff});
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
        attach_decoded_upload_evidence(
            entry,
            entry.texture_width,
            entry.texture_height,
            std::byte{static_cast<unsigned char>(0x40 + plan_entry.request_index)});
        handoff.entries.push_back(entry);
        handoff.uploaded_byte_count += entry.uploaded_byte_count;
        handoff.total_mip_level_count += entry.mip_level_count;
        handoff.accepted_mip_level_count += entry.accepted_mip_level_count;
    }

    return make_render_image_texture_frame_resource_packet_plan(handoff);
}

quiz_vulkan::render::render_image_renderer_texture_quad_packet make_test_renderer_texture_quad_packet(
    std::string node_id,
    std::size_t packet_index,
    bool ready)
{
    using namespace quiz_vulkan::render;

    const render_image_sampler_policy sampler;
    const std::string sampler_key = render_image_sampler_policy_stable_fragment(sampler);
    const std::string texture_uri = "asset://textures/" + node_id + ".ppm";
    const std::string stable_draw_identity = "frame=diff-frame|node=" + node_id + "|parent=root";
    const std::string stable_texture_cache_key = texture_uri + "|" + sampler_key;
    render_image_renderer_texture_quad_packet packet{
        .packet_index = packet_index,
        .frame_label = "diff-frame",
        .draw_command_index = packet_index,
        .image_command_index = packet_index,
        .texture_request_index = packet_index,
        .node_id = node_id,
        .parent_node_id = "root",
        .bounds = render_rect{
            .x = static_cast<float>(packet_index * 10),
            .y = 4.0f,
            .width = 32.0f,
            .height = 16.0f,
        },
        .content_bounds = render_rect{
            .x = static_cast<float>(packet_index * 10 + 1),
            .y = 5.0f,
            .width = 30.0f,
            .height = 14.0f,
        },
        .image = render_image_ref{
            .uri = texture_uri,
            .alt_text = node_id,
            .aspect_ratio = 2.0f,
            .sampler = sampler,
        },
        .uri = texture_uri,
        .alt_text = node_id,
        .aspect_ratio = 2.0f,
        .sampler = sampler,
        .sampler_policy = make_render_image_sampler_policy_diagnostic(sampler),
        .sampler_key = sampler_key,
        .texture_key = render_image_texture_key{.source_key = texture_uri, .sampler = sampler},
        .texture_key_diagnostic = make_render_image_texture_key_diagnostic(
            render_image_texture_key{.source_key = texture_uri, .sampler = sampler}),
        .stable_draw_command_identity = stable_draw_identity,
        .stable_texture_cache_key = stable_texture_cache_key,
        .stable_quad_packet_identity = stable_draw_identity + "|texture=" + stable_texture_cache_key,
        .composition_status = ready
            ? render_image_draw_list_texture_frame_composition_entry_status::ready
            : render_image_draw_list_texture_frame_composition_entry_status::resource_packet_blocked,
        .composition_status_name = render_image_draw_list_texture_frame_composition_entry_status_name(
            ready
                ? render_image_draw_list_texture_frame_composition_entry_status::ready
                : render_image_draw_list_texture_frame_composition_entry_status::resource_packet_blocked),
        .resource_packet_status = ready
            ? render_image_texture_frame_resource_packet_status::resource_packet_ready
            : render_image_texture_frame_resource_packet_status::blocked,
        .resource_packet_status_name = render_image_texture_frame_resource_packet_status_name(
            ready
                ? render_image_texture_frame_resource_packet_status::resource_packet_ready
                : render_image_texture_frame_resource_packet_status::blocked),
        .texture_id = 900 + packet_index,
        .texture_revision = 4,
        .texture_width = 64,
        .texture_height = 32,
        .upload_request_id = 1200 + packet_index,
        .upload_generation_id = 8,
        .uploaded_byte_count = 128,
        .entered_texture_batch = true,
        .frame_entry_present = true,
        .resource_packet_present = true,
        .resource_packet_ready = ready,
        .renderer_handoff_ready = ready,
        .blocker_summary = ready ? "" : "resource packet blocked renderer texture quad packet",
    };
    packet.decoded_resource_evidence_present = true;
    packet.decoded_payload_hash = ready ? 10'000 + packet_index : 0;
    packet.decoded_byte_count = ready ? 256 + packet_index : 0;
    packet.upload_layout_byte_count = packet.decoded_byte_count;
    packet.upload_layout_row_stride_byte_count = ready ? 16 : 0;
    packet.staging_payload_byte_count = ready ? 256 + packet_index : 0;
    packet.staging_row_copy_count = ready ? 16 : 0;
    packet.decoded_payload_valid = ready;
    packet.upload_payload_layout_ready = ready;
    packet.staging_payload_ready = ready;
    packet.decoded_resource_ready = ready;
    packet.decoded_resource_blocked = !ready;
    packet.decoded_resource_summary = "decoded_bytes=" + std::to_string(packet.decoded_byte_count)
        + "; staging_bytes=" + std::to_string(packet.staging_payload_byte_count)
        + "; payload_hash=" + std::to_string(packet.decoded_payload_hash);
    packet.decoded_resource_blocker_summary =
        ready ? "" : "resource packet blocked renderer texture quad packet";
    finalize_render_image_renderer_texture_quad_packet(packet);
    return packet;
}

quiz_vulkan::render::render_image_renderer_texture_quad_packet_summary make_test_renderer_texture_quad_summary(
    std::vector<quiz_vulkan::render::render_image_renderer_texture_quad_packet> packets)
{
    using namespace quiz_vulkan::render;

    render_image_renderer_texture_quad_packet_summary summary{
        .frame_label = "diff-frame",
        .draw_command_count = packets.size(),
        .image_command_count = packets.size(),
    };

    std::map<std::uint64_t, bool> decoded_payload_hashes;
    for (render_image_renderer_texture_quad_packet& packet : packets) {
        packet.packet_index = summary.packets.size();
        if (packet.decoded_resource_evidence_present && packet.decoded_payload_hash != 0) {
            decoded_payload_hashes.emplace(packet.decoded_payload_hash, true);
        }
        count_render_image_renderer_texture_quad_packet(summary, packet);
        summary.packets.push_back(packet);
    }

    summary.packet_count = summary.packets.size();
    summary.decoded_payload_hash_count = decoded_payload_hashes.size();
    summary.renderer_quad_packets_ready = summary.packet_count != 0 && !summary.has_blockers;
    summary.decoded_resource_summary =
        "decoded_resources=" + std::to_string(summary.decoded_resource_ready_count)
        + "; payload_hashes=" + std::to_string(summary.decoded_payload_hash_count)
        + "; decoded_bytes=" + std::to_string(summary.decoded_payload_byte_count)
        + "; staging_bytes=" + std::to_string(summary.staging_payload_byte_count);
    if (summary.decoded_resource_blocker_summary.empty()) {
        summary.decoded_resource_blocker_summary = "no decoded resource blockers";
    }
    summary.status = summary.packet_count == 0
        ? render_image_renderer_texture_quad_packet_summary_status::empty
        : (summary.has_blockers
            ? render_image_renderer_texture_quad_packet_summary_status::blocked
            : render_image_renderer_texture_quad_packet_summary_status::ready);
    summary.status_name = render_image_renderer_texture_quad_packet_summary_status_name(summary.status);
    summary.diagnostic = summary.has_blockers
        ? "test renderer texture quad packet summary has blockers"
        : "test renderer texture quad packet summary is ready";
    return summary;
}

const quiz_vulkan::render::render_image_renderer_texture_quad_packet_diff_entry*
find_quad_packet_diff_entry(
    const quiz_vulkan::render::render_image_renderer_texture_quad_packet_summary_diff& diff,
    const std::string& identity_fragment)
{
    for (const quiz_vulkan::render::render_image_renderer_texture_quad_packet_diff_entry& entry :
         diff.entries) {
        if (entry.stable_diff_identity.find(identity_fragment) != std::string::npos) {
            return &entry;
        }
    }
    return nullptr;
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

    require(handoff.entries.size() == 2, "draw-list handoff exposes only image command entries");
    require(handoff.planned_images.size() == 2, "draw-list handoff emits planned image refs");
    require(handoff.planned_requests.size() == 2, "draw-list handoff emits texture requests");
    require(handoff.entries[0].draw_command_index == 1, "handoff keeps source draw command index");
    require(handoff.entries[0].texture_request_index == 0, "handoff assigns first texture request index");
    require(handoff.entries[0].planned_texture_request, "ready handoff entry is texture-request planned");
    require(handoff.entries[0].sampler_policy.valid, "handoff preserves valid sampler diagnostics");
    require(handoff.entries[0].texture_key_diagnostic.valid, "handoff preserves valid texture key diagnostics");
    require(
        handoff.entries[0].texture_key.source_key == "asset://textures/card-front.ppm",
        "handoff normalizes first image texture key");
    require(
        handoff.planned_images[0].uri == draw_list.commands[1].image.uri,
        "handoff planned image keeps draw-list image ref");
    require(
        handoff.planned_requests[0].uri == "asset://textures/card-front.ppm",
        "handoff planned request keeps normalized uri");

    require(batch_plan.entries.size() == 2, "texture batch plan keeps one entry per ready handoff");
    require(batch_plan.entries[0].request_index == handoff.entries[0].texture_request_index, "batch entry maps to handoff request index");
    require(batch_plan.entries[0].image.uri == handoff.entries[0].image.uri, "batch entry preserves handoff image ref");
    require(batch_plan.entries[0].pipeline_request.uri == handoff.entries[0].pipeline_request.uri, "batch entry preserves handoff request uri");
    require(batch_plan.entries[0].sampler_policy.valid, "batch entry keeps sampler diagnostics");
    require(batch_plan.entries[0].texture_key_diagnostic.valid, "batch entry keeps texture key diagnostics");
    require(batch_plan.entries[0].texture_key == handoff.entries[0].texture_key, "batch entry preserves handoff texture key");
    require(
        batch_plan.entries[0].stable_texture_cache_key == handoff.entries[0].stable_texture_cache_key,
        "batch entry preserves handoff cache identity");

    require(frame.entries.size() == 2, "texture frame snapshot keeps one entry per planned image");
    require(frame.entries[0].request_index == batch_plan.entries[0].request_index, "frame entry maps to batch request");
    require(frame.entries[0].render_image_uri == batch_plan.entries[0].image.uri, "frame entry preserves batch image uri");
    require(frame.entries[0].sampler_policy.valid, "frame entry keeps sampler diagnostics");
    require(
        frame.entries[0].stable_texture_cache_key == batch_plan.entries[0].stable_texture_cache_key,
        "frame entry preserves batch cache identity");
    require(frame.entries[0].renderer_handoff_ready, "frame entry is ready for renderer handoff");

    require(resources.entries.size() == 2, "resource packet plan keeps one entry per planned image");
    require(resources.entries[0].request_index == batch_plan.entries[0].request_index, "resource packet maps to batch request");
    require(
        resources.entries[0].stable_texture_cache_key == batch_plan.entries[0].stable_texture_cache_key,
        "resource packet preserves batch cache identity");
    require(resources.entries[0].sampler_summary.find("min=nearest") != std::string::npos, "resource packet preserves sampler evidence");
    require(resources.entries[0].resource_packet_ready, "resource packet is ready before composition");

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
    require(first.decoded_resource_evidence_present, "first entry links decoded resource evidence");
    require(first.decoded_resource_ready, "first entry records decoded resource readiness");
    require(first.decoded_payload_hash != 0, "first entry records decoded payload hash");
    require(first.decoded_byte_count == 8192, "first entry records decoded byte count");
    require(first.upload_layout_byte_count == 8192, "first entry records upload layout byte count");
    require(first.upload_layout_row_stride_byte_count == 256, "first entry records upload row stride");
    require(first.staging_payload_byte_count == 8192, "first entry records staging byte count");
    require(first.staging_row_copy_count == 32, "first entry records staging row copies");
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
    require(second.decoded_resource_ready, "second entry carries decoded resource readiness");
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

void test_renderer_texture_quad_packets_preserve_ready_composed_evidence()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.wrap_u = render_image_wrap_mode::repeat;

    const render_rect image_bounds{.x = 10.0f, .y = 20.0f, .width = 64.0f, .height = 32.0f};
    const render_rect image_content{.x = 12.0f, .y = 22.0f, .width = 60.0f, .height = 28.0f};
    const render_rect badge_bounds{.x = 90.0f, .y = 20.0f, .width = 40.0f, .height = 40.0f};
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
            .bounds = image_bounds,
            .content_bounds = image_content,
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
            .bounds = badge_bounds,
            .content_bounds = badge_bounds,
            .image = render_image_ref{
                .uri = "asset://textures/badge.ppm",
                .alt_text = "badge",
                .aspect_ratio = 1.0f,
            },
        },
    };

    const render_image_draw_list_frame_handoff_snapshot handoff =
        make_render_image_draw_list_frame_handoff_snapshot(draw_list, "quad-frame");
    const render_image_texture_batch_plan batch_plan = plan_render_image_texture_batch(handoff);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(batch_plan, make_ready_batch_execution(batch_plan));
    const render_image_texture_frame_resource_packet_plan resources =
        make_ready_resource_packet_plan(batch_plan);
    const render_image_draw_list_texture_frame_composition composition =
        make_render_image_draw_list_texture_frame_composition(handoff, batch_plan, frame, resources);

    const render_image_renderer_texture_quad_packet_summary summary =
        make_render_image_renderer_texture_quad_packet_summary(composition);

    require(summary.ok(), "renderer texture quad packet summary is ready");
    require(
        summary.status == render_image_renderer_texture_quad_packet_summary_status::ready,
        "renderer quad summary records ready status");
    require(summary.status_name == "ready", "renderer quad summary ready status name is stable");
    require(summary.frame_label == "quad-frame", "renderer quad summary preserves frame label");
    require(summary.draw_command_count == 3, "renderer quad summary preserves draw command count");
    require(summary.non_image_command_count == 1, "renderer quad summary preserves skipped command count");
    require(summary.image_command_count == 2, "renderer quad summary preserves image command count");
    require(summary.packet_count == 2, "renderer quad summary emits one packet per image command");
    require(summary.ready_packet_count == 2, "renderer quad summary counts ready packets");
    require(summary.blocked_packet_count == 0, "renderer quad summary has no blocked packets");
    require(summary.unique_stable_quad_packet_identity_count == 2, "renderer quad summary records unique packet ids");
    require(summary.unique_texture_cache_key_count == 2, "renderer quad summary records unique texture cache keys");
    require(summary.unique_sampler_key_count == 2, "renderer quad summary records unique sampler keys");
    require(summary.uploaded_byte_count == 128, "renderer quad summary records uploaded byte evidence");
    require(summary.decoded_resource_ready_count == 2, "renderer quad summary records decoded resource count");
    require(summary.decoded_payload_hash_count == 2, "renderer quad summary records decoded payload hashes");
    require(summary.decoded_payload_byte_count == 16512, "renderer quad summary records decoded byte evidence");
    require(summary.staging_payload_byte_count == 16512, "renderer quad summary records staging byte evidence");
    require(
        summary.decoded_resource_summary
            == "decoded_resources=2; payload_hashes=2; decoded_bytes=16512; staging_bytes=16512",
        "renderer quad summary decoded resource summary is stable");
    require(summary.renderer_quad_packets_ready, "renderer quad summary records renderer readiness");
    require(
        summary.skipped_command_summary == "skipped non-image draw commands=1",
        "renderer quad summary keeps non-image evidence");
    require(
        summary.diagnostic == "image renderer texture quad packet summary is ready",
        "renderer quad summary ready diagnostic is stable");

    const render_image_renderer_texture_quad_packet& first = summary.packets[0];
    require(first.ok(), "first renderer quad packet is ready");
    require(first.packet_index == 0, "first renderer quad packet index is deterministic");
    require(first.draw_command_index == 1, "first renderer quad packet preserves draw command index");
    require(first.image_command_index == 0, "first renderer quad packet preserves image command order");
    require(first.texture_request_index == 0, "first renderer quad packet preserves texture request index");
    require(first.node_id == "card-image", "first renderer quad packet preserves node id");
    require(first.parent_node_id == "card", "first renderer quad packet preserves parent node id");
    require(first.bounds.x == image_bounds.x && first.bounds.width == image_bounds.width, "first renderer quad packet preserves bounds");
    require(
        first.content_bounds.x == image_content.x && first.content_bounds.width == image_content.width,
        "first renderer quad packet preserves content bounds");
    require(first.uri == "asset://textures/card-front.ppm", "first renderer quad packet preserves image uri");
    require(first.image.uri == "asset://textures/card-front.ppm", "first renderer quad packet preserves image ref");
    require(first.alt_text == "front", "first renderer quad packet preserves alt text");
    require(first.aspect_ratio == 2.0f, "first renderer quad packet preserves aspect ratio");
    require(first.sampler.min_filter == render_image_filter::nearest, "first renderer quad packet preserves sampler filter");
    require(first.sampler.wrap_u == render_image_wrap_mode::repeat, "first renderer quad packet preserves sampler wrap");
    require(first.sampler_key.find("min=nearest") != std::string::npos, "first renderer quad packet records sampler identity");
    require(
        first.stable_draw_command_identity == "frame=quad-frame|node=card-image|parent=card",
        "first renderer quad packet preserves stable draw command identity");
    require(
        first.stable_texture_cache_key.find("card-front.ppm") != std::string::npos,
        "first renderer quad packet preserves stable texture cache key");
    require(
        first.stable_quad_packet_identity.find("texture=") != std::string::npos,
        "first renderer quad packet materializes stable packet identity");
    require(first.entered_texture_batch, "first renderer quad packet records texture batch entry");
    require(first.frame_entry_present, "first renderer quad packet records frame evidence");
    require(first.resource_packet_present, "first renderer quad packet records resource packet evidence");
    require(first.resource_packet_ready, "first renderer quad packet records resource packet readiness");
    require(first.renderer_handoff_ready, "first renderer quad packet records renderer handoff readiness");
    require(first.texture_id == 700, "first renderer quad packet carries texture id");
    require(first.texture_revision == 3, "first renderer quad packet carries texture revision");
    require(first.texture_width == 64, "first renderer quad packet carries texture width");
    require(first.texture_height == 32, "first renderer quad packet carries texture height");
    require(first.upload_request_id == 900, "first renderer quad packet carries upload request id");
    require(first.upload_generation_id == 11, "first renderer quad packet carries upload generation id");
    require(first.uploaded_byte_count == 64, "first renderer quad packet carries uploaded byte count");
    require(first.decoded_resource_evidence_present, "first renderer quad packet carries decoded evidence");
    require(first.decoded_resource_ready, "first renderer quad packet records decoded readiness");
    require(first.decoded_payload_hash != 0, "first renderer quad packet carries decoded payload hash");
    require(first.decoded_byte_count == 8192, "first renderer quad packet carries decoded byte count");
    require(first.upload_layout_byte_count == 8192, "first renderer quad packet carries upload layout bytes");
    require(first.upload_layout_row_stride_byte_count == 256, "first renderer quad packet carries row stride");
    require(first.staging_payload_byte_count == 8192, "first renderer quad packet carries staging bytes");
    require(first.staging_row_copy_count == 32, "first renderer quad packet carries staging row copies");
    require(
        first.status == render_image_renderer_texture_quad_packet_status::ready,
        "first renderer quad packet status is ready");
    require(first.status_name == "ready", "first renderer quad packet status name is stable");
    require(
        first.diagnostic == "image renderer texture quad packet is ready",
        "first renderer quad packet ready diagnostic is stable");

    const render_image_renderer_texture_quad_packet& second = summary.packets[1];
    require(second.ok(), "second renderer quad packet is ready");
    require(second.packet_index == 1, "second renderer quad packet index is deterministic");
    require(second.draw_command_index == 2, "second renderer quad packet preserves command order");
    require(second.texture_id == 701, "second renderer quad packet carries second texture id");
    require(second.uri == "asset://textures/badge.ppm", "second renderer quad packet preserves uri");
}

void test_renderer_texture_quad_packets_keep_blockers_and_identity_diagnostics()
{
    using namespace quiz_vulkan::render;

    const render_rect bounds{.x = 0.0f, .y = 0.0f, .width = 16.0f, .height = 16.0f};
    render_draw_list draw_list;
    draw_list.commands = {
        render_draw_command{
            .type = render_draw_command_type::image,
            .parent_node_id = "root",
            .bounds = bounds,
            .content_bounds = bounds,
            .image = render_image_ref{.uri = "asset://textures/missing-identity.ppm"},
        },
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "shared-image",
            .parent_node_id = "root",
            .bounds = bounds,
            .content_bounds = bounds,
            .image = render_image_ref{.uri = "asset://textures/shared.ppm"},
        },
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "shared-image",
            .parent_node_id = "root",
            .bounds = bounds,
            .content_bounds = bounds,
            .image = render_image_ref{.uri = "asset://textures/shared.ppm"},
        },
        render_draw_command{
            .type = render_draw_command_type::text,
            .node_id = "caption",
            .parent_node_id = "root",
            .bounds = bounds,
            .content_bounds = bounds,
        },
    };

    const render_image_draw_list_frame_handoff_snapshot handoff =
        make_render_image_draw_list_frame_handoff_snapshot(draw_list, "quad-blocked-frame");
    const render_image_texture_batch_plan batch_plan = plan_render_image_texture_batch(handoff);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(batch_plan, make_ready_batch_execution(batch_plan));
    const render_image_texture_frame_resource_packet_plan resources =
        make_ready_resource_packet_plan(batch_plan);
    const render_image_draw_list_texture_frame_composition composition =
        make_render_image_draw_list_texture_frame_composition(handoff, batch_plan, frame, resources);

    const render_image_renderer_texture_quad_packet_summary summary =
        make_render_image_renderer_texture_quad_packet_summary(composition);

    require(!summary.ok(), "renderer texture quad summary with identity blockers is blocked");
    require(
        summary.status == render_image_renderer_texture_quad_packet_summary_status::blocked,
        "renderer quad summary records blocked status");
    require(summary.packet_count == 3, "renderer quad summary emits image-only packets");
    require(summary.ready_packet_count == 0, "renderer quad summary blocks duplicate identity packets");
    require(summary.blocked_packet_count == 3, "renderer quad summary counts blocked packets");
    require(summary.missing_stable_identity_count == 1, "renderer quad summary counts missing stable identity");
    require(summary.duplicate_stable_identity_count == 2, "renderer quad summary counts duplicate stable identities");
    require(summary.has_missing_stable_identities, "renderer quad summary flags missing identities");
    require(summary.has_duplicate_stable_identities, "renderer quad summary flags duplicate identities");
    require(summary.non_image_command_count == 1, "renderer quad summary keeps non-image skip count");
    require(
        summary.diagnostic == "image renderer texture quad packet summary has blocked image packets",
        "renderer quad summary blocked diagnostic is stable");

    const render_image_renderer_texture_quad_packet& missing = summary.packets[0];
    require(
        missing.status == render_image_renderer_texture_quad_packet_status::blocked_missing_stable_identity,
        "missing identity packet reports missing identity blocker");
    require(missing.missing_stable_identity, "missing identity packet carries missing identity flag");
    require(!missing.entered_texture_batch, "missing identity packet does not enter texture batch");
    require(!missing.resource_packet_present, "missing identity packet has no resource packet");
    require(missing.draw_command_index == 0, "missing identity packet preserves draw command index");
    require(
        missing.diagnostic == "image renderer texture quad packet is blocked by missing stable identity",
        "missing identity packet diagnostic is stable");

    const render_image_renderer_texture_quad_packet& first_duplicate = summary.packets[1];
    require(
        first_duplicate.status == render_image_renderer_texture_quad_packet_status::blocked_duplicate_stable_identity,
        "first duplicate packet is blocked by summary duplicate detection");
    require(first_duplicate.duplicate_stable_identity, "first duplicate packet carries duplicate flag");
    require(first_duplicate.entered_texture_batch, "first duplicate packet still records batch evidence");
    require(first_duplicate.frame_entry_present, "first duplicate packet still records frame evidence");
    require(first_duplicate.resource_packet_present, "first duplicate packet still records resource evidence");
    require(first_duplicate.draw_command_index == 1, "first duplicate packet preserves draw command index");

    const render_image_renderer_texture_quad_packet& second_duplicate = summary.packets[2];
    require(
        second_duplicate.status
            == render_image_renderer_texture_quad_packet_status::blocked_duplicate_stable_identity,
        "second duplicate packet keeps duplicate blocker");
    require(second_duplicate.duplicate_stable_identity, "second duplicate packet carries duplicate flag");
    require(!second_duplicate.entered_texture_batch, "second duplicate packet does not enter texture batch");
    require(!second_duplicate.resource_packet_present, "second duplicate packet has no resource packet");
    require(second_duplicate.draw_command_index == 2, "second duplicate packet preserves draw command index");
    require(
        second_duplicate.blocker_summary == "renderer texture quad packet has duplicate stable identity",
        "second duplicate packet blocker summary is stable");
}

void test_renderer_texture_quad_packet_diff_reports_noop_for_identical_summaries()
{
    using namespace quiz_vulkan::render;

    const render_image_renderer_texture_quad_packet_summary before =
        make_test_renderer_texture_quad_summary({
            make_test_renderer_texture_quad_packet("card", 0, true),
            make_test_renderer_texture_quad_packet("badge", 1, true),
        });
    const render_image_renderer_texture_quad_packet_summary after = before;

    const render_image_renderer_texture_quad_packet_summary_diff diff =
        diff_render_image_renderer_texture_quad_packet_summaries(before, after);

    require(diff.ok(), "unchanged renderer texture quad diff is ok");
    require(
        diff.status == render_image_renderer_texture_quad_packet_summary_diff_status::unchanged,
        "unchanged renderer texture quad diff records unchanged status");
    require(diff.status_name == "unchanged", "unchanged renderer texture quad diff status name is stable");
    require(!diff.has_changes, "unchanged renderer texture quad diff has no changes");
    require(diff.before_packet_count == 2, "unchanged renderer texture quad diff records before count");
    require(diff.after_packet_count == 2, "unchanged renderer texture quad diff records after count");
    require(diff.packet_count_delta == 0, "unchanged renderer texture quad diff records zero packet delta");
    require(diff.entries.size() == 2, "unchanged renderer texture quad diff keeps entry evidence");
    require(diff.unchanged_packet_count == 2, "unchanged renderer texture quad diff counts unchanged packets");
    require(diff.changed_packet_count == 0, "unchanged renderer texture quad diff has no changed packets");
    require(diff.added_packet_count == 0, "unchanged renderer texture quad diff has no added packets");
    require(diff.removed_packet_count == 0, "unchanged renderer texture quad diff has no removed packets");
    require(diff.regression_count == 0, "unchanged renderer texture quad diff has no regressions");
    require(diff.recovery_count == 0, "unchanged renderer texture quad diff has no recoveries");
    require(
        diff.changed_identity_summary == "no renderer texture quad packet identity changes",
        "unchanged renderer texture quad diff identity summary is stable");
    require(
        diff.diagnostic == "image renderer texture quad packet summary diff is unchanged",
        "unchanged renderer texture quad diff diagnostic is stable");

    const render_image_renderer_texture_quad_packet_diff_entry& first = diff.entries[0];
    require(
        first.status == render_image_renderer_texture_quad_packet_diff_entry_status::unchanged,
        "unchanged renderer texture quad diff entry status is unchanged");
    require(first.classification_name == "neutral", "unchanged renderer texture quad diff entry is neutral");
    require(!first.changed(), "unchanged renderer texture quad diff entry reports no change");
}

void test_renderer_texture_quad_packet_diff_classifies_ready_blocked_transitions()
{
    using namespace quiz_vulkan::render;

    const render_image_renderer_texture_quad_packet_summary before =
        make_test_renderer_texture_quad_summary({
            make_test_renderer_texture_quad_packet("ready-then-blocked", 0, true),
            make_test_renderer_texture_quad_packet("blocked-then-ready", 1, false),
        });
    const render_image_renderer_texture_quad_packet_summary after =
        make_test_renderer_texture_quad_summary({
            make_test_renderer_texture_quad_packet("ready-then-blocked", 0, false),
            make_test_renderer_texture_quad_packet("blocked-then-ready", 1, true),
        });

    const render_image_renderer_texture_quad_packet_summary_diff diff =
        diff_render_image_renderer_texture_quad_packet_summaries(before, after);

    require(!diff.ok(), "renderer texture quad diff with regression is not ok");
    require(diff.has_changes, "renderer texture quad transition diff records changes");
    require(diff.changed_packet_count == 2, "renderer texture quad transition diff counts changed packets");
    require(diff.readiness_changed_count == 2, "renderer texture quad transition diff counts readiness changes");
    require(diff.blocker_changed_count == 2, "renderer texture quad transition diff counts blocker changes");
    require(diff.regression_count == 1, "renderer texture quad transition diff counts ready to blocked regression");
    require(diff.recovery_count == 1, "renderer texture quad transition diff counts blocked to ready recovery");
    require(diff.has_regressions, "renderer texture quad transition diff flags regressions");
    require(diff.has_recoveries, "renderer texture quad transition diff flags recoveries");

    const render_image_renderer_texture_quad_packet_diff_entry* regression =
        find_quad_packet_diff_entry(diff, "ready-then-blocked");
    require(regression != nullptr, "renderer texture quad transition diff exposes regression entry");
    require(regression->regression, "renderer texture quad transition entry marks regression");
    require(
        regression->classification
            == render_image_renderer_texture_quad_packet_diff_classification::regression,
        "renderer texture quad transition entry classification is regression");
    require(regression->before_ready, "renderer texture quad regression entry records before ready");
    require(regression->after_blocked, "renderer texture quad regression entry records after blocked");
    require(
        regression->diagnostic == "image renderer texture quad packet regressed from ready to blocked",
        "renderer texture quad regression diagnostic is stable");

    const render_image_renderer_texture_quad_packet_diff_entry* recovery =
        find_quad_packet_diff_entry(diff, "blocked-then-ready");
    require(recovery != nullptr, "renderer texture quad transition diff exposes recovery entry");
    require(recovery->recovery, "renderer texture quad transition entry marks recovery");
    require(
        recovery->classification
            == render_image_renderer_texture_quad_packet_diff_classification::recovery,
        "renderer texture quad transition entry classification is recovery");
    require(recovery->before_blocked, "renderer texture quad recovery entry records before blocked");
    require(recovery->after_ready, "renderer texture quad recovery entry records after ready");
    require(
        recovery->diagnostic == "image renderer texture quad packet recovered from blocked to ready",
        "renderer texture quad recovery diagnostic is stable");
}

void test_renderer_texture_quad_packet_diff_counts_layout_texture_sampler_cache_and_upload_changes()
{
    using namespace quiz_vulkan::render;

    render_image_renderer_texture_quad_packet before_packet =
        make_test_renderer_texture_quad_packet("mutable", 0, true);
    render_image_renderer_texture_quad_packet after_packet = before_packet;
    after_packet.bounds.width += 3.0f;
    after_packet.content_bounds.x += 2.0f;
    after_packet.texture_id += 10;
    after_packet.texture_revision += 1;
    after_packet.texture_width += 8;
    after_packet.sampler.min_filter = render_image_filter::nearest;
    after_packet.sampler_key = render_image_sampler_policy_stable_fragment(after_packet.sampler);
    after_packet.stable_texture_cache_key = "asset://textures/mutable-updated.ppm|" + after_packet.sampler_key;
    after_packet.stable_quad_packet_identity =
        after_packet.stable_draw_command_identity + "|texture=" + after_packet.stable_texture_cache_key;
    after_packet.upload_request_id += 20;
    after_packet.upload_generation_id += 2;
    after_packet.uploaded_byte_count += 32;
    after_packet.decoded_payload_hash += 1;
    after_packet.decoded_byte_count += 4;
    after_packet.upload_layout_byte_count += 4;
    after_packet.staging_payload_byte_count += 8;
    after_packet.decoded_resource_summary =
        "decoded_bytes=" + std::to_string(after_packet.decoded_byte_count)
        + "; staging_bytes=" + std::to_string(after_packet.staging_payload_byte_count)
        + "; payload_hash=" + std::to_string(after_packet.decoded_payload_hash);
    finalize_render_image_renderer_texture_quad_packet(after_packet);

    const render_image_renderer_texture_quad_packet_summary before =
        make_test_renderer_texture_quad_summary({before_packet});
    const render_image_renderer_texture_quad_packet_summary after =
        make_test_renderer_texture_quad_summary({after_packet});

    const render_image_renderer_texture_quad_packet_summary_diff diff =
        diff_render_image_renderer_texture_quad_packet_summaries(before, after);

    require(diff.ok(), "renderer texture quad mutation diff without regression is ok");
    require(diff.has_changes, "renderer texture quad mutation diff records changes");
    require(diff.changed_packet_count == 1, "renderer texture quad mutation diff counts changed packet");
    require(diff.stable_quad_packet_identity_changed_count == 1, "renderer texture quad mutation diff counts packet identity change");
    require(diff.bounds_changed_count == 1, "renderer texture quad mutation diff counts bounds change");
    require(diff.content_bounds_changed_count == 1, "renderer texture quad mutation diff counts content bounds change");
    require(diff.texture_id_changed_count == 1, "renderer texture quad mutation diff counts texture id change");
    require(diff.texture_revision_changed_count == 1, "renderer texture quad mutation diff counts texture revision change");
    require(diff.texture_size_changed_count == 1, "renderer texture quad mutation diff counts texture size change");
    require(diff.sampler_changed_count == 1, "renderer texture quad mutation diff counts sampler change");
    require(diff.cache_key_changed_count == 1, "renderer texture quad mutation diff counts cache key change");
    require(diff.upload_request_changed_count == 1, "renderer texture quad mutation diff counts upload request change");
    require(diff.upload_generation_changed_count == 1, "renderer texture quad mutation diff counts upload generation change");
    require(diff.uploaded_byte_count_changed_count == 1, "renderer texture quad mutation diff counts uploaded bytes change");
    require(diff.decoded_payload_hash_changed_count == 1, "renderer texture quad mutation diff counts decoded payload hash change");
    require(diff.decoded_byte_count_changed_count == 1, "renderer texture quad mutation diff counts decoded byte change");
    require(diff.upload_layout_byte_count_changed_count == 1, "renderer texture quad mutation diff counts upload layout byte change");
    require(diff.staging_payload_byte_count_changed_count == 1, "renderer texture quad mutation diff counts staging byte change");
    require(diff.has_decoded_resource_changes, "renderer texture quad mutation diff flags decoded resource changes");
    require(diff.has_identity_changes, "renderer texture quad mutation diff flags identity changes");
    require(diff.has_layout_changes, "renderer texture quad mutation diff flags layout changes");
    require(diff.has_texture_changes, "renderer texture quad mutation diff flags texture changes");
    require(diff.has_sampler_or_cache_changes, "renderer texture quad mutation diff flags sampler/cache changes");
    require(diff.has_upload_changes, "renderer texture quad mutation diff flags upload changes");

    const render_image_renderer_texture_quad_packet_diff_entry* entry =
        find_quad_packet_diff_entry(diff, "mutable");
    require(entry != nullptr, "renderer texture quad mutation diff exposes changed entry");
    require(entry->changed(), "renderer texture quad mutation diff entry reports changed");
    require(entry->classification_name == "churn", "renderer texture quad mutation diff entry is churn");
    require(entry->uploaded_byte_delta == 32, "renderer texture quad mutation diff entry records byte delta");
    require(entry->decoded_byte_delta == 4, "renderer texture quad mutation diff entry records decoded byte delta");
    require(entry->staging_payload_byte_delta == 8, "renderer texture quad mutation diff entry records staging byte delta");
    require(
        entry->diagnostic == "image renderer texture quad packet changed",
        "renderer texture quad mutation diff diagnostic is stable");
}

void test_renderer_texture_quad_packet_diff_reports_added_removed_duplicate_and_missing_identity_changes()
{
    using namespace quiz_vulkan::render;

    render_image_renderer_texture_quad_packet missing_before =
        make_test_renderer_texture_quad_packet("missing-id", 2, true);
    missing_before.missing_stable_identity = true;
    finalize_render_image_renderer_texture_quad_packet(missing_before);

    render_image_renderer_texture_quad_packet duplicate_before =
        make_test_renderer_texture_quad_packet("duplicate-id", 3, true);
    duplicate_before.duplicate_stable_identity = true;
    finalize_render_image_renderer_texture_quad_packet(duplicate_before);

    const render_image_renderer_texture_quad_packet_summary before =
        make_test_renderer_texture_quad_summary({
            make_test_renderer_texture_quad_packet("stable", 0, true),
            make_test_renderer_texture_quad_packet("removed", 1, true),
            missing_before,
            duplicate_before,
        });
    const render_image_renderer_texture_quad_packet_summary after =
        make_test_renderer_texture_quad_summary({
            make_test_renderer_texture_quad_packet("stable", 0, true),
            make_test_renderer_texture_quad_packet("added", 1, true),
            make_test_renderer_texture_quad_packet("missing-id", 2, true),
            make_test_renderer_texture_quad_packet("duplicate-id", 3, true),
        });

    const render_image_renderer_texture_quad_packet_summary_diff diff =
        diff_render_image_renderer_texture_quad_packet_summaries(before, after);

    require(diff.has_changes, "renderer texture quad identity diff records changes");
    require(diff.entries.size() == 5, "renderer texture quad identity diff records deterministic union entries");
    require(diff.unchanged_packet_count == 1, "renderer texture quad identity diff counts unchanged packet");
    require(diff.added_packet_count == 1, "renderer texture quad identity diff counts added packet");
    require(diff.removed_packet_count == 1, "renderer texture quad identity diff counts removed packet");
    require(diff.changed_packet_count == 2, "renderer texture quad identity diff counts changed identity packets");
    require(diff.missing_stable_identity_changed_count == 1, "renderer texture quad identity diff counts missing identity transition");
    require(diff.duplicate_stable_identity_changed_count == 1, "renderer texture quad identity diff counts duplicate identity transition");
    require(diff.recovery_count == 2, "renderer texture quad identity diff treats identity blocker removals as recovery");
    require(diff.has_identity_changes, "renderer texture quad identity diff flags identity changes");
    require(
        diff.entries[0].stable_diff_identity <= diff.entries[1].stable_diff_identity
            && diff.entries[1].stable_diff_identity <= diff.entries[2].stable_diff_identity
            && diff.entries[2].stable_diff_identity <= diff.entries[3].stable_diff_identity
            && diff.entries[3].stable_diff_identity <= diff.entries[4].stable_diff_identity,
        "renderer texture quad identity diff order is deterministic");

    const render_image_renderer_texture_quad_packet_diff_entry* added =
        find_quad_packet_diff_entry(diff, "added");
    require(added != nullptr, "renderer texture quad identity diff exposes added packet");
    require(
        added->status == render_image_renderer_texture_quad_packet_diff_entry_status::added,
        "renderer texture quad identity diff added packet status is stable");
    require(!added->before_present && added->after_present, "renderer texture quad added packet records presence");

    const render_image_renderer_texture_quad_packet_diff_entry* removed =
        find_quad_packet_diff_entry(diff, "removed");
    require(removed != nullptr, "renderer texture quad identity diff exposes removed packet");
    require(
        removed->status == render_image_renderer_texture_quad_packet_diff_entry_status::removed,
        "renderer texture quad identity diff removed packet status is stable");
    require(removed->before_present && !removed->after_present, "renderer texture quad removed packet records presence");

    const render_image_renderer_texture_quad_packet_diff_entry* missing =
        find_quad_packet_diff_entry(diff, "missing-id");
    require(missing != nullptr, "renderer texture quad identity diff exposes missing identity transition");
    require(missing->missing_stable_identity_changed, "renderer texture quad identity diff records missing identity change");
    require(missing->recovery, "renderer texture quad identity diff classifies missing identity removal as recovery");

    const render_image_renderer_texture_quad_packet_diff_entry* duplicate =
        find_quad_packet_diff_entry(diff, "duplicate-id");
    require(duplicate != nullptr, "renderer texture quad identity diff exposes duplicate identity transition");
    require(duplicate->duplicate_stable_identity_changed, "renderer texture quad identity diff records duplicate identity change");
    require(duplicate->recovery, "renderer texture quad identity diff classifies duplicate identity removal as recovery");
}

void test_renderer_texture_quad_draw_payloads_preserve_ready_packet_data()
{
    using namespace quiz_vulkan::render;

    const render_image_renderer_texture_quad_packet_summary summary =
        make_test_renderer_texture_quad_summary({
            make_test_renderer_texture_quad_packet("card", 0, true),
            make_test_renderer_texture_quad_packet("badge", 1, true),
        });

    const render_image_renderer_texture_quad_draw_payload_frame frame =
        make_render_image_renderer_texture_quad_draw_payload_frame(summary);

    require(frame.ok(), "ready renderer texture quad payload frame is ok");
    require(
        frame.status == render_image_renderer_texture_quad_draw_payload_frame_status::draw_ready,
        "ready renderer texture quad payload frame status is draw ready");
    require(frame.status_name == "draw_ready", "ready renderer texture quad payload frame status name is stable");
    require(frame.payload_count == 2, "ready renderer texture quad payload frame records payload count");
    require(frame.draw_ready_payload_count == 2, "ready renderer texture quad payload frame counts draw-ready payloads");
    require(frame.placeholder_payload_count == 0, "ready renderer texture quad payload frame has no placeholders");
    require(frame.blocked_payload_count == 0, "ready renderer texture quad payload frame has no blockers");
    require(frame.decoded_resource_ready_payload_count == 2, "ready renderer texture quad payload frame counts decoded resources");
    require(frame.decoded_payload_hash_count == 2, "ready renderer texture quad payload frame counts decoded hashes");
    require(frame.decoded_payload_byte_count == 513, "ready renderer texture quad payload frame sums decoded bytes");
    require(frame.staging_payload_byte_count == 513, "ready renderer texture quad payload frame sums staging bytes");
    require(
        frame.decoded_resource_summary
            == "decoded_payloads=2; payload_hashes=2; decoded_bytes=513; staging_bytes=513",
        "ready renderer texture quad payload frame decoded summary is stable");
    require(frame.draw_payloads_ready, "ready renderer texture quad payload frame marks payloads ready");
    require(
        frame.diagnostic == "image renderer texture quad draw payload frame is ready",
        "ready renderer texture quad payload frame diagnostic is stable");

    const render_image_renderer_texture_quad_draw_payload& first = frame.payloads[0];
    require(first.ok(), "ready renderer texture quad payload is ok");
    require(
        first.status == render_image_renderer_texture_quad_draw_payload_status::draw_ready,
        "ready renderer texture quad payload status is draw ready");
    require(first.status_name == "draw_ready", "ready renderer texture quad payload status name is stable");
    require(first.draw_ready, "ready renderer texture quad payload records draw-ready flag");
    require(!first.placeholder_backed, "ready renderer texture quad payload is not placeholder-backed");
    require(!first.blocked, "ready renderer texture quad payload is not blocked");
    require(first.payload_index == 0, "ready renderer texture quad payload index is deterministic");
    require(first.source_packet_index == 0, "ready renderer texture quad payload preserves source packet index");
    require(first.draw_command_index == 0, "ready renderer texture quad payload preserves command index");
    require(first.node_id == "card", "ready renderer texture quad payload preserves node id");
    require(first.parent_node_id == "root", "ready renderer texture quad payload preserves parent node id");
    require(first.bounds.width == 32.0f, "ready renderer texture quad payload preserves bounds");
    require(first.content_bounds.width == 30.0f, "ready renderer texture quad payload preserves content bounds");
    require(first.uri == "asset://textures/card.ppm", "ready renderer texture quad payload preserves source uri");
    require(first.alt_text == "card", "ready renderer texture quad payload preserves alt text");
    require(first.texture_id == 900, "ready renderer texture quad payload preserves texture id");
    require(first.texture_revision == 4, "ready renderer texture quad payload preserves texture revision");
    require(first.texture_width == 64, "ready renderer texture quad payload preserves texture width");
    require(first.texture_height == 32, "ready renderer texture quad payload preserves texture height");
    require(first.upload_request_id == 1200, "ready renderer texture quad payload preserves upload request");
    require(first.upload_generation_id == 8, "ready renderer texture quad payload preserves upload generation");
    require(first.uploaded_byte_count == 128, "ready renderer texture quad payload preserves uploaded byte count");
    require(first.decoded_resource_evidence_present, "ready renderer texture quad payload preserves decoded evidence flag");
    require(first.decoded_resource_ready, "ready renderer texture quad payload preserves decoded readiness");
    require(first.decoded_payload_hash != 0, "ready renderer texture quad payload preserves decoded hash");
    require(first.decoded_byte_count == 256, "ready renderer texture quad payload preserves decoded bytes");
    require(first.upload_layout_byte_count == 256, "ready renderer texture quad payload preserves layout bytes");
    require(first.staging_payload_byte_count == 256, "ready renderer texture quad payload preserves staging bytes");
    require(first.sampler_key.find("min=linear") != std::string::npos, "ready renderer texture quad payload preserves sampler key");
    require(first.stable_texture_cache_key.find("card.ppm") != std::string::npos, "ready renderer texture quad payload preserves cache key");
    require(
        first.stable_payload_identity.find("payload=texture:900:4") != std::string::npos,
        "ready renderer texture quad payload identity includes texture handle");
    require(
        first.diagnostic == "image renderer texture quad draw payload is ready",
        "ready renderer texture quad payload diagnostic is stable");
}

void test_renderer_texture_quad_draw_payloads_use_policy_placeholder_for_blocked_packets()
{
    using namespace quiz_vulkan::render;

    const render_image_renderer_texture_quad_packet_summary summary =
        make_test_renderer_texture_quad_summary({
            make_test_renderer_texture_quad_packet("blocked", 0, false),
        });
    render_image_renderer_texture_quad_draw_payload_options options;
    options.placeholder_policy.enabled = true;
    options.placeholder_policy.width = 3;
    options.placeholder_policy.height = 5;
    options.placeholder_policy.source_key_prefix = "placeholder://quad/";

    const render_image_renderer_texture_quad_draw_payload_frame frame =
        make_render_image_renderer_texture_quad_draw_payload_frame(summary, options);

    require(frame.ok(), "placeholder renderer texture quad payload frame is ok");
    require(
        frame.status == render_image_renderer_texture_quad_draw_payload_frame_status::placeholder_backed,
        "placeholder renderer texture quad payload frame status is placeholder-backed");
    require(frame.placeholder_policy_enabled, "placeholder renderer texture quad payload frame records policy");
    require(frame.placeholder_payload_count == 1, "placeholder renderer texture quad payload frame counts placeholder");
    require(frame.fallback_placeholder_payload_count == 1, "placeholder renderer texture quad payload frame counts fallback placeholder");
    require(frame.blocked_payload_count == 0, "placeholder renderer texture quad payload frame has no blockers");
    require(frame.decoded_resource_blocked_payload_count == 1, "placeholder renderer texture quad payload frame records decoded blocker");
    require(frame.has_decoded_resource_blockers, "placeholder renderer texture quad payload frame preserves decoded blocker flag");
    require(frame.has_fallback_placeholders, "placeholder renderer texture quad payload frame flags fallback placeholders");
    require(
        frame.diagnostic == "image renderer texture quad draw payload frame is placeholder-backed",
        "placeholder renderer texture quad payload frame diagnostic is stable");

    const render_image_renderer_texture_quad_draw_payload& payload = frame.payloads[0];
    require(payload.ok(), "placeholder renderer texture quad payload is ok");
    require(
        payload.status == render_image_renderer_texture_quad_draw_payload_status::placeholder_backed,
        "placeholder renderer texture quad payload status is placeholder-backed");
    require(!payload.draw_ready, "fallback placeholder renderer texture quad payload is not real draw-ready");
    require(payload.placeholder_backed, "fallback placeholder renderer texture quad payload records placeholder");
    require(payload.fallback_placeholder, "fallback placeholder renderer texture quad payload records fallback flag");
    require(!payload.blocked, "fallback placeholder renderer texture quad payload is not blocked");
    require(payload.texture_id == 0, "fallback placeholder renderer texture quad payload does not invent texture id");
    require(payload.texture_width == 3, "fallback placeholder renderer texture quad payload uses policy width");
    require(payload.texture_height == 5, "fallback placeholder renderer texture quad payload uses policy height");
    require(payload.decoded_resource_evidence_present, "fallback placeholder renderer texture quad payload keeps decoded evidence");
    require(payload.decoded_resource_blocked, "fallback placeholder renderer texture quad payload keeps decoded blocker flag");
    require(
        payload.decoded_resource_blocker_summary == "resource packet blocked renderer texture quad packet",
        "fallback placeholder renderer texture quad payload preserves decoded blocker summary");
    require(
        payload.placeholder_key.source_key.find("placeholder://quad/upload_failed/asset://textures/blocked.ppm")
            == 0,
        "fallback placeholder renderer texture quad payload uses deterministic placeholder key");
    require(
        payload.stable_texture_cache_key.find("placeholder://quad/upload_failed/asset://textures/blocked.ppm")
            != std::string::npos,
        "fallback placeholder renderer texture quad payload exposes placeholder cache key");
    require(
        payload.stable_payload_identity.find("payload=placeholder:") != std::string::npos,
        "fallback placeholder renderer texture quad payload identity records placeholder");
    require(
        payload.diagnostic.find("using deterministic placeholder texture for upload_failed") == 0,
        "fallback placeholder renderer texture quad payload diagnostic is stable");
}

void test_renderer_texture_quad_draw_payloads_keep_blocked_packets_blocked_without_policy()
{
    using namespace quiz_vulkan::render;

    const render_image_renderer_texture_quad_packet_summary summary =
        make_test_renderer_texture_quad_summary({
            make_test_renderer_texture_quad_packet("blocked", 0, false),
        });

    const render_image_renderer_texture_quad_draw_payload_frame frame =
        make_render_image_renderer_texture_quad_draw_payload_frame(summary);

    require(!frame.ok(), "blocked renderer texture quad payload frame is not ok");
    require(
        frame.status == render_image_renderer_texture_quad_draw_payload_frame_status::blocked,
        "blocked renderer texture quad payload frame status is blocked");
    require(!frame.placeholder_policy_enabled, "blocked renderer texture quad payload frame records disabled policy");
    require(frame.placeholder_payload_count == 0, "blocked renderer texture quad payload frame has no placeholder");
    require(frame.blocked_payload_count == 1, "blocked renderer texture quad payload frame counts blocker");
    require(frame.has_blockers, "blocked renderer texture quad payload frame flags blockers");
    require(
        frame.diagnostic == "image renderer texture quad draw payload frame has blocked payloads",
        "blocked renderer texture quad payload frame diagnostic is stable");

    const render_image_renderer_texture_quad_draw_payload& payload = frame.payloads[0];
    require(!payload.ok(), "blocked renderer texture quad payload is not ok");
    require(
        payload.status == render_image_renderer_texture_quad_draw_payload_status::blocked,
        "blocked renderer texture quad payload status is blocked");
    require(!payload.draw_ready, "blocked renderer texture quad payload is not draw ready");
    require(!payload.placeholder_backed, "blocked renderer texture quad payload is not placeholder-backed");
    require(payload.blocked, "blocked renderer texture quad payload records blocked flag");
    require(payload.decoded_resource_blocked, "blocked renderer texture quad payload preserves decoded blocker flag");
    require(
        payload.blocker_summary == "resource packet blocked renderer texture quad packet",
        "blocked renderer texture quad payload preserves blocker summary");
    require(
        payload.decoded_resource_blocker_summary == "resource packet blocked renderer texture quad packet",
        "blocked renderer texture quad payload preserves decoded blocker summary");
    require(
        payload.stable_payload_identity.find("payload=blocked:blocked_resource_packet") != std::string::npos,
        "blocked renderer texture quad payload identity records blocked reason");
    require(
        payload.diagnostic
            == "image renderer texture quad draw payload is blocked: "
               "resource packet blocked renderer texture quad packet",
        "blocked renderer texture quad payload diagnostic is stable");
}

void test_renderer_texture_quad_draw_payload_identity_is_stable_and_deterministic()
{
    using namespace quiz_vulkan::render;

    const render_image_renderer_texture_quad_packet_summary summary =
        make_test_renderer_texture_quad_summary({
            make_test_renderer_texture_quad_packet("card", 0, true),
            make_test_renderer_texture_quad_packet("badge", 1, true),
        });

    const render_image_renderer_texture_quad_draw_payload_frame first =
        make_render_image_renderer_texture_quad_draw_payload_frame(summary);
    const render_image_renderer_texture_quad_draw_payload_frame second =
        make_render_image_renderer_texture_quad_draw_payload_frame(summary);

    require(first.payloads.size() == second.payloads.size(), "deterministic payload frame sizes match");
    require(first.payloads[0].stable_payload_identity == second.payloads[0].stable_payload_identity, "first payload identity is stable");
    require(first.payloads[1].stable_payload_identity == second.payloads[1].stable_payload_identity, "second payload identity is stable");
    require(first.payloads[0].source_packet_index == 0, "first payload preserves deterministic packet order");
    require(first.payloads[1].source_packet_index == 1, "second payload preserves deterministic packet order");
    require(first.payloads[0].stable_payload_identity != first.payloads[1].stable_payload_identity, "payload identities remain distinct");
    require(
        first.payload_identity_summary == second.payload_identity_summary,
        "payload identity summary is stable across repeated consumption");
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
    require(
        render_image_renderer_texture_quad_packet_status_name(
            render_image_renderer_texture_quad_packet_status::blocked_missing_resource_packet)
            == "blocked_missing_resource_packet",
        "renderer texture quad missing resource status name is stable");
    require(
        render_image_renderer_texture_quad_packet_status_is_blocked(
            render_image_renderer_texture_quad_packet_status::blocked_duplicate_stable_identity),
        "renderer texture quad duplicate identity status is blocked");
    require(
        render_image_renderer_texture_quad_packet_summary_status_name(
            render_image_renderer_texture_quad_packet_summary_status::ready)
            == "ready",
        "renderer texture quad summary status name is stable");
    require(
        render_image_renderer_texture_quad_packet_diff_entry_status_name(
            render_image_renderer_texture_quad_packet_diff_entry_status::changed)
            == "changed",
        "renderer texture quad diff entry status name is stable");
    require(
        render_image_renderer_texture_quad_packet_diff_classification_name(
            render_image_renderer_texture_quad_packet_diff_classification::recovery)
            == "recovery",
        "renderer texture quad diff classification name is stable");
    require(
        render_image_renderer_texture_quad_packet_summary_diff_status_name(
            render_image_renderer_texture_quad_packet_summary_diff_status::unchanged)
            == "unchanged",
        "renderer texture quad summary diff status name is stable");
    require(
        render_image_renderer_texture_quad_draw_payload_status_name(
            render_image_renderer_texture_quad_draw_payload_status::placeholder_backed)
            == "placeholder_backed",
        "renderer texture quad draw payload status name is stable");
    require(
        render_image_renderer_texture_quad_draw_payload_frame_status_name(
            render_image_renderer_texture_quad_draw_payload_frame_status::draw_ready)
            == "draw_ready",
        "renderer texture quad draw payload frame status name is stable");
}

} // namespace

int main()
{
    test_resource_packet_plan_reports_ready_and_placeholder_entries();
    test_resource_packet_plan_reports_missing_binding_upload_and_retry_blockers();
    test_draw_list_texture_frame_composition_links_ready_commands_to_resources();
    test_draw_list_texture_frame_composition_keeps_blocked_handoff_out_of_batch();
    test_renderer_texture_quad_packets_preserve_ready_composed_evidence();
    test_renderer_texture_quad_packets_keep_blockers_and_identity_diagnostics();
    test_renderer_texture_quad_packet_diff_reports_noop_for_identical_summaries();
    test_renderer_texture_quad_packet_diff_classifies_ready_blocked_transitions();
    test_renderer_texture_quad_packet_diff_counts_layout_texture_sampler_cache_and_upload_changes();
    test_renderer_texture_quad_packet_diff_reports_added_removed_duplicate_and_missing_identity_changes();
    test_renderer_texture_quad_draw_payloads_preserve_ready_packet_data();
    test_renderer_texture_quad_draw_payloads_use_policy_placeholder_for_blocked_packets();
    test_renderer_texture_quad_draw_payloads_keep_blocked_packets_blocked_without_policy();
    test_renderer_texture_quad_draw_payload_identity_is_stable_and_deterministic();
    test_resource_packet_status_helpers_are_stable();
    return 0;
}
