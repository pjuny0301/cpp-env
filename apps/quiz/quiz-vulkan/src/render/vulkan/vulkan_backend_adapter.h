#pragma once

#include "render/render_draw_list.h"
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

struct vulkan_surface_extent {
    std::size_t width = 0;
    std::size_t height = 0;

    bool valid() const
    {
        return width > 0 && height > 0;
    }
};

struct vulkan_backend_lifecycle_readiness {
    bool instance_ready = false;
    bool device_ready = false;
    bool swapchain_ready = false;
    bool pipeline_ready = false;
    bool command_recorder_ready = false;

    bool ready_for_frame() const
    {
        return instance_ready && device_ready && swapchain_ready
            && pipeline_ready && command_recorder_ready;
    }
};

struct vulkan_swapchain_image_id {
    std::size_t value = 0;
};

struct vulkan_swapchain_image_state {
    vulkan_swapchain_image_id id;
    bool available = false;
    bool acquired = false;
    bool presented = false;

    bool ready_for_recording() const
    {
        return available && acquired;
    }
};

enum class vulkan_swapchain_acquire_status {
    not_requested,
    acquired,
    failed,
};

std::string_view swapchain_acquire_status_name(vulkan_swapchain_acquire_status status);

enum class vulkan_swapchain_present_status {
    not_requested,
    presented,
    failed,
};

std::string_view swapchain_present_status_name(vulkan_swapchain_present_status status);

struct vulkan_swapchain_acquire_result {
    vulkan_swapchain_acquire_status status = vulkan_swapchain_acquire_status::not_requested;
    vulkan_swapchain_image_state image;

    bool completed() const
    {
        return status == vulkan_swapchain_acquire_status::acquired && image.ready_for_recording();
    }
};

struct vulkan_swapchain_present_result {
    vulkan_swapchain_present_status status = vulkan_swapchain_present_status::not_requested;
    vulkan_swapchain_image_id image_id;

    bool completed() const
    {
        return status == vulkan_swapchain_present_status::presented;
    }
};

struct vulkan_backend_swapchain_lifecycle_state {
    bool acquire_requested = false;
    bool present_requested = false;
    vulkan_swapchain_acquire_result acquire;
    vulkan_swapchain_present_result present;

    bool acquired() const
    {
        return acquire_requested && acquire.completed();
    }

    bool presented() const
    {
        return present_requested && present.completed();
    }

    bool completed() const
    {
        return acquired() && presented();
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
    std::vector<vulkan_batch_resource_binding_snapshot> batch_snapshots;

    bool completed() const
    {
        return checked && !missing_resource && batch_snapshots.size() == planned_batch_count;
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

struct vulkan_shader_module_id {
    std::string value;

    bool empty() const
    {
        return value.empty();
    }
};

enum class vulkan_shader_stage {
    vertex,
    fragment,
};

std::string_view shader_stage_name(vulkan_shader_stage stage);

struct vulkan_shader_module_descriptor {
    vulkan_shader_module_id id;
    vulkan_shader_stage stage = vulkan_shader_stage::vertex;
    std::string entry_point = "main";

    bool valid() const
    {
        return !id.empty() && !entry_point.empty();
    }
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

    bool supports(vulkan_batch_kind kind) const;
    const vulkan_pipeline_descriptor* descriptor_for(vulkan_batch_kind kind) const;
    bool completed() const;
};

struct diagnostic_vulkan_pipeline_cache_options {
    bool default_available = true;
    bool use_default_shader_modules = true;
    bool use_default_pipeline_descriptors = true;
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

struct vulkan_backend_command_recorder_state {
    bool ready = false;
    bool frame_open = false;
    bool command_buffer_recorded = false;
    vulkan_command_recorder_failure_stage failure_stage = vulkan_command_recorder_failure_stage::none;
    std::size_t failure_recording_index = 0;
    std::size_t planned_batch_count = 0;
    std::size_t recorded_batch_count = 0;
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
            && recorded_batches.size() == recorded_batch_count;
    }

    bool failed() const
    {
        return failure_stage != vulkan_command_recorder_failure_stage::none;
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
    vulkan_backend_frame_sync_state frame_sync;
    vulkan_backend_frame_lifecycle_policy_state lifecycle_policy;
    vulkan_backend_resource_binding_state resource_bindings;
    vulkan_backend_resource_registry_state resource_registry;
    vulkan_backend_pipeline_state pipeline;
    vulkan_backend_command_recorder_state command_recorder;
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
            && frame_sync.completed()
            && lifecycle_policy.completed()
            && resource_bindings.completed()
            && resource_registry.completed()
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
    vulkan_backend_lifecycle_readiness current_lifecycle_readiness() const override;
    vulkan_surface_extent current_surface_extent() const override;
    bool begin_frame(vulkan_surface_extent surface) override;
    vulkan_swapchain_acquire_result acquire_next_image(vulkan_surface_extent surface) override;
    bool record_frame_commands(const vulkan_frame_plan& plan) override;
    bool submit_frame() override;
    vulkan_swapchain_present_result present_image(vulkan_swapchain_image_id image_id) override;
    bool present_frame() override;
};

vulkan_backend_resource_binding_state build_vulkan_resource_binding_state(
    const render_draw_list& draw_list,
    const vulkan_frame_plan& plan);

vulkan_backend_resource_registry_state build_vulkan_resource_registry_state(
    const render_draw_list& draw_list,
    const vulkan_frame_plan& plan,
    const vulkan_backend_resource_binding_state& resource_bindings);

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
