#include "render/vulkan/vulkan_backend_adapter.h"

#include <algorithm>
#include <string>
#include <utility>

namespace quiz_vulkan::render::vulkan_backend {
namespace {

bool has_visible_area(const render_rect& rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

vulkan_backend_fallback_reason first_unready_reason(
    const vulkan_backend_lifecycle_readiness& lifecycle)
{
    if (!lifecycle.instance_ready) {
        return vulkan_backend_fallback_reason::instance_unavailable;
    }
    if (!lifecycle.device_ready) {
        return vulkan_backend_fallback_reason::device_unavailable;
    }
    if (!lifecycle.swapchain_ready) {
        return vulkan_backend_fallback_reason::swapchain_unavailable;
    }
    if (!lifecycle.pipeline_ready) {
        return vulkan_backend_fallback_reason::pipeline_unavailable;
    }
    if (!lifecycle.command_recorder_ready) {
        return vulkan_backend_fallback_reason::command_recorder_unavailable;
    }

    return vulkan_backend_fallback_reason::none;
}

vulkan_recorded_draw_batch make_recorded_draw_batch(
    const vulkan_draw_batch& batch,
    std::size_t recording_index)
{
    return vulkan_recorded_draw_batch{
        .kind = batch.kind,
        .command_index = batch.command_index,
        .recording_index = recording_index,
        .bounds = batch.bounds,
        .clipped_bounds = batch.clipped_bounds,
        .scissor = batch.scissor,
    };
}

void mark_recorder_failure(
    vulkan_backend_command_recorder_state& state,
    vulkan_command_recorder_failure_stage stage,
    std::size_t recording_index)
{
    state.failure_stage = stage;
    state.failure_recording_index = recording_index;
}

vulkan_frame_lifecycle_step_snapshot make_frame_lifecycle_snapshot(
    vulkan_frame_lifecycle_step step,
    std::size_t expected_order)
{
    return vulkan_frame_lifecycle_step_snapshot{
        .step = step,
        .expected_order = expected_order,
    };
}

vulkan_backend_frame_lifecycle_policy_state make_frame_lifecycle_policy_state()
{
    return vulkan_backend_frame_lifecycle_policy_state{
        .checked = true,
        .snapshots = {
            make_frame_lifecycle_snapshot(vulkan_frame_lifecycle_step::acquire, 0),
            make_frame_lifecycle_snapshot(vulkan_frame_lifecycle_step::begin, 1),
            make_frame_lifecycle_snapshot(vulkan_frame_lifecycle_step::render, 2),
            make_frame_lifecycle_snapshot(vulkan_frame_lifecycle_step::submit, 3),
            make_frame_lifecycle_snapshot(vulkan_frame_lifecycle_step::present, 4),
        },
    };
}

vulkan_frame_lifecycle_step_snapshot* find_lifecycle_snapshot(
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

bool previous_lifecycle_steps_completed(
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

vulkan_frame_lifecycle_failure_classification classify_lifecycle_failure(
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
    case vulkan_backend_fallback_reason::pipeline_unavailable:
    case vulkan_backend_fallback_reason::command_recorder_unavailable:
    case vulkan_backend_fallback_reason::surface_unavailable:
    case vulkan_backend_fallback_reason::viewport_unavailable:
    case vulkan_backend_fallback_reason::resource_binding_unavailable:
        return vulkan_frame_lifecycle_failure_classification::fatal;
    }

    return vulkan_frame_lifecycle_failure_classification::fatal;
}

vulkan_backend_frame_fallback_summary make_frame_fallback_summary(
    bool required,
    vulkan_backend_fallback_reason reason,
    vulkan_backend_frame_stage reached_stage)
{
    const vulkan_frame_lifecycle_failure_classification classification =
        classify_lifecycle_failure(reason);
    return vulkan_backend_frame_fallback_summary{
        .checked = true,
        .required = required,
        .reason = reason,
        .reached_stage = reached_stage,
        .recoverable = required
            && classification == vulkan_frame_lifecycle_failure_classification::recoverable,
        .fatal = required
            && classification == vulkan_frame_lifecycle_failure_classification::fatal,
        .reason_count = required && reason != vulkan_backend_fallback_reason::not_requested ? 1U : 0U,
    };
}

void mark_frame_fallback(
    vulkan_backend_frame_result& result,
    vulkan_backend_fallback_reason reason)
{
    result.fallback_required = reason != vulkan_backend_fallback_reason::none;
    result.fallback_reason = reason;
    result.fallback_summary = make_frame_fallback_summary(
        result.fallback_required,
        reason,
        result.reached_stage);
}

void start_lifecycle_step(
    vulkan_backend_frame_lifecycle_policy_state& state,
    vulkan_frame_lifecycle_step step)
{
    vulkan_frame_lifecycle_step_snapshot* snapshot = find_lifecycle_snapshot(state, step);
    if (snapshot == nullptr || snapshot->attempted) {
        state.ordering_valid = false;
        return;
    }

    snapshot->attempted = true;
    snapshot->observed_order = state.attempted_step_count;
    ++state.attempted_step_count;
    if (snapshot->observed_order != snapshot->expected_order
        || !previous_lifecycle_steps_completed(state, *snapshot)) {
        snapshot->status = vulkan_frame_lifecycle_order_status::out_of_order;
        state.ordering_valid = false;
        return;
    }

    snapshot->status = vulkan_frame_lifecycle_order_status::started;
}

void complete_lifecycle_step(
    vulkan_backend_frame_lifecycle_policy_state& state,
    vulkan_frame_lifecycle_step step)
{
    vulkan_frame_lifecycle_step_snapshot* snapshot = find_lifecycle_snapshot(state, step);
    if (snapshot == nullptr || !snapshot->attempted || snapshot->completed || snapshot->failed()) {
        state.ordering_valid = false;
        return;
    }

    snapshot->completed = true;
    snapshot->status = vulkan_frame_lifecycle_order_status::completed;
    ++state.completed_step_count;
}

void skip_lifecycle_steps_after_failure(
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

void fail_lifecycle_step(
    vulkan_backend_frame_lifecycle_policy_state& state,
    vulkan_frame_lifecycle_step step,
    vulkan_backend_fallback_reason reason)
{
    vulkan_frame_lifecycle_step_snapshot* snapshot = find_lifecycle_snapshot(state, step);
    if (snapshot == nullptr) {
        state.ordering_valid = false;
        return;
    }
    if (!snapshot->attempted) {
        start_lifecycle_step(state, step);
        snapshot = find_lifecycle_snapshot(state, step);
        if (snapshot == nullptr) {
            state.ordering_valid = false;
            return;
        }
    }

    const vulkan_frame_lifecycle_failure_classification classification =
        classify_lifecycle_failure(reason);
    snapshot->status = vulkan_frame_lifecycle_order_status::failed;
    snapshot->failure_classification = classification;
    snapshot->fallback_reason = reason;
    state.failed_step = step;
    state.fallback_reason = reason;
    state.recoverable_failure =
        classification == vulkan_frame_lifecycle_failure_classification::recoverable;
    state.fatal_failure = classification == vulkan_frame_lifecycle_failure_classification::fatal;
    skip_lifecycle_steps_after_failure(state, *snapshot);
}

vulkan_backend_frame_present_policy_state make_frame_present_policy_state()
{
    return vulkan_backend_frame_present_policy_state{
        .checked = true,
        .acquire = vulkan_frame_acquire_policy_diagnostics{
            .checked = true,
            .requested = false,
            .swapchain_status = vulkan_swapchain_acquire_status::not_requested,
            .image_id = {},
            .image_available = false,
            .image_acquired = false,
            .backpressured = false,
            .status = vulkan_frame_acquire_policy_status::not_requested,
            .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        },
        .present = vulkan_frame_present_result_summary{
            .checked = true,
            .image_present_requested = false,
            .frame_present_requested = false,
            .image_id = {},
            .swapchain_status = vulkan_swapchain_present_status::not_requested,
            .image_presented = false,
            .frame_presented = false,
            .status = vulkan_frame_present_result_status::not_requested,
            .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        },
    };
}

void mark_acquire_policy_requested(vulkan_backend_frame_present_policy_state& state)
{
    state.checked = true;
    ++state.acquire_request_count;
    state.acquire.checked = true;
    state.acquire.requested = true;
    state.acquire.status = vulkan_frame_acquire_policy_status::not_requested;
    state.acquire.fallback_reason = vulkan_backend_fallback_reason::not_requested;
}

void mark_acquire_policy_result(
    vulkan_backend_frame_present_policy_state& state,
    const vulkan_swapchain_acquire_result& acquire)
{
    state.acquire.swapchain_status = acquire.status;
    state.acquire.image_id = acquire.image.id;
    state.acquire.image_available = acquire.image.available;
    state.acquire.image_acquired = acquire.image.acquired;

    if (acquire.completed()) {
        state.acquire.status = vulkan_frame_acquire_policy_status::acquired;
        state.acquire.fallback_reason = vulkan_backend_fallback_reason::none;
        return;
    }

    const bool backpressured =
        acquire.status == vulkan_swapchain_acquire_status::backpressured
        || (acquire.status == vulkan_swapchain_acquire_status::acquired
            && !acquire.image.ready_for_recording());
    state.acquire.backpressured = backpressured;
    state.backpressure_detected = backpressured;
    state.acquire.status = backpressured
        ? vulkan_frame_acquire_policy_status::backpressured
        : vulkan_frame_acquire_policy_status::failed;
    state.acquire.fallback_reason = vulkan_backend_fallback_reason::acquire_image_failed;
}

void mark_present_policy_image_requested(
    vulkan_backend_frame_present_policy_state& state,
    vulkan_swapchain_image_id image_id)
{
    state.checked = true;
    ++state.present_image_request_count;
    state.present.checked = true;
    state.present.image_present_requested = true;
    state.present.image_id = image_id;
    state.present.status = vulkan_frame_present_result_status::not_requested;
    state.present.fallback_reason = vulkan_backend_fallback_reason::not_requested;
}

void mark_present_policy_image_result(
    vulkan_backend_frame_present_policy_state& state,
    const vulkan_swapchain_present_result& present)
{
    state.present.swapchain_status = present.status;
    state.present.image_id = present.image_id;
    state.present.image_presented = present.completed();
    state.present.status = present.completed()
        ? vulkan_frame_present_result_status::image_presented
        : vulkan_frame_present_result_status::image_failed;
    state.present.fallback_reason = present.completed()
        ? vulkan_backend_fallback_reason::none
        : vulkan_backend_fallback_reason::present_image_failed;
}

void mark_present_policy_frame_requested(vulkan_backend_frame_present_policy_state& state)
{
    state.present.frame_present_requested = true;
}

void mark_present_policy_frame_result(
    vulkan_backend_frame_present_policy_state& state,
    bool frame_presented)
{
    state.present.frame_presented = frame_presented;
    state.present.status = frame_presented
        ? vulkan_frame_present_result_status::frame_presented
        : vulkan_frame_present_result_status::frame_failed;
    state.present.fallback_reason = frame_presented
        ? vulkan_backend_fallback_reason::none
        : vulkan_backend_fallback_reason::present_frame_failed;
}

std::size_t clamp_extent_value(std::size_t value, std::size_t min_value, std::size_t max_value)
{
    return std::clamp(value, min_value, max_value);
}

vulkan_backend_swapchain_policy_state make_swapchain_policy_state(vulkan_surface_extent requested_extent)
{
    vulkan_swapchain_extent_policy_state extent;
    extent.checked = true;
    extent.requested_extent = requested_extent;
    extent.extent_supported = requested_extent.valid();
    extent.selected_extent = requested_extent.valid()
        ? vulkan_surface_extent{
            .width = clamp_extent_value(
                requested_extent.width,
                extent.min_extent.width,
                extent.max_extent.width),
            .height = clamp_extent_value(
                requested_extent.height,
                extent.min_extent.height,
                extent.max_extent.height),
        }
        : vulkan_surface_extent{};
    extent.extent_clamped =
        extent.selected_extent.width != requested_extent.width
        || extent.selected_extent.height != requested_extent.height;

    return vulkan_backend_swapchain_policy_state{
        .checked = true,
        .extent = extent,
        .present_mode = vulkan_swapchain_present_mode_policy_state{
            .checked = true,
            .requested_mode = vulkan_swapchain_present_mode::fifo,
            .selected_mode = vulkan_swapchain_present_mode::fifo,
            .requested_mode_supported = true,
            .fallback_to_fifo = false,
        },
    };
}

bool same_shader_id(const vulkan_shader_module_id& left, const vulkan_shader_module_id& right)
{
    return left.value == right.value;
}

bool same_pipeline_compatibility_key(
    const vulkan_pipeline_compatibility_key& left,
    const vulkan_pipeline_compatibility_key& right)
{
    return left.batch_kind == right.batch_kind
        && left.color_attachment_count == right.color_attachment_count
        && left.has_depth_attachment == right.has_depth_attachment
        && left.surface_compatible == right.surface_compatible
        && same_shader_id(left.vertex_shader, right.vertex_shader)
        && same_shader_id(left.fragment_shader, right.fragment_shader);
}

bool compatibility_key_seen(
    const std::vector<vulkan_pipeline_compatibility_key>& keys,
    const vulkan_pipeline_compatibility_key& candidate)
{
    for (const vulkan_pipeline_compatibility_key& key : keys) {
        if (same_pipeline_compatibility_key(key, candidate)) {
            return true;
        }
    }
    return false;
}

vulkan_pipeline_compatibility_key make_pipeline_compatibility_key(
    const vulkan_draw_batch& batch,
    const vulkan_render_pass_descriptor& render_pass,
    const vulkan_pipeline_descriptor* descriptor)
{
    return vulkan_pipeline_compatibility_key{
        .batch_kind = batch.kind,
        .color_attachment_count = render_pass.color_attachment_count,
        .has_depth_attachment = render_pass.has_depth_attachment,
        .surface_compatible = render_pass.surface_compatible,
        .vertex_shader = descriptor == nullptr ? vulkan_shader_module_id{} : descriptor->vertex_shader,
        .fragment_shader = descriptor == nullptr ? vulkan_shader_module_id{} : descriptor->fragment_shader,
    };
}

void append_pipeline_compatibility_key(
    vulkan_backend_pipeline_state& state,
    vulkan_pipeline_compatibility_key key)
{
    state.compatibility.checked = true;
    ++state.compatibility.requested_key_count;
    if (key.compatible()) {
        ++state.compatibility.compatible_key_count;
    } else {
        ++state.compatibility.incompatible_key_count;
    }
    if (!compatibility_key_seen(state.compatibility.keys, key)) {
        ++state.compatibility.unique_key_count;
    }
    state.compatibility.keys.push_back(std::move(key));
}

vulkan_shader_module_binding_readiness make_shader_module_binding_readiness(
    const vulkan_draw_batch& batch,
    vulkan_shader_stage stage,
    const vulkan_shader_module_id& shader_id)
{
    return vulkan_shader_module_binding_readiness{
        .batch_kind = batch.kind,
        .command_index = batch.command_index,
        .stage = stage,
        .shader_id = shader_id,
        .entry_point = "main",
        .descriptor_declared = !shader_id.empty(),
    };
}

void append_shader_binding_readiness(
    vulkan_backend_pipeline_state& state,
    vulkan_shader_module_binding_readiness readiness)
{
    state.shader_bindings.checked = true;
    ++state.shader_bindings.requested_binding_count;
    if (readiness.completed()) {
        ++state.shader_bindings.ready_binding_count;
    } else {
        ++state.shader_bindings.missing_binding_count;
    }
    state.shader_bindings.bindings.push_back(std::move(readiness));
}

bool require_shader_binding(
    vulkan_backend_pipeline_state& state,
    diagnostic_vulkan_shader_registry& shader_registry,
    const vulkan_draw_batch& batch,
    vulkan_shader_stage stage,
    const vulkan_shader_module_id& shader_id)
{
    vulkan_shader_module_binding_readiness readiness =
        make_shader_module_binding_readiness(batch, stage, shader_id);
    readiness.registry_checked = true;
    readiness.module_registered = shader_registry.require_shader(batch.kind, stage, shader_id);
    readiness.ready = readiness.descriptor_declared && readiness.module_registered;
    const bool ready = readiness.ready;
    append_shader_binding_readiness(state, std::move(readiness));
    return ready;
}

vulkan_shader_module_id shader_id(std::string value)
{
    return vulkan_shader_module_id{.value = std::move(value)};
}

std::vector<vulkan_shader_module_descriptor> make_default_shader_modules()
{
    return {
        vulkan_shader_module_descriptor{
            .id = shader_id("quad.vertex"),
            .stage = vulkan_shader_stage::vertex,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("quad.fragment"),
            .stage = vulkan_shader_stage::fragment,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("text.vertex"),
            .stage = vulkan_shader_stage::vertex,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("text.fragment"),
            .stage = vulkan_shader_stage::fragment,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("image.vertex"),
            .stage = vulkan_shader_stage::vertex,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("image.fragment"),
            .stage = vulkan_shader_stage::fragment,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("debug_bounds.vertex"),
            .stage = vulkan_shader_stage::vertex,
        },
        vulkan_shader_module_descriptor{
            .id = shader_id("debug_bounds.fragment"),
            .stage = vulkan_shader_stage::fragment,
        },
    };
}

std::vector<vulkan_pipeline_descriptor> make_default_pipeline_descriptors()
{
    return {
        vulkan_pipeline_descriptor{
            .kind = vulkan_batch_kind::quad,
            .vertex_shader = shader_id("quad.vertex"),
            .fragment_shader = shader_id("quad.fragment"),
        },
        vulkan_pipeline_descriptor{
            .kind = vulkan_batch_kind::text,
            .vertex_shader = shader_id("text.vertex"),
            .fragment_shader = shader_id("text.fragment"),
        },
        vulkan_pipeline_descriptor{
            .kind = vulkan_batch_kind::image,
            .vertex_shader = shader_id("image.vertex"),
            .fragment_shader = shader_id("image.fragment"),
        },
        vulkan_pipeline_descriptor{
            .kind = vulkan_batch_kind::debug_bounds,
            .vertex_shader = shader_id("debug_bounds.vertex"),
            .fragment_shader = shader_id("debug_bounds.fragment"),
        },
    };
}

void append_shader_modules(
    std::vector<vulkan_shader_module_descriptor>& modules,
    std::vector<vulkan_shader_module_descriptor> extra_modules)
{
    modules.reserve(modules.size() + extra_modules.size());
    for (vulkan_shader_module_descriptor& module : extra_modules) {
        modules.push_back(std::move(module));
    }
}

void apply_pipeline_descriptor_overrides(
    std::vector<vulkan_pipeline_descriptor>& descriptors,
    std::vector<vulkan_pipeline_descriptor> overrides)
{
    for (vulkan_pipeline_descriptor& override_descriptor : overrides) {
        bool replaced = false;
        for (vulkan_pipeline_descriptor& descriptor : descriptors) {
            if (descriptor.kind == override_descriptor.kind) {
                descriptor = std::move(override_descriptor);
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            descriptors.push_back(std::move(override_descriptor));
        }
    }
}

std::vector<vulkan_pipeline_capability> make_pipeline_capabilities(bool available)
{
    return {
        vulkan_pipeline_capability{.kind = vulkan_batch_kind::quad, .available = available},
        vulkan_pipeline_capability{.kind = vulkan_batch_kind::text, .available = available},
        vulkan_pipeline_capability{.kind = vulkan_batch_kind::image, .available = available},
        vulkan_pipeline_capability{.kind = vulkan_batch_kind::debug_bounds, .available = available},
    };
}

std::vector<vulkan_pipeline_cache_entry> make_pipeline_cache_entries(
    const std::vector<vulkan_pipeline_capability>& capabilities)
{
    std::vector<vulkan_pipeline_cache_entry> entries;
    entries.reserve(capabilities.size());
    for (const vulkan_pipeline_capability& capability : capabilities) {
        entries.push_back(vulkan_pipeline_cache_entry{
            .kind = capability.kind,
            .available = capability.available,
        });
    }
    return entries;
}

void apply_pipeline_overrides(
    std::vector<vulkan_pipeline_capability>& capabilities,
    const std::vector<vulkan_pipeline_capability>& overrides)
{
    for (const vulkan_pipeline_capability& override_capability : overrides) {
        for (vulkan_pipeline_capability& capability : capabilities) {
            if (capability.kind == override_capability.kind) {
                capability.available = override_capability.available;
                break;
            }
        }
    }
}

vulkan_pipeline_cache_entry* find_pipeline_entry(
    std::vector<vulkan_pipeline_cache_entry>& entries,
    vulkan_batch_kind kind)
{
    for (vulkan_pipeline_cache_entry& entry : entries) {
        if (entry.kind == kind) {
            return &entry;
        }
    }
    return nullptr;
}

const vulkan_pipeline_descriptor* find_pipeline_descriptor(
    const std::vector<vulkan_pipeline_descriptor>& descriptors,
    vulkan_batch_kind kind)
{
    for (const vulkan_pipeline_descriptor& descriptor : descriptors) {
        if (descriptor.matches(kind)) {
            return &descriptor;
        }
    }
    return nullptr;
}

vulkan_pipeline_lifecycle_snapshot make_pipeline_lifecycle_snapshot(
    const vulkan_draw_batch& batch,
    vulkan_pipeline_lifecycle_status render_pass_status)
{
    return vulkan_pipeline_lifecycle_snapshot{
        .batch_kind = batch.kind,
        .command_index = batch.command_index,
        .render_pass_status = render_pass_status,
        .failed_stage = vulkan_pipeline_lifecycle_stage::render_pass,
        .missing_shader_stage = vulkan_shader_stage::vertex,
        .missing_shader_id = {},
    };
}

vulkan_pipeline_shader_stage_snapshot make_pipeline_shader_stage_snapshot(
    const vulkan_draw_batch& batch,
    const vulkan_pipeline_descriptor& descriptor)
{
    return vulkan_pipeline_shader_stage_snapshot{
        .batch_kind = batch.kind,
        .command_index = batch.command_index,
        .vertex_shader = descriptor.vertex_shader,
        .fragment_shader = descriptor.fragment_shader,
    };
}

void mark_pipeline_lifecycle_failure(
    vulkan_backend_pipeline_state& state,
    vulkan_pipeline_lifecycle_snapshot snapshot,
    vulkan_pipeline_lifecycle_stage stage)
{
    snapshot.failed_stage = stage;
    if (stage == vulkan_pipeline_lifecycle_stage::render_pass) {
        snapshot.render_pass_status = vulkan_pipeline_lifecycle_status::unavailable;
    } else if (stage == vulkan_pipeline_lifecycle_stage::shader_stages) {
        snapshot.shader_stage_status = vulkan_pipeline_lifecycle_status::unavailable;
    } else {
        snapshot.pipeline_status = vulkan_pipeline_lifecycle_status::unavailable;
    }

    state.lifecycle.missing_pipeline = stage == vulkan_pipeline_lifecycle_stage::pipeline;
    state.lifecycle.missing_shader_stage = stage == vulkan_pipeline_lifecycle_stage::shader_stages;
    state.lifecycle.missing_render_pass = stage == vulkan_pipeline_lifecycle_stage::render_pass;
    state.lifecycle.failed_stage = stage;
    state.lifecycle.missing_batch_kind = snapshot.batch_kind;
    state.lifecycle.missing_command_index = snapshot.command_index;
    state.lifecycle.pipeline_snapshots.push_back(std::move(snapshot));
}

void mark_pipeline_lifecycle_shader_failure(
    vulkan_backend_pipeline_state& state,
    vulkan_pipeline_lifecycle_snapshot snapshot,
    vulkan_pipeline_shader_stage_snapshot shader_snapshot,
    vulkan_shader_stage stage,
    const vulkan_shader_module_id& shader_id)
{
    snapshot.missing_shader_stage = stage;
    snapshot.missing_shader_id = shader_id;
    state.lifecycle.missing_shader_stage_kind = stage;
    state.lifecycle.missing_shader_id = shader_id;
    state.lifecycle.shader_stage_snapshots.push_back(std::move(shader_snapshot));
    mark_pipeline_lifecycle_failure(
        state,
        std::move(snapshot),
        vulkan_pipeline_lifecycle_stage::shader_stages);
}

void mark_pipeline_lifecycle_success(
    vulkan_backend_pipeline_state& state,
    vulkan_pipeline_lifecycle_snapshot snapshot,
    vulkan_pipeline_shader_stage_snapshot shader_snapshot)
{
    snapshot.shader_stage_status = vulkan_pipeline_lifecycle_status::ready;
    snapshot.pipeline_status = vulkan_pipeline_lifecycle_status::ready;
    shader_snapshot.vertex_stage_ready = true;
    shader_snapshot.fragment_stage_ready = true;
    state.lifecycle.shader_stage_snapshots.push_back(std::move(shader_snapshot));
    state.lifecycle.pipeline_snapshots.push_back(std::move(snapshot));
}

void mark_missing_pipeline_descriptor(
    vulkan_backend_pipeline_state& state,
    const vulkan_draw_batch& batch)
{
    state.missing_pipeline = true;
    state.missing_descriptor = true;
    state.ready = false;
    state.missing_batch_kind = batch.kind;
    state.missing_command_index = batch.command_index;
}

void mark_missing_pipeline_shader(
    vulkan_backend_pipeline_state& state,
    const vulkan_draw_batch& batch,
    const vulkan_backend_shader_registry_state& registry_state)
{
    state.shader_registry = registry_state;
    state.missing_pipeline = true;
    state.missing_shader = true;
    state.ready = false;
    state.missing_batch_kind = batch.kind;
    state.missing_command_index = batch.command_index;
    state.missing_shader_stage = state.shader_registry.missing_stage;
    state.missing_shader_id = state.shader_registry.missing_shader_id;
}

bool ensure_frame_plan_pipelines(
    vulkan_pipeline_cache_interface& pipeline_cache,
    const vulkan_frame_plan& plan,
    vulkan_backend_frame_result& result)
{
    for (const vulkan_draw_batch& batch : plan.batches) {
        if (!pipeline_cache.ensure_pipeline(batch)) {
            result.pipeline = pipeline_cache.pipeline_state();
            mark_frame_fallback(result, vulkan_backend_fallback_reason::pipeline_unavailable);
            return false;
        }
    }

    result.pipeline = pipeline_cache.pipeline_state();
    return true;
}

std::string resource_id(std::string prefix, const vulkan_draw_batch& batch)
{
    prefix += ":";
    prefix += batch.node_id.empty() ? std::to_string(batch.command_index) : batch.node_id;
    return prefix;
}

const render_draw_command* source_command_for_batch(
    const render_draw_list& draw_list,
    const vulkan_draw_batch& batch)
{
    if (batch.command_index >= draw_list.commands.size()) {
        return nullptr;
    }
    return &draw_list.commands[batch.command_index];
}

std::size_t text_payload_size(const render_draw_command& command)
{
    std::size_t byte_count = 0;
    for (const render_text_run& run : command.text_runs) {
        byte_count += run.text.size();
    }
    return byte_count;
}

std::string first_text_style_or_fallback(const render_draw_command& command)
{
    for (const render_text_run& run : command.text_runs) {
        if (!run.text.empty()) {
            return run.style_token.empty() ? std::string{"fallback"} : run.style_token;
        }
    }
    return {};
}

vulkan_resource_binding_snapshot make_resource_binding(
    std::size_t binding,
    vulkan_resource_binding_kind kind,
    std::string resource,
    bool required = true)
{
    const bool available = !resource.empty();
    return vulkan_resource_binding_snapshot{
        .set = 0,
        .binding = binding,
        .kind = kind,
        .resource_id = std::move(resource),
        .required = required,
        .available = available,
    };
}

std::string missing_resource_id_for(
    const vulkan_batch_resource_binding_snapshot& snapshot,
    vulkan_resource_binding_kind kind)
{
    switch (kind) {
    case vulkan_resource_binding_kind::image_texture:
        return "missing_image_texture:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    case vulkan_resource_binding_kind::text_run_buffer:
        return "missing_text_run_buffer:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    case vulkan_resource_binding_kind::text_glyph_atlas:
        return "missing_text_glyph_atlas:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    case vulkan_resource_binding_kind::batch_uniform:
        return "missing_batch_uniform:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    case vulkan_resource_binding_kind::quad_vertex_buffer:
        return "missing_quad_vertex_buffer:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    case vulkan_resource_binding_kind::image_sampler:
        return "missing_image_sampler:"
            + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
    }

    return "missing_resource:"
        + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
}

void append_binding(
    vulkan_batch_resource_binding_snapshot& snapshot,
    vulkan_resource_binding_snapshot binding)
{
    if (!binding.bound() && !snapshot.missing_resource) {
        snapshot.missing_resource = true;
        snapshot.missing_binding_kind = binding.kind;
        snapshot.missing_resource_id = missing_resource_id_for(snapshot, binding.kind);
    }
    snapshot.bindings.push_back(std::move(binding));
    snapshot.binding_count = snapshot.bindings.size();
    snapshot.descriptor_set_count = snapshot.bindings.empty() ? 0 : 1;
}

std::size_t expected_descriptor_binding_count(vulkan_batch_kind kind)
{
    switch (kind) {
    case vulkan_batch_kind::quad:
    case vulkan_batch_kind::debug_bounds:
        return 2;
    case vulkan_batch_kind::text:
    case vulkan_batch_kind::image:
        return 3;
    }

    return 0;
}

vulkan_resource_binding_kind expected_descriptor_binding_kind(
    vulkan_batch_kind kind,
    std::size_t binding)
{
    if (binding == 0) {
        return vulkan_resource_binding_kind::batch_uniform;
    }

    switch (kind) {
    case vulkan_batch_kind::quad:
    case vulkan_batch_kind::debug_bounds:
        return vulkan_resource_binding_kind::quad_vertex_buffer;
    case vulkan_batch_kind::image:
        return binding == 1
            ? vulkan_resource_binding_kind::image_texture
            : vulkan_resource_binding_kind::image_sampler;
    case vulkan_batch_kind::text:
        return binding == 1
            ? vulkan_resource_binding_kind::text_run_buffer
            : vulkan_resource_binding_kind::text_glyph_atlas;
    }

    return vulkan_resource_binding_kind::batch_uniform;
}

bool descriptor_binding_pair_seen(
    const std::vector<vulkan_descriptor_binding_validation_snapshot>& bindings,
    std::size_t set,
    std::size_t binding)
{
    for (const vulkan_descriptor_binding_validation_snapshot& snapshot : bindings) {
        if (snapshot.set == set && snapshot.binding == binding) {
            return true;
        }
    }
    return false;
}

vulkan_descriptor_validation_status descriptor_binding_status(
    const vulkan_descriptor_binding_validation_snapshot& binding)
{
    if (binding.duplicate_binding) {
        return vulkan_descriptor_validation_status::duplicate_binding;
    }
    if (!binding.binding_index_matches_order) {
        return vulkan_descriptor_validation_status::invalid_layout;
    }
    if (!binding.bound) {
        return vulkan_descriptor_validation_status::missing_required_resource;
    }
    return vulkan_descriptor_validation_status::valid;
}

vulkan_descriptor_set_validation_snapshot make_descriptor_set_validation_snapshot(
    const vulkan_batch_resource_binding_snapshot& batch)
{
    vulkan_descriptor_set_validation_snapshot validation{
        .batch_kind = batch.batch_kind,
        .command_index = batch.command_index,
        .node_id = batch.node_id,
        .set = 0,
        .expected_binding_count = expected_descriptor_binding_count(batch.batch_kind),
        .actual_binding_count = batch.bindings.size(),
        .checked = true,
        .descriptor_set_declared = batch.descriptor_set_count > 0,
        .binding_count_matches = batch.bindings.size() == expected_descriptor_binding_count(batch.batch_kind),
        .bindings = {},
    };
    validation.invalid_layout = !validation.descriptor_set_declared || !validation.binding_count_matches;

    validation.bindings.reserve(batch.bindings.size());
    for (std::size_t binding_index = 0; binding_index < batch.bindings.size(); ++binding_index) {
        const vulkan_resource_binding_snapshot& binding = batch.bindings[binding_index];
        vulkan_descriptor_binding_validation_snapshot binding_validation{
            .set = binding.set,
            .binding = binding.binding,
            .kind = binding.kind,
            .resource_id = binding.resource_id,
            .required = binding.required,
            .available = binding.available,
            .bound = binding.bound(),
            .binding_index_matches_order =
                binding.binding == binding_index
                && binding.kind == expected_descriptor_binding_kind(batch.batch_kind, binding_index),
            .duplicate_binding = descriptor_binding_pair_seen(validation.bindings, binding.set, binding.binding),
        };
        binding_validation.status = descriptor_binding_status(binding_validation);

        if (!binding_validation.completed()
            && validation.status == vulkan_descriptor_validation_status::not_checked) {
            validation.status = binding_validation.status;
            validation.failed_binding_kind = binding.kind;
            validation.failed_binding = binding.binding;
        }
        validation.missing_required_resource =
            validation.missing_required_resource
            || binding_validation.status == vulkan_descriptor_validation_status::missing_required_resource;
        validation.duplicate_binding =
            validation.duplicate_binding
            || binding_validation.status == vulkan_descriptor_validation_status::duplicate_binding;
        validation.invalid_layout =
            validation.invalid_layout
            || binding_validation.status == vulkan_descriptor_validation_status::invalid_layout;
        validation.bindings.push_back(std::move(binding_validation));
    }

    if (validation.status == vulkan_descriptor_validation_status::not_checked) {
        const bool descriptor_set_is_valid =
            validation.checked
            && validation.descriptor_set_declared
            && validation.binding_count_matches
            && !validation.missing_required_resource
            && !validation.duplicate_binding
            && !validation.invalid_layout
            && validation.actual_binding_count == validation.bindings.size();
        validation.status = descriptor_set_is_valid
            ? vulkan_descriptor_validation_status::valid
            : vulkan_descriptor_validation_status::invalid_layout;
    }

    return validation;
}

void append_descriptor_validation_snapshot(
    vulkan_backend_descriptor_validation_state& state,
    vulkan_descriptor_set_validation_snapshot snapshot)
{
    ++state.descriptor_set_count;
    state.requested_binding_count += snapshot.actual_binding_count;
    for (const vulkan_descriptor_binding_validation_snapshot& binding : snapshot.bindings) {
        if (binding.completed()) {
            ++state.valid_binding_count;
        } else {
            ++state.invalid_binding_count;
        }
    }

    if (snapshot.completed()) {
        ++state.valid_descriptor_set_count;
    } else {
        ++state.invalid_descriptor_set_count;
        if (!state.missing_required_resource && !state.duplicate_binding && !state.invalid_layout) {
            state.failed_batch_kind = snapshot.batch_kind;
            state.failed_command_index = snapshot.command_index;
            state.failed_binding_kind = snapshot.failed_binding_kind;
            state.failed_binding = snapshot.failed_binding;
        }
    }

    state.missing_required_resource =
        state.missing_required_resource || snapshot.missing_required_resource;
    state.duplicate_binding = state.duplicate_binding || snapshot.duplicate_binding;
    state.invalid_layout = state.invalid_layout || snapshot.invalid_layout;
    state.descriptor_sets.push_back(std::move(snapshot));
}

vulkan_backend_descriptor_validation_state build_descriptor_validation_state(
    const std::vector<vulkan_batch_resource_binding_snapshot>& batches,
    std::size_t planned_batch_count)
{
    vulkan_backend_descriptor_validation_state state{
        .checked = true,
        .missing_required_resource = false,
        .duplicate_binding = false,
        .invalid_layout = false,
        .failed_batch_kind = vulkan_batch_kind::quad,
        .failed_command_index = 0,
        .failed_binding_kind = vulkan_resource_binding_kind::batch_uniform,
        .failed_binding = 0,
        .planned_batch_count = planned_batch_count,
        .descriptor_set_count = 0,
        .valid_descriptor_set_count = 0,
        .invalid_descriptor_set_count = 0,
        .requested_binding_count = 0,
        .valid_binding_count = 0,
        .invalid_binding_count = 0,
        .descriptor_sets = {},
    };
    state.descriptor_sets.reserve(batches.size());

    for (const vulkan_batch_resource_binding_snapshot& batch : batches) {
        append_descriptor_validation_snapshot(
            state,
            make_descriptor_set_validation_snapshot(batch));
    }

    return state;
}

std::string image_sampler_resource_id(const render_draw_command& command)
{
    return "image_sampler:"
        + std::to_string(static_cast<int>(command.image.sampler.min_filter))
        + ":" + std::to_string(static_cast<int>(command.image.sampler.mag_filter))
        + ":" + std::to_string(static_cast<int>(command.image.sampler.mipmap_mode))
        + ":" + std::to_string(static_cast<int>(command.image.sampler.wrap_u))
        + ":" + std::to_string(static_cast<int>(command.image.sampler.wrap_v));
}

vulkan_batch_resource_binding_snapshot make_batch_resource_binding_snapshot(
    const render_draw_list& draw_list,
    const vulkan_draw_batch& batch)
{
    vulkan_batch_resource_binding_snapshot snapshot{
        .batch_kind = batch.kind,
        .command_index = batch.command_index,
        .node_id = batch.node_id,
        .descriptor_set_count = 0,
        .binding_count = 0,
        .missing_resource = false,
        .missing_binding_kind = vulkan_resource_binding_kind::batch_uniform,
        .missing_resource_id = {},
        .bindings = {},
    };

    append_binding(
        snapshot,
        make_resource_binding(
            0,
            vulkan_resource_binding_kind::batch_uniform,
            resource_id("batch_uniform", batch)));

    const render_draw_command* command = source_command_for_batch(draw_list, batch);
    switch (batch.kind) {
    case vulkan_batch_kind::quad:
    case vulkan_batch_kind::debug_bounds:
        append_binding(
            snapshot,
            make_resource_binding(
                1,
                vulkan_resource_binding_kind::quad_vertex_buffer,
                resource_id("quad_vertices", batch)));
        break;
    case vulkan_batch_kind::image:
        append_binding(
            snapshot,
            make_resource_binding(
                1,
                vulkan_resource_binding_kind::image_texture,
                command == nullptr ? std::string{} : command->image.uri));
        append_binding(
            snapshot,
            make_resource_binding(
                2,
                vulkan_resource_binding_kind::image_sampler,
                command == nullptr ? std::string{} : image_sampler_resource_id(*command)));
        break;
    case vulkan_batch_kind::text: {
        const std::size_t payload_size = command == nullptr ? 0 : text_payload_size(*command);
        append_binding(
            snapshot,
            make_resource_binding(
                1,
                vulkan_resource_binding_kind::text_run_buffer,
                payload_size == 0 ? std::string{} : resource_id("text_runs", batch)));
        append_binding(
            snapshot,
            make_resource_binding(
                2,
                vulkan_resource_binding_kind::text_glyph_atlas,
                command == nullptr || payload_size == 0
                    ? std::string{}
                    : std::string{"glyph_atlas:"} + first_text_style_or_fallback(*command)));
        break;
    }
    }

    return snapshot;
}

vulkan_frame_sync_token_id make_frame_sync_token(
    const vulkan_frame_in_flight_id& frame,
    std::size_t slot,
    vulkan_frame_sync_token_kind kind)
{
    return vulkan_frame_sync_token_id{
        .value = (frame.sequence * 100) + (frame.index * 10) + slot,
        .kind = kind,
    };
}

vulkan_backend_frame_sync_state make_diagnostic_frame_sync_state()
{
    const vulkan_frame_in_flight_id frame{
        .index = 0,
        .frame_count = 2,
        .sequence = 1,
    };

    const vulkan_frame_sync_token_id image_available = make_frame_sync_token(
        frame,
        1,
        vulkan_frame_sync_token_kind::semaphore);
    const vulkan_frame_sync_token_id acquire_fence = make_frame_sync_token(
        frame,
        2,
        vulkan_frame_sync_token_kind::fence);
    const vulkan_frame_sync_token_id render_finished = make_frame_sync_token(
        frame,
        3,
        vulkan_frame_sync_token_kind::semaphore);
    const vulkan_frame_sync_token_id frame_fence = make_frame_sync_token(
        frame,
        4,
        vulkan_frame_sync_token_kind::fence);

    return vulkan_backend_frame_sync_state{
        .frame = frame,
        .acquire_signal_image_available_semaphore = vulkan_frame_sync_signal_state{
            .token = image_available,
        },
        .acquire_signal_fence = vulkan_frame_sync_signal_state{
            .token = acquire_fence,
        },
        .submit_wait_image_available_semaphore = vulkan_frame_sync_wait_state{
            .token = image_available,
        },
        .submit_signal_render_finished_semaphore = vulkan_frame_sync_signal_state{
            .token = render_finished,
        },
        .submit_signal_frame_fence = vulkan_frame_sync_signal_state{
            .token = frame_fence,
        },
        .present_wait_render_finished_semaphore = vulkan_frame_sync_wait_state{
            .token = render_finished,
        },
    };
}

void request_signal(vulkan_frame_sync_signal_state& state)
{
    state.requested = true;
    state.status = vulkan_frame_sync_signal_status::pending;
}

void complete_signal(vulkan_frame_sync_signal_state& state, bool success)
{
    state.status = success
        ? vulkan_frame_sync_signal_status::signaled
        : vulkan_frame_sync_signal_status::failed;
}

void request_wait(vulkan_frame_sync_wait_state& state)
{
    state.requested = true;
    state.status = vulkan_frame_sync_wait_status::pending;
}

void complete_wait(vulkan_frame_sync_wait_state& state, bool success)
{
    state.status = success
        ? vulkan_frame_sync_wait_status::waited
        : vulkan_frame_sync_wait_status::failed;
}

vulkan_command_buffer_id make_command_buffer_id(const vulkan_frame_in_flight_id& frame)
{
    return vulkan_command_buffer_id{
        .value = (frame.sequence * 1000) + (frame.index * 10) + 1,
    };
}

vulkan_backend_frame_resource_lifetime_state make_frame_resource_lifetime_state(
    const vulkan_frame_plan& plan)
{
    return vulkan_backend_frame_resource_lifetime_state{
        .checked = true,
        .fallback_cleanup = false,
        .planned_batch_count = plan.batches.size(),
        .tracked_resource_count = 0,
        .acquired_resource_count = 0,
        .released_resource_count = 0,
        .pending_resource_count = 0,
        .fallback_release_count = 0,
        .resources = {},
    };
}

std::string frame_resource_id(
    std::string prefix,
    std::size_t value)
{
    prefix += ":";
    prefix += std::to_string(value);
    return prefix;
}

std::string descriptor_set_resource_id(
    const vulkan_batch_resource_binding_snapshot& snapshot)
{
    return "descriptor_set:"
        + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
}

void refresh_frame_resource_lifetime_counts(
    vulkan_backend_frame_resource_lifetime_state& state)
{
    state.tracked_resource_count = state.resources.size();
    state.acquired_resource_count = 0;
    state.released_resource_count = 0;
    state.pending_resource_count = 0;
    state.fallback_release_count = 0;

    for (const vulkan_frame_resource_lifetime_snapshot& resource : state.resources) {
        if (resource.acquired) {
            ++state.acquired_resource_count;
        }
        if (resource.released) {
            ++state.released_resource_count;
        }
        if (resource.pending()) {
            ++state.pending_resource_count;
        }
        if (resource.release_stage == vulkan_frame_resource_release_stage::fallback_cleanup) {
            ++state.fallback_release_count;
        }
    }
}

void track_frame_resource(
    vulkan_backend_frame_resource_lifetime_state& state,
    vulkan_frame_resource_lifetime_snapshot resource)
{
    resource.acquired = true;
    state.resources.push_back(std::move(resource));
    refresh_frame_resource_lifetime_counts(state);
}

void track_descriptor_set_resources(
    vulkan_backend_frame_resource_lifetime_state& state,
    const vulkan_backend_resource_binding_state& resource_bindings)
{
    for (const vulkan_batch_resource_binding_snapshot& snapshot : resource_bindings.batch_snapshots) {
        if (!snapshot.completed() || snapshot.descriptor_set_count == 0) {
            continue;
        }
        track_frame_resource(
            state,
            vulkan_frame_resource_lifetime_snapshot{
                .kind = vulkan_frame_resource_kind::descriptor_set,
                .resource_id = descriptor_set_resource_id(snapshot),
                .command_index = snapshot.command_index,
            });
    }
}

void track_swapchain_image_resource(
    vulkan_backend_frame_resource_lifetime_state& state,
    vulkan_swapchain_image_id image_id)
{
    track_frame_resource(
        state,
        vulkan_frame_resource_lifetime_snapshot{
            .kind = vulkan_frame_resource_kind::swapchain_image,
            .resource_id = frame_resource_id("swapchain_image", image_id.value),
        });
}

void track_command_buffer_resource(
    vulkan_backend_frame_resource_lifetime_state& state,
    vulkan_command_buffer_id command_buffer)
{
    if (!command_buffer.valid()) {
        refresh_frame_resource_lifetime_counts(state);
        return;
    }

    track_frame_resource(
        state,
        vulkan_frame_resource_lifetime_snapshot{
            .kind = vulkan_frame_resource_kind::command_buffer,
            .resource_id = frame_resource_id("command_buffer", command_buffer.value),
        });
}

void release_frame_resources(
    vulkan_backend_frame_resource_lifetime_state& state,
    vulkan_frame_resource_release_stage release_stage)
{
    if (!state.checked) {
        return;
    }
    if (release_stage == vulkan_frame_resource_release_stage::fallback_cleanup) {
        state.fallback_cleanup = true;
    }

    for (vulkan_frame_resource_lifetime_snapshot& resource : state.resources) {
        if (!resource.acquired || resource.released) {
            continue;
        }
        resource.released = true;
        resource.release_stage = release_stage;
    }

    refresh_frame_resource_lifetime_counts(state);
}

vulkan_backend_command_buffer_submit_state make_command_buffer_submit_state(
    const vulkan_frame_in_flight_id& frame,
    std::size_t planned_batch_count)
{
    const vulkan_command_buffer_id command_buffer = make_command_buffer_id(frame);
    return vulkan_backend_command_buffer_submit_state{
        .checked = true,
        .recording = vulkan_command_buffer_recording_diagnostics{
            .command_buffer = command_buffer,
            .planned_batch_count = planned_batch_count,
        },
        .submit = vulkan_frame_submit_diagnostics{
            .command_buffer = command_buffer,
            .frame = frame,
        },
    };
}

void mark_command_buffer_recording_started(vulkan_backend_command_buffer_submit_state& state)
{
    state.recording.begin_requested = true;
    state.recording.status = vulkan_command_buffer_recording_status::recording;
}

void mark_command_buffer_recording_failed(
    vulkan_backend_command_buffer_submit_state& state,
    const vulkan_backend_command_recorder_state& recorder)
{
    state.recording.status = vulkan_command_buffer_recording_status::failed;
    state.recording.finish_requested =
        recorder.failure_stage == vulkan_command_recorder_failure_stage::finish_recording;
    state.recording.recorded_batch_count = recorder.recorded_batch_count;
    state.recording.failure_stage = recorder.failure_stage;
    state.recording.failure_recording_index = recorder.failure_recording_index;
}

void mark_command_buffer_recording_finished(
    vulkan_backend_command_buffer_submit_state& state,
    const vulkan_backend_command_recorder_state& recorder)
{
    state.recording.finish_requested = true;
    state.recording.recorded_batch_count = recorder.recorded_batch_count;
    state.recording.failure_stage = recorder.failure_stage;
    state.recording.failure_recording_index = recorder.failure_recording_index;
    state.recording.status = recorder.failed()
        ? vulkan_command_buffer_recording_status::failed
        : vulkan_command_buffer_recording_status::recorded;
}

void mark_frame_submit_requested(
    vulkan_backend_command_buffer_submit_state& state,
    std::size_t submitted_batch_count)
{
    state.submit.submit_requested = true;
    state.submit.submitted_batch_count = submitted_batch_count;
    state.submit.status = vulkan_frame_submit_status::pending;
    state.submit.wait_image_available_status = vulkan_frame_sync_wait_status::pending;
    state.submit.signal_render_finished_status = vulkan_frame_sync_signal_status::pending;
    state.submit.signal_frame_fence_status = vulkan_frame_sync_signal_status::pending;
}

void mark_frame_submit_result(
    vulkan_backend_command_buffer_submit_state& state,
    const vulkan_backend_frame_sync_state& sync,
    bool success)
{
    state.submit.status = success
        ? vulkan_frame_submit_status::submitted
        : vulkan_frame_submit_status::failed;
    state.submit.wait_image_available_status =
        sync.submit_wait_image_available_semaphore.status;
    state.submit.signal_render_finished_status =
        sync.submit_signal_render_finished_semaphore.status;
    state.submit.signal_frame_fence_status = sync.submit_signal_frame_fence.status;
}

struct resource_registry_descriptor_key {
    std::size_t set = 0;
    std::size_t binding = 0;
    vulkan_resource_binding_kind kind = vulkan_resource_binding_kind::batch_uniform;
    std::string resource_id;
};

bool same_descriptor_key(
    const resource_registry_descriptor_key& left,
    const resource_registry_descriptor_key& right)
{
    return left.set == right.set
        && left.binding == right.binding
        && left.kind == right.kind
        && left.resource_id == right.resource_id;
}

bool descriptor_key_was_seen(
    const std::vector<resource_registry_descriptor_key>& keys,
    const resource_registry_descriptor_key& candidate)
{
    for (const resource_registry_descriptor_key& key : keys) {
        if (same_descriptor_key(key, candidate)) {
            return true;
        }
    }
    return false;
}

vulkan_resource_registry_entry* find_registry_entry(
    std::vector<vulkan_resource_registry_entry>& resources,
    vulkan_resource_binding_kind kind,
    const std::string& resource_id)
{
    for (vulkan_resource_registry_entry& entry : resources) {
        if (entry.kind == kind && entry.resource_id == resource_id) {
            return &entry;
        }
    }
    return nullptr;
}

vulkan_descriptor_validation_status descriptor_validation_status_for(
    const vulkan_backend_descriptor_validation_state& validation)
{
    if (!validation.checked) {
        return vulkan_descriptor_validation_status::not_checked;
    }
    for (const vulkan_descriptor_set_validation_snapshot& descriptor_set :
         validation.descriptor_sets) {
        if (!descriptor_set.completed()) {
            return descriptor_set.status;
        }
    }
    return validation.completed()
        ? vulkan_descriptor_validation_status::valid
        : vulkan_descriptor_validation_status::invalid_layout;
}

vulkan_command_recorder_gate_state make_command_recorder_gate_state(
    const vulkan_backend_resource_binding_state& resource_bindings,
    const vulkan_backend_resource_registry_state& resource_registry)
{
    const vulkan_backend_descriptor_validation_state& validation =
        resource_bindings.descriptor_validation;
    vulkan_command_recorder_gate_state state{
        .checked = true,
        .recording_allowed = false,
        .resource_bindings_checked = resource_bindings.checked,
        .resource_bindings_completed = resource_bindings.completed(),
        .descriptor_validation_checked = validation.checked,
        .descriptor_validation_completed = validation.completed(),
        .resource_registry_checked = resource_registry.checked,
        .resource_registry_completed = resource_registry.completed(),
        .missing_required_resource = validation.missing_required_resource,
        .duplicate_binding = validation.duplicate_binding,
        .invalid_layout = validation.invalid_layout,
        .status = vulkan_command_recorder_gate_status::not_checked,
        .descriptor_status = descriptor_validation_status_for(validation),
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .blocked_batch_kind = validation.failed_batch_kind,
        .blocked_command_index = validation.failed_command_index,
        .blocked_binding_kind = validation.failed_binding_kind,
        .blocked_binding = validation.failed_binding,
        .blocked_resource_id = resource_bindings.missing_resource_id,
        .planned_batch_count = resource_bindings.planned_batch_count,
        .descriptor_set_count = validation.descriptor_set_count,
        .invalid_descriptor_set_count = validation.invalid_descriptor_set_count,
        .missing_resource_count = resource_registry.missing_resource_count,
    };

    if (resource_bindings.completed() && resource_registry.completed()) {
        state.recording_allowed = true;
        state.status = vulkan_command_recorder_gate_status::allowed;
        state.descriptor_status = vulkan_descriptor_validation_status::valid;
        state.fallback_reason = vulkan_backend_fallback_reason::none;
        return state;
    }

    state.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
    if (validation.checked && !validation.completed()) {
        state.status = vulkan_command_recorder_gate_status::blocked_by_descriptor_validation;
        return state;
    }
    if (resource_bindings.checked && !resource_bindings.completed()) {
        state.status = vulkan_command_recorder_gate_status::blocked_by_resource_binding;
        state.blocked_batch_kind = resource_bindings.missing_batch_kind;
        state.blocked_command_index = resource_bindings.missing_command_index;
        state.blocked_binding_kind = resource_bindings.missing_binding_kind;
        state.blocked_binding = 0;
        return state;
    }

    state.status = vulkan_command_recorder_gate_status::blocked_by_resource_registry;
    if (!resource_registry.missing_resources.empty()) {
        const vulkan_resource_registry_missing_resource& missing =
            resource_registry.missing_resources.front();
        state.blocked_batch_kind = missing.batch_kind;
        state.blocked_command_index = missing.command_index;
        state.blocked_binding_kind = missing.kind;
        state.blocked_binding = missing.binding;
        state.blocked_resource_id = missing.resource_id;
    }
    return state;
}

} // namespace

std::string_view fallback_reason_name(vulkan_backend_fallback_reason reason)
{
    switch (reason) {
    case vulkan_backend_fallback_reason::none:
        return "none";
    case vulkan_backend_fallback_reason::not_requested:
        return "not_requested";
    case vulkan_backend_fallback_reason::instance_unavailable:
        return "instance_unavailable";
    case vulkan_backend_fallback_reason::device_unavailable:
        return "device_unavailable";
    case vulkan_backend_fallback_reason::swapchain_unavailable:
        return "swapchain_unavailable";
    case vulkan_backend_fallback_reason::pipeline_unavailable:
        return "pipeline_unavailable";
    case vulkan_backend_fallback_reason::command_recorder_unavailable:
        return "command_recorder_unavailable";
    case vulkan_backend_fallback_reason::surface_unavailable:
        return "surface_unavailable";
    case vulkan_backend_fallback_reason::viewport_unavailable:
        return "viewport_unavailable";
    case vulkan_backend_fallback_reason::begin_frame_failed:
        return "begin_frame_failed";
    case vulkan_backend_fallback_reason::acquire_image_failed:
        return "acquire_image_failed";
    case vulkan_backend_fallback_reason::resource_binding_unavailable:
        return "resource_binding_unavailable";
    case vulkan_backend_fallback_reason::record_commands_failed:
        return "record_commands_failed";
    case vulkan_backend_fallback_reason::submit_frame_failed:
        return "submit_frame_failed";
    case vulkan_backend_fallback_reason::present_image_failed:
        return "present_image_failed";
    case vulkan_backend_fallback_reason::present_frame_failed:
        return "present_frame_failed";
    }

    return "unknown";
}

std::string_view frame_stage_name(vulkan_backend_frame_stage stage)
{
    switch (stage) {
    case vulkan_backend_frame_stage::not_started:
        return "not_started";
    case vulkan_backend_frame_stage::backend_attempted:
        return "backend_attempted";
    case vulkan_backend_frame_stage::lifecycle_ready:
        return "lifecycle_ready";
    case vulkan_backend_frame_stage::surface_extent_ready:
        return "surface_extent_ready";
    case vulkan_backend_frame_stage::frame_plan_ready:
        return "frame_plan_ready";
    case vulkan_backend_frame_stage::frame_begun:
        return "frame_begun";
    case vulkan_backend_frame_stage::commands_recorded:
        return "commands_recorded";
    case vulkan_backend_frame_stage::frame_submitted:
        return "frame_submitted";
    case vulkan_backend_frame_stage::frame_presented:
        return "frame_presented";
    }

    return "unknown";
}

std::string_view frame_lifecycle_step_name(vulkan_frame_lifecycle_step step)
{
    switch (step) {
    case vulkan_frame_lifecycle_step::acquire:
        return "acquire";
    case vulkan_frame_lifecycle_step::begin:
        return "begin";
    case vulkan_frame_lifecycle_step::render:
        return "render";
    case vulkan_frame_lifecycle_step::submit:
        return "submit";
    case vulkan_frame_lifecycle_step::present:
        return "present";
    }

    return "unknown";
}

std::string_view frame_lifecycle_order_status_name(vulkan_frame_lifecycle_order_status status)
{
    switch (status) {
    case vulkan_frame_lifecycle_order_status::not_started:
        return "not_started";
    case vulkan_frame_lifecycle_order_status::started:
        return "started";
    case vulkan_frame_lifecycle_order_status::completed:
        return "completed";
    case vulkan_frame_lifecycle_order_status::skipped:
        return "skipped";
    case vulkan_frame_lifecycle_order_status::failed:
        return "failed";
    case vulkan_frame_lifecycle_order_status::out_of_order:
        return "out_of_order";
    }

    return "unknown";
}

std::string_view frame_lifecycle_failure_classification_name(
    vulkan_frame_lifecycle_failure_classification classification)
{
    switch (classification) {
    case vulkan_frame_lifecycle_failure_classification::none:
        return "none";
    case vulkan_frame_lifecycle_failure_classification::recoverable:
        return "recoverable";
    case vulkan_frame_lifecycle_failure_classification::fatal:
        return "fatal";
    }

    return "unknown";
}

std::string_view swapchain_acquire_status_name(vulkan_swapchain_acquire_status status)
{
    switch (status) {
    case vulkan_swapchain_acquire_status::not_requested:
        return "not_requested";
    case vulkan_swapchain_acquire_status::acquired:
        return "acquired";
    case vulkan_swapchain_acquire_status::backpressured:
        return "backpressured";
    case vulkan_swapchain_acquire_status::failed:
        return "failed";
    }

    return "unknown";
}

std::string_view swapchain_present_status_name(vulkan_swapchain_present_status status)
{
    switch (status) {
    case vulkan_swapchain_present_status::not_requested:
        return "not_requested";
    case vulkan_swapchain_present_status::presented:
        return "presented";
    case vulkan_swapchain_present_status::failed:
        return "failed";
    }

    return "unknown";
}

std::string_view swapchain_present_mode_name(vulkan_swapchain_present_mode mode)
{
    switch (mode) {
    case vulkan_swapchain_present_mode::immediate:
        return "immediate";
    case vulkan_swapchain_present_mode::mailbox:
        return "mailbox";
    case vulkan_swapchain_present_mode::fifo:
        return "fifo";
    case vulkan_swapchain_present_mode::fifo_relaxed:
        return "fifo_relaxed";
    }

    return "unknown";
}

std::string_view frame_acquire_policy_status_name(vulkan_frame_acquire_policy_status status)
{
    switch (status) {
    case vulkan_frame_acquire_policy_status::not_checked:
        return "not_checked";
    case vulkan_frame_acquire_policy_status::not_requested:
        return "not_requested";
    case vulkan_frame_acquire_policy_status::acquired:
        return "acquired";
    case vulkan_frame_acquire_policy_status::backpressured:
        return "backpressured";
    case vulkan_frame_acquire_policy_status::failed:
        return "failed";
    }

    return "unknown";
}

std::string_view frame_present_result_status_name(vulkan_frame_present_result_status status)
{
    switch (status) {
    case vulkan_frame_present_result_status::not_checked:
        return "not_checked";
    case vulkan_frame_present_result_status::not_requested:
        return "not_requested";
    case vulkan_frame_present_result_status::image_presented:
        return "image_presented";
    case vulkan_frame_present_result_status::frame_presented:
        return "frame_presented";
    case vulkan_frame_present_result_status::image_failed:
        return "image_failed";
    case vulkan_frame_present_result_status::frame_failed:
        return "frame_failed";
    }

    return "unknown";
}

std::string_view frame_resource_kind_name(vulkan_frame_resource_kind kind)
{
    switch (kind) {
    case vulkan_frame_resource_kind::swapchain_image:
        return "swapchain_image";
    case vulkan_frame_resource_kind::command_buffer:
        return "command_buffer";
    case vulkan_frame_resource_kind::descriptor_set:
        return "descriptor_set";
    }

    return "unknown";
}

std::string_view frame_resource_release_stage_name(vulkan_frame_resource_release_stage stage)
{
    switch (stage) {
    case vulkan_frame_resource_release_stage::none:
        return "none";
    case vulkan_frame_resource_release_stage::after_present:
        return "after_present";
    case vulkan_frame_resource_release_stage::fallback_cleanup:
        return "fallback_cleanup";
    }

    return "unknown";
}

std::string_view descriptor_validation_status_name(vulkan_descriptor_validation_status status)
{
    switch (status) {
    case vulkan_descriptor_validation_status::not_checked:
        return "not_checked";
    case vulkan_descriptor_validation_status::valid:
        return "valid";
    case vulkan_descriptor_validation_status::missing_required_resource:
        return "missing_required_resource";
    case vulkan_descriptor_validation_status::duplicate_binding:
        return "duplicate_binding";
    case vulkan_descriptor_validation_status::invalid_layout:
        return "invalid_layout";
    }

    return "unknown";
}

std::string_view command_recorder_failure_stage_name(vulkan_command_recorder_failure_stage stage)
{
    switch (stage) {
    case vulkan_command_recorder_failure_stage::none:
        return "none";
    case vulkan_command_recorder_failure_stage::begin_recording:
        return "begin_recording";
    case vulkan_command_recorder_failure_stage::record_draw_batch:
        return "record_draw_batch";
    case vulkan_command_recorder_failure_stage::finish_recording:
        return "finish_recording";
    }

    return "unknown";
}

std::string_view command_recorder_gate_status_name(vulkan_command_recorder_gate_status status)
{
    switch (status) {
    case vulkan_command_recorder_gate_status::not_checked:
        return "not_checked";
    case vulkan_command_recorder_gate_status::allowed:
        return "allowed";
    case vulkan_command_recorder_gate_status::blocked_by_descriptor_validation:
        return "blocked_by_descriptor_validation";
    case vulkan_command_recorder_gate_status::blocked_by_resource_binding:
        return "blocked_by_resource_binding";
    case vulkan_command_recorder_gate_status::blocked_by_resource_registry:
        return "blocked_by_resource_registry";
    }

    return "unknown";
}

std::string_view command_buffer_recording_status_name(
    vulkan_command_buffer_recording_status status)
{
    switch (status) {
    case vulkan_command_buffer_recording_status::not_started:
        return "not_started";
    case vulkan_command_buffer_recording_status::recording:
        return "recording";
    case vulkan_command_buffer_recording_status::recorded:
        return "recorded";
    case vulkan_command_buffer_recording_status::failed:
        return "failed";
    }

    return "unknown";
}

std::string_view frame_submit_status_name(vulkan_frame_submit_status status)
{
    switch (status) {
    case vulkan_frame_submit_status::not_requested:
        return "not_requested";
    case vulkan_frame_submit_status::pending:
        return "pending";
    case vulkan_frame_submit_status::submitted:
        return "submitted";
    case vulkan_frame_submit_status::failed:
        return "failed";
    }

    return "unknown";
}

std::string_view shader_stage_name(vulkan_shader_stage stage)
{
    switch (stage) {
    case vulkan_shader_stage::vertex:
        return "vertex";
    case vulkan_shader_stage::fragment:
        return "fragment";
    }

    return "unknown";
}

std::string_view pipeline_lifecycle_stage_name(vulkan_pipeline_lifecycle_stage stage)
{
    switch (stage) {
    case vulkan_pipeline_lifecycle_stage::render_pass:
        return "render_pass";
    case vulkan_pipeline_lifecycle_stage::shader_stages:
        return "shader_stages";
    case vulkan_pipeline_lifecycle_stage::pipeline:
        return "pipeline";
    }

    return "unknown";
}

std::string_view pipeline_lifecycle_status_name(vulkan_pipeline_lifecycle_status status)
{
    switch (status) {
    case vulkan_pipeline_lifecycle_status::not_checked:
        return "not_checked";
    case vulkan_pipeline_lifecycle_status::ready:
        return "ready";
    case vulkan_pipeline_lifecycle_status::unavailable:
        return "unavailable";
    }

    return "unknown";
}

bool vulkan_backend_shader_registry_state::contains(
    const vulkan_shader_module_id& id,
    vulkan_shader_stage stage) const
{
    for (const vulkan_shader_module_descriptor& module : modules) {
        if (module.id.value == id.value && module.stage == stage && module.valid()) {
            return true;
        }
    }
    return false;
}

diagnostic_vulkan_shader_registry::diagnostic_vulkan_shader_registry()
    : diagnostic_vulkan_shader_registry(make_default_shader_modules())
{
}

diagnostic_vulkan_shader_registry::diagnostic_vulkan_shader_registry(
    std::vector<vulkan_shader_module_descriptor> modules)
{
    state_.modules = std::move(modules);
    state_.registered_shader_count = state_.modules.size();
}

bool diagnostic_vulkan_shader_registry::require_shader(
    vulkan_batch_kind kind,
    vulkan_shader_stage stage,
    const vulkan_shader_module_id& id)
{
    state_.registry_checked = true;
    if (state_.contains(id, stage)) {
        return true;
    }

    state_.missing_shader = true;
    state_.missing_batch_kind = kind;
    state_.missing_stage = stage;
    state_.missing_shader_id = id;
    return false;
}

const vulkan_backend_shader_registry_state& diagnostic_vulkan_shader_registry::registry_state() const
{
    return state_;
}

bool vulkan_backend_pipeline_state::supports(vulkan_batch_kind kind) const
{
    for (const vulkan_pipeline_capability& capability : capabilities) {
        if (capability.kind == kind) {
            return capability.available;
        }
    }
    return false;
}

const vulkan_pipeline_descriptor* vulkan_backend_pipeline_state::descriptor_for(vulkan_batch_kind kind) const
{
    return find_pipeline_descriptor(pipeline_descriptors, kind);
}

bool vulkan_backend_pipeline_state::completed() const
{
    return ready && !missing_pipeline && lifecycle.completed()
        && compatibility.completed()
        && shader_bindings.completed();
}

vulkan_backend_lifecycle_readiness null_vulkan_backend_device::current_lifecycle_readiness() const
{
    return {};
}

vulkan_surface_extent null_vulkan_backend_device::current_surface_extent() const
{
    return {};
}

bool null_vulkan_backend_device::begin_frame(vulkan_surface_extent surface)
{
    static_cast<void>(surface);
    return false;
}

vulkan_swapchain_acquire_result null_vulkan_backend_device::acquire_next_image(vulkan_surface_extent surface)
{
    static_cast<void>(surface);
    return vulkan_swapchain_acquire_result{
        .status = vulkan_swapchain_acquire_status::failed,
        .image = {},
    };
}

bool null_vulkan_backend_device::record_frame_commands(const vulkan_frame_plan& plan)
{
    static_cast<void>(plan);
    return false;
}

bool null_vulkan_backend_device::submit_frame()
{
    return false;
}

vulkan_swapchain_present_result null_vulkan_backend_device::present_image(
    vulkan_swapchain_image_id image_id)
{
    return vulkan_swapchain_present_result{
        .status = vulkan_swapchain_present_status::failed,
        .image_id = image_id,
    };
}

bool null_vulkan_backend_device::present_frame()
{
    return false;
}

diagnostic_vulkan_pipeline_cache::diagnostic_vulkan_pipeline_cache()
    : diagnostic_vulkan_pipeline_cache(diagnostic_vulkan_pipeline_cache_options{})
{
}

diagnostic_vulkan_pipeline_cache::diagnostic_vulkan_pipeline_cache(
    diagnostic_vulkan_pipeline_cache_options options)
    : options_(std::move(options))
{
    std::vector<vulkan_shader_module_descriptor> shader_modules;
    if (options_.use_default_shader_modules) {
        shader_modules = make_default_shader_modules();
    }
    append_shader_modules(shader_modules, std::move(options_.shader_modules));
    shader_registry_ = diagnostic_vulkan_shader_registry(std::move(shader_modules));
    state_.shader_registry = shader_registry_.registry_state();

    state_.capabilities = make_pipeline_capabilities(options_.default_available);
    apply_pipeline_overrides(state_.capabilities, options_.overrides);
    state_.cache_entries = make_pipeline_cache_entries(state_.capabilities);
    if (options_.use_default_pipeline_descriptors) {
        state_.pipeline_descriptors = make_default_pipeline_descriptors();
    }
    apply_pipeline_descriptor_overrides(
        state_.pipeline_descriptors,
        std::move(options_.pipeline_descriptors));
    state_.lifecycle.checked = true;
    state_.lifecycle.render_pass = options_.render_pass;
    state_.lifecycle.missing_render_pass = !state_.lifecycle.render_pass.valid();
    state_.compatibility.checked = true;
    state_.shader_bindings.checked = true;
    state_.ready = state_.lifecycle.render_pass_ready();
}

bool diagnostic_vulkan_pipeline_cache::ensure_pipeline(const vulkan_draw_batch& batch)
{
    state_.cache_checked = true;
    ++state_.requested_pipeline_count;
    ++state_.lifecycle.requested_pipeline_count;
    const vulkan_pipeline_descriptor* descriptor = state_.descriptor_for(batch.kind);
    append_pipeline_compatibility_key(
        state_,
        make_pipeline_compatibility_key(
            batch,
            state_.lifecycle.render_pass,
            descriptor));

    vulkan_pipeline_lifecycle_snapshot lifecycle_snapshot = make_pipeline_lifecycle_snapshot(
        batch,
        state_.lifecycle.render_pass_ready()
            ? vulkan_pipeline_lifecycle_status::ready
            : vulkan_pipeline_lifecycle_status::unavailable);
    if (!state_.lifecycle.render_pass_ready()) {
        state_.missing_pipeline = true;
        state_.ready = false;
        state_.missing_batch_kind = batch.kind;
        state_.missing_command_index = batch.command_index;
        mark_pipeline_lifecycle_failure(
            state_,
            std::move(lifecycle_snapshot),
            vulkan_pipeline_lifecycle_stage::render_pass);
        return false;
    }

    vulkan_pipeline_cache_entry* entry = find_pipeline_entry(state_.cache_entries, batch.kind);
    if (entry == nullptr) {
        state_.missing_pipeline = true;
        state_.ready = false;
        state_.missing_batch_kind = batch.kind;
        state_.missing_command_index = batch.command_index;
        lifecycle_snapshot.shader_stage_status = vulkan_pipeline_lifecycle_status::not_checked;
        mark_pipeline_lifecycle_failure(
            state_,
            std::move(lifecycle_snapshot),
            vulkan_pipeline_lifecycle_stage::pipeline);
        return false;
    }

    entry->requested = true;
    ++entry->request_count;
    entry->last_command_index = batch.command_index;
    if (!entry->available) {
        state_.missing_pipeline = true;
        state_.ready = false;
        state_.missing_batch_kind = batch.kind;
        state_.missing_command_index = batch.command_index;
        lifecycle_snapshot.shader_stage_status = vulkan_pipeline_lifecycle_status::not_checked;
        mark_pipeline_lifecycle_failure(
            state_,
            std::move(lifecycle_snapshot),
            vulkan_pipeline_lifecycle_stage::pipeline);
        return false;
    }

    if (descriptor == nullptr || !descriptor->complete()) {
        mark_missing_pipeline_descriptor(state_, batch);
        lifecycle_snapshot.shader_stage_status = vulkan_pipeline_lifecycle_status::not_checked;
        mark_pipeline_lifecycle_failure(
            state_,
            std::move(lifecycle_snapshot),
            vulkan_pipeline_lifecycle_stage::pipeline);
        return false;
    }
    vulkan_pipeline_shader_stage_snapshot shader_stage_snapshot =
        make_pipeline_shader_stage_snapshot(batch, *descriptor);
    if (!require_shader_binding(
            state_,
            shader_registry_,
            batch,
            vulkan_shader_stage::vertex,
            descriptor->vertex_shader)) {
        mark_missing_pipeline_shader(state_, batch, shader_registry_.registry_state());
        mark_pipeline_lifecycle_shader_failure(
            state_,
            std::move(lifecycle_snapshot),
            std::move(shader_stage_snapshot),
            vulkan_shader_stage::vertex,
            descriptor->vertex_shader);
        return false;
    }
    shader_stage_snapshot.vertex_stage_ready = true;
    if (!require_shader_binding(
            state_,
            shader_registry_,
            batch,
            vulkan_shader_stage::fragment,
            descriptor->fragment_shader)) {
        mark_missing_pipeline_shader(state_, batch, shader_registry_.registry_state());
        mark_pipeline_lifecycle_shader_failure(
            state_,
            std::move(lifecycle_snapshot),
            std::move(shader_stage_snapshot),
            vulkan_shader_stage::fragment,
            descriptor->fragment_shader);
        return false;
    }
    shader_stage_snapshot.fragment_stage_ready = true;

    state_.shader_registry = shader_registry_.registry_state();
    mark_pipeline_lifecycle_success(
        state_,
        std::move(lifecycle_snapshot),
        std::move(shader_stage_snapshot));
    state_.ready = !state_.missing_pipeline && state_.lifecycle.render_pass_ready();
    return true;
}

const vulkan_backend_pipeline_state& diagnostic_vulkan_pipeline_cache::pipeline_state() const
{
    return state_;
}

diagnostic_vulkan_command_recorder::diagnostic_vulkan_command_recorder(bool ready)
    : diagnostic_vulkan_command_recorder(diagnostic_vulkan_command_recorder_options{.ready = ready})
{
}

diagnostic_vulkan_command_recorder::diagnostic_vulkan_command_recorder(
    diagnostic_vulkan_command_recorder_options options)
    : options_(options)
{
    state_.ready = options_.ready;
}

bool diagnostic_vulkan_command_recorder::begin_recording(
    vulkan_surface_extent surface,
    std::size_t planned_batch_count)
{
    state_ = {};
    state_.ready = options_.ready;
    state_.planned_batch_count = planned_batch_count;
    if (!options_.ready || !surface.valid()
        || options_.fail_at == vulkan_command_recorder_failure_stage::begin_recording) {
        mark_recorder_failure(state_, vulkan_command_recorder_failure_stage::begin_recording, 0);
        return false;
    }

    state_.frame_open = true;
    return true;
}

bool diagnostic_vulkan_command_recorder::record_draw_batch(const vulkan_draw_batch& batch)
{
    const std::size_t recording_index = state_.recorded_batch_count;
    if (state_.failed()) {
        return false;
    }
    if (!state_.ready || !state_.frame_open || state_.command_buffer_recorded) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::record_draw_batch,
            recording_index);
        return false;
    }
    if (options_.fail_at == vulkan_command_recorder_failure_stage::record_draw_batch
        && recording_index == options_.fail_recording_index) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::record_draw_batch,
            recording_index);
        return false;
    }

    state_.recorded_batches.push_back(make_recorded_draw_batch(batch, recording_index));
    state_.recorded_batch_count = state_.recorded_batches.size();
    if (state_.recorded_batch_count > state_.planned_batch_count) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::record_draw_batch,
            recording_index);
        return false;
    }

    return state_.recorded_batch_count <= state_.planned_batch_count;
}

bool diagnostic_vulkan_command_recorder::finish_recording()
{
    if (state_.failed()) {
        return false;
    }
    if (!state_.ready || !state_.frame_open || state_.command_buffer_recorded) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::finish_recording,
            state_.recorded_batch_count);
        return false;
    }
    if (options_.fail_at == vulkan_command_recorder_failure_stage::finish_recording) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::finish_recording,
            state_.recorded_batch_count);
        return false;
    }
    if (state_.recorded_batch_count != state_.planned_batch_count) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::finish_recording,
            state_.recorded_batch_count);
        return false;
    }

    state_.command_buffer_recorded = true;
    return true;
}

const vulkan_backend_command_recorder_state& diagnostic_vulkan_command_recorder::recorder_state() const
{
    return state_;
}

vulkan_backend_resource_binding_state build_vulkan_resource_binding_state(
    const render_draw_list& draw_list,
    const vulkan_frame_plan& plan)
{
    vulkan_backend_resource_binding_state state;
    state.checked = true;
    state.planned_batch_count = plan.batches.size();
    state.batch_snapshots.reserve(plan.batches.size());

    for (const vulkan_draw_batch& batch : plan.batches) {
        vulkan_batch_resource_binding_snapshot snapshot = make_batch_resource_binding_snapshot(draw_list, batch);
        state.descriptor_set_count += snapshot.descriptor_set_count;
        state.binding_count += snapshot.binding_count;
        if (snapshot.missing_resource && !state.missing_resource) {
            state.missing_resource = true;
            state.missing_batch_kind = snapshot.batch_kind;
            state.missing_command_index = snapshot.command_index;
            state.missing_binding_kind = snapshot.missing_binding_kind;
            state.missing_resource_id = snapshot.missing_resource_id;
        }
        state.batch_snapshots.push_back(std::move(snapshot));
    }
    state.descriptor_validation = build_descriptor_validation_state(
        state.batch_snapshots,
        state.planned_batch_count);

    return state;
}

vulkan_backend_resource_registry_state build_vulkan_resource_registry_state(
    const render_draw_list& draw_list,
    const vulkan_frame_plan& plan,
    const vulkan_backend_resource_binding_state& resource_bindings)
{
    static_cast<void>(draw_list);
    static_cast<void>(plan);

    vulkan_backend_resource_registry_state state;
    state.checked = true;
    state.planned_batch_count = resource_bindings.planned_batch_count;

    std::vector<resource_registry_descriptor_key> descriptor_keys;
    descriptor_keys.reserve(resource_bindings.binding_count);

    for (const vulkan_batch_resource_binding_snapshot& batch : resource_bindings.batch_snapshots) {
        for (const vulkan_resource_binding_snapshot& binding : batch.bindings) {
            ++state.descriptor_binding_count;
            if (!binding.bound()) {
                state.missing_resources.push_back(vulkan_resource_registry_missing_resource{
                    .kind = binding.kind,
                    .batch_kind = batch.batch_kind,
                    .command_index = batch.command_index,
                    .set = binding.set,
                    .binding = binding.binding,
                    .resource_id = missing_resource_id_for(batch, binding.kind),
                });
                continue;
            }

            const resource_registry_descriptor_key descriptor_key{
                .set = binding.set,
                .binding = binding.binding,
                .kind = binding.kind,
                .resource_id = binding.resource_id,
            };
            if (descriptor_key_was_seen(descriptor_keys, descriptor_key)) {
                ++state.descriptor_reuse_count;
            } else {
                descriptor_keys.push_back(descriptor_key);
            }

            vulkan_resource_registry_entry* entry = find_registry_entry(
                state.resources,
                binding.kind,
                binding.resource_id);
            if (entry != nullptr) {
                ++state.resource_reuse_count;
                ++entry->use_count;
                entry->last_command_index = batch.command_index;
                continue;
            }

            state.resources.push_back(vulkan_resource_registry_entry{
                .kind = binding.kind,
                .resource_id = binding.resource_id,
                .first_command_index = batch.command_index,
                .last_command_index = batch.command_index,
                .use_count = 1,
            });
        }
    }

    state.registered_resource_count = state.resources.size();
    state.missing_resource_count = state.missing_resources.size();
    return state;
}

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    const render_draw_list& draw_list,
    render_rect viewport)
{
    diagnostic_vulkan_pipeline_cache pipeline_cache;
    diagnostic_vulkan_command_recorder command_recorder;
    return submit_vulkan_backend_frame(device, pipeline_cache, command_recorder, draw_list, viewport);
}

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    vulkan_command_recorder_interface& command_recorder,
    const render_draw_list& draw_list,
    render_rect viewport)
{
    diagnostic_vulkan_pipeline_cache pipeline_cache;
    return submit_vulkan_backend_frame(device, pipeline_cache, command_recorder, draw_list, viewport);
}

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    vulkan_pipeline_cache_interface& pipeline_cache,
    vulkan_command_recorder_interface& command_recorder,
    const render_draw_list& draw_list,
    render_rect viewport)
{
    vulkan_backend_frame_result result;
    result.attempted = true;
    result.reached_stage = vulkan_backend_frame_stage::backend_attempted;
    result.lifecycle = device.current_lifecycle_readiness();
    result.command_recorder.ready = result.lifecycle.command_recorder_ready;
    const vulkan_backend_fallback_reason unready_reason = first_unready_reason(result.lifecycle);
    if (unready_reason != vulkan_backend_fallback_reason::none) {
        mark_frame_fallback(result, unready_reason);
        return result;
    }
    result.lifecycle_ready = true;
    result.reached_stage = vulkan_backend_frame_stage::lifecycle_ready;

    result.surface = device.current_surface_extent();
    result.swapchain_policy = make_swapchain_policy_state(result.surface);
    if (!result.surface.valid()) {
        mark_frame_fallback(result, vulkan_backend_fallback_reason::surface_unavailable);
        return result;
    }
    result.reached_stage = vulkan_backend_frame_stage::surface_extent_ready;
    if (!has_visible_area(viewport)) {
        mark_frame_fallback(result, vulkan_backend_fallback_reason::viewport_unavailable);
        return result;
    }
    result.surface_ready = true;

    const vulkan_frame_plan plan = build_vulkan_frame_plan(
        draw_list,
        vulkan_frame_plan_options{
            .viewport = viewport,
            .surface_width = result.surface.width,
            .surface_height = result.surface.height,
        });
    result.planned_batch_count = plan.batches.size();
    result.command_recorder.planned_batch_count = plan.batches.size();
    result.clipped_draw_call_count = plan.clipped_draw_call_count;
    result.discarded_draw_call_count = plan.discarded_draw_call_count;
    result.reached_stage = vulkan_backend_frame_stage::frame_plan_ready;
    result.frame_resources = make_frame_resource_lifetime_state(plan);

    if (!ensure_frame_plan_pipelines(pipeline_cache, plan, result)) {
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        return result;
    }

    result.resource_bindings = build_vulkan_resource_binding_state(draw_list, plan);
    result.resource_registry = build_vulkan_resource_registry_state(
        draw_list,
        plan,
        result.resource_bindings);
    result.command_recorder.gate = make_command_recorder_gate_state(
        result.resource_bindings,
        result.resource_registry);
    if (result.command_recorder.gate.blocked()) {
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        mark_frame_fallback(result, vulkan_backend_fallback_reason::resource_binding_unavailable);
        return result;
    }
    track_descriptor_set_resources(result.frame_resources, result.resource_bindings);

    result.lifecycle_policy = make_frame_lifecycle_policy_state();
    result.present_policy = make_frame_present_policy_state();
    const auto fail_frame_lifecycle = [&result](
                                          vulkan_frame_lifecycle_step step,
                                          vulkan_backend_fallback_reason reason) {
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        mark_frame_fallback(result, reason);
        fail_lifecycle_step(result.lifecycle_policy, step, reason);
    };

    result.frame_sync = make_diagnostic_frame_sync_state();
    start_lifecycle_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::acquire);
    request_signal(result.frame_sync.acquire_signal_image_available_semaphore);
    request_signal(result.frame_sync.acquire_signal_fence);
    result.swapchain.acquire_requested = true;
    mark_acquire_policy_requested(result.present_policy);
    result.swapchain.acquire = device.acquire_next_image(result.surface);
    mark_acquire_policy_result(result.present_policy, result.swapchain.acquire);
    complete_signal(
        result.frame_sync.acquire_signal_image_available_semaphore,
        result.swapchain.acquire.completed());
    complete_signal(result.frame_sync.acquire_signal_fence, result.swapchain.acquire.completed());
    if (!result.swapchain.acquire.completed()) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::acquire,
            vulkan_backend_fallback_reason::acquire_image_failed);
        return result;
    }
    track_swapchain_image_resource(result.frame_resources, result.swapchain.acquire.image.id);
    complete_lifecycle_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::acquire);

    start_lifecycle_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::begin);
    result.frame_begun = device.begin_frame(result.surface);
    if (!result.frame_begun) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::begin,
            vulkan_backend_fallback_reason::begin_frame_failed);
        return result;
    }
    complete_lifecycle_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::begin);
    result.reached_stage = vulkan_backend_frame_stage::frame_begun;

    result.command_buffer_submit = make_command_buffer_submit_state(
        result.frame_sync.frame,
        plan.batches.size());
    track_command_buffer_resource(
        result.frame_resources,
        result.command_buffer_submit.recording.command_buffer);
    start_lifecycle_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::render);
    mark_command_buffer_recording_started(result.command_buffer_submit);
    if (!command_recorder.begin_recording(result.surface, plan.batches.size())) {
        const vulkan_command_recorder_gate_state command_recorder_gate =
            result.command_recorder.gate;
        result.command_recorder = command_recorder.recorder_state();
        result.command_recorder.gate = command_recorder_gate;
        mark_command_buffer_recording_failed(
            result.command_buffer_submit,
            result.command_recorder);
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::render,
            vulkan_backend_fallback_reason::record_commands_failed);
        return result;
    }
    for (const vulkan_draw_batch& batch : plan.batches) {
        if (!command_recorder.record_draw_batch(batch)) {
            const vulkan_command_recorder_gate_state command_recorder_gate =
                result.command_recorder.gate;
            result.command_recorder = command_recorder.recorder_state();
            result.command_recorder.gate = command_recorder_gate;
            mark_command_buffer_recording_failed(
                result.command_buffer_submit,
                result.command_recorder);
            fail_frame_lifecycle(
                vulkan_frame_lifecycle_step::render,
                vulkan_backend_fallback_reason::record_commands_failed);
            return result;
        }
    }
    if (!command_recorder.finish_recording()) {
        const vulkan_command_recorder_gate_state command_recorder_gate =
            result.command_recorder.gate;
        result.command_recorder = command_recorder.recorder_state();
        result.command_recorder.gate = command_recorder_gate;
        mark_command_buffer_recording_failed(
            result.command_buffer_submit,
            result.command_recorder);
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::render,
            vulkan_backend_fallback_reason::record_commands_failed);
        return result;
    }
    const vulkan_command_recorder_gate_state command_recorder_gate =
        result.command_recorder.gate;
    result.command_recorder = command_recorder.recorder_state();
    result.command_recorder.gate = command_recorder_gate;
    mark_command_buffer_recording_finished(
        result.command_buffer_submit,
        result.command_recorder);

    result.commands_recorded = device.record_frame_commands(plan);
    if (!result.commands_recorded) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::render,
            vulkan_backend_fallback_reason::record_commands_failed);
        return result;
    }
    complete_lifecycle_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::render);
    result.recorded_batch_count = plan.batches.size();
    result.reached_stage = vulkan_backend_frame_stage::commands_recorded;

    start_lifecycle_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::submit);
    request_wait(result.frame_sync.submit_wait_image_available_semaphore);
    request_signal(result.frame_sync.submit_signal_render_finished_semaphore);
    request_signal(result.frame_sync.submit_signal_frame_fence);
    mark_frame_submit_requested(result.command_buffer_submit, result.recorded_batch_count);
    result.frame_submitted = device.submit_frame();
    complete_wait(result.frame_sync.submit_wait_image_available_semaphore, result.frame_submitted);
    complete_signal(result.frame_sync.submit_signal_render_finished_semaphore, result.frame_submitted);
    complete_signal(result.frame_sync.submit_signal_frame_fence, result.frame_submitted);
    mark_frame_submit_result(
        result.command_buffer_submit,
        result.frame_sync,
        result.frame_submitted);
    if (!result.frame_submitted) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::submit,
            vulkan_backend_fallback_reason::submit_frame_failed);
        return result;
    }
    complete_lifecycle_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::submit);
    result.reached_stage = vulkan_backend_frame_stage::frame_submitted;

    start_lifecycle_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::present);
    request_wait(result.frame_sync.present_wait_render_finished_semaphore);
    result.swapchain.present_requested = true;
    mark_present_policy_image_requested(result.present_policy, result.swapchain.acquire.image.id);
    result.swapchain.present = device.present_image(result.swapchain.acquire.image.id);
    mark_present_policy_image_result(result.present_policy, result.swapchain.present);
    complete_wait(
        result.frame_sync.present_wait_render_finished_semaphore,
        result.swapchain.present.completed());
    result.swapchain.acquire.image.presented = result.swapchain.present.completed();
    if (!result.swapchain.present.completed()) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::present,
            vulkan_backend_fallback_reason::present_image_failed);
        return result;
    }

    mark_present_policy_frame_requested(result.present_policy);
    result.frame_presented = device.present_frame();
    mark_present_policy_frame_result(result.present_policy, result.frame_presented);
    if (!result.frame_presented) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::present,
            vulkan_backend_fallback_reason::present_frame_failed);
        return result;
    }
    complete_lifecycle_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::present);
    result.reached_stage = vulkan_backend_frame_stage::frame_presented;

    release_frame_resources(
        result.frame_resources,
        vulkan_frame_resource_release_stage::after_present);
    mark_frame_fallback(result, vulkan_backend_fallback_reason::none);
    return result;
}

} // namespace quiz_vulkan::render::vulkan_backend
