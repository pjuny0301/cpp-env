#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_command_recording.h"
#include "render/vulkan/vulkan_backend_command_submit.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_frame_lifecycle.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_render_pass.h"
#include "render/vulkan/vulkan_backend_swapchain.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__) || defined(__unix__) || defined(__linux__)
#include <dlfcn.h>
#endif

namespace quiz_vulkan::render::vulkan_backend {
namespace {

constexpr std::string_view k_vulkan_loader_required_symbol_name = "vkGetInstanceProcAddr";

std::string required_symbol_name_for(const vulkan_loader_probe_request& request)
{
    if (request.required_symbol_name.empty()) {
        return std::string{k_vulkan_loader_required_symbol_name};
    }

    return request.required_symbol_name;
}

void append_unique_library_name(
    std::vector<std::string>& names,
    const std::string& candidate_name)
{
    if (candidate_name.empty()) {
        return;
    }
    if (std::find(names.begin(), names.end(), candidate_name) != names.end()) {
        return;
    }

    names.push_back(candidate_name);
}

std::vector<std::string> merge_loader_candidate_names(
    const vulkan_loader_probe_request& request,
    const std::vector<std::string>& default_library_names)
{
    std::vector<std::string> candidate_names;
    candidate_names.reserve(
        request.candidate_library_names.size() + default_library_names.size());
    for (const std::string& candidate_name : request.candidate_library_names) {
        append_unique_library_name(candidate_names, candidate_name);
    }
    if (request.use_default_library_names) {
        for (const std::string& candidate_name : default_library_names) {
            append_unique_library_name(candidate_names, candidate_name);
        }
    }

    return candidate_names;
}

vulkan_loader_probe_result make_loader_probe_result(
    const vulkan_loader_probe_request& request)
{
    return vulkan_loader_probe_result{
        .checked = true,
        .status = vulkan_loader_probe_status::library_missing,
        .attempted_library_names = {},
        .loaded_library_name = {},
        .required_symbol_name = required_symbol_name_for(request),
        .attempted_library_count = 0,
        .library_found = false,
        .required_symbol_found = false,
    };
}

void record_loader_attempt(
    vulkan_loader_probe_result& result,
    const std::string& library_name)
{
    result.attempted_library_names.push_back(library_name);
    result.attempted_library_count = result.attempted_library_names.size();
}

void mark_loader_library_found(
    vulkan_loader_probe_result& result,
    const std::string& library_name)
{
    result.library_found = true;
    if (result.loaded_library_name.empty()) {
        result.loaded_library_name = library_name;
    }
}

void mark_loader_available(
    vulkan_loader_probe_result& result,
    const std::string& library_name)
{
    result.status = vulkan_loader_probe_status::available;
    result.loaded_library_name = library_name;
    result.library_found = true;
    result.required_symbol_found = true;
}

void mark_loader_probe_completed(vulkan_loader_probe_result& result)
{
    if (result.required_symbol_found) {
        result.status = vulkan_loader_probe_status::available;
        return;
    }

    result.status = result.library_found
        ? vulkan_loader_probe_status::required_symbol_missing
        : vulkan_loader_probe_status::library_missing;
}

vulkan_loader_readiness_status loader_readiness_status_for_probe(
    vulkan_loader_probe_status status)
{
    switch (status) {
    case vulkan_loader_probe_status::not_checked:
        return vulkan_loader_readiness_status::not_checked;
    case vulkan_loader_probe_status::available:
        return vulkan_loader_readiness_status::ready;
    case vulkan_loader_probe_status::library_missing:
        return vulkan_loader_readiness_status::library_missing;
    case vulkan_loader_probe_status::required_symbol_missing:
        return vulkan_loader_readiness_status::required_symbol_missing;
    }

    return vulkan_loader_readiness_status::not_checked;
}

class native_loader_library {
public:
    explicit native_loader_library(const std::string& library_name)
    {
#if defined(_WIN32)
        handle_ = ::LoadLibraryA(library_name.c_str());
#elif defined(__APPLE__) || defined(__unix__) || defined(__linux__)
        int open_flags = RTLD_LAZY;
#if defined(RTLD_LOCAL)
        open_flags |= RTLD_LOCAL;
#endif
        handle_ = ::dlopen(library_name.c_str(), open_flags);
#else
        static_cast<void>(library_name);
#endif
    }

    native_loader_library(const native_loader_library&) = delete;
    native_loader_library& operator=(const native_loader_library&) = delete;

    ~native_loader_library()
    {
#if defined(_WIN32)
        if (handle_ != nullptr) {
            ::FreeLibrary(handle_);
        }
#elif defined(__APPLE__) || defined(__unix__) || defined(__linux__)
        if (handle_ != nullptr) {
            ::dlclose(handle_);
        }
#endif
    }

    bool valid() const
    {
        return handle_ != nullptr;
    }

    bool has_symbol(const std::string& symbol_name) const
    {
        if (handle_ == nullptr) {
            return false;
        }

#if defined(_WIN32)
        return ::GetProcAddress(handle_, symbol_name.c_str()) != nullptr;
#elif defined(__APPLE__) || defined(__unix__) || defined(__linux__)
        return ::dlsym(handle_, symbol_name.c_str()) != nullptr;
#else
        static_cast<void>(symbol_name);
        return false;
#endif
    }

private:
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#elif defined(__APPLE__) || defined(__unix__) || defined(__linux__)
    void* handle_ = nullptr;
#else
    void* handle_ = nullptr;
#endif
};

bool has_visible_area(const render_rect& rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

vulkan_backend_fallback_reason first_unready_reason(
    const vulkan_backend_lifecycle_readiness& lifecycle)
{
    if (!lifecycle.effective_instance_ready()) {
        return vulkan_backend_fallback_reason::instance_unavailable;
    }
    if (!lifecycle.effective_device_ready()) {
        return vulkan_backend_fallback_reason::device_unavailable;
    }
    if (!lifecycle.effective_swapchain_ready()) {
        return vulkan_backend_fallback_reason::swapchain_unavailable;
    }
    if (lifecycle.render_pass.checked && !lifecycle.effective_render_pass_ready()) {
        return vulkan_backend_fallback_reason::render_pass_unavailable;
    }
    if (!lifecycle.effective_pipeline_ready()) {
        return vulkan_backend_fallback_reason::pipeline_unavailable;
    }
    if (!lifecycle.effective_command_recorder_ready()) {
        return vulkan_backend_fallback_reason::command_recorder_unavailable;
    }
    if (lifecycle.command_submit.checked && !lifecycle.effective_command_submit_ready()) {
        if (lifecycle.command_submit.status
            == vulkan_command_submit_readiness_status::present_target_unavailable) {
            return vulkan_backend_fallback_reason::present_frame_failed;
        }
        return vulkan_backend_fallback_reason::submit_frame_failed;
    }
    if (lifecycle.command_submit.checked && !lifecycle.effective_present_target_ready()) {
        return vulkan_backend_fallback_reason::present_frame_failed;
    }

    return vulkan_backend_fallback_reason::none;
}

int frame_stage_order(vulkan_backend_frame_stage stage)
{
    switch (stage) {
    case vulkan_backend_frame_stage::not_started:
        return 0;
    case vulkan_backend_frame_stage::backend_attempted:
        return 1;
    case vulkan_backend_frame_stage::lifecycle_ready:
        return 2;
    case vulkan_backend_frame_stage::surface_extent_ready:
        return 3;
    case vulkan_backend_frame_stage::frame_plan_ready:
        return 4;
    case vulkan_backend_frame_stage::frame_begun:
        return 5;
    case vulkan_backend_frame_stage::commands_recorded:
        return 6;
    case vulkan_backend_frame_stage::frame_submitted:
        return 7;
    case vulkan_backend_frame_stage::frame_presented:
        return 8;
    }

    return 0;
}

bool reached_frame_stage(
    vulkan_backend_frame_stage reached_stage,
    vulkan_backend_frame_stage target_stage)
{
    return frame_stage_order(reached_stage) >= frame_stage_order(target_stage);
}

vulkan_backend_frame_pipeline_handoff_status frame_pipeline_status_for_fallback(
    vulkan_backend_fallback_reason reason)
{
    switch (reason) {
    case vulkan_backend_fallback_reason::none:
        return vulkan_backend_frame_pipeline_handoff_status::ready;
    case vulkan_backend_fallback_reason::not_requested:
        return vulkan_backend_frame_pipeline_handoff_status::not_checked;
    case vulkan_backend_fallback_reason::instance_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::instance_unavailable;
    case vulkan_backend_fallback_reason::device_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::device_unavailable;
    case vulkan_backend_fallback_reason::swapchain_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::swapchain_unavailable;
    case vulkan_backend_fallback_reason::render_pass_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::render_pass_unavailable;
    case vulkan_backend_fallback_reason::pipeline_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::pipeline_unavailable;
    case vulkan_backend_fallback_reason::command_recorder_unavailable:
    case vulkan_backend_fallback_reason::record_commands_failed:
        return vulkan_backend_frame_pipeline_handoff_status::command_recording_unavailable;
    case vulkan_backend_fallback_reason::surface_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::surface_unavailable;
    case vulkan_backend_fallback_reason::viewport_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::viewport_unavailable;
    case vulkan_backend_fallback_reason::begin_frame_failed:
    case vulkan_backend_fallback_reason::acquire_image_failed:
        return vulkan_backend_frame_pipeline_handoff_status::frame_lifecycle_unavailable;
    case vulkan_backend_fallback_reason::resource_binding_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::resource_binding_unavailable;
    case vulkan_backend_fallback_reason::submit_frame_failed:
        return vulkan_backend_frame_pipeline_handoff_status::submit_unavailable;
    case vulkan_backend_fallback_reason::present_image_failed:
    case vulkan_backend_fallback_reason::present_frame_failed:
        return vulkan_backend_frame_pipeline_handoff_status::present_unavailable;
    }

    return vulkan_backend_frame_pipeline_handoff_status::not_checked;
}

void count_handoff_batch_kind(
    vulkan_backend_frame_pipeline_handoff& handoff,
    vulkan_batch_kind kind)
{
    switch (kind) {
    case vulkan_batch_kind::quad:
        ++handoff.quad_batch_count;
        break;
    case vulkan_batch_kind::text:
        ++handoff.text_batch_count;
        break;
    case vulkan_batch_kind::image:
        ++handoff.image_batch_count;
        break;
    case vulkan_batch_kind::debug_bounds:
        ++handoff.debug_bounds_batch_count;
        break;
    }
}

void count_handoff_batches_from_frame(
    vulkan_backend_frame_pipeline_handoff& handoff,
    const vulkan_backend_frame_result& frame)
{
    if (!frame.resource_bindings.batch_snapshots.empty()) {
        for (const vulkan_batch_resource_binding_snapshot& snapshot :
             frame.resource_bindings.batch_snapshots) {
            count_handoff_batch_kind(handoff, snapshot.batch_kind);
        }
        return;
    }

    if (!frame.pipeline.lifecycle.pipeline_snapshots.empty()) {
        for (const vulkan_pipeline_lifecycle_snapshot& snapshot :
             frame.pipeline.lifecycle.pipeline_snapshots) {
            count_handoff_batch_kind(handoff, snapshot.batch_kind);
        }
        return;
    }

    for (const vulkan_recorded_draw_batch& batch : frame.command_recorder.recorded_batches) {
        count_handoff_batch_kind(handoff, batch.kind);
    }
}

vulkan_command_packet_category command_packet_category_for(vulkan_batch_kind kind)
{
    switch (kind) {
    case vulkan_batch_kind::quad:
        return vulkan_command_packet_category::rect;
    case vulkan_batch_kind::text:
        return vulkan_command_packet_category::text;
    case vulkan_batch_kind::image:
        return vulkan_command_packet_category::image;
    case vulkan_batch_kind::debug_bounds:
        return vulkan_command_packet_category::debug_bounds;
    }

    return vulkan_command_packet_category::rect;
}

void count_command_packet_category(
    vulkan_command_packet_bridge_result& bridge,
    vulkan_command_packet_category category)
{
    switch (category) {
    case vulkan_command_packet_category::rect:
        ++bridge.rect_packet_count;
        break;
    case vulkan_command_packet_category::text:
        ++bridge.text_packet_count;
        break;
    case vulkan_command_packet_category::image:
        ++bridge.image_packet_count;
        break;
    case vulkan_command_packet_category::debug_bounds:
        ++bridge.debug_bounds_packet_count;
        break;
    }
}

void count_packet_execution_category(
    vulkan_command_packet_execution_result& result,
    vulkan_command_packet_category category)
{
    switch (category) {
    case vulkan_command_packet_category::rect:
        ++result.rect_packet_count;
        break;
    case vulkan_command_packet_category::text:
        ++result.text_packet_count;
        break;
    case vulkan_command_packet_category::image:
        ++result.image_packet_count;
        break;
    case vulkan_command_packet_category::debug_bounds:
        ++result.debug_bounds_packet_count;
        break;
    }
}

const vulkan_batch_resource_binding_snapshot* find_binding_snapshot_for_batch(
    const vulkan_backend_resource_binding_state& resource_bindings,
    const vulkan_draw_batch& batch,
    std::size_t batch_index)
{
    if (batch_index < resource_bindings.batch_snapshots.size()) {
        const vulkan_batch_resource_binding_snapshot& snapshot =
            resource_bindings.batch_snapshots[batch_index];
        if (snapshot.command_index == batch.command_index && snapshot.batch_kind == batch.kind) {
            return &snapshot;
        }
    }

    for (const vulkan_batch_resource_binding_snapshot& snapshot :
         resource_bindings.batch_snapshots) {
        if (snapshot.command_index == batch.command_index && snapshot.batch_kind == batch.kind) {
            return &snapshot;
        }
    }
    return nullptr;
}

vulkan_command_packet make_command_packet(
    const vulkan_draw_batch& batch,
    const vulkan_batch_resource_binding_snapshot& bindings,
    std::size_t packet_index)
{
    return vulkan_command_packet{
        .category = command_packet_category_for(batch.kind),
        .batch_kind = batch.kind,
        .command_index = batch.command_index,
        .packet_index = packet_index,
        .node_id = batch.node_id,
        .bounds = batch.bounds,
        .clipped_bounds = batch.clipped_bounds,
        .scissor = batch.scissor,
        .vertices = batch.vertices,
        .descriptor_set_count = bindings.descriptor_set_count,
        .binding_count = bindings.binding_count,
        .bindings = bindings.bindings,
    };
}

vulkan_command_packet_execution_snapshot make_execution_event(
    vulkan_command_packet_execution_event event)
{
    return vulkan_command_packet_execution_snapshot{
        .event = event,
        .attempted = true,
    };
}

vulkan_command_packet_execution_snapshot make_packet_execution_event(
    const vulkan_command_packet& packet)
{
    return vulkan_command_packet_execution_snapshot{
        .event = vulkan_command_packet_execution_event::packet,
        .category = packet.category,
        .batch_kind = packet.batch_kind,
        .packet_index = packet.packet_index,
        .command_index = packet.command_index,
        .attempted = true,
    };
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

bool ensure_frame_plan_pipelines(
    vulkan_pipeline_cache_interface& pipeline_cache,
    const vulkan_frame_plan& plan,
    vulkan_backend_frame_result& result)
{
    for (const vulkan_draw_batch& batch : plan.batches) {
        if (!pipeline_cache.ensure_pipeline(batch)) {
            result.pipeline = pipeline_cache.pipeline_state();
            frame_lifecycle::mark_fallback(
                result,
                vulkan_backend_fallback_reason::pipeline_unavailable);
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

std::string_view vulkan_loader_required_symbol_name()
{
    return k_vulkan_loader_required_symbol_name;
}

std::vector<std::string> default_vulkan_loader_library_names()
{
#if defined(_WIN32)
    return {"vulkan-1.dll"};
#elif defined(__APPLE__)
    return {"libvulkan.1.dylib", "libvulkan.dylib"};
#else
    return {"libvulkan.so.1", "libvulkan.so"};
#endif
}

fake_vulkan_loader::fake_vulkan_loader()
    : fake_vulkan_loader(fake_vulkan_loader_options{})
{
}

fake_vulkan_loader::fake_vulkan_loader(fake_vulkan_loader_options options)
    : options_(std::move(options))
{
}

vulkan_loader_probe_result fake_vulkan_loader::probe_loader(
    const vulkan_loader_probe_request& request)
{
    vulkan_loader_probe_result result = make_loader_probe_result(request);
    const std::vector<std::string> candidate_names = merge_loader_candidate_names(
        request,
        {options_.library_name});

    for (const std::string& candidate_name : candidate_names) {
        record_loader_attempt(result, candidate_name);
    }

    if (candidate_names.empty() || !options_.library_available) {
        mark_loader_probe_completed(result);
        return result;
    }

    mark_loader_library_found(result, candidate_names.front());
    if (options_.required_symbol_available) {
        mark_loader_available(result, candidate_names.front());
        return result;
    }

    mark_loader_probe_completed(result);
    return result;
}

vulkan_loader_probe_result system_vulkan_loader::probe_loader(
    const vulkan_loader_probe_request& request)
{
    vulkan_loader_probe_result result = make_loader_probe_result(request);
    const std::vector<std::string> candidate_names = merge_loader_candidate_names(
        request,
        default_vulkan_loader_library_names());

    for (const std::string& candidate_name : candidate_names) {
        record_loader_attempt(result, candidate_name);

        native_loader_library library(candidate_name);
        if (!library.valid()) {
            continue;
        }

        mark_loader_library_found(result, candidate_name);
        if (library.has_symbol(result.required_symbol_name)) {
            mark_loader_available(result, candidate_name);
            return result;
        }
    }

    mark_loader_probe_completed(result);
    return result;
}

vulkan_loader_probe_result probe_vulkan_loader(
    vulkan_loader_interface& loader,
    const vulkan_loader_probe_request& request)
{
    return loader.probe_loader(request);
}

vulkan_loader_readiness_state make_vulkan_loader_readiness_state(
    const vulkan_loader_probe_result& probe_result)
{
    const vulkan_loader_readiness_status status =
        loader_readiness_status_for_probe(probe_result.status);
    const bool instance_ready =
        probe_result.checked
        && status == vulkan_loader_readiness_status::ready
        && probe_result.library_found
        && probe_result.required_symbol_found;

    return vulkan_loader_readiness_state{
        .checked = probe_result.checked,
        .status = status,
        .probe_status = probe_result.status,
        .loader_library_available = probe_result.library_found,
        .instance_proc_address_available = probe_result.required_symbol_found,
        .instance_ready = instance_ready,
        .loaded_library_name = probe_result.loaded_library_name,
        .required_symbol_name = probe_result.required_symbol_name,
        .attempted_library_count = probe_result.attempted_library_count,
    };
}

vulkan_backend_lifecycle_readiness apply_vulkan_loader_readiness_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_loader_readiness_state loader)
{
    lifecycle.loader = std::move(loader);
    if (lifecycle.loader.checked) {
        lifecycle.instance_ready = lifecycle.loader.ready_for_instance();
    }

    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_instance_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_instance_create_result instance)
{
    if (!instance.checked) {
        return lifecycle;
    }

    lifecycle.loader = instance.loader;
    lifecycle.instance = std::move(instance);
    lifecycle.instance_ready = lifecycle.instance.ready_for_device();
    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_device_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_device_create_result device)
{
    if (!device.checked) {
        return lifecycle;
    }

    lifecycle = apply_vulkan_instance_create_result_to_lifecycle(
        std::move(lifecycle),
        device.instance);
    lifecycle.device = std::move(device);
    lifecycle.device_ready = lifecycle.device.ready_for_backend();
    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_swapchain_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_swapchain_create_result swapchain)
{
    if (!swapchain.checked) {
        return lifecycle;
    }

    lifecycle = apply_vulkan_device_create_result_to_lifecycle(
        std::move(lifecycle),
        swapchain.device);
    lifecycle.swapchain = std::move(swapchain);
    lifecycle.swapchain_ready = lifecycle.swapchain.ready_for_frame();
    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_render_pass_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_render_pass_create_result render_pass)
{
    if (!render_pass.checked) {
        return lifecycle;
    }

    lifecycle = apply_vulkan_swapchain_create_result_to_lifecycle(
        std::move(lifecycle),
        render_pass.swapchain);
    lifecycle.render_pass = std::move(render_pass);
    lifecycle.render_pass_ready = lifecycle.render_pass.ready_for_pipeline();
    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_command_recording_readiness_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_command_recording_readiness_result command_recording)
{
    if (!command_recording.checked) {
        return lifecycle;
    }

    lifecycle = apply_vulkan_render_pass_create_result_to_lifecycle(
        std::move(lifecycle),
        command_recording.render_pass);
    lifecycle.command_recording = std::move(command_recording);
    lifecycle.pipeline_ready = lifecycle.command_recording.pipeline_ready();
    lifecycle.command_recorder_ready = lifecycle.command_recording.ready_for_recording();
    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_command_submit_readiness_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_command_submit_readiness_result command_submit)
{
    if (!command_submit.checked) {
        return lifecycle;
    }

    lifecycle = apply_vulkan_command_recording_readiness_to_lifecycle(
        std::move(lifecycle),
        command_submit.command_recording);
    lifecycle.command_submit = std::move(command_submit);
    return lifecycle;
}

vulkan_backend_queue_submit_adapter_summary summarize_vulkan_queue_submit_adapter_result(
    const vulkan_queue_submit_present_result& queue_submit)
{
    return vulkan_backend_queue_submit_adapter_summary{
        .checked = queue_submit.checked,
        .status = queue_submit.status,
        .submit_called = queue_submit.submit_called,
        .present_called = queue_submit.present_called,
        .submit_before_present = queue_submit.submit_before_present(),
        .recoverable_failure = queue_submit.recoverable_failure(),
        .fatal_failure = queue_submit.fatal_failure(),
        .diagnostic = queue_submit.diagnostic,
    };
}

vulkan_command_packet_bridge_result build_vulkan_command_packet_bridge(
    const vulkan_frame_plan& plan,
    const vulkan_backend_pipeline_state& pipeline,
    const vulkan_backend_resource_binding_state& resource_bindings,
    const vulkan_backend_resource_registry_state& resource_registry)
{
    const bool pipeline_required = !plan.batches.empty();
    const bool pipeline_checked = !pipeline_required
        || pipeline.cache_checked
        || pipeline.requested_pipeline_count > 0
        || pipeline.lifecycle.checked
        || pipeline.pipeline_readiness_summary.checked;
    const bool pipeline_ready = !pipeline_required || pipeline.completed();

    vulkan_command_packet_bridge_result bridge{
        .checked = true,
        .status = vulkan_command_packet_bridge_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .pipeline_checked = pipeline_checked,
        .pipeline_ready = pipeline_ready,
        .resource_bindings_checked = resource_bindings.checked,
        .resource_bindings_ready = resource_bindings.completed(),
        .resource_registry_checked = resource_registry.checked,
        .resource_registry_ready = resource_registry.completed(),
        .blocked_batch_kind = pipeline.missing_batch_kind,
        .blocked_command_index = pipeline.missing_command_index,
        .blocked_resource_id = {},
        .planned_batch_count = plan.batches.size(),
        .packet_count = 0,
        .rect_packet_count = 0,
        .text_packet_count = 0,
        .image_packet_count = 0,
        .debug_bounds_packet_count = 0,
        .clipped_packet_count = 0,
        .discarded_draw_call_count = plan.discarded_draw_call_count,
        .packets = {},
    };

    if (!pipeline_ready) {
        bridge.status = vulkan_command_packet_bridge_status::pipeline_unavailable;
        bridge.fallback_reason = vulkan_backend_fallback_reason::pipeline_unavailable;
        return bridge;
    }

    if (!resource_bindings.completed() || !resource_registry.completed()) {
        bridge.status = vulkan_command_packet_bridge_status::resource_binding_unavailable;
        bridge.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
        if (resource_bindings.checked && !resource_bindings.completed()) {
            bridge.blocked_batch_kind = resource_bindings.missing_batch_kind;
            bridge.blocked_command_index = resource_bindings.missing_command_index;
            bridge.blocked_resource_id = resource_bindings.missing_resource_id;
        } else if (!resource_registry.missing_resources.empty()) {
            const vulkan_resource_registry_missing_resource& missing =
                resource_registry.missing_resources.front();
            bridge.blocked_batch_kind = missing.batch_kind;
            bridge.blocked_command_index = missing.command_index;
            bridge.blocked_resource_id = missing.resource_id;
        }
        return bridge;
    }

    bridge.status = vulkan_command_packet_bridge_status::ready;
    bridge.fallback_reason = vulkan_backend_fallback_reason::none;
    bridge.packets.reserve(plan.batches.size());

    for (std::size_t batch_index = 0; batch_index < plan.batches.size(); ++batch_index) {
        const vulkan_draw_batch& batch = plan.batches[batch_index];
        const vulkan_batch_resource_binding_snapshot* bindings =
            find_binding_snapshot_for_batch(resource_bindings, batch, batch_index);
        if (bindings == nullptr || !bindings->completed()) {
            bridge.status = vulkan_command_packet_bridge_status::resource_binding_unavailable;
            bridge.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
            bridge.blocked_batch_kind = batch.kind;
            bridge.blocked_command_index = batch.command_index;
            bridge.blocked_resource_id = bindings == nullptr ? std::string{} : bindings->missing_resource_id;
            bridge.packets.clear();
            bridge.packet_count = 0;
            bridge.rect_packet_count = 0;
            bridge.text_packet_count = 0;
            bridge.image_packet_count = 0;
            bridge.debug_bounds_packet_count = 0;
            bridge.clipped_packet_count = 0;
            return bridge;
        }

        vulkan_command_packet packet = make_command_packet(batch, *bindings, bridge.packets.size());
        count_command_packet_category(bridge, packet.category);
        if (packet.clipped_bounds.x != packet.bounds.x
            || packet.clipped_bounds.y != packet.bounds.y
            || packet.clipped_bounds.width != packet.bounds.width
            || packet.clipped_bounds.height != packet.bounds.height) {
            ++bridge.clipped_packet_count;
        }
        bridge.packets.push_back(std::move(packet));
    }

    bridge.packet_count = bridge.packets.size();
    return bridge;
}

fake_vulkan_command_packet_executor::fake_vulkan_command_packet_executor() = default;

fake_vulkan_command_packet_executor::fake_vulkan_command_packet_executor(
    fake_vulkan_command_packet_executor_options options)
    : options_(options)
{
}

vulkan_command_packet_execution_result fake_vulkan_command_packet_executor::execute_packets(
    const vulkan_command_packet_bridge_result& bridge)
{
    result_ = vulkan_command_packet_execution_result{
        .checked = true,
        .status = vulkan_command_packet_execution_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .packet_bridge_checked = bridge.checked,
        .packet_bridge_ready = bridge.completed(),
        .planned_packet_count = bridge.packet_count,
        .events = {},
    };

    if (!bridge.completed()) {
        result_.status = vulkan_command_packet_execution_status::packet_bridge_unavailable;
        result_.fallback_reason = bridge.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::record_commands_failed
            : bridge.fallback_reason;
        return result_;
    }

    vulkan_command_packet_execution_snapshot begin =
        make_execution_event(vulkan_command_packet_execution_event::begin);
    result_.begin_attempted = true;
    if (options_.fail_begin) {
        begin.failed = true;
        result_.status = vulkan_command_packet_execution_status::begin_failed;
        result_.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        result_.events.push_back(begin);
        return result_;
    }
    begin.completed = true;
    result_.begin_completed = true;
    result_.events.push_back(begin);

    for (const vulkan_command_packet& packet : bridge.packets) {
        vulkan_command_packet_execution_snapshot packet_event =
            make_packet_execution_event(packet);
        ++result_.attempted_packet_count;
        if (options_.fail_packet && packet.packet_index == options_.fail_packet_index) {
            packet_event.failed = true;
            result_.status = vulkan_command_packet_execution_status::packet_failed;
            result_.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
            result_.has_failed_packet = true;
            result_.first_failed_category = packet.category;
            result_.first_failed_batch_kind = packet.batch_kind;
            result_.first_failed_packet_index = packet.packet_index;
            result_.first_failed_command_index = packet.command_index;
            result_.events.push_back(packet_event);
            return result_;
        }

        packet_event.completed = true;
        ++result_.executed_packet_count;
        count_packet_execution_category(result_, packet.category);
        result_.events.push_back(packet_event);
    }

    vulkan_command_packet_execution_snapshot end =
        make_execution_event(vulkan_command_packet_execution_event::end);
    result_.end_attempted = true;
    if (options_.fail_end) {
        end.failed = true;
        result_.status = vulkan_command_packet_execution_status::end_failed;
        result_.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        result_.events.push_back(end);
        return result_;
    }

    end.completed = true;
    result_.end_completed = true;
    result_.status = vulkan_command_packet_execution_status::completed;
    result_.fallback_reason = vulkan_backend_fallback_reason::none;
    result_.events.push_back(end);
    return result_;
}

const vulkan_command_packet_execution_result&
fake_vulkan_command_packet_executor::execution_result() const
{
    return result_;
}

vulkan_backend_frame_pipeline_handoff summarize_vulkan_frame_pipeline_handoff(
    const vulkan_backend_frame_result& frame)
{
    const bool checked = frame.attempted || frame.fallback_summary.checked
        || frame.reached_stage != vulkan_backend_frame_stage::not_started;
    const bool pipeline_required = frame.planned_batch_count > 0;
    const bool pipeline_checked = frame.pipeline.cache_checked
        || frame.pipeline.requested_pipeline_count > 0
        || frame.pipeline.lifecycle.checked
        || frame.pipeline.pipeline_readiness_summary.checked;
    const bool pipeline_completed = !pipeline_required || frame.pipeline.completed();
    const bool command_submit_readiness_ready =
        frame.lifecycle.effective_command_submit_ready()
        && frame.lifecycle.effective_present_target_ready();
    const bool frame_submit_completed =
        frame.command_buffer_submit.checked && frame.command_buffer_submit.submit.completed();
    const bool present_completed =
        frame.present_policy.present.completed() || frame.frame_presented;
    const bool resource_bindings_completed = frame.resource_bindings.completed();
    const bool resource_registry_completed = frame.resource_registry.completed();
    const bool command_packets_completed = frame.command_packets.completed();
    const bool command_packet_execution_completed = frame.command_packet_execution.completed();
    const bool command_recorder_gate_allowed = frame.command_recorder.gate.completed();
    const bool command_recording_ready =
        frame.lifecycle.effective_command_recorder_ready()
        && pipeline_completed
        && resource_bindings_completed
        && resource_registry_completed
        && command_packets_completed
        && command_packet_execution_completed
        && command_recorder_gate_allowed
        && frame.command_recorder.completed()
        && frame.commands_recorded
        && (!frame.command_buffer_submit.checked
            || frame.command_buffer_submit.recording.completed());

    vulkan_backend_frame_pipeline_handoff handoff{
        .checked = checked,
        .status = checked
            ? frame_pipeline_status_for_fallback(frame.fallback_reason)
            : vulkan_backend_frame_pipeline_handoff_status::not_checked,
        .fallback_reason = frame.fallback_reason,
        .reached_stage = frame.reached_stage,
        .cpu_fallback_available = frame.fallback_required,
        .loader_checked = frame.lifecycle.loader.checked,
        .loader_ready = frame.lifecycle.loader.checked
            ? frame.lifecycle.loader.ready_for_instance()
            : frame.lifecycle.effective_instance_ready(),
        .instance_ready = frame.lifecycle.effective_instance_ready(),
        .device_ready = frame.lifecycle.effective_device_ready(),
        .swapchain_ready = frame.lifecycle.effective_swapchain_ready(),
        .render_pass_ready = frame.lifecycle.effective_render_pass_ready(),
        .surface_ready = frame.surface_ready,
        .frame_plan_ready = reached_frame_stage(
            frame.reached_stage,
            vulkan_backend_frame_stage::frame_plan_ready),
        .pipeline_required = pipeline_required,
        .pipeline_checked = pipeline_checked,
        .pipeline_completed = pipeline_completed,
        .pipeline_readiness_summary_checked = frame.pipeline.pipeline_readiness_summary.checked,
        .pipeline_readiness_summary_completed =
            frame.pipeline.pipeline_readiness_summary.checked
            && frame.pipeline.pipeline_readiness_summary.completed(),
        .shader_modules_ready = !frame.pipeline.shader_modules.checked
            || frame.pipeline.shader_modules.completed(),
        .pipeline_layout_ready = !frame.pipeline.pipeline_layout.checked
            || frame.pipeline.pipeline_layout.ready_for_pipeline(),
        .graphics_pipeline_ready = !frame.pipeline.graphics_pipeline.checked
            || frame.pipeline.graphics_pipeline.ready_for_draw(),
        .resource_bindings_checked = frame.resource_bindings.checked,
        .resource_bindings_completed = resource_bindings_completed,
        .resource_registry_checked = frame.resource_registry.checked,
        .resource_registry_completed = resource_registry_completed,
        .command_packets_checked = frame.command_packets.checked,
        .command_packets_completed = command_packets_completed,
        .command_packet_execution_checked = frame.command_packet_execution.checked,
        .command_packet_execution_completed = command_packet_execution_completed,
        .command_recorder_lifecycle_ready = frame.lifecycle.effective_command_recorder_ready(),
        .command_recorder_gate_checked = frame.command_recorder.gate.checked,
        .command_recorder_gate_allowed = command_recorder_gate_allowed,
        .command_recording_ready = command_recording_ready,
        .command_submit_readiness_checked = frame.lifecycle.command_submit.checked,
        .command_submit_readiness_ready = command_submit_readiness_ready,
        .frame_submit_completed = frame_submit_completed,
        .present_completed = present_completed,
        .frame_lifecycle_checked = frame.lifecycle_policy.checked,
        .frame_lifecycle_completed = frame.lifecycle_policy.completed(),
        .frame_lifecycle_attempted_step_count = frame.lifecycle_policy.attempted_step_count,
        .frame_lifecycle_completed_step_count = frame.lifecycle_policy.completed_step_count,
        .planned_batch_count = frame.planned_batch_count,
        .recorded_batch_count = frame.recorded_batch_count,
        .clipped_draw_call_count = frame.clipped_draw_call_count,
        .discarded_draw_call_count = frame.discarded_draw_call_count,
    };
    count_handoff_batches_from_frame(handoff, frame);
    return handoff;
}

vulkan_backend_frame_result apply_vulkan_queue_submit_adapter_result_to_frame(
    vulkan_backend_frame_result frame,
    vulkan_queue_submit_present_result queue_submit)
{
    frame.command_submit = queue_submit.command_submit;
    frame.queue_submit_adapter = summarize_vulkan_queue_submit_adapter_result(queue_submit);
    frame.queue_submit = std::move(queue_submit);
    frame.pipeline_handoff = summarize_vulkan_frame_pipeline_handoff(frame);
    return frame;
}

null_vulkan_backend_device::null_vulkan_backend_device() = default;

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_loader_readiness_state loader_readiness)
    : loader_readiness_(std::move(loader_readiness))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    const vulkan_loader_probe_result& loader_probe)
    : null_vulkan_backend_device(make_vulkan_loader_readiness_state(loader_probe))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_instance_create_result instance_result)
    : loader_readiness_(instance_result.loader)
    , instance_result_(std::move(instance_result))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_device_create_result device_result)
    : loader_readiness_(device_result.instance.loader)
    , instance_result_(device_result.instance)
    , device_result_(std::move(device_result))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_swapchain_create_result swapchain_result)
    : loader_readiness_(swapchain_result.device.instance.loader)
    , instance_result_(swapchain_result.device.instance)
    , device_result_(swapchain_result.device)
    , swapchain_result_(std::move(swapchain_result))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_render_pass_create_result render_pass_result)
    : loader_readiness_(render_pass_result.swapchain.device.instance.loader)
    , instance_result_(render_pass_result.swapchain.device.instance)
    , device_result_(render_pass_result.swapchain.device)
    , swapchain_result_(render_pass_result.swapchain)
    , render_pass_result_(std::move(render_pass_result))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_command_recording_readiness_result command_recording_result)
    : loader_readiness_(command_recording_result.render_pass.swapchain.device.instance.loader)
    , instance_result_(command_recording_result.render_pass.swapchain.device.instance)
    , device_result_(command_recording_result.render_pass.swapchain.device)
    , swapchain_result_(command_recording_result.render_pass.swapchain)
    , render_pass_result_(command_recording_result.render_pass)
    , command_recording_result_(std::move(command_recording_result))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_command_submit_readiness_result command_submit_result)
    : loader_readiness_(
        command_submit_result.command_recording.render_pass.swapchain.device.instance.loader)
    , instance_result_(
        command_submit_result.command_recording.render_pass.swapchain.device.instance)
    , device_result_(command_submit_result.command_recording.render_pass.swapchain.device)
    , swapchain_result_(command_submit_result.command_recording.render_pass.swapchain)
    , render_pass_result_(command_submit_result.command_recording.render_pass)
    , command_recording_result_(command_submit_result.command_recording)
    , command_submit_result_(std::move(command_submit_result))
{
}

vulkan_backend_lifecycle_readiness null_vulkan_backend_device::current_lifecycle_readiness() const
{
    vulkan_backend_lifecycle_readiness lifecycle =
        apply_vulkan_loader_readiness_to_lifecycle({}, loader_readiness_);
    lifecycle = apply_vulkan_instance_create_result_to_lifecycle(
        std::move(lifecycle),
        instance_result_);
    lifecycle = apply_vulkan_device_create_result_to_lifecycle(
        std::move(lifecycle),
        device_result_);
    lifecycle = apply_vulkan_swapchain_create_result_to_lifecycle(
        std::move(lifecycle),
        swapchain_result_);
    lifecycle = apply_vulkan_render_pass_create_result_to_lifecycle(
        std::move(lifecycle),
        render_pass_result_);
    lifecycle = apply_vulkan_command_recording_readiness_to_lifecycle(
        std::move(lifecycle),
        command_recording_result_);
    return apply_vulkan_command_submit_readiness_to_lifecycle(
        std::move(lifecycle),
        command_submit_result_);
}

vulkan_surface_extent null_vulkan_backend_device::current_surface_extent() const
{
    if (swapchain_result_.ready_for_frame()) {
        return swapchain_result_.selected_extent;
    }

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
    const auto finish_frame = [&result]() -> vulkan_backend_frame_result {
        result.pipeline_handoff = summarize_vulkan_frame_pipeline_handoff(result);
        return result;
    };
    result.lifecycle = device.current_lifecycle_readiness();
    result.command_submit = result.lifecycle.command_submit;
    result.command_recorder.ready = result.lifecycle.command_recorder_ready;
    const vulkan_backend_fallback_reason unready_reason = first_unready_reason(result.lifecycle);
    if (unready_reason != vulkan_backend_fallback_reason::none) {
        frame_lifecycle::mark_fallback(result, unready_reason);
        return finish_frame();
    }
    result.lifecycle_ready = true;
    result.reached_stage = vulkan_backend_frame_stage::lifecycle_ready;

    result.surface = device.current_surface_extent();
    result.swapchain_policy = make_swapchain_policy_state(result.surface);
    if (!result.surface.valid()) {
        frame_lifecycle::mark_fallback(result, vulkan_backend_fallback_reason::surface_unavailable);
        return finish_frame();
    }
    result.reached_stage = vulkan_backend_frame_stage::surface_extent_ready;
    if (!has_visible_area(viewport)) {
        frame_lifecycle::mark_fallback(
            result,
            vulkan_backend_fallback_reason::viewport_unavailable);
        return finish_frame();
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
        result.command_packets = build_vulkan_command_packet_bridge(
            plan,
            result.pipeline,
            result.resource_bindings,
            result.resource_registry);
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        return finish_frame();
    }

    result.resource_bindings = build_vulkan_resource_binding_state(draw_list, plan);
    result.resource_registry = build_vulkan_resource_registry_state(
        draw_list,
        plan,
        result.resource_bindings);
    result.command_packets = build_vulkan_command_packet_bridge(
        plan,
        result.pipeline,
        result.resource_bindings,
        result.resource_registry);
    result.command_recorder.gate = make_command_recorder_gate_state(
        result.resource_bindings,
        result.resource_registry);
    if (result.command_packets.blocked() || result.command_recorder.gate.blocked()) {
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        frame_lifecycle::mark_fallback(
            result,
            vulkan_backend_fallback_reason::resource_binding_unavailable);
        return finish_frame();
    }
    track_descriptor_set_resources(result.frame_resources, result.resource_bindings);
    fake_vulkan_command_packet_executor packet_executor;
    result.command_packet_execution = packet_executor.execute_packets(result.command_packets);
    if (result.command_packet_execution.failed()) {
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        frame_lifecycle::mark_fallback(
            result,
            vulkan_backend_fallback_reason::record_commands_failed);
        return finish_frame();
    }

    result.lifecycle_policy = frame_lifecycle::make_policy_state();
    result.present_policy = make_frame_present_policy_state();
    const auto fail_frame_lifecycle = [&result](
                                          vulkan_frame_lifecycle_step step,
                                          vulkan_backend_fallback_reason reason) {
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        frame_lifecycle::mark_fallback(result, reason);
        frame_lifecycle::fail_step(result.lifecycle_policy, step, reason);
    };

    result.frame_sync = make_diagnostic_frame_sync_state();
    frame_lifecycle::start_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::acquire);
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
        return finish_frame();
    }
    track_swapchain_image_resource(result.frame_resources, result.swapchain.acquire.image.id);
    frame_lifecycle::complete_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::acquire);

    frame_lifecycle::start_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::begin);
    result.frame_begun = device.begin_frame(result.surface);
    if (!result.frame_begun) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::begin,
            vulkan_backend_fallback_reason::begin_frame_failed);
        return finish_frame();
    }
    frame_lifecycle::complete_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::begin);
    result.reached_stage = vulkan_backend_frame_stage::frame_begun;

    result.command_buffer_submit = make_command_buffer_submit_state(
        result.frame_sync.frame,
        plan.batches.size());
    track_command_buffer_resource(
        result.frame_resources,
        result.command_buffer_submit.recording.command_buffer);
    frame_lifecycle::start_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::render);
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
        return finish_frame();
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
            return finish_frame();
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
        return finish_frame();
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
        return finish_frame();
    }
    frame_lifecycle::complete_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::render);
    result.recorded_batch_count = plan.batches.size();
    result.reached_stage = vulkan_backend_frame_stage::commands_recorded;

    frame_lifecycle::start_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::submit);
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
        return finish_frame();
    }
    frame_lifecycle::complete_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::submit);
    result.reached_stage = vulkan_backend_frame_stage::frame_submitted;

    frame_lifecycle::start_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::present);
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
        return finish_frame();
    }

    mark_present_policy_frame_requested(result.present_policy);
    result.frame_presented = device.present_frame();
    mark_present_policy_frame_result(result.present_policy, result.frame_presented);
    if (!result.frame_presented) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::present,
            vulkan_backend_fallback_reason::present_frame_failed);
        return finish_frame();
    }
    frame_lifecycle::complete_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::present);
    result.reached_stage = vulkan_backend_frame_stage::frame_presented;

    release_frame_resources(
        result.frame_resources,
        vulkan_frame_resource_release_stage::after_present);
    frame_lifecycle::mark_fallback(result, vulkan_backend_fallback_reason::none);
    return finish_frame();
}

} // namespace quiz_vulkan::render::vulkan_backend
