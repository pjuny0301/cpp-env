#include "render/vulkan/vulkan_backend_adapter.h"

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
            result.fallback_reason = vulkan_backend_fallback_reason::pipeline_unavailable;
            return false;
        }
    }

    result.pipeline = pipeline_cache.pipeline_state();
    return true;
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

std::string_view swapchain_acquire_status_name(vulkan_swapchain_acquire_status status)
{
    switch (status) {
    case vulkan_swapchain_acquire_status::not_requested:
        return "not_requested";
    case vulkan_swapchain_acquire_status::acquired:
        return "acquired";
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
    return ready && !missing_pipeline;
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
    state_.ready = true;
}

bool diagnostic_vulkan_pipeline_cache::ensure_pipeline(const vulkan_draw_batch& batch)
{
    state_.cache_checked = true;
    ++state_.requested_pipeline_count;

    vulkan_pipeline_cache_entry* entry = find_pipeline_entry(state_.cache_entries, batch.kind);
    if (entry == nullptr) {
        state_.missing_pipeline = true;
        state_.ready = false;
        state_.missing_batch_kind = batch.kind;
        state_.missing_command_index = batch.command_index;
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
        return false;
    }

    const vulkan_pipeline_descriptor* descriptor = state_.descriptor_for(batch.kind);
    if (descriptor == nullptr || !descriptor->complete()) {
        mark_missing_pipeline_descriptor(state_, batch);
        return false;
    }
    if (!shader_registry_.require_shader(batch.kind, vulkan_shader_stage::vertex, descriptor->vertex_shader)) {
        mark_missing_pipeline_shader(state_, batch, shader_registry_.registry_state());
        return false;
    }
    if (!shader_registry_.require_shader(batch.kind, vulkan_shader_stage::fragment, descriptor->fragment_shader)) {
        mark_missing_pipeline_shader(state_, batch, shader_registry_.registry_state());
        return false;
    }

    state_.shader_registry = shader_registry_.registry_state();
    state_.ready = !state_.missing_pipeline;
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
    result.fallback_reason = first_unready_reason(result.lifecycle);
    if (result.fallback_reason != vulkan_backend_fallback_reason::none) {
        return result;
    }
    result.lifecycle_ready = true;
    result.reached_stage = vulkan_backend_frame_stage::lifecycle_ready;

    result.surface = device.current_surface_extent();
    if (!result.surface.valid()) {
        result.fallback_reason = vulkan_backend_fallback_reason::surface_unavailable;
        return result;
    }
    result.reached_stage = vulkan_backend_frame_stage::surface_extent_ready;
    if (!has_visible_area(viewport)) {
        result.fallback_reason = vulkan_backend_fallback_reason::viewport_unavailable;
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

    if (!ensure_frame_plan_pipelines(pipeline_cache, plan, result)) {
        return result;
    }

    result.frame_begun = device.begin_frame(result.surface);
    if (!result.frame_begun) {
        result.fallback_reason = vulkan_backend_fallback_reason::begin_frame_failed;
        return result;
    }
    result.reached_stage = vulkan_backend_frame_stage::frame_begun;

    result.frame_sync = make_diagnostic_frame_sync_state();
    request_signal(result.frame_sync.acquire_signal_image_available_semaphore);
    request_signal(result.frame_sync.acquire_signal_fence);
    result.swapchain.acquire_requested = true;
    result.swapchain.acquire = device.acquire_next_image(result.surface);
    complete_signal(
        result.frame_sync.acquire_signal_image_available_semaphore,
        result.swapchain.acquire.completed());
    complete_signal(result.frame_sync.acquire_signal_fence, result.swapchain.acquire.completed());
    if (!result.swapchain.acquire.completed()) {
        result.fallback_reason = vulkan_backend_fallback_reason::acquire_image_failed;
        return result;
    }

    if (!command_recorder.begin_recording(result.surface, plan.batches.size())) {
        result.command_recorder = command_recorder.recorder_state();
        result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        return result;
    }
    for (const vulkan_draw_batch& batch : plan.batches) {
        if (!command_recorder.record_draw_batch(batch)) {
            result.command_recorder = command_recorder.recorder_state();
            result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
            return result;
        }
    }
    if (!command_recorder.finish_recording()) {
        result.command_recorder = command_recorder.recorder_state();
        result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        return result;
    }
    result.command_recorder = command_recorder.recorder_state();

    result.commands_recorded = device.record_frame_commands(plan);
    if (!result.commands_recorded) {
        result.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        return result;
    }
    result.recorded_batch_count = plan.batches.size();
    result.reached_stage = vulkan_backend_frame_stage::commands_recorded;

    request_wait(result.frame_sync.submit_wait_image_available_semaphore);
    request_signal(result.frame_sync.submit_signal_render_finished_semaphore);
    request_signal(result.frame_sync.submit_signal_frame_fence);
    result.frame_submitted = device.submit_frame();
    complete_wait(result.frame_sync.submit_wait_image_available_semaphore, result.frame_submitted);
    complete_signal(result.frame_sync.submit_signal_render_finished_semaphore, result.frame_submitted);
    complete_signal(result.frame_sync.submit_signal_frame_fence, result.frame_submitted);
    if (!result.frame_submitted) {
        result.fallback_reason = vulkan_backend_fallback_reason::submit_frame_failed;
        return result;
    }
    result.reached_stage = vulkan_backend_frame_stage::frame_submitted;

    request_wait(result.frame_sync.present_wait_render_finished_semaphore);
    result.swapchain.present_requested = true;
    result.swapchain.present = device.present_image(result.swapchain.acquire.image.id);
    complete_wait(
        result.frame_sync.present_wait_render_finished_semaphore,
        result.swapchain.present.completed());
    result.swapchain.acquire.image.presented = result.swapchain.present.completed();
    if (!result.swapchain.present.completed()) {
        result.fallback_reason = vulkan_backend_fallback_reason::present_image_failed;
        return result;
    }

    result.frame_presented = device.present_frame();
    if (!result.frame_presented) {
        result.fallback_reason = vulkan_backend_fallback_reason::present_frame_failed;
        return result;
    }
    result.reached_stage = vulkan_backend_frame_stage::frame_presented;

    result.fallback_required = false;
    result.fallback_reason = vulkan_backend_fallback_reason::none;
    return result;
}

} // namespace quiz_vulkan::render::vulkan_backend
