#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_render_pass.h"
#include "render/vulkan/vulkan_backend_swapchain.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

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

quiz_vulkan::render::vulkan_backend::vulkan_native_function_table_diagnostics
make_framebuffer_function_table(const std::vector<std::string>& missing_symbols = {})
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_symbol_resolver_options{
            .missing_symbols = missing_symbols,
            .pointer_base = vulkan_backend::vulkan_native_function_pointer{.value = 980},
        });
    return vulkan_backend::collect_vulkan_native_function_table(
        resolver,
        make_ready_loader(),
        vulkan_backend::vulkan_native_function_table_request{
            .symbols = vulkan_backend::default_vulkan_native_framebuffer_entrypoints(),
            .include_default_backend_entrypoints = false,
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_native_framebuffer_dispatch_table
make_ready_native_framebuffer_dispatch_table()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::collect_vulkan_native_framebuffer_dispatch_table(
        make_framebuffer_function_table());
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_image_view_target
make_ready_image_view_target(std::size_t image_index)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_swapchain_image_view_target{
        .image_index = image_index,
        .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = image_index + 1},
        .image_handle =
            vulkan_backend::vulkan_swapchain_image_handle{.value = 7000 + image_index + 1},
        .selected_image_format = vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm,
        .extent = make_extent(),
        .usage_intent =
            vulkan_backend::vulkan_swapchain_image_view_usage_intent::color_attachment,
        .aspect_intent = vulkan_backend::vulkan_swapchain_image_view_aspect_intent::color,
        .image_view =
            vulkan_backend::vulkan_swapchain_image_view_handle{.value = 8000 + image_index + 1},
        .lifecycle_status =
            vulkan_backend::vulkan_native_swapchain_image_view_target_lifecycle_status::ready,
        .image_ready = true,
        .image_view_ready = true,
        .render_target_ready = true,
        .vk_create_image_view_called = true,
        .vk_destroy_image_view_called = false,
        .native_result = 0,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_image_view_targets_execution_result
make_ready_image_view_targets()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_swapchain_image_view_targets_execution_result{
        .checked = true,
        .status =
            vulkan_backend::vulkan_native_swapchain_image_view_targets_execution_status::ready,
        .images_execution = {},
        .dispatch_table = {},
        .device = vulkan_backend::vulkan_device_handle{.value = 80},
        .swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 90},
        .extent = make_extent(),
        .selected_image_format = vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm,
        .usage_intent =
            vulkan_backend::vulkan_swapchain_image_view_usage_intent::color_attachment,
        .aspect_intent = vulkan_backend::vulkan_swapchain_image_view_aspect_intent::color,
        .image_count = 3,
        .ready_image_view_count = 3,
        .targets = {
            make_ready_image_view_target(0),
            make_ready_image_view_target(1),
            make_ready_image_view_target(2),
        },
        .create_image_view_symbol = vulkan_backend::vulkan_native_function_pointer{.value = 951},
        .destroy_image_view_symbol = vulkan_backend::vulkan_native_function_pointer{.value = 952},
        .images_execution_ready = true,
        .dispatch_table_ready = true,
        .vk_create_image_view_called = true,
        .native_result = 0,
        .diagnostic = "Native Vulkan image view targets are ready",
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

void test_native_framebuffer_dispatch_table_resolves_create_destroy_symbols()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_framebuffer_dispatch_table dispatch =
        make_ready_native_framebuffer_dispatch_table();
    require(dispatch.checked, "native framebuffer dispatch table is checked");
    require(
        dispatch.status == vulkan_backend::vulkan_native_framebuffer_dispatch_table_status::ready,
        "native framebuffer dispatch table reports ready");
    require(dispatch.ready_for_create(), "native framebuffer dispatch reaches create gate");
    require(dispatch.ready_for_destroy(), "native framebuffer dispatch reaches destroy gate");
    require(dispatch.create_framebuffer.value == 981, "dispatch records create framebuffer pointer");
    require(dispatch.destroy_framebuffer.value == 982, "dispatch records destroy framebuffer pointer");

    const vulkan_backend::vulkan_native_framebuffer_dispatch_table missing_create =
        vulkan_backend::collect_vulkan_native_framebuffer_dispatch_table(
            make_framebuffer_function_table({"vkCreateFramebuffer"}));
    require(
        missing_create.status
            == vulkan_backend::vulkan_native_framebuffer_dispatch_table_status::missing_create_symbol,
        "native framebuffer dispatch reports missing create symbol");
    require(
        missing_create.missing_symbol_name == "vkCreateFramebuffer",
        "native framebuffer dispatch records missing create symbol");

    const vulkan_backend::vulkan_native_framebuffer_dispatch_table missing_destroy =
        vulkan_backend::collect_vulkan_native_framebuffer_dispatch_table(
            make_framebuffer_function_table({"vkDestroyFramebuffer"}));
    require(
        missing_destroy.status
            == vulkan_backend::vulkan_native_framebuffer_dispatch_table_status::missing_destroy_symbol,
        "native framebuffer dispatch reports missing destroy symbol");
    require(
        missing_destroy.missing_symbol_name == "vkDestroyFramebuffer",
        "native framebuffer dispatch records missing destroy symbol");
}

void test_native_framebuffer_targets_create_and_destroy_fake_handles()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_render_pass_factory render_pass_factory = make_render_pass_factory();
    const vulkan_backend::vulkan_render_pass_create_result render_pass =
        vulkan_backend::create_vulkan_render_pass(
            render_pass_factory,
            make_ready_swapchain(),
            make_render_pass_request());
    const vulkan_backend::vulkan_native_framebuffer_dispatch_table dispatch =
        make_ready_native_framebuffer_dispatch_table();
    vulkan_backend::fake_vulkan_native_framebuffer_operation operation(
        vulkan_backend::fake_vulkan_native_framebuffer_operation_options{
            .framebuffer_handle_base =
                vulkan_backend::vulkan_native_function_pointer{.value = 13000},
        });

    const vulkan_backend::vulkan_native_framebuffer_targets_execution_result targets =
        vulkan_backend::create_native_vulkan_framebuffer_targets(
            operation,
            vulkan_backend::vulkan_native_framebuffer_targets_execution_request{
                .image_view_targets = make_ready_image_view_targets(),
                .render_pass = render_pass,
                .dispatch_table = dispatch,
                .intent = vulkan_backend::vulkan_native_framebuffer_target_intent{
                    .attachments = make_render_pass_request().attachments,
                    .layers = 1,
                },
            });

    require(targets.checked, "native framebuffer targets are checked");
    require(
        targets.status == vulkan_backend::vulkan_native_framebuffer_targets_execution_status::ready,
        "native framebuffer targets report ready");
    require(
        targets.ready_for_command_recording(),
        "native framebuffer targets reach command recording gate");
    require(!targets.blocked(), "ready native framebuffer targets are not blocked");
    require(targets.device.value == 80, "native framebuffer targets preserve device handle");
    require(targets.render_pass_handle.value == 300, "native framebuffer targets preserve render pass handle");
    require(targets.extent.width == 1280, "native framebuffer targets preserve selected width");
    require(targets.extent.height == 720, "native framebuffer targets preserve selected height");
    require(targets.layers == 1, "native framebuffer targets preserve layer count");
    require(targets.attachments.size() == 1, "native framebuffer targets preserve attachment intent");
    require(targets.target_count == 3, "native framebuffer targets preserve target count");
    require(targets.ready_framebuffer_count == 3, "native framebuffer targets record ready count");
    require(targets.targets.size() == 3, "native framebuffer targets store per-image records");
    require(targets.targets[0].image_id.value == 1, "first framebuffer target records image id");
    require(targets.targets[0].image_view.value == 8001, "first framebuffer target records image view handle");
    require(targets.targets[0].framebuffer.value == 13001, "first framebuffer target records framebuffer handle");
    require(targets.targets[0].layers == 1, "first framebuffer target records layer count");
    require(
        targets.targets[0].attachments[0].format
            == vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
        "first framebuffer target records attachment format");
    require(
        targets.targets[0].lifecycle_status
            == vulkan_backend::vulkan_native_framebuffer_target_lifecycle_status::ready,
        "first framebuffer target records ready lifecycle");
    require(targets.targets[0].ready(), "first framebuffer target is ready");
    require(
        operation.state().create_call_count == 1,
        "fake framebuffer creation was called once");
    require(
        operation.state().last_create_framebuffer_symbol.value == 981,
        "fake framebuffer creation records create pointer");
    require(
        operation.state().created_targets.size() == 3,
        "fake framebuffer creation records created targets");

    const vulkan_backend::vulkan_native_framebuffer_targets_destroy_result destroyed =
        vulkan_backend::destroy_native_vulkan_framebuffer_targets(
            operation,
            vulkan_backend::vulkan_native_framebuffer_targets_destroy_request{
                .targets = targets,
                .dispatch_table = dispatch,
            });
    require(destroyed.checked, "native framebuffer destroy is checked");
    require(
        destroyed.status == vulkan_backend::vulkan_native_framebuffer_targets_destroy_status::destroyed,
        "native framebuffer destroy reports destroyed");
    require(destroyed.destroyed(), "native framebuffer destroy reaches destroyed gate");
    require(destroyed.requested_framebuffer_count == 3, "destroy records requested count");
    require(destroyed.destroyed_framebuffer_count == 3, "destroy records destroyed count");
    require(
        operation.state().destroy_call_count == 1,
        "fake framebuffer destroy was called once");
    require(
        operation.state().destroyed_framebuffers[0].value == 13001,
        "fake framebuffer destroy records first destroyed framebuffer");
    require(
        destroyed.targets.targets[0].lifecycle_status
            == vulkan_backend::vulkan_native_framebuffer_target_lifecycle_status::destroyed,
        "fake framebuffer destroy marks first target destroyed");
    require(
        destroyed.targets.targets[0].vk_destroy_framebuffer_called,
        "fake framebuffer destroy records per-target destroy call");
}

void test_native_framebuffer_targets_report_blockers_and_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_render_pass_factory render_pass_factory = make_render_pass_factory();
    const vulkan_backend::vulkan_render_pass_create_result render_pass =
        vulkan_backend::create_vulkan_render_pass(
            render_pass_factory,
            make_ready_swapchain(),
            make_render_pass_request());
    const vulkan_backend::vulkan_native_framebuffer_dispatch_table dispatch =
        make_ready_native_framebuffer_dispatch_table();
    vulkan_backend::fake_vulkan_native_framebuffer_operation operation;

    const vulkan_backend::vulkan_native_framebuffer_targets_execution_result missing_views =
        vulkan_backend::create_native_vulkan_framebuffer_targets(
            operation,
            vulkan_backend::vulkan_native_framebuffer_targets_execution_request{
                .image_view_targets = {},
                .render_pass = render_pass,
                .dispatch_table = dispatch,
            });
    require(
        missing_views.status
            == vulkan_backend::vulkan_native_framebuffer_targets_execution_status::image_view_targets_unavailable,
        "framebuffer targets block missing image view target stage");

    const vulkan_backend::vulkan_native_framebuffer_dispatch_table missing_dispatch =
        vulkan_backend::collect_vulkan_native_framebuffer_dispatch_table(
            make_framebuffer_function_table({"vkCreateFramebuffer"}));
    const vulkan_backend::vulkan_native_framebuffer_targets_execution_result dispatch_blocked =
        vulkan_backend::create_native_vulkan_framebuffer_targets(
            operation,
            vulkan_backend::vulkan_native_framebuffer_targets_execution_request{
                .image_view_targets = make_ready_image_view_targets(),
                .render_pass = render_pass,
                .dispatch_table = missing_dispatch,
            });
    require(
        dispatch_blocked.status
            == vulkan_backend::vulkan_native_framebuffer_targets_execution_status::dispatch_table_unavailable,
        "framebuffer targets block missing dispatch symbols");

    vulkan_backend::vulkan_native_swapchain_image_view_targets_execution_result invalid_device =
        make_ready_image_view_targets();
    invalid_device.device = {};
    const vulkan_backend::vulkan_native_framebuffer_targets_execution_result invalid_device_result =
        vulkan_backend::create_native_vulkan_framebuffer_targets(
            operation,
            vulkan_backend::vulkan_native_framebuffer_targets_execution_request{
                .image_view_targets = invalid_device,
                .render_pass = render_pass,
                .dispatch_table = dispatch,
            });
    require(
        invalid_device_result.status
            == vulkan_backend::vulkan_native_framebuffer_targets_execution_status::invalid_device,
        "framebuffer targets block invalid device");

    vulkan_backend::vulkan_render_pass_create_result missing_render_pass = render_pass;
    missing_render_pass.render_pass = {};
    const vulkan_backend::vulkan_native_framebuffer_targets_execution_result missing_render_pass_result =
        vulkan_backend::create_native_vulkan_framebuffer_targets(
            operation,
            vulkan_backend::vulkan_native_framebuffer_targets_execution_request{
                .image_view_targets = make_ready_image_view_targets(),
                .render_pass = missing_render_pass,
                .dispatch_table = dispatch,
            });
    require(
        missing_render_pass_result.status
            == vulkan_backend::vulkan_native_framebuffer_targets_execution_status::missing_render_pass,
        "framebuffer targets block missing render pass handle");

    vulkan_backend::vulkan_render_pass_create_result mismatched_extent = render_pass;
    mismatched_extent.selected_extent = vulkan_backend::vulkan_surface_extent{.width = 640, .height = 480};
    const vulkan_backend::vulkan_native_framebuffer_targets_execution_result extent_mismatch =
        vulkan_backend::create_native_vulkan_framebuffer_targets(
            operation,
            vulkan_backend::vulkan_native_framebuffer_targets_execution_request{
                .image_view_targets = make_ready_image_view_targets(),
                .render_pass = mismatched_extent,
                .dispatch_table = dispatch,
            });
    require(
        extent_mismatch.status
            == vulkan_backend::vulkan_native_framebuffer_targets_execution_status::extent_mismatch,
        "framebuffer targets block extent mismatch");

    vulkan_backend::vulkan_native_swapchain_image_view_targets_execution_result missing_image_view =
        make_ready_image_view_targets();
    missing_image_view.targets[0].image_view = {};
    missing_image_view.targets[0].image_view_ready = false;
    const vulkan_backend::vulkan_native_framebuffer_targets_execution_result missing_image_view_result =
        vulkan_backend::create_native_vulkan_framebuffer_targets(
            operation,
            vulkan_backend::vulkan_native_framebuffer_targets_execution_request{
                .image_view_targets = missing_image_view,
                .render_pass = render_pass,
                .dispatch_table = dispatch,
            });
    require(
        missing_image_view_result.status
            == vulkan_backend::vulkan_native_framebuffer_targets_execution_status::missing_image_view,
        "framebuffer targets block missing image view handle");

    vulkan_backend::fake_vulkan_native_framebuffer_operation failing_create(
        vulkan_backend::fake_vulkan_native_framebuffer_operation_options{
            .fail_create = true,
            .create_failure_result = -21,
        });
    const vulkan_backend::vulkan_native_framebuffer_targets_execution_result failed_create =
        vulkan_backend::create_native_vulkan_framebuffer_targets(
            failing_create,
            vulkan_backend::vulkan_native_framebuffer_targets_execution_request{
                .image_view_targets = make_ready_image_view_targets(),
                .render_pass = render_pass,
                .dispatch_table = dispatch,
            });
    require(
        failed_create.status
            == vulkan_backend::vulkan_native_framebuffer_targets_execution_status::create_failed,
        "framebuffer targets report fake create failure");
    require(failed_create.native_result == -21, "framebuffer targets record create failure result");

    const vulkan_backend::vulkan_native_framebuffer_targets_execution_result ready =
        vulkan_backend::create_native_vulkan_framebuffer_targets(
            operation,
            vulkan_backend::vulkan_native_framebuffer_targets_execution_request{
                .image_view_targets = make_ready_image_view_targets(),
                .render_pass = render_pass,
                .dispatch_table = dispatch,
            });
    vulkan_backend::fake_vulkan_native_framebuffer_operation failing_destroy(
        vulkan_backend::fake_vulkan_native_framebuffer_operation_options{
            .fail_destroy = true,
            .destroy_failure_result = -22,
        });
    const vulkan_backend::vulkan_native_framebuffer_targets_destroy_result failed_destroy =
        vulkan_backend::destroy_native_vulkan_framebuffer_targets(
            failing_destroy,
            vulkan_backend::vulkan_native_framebuffer_targets_destroy_request{
                .targets = ready,
                .dispatch_table = dispatch,
            });
    require(
        failed_destroy.status
            == vulkan_backend::vulkan_native_framebuffer_targets_destroy_status::destroy_failed,
        "framebuffer target destroy reports fake destroy failure");
    require(failed_destroy.native_result == -22, "framebuffer destroy records failure result");
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
    require(
        vulkan_backend::native_framebuffer_dispatch_table_status_name(
            vulkan_backend::vulkan_native_framebuffer_dispatch_table_status::missing_create_symbol)
            == std::string_view{"missing_create_symbol"},
        "native framebuffer dispatch status name for missing create is stable");
    require(
        vulkan_backend::native_framebuffer_target_lifecycle_status_name(
            vulkan_backend::vulkan_native_framebuffer_target_lifecycle_status::ready)
            == std::string_view{"ready"},
        "native framebuffer target lifecycle status name for ready is stable");
    require(
        vulkan_backend::native_framebuffer_targets_execution_status_name(
            vulkan_backend::vulkan_native_framebuffer_targets_execution_status::extent_mismatch)
            == std::string_view{"extent_mismatch"},
        "native framebuffer target execution status name for extent mismatch is stable");
    require(
        vulkan_backend::native_framebuffer_targets_destroy_status_name(
            vulkan_backend::vulkan_native_framebuffer_targets_destroy_status::destroyed)
            == std::string_view{"destroyed"},
        "native framebuffer target destroy status name for destroyed is stable");
}

} // namespace

int main()
{
    test_render_pass_factory_marks_ready_swapchain_render_pass_ready();
    test_render_pass_factory_keeps_swapchain_failure_at_swapchain_gate();
    test_render_pass_factory_reports_invalid_extent_and_format();
    test_render_pass_factory_reports_missing_attachments();
    test_render_pass_factory_reports_fake_creation_failure();
    test_native_framebuffer_dispatch_table_resolves_create_destroy_symbols();
    test_native_framebuffer_targets_create_and_destroy_fake_handles();
    test_native_framebuffer_targets_report_blockers_and_failures();
    test_lifecycle_fallback_moves_to_render_pass_only_after_swapchain_ready();
    test_vulkan_render_pass_create_status_names_are_stable();
    return 0;
}
