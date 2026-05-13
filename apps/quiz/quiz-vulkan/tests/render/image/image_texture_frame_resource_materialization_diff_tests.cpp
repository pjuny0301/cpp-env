#include "render/image/image_texture_frame_resource_materialization_diff.h"

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

template <typename T>
concept HasFakePipelineEntriesField = requires(T value) {
    { value.entries } -> std::same_as<std::vector<quiz_vulkan::render::fake_image_texture_pipeline_entry_snapshot>&>;
};

static_assert(!HasFakeCacheSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_cache_handoff_delta>);
static_assert(!HasFakeCacheSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_upload_handoff_delta>);
static_assert(!HasFakeCacheSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_sampler_handoff_delta>);
static_assert(!HasFakeCacheSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_materialization_entry_diff>);
static_assert(!HasFakeCacheSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_materialization_diff>);
static_assert(!HasFakeUploadSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_cache_handoff_delta>);
static_assert(!HasFakeUploadSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_upload_handoff_delta>);
static_assert(!HasFakeUploadSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_sampler_handoff_delta>);
static_assert(!HasFakeUploadSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_materialization_entry_diff>);
static_assert(!HasFakeUploadSnapshotField<
              quiz_vulkan::render::render_image_texture_frame_resource_materialization_diff>);
static_assert(!HasFakePipelineEntriesField<
              quiz_vulkan::render::render_image_texture_frame_resource_materialization_entry_diff>);
static_assert(!HasFakePipelineEntriesField<
              quiz_vulkan::render::render_image_texture_frame_resource_materialization_diff>);

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
    quiz_vulkan::render::render_image_texture_frame_resource_packet_status status,
    quiz_vulkan::render::render_image_sampler_policy sampler = {},
    std::uint64_t upload_request_id = 0,
    std::uint64_t upload_generation_id = 0,
    quiz_vulkan::render::render_image_texture_id texture_id = 0,
    std::size_t uploaded_byte_count = 0,
    std::size_t mip_level_count = 0)
{
    using namespace quiz_vulkan::render;

    const std::string sampler_key = render_image_sampler_policy_stable_fragment(sampler);
    const bool bindable = render_image_texture_frame_resource_packet_status_is_bindable(status);
    const bool placeholder = status == render_image_texture_frame_resource_packet_status::placeholder_backed;
    const bool blocked = render_image_texture_frame_resource_packet_status_is_blocked(status);
    const std::uint64_t resolved_upload_request_id = upload_request_id != 0
        ? upload_request_id
        : (bindable ? 200 + request_index : 0);
    const std::uint64_t resolved_upload_generation_id = upload_generation_id != 0
        ? upload_generation_id
        : (bindable ? 5 + request_index : 0);
    const render_image_texture_id resolved_texture_id = texture_id != 0
        ? texture_id
        : (bindable ? 100 + request_index : 0);
    const std::size_t resolved_uploaded_byte_count = uploaded_byte_count != 0
        ? uploaded_byte_count
        : (placeholder ? 16u : (bindable ? 8u : 0u));
    const std::size_t resolved_mip_level_count = mip_level_count != 0 ? mip_level_count : (bindable ? 1u : 0u);

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
        .texture_id = resolved_texture_id,
        .texture_revision = bindable ? 7u + request_index : 0u,
        .texture_width = placeholder ? 2u : (bindable ? 4u : 0u),
        .texture_height = placeholder ? 2u : (bindable ? 1u : 0u),
        .upload_request_id = resolved_upload_request_id,
        .upload_generation_id = resolved_upload_generation_id,
        .mip_level_count = resolved_mip_level_count,
        .uploaded_byte_count = resolved_uploaded_byte_count,
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
        .removed = status == render_image_texture_frame_resource_packet_status::removed,
        .cache_reused = request_index == 1,
        .expected_cache_reuse = request_index == 1,
        .cache_key_summary = source_key,
        .sampler_summary = sampler_key,
        .blocker_summary = blocked ? "resource packet is blocked" : "",
        .diagnostic = bindable ? "resource packet can materialize" : "resource packet is blocked",
    };

    if (packet.retry_backoff_blocked) {
        packet.retry_backoff_summary = "retryable";
        packet.retry_after_queue_sequence_delta = 3;
        packet.next_retry_sequence = 11;
        packet.blocker_summary = "upload failed but can retry: invalid_image";
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

void test_unchanged_materialization_diff_is_stable()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_frame_resource_packet_materialization before = materialize_packets({
        make_packet(0, "asset://textures/card.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(1, "asset://textures/placeholder.ppm", render_image_texture_frame_resource_packet_status::placeholder_backed),
    });
    const render_image_texture_frame_resource_packet_materialization after = materialize_packets({
        make_packet(0, "asset://textures/card.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(1, "asset://textures/placeholder.ppm", render_image_texture_frame_resource_packet_status::placeholder_backed),
    });

    const render_image_texture_frame_resource_materialization_diff diff =
        diff_render_image_texture_frame_resource_materializations(before, after);

    require(diff.ok(), "unchanged materialization diff is ok");
    require(!diff.has_changes, "unchanged materialization diff reports no changes");
    require(!diff.has_regression, "unchanged materialization diff reports no regression");
    require(diff.unchanged_entry_count == 2, "unchanged materialization diff counts unchanged entries");
    require(diff.entries.size() == 2, "unchanged materialization diff preserves entry count");
    require(diff.entries[0].request_index == 0, "unchanged diff sorts first request");
    require(diff.entries[1].request_index == 1, "unchanged diff sorts second request");
    require(
        diff.cache_handoff_delta_summary
            == "cache_records=2->2; changed=0; stable_cache_keys_changed=0; texture_handles_changed=0",
        "unchanged cache handoff delta summary is stable");
    require(
        diff.upload_handoff_delta_summary
            == "upload_records=2->2; changed=0; upload_requests_changed=0; uploaded_bytes=24->24",
        "unchanged upload handoff delta summary is stable");
    require(
        diff.sampler_handoff_delta_summary
            == "sampler_records=2->2; changed=0; sampler_keys_changed=0",
        "unchanged sampler handoff delta summary is stable");
    require(
        diff.diagnostic == "image frame resource materialization diff is unchanged",
        "unchanged materialization diagnostic is stable");
}

void test_materialization_diff_reports_renderer_boundary_handoff_deltas()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_frame_resource_packet_materialization before = materialize_packets({
        make_packet(
            0,
            "asset://textures/card.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready,
            {},
            200,
            5,
            100,
            8,
            1),
        make_packet(1, "asset://textures/stable.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
    });
    const render_image_texture_frame_resource_packet_materialization after = materialize_packets({
        make_packet(
            0,
            "asset://textures/card-hd.ppm",
            render_image_texture_frame_resource_packet_status::resource_packet_ready,
            nearest_sampler,
            300,
            9,
            700,
            20,
            2),
        make_packet(1, "asset://textures/stable.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
    });

    const render_image_texture_frame_resource_materialization_diff diff =
        diff_render_image_texture_frame_resource_materializations(before, after);

    require(diff.ok(), "ready-to-ready materialization handoff diff is ok");
    require(diff.has_changes, "ready-to-ready materialization handoff diff reports changes");
    require(!diff.has_regression, "ready-to-ready materialization handoff diff has no regression");
    require(diff.changed_entry_count == 1, "ready-to-ready materialization handoff diff counts changed entry");
    require(diff.unchanged_entry_count == 1, "ready-to-ready materialization handoff diff counts unchanged entry");
    require(diff.cache_handoff_changed_count == 1, "cache handoff changed count is stable");
    require(diff.upload_handoff_changed_count == 1, "upload handoff changed count is stable");
    require(diff.sampler_handoff_changed_count == 1, "sampler handoff changed count is stable");
    require(diff.stable_texture_cache_key_changed_count == 1, "stable texture cache key change is reported");
    require(diff.texture_handle_changed_count == 1, "texture handle change is reported");
    require(diff.upload_request_changed_count == 1, "upload request change is reported");
    require(diff.upload_generation_changed_count == 1, "upload generation change is reported");
    require(diff.uploaded_byte_changed_count == 1, "uploaded byte change is reported");
    require(diff.mip_level_changed_count == 1, "mip level change is reported");
    require(diff.sampler_changed_count == 1, "sampler policy change is reported");
    require(diff.sampler_key_changed_count == 1, "sampler key change is reported");
    require(diff.uploaded_byte_delta == 12, "aggregate uploaded byte delta is stable");
    require(
        diff.entries[0].cache_delta.after_stable_texture_cache_key.find("asset://textures/card-hd.ppm")
            != std::string::npos,
        "cache delta preserves after stable cache key");
    require(diff.entries[0].upload_delta.after_upload_request_id == 300, "upload delta preserves upload request id");
    require(
        diff.entries[0].sampler_delta.after_sampler_summary.find("min=nearest") != std::string::npos,
        "sampler delta preserves after sampler summary");
}

void test_materialization_diff_reports_blocker_placeholder_regression_and_recovery()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_frame_resource_packet_materialization before = materialize_packets({
        make_packet(0, "asset://textures/card.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(1, "asset://textures/retry.ppm", render_image_texture_frame_resource_packet_status::blocked_retry_backoff),
        make_packet(2, "asset://textures/full.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
    });
    const render_image_texture_frame_resource_packet_materialization after = materialize_packets({
        make_packet(0, "asset://textures/card.ppm", render_image_texture_frame_resource_packet_status::blocked_missing_upload_result),
        make_packet(1, "asset://textures/retry.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(2, "asset://textures/full.ppm", render_image_texture_frame_resource_packet_status::placeholder_backed),
    });

    const render_image_texture_frame_resource_materialization_diff diff =
        diff_render_image_texture_frame_resource_materializations(before, after);

    require(!diff.ok(), "regressed materialization diff is not ok");
    require(diff.has_changes, "regressed materialization diff reports changes");
    require(diff.has_regression, "regressed materialization diff reports regression");
    require(diff.has_recovery, "regressed materialization diff also reports recovery");
    require(diff.changed_entry_count == 3, "regressed materialization diff counts changed entries");
    require(diff.materialization_status_changed_count == 3, "materialization status changes are counted");
    require(diff.materialized_changed_count == 2, "materialized changes are counted");
    require(diff.placeholder_changed_count == 1, "placeholder changes are counted");
    require(diff.blocker_changed_count == 2, "blocker changes are counted");
    require(diff.missing_upload_result_changed_count == 1, "missing upload changes are counted");
    require(diff.retry_backoff_changed_count == 1, "retry backoff changes are counted");
    require(diff.cache_handoff_changed_count == 3, "cache handoff record addition/removal changes are counted");
    require(diff.upload_handoff_changed_count == 3, "upload handoff record addition/removal changes are counted");
    require(diff.sampler_handoff_changed_count == 3, "sampler handoff record addition/removal changes are counted");
    require(diff.placeholder_packet_delta == 1, "placeholder aggregate delta is stable");
    require(diff.missing_upload_result_delta == 1, "missing upload aggregate delta is stable");
    require(diff.retry_backoff_blocked_delta == -1, "retry backoff aggregate delta is stable");
    require(
        diff.regression_summary.find("placeholder packets increased") != std::string::npos,
        "regression summary reports placeholder growth");
    require(
        diff.regression_summary.find("missing upload results increased") != std::string::npos,
        "regression summary reports missing upload growth");
}

void test_materialization_diff_reports_added_and_removed_entries()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_frame_resource_packet_materialization before = materialize_packets({
        make_packet(0, "asset://textures/stable.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(1, "asset://textures/removed.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
    });
    const render_image_texture_frame_resource_packet_materialization after = materialize_packets({
        make_packet(0, "asset://textures/stable.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
        make_packet(2, "asset://textures/added.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
    });

    const render_image_texture_frame_resource_materialization_diff diff =
        diff_render_image_texture_frame_resource_materializations(before, after);

    require(!diff.ok(), "removed ready materialization diff is not ok");
    require(diff.has_changes, "added and removed materialization diff reports changes");
    require(diff.has_regression, "removed ready materialization entry is a regression");
    require(diff.added_entry_count == 1, "added materialization entry is counted");
    require(diff.removed_entry_count == 1, "removed materialization entry is counted");
    require(diff.unchanged_entry_count == 1, "unchanged materialization entry is counted");
    require(diff.entries.size() == 3, "added and removed materialization diff includes request union");
    require(diff.entries[0].request_index == 0, "request union keeps stable first index");
    require(diff.entries[1].request_index == 1, "request union keeps removed index");
    require(diff.entries[2].request_index == 2, "request union keeps added index");
    require(diff.entries[1].status == render_image_texture_frame_resource_materialization_diff_entry_status::removed,
        "removed materialization entry status is stable");
    require(diff.entries[2].status == render_image_texture_frame_resource_materialization_diff_entry_status::added,
        "added materialization entry status is stable");
    require(diff.changed_summary == "added=1; removed=1", "added and removed summary is stable");
}

void test_materialization_diff_helpers_are_stable()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_frame_resource_packet_materialization materialization = materialize_packets({
        make_packet(3, "asset://textures/card.ppm", render_image_texture_frame_resource_packet_status::resource_packet_ready),
    });

    std::map<std::size_t, bool> request_indexes;
    append_render_image_texture_frame_resource_materialization_request_indexes(request_indexes, materialization);

    require(
        render_image_texture_frame_resource_materialization_diff_entry_status_name(
            render_image_texture_frame_resource_materialization_diff_entry_status::changed)
            == "changed",
        "materialization diff status name is stable");
    require(
        render_image_texture_frame_resource_materialization_entry_for_request_index(materialization, 3)
            == &materialization.entries[0],
        "materialization entry lookup is stable");
    require(request_indexes.contains(3), "materialization request index append helper is stable");
    require(
        render_image_texture_frame_resource_materialization_uploaded_byte_count(materialization) == 8,
        "materialization uploaded byte helper is stable");
    require(
        render_image_texture_frame_resource_cache_handoff_record_equal(
            materialization.entries[0].cache_record,
            materialization.entries[0].cache_record),
        "cache handoff equality helper is stable");
    require(
        render_image_texture_frame_resource_upload_handoff_record_equal(
            materialization.entries[0].upload_record,
            materialization.entries[0].upload_record),
        "upload handoff equality helper is stable");
    require(
        render_image_texture_frame_resource_sampler_handoff_record_equal(
            materialization.entries[0].sampler_record,
            materialization.entries[0].sampler_record),
        "sampler handoff equality helper is stable");
    require(
        render_image_texture_frame_resource_materialization_entry_equal(
            materialization.entries[0],
            materialization.entries[0]),
        "materialization entry equality helper is stable");
}

} // namespace

int main()
{
    test_unchanged_materialization_diff_is_stable();
    test_materialization_diff_reports_renderer_boundary_handoff_deltas();
    test_materialization_diff_reports_blocker_placeholder_regression_and_recovery();
    test_materialization_diff_reports_added_and_removed_entries();
    test_materialization_diff_helpers_are_stable();
    return 0;
}
