#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_render_pass.h"
#include "render/vulkan/vulkan_backend_swapchain.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_state make_ready_loader()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_loader_readiness_state{
        .checked = true,
        .status = vulkan_backend::vulkan_loader_readiness_status::ready,
        .probe_status = vulkan_backend::vulkan_loader_probe_status::available,
        .loader_library_available = true,
        .instance_proc_address_available = true,
        .instance_ready = true,
        .loaded_library_name = "fake-vulkan-loader",
        .required_symbol_name = std::string{vulkan_backend::vulkan_loader_required_symbol_name()},
        .attempted_library_count = 1,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_instance_create_result make_created_instance()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_instance_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_instance_create_status::created,
        .loader = make_ready_loader(),
        .handle = vulkan_backend::vulkan_instance_handle{.value = 70},
        .selected_extensions = {"VK_KHR_surface"},
        .enabled_layers = {},
        .diagnostic = "Vulkan instance created",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_device_create_result make_ready_device()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_device_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_device_create_status::created,
        .instance = make_created_instance(),
        .handle = vulkan_backend::vulkan_device_handle{.value = 80},
        .selected_extensions = {"VK_KHR_swapchain"},
        .selected_queues = {
            vulkan_backend::vulkan_device_queue_selection{
                .capability = vulkan_backend::vulkan_device_queue_capability::graphics,
                .family_index = 0,
                .queue = vulkan_backend::vulkan_queue_handle{.value = 100},
            },
            vulkan_backend::vulkan_device_queue_selection{
                .capability = vulkan_backend::vulkan_device_queue_capability::present,
                .family_index = 1,
                .queue = vulkan_backend::vulkan_queue_handle{.value = 101},
            },
        },
        .diagnostic = "Vulkan device created",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_surface_extent make_extent()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{
        .width = 1280,
        .height = 720,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_result make_ready_swapchain()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_swapchain_create_status::created,
        .device = make_ready_device(),
        .handle = vulkan_backend::vulkan_swapchain_handle{.value = 90},
        .requested_extent = make_extent(),
        .selected_extent = make_extent(),
        .requested_present_mode = vulkan_backend::vulkan_swapchain_present_mode::fifo,
        .selected_present_mode = vulkan_backend::vulkan_swapchain_present_mode::fifo,
        .requested_present_mode_supported = true,
        .fallback_to_fifo = false,
        .min_image_count = 3,
        .diagnostic = "Vulkan swapchain created",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_result make_unavailable_swapchain()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_swapchain_create_status::invalid_surface_extent,
        .device = make_ready_device(),
        .handle = {},
        .requested_extent = vulkan_backend::vulkan_surface_extent{.width = 0, .height = 720},
        .selected_extent = {},
        .requested_present_mode = vulkan_backend::vulkan_swapchain_present_mode::fifo,
        .selected_present_mode = vulkan_backend::vulkan_swapchain_present_mode::fifo,
        .requested_present_mode_supported = true,
        .fallback_to_fifo = false,
        .min_image_count = 3,
        .diagnostic = "Vulkan swapchain surface extent is invalid",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_render_pass_create_request make_render_pass_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_render_pass_create_request{
        .framebuffer_extent = make_extent(),
        .attachments = {
            vulkan_backend::vulkan_render_pass_attachment_request{
                .role = vulkan_backend::vulkan_render_pass_attachment_role::color,
                .format = vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
            },
        },
    };
}

quiz_vulkan::render::vulkan_backend::fake_vulkan_render_pass_factory make_render_pass_factory()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::fake_vulkan_render_pass_factory(
        vulkan_backend::fake_vulkan_render_pass_factory_options{
            .supported_formats = {
                vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
                vulkan_backend::vulkan_render_pass_attachment_format::rgba8_unorm,
            },
            .fail_creation = false,
            .render_pass = vulkan_backend::vulkan_render_pass_handle{.value = 300},
            .framebuffer = vulkan_backend::vulkan_framebuffer_handle{.value = 400},
        });
}

void test_render_pass_factory_marks_ready_swapchain_render_pass_ready()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_render_pass_factory factory = make_render_pass_factory();
    const vulkan_backend::vulkan_render_pass_create_result result =
        vulkan_backend::create_vulkan_render_pass(
            factory,
            make_ready_swapchain(),
            make_render_pass_request());

    require(result.checked, "render pass result records that creation was checked");
    require(
        result.status == vulkan_backend::vulkan_render_pass_create_status::created,
        "render pass result reports created status");
    require(result.ready_for_pipeline(), "created render pass reaches the pipeline gate");
    require(result.swapchain.ready_for_frame(), "created render pass preserves ready swapchain");
    require(result.render_pass.valid(), "created render pass returns a valid opaque handle");
    require(result.framebuffer.valid(), "created render pass returns a valid framebuffer handle");
    require(result.render_pass.value == 300, "created render pass returns configured render pass handle");
    require(result.framebuffer.value == 400, "created render pass returns configured framebuffer handle");
    require(result.selected_extent.width == 1280, "created render pass records selected width");
    require(result.selected_extent.height == 720, "created render pass records selected height");
    require(result.selected_attachments.size() == 1, "created render pass records selected attachment");
    require(
        result.selected_attachments[0].format
            == vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
        "created render pass records selected color format");

    vulkan_backend::null_vulkan_backend_device device(result);
    const vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle =
        device.current_lifecycle_readiness();
    require(lifecycle.swapchain_ready, "render pass lifecycle keeps swapchain ready");
    require(lifecycle.render_pass_ready, "render pass lifecycle marks render pass ready");
    require(lifecycle.effective_render_pass_ready(), "render pass lifecycle effective readiness is true");
    require(device.current_surface_extent().width == 1280, "null backend keeps swapchain width");
    require(device.current_surface_extent().height == 720, "null backend keeps swapchain height");

    const quiz_vulkan::render::render_draw_list draw_list;
    const vulkan_backend::vulkan_backend_frame_result frame =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(
        frame.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        "ready render pass moves fallback to the pipeline gate");
}

void test_render_pass_factory_keeps_swapchain_failure_at_swapchain_gate()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_render_pass_factory factory = make_render_pass_factory();
    const vulkan_backend::vulkan_render_pass_create_result result =
        vulkan_backend::create_vulkan_render_pass(
            factory,
            make_unavailable_swapchain(),
            make_render_pass_request());

    require(result.checked, "swapchain-unavailable render pass result is checked");
    require(
        result.status == vulkan_backend::vulkan_render_pass_create_status::swapchain_unavailable,
        "swapchain-unavailable render pass result reports swapchain unavailable");
    require(!result.ready_for_pipeline(), "swapchain-unavailable render pass does not reach pipeline gate");
    require(!result.render_pass.valid(), "swapchain-unavailable render pass has no render pass handle");
    require(!result.framebuffer.valid(), "swapchain-unavailable render pass has no framebuffer handle");
    require(
        result.diagnostic == "Vulkan swapchain is not ready for render pass creation",
        "swapchain-unavailable render pass records diagnostic");

    vulkan_backend::null_vulkan_backend_device device(result);
    const quiz_vulkan::render::render_draw_list draw_list;
    const vulkan_backend::vulkan_backend_frame_result frame =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(
        frame.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::swapchain_unavailable,
        "swapchain-unavailable render pass stays at the swapchain fallback gate");
}

void test_render_pass_factory_reports_invalid_extent_and_format()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_render_pass_factory factory = make_render_pass_factory();
    const vulkan_backend::vulkan_render_pass_create_result invalid_extent =
        vulkan_backend::create_vulkan_render_pass(
            factory,
            make_ready_swapchain(),
            vulkan_backend::vulkan_render_pass_create_request{
                .framebuffer_extent = vulkan_backend::vulkan_surface_extent{.width = 0, .height = 720},
                .attachments = make_render_pass_request().attachments,
            });
    require(invalid_extent.checked, "invalid-extent render pass result is checked");
    require(
        invalid_extent.status == vulkan_backend::vulkan_render_pass_create_status::invalid_extent,
        "invalid-extent render pass result reports invalid extent");
    require(!invalid_extent.ready_for_pipeline(), "invalid-extent render pass does not reach pipeline gate");
    require(
        invalid_extent.diagnostic == "Vulkan framebuffer extent is invalid",
        "invalid-extent render pass records diagnostic");

    const vulkan_backend::vulkan_render_pass_create_result invalid_format =
        vulkan_backend::create_vulkan_render_pass(
            factory,
            make_ready_swapchain(),
            vulkan_backend::vulkan_render_pass_create_request{
                .framebuffer_extent = make_extent(),
                .attachments = {
                    vulkan_backend::vulkan_render_pass_attachment_request{
                        .role = vulkan_backend::vulkan_render_pass_attachment_role::color,
                        .format = vulkan_backend::vulkan_render_pass_attachment_format::undefined,
                    },
                },
            });
    require(invalid_format.checked, "invalid-format render pass result is checked");
    require(
        invalid_format.status == vulkan_backend::vulkan_render_pass_create_status::invalid_format,
        "invalid-format render pass result reports invalid format");
    require(!invalid_format.ready_for_pipeline(), "invalid-format render pass does not reach pipeline gate");
    require(
        invalid_format.diagnostic == "Vulkan render pass attachment format is invalid: undefined",
        "invalid-format render pass records diagnostic");
}

void test_render_pass_factory_reports_missing_attachments()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_render_pass_factory factory = make_render_pass_factory();
    const vulkan_backend::vulkan_render_pass_create_result result =
        vulkan_backend::create_vulkan_render_pass(
            factory,
            make_ready_swapchain(),
            vulkan_backend::vulkan_render_pass_create_request{
                .framebuffer_extent = make_extent(),
                .attachments = {},
            });

    require(result.checked, "missing-attachment render pass result is checked");
    require(
        result.status == vulkan_backend::vulkan_render_pass_create_status::missing_attachments,
        "missing-attachment render pass result reports missing attachments");
    require(!result.ready_for_pipeline(), "missing-attachment render pass does not reach pipeline gate");
    require(
        result.diagnostic == "Vulkan render pass is missing a color attachment",
        "missing-attachment render pass records diagnostic");
}

void test_render_pass_factory_reports_fake_creation_failure()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_render_pass_factory factory(
        vulkan_backend::fake_vulkan_render_pass_factory_options{
            .supported_formats = {
                vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
            },
            .fail_creation = true,
            .render_pass = vulkan_backend::vulkan_render_pass_handle{.value = 301},
            .framebuffer = vulkan_backend::vulkan_framebuffer_handle{.value = 401},
        });
    const vulkan_backend::vulkan_render_pass_create_result result =
        vulkan_backend::create_vulkan_render_pass(
            factory,
            make_ready_swapchain(),
            make_render_pass_request());

    require(result.checked, "creation-failure render pass result is checked");
    require(
        result.status == vulkan_backend::vulkan_render_pass_create_status::creation_failed,
        "creation-failure render pass result reports creation failed");
    require(!result.ready_for_pipeline(), "creation-failure render pass does not reach pipeline gate");
    require(
        result.diagnostic == "Vulkan render pass/framebuffer creation failed",
        "creation-failure render pass records diagnostic");
}

void test_lifecycle_fallback_moves_to_render_pass_only_after_swapchain_ready()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_render_pass_factory factory = make_render_pass_factory();
    const vulkan_backend::vulkan_render_pass_create_result result =
        vulkan_backend::create_vulkan_render_pass(
            factory,
            make_ready_swapchain(),
            vulkan_backend::vulkan_render_pass_create_request{
                .framebuffer_extent = make_extent(),
                .attachments = {
                    vulkan_backend::vulkan_render_pass_attachment_request{
                        .role = vulkan_backend::vulkan_render_pass_attachment_role::color,
                        .format = vulkan_backend::vulkan_render_pass_attachment_format::undefined,
                    },
                },
            });

    vulkan_backend::null_vulkan_backend_device device(result);
    const quiz_vulkan::render::render_draw_list draw_list;
    const vulkan_backend::vulkan_backend_frame_result frame =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});
    require(frame.lifecycle.swapchain_ready, "render-pass fallback keeps swapchain ready");
    require(!frame.lifecycle.render_pass_ready, "render-pass fallback keeps render pass unavailable");
    require(
        frame.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::render_pass_unavailable,
        "render-pass failure moves fallback to render pass gate after swapchain is ready");
}

void test_vulkan_render_pass_create_status_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::render_pass_create_status_name(
            vulkan_backend::vulkan_render_pass_create_status::not_requested)
            == std::string_view{"not_requested"},
        "render pass create status name for not requested is stable");
    require(
        vulkan_backend::render_pass_create_status_name(
            vulkan_backend::vulkan_render_pass_create_status::created)
            == std::string_view{"created"},
        "render pass create status name for created is stable");
    require(
        vulkan_backend::render_pass_create_status_name(
            vulkan_backend::vulkan_render_pass_create_status::swapchain_unavailable)
            == std::string_view{"swapchain_unavailable"},
        "render pass create status name for swapchain unavailable is stable");
    require(
        vulkan_backend::render_pass_create_status_name(
            vulkan_backend::vulkan_render_pass_create_status::invalid_extent)
            == std::string_view{"invalid_extent"},
        "render pass create status name for invalid extent is stable");
    require(
        vulkan_backend::render_pass_create_status_name(
            vulkan_backend::vulkan_render_pass_create_status::invalid_format)
            == std::string_view{"invalid_format"},
        "render pass create status name for invalid format is stable");
    require(
        vulkan_backend::render_pass_create_status_name(
            vulkan_backend::vulkan_render_pass_create_status::missing_attachments)
            == std::string_view{"missing_attachments"},
        "render pass create status name for missing attachments is stable");
    require(
        vulkan_backend::render_pass_create_status_name(
            vulkan_backend::vulkan_render_pass_create_status::creation_failed)
            == std::string_view{"creation_failed"},
        "render pass create status name for creation failed is stable");
    require(
        vulkan_backend::fallback_reason_name(
            vulkan_backend::vulkan_backend_fallback_reason::render_pass_unavailable)
            == std::string_view{"render_pass_unavailable"},
        "render pass fallback reason name is stable");
}

} // namespace

int main()
{
    test_render_pass_factory_marks_ready_swapchain_render_pass_ready();
    test_render_pass_factory_keeps_swapchain_failure_at_swapchain_gate();
    test_render_pass_factory_reports_invalid_extent_and_format();
    test_render_pass_factory_reports_missing_attachments();
    test_render_pass_factory_reports_fake_creation_failure();
    test_lifecycle_fallback_moves_to_render_pass_only_after_swapchain_ready();
    test_vulkan_render_pass_create_status_names_are_stable();
    return 0;
}
