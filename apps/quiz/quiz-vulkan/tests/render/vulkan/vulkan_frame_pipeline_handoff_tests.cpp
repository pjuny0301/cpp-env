#include "render/vulkan/vulkan_backend_adapter.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>
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
        if (!acquire_succeeds) {
            return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result{
                .status = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::failed,
                .image = {},
            };
        }

        const bool image_ready =
            acquire_status == quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::acquired
            || acquire_status == quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::suboptimal;

        return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_result{
            .status = acquire_status,
            .image = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_state{
                .id = image_ready ? next_image_id
                                  : quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id{},
                .available = image_ready,
                .acquired = image_ready,
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
                ? present_image_status
                : quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_status::failed,
            .image_id = image_id,
        };
    }

    bool present_frame() override
    {
        calls.push_back("present");
        return present_frame_succeeds;
    }

    bool acquire_succeeds = true;
    bool begin_succeeds = true;
    bool record_succeeds = true;
    bool submit_succeeds = true;
    bool present_image_succeeds = true;
    bool present_frame_succeeds = true;
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_id next_image_id{.value = 7};
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status acquire_status =
        quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status::acquired;
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_status present_image_status =
        quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_status::presented;
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

quiz_vulkan::render::render_draw_command make_quad_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::quad,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "test", .color = render_color{0.2f, 0.4f, 0.8f, 1.0f}},
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
        .paint = render_paint{.source = "text", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {render_text_run{.text = std::move(text), .style_token = "body"}},
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

quiz_vulkan::render::render_draw_list make_quad_draw_list()
{
    quiz_vulkan::render::render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        quiz_vulkan::render::render_rect{0.0f, 0.0f, 24.0f, 24.0f}));
    return draw_list;
}

quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_state make_ready_loader()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_state{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_status::ready,
        .probe_status = quiz_vulkan::render::vulkan_backend::vulkan_loader_probe_status::available,
        .loader_library_available = true,
        .instance_proc_address_available = true,
        .instance_ready = true,
        .loaded_library_name = "fake-vulkan-loader",
        .required_symbol_name = "vkGetInstanceProcAddr",
        .attempted_library_count = 1,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_instance_create_result make_ready_instance()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_instance_create_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_instance_create_status::created,
        .loader = make_ready_loader(),
        .handle = quiz_vulkan::render::vulkan_backend::vulkan_instance_handle{.value = 1},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_device_create_result make_ready_device()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_device_create_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_device_create_status::created,
        .instance = make_ready_instance(),
        .handle = quiz_vulkan::render::vulkan_backend::vulkan_device_handle{.value = 2},
        .selected_queues = {
            quiz_vulkan::render::vulkan_backend::vulkan_device_queue_selection{
                .capability = quiz_vulkan::render::vulkan_backend::vulkan_device_queue_capability::graphics,
                .family_index = 0,
                .queue = quiz_vulkan::render::vulkan_backend::vulkan_queue_handle{.value = 3},
            },
        },
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_result make_ready_swapchain()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_status::created,
        .device = make_ready_device(),
        .handle = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_handle{.value = 4},
        .requested_extent = quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64},
        .selected_extent = quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64},
        .requested_present_mode = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_mode::fifo,
        .selected_present_mode = quiz_vulkan::render::vulkan_backend::vulkan_swapchain_present_mode::fifo,
        .requested_present_mode_supported = true,
        .fallback_to_fifo = false,
        .min_image_count = 2,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_render_pass_create_result make_ready_render_pass()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_render_pass_create_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_render_pass_create_status::created,
        .swapchain = make_ready_swapchain(),
        .render_pass = quiz_vulkan::render::vulkan_backend::vulkan_render_pass_handle{.value = 5},
        .framebuffer = quiz_vulkan::render::vulkan_backend::vulkan_framebuffer_handle{.value = 6},
        .requested_extent = quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64},
        .selected_extent = quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64},
        .requested_attachments = {
            quiz_vulkan::render::vulkan_backend::vulkan_render_pass_attachment_request{},
        },
        .selected_attachments = {
            quiz_vulkan::render::vulkan_backend::vulkan_render_pass_attachment_request{},
        },
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_sdk_external_header_evidence
make_ready_external_header_evidence()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_sdk_external_header_evidence{
        .checked = true,
        .vulkan = vulkan_backend::vulkan_sdk_vulkan_header_evidence{
            .available = true,
            .api_version_macro_available = true,
            .header_version_macro_available = true,
            .api_version = vulkan_backend::vulkan_sdk_api_version_1_4(),
            .header_version = 341,
            .instance_handle_size = sizeof(void*),
            .device_handle_size = sizeof(void*),
            .result_type_size = sizeof(int),
            .success_constant_available = true,
            .success_value = 0,
            .surface_extension_constant_available = true,
            .swapchain_extension_constant_available = true,
            .surface_extension_name = "VK_KHR_surface",
            .swapchain_extension_name = "VK_KHR_swapchain",
            .diagnostic = "fake Vulkan external headers available",
        },
        .vma = vulkan_backend::vulkan_sdk_vma_header_evidence{
            .available = true,
            .safe_to_include = true,
            .vulkan_headers_required = true,
            .vma_vulkan_version = 1003000,
            .allocator_handle_size = sizeof(void*),
            .allocation_handle_size = sizeof(void*),
            .diagnostic = "fake VMA external headers available",
        },
        .diagnostic = "fake external headers available",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_sdk_external_header_evidence
make_missing_external_header_evidence()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_sdk_external_header_evidence{
        .checked = true,
        .vulkan = vulkan_backend::vulkan_sdk_vulkan_header_evidence{
            .available = false,
            .diagnostic = "fake Vulkan external headers missing",
        },
        .vma = vulkan_backend::vulkan_sdk_vma_header_evidence{
            .available = false,
            .safe_to_include = false,
            .vulkan_headers_required = true,
            .diagnostic = "fake VMA external headers missing",
        },
        .diagnostic = "fake external headers missing",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_sdk_capability_result make_ready_sdk_capabilities()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_sdk_capability_result{
        .checked = true,
        .status = vulkan_backend::vulkan_sdk_capability_status::ready,
        .fallback_status = vulkan_backend::vulkan_sdk_adapter_fallback_status::none,
        .minimum_api_version = vulkan_backend::vulkan_sdk_api_version_1_3(),
        .external_headers = make_ready_external_header_evidence(),
        .headers_available = true,
        .api_version_available = true,
        .api_version_compatible = true,
        .required_extensions_ready = true,
        .native_function_table_required = true,
        .native_function_table_ready = true,
        .required_extension_count = 2,
        .available_required_extension_count = 2,
        .diagnostic = "fake Vulkan SDK path ready",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_sdk_capability_result make_missing_sdk_capabilities()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_sdk_capability_result{
        .checked = true,
        .status = vulkan_backend::vulkan_sdk_capability_status::headers_unavailable,
        .fallback_status = vulkan_backend::vulkan_sdk_adapter_fallback_status::headers_unavailable,
        .minimum_api_version = vulkan_backend::vulkan_sdk_api_version_1_3(),
        .external_headers = make_missing_external_header_evidence(),
        .headers_available = false,
        .api_version_available = false,
        .api_version_compatible = false,
        .required_extensions_ready = false,
        .native_function_table_required = true,
        .native_function_table_ready = false,
        .required_extension_count = 2,
        .diagnostic = "fake Vulkan SDK path missing",
    };
}

void require_handoff_fallback(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_pipeline_handoff& handoff,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason reason,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status status,
    quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_stage reached_stage)
{
    require(handoff.checked, "frame pipeline handoff summary is checked");
    require(handoff.blocked(), "frame pipeline handoff reports blocked fallback");
    require(!handoff.completed(), "blocked frame pipeline handoff does not complete");
    require(handoff.cpu_fallback_available, "blocked frame pipeline handoff keeps CPU fallback available");
    require(handoff.fallback_reason == reason, "frame pipeline handoff records fallback reason");
    require(handoff.status == status, "frame pipeline handoff records status");
    require(handoff.reached_stage == reached_stage, "frame pipeline handoff records reached stage");
}

void mark_native_frame_execution_ready(
    quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_result& result)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    result.command_buffer_recording.native_function_table_checked = true;
    result.command_buffer_recording.native_command_buffer_recording_ready = true;
    result.command_buffer_recording.native_function_table_status =
        vulkan_backend::vulkan_native_function_table_status::ready;
    result.command_buffer_recording.missing_native_symbol_name.clear();

    result.submit_batch_plan.native_function_table_checked = true;
    result.submit_batch_plan.native_queue_submit_ready = true;
    result.submit_batch_plan.native_function_table_status =
        vulkan_backend::vulkan_native_function_table_status::ready;
    result.submit_batch_plan.missing_native_symbol_name.clear();

    result.present_completion_plan.native_function_table_checked = true;
    result.present_completion_plan.native_queue_present_ready = true;
    result.present_completion_plan.native_function_table_status =
        vulkan_backend::vulkan_native_function_table_status::ready;
    result.present_completion_plan.missing_native_symbol_name.clear();
}

quiz_vulkan::render::vulkan_backend::vulkan_native_render_pass_scope_record_result
make_ready_render_pass_scope_for_frame(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_result& frame)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const std::size_t selected_index =
        frame.swapchain.acquire.image.id.value > 0
            ? frame.swapchain.acquire.image.id.value - 1
            : 0;

    return vulkan_backend::vulkan_native_render_pass_scope_record_result{
        .checked = true,
        .status = vulkan_backend::vulkan_native_render_pass_scope_record_status::recorded,
        .selected_framebuffer_target_index = selected_index,
        .framebuffer_target_count = selected_index + 1,
        .image_id = frame.swapchain.acquire.image.id,
        .command_buffer =
            vulkan_backend::vulkan_command_recording_command_buffer_handle{
                .value = static_cast<std::uintptr_t>(
                    9000 + frame.command_buffer_submit.recording.command_buffer.value),
            },
        .render_pass = vulkan_backend::vulkan_render_pass_handle{.value = 300},
        .framebuffer =
            vulkan_backend::vulkan_framebuffer_handle{.value = 13000 + selected_index + 1},
        .extent = frame.surface,
        .begin_render_pass_symbol = vulkan_backend::vulkan_native_function_pointer{.value = 1401},
        .end_render_pass_symbol = vulkan_backend::vulkan_native_function_pointer{.value = 1402},
        .framebuffer_targets_ready = true,
        .command_buffer_ready = true,
        .render_pass_ready = true,
        .framebuffer_ready = true,
        .extent_ready = true,
        .extent_matches = true,
        .dispatch_table_ready = true,
        .vk_cmd_begin_render_pass_called = true,
        .vk_cmd_end_render_pass_called = true,
        .diagnostic = "Scoped frame render pass recorded",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_scoped_command_packet_execution_result
execute_scoped_packets_for_frame(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_result& frame,
    quiz_vulkan::render::vulkan_backend::fake_vulkan_command_packet_executor_options options = {})
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_packet_executor executor(options);
    return vulkan_backend::execute_vulkan_scoped_command_packets(
        executor,
        vulkan_backend::vulkan_scoped_command_packet_execution_request{
            .render_pass_scope = make_ready_render_pass_scope_for_frame(frame),
            .packet_bridge = frame.command_packets,
        });
}

void test_vulkan_frame_pipeline_handoff_status_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::not_checked)
            == std::string_view{"not_checked"},
        "frame handoff status name for not checked is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::ready)
            == std::string_view{"ready"},
        "frame handoff status name for ready is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::pipeline_unavailable)
            == std::string_view{"pipeline_unavailable"},
        "frame handoff status name for pipeline unavailable is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::resource_binding_unavailable)
            == std::string_view{"resource_binding_unavailable"},
        "frame handoff status name for resource binding unavailable is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::command_recording_unavailable)
            == std::string_view{"command_recording_unavailable"},
        "frame handoff status name for command recording unavailable is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::frame_lifecycle_unavailable)
            == std::string_view{"frame_lifecycle_unavailable"},
        "frame handoff status name for frame lifecycle unavailable is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::submit_unavailable)
            == std::string_view{"submit_unavailable"},
        "frame handoff status name for submit unavailable is stable");
    require(
        vulkan_backend::frame_pipeline_handoff_status_name(
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::present_unavailable)
            == std::string_view{"present_unavailable"},
        "frame handoff status name for present unavailable is stable");
}

void test_vulkan_frame_pipeline_handoff_maps_lifecycle_prerequisites()
{
    using namespace quiz_vulkan::render;

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.instance_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::instance_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.device_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::device_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::device_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.swapchain_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::swapchain_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::swapchain_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.render_pass = make_ready_render_pass();
        device.lifecycle.render_pass.status = vulkan_backend::vulkan_render_pass_create_status::invalid_format;
        device.lifecycle.render_pass.render_pass = {};
        device.lifecycle.render_pass_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::render_pass_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::render_pass_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.pipeline_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::pipeline_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.lifecycle.command_recorder_ready = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::command_recorder_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::command_recording_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::backend_attempted);
    }
}

void test_vulkan_frame_pipeline_handoff_maps_surface_and_frame_lifecycle_failures()
{
    using namespace quiz_vulkan::render;

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{});
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::surface_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::surface_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::lifecycle_ready);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 0.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::viewport_unavailable,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::viewport_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::surface_extent_ready);
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.acquire_succeeds = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::frame_lifecycle_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
        require(result.pipeline_handoff.frame_lifecycle_attempted_step_count == 1, "handoff records one attempted lifecycle step");
        require(result.pipeline_handoff.frame_lifecycle_completed_step_count == 0, "handoff records no completed lifecycle steps");
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.submit_succeeds = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::submit_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::commands_recorded);
        require(result.pipeline_handoff.command_recording_ready, "handoff records command recording ready before submit failure");
        require(!result.pipeline_handoff.frame_submit_completed, "handoff records incomplete frame submit");
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.present_image_succeeds = false;
        const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require_handoff_fallback(
            result.pipeline_handoff,
            vulkan_backend::vulkan_backend_fallback_reason::present_image_failed,
            vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::present_unavailable,
            vulkan_backend::vulkan_backend_frame_stage::frame_submitted);
        require(result.pipeline_handoff.frame_submit_completed, "handoff records completed frame submit before present failure");
        require(!result.pipeline_handoff.present_completed, "handoff records incomplete present");
    }
}

void test_vulkan_frame_pipeline_handoff_requires_pipeline_before_resource_binding()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{0.0f, 0.0f, 32.0f, 32.0f},
        "fixture://renderer/image.png"));

    vulkan_backend::diagnostic_vulkan_pipeline_cache pipeline_cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .default_available = true,
            .overrides = {
                vulkan_backend::vulkan_pipeline_capability{
                    .kind = vulkan_backend::vulkan_batch_kind::image,
                    .available = false,
                },
            },
        });
    vulkan_backend::diagnostic_vulkan_command_recorder command_recorder;
    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});

    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        pipeline_cache,
        command_recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require_handoff_fallback(
        result.pipeline_handoff,
        vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::pipeline_unavailable,
        vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
    require(result.pipeline_handoff.pipeline_required, "handoff records pipeline required");
    require(result.pipeline_handoff.pipeline_checked, "handoff records pipeline checked");
    require(!result.pipeline_handoff.pipeline_completed, "handoff records incomplete pipeline");
    require(!result.pipeline_handoff.resource_bindings_checked, "handoff does not check resource binding after pipeline failure");
    require(!result.pipeline_handoff.command_recorder_gate_checked, "handoff does not check command recorder gate after pipeline failure");
    require(!result.pipeline_handoff.command_recording_ready, "handoff does not consider command recording ready without pipeline");
    require(device.calls.empty(), "pipeline failure stops before device lifecycle calls");
}

void test_vulkan_frame_pipeline_handoff_requires_resource_binding_before_command_recording()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{0.0f, 0.0f, 32.0f, 32.0f},
        ""));

    vulkan_backend::diagnostic_vulkan_pipeline_cache pipeline_cache;
    vulkan_backend::diagnostic_vulkan_command_recorder command_recorder;
    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});

    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        pipeline_cache,
        command_recorder,
        draw_list,
        render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require_handoff_fallback(
        result.pipeline_handoff,
        vulkan_backend::vulkan_backend_fallback_reason::resource_binding_unavailable,
        vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::resource_binding_unavailable,
        vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready);
    require(result.pipeline_handoff.pipeline_completed, "handoff records completed pipeline before resource failure");
    require(result.pipeline_handoff.resource_bindings_checked, "handoff checks resource bindings");
    require(!result.pipeline_handoff.resource_bindings_completed, "handoff records incomplete resource binding");
    require(result.pipeline_handoff.resource_registry_checked, "handoff checks resource registry");
    require(!result.pipeline_handoff.resource_registry_completed, "handoff records incomplete resource registry");
    require(result.pipeline_handoff.command_recorder_gate_checked, "handoff checks command recorder gate");
    require(!result.pipeline_handoff.command_recorder_gate_allowed, "handoff blocks command recorder gate");
    require(!result.pipeline_handoff.command_recording_ready, "handoff does not consider command recording ready without resources");
    require(device.calls.empty(), "resource binding failure stops before device lifecycle calls");
}

void test_vulkan_frame_pipeline_handoff_accepts_draw_list_render_data()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 16.0f, 16.0f}));
    draw_list.commands.push_back(make_text_command(
        "text",
        render_rect{16.0f, 0.0f, 24.0f, 16.0f},
        "frame text"));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{40.0f, 0.0f, 16.0f, 16.0f},
        "fixture://renderer/image.png"));

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
    const vulkan_backend::vulkan_backend_frame_result result = vulkan_backend::submit_vulkan_backend_frame(
        device,
        draw_list,
        render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    const vulkan_backend::vulkan_backend_frame_pipeline_handoff& handoff = result.pipeline_handoff;
    require(result.completed(), "frame result completes with generic draw-list render data");
    require(handoff.completed(), "frame pipeline handoff completes");
    require(
        handoff.status == vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::ready,
        "frame pipeline handoff reports ready");
    require(!handoff.cpu_fallback_available, "ready handoff does not require CPU fallback");
    require(handoff.planned_batch_count == 3, "handoff records three planned render batches");
    require(handoff.recorded_batch_count == 3, "handoff records three recorded render batches");
    require(handoff.quad_batch_count == 1, "handoff counts quad batch");
    require(handoff.text_batch_count == 1, "handoff counts text batch as render data");
    require(handoff.image_batch_count == 1, "handoff counts image batch as render data");
    require(handoff.pipeline_completed, "handoff requires completed pipeline before command recording");
    require(handoff.resource_bindings_completed, "handoff requires completed resource binding before command recording");
    require(handoff.resource_registry_completed, "handoff requires completed resource registry before command recording");
    require(handoff.command_packets_checked, "handoff checks command packets before command recording");
    require(handoff.command_packets_completed, "handoff requires completed command packets before command recording");
    require(handoff.command_packet_execution_checked, "handoff checks command packet execution before command recording");
    require(handoff.command_packet_execution_completed, "handoff requires completed packet execution before command recording");
    require(handoff.command_recorder_operations_checked, "handoff checks command recorder operation plan");
    require(handoff.command_recorder_operations_completed, "handoff requires completed recorder operation plan");
    require(
        result.swapchain_image_acquire_plan.ready_for_command_recording(),
        "swapchain image acquire plan reaches command recording gate");
    require(
        result.swapchain_image_acquire_plan.status
            == vulkan_backend::vulkan_swapchain_image_acquire_plan_status::ready,
        "swapchain image acquire plan reports ready before command recording");
    require(
        result.swapchain_image_acquire_plan.selected_image_index == device.next_image_id.value,
        "swapchain image acquire plan records selected image index");
    require(handoff.swapchain_recreate_policy_checked, "handoff checks swapchain recreate policy");
    require(handoff.swapchain_keep_rendering, "handoff records keep-rendering swapchain policy");
    require(!handoff.swapchain_recreate_immediately, "handoff does not request immediate recreate");
    require(!handoff.swapchain_recreate_after_frame, "handoff does not request after-frame recreate");
    require(!handoff.swapchain_skip_submit, "handoff does not skip submit for ready swapchain");
    require(!handoff.swapchain_fatal_error, "handoff does not surface fatal swapchain error");
    require(handoff.command_buffer_recording_checked, "handoff checks command buffer recording result");
    require(handoff.command_buffer_recording_completed, "handoff requires completed command buffer recording result");
    require(handoff.command_buffer_ready_for_submit, "handoff exposes command buffer readiness before submit");
    require(handoff.submit_batch_planning_checked, "handoff checks submit batch planning before submit");
    require(handoff.submit_batch_planning_completed, "handoff requires completed submit batch planning before submit");
    require(handoff.submit_batch_ready_for_queue, "handoff exposes submit batch readiness before queue submit");
    require(handoff.present_completion_planning_checked, "handoff checks present completion planning before submit");
    require(handoff.present_completion_planning_completed, "handoff requires completed present completion planning");
    require(handoff.frame_completion_ready, "handoff exposes frame completion readiness before present");
    require(handoff.command_recorder_gate_allowed, "handoff allows command recorder after prerequisites");
    require(handoff.command_recording_ready, "handoff records command recording ready");
    require(handoff.frame_submit_completed, "handoff records frame submit completed");
    require(handoff.present_completed, "handoff records present completed");
    require(handoff.frame_lifecycle_checked, "handoff records frame lifecycle policy checked");
    require(handoff.frame_lifecycle_completed, "handoff records frame lifecycle completed");
    require(handoff.frame_lifecycle_attempted_step_count == 5, "handoff records five lifecycle attempts");
    require(handoff.frame_lifecycle_completed_step_count == 5, "handoff records five completed lifecycle steps");
    require(device.calls.size() == 6, "backend executes stable frame lifecycle call count");
    require(device.calls[0] == "acquire", "backend acquires before frame begin");
    require(device.calls[1] == "begin", "backend begins after acquire");
    require(device.calls[2] == "record", "backend records after begin");
    require(device.calls[3] == "submit", "backend submits after recording");
    require(device.calls[4] == "present_image", "backend presents image after submit");
    require(device.calls[5] == "present", "backend presents frame last");
}

void test_vulkan_frame_pipeline_handoff_reports_swapchain_recreate_policy()
{
    using namespace quiz_vulkan::render;

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.acquire_status = vulkan_backend::vulkan_swapchain_acquire_status::suboptimal;
        const vulkan_backend::vulkan_backend_frame_result result =
            vulkan_backend::submit_vulkan_backend_frame(
                device,
                make_quad_draw_list(),
                render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require(result.completed(), "suboptimal acquire still completes the frame");
        require(
            result.pipeline_handoff.swapchain_recreate_policy_checked,
            "handoff records checked recreate policy for suboptimal acquire");
        require(
            result.pipeline_handoff.swapchain_recreate_after_frame,
            "handoff records after-frame recreate for suboptimal acquire");
        require(
            !result.pipeline_handoff.swapchain_skip_submit,
            "suboptimal acquire does not skip submit");
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.acquire_status = vulkan_backend::vulkan_swapchain_acquire_status::out_of_date;
        const vulkan_backend::vulkan_backend_frame_result result =
            vulkan_backend::submit_vulkan_backend_frame(
                device,
                make_quad_draw_list(),
                render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require(
            result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
            "out-of-date acquire still falls back at acquire gate");
        require(
            result.pipeline_handoff.swapchain_recreate_policy_checked,
            "handoff records checked recreate policy for out-of-date acquire");
        require(
            result.pipeline_handoff.swapchain_recreate_immediately,
            "handoff records immediate recreate for out-of-date acquire");
        require(result.pipeline_handoff.swapchain_skip_submit, "out-of-date acquire skips submit");
    }

    {
        fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
        device.present_image_status = vulkan_backend::vulkan_swapchain_present_status::suboptimal;
        const vulkan_backend::vulkan_backend_frame_result result =
            vulkan_backend::submit_vulkan_backend_frame(
                device,
                make_quad_draw_list(),
                render_rect{0.0f, 0.0f, 64.0f, 64.0f});
        require(result.completed(), "suboptimal present still completes the frame");
        require(
            result.pipeline_handoff.swapchain_recreate_after_frame,
            "handoff records after-frame recreate for suboptimal present");
        require(!result.pipeline_handoff.swapchain_fatal_error, "suboptimal present is not fatal");
    }
}

void test_vulkan_frame_pipeline_handoff_reports_sdk_native_path_summary()
{
    using namespace quiz_vulkan::render;

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
    const vulkan_backend::vulkan_backend_frame_result result =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(result.completed(), "baseline frame completes before SDK summary is applied");

    const vulkan_backend::vulkan_backend_frame_result sdk_ready =
        vulkan_backend::apply_vulkan_sdk_capability_result_to_frame(
            result,
            make_ready_sdk_capabilities());
    require(sdk_ready.completed(), "ready SDK summary keeps frame completed");
    require(
        sdk_ready.pipeline_handoff.sdk_native_path_checked,
        "handoff records checked SDK native path");
    require(sdk_ready.pipeline_handoff.sdk_adapter_ready, "handoff records SDK adapter ready");
    require(
        sdk_ready.pipeline_handoff.sdk_native_path_status
            == vulkan_backend::vulkan_sdk_native_path_status::ready,
        "handoff records SDK-ready native-path status");
    require(
        sdk_ready.pipeline_handoff.sdk_external_headers_checked,
        "handoff records checked external header evidence separately");
    require(
        sdk_ready.pipeline_handoff.sdk_vulkan_headers_available,
        "handoff records Vulkan header availability separately from native functions");
    require(
        sdk_ready.pipeline_handoff.sdk_vma_headers_available,
        "handoff records VMA header availability separately from native functions");
    require(
        sdk_ready.pipeline_handoff.sdk_external_headers.vulkan.header_version == 341,
        "handoff carries Vulkan header version evidence");

    const vulkan_backend::vulkan_backend_frame_result sdk_missing =
        vulkan_backend::apply_vulkan_sdk_capability_result_to_frame(
            result,
            make_missing_sdk_capabilities());
    require(!sdk_missing.completed(), "missing SDK summary prevents native-ready frame completion");
    require(
        sdk_missing.pipeline_handoff.sdk_native_path_checked,
        "handoff records missing SDK native-path check");
    require(!sdk_missing.pipeline_handoff.sdk_adapter_ready, "handoff records SDK adapter unavailable");
    require(
        sdk_missing.pipeline_handoff.sdk_native_path_status
            == vulkan_backend::vulkan_sdk_native_path_status::sdk_missing,
        "handoff records SDK-missing native-path status");
    require(
        sdk_missing.pipeline_handoff.sdk_capability_status
            == vulkan_backend::vulkan_sdk_capability_status::headers_unavailable,
        "handoff records SDK missing-header capability status");
    require(
        sdk_missing.pipeline_handoff.sdk_external_headers_checked,
        "handoff records checked missing external header evidence");
    require(
        !sdk_missing.pipeline_handoff.sdk_vulkan_headers_available,
        "handoff reports missing Vulkan headers separately from native function table");
    require(
        !sdk_missing.pipeline_handoff.sdk_vma_headers_available,
        "handoff reports missing VMA headers separately from native function table");
    require(
        sdk_missing.pipeline_handoff.sdk_diagnostic == "fake Vulkan SDK path missing",
        "handoff records SDK missing diagnostic");
}

void test_vulkan_frame_pipeline_handoff_reports_cpu_fallback_native_execution_summary()
{
    using namespace quiz_vulkan::render;

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
    const vulkan_backend::vulkan_backend_frame_result result =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    const vulkan_backend::vulkan_backend_frame_native_execution_summary& native_execution =
        result.pipeline_handoff.native_frame_execution;
    require(result.completed(), "baseline frame still completes through diagnostic backend");
    require(native_execution.checked, "handoff checks native frame execution summary");
    require(native_execution.operation.checked, "native execution summary contains operation summary");
    require(native_execution.diff.checked, "native execution summary contains diff diagnostics");
    require(native_execution.plan.checked, "native execution summary contains execution plan");
    require(
        native_execution.operation.status
            == vulkan_backend::vulkan_native_frame_operation_status::native_function_table_unavailable,
        "native execution summary reports missing native function table");
    require(
        native_execution.operation.blocker_stage
            == vulkan_backend::vulkan_native_frame_operation_stage::native_function_table,
        "native execution summary records function table blocker");
    require(native_execution.should_use_cpu_fallback(), "native execution summary selects CPU fallback");
    require(native_execution.should_skip_native_frame(), "native execution summary skips remaining native steps");
    require(
        native_execution.acquire_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::fallback,
        "native execution summary falls back at acquire without native functions");
    require(
        native_execution.record_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::skip,
        "native execution summary skips record after acquire fallback");
    require(
        native_execution.submit_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::skip,
        "native execution summary skips submit after acquire fallback");
    require(
        native_execution.present_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::skip,
        "native execution summary skips present after acquire fallback");
    require(!native_execution.native_acquire_would_execute, "native acquire does not execute");
    require(!native_execution.native_present_would_execute, "native present does not execute");
}

void test_vulkan_frame_pipeline_handoff_reports_native_execute_summary_when_ready()
{
    using namespace quiz_vulkan::render;

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
    vulkan_backend::vulkan_backend_frame_result result =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    mark_native_frame_execution_ready(result);
    result.pipeline_handoff = vulkan_backend::summarize_vulkan_frame_pipeline_handoff(result);

    const vulkan_backend::vulkan_backend_frame_native_execution_summary& native_execution =
        result.pipeline_handoff.native_frame_execution;
    require(result.completed(), "native-ready summary keeps frame completed");
    require(native_execution.operation.completed(), "native frame operation summary completes");
    require(native_execution.should_execute_native_frame(), "native execution summary executes frame");
    require(!native_execution.should_use_cpu_fallback(), "native-ready execution does not fallback");
    require(!native_execution.should_skip_native_frame(), "native-ready execution does not skip");
    require(
        native_execution.acquire_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::execute,
        "native execution summary executes acquire");
    require(
        native_execution.record_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::execute,
        "native execution summary executes record");
    require(
        native_execution.submit_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::execute,
        "native execution summary executes submit");
    require(
        native_execution.present_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::execute,
        "native execution summary executes present");
    require(native_execution.native_acquire_would_execute, "native acquire would execute");
    require(native_execution.native_record_would_execute, "native record would execute");
    require(native_execution.native_submit_would_execute, "native submit would execute");
    require(native_execution.native_present_would_execute, "native present would execute");
}

void test_vulkan_frame_pipeline_handoff_reports_native_submit_fallback_summary()
{
    using namespace quiz_vulkan::render;

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
    device.submit_succeeds = false;
    vulkan_backend::vulkan_backend_frame_result result =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    mark_native_frame_execution_ready(result);
    result.pipeline_handoff = vulkan_backend::summarize_vulkan_frame_pipeline_handoff(result);

    const vulkan_backend::vulkan_backend_frame_native_execution_summary& native_execution =
        result.pipeline_handoff.native_frame_execution;
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "frame result records submit fallback");
    require(native_execution.should_use_cpu_fallback(), "native submit failure uses CPU fallback");
    require(native_execution.should_skip_native_frame(), "native submit failure skips trailing steps");
    require(
        native_execution.acquire_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::execute,
        "submit fallback summary executes acquire");
    require(
        native_execution.record_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::execute,
        "submit fallback summary executes record");
    require(
        native_execution.submit_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::fallback,
        "submit fallback summary falls back at submit");
    require(
        native_execution.present_decision
            == vulkan_backend::vulkan_native_frame_execution_decision::skip,
        "submit fallback summary skips present after submit fallback");
    require(native_execution.native_acquire_would_execute, "submit fallback summary keeps acquire native");
    require(native_execution.native_record_would_execute, "submit fallback summary keeps record native");
    require(!native_execution.native_submit_would_execute, "submit fallback summary does not execute submit");
    require(!native_execution.native_present_would_execute, "submit fallback summary does not execute present");
}

void test_vulkan_frame_pipeline_handoff_reports_scoped_command_packet_summary()
{
    using namespace quiz_vulkan::render;

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
    const vulkan_backend::vulkan_backend_frame_result frame =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(frame.completed(), "baseline submitted frame completes before scoped packet summary");
    require(
        !frame.pipeline_handoff.scoped_command_packets.checked,
        "baseline handoff leaves scoped packet summary unchecked");

    const vulkan_backend::vulkan_backend_frame_result scoped_frame =
        vulkan_backend::apply_vulkan_scoped_command_packet_execution_result_to_frame(
            frame,
            execute_scoped_packets_for_frame(frame));
    const vulkan_backend::vulkan_backend_frame_scoped_command_packet_summary& summary =
        scoped_frame.pipeline_handoff.scoped_command_packets;

    require(scoped_frame.completed(), "scoped packet summary keeps submitted frame completed");
    require(scoped_frame.scoped_command_packet_execution.completed(), "frame owns scoped packet execution result");
    require(summary.checked, "handoff checks scoped packet execution");
    require(summary.completed(), "handoff records completed scoped packet execution");
    require(
        summary.status == vulkan_backend::vulkan_scoped_command_packet_execution_status::completed,
        "handoff records scoped packet execution status");
    require(
        summary.packets_executed_inside_render_pass_scope,
        "handoff reports packets executed inside selected render pass scope");
    require(!summary.scoped_execution_empty, "handoff records non-empty scoped execution");
    require(summary.render_pass_scope_ready, "handoff records ready render pass scope");
    require(summary.command_buffer_ready, "handoff records scoped command buffer readiness");
    require(summary.packet_bridge_ready, "handoff records packet bridge readiness");
    require(summary.packet_execution_ready, "handoff records packet execution readiness");
    require(summary.operation_plan_ready, "handoff records operation plan readiness");
    require(summary.render_pass_begin_completed, "handoff records render pass begin completion");
    require(summary.render_pass_end_completed, "handoff records render pass end completion");
    require(!summary.render_pass_end_skipped, "handoff records render pass end was not skipped");
    require(
        summary.render_pass_scope_id == device.next_image_id.value,
        "handoff records selected render pass scope id");
    require(
        summary.selected_framebuffer_target_index == device.next_image_id.value - 1,
        "handoff records selected framebuffer target index");
    require(
        summary.image_id.value == device.next_image_id.value,
        "handoff records selected swapchain image id");
    require(summary.framebuffer.valid(), "handoff records selected framebuffer handle");
    require(summary.command_buffer.valid(), "handoff records scoped command buffer handle");
    require(summary.planned_packet_count == 1, "handoff records one planned scoped packet");
    require(summary.attempted_packet_count == 1, "handoff records one attempted scoped packet");
    require(summary.executed_packet_count == 1, "handoff records one executed scoped packet");
    require(summary.rect_packet_count == 1, "handoff records rect packet count");
    require(summary.text_packet_count == 0, "handoff records no text packets");
    require(summary.image_packet_count == 0, "handoff records no image packets");
    require(summary.debug_bounds_packet_count == 0, "handoff records no debug packets");
    require(summary.diagnostic == "Scoped Vulkan command packet execution completed",
        "handoff supplies scoped packet execution diagnostic");
}

void test_vulkan_frame_pipeline_handoff_reports_scoped_packet_failure()
{
    using namespace quiz_vulkan::render;

    fake_vulkan_backend_device device(vulkan_backend::vulkan_surface_extent{.width = 64, .height = 64});
    const vulkan_backend::vulkan_backend_frame_result frame =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            make_quad_draw_list(),
            render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    const vulkan_backend::vulkan_backend_frame_result scoped_frame =
        vulkan_backend::apply_vulkan_scoped_command_packet_execution_result_to_frame(
            frame,
            execute_scoped_packets_for_frame(
                frame,
                vulkan_backend::fake_vulkan_command_packet_executor_options{
                    .fail_packet = true,
                    .fail_packet_index = 0,
                }));
    const vulkan_backend::vulkan_backend_frame_scoped_command_packet_summary& summary =
        scoped_frame.pipeline_handoff.scoped_command_packets;

    require(!scoped_frame.completed(), "failed scoped packet execution prevents frame completion summary");
    require(!scoped_frame.pipeline_handoff.completed(), "handoff does not complete with scoped packet failure");
    require(summary.checked, "handoff checks failed scoped packet execution");
    require(summary.failed(), "handoff records failed scoped packet execution");
    require(
        summary.status == vulkan_backend::vulkan_scoped_command_packet_execution_status::packet_failed,
        "handoff records scoped packet failure status");
    require(
        !summary.packets_executed_inside_render_pass_scope,
        "handoff does not report scoped packet execution after failure");
    require(summary.has_failed_packet, "handoff records failed scoped packet evidence");
    require(summary.first_failed_packet_index == 0, "handoff records first failed packet index");
    require(summary.first_failed_command_index == 0, "handoff records first failed command index");
    require(summary.attempted_packet_count == 1, "handoff records attempted failing packet");
    require(summary.executed_packet_count == 0, "handoff records no executed packets after failure");
    require(!scoped_frame.pipeline_handoff.command_recording_ready, "handoff blocks command recording readiness");
}

} // namespace

int main()
{
    test_vulkan_frame_pipeline_handoff_status_names_are_stable();
    test_vulkan_frame_pipeline_handoff_maps_lifecycle_prerequisites();
    test_vulkan_frame_pipeline_handoff_maps_surface_and_frame_lifecycle_failures();
    test_vulkan_frame_pipeline_handoff_requires_pipeline_before_resource_binding();
    test_vulkan_frame_pipeline_handoff_requires_resource_binding_before_command_recording();
    test_vulkan_frame_pipeline_handoff_accepts_draw_list_render_data();
    test_vulkan_frame_pipeline_handoff_reports_swapchain_recreate_policy();
    test_vulkan_frame_pipeline_handoff_reports_sdk_native_path_summary();
    test_vulkan_frame_pipeline_handoff_reports_cpu_fallback_native_execution_summary();
    test_vulkan_frame_pipeline_handoff_reports_native_execute_summary_when_ready();
    test_vulkan_frame_pipeline_handoff_reports_native_submit_fallback_summary();
    test_vulkan_frame_pipeline_handoff_reports_scoped_command_packet_summary();
    test_vulkan_frame_pipeline_handoff_reports_scoped_packet_failure();
    return 0;
}
