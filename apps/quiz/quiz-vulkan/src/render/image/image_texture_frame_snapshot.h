#pragma once

#include "render/image/image_texture_pipeline.h"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_image_texture_frame_snapshot_status {
    ready,
    partial,
    empty,
};

inline std::string render_image_texture_frame_snapshot_status_name(
    render_image_texture_frame_snapshot_status status)
{
    switch (status) {
    case render_image_texture_frame_snapshot_status::ready:
        return "ready";
    case render_image_texture_frame_snapshot_status::partial:
        return "partial";
    case render_image_texture_frame_snapshot_status::empty:
        return "empty";
    }

    return "unknown";
}

struct render_image_texture_frame_entry_snapshot {
    std::size_t sequence = 0;
    std::size_t request_index = 0;
    render_image_texture_batch_plan_entry_status plan_status =
        render_image_texture_batch_plan_entry_status::resolve_failed;
    render_image_texture_batch_execution_entry_status execution_status =
        render_image_texture_batch_execution_entry_status::skipped_invalid_request;
    render_image_texture_pipeline_status pipeline_status =
        render_image_texture_pipeline_status::resolve_failed;
    render_image_source_bytes_load_status source_bytes_status =
        render_image_source_bytes_load_status::missing_source;
    render_image_texture_status texture_status = render_image_texture_status::missing_source;
    render_image_texture_handle_map_entry_status handle_status =
        render_image_texture_handle_map_entry_status::missing_execution;
    std::string render_image_uri;
    std::string normalized_uri;
    render_image_cache_key cache_key;
    render_image_source_kind source_kind = render_image_source_kind::unsupported;
    render_image_sampler_policy sampler;
    render_image_sampler_policy_diagnostic sampler_policy;
    render_image_texture_key texture_key;
    std::string stable_texture_cache_key;
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::size_t texture_width = 0;
    std::size_t texture_height = 0;
    bool planned = false;
    bool executed = false;
    bool ready = false;
    bool mapped = false;
    bool renderer_handoff_ready = false;
    bool placeholder_texture = false;
    bool cache_reused = false;
    bool expected_cache_reuse = false;
    bool residency_budget_pressure = false;
    render_image_texture_residency_budget_pressure_status residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    std::string residency_pressure_status_name;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_handoff_ready;
    }
};

struct render_image_texture_frame_snapshot {
    render_image_texture_frame_snapshot_status status =
        render_image_texture_frame_snapshot_status::empty;
    std::string status_name;
    std::size_t request_count = 0;
    std::size_t planned_request_count = 0;
    std::size_t invalid_request_count = 0;
    std::size_t executed_request_count = 0;
    std::size_t skipped_request_count = 0;
    std::size_t ready_count = 0;
    std::size_t failure_count = 0;
    std::size_t mapped_texture_count = 0;
    std::size_t missing_texture_count = 0;
    std::size_t placeholder_texture_count = 0;
    std::size_t cache_reused_count = 0;
    std::size_t unique_source_key_count = 0;
    std::size_t unique_texture_cache_key_count = 0;
    std::size_t unique_texture_id_count = 0;
    std::size_t unique_resident_texture_count = 0;
    std::size_t unique_resident_pixel_count = 0;
    std::size_t unique_resident_rgba8_byte_count = 0;
    std::size_t eviction_candidate_count = 0;
    std::size_t retry_candidate_count = 0;
    bool plan_ready = false;
    bool execution_ready = false;
    bool handle_map_ready = false;
    bool renderer_handoff_ready = false;
    bool residency_budget_diagnostics_available = false;
    bool residency_budget_pressure = false;
    bool pixel_budget_pressure = false;
    bool texture_budget_pressure = false;
    render_image_texture_residency_budget_pressure_status residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    std::string residency_pressure_status_name;
    std::vector<render_image_texture_frame_entry_snapshot> entries;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_texture_frame_snapshot_status::ready;
    }
};

inline const render_image_texture_batch_execution_entry* render_image_texture_batch_execution_entry_for_request_index(
    const render_image_texture_batch_execution_diagnostics& execution,
    std::size_t request_index)
{
    for (const render_image_texture_batch_execution_entry& entry : execution.entries) {
        if (entry.request_index == request_index) {
            return &entry;
        }
    }
    return nullptr;
}

inline render_image_texture_frame_snapshot_status render_image_texture_frame_snapshot_status_for(
    const render_image_texture_batch_plan& plan,
    const render_image_texture_batch_execution_diagnostics& execution,
    const render_image_texture_handle_map_diagnostics& handle_map)
{
    if (plan.request_count == 0) {
        return render_image_texture_frame_snapshot_status::empty;
    }
    if (plan.ok() && execution.ok() && handle_map.ok()) {
        return render_image_texture_frame_snapshot_status::ready;
    }
    return render_image_texture_frame_snapshot_status::partial;
}

inline render_image_texture_frame_snapshot make_render_image_texture_frame_snapshot(
    const render_image_texture_batch_plan& plan,
    const render_image_texture_batch_execution_diagnostics& execution,
    const render_image_texture_handle_map_diagnostics& handle_map)
{
    render_image_texture_frame_snapshot snapshot{
        .status = render_image_texture_frame_snapshot_status_for(plan, execution, handle_map),
        .request_count = plan.request_count,
        .planned_request_count = plan.planned_request_count,
        .invalid_request_count = plan.invalid_request_count,
        .executed_request_count = execution.executed_request_count,
        .skipped_request_count = execution.skipped_request_count,
        .ready_count = execution.ready_count,
        .failure_count = execution.failure_count,
        .mapped_texture_count = handle_map.mapped_count,
        .missing_texture_count = handle_map.missing_count,
        .placeholder_texture_count = handle_map.placeholder_texture_count,
        .cache_reused_count = handle_map.cache_reused_count,
        .unique_source_key_count = plan.unique_source_key_count,
        .unique_texture_cache_key_count = plan.unique_texture_key_count,
        .unique_texture_id_count = handle_map.unique_texture_id_count,
        .unique_resident_texture_count = execution.residency_budget.unique_resident_texture_count,
        .unique_resident_pixel_count = execution.residency_budget.unique_resident_pixel_count,
        .unique_resident_rgba8_byte_count = execution.residency_budget.unique_resident_rgba8_byte_count,
        .eviction_candidate_count = execution.residency_budget.eviction_candidate_count,
        .retry_candidate_count = execution.residency_budget.retry_candidate_count,
        .plan_ready = plan.ok(),
        .execution_ready = execution.ok(),
        .handle_map_ready = handle_map.ok(),
        .renderer_handoff_ready = handle_map.renderer_handoff_ready,
        .residency_budget_diagnostics_available = execution.residency_budget_diagnostics_available,
        .residency_budget_pressure = execution.residency_budget.budget_pressure,
        .pixel_budget_pressure = execution.residency_budget.pixel_budget_pressure,
        .texture_budget_pressure = execution.residency_budget.texture_budget_pressure,
        .residency_pressure_status = execution.residency_budget.pressure_status,
        .residency_pressure_status_name = execution.residency_budget.pressure_status_name,
    };
    snapshot.status_name = render_image_texture_frame_snapshot_status_name(snapshot.status);

    for (const render_image_texture_handle_map_entry& handle_entry : handle_map.entries) {
        const render_image_texture_batch_plan_entry* plan_entry =
            render_image_texture_batch_plan_entry_for_request_index(plan, handle_entry.request_index);
        const render_image_texture_batch_execution_entry* execution_entry =
            render_image_texture_batch_execution_entry_for_request_index(execution, handle_entry.request_index);
        render_image_texture_frame_entry_snapshot entry{
            .sequence = handle_entry.sequence,
            .request_index = handle_entry.request_index,
            .plan_status = handle_entry.plan_status,
            .execution_status = handle_entry.execution_status,
            .pipeline_status = handle_entry.pipeline_status,
            .source_bytes_status = execution_entry == nullptr
                ? render_image_source_bytes_load_status::missing_source
                : execution_entry->source_bytes_status,
            .texture_status = execution_entry == nullptr
                ? render_image_texture_status::missing_source
                : execution_entry->texture_status,
            .handle_status = handle_entry.status,
            .render_image_uri = handle_entry.render_image_uri,
            .normalized_uri = handle_entry.normalized_uri,
            .cache_key = handle_entry.cache_key,
            .source_kind = handle_entry.source_kind,
            .sampler = handle_entry.sampler,
            .sampler_policy = handle_entry.sampler_policy,
            .texture_key = handle_entry.texture_key,
            .stable_texture_cache_key = handle_entry.stable_texture_cache_key,
            .texture_id = handle_entry.texture_id,
            .texture_revision = handle_entry.texture_revision,
            .texture_width = handle_entry.texture_width,
            .texture_height = handle_entry.texture_height,
            .planned = plan_entry != nullptr && plan_entry->ok(),
            .executed = execution_entry != nullptr && execution_entry->executed,
            .ready = handle_entry.ready,
            .mapped = handle_entry.mapped,
            .renderer_handoff_ready = handle_entry.ok(),
            .placeholder_texture = handle_entry.placeholder_texture,
            .cache_reused = handle_entry.cache_reused,
            .expected_cache_reuse = handle_entry.expected_cache_reuse,
            .residency_budget_pressure = handle_entry.residency_budget_pressure,
            .residency_pressure_status = handle_entry.residency_pressure_status,
            .residency_pressure_status_name = handle_entry.residency_pressure_status_name,
            .diagnostic = handle_entry.diagnostic,
        };
        if (entry.diagnostic.empty()) {
            entry.diagnostic = entry.renderer_handoff_ready
                ? "image texture frame entry is ready for renderer handoff"
                : "image texture frame entry is unavailable for renderer handoff";
        }
        snapshot.entries.push_back(std::move(entry));
    }

    snapshot.diagnostic = snapshot.status == render_image_texture_frame_snapshot_status::ready
        ? (snapshot.residency_budget_pressure
            ? "image texture frame snapshot ready with residency budget pressure"
            : "image texture frame snapshot ready")
        : (snapshot.status == render_image_texture_frame_snapshot_status::empty
            ? "image texture frame snapshot has no image requests"
            : "image texture frame snapshot is partial");
    return snapshot;
}

inline render_image_texture_frame_snapshot make_render_image_texture_frame_snapshot(
    const render_image_texture_batch_plan& plan,
    const render_image_texture_batch_execution_diagnostics& execution)
{
    return make_render_image_texture_frame_snapshot(
        plan,
        execution,
        make_render_image_texture_handle_map_diagnostics(plan, execution));
}

enum class render_image_texture_frame_snapshot_diff_entry_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string render_image_texture_frame_snapshot_diff_entry_status_name(
    render_image_texture_frame_snapshot_diff_entry_status status)
{
    switch (status) {
    case render_image_texture_frame_snapshot_diff_entry_status::unchanged:
        return "unchanged";
    case render_image_texture_frame_snapshot_diff_entry_status::added:
        return "added";
    case render_image_texture_frame_snapshot_diff_entry_status::removed:
        return "removed";
    case render_image_texture_frame_snapshot_diff_entry_status::changed:
        return "changed";
    }

    return "unknown";
}

struct render_image_texture_frame_entry_diff {
    std::size_t request_index = 0;
    render_image_texture_frame_snapshot_diff_entry_status status =
        render_image_texture_frame_snapshot_diff_entry_status::unchanged;
    std::string status_name;
    std::string before_render_image_uri;
    std::string after_render_image_uri;
    render_image_cache_key before_cache_key;
    render_image_cache_key after_cache_key;
    render_image_sampler_policy before_sampler;
    render_image_sampler_policy after_sampler;
    std::string before_stable_texture_cache_key;
    std::string after_stable_texture_cache_key;
    render_image_texture_id before_texture_id = 0;
    render_image_texture_id after_texture_id = 0;
    render_image_revision before_texture_revision = 0;
    render_image_revision after_texture_revision = 0;
    bool before_ready = false;
    bool after_ready = false;
    bool before_renderer_handoff_ready = false;
    bool after_renderer_handoff_ready = false;
    bool before_placeholder_texture = false;
    bool after_placeholder_texture = false;
    bool before_residency_budget_pressure = false;
    bool after_residency_budget_pressure = false;
    render_image_texture_batch_execution_entry_status before_execution_status =
        render_image_texture_batch_execution_entry_status::skipped_invalid_request;
    render_image_texture_batch_execution_entry_status after_execution_status =
        render_image_texture_batch_execution_entry_status::skipped_invalid_request;
    render_image_texture_residency_budget_pressure_status before_residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    render_image_texture_residency_budget_pressure_status after_residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    bool texture_handle_added = false;
    bool texture_handle_removed = false;
    bool texture_handle_changed = false;
    bool cache_key_changed = false;
    bool sampler_changed = false;
    bool stable_texture_cache_key_changed = false;
    bool placeholder_changed = false;
    bool request_success_changed = false;
    bool failure_status_changed = false;
    bool residency_pressure_changed = false;
    bool regression = false;
    std::string diagnostic;

    bool changed() const
    {
        return status != render_image_texture_frame_snapshot_diff_entry_status::unchanged;
    }

    bool ok() const
    {
        return !regression;
    }
};

struct render_image_texture_frame_snapshot_diff {
    std::size_t before_request_count = 0;
    std::size_t after_request_count = 0;
    std::size_t unchanged_entry_count = 0;
    std::size_t added_entry_count = 0;
    std::size_t removed_entry_count = 0;
    std::size_t changed_entry_count = 0;
    std::size_t texture_handle_added_count = 0;
    std::size_t texture_handle_removed_count = 0;
    std::size_t texture_handle_changed_count = 0;
    std::size_t cache_key_changed_count = 0;
    std::size_t sampler_changed_count = 0;
    std::size_t placeholder_delta_count = 0;
    std::size_t failure_delta_count = 0;
    std::size_t request_success_delta_count = 0;
    std::size_t residency_pressure_delta_count = 0;
    std::size_t before_ready_count = 0;
    std::size_t after_ready_count = 0;
    std::size_t before_failure_count = 0;
    std::size_t after_failure_count = 0;
    std::size_t before_placeholder_texture_count = 0;
    std::size_t after_placeholder_texture_count = 0;
    std::size_t before_missing_texture_count = 0;
    std::size_t after_missing_texture_count = 0;
    bool before_renderer_handoff_ready = false;
    bool after_renderer_handoff_ready = false;
    bool before_residency_budget_pressure = false;
    bool after_residency_budget_pressure = false;
    render_image_texture_residency_budget_pressure_status before_residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    render_image_texture_residency_budget_pressure_status after_residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    bool renderer_handoff_regressed = false;
    bool renderer_handoff_recovered = false;
    bool request_success_regressed = false;
    bool request_success_recovered = false;
    bool failure_count_regressed = false;
    bool failure_count_recovered = false;
    bool missing_texture_regressed = false;
    bool missing_texture_recovered = false;
    bool placeholder_regressed = false;
    bool placeholder_recovered = false;
    bool residency_pressure_regressed = false;
    bool residency_pressure_recovered = false;
    bool has_changes = false;
    bool has_regression = false;
    std::string regression_summary;
    std::vector<render_image_texture_frame_entry_diff> entries;
    std::string diagnostic;

    bool ok() const
    {
        return !has_regression;
    }
};

inline const render_image_texture_frame_entry_snapshot* render_image_texture_frame_entry_for_request_index(
    const render_image_texture_frame_snapshot& snapshot,
    std::size_t request_index)
{
    for (const render_image_texture_frame_entry_snapshot& entry : snapshot.entries) {
        if (entry.request_index == request_index) {
            return &entry;
        }
    }
    return nullptr;
}

inline std::size_t render_image_texture_frame_pressure_rank(
    render_image_texture_residency_budget_pressure_status status)
{
    switch (status) {
    case render_image_texture_residency_budget_pressure_status::within_budget:
        return 0;
    case render_image_texture_residency_budget_pressure_status::over_pixel_budget:
    case render_image_texture_residency_budget_pressure_status::over_texture_budget:
        return 1;
    case render_image_texture_residency_budget_pressure_status::over_pixel_and_texture_budget:
        return 2;
    }

    return 0;
}

inline void append_render_image_texture_frame_regression_reason(
    std::string& summary,
    std::string_view reason)
{
    if (!summary.empty()) {
        summary += "; ";
    }
    summary += reason;
}

inline render_image_texture_frame_entry_diff make_render_image_texture_frame_entry_diff(
    const render_image_texture_frame_entry_snapshot* before_entry,
    const render_image_texture_frame_entry_snapshot* after_entry,
    std::size_t request_index)
{
    render_image_texture_frame_entry_diff diff{
        .request_index = request_index,
    };

    if (before_entry != nullptr) {
        diff.before_render_image_uri = before_entry->render_image_uri;
        diff.before_cache_key = before_entry->cache_key;
        diff.before_sampler = before_entry->sampler;
        diff.before_stable_texture_cache_key = before_entry->stable_texture_cache_key;
        diff.before_texture_id = before_entry->texture_id;
        diff.before_texture_revision = before_entry->texture_revision;
        diff.before_ready = before_entry->ready;
        diff.before_renderer_handoff_ready = before_entry->renderer_handoff_ready;
        diff.before_placeholder_texture = before_entry->placeholder_texture;
        diff.before_residency_budget_pressure = before_entry->residency_budget_pressure;
        diff.before_execution_status = before_entry->execution_status;
        diff.before_residency_pressure_status = before_entry->residency_pressure_status;
    }

    if (after_entry != nullptr) {
        diff.after_render_image_uri = after_entry->render_image_uri;
        diff.after_cache_key = after_entry->cache_key;
        diff.after_sampler = after_entry->sampler;
        diff.after_stable_texture_cache_key = after_entry->stable_texture_cache_key;
        diff.after_texture_id = after_entry->texture_id;
        diff.after_texture_revision = after_entry->texture_revision;
        diff.after_ready = after_entry->ready;
        diff.after_renderer_handoff_ready = after_entry->renderer_handoff_ready;
        diff.after_placeholder_texture = after_entry->placeholder_texture;
        diff.after_residency_budget_pressure = after_entry->residency_budget_pressure;
        diff.after_execution_status = after_entry->execution_status;
        diff.after_residency_pressure_status = after_entry->residency_pressure_status;
    }

    if (before_entry == nullptr && after_entry != nullptr) {
        diff.status = render_image_texture_frame_snapshot_diff_entry_status::added;
        diff.texture_handle_added = after_entry->texture_id != 0;
        diff.placeholder_changed = after_entry->placeholder_texture;
        diff.request_success_changed = after_entry->renderer_handoff_ready;
        diff.residency_pressure_changed = after_entry->residency_budget_pressure;
        diff.regression = after_entry->placeholder_texture || after_entry->residency_budget_pressure
            || !after_entry->renderer_handoff_ready;
    } else if (before_entry != nullptr && after_entry == nullptr) {
        diff.status = render_image_texture_frame_snapshot_diff_entry_status::removed;
        diff.texture_handle_removed = before_entry->texture_id != 0;
        diff.placeholder_changed = before_entry->placeholder_texture;
        diff.request_success_changed = before_entry->renderer_handoff_ready;
        diff.residency_pressure_changed = before_entry->residency_budget_pressure;
        diff.regression = before_entry->renderer_handoff_ready;
    } else if (before_entry != nullptr && after_entry != nullptr) {
        diff.texture_handle_added = before_entry->texture_id == 0 && after_entry->texture_id != 0;
        diff.texture_handle_removed = before_entry->texture_id != 0 && after_entry->texture_id == 0;
        diff.texture_handle_changed = before_entry->texture_id != 0 && after_entry->texture_id != 0
            && (before_entry->texture_id != after_entry->texture_id
                || before_entry->texture_revision != after_entry->texture_revision);
        diff.cache_key_changed = before_entry->cache_key != after_entry->cache_key;
        diff.sampler_changed = !(before_entry->sampler == after_entry->sampler);
        diff.stable_texture_cache_key_changed =
            before_entry->stable_texture_cache_key != after_entry->stable_texture_cache_key;
        diff.placeholder_changed = before_entry->placeholder_texture != after_entry->placeholder_texture;
        diff.request_success_changed =
            before_entry->renderer_handoff_ready != after_entry->renderer_handoff_ready;
        diff.failure_status_changed = before_entry->execution_status != after_entry->execution_status;
        diff.residency_pressure_changed =
            before_entry->residency_pressure_status != after_entry->residency_pressure_status;
        diff.regression = (before_entry->renderer_handoff_ready && !after_entry->renderer_handoff_ready)
            || (!before_entry->placeholder_texture && after_entry->placeholder_texture)
            || render_image_texture_frame_pressure_rank(after_entry->residency_pressure_status)
                > render_image_texture_frame_pressure_rank(before_entry->residency_pressure_status);
        if (diff.texture_handle_added || diff.texture_handle_removed || diff.texture_handle_changed
            || diff.cache_key_changed || diff.sampler_changed || diff.stable_texture_cache_key_changed
            || diff.placeholder_changed || diff.request_success_changed || diff.failure_status_changed
            || diff.residency_pressure_changed) {
            diff.status = render_image_texture_frame_snapshot_diff_entry_status::changed;
        }
    }

    diff.status_name = render_image_texture_frame_snapshot_diff_entry_status_name(diff.status);
    diff.diagnostic = diff.status == render_image_texture_frame_snapshot_diff_entry_status::unchanged
        ? "image texture frame entry unchanged"
        : (diff.regression
            ? "image texture frame entry changed with regression"
            : "image texture frame entry changed");
    return diff;
}

inline render_image_texture_frame_snapshot_diff diff_render_image_texture_frame_snapshots(
    const render_image_texture_frame_snapshot& before,
    const render_image_texture_frame_snapshot& after)
{
    render_image_texture_frame_snapshot_diff diff{
        .before_request_count = before.request_count,
        .after_request_count = after.request_count,
        .before_ready_count = before.ready_count,
        .after_ready_count = after.ready_count,
        .before_failure_count = before.failure_count,
        .after_failure_count = after.failure_count,
        .before_placeholder_texture_count = before.placeholder_texture_count,
        .after_placeholder_texture_count = after.placeholder_texture_count,
        .before_missing_texture_count = before.missing_texture_count,
        .after_missing_texture_count = after.missing_texture_count,
        .before_renderer_handoff_ready = before.renderer_handoff_ready,
        .after_renderer_handoff_ready = after.renderer_handoff_ready,
        .before_residency_budget_pressure = before.residency_budget_pressure,
        .after_residency_budget_pressure = after.residency_budget_pressure,
        .before_residency_pressure_status = before.residency_pressure_status,
        .after_residency_pressure_status = after.residency_pressure_status,
    };

    std::map<std::size_t, bool> request_indices;
    for (const render_image_texture_frame_entry_snapshot& entry : before.entries) {
        request_indices.emplace(entry.request_index, true);
    }
    for (const render_image_texture_frame_entry_snapshot& entry : after.entries) {
        request_indices.emplace(entry.request_index, true);
    }

    for (const auto& [request_index, _] : request_indices) {
        const render_image_texture_frame_entry_snapshot* before_entry =
            render_image_texture_frame_entry_for_request_index(before, request_index);
        const render_image_texture_frame_entry_snapshot* after_entry =
            render_image_texture_frame_entry_for_request_index(after, request_index);
        render_image_texture_frame_entry_diff entry_diff =
            make_render_image_texture_frame_entry_diff(before_entry, after_entry, request_index);

        switch (entry_diff.status) {
        case render_image_texture_frame_snapshot_diff_entry_status::unchanged:
            ++diff.unchanged_entry_count;
            break;
        case render_image_texture_frame_snapshot_diff_entry_status::added:
            ++diff.added_entry_count;
            break;
        case render_image_texture_frame_snapshot_diff_entry_status::removed:
            ++diff.removed_entry_count;
            break;
        case render_image_texture_frame_snapshot_diff_entry_status::changed:
            ++diff.changed_entry_count;
            break;
        }

        if (entry_diff.texture_handle_added) {
            ++diff.texture_handle_added_count;
        }
        if (entry_diff.texture_handle_removed) {
            ++diff.texture_handle_removed_count;
        }
        if (entry_diff.texture_handle_changed) {
            ++diff.texture_handle_changed_count;
        }
        if (entry_diff.cache_key_changed) {
            ++diff.cache_key_changed_count;
        }
        if (entry_diff.sampler_changed) {
            ++diff.sampler_changed_count;
        }
        if (entry_diff.placeholder_changed) {
            ++diff.placeholder_delta_count;
        }
        if (entry_diff.failure_status_changed) {
            ++diff.failure_delta_count;
        }
        if (entry_diff.request_success_changed) {
            ++diff.request_success_delta_count;
        }
        if (entry_diff.residency_pressure_changed) {
            ++diff.residency_pressure_delta_count;
        }
        if (entry_diff.regression) {
            diff.has_regression = true;
        }

        diff.entries.push_back(std::move(entry_diff));
    }

    diff.renderer_handoff_regressed = before.renderer_handoff_ready && !after.renderer_handoff_ready;
    diff.renderer_handoff_recovered = !before.renderer_handoff_ready && after.renderer_handoff_ready;
    diff.request_success_regressed = after.ready_count < before.ready_count;
    diff.request_success_recovered = after.ready_count > before.ready_count;
    diff.failure_count_regressed = after.failure_count > before.failure_count;
    diff.failure_count_recovered = after.failure_count < before.failure_count;
    diff.missing_texture_regressed = after.missing_texture_count > before.missing_texture_count;
    diff.missing_texture_recovered = after.missing_texture_count < before.missing_texture_count;
    diff.placeholder_regressed = after.placeholder_texture_count > before.placeholder_texture_count;
    diff.placeholder_recovered = after.placeholder_texture_count < before.placeholder_texture_count;
    diff.residency_pressure_regressed =
        render_image_texture_frame_pressure_rank(after.residency_pressure_status)
        > render_image_texture_frame_pressure_rank(before.residency_pressure_status);
    diff.residency_pressure_recovered =
        render_image_texture_frame_pressure_rank(after.residency_pressure_status)
        < render_image_texture_frame_pressure_rank(before.residency_pressure_status);
    diff.has_changes = diff.added_entry_count != 0 || diff.removed_entry_count != 0
        || diff.changed_entry_count != 0 || before.status != after.status
        || before.ready_count != after.ready_count || before.failure_count != after.failure_count
        || before.placeholder_texture_count != after.placeholder_texture_count
        || before.missing_texture_count != after.missing_texture_count
        || before.residency_pressure_status != after.residency_pressure_status;
    diff.has_regression = diff.has_regression || diff.renderer_handoff_regressed
        || diff.request_success_regressed || diff.failure_count_regressed
        || diff.missing_texture_regressed || diff.placeholder_regressed
        || diff.residency_pressure_regressed;

    if (diff.renderer_handoff_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "renderer handoff regressed");
    }
    if (diff.request_success_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "ready requests decreased");
    }
    if (diff.failure_count_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "failures increased");
    }
    if (diff.missing_texture_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "missing textures increased");
    }
    if (diff.placeholder_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "placeholder textures increased");
    }
    if (diff.residency_pressure_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "residency pressure increased");
    }
    if (diff.has_regression && diff.regression_summary.empty()) {
        diff.regression_summary = "request entries regressed";
    }
    if (diff.regression_summary.empty()) {
        diff.regression_summary = diff.has_changes
            ? "image texture frame snapshot diff has changes without regressions"
            : "image texture frame snapshot diff has no changes";
    }

    diff.diagnostic = diff.has_regression
        ? "image texture frame snapshot diff reports regressions"
        : (diff.has_changes
            ? "image texture frame snapshot diff reports changes"
            : "image texture frame snapshot diff is unchanged");
    return diff;
}

enum class render_image_texture_frame_binding_packet_status {
    ready,
    placeholder,
    failed,
    removed,
};

inline std::string render_image_texture_frame_binding_packet_status_name(
    render_image_texture_frame_binding_packet_status status)
{
    switch (status) {
    case render_image_texture_frame_binding_packet_status::ready:
        return "ready";
    case render_image_texture_frame_binding_packet_status::placeholder:
        return "placeholder";
    case render_image_texture_frame_binding_packet_status::failed:
        return "failed";
    case render_image_texture_frame_binding_packet_status::removed:
        return "removed";
    }

    return "unknown";
}

struct render_image_texture_frame_binding_packet {
    std::size_t sequence = 0;
    std::size_t request_index = 0;
    render_image_texture_frame_binding_packet_status status =
        render_image_texture_frame_binding_packet_status::failed;
    std::string status_name;
    std::string render_image_uri;
    std::string normalized_uri;
    render_image_cache_key cache_key;
    render_image_source_kind source_kind = render_image_source_kind::unsupported;
    render_image_sampler_policy sampler;
    render_image_sampler_policy_diagnostic sampler_policy;
    std::string sampler_key;
    render_image_texture_key texture_key;
    std::string stable_texture_cache_key;
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::size_t texture_width = 0;
    std::size_t texture_height = 0;
    render_image_texture_batch_execution_entry_status execution_status =
        render_image_texture_batch_execution_entry_status::skipped_invalid_request;
    render_image_texture_pipeline_status pipeline_status =
        render_image_texture_pipeline_status::resolve_failed;
    render_image_texture_status texture_status = render_image_texture_status::missing_source;
    render_image_texture_handle_map_entry_status handle_status =
        render_image_texture_handle_map_entry_status::missing_execution;
    bool has_texture = false;
    bool renderer_handoff_ready = false;
    bool placeholder_texture = false;
    bool failed = true;
    bool removed = false;
    bool cache_reused = false;
    bool expected_cache_reuse = false;
    bool residency_budget_pressure = false;
    render_image_texture_residency_budget_pressure_status residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    std::string residency_pressure_status_name;
    bool added_in_frame = false;
    bool removed_from_frame = false;
    bool changed_in_frame = false;
    bool unchanged_in_frame = false;
    bool readiness_changed = false;
    bool readiness_regressed = false;
    bool readiness_recovered = false;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_texture_frame_binding_packet_status::ready
            || status == render_image_texture_frame_binding_packet_status::placeholder;
    }
};

struct render_image_texture_frame_binding_plan {
    std::size_t request_count = 0;
    std::size_t current_packet_count = 0;
    std::size_t packet_count = 0;
    std::size_t bindable_packet_count = 0;
    std::size_t ready_packet_count = 0;
    std::size_t placeholder_packet_count = 0;
    std::size_t failed_packet_count = 0;
    std::size_t removed_packet_count = 0;
    std::size_t added_packet_count = 0;
    std::size_t changed_packet_count = 0;
    std::size_t unchanged_packet_count = 0;
    std::size_t readiness_changed_count = 0;
    std::size_t readiness_regressed_count = 0;
    std::size_t readiness_recovered_count = 0;
    std::size_t residency_pressure_packet_count = 0;
    bool renderer_handoff_ready = false;
    bool residency_budget_pressure = false;
    render_image_texture_residency_budget_pressure_status residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    std::string residency_pressure_status_name;
    std::vector<render_image_texture_frame_binding_packet> packets;
    std::string readiness_summary;
    std::string diagnostic;

    bool ok() const
    {
        return renderer_handoff_ready;
    }
};

inline render_image_texture_frame_binding_packet_status render_image_texture_frame_binding_packet_status_for(
    const render_image_texture_frame_entry_snapshot& entry)
{
    if (!entry.renderer_handoff_ready || entry.texture_id == 0) {
        return render_image_texture_frame_binding_packet_status::failed;
    }
    if (entry.placeholder_texture) {
        return render_image_texture_frame_binding_packet_status::placeholder;
    }
    return render_image_texture_frame_binding_packet_status::ready;
}

inline const render_image_texture_frame_entry_diff* render_image_texture_frame_diff_entry_for_request_index(
    const render_image_texture_frame_snapshot_diff& diff,
    std::size_t request_index)
{
    for (const render_image_texture_frame_entry_diff& entry : diff.entries) {
        if (entry.request_index == request_index) {
            return &entry;
        }
    }
    return nullptr;
}

inline render_image_texture_frame_binding_packet make_render_image_texture_frame_binding_packet(
    const render_image_texture_frame_entry_snapshot& entry)
{
    render_image_texture_frame_binding_packet packet{
        .sequence = entry.sequence,
        .request_index = entry.request_index,
        .status = render_image_texture_frame_binding_packet_status_for(entry),
        .render_image_uri = entry.render_image_uri,
        .normalized_uri = entry.normalized_uri,
        .cache_key = entry.cache_key,
        .source_kind = entry.source_kind,
        .sampler = entry.sampler,
        .sampler_policy = entry.sampler_policy,
        .texture_key = entry.texture_key,
        .stable_texture_cache_key = entry.stable_texture_cache_key,
        .texture_id = entry.texture_id,
        .texture_revision = entry.texture_revision,
        .texture_width = entry.texture_width,
        .texture_height = entry.texture_height,
        .execution_status = entry.execution_status,
        .pipeline_status = entry.pipeline_status,
        .texture_status = entry.texture_status,
        .handle_status = entry.handle_status,
        .has_texture = entry.texture_id != 0,
        .renderer_handoff_ready = entry.renderer_handoff_ready,
        .placeholder_texture = entry.placeholder_texture,
        .failed = !entry.renderer_handoff_ready || entry.texture_id == 0,
        .removed = false,
        .cache_reused = entry.cache_reused,
        .expected_cache_reuse = entry.expected_cache_reuse,
        .residency_budget_pressure = entry.residency_budget_pressure,
        .residency_pressure_status = entry.residency_pressure_status,
        .residency_pressure_status_name = entry.residency_pressure_status_name,
        .diagnostic = entry.diagnostic,
    };
    packet.status_name = render_image_texture_frame_binding_packet_status_name(packet.status);
    packet.sampler_key = packet.sampler_policy.stable_key_fragment.empty()
        ? render_image_sampler_policy_stable_fragment(packet.sampler)
        : packet.sampler_policy.stable_key_fragment;
    if (packet.diagnostic.empty()) {
        packet.diagnostic = packet.status == render_image_texture_frame_binding_packet_status::ready
            ? "image texture binding packet is ready"
            : (packet.status == render_image_texture_frame_binding_packet_status::placeholder
                ? "image texture binding packet uses placeholder texture"
                : "image texture binding packet is unavailable for renderer handoff");
    }
    return packet;
}

inline render_image_texture_frame_binding_packet make_render_image_texture_frame_removed_binding_packet(
    const render_image_texture_frame_entry_diff& diff)
{
    render_image_sampler_policy_diagnostic sampler_policy =
        make_render_image_sampler_policy_diagnostic(diff.before_sampler);
    render_image_texture_frame_binding_packet packet{
        .request_index = diff.request_index,
        .status = render_image_texture_frame_binding_packet_status::removed,
        .render_image_uri = diff.before_render_image_uri,
        .cache_key = diff.before_cache_key,
        .sampler = diff.before_sampler,
        .sampler_policy = sampler_policy,
        .sampler_key = sampler_policy.stable_key_fragment,
        .texture_key = render_image_texture_key{
            .source_key = diff.before_cache_key,
            .sampler = diff.before_sampler,
        },
        .stable_texture_cache_key = diff.before_stable_texture_cache_key,
        .texture_id = diff.before_texture_id,
        .texture_revision = diff.before_texture_revision,
        .execution_status = diff.before_execution_status,
        .has_texture = diff.before_texture_id != 0,
        .renderer_handoff_ready = false,
        .placeholder_texture = diff.before_placeholder_texture,
        .failed = false,
        .removed = true,
        .residency_budget_pressure = diff.before_residency_budget_pressure,
        .residency_pressure_status = diff.before_residency_pressure_status,
        .residency_pressure_status_name =
            render_image_texture_residency_budget_pressure_status_name(diff.before_residency_pressure_status),
        .removed_from_frame = true,
        .readiness_changed = diff.request_success_changed,
        .readiness_regressed = diff.before_renderer_handoff_ready,
        .diagnostic = "image texture binding packet was removed from frame",
    };
    packet.status_name = render_image_texture_frame_binding_packet_status_name(packet.status);
    return packet;
}

inline void count_render_image_texture_frame_binding_packet(
    render_image_texture_frame_binding_plan& plan,
    const render_image_texture_frame_binding_packet& packet)
{
    ++plan.packet_count;
    switch (packet.status) {
    case render_image_texture_frame_binding_packet_status::ready:
        ++plan.bindable_packet_count;
        ++plan.ready_packet_count;
        break;
    case render_image_texture_frame_binding_packet_status::placeholder:
        ++plan.bindable_packet_count;
        ++plan.placeholder_packet_count;
        break;
    case render_image_texture_frame_binding_packet_status::failed:
        ++plan.failed_packet_count;
        break;
    case render_image_texture_frame_binding_packet_status::removed:
        ++plan.removed_packet_count;
        break;
    }
    if (packet.added_in_frame) {
        ++plan.added_packet_count;
    }
    if (packet.changed_in_frame) {
        ++plan.changed_packet_count;
    }
    if (packet.unchanged_in_frame) {
        ++plan.unchanged_packet_count;
    }
    if (packet.readiness_changed) {
        ++plan.readiness_changed_count;
    }
    if (packet.readiness_regressed) {
        ++plan.readiness_regressed_count;
    }
    if (packet.readiness_recovered) {
        ++plan.readiness_recovered_count;
    }
    if (packet.residency_budget_pressure) {
        ++plan.residency_pressure_packet_count;
    }
}

inline void finalize_render_image_texture_frame_binding_plan(
    render_image_texture_frame_binding_plan& plan)
{
    if (plan.packet_count == 0) {
        plan.readiness_summary = "image texture frame binding plan has no texture bindings";
    } else if (plan.readiness_regressed_count != 0) {
        plan.readiness_summary = "image texture frame binding readiness regressed";
    } else if (plan.readiness_recovered_count != 0) {
        plan.readiness_summary = "image texture frame binding readiness recovered";
    } else if (plan.added_packet_count != 0 || plan.removed_packet_count != 0 || plan.changed_packet_count != 0) {
        plan.readiness_summary = "image texture frame binding packets changed";
    } else {
        plan.readiness_summary = "image texture frame binding packets unchanged";
    }

    if (plan.packet_count == 0) {
        plan.diagnostic = "image texture frame binding plan has no texture bindings";
    } else if (plan.failed_packet_count != 0) {
        plan.diagnostic = "image texture frame binding plan contains failed bindings";
    } else if (plan.residency_budget_pressure) {
        plan.diagnostic = "image texture frame binding plan ready with residency budget pressure";
    } else if (plan.placeholder_packet_count != 0) {
        plan.diagnostic = "image texture frame binding plan ready with placeholder bindings";
    } else if (plan.added_packet_count != 0 || plan.removed_packet_count != 0 || plan.changed_packet_count != 0) {
        plan.diagnostic = "image texture frame binding plan ready with frame delta";
    } else {
        plan.diagnostic = "image texture frame binding plan ready";
    }
}

inline render_image_texture_frame_binding_plan make_render_image_texture_frame_binding_plan(
    const render_image_texture_frame_snapshot& frame)
{
    render_image_texture_frame_binding_plan plan{
        .request_count = frame.request_count,
        .current_packet_count = frame.entries.size(),
        .renderer_handoff_ready = frame.renderer_handoff_ready,
        .residency_budget_pressure = frame.residency_budget_pressure,
        .residency_pressure_status = frame.residency_pressure_status,
        .residency_pressure_status_name = frame.residency_pressure_status_name,
    };

    for (const render_image_texture_frame_entry_snapshot& entry : frame.entries) {
        render_image_texture_frame_binding_packet packet =
            make_render_image_texture_frame_binding_packet(entry);
        packet.unchanged_in_frame = true;
        count_render_image_texture_frame_binding_packet(plan, packet);
        plan.packets.push_back(std::move(packet));
    }

    finalize_render_image_texture_frame_binding_plan(plan);
    return plan;
}

inline render_image_texture_frame_binding_plan make_render_image_texture_frame_binding_plan(
    const render_image_texture_frame_snapshot& frame,
    const render_image_texture_frame_snapshot_diff& diff)
{
    render_image_texture_frame_binding_plan plan{
        .request_count = frame.request_count,
        .current_packet_count = frame.entries.size(),
        .renderer_handoff_ready = frame.renderer_handoff_ready,
        .residency_budget_pressure = frame.residency_budget_pressure,
        .residency_pressure_status = frame.residency_pressure_status,
        .residency_pressure_status_name = frame.residency_pressure_status_name,
    };

    for (const render_image_texture_frame_entry_snapshot& entry : frame.entries) {
        render_image_texture_frame_binding_packet packet =
            make_render_image_texture_frame_binding_packet(entry);
        if (const render_image_texture_frame_entry_diff* entry_diff =
                render_image_texture_frame_diff_entry_for_request_index(diff, entry.request_index);
            entry_diff != nullptr) {
            packet.added_in_frame =
                entry_diff->status == render_image_texture_frame_snapshot_diff_entry_status::added;
            packet.changed_in_frame =
                entry_diff->status == render_image_texture_frame_snapshot_diff_entry_status::changed;
            packet.unchanged_in_frame =
                entry_diff->status == render_image_texture_frame_snapshot_diff_entry_status::unchanged;
            packet.readiness_changed = entry_diff->request_success_changed;
            packet.readiness_regressed =
                entry_diff->before_renderer_handoff_ready && !entry_diff->after_renderer_handoff_ready;
            packet.readiness_recovered =
                entry_diff->status != render_image_texture_frame_snapshot_diff_entry_status::added
                && !entry_diff->before_renderer_handoff_ready && entry_diff->after_renderer_handoff_ready;
        }
        count_render_image_texture_frame_binding_packet(plan, packet);
        plan.packets.push_back(std::move(packet));
    }

    for (const render_image_texture_frame_entry_diff& entry_diff : diff.entries) {
        if (entry_diff.status != render_image_texture_frame_snapshot_diff_entry_status::removed) {
            continue;
        }
        render_image_texture_frame_binding_packet packet =
            make_render_image_texture_frame_removed_binding_packet(entry_diff);
        count_render_image_texture_frame_binding_packet(plan, packet);
        plan.packets.push_back(std::move(packet));
    }

    finalize_render_image_texture_frame_binding_plan(plan);
    return plan;
}

inline render_image_texture_frame_binding_plan make_render_image_texture_frame_binding_plan(
    const render_image_texture_frame_snapshot& before,
    const render_image_texture_frame_snapshot& after)
{
    return make_render_image_texture_frame_binding_plan(
        after,
        diff_render_image_texture_frame_snapshots(before, after));
}

enum class render_image_texture_frame_binding_plan_diff_entry_status {
    unchanged,
    added,
    removed,
    changed,
};

inline std::string render_image_texture_frame_binding_plan_diff_entry_status_name(
    render_image_texture_frame_binding_plan_diff_entry_status status)
{
    switch (status) {
    case render_image_texture_frame_binding_plan_diff_entry_status::unchanged:
        return "unchanged";
    case render_image_texture_frame_binding_plan_diff_entry_status::added:
        return "added";
    case render_image_texture_frame_binding_plan_diff_entry_status::removed:
        return "removed";
    case render_image_texture_frame_binding_plan_diff_entry_status::changed:
        return "changed";
    }

    return "unknown";
}

struct render_image_texture_frame_binding_packet_diff {
    std::size_t request_index = 0;
    render_image_texture_frame_binding_plan_diff_entry_status status =
        render_image_texture_frame_binding_plan_diff_entry_status::unchanged;
    std::string status_name;
    std::string before_render_image_uri;
    std::string after_render_image_uri;
    std::string before_normalized_uri;
    std::string after_normalized_uri;
    render_image_cache_key before_cache_key;
    render_image_cache_key after_cache_key;
    std::string before_sampler_key;
    std::string after_sampler_key;
    std::string before_stable_texture_cache_key;
    std::string after_stable_texture_cache_key;
    render_image_texture_id before_texture_id = 0;
    render_image_texture_id after_texture_id = 0;
    render_image_revision before_texture_revision = 0;
    render_image_revision after_texture_revision = 0;
    render_image_texture_frame_binding_packet_status before_packet_status =
        render_image_texture_frame_binding_packet_status::failed;
    render_image_texture_frame_binding_packet_status after_packet_status =
        render_image_texture_frame_binding_packet_status::failed;
    bool before_renderer_handoff_ready = false;
    bool after_renderer_handoff_ready = false;
    bool before_placeholder_texture = false;
    bool after_placeholder_texture = false;
    bool before_failed = false;
    bool after_failed = false;
    bool before_residency_budget_pressure = false;
    bool after_residency_budget_pressure = false;
    render_image_texture_residency_budget_pressure_status before_residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    render_image_texture_residency_budget_pressure_status after_residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    bool texture_binding_added = false;
    bool texture_binding_removed = false;
    bool texture_binding_changed = false;
    bool readiness_changed = false;
    bool readiness_regressed = false;
    bool readiness_recovered = false;
    bool placeholder_changed = false;
    bool failure_changed = false;
    bool sampler_policy_changed = false;
    bool source_uri_changed = false;
    bool stable_uri_changed = false;
    bool cache_key_changed = false;
    bool stable_texture_cache_key_changed = false;
    bool residency_pressure_changed = false;
    bool regression = false;
    std::string diagnostic;

    bool changed() const
    {
        return status != render_image_texture_frame_binding_plan_diff_entry_status::unchanged;
    }

    bool ok() const
    {
        return !regression;
    }
};

struct render_image_texture_frame_binding_plan_diff {
    std::size_t before_request_count = 0;
    std::size_t after_request_count = 0;
    std::size_t before_packet_count = 0;
    std::size_t after_packet_count = 0;
    std::size_t unchanged_packet_count = 0;
    std::size_t added_packet_count = 0;
    std::size_t removed_packet_count = 0;
    std::size_t changed_packet_count = 0;
    std::size_t texture_binding_added_count = 0;
    std::size_t texture_binding_removed_count = 0;
    std::size_t texture_binding_changed_count = 0;
    std::size_t readiness_changed_count = 0;
    std::size_t readiness_regressed_count = 0;
    std::size_t readiness_recovered_count = 0;
    std::size_t placeholder_delta_count = 0;
    std::size_t failure_delta_count = 0;
    std::size_t sampler_policy_changed_count = 0;
    std::size_t source_uri_changed_count = 0;
    std::size_t stable_uri_changed_count = 0;
    std::size_t cache_key_changed_count = 0;
    std::size_t stable_texture_cache_key_changed_count = 0;
    std::size_t residency_pressure_delta_count = 0;
    std::size_t before_bindable_packet_count = 0;
    std::size_t after_bindable_packet_count = 0;
    std::size_t before_ready_packet_count = 0;
    std::size_t after_ready_packet_count = 0;
    std::size_t before_placeholder_packet_count = 0;
    std::size_t after_placeholder_packet_count = 0;
    std::size_t before_failed_packet_count = 0;
    std::size_t after_failed_packet_count = 0;
    bool before_renderer_handoff_ready = false;
    bool after_renderer_handoff_ready = false;
    bool before_residency_budget_pressure = false;
    bool after_residency_budget_pressure = false;
    render_image_texture_residency_budget_pressure_status before_residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    render_image_texture_residency_budget_pressure_status after_residency_pressure_status =
        render_image_texture_residency_budget_pressure_status::within_budget;
    bool renderer_handoff_regressed = false;
    bool renderer_handoff_recovered = false;
    bool failure_count_regressed = false;
    bool failure_count_recovered = false;
    bool placeholder_count_regressed = false;
    bool placeholder_count_recovered = false;
    bool residency_pressure_regressed = false;
    bool residency_pressure_recovered = false;
    bool has_changes = false;
    bool has_regression = false;
    std::string readiness_summary;
    std::string regression_summary;
    std::vector<render_image_texture_frame_binding_packet_diff> entries;
    std::string diagnostic;

    bool ok() const
    {
        return !has_regression;
    }
};

inline const render_image_texture_frame_binding_packet* render_image_texture_frame_binding_packet_for_request_index(
    const render_image_texture_frame_binding_plan& plan,
    std::size_t request_index)
{
    for (const render_image_texture_frame_binding_packet& packet : plan.packets) {
        if (packet.request_index == request_index) {
            return &packet;
        }
    }
    return nullptr;
}

inline render_image_texture_frame_binding_packet_diff make_render_image_texture_frame_binding_packet_diff(
    const render_image_texture_frame_binding_packet* before_packet,
    const render_image_texture_frame_binding_packet* after_packet,
    std::size_t request_index)
{
    render_image_texture_frame_binding_packet_diff diff{
        .request_index = request_index,
    };

    if (before_packet != nullptr) {
        diff.before_render_image_uri = before_packet->render_image_uri;
        diff.before_normalized_uri = before_packet->normalized_uri;
        diff.before_cache_key = before_packet->cache_key;
        diff.before_sampler_key = before_packet->sampler_key;
        diff.before_stable_texture_cache_key = before_packet->stable_texture_cache_key;
        diff.before_texture_id = before_packet->texture_id;
        diff.before_texture_revision = before_packet->texture_revision;
        diff.before_packet_status = before_packet->status;
        diff.before_renderer_handoff_ready = before_packet->renderer_handoff_ready;
        diff.before_placeholder_texture = before_packet->placeholder_texture;
        diff.before_failed = before_packet->failed;
        diff.before_residency_budget_pressure = before_packet->residency_budget_pressure;
        diff.before_residency_pressure_status = before_packet->residency_pressure_status;
    }

    if (after_packet != nullptr) {
        diff.after_render_image_uri = after_packet->render_image_uri;
        diff.after_normalized_uri = after_packet->normalized_uri;
        diff.after_cache_key = after_packet->cache_key;
        diff.after_sampler_key = after_packet->sampler_key;
        diff.after_stable_texture_cache_key = after_packet->stable_texture_cache_key;
        diff.after_texture_id = after_packet->texture_id;
        diff.after_texture_revision = after_packet->texture_revision;
        diff.after_packet_status = after_packet->status;
        diff.after_renderer_handoff_ready = after_packet->renderer_handoff_ready;
        diff.after_placeholder_texture = after_packet->placeholder_texture;
        diff.after_failed = after_packet->failed;
        diff.after_residency_budget_pressure = after_packet->residency_budget_pressure;
        diff.after_residency_pressure_status = after_packet->residency_pressure_status;
    }

    if (before_packet == nullptr && after_packet != nullptr) {
        diff.status = render_image_texture_frame_binding_plan_diff_entry_status::added;
        diff.texture_binding_added = after_packet->texture_id != 0;
        diff.readiness_changed = after_packet->renderer_handoff_ready;
        diff.placeholder_changed = after_packet->placeholder_texture;
        diff.failure_changed = after_packet->failed;
        diff.residency_pressure_changed = after_packet->residency_budget_pressure;
        diff.regression = after_packet->failed || after_packet->placeholder_texture
            || after_packet->residency_budget_pressure;
    } else if (before_packet != nullptr && after_packet == nullptr) {
        diff.status = render_image_texture_frame_binding_plan_diff_entry_status::removed;
        diff.texture_binding_removed = before_packet->texture_id != 0;
        diff.readiness_changed = before_packet->renderer_handoff_ready;
        diff.readiness_regressed = before_packet->renderer_handoff_ready;
        diff.placeholder_changed = before_packet->placeholder_texture;
        diff.failure_changed = before_packet->failed;
        diff.residency_pressure_changed = before_packet->residency_budget_pressure;
        diff.regression = before_packet->renderer_handoff_ready;
    } else if (before_packet != nullptr && after_packet != nullptr) {
        diff.texture_binding_added = before_packet->texture_id == 0 && after_packet->texture_id != 0;
        diff.texture_binding_removed = before_packet->texture_id != 0 && after_packet->texture_id == 0;
        diff.texture_binding_changed = before_packet->texture_id != 0 && after_packet->texture_id != 0
            && (before_packet->texture_id != after_packet->texture_id
                || before_packet->texture_revision != after_packet->texture_revision);
        diff.readiness_changed =
            before_packet->renderer_handoff_ready != after_packet->renderer_handoff_ready;
        diff.readiness_regressed =
            before_packet->renderer_handoff_ready && !after_packet->renderer_handoff_ready;
        diff.readiness_recovered =
            !before_packet->renderer_handoff_ready && after_packet->renderer_handoff_ready;
        diff.placeholder_changed =
            before_packet->placeholder_texture != after_packet->placeholder_texture;
        diff.failure_changed = before_packet->failed != after_packet->failed;
        diff.sampler_policy_changed = before_packet->sampler_key != after_packet->sampler_key
            || !(before_packet->sampler == after_packet->sampler);
        diff.source_uri_changed = before_packet->render_image_uri != after_packet->render_image_uri;
        diff.stable_uri_changed = before_packet->normalized_uri != after_packet->normalized_uri;
        diff.cache_key_changed = before_packet->cache_key != after_packet->cache_key;
        diff.stable_texture_cache_key_changed =
            before_packet->stable_texture_cache_key != after_packet->stable_texture_cache_key;
        diff.residency_pressure_changed =
            before_packet->residency_pressure_status != after_packet->residency_pressure_status
            || before_packet->residency_budget_pressure != after_packet->residency_budget_pressure;
        diff.regression = diff.readiness_regressed
            || (!before_packet->placeholder_texture && after_packet->placeholder_texture)
            || (!before_packet->failed && after_packet->failed)
            || render_image_texture_frame_pressure_rank(after_packet->residency_pressure_status)
                > render_image_texture_frame_pressure_rank(before_packet->residency_pressure_status);
        if (diff.texture_binding_added || diff.texture_binding_removed || diff.texture_binding_changed
            || diff.readiness_changed || diff.placeholder_changed || diff.failure_changed
            || diff.sampler_policy_changed || diff.source_uri_changed || diff.stable_uri_changed
            || diff.cache_key_changed || diff.stable_texture_cache_key_changed
            || diff.residency_pressure_changed
            || before_packet->status != after_packet->status) {
            diff.status = render_image_texture_frame_binding_plan_diff_entry_status::changed;
        }
    }

    diff.status_name = render_image_texture_frame_binding_plan_diff_entry_status_name(diff.status);
    diff.diagnostic = diff.status == render_image_texture_frame_binding_plan_diff_entry_status::unchanged
        ? "image texture frame binding packet unchanged"
        : (diff.regression
            ? "image texture frame binding packet changed with regression"
            : "image texture frame binding packet changed");
    return diff;
}

inline render_image_texture_frame_binding_plan_diff diff_render_image_texture_frame_binding_plans(
    const render_image_texture_frame_binding_plan& before,
    const render_image_texture_frame_binding_plan& after)
{
    render_image_texture_frame_binding_plan_diff diff{
        .before_request_count = before.request_count,
        .after_request_count = after.request_count,
        .before_packet_count = before.packet_count,
        .after_packet_count = after.packet_count,
        .before_bindable_packet_count = before.bindable_packet_count,
        .after_bindable_packet_count = after.bindable_packet_count,
        .before_ready_packet_count = before.ready_packet_count,
        .after_ready_packet_count = after.ready_packet_count,
        .before_placeholder_packet_count = before.placeholder_packet_count,
        .after_placeholder_packet_count = after.placeholder_packet_count,
        .before_failed_packet_count = before.failed_packet_count,
        .after_failed_packet_count = after.failed_packet_count,
        .before_renderer_handoff_ready = before.renderer_handoff_ready,
        .after_renderer_handoff_ready = after.renderer_handoff_ready,
        .before_residency_budget_pressure = before.residency_budget_pressure,
        .after_residency_budget_pressure = after.residency_budget_pressure,
        .before_residency_pressure_status = before.residency_pressure_status,
        .after_residency_pressure_status = after.residency_pressure_status,
    };

    std::map<std::size_t, bool> request_indices;
    for (const render_image_texture_frame_binding_packet& packet : before.packets) {
        request_indices.emplace(packet.request_index, true);
    }
    for (const render_image_texture_frame_binding_packet& packet : after.packets) {
        request_indices.emplace(packet.request_index, true);
    }

    for (const auto& [request_index, _] : request_indices) {
        const render_image_texture_frame_binding_packet* before_packet =
            render_image_texture_frame_binding_packet_for_request_index(before, request_index);
        const render_image_texture_frame_binding_packet* after_packet =
            render_image_texture_frame_binding_packet_for_request_index(after, request_index);
        render_image_texture_frame_binding_packet_diff entry_diff =
            make_render_image_texture_frame_binding_packet_diff(before_packet, after_packet, request_index);

        switch (entry_diff.status) {
        case render_image_texture_frame_binding_plan_diff_entry_status::unchanged:
            ++diff.unchanged_packet_count;
            break;
        case render_image_texture_frame_binding_plan_diff_entry_status::added:
            ++diff.added_packet_count;
            break;
        case render_image_texture_frame_binding_plan_diff_entry_status::removed:
            ++diff.removed_packet_count;
            break;
        case render_image_texture_frame_binding_plan_diff_entry_status::changed:
            ++diff.changed_packet_count;
            break;
        }

        if (entry_diff.texture_binding_added) {
            ++diff.texture_binding_added_count;
        }
        if (entry_diff.texture_binding_removed) {
            ++diff.texture_binding_removed_count;
        }
        if (entry_diff.texture_binding_changed) {
            ++diff.texture_binding_changed_count;
        }
        if (entry_diff.readiness_changed) {
            ++diff.readiness_changed_count;
        }
        if (entry_diff.readiness_regressed) {
            ++diff.readiness_regressed_count;
        }
        if (entry_diff.readiness_recovered) {
            ++diff.readiness_recovered_count;
        }
        if (entry_diff.placeholder_changed) {
            ++diff.placeholder_delta_count;
        }
        if (entry_diff.failure_changed) {
            ++diff.failure_delta_count;
        }
        if (entry_diff.sampler_policy_changed) {
            ++diff.sampler_policy_changed_count;
        }
        if (entry_diff.source_uri_changed) {
            ++diff.source_uri_changed_count;
        }
        if (entry_diff.stable_uri_changed) {
            ++diff.stable_uri_changed_count;
        }
        if (entry_diff.cache_key_changed) {
            ++diff.cache_key_changed_count;
        }
        if (entry_diff.stable_texture_cache_key_changed) {
            ++diff.stable_texture_cache_key_changed_count;
        }
        if (entry_diff.residency_pressure_changed) {
            ++diff.residency_pressure_delta_count;
        }
        if (entry_diff.regression) {
            diff.has_regression = true;
        }

        diff.entries.push_back(std::move(entry_diff));
    }

    diff.renderer_handoff_regressed = before.renderer_handoff_ready && !after.renderer_handoff_ready;
    diff.renderer_handoff_recovered = !before.renderer_handoff_ready && after.renderer_handoff_ready;
    diff.failure_count_regressed = after.failed_packet_count > before.failed_packet_count;
    diff.failure_count_recovered = after.failed_packet_count < before.failed_packet_count;
    diff.placeholder_count_regressed = after.placeholder_packet_count > before.placeholder_packet_count;
    diff.placeholder_count_recovered = after.placeholder_packet_count < before.placeholder_packet_count;
    diff.residency_pressure_regressed =
        render_image_texture_frame_pressure_rank(after.residency_pressure_status)
        > render_image_texture_frame_pressure_rank(before.residency_pressure_status);
    diff.residency_pressure_recovered =
        render_image_texture_frame_pressure_rank(after.residency_pressure_status)
        < render_image_texture_frame_pressure_rank(before.residency_pressure_status);
    diff.has_changes = diff.added_packet_count != 0 || diff.removed_packet_count != 0
        || diff.changed_packet_count != 0 || before.renderer_handoff_ready != after.renderer_handoff_ready
        || before.failed_packet_count != after.failed_packet_count
        || before.placeholder_packet_count != after.placeholder_packet_count
        || before.residency_pressure_status != after.residency_pressure_status;
    diff.has_regression = diff.has_regression || diff.renderer_handoff_regressed
        || diff.failure_count_regressed || diff.placeholder_count_regressed
        || diff.residency_pressure_regressed;

    if (diff.readiness_regressed_count != 0 || diff.renderer_handoff_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "binding readiness regressed");
    }
    if (diff.failure_count_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "binding failures increased");
    }
    if (diff.placeholder_count_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "placeholder bindings increased");
    }
    if (diff.residency_pressure_regressed) {
        append_render_image_texture_frame_regression_reason(diff.regression_summary, "binding residency pressure increased");
    }
    if (diff.has_regression && diff.regression_summary.empty()) {
        diff.regression_summary = "binding packets regressed";
    }
    if (diff.regression_summary.empty()) {
        diff.regression_summary = diff.has_changes
            ? "image texture frame binding plan diff has changes without regressions"
            : "image texture frame binding plan diff has no changes";
    }

    if (diff.readiness_regressed_count != 0) {
        diff.readiness_summary = "image texture frame binding readiness regressed";
    } else if (diff.readiness_recovered_count != 0) {
        diff.readiness_summary = "image texture frame binding readiness recovered";
    } else if (diff.readiness_changed_count != 0) {
        diff.readiness_summary = "image texture frame binding readiness changed";
    } else {
        diff.readiness_summary = "image texture frame binding readiness unchanged";
    }

    diff.diagnostic = diff.has_regression
        ? "image texture frame binding plan diff reports regressions"
        : (diff.has_changes
            ? "image texture frame binding plan diff reports changes"
            : "image texture frame binding plan diff is unchanged");
    return diff;
}

} // namespace quiz_vulkan::render

#include "render/image/image_texture_frame_upload_handoff.h"
