#pragma once

#include "render/render_draw_list.h"
#include "render/vulkan/vulkan_backend_command_recording.h"
#include "render/vulkan/vulkan_backend_command_submit.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_graphics_pipeline.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_queue_submit.h"
#include "render/vulkan/vulkan_backend_render_pass.h"
#include "render/vulkan/vulkan_backend_pipeline_layout.h"
#include "render/vulkan/vulkan_backend_shader_module.h"
#include "render/vulkan/vulkan_backend_swapchain.h"
#include "render/vulkan/vulkan_frame_plan.h"

#include <cstddef>
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
    vulkan_backend_lifecycle_readiness lifecycle;
    vulkan_backend_swapchain_lifecycle_state swapchain;
    vulkan_backend_swapchain_policy_state swapchain_policy;
    vulkan_backend_frame_sync_state frame_sync;
    vulkan_backend_frame_resource_lifetime_state frame_resources;
    vulkan_backend_frame_lifecycle_policy_state lifecycle_policy;
    vulkan_backend_frame_present_policy_state present_policy;
    vulkan_backend_resource_binding_state resource_bindings;
    vulkan_backend_resource_registry_state resource_registry;
    vulkan_backend_pipeline_state pipeline;
    vulkan_backend_command_recorder_state command_recorder;
    vulkan_command_submit_readiness_result command_submit;
    vulkan_queue_submit_present_result queue_submit;
    vulkan_backend_queue_submit_adapter_summary queue_submit_adapter;
    vulkan_backend_command_buffer_submit_state command_buffer_submit;
    vulkan_backend_frame_fallback_summary fallback_summary;
    vulkan_backend_frame_stage reached_stage = vulkan_backend_frame_stage::not_started;
    bool lifecycle_ready = false;
    bool surface_ready = false;
    bool frame_begun = false;
    bool commands_recorded = false;
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
            && frame_sync.completed()
            && frame_resources.completed()
            && lifecycle_policy.completed()
            && present_policy.completed()
            && command_buffer_submit.completed()
            && (!queue_submit.checked || queue_submit.completed())
            && (!queue_submit_adapter.checked || queue_submit_adapter.completed())
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

} // namespace quiz_vulkan::render::vulkan_backend
