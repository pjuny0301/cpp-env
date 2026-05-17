#pragma once

#include <cstddef>
#include <string>

namespace quiz_vulkan::render {
class image_texture_pipeline_interface;
struct render_image_draw_list_frame_handoff_snapshot;
struct render_image_texture_batch_plan;
struct render_image_texture_frame_snapshot;
struct vulkan_renderer_image_texture_payload_frame;
} // namespace quiz_vulkan::render

namespace quiz_vulkan {

struct app_image_texture_payload_report {
    std::size_t resource_packet_count = 0;
    std::size_t resource_ready_count = 0;
    std::size_t quad_packet_count = 0;
    std::size_t quad_ready_count = 0;
    std::size_t payload_count = 0;
    std::size_t payload_ready_count = 0;
    std::size_t payload_placeholder_count = 0;
    std::size_t payload_blocked_count = 0;
    bool upload_result_available = false;
    bool resource_packets_ready = false;
    bool quad_packets_ready = false;
    bool draw_payloads_ready = false;
    std::string diagnostic;
};

app_image_texture_payload_report prepare_app_image_texture_payloads(
    const render::render_image_draw_list_frame_handoff_snapshot& handoff,
    const render::render_image_texture_batch_plan& plan,
    const render::render_image_texture_frame_snapshot& texture_frame,
    const render::image_texture_pipeline_interface& image_texture_pipeline,
    render::vulkan_renderer_image_texture_payload_frame* renderer_payloads = nullptr);

} // namespace quiz_vulkan
