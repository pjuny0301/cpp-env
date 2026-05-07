#include "render/vulkan/vulkan_backend_frame_lifecycle.h"

namespace quiz_vulkan::render::vulkan_backend::frame_lifecycle {
namespace {

vulkan_frame_lifecycle_step_snapshot make_snapshot(
    vulkan_frame_lifecycle_step step,
    std::size_t expected_order)
{
    return vulkan_frame_lifecycle_step_snapshot{
        .step = step,
        .expected_order = expected_order,
    };
}

vulkan_frame_lifecycle_step_snapshot* find_snapshot(
    vulkan_backend_frame_lifecycle_policy_state& state,
    vulkan_frame_lifecycle_step step)
{
    for (vulkan_frame_lifecycle_step_snapshot& snapshot : state.snapshots) {
        if (snapshot.step == step) {
            return &snapshot;
        }
    }
    return nullptr;
}

bool previous_steps_completed(
    const vulkan_backend_frame_lifecycle_policy_state& state,
    const vulkan_frame_lifecycle_step_snapshot& current)
{
    for (const vulkan_frame_lifecycle_step_snapshot& snapshot : state.snapshots) {
        if (snapshot.expected_order >= current.expected_order) {
            continue;
        }
        if (!snapshot.completed) {
            return false;
        }
    }
    return true;
}

vulkan_frame_lifecycle_failure_classification classify_failure(
    vulkan_backend_fallback_reason reason)
{
    switch (reason) {
    case vulkan_backend_fallback_reason::acquire_image_failed:
    case vulkan_backend_fallback_reason::submit_frame_failed:
    case vulkan_backend_fallback_reason::present_image_failed:
    case vulkan_backend_fallback_reason::present_frame_failed:
        return vulkan_frame_lifecycle_failure_classification::recoverable;
    case vulkan_backend_fallback_reason::begin_frame_failed:
    case vulkan_backend_fallback_reason::record_commands_failed:
        return vulkan_frame_lifecycle_failure_classification::fatal;
    case vulkan_backend_fallback_reason::none:
        return vulkan_frame_lifecycle_failure_classification::none;
    case vulkan_backend_fallback_reason::not_requested:
    case vulkan_backend_fallback_reason::instance_unavailable:
    case vulkan_backend_fallback_reason::device_unavailable:
    case vulkan_backend_fallback_reason::swapchain_unavailable:
    case vulkan_backend_fallback_reason::render_pass_unavailable:
    case vulkan_backend_fallback_reason::pipeline_unavailable:
    case vulkan_backend_fallback_reason::command_recorder_unavailable:
    case vulkan_backend_fallback_reason::surface_unavailable:
    case vulkan_backend_fallback_reason::viewport_unavailable:
    case vulkan_backend_fallback_reason::resource_binding_unavailable:
        return vulkan_frame_lifecycle_failure_classification::fatal;
    }

    return vulkan_frame_lifecycle_failure_classification::fatal;
}

vulkan_backend_frame_fallback_summary make_fallback_summary(
    bool required,
    vulkan_backend_fallback_reason reason,
    vulkan_backend_frame_stage reached_stage)
{
    const vulkan_frame_lifecycle_failure_classification classification =
        classify_failure(reason);
    return vulkan_backend_frame_fallback_summary{
        .checked = true,
        .required = required,
        .reason = reason,
        .reached_stage = reached_stage,
        .recoverable = required
            && classification == vulkan_frame_lifecycle_failure_classification::recoverable,
        .fatal = required
            && classification == vulkan_frame_lifecycle_failure_classification::fatal,
        .reason_count =
            required && reason != vulkan_backend_fallback_reason::not_requested ? 1U : 0U,
    };
}

void skip_steps_after_failure(
    vulkan_backend_frame_lifecycle_policy_state& state,
    const vulkan_frame_lifecycle_step_snapshot& failed_snapshot)
{
    for (vulkan_frame_lifecycle_step_snapshot& snapshot : state.snapshots) {
        if (snapshot.expected_order <= failed_snapshot.expected_order || snapshot.attempted) {
            continue;
        }
        snapshot.status = vulkan_frame_lifecycle_order_status::skipped;
    }
}

} // namespace

vulkan_backend_frame_lifecycle_policy_state make_policy_state()
{
    return vulkan_backend_frame_lifecycle_policy_state{
        .checked = true,
        .snapshots = {
            make_snapshot(vulkan_frame_lifecycle_step::acquire, 0),
            make_snapshot(vulkan_frame_lifecycle_step::begin, 1),
            make_snapshot(vulkan_frame_lifecycle_step::render, 2),
            make_snapshot(vulkan_frame_lifecycle_step::submit, 3),
            make_snapshot(vulkan_frame_lifecycle_step::present, 4),
        },
    };
}

void mark_fallback(
    vulkan_backend_frame_result& result,
    vulkan_backend_fallback_reason reason)
{
    result.fallback_required = reason != vulkan_backend_fallback_reason::none;
    result.fallback_reason = reason;
    result.fallback_summary = make_fallback_summary(
        result.fallback_required,
        reason,
        result.reached_stage);
}

void start_step(
    vulkan_backend_frame_lifecycle_policy_state& state,
    vulkan_frame_lifecycle_step step)
{
    vulkan_frame_lifecycle_step_snapshot* snapshot = find_snapshot(state, step);
    if (snapshot == nullptr || snapshot->attempted) {
        state.ordering_valid = false;
        return;
    }

    snapshot->attempted = true;
    snapshot->observed_order = state.attempted_step_count;
    ++state.attempted_step_count;
    if (snapshot->observed_order != snapshot->expected_order
        || !previous_steps_completed(state, *snapshot)) {
        snapshot->status = vulkan_frame_lifecycle_order_status::out_of_order;
        state.ordering_valid = false;
        return;
    }

    snapshot->status = vulkan_frame_lifecycle_order_status::started;
}

void complete_step(
    vulkan_backend_frame_lifecycle_policy_state& state,
    vulkan_frame_lifecycle_step step)
{
    vulkan_frame_lifecycle_step_snapshot* snapshot = find_snapshot(state, step);
    if (snapshot == nullptr || !snapshot->attempted || snapshot->completed || snapshot->failed()) {
        state.ordering_valid = false;
        return;
    }

    snapshot->completed = true;
    snapshot->status = vulkan_frame_lifecycle_order_status::completed;
    ++state.completed_step_count;
}

void fail_step(
    vulkan_backend_frame_lifecycle_policy_state& state,
    vulkan_frame_lifecycle_step step,
    vulkan_backend_fallback_reason reason)
{
    vulkan_frame_lifecycle_step_snapshot* snapshot = find_snapshot(state, step);
    if (snapshot == nullptr) {
        state.ordering_valid = false;
        return;
    }
    if (!snapshot->attempted) {
        start_step(state, step);
        snapshot = find_snapshot(state, step);
        if (snapshot == nullptr) {
            state.ordering_valid = false;
            return;
        }
    }

    const vulkan_frame_lifecycle_failure_classification classification =
        classify_failure(reason);
    snapshot->status = vulkan_frame_lifecycle_order_status::failed;
    snapshot->failure_classification = classification;
    snapshot->fallback_reason = reason;
    state.failed_step = step;
    state.fallback_reason = reason;
    state.recoverable_failure =
        classification == vulkan_frame_lifecycle_failure_classification::recoverable;
    state.fatal_failure = classification == vulkan_frame_lifecycle_failure_classification::fatal;
    skip_steps_after_failure(state, *snapshot);
}

} // namespace quiz_vulkan::render::vulkan_backend::frame_lifecycle
