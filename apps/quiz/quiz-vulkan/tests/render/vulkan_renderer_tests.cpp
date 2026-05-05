#include "render/vulkan/vulkan_renderer.h"
#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_frame_plan.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

namespace {

class fake_vulkan_backend_device final : public quiz_vulkan::render::vulkan_backend::vulkan_backend_device_interface {
public:
    explicit fake_vulkan_backend_device(quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface)
        : surface_(surface)
    {
    }

    quiz_vulkan::render::vulkan_backend::vulkan_backend_lifecycle_readiness current_lifecycle_readiness() const override
    {
        return lifecycle;
    }

    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent current_surface_extent() const override
    {
        return surface_;
    }

    bool begin_frame(quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface) override
    {
        calls.push_back("begin");
        begun_surface = surface;
        return begin_succeeds;
    }

    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result acquire_next_image(
        quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface) override
    {
        calls.push_back("acquire");
        acquired_surface = surface;
        if (acquire_backpressured) {
            return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result{
                .status = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::backpressured,
                .image = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_state{
                    .id = next_image_id,
                    .available = false,
                    .acquired = false,
                    .presented = false,
                },
            };
        }
        if (!acquire_succeeds) {
            return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result{
                .status = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::failed,
                .image = {},
            };
        }

        return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result{
            .status = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::acquired,
            .image = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_state{
                .id = next_image_id,
                .available = true,
                .acquired = true,
                .presented = false,
            },
        };
    }

    bool record_frame_commands(const quiz_vulkan::render::vulkan_backend::vulkan_frame_plan& plan) override
    {
        calls.push_back("record");
        recorded_plan = plan;
        return record_succeeds;
    }

    bool submit_frame() override
    {
        calls.push_back("submit");
        return submit_succeeds;
    }

    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_result present_image(
        quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id image_id) override
    {
        calls.push_back("present_image");
        presented_image_id = image_id;
        return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_result{
            .status = present_image_succeeds
                ? quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_status::presented
                : quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_status::failed,
            .image_id = image_id,
        };
    }

    bool present_frame() override
    {
        calls.push_back("present");
        return present_succeeds;
    }

    bool begin_succeeds = true;
    bool acquire_succeeds = true;
    bool acquire_backpressured = false;
    bool record_succeeds = true;
    bool submit_succeeds = true;
    bool present_image_succeeds = true;
    bool present_succeeds = true;
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id next_image_id{.value = 7};
    quiz_vulkan::render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle{
        .instance_ready = true,
        .device_ready = true,
        .swapchain_ready = true,
        .pipeline_ready = true,
        .command_recorder_ready = true,
    };
    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent begun_surface;
    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent acquired_surface;
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id presented_image_id;
    quiz_vulkan::render::vulkan_backend::vulkan_frame_plan recorded_plan;
    std::vector<std::string> calls;

private:
    quiz_vulkan::render::vulkan_backend::vulkan_surface_extent surface_;
};

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

std::size_t framebuffer_pixel_offset(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer,
    std::size_t x,
    std::size_t y)
{
    return (y * framebuffer.width + x) * 4;
}

std::size_t count_nonzero_framebuffer_pixels(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer)
{
    std::size_t count = 0;
    for (std::size_t offset = 0; offset + 3 < framebuffer.rgba.size(); offset += 4) {
        if (framebuffer.rgba[offset] != 0 || framebuffer.rgba[offset + 1] != 0
            || framebuffer.rgba[offset + 2] != 0 || framebuffer.rgba[offset + 3] != 0) {
            ++count;
        }
    }
    return count;
}

bool framebuffer_pixel_is(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer,
    std::size_t x,
    std::size_t y,
    unsigned char red,
    unsigned char green,
    unsigned char blue,
    unsigned char alpha)
{
    if (x >= framebuffer.width || y >= framebuffer.height) {
        return false;
    }

    const std::size_t offset = framebuffer_pixel_offset(framebuffer, x, y);
    return offset + 3 < framebuffer.rgba.size()
        && framebuffer.rgba[offset] == red
        && framebuffer.rgba[offset + 1] == green
        && framebuffer.rgba[offset + 2] == blue
        && framebuffer.rgba[offset + 3] == alpha;
}

void test_vulkan_backend_fallback_reason_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::none)
            == std::string_view{"none"},
        "fallback reason name for none is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::not_requested)
            == std::string_view{"not_requested"},
        "fallback reason name for not requested is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable)
            == std::string_view{"instance_unavailable"},
        "fallback reason name for unavailable instance is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::device_unavailable)
            == std::string_view{"device_unavailable"},
        "fallback reason name for unavailable device is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::swapchain_unavailable)
            == std::string_view{"swapchain_unavailable"},
        "fallback reason name for unavailable swapchain is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable)
            == std::string_view{"pipeline_unavailable"},
        "fallback reason name for unavailable pipeline is stable");
    require(
        vulkan_backend::fallback_reason_name(
            vulkan_backend::vulkan_backend_fallback_reason::command_recorder_unavailable)
            == std::string_view{"command_recorder_unavailable"},
        "fallback reason name for unavailable command recorder is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::surface_unavailable)
            == std::string_view{"surface_unavailable"},
        "fallback reason name for surface unavailable is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::viewport_unavailable)
            == std::string_view{"viewport_unavailable"},
        "fallback reason name for viewport unavailable is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::begin_frame_failed)
            == std::string_view{"begin_frame_failed"},
        "fallback reason name for begin frame failure is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed)
            == std::string_view{"acquire_image_failed"},
        "fallback reason name for acquire image failure is stable");
    require(
        vulkan_backend::fallback_reason_name(
            vulkan_backend::vulkan_backend_fallback_reason::resource_binding_unavailable)
            == std::string_view{"resource_binding_unavailable"},
        "fallback reason name for resource binding unavailable is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed)
            == std::string_view{"record_commands_failed"},
        "fallback reason name for command recording failure is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed)
            == std::string_view{"submit_frame_failed"},
        "fallback reason name for submit failure is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::present_image_failed)
            == std::string_view{"present_image_failed"},
        "fallback reason name for present image failure is stable");
    require(
        vulkan_backend::fallback_reason_name(vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed)
            == std::string_view{"present_frame_failed"},
        "fallback reason name for present failure is stable");
}

void test_vulkan_swapchain_status_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::swapchain_acquire_status_name(
            vulkan_backend::vulkan_swapchain_acquire_status::not_requested)
            == std::string_view{"not_requested"},
        "swapchain acquire status name for not requested is stable");
    require(
        vulkan_backend::swapchain_acquire_status_name(
            vulkan_backend::vulkan_swapchain_acquire_status::acquired)
            == std::string_view{"acquired"},
        "swapchain acquire status name for acquired is stable");
    require(
        vulkan_backend::swapchain_acquire_status_name(
            vulkan_backend::vulkan_swapchain_acquire_status::backpressured)
            == std::string_view{"backpressured"},
        "swapchain acquire status name for backpressured is stable");
    require(
        vulkan_backend::swapchain_acquire_status_name(
            vulkan_backend::vulkan_swapchain_acquire_status::failed)
            == std::string_view{"failed"},
        "swapchain acquire status name for failed is stable");
    require(
        vulkan_backend::swapchain_present_status_name(
            vulkan_backend::vulkan_swapchain_present_status::not_requested)
            == std::string_view{"not_requested"},
        "swapchain present status name for not requested is stable");
    require(
        vulkan_backend::swapchain_present_status_name(
            vulkan_backend::vulkan_swapchain_present_status::presented)
            == std::string_view{"presented"},
        "swapchain present status name for presented is stable");
    require(
        vulkan_backend::swapchain_present_status_name(
            vulkan_backend::vulkan_swapchain_present_status::failed)
            == std::string_view{"failed"},
        "swapchain present status name for failed is stable");
    require(
        vulkan_backend::swapchain_present_mode_name(
            vulkan_backend::vulkan_swapchain_present_mode::immediate)
            == std::string_view{"immediate"},
        "swapchain present mode name for immediate is stable");
    require(
        vulkan_backend::swapchain_present_mode_name(
            vulkan_backend::vulkan_swapchain_present_mode::mailbox)
            == std::string_view{"mailbox"},
        "swapchain present mode name for mailbox is stable");
    require(
        vulkan_backend::swapchain_present_mode_name(
            vulkan_backend::vulkan_swapchain_present_mode::fifo)
            == std::string_view{"fifo"},
        "swapchain present mode name for fifo is stable");
    require(
        vulkan_backend::swapchain_present_mode_name(
            vulkan_backend::vulkan_swapchain_present_mode::fifo_relaxed)
            == std::string_view{"fifo_relaxed"},
        "swapchain present mode name for relaxed fifo is stable");
}

void test_vulkan_frame_present_policy_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::frame_acquire_policy_status_name(
            vulkan_backend::vulkan_frame_acquire_policy_status::not_checked)
            == std::string_view{"not_checked"},
        "frame acquire policy status name for not checked is stable");
    require(
        vulkan_backend::frame_acquire_policy_status_name(
            vulkan_backend::vulkan_frame_acquire_policy_status::not_requested)
            == std::string_view{"not_requested"},
        "frame acquire policy status name for not requested is stable");
    require(
        vulkan_backend::frame_acquire_policy_status_name(
            vulkan_backend::vulkan_frame_acquire_policy_status::acquired)
            == std::string_view{"acquired"},
        "frame acquire policy status name for acquired is stable");
    require(
        vulkan_backend::frame_acquire_policy_status_name(
            vulkan_backend::vulkan_frame_acquire_policy_status::backpressured)
            == std::string_view{"backpressured"},
        "frame acquire policy status name for backpressured is stable");
    require(
        vulkan_backend::frame_acquire_policy_status_name(
            vulkan_backend::vulkan_frame_acquire_policy_status::failed)
            == std::string_view{"failed"},
        "frame acquire policy status name for failed is stable");

    require(
        vulkan_backend::frame_present_result_status_name(
            vulkan_backend::vulkan_frame_present_result_status::not_checked)
            == std::string_view{"not_checked"},
        "frame present result status name for not checked is stable");
    require(
        vulkan_backend::frame_present_result_status_name(
            vulkan_backend::vulkan_frame_present_result_status::not_requested)
            == std::string_view{"not_requested"},
        "frame present result status name for not requested is stable");
    require(
        vulkan_backend::frame_present_result_status_name(
            vulkan_backend::vulkan_frame_present_result_status::image_presented)
            == std::string_view{"image_presented"},
        "frame present result status name for image presented is stable");
    require(
        vulkan_backend::frame_present_result_status_name(
            vulkan_backend::vulkan_frame_present_result_status::frame_presented)
            == std::string_view{"frame_presented"},
        "frame present result status name for frame presented is stable");
    require(
        vulkan_backend::frame_present_result_status_name(
            vulkan_backend::vulkan_frame_present_result_status::image_failed)
            == std::string_view{"image_failed"},
        "frame present result status name for image failed is stable");
    require(
        vulkan_backend::frame_present_result_status_name(
            vulkan_backend::vulkan_frame_present_result_status::frame_failed)
            == std::string_view{"frame_failed"},
        "frame present result status name for frame failed is stable");
}

void test_vulkan_backend_frame_stage_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::not_started)
            == std::string_view{"not_started"},
        "frame stage name for not started is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::backend_attempted)
            == std::string_view{"backend_attempted"},
        "frame stage name for backend attempted is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::lifecycle_ready)
            == std::string_view{"lifecycle_ready"},
        "frame stage name for lifecycle ready is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::surface_extent_ready)
            == std::string_view{"surface_extent_ready"},
        "frame stage name for surface extent ready is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready)
            == std::string_view{"frame_plan_ready"},
        "frame stage name for frame plan ready is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::frame_begun)
            == std::string_view{"frame_begun"},
        "frame stage name for frame begun is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::commands_recorded)
            == std::string_view{"commands_recorded"},
        "frame stage name for commands recorded is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::frame_submitted)
            == std::string_view{"frame_submitted"},
        "frame stage name for frame submitted is stable");
    require(
        vulkan_backend::frame_stage_name(vulkan_backend::vulkan_backend_frame_stage::frame_presented)
            == std::string_view{"frame_presented"},
        "frame stage name for frame presented is stable");
}

void test_vulkan_frame_submit_status_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::frame_submit_status_name(
            vulkan_backend::vulkan_frame_submit_status::not_requested)
            == std::string_view{"not_requested"},
        "frame submit status name for not requested is stable");
    require(
        vulkan_backend::frame_submit_status_name(vulkan_backend::vulkan_frame_submit_status::pending)
            == std::string_view{"pending"},
        "frame submit status name for pending is stable");
    require(
        vulkan_backend::frame_submit_status_name(vulkan_backend::vulkan_frame_submit_status::submitted)
            == std::string_view{"submitted"},
        "frame submit status name for submitted is stable");
    require(
        vulkan_backend::frame_submit_status_name(vulkan_backend::vulkan_frame_submit_status::failed)
            == std::string_view{"failed"},
        "frame submit status name for failed is stable");
}

void test_vulkan_frame_lifecycle_policy_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::frame_lifecycle_step_name(vulkan_backend::vulkan_frame_lifecycle_step::acquire)
            == std::string_view{"acquire"},
        "frame lifecycle step name for acquire is stable");
    require(
        vulkan_backend::frame_lifecycle_step_name(vulkan_backend::vulkan_frame_lifecycle_step::begin)
            == std::string_view{"begin"},
        "frame lifecycle step name for begin is stable");
    require(
        vulkan_backend::frame_lifecycle_step_name(vulkan_backend::vulkan_frame_lifecycle_step::render)
            == std::string_view{"render"},
        "frame lifecycle step name for render is stable");
    require(
        vulkan_backend::frame_lifecycle_step_name(vulkan_backend::vulkan_frame_lifecycle_step::submit)
            == std::string_view{"submit"},
        "frame lifecycle step name for submit is stable");
    require(
        vulkan_backend::frame_lifecycle_step_name(vulkan_backend::vulkan_frame_lifecycle_step::present)
            == std::string_view{"present"},
        "frame lifecycle step name for present is stable");
    require(
        vulkan_backend::frame_lifecycle_order_status_name(
            vulkan_backend::vulkan_frame_lifecycle_order_status::not_started)
            == std::string_view{"not_started"},
        "frame lifecycle status name for not started is stable");
    require(
        vulkan_backend::frame_lifecycle_order_status_name(
            vulkan_backend::vulkan_frame_lifecycle_order_status::started)
            == std::string_view{"started"},
        "frame lifecycle status name for started is stable");
    require(
        vulkan_backend::frame_lifecycle_order_status_name(
            vulkan_backend::vulkan_frame_lifecycle_order_status::completed)
            == std::string_view{"completed"},
        "frame lifecycle status name for completed is stable");
    require(
        vulkan_backend::frame_lifecycle_order_status_name(
            vulkan_backend::vulkan_frame_lifecycle_order_status::skipped)
            == std::string_view{"skipped"},
        "frame lifecycle status name for skipped is stable");
    require(
        vulkan_backend::frame_lifecycle_order_status_name(
            vulkan_backend::vulkan_frame_lifecycle_order_status::failed)
            == std::string_view{"failed"},
        "frame lifecycle status name for failed is stable");
    require(
        vulkan_backend::frame_lifecycle_order_status_name(
            vulkan_backend::vulkan_frame_lifecycle_order_status::out_of_order)
            == std::string_view{"out_of_order"},
        "frame lifecycle status name for out of order is stable");
    require(
        vulkan_backend::frame_lifecycle_failure_classification_name(
            vulkan_backend::vulkan_frame_lifecycle_failure_classification::none)
            == std::string_view{"none"},
        "frame lifecycle classification name for none is stable");
    require(
        vulkan_backend::frame_lifecycle_failure_classification_name(
            vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable)
            == std::string_view{"recoverable"},
        "frame lifecycle classification name for recoverable is stable");
    require(
        vulkan_backend::frame_lifecycle_failure_classification_name(
            vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal)
            == std::string_view{"fatal"},
        "frame lifecycle classification name for fatal is stable");
}

void test_vulkan_frame_resource_lifetime_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::frame_resource_kind_name(vulkan_backend::vulkan_frame_resource_kind::swapchain_image)
            == std::string_view{"swapchain_image"},
        "frame resource kind name for swapchain image is stable");
    require(
        vulkan_backend::frame_resource_kind_name(vulkan_backend::vulkan_frame_resource_kind::command_buffer)
            == std::string_view{"command_buffer"},
        "frame resource kind name for command buffer is stable");
    require(
        vulkan_backend::frame_resource_kind_name(vulkan_backend::vulkan_frame_resource_kind::descriptor_set)
            == std::string_view{"descriptor_set"},
        "frame resource kind name for descriptor set is stable");
    require(
        vulkan_backend::frame_resource_release_stage_name(vulkan_backend::vulkan_frame_resource_release_stage::none)
            == std::string_view{"none"},
        "frame resource release stage name for none is stable");
    require(
        vulkan_backend::frame_resource_release_stage_name(
            vulkan_backend::vulkan_frame_resource_release_stage::after_present)
            == std::string_view{"after_present"},
        "frame resource release stage name for after present is stable");
    require(
        vulkan_backend::frame_resource_release_stage_name(
            vulkan_backend::vulkan_frame_resource_release_stage::fallback_cleanup)
            == std::string_view{"fallback_cleanup"},
        "frame resource release stage name for fallback cleanup is stable");
}

void test_vulkan_descriptor_validation_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::descriptor_validation_status_name(
            vulkan_backend::vulkan_descriptor_validation_status::not_checked)
            == std::string_view{"not_checked"},
        "descriptor validation status name for not checked is stable");
    require(
        vulkan_backend::descriptor_validation_status_name(vulkan_backend::vulkan_descriptor_validation_status::valid)
            == std::string_view{"valid"},
        "descriptor validation status name for valid is stable");
    require(
        vulkan_backend::descriptor_validation_status_name(
            vulkan_backend::vulkan_descriptor_validation_status::missing_required_resource)
            == std::string_view{"missing_required_resource"},
        "descriptor validation status name for missing resource is stable");
    require(
        vulkan_backend::descriptor_validation_status_name(
            vulkan_backend::vulkan_descriptor_validation_status::duplicate_binding)
            == std::string_view{"duplicate_binding"},
        "descriptor validation status name for duplicate binding is stable");
    require(
        vulkan_backend::descriptor_validation_status_name(
            vulkan_backend::vulkan_descriptor_validation_status::invalid_layout)
            == std::string_view{"invalid_layout"},
        "descriptor validation status name for invalid layout is stable");
}

void test_vulkan_shader_stage_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::shader_stage_name(vulkan_backend::vulkan_shader_stage::vertex)
            == std::string_view{"vertex"},
        "shader stage name for vertex is stable");
    require(
        vulkan_backend::shader_stage_name(vulkan_backend::vulkan_shader_stage::fragment)
            == std::string_view{"fragment"},
        "shader stage name for fragment is stable");
}

void test_vulkan_pipeline_lifecycle_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::pipeline_lifecycle_stage_name(
            vulkan_backend::vulkan_pipeline_lifecycle_stage::render_pass)
            == std::string_view{"render_pass"},
        "pipeline lifecycle stage name for render pass is stable");
    require(
        vulkan_backend::pipeline_lifecycle_stage_name(
            vulkan_backend::vulkan_pipeline_lifecycle_stage::shader_stages)
            == std::string_view{"shader_stages"},
        "pipeline lifecycle stage name for shader stages is stable");
    require(
        vulkan_backend::pipeline_lifecycle_stage_name(
            vulkan_backend::vulkan_pipeline_lifecycle_stage::pipeline)
            == std::string_view{"pipeline"},
        "pipeline lifecycle stage name for pipeline is stable");
    require(
        vulkan_backend::pipeline_lifecycle_status_name(
            vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked)
            == std::string_view{"not_checked"},
        "pipeline lifecycle status name for not checked is stable");
    require(
        vulkan_backend::pipeline_lifecycle_status_name(
            vulkan_backend::vulkan_pipeline_lifecycle_status::ready)
            == std::string_view{"ready"},
        "pipeline lifecycle status name for ready is stable");
    require(
        vulkan_backend::pipeline_lifecycle_status_name(
            vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable)
            == std::string_view{"unavailable"},
        "pipeline lifecycle status name for unavailable is stable");
}

quiz_vulkan::render::render_draw_command make_quad_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    quiz_vulkan::render::render_color color)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::quad,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "test", .color = color},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
}

quiz_vulkan::render::render_draw_command make_text_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    std::string text)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::text,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "white", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {render_text_run{.text = std::move(text), .style_token = "title"}},
        .image = {},
    };
}

quiz_vulkan::render::render_draw_command make_image_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    std::string uri)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::image,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "image", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = render_image_ref{.uri = std::move(uri), .alt_text = "fixture", .aspect_ratio = 1.0f},
    };
}

void test_draw_list_submission_counts_generic_work()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "panel",
        render_rect{20.0f, 16.0f, 260.0f, 120.0f},
        render_color{0.14f, 0.22f, 0.31f, 1.0f}));
    draw_list.commands.push_back(make_text_command(
        "panel_title",
        render_rect{32.0f, 28.0f, 236.0f, 24.0f},
        "Renderer smoke"));
    draw_list.commands.push_back(make_image_command(
        "panel_image",
        render_rect{32.0f, 56.0f, 96.0f, 64.0f},
        "fixture://renderer/card.png"));

    vulkan_renderer renderer;
    renderer.submit(draw_list);

    const vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    require(stats.command_count == 3, "renderer consumed explicit render command count");
    require(stats.draw_call_count == 3, "quad, text, and image are draw calls");
    require(stats.visible_draw_call_count == 3, "all test draw calls are visible");
    require(stats.quad_count == 1, "renderer counts generic quads");
    require(stats.text_count == 1, "renderer counts generic text commands");
    require(stats.image_count == 1, "renderer counts generic image commands");
    require(stats.text_run_count == 1, "renderer counts text runs without owning text layout");
    require(stats.text_character_count == std::string{"Renderer smoke"}.size(), "renderer counts text payload bytes");

    const vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    require(summary.used_cpu_fallback(), "current backend reports CPU fallback");
    require(summary.nonblank(), "generic draw list shades fallback pixels");
    require(summary.surface_width == renderer.options().fallback_surface_width, "surface width is recorded");
    require(summary.surface_height == renderer.options().fallback_surface_height, "surface height is recorded");
    require(summary.discarded_draw_call_count == 0, "all explicit draw calls are drawable");
    require(summary.backend_fallback_required, "renderer summary exposes backend fallback requirement");
    require(!summary.backend_surface_ready, "renderer summary exposes missing backend surface");
    require(!summary.backend_frame_begun, "renderer summary exposes frame begin status");
    require(!summary.backend_commands_recorded, "renderer summary exposes command recording status");
    require(!summary.backend_frame_submitted, "renderer summary exposes frame submit status");
    require(!summary.backend_frame_presented, "renderer summary exposes frame present status");
    require(!summary.backend_acquire_policy_checked, "renderer summary exposes unchecked acquire policy");
    require(!summary.backend_acquire_requested, "renderer summary exposes acquire request status");
    require(!summary.backend_acquire_backpressured, "renderer summary exposes acquire backpressure status");
    require(
        summary.backend_acquire_policy_status
            == vulkan_backend::vulkan_frame_acquire_policy_status::not_checked,
        "renderer summary exposes acquire policy status");
    require(!summary.backend_present_policy_checked, "renderer summary exposes unchecked present policy");
    require(!summary.backend_present_image_requested, "renderer summary exposes image present request status");
    require(!summary.backend_present_frame_requested, "renderer summary exposes frame present request status");
    require(!summary.backend_present_image_presented, "renderer summary exposes image present result");
    require(
        summary.backend_present_result_status
            == vulkan_backend::vulkan_frame_present_result_status::not_checked,
        "renderer summary exposes present result status");
    require(summary.backend_attempted, "renderer summary exposes backend attempt status");
    require(summary.backend_planned_batch_count == 0, "renderer summary exposes backend planned batch count");
    require(summary.backend_recorded_batch_count == 0, "renderer summary exposes backend recorded batch count");
    require(summary.backend_surface_width == 0, "renderer summary exposes backend surface width");
    require(summary.backend_surface_height == 0, "renderer summary exposes backend surface height");
    require(
        summary.backend_reached_stage == vulkan_backend::vulkan_backend_frame_stage::backend_attempted,
        "renderer summary exposes reached backend stage");
    require(!summary.backend_instance_ready, "renderer summary exposes backend instance readiness");
    require(!summary.backend_device_ready, "renderer summary exposes backend device readiness");
    require(!summary.backend_swapchain_ready, "renderer summary exposes backend swapchain readiness");
    require(!summary.backend_pipeline_ready, "renderer summary exposes backend pipeline readiness");
    require(!summary.backend_command_recorder_ready, "renderer summary exposes backend command recorder readiness");
    require(!summary.backend_command_recorder_frame_open, "renderer summary exposes command recorder frame-open state");
    require(!summary.backend_command_buffer_recorded, "renderer summary exposes command buffer recorded state");
    require(!summary.backend_command_recorder_gate_checked, "renderer summary exposes unchecked recorder gate");
    require(!summary.backend_command_recorder_gate_allowed, "renderer summary exposes blocked recorder gate by default");
    require(
        summary.backend_command_recorder_gate_status
            == vulkan_backend::vulkan_command_recorder_gate_status::not_checked,
        "renderer summary exposes recorder gate status");
    require(!summary.backend_lifecycle_ready, "renderer summary exposes aggregate lifecycle readiness");
    require(
        summary.backend_fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable,
        "renderer summary exposes backend fallback reason");

    const vulkan_backend::vulkan_backend_frame_result& backend_result = renderer.last_backend_frame_result();
    require(backend_result.fallback_required, "renderer retains backend fallback requirement");
    require(backend_result.attempted, "renderer retains backend attempt status");
    require(!backend_result.lifecycle.ready_for_frame(), "renderer retains backend lifecycle readiness");
    require(!backend_result.command_recorder.ready, "renderer retains command recorder readiness");
    require(backend_result.command_recorder.empty(), "renderer retains empty command recorder state");
    require(!backend_result.lifecycle_ready, "renderer retains aggregate lifecycle readiness");
    require(!backend_result.surface_ready, "renderer retains backend surface readiness");
    require(!backend_result.frame_begun, "renderer retains backend begin status");
    require(!backend_result.commands_recorded, "renderer retains backend record status");
    require(!backend_result.frame_submitted, "renderer retains backend submit status");
    require(!backend_result.frame_presented, "renderer retains backend present status");
    require(backend_result.planned_batch_count == 0, "renderer retains backend planned batch count");
    require(backend_result.recorded_batch_count == 0, "renderer retains backend recorded batch count");
    require(
        backend_result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::backend_attempted,
        "renderer retains reached backend stage");
    require(
        backend_result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable,
        "renderer retains backend fallback reason");
    require(count_nonzero_framebuffer_pixels(renderer.last_framebuffer()) > 0, "fallback framebuffer receives pixels");

    renderer.clear();
    require(renderer.last_draw_list().empty(), "clear drops the retained draw list");
    require(renderer.last_frame_stats().empty(), "clear drops frame stats");
    require(!renderer.last_frame_summary().nonblank(), "clear drops summary coverage");
    require(renderer.last_frame_summary().backend_fallback_required, "clear resets backend summary diagnostics");
    require(!renderer.last_frame_summary().backend_attempted, "clear resets backend attempt diagnostics");
    require(
        renderer.last_frame_summary().backend_reached_stage
            == vulkan_backend::vulkan_backend_frame_stage::not_started,
        "clear resets backend reached stage");
    require(
        renderer.last_frame_summary().backend_fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::not_requested,
        "clear resets backend fallback reason");
    require(renderer.last_backend_frame_result().fallback_required, "clear resets retained backend result");
    require(!renderer.last_backend_frame_result().attempted, "clear resets retained backend attempt status");
    require(
        renderer.last_backend_frame_result().reached_stage
            == vulkan_backend::vulkan_backend_frame_stage::not_started,
        "clear resets retained backend reached stage");
    require(
        renderer.last_backend_frame_result().fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::not_requested,
        "clear resets retained backend fallback reason");
    require(renderer.last_framebuffer().rgba.empty(), "clear drops framebuffer bytes");
}

void test_renderer_backend_diagnostics_report_vulkan_not_requested()
{
    using namespace quiz_vulkan::render;

    vulkan_renderer renderer(vulkan_renderer_options{
        .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        .fallback_surface_width = 10,
        .fallback_surface_height = 10,
        .prefer_vulkan = false,
    });
    renderer.submit(std::vector<render_draw_command>{make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f})});

    const vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    require(summary.used_cpu_fallback(), "renderer keeps CPU fallback when Vulkan is not requested");
    require(summary.nonblank(), "renderer still shades fallback pixels when Vulkan is not requested");
    require(!summary.backend_attempted, "renderer summary records that Vulkan was not attempted");
    require(summary.backend_fallback_required, "renderer summary still requires fallback when Vulkan is not requested");
    require(
        summary.backend_reached_stage == vulkan_backend::vulkan_backend_frame_stage::not_started,
        "renderer summary records no reached Vulkan stage when not requested");
    require(
        summary.backend_fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::not_requested,
        "renderer summary records Vulkan not requested fallback reason");

    const vulkan_backend::vulkan_backend_frame_result& backend_result = renderer.last_backend_frame_result();
    require(!backend_result.attempted, "renderer backend result records Vulkan was not attempted");
    require(backend_result.fallback_required, "renderer backend result keeps fallback required");
    require(
        backend_result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::not_started,
        "renderer backend result records no reached Vulkan stage when not requested");
    require(
        backend_result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::not_requested,
        "renderer backend result records Vulkan not requested fallback reason");
}

void test_cpu_fallback_clips_and_discards()
{
    using namespace quiz_vulkan::render;

    render_draw_command clip{
        .type = render_draw_command_type::push_clip,
        .node_id = "clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{0.0f, 0.0f, 50.0f, 50.0f},
        .content_bounds = render_rect{0.0f, 0.0f, 50.0f, 50.0f},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command quad = make_quad_command(
        "quad",
        render_rect{25.0f, 25.0f, 50.0f, 50.0f},
        render_color{0.0f, 1.0f, 0.0f, 1.0f});
    render_draw_command transparent = make_quad_command(
        "transparent",
        render_rect{10.0f, 10.0f, 10.0f, 10.0f},
        render_color{0.0f, 0.0f, 0.0f, 0.0f});
    render_draw_command pop{
        .type = render_draw_command_type::pop_clip,
        .node_id = "clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = clip.bounds,
        .content_bounds = clip.content_bounds,
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };

    vulkan_renderer_options options{
        .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        .fallback_surface_width = 10,
        .fallback_surface_height = 10,
        .prefer_vulkan = true,
    };

    vulkan_renderer renderer(options);
    renderer.submit(std::vector<render_draw_command>{clip, quad, transparent, pop});

    const vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    require(stats.command_count == 4, "clip test command count");
    require(stats.draw_call_count == 2, "only quads are draw calls");
    require(stats.visible_draw_call_count == 1, "transparent quad is not visible");
    require(stats.push_clip_count == 1, "push clip counted");
    require(stats.pop_clip_count == 1, "pop clip counted");

    const vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    require(summary.used_cpu_fallback(), "clip test uses CPU fallback");
    require(summary.nonblank(), "visible clipped quad shades pixels");
    require(summary.clipped_draw_call_count == 1, "quad is clipped by clip scope");
    require(summary.discarded_draw_call_count == 1, "transparent quad is discarded");
    require(summary.shaded_pixel_count == 9, "clipped quad shades expected pixel area");

    const vulkan_renderer_framebuffer& framebuffer = renderer.last_framebuffer();
    require(framebuffer.width == options.fallback_surface_width, "framebuffer records fallback width");
    require(framebuffer.height == options.fallback_surface_height, "framebuffer records fallback height");
    require(framebuffer.rgba.size() == framebuffer.width * framebuffer.height * 4, "framebuffer stores RGBA bytes");
    require(count_nonzero_framebuffer_pixels(framebuffer) == 9, "only clipped visible quad pixels are colored");
    require(framebuffer_pixel_is(framebuffer, 2, 2, 0, 255, 0, 255), "visible clipped quad writes green RGBA");
    require(framebuffer_pixel_is(framebuffer, 1, 1, 0, 0, 0, 0), "transparent command does not color pixels");
    require(framebuffer_pixel_is(framebuffer, 5, 5, 0, 0, 0, 0), "clipped command does not color out-of-clip pixels");
}

void test_degenerate_surface_discards_draw_calls()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));

    vulkan_renderer renderer(vulkan_renderer_options{
        .viewport = render_rect{0.0f, 0.0f, 0.0f, 100.0f},
        .fallback_surface_width = 10,
        .fallback_surface_height = 10,
        .prefer_vulkan = true,
    });
    renderer.submit(draw_list);

    require(renderer.last_frame_stats().draw_call_count == 1, "degenerate viewport still counts submitted draw call");
    require(!renderer.last_frame_summary().nonblank(), "degenerate viewport cannot shade pixels");
    require(renderer.last_frame_summary().discarded_draw_call_count == 1, "degenerate viewport discards draw call");
    require(count_nonzero_framebuffer_pixels(renderer.last_framebuffer()) == 0, "degenerate viewport leaves framebuffer blank");
}

void test_vulkan_frame_plan_builds_scissored_batches_from_render_contracts()
{
    using namespace quiz_vulkan::render;

    render_draw_command clip{
        .type = render_draw_command_type::push_clip,
        .node_id = "clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{10.0f, 20.0f, 50.0f, 40.0f},
        .content_bounds = render_rect{10.0f, 20.0f, 50.0f, 40.0f},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command quad = make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 30.0f, 40.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f});
    render_draw_command text = make_text_command(
        "text",
        render_rect{20.0f, 30.0f, 10.0f, 10.0f},
        "batch");
    render_draw_command transparent = make_quad_command(
        "transparent",
        render_rect{20.0f, 30.0f, 10.0f, 10.0f},
        render_color{0.0f, 0.0f, 0.0f, 0.0f});
    render_draw_command pop{
        .type = render_draw_command_type::pop_clip,
        .node_id = "clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = clip.bounds,
        .content_bounds = clip.content_bounds,
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };

    render_draw_list draw_list;
    draw_list.commands = {clip, quad, text, transparent, pop};

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    require(plan.batches.size() == 2, "frame plan emits drawable quad and text batches");
    require(plan.clipped_draw_call_count == 1, "frame plan records clipped backend batch");
    require(plan.discarded_draw_call_count == 1, "frame plan discards transparent backend batch");

    const vulkan_backend::vulkan_draw_batch& quad_batch = plan.batches[0];
    require(quad_batch.kind == vulkan_backend::vulkan_batch_kind::quad, "first backend batch is a quad");
    require(quad_batch.command_index == 1, "quad backend batch keeps source command index");
    require(quad_batch.clipped_bounds.x == 10.0f, "quad clipped x");
    require(quad_batch.clipped_bounds.y == 20.0f, "quad clipped y");
    require(quad_batch.clipped_bounds.width == 20.0f, "quad clipped width");
    require(quad_batch.clipped_bounds.height == 20.0f, "quad clipped height");
    require(quad_batch.scissor.x == 1, "quad scissor x maps to surface");
    require(quad_batch.scissor.y == 2, "quad scissor y maps to surface");
    require(quad_batch.scissor.width == 2, "quad scissor width maps to surface");
    require(quad_batch.scissor.height == 2, "quad scissor height maps to surface");
    require(quad_batch.vertices[0].x == 10.0f, "quad vertex 0 x uses clipped bounds");
    require(quad_batch.vertices[0].y == 20.0f, "quad vertex 0 y uses clipped bounds");
    require(quad_batch.vertices[2].x == 30.0f, "quad vertex 2 x uses clipped bounds");
    require(quad_batch.vertices[2].y == 40.0f, "quad vertex 2 y uses clipped bounds");

    const vulkan_backend::vulkan_draw_batch& text_batch = plan.batches[1];
    require(text_batch.kind == vulkan_backend::vulkan_batch_kind::text, "text command becomes a text backend batch");
    require(text_batch.command_index == 2, "text backend batch keeps source command index");
    require(text_batch.scissor.x == 2, "text scissor x maps to surface");
    require(text_batch.scissor.y == 3, "text scissor y maps to surface");
    require(text_batch.scissor.width == 1, "text scissor width maps to surface");
    require(text_batch.scissor.height == 1, "text scissor height maps to surface");
}

void test_vulkan_frame_plan_applies_nested_clips()
{
    using namespace quiz_vulkan::render;

    render_draw_command outer_clip{
        .type = render_draw_command_type::push_clip,
        .node_id = "outer_clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{0.0f, 0.0f, 80.0f, 80.0f},
        .content_bounds = render_rect{0.0f, 0.0f, 80.0f, 80.0f},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command inner_clip{
        .type = render_draw_command_type::push_clip,
        .node_id = "inner_clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{20.0f, 20.0f, 40.0f, 40.0f},
        .content_bounds = render_rect{20.0f, 20.0f, 40.0f, 40.0f},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command quad = make_quad_command(
        "nested_quad",
        render_rect{10.0f, 10.0f, 60.0f, 60.0f},
        render_color{0.4f, 0.6f, 0.8f, 1.0f});
    render_draw_command pop_inner{
        .type = render_draw_command_type::pop_clip,
        .node_id = "inner_clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = inner_clip.bounds,
        .content_bounds = inner_clip.content_bounds,
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command pop_outer{
        .type = render_draw_command_type::pop_clip,
        .node_id = "outer_clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = outer_clip.bounds,
        .content_bounds = outer_clip.content_bounds,
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };

    render_draw_list draw_list;
    draw_list.commands = {outer_clip, inner_clip, quad, pop_inner, pop_outer};

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    require(plan.batches.size() == 1, "nested clips produce one drawable batch");
    require(plan.clipped_draw_call_count == 1, "nested clips mark clipped draw call");
    require(plan.discarded_draw_call_count == 0, "nested clips do not discard visible batch");

    const vulkan_backend::vulkan_draw_batch& batch = plan.batches.front();
    require(batch.command_index == 2, "nested clipped batch keeps source command index");
    require(batch.clipped_bounds.x == 20.0f, "nested clipped batch x");
    require(batch.clipped_bounds.y == 20.0f, "nested clipped batch y");
    require(batch.clipped_bounds.width == 40.0f, "nested clipped batch width");
    require(batch.clipped_bounds.height == 40.0f, "nested clipped batch height");
    require(batch.scissor.x == 2, "nested clipped batch scissor x");
    require(batch.scissor.y == 2, "nested clipped batch scissor y");
    require(batch.scissor.width == 4, "nested clipped batch scissor width");
    require(batch.scissor.height == 4, "nested clipped batch scissor height");
}

void test_vulkan_frame_plan_ignores_unmatched_pop_clip()
{
    using namespace quiz_vulkan::render;

    render_draw_command unmatched_pop{
        .type = render_draw_command_type::pop_clip,
        .node_id = "unmatched_pop",
        .parent_node_id = {},
        .depth = 0,
        .bounds = {},
        .content_bounds = {},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command quad = make_quad_command(
        "quad",
        render_rect{10.0f, 20.0f, 30.0f, 40.0f},
        render_color{0.8f, 0.7f, 0.6f, 1.0f});

    render_draw_list draw_list;
    draw_list.commands = {unmatched_pop, quad};

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    require(plan.batches.size() == 1, "unmatched pop clip does not discard later batch");
    require(plan.clipped_draw_call_count == 0, "unmatched pop clip does not clip later batch");
    require(plan.discarded_draw_call_count == 0, "unmatched pop clip does not discard later batch");

    const vulkan_backend::vulkan_draw_batch& batch = plan.batches.front();
    require(batch.command_index == 1, "batch after unmatched pop keeps source command index");
    require(batch.clipped_bounds.x == 10.0f, "batch after unmatched pop x");
    require(batch.clipped_bounds.y == 20.0f, "batch after unmatched pop y");
    require(batch.clipped_bounds.width == 30.0f, "batch after unmatched pop width");
    require(batch.clipped_bounds.height == 40.0f, "batch after unmatched pop height");
    require(batch.scissor.x == 1, "batch after unmatched pop scissor x");
    require(batch.scissor.y == 2, "batch after unmatched pop scissor y");
    require(batch.scissor.width == 3, "batch after unmatched pop scissor width");
    require(batch.scissor.height == 4, "batch after unmatched pop scissor height");
}

void test_vulkan_diagnostic_pipeline_cache_reports_batch_capabilities()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{40.0f, 40.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache;
    require(cache.ensure_pipeline(plan.batches[0]), "diagnostic pipeline cache provides quad pipeline");
    require(cache.ensure_pipeline(plan.batches[1]), "diagnostic pipeline cache provides image pipeline");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.cache_checked, "pipeline cache records that pipelines were checked");
    require(state.ready, "pipeline cache remains ready when all requested pipelines exist");
    require(state.completed(), "pipeline cache reports completed state when all requested pipelines exist");
    require(!state.missing_pipeline, "pipeline cache reports no missing pipeline");
    require(state.requested_pipeline_count == 2, "pipeline cache records requested pipeline count");
    require(state.capabilities.size() == 4, "pipeline cache exposes one capability per batch kind");
    require(state.cache_entries.size() == 4, "pipeline cache exposes one cache entry per batch kind");
    require(state.pipeline_descriptors.size() == 4, "pipeline cache exposes one descriptor per batch kind");
    require(state.shader_registry.registered_shader_count == 8, "pipeline cache registers vertex and fragment shaders");
    require(state.shader_registry.registry_checked, "pipeline cache verifies shaders while checking pipelines");
    require(!state.shader_registry.missing_shader, "pipeline cache reports no missing shader on happy path");
    require(state.lifecycle.checked, "pipeline lifecycle is checked");
    require(state.lifecycle.render_pass.valid(), "pipeline lifecycle has a valid render pass descriptor");
    require(state.lifecycle.render_pass_ready(), "pipeline lifecycle render pass is ready");
    require(state.lifecycle.completed(), "pipeline lifecycle completes when requested pipelines are ready");
    require(state.compatibility.checked, "pipeline compatibility summary is checked");
    require(state.compatibility.completed(), "pipeline compatibility summary completes");
    require(state.compatibility.requested_key_count == 2, "pipeline compatibility records requested key count");
    require(state.compatibility.compatible_key_count == 2, "pipeline compatibility records compatible key count");
    require(state.compatibility.incompatible_key_count == 0, "pipeline compatibility records no incompatible keys");
    require(state.compatibility.unique_key_count == 2, "pipeline compatibility records unique key count");
    require(state.compatibility.keys.size() == 2, "pipeline compatibility stores one key per request");
    require(
        state.compatibility.keys[0].batch_kind == vulkan_backend::vulkan_batch_kind::quad,
        "pipeline compatibility first key records quad batch");
    require(
        state.compatibility.keys[0].color_attachment_count == 1,
        "pipeline compatibility first key records color attachment count");
    require(!state.compatibility.keys[0].has_depth_attachment, "pipeline compatibility first key records no depth");
    require(state.compatibility.keys[0].surface_compatible, "pipeline compatibility first key records surface compatibility");
    require(state.compatibility.keys[0].vertex_shader.value == "quad.vertex", "pipeline compatibility first key records vertex shader");
    require(
        state.compatibility.keys[0].fragment_shader.value == "quad.fragment",
        "pipeline compatibility first key records fragment shader");
    require(state.compatibility.keys[0].compatible(), "pipeline compatibility first key is compatible");
    require(
        state.compatibility.keys[1].batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "pipeline compatibility second key records image batch");
    require(state.compatibility.keys[1].vertex_shader.value == "image.vertex", "pipeline compatibility second key records vertex shader");
    require(
        state.compatibility.keys[1].fragment_shader.value == "image.fragment",
        "pipeline compatibility second key records fragment shader");
    require(state.compatibility.keys[1].compatible(), "pipeline compatibility second key is compatible");
    require(state.shader_bindings.checked, "shader binding readiness is checked");
    require(state.shader_bindings.completed(), "shader binding readiness completes");
    require(state.shader_bindings.requested_binding_count == 4, "shader binding readiness records requested bindings");
    require(state.shader_bindings.ready_binding_count == 4, "shader binding readiness records ready bindings");
    require(state.shader_bindings.missing_binding_count == 0, "shader binding readiness records no missing bindings");
    require(state.shader_bindings.bindings.size() == 4, "shader binding readiness stores stage bindings");
    require(
        state.shader_bindings.bindings[0].stage == vulkan_backend::vulkan_shader_stage::vertex,
        "shader binding readiness first binding is vertex");
    require(
        state.shader_bindings.bindings[0].shader_id.value == "quad.vertex",
        "shader binding readiness first binding records quad vertex shader");
    require(state.shader_bindings.bindings[0].completed(), "shader binding readiness first binding completes");
    require(
        state.shader_bindings.bindings[1].stage == vulkan_backend::vulkan_shader_stage::fragment,
        "shader binding readiness second binding is fragment");
    require(
        state.shader_bindings.bindings[1].shader_id.value == "quad.fragment",
        "shader binding readiness second binding records quad fragment shader");
    require(state.shader_bindings.bindings[1].completed(), "shader binding readiness second binding completes");
    require(
        state.shader_bindings.bindings[2].shader_id.value == "image.vertex",
        "shader binding readiness third binding records image vertex shader");
    require(
        state.shader_bindings.bindings[3].shader_id.value == "image.fragment",
        "shader binding readiness fourth binding records image fragment shader");
    require(!state.lifecycle.missing_render_pass, "pipeline lifecycle reports no missing render pass");
    require(!state.lifecycle.missing_shader_stage, "pipeline lifecycle reports no missing shader stage");
    require(!state.lifecycle.missing_pipeline, "pipeline lifecycle reports no missing pipeline");
    require(state.lifecycle.requested_pipeline_count == 2, "pipeline lifecycle records requested pipeline count");
    require(state.lifecycle.pipeline_snapshots.size() == 2, "pipeline lifecycle stores one snapshot per request");
    require(state.lifecycle.shader_stage_snapshots.size() == 2, "pipeline lifecycle stores one shader snapshot per request");
    require(
        state.descriptor_for(vulkan_backend::vulkan_batch_kind::quad) != nullptr,
        "pipeline cache exposes quad descriptor");
    require(
        state.descriptor_for(vulkan_backend::vulkan_batch_kind::image) != nullptr,
        "pipeline cache exposes image descriptor");
    require(state.supports(vulkan_backend::vulkan_batch_kind::quad), "pipeline cache supports quad pipeline");
    require(state.supports(vulkan_backend::vulkan_batch_kind::text), "pipeline cache supports text pipeline");
    require(state.supports(vulkan_backend::vulkan_batch_kind::image), "pipeline cache supports image pipeline");
    require(state.supports(vulkan_backend::vulkan_batch_kind::debug_bounds), "pipeline cache supports debug pipeline");

    require(state.cache_entries[0].kind == vulkan_backend::vulkan_batch_kind::quad, "first pipeline cache entry is quad");
    require(state.cache_entries[0].requested, "quad pipeline cache entry is requested");
    require(state.cache_entries[0].request_count == 1, "quad pipeline cache entry records request count");
    require(state.cache_entries[0].last_command_index == 0, "quad pipeline cache entry records source command index");
    require(state.cache_entries[1].kind == vulkan_backend::vulkan_batch_kind::text, "second pipeline cache entry is text");
    require(!state.cache_entries[1].requested, "text pipeline cache entry is not requested");
    require(state.cache_entries[2].kind == vulkan_backend::vulkan_batch_kind::image, "third pipeline cache entry is image");
    require(state.cache_entries[2].requested, "image pipeline cache entry is requested");
    require(state.cache_entries[2].request_count == 1, "image pipeline cache entry records request count");
    require(state.cache_entries[2].last_command_index == 1, "image pipeline cache entry records source command index");
    require(
        state.cache_entries[3].kind == vulkan_backend::vulkan_batch_kind::debug_bounds,
        "fourth pipeline cache entry is debug bounds");
    require(!state.cache_entries[3].requested, "debug pipeline cache entry is not requested");

    const vulkan_backend::vulkan_pipeline_lifecycle_snapshot& quad_lifecycle =
        state.lifecycle.pipeline_snapshots[0];
    require(quad_lifecycle.batch_kind == vulkan_backend::vulkan_batch_kind::quad, "first lifecycle request is quad");
    require(quad_lifecycle.command_index == 0, "quad lifecycle snapshot records command index");
    require(quad_lifecycle.completed(), "quad lifecycle snapshot completes");
    require(
        quad_lifecycle.render_pass_status == vulkan_backend::vulkan_pipeline_lifecycle_status::ready,
        "quad lifecycle render pass is ready");
    require(
        quad_lifecycle.shader_stage_status == vulkan_backend::vulkan_pipeline_lifecycle_status::ready,
        "quad lifecycle shader stages are ready");
    require(
        quad_lifecycle.pipeline_status == vulkan_backend::vulkan_pipeline_lifecycle_status::ready,
        "quad lifecycle pipeline is ready");

    const vulkan_backend::vulkan_pipeline_shader_stage_snapshot& quad_shaders =
        state.lifecycle.shader_stage_snapshots[0];
    require(quad_shaders.completed(), "quad shader lifecycle snapshot completes");
    require(quad_shaders.vertex_shader.value == "quad.vertex", "quad shader lifecycle records vertex shader id");
    require(quad_shaders.fragment_shader.value == "quad.fragment", "quad shader lifecycle records fragment shader id");
    require(quad_shaders.vertex_stage_ready, "quad vertex stage is ready");
    require(quad_shaders.fragment_stage_ready, "quad fragment stage is ready");

    const vulkan_backend::vulkan_pipeline_lifecycle_snapshot& image_lifecycle =
        state.lifecycle.pipeline_snapshots[1];
    require(image_lifecycle.batch_kind == vulkan_backend::vulkan_batch_kind::image, "second lifecycle request is image");
    require(image_lifecycle.command_index == 1, "image lifecycle snapshot records command index");
    require(image_lifecycle.completed(), "image lifecycle snapshot completes");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_pipeline()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{40.0f, 40.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .default_available = true,
            .overrides = {vulkan_backend::vulkan_pipeline_capability{
                .kind = vulkan_backend::vulkan_batch_kind::image,
                .available = false,
            }},
        });

    require(cache.ensure_pipeline(plan.batches[0]), "pipeline cache provides available quad pipeline");
    require(!cache.ensure_pipeline(plan.batches[1]), "pipeline cache reports missing image pipeline");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.cache_checked, "missing pipeline cache records that pipelines were checked");
    require(!state.ready, "missing pipeline cache is not ready");
    require(!state.completed(), "missing pipeline cache does not complete");
    require(state.missing_pipeline, "missing pipeline cache records missing pipeline");
    require(
        state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "missing pipeline cache records missing image pipeline");
    require(state.missing_command_index == 1, "missing pipeline cache records source command index");
    require(state.requested_pipeline_count == 2, "missing pipeline cache records request count through failure");
    require(state.supports(vulkan_backend::vulkan_batch_kind::quad), "missing pipeline cache still supports quad");
    require(!state.supports(vulkan_backend::vulkan_batch_kind::image), "missing pipeline cache reports unsupported image");
    require(state.cache_entries[0].requested, "missing pipeline cache records quad request");
    require(state.cache_entries[2].requested, "missing pipeline cache records image request");
    require(!state.cache_entries[2].available, "missing pipeline cache entry records unavailable image pipeline");
    require(state.cache_entries[2].last_command_index == 1, "missing pipeline cache entry records failed command index");
    require(state.lifecycle.checked, "missing pipeline lifecycle is checked");
    require(!state.lifecycle.completed(), "missing pipeline lifecycle does not complete");
    require(!state.lifecycle.missing_render_pass, "missing pipeline lifecycle keeps render pass ready");
    require(!state.lifecycle.missing_shader_stage, "missing pipeline lifecycle does not check shaders");
    require(state.lifecycle.missing_pipeline, "missing pipeline lifecycle records pipeline failure");
    require(
        state.lifecycle.failed_stage == vulkan_backend::vulkan_pipeline_lifecycle_stage::pipeline,
        "missing pipeline lifecycle identifies pipeline stage");
    require(
        state.lifecycle.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "missing pipeline lifecycle records image batch");
    require(state.lifecycle.missing_command_index == 1, "missing pipeline lifecycle records command index");
    require(state.lifecycle.pipeline_snapshots.size() == 2, "missing pipeline lifecycle stores snapshots through failure");
    require(state.lifecycle.pipeline_snapshots[0].completed(), "missing pipeline lifecycle preserves completed quad snapshot");
    require(
        state.lifecycle.pipeline_snapshots[1].pipeline_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable,
        "missing pipeline lifecycle marks image pipeline unavailable");
    require(
        state.lifecycle.pipeline_snapshots[1].failed_stage
            == vulkan_backend::vulkan_pipeline_lifecycle_stage::pipeline,
        "missing pipeline lifecycle snapshot records failed stage");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_unavailable_render_pass()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .render_pass = vulkan_backend::vulkan_render_pass_descriptor{
                .color_attachment_count = 0,
                .has_depth_attachment = false,
                .surface_compatible = true,
            },
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports unavailable render pass");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.cache_checked, "render pass failure checks pipeline cache");
    require(!state.ready, "render pass failure marks pipeline cache not ready");
    require(!state.completed(), "render pass failure does not complete pipeline cache");
    require(state.missing_pipeline, "render pass failure marks pipeline unavailable");
    require(state.lifecycle.checked, "render pass lifecycle is checked");
    require(state.compatibility.checked, "render pass failure checks pipeline compatibility");
    require(!state.compatibility.completed(), "render pass failure compatibility does not complete");
    require(state.compatibility.requested_key_count == 1, "render pass failure records one compatibility key");
    require(state.compatibility.compatible_key_count == 0, "render pass failure records no compatible keys");
    require(state.compatibility.incompatible_key_count == 1, "render pass failure records incompatible key");
    require(state.compatibility.unique_key_count == 1, "render pass failure records one unique key");
    require(state.compatibility.keys.size() == 1, "render pass failure stores compatibility key");
    require(!state.compatibility.keys.front().compatible(), "render pass failure key is incompatible");
    require(
        state.compatibility.keys.front().color_attachment_count == 0,
        "render pass failure compatibility key records missing color attachment");
    require(state.shader_bindings.checked, "render pass failure initializes shader binding readiness");
    require(state.shader_bindings.requested_binding_count == 0, "render pass failure performs no shader binding checks");
    require(state.shader_bindings.bindings.empty(), "render pass failure stores no shader bindings");
    require(!state.lifecycle.render_pass.valid(), "render pass lifecycle records invalid descriptor");
    require(!state.lifecycle.render_pass_ready(), "render pass lifecycle is not ready");
    require(state.lifecycle.missing_render_pass, "render pass lifecycle records missing render pass");
    require(!state.lifecycle.missing_shader_stage, "render pass lifecycle does not check shaders");
    require(!state.lifecycle.missing_pipeline, "render pass lifecycle failure is not a pipeline-object miss");
    require(
        state.lifecycle.failed_stage == vulkan_backend::vulkan_pipeline_lifecycle_stage::render_pass,
        "render pass lifecycle identifies render pass stage");
    require(state.lifecycle.missing_command_index == 0, "render pass lifecycle records command index");
    require(state.lifecycle.requested_pipeline_count == 1, "render pass lifecycle records one request");
    require(state.lifecycle.pipeline_snapshots.size() == 1, "render pass lifecycle stores failed request snapshot");
    require(state.lifecycle.shader_stage_snapshots.empty(), "render pass lifecycle stores no shader snapshot");
    require(
        state.lifecycle.pipeline_snapshots.front().render_pass_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable,
        "render pass lifecycle marks render pass unavailable");
    require(
        state.lifecycle.pipeline_snapshots.front().shader_stage_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked,
        "render pass lifecycle does not check shader stages");
    require(
        state.lifecycle.pipeline_snapshots.front().pipeline_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked,
        "render pass lifecycle does not check graphics pipeline");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_vertex_shader()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .use_default_shader_modules = false,
            .shader_modules = {vulkan_backend::vulkan_shader_module_descriptor{
                .id = vulkan_backend::vulkan_shader_module_id{.value = "quad.fragment"},
                .stage = vulkan_backend::vulkan_shader_stage::fragment,
            }},
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports missing vertex shader");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.missing_pipeline, "missing vertex shader marks pipeline missing");
    require(!state.missing_descriptor, "missing vertex shader keeps descriptor available");
    require(state.missing_shader, "missing vertex shader marks shader missing");
    require(
        state.missing_shader_stage == vulkan_backend::vulkan_shader_stage::vertex,
        "missing vertex shader records vertex stage");
    require(state.missing_shader_id.value == "quad.vertex", "missing vertex shader records shader module id");
    require(state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::quad, "missing vertex shader records batch kind");
    require(state.missing_command_index == 0, "missing vertex shader records command index");
    require(state.shader_registry.registry_checked, "missing vertex shader checks shader registry");
    require(state.shader_registry.missing_shader, "shader registry records missing vertex shader");
    require(
        state.shader_registry.missing_stage == vulkan_backend::vulkan_shader_stage::vertex,
        "shader registry records missing vertex stage");
    require(state.shader_registry.missing_shader_id.value == "quad.vertex", "shader registry records vertex id");
    require(state.compatibility.checked, "missing vertex shader checks pipeline compatibility");
    require(state.compatibility.completed(), "missing vertex shader keeps compatibility key complete");
    require(state.compatibility.requested_key_count == 1, "missing vertex shader records one compatibility key");
    require(state.compatibility.compatible_key_count == 1, "missing vertex shader compatibility key is structurally compatible");
    require(state.compatibility.keys.front().vertex_shader.value == "quad.vertex", "missing vertex shader compatibility records vertex id");
    require(state.shader_bindings.checked, "missing vertex shader checks shader binding readiness");
    require(!state.shader_bindings.completed(), "missing vertex shader binding readiness does not complete");
    require(state.shader_bindings.requested_binding_count == 1, "missing vertex shader checks one binding");
    require(state.shader_bindings.ready_binding_count == 0, "missing vertex shader has no ready bindings");
    require(state.shader_bindings.missing_binding_count == 1, "missing vertex shader records one missing binding");
    require(state.shader_bindings.bindings.size() == 1, "missing vertex shader stores one binding");
    require(
        state.shader_bindings.bindings.front().stage == vulkan_backend::vulkan_shader_stage::vertex,
        "missing vertex shader binding records vertex stage");
    require(
        state.shader_bindings.bindings.front().shader_id.value == "quad.vertex",
        "missing vertex shader binding records shader id");
    require(state.shader_bindings.bindings.front().registry_checked, "missing vertex shader binding checks registry");
    require(!state.shader_bindings.bindings.front().module_registered, "missing vertex shader binding records missing module");
    require(!state.shader_bindings.bindings.front().completed(), "missing vertex shader binding does not complete");
    require(!state.lifecycle.completed(), "missing vertex shader lifecycle does not complete");
    require(!state.lifecycle.missing_render_pass, "missing vertex shader lifecycle keeps render pass ready");
    require(state.lifecycle.missing_shader_stage, "missing vertex shader lifecycle records shader stage failure");
    require(!state.lifecycle.missing_pipeline, "missing vertex shader lifecycle is not a pipeline-object miss");
    require(
        state.lifecycle.failed_stage == vulkan_backend::vulkan_pipeline_lifecycle_stage::shader_stages,
        "missing vertex shader lifecycle identifies shader stage");
    require(
        state.lifecycle.missing_shader_stage_kind == vulkan_backend::vulkan_shader_stage::vertex,
        "missing vertex shader lifecycle records vertex stage");
    require(state.lifecycle.missing_shader_id.value == "quad.vertex", "missing vertex shader lifecycle records shader id");
    require(state.lifecycle.pipeline_snapshots.size() == 1, "missing vertex shader lifecycle stores request snapshot");
    require(state.lifecycle.shader_stage_snapshots.size() == 1, "missing vertex shader lifecycle stores shader snapshot");
    require(
        state.lifecycle.pipeline_snapshots.front().shader_stage_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable,
        "missing vertex shader lifecycle marks shader stages unavailable");
    require(
        state.lifecycle.pipeline_snapshots.front().pipeline_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked,
        "missing vertex shader lifecycle does not check graphics pipeline");
    require(
        !state.lifecycle.shader_stage_snapshots.front().vertex_stage_ready,
        "missing vertex shader lifecycle records unavailable vertex stage");
    require(
        !state.lifecycle.shader_stage_snapshots.front().fragment_stage_ready,
        "missing vertex shader lifecycle stops before fragment stage");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_fragment_shader()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .use_default_shader_modules = false,
            .shader_modules = {vulkan_backend::vulkan_shader_module_descriptor{
                .id = vulkan_backend::vulkan_shader_module_id{.value = "image.vertex"},
                .stage = vulkan_backend::vulkan_shader_stage::vertex,
            }},
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports missing fragment shader");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.missing_pipeline, "missing fragment shader marks pipeline missing");
    require(!state.missing_descriptor, "missing fragment shader keeps descriptor available");
    require(state.missing_shader, "missing fragment shader marks shader missing");
    require(
        state.missing_shader_stage == vulkan_backend::vulkan_shader_stage::fragment,
        "missing fragment shader records fragment stage");
    require(state.missing_shader_id.value == "image.fragment", "missing fragment shader records shader module id");
    require(state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image, "missing fragment shader records batch kind");
    require(state.missing_command_index == 0, "missing fragment shader records command index");
    require(state.shader_registry.registry_checked, "missing fragment shader checks shader registry");
    require(state.shader_registry.missing_shader, "shader registry records missing fragment shader");
    require(
        state.shader_registry.missing_stage == vulkan_backend::vulkan_shader_stage::fragment,
        "shader registry records missing fragment stage");
    require(state.shader_registry.missing_shader_id.value == "image.fragment", "shader registry records fragment id");
    require(state.compatibility.checked, "missing fragment shader checks pipeline compatibility");
    require(state.compatibility.completed(), "missing fragment shader keeps compatibility key complete");
    require(state.compatibility.requested_key_count == 1, "missing fragment shader records one compatibility key");
    require(state.compatibility.compatible_key_count == 1, "missing fragment shader compatibility key is structurally compatible");
    require(
        state.compatibility.keys.front().fragment_shader.value == "image.fragment",
        "missing fragment shader compatibility records fragment id");
    require(state.shader_bindings.checked, "missing fragment shader checks shader binding readiness");
    require(!state.shader_bindings.completed(), "missing fragment shader binding readiness does not complete");
    require(state.shader_bindings.requested_binding_count == 2, "missing fragment shader checks two bindings");
    require(state.shader_bindings.ready_binding_count == 1, "missing fragment shader records ready vertex binding");
    require(state.shader_bindings.missing_binding_count == 1, "missing fragment shader records one missing binding");
    require(state.shader_bindings.bindings.size() == 2, "missing fragment shader stores two bindings");
    require(state.shader_bindings.bindings.front().completed(), "missing fragment shader records completed vertex binding");
    require(
        state.shader_bindings.bindings.back().stage == vulkan_backend::vulkan_shader_stage::fragment,
        "missing fragment shader binding records fragment stage");
    require(
        state.shader_bindings.bindings.back().shader_id.value == "image.fragment",
        "missing fragment shader binding records shader id");
    require(state.shader_bindings.bindings.back().registry_checked, "missing fragment shader binding checks registry");
    require(!state.shader_bindings.bindings.back().module_registered, "missing fragment shader binding records missing module");
    require(!state.shader_bindings.bindings.back().completed(), "missing fragment shader binding does not complete");
    require(!state.lifecycle.completed(), "missing fragment shader lifecycle does not complete");
    require(!state.lifecycle.missing_render_pass, "missing fragment shader lifecycle keeps render pass ready");
    require(state.lifecycle.missing_shader_stage, "missing fragment shader lifecycle records shader stage failure");
    require(!state.lifecycle.missing_pipeline, "missing fragment shader lifecycle is not a pipeline-object miss");
    require(
        state.lifecycle.failed_stage == vulkan_backend::vulkan_pipeline_lifecycle_stage::shader_stages,
        "missing fragment shader lifecycle identifies shader stage");
    require(
        state.lifecycle.missing_shader_stage_kind == vulkan_backend::vulkan_shader_stage::fragment,
        "missing fragment shader lifecycle records fragment stage");
    require(
        state.lifecycle.missing_shader_id.value == "image.fragment",
        "missing fragment shader lifecycle records shader id");
    require(state.lifecycle.pipeline_snapshots.size() == 1, "missing fragment shader lifecycle stores request snapshot");
    require(state.lifecycle.shader_stage_snapshots.size() == 1, "missing fragment shader lifecycle stores shader snapshot");
    require(
        state.lifecycle.pipeline_snapshots.front().shader_stage_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable,
        "missing fragment shader lifecycle marks shader stages unavailable");
    require(
        state.lifecycle.pipeline_snapshots.front().pipeline_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked,
        "missing fragment shader lifecycle does not check graphics pipeline");
    require(
        state.lifecycle.shader_stage_snapshots.front().vertex_stage_ready,
        "missing fragment shader lifecycle records ready vertex stage");
    require(
        !state.lifecycle.shader_stage_snapshots.front().fragment_stage_ready,
        "missing fragment shader lifecycle records unavailable fragment stage");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_descriptor()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_text_command(
        "text",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        "missing descriptor"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .use_default_pipeline_descriptors = false,
            .pipeline_descriptors = {vulkan_backend::vulkan_pipeline_descriptor{
                .kind = vulkan_backend::vulkan_batch_kind::quad,
                .vertex_shader = vulkan_backend::vulkan_shader_module_id{.value = "quad.vertex"},
                .fragment_shader = vulkan_backend::vulkan_shader_module_id{.value = "quad.fragment"},
            }},
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports missing text descriptor");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.missing_pipeline, "missing descriptor marks pipeline missing");
    require(state.missing_descriptor, "missing descriptor flag is set");
    require(!state.missing_shader, "missing descriptor does not report missing shader");
    require(
        state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::text,
        "missing descriptor records text batch kind");
    require(state.missing_command_index == 0, "missing descriptor records command index");
    require(state.pipeline_descriptors.size() == 1, "pipeline cache uses injected descriptor set");
    require(
        state.descriptor_for(vulkan_backend::vulkan_batch_kind::text) == nullptr,
        "pipeline cache exposes no text descriptor");
    require(!state.shader_registry.registry_checked, "missing descriptor stops before shader registry lookup");
    require(!state.lifecycle.completed(), "missing descriptor lifecycle does not complete");
    require(!state.lifecycle.missing_render_pass, "missing descriptor lifecycle keeps render pass ready");
    require(!state.lifecycle.missing_shader_stage, "missing descriptor lifecycle does not check shaders");
    require(state.lifecycle.missing_pipeline, "missing descriptor lifecycle records pipeline-stage failure");
    require(
        state.lifecycle.failed_stage == vulkan_backend::vulkan_pipeline_lifecycle_stage::pipeline,
        "missing descriptor lifecycle identifies pipeline stage");
    require(state.lifecycle.pipeline_snapshots.size() == 1, "missing descriptor lifecycle stores request snapshot");
    require(state.lifecycle.shader_stage_snapshots.empty(), "missing descriptor lifecycle stores no shader snapshot");
    require(
        state.lifecycle.pipeline_snapshots.front().render_pass_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::ready,
        "missing descriptor lifecycle records ready render pass");
    require(
        state.lifecycle.pipeline_snapshots.front().shader_stage_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked,
        "missing descriptor lifecycle does not check shader stages");
    require(
        state.lifecycle.pipeline_snapshots.front().pipeline_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable,
        "missing descriptor lifecycle marks pipeline unavailable");
}

void require_completed_frame_sync(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_sync_state& sync)
{
    using namespace quiz_vulkan::render;

    require(sync.frame.valid(), "frame sync records a valid frame-in-flight id");
    require(sync.frame.index == 0, "frame sync records deterministic frame index");
    require(sync.frame.frame_count == 2, "frame sync records deterministic frames-in-flight count");
    require(sync.frame.sequence == 1, "frame sync records deterministic frame sequence");

    require(
        sync.acquire_signal_image_available_semaphore.token.kind
            == vulkan_backend::vulkan_frame_sync_token_kind::semaphore,
        "frame sync acquire image-available token is a semaphore");
    require(
        sync.acquire_signal_fence.token.kind == vulkan_backend::vulkan_frame_sync_token_kind::fence,
        "frame sync acquire token is a fence");
    require(
        sync.submit_signal_render_finished_semaphore.token.kind
            == vulkan_backend::vulkan_frame_sync_token_kind::semaphore,
        "frame sync submit render-finished token is a semaphore");
    require(
        sync.submit_signal_frame_fence.token.kind == vulkan_backend::vulkan_frame_sync_token_kind::fence,
        "frame sync submit frame token is a fence");

    require(sync.acquire_signal_image_available_semaphore.completed(), "frame sync acquire semaphore is signaled");
    require(sync.acquire_signal_fence.completed(), "frame sync acquire fence is signaled");
    require(sync.submit_wait_image_available_semaphore.completed(), "frame sync submit waits on image-available");
    require(sync.submit_signal_render_finished_semaphore.completed(), "frame sync submit signals render-finished");
    require(sync.submit_signal_frame_fence.completed(), "frame sync submit signals frame fence");
    require(sync.present_wait_render_finished_semaphore.completed(), "frame sync present waits on render-finished");
    require(sync.acquire_completed(), "frame sync records completed acquire synchronization");
    require(sync.submit_completed(), "frame sync records completed submit synchronization");
    require(sync.present_completed(), "frame sync records completed present synchronization");
    require(sync.completed(), "frame sync records completed frame synchronization");

    require(
        sync.submit_wait_image_available_semaphore.token.value
            == sync.acquire_signal_image_available_semaphore.token.value,
        "frame sync submit waits on the acquired image-available semaphore");
    require(
        sync.present_wait_render_finished_semaphore.token.value
            == sync.submit_signal_render_finished_semaphore.token.value,
        "frame sync present waits on the submit render-finished semaphore");
}

void require_completed_command_buffer_submit_state(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_command_buffer_submit_state& state,
    std::size_t planned_batch_count)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "command buffer submit diagnostics are checked");
    require(state.completed(), "command buffer submit diagnostics complete");
    require(state.recording.command_buffer.valid(), "command buffer recording id is valid");
    require(state.recording.command_buffer.value == 1001, "command buffer id is deterministic");
    require(
        state.submit.command_buffer.value == state.recording.command_buffer.value,
        "submit diagnostics reference recorded command buffer");
    require(state.submit.frame.valid(), "submit diagnostics reference valid frame-in-flight");
    require(state.submit.frame.sequence == 1, "submit diagnostics frame sequence is deterministic");
    require(state.recording.begin_requested, "command buffer recording begins");
    require(state.recording.finish_requested, "command buffer recording finishes");
    require(
        state.recording.status == vulkan_backend::vulkan_command_buffer_recording_status::recorded,
        "command buffer recording status is recorded");
    require(
        state.recording.failure_stage == vulkan_backend::vulkan_command_recorder_failure_stage::none,
        "command buffer recording has no failure stage");
    require(state.recording.planned_batch_count == planned_batch_count, "command buffer planned count is stable");
    require(state.recording.recorded_batch_count == planned_batch_count, "command buffer recorded count is stable");
    require(state.recording.completed(), "command buffer recording diagnostics complete");
    require(!state.recording.failed(), "command buffer recording diagnostics do not fail");
    require(state.submit.submit_requested, "frame submit is requested");
    require(state.submit.submitted_batch_count == planned_batch_count, "frame submit batch count is stable");
    require(
        state.submit.status == vulkan_backend::vulkan_frame_submit_status::submitted,
        "frame submit status is submitted");
    require(
        state.submit.wait_image_available_status == vulkan_backend::vulkan_frame_sync_wait_status::waited,
        "frame submit waits for image available");
    require(
        state.submit.signal_render_finished_status == vulkan_backend::vulkan_frame_sync_signal_status::signaled,
        "frame submit signals render finished");
    require(
        state.submit.signal_frame_fence_status == vulkan_backend::vulkan_frame_sync_signal_status::signaled,
        "frame submit signals frame fence");
    require(state.submit.completed(), "frame submit diagnostics complete");
    require(!state.submit.failed(), "frame submit diagnostics do not fail");
}

void require_allowed_command_recorder_gate(
    const quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_gate_state& gate,
    std::size_t planned_batch_count)
{
    using namespace quiz_vulkan::render;

    require(gate.checked, "command recorder gate is checked");
    require(gate.completed(), "command recorder gate allows recording");
    require(!gate.blocked(), "command recorder gate is not blocked");
    require(gate.recording_allowed, "command recorder gate records allowed state");
    require(
        gate.status == vulkan_backend::vulkan_command_recorder_gate_status::allowed,
        "command recorder gate status is allowed");
    require(
        gate.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "allowed command recorder gate has no fallback reason");
    require(gate.resource_bindings_checked, "allowed gate checks resource bindings");
    require(gate.resource_bindings_completed, "allowed gate completes resource bindings");
    require(gate.descriptor_validation_checked, "allowed gate checks descriptor validation");
    require(gate.descriptor_validation_completed, "allowed gate completes descriptor validation");
    require(gate.resource_registry_checked, "allowed gate checks resource registry");
    require(gate.resource_registry_completed, "allowed gate completes resource registry");
    require(!gate.missing_required_resource, "allowed gate has no missing required resource");
    require(!gate.duplicate_binding, "allowed gate has no duplicate binding");
    require(!gate.invalid_layout, "allowed gate has no invalid descriptor layout");
    require(
        gate.descriptor_status == vulkan_backend::vulkan_descriptor_validation_status::valid,
        "allowed gate descriptor status is valid");
    require(gate.planned_batch_count == planned_batch_count, "allowed gate tracks planned batch count");
    require(gate.invalid_descriptor_set_count == 0, "allowed gate has no invalid descriptor sets");
    require(gate.missing_resource_count == 0, "allowed gate has no missing registry resources");
}

void require_failed_frame_submit_state(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_command_buffer_submit_state& state,
    std::size_t planned_batch_count)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "failed frame submit diagnostics are checked");
    require(!state.completed(), "failed frame submit diagnostics do not complete");
    require(state.recording.completed(), "failed frame submit has completed command buffer recording");
    require(state.recording.planned_batch_count == planned_batch_count, "failed frame submit planned count is stable");
    require(state.recording.recorded_batch_count == planned_batch_count, "failed frame submit recorded count is stable");
    require(state.submit.submit_requested, "failed frame submit was requested");
    require(state.submit.submitted_batch_count == planned_batch_count, "failed frame submit batch count is stable");
    require(
        state.submit.status == vulkan_backend::vulkan_frame_submit_status::failed,
        "failed frame submit status is failed");
    require(state.submit.failed(), "failed frame submit helper reports failure");
    require(
        state.submit.wait_image_available_status == vulkan_backend::vulkan_frame_sync_wait_status::failed,
        "failed frame submit records failed image wait");
    require(
        state.submit.signal_render_finished_status == vulkan_backend::vulkan_frame_sync_signal_status::failed,
        "failed frame submit records failed render-finished signal");
    require(
        state.submit.signal_frame_fence_status == vulkan_backend::vulkan_frame_sync_signal_status::failed,
        "failed frame submit records failed frame fence signal");
}

void require_completed_frame_present_policy(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_present_policy_state& state,
    std::size_t image_id)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "frame present policy is checked");
    require(state.completed(), "frame present policy completes");
    require(!state.failed(), "frame present policy does not fail");
    require(state.acquire_request_count == 1, "frame present policy records one acquire request");
    require(state.present_image_request_count == 1, "frame present policy records one image present request");
    require(!state.backpressure_detected, "frame present policy records no backpressure");

    require(state.acquire.checked, "frame acquire policy is checked");
    require(state.acquire.completed(), "frame acquire policy completes");
    require(!state.acquire.failed(), "frame acquire policy does not fail");
    require(state.acquire.requested, "frame acquire policy records request");
    require(
        state.acquire.status == vulkan_backend::vulkan_frame_acquire_policy_status::acquired,
        "frame acquire policy status is acquired");
    require(
        state.acquire.swapchain_status == vulkan_backend::vulkan_swapchain_acquire_status::acquired,
        "frame acquire policy records swapchain acquired status");
    require(state.acquire.image_id.value == image_id, "frame acquire policy records image id");
    require(state.acquire.image_available, "frame acquire policy records available image");
    require(state.acquire.image_acquired, "frame acquire policy records acquired image");
    require(!state.acquire.backpressured, "frame acquire policy does not report backpressure");
    require(
        state.acquire.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "frame acquire policy has no fallback reason");

    require(state.present.checked, "frame present result summary is checked");
    require(state.present.completed(), "frame present result summary completes");
    require(!state.present.failed(), "frame present result summary does not fail");
    require(state.present.image_present_requested, "frame present result summary records image present request");
    require(state.present.frame_present_requested, "frame present result summary records frame present request");
    require(state.present.image_id.value == image_id, "frame present result summary records image id");
    require(
        state.present.swapchain_status == vulkan_backend::vulkan_swapchain_present_status::presented,
        "frame present result summary records swapchain presented status");
    require(state.present.image_presented, "frame present result summary records image presented");
    require(state.present.frame_presented, "frame present result summary records frame presented");
    require(
        state.present.status == vulkan_backend::vulkan_frame_present_result_status::frame_presented,
        "frame present result summary status is frame presented");
    require(
        state.present.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "frame present result summary has no fallback reason");
}

void require_failed_frame_acquire_policy(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_present_policy_state& state,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_acquire_policy_status acquire_status,
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status swapchain_status,
    bool backpressured,
    std::size_t image_id)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "failed frame acquire policy is checked");
    require(!state.completed(), "failed frame acquire policy does not complete");
    require(state.failed(), "failed frame acquire policy reports failure");
    require(state.acquire_request_count == 1, "failed frame acquire policy records one acquire request");
    require(state.present_image_request_count == 0, "failed frame acquire policy records no present request");
    require(state.backpressure_detected == backpressured, "failed frame acquire policy records backpressure flag");
    require(state.acquire.checked, "failed frame acquire policy checks acquire");
    require(state.acquire.requested, "failed frame acquire policy records request");
    require(state.acquire.failed(), "failed frame acquire policy helper reports failure");
    require(state.acquire.status == acquire_status, "failed frame acquire policy records acquire status");
    require(state.acquire.swapchain_status == swapchain_status, "failed frame acquire policy records swapchain status");
    require(state.acquire.image_id.value == image_id, "failed frame acquire policy records image id");
    require(!state.acquire.image_available, "failed frame acquire policy records unavailable image");
    require(!state.acquire.image_acquired, "failed frame acquire policy records unacquired image");
    require(state.acquire.backpressured == backpressured, "failed frame acquire policy records acquire backpressure");
    require(
        state.acquire.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
        "failed frame acquire policy records acquire fallback reason");
    require(state.present.checked, "failed frame acquire policy checks present summary");
    require(!state.present.image_present_requested, "failed frame acquire policy does not request image present");
    require(!state.present.frame_present_requested, "failed frame acquire policy does not request frame present");
    require(
        state.present.status == vulkan_backend::vulkan_frame_present_result_status::not_requested,
        "failed frame acquire policy leaves present status not requested");
}

void require_failed_frame_present_policy(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_present_policy_state& state,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_present_result_status present_status,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason fallback_reason,
    bool frame_present_requested,
    bool image_presented,
    bool frame_presented,
    std::size_t image_id)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "failed frame present policy is checked");
    require(!state.completed(), "failed frame present policy does not complete");
    require(state.failed(), "failed frame present policy reports failure");
    require(state.acquire_request_count == 1, "failed frame present policy records one acquire request");
    require(state.present_image_request_count == 1, "failed frame present policy records one image present request");
    require(!state.backpressure_detected, "failed frame present policy records no backpressure");
    require(state.acquire.completed(), "failed frame present policy records completed acquire");
    require(state.present.checked, "failed frame present policy checks present result");
    require(state.present.failed(), "failed frame present result helper reports failure");
    require(state.present.image_present_requested, "failed frame present policy records image present request");
    require(
        state.present.frame_present_requested == frame_present_requested,
        "failed frame present policy records frame present request flag");
    require(state.present.image_id.value == image_id, "failed frame present policy records image id");
    require(state.present.image_presented == image_presented, "failed frame present policy records image result");
    require(state.present.frame_presented == frame_presented, "failed frame present policy records frame result");
    require(state.present.status == present_status, "failed frame present policy records result status");
    require(state.present.fallback_reason == fallback_reason, "failed frame present policy records fallback reason");
}

void require_completed_swapchain_policy(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_swapchain_policy_state& state,
    std::size_t width,
    std::size_t height)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "swapchain policy is checked");
    require(state.completed(), "swapchain policy completes");
    require(state.extent.checked, "swapchain extent policy is checked");
    require(state.extent.completed(), "swapchain extent policy completes");
    require(state.extent.requested_extent.width == width, "swapchain extent policy records requested width");
    require(state.extent.requested_extent.height == height, "swapchain extent policy records requested height");
    require(state.extent.selected_extent.width == width, "swapchain extent policy records selected width");
    require(state.extent.selected_extent.height == height, "swapchain extent policy records selected height");
    require(state.extent.min_extent.width == 1, "swapchain extent policy records minimum width");
    require(state.extent.min_extent.height == 1, "swapchain extent policy records minimum height");
    require(state.extent.max_extent.width == 4096, "swapchain extent policy records maximum width");
    require(state.extent.max_extent.height == 4096, "swapchain extent policy records maximum height");
    require(state.extent.extent_supported, "swapchain extent policy marks extent supported");
    require(!state.extent.extent_clamped, "swapchain extent policy does not clamp supported fixture extent");
    require(state.present_mode.checked, "swapchain present mode policy is checked");
    require(state.present_mode.completed(), "swapchain present mode policy completes");
    require(
        state.present_mode.requested_mode == vulkan_backend::vulkan_swapchain_present_mode::fifo,
        "swapchain present mode policy requests fifo");
    require(
        state.present_mode.selected_mode == vulkan_backend::vulkan_swapchain_present_mode::fifo,
        "swapchain present mode policy selects fifo");
    require(state.present_mode.requested_mode_supported, "swapchain present mode policy supports fifo");
    require(!state.present_mode.fallback_to_fifo, "swapchain present mode policy does not fallback from fifo");
}

void require_frame_fallback_summary(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_fallback_summary& summary,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason reason,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_failure_classification classification,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_stage reached_stage)
{
    using namespace quiz_vulkan::render;

    require(summary.checked, "frame fallback summary is checked");
    require(summary.required, "frame fallback summary marks fallback required");
    require(summary.reason == reason, "frame fallback summary records fallback reason");
    require(summary.reached_stage == reached_stage, "frame fallback summary records reached stage");
    require(
        summary.recoverable
            == (classification == vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable),
        "frame fallback summary records recoverable classification");
    require(
        summary.fatal == (classification == vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal),
        "frame fallback summary records fatal classification");
    require(summary.reason_count == 1, "frame fallback summary records one fallback reason");
    require(!summary.completed(), "frame fallback summary does not complete when fallback is required");
}

void require_completed_frame_fallback_summary(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_fallback_summary& summary)
{
    using namespace quiz_vulkan::render;

    require(summary.checked, "completed frame fallback summary is checked");
    require(!summary.required, "completed frame fallback summary does not require fallback");
    require(
        summary.reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "completed frame fallback summary records no fallback reason");
    require(
        summary.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_presented,
        "completed frame fallback summary records presented frame stage");
    require(!summary.recoverable, "completed frame fallback summary is not recoverable failure");
    require(!summary.fatal, "completed frame fallback summary is not fatal failure");
    require(summary.reason_count == 0, "completed frame fallback summary has no reasons");
    require(summary.completed(), "completed frame fallback summary completes");
}

const quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_step_snapshot* find_lifecycle_snapshot(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_lifecycle_policy_state& state,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_step step)
{
    for (const quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_step_snapshot& snapshot :
         state.snapshots) {
        if (snapshot.step == step) {
            return &snapshot;
        }
    }
    return nullptr;
}

void require_stable_frame_lifecycle_snapshot_order(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_lifecycle_policy_state& state)
{
    using namespace quiz_vulkan::render;

    require(state.snapshots.size() == 5, "frame lifecycle policy stores five stable snapshots");
    require(
        state.snapshots[0].step == vulkan_backend::vulkan_frame_lifecycle_step::acquire,
        "frame lifecycle first snapshot is acquire");
    require(state.snapshots[0].expected_order == 0, "frame lifecycle acquire expected order is stable");
    require(
        state.snapshots[1].step == vulkan_backend::vulkan_frame_lifecycle_step::begin,
        "frame lifecycle second snapshot is begin");
    require(state.snapshots[1].expected_order == 1, "frame lifecycle begin expected order is stable");
    require(
        state.snapshots[2].step == vulkan_backend::vulkan_frame_lifecycle_step::render,
        "frame lifecycle third snapshot is render");
    require(state.snapshots[2].expected_order == 2, "frame lifecycle render expected order is stable");
    require(
        state.snapshots[3].step == vulkan_backend::vulkan_frame_lifecycle_step::submit,
        "frame lifecycle fourth snapshot is submit");
    require(state.snapshots[3].expected_order == 3, "frame lifecycle submit expected order is stable");
    require(
        state.snapshots[4].step == vulkan_backend::vulkan_frame_lifecycle_step::present,
        "frame lifecycle fifth snapshot is present");
    require(state.snapshots[4].expected_order == 4, "frame lifecycle present expected order is stable");
}

void require_completed_frame_lifecycle_policy(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_lifecycle_policy_state& state)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "frame lifecycle policy is checked");
    require(state.completed(), "frame lifecycle policy completes");
    require(state.ordering_valid, "frame lifecycle policy ordering is valid");
    require(!state.recoverable_failure, "frame lifecycle policy has no recoverable failure");
    require(!state.fatal_failure, "frame lifecycle policy has no fatal failure");
    require(
        state.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "frame lifecycle policy has no fallback reason");
    require(state.attempted_step_count == 5, "frame lifecycle policy attempts every step");
    require(state.completed_step_count == 5, "frame lifecycle policy completes every step");
    require_stable_frame_lifecycle_snapshot_order(state);

    for (const vulkan_backend::vulkan_frame_lifecycle_step_snapshot& snapshot : state.snapshots) {
        require(snapshot.attempted, "completed frame lifecycle snapshot is attempted");
        require(snapshot.completed, "completed frame lifecycle snapshot is completed");
        require(
            snapshot.status == vulkan_backend::vulkan_frame_lifecycle_order_status::completed,
            "completed frame lifecycle snapshot reports completed status");
        require(
            snapshot.failure_classification
                == vulkan_backend::vulkan_frame_lifecycle_failure_classification::none,
            "completed frame lifecycle snapshot has no failure classification");
        require(
            snapshot.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
            "completed frame lifecycle snapshot has no fallback reason");
        require(!snapshot.failed(), "completed frame lifecycle snapshot does not fail");
        require(snapshot.observed_order == snapshot.expected_order, "completed frame lifecycle order is deterministic");
    }
}

void require_failed_frame_lifecycle_policy(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_lifecycle_policy_state& state,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_step failed_step,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_lifecycle_failure_classification classification,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason reason,
    std::size_t attempted_step_count,
    std::size_t completed_step_count)
{
    using namespace quiz_vulkan::render;

    require(state.checked, "failed frame lifecycle policy is checked");
    require(!state.completed(), "failed frame lifecycle policy does not complete");
    require(state.ordering_valid, "failed frame lifecycle policy keeps ordering valid");
    require(
        state.recoverable_failure
            == (classification == vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable),
        "failed frame lifecycle policy records recoverable classification");
    require(
        state.fatal_failure
            == (classification == vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal),
        "failed frame lifecycle policy records fatal classification");
    require(state.failed_step == failed_step, "failed frame lifecycle policy records failed step");
    require(state.fallback_reason == reason, "failed frame lifecycle policy records fallback reason");
    require(state.attempted_step_count == attempted_step_count, "failed frame lifecycle attempted count is stable");
    require(state.completed_step_count == completed_step_count, "failed frame lifecycle completed count is stable");
    require_stable_frame_lifecycle_snapshot_order(state);

    const vulkan_backend::vulkan_frame_lifecycle_step_snapshot* failed_snapshot =
        find_lifecycle_snapshot(state, failed_step);
    require(failed_snapshot != nullptr, "failed frame lifecycle snapshot exists");
    require(failed_snapshot->attempted, "failed frame lifecycle snapshot is attempted");
    require(!failed_snapshot->completed, "failed frame lifecycle snapshot is not completed");
    require(
        failed_snapshot->status == vulkan_backend::vulkan_frame_lifecycle_order_status::failed,
        "failed frame lifecycle snapshot reports failed status");
    require(failed_snapshot->failure_classification == classification, "failed snapshot records classification");
    require(failed_snapshot->fallback_reason == reason, "failed snapshot records fallback reason");
    require(failed_snapshot->failed(), "failed snapshot exposes failure helper");
    require(
        failed_snapshot->observed_order == failed_snapshot->expected_order,
        "failed snapshot keeps deterministic observed order");

    for (const vulkan_backend::vulkan_frame_lifecycle_step_snapshot& snapshot : state.snapshots) {
        if (snapshot.expected_order < failed_snapshot->expected_order) {
            require(snapshot.attempted, "pre-failure lifecycle snapshot is attempted");
            require(snapshot.completed, "pre-failure lifecycle snapshot is completed");
            require(
                snapshot.status == vulkan_backend::vulkan_frame_lifecycle_order_status::completed,
                "pre-failure lifecycle snapshot completed before failure");
        }
        if (snapshot.expected_order > failed_snapshot->expected_order) {
            require(!snapshot.attempted, "post-failure lifecycle snapshot is not attempted");
            require(!snapshot.completed, "post-failure lifecycle snapshot is not completed");
            require(
                snapshot.status == vulkan_backend::vulkan_frame_lifecycle_order_status::skipped,
                "post-failure lifecycle snapshot is skipped");
        }
    }
}

void test_vulkan_resource_binding_state_builds_batch_snapshots()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{20.0f, 0.0f, 20.0f, 20.0f},
        "fixture://renderer/card.png"));
    draw_list.commands.push_back(make_text_command(
        "text",
        render_rect{40.0f, 0.0f, 40.0f, 20.0f},
        "bindings"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 100,
            .surface_height = 100,
        });
    const vulkan_backend::vulkan_backend_resource_binding_state state =
        vulkan_backend::build_vulkan_resource_binding_state(draw_list, plan);

    require(state.checked, "resource binding state is checked");
    require(!state.missing_resource, "resource binding state reports no missing resources");
    require(state.completed(), "resource binding state completes for quad, image, and text resources");
    require(state.planned_batch_count == 3, "resource binding state tracks planned batch count");
    require(state.descriptor_set_count == 3, "resource binding state creates one descriptor set per batch");
    require(state.binding_count == 8, "resource binding state has stable aggregate bind count");
    require(state.batch_snapshots.size() == 3, "resource binding state stores one snapshot per batch");
    require(state.descriptor_validation.checked, "resource binding state checks descriptor validation");
    require(state.descriptor_validation.completed(), "descriptor validation completes for valid bindings");
    require(!state.descriptor_validation.missing_required_resource, "descriptor validation reports no missing resources");
    require(!state.descriptor_validation.duplicate_binding, "descriptor validation reports no duplicate bindings");
    require(!state.descriptor_validation.invalid_layout, "descriptor validation reports no invalid layouts");
    require(state.descriptor_validation.planned_batch_count == 3, "descriptor validation tracks planned batch count");
    require(state.descriptor_validation.descriptor_set_count == 3, "descriptor validation counts descriptor sets");
    require(
        state.descriptor_validation.valid_descriptor_set_count == 3,
        "descriptor validation counts valid descriptor sets");
    require(
        state.descriptor_validation.invalid_descriptor_set_count == 0,
        "descriptor validation counts no invalid descriptor sets");
    require(state.descriptor_validation.requested_binding_count == 8, "descriptor validation counts requested bindings");
    require(state.descriptor_validation.valid_binding_count == 8, "descriptor validation counts valid bindings");
    require(state.descriptor_validation.invalid_binding_count == 0, "descriptor validation counts no invalid bindings");
    require(state.descriptor_validation.descriptor_sets.size() == 3, "descriptor validation stores one set per batch");

    const vulkan_backend::vulkan_batch_resource_binding_snapshot& quad = state.batch_snapshots[0];
    require(quad.batch_kind == vulkan_backend::vulkan_batch_kind::quad, "first binding snapshot is quad");
    require(quad.command_index == 0, "quad binding snapshot keeps command index");
    require(quad.descriptor_set_count == 1, "quad binding snapshot has one descriptor set");
    require(quad.binding_count == 2, "quad binding snapshot has stable bind count");
    require(quad.completed(), "quad binding snapshot completes");
    require(
        quad.bindings[0].kind == vulkan_backend::vulkan_resource_binding_kind::batch_uniform,
        "quad binding snapshot binds batch uniform first");
    require(
        quad.bindings[1].kind == vulkan_backend::vulkan_resource_binding_kind::quad_vertex_buffer,
        "quad binding snapshot binds quad vertices second");
    require(quad.bindings[0].resource_id == "batch_uniform:quad", "quad uniform resource id is stable");
    require(quad.bindings[1].resource_id == "quad_vertices:quad", "quad vertex resource id is stable");

    const vulkan_backend::vulkan_descriptor_set_validation_snapshot& quad_descriptor =
        state.descriptor_validation.descriptor_sets[0];
    require(quad_descriptor.batch_kind == vulkan_backend::vulkan_batch_kind::quad, "first descriptor set is quad");
    require(quad_descriptor.command_index == 0, "quad descriptor validation keeps command index");
    require(quad_descriptor.expected_binding_count == 2, "quad descriptor expects two bindings");
    require(quad_descriptor.actual_binding_count == 2, "quad descriptor validates two actual bindings");
    require(quad_descriptor.completed(), "quad descriptor validation completes");
    require(
        quad_descriptor.status == vulkan_backend::vulkan_descriptor_validation_status::valid,
        "quad descriptor validation status is valid");
    require(quad_descriptor.bindings[0].binding_index_matches_order, "quad uniform binding order is valid");
    require(quad_descriptor.bindings[1].binding_index_matches_order, "quad vertex binding order is valid");
    require(quad_descriptor.bindings[1].completed(), "quad vertex descriptor binding completes");

    const vulkan_backend::vulkan_batch_resource_binding_snapshot& image = state.batch_snapshots[1];
    require(image.batch_kind == vulkan_backend::vulkan_batch_kind::image, "second binding snapshot is image");
    require(image.binding_count == 3, "image binding snapshot has stable bind count");
    require(image.completed(), "image binding snapshot completes");
    require(
        image.bindings[1].kind == vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "image binding snapshot binds texture resource");
    require(
        image.bindings[2].kind == vulkan_backend::vulkan_resource_binding_kind::image_sampler,
        "image binding snapshot binds sampler resource");
    require(
        image.bindings[1].resource_id == "fixture://renderer/card.png",
        "image binding snapshot uses image URI as texture resource");
    require(
        state.descriptor_validation.descriptor_sets[1].expected_binding_count == 3,
        "image descriptor validation expects texture and sampler bindings");
    require(
        state.descriptor_validation.descriptor_sets[1].bindings[2].kind
            == vulkan_backend::vulkan_resource_binding_kind::image_sampler,
        "image descriptor validation records sampler binding");

    const vulkan_backend::vulkan_batch_resource_binding_snapshot& text = state.batch_snapshots[2];
    require(text.batch_kind == vulkan_backend::vulkan_batch_kind::text, "third binding snapshot is text");
    require(text.binding_count == 3, "text binding snapshot has stable bind count");
    require(text.completed(), "text binding snapshot completes");
    require(
        text.bindings[1].kind == vulkan_backend::vulkan_resource_binding_kind::text_run_buffer,
        "text binding snapshot binds text run buffer");
    require(
        text.bindings[2].kind == vulkan_backend::vulkan_resource_binding_kind::text_glyph_atlas,
        "text binding snapshot binds glyph atlas");
    require(text.bindings[1].resource_id == "text_runs:text", "text run resource id is stable");
    require(text.bindings[2].resource_id == "glyph_atlas:title", "text glyph atlas resource id is stable");
    require(
        state.descriptor_validation.descriptor_sets[2].bindings[2].kind
            == vulkan_backend::vulkan_resource_binding_kind::text_glyph_atlas,
        "text descriptor validation records glyph atlas binding");
}

const quiz_vulkan::render::vulkan_backend::vulkan_resource_registry_entry* find_registry_resource(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_resource_registry_state& state,
    quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_kind kind,
    std::string_view resource_id)
{
    for (const quiz_vulkan::render::vulkan_backend::vulkan_resource_registry_entry& entry : state.resources) {
        if (entry.kind == kind && entry.resource_id == resource_id) {
            return &entry;
        }
    }
    return nullptr;
}

const quiz_vulkan::render::vulkan_backend::vulkan_frame_resource_lifetime_snapshot* find_frame_resource(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_resource_lifetime_state& state,
    quiz_vulkan::render::vulkan_backend::vulkan_frame_resource_kind kind,
    std::string_view resource_id)
{
    for (const quiz_vulkan::render::vulkan_backend::vulkan_frame_resource_lifetime_snapshot& resource :
         state.resources) {
        if (resource.kind == kind && resource.resource_id == resource_id) {
            return &resource;
        }
    }
    return nullptr;
}

void test_vulkan_resource_registry_state_tracks_reuse_counts()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image_a",
        render_rect{20.0f, 0.0f, 20.0f, 20.0f},
        "fixture://renderer/shared.png"));
    draw_list.commands.push_back(make_image_command(
        "image_b",
        render_rect{40.0f, 0.0f, 20.0f, 20.0f},
        "fixture://renderer/shared.png"));
    draw_list.commands.push_back(make_text_command(
        "text_a",
        render_rect{0.0f, 24.0f, 40.0f, 20.0f},
        "first"));
    draw_list.commands.push_back(make_text_command(
        "text_b",
        render_rect{40.0f, 24.0f, 40.0f, 20.0f},
        "second"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 100,
            .surface_height = 100,
        });
    const vulkan_backend::vulkan_backend_resource_binding_state bindings =
        vulkan_backend::build_vulkan_resource_binding_state(draw_list, plan);
    const vulkan_backend::vulkan_backend_resource_registry_state registry =
        vulkan_backend::build_vulkan_resource_registry_state(draw_list, plan, bindings);

    require(registry.checked, "resource registry state is checked");
    require(registry.completed(), "resource registry state completes when all resources are bound");
    require(registry.planned_batch_count == 5, "resource registry tracks planned batch count");
    require(registry.descriptor_binding_count == 14, "resource registry counts descriptor binding slots");
    require(registry.registered_resource_count == 11, "resource registry stores unique resources");
    require(registry.descriptor_reuse_count == 3, "resource registry counts descriptor reuse");
    require(registry.resource_reuse_count == 3, "resource registry counts resource reuse");
    require(registry.missing_resource_count == 0, "resource registry reports no missing resources");
    require(registry.missing_resources.empty(), "resource registry missing-resource list is empty");

    const vulkan_backend::vulkan_resource_registry_entry* image_texture = find_registry_resource(
        registry,
        vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "fixture://renderer/shared.png");
    require(image_texture != nullptr, "resource registry stores shared image texture");
    require(image_texture->use_count == 2, "resource registry records shared image texture use count");
    require(image_texture->reused(), "resource registry marks shared image texture reused");
    require(image_texture->first_command_index == 1, "resource registry records first image command index");
    require(image_texture->last_command_index == 2, "resource registry records last image command index");

    const vulkan_backend::vulkan_resource_registry_entry* image_sampler = find_registry_resource(
        registry,
        vulkan_backend::vulkan_resource_binding_kind::image_sampler,
        "image_sampler:1:1:0:0:0");
    require(image_sampler != nullptr, "resource registry stores shared image sampler");
    require(image_sampler->use_count == 2, "resource registry records shared image sampler use count");

    const vulkan_backend::vulkan_resource_registry_entry* glyph_atlas = find_registry_resource(
        registry,
        vulkan_backend::vulkan_resource_binding_kind::text_glyph_atlas,
        "glyph_atlas:title");
    require(glyph_atlas != nullptr, "resource registry stores shared glyph atlas");
    require(glyph_atlas->use_count == 2, "resource registry records shared glyph atlas use count");

    require(
        registry.resources[0].resource_id == "batch_uniform:quad",
        "resource registry keeps stable first-use resource order");
    require(
        registry.resources[3].resource_id == "fixture://renderer/shared.png",
        "resource registry keeps stable shared image texture order");
}

void test_vulkan_resource_registry_state_reports_missing_resources()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{0.0f, 0.0f, 40.0f, 40.0f},
        ""));
    draw_list.commands.push_back(render_draw_command{
        .type = render_draw_command_type::text,
        .node_id = "text",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{40.0f, 0.0f, 40.0f, 24.0f},
        .content_bounds = render_rect{40.0f, 0.0f, 40.0f, 24.0f},
        .paint = render_paint{.source = "white", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    });

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 100,
            .surface_height = 100,
        });
    const vulkan_backend::vulkan_backend_resource_binding_state bindings =
        vulkan_backend::build_vulkan_resource_binding_state(draw_list, plan);
    const vulkan_backend::vulkan_backend_resource_registry_state registry =
        vulkan_backend::build_vulkan_resource_registry_state(draw_list, plan, bindings);

    require(bindings.descriptor_validation.checked, "missing-resource descriptor validation is checked");
    require(!bindings.descriptor_validation.completed(), "missing-resource descriptor validation does not complete");
    require(
        bindings.descriptor_validation.missing_required_resource,
        "missing-resource descriptor validation reports missing required resource");
    require(!bindings.descriptor_validation.duplicate_binding, "missing-resource descriptor validation has no duplicate binding");
    require(!bindings.descriptor_validation.invalid_layout, "missing-resource descriptor validation has valid layout");
    require(bindings.descriptor_validation.planned_batch_count == 2, "missing-resource validation tracks planned batches");
    require(bindings.descriptor_validation.descriptor_set_count == 2, "missing-resource validation counts descriptor sets");
    require(
        bindings.descriptor_validation.valid_descriptor_set_count == 0,
        "missing-resource validation counts no fully valid descriptor sets");
    require(
        bindings.descriptor_validation.invalid_descriptor_set_count == 2,
        "missing-resource validation counts invalid descriptor sets");
    require(
        bindings.descriptor_validation.requested_binding_count == 6,
        "missing-resource validation counts requested bindings");
    require(bindings.descriptor_validation.valid_binding_count == 3, "missing-resource validation counts valid bindings");
    require(
        bindings.descriptor_validation.invalid_binding_count == 3,
        "missing-resource validation counts invalid bindings");
    require(
        bindings.descriptor_validation.failed_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "missing-resource validation records first failed batch kind");
    require(bindings.descriptor_validation.failed_command_index == 0, "missing-resource validation records command index");
    require(
        bindings.descriptor_validation.failed_binding_kind == vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "missing-resource validation records first missing binding kind");
    require(bindings.descriptor_validation.failed_binding == 1, "missing-resource validation records first missing binding slot");

    const vulkan_backend::vulkan_descriptor_set_validation_snapshot& image_descriptor =
        bindings.descriptor_validation.descriptor_sets[0];
    require(image_descriptor.missing_required_resource, "image descriptor validation records missing texture");
    require(
        image_descriptor.status == vulkan_backend::vulkan_descriptor_validation_status::missing_required_resource,
        "image descriptor validation status reports missing resource");
    require(image_descriptor.bindings[1].bound == false, "image descriptor texture binding is not bound");
    require(
        image_descriptor.bindings[1].status
            == vulkan_backend::vulkan_descriptor_validation_status::missing_required_resource,
        "image descriptor texture binding status reports missing resource");

    const vulkan_backend::vulkan_descriptor_set_validation_snapshot& text_descriptor =
        bindings.descriptor_validation.descriptor_sets[1];
    require(text_descriptor.missing_required_resource, "text descriptor validation records missing text resources");
    require(text_descriptor.invalid_layout == false, "text descriptor validation keeps expected layout");
    require(
        text_descriptor.bindings[1].status
            == vulkan_backend::vulkan_descriptor_validation_status::missing_required_resource,
        "text run descriptor binding status reports missing resource");

    require(registry.checked, "missing-resource registry is checked");
    require(!registry.completed(), "missing-resource registry does not complete");
    require(registry.planned_batch_count == 2, "missing-resource registry tracks planned batches");
    require(registry.descriptor_binding_count == 6, "missing-resource registry counts attempted descriptor slots");
    require(registry.registered_resource_count == 3, "missing-resource registry records available resources");
    require(registry.missing_resource_count == 3, "missing-resource registry counts all missing resources");
    require(registry.missing_resources.size() == 3, "missing-resource registry stores all missing resources");
    require(
        registry.missing_resources[0].kind == vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "missing-resource registry records missing image texture first");
    require(registry.missing_resources[0].resource_id == "missing_image_texture:image", "missing image id is stable");
    require(
        registry.missing_resources[1].kind == vulkan_backend::vulkan_resource_binding_kind::text_run_buffer,
        "missing-resource registry records missing text run buffer second");
    require(registry.missing_resources[1].resource_id == "missing_text_run_buffer:text", "missing text run id is stable");
    require(
        registry.missing_resources[2].kind == vulkan_backend::vulkan_resource_binding_kind::text_glyph_atlas,
        "missing-resource registry records missing glyph atlas third");
    require(registry.missing_resources[2].resource_id == "missing_text_glyph_atlas:text", "missing glyph atlas id is stable");
}

void test_vulkan_backend_adapter_completes_fake_device_lifecycle()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{8.0f, 16.0f, 16.0f, 16.0f},
        render_color{0.25f, 0.5f, 0.75f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 32});
    vulkan_backend::diagnostic_vulkan_command_recorder recorder;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require(result.completed(), "fake backend completes frame lifecycle");
    require(result.attempted, "fake backend records lifecycle attempt");
    require(!result.fallback_required, "completed fake backend frame does not require fallback");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "completed fake backend frame has no fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_presented,
        "completed fake backend reports presented frame stage");
    require(result.lifecycle.ready_for_frame(), "fake backend reports lifecycle resources ready");
    require(result.lifecycle_ready, "fake backend lifecycle is frame-ready");
    require(result.surface_ready, "fake backend surface is ready");
    require(result.frame_begun, "fake backend begins a frame");
    require(result.swapchain.acquire_requested, "fake backend requests swapchain image acquisition");
    require(result.swapchain.acquire.completed(), "fake backend acquires a swapchain image");
    require(result.swapchain.acquire.image.ready_for_recording(), "acquired swapchain image is ready for recording");
    require(result.swapchain.acquire.image.id.value == 7, "fake backend records acquired swapchain image id");
    require(result.commands_recorded, "fake backend records frame commands");
    require(result.frame_submitted, "fake backend submits frame");
    require(result.swapchain.present_requested, "fake backend requests swapchain image presentation");
    require(result.swapchain.present.completed(), "fake backend presents acquired swapchain image");
    require(result.swapchain.present.image_id.value == 7, "fake backend presents the acquired image id");
    require(result.swapchain.completed(), "fake backend records completed swapchain lifecycle");
    require_completed_swapchain_policy(result.swapchain_policy, 64, 32);
    require_completed_frame_sync(result.frame_sync);
    require_completed_frame_lifecycle_policy(result.lifecycle_policy);
    require_completed_frame_present_policy(result.present_policy, 7);
    require_completed_frame_fallback_summary(result.fallback_summary);
    require_completed_command_buffer_submit_state(result.command_buffer_submit, 1);
    require(result.resource_bindings.completed(), "fake backend records completed resource binding state");
    require(result.resource_bindings.planned_batch_count == 1, "fake backend resource bindings track batch count");
    require(result.resource_bindings.binding_count == 2, "fake backend quad resource bindings have stable bind count");
    require(result.resource_registry.completed(), "fake backend records completed resource registry state");
    require(result.resource_registry.descriptor_binding_count == 2, "fake backend resource registry counts descriptor slots");
    require(result.resource_registry.registered_resource_count == 2, "fake backend resource registry stores quad resources");
    require(result.resource_registry.resource_reuse_count == 0, "fake backend resource registry records no quad reuse");
    require(result.frame_resources.checked, "fake backend checks frame resource lifetime");
    require(result.frame_resources.completed(), "fake backend releases all frame resources after presentation");
    require(!result.frame_resources.fallback_cleanup, "completed fake backend frame does not use fallback cleanup");
    require(result.frame_resources.planned_batch_count == 1, "fake backend frame resources track planned batch count");
    require(result.frame_resources.tracked_resource_count == 3, "fake backend tracks descriptor, image, and command resources");
    require(result.frame_resources.acquired_resource_count == 3, "fake backend acquires all tracked frame resources");
    require(result.frame_resources.released_resource_count == 3, "fake backend releases all tracked frame resources");
    require(result.frame_resources.pending_resource_count == 0, "fake backend leaves no pending frame resources");
    require(result.frame_resources.fallback_release_count == 0, "fake backend has no fallback resource releases");

    const vulkan_backend::vulkan_frame_resource_lifetime_snapshot* descriptor_set =
        find_frame_resource(
            result.frame_resources,
            vulkan_backend::vulkan_frame_resource_kind::descriptor_set,
            "descriptor_set:quad");
    require(descriptor_set != nullptr, "fake backend tracks quad descriptor set lifetime");
    require(descriptor_set->command_index == 0, "descriptor set lifetime keeps source command index");
    require(descriptor_set->completed(), "descriptor set lifetime completes");
    require(
        descriptor_set->release_stage == vulkan_backend::vulkan_frame_resource_release_stage::after_present,
        "descriptor set releases after completed presentation");

    const vulkan_backend::vulkan_frame_resource_lifetime_snapshot* swapchain_image =
        find_frame_resource(
            result.frame_resources,
            vulkan_backend::vulkan_frame_resource_kind::swapchain_image,
            "swapchain_image:7");
    require(swapchain_image != nullptr, "fake backend tracks acquired swapchain image lifetime");
    require(swapchain_image->completed(), "swapchain image lifetime completes");
    require(
        swapchain_image->release_stage == vulkan_backend::vulkan_frame_resource_release_stage::after_present,
        "swapchain image releases after completed presentation");

    const vulkan_backend::vulkan_frame_resource_lifetime_snapshot* command_buffer =
        find_frame_resource(
            result.frame_resources,
            vulkan_backend::vulkan_frame_resource_kind::command_buffer,
            "command_buffer:1001");
    require(command_buffer != nullptr, "fake backend tracks command buffer lifetime");
    require(command_buffer->completed(), "command buffer lifetime completes");
    require(
        command_buffer->release_stage == vulkan_backend::vulkan_frame_resource_release_stage::after_present,
        "command buffer releases after completed presentation");
    require(result.frame_presented, "fake backend presents frame");
    require(result.planned_batch_count == 1, "fake backend receives one planned batch");
    require(result.recorded_batch_count == 1, "fake backend records one batch");
    require(result.clipped_draw_call_count == 0, "unclipped fake backend batch is not clipped");
    require(result.discarded_draw_call_count == 0, "visible fake backend batch is not discarded");
    require(result.pipeline.completed(), "fake backend pipeline cache reports completed state");
    require(result.pipeline.lifecycle.completed(), "fake backend pipeline lifecycle reports completed state");
    require(result.pipeline.compatibility.completed(), "fake backend pipeline compatibility summary completes");
    require(result.pipeline.compatibility.requested_key_count == 1, "fake backend pipeline compatibility records one request");
    require(result.pipeline.compatibility.keys.front().batch_kind == vulkan_backend::vulkan_batch_kind::quad, "fake backend pipeline compatibility records quad key");
    require(result.pipeline.shader_bindings.completed(), "fake backend shader binding readiness completes");
    require(result.pipeline.shader_bindings.requested_binding_count == 2, "fake backend shader binding readiness records two stages");
    require(result.pipeline.lifecycle.pipeline_snapshots.size() == 1, "fake backend pipeline lifecycle stores one request");
    require(
        result.pipeline.lifecycle.pipeline_snapshots.front().completed(),
        "fake backend pipeline lifecycle request completes");
    require(result.pipeline.supports(vulkan_backend::vulkan_batch_kind::quad), "fake backend pipeline cache supports quads");
    require(result.pipeline.requested_pipeline_count == 1, "fake backend pipeline cache records one pipeline request");
    require(result.pipeline.cache_entries[0].requested, "fake backend pipeline cache records quad request");
    require(result.command_recorder.ready, "fake backend command recorder is ready");
    require(result.command_recorder.frame_open, "fake backend command recorder opens a frame");
    require(result.command_recorder.command_buffer_recorded, "fake backend command buffer is recorded");
    require(result.command_recorder.planned_batch_count == 1, "fake backend command recorder sees planned batch count");
    require(result.command_recorder.recorded_batch_count == 1, "fake backend command recorder sees recorded batch count");
    require_allowed_command_recorder_gate(result.command_recorder.gate, 1);
    require(result.command_recorder.completed(), "fake backend command recorder reports completed state");
    require(!result.command_recorder.failed(), "fake backend command recorder reports no failure");
    require(recorder.recorder_state().completed(), "adapter drives injected command recorder to completion");
    require(result.command_recorder.recorded_batches.size() == 1, "fake backend command recorder stores one batch");
    require(
        result.command_recorder.recorded_batches.front().command_index == 0,
        "fake backend command recorder stores source command index");
    require(
        result.command_recorder.recorded_batches.front().recording_index == 0,
        "fake backend command recorder stores recording order");

    require(device.calls.size() == 6, "fake backend lifecycle call count");
    require(device.calls[0] == "acquire", "fake backend acquires image before beginning");
    require(device.calls[1] == "begin", "fake backend begins before recording");
    require(device.calls[2] == "record", "fake backend records before submit");
    require(device.calls[3] == "submit", "fake backend submits before present");
    require(device.calls[4] == "present_image", "fake backend presents image after submit");
    require(device.calls[5] == "present", "fake backend presents frame last");
    require(device.begun_surface.width == 64, "fake backend begin receives surface width");
    require(device.begun_surface.height == 32, "fake backend begin receives surface height");
    require(device.acquired_surface.width == 64, "fake backend acquire receives surface width");
    require(device.acquired_surface.height == 32, "fake backend acquire receives surface height");
    require(device.presented_image_id.value == 7, "fake backend present receives acquired image id");

    require(device.recorded_plan.surface_width == 64, "recorded frame plan uses device surface width");
    require(device.recorded_plan.surface_height == 32, "recorded frame plan uses device surface height");
    require(device.recorded_plan.batches.size() == 1, "recorded frame plan keeps one batch");
    const vulkan_backend::vulkan_draw_batch& batch = device.recorded_plan.batches.front();
    require(batch.kind == vulkan_backend::vulkan_batch_kind::quad, "recorded batch remains a quad");
    require(batch.scissor.x == 8, "recorded batch scissor x maps to device surface");
    require(batch.scissor.y == 8, "recorded batch scissor y maps to device surface");
    require(batch.scissor.width == 16, "recorded batch scissor width maps to device surface");
    require(batch.scissor.height == 8, "recorded batch scissor height maps to device surface");
}

void test_vulkan_backend_adapter_releases_frame_resources_on_fallback_cleanup()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.submit_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend frame does not complete when submit fails");
    require(result.fallback_required, "submit failure requires CPU fallback");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "submit failure reports submit fallback reason");
    require(result.frame_resources.checked, "submit failure checks frame resource lifetime");
    require(result.frame_resources.completed(), "submit failure cleans up all acquired frame resources");
    require(result.frame_resources.fallback_cleanup, "submit failure records fallback resource cleanup");
    require(result.frame_resources.planned_batch_count == 1, "submit failure frame resources keep planned batch count");
    require(result.frame_resources.tracked_resource_count == 3, "submit failure tracks three acquired frame resources");
    require(result.frame_resources.acquired_resource_count == 3, "submit failure acquired resource count is stable");
    require(result.frame_resources.released_resource_count == 3, "submit failure releases all acquired frame resources");
    require(result.frame_resources.pending_resource_count == 0, "submit failure leaves no pending frame resources");
    require(result.frame_resources.fallback_release_count == 3, "submit failure releases every resource at fallback boundary");

    const vulkan_backend::vulkan_frame_resource_lifetime_snapshot* descriptor_set =
        find_frame_resource(
            result.frame_resources,
            vulkan_backend::vulkan_frame_resource_kind::descriptor_set,
            "descriptor_set:quad");
    require(descriptor_set != nullptr, "fallback cleanup tracks descriptor set lifetime");
    require(
        descriptor_set->release_stage == vulkan_backend::vulkan_frame_resource_release_stage::fallback_cleanup,
        "fallback cleanup releases descriptor set");

    const vulkan_backend::vulkan_frame_resource_lifetime_snapshot* swapchain_image =
        find_frame_resource(
            result.frame_resources,
            vulkan_backend::vulkan_frame_resource_kind::swapchain_image,
            "swapchain_image:7");
    require(swapchain_image != nullptr, "fallback cleanup tracks swapchain image lifetime");
    require(
        swapchain_image->release_stage == vulkan_backend::vulkan_frame_resource_release_stage::fallback_cleanup,
        "fallback cleanup releases swapchain image");

    const vulkan_backend::vulkan_frame_resource_lifetime_snapshot* command_buffer =
        find_frame_resource(
            result.frame_resources,
            vulkan_backend::vulkan_frame_resource_kind::command_buffer,
            "command_buffer:1001");
    require(command_buffer != nullptr, "fallback cleanup tracks command buffer lifetime");
    require(
        command_buffer->release_stage == vulkan_backend::vulkan_frame_resource_release_stage::fallback_cleanup,
        "fallback cleanup releases command buffer");
}

void test_vulkan_backend_adapter_preserves_plan_diagnostics()
{
    using namespace quiz_vulkan::render;

    render_draw_command clip{
        .type = render_draw_command_type::push_clip,
        .node_id = "clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{0.0f, 0.0f, 50.0f, 50.0f},
        .content_bounds = render_rect{0.0f, 0.0f, 50.0f, 50.0f},
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
    render_draw_command quad = make_quad_command(
        "quad",
        render_rect{25.0f, 25.0f, 50.0f, 50.0f},
        render_color{0.0f, 1.0f, 0.0f, 1.0f});
    render_draw_command transparent = make_quad_command(
        "transparent",
        render_rect{10.0f, 10.0f, 10.0f, 10.0f},
        render_color{0.0f, 0.0f, 0.0f, 0.0f});
    render_draw_command pop{
        .type = render_draw_command_type::pop_clip,
        .node_id = "clip",
        .parent_node_id = {},
        .depth = 0,
        .bounds = clip.bounds,
        .content_bounds = clip.content_bounds,
        .paint = {},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };

    render_draw_list draw_list;
    draw_list.commands = {clip, quad, transparent, pop};

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 10, .height = 10});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(result.completed(), "backend completes clipped diagnostic frame");
    require(result.planned_batch_count == 1, "backend reports drawable clipped batch count");
    require(result.recorded_batch_count == 1, "backend reports recorded clipped batch count");
    require(result.clipped_draw_call_count == 1, "backend reports clipped draw call count");
    require(result.discarded_draw_call_count == 1, "backend reports discarded draw call count");
    require(device.recorded_plan.batches.size() == 1, "recorded plan contains only visible clipped batch");
    require(device.recorded_plan.clipped_draw_call_count == 1, "recorded plan preserves clipped draw count");
    require(device.recorded_plan.discarded_draw_call_count == 1, "recorded plan preserves discarded draw count");

    const vulkan_backend::vulkan_draw_batch& batch = device.recorded_plan.batches.front();
    require(batch.command_index == 1, "recorded clipped batch keeps source command index");
    require(batch.clipped_bounds.x == 25.0f, "recorded clipped batch x");
    require(batch.clipped_bounds.y == 25.0f, "recorded clipped batch y");
    require(batch.clipped_bounds.width == 25.0f, "recorded clipped batch width");
    require(batch.clipped_bounds.height == 25.0f, "recorded clipped batch height");
    require(batch.scissor.x == 2, "recorded clipped batch scissor x");
    require(batch.scissor.y == 2, "recorded clipped batch scissor y");
    require(batch.scissor.width == 3, "recorded clipped batch scissor width");
    require(batch.scissor.height == 3, "recorded clipped batch scissor height");
}

void test_vulkan_backend_adapter_completes_empty_frame()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(result.completed(), "backend completes empty frame lifecycle");
    require(result.attempted, "backend records attempted empty frame");
    require(!result.fallback_required, "empty frame does not require fallback with fake device");
    require(result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none, "empty frame has no fallback reason");
    require(result.surface_ready, "backend surface is ready for empty frame");
    require(result.frame_begun, "backend begins empty frame");
    require(result.commands_recorded, "backend records empty frame commands");
    require(result.frame_submitted, "backend submits empty frame");
    require(result.frame_presented, "backend presents empty frame");
    require(result.planned_batch_count == 0, "empty frame has no planned batches");
    require(result.recorded_batch_count == 0, "empty frame has no recorded batches");
    require(result.command_recorder.ready, "empty frame has a ready command recorder");
    require(result.command_recorder.frame_open, "empty frame opens command recorder state");
    require(result.command_recorder.command_buffer_recorded, "empty frame records an empty command buffer");
    require(result.command_recorder.empty(), "empty frame command recorder has no batches");
    require_allowed_command_recorder_gate(result.command_recorder.gate, 0);
    require_completed_command_buffer_submit_state(result.command_buffer_submit, 0);
    require(result.swapchain.completed(), "empty frame still completes swapchain lifecycle");
    require_completed_frame_lifecycle_policy(result.lifecycle_policy);
    require_completed_frame_present_policy(result.present_policy, 7);
    require(result.resource_bindings.completed(), "empty frame records completed empty resource binding state");
    require(result.resource_bindings.binding_count == 0, "empty frame records no resource bindings");
    require(result.resource_registry.completed(), "empty frame records completed empty resource registry state");
    require(result.resource_registry.registered_resource_count == 0, "empty frame records no registered resources");
    require(result.clipped_draw_call_count == 0, "empty frame has no clipped draw calls");
    require(result.discarded_draw_call_count == 0, "empty frame has no discarded draw calls");
    require(device.recorded_plan.empty(), "empty frame records empty plan");
    require(device.calls.size() == 6, "empty frame still runs lifecycle");
}

void test_vulkan_backend_adapter_completes_all_discarded_frame()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "transparent",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 0.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(result.completed(), "backend completes all-discarded frame lifecycle");
    require(!result.fallback_required, "all-discarded frame does not require fallback with fake device");
    require(result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none, "all-discarded frame has no fallback reason");
    require(result.planned_batch_count == 0, "all-discarded frame has no planned batches");
    require(result.recorded_batch_count == 0, "all-discarded frame has no recorded batches");
    require(result.command_recorder.ready, "all-discarded frame has a ready command recorder");
    require(result.command_recorder.frame_open, "all-discarded frame opens command recorder state");
    require(result.command_recorder.command_buffer_recorded, "all-discarded frame records an empty command buffer");
    require(result.command_recorder.empty(), "all-discarded frame command recorder has no batches");
    require_allowed_command_recorder_gate(result.command_recorder.gate, 0);
    require_completed_command_buffer_submit_state(result.command_buffer_submit, 0);
    require(result.swapchain.completed(), "all-discarded frame still completes swapchain lifecycle");
    require_completed_frame_lifecycle_policy(result.lifecycle_policy);
    require_completed_frame_present_policy(result.present_policy, 7);
    require(result.resource_bindings.completed(), "all-discarded frame records completed empty resource binding state");
    require(result.resource_registry.completed(), "all-discarded frame records completed empty resource registry state");
    require(result.clipped_draw_call_count == 0, "all-discarded frame has no clipped draw calls");
    require(result.discarded_draw_call_count == 1, "all-discarded frame reports discarded draw call");
    require(device.recorded_plan.empty(), "all-discarded frame records empty plan");
    require(device.recorded_plan.discarded_draw_call_count == 1, "recorded all-discarded plan preserves discarded count");
    require(device.calls.size() == 6, "all-discarded frame still runs lifecycle");
}

void test_vulkan_backend_adapter_falls_back_when_batch_pipeline_is_missing()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 40.0f, 40.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{50.0f, 50.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    vulkan_backend::diagnostic_vulkan_pipeline_cache pipeline_cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .default_available = true,
            .overrides = {vulkan_backend::vulkan_pipeline_capability{
                .kind = vulkan_backend::vulkan_batch_kind::image,
                .available = false,
            }},
        });
    vulkan_backend::diagnostic_vulkan_command_recorder recorder;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        pipeline_cache,
        recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when a required batch pipeline is missing");
    require(result.lifecycle_ready, "backend lifecycle is ready before missing pipeline fallback");
    require(result.surface_ready, "backend surface is ready before missing pipeline fallback");
    require(!result.frame_begun, "backend does not begin frame when a batch pipeline is missing");
    require(!result.commands_recorded, "backend does not record commands when a batch pipeline is missing");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        "backend reports pipeline unavailable when a batch pipeline is missing");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready,
        "backend stops after frame plan when a batch pipeline is missing");
    require_completed_swapchain_policy(result.swapchain_policy, 16, 16);
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
    require(result.planned_batch_count == 2, "backend reports planned batches before missing pipeline fallback");
    require(result.recorded_batch_count == 0, "backend records no batches when a pipeline is missing");
    require(result.pipeline.cache_checked, "backend records pipeline cache check before missing pipeline fallback");
    require(!result.pipeline.ready, "backend records unready pipeline cache");
    require(result.pipeline.missing_pipeline, "backend records missing pipeline");
    require(
        result.pipeline.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "backend records missing image pipeline");
    require(result.pipeline.missing_command_index == 1, "backend records command index for missing pipeline");
    require(result.pipeline.requested_pipeline_count == 2, "backend records pipeline requests through missing pipeline");
    require(result.pipeline.cache_entries[0].requested, "backend records successful quad pipeline request");
    require(result.pipeline.cache_entries[2].requested, "backend records failed image pipeline request");
    require(!result.pipeline.cache_entries[2].available, "backend records unavailable image pipeline");
    require(result.command_recorder.ready, "backend keeps command recorder ready before missing pipeline fallback");
    require(!result.command_recorder.frame_open, "backend does not open command recorder when pipeline is missing");
    require(result.command_recorder.planned_batch_count == 2, "backend preserves planned batch count before recorder");
    require(device.calls.empty(), "backend does not call device lifecycle when a batch pipeline is missing");
    require(!recorder.recorder_state().frame_open, "injected recorder is not opened when a batch pipeline is missing");
}

void test_vulkan_backend_adapter_falls_back_without_surface()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete without a surface");
    require(result.attempted, "backend records attempted frame without surface");
    require(result.fallback_required, "backend requires fallback without a surface");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::surface_unavailable,
        "backend reports surface unavailable fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::lifecycle_ready,
        "backend reaches lifecycle-ready stage before missing surface fallback");
    require(result.swapchain_policy.checked, "backend checks swapchain policy before missing surface fallback");
    require(result.swapchain_policy.extent.checked, "backend checks extent policy before missing surface fallback");
    require(!result.swapchain_policy.completed(), "backend swapchain policy does not complete without surface");
    require(!result.swapchain_policy.extent.completed(), "backend extent policy does not complete without surface");
    require(!result.swapchain_policy.extent.extent_supported, "backend extent policy rejects zero surface");
    require(!result.swapchain_policy.extent.selected_extent.valid(), "backend extent policy has no selected extent");
    require(
        result.swapchain_policy.present_mode.selected_mode == vulkan_backend::vulkan_swapchain_present_mode::fifo,
        "backend present mode policy remains deterministic without surface");
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::surface_unavailable,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_frame_stage::lifecycle_ready);
    require(!result.surface_ready, "zero surface is not ready");
    require(!result.frame_begun, "backend does not begin frame without surface");
    require(!result.commands_recorded, "backend does not record without surface");
    require(!result.frame_submitted, "backend does not submit without surface");
    require(!result.frame_presented, "backend does not present without surface");
    require(result.planned_batch_count == 0, "backend does not build batches without surface");
    require(result.recorded_batch_count == 0, "backend does not record batches without surface");
    require(device.calls.empty(), "backend does not call device lifecycle without surface");
}

void test_vulkan_backend_adapter_falls_back_without_viewport()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 0.0f, 100.0f});

    require(!result.completed(), "backend cannot complete without a viewport");
    require(result.attempted, "backend records attempted frame without viewport");
    require(result.fallback_required, "backend requires fallback without viewport");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::viewport_unavailable,
        "backend reports viewport unavailable fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::surface_extent_ready,
        "backend reaches surface extent stage before missing viewport fallback");
    require(result.surface.width == 16, "backend records available surface width without viewport");
    require(result.surface.height == 16, "backend records available surface height without viewport");
    require_completed_swapchain_policy(result.swapchain_policy, 16, 16);
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::viewport_unavailable,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_frame_stage::surface_extent_ready);
    require(!result.surface_ready, "backend surface is not frame-ready without viewport");
    require(!result.frame_begun, "backend does not begin frame without viewport");
    require(!result.commands_recorded, "backend does not record without viewport");
    require(!result.frame_submitted, "backend does not submit without viewport");
    require(!result.frame_presented, "backend does not present without viewport");
    require(result.planned_batch_count == 0, "backend does not build batches without viewport");
    require(result.recorded_batch_count == 0, "backend does not record batches without viewport");
    require(device.calls.empty(), "backend does not call device lifecycle without viewport");
}

void test_vulkan_backend_adapter_falls_back_when_begin_fails()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.begin_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when frame begin fails");
    require(result.attempted, "backend records attempted frame when begin fails");
    require(result.fallback_required, "backend requires fallback when begin fails");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::begin_frame_failed,
        "backend reports begin fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready,
        "backend reaches frame plan stage before begin failure");
    require_completed_swapchain_policy(result.swapchain_policy, 16, 16);
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::begin_frame_failed,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
    require(result.surface_ready, "backend had a surface before begin failed");
    require(!result.frame_begun, "backend reports failed frame begin");
    require(result.swapchain.acquire_requested, "backend acquires image before failed begin");
    require(result.swapchain.acquired(), "backend records acquired image before failed begin");
    require(result.frame_sync.acquire_completed(), "backend completes acquire sync before failed begin");
    require(!result.swapchain.present_requested, "backend does not present image after failed begin");
    require_failed_frame_lifecycle_policy(
        result.lifecycle_policy,
        vulkan_backend::vulkan_frame_lifecycle_step::begin,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal,
        vulkan_backend::vulkan_backend_fallback_reason::begin_frame_failed,
        2,
        1);
    require(!result.commands_recorded, "backend does not record commands after failed begin");
    require(!result.frame_submitted, "backend does not submit after failed begin");
    require(!result.frame_presented, "backend does not present after failed begin");
    require(result.planned_batch_count == 1, "backend reports planned batch count before begin failure");
    require(result.recorded_batch_count == 0, "backend does not record batches after begin failure");
    require(result.command_recorder.ready, "command recorder was ready before begin failure");
    require(!result.command_recorder.frame_open, "command recorder does not open frame after failed begin");
    require(!result.command_recorder.command_buffer_recorded, "command recorder does not record after failed begin");
    require(result.command_recorder.planned_batch_count == 1, "command recorder records planned count before begin failure");
    require(result.command_recorder.recorded_batch_count == 0, "command recorder has no recorded count after begin failure");
    require(device.calls.size() == 2, "backend stops lifecycle after failed begin");
    require(device.calls[0] == "acquire", "backend acquires image before failed begin");
    require(device.calls[1] == "begin", "backend calls begin before stopping");
}

void test_vulkan_backend_adapter_falls_back_when_acquire_fails()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.acquire_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when image acquisition fails");
    require(result.attempted, "backend records attempted frame when image acquisition fails");
    require(result.fallback_required, "backend requires fallback when image acquisition fails");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
        "backend reports acquire image fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready,
        "backend reaches frame plan stage before acquire failure");
    require_completed_swapchain_policy(result.swapchain_policy, 16, 16);
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable,
        vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
    require(result.surface_ready, "backend had a surface before acquire failed");
    require(!result.frame_begun, "backend does not begin frame after acquire failure");
    require(result.swapchain.acquire_requested, "backend requests image acquisition before acquire failure");
    require(
        result.swapchain.acquire.status == vulkan_backend::vulkan_swapchain_acquire_status::failed,
        "backend records failed image acquisition status");
    require(!result.swapchain.acquire.completed(), "backend records incomplete image acquisition");
    require(!result.swapchain.acquire.image.available, "backend records no acquired image on acquire failure");
    require(!result.swapchain.present_requested, "backend does not present image after acquire failure");
    require(result.frame_sync.frame.valid(), "backend records frame sync identity before acquire failure");
    require(
        result.frame_sync.acquire_signal_image_available_semaphore.failed(),
        "backend records failed acquire semaphore signal");
    require(result.frame_sync.acquire_signal_fence.failed(), "backend records failed acquire fence signal");
    require(!result.frame_sync.submit_wait_image_available_semaphore.requested, "backend does not submit-wait after acquire failure");
    require(!result.frame_sync.submit_signal_render_finished_semaphore.requested, "backend does not submit-signal after acquire failure");
    require(!result.frame_sync.present_wait_render_finished_semaphore.requested, "backend does not present-wait after acquire failure");
    require_failed_frame_lifecycle_policy(
        result.lifecycle_policy,
        vulkan_backend::vulkan_frame_lifecycle_step::acquire,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable,
        vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
        1,
        0);
    require_failed_frame_acquire_policy(
        result.present_policy,
        vulkan_backend::vulkan_frame_acquire_policy_status::failed,
        vulkan_backend::vulkan_swapchain_acquire_status::failed,
        false,
        0);
    require(!result.commands_recorded, "backend does not record commands after acquire failure");
    require(!result.frame_submitted, "backend does not submit after acquire failure");
    require(!result.frame_presented, "backend does not present frame after acquire failure");
    require(result.planned_batch_count == 1, "backend reports planned batch count before acquire failure");
    require(result.recorded_batch_count == 0, "backend records no batches after acquire failure");
    require(result.command_recorder.ready, "command recorder was ready before acquire failure");
    require(!result.command_recorder.frame_open, "command recorder does not open frame after acquire failure");
    require(!result.command_recorder.command_buffer_recorded, "command recorder does not record after acquire failure");
    require(result.command_recorder.planned_batch_count == 1, "command recorder records planned count before acquire failure");
    require(device.calls.size() == 1, "backend stops lifecycle after failed acquire");
    require(device.calls[0] == "acquire", "backend acquires before stopping");
}

void test_vulkan_backend_adapter_reports_acquire_backpressure()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.acquire_backpressured = true;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when image acquisition is backpressured");
    require(result.attempted, "backend records attempted frame when image acquisition is backpressured");
    require(result.fallback_required, "backend requires fallback when image acquisition is backpressured");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
        "backend reports acquire fallback reason for backpressure");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready,
        "backend reaches frame plan stage before acquire backpressure");
    require_completed_swapchain_policy(result.swapchain_policy, 16, 16);
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable,
        vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
    require(result.swapchain.acquire_requested, "backend requests image acquisition before backpressure");
    require(
        result.swapchain.acquire.status == vulkan_backend::vulkan_swapchain_acquire_status::backpressured,
        "backend records backpressured swapchain acquisition status");
    require(!result.swapchain.acquire.completed(), "backend records incomplete backpressured acquisition");
    require(!result.swapchain.present_requested, "backend does not present image after acquire backpressure");
    require(result.frame_sync.acquire_signal_image_available_semaphore.failed(), "backend records failed acquire semaphore on backpressure");
    require(result.frame_sync.acquire_signal_fence.failed(), "backend records failed acquire fence on backpressure");
    require_failed_frame_lifecycle_policy(
        result.lifecycle_policy,
        vulkan_backend::vulkan_frame_lifecycle_step::acquire,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable,
        vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
        1,
        0);
    require_failed_frame_acquire_policy(
        result.present_policy,
        vulkan_backend::vulkan_frame_acquire_policy_status::backpressured,
        vulkan_backend::vulkan_swapchain_acquire_status::backpressured,
        true,
        7);
    require(!result.frame_begun, "backend does not begin frame after acquire backpressure");
    require(!result.commands_recorded, "backend does not record commands after acquire backpressure");
    require(!result.frame_submitted, "backend does not submit after acquire backpressure");
    require(!result.frame_presented, "backend does not present after acquire backpressure");
    require(device.calls.size() == 1, "backend stops lifecycle after acquire backpressure");
    require(device.calls[0] == "acquire", "backend acquires before reporting backpressure");
}

void test_vulkan_backend_adapter_falls_back_when_submit_fails()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.submit_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when frame submit fails");
    require(result.attempted, "backend records attempted frame when submit fails");
    require(result.fallback_required, "backend requires fallback when submit fails");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "backend reports submit fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::commands_recorded,
        "backend reaches commands recorded stage before submit failure");
    require_completed_swapchain_policy(result.swapchain_policy, 16, 16);
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable,
        vulkan_backend::vulkan_backend_frame_stage::commands_recorded);
    require(result.surface_ready, "backend had a surface before submit failed");
    require(result.frame_begun, "backend began frame before submit failed");
    require(result.swapchain.acquired(), "backend acquired image before submit failed");
    require(!result.swapchain.present_requested, "backend does not present image after failed submit");
    require(result.frame_sync.acquire_completed(), "backend completes acquire sync before submit failure");
    require(!result.frame_sync.submit_completed(), "backend does not complete submit sync after submit failure");
    require(result.frame_sync.submit_wait_image_available_semaphore.failed(), "backend records failed submit wait");
    require(result.frame_sync.submit_signal_render_finished_semaphore.failed(), "backend records failed submit semaphore signal");
    require(result.frame_sync.submit_signal_frame_fence.failed(), "backend records failed submit fence signal");
    require(!result.frame_sync.present_wait_render_finished_semaphore.requested, "backend does not present-wait after submit failure");
    require(result.present_policy.acquire.completed(), "present policy records completed acquire before submit failure");
    require(!result.present_policy.present.image_present_requested, "present policy records no present after submit failure");
    require_failed_frame_lifecycle_policy(
        result.lifecycle_policy,
        vulkan_backend::vulkan_frame_lifecycle_step::submit,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable,
        vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        4,
        3);
    require(result.commands_recorded, "backend recorded commands before submit failed");
    require(!result.frame_submitted, "backend reports failed frame submit");
    require(!result.frame_presented, "backend does not present after failed submit");
    require(result.planned_batch_count == 1, "backend reports planned batch count before submit failure");
    require(result.recorded_batch_count == 1, "backend reports recorded batch count before submit failure");
    require(result.command_recorder.frame_open, "command recorder frame remains tracked before submit failure");
    require(result.command_recorder.command_buffer_recorded, "command recorder reports recorded buffer before submit failure");
    require(result.command_recorder.recorded_batch_count == 1, "command recorder tracks recorded count before submit failure");
    require_failed_frame_submit_state(result.command_buffer_submit, 1);
    require(device.calls.size() == 4, "backend stops lifecycle after failed submit");
    require(device.calls[0] == "acquire", "backend acquires image before failed submit");
    require(device.calls[1] == "begin", "backend begins before failed submit");
    require(device.calls[2] == "record", "backend records before failed submit");
    require(device.calls[3] == "submit", "backend submits before stopping");
}

void test_vulkan_backend_adapter_falls_back_when_present_image_fails()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.present_image_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when swapchain image presentation fails");
    require(result.attempted, "backend records attempted frame when image presentation fails");
    require(result.fallback_required, "backend requires fallback when image presentation fails");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::present_image_failed,
        "backend reports present image fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_submitted,
        "backend reaches frame submitted stage before image presentation failure");
    require_completed_swapchain_policy(result.swapchain_policy, 16, 16);
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::present_image_failed,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable,
        vulkan_backend::vulkan_backend_frame_stage::frame_submitted);
    require(result.surface_ready, "backend had a surface before image presentation failed");
    require(result.frame_begun, "backend began frame before image presentation failed");
    require(result.swapchain.acquired(), "backend acquired image before image presentation failed");
    require(result.commands_recorded, "backend recorded commands before image presentation failed");
    require(result.frame_submitted, "backend submitted frame before image presentation failed");
    require(result.swapchain.present_requested, "backend requests image presentation before failure");
    require(
        result.swapchain.present.status == vulkan_backend::vulkan_swapchain_present_status::failed,
        "backend records failed image presentation status");
    require(!result.swapchain.present.completed(), "backend records incomplete image presentation");
    require(result.swapchain.present.image_id.value == 7, "backend records failed presentation image id");
    require(result.frame_sync.acquire_completed(), "backend completes acquire sync before image presentation failure");
    require(result.frame_sync.submit_completed(), "backend completes submit sync before image presentation failure");
    require(
        result.frame_sync.present_wait_render_finished_semaphore.failed(),
        "backend records failed present wait after image presentation failure");
    require(!result.frame_sync.present_completed(), "backend does not complete present sync after image failure");
    require_failed_frame_lifecycle_policy(
        result.lifecycle_policy,
        vulkan_backend::vulkan_frame_lifecycle_step::present,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable,
        vulkan_backend::vulkan_backend_fallback_reason::present_image_failed,
        5,
        4);
    require_failed_frame_present_policy(
        result.present_policy,
        vulkan_backend::vulkan_frame_present_result_status::image_failed,
        vulkan_backend::vulkan_backend_fallback_reason::present_image_failed,
        false,
        false,
        false,
        7);
    require(!result.frame_presented, "backend does not mark frame presented after image presentation failure");
    require(result.planned_batch_count == 1, "backend reports planned batch count before image presentation failure");
    require(result.recorded_batch_count == 1, "backend reports recorded batch count before image presentation failure");
    require(result.command_recorder.frame_open, "command recorder frame remains tracked before image presentation failure");
    require(
        result.command_recorder.command_buffer_recorded,
        "command recorder reports recorded buffer before image presentation failure");
    require(result.command_recorder.recorded_batch_count == 1, "command recorder tracks recorded count before image failure");
    require_completed_command_buffer_submit_state(result.command_buffer_submit, 1);
    require(device.calls.size() == 5, "backend stops lifecycle after failed image presentation");
    require(device.calls[0] == "acquire", "backend acquires image before image presentation failure");
    require(device.calls[1] == "begin", "backend begins before image presentation failure");
    require(device.calls[2] == "record", "backend records before image presentation failure");
    require(device.calls[3] == "submit", "backend submits before image presentation failure");
    require(device.calls[4] == "present_image", "backend presents image before stopping");
}

void test_vulkan_backend_adapter_falls_back_when_present_fails()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 100.0f, 100.0f},
        render_color{1.0f, 1.0f, 1.0f, 1.0f}));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 16, .height = 16});
    device.present_succeeds = false;
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 100.0f, 100.0f});

    require(!result.completed(), "backend cannot complete when presentation fails");
    require(result.attempted, "backend records attempted frame when presentation fails");
    require(result.fallback_required, "backend requires fallback when presentation fails");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        "backend reports presentation fallback reason");
    require(
        result.reached_stage == vulkan_backend::vulkan_backend_frame_stage::frame_submitted,
        "backend reaches frame submitted stage before presentation failure");
    require_completed_swapchain_policy(result.swapchain_policy, 16, 16);
    require_frame_fallback_summary(
        result.fallback_summary,
        vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable,
        vulkan_backend::vulkan_backend_frame_stage::frame_submitted);
    require(result.surface_ready, "backend had a surface before presentation failed");
    require(result.frame_begun, "backend began frame before presentation failed");
    require(result.swapchain.acquired(), "backend acquired image before presentation failed");
    require(result.swapchain.presented(), "backend presented swapchain image before frame presentation failed");
    require(result.frame_sync.completed(), "backend completes frame sync before frame presentation failure");
    require_failed_frame_lifecycle_policy(
        result.lifecycle_policy,
        vulkan_backend::vulkan_frame_lifecycle_step::present,
        vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable,
        vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        5,
        4);
    require_failed_frame_present_policy(
        result.present_policy,
        vulkan_backend::vulkan_frame_present_result_status::frame_failed,
        vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        true,
        true,
        false,
        7);
    require(result.commands_recorded, "backend recorded commands before presentation failed");
    require(result.frame_submitted, "backend submitted frame before presentation failed");
    require(!result.frame_presented, "backend reports failed presentation");
    require(result.planned_batch_count == 1, "backend reports planned batch count before presentation failure");
    require(result.recorded_batch_count == 1, "backend reports recorded batch count before presentation failure");
    require(result.command_recorder.frame_open, "command recorder frame remains tracked before presentation failure");
    require(result.command_recorder.command_buffer_recorded, "command recorder reports recorded buffer before presentation failure");
    require(result.command_recorder.recorded_batch_count == 1, "command recorder tracks recorded count before presentation failure");
    require_completed_command_buffer_submit_state(result.command_buffer_submit, 1);
    require(device.calls.size() == 6, "backend reaches present call before failing");
    require(device.calls[0] == "acquire", "backend acquires image before presentation failure");
    require(device.calls[1] == "begin", "backend begins before presentation failure");
    require(device.calls[2] == "record", "backend records before presentation failure");
    require(device.calls[3] == "submit", "backend submits before presentation failure");
    require(device.calls[4] == "present_image", "backend presents image before frame presentation failure");
    require(device.calls[5] == "present", "backend presents before reporting failure");
}

} // namespace

int main()
{
    test_vulkan_backend_fallback_reason_names_are_stable();
    test_vulkan_swapchain_status_names_are_stable();
    test_vulkan_frame_present_policy_names_are_stable();
    test_vulkan_backend_frame_stage_names_are_stable();
    test_vulkan_frame_submit_status_names_are_stable();
    test_vulkan_frame_lifecycle_policy_names_are_stable();
    test_vulkan_frame_resource_lifetime_names_are_stable();
    test_vulkan_descriptor_validation_names_are_stable();
    test_vulkan_shader_stage_names_are_stable();
    test_vulkan_pipeline_lifecycle_names_are_stable();
    test_draw_list_submission_counts_generic_work();
    test_renderer_backend_diagnostics_report_vulkan_not_requested();
    test_cpu_fallback_clips_and_discards();
    test_degenerate_surface_discards_draw_calls();
    test_vulkan_frame_plan_builds_scissored_batches_from_render_contracts();
    test_vulkan_frame_plan_applies_nested_clips();
    test_vulkan_frame_plan_ignores_unmatched_pop_clip();
    test_vulkan_diagnostic_pipeline_cache_reports_batch_capabilities();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_pipeline();
    test_vulkan_diagnostic_pipeline_cache_identifies_unavailable_render_pass();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_vertex_shader();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_fragment_shader();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_descriptor();
    test_vulkan_resource_binding_state_builds_batch_snapshots();
    test_vulkan_resource_registry_state_tracks_reuse_counts();
    test_vulkan_resource_registry_state_reports_missing_resources();
    test_vulkan_backend_adapter_completes_fake_device_lifecycle();
    test_vulkan_backend_adapter_releases_frame_resources_on_fallback_cleanup();
    test_vulkan_backend_adapter_preserves_plan_diagnostics();
    test_vulkan_backend_adapter_completes_empty_frame();
    test_vulkan_backend_adapter_completes_all_discarded_frame();
    test_vulkan_backend_adapter_falls_back_when_batch_pipeline_is_missing();
    test_vulkan_backend_adapter_falls_back_without_surface();
    test_vulkan_backend_adapter_falls_back_without_viewport();
    test_vulkan_backend_adapter_falls_back_when_begin_fails();
    test_vulkan_backend_adapter_falls_back_when_acquire_fails();
    test_vulkan_backend_adapter_reports_acquire_backpressure();
    test_vulkan_backend_adapter_falls_back_when_submit_fails();
    test_vulkan_backend_adapter_falls_back_when_present_image_fails();
    test_vulkan_backend_adapter_falls_back_when_present_fails();
    return 0;
}
