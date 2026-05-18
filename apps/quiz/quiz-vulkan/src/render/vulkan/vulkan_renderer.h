#pragma once

#include "render/render_draw_list.h"
#include "render/vulkan/vulkan_backend_adapter.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace quiz_vulkan::render {

struct vulkan_renderer_frame_stats {
    std::size_t command_count = 0;
    std::size_t draw_call_count = 0;
    std::size_t visible_draw_call_count = 0;
    std::size_t quad_count = 0;
    std::size_t text_count = 0;
    std::size_t image_count = 0;
    std::size_t push_clip_count = 0;
    std::size_t pop_clip_count = 0;
    std::size_t debug_bounds_count = 0;
    std::size_t text_run_count = 0;
    std::size_t text_character_count = 0;

    bool empty() const
    {
        return command_count == 0;
    }
};

struct vulkan_renderer_framebuffer {
    std::size_t width = 0;
    std::size_t height = 0;
    std::vector<unsigned char> rgba;
};

struct vulkan_renderer_image_texture_payload {
    std::size_t payload_index = 0;
    std::size_t draw_command_index = 0;
    render_node_id node_id;
    render_rect bounds;
    render_rect content_bounds;
    std::string stable_texture_cache_key;
    render_image_texture_id texture_id = 0;
    render_image_revision texture_revision = 0;
    std::size_t texture_width = 0;
    std::size_t texture_height = 0;
    bool draw_ready = false;
    bool placeholder_backed = false;
    bool blocked = false;
};

struct vulkan_renderer_image_texture_payload_frame {
    std::size_t payload_count = 0;
    std::size_t draw_ready_payload_count = 0;
    std::size_t placeholder_payload_count = 0;
    std::size_t blocked_payload_count = 0;
    bool draw_payloads_ready = false;
    std::vector<vulkan_renderer_image_texture_payload> payloads;
};

enum class vulkan_renderer_backend {
    cpu_fallback,
    vulkan,
};

enum class vulkan_renderer_native_window_kind {
    none,
    win32_hwnd,
};

struct vulkan_renderer_native_window_target {
    vulkan_renderer_native_window_kind kind = vulkan_renderer_native_window_kind::none;
    std::uintptr_t window = 0;
    std::uintptr_t display = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return kind != vulkan_renderer_native_window_kind::none && window != 0;
    }
};

struct vulkan_renderer_options {
    render_rect viewport{0.0f, 0.0f, 1280.0f, 720.0f};
    std::size_t fallback_surface_width = 160;
    std::size_t fallback_surface_height = 90;
    bool prefer_vulkan = true;
    vulkan_renderer_native_window_target native_window;
    vulkan_backend::vulkan_backend_device_interface* backend_device = nullptr;
    vulkan_backend::vulkan_pipeline_cache_interface* backend_pipeline_cache = nullptr;
    vulkan_backend::vulkan_command_recorder_interface* backend_command_recorder = nullptr;
    vulkan_backend::vulkan_command_packet_executor_interface* backend_command_packet_executor = nullptr;
};

struct vulkan_renderer_frame_summary {
    vulkan_renderer_backend backend = vulkan_renderer_backend::cpu_fallback;
    render_rect viewport;
    std::size_t surface_width = 0;
    std::size_t surface_height = 0;
    std::size_t shaded_pixel_count = 0;
    std::size_t clipped_draw_call_count = 0;
    std::size_t discarded_draw_call_count = 0;
    std::size_t backend_surface_width = 0;
    std::size_t backend_surface_height = 0;
    std::size_t backend_planned_batch_count = 0;
    std::size_t backend_recorded_batch_count = 0;
    std::size_t image_texture_payload_count = 0;
    std::size_t image_texture_payload_ready_count = 0;
    std::size_t image_texture_payload_placeholder_count = 0;
    std::size_t image_texture_payload_blocked_count = 0;
    vulkan_backend::vulkan_backend_frame_stage backend_reached_stage =
        vulkan_backend::vulkan_backend_frame_stage::not_started;
    vulkan_renderer_native_window_kind native_window_kind =
        vulkan_renderer_native_window_kind::none;
    bool native_window_target_ready = false;
    bool image_texture_payloads_consumed = false;
    bool image_texture_payloads_ready = false;
    bool backend_instance_ready = false;
    bool backend_device_ready = false;
    bool backend_swapchain_ready = false;
    bool backend_pipeline_ready = false;
    bool backend_command_recorder_ready = false;
    bool backend_command_recorder_frame_open = false;
    bool backend_command_buffer_recorded = false;
    bool backend_command_recorder_gate_checked = false;
    bool backend_command_recorder_gate_allowed = false;
    vulkan_backend::vulkan_command_recorder_gate_status backend_command_recorder_gate_status =
        vulkan_backend::vulkan_command_recorder_gate_status::not_checked;
    bool backend_lifecycle_ready = false;
    bool backend_surface_ready = false;
    bool backend_frame_begun = false;
    bool backend_commands_recorded = false;
    bool backend_frame_submitted = false;
    bool backend_frame_presented = false;
    bool backend_acquire_policy_checked = false;
    bool backend_acquire_requested = false;
    bool backend_acquire_backpressured = false;
    vulkan_backend::vulkan_frame_acquire_policy_status backend_acquire_policy_status =
        vulkan_backend::vulkan_frame_acquire_policy_status::not_checked;
    bool backend_present_policy_checked = false;
    bool backend_present_image_requested = false;
    bool backend_present_frame_requested = false;
    bool backend_present_image_presented = false;
    vulkan_backend::vulkan_frame_present_result_status backend_present_result_status =
        vulkan_backend::vulkan_frame_present_result_status::not_checked;
    bool backend_submit_adapter_checked = false;
    vulkan_backend::vulkan_queue_submit_present_status backend_submit_adapter_status =
        vulkan_backend::vulkan_queue_submit_present_status::not_requested;
    bool backend_submit_adapter_submit_called = false;
    bool backend_submit_adapter_present_called = false;
    bool backend_submit_adapter_submit_before_present = false;
    bool backend_submit_adapter_recoverable_failure = false;
    bool backend_submit_adapter_fatal_failure = false;
    bool backend_attempted = false;
    bool backend_fallback_required = true;
    vulkan_backend::vulkan_backend_fallback_reason backend_fallback_reason =
        vulkan_backend::vulkan_backend_fallback_reason::not_requested;

    bool used_cpu_fallback() const
    {
        return backend == vulkan_renderer_backend::cpu_fallback;
    }

    bool nonblank() const
    {
        return shaded_pixel_count > 0;
    }
};

class vulkan_renderer {
public:
    vulkan_renderer() = default;
    explicit vulkan_renderer(vulkan_renderer_options options);

    void submit(const render_draw_list& draw_list);
    void submit(
        const render_draw_list& draw_list,
        const vulkan_renderer_image_texture_payload_frame& image_texture_payloads);
    void submit(const std::vector<render_draw_command>& commands);
    void clear();

    const render_draw_list& last_draw_list() const;
    const vulkan_renderer_frame_stats& last_frame_stats() const;
    const vulkan_renderer_frame_summary& last_frame_summary() const;
    const vulkan_backend::vulkan_backend_frame_result& last_backend_frame_result() const;
    const vulkan_renderer_framebuffer& last_framebuffer() const;

    const vulkan_renderer_options& options() const;
    void set_options(vulkan_renderer_options options);

private:
    void submit(
        const render_draw_list& draw_list,
        const vulkan_renderer_image_texture_payload_frame* image_texture_payloads);

    static vulkan_renderer_frame_stats count_commands(const render_draw_list& draw_list);
    static vulkan_renderer_frame_summary summarize_cpu_fallback(
        const render_draw_list& draw_list,
        const vulkan_renderer_frame_stats& stats,
        const vulkan_backend::vulkan_backend_frame_result& backend_result,
        const vulkan_renderer_options& options,
        const vulkan_renderer_image_texture_payload_frame* image_texture_payloads);
    static vulkan_renderer_framebuffer rasterize_cpu_fallback_framebuffer(
        const render_draw_list& draw_list,
        const vulkan_renderer_options& options,
        const vulkan_renderer_image_texture_payload_frame* image_texture_payloads);

    vulkan_renderer_options options_;
    render_draw_list last_draw_list_;
    vulkan_renderer_frame_stats last_frame_stats_;
    vulkan_renderer_frame_summary last_frame_summary_;
    vulkan_backend::vulkan_backend_frame_result last_backend_frame_result_;
    vulkan_renderer_framebuffer last_framebuffer_;
};

} // namespace quiz_vulkan::render
