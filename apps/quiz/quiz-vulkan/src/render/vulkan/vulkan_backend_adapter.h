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
    record_commands_failed,
    submit_frame_failed,
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
            && frame_submitted && frame_presented && !fallback_required;
    }
};

class vulkan_backend_device_interface {
public:
    virtual ~vulkan_backend_device_interface() = default;

    virtual vulkan_backend_lifecycle_readiness current_lifecycle_readiness() const = 0;
    virtual vulkan_surface_extent current_surface_extent() const = 0;
    virtual bool begin_frame(vulkan_surface_extent surface) = 0;
    virtual bool record_frame_commands(const vulkan_frame_plan& plan) = 0;
    virtual bool submit_frame() = 0;
    virtual bool present_frame() = 0;
};

class null_vulkan_backend_device final : public vulkan_backend_device_interface {
public:
    vulkan_backend_lifecycle_readiness current_lifecycle_readiness() const override;
    vulkan_surface_extent current_surface_extent() const override;
    bool begin_frame(vulkan_surface_extent surface) override;
    bool record_frame_commands(const vulkan_frame_plan& plan) override;
    bool submit_frame() override;
    bool present_frame() override;
};

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
