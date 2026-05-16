#pragma once

#include "render/render_draw_list.h"
#include "render/vulkan/vulkan_backend_command_recording.h"
#include "render/vulkan/vulkan_backend_command_submit.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_graphics_pipeline.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_native_readiness.h"
#include "render/vulkan/vulkan_backend_native_symbols.h"
#include "render/vulkan/vulkan_backend_queue_submit.h"
#include "render/vulkan/vulkan_backend_render_pass.h"
#include "render/vulkan/vulkan_backend_pipeline_layout.h"
#include "render/vulkan/vulkan_backend_sdk.h"
#include "render/vulkan/vulkan_backend_shader_module.h"
#include "render/vulkan/vulkan_backend_swapchain.h"
#include "render/vulkan/vulkan_frame_plan.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

enum class vulkan_backend_fallback_reason {
    none,
    not_requested,
    instance_unavailable,
    device_unavailable,
    swapchain_unavailable,
    render_pass_unavailable,
    pipeline_unavailable,
    command_recorder_unavailable,
    surface_unavailable,
    viewport_unavailable,
    begin_frame_failed,
    acquire_image_failed,
    resource_binding_unavailable,
    record_commands_failed,
    submit_frame_failed,
    present_image_failed,
    present_frame_failed,
};

std::string_view fallback_reason_name(vulkan_backend_fallback_reason reason);

enum class vulkan_backend_frame_stage {
    not_started,
    backend_attempted,
    lifecycle_ready,
    surface_extent_ready,
    frame_plan_ready,
    frame_begun,
    commands_recorded,
    frame_submitted,
    frame_presented,
};

std::string_view frame_stage_name(vulkan_backend_frame_stage stage);

enum class vulkan_frame_lifecycle_step {
    acquire,
    begin,
    render,
    submit,
    present,
};

std::string_view frame_lifecycle_step_name(vulkan_frame_lifecycle_step step);

enum class vulkan_frame_lifecycle_order_status {
    not_started,
    started,
    completed,
    skipped,
    failed,
    out_of_order,
};

std::string_view frame_lifecycle_order_status_name(vulkan_frame_lifecycle_order_status status);

enum class vulkan_frame_lifecycle_failure_classification {
    none,
    recoverable,
    fatal,
};

std::string_view frame_lifecycle_failure_classification_name(
    vulkan_frame_lifecycle_failure_classification classification);

struct vulkan_frame_lifecycle_step_snapshot {
    vulkan_frame_lifecycle_step step = vulkan_frame_lifecycle_step::acquire;
    std::size_t expected_order = 0;
    std::size_t observed_order = 0;
    bool attempted = false;
    bool completed = false;
    vulkan_frame_lifecycle_order_status status = vulkan_frame_lifecycle_order_status::not_started;
    vulkan_frame_lifecycle_failure_classification failure_classification =
        vulkan_frame_lifecycle_failure_classification::none;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::none;

    bool failed() const
    {
        return status == vulkan_frame_lifecycle_order_status::failed
            || status == vulkan_frame_lifecycle_order_status::out_of_order;
    }
};

struct vulkan_backend_frame_lifecycle_policy_state {
    bool checked = false;
    bool ordering_valid = true;
    bool recoverable_failure = false;
    bool fatal_failure = false;
    vulkan_frame_lifecycle_step failed_step = vulkan_frame_lifecycle_step::acquire;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::none;
    std::size_t attempted_step_count = 0;
    std::size_t completed_step_count = 0;
    std::vector<vulkan_frame_lifecycle_step_snapshot> snapshots;

    bool completed() const
    {
        return checked && ordering_valid && !recoverable_failure && !fatal_failure
            && attempted_step_count == snapshots.size()
            && completed_step_count == snapshots.size();
    }
};

struct vulkan_backend_lifecycle_readiness {
    bool instance_ready = false;
    bool device_ready = false;
    bool swapchain_ready = false;
    bool render_pass_ready = false;
    bool pipeline_ready = false;
    bool command_recorder_ready = false;
    vulkan_loader_readiness_state loader;
    vulkan_instance_create_result instance;
    vulkan_device_create_result device;
    vulkan_swapchain_create_result swapchain;
    vulkan_render_pass_create_result render_pass;
    vulkan_command_recording_readiness_result command_recording;
    vulkan_command_submit_readiness_result command_submit;

    bool effective_instance_ready() const
    {
        if (command_submit.checked) {
            return command_submit.command_recording.render_pass.swapchain.device.instance.ready_for_device();
        }
        if (command_recording.checked) {
            return command_recording.render_pass.swapchain.device.instance.ready_for_device();
        }
        if (render_pass.checked) {
            return render_pass.swapchain.device.instance.ready_for_device();
        }
        if (swapchain.checked) {
            return swapchain.device.instance.ready_for_device();
        }
        if (device.checked) {
            return device.instance.ready_for_device();
        }
        if (instance.checked) {
            return instance.ready_for_device();
        }
        if (loader.checked) {
            return loader.ready_for_instance();
        }

        return instance_ready;
    }

    bool effective_device_ready() const
    {
        if (command_submit.checked) {
            return command_submit.command_recording.render_pass.swapchain.device.ready_for_backend();
        }
        if (command_recording.checked) {
            return command_recording.render_pass.swapchain.device.ready_for_backend();
        }
        if (render_pass.checked) {
            return render_pass.swapchain.device.ready_for_backend();
        }
        if (swapchain.checked) {
            return swapchain.device.ready_for_backend();
        }
        if (device.checked) {
            return device.ready_for_backend();
        }

        return device_ready;
    }

    bool effective_swapchain_ready() const
    {
        if (command_submit.checked) {
            return command_submit.command_recording.render_pass.swapchain.ready_for_frame();
        }
        if (command_recording.checked) {
            return command_recording.render_pass.swapchain.ready_for_frame();
        }
        if (render_pass.checked) {
            return render_pass.swapchain.ready_for_frame();
        }
        if (swapchain.checked) {
            return swapchain.ready_for_frame();
        }

        return swapchain_ready;
    }

    bool effective_render_pass_ready() const
    {
        if (command_submit.checked) {
            return command_submit.command_recording.render_pass_ready()
                && command_submit.command_recording.framebuffer_ready();
        }
        if (command_recording.checked) {
            return command_recording.render_pass_ready()
                && command_recording.framebuffer_ready();
        }
        if (render_pass.checked) {
            return render_pass.ready_for_pipeline();
        }

        return render_pass_ready || !render_pass.checked;
    }

    bool effective_pipeline_ready() const
    {
        if (command_submit.checked) {
            return command_submit.command_recording.pipeline_ready();
        }
        if (command_recording.checked) {
            return command_recording.pipeline_ready();
        }

        return pipeline_ready;
    }

    bool effective_command_recorder_ready() const
    {
        if (command_submit.checked) {
            return command_submit.command_recording.ready_for_recording();
        }
        if (command_recording.checked) {
            return command_recording.ready_for_recording();
        }

        return command_recorder_ready;
    }

    bool effective_command_submit_ready() const
    {
        if (command_submit.checked) {
            return command_submit.ready_for_submit();
        }

        return true;
    }

    bool effective_present_target_ready() const
    {
        if (command_submit.checked) {
            return command_submit.ready_for_present();
        }

        return true;
    }

    bool ready_for_frame() const
    {
        return effective_instance_ready() && effective_device_ready() && effective_swapchain_ready()
            && effective_render_pass_ready() && effective_pipeline_ready()
            && effective_command_recorder_ready() && effective_command_submit_ready()
            && effective_present_target_ready();
    }
};

vulkan_backend_lifecycle_readiness apply_vulkan_loader_readiness_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_loader_readiness_state loader);

vulkan_backend_lifecycle_readiness apply_vulkan_instance_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_instance_create_result instance);

vulkan_backend_lifecycle_readiness apply_vulkan_device_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_device_create_result device);

vulkan_backend_lifecycle_readiness apply_vulkan_swapchain_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_swapchain_create_result swapchain);

vulkan_backend_lifecycle_readiness apply_vulkan_render_pass_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_render_pass_create_result render_pass);

vulkan_backend_lifecycle_readiness apply_vulkan_command_recording_readiness_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_command_recording_readiness_result command_recording);

vulkan_backend_lifecycle_readiness apply_vulkan_command_submit_readiness_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_command_submit_readiness_result command_submit);

enum class vulkan_frame_acquire_policy_status {
    not_checked,
    not_requested,
    acquired,
    backpressured,
    failed,
};

std::string_view frame_acquire_policy_status_name(vulkan_frame_acquire_policy_status status);

enum class vulkan_frame_present_result_status {
    not_checked,
    not_requested,
    image_presented,
    frame_presented,
    image_failed,
    frame_failed,
};

std::string_view frame_present_result_status_name(vulkan_frame_present_result_status status);

struct vulkan_frame_acquire_policy_diagnostics {
    bool checked = false;
    bool requested = false;
    vulkan_swapchain_acquire_status swapchain_status = vulkan_swapchain_acquire_status::not_requested;
    vulkan_swapchain_image_id image_id;
    bool image_available = false;
    bool image_acquired = false;
    bool backpressured = false;
    vulkan_frame_acquire_policy_status status = vulkan_frame_acquire_policy_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;

    bool completed() const
    {
        return checked && requested && image_available && image_acquired
            && status == vulkan_frame_acquire_policy_status::acquired
            && fallback_reason == vulkan_backend_fallback_reason::none;
    }

    bool failed() const
    {
        return checked && requested
            && (status == vulkan_frame_acquire_policy_status::backpressured
                || status == vulkan_frame_acquire_policy_status::failed);
    }
};

struct vulkan_frame_present_result_summary {
    bool checked = false;
    bool image_present_requested = false;
    bool frame_present_requested = false;
    vulkan_swapchain_image_id image_id;
    vulkan_swapchain_present_status swapchain_status = vulkan_swapchain_present_status::not_requested;
    bool image_presented = false;
    bool frame_presented = false;
    vulkan_frame_present_result_status status = vulkan_frame_present_result_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;

    bool completed() const
    {
        return checked && image_present_requested && frame_present_requested
            && image_presented && frame_presented
            && status == vulkan_frame_present_result_status::frame_presented
            && fallback_reason == vulkan_backend_fallback_reason::none;
    }

    bool failed() const
    {
        return checked
            && (status == vulkan_frame_present_result_status::image_failed
                || status == vulkan_frame_present_result_status::frame_failed);
    }
};

struct vulkan_backend_frame_present_policy_state {
    bool checked = false;
    std::size_t acquire_request_count = 0;
    std::size_t present_image_request_count = 0;
    bool backpressure_detected = false;
    vulkan_frame_acquire_policy_diagnostics acquire;
    vulkan_frame_present_result_summary present;

    bool completed() const
    {
        return checked && acquire.completed() && present.completed()
            && acquire_request_count == 1 && present_image_request_count == 1
            && !backpressure_detected;
    }

    bool failed() const
    {
        return checked && (acquire.failed() || present.failed());
    }
};

struct vulkan_frame_in_flight_id {
    std::size_t index = 0;
    std::size_t frame_count = 0;
    std::size_t sequence = 0;

    bool valid() const
    {
        return frame_count > 0 && index < frame_count && sequence > 0;
    }
};

enum class vulkan_frame_sync_token_kind {
    semaphore,
    fence,
};

struct vulkan_frame_sync_token_id {
    std::size_t value = 0;
    vulkan_frame_sync_token_kind kind = vulkan_frame_sync_token_kind::semaphore;

    bool valid() const
    {
        return value > 0;
    }
};

enum class vulkan_frame_sync_signal_status {
    not_requested,
    pending,
    signaled,
    failed,
};

enum class vulkan_frame_sync_wait_status {
    not_requested,
    pending,
    waited,
    failed,
};

struct vulkan_frame_sync_signal_state {
    vulkan_frame_sync_token_id token;
    bool requested = false;
    vulkan_frame_sync_signal_status status = vulkan_frame_sync_signal_status::not_requested;

    bool completed() const
    {
        return requested && token.valid() && status == vulkan_frame_sync_signal_status::signaled;
    }

    bool failed() const
    {
        return requested && status == vulkan_frame_sync_signal_status::failed;
    }
};

struct vulkan_frame_sync_wait_state {
    vulkan_frame_sync_token_id token;
    bool requested = false;
    vulkan_frame_sync_wait_status status = vulkan_frame_sync_wait_status::not_requested;

    bool completed() const
    {
        return requested && token.valid() && status == vulkan_frame_sync_wait_status::waited;
    }

    bool failed() const
    {
        return requested && status == vulkan_frame_sync_wait_status::failed;
    }
};

struct vulkan_backend_frame_sync_state {
    vulkan_frame_in_flight_id frame;
    vulkan_frame_sync_signal_state acquire_signal_image_available_semaphore;
    vulkan_frame_sync_signal_state acquire_signal_fence;
    vulkan_frame_sync_wait_state submit_wait_image_available_semaphore;
    vulkan_frame_sync_signal_state submit_signal_render_finished_semaphore;
    vulkan_frame_sync_signal_state submit_signal_frame_fence;
    vulkan_frame_sync_wait_state present_wait_render_finished_semaphore;

    bool acquire_completed() const
    {
        return frame.valid()
            && acquire_signal_image_available_semaphore.completed()
            && acquire_signal_fence.completed();
    }

    bool submit_completed() const
    {
        return submit_wait_image_available_semaphore.completed()
            && submit_signal_render_finished_semaphore.completed()
            && submit_signal_frame_fence.completed();
    }

    bool present_completed() const
    {
        return present_wait_render_finished_semaphore.completed();
    }

    bool completed() const
    {
        return acquire_completed() && submit_completed() && present_completed();
    }
};

enum class vulkan_frame_resource_kind {
    swapchain_image,
    command_buffer,
    descriptor_set,
};

std::string_view frame_resource_kind_name(vulkan_frame_resource_kind kind);

enum class vulkan_frame_resource_release_stage {
    none,
    after_present,
    fallback_cleanup,
};

std::string_view frame_resource_release_stage_name(vulkan_frame_resource_release_stage stage);

struct vulkan_frame_resource_lifetime_snapshot {
    vulkan_frame_resource_kind kind = vulkan_frame_resource_kind::swapchain_image;
    std::string resource_id;
    std::size_t command_index = 0;
    bool acquired = false;
    bool released = false;
    vulkan_frame_resource_release_stage release_stage = vulkan_frame_resource_release_stage::none;

    bool pending() const
    {
        return acquired && !released;
    }

    bool completed() const
    {
        return acquired && released
            && release_stage != vulkan_frame_resource_release_stage::none
            && !resource_id.empty();
    }
};

struct vulkan_backend_frame_resource_lifetime_state {
    bool checked = false;
    bool fallback_cleanup = false;
    std::size_t planned_batch_count = 0;
    std::size_t tracked_resource_count = 0;
    std::size_t acquired_resource_count = 0;
    std::size_t released_resource_count = 0;
    std::size_t pending_resource_count = 0;
    std::size_t fallback_release_count = 0;
    std::vector<vulkan_frame_resource_lifetime_snapshot> resources;

    bool completed() const
    {
        return checked
            && tracked_resource_count == resources.size()
            && acquired_resource_count == tracked_resource_count
            && released_resource_count == tracked_resource_count
            && pending_resource_count == 0;
    }
};

enum class vulkan_resource_binding_kind {
    batch_uniform,
    quad_vertex_buffer,
    image_texture,
    image_sampler,
    text_run_buffer,
    text_glyph_atlas,
};

struct vulkan_resource_binding_snapshot {
    std::size_t set = 0;
    std::size_t binding = 0;
    vulkan_resource_binding_kind kind = vulkan_resource_binding_kind::batch_uniform;
    std::string resource_id;
    bool required = true;
    bool available = false;

    bool bound() const
    {
        return !required || (available && !resource_id.empty());
    }
};

enum class vulkan_descriptor_validation_status {
    not_checked,
    valid,
    missing_required_resource,
    duplicate_binding,
    invalid_layout,
};

std::string_view descriptor_validation_status_name(vulkan_descriptor_validation_status status);

struct vulkan_descriptor_binding_validation_snapshot {
    std::size_t set = 0;
    std::size_t binding = 0;
    vulkan_resource_binding_kind kind = vulkan_resource_binding_kind::batch_uniform;
    std::string resource_id;
    bool required = true;
    bool available = false;
    bool bound = false;
    bool binding_index_matches_order = false;
    bool duplicate_binding = false;
    vulkan_descriptor_validation_status status = vulkan_descriptor_validation_status::not_checked;

    bool completed() const
    {
        return status == vulkan_descriptor_validation_status::valid
            && bound && binding_index_matches_order && !duplicate_binding;
    }
};

struct vulkan_descriptor_set_validation_snapshot {
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t command_index = 0;
    render_node_id node_id;
    std::size_t set = 0;
    std::size_t expected_binding_count = 0;
    std::size_t actual_binding_count = 0;
    bool checked = false;
    bool descriptor_set_declared = false;
    bool binding_count_matches = false;
    bool missing_required_resource = false;
    bool duplicate_binding = false;
    bool invalid_layout = false;
    vulkan_resource_binding_kind failed_binding_kind = vulkan_resource_binding_kind::batch_uniform;
    std::size_t failed_binding = 0;
    vulkan_descriptor_validation_status status = vulkan_descriptor_validation_status::not_checked;
    std::vector<vulkan_descriptor_binding_validation_snapshot> bindings;

    bool completed() const
    {
        return checked && descriptor_set_declared && binding_count_matches
            && !missing_required_resource && !duplicate_binding && !invalid_layout
            && status == vulkan_descriptor_validation_status::valid
            && actual_binding_count == bindings.size();
    }
};

struct vulkan_backend_descriptor_validation_state {
    bool checked = false;
    bool missing_required_resource = false;
    bool duplicate_binding = false;
    bool invalid_layout = false;
    vulkan_batch_kind failed_batch_kind = vulkan_batch_kind::quad;
    std::size_t failed_command_index = 0;
    vulkan_resource_binding_kind failed_binding_kind = vulkan_resource_binding_kind::batch_uniform;
    std::size_t failed_binding = 0;
    std::size_t planned_batch_count = 0;
    std::size_t descriptor_set_count = 0;
    std::size_t valid_descriptor_set_count = 0;
    std::size_t invalid_descriptor_set_count = 0;
    std::size_t requested_binding_count = 0;
    std::size_t valid_binding_count = 0;
    std::size_t invalid_binding_count = 0;
    std::vector<vulkan_descriptor_set_validation_snapshot> descriptor_sets;

    bool completed() const
    {
        return checked && !missing_required_resource && !duplicate_binding && !invalid_layout
            && invalid_descriptor_set_count == 0
            && invalid_binding_count == 0
            && descriptor_sets.size() == planned_batch_count;
    }
};

struct vulkan_batch_resource_binding_snapshot {
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t command_index = 0;
    render_node_id node_id;
    std::size_t descriptor_set_count = 0;
    std::size_t binding_count = 0;
    bool missing_resource = false;
    vulkan_resource_binding_kind missing_binding_kind = vulkan_resource_binding_kind::batch_uniform;
    std::string missing_resource_id;
    std::vector<vulkan_resource_binding_snapshot> bindings;

    bool completed() const
    {
        return !missing_resource && binding_count == bindings.size();
    }
};

struct vulkan_backend_resource_binding_state {
    bool checked = false;
    bool missing_resource = false;
    vulkan_batch_kind missing_batch_kind = vulkan_batch_kind::quad;
    std::size_t missing_command_index = 0;
    vulkan_resource_binding_kind missing_binding_kind = vulkan_resource_binding_kind::batch_uniform;
    std::string missing_resource_id;
    std::size_t planned_batch_count = 0;
    std::size_t descriptor_set_count = 0;
    std::size_t binding_count = 0;
    vulkan_backend_descriptor_validation_state descriptor_validation;
    std::vector<vulkan_batch_resource_binding_snapshot> batch_snapshots;

    bool completed() const
    {
        return checked && !missing_resource && descriptor_validation.completed()
            && batch_snapshots.size() == planned_batch_count;
    }
};

struct vulkan_resource_registry_entry {
    vulkan_resource_binding_kind kind = vulkan_resource_binding_kind::batch_uniform;
    std::string resource_id;
    std::size_t first_command_index = 0;
    std::size_t last_command_index = 0;
    std::size_t use_count = 0;

    bool reused() const
    {
        return use_count > 1;
    }
};

struct vulkan_resource_registry_missing_resource {
    vulkan_resource_binding_kind kind = vulkan_resource_binding_kind::batch_uniform;
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t command_index = 0;
    std::size_t set = 0;
    std::size_t binding = 0;
    std::string resource_id;
};

struct vulkan_backend_resource_registry_state {
    bool checked = false;
    std::size_t planned_batch_count = 0;
    std::size_t descriptor_binding_count = 0;
    std::size_t registered_resource_count = 0;
    std::size_t descriptor_reuse_count = 0;
    std::size_t resource_reuse_count = 0;
    std::size_t missing_resource_count = 0;
    std::vector<vulkan_resource_registry_entry> resources;
    std::vector<vulkan_resource_registry_missing_resource> missing_resources;

    bool completed() const
    {
        return checked && missing_resource_count == 0;
    }
};

struct vulkan_recorded_draw_batch {
    vulkan_batch_kind kind = vulkan_batch_kind::quad;
    std::size_t command_index = 0;
    std::size_t recording_index = 0;
    render_rect bounds;
    render_rect clipped_bounds;
    vulkan_scissor_rect scissor;
};

struct vulkan_pipeline_descriptor {
    vulkan_batch_kind kind = vulkan_batch_kind::quad;
    vulkan_shader_module_id vertex_shader;
    vulkan_shader_module_id fragment_shader;

    bool matches(vulkan_batch_kind batch_kind) const
    {
        return kind == batch_kind;
    }

    bool complete() const
    {
        return !vertex_shader.empty() && !fragment_shader.empty();
    }
};

struct vulkan_backend_shader_registry_state {
    bool registry_checked = false;
    bool missing_shader = false;
    vulkan_batch_kind missing_batch_kind = vulkan_batch_kind::quad;
    vulkan_shader_stage missing_stage = vulkan_shader_stage::vertex;
    vulkan_shader_module_id missing_shader_id;
    std::size_t registered_shader_count = 0;
    std::vector<vulkan_shader_module_descriptor> modules;

    bool contains(const vulkan_shader_module_id& id, vulkan_shader_stage stage) const;
};

class diagnostic_vulkan_shader_registry {
public:
    diagnostic_vulkan_shader_registry();
    explicit diagnostic_vulkan_shader_registry(std::vector<vulkan_shader_module_descriptor> modules);

    bool require_shader(
        vulkan_batch_kind kind,
        vulkan_shader_stage stage,
        const vulkan_shader_module_id& id);
    const vulkan_backend_shader_registry_state& registry_state() const;

private:
    vulkan_backend_shader_registry_state state_;
};

struct vulkan_pipeline_capability {
    vulkan_batch_kind kind = vulkan_batch_kind::quad;
    bool available = false;
};

struct vulkan_pipeline_cache_entry {
    vulkan_batch_kind kind = vulkan_batch_kind::quad;
    bool available = false;
    bool requested = false;
    std::size_t request_count = 0;
    std::size_t last_command_index = 0;
};

enum class vulkan_pipeline_lifecycle_stage {
    render_pass,
    shader_stages,
    pipeline,
};

std::string_view pipeline_lifecycle_stage_name(vulkan_pipeline_lifecycle_stage stage);

enum class vulkan_pipeline_lifecycle_status {
    not_checked,
    ready,
    unavailable,
};

std::string_view pipeline_lifecycle_status_name(vulkan_pipeline_lifecycle_status status);

struct vulkan_render_pass_descriptor {
    std::size_t color_attachment_count = 1;
    bool has_depth_attachment = false;
    bool surface_compatible = true;

    bool valid() const
    {
        return color_attachment_count > 0 && surface_compatible;
    }
};

struct vulkan_pipeline_shader_stage_snapshot {
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t command_index = 0;
    vulkan_shader_module_id vertex_shader;
    vulkan_shader_module_id fragment_shader;
    bool vertex_stage_ready = false;
    bool fragment_stage_ready = false;

    bool completed() const
    {
        return vertex_stage_ready && fragment_stage_ready;
    }
};

struct vulkan_pipeline_compatibility_key {
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t color_attachment_count = 0;
    bool has_depth_attachment = false;
    bool surface_compatible = false;
    vulkan_shader_module_id vertex_shader;
    vulkan_shader_module_id fragment_shader;

    bool compatible() const
    {
        return color_attachment_count > 0 && surface_compatible
            && !vertex_shader.empty() && !fragment_shader.empty();
    }
};

struct vulkan_pipeline_compatibility_key_summary {
    bool checked = false;
    std::size_t requested_key_count = 0;
    std::size_t compatible_key_count = 0;
    std::size_t incompatible_key_count = 0;
    std::size_t unique_key_count = 0;
    std::vector<vulkan_pipeline_compatibility_key> keys;

    bool completed() const
    {
        return checked && requested_key_count == keys.size()
            && incompatible_key_count == 0;
    }
};

struct vulkan_shader_module_binding_readiness {
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t command_index = 0;
    vulkan_shader_stage stage = vulkan_shader_stage::vertex;
    vulkan_shader_module_id shader_id;
    std::string entry_point = "main";
    bool descriptor_declared = false;
    bool registry_checked = false;
    bool module_registered = false;
    bool ready = false;

    bool completed() const
    {
        return descriptor_declared && registry_checked && module_registered
            && ready && !shader_id.empty() && !entry_point.empty();
    }
};

struct vulkan_backend_shader_binding_readiness_state {
    bool checked = false;
    std::size_t requested_binding_count = 0;
    std::size_t ready_binding_count = 0;
    std::size_t missing_binding_count = 0;
    std::vector<vulkan_shader_module_binding_readiness> bindings;

    bool completed() const
    {
        return checked && requested_binding_count == bindings.size()
            && missing_binding_count == 0;
    }
};

struct vulkan_pipeline_lifecycle_snapshot {
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t command_index = 0;
    vulkan_pipeline_lifecycle_status render_pass_status =
        vulkan_pipeline_lifecycle_status::not_checked;
    vulkan_pipeline_lifecycle_status shader_stage_status =
        vulkan_pipeline_lifecycle_status::not_checked;
    vulkan_pipeline_lifecycle_status pipeline_status =
        vulkan_pipeline_lifecycle_status::not_checked;
    vulkan_pipeline_lifecycle_stage failed_stage = vulkan_pipeline_lifecycle_stage::render_pass;
    vulkan_shader_stage missing_shader_stage = vulkan_shader_stage::vertex;
    vulkan_shader_module_id missing_shader_id;

    bool completed() const
    {
        return render_pass_status == vulkan_pipeline_lifecycle_status::ready
            && shader_stage_status == vulkan_pipeline_lifecycle_status::ready
            && pipeline_status == vulkan_pipeline_lifecycle_status::ready;
    }
};

struct vulkan_backend_pipeline_lifecycle_state {
    bool checked = false;
    vulkan_render_pass_descriptor render_pass;
    bool missing_render_pass = false;
    bool missing_shader_stage = false;
    bool missing_pipeline = false;
    vulkan_pipeline_lifecycle_stage failed_stage = vulkan_pipeline_lifecycle_stage::render_pass;
    vulkan_batch_kind missing_batch_kind = vulkan_batch_kind::quad;
    std::size_t missing_command_index = 0;
    vulkan_shader_stage missing_shader_stage_kind = vulkan_shader_stage::vertex;
    vulkan_shader_module_id missing_shader_id;
    std::size_t requested_pipeline_count = 0;
    std::vector<vulkan_pipeline_shader_stage_snapshot> shader_stage_snapshots;
    std::vector<vulkan_pipeline_lifecycle_snapshot> pipeline_snapshots;

    bool render_pass_ready() const
    {
        return render_pass.valid() && !missing_render_pass;
    }

    bool completed() const
    {
        return checked && render_pass_ready() && !missing_shader_stage && !missing_pipeline
            && pipeline_snapshots.size() == requested_pipeline_count
            && shader_stage_snapshots.size() == requested_pipeline_count;
    }
};

struct vulkan_backend_pipeline_state {
    bool cache_checked = false;
    bool ready = false;
    bool missing_pipeline = false;
    bool missing_descriptor = false;
    bool missing_shader = false;
    vulkan_batch_kind missing_batch_kind = vulkan_batch_kind::quad;
    std::size_t missing_command_index = 0;
    vulkan_shader_stage missing_shader_stage = vulkan_shader_stage::vertex;
    vulkan_shader_module_id missing_shader_id;
    std::size_t requested_pipeline_count = 0;
    std::vector<vulkan_pipeline_capability> capabilities;
    std::vector<vulkan_pipeline_cache_entry> cache_entries;
    std::vector<vulkan_pipeline_descriptor> pipeline_descriptors;
    vulkan_backend_shader_registry_state shader_registry;
    vulkan_backend_shader_module_readiness_state shader_modules;
    vulkan_pipeline_compatibility_key_summary compatibility;
    vulkan_backend_shader_binding_readiness_state shader_bindings;
    vulkan_pipeline_layout_create_result pipeline_layout;
    vulkan_graphics_pipeline_create_result graphics_pipeline;
    vulkan_backend_pipeline_readiness_summary pipeline_readiness_summary;
    vulkan_backend_pipeline_lifecycle_state lifecycle;

    bool supports(vulkan_batch_kind kind) const;
    const vulkan_pipeline_descriptor* descriptor_for(vulkan_batch_kind kind) const;
    bool completed() const;
};

struct diagnostic_vulkan_pipeline_cache_options {
    bool default_available = true;
    bool use_default_shader_modules = true;
    bool use_default_pipeline_descriptors = true;
    vulkan_render_pass_descriptor render_pass;
    std::vector<vulkan_pipeline_capability> overrides;
    std::vector<vulkan_shader_module_descriptor> shader_modules;
    std::vector<vulkan_pipeline_descriptor> pipeline_descriptors;
};

class vulkan_pipeline_cache_interface {
public:
    virtual ~vulkan_pipeline_cache_interface() = default;

    virtual bool ensure_pipeline(const vulkan_draw_batch& batch) = 0;
    virtual const vulkan_backend_pipeline_state& pipeline_state() const = 0;
};

class diagnostic_vulkan_pipeline_cache final : public vulkan_pipeline_cache_interface {
public:
    diagnostic_vulkan_pipeline_cache();
    explicit diagnostic_vulkan_pipeline_cache(diagnostic_vulkan_pipeline_cache_options options);

    bool ensure_pipeline(const vulkan_draw_batch& batch) override;
    const vulkan_backend_pipeline_state& pipeline_state() const override;

private:
    diagnostic_vulkan_pipeline_cache_options options_;
    diagnostic_vulkan_shader_registry shader_registry_;
    vulkan_backend_pipeline_state state_;
};

enum class vulkan_command_recorder_failure_stage {
    none,
    begin_recording,
    record_draw_batch,
    finish_recording,
};

std::string_view command_recorder_failure_stage_name(vulkan_command_recorder_failure_stage stage);

enum class vulkan_command_recorder_gate_status {
    not_checked,
    allowed,
    blocked_by_descriptor_validation,
    blocked_by_resource_binding,
    blocked_by_resource_registry,
};

std::string_view command_recorder_gate_status_name(vulkan_command_recorder_gate_status status);

struct vulkan_command_buffer_id {
    std::size_t value = 0;

    bool valid() const
    {
        return value > 0;
    }
};

enum class vulkan_command_buffer_recording_status {
    not_started,
    recording,
    recorded,
    failed,
};

std::string_view command_buffer_recording_status_name(
    vulkan_command_buffer_recording_status status);

enum class vulkan_frame_submit_status {
    not_requested,
    pending,
    submitted,
    failed,
};

std::string_view frame_submit_status_name(vulkan_frame_submit_status status);

struct vulkan_command_buffer_recording_diagnostics {
    vulkan_command_buffer_id command_buffer;
    vulkan_command_buffer_recording_status status =
        vulkan_command_buffer_recording_status::not_started;
    bool begin_requested = false;
    bool finish_requested = false;
    std::size_t planned_batch_count = 0;
    std::size_t recorded_batch_count = 0;
    vulkan_command_recorder_failure_stage failure_stage =
        vulkan_command_recorder_failure_stage::none;
    std::size_t failure_recording_index = 0;

    bool completed() const
    {
        return command_buffer.valid() && begin_requested && finish_requested
            && status == vulkan_command_buffer_recording_status::recorded
            && failure_stage == vulkan_command_recorder_failure_stage::none
            && recorded_batch_count == planned_batch_count;
    }

    bool failed() const
    {
        return status == vulkan_command_buffer_recording_status::failed
            || failure_stage != vulkan_command_recorder_failure_stage::none;
    }
};

struct vulkan_frame_submit_diagnostics {
    vulkan_command_buffer_id command_buffer;
    vulkan_frame_in_flight_id frame;
    vulkan_frame_submit_status status = vulkan_frame_submit_status::not_requested;
    bool submit_requested = false;
    std::size_t submitted_batch_count = 0;
    vulkan_frame_sync_wait_status wait_image_available_status =
        vulkan_frame_sync_wait_status::not_requested;
    vulkan_frame_sync_signal_status signal_render_finished_status =
        vulkan_frame_sync_signal_status::not_requested;
    vulkan_frame_sync_signal_status signal_frame_fence_status =
        vulkan_frame_sync_signal_status::not_requested;

    bool completed() const
    {
        return command_buffer.valid() && frame.valid() && submit_requested
            && status == vulkan_frame_submit_status::submitted
            && wait_image_available_status == vulkan_frame_sync_wait_status::waited
            && signal_render_finished_status == vulkan_frame_sync_signal_status::signaled
            && signal_frame_fence_status == vulkan_frame_sync_signal_status::signaled;
    }

    bool failed() const
    {
        return submit_requested && status == vulkan_frame_submit_status::failed;
    }
};

struct vulkan_backend_command_buffer_submit_state {
    bool checked = false;
    vulkan_command_buffer_recording_diagnostics recording;
    vulkan_frame_submit_diagnostics submit;

    bool completed() const
    {
        return checked && recording.completed() && submit.completed();
    }
};

struct vulkan_command_recorder_gate_state {
    bool checked = false;
    bool recording_allowed = false;
    bool resource_bindings_checked = false;
    bool resource_bindings_completed = false;
    bool descriptor_validation_checked = false;
    bool descriptor_validation_completed = false;
    bool resource_registry_checked = false;
    bool resource_registry_completed = false;
    bool missing_required_resource = false;
    bool duplicate_binding = false;
    bool invalid_layout = false;
    vulkan_command_recorder_gate_status status = vulkan_command_recorder_gate_status::not_checked;
    vulkan_descriptor_validation_status descriptor_status =
        vulkan_descriptor_validation_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    vulkan_batch_kind blocked_batch_kind = vulkan_batch_kind::quad;
    std::size_t blocked_command_index = 0;
    vulkan_resource_binding_kind blocked_binding_kind = vulkan_resource_binding_kind::batch_uniform;
    std::size_t blocked_binding = 0;
    std::string blocked_resource_id;
    std::size_t planned_batch_count = 0;
    std::size_t descriptor_set_count = 0;
    std::size_t invalid_descriptor_set_count = 0;
    std::size_t missing_resource_count = 0;

    bool completed() const
    {
        return checked && recording_allowed
            && status == vulkan_command_recorder_gate_status::allowed
            && fallback_reason == vulkan_backend_fallback_reason::none;
    }

    bool blocked() const
    {
        return checked && !recording_allowed
            && status != vulkan_command_recorder_gate_status::not_checked;
    }
};

struct vulkan_backend_command_recorder_state {
    bool ready = false;
    bool frame_open = false;
    bool command_buffer_recorded = false;
    vulkan_command_recorder_failure_stage failure_stage = vulkan_command_recorder_failure_stage::none;
    std::size_t failure_recording_index = 0;
    std::size_t planned_batch_count = 0;
    std::size_t recorded_batch_count = 0;
    vulkan_command_recorder_gate_state gate;
    std::vector<vulkan_recorded_draw_batch> recorded_batches;

    bool empty() const
    {
        return planned_batch_count == 0 && recorded_batch_count == 0
            && recorded_batches.empty();
    }

    bool completed() const
    {
        return ready && frame_open && command_buffer_recorded
            && recorded_batch_count == planned_batch_count
            && (!gate.checked || gate.completed())
            && recorded_batches.size() == recorded_batch_count;
    }

    bool failed() const
    {
        return failure_stage != vulkan_command_recorder_failure_stage::none;
    }
};

struct vulkan_backend_frame_fallback_summary {
    bool checked = false;
    bool required = true;
    vulkan_backend_fallback_reason reason = vulkan_backend_fallback_reason::not_requested;
    vulkan_backend_frame_stage reached_stage = vulkan_backend_frame_stage::not_started;
    bool recoverable = false;
    bool fatal = false;
    std::size_t reason_count = 0;

    bool completed() const
    {
        return checked && !required && reason == vulkan_backend_fallback_reason::none;
    }
};

struct vulkan_backend_queue_submit_adapter_summary {
    bool checked = false;
    vulkan_queue_submit_present_status status =
        vulkan_queue_submit_present_status::not_requested;
    bool submit_called = false;
    bool present_called = false;
    bool submit_before_present = false;
    bool recoverable_failure = false;
    bool fatal_failure = false;
    std::string diagnostic;

    bool completed() const
    {
        return checked
            && status == vulkan_queue_submit_present_status::submitted_and_presented
            && submit_called && present_called && submit_before_present
            && !recoverable_failure && !fatal_failure;
    }

    bool failed() const
    {
        return checked
            && (recoverable_failure || fatal_failure
                || status == vulkan_queue_submit_present_status::command_submit_unavailable
                || status == vulkan_queue_submit_present_status::command_buffer_unavailable
                || status == vulkan_queue_submit_present_status::submit_queue_unavailable
                || status == vulkan_queue_submit_present_status::present_target_unavailable
                || status == vulkan_queue_submit_present_status::adapter_unavailable);
    }
};

enum class vulkan_command_packet_category {
    rect,
    text,
    image,
    debug_bounds,
};

std::string_view command_packet_category_name(vulkan_command_packet_category category);

enum class vulkan_command_packet_bridge_status {
    not_checked,
    ready,
    pipeline_unavailable,
    resource_binding_unavailable,
};

std::string_view command_packet_bridge_status_name(
    vulkan_command_packet_bridge_status status);

struct vulkan_command_packet {
    vulkan_command_packet_category category = vulkan_command_packet_category::rect;
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t command_index = 0;
    std::size_t packet_index = 0;
    render_node_id node_id;
    render_rect bounds;
    render_rect clipped_bounds;
    vulkan_scissor_rect scissor;
    std::array<vulkan_quad_vertex, 4> vertices{};
    std::size_t descriptor_set_count = 0;
    std::size_t binding_count = 0;
    std::vector<vulkan_resource_binding_snapshot> bindings;

    bool completed() const
    {
        return binding_count == bindings.size() && !scissor.empty();
    }
};

struct vulkan_command_packet_bridge_result {
    bool checked = false;
    vulkan_command_packet_bridge_status status =
        vulkan_command_packet_bridge_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    bool pipeline_checked = false;
    bool pipeline_ready = false;
    bool resource_bindings_checked = false;
    bool resource_bindings_ready = false;
    bool resource_registry_checked = false;
    bool resource_registry_ready = false;
    vulkan_batch_kind blocked_batch_kind = vulkan_batch_kind::quad;
    std::size_t blocked_command_index = 0;
    std::string blocked_resource_id;
    std::size_t planned_batch_count = 0;
    std::size_t packet_count = 0;
    std::size_t rect_packet_count = 0;
    std::size_t text_packet_count = 0;
    std::size_t image_packet_count = 0;
    std::size_t debug_bounds_packet_count = 0;
    std::size_t clipped_packet_count = 0;
    std::size_t discarded_draw_call_count = 0;
    std::vector<vulkan_command_packet> packets;

    bool completed() const
    {
        return checked && status == vulkan_command_packet_bridge_status::ready
            && fallback_reason == vulkan_backend_fallback_reason::none
            && pipeline_checked && pipeline_ready
            && resource_bindings_checked && resource_bindings_ready
            && resource_registry_checked && resource_registry_ready
            && packet_count == planned_batch_count && packet_count == packets.size();
    }

    bool blocked() const
    {
        return checked && status != vulkan_command_packet_bridge_status::ready;
    }
};

enum class vulkan_command_packet_execution_status {
    not_checked,
    completed,
    packet_bridge_unavailable,
    begin_failed,
    packet_failed,
    end_failed,
};

std::string_view command_packet_execution_status_name(
    vulkan_command_packet_execution_status status);

enum class vulkan_command_packet_execution_event {
    begin,
    packet,
    end,
};

std::string_view command_packet_execution_event_name(
    vulkan_command_packet_execution_event event);

struct vulkan_command_packet_execution_snapshot {
    vulkan_command_packet_execution_event event = vulkan_command_packet_execution_event::begin;
    vulkan_command_packet_category category = vulkan_command_packet_category::rect;
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t packet_index = 0;
    std::size_t command_index = 0;
    bool attempted = false;
    bool completed = false;
    bool failed = false;

    bool successful() const
    {
        return attempted && completed && !failed;
    }
};

struct vulkan_command_packet_execution_result {
    bool checked = false;
    vulkan_command_packet_execution_status status =
        vulkan_command_packet_execution_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    bool packet_bridge_checked = false;
    bool packet_bridge_ready = false;
    bool begin_attempted = false;
    bool begin_completed = false;
    bool end_attempted = false;
    bool end_completed = false;
    bool has_failed_packet = false;
    vulkan_command_packet_category first_failed_category = vulkan_command_packet_category::rect;
    vulkan_batch_kind first_failed_batch_kind = vulkan_batch_kind::quad;
    std::size_t first_failed_packet_index = 0;
    std::size_t first_failed_command_index = 0;
    std::size_t planned_packet_count = 0;
    std::size_t attempted_packet_count = 0;
    std::size_t executed_packet_count = 0;
    std::size_t rect_packet_count = 0;
    std::size_t text_packet_count = 0;
    std::size_t image_packet_count = 0;
    std::size_t debug_bounds_packet_count = 0;
    std::vector<vulkan_command_packet_execution_snapshot> events;

    bool completed() const
    {
        return checked && status == vulkan_command_packet_execution_status::completed
            && fallback_reason == vulkan_backend_fallback_reason::none
            && packet_bridge_checked && packet_bridge_ready
            && begin_attempted && begin_completed && end_attempted && end_completed
            && !has_failed_packet
            && attempted_packet_count == planned_packet_count
            && executed_packet_count == planned_packet_count;
    }

    bool failed() const
    {
        return checked
            && status != vulkan_command_packet_execution_status::not_checked
            && status != vulkan_command_packet_execution_status::completed;
    }
};

enum class vulkan_native_command_packet_execution_status {
    not_checked,
    completed,
    packet_bridge_unavailable,
    native_function_table_unavailable,
    native_command_symbol_unavailable,
    command_buffer_unavailable,
    pipeline_unavailable,
    pipeline_layout_unavailable,
    descriptor_sets_unavailable,
    invalid_packet_data,
};

std::string_view native_command_packet_execution_status_name(
    vulkan_native_command_packet_execution_status status);

enum class vulkan_native_command_packet_call_kind {
    bind_pipeline,
    bind_descriptor_sets,
    set_viewport,
    set_scissor,
    draw,
};

std::string_view native_command_packet_call_kind_name(
    vulkan_native_command_packet_call_kind kind);

struct vulkan_native_descriptor_set_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_native_command_packet_descriptor_set {
    std::size_t packet_index = 0;
    std::size_t set = 0;
    vulkan_native_descriptor_set_handle descriptor_set;
    bool required = true;
    bool available = false;

    bool completed() const
    {
        return !required || (available && descriptor_set.valid());
    }
};

enum class vulkan_native_descriptor_set_allocation_status {
    not_checked,
    ready,
    packet_bridge_unavailable,
    resource_binding_unavailable,
    resource_binding_mismatch,
};

std::string_view native_descriptor_set_allocation_status_name(
    vulkan_native_descriptor_set_allocation_status status);

struct vulkan_native_descriptor_set_fake_allocator_options {
    std::uintptr_t first_descriptor_set_handle = 7000;
};

struct vulkan_native_descriptor_set_allocation_result {
    bool checked = false;
    vulkan_native_descriptor_set_allocation_status status =
        vulkan_native_descriptor_set_allocation_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    bool packet_bridge_checked = false;
    bool packet_bridge_ready = false;
    bool resource_bindings_checked = false;
    bool resource_bindings_ready = false;
    std::size_t planned_packet_count = 0;
    std::size_t planned_descriptor_set_count = 0;
    std::size_t allocated_descriptor_set_count = 0;
    std::size_t failed_packet_index = 0;
    std::size_t failed_command_index = 0;
    std::size_t failed_set = 0;
    std::string diagnostic;
    std::vector<vulkan_native_command_packet_descriptor_set> descriptor_sets;

    bool completed() const
    {
        if (!checked || status != vulkan_native_descriptor_set_allocation_status::ready
            || fallback_reason != vulkan_backend_fallback_reason::none
            || !packet_bridge_checked || !packet_bridge_ready
            || !resource_bindings_checked || !resource_bindings_ready
            || allocated_descriptor_set_count != planned_descriptor_set_count
            || descriptor_sets.size() != allocated_descriptor_set_count) {
            return false;
        }

        for (std::size_t descriptor_index = 0;
             descriptor_index < descriptor_sets.size();
             ++descriptor_index) {
            const vulkan_native_command_packet_descriptor_set& descriptor_set =
                descriptor_sets[descriptor_index];
            if (!descriptor_set.completed()) {
                return false;
            }
            for (std::size_t next_index = descriptor_index + 1;
                 next_index < descriptor_sets.size();
                 ++next_index) {
                const vulkan_native_command_packet_descriptor_set& next_descriptor_set =
                    descriptor_sets[next_index];
                if (descriptor_set.packet_index == next_descriptor_set.packet_index
                    && descriptor_set.set == next_descriptor_set.set) {
                    return false;
                }
            }
        }

        return true;
    }
};

struct vulkan_native_command_packet_executor_evidence {
    vulkan_native_function_table_diagnostics native_functions;
    vulkan_command_recording_command_buffer_handle command_buffer;
    vulkan_graphics_pipeline_handle pipeline;
    vulkan_pipeline_layout_handle pipeline_layout;
    render_rect viewport;
    bool viewport_available = false;
    std::vector<vulkan_native_command_packet_descriptor_set> descriptor_sets;
};

struct vulkan_native_command_packet_call_evidence {
    vulkan_native_command_packet_call_kind kind =
        vulkan_native_command_packet_call_kind::bind_pipeline;
    std::string symbol_name;
    vulkan_command_recording_command_buffer_handle command_buffer;
    vulkan_graphics_pipeline_handle pipeline;
    vulkan_pipeline_layout_handle pipeline_layout;
    std::size_t packet_index = 0;
    std::size_t command_index = 0;
    std::size_t descriptor_set_count = 0;
    std::size_t vertex_count = 0;
    render_rect viewport;
    vulkan_scissor_rect scissor;
    bool attempted = false;
    bool completed = false;
    bool failed = false;

    bool successful() const
    {
        return command_buffer.valid() && attempted && completed && !failed
            && !symbol_name.empty();
    }
};

struct vulkan_native_command_packet_execution_result {
    bool checked = false;
    vulkan_native_command_packet_execution_status status =
        vulkan_native_command_packet_execution_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    bool packet_bridge_checked = false;
    bool packet_bridge_ready = false;
    bool native_function_table_checked = false;
    bool native_command_symbols_ready = false;
    vulkan_native_function_table_status native_function_table_status =
        vulkan_native_function_table_status::not_checked;
    std::string missing_native_symbol_name;
    vulkan_command_recording_command_buffer_handle command_buffer;
    bool command_buffer_ready = false;
    bool pipeline_ready = false;
    bool pipeline_layout_ready = false;
    bool viewport_ready = false;
    bool descriptor_sets_ready = false;
    bool has_failed_packet = false;
    vulkan_command_packet_category first_failed_category = vulkan_command_packet_category::rect;
    vulkan_batch_kind first_failed_batch_kind = vulkan_batch_kind::quad;
    std::size_t first_failed_packet_index = 0;
    std::size_t first_failed_command_index = 0;
    std::size_t planned_packet_count = 0;
    std::size_t attempted_packet_count = 0;
    std::size_t translated_packet_count = 0;
    std::size_t attempted_native_call_count = 0;
    std::size_t completed_native_call_count = 0;
    std::string diagnostic;
    std::vector<vulkan_native_command_packet_call_evidence> calls;

    bool completed() const
    {
        return checked && status == vulkan_native_command_packet_execution_status::completed
            && fallback_reason == vulkan_backend_fallback_reason::none
            && packet_bridge_checked && packet_bridge_ready
            && native_function_table_checked && native_command_symbols_ready
            && command_buffer_ready && pipeline_ready && pipeline_layout_ready
            && viewport_ready && descriptor_sets_ready && !has_failed_packet
            && attempted_packet_count == planned_packet_count
            && translated_packet_count == planned_packet_count
            && attempted_native_call_count == completed_native_call_count
            && attempted_native_call_count == calls.size();
    }

    bool failed() const
    {
        return checked
            && status != vulkan_native_command_packet_execution_status::not_checked
            && status != vulkan_native_command_packet_execution_status::completed;
    }
};

enum class vulkan_command_recorder_operation_plan_status {
    not_checked,
    ready,
    packet_bridge_unavailable,
    packet_execution_unavailable,
};

std::string_view command_recorder_operation_plan_status_name(
    vulkan_command_recorder_operation_plan_status status);

enum class vulkan_command_recorder_operation_kind {
    draw_rect,
    draw_text,
    draw_image,
    draw_debug_bounds,
};

std::string_view command_recorder_operation_kind_name(
    vulkan_command_recorder_operation_kind kind);

struct vulkan_command_recorder_operation_summary {
    vulkan_command_recorder_operation_kind kind =
        vulkan_command_recorder_operation_kind::draw_rect;
    vulkan_command_packet_category category = vulkan_command_packet_category::rect;
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t operation_index = 0;
    std::size_t packet_index = 0;
    std::size_t command_index = 0;
    render_node_id node_id;
    render_rect bounds;
    render_rect clipped_bounds;
    vulkan_scissor_rect scissor;
    std::size_t vertex_count = 0;
    std::size_t descriptor_set_count = 0;
    std::size_t binding_count = 0;

    bool completed() const
    {
        return vertex_count > 0 && binding_count >= descriptor_set_count && !scissor.empty();
    }
};

struct vulkan_command_recorder_operation_plan {
    bool checked = false;
    vulkan_command_recorder_operation_plan_status status =
        vulkan_command_recorder_operation_plan_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    bool packet_bridge_checked = false;
    bool packet_bridge_ready = false;
    bool packet_execution_checked = false;
    bool packet_execution_ready = false;
    vulkan_command_packet_category blocked_category = vulkan_command_packet_category::rect;
    vulkan_batch_kind blocked_batch_kind = vulkan_batch_kind::quad;
    std::size_t blocked_packet_index = 0;
    std::size_t blocked_command_index = 0;
    std::size_t planned_packet_count = 0;
    std::size_t operation_count = 0;
    std::size_t rect_operation_count = 0;
    std::size_t text_operation_count = 0;
    std::size_t image_operation_count = 0;
    std::size_t debug_bounds_operation_count = 0;
    std::size_t clipped_operation_count = 0;
    std::size_t discarded_draw_call_count = 0;
    std::vector<vulkan_command_recorder_operation_summary> operations;

    bool completed() const
    {
        return checked && status == vulkan_command_recorder_operation_plan_status::ready
            && fallback_reason == vulkan_backend_fallback_reason::none
            && packet_bridge_checked && packet_bridge_ready
            && packet_execution_checked && packet_execution_ready
            && operation_count == planned_packet_count
            && operation_count == operations.size();
    }

    bool blocked() const
    {
        return checked && status != vulkan_command_recorder_operation_plan_status::ready;
    }
};

enum class vulkan_command_buffer_record_result_status {
    not_checked,
    recorded,
    operation_plan_unavailable,
    native_entrypoint_unavailable,
    command_buffer_unavailable,
    begin_failed,
    operation_failed,
    end_failed,
};

std::string_view command_buffer_record_result_status_name(
    vulkan_command_buffer_record_result_status status);

enum class vulkan_command_buffer_record_event_kind {
    begin,
    operation,
    end,
};

std::string_view command_buffer_record_event_kind_name(
    vulkan_command_buffer_record_event_kind kind);

struct vulkan_command_buffer_record_event {
    vulkan_command_buffer_record_event_kind event =
        vulkan_command_buffer_record_event_kind::begin;
    vulkan_command_buffer_id command_buffer;
    vulkan_command_recorder_operation_kind operation_kind =
        vulkan_command_recorder_operation_kind::draw_rect;
    vulkan_command_packet_category category = vulkan_command_packet_category::rect;
    vulkan_batch_kind batch_kind = vulkan_batch_kind::quad;
    std::size_t operation_index = 0;
    std::size_t packet_index = 0;
    std::size_t command_index = 0;
    bool attempted = false;
    bool completed = false;
    bool failed = false;

    bool successful() const
    {
        return command_buffer.valid() && attempted && completed && !failed;
    }
};

struct vulkan_command_buffer_record_result {
    bool checked = false;
    vulkan_command_buffer_record_result_status status =
        vulkan_command_buffer_record_result_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    vulkan_command_buffer_id command_buffer;
    bool operation_plan_checked = false;
    bool operation_plan_ready = false;
    bool native_function_table_checked = false;
    bool native_command_buffer_recording_ready = false;
    vulkan_native_function_table_status native_function_table_status =
        vulkan_native_function_table_status::not_checked;
    std::string missing_native_symbol_name;
    bool sdk_native_path_checked = false;
    bool sdk_adapter_ready = false;
    vulkan_sdk_native_path_status sdk_native_path_status =
        vulkan_sdk_native_path_status::not_checked;
    vulkan_sdk_capability_status sdk_capability_status =
        vulkan_sdk_capability_status::not_checked;
    vulkan_sdk_adapter_fallback_status sdk_adapter_fallback_status =
        vulkan_sdk_adapter_fallback_status::none;
    std::string sdk_diagnostic;
    bool begin_attempted = false;
    bool begin_completed = false;
    bool end_attempted = false;
    bool end_completed = false;
    bool has_failed_operation = false;
    vulkan_command_recorder_operation_kind first_failed_operation_kind =
        vulkan_command_recorder_operation_kind::draw_rect;
    vulkan_command_packet_category first_failed_category = vulkan_command_packet_category::rect;
    vulkan_batch_kind first_failed_batch_kind = vulkan_batch_kind::quad;
    std::size_t first_failed_operation_index = 0;
    std::size_t first_failed_packet_index = 0;
    std::size_t first_failed_command_index = 0;
    std::size_t planned_operation_count = 0;
    std::size_t attempted_operation_count = 0;
    std::size_t recorded_operation_count = 0;
    std::size_t rect_operation_count = 0;
    std::size_t text_operation_count = 0;
    std::size_t image_operation_count = 0;
    std::size_t debug_bounds_operation_count = 0;
    std::string diagnostic;
    std::vector<vulkan_command_buffer_record_event> events;

    bool completed() const
    {
        return checked && status == vulkan_command_buffer_record_result_status::recorded
            && fallback_reason == vulkan_backend_fallback_reason::none
            && command_buffer.valid()
            && operation_plan_checked && operation_plan_ready
            && (!native_function_table_checked || native_command_buffer_recording_ready)
            && (!sdk_native_path_checked || sdk_adapter_ready)
            && begin_attempted && begin_completed && end_attempted && end_completed
            && !has_failed_operation
            && attempted_operation_count == planned_operation_count
            && recorded_operation_count == planned_operation_count;
    }

    bool failed() const
    {
        return checked
            && status != vulkan_command_buffer_record_result_status::not_checked
            && status != vulkan_command_buffer_record_result_status::recorded;
    }
};

struct fake_vulkan_command_buffer_operation_recorder_options {
    bool fail_begin = false;
    bool fail_end = false;
    bool fail_operation = false;
    std::size_t fail_operation_index = 0;
};

class vulkan_command_buffer_operation_recorder_interface {
public:
    virtual ~vulkan_command_buffer_operation_recorder_interface() = default;

    virtual vulkan_command_buffer_record_result record_operations(
        vulkan_command_buffer_id command_buffer,
        const vulkan_command_recorder_operation_plan& operation_plan) = 0;
    virtual const vulkan_command_buffer_record_result& record_result() const = 0;
};

class fake_vulkan_command_buffer_operation_recorder final
    : public vulkan_command_buffer_operation_recorder_interface {
public:
    fake_vulkan_command_buffer_operation_recorder();
    explicit fake_vulkan_command_buffer_operation_recorder(
        fake_vulkan_command_buffer_operation_recorder_options options);

    vulkan_command_buffer_record_result record_operations(
        vulkan_command_buffer_id command_buffer,
        const vulkan_command_recorder_operation_plan& operation_plan) override;
    const vulkan_command_buffer_record_result& record_result() const override;

private:
    fake_vulkan_command_buffer_operation_recorder_options options_;
    vulkan_command_buffer_record_result result_;
};

vulkan_command_buffer_record_result record_vulkan_command_buffer_operations(
    vulkan_command_buffer_operation_recorder_interface& recorder,
    vulkan_command_buffer_id command_buffer,
    const vulkan_command_recorder_operation_plan& operation_plan,
    const vulkan_native_function_table_diagnostics& native_functions = {},
    const vulkan_sdk_capability_result& sdk_capabilities = {});

enum class vulkan_submit_batch_plan_status {
    not_checked,
    ready,
    command_buffer_recording_unavailable,
    native_queue_submit_unavailable,
    command_submit_unavailable,
    command_buffer_unavailable,
    sync_primitives_unavailable,
    submit_queue_unavailable,
    present_target_unavailable,
    submit_failed_recoverable,
    submit_failed_fatal,
};

std::string_view submit_batch_plan_status_name(vulkan_submit_batch_plan_status status);

enum class vulkan_submit_batch_sync_intent_kind {
    wait_image_available,
    signal_render_finished,
    signal_frame_fence,
};

std::string_view submit_batch_sync_intent_kind_name(
    vulkan_submit_batch_sync_intent_kind kind);

struct vulkan_submit_batch_sync_intent {
    vulkan_submit_batch_sync_intent_kind kind =
        vulkan_submit_batch_sync_intent_kind::wait_image_available;
    vulkan_command_submit_sync_handle handle;
    bool required = false;
    bool available = false;

    bool completed() const
    {
        return !required || (available && handle.valid());
    }
};

struct vulkan_submit_batch_present_intent {
    bool requested = false;
    bool target_available = false;
    vulkan_swapchain_image_id image_id;
    vulkan_command_submit_sync_handle wait_render_finished_semaphore;

    bool completed() const
    {
        return !requested
            || (target_available && image_id.value > 0
                && wait_render_finished_semaphore.valid());
    }
};

struct vulkan_submit_batch_record {
    std::size_t batch_index = 0;
    vulkan_command_buffer_id recorded_command_buffer;
    vulkan_command_recording_command_buffer_handle submit_command_buffer;
    vulkan_queue_handle submit_queue;
    std::size_t recorded_operation_count = 0;
    std::size_t wait_intent_count = 0;
    std::size_t signal_intent_count = 0;
    std::size_t present_intent_count = 0;

    bool completed() const
    {
        return recorded_command_buffer.valid() && submit_command_buffer.valid()
            && submit_queue.valid();
    }
};

struct vulkan_submit_batch_plan_result {
    bool checked = false;
    vulkan_submit_batch_plan_status status = vulkan_submit_batch_plan_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    bool command_buffer_recording_checked = false;
    bool command_buffer_recording_ready = false;
    bool native_function_table_checked = false;
    bool native_queue_submit_ready = false;
    vulkan_native_function_table_status native_function_table_status =
        vulkan_native_function_table_status::not_checked;
    std::string missing_native_symbol_name;
    bool sdk_native_path_checked = false;
    bool sdk_adapter_ready = false;
    vulkan_sdk_native_path_status sdk_native_path_status =
        vulkan_sdk_native_path_status::not_checked;
    vulkan_sdk_capability_status sdk_capability_status =
        vulkan_sdk_capability_status::not_checked;
    vulkan_sdk_adapter_fallback_status sdk_adapter_fallback_status =
        vulkan_sdk_adapter_fallback_status::none;
    std::string sdk_diagnostic;
    bool command_submit_readiness_checked = false;
    bool command_submit_readiness_ready = false;
    bool command_buffer_available = false;
    bool sync_primitives_available = false;
    bool submit_queue_available = false;
    bool present_target_available = false;
    bool submit_ready = false;
    bool present_ready = false;
    std::size_t recorded_operation_count = 0;
    std::size_t submit_batch_count = 0;
    std::size_t wait_intent_count = 0;
    std::size_t signal_intent_count = 0;
    std::size_t present_intent_count = 0;
    vulkan_command_buffer_id recorded_command_buffer;
    vulkan_command_recording_command_buffer_handle submit_command_buffer;
    vulkan_queue_handle submit_queue;
    vulkan_command_submit_sync_primitives sync_primitives;
    vulkan_swapchain_image_id image_id;
    std::vector<vulkan_submit_batch_record> submit_batches;
    std::vector<vulkan_submit_batch_sync_intent> wait_intents;
    std::vector<vulkan_submit_batch_sync_intent> signal_intents;
    std::vector<vulkan_submit_batch_present_intent> present_intents;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_submit_batch_plan_status::ready
            && fallback_reason == vulkan_backend_fallback_reason::none
            && command_buffer_recording_ready
            && (!native_function_table_checked || native_queue_submit_ready)
            && (!sdk_native_path_checked || sdk_adapter_ready)
            && command_submit_readiness_ready
            && command_buffer_available && sync_primitives_available
            && submit_queue_available && present_target_available
            && submit_ready && present_ready
            && submit_batch_count == submit_batches.size()
            && wait_intent_count == wait_intents.size()
            && signal_intent_count == signal_intents.size()
            && present_intent_count == present_intents.size();
    }

    bool blocked() const
    {
        return checked && status != vulkan_submit_batch_plan_status::ready;
    }
};

enum class vulkan_present_completion_plan_status {
    not_checked,
    ready,
    submit_batch_unavailable,
    native_queue_present_unavailable,
    present_request_unavailable,
    present_adapter_unavailable,
    submit_failed_recoverable,
    submit_failed_fatal,
    present_failed_recoverable,
    present_failed_fatal,
};

std::string_view present_completion_plan_status_name(
    vulkan_present_completion_plan_status status);

enum class vulkan_frame_completion_status {
    not_checked,
    ready_for_present,
    completed,
    submit_unavailable,
    present_unavailable,
    submit_failed_recoverable,
    submit_failed_fatal,
    present_failed_recoverable,
    present_failed_fatal,
};

std::string_view frame_completion_status_name(vulkan_frame_completion_status status);

struct vulkan_present_request_summary {
    bool requested = false;
    bool source_adapter_checked = false;
    vulkan_queue_handle present_queue;
    vulkan_swapchain_handle swapchain;
    vulkan_swapchain_image_id image_id;
    vulkan_command_submit_sync_handle wait_render_finished_semaphore;

    bool completed() const
    {
        return requested && present_queue.valid() && swapchain.valid()
            && image_id.value > 0 && wait_render_finished_semaphore.valid();
    }
};

struct vulkan_present_result_summary {
    bool checked = false;
    vulkan_queue_submit_adapter_call_status status =
        vulkan_queue_submit_adapter_call_status::not_called;
    bool present_called = false;
    bool submit_before_present = false;
    bool recoverable_failure = false;
    bool fatal_failure = false;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_queue_submit_adapter_call_status::completed
            && present_called && submit_before_present && !recoverable_failure
            && !fatal_failure;
    }

    bool failed() const
    {
        return recoverable_failure || fatal_failure;
    }
};

struct vulkan_present_completion_plan_result {
    bool checked = false;
    vulkan_present_completion_plan_status status =
        vulkan_present_completion_plan_status::not_checked;
    vulkan_frame_completion_status frame_status =
        vulkan_frame_completion_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    bool submit_batch_checked = false;
    bool submit_batch_ready = false;
    bool native_function_table_checked = false;
    bool native_queue_present_ready = false;
    vulkan_native_function_table_status native_function_table_status =
        vulkan_native_function_table_status::not_checked;
    std::string missing_native_symbol_name;
    bool sdk_native_path_checked = false;
    bool sdk_adapter_ready = false;
    vulkan_sdk_native_path_status sdk_native_path_status =
        vulkan_sdk_native_path_status::not_checked;
    vulkan_sdk_capability_status sdk_capability_status =
        vulkan_sdk_capability_status::not_checked;
    vulkan_sdk_adapter_fallback_status sdk_adapter_fallback_status =
        vulkan_sdk_adapter_fallback_status::none;
    std::string sdk_diagnostic;
    bool queue_present_adapter_checked = false;
    bool queue_present_adapter_ready = false;
    bool present_request_ready = false;
    bool present_result_ready = false;
    bool frame_completion_ready = false;
    bool recoverable_failure = false;
    bool fatal_failure = false;
    std::size_t submit_batch_count = 0;
    std::size_t present_intent_count = 0;
    vulkan_present_request_summary request;
    vulkan_present_result_summary result;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_present_completion_plan_status::ready
            && (frame_status == vulkan_frame_completion_status::ready_for_present
                || frame_status == vulkan_frame_completion_status::completed)
            && fallback_reason == vulkan_backend_fallback_reason::none
            && submit_batch_ready
            && (!native_function_table_checked || native_queue_present_ready)
            && (!sdk_native_path_checked || sdk_adapter_ready)
            && queue_present_adapter_ready
            && present_request_ready && present_result_ready
            && frame_completion_ready && request.completed()
            && result.completed() && !recoverable_failure && !fatal_failure;
    }

    bool blocked() const
    {
        return checked && status != vulkan_present_completion_plan_status::ready;
    }
};

enum class vulkan_native_queue_present_operation_status {
    not_checked,
    ready,
    acquire_operation_unavailable,
    submit_batch_unavailable,
    present_completion_unavailable,
    native_entrypoints_unavailable,
    required_extension_unavailable,
    missing_queue_present_symbol,
    present_queue_unavailable,
    swapchain_unavailable,
    acquired_image_unavailable,
    submitted_frame_unavailable,
    present_result_unavailable,
    out_of_date,
    suboptimal,
    present_failed_recoverable,
    present_failed_fatal,
};

std::string_view native_queue_present_operation_status_name(
    vulkan_native_queue_present_operation_status status);

struct vulkan_native_queue_present_operation_request {
    vulkan_native_swapchain_acquire_operation_result acquire_operation;
    vulkan_submit_batch_plan_result submit_batch;
    vulkan_present_completion_plan_result present_completion;
    vulkan_native_swapchain_entrypoint_readiness native_entrypoints;
    vulkan_swapchain_present_result present_result;
    bool allow_suboptimal = true;
};

struct vulkan_native_queue_present_operation_summary {
    bool checked = false;
    vulkan_native_queue_present_operation_status status =
        vulkan_native_queue_present_operation_status::not_checked;
    std::string entrypoint_name = "vkQueuePresentKHR";
    bool vk_queue_present_callable = false;
    bool frame_lifecycle_may_complete = false;
    vulkan_device_handle device;
    vulkan_swapchain_handle swapchain;
    vulkan_queue_handle present_queue;
    vulkan_swapchain_image_id image_id;
    vulkan_swapchain_image_handle image_handle;
    vulkan_command_submit_sync_handle wait_render_finished_semaphore;
    vulkan_swapchain_present_status present_status =
        vulkan_swapchain_present_status::not_requested;
    bool acquire_operation_checked = false;
    bool acquired_image_ready = false;
    bool submit_batch_checked = false;
    bool submit_batch_ready = false;
    bool present_completion_checked = false;
    bool present_completion_ready = false;
    bool native_entrypoints_checked = false;
    bool required_extensions_ready = false;
    bool queue_present_symbol_ready = false;
    bool present_queue_ready = false;
    bool swapchain_ready = false;
    bool submitted_frame_ready = false;
    bool present_request_ready = false;
    bool present_adapter_result_ready = false;
    bool present_result_checked = false;
    bool present_result_completed = false;
    bool submit_before_present = false;
    bool out_of_date = false;
    bool suboptimal = false;
    bool recoverable_failure = false;
    bool fatal_failure = false;
    std::string missing_required_extension;
    std::string missing_symbol_name;
    std::string diagnostic;

    bool ready_for_frame_completion() const
    {
        return checked && frame_lifecycle_may_complete
            && (status == vulkan_native_queue_present_operation_status::ready
                || status == vulkan_native_queue_present_operation_status::suboptimal);
    }
};

struct vulkan_native_queue_present_operation_result {
    bool checked = false;
    vulkan_native_queue_present_operation_status status =
        vulkan_native_queue_present_operation_status::not_checked;
    vulkan_native_swapchain_acquire_operation_result acquire_operation;
    vulkan_submit_batch_plan_result submit_batch;
    vulkan_present_completion_plan_result present_completion;
    vulkan_native_swapchain_entrypoint_readiness native_entrypoints;
    vulkan_device_handle device;
    vulkan_swapchain_handle swapchain;
    vulkan_queue_handle present_queue;
    vulkan_swapchain_image_id image_id;
    vulkan_swapchain_image_handle image_handle;
    vulkan_command_submit_sync_handle wait_render_finished_semaphore;
    vulkan_swapchain_present_status present_status =
        vulkan_swapchain_present_status::not_requested;
    bool acquire_operation_checked = false;
    bool acquired_image_ready = false;
    bool submit_batch_checked = false;
    bool submit_batch_ready = false;
    bool present_completion_checked = false;
    bool present_completion_ready = false;
    bool native_entrypoints_checked = false;
    bool required_extensions_ready = false;
    bool queue_present_symbol_ready = false;
    bool present_queue_ready = false;
    bool swapchain_ready = false;
    bool submitted_frame_ready = false;
    bool present_request_ready = false;
    bool present_adapter_result_ready = false;
    bool present_result_checked = false;
    bool present_result_completed = false;
    bool submit_before_present = false;
    bool out_of_date = false;
    bool suboptimal = false;
    bool recoverable_failure = false;
    bool fatal_failure = false;
    bool vk_queue_present_callable = false;
    bool frame_lifecycle_may_complete = false;
    std::string missing_required_extension;
    std::string missing_symbol_name;
    std::string diagnostic;
    vulkan_native_queue_present_operation_summary operation;

    bool can_call_vk_queue_present() const
    {
        return checked && vk_queue_present_callable;
    }

    bool ready_for_frame_completion() const
    {
        return checked && operation.ready_for_frame_completion();
    }

    bool blocked() const
    {
        return checked && !ready_for_frame_completion();
    }
};

} // namespace quiz_vulkan::render::vulkan_backend

#define QUIZ_VULKAN_BACKEND_NATIVE_FRAME_OPERATION_DEPENDENCIES_READY
#include "render/vulkan/vulkan_backend_native_frame_operation.h"
#undef QUIZ_VULKAN_BACKEND_NATIVE_FRAME_OPERATION_DEPENDENCIES_READY

namespace quiz_vulkan::render::vulkan_backend {

struct fake_vulkan_command_packet_executor_options {
    bool fail_begin = false;
    bool fail_end = false;
    bool fail_packet = false;
    std::size_t fail_packet_index = 0;
};

class vulkan_command_packet_executor_interface {
public:
    virtual ~vulkan_command_packet_executor_interface() = default;

    virtual vulkan_command_packet_execution_result execute_packets(
        const vulkan_command_packet_bridge_result& bridge) = 0;
    virtual const vulkan_command_packet_execution_result& execution_result() const = 0;
};

class fake_vulkan_command_packet_executor final : public vulkan_command_packet_executor_interface {
public:
    fake_vulkan_command_packet_executor();
    explicit fake_vulkan_command_packet_executor(
        fake_vulkan_command_packet_executor_options options);

    vulkan_command_packet_execution_result execute_packets(
        const vulkan_command_packet_bridge_result& bridge) override;
    const vulkan_command_packet_execution_result& execution_result() const override;

private:
    fake_vulkan_command_packet_executor_options options_;
    vulkan_command_packet_execution_result result_;
};

class vulkan_native_command_packet_executor final
    : public vulkan_command_packet_executor_interface {
public:
    explicit vulkan_native_command_packet_executor(
        vulkan_native_command_packet_executor_evidence evidence);

    vulkan_command_packet_execution_result execute_packets(
        const vulkan_command_packet_bridge_result& bridge) override;
    const vulkan_command_packet_execution_result& execution_result() const override;
    const vulkan_native_command_packet_execution_result& native_execution_result() const;

private:
    vulkan_native_command_packet_executor_evidence evidence_;
    vulkan_command_packet_execution_result result_;
    vulkan_native_command_packet_execution_result native_result_;
};

enum class vulkan_scoped_command_packet_execution_status {
    not_checked,
    completed,
    render_pass_scope_unavailable,
    packet_bridge_unavailable,
    begin_failed,
    packet_failed,
    end_failed,
};

std::string_view scoped_command_packet_execution_status_name(
    vulkan_scoped_command_packet_execution_status status);

struct vulkan_scoped_command_packet_execution_request {
    vulkan_native_render_pass_scope_record_result render_pass_scope;
    vulkan_command_packet_bridge_result packet_bridge;
};

struct vulkan_scoped_command_packet_execution_result {
    bool checked = false;
    vulkan_scoped_command_packet_execution_status status =
        vulkan_scoped_command_packet_execution_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    vulkan_native_render_pass_scope_record_result render_pass_scope;
    vulkan_command_packet_bridge_result packet_bridge;
    vulkan_command_packet_execution_result packet_execution;
    vulkan_command_recorder_operation_plan operation_plan;
    std::size_t render_pass_scope_id = 0;
    std::size_t selected_framebuffer_target_index = 0;
    vulkan_swapchain_image_id image_id;
    vulkan_framebuffer_handle framebuffer;
    vulkan_command_recording_command_buffer_handle command_buffer;
    bool render_pass_scope_checked = false;
    bool render_pass_scope_ready = false;
    bool command_buffer_ready = false;
    bool packet_bridge_checked = false;
    bool packet_bridge_ready = false;
    bool scoped_execution_empty = false;
    bool render_pass_begin_attempted = false;
    bool render_pass_begin_completed = false;
    bool render_pass_end_attempted = false;
    bool render_pass_end_completed = false;
    bool render_pass_end_skipped = false;
    bool packet_execution_checked = false;
    bool packet_execution_ready = false;
    bool operation_plan_checked = false;
    bool operation_plan_ready = false;
    bool has_failed_packet = false;
    vulkan_command_packet_category first_failed_category = vulkan_command_packet_category::rect;
    vulkan_batch_kind first_failed_batch_kind = vulkan_batch_kind::quad;
    std::size_t first_failed_packet_index = 0;
    std::size_t first_failed_command_index = 0;
    std::size_t planned_packet_count = 0;
    std::size_t attempted_packet_count = 0;
    std::size_t executed_packet_count = 0;
    std::size_t rect_packet_count = 0;
    std::size_t text_packet_count = 0;
    std::size_t image_packet_count = 0;
    std::size_t debug_bounds_packet_count = 0;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_scoped_command_packet_execution_status::completed
            && fallback_reason == vulkan_backend_fallback_reason::none
            && render_pass_scope_ready && command_buffer_ready
            && packet_bridge_ready && packet_execution_ready && operation_plan_ready
            && render_pass_begin_attempted && render_pass_begin_completed
            && render_pass_end_attempted && render_pass_end_completed
            && !render_pass_end_skipped && !has_failed_packet
            && planned_packet_count == packet_bridge.packet_count
            && executed_packet_count == planned_packet_count;
    }

    bool failed() const
    {
        return checked
            && status != vulkan_scoped_command_packet_execution_status::not_checked
            && status != vulkan_scoped_command_packet_execution_status::completed;
    }
};

vulkan_scoped_command_packet_execution_result execute_vulkan_scoped_command_packets(
    vulkan_command_packet_executor_interface& executor,
    const vulkan_scoped_command_packet_execution_request& request);

enum class vulkan_backend_frame_pipeline_handoff_status {
    not_checked,
    ready,
    instance_unavailable,
    device_unavailable,
    swapchain_unavailable,
    render_pass_unavailable,
    surface_unavailable,
    viewport_unavailable,
    pipeline_unavailable,
    resource_binding_unavailable,
    command_recording_unavailable,
    frame_lifecycle_unavailable,
    submit_unavailable,
    present_unavailable,
};

std::string_view frame_pipeline_handoff_status_name(
    vulkan_backend_frame_pipeline_handoff_status status);

struct vulkan_backend_frame_native_execution_summary {
    bool checked = false;
    vulkan_native_frame_operation_summary operation;
    vulkan_native_frame_operation_diff_diagnostics diff;
    vulkan_native_frame_operation_execution_plan plan;
    vulkan_native_frame_execution_decision acquire_decision =
        vulkan_native_frame_execution_decision::not_checked;
    vulkan_native_frame_execution_decision record_decision =
        vulkan_native_frame_execution_decision::not_checked;
    vulkan_native_frame_execution_decision submit_decision =
        vulkan_native_frame_execution_decision::not_checked;
    vulkan_native_frame_execution_decision present_decision =
        vulkan_native_frame_execution_decision::not_checked;
    bool native_acquire_would_execute = false;
    bool native_record_would_execute = false;
    bool native_submit_would_execute = false;
    bool native_present_would_execute = false;

    bool should_execute_native_frame() const
    {
        return checked && plan.should_execute_native_frame();
    }

    bool should_skip_native_frame() const
    {
        return checked && plan.skip_required;
    }

    bool should_use_cpu_fallback() const
    {
        return checked && plan.should_use_cpu_fallback();
    }
};

struct vulkan_backend_frame_scoped_command_packet_summary {
    bool checked = false;
    bool ready = false;
    vulkan_scoped_command_packet_execution_status status =
        vulkan_scoped_command_packet_execution_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    bool render_pass_scope_ready = false;
    bool command_buffer_ready = false;
    bool packet_bridge_ready = false;
    bool packet_execution_ready = false;
    bool operation_plan_ready = false;
    bool render_pass_begin_completed = false;
    bool render_pass_end_completed = false;
    bool render_pass_end_skipped = false;
    bool scoped_execution_empty = false;
    bool packets_executed_inside_render_pass_scope = false;
    bool commands_recorded_gated_by_scoped_execution = false;
    bool has_failed_packet = false;
    vulkan_command_packet_category first_failed_category = vulkan_command_packet_category::rect;
    vulkan_batch_kind first_failed_batch_kind = vulkan_batch_kind::quad;
    std::size_t first_failed_packet_index = 0;
    std::size_t first_failed_command_index = 0;
    std::size_t render_pass_scope_id = 0;
    std::size_t selected_framebuffer_target_index = 0;
    vulkan_swapchain_image_id image_id;
    vulkan_framebuffer_handle framebuffer;
    vulkan_command_recording_command_buffer_handle command_buffer;
    std::size_t planned_packet_count = 0;
    std::size_t attempted_packet_count = 0;
    std::size_t executed_packet_count = 0;
    std::size_t rect_packet_count = 0;
    std::size_t text_packet_count = 0;
    std::size_t image_packet_count = 0;
    std::size_t debug_bounds_packet_count = 0;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_scoped_command_packet_execution_status::completed
            && fallback_reason == vulkan_backend_fallback_reason::none
            && ready && render_pass_scope_ready && command_buffer_ready && packet_bridge_ready
            && packet_execution_ready && operation_plan_ready
            && render_pass_begin_completed && render_pass_end_completed
            && !render_pass_end_skipped && !has_failed_packet
            && planned_packet_count == executed_packet_count
            && attempted_packet_count == planned_packet_count;
    }

    bool failed() const
    {
        return checked
            && status != vulkan_scoped_command_packet_execution_status::not_checked
            && status != vulkan_scoped_command_packet_execution_status::completed;
    }
};

struct vulkan_backend_frame_pipeline_handoff {
    bool checked = false;
    vulkan_backend_frame_pipeline_handoff_status status =
        vulkan_backend_frame_pipeline_handoff_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    vulkan_backend_frame_stage reached_stage = vulkan_backend_frame_stage::not_started;
    bool cpu_fallback_available = true;
    bool loader_checked = false;
    bool loader_ready = false;
    bool instance_ready = false;
    bool device_ready = false;
    bool swapchain_ready = false;
    bool swapchain_recreate_policy_checked = false;
    vulkan_swapchain_recreate_policy_action swapchain_recreate_action =
        vulkan_swapchain_recreate_policy_action::not_checked;
    bool swapchain_keep_rendering = false;
    bool swapchain_recreate_immediately = false;
    bool swapchain_recreate_after_frame = false;
    bool swapchain_skip_submit = false;
    bool swapchain_fatal_error = false;
    bool render_pass_ready = false;
    bool surface_ready = false;
    bool frame_plan_ready = false;
    bool pipeline_required = false;
    bool pipeline_checked = false;
    bool pipeline_completed = false;
    bool pipeline_readiness_summary_checked = false;
    bool pipeline_readiness_summary_completed = false;
    bool shader_modules_ready = false;
    bool pipeline_layout_ready = false;
    bool graphics_pipeline_ready = false;
    bool resource_bindings_checked = false;
    bool resource_bindings_completed = false;
    bool resource_registry_checked = false;
    bool resource_registry_completed = false;
    bool command_packets_checked = false;
    bool command_packets_completed = false;
    bool command_packet_execution_checked = false;
    bool command_packet_execution_completed = false;
    bool command_recorder_operations_checked = false;
    bool command_recorder_operations_completed = false;
    bool command_buffer_recording_checked = false;
    bool command_buffer_recording_completed = false;
    bool command_buffer_ready_for_submit = false;
    bool native_function_table_checked = false;
    bool native_command_buffer_recording_ready = false;
    bool native_queue_submit_ready = false;
    bool native_queue_present_ready = false;
    vulkan_native_function_table_status native_function_table_status =
        vulkan_native_function_table_status::not_checked;
    std::string missing_native_symbol_name;
    vulkan_backend_frame_native_execution_summary native_frame_execution;
    vulkan_backend_frame_scoped_command_packet_summary scoped_command_packets;
    bool sdk_native_path_checked = false;
    bool sdk_adapter_ready = false;
    vulkan_sdk_native_path_status sdk_native_path_status =
        vulkan_sdk_native_path_status::not_checked;
    vulkan_sdk_capability_status sdk_capability_status =
        vulkan_sdk_capability_status::not_checked;
    vulkan_sdk_adapter_fallback_status sdk_adapter_fallback_status =
        vulkan_sdk_adapter_fallback_status::none;
    vulkan_sdk_external_header_evidence sdk_external_headers;
    bool sdk_external_headers_checked = false;
    bool sdk_vulkan_headers_available = false;
    bool sdk_vma_headers_available = false;
    std::string sdk_diagnostic;
    bool submit_batch_planning_checked = false;
    bool submit_batch_planning_completed = false;
    bool submit_batch_ready_for_queue = false;
    bool present_completion_planning_checked = false;
    bool present_completion_planning_completed = false;
    bool frame_completion_ready = false;
    bool command_recorder_lifecycle_ready = false;
    bool command_recorder_gate_checked = false;
    bool command_recorder_gate_allowed = false;
    bool command_recording_ready = false;
    bool command_submit_readiness_checked = false;
    bool command_submit_readiness_ready = false;
    bool frame_submit_completed = false;
    bool present_completed = false;
    bool frame_lifecycle_checked = false;
    bool frame_lifecycle_completed = false;
    std::size_t frame_lifecycle_attempted_step_count = 0;
    std::size_t frame_lifecycle_completed_step_count = 0;
    std::size_t planned_batch_count = 0;
    std::size_t recorded_batch_count = 0;
    std::size_t quad_batch_count = 0;
    std::size_t text_batch_count = 0;
    std::size_t image_batch_count = 0;
    std::size_t debug_bounds_batch_count = 0;
    std::size_t clipped_draw_call_count = 0;
    std::size_t discarded_draw_call_count = 0;

    bool completed() const
    {
        return checked && status == vulkan_backend_frame_pipeline_handoff_status::ready
            && fallback_reason == vulkan_backend_fallback_reason::none
            && loader_ready && instance_ready && device_ready && swapchain_ready
            && (!swapchain_recreate_policy_checked
                || swapchain_keep_rendering || swapchain_recreate_after_frame)
            && !swapchain_recreate_immediately && !swapchain_skip_submit
            && !swapchain_fatal_error
            && render_pass_ready && surface_ready && frame_plan_ready && pipeline_completed
            && resource_bindings_completed && resource_registry_completed
            && command_packets_completed && command_packet_execution_completed
            && command_recorder_operations_completed
            && command_buffer_recording_completed && command_buffer_ready_for_submit
            && (!native_function_table_checked
                || (native_command_buffer_recording_ready && native_queue_submit_ready
                    && native_queue_present_ready))
            && (!scoped_command_packets.checked || scoped_command_packets.completed())
            && (!sdk_native_path_checked || sdk_adapter_ready)
            && submit_batch_planning_completed && submit_batch_ready_for_queue
            && present_completion_planning_completed && frame_completion_ready
            && command_recorder_lifecycle_ready && command_recorder_gate_allowed
            && command_recording_ready
            && command_submit_readiness_ready
            && frame_submit_completed && present_completed && frame_lifecycle_completed;
    }

    bool blocked() const
    {
        return checked && status != vulkan_backend_frame_pipeline_handoff_status::ready;
    }
};

struct diagnostic_vulkan_command_recorder_options {
    bool ready = true;
    vulkan_command_recorder_failure_stage fail_at = vulkan_command_recorder_failure_stage::none;
    std::size_t fail_recording_index = 0;
};

class vulkan_command_recorder_interface {
public:
    virtual ~vulkan_command_recorder_interface() = default;

    virtual bool begin_recording(vulkan_surface_extent surface, std::size_t planned_batch_count) = 0;
    virtual bool record_draw_batch(const vulkan_draw_batch& batch) = 0;
    virtual bool finish_recording() = 0;
    virtual const vulkan_backend_command_recorder_state& recorder_state() const = 0;
};

class diagnostic_vulkan_command_recorder final : public vulkan_command_recorder_interface {
public:
    explicit diagnostic_vulkan_command_recorder(bool ready = true);
    explicit diagnostic_vulkan_command_recorder(diagnostic_vulkan_command_recorder_options options);

    bool begin_recording(vulkan_surface_extent surface, std::size_t planned_batch_count) override;
    bool record_draw_batch(const vulkan_draw_batch& batch) override;
    bool finish_recording() override;
    const vulkan_backend_command_recorder_state& recorder_state() const override;

private:
    diagnostic_vulkan_command_recorder_options options_;
    vulkan_backend_command_recorder_state state_;
};

struct vulkan_backend_frame_result {
    vulkan_surface_extent surface;
    render_rect viewport;
    vulkan_backend_lifecycle_readiness lifecycle;
    vulkan_backend_swapchain_lifecycle_state swapchain;
    vulkan_backend_swapchain_policy_state swapchain_policy;
    vulkan_swapchain_image_acquire_plan_result swapchain_image_acquire_plan;
    vulkan_swapchain_recreate_policy_result swapchain_recreate_policy;
    vulkan_backend_frame_sync_state frame_sync;
    vulkan_backend_frame_resource_lifetime_state frame_resources;
    vulkan_backend_frame_lifecycle_policy_state lifecycle_policy;
    vulkan_backend_frame_present_policy_state present_policy;
    vulkan_backend_resource_binding_state resource_bindings;
    vulkan_backend_resource_registry_state resource_registry;
    vulkan_backend_pipeline_state pipeline;
    vulkan_command_packet_bridge_result command_packets;
    vulkan_command_packet_execution_result command_packet_execution;
    vulkan_scoped_command_packet_execution_result scoped_command_packet_execution;
    vulkan_command_recorder_operation_plan command_recorder_operations;
    vulkan_command_buffer_record_result command_buffer_recording;
    vulkan_submit_batch_plan_result submit_batch_plan;
    vulkan_present_completion_plan_result present_completion_plan;
    vulkan_sdk_capability_result sdk_capabilities;
    vulkan_backend_command_recorder_state command_recorder;
    vulkan_command_submit_readiness_result command_submit;
    vulkan_queue_submit_present_result queue_submit;
    vulkan_backend_queue_submit_adapter_summary queue_submit_adapter;
    vulkan_backend_command_buffer_submit_state command_buffer_submit;
    vulkan_backend_frame_fallback_summary fallback_summary;
    vulkan_backend_frame_pipeline_handoff pipeline_handoff;
    vulkan_backend_frame_stage reached_stage = vulkan_backend_frame_stage::not_started;
    bool lifecycle_ready = false;
    bool surface_ready = false;
    bool frame_begun = false;
    bool commands_recorded = false;
    bool commands_recorded_gated_by_scoped_execution = false;
    bool frame_submitted = false;
    bool frame_presented = false;
    bool attempted = false;
    bool fallback_required = true;
    vulkan_backend_fallback_reason fallback_reason = vulkan_backend_fallback_reason::not_requested;
    std::size_t planned_batch_count = 0;
    std::size_t recorded_batch_count = 0;
    std::size_t clipped_draw_call_count = 0;
    std::size_t discarded_draw_call_count = 0;

    bool completed() const
    {
        return reached_stage == vulkan_backend_frame_stage::frame_presented
            && lifecycle_ready && surface_ready && frame_begun && commands_recorded
            && frame_submitted && frame_presented && swapchain.completed()
            && swapchain_policy.completed()
            && swapchain_image_acquire_plan.ready_for_command_recording()
            && (!swapchain_recreate_policy.checked
                || swapchain_recreate_policy.allows_frame_progress())
            && frame_sync.completed()
            && frame_resources.completed()
            && lifecycle_policy.completed()
            && present_policy.completed()
            && command_buffer_submit.completed()
            && (!queue_submit.checked || queue_submit.completed())
            && (!queue_submit_adapter.checked || queue_submit_adapter.completed())
            && (!pipeline_handoff.checked || pipeline_handoff.completed())
            && command_packets.completed()
            && command_packet_execution.completed()
            && (!scoped_command_packet_execution.checked
                || scoped_command_packet_execution.completed())
            && command_recorder_operations.completed()
            && command_buffer_recording.completed()
            && submit_batch_plan.completed()
            && present_completion_plan.completed()
            && (!sdk_capabilities.checked || sdk_capabilities.adapter_ready())
            && command_recorder.completed()
            && command_recorder.gate.completed()
            && resource_bindings.completed()
            && resource_registry.completed()
            && fallback_summary.completed()
            && !fallback_required;
    }
};

class vulkan_backend_device_interface {
public:
    virtual ~vulkan_backend_device_interface() = default;

    virtual vulkan_backend_lifecycle_readiness current_lifecycle_readiness() const = 0;
    virtual vulkan_surface_extent current_surface_extent() const = 0;
    virtual bool begin_frame(vulkan_surface_extent surface) = 0;
    virtual vulkan_swapchain_acquire_result acquire_next_image(vulkan_surface_extent surface) = 0;
    virtual bool record_frame_commands(const vulkan_frame_plan& plan) = 0;
    virtual bool submit_frame() = 0;
    virtual vulkan_swapchain_present_result present_image(vulkan_swapchain_image_id image_id) = 0;
    virtual bool present_frame() = 0;
};

class null_vulkan_backend_device final : public vulkan_backend_device_interface {
public:
    null_vulkan_backend_device();
    explicit null_vulkan_backend_device(vulkan_loader_readiness_state loader_readiness);
    explicit null_vulkan_backend_device(const vulkan_loader_probe_result& loader_probe);
    explicit null_vulkan_backend_device(vulkan_instance_create_result instance_result);
    explicit null_vulkan_backend_device(vulkan_device_create_result device_result);
    explicit null_vulkan_backend_device(vulkan_swapchain_create_result swapchain_result);
    explicit null_vulkan_backend_device(vulkan_render_pass_create_result render_pass_result);
    explicit null_vulkan_backend_device(
        vulkan_command_recording_readiness_result command_recording_result);
    explicit null_vulkan_backend_device(
        vulkan_command_submit_readiness_result command_submit_result);

    vulkan_backend_lifecycle_readiness current_lifecycle_readiness() const override;
    vulkan_surface_extent current_surface_extent() const override;
    bool begin_frame(vulkan_surface_extent surface) override;
    vulkan_swapchain_acquire_result acquire_next_image(vulkan_surface_extent surface) override;
    bool record_frame_commands(const vulkan_frame_plan& plan) override;
    bool submit_frame() override;
    vulkan_swapchain_present_result present_image(vulkan_swapchain_image_id image_id) override;
    bool present_frame() override;

private:
    vulkan_loader_readiness_state loader_readiness_;
    vulkan_instance_create_result instance_result_;
    vulkan_device_create_result device_result_;
    vulkan_swapchain_create_result swapchain_result_;
    vulkan_render_pass_create_result render_pass_result_;
    vulkan_command_recording_readiness_result command_recording_result_;
    vulkan_command_submit_readiness_result command_submit_result_;
};

vulkan_backend_resource_binding_state build_vulkan_resource_binding_state(
    const render_draw_list& draw_list,
    const vulkan_frame_plan& plan);

vulkan_backend_resource_registry_state build_vulkan_resource_registry_state(
    const render_draw_list& draw_list,
    const vulkan_frame_plan& plan,
    const vulkan_backend_resource_binding_state& resource_bindings);

vulkan_backend_queue_submit_adapter_summary summarize_vulkan_queue_submit_adapter_result(
    const vulkan_queue_submit_present_result& queue_submit);

vulkan_backend_frame_result apply_vulkan_queue_submit_adapter_result_to_frame(
    vulkan_backend_frame_result frame,
    vulkan_queue_submit_present_result queue_submit);

vulkan_backend_frame_result apply_vulkan_scoped_command_packet_execution_result_to_frame(
    vulkan_backend_frame_result frame,
    vulkan_scoped_command_packet_execution_result scoped_command_packets);

vulkan_command_packet_bridge_result build_vulkan_command_packet_bridge(
    const vulkan_frame_plan& plan,
    const vulkan_backend_pipeline_state& pipeline,
    const vulkan_backend_resource_binding_state& resource_bindings,
    const vulkan_backend_resource_registry_state& resource_registry);

vulkan_native_command_packet_executor_evidence build_vulkan_native_command_packet_executor_evidence(
    const vulkan_backend_frame_result& frame,
    const vulkan_native_function_table_diagnostics& native_functions = {});

vulkan_native_descriptor_set_allocation_result build_fake_vulkan_native_descriptor_set_allocation_result(
    const vulkan_command_packet_bridge_result& bridge,
    const vulkan_backend_resource_binding_state& resource_bindings,
    vulkan_native_descriptor_set_fake_allocator_options options = {});

vulkan_native_command_packet_executor_evidence merge_vulkan_native_descriptor_set_allocation_result(
    vulkan_native_command_packet_executor_evidence evidence,
    const vulkan_native_descriptor_set_allocation_result& descriptor_set_allocation);

vulkan_native_command_packet_executor_evidence build_vulkan_native_command_packet_executor_evidence(
    const vulkan_backend_frame_result& frame,
    const vulkan_native_descriptor_set_allocation_result& descriptor_set_allocation,
    const vulkan_native_function_table_diagnostics& native_functions = {});

vulkan_command_recorder_operation_plan build_vulkan_command_recorder_operation_plan(
    const vulkan_command_packet_bridge_result& bridge,
    const vulkan_command_packet_execution_result& execution);

vulkan_submit_batch_plan_result build_vulkan_submit_batch_plan(
    const vulkan_command_buffer_record_result& command_buffer_recording,
    const vulkan_command_submit_readiness_result& command_submit,
    const vulkan_native_function_table_diagnostics& native_functions = {},
    const vulkan_sdk_capability_result& sdk_capabilities = {});

vulkan_present_completion_plan_result build_vulkan_present_completion_plan(
    const vulkan_submit_batch_plan_result& submit_batch,
    const vulkan_queue_submit_present_result& queue_present,
    const vulkan_native_function_table_diagnostics& native_functions = {},
    const vulkan_sdk_capability_result& sdk_capabilities = {});

vulkan_native_queue_present_operation_result build_vulkan_native_queue_present_operation_plan(
    const vulkan_native_queue_present_operation_request& request);

vulkan_backend_frame_result apply_vulkan_sdk_capability_result_to_frame(
    vulkan_backend_frame_result frame,
    vulkan_sdk_capability_result sdk_capabilities);

vulkan_backend_frame_pipeline_handoff summarize_vulkan_frame_pipeline_handoff(
    const vulkan_backend_frame_result& frame);

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    const render_draw_list& draw_list,
    render_rect viewport);

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    vulkan_command_recorder_interface& command_recorder,
    const render_draw_list& draw_list,
    render_rect viewport);

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    vulkan_pipeline_cache_interface& pipeline_cache,
    vulkan_command_recorder_interface& command_recorder,
    const render_draw_list& draw_list,
    render_rect viewport);

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    vulkan_pipeline_cache_interface& pipeline_cache,
    vulkan_command_recorder_interface& command_recorder,
    vulkan_command_packet_executor_interface& scoped_command_packet_executor,
    const render_draw_list& draw_list,
    render_rect viewport);

} // namespace quiz_vulkan::render::vulkan_backend
