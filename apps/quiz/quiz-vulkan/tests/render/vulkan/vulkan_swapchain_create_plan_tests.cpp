#include "render/vulkan/vulkan_backend_swapchain.h"

#include <cassert>
#include <cstdio>
#include <string_view>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_surface_capabilities_snapshot
make_capabilities()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_surface_capabilities_snapshot{
        .checked = true,
        .min_image_count = 2,
        .max_image_count = 4,
        .current_extent = {},
        .min_extent = vulkan_backend::vulkan_surface_extent{.width = 640, .height = 480},
        .max_extent = vulkan_backend::vulkan_surface_extent{.width = 1920, .height = 1080},
        .supported_transforms = {
            vulkan_backend::vulkan_swapchain_surface_transform::identity,
            vulkan_backend::vulkan_swapchain_surface_transform::rotate_90,
        },
        .current_transform = vulkan_backend::vulkan_swapchain_surface_transform::rotate_90,
        .supported_composite_alpha = {
            vulkan_backend::vulkan_swapchain_composite_alpha::opaque,
            vulkan_backend::vulkan_swapchain_composite_alpha::pre_multiplied,
        },
        .surface_formats = {
            vulkan_backend::vulkan_swapchain_surface_format{
                .format = vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm,
                .color_space = vulkan_backend::vulkan_swapchain_color_space::srgb_nonlinear,
            },
            vulkan_backend::vulkan_swapchain_surface_format{
                .format = vulkan_backend::vulkan_swapchain_image_format::rgba8_unorm,
                .color_space = vulkan_backend::vulkan_swapchain_color_space::srgb_nonlinear,
            },
        },
        .present_modes = {
            vulkan_backend::vulkan_swapchain_present_mode::fifo,
            vulkan_backend::vulkan_swapchain_present_mode::mailbox,
        },
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_plan_intent
make_intent()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_swapchain_create_plan_intent{
        .desired_vsync = false,
        .desired_extent = vulkan_backend::vulkan_surface_extent{.width = 1280, .height = 720},
        .desired_image_count = 3,
        .desired_surface_format = vulkan_backend::vulkan_swapchain_surface_format{
            .format = vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm,
            .color_space = vulkan_backend::vulkan_swapchain_color_space::srgb_nonlinear,
        },
        .desired_transform = vulkan_backend::vulkan_swapchain_surface_transform::identity,
        .desired_composite_alpha = vulkan_backend::vulkan_swapchain_composite_alpha::opaque,
        .graphics_queue_family_available = true,
        .present_queue_family_available = true,
        .graphics_queue_family_index = 0,
        .present_queue_family_index = 1,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_create_plan_result
make_ready_create_plan()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::build_vulkan_swapchain_create_plan(
        vulkan_backend::vulkan_swapchain_create_plan_request{
            .capabilities = make_capabilities(),
            .intent = make_intent(),
            .recreate_reference = {},
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_entrypoint_readiness
make_ready_native_swapchain_entrypoints()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_swapchain_entrypoint_readiness{
        .checked = true,
        .function_table_checked = true,
        .required_extensions_ready = true,
        .create_swapchain_ready = true,
        .destroy_swapchain_ready = true,
        .get_swapchain_images_ready = true,
        .acquire_next_image_ready = true,
        .queue_present_ready = true,
        .function_table_status =
            vulkan_backend::vulkan_native_function_table_status::ready,
        .required_extension_count = 1,
        .available_required_extension_count = 1,
        .diagnostic = "native swapchain entrypoints ready",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_create_operation_request
make_ready_native_swapchain_create_operation_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_swapchain_create_operation_request{
        .create_plan = make_ready_create_plan(),
        .native_entrypoints = make_ready_native_swapchain_entrypoints(),
        .device = vulkan_backend::vulkan_device_handle{.value = 77},
        .surface = vulkan_backend::vulkan_surface_handle{.value = 88},
        .old_swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 99},
        .require_recreate_compatibility = false,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_create_operation_result
make_ready_native_swapchain_create_operation()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(
        make_ready_native_swapchain_create_operation_request());
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_images_operation_request
make_ready_native_swapchain_images_operation_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_swapchain_images_operation_request{
        .create_operation = make_ready_native_swapchain_create_operation(),
        .native_entrypoints = make_ready_native_swapchain_entrypoints(),
        .swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 123},
        .image_handle_base = vulkan_backend::vulkan_native_function_pointer{.value = 5000},
    };
}

void test_swapchain_create_plan_selects_requested_supported_values()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_swapchain_create_plan_result plan =
        vulkan_backend::build_vulkan_swapchain_create_plan(
            vulkan_backend::vulkan_swapchain_create_plan_request{
                .capabilities = make_capabilities(),
                .intent = make_intent(),
                .recreate_reference = {},
            });

    require(plan.checked, "swapchain create plan is checked");
    require(
        plan.status == vulkan_backend::vulkan_swapchain_create_plan_status::ready,
        "swapchain create plan reports ready");
    require(plan.ready_for_create(), "swapchain create plan reaches create gate");
    require(!plan.blocked(), "ready swapchain create plan is not blocked");
    require(plan.capabilities_checked, "swapchain create plan records capability snapshot");
    require(plan.desired_format_supported, "swapchain create plan keeps desired format");
    require(!plan.fallback_format_selected, "swapchain create plan does not fallback format");
    require(
        plan.selected_surface_format.format
            == vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm,
        "swapchain create plan selects desired format");
    require(
        plan.selected_surface_format.color_space
            == vulkan_backend::vulkan_swapchain_color_space::srgb_nonlinear,
        "swapchain create plan selects desired color space");
    require(
        plan.selected_present_mode == vulkan_backend::vulkan_swapchain_present_mode::mailbox,
        "swapchain create plan selects mailbox for non-vsync intent");
    require(plan.desired_present_mode_supported, "mailbox intent is supported");
    require(!plan.fallback_present_mode_selected, "swapchain create plan does not fallback mode");
    require(plan.selected_image_count == 3, "swapchain create plan keeps desired image count");
    require(plan.selected_extent.width == 1280, "swapchain create plan keeps desired width");
    require(plan.selected_extent.height == 720, "swapchain create plan keeps desired height");
    require(!plan.extent_clamped, "swapchain create plan does not clamp supported extent");
    require(!plan.image_count_clamped, "swapchain create plan does not clamp supported image count");
    require(
        plan.selected_transform == vulkan_backend::vulkan_swapchain_surface_transform::identity,
        "swapchain create plan selects desired transform");
    require(plan.transform_supported, "swapchain create plan marks desired transform supported");
    require(
        plan.selected_composite_alpha == vulkan_backend::vulkan_swapchain_composite_alpha::opaque,
        "swapchain create plan selects desired composite alpha");
    require(plan.composite_alpha_supported, "swapchain create plan marks desired alpha supported");
    require(
        plan.selected_sharing_mode == vulkan_backend::vulkan_swapchain_image_sharing_mode::concurrent,
        "swapchain create plan selects concurrent sharing for split queues");
    require(plan.sharing_concurrent, "swapchain create plan records concurrent sharing");
    require(plan.recreate_compatible, "swapchain create plan is compatible without reference");
}

void test_swapchain_create_plan_selects_vsync_fifo_and_clamps_extent_and_image_count()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_swapchain_create_plan_intent intent = make_intent();
    intent.desired_vsync = true;
    intent.desired_extent = vulkan_backend::vulkan_surface_extent{.width = 320, .height = 2000};
    intent.desired_image_count = 8;
    intent.graphics_queue_family_index = 2;
    intent.present_queue_family_index = 2;

    const vulkan_backend::vulkan_swapchain_create_plan_result plan =
        vulkan_backend::build_vulkan_swapchain_create_plan(
            vulkan_backend::vulkan_swapchain_create_plan_request{
                .capabilities = make_capabilities(),
                .intent = intent,
                .recreate_reference = {},
            });

    require(plan.ready_for_create(), "clamped swapchain create plan is ready");
    require(
        plan.selected_present_mode == vulkan_backend::vulkan_swapchain_present_mode::fifo,
        "swapchain create plan selects fifo for vsync intent");
    require(plan.desired_present_mode_supported, "fifo vsync mode is supported");
    require(plan.selected_extent.width == 640, "swapchain create plan clamps width to min");
    require(plan.selected_extent.height == 1080, "swapchain create plan clamps height to max");
    require(plan.extent_clamped, "swapchain create plan records clamped extent");
    require(plan.selected_image_count == 4, "swapchain create plan clamps image count to max");
    require(plan.image_count_clamped, "swapchain create plan records clamped image count");
    require(plan.max_image_count_clamped, "swapchain create plan records max image count clamp");
    require(!plan.min_image_count_clamped, "swapchain create plan does not record min clamp");
    require(
        plan.selected_sharing_mode == vulkan_backend::vulkan_swapchain_image_sharing_mode::exclusive,
        "swapchain create plan selects exclusive sharing for same queue family");
}

void test_swapchain_create_plan_reports_missing_format_and_present_mode()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_swapchain_surface_capabilities_snapshot missing_format =
        make_capabilities();
    missing_format.surface_formats = {};
    const vulkan_backend::vulkan_swapchain_create_plan_result format_plan =
        vulkan_backend::build_vulkan_swapchain_create_plan(
            vulkan_backend::vulkan_swapchain_create_plan_request{
                .capabilities = missing_format,
                .intent = make_intent(),
                .recreate_reference = {},
            });
    require(
        format_plan.status
            == vulkan_backend::vulkan_swapchain_create_plan_status::missing_surface_format,
        "swapchain create plan reports missing surface format");
    require(format_plan.blocked(), "missing surface format blocks swapchain create plan");

    vulkan_backend::vulkan_swapchain_surface_capabilities_snapshot missing_mode =
        make_capabilities();
    missing_mode.present_modes = {};
    const vulkan_backend::vulkan_swapchain_create_plan_result mode_plan =
        vulkan_backend::build_vulkan_swapchain_create_plan(
            vulkan_backend::vulkan_swapchain_create_plan_request{
                .capabilities = missing_mode,
                .intent = make_intent(),
                .recreate_reference = {},
            });
    require(
        mode_plan.status
            == vulkan_backend::vulkan_swapchain_create_plan_status::missing_present_mode,
        "swapchain create plan reports missing present mode");
    require(mode_plan.blocked(), "missing present mode blocks swapchain create plan");
}

void test_swapchain_create_plan_reports_unsupported_zero_extent()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_swapchain_surface_capabilities_snapshot capabilities =
        make_capabilities();
    capabilities.min_extent = {};
    capabilities.max_extent = {};
    vulkan_backend::vulkan_swapchain_create_plan_intent intent = make_intent();
    intent.desired_extent = {};

    const vulkan_backend::vulkan_swapchain_create_plan_result plan =
        vulkan_backend::build_vulkan_swapchain_create_plan(
            vulkan_backend::vulkan_swapchain_create_plan_request{
                .capabilities = capabilities,
                .intent = intent,
                .recreate_reference = {},
            });

    require(
        plan.status
            == vulkan_backend::vulkan_swapchain_create_plan_status::unsupported_zero_extent,
        "swapchain create plan reports unsupported zero extent");
    require(plan.zero_extent_unsupported, "swapchain create plan records zero extent flag");
    require(plan.blocked(), "zero extent blocks swapchain create plan");
}

void test_swapchain_create_plan_reports_recreate_compatibility()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_swapchain_recreate_compatibility_snapshot compatible_reference{
        .checked = true,
        .previous_extent = vulkan_backend::vulkan_surface_extent{.width = 1280, .height = 720},
        .previous_image_count = 3,
        .previous_surface_format = vulkan_backend::vulkan_swapchain_surface_format{
            .format = vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm,
            .color_space = vulkan_backend::vulkan_swapchain_color_space::srgb_nonlinear,
        },
        .previous_present_mode = vulkan_backend::vulkan_swapchain_present_mode::mailbox,
    };
    const vulkan_backend::vulkan_swapchain_create_plan_result compatible =
        vulkan_backend::build_vulkan_swapchain_create_plan(
            vulkan_backend::vulkan_swapchain_create_plan_request{
                .capabilities = make_capabilities(),
                .intent = make_intent(),
                .recreate_reference = compatible_reference,
            });
    require(compatible.ready_for_create(), "compatible recreate plan is ready");
    require(compatible.recreate_reference_checked, "compatible recreate plan checks reference");
    require(compatible.recreate_compatible, "compatible recreate plan reports compatibility");
    require(compatible.recreate_extent_compatible, "compatible recreate plan keeps extent");
    require(compatible.recreate_format_compatible, "compatible recreate plan keeps format");
    require(compatible.recreate_present_mode_compatible, "compatible recreate plan keeps mode");
    require(compatible.recreate_image_count_compatible, "compatible recreate plan keeps image count");

    vulkan_backend::vulkan_swapchain_recreate_compatibility_snapshot incompatible_reference =
        compatible_reference;
    incompatible_reference.previous_extent =
        vulkan_backend::vulkan_surface_extent{.width = 800, .height = 600};
    incompatible_reference.previous_surface_format =
        vulkan_backend::vulkan_swapchain_surface_format{
            .format = vulkan_backend::vulkan_swapchain_image_format::rgba8_unorm,
            .color_space = vulkan_backend::vulkan_swapchain_color_space::srgb_nonlinear,
        };
    const vulkan_backend::vulkan_swapchain_create_plan_result incompatible =
        vulkan_backend::build_vulkan_swapchain_create_plan(
            vulkan_backend::vulkan_swapchain_create_plan_request{
                .capabilities = make_capabilities(),
                .intent = make_intent(),
                .recreate_reference = incompatible_reference,
            });
    require(incompatible.ready_for_create(), "incompatible recreate plan still reaches create gate");
    require(!incompatible.recreate_compatible, "incompatible recreate plan reports incompatibility");
    require(!incompatible.recreate_extent_compatible, "incompatible recreate plan reports extent change");
    require(!incompatible.recreate_format_compatible, "incompatible recreate plan reports format change");
}

void test_native_swapchain_create_operation_reports_ready_operation_summary()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_swapchain_create_operation_result operation =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(
            make_ready_native_swapchain_create_operation_request());

    require(operation.checked, "native swapchain create operation is checked");
    require(operation.can_call_vk_create_swapchain(), "native operation can call vkCreateSwapchainKHR");
    require(!operation.blocked(), "ready native operation is not blocked");
    require(
        operation.status == vulkan_backend::vulkan_native_swapchain_create_operation_status::ready,
        "native operation status is ready");
    require(operation.create_plan_checked, "native operation records checked create plan");
    require(operation.create_plan_ready, "native operation records ready create plan");
    require(operation.native_entrypoints_checked, "native operation records native readiness check");
    require(
        operation.native_entrypoints_ready_for_create,
        "native operation records create entrypoints ready");
    require(operation.device_valid, "native operation records valid device");
    require(operation.surface_valid, "native operation records valid surface");
    require(operation.required_extensions_ready, "native operation records extension readiness");
    require(operation.create_symbol_ready, "native operation records create symbol readiness");
    require(operation.destroy_symbol_ready, "native operation records destroy symbol readiness");
    require(operation.get_images_symbol_ready, "native operation records get-images symbol readiness");
    require(operation.vk_create_swapchain_callable, "native operation exposes vkCreateSwapchainKHR gate");
    require(operation.selected_extent.width == 1280, "native operation preserves selected width");
    require(operation.selected_extent.height == 720, "native operation preserves selected height");
    require(
        operation.selected_surface_format.format
            == vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm,
        "native operation preserves selected format");
    require(
        operation.selected_present_mode == vulkan_backend::vulkan_swapchain_present_mode::mailbox,
        "native operation preserves selected present mode");
    require(operation.selected_image_count == 3, "native operation preserves image count");
    require(operation.device.value == 77, "native operation preserves device handle");
    require(operation.surface.value == 88, "native operation preserves surface handle");
    require(operation.old_swapchain.value == 99, "native operation preserves old swapchain handle");
    require(operation.operation.ready(), "native operation summary is ready");
    require(
        operation.operation.entrypoint_name == "vkCreateSwapchainKHR",
        "native operation summary names the stable entrypoint");
    require(
        operation.operation.selected_sharing_mode
            == vulkan_backend::vulkan_swapchain_image_sharing_mode::concurrent,
        "native operation summary preserves sharing mode");
}

void test_native_swapchain_create_operation_blocks_plan_and_recreate_compatibility()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_swapchain_create_operation_request request =
        make_ready_native_swapchain_create_operation_request();
    vulkan_backend::vulkan_swapchain_surface_capabilities_snapshot capabilities =
        make_capabilities();
    capabilities.surface_formats = {};
    request.create_plan = vulkan_backend::build_vulkan_swapchain_create_plan(
        vulkan_backend::vulkan_swapchain_create_plan_request{
            .capabilities = capabilities,
            .intent = make_intent(),
            .recreate_reference = {},
        });
    const vulkan_backend::vulkan_native_swapchain_create_operation_result missing_plan =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(request);

    require(
        missing_plan.status
            == vulkan_backend::vulkan_native_swapchain_create_operation_status::create_plan_unavailable,
        "native operation blocks unavailable create plan");
    require(!missing_plan.can_call_vk_create_swapchain(), "unavailable create plan cannot call Vulkan");
    require(missing_plan.blocked(), "unavailable create plan reports blocked");

    vulkan_backend::vulkan_swapchain_recreate_compatibility_snapshot incompatible_reference{
        .checked = true,
        .previous_extent = vulkan_backend::vulkan_surface_extent{.width = 800, .height = 600},
        .previous_image_count = 2,
        .previous_surface_format = vulkan_backend::vulkan_swapchain_surface_format{
            .format = vulkan_backend::vulkan_swapchain_image_format::rgba8_unorm,
            .color_space = vulkan_backend::vulkan_swapchain_color_space::srgb_nonlinear,
        },
        .previous_present_mode = vulkan_backend::vulkan_swapchain_present_mode::fifo,
    };
    request = make_ready_native_swapchain_create_operation_request();
    request.create_plan = vulkan_backend::build_vulkan_swapchain_create_plan(
        vulkan_backend::vulkan_swapchain_create_plan_request{
            .capabilities = make_capabilities(),
            .intent = make_intent(),
            .recreate_reference = incompatible_reference,
        });
    request.require_recreate_compatibility = true;
    const vulkan_backend::vulkan_native_swapchain_create_operation_result recreate =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(request);

    require(
        recreate.status
            == vulkan_backend::vulkan_native_swapchain_create_operation_status::recreate_incompatible,
        "native operation blocks required recreate compatibility");
    require(recreate.recreate_reference_checked, "native operation records recreate reference check");
    require(!recreate.recreate_compatible, "native operation records recreate incompatibility");
    require(recreate.recreate_compatibility_required, "native operation records recreate requirement");
    require(recreate.recreate_compatibility_blocked, "native operation records recreate blocker");
    require(
        recreate.operation.recreate_compatibility_blocked,
        "native operation summary records recreate blocker");
}

void test_native_swapchain_create_operation_blocks_invalid_device_and_surface()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_swapchain_create_operation_request request =
        make_ready_native_swapchain_create_operation_request();
    request.device = {};
    const vulkan_backend::vulkan_native_swapchain_create_operation_result invalid_device =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(request);
    require(
        invalid_device.status
            == vulkan_backend::vulkan_native_swapchain_create_operation_status::invalid_device,
        "native operation blocks invalid device");
    require(!invalid_device.device_valid, "native operation records invalid device");

    request = make_ready_native_swapchain_create_operation_request();
    request.surface = {};
    const vulkan_backend::vulkan_native_swapchain_create_operation_result invalid_surface =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(request);
    require(
        invalid_surface.status
            == vulkan_backend::vulkan_native_swapchain_create_operation_status::invalid_surface,
        "native operation blocks invalid surface");
    require(!invalid_surface.surface_valid, "native operation records invalid surface");
}

void test_native_swapchain_create_operation_blocks_native_extension_and_symbols()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_swapchain_create_operation_request request =
        make_ready_native_swapchain_create_operation_request();
    request.native_entrypoints.required_extensions_ready = false;
    request.native_entrypoints.missing_required_extension = "VK_KHR_swapchain";
    request.native_entrypoints.diagnostic = "missing VK_KHR_swapchain";
    const vulkan_backend::vulkan_native_swapchain_create_operation_result missing_extension =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(request);
    require(
        missing_extension.status
            == vulkan_backend::vulkan_native_swapchain_create_operation_status::required_extension_unavailable,
        "native operation blocks missing swapchain extension");
    require(
        missing_extension.missing_required_extension == "VK_KHR_swapchain",
        "native operation records missing required extension");

    request = make_ready_native_swapchain_create_operation_request();
    request.native_entrypoints.create_swapchain_ready = false;
    request.native_entrypoints.missing_symbol_name = "vkCreateSwapchainKHR";
    const vulkan_backend::vulkan_native_swapchain_create_operation_result missing_create =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(request);
    require(
        missing_create.status
            == vulkan_backend::vulkan_native_swapchain_create_operation_status::missing_create_symbol,
        "native operation blocks missing create symbol");
    require(
        missing_create.missing_symbol_name == "vkCreateSwapchainKHR",
        "native operation records missing create symbol");

    request = make_ready_native_swapchain_create_operation_request();
    request.native_entrypoints.destroy_swapchain_ready = false;
    request.native_entrypoints.missing_symbol_name = "vkDestroySwapchainKHR";
    const vulkan_backend::vulkan_native_swapchain_create_operation_result missing_destroy =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(request);
    require(
        missing_destroy.status
            == vulkan_backend::vulkan_native_swapchain_create_operation_status::missing_destroy_symbol,
        "native operation blocks missing destroy symbol");

    request = make_ready_native_swapchain_create_operation_request();
    request.native_entrypoints.get_swapchain_images_ready = false;
    request.native_entrypoints.missing_symbol_name = "vkGetSwapchainImagesKHR";
    const vulkan_backend::vulkan_native_swapchain_create_operation_result missing_images =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(request);
    require(
        missing_images.status
            == vulkan_backend::vulkan_native_swapchain_create_operation_status::missing_images_symbol,
        "native operation blocks missing get-images symbol");
}

void test_native_swapchain_images_operation_reports_ready_operation_summary()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_swapchain_images_operation_result operation =
        vulkan_backend::build_vulkan_native_swapchain_images_operation_plan(
            make_ready_native_swapchain_images_operation_request());

    require(operation.checked, "native swapchain images operation is checked");
    require(operation.can_call_vk_get_swapchain_images(), "native images operation can call Vulkan");
    require(!operation.blocked(), "ready native images operation is not blocked");
    require(
        operation.status == vulkan_backend::vulkan_native_swapchain_images_operation_status::ready,
        "native images operation status is ready");
    require(operation.create_operation_checked, "native images operation records create check");
    require(operation.create_operation_ready, "native images operation records ready create operation");
    require(operation.native_entrypoints_checked, "native images operation records native check");
    require(operation.required_extensions_ready, "native images operation records extension readiness");
    require(operation.get_images_symbol_ready, "native images operation records image symbol readiness");
    require(operation.device_valid, "native images operation records valid device");
    require(operation.swapchain_valid, "native images operation records valid swapchain");
    require(operation.vk_get_swapchain_images_callable, "native images operation exposes call gate");
    require(operation.expected_image_count == 3, "native images operation preserves expected count");
    require(operation.enumerated_image_count == 3, "native images operation enumerates expected count");
    require(operation.images.size() == 3, "native images operation stores selected images");
    require(operation.images[0].image_id.value == 1, "native images operation assigns first image id");
    require(operation.images[0].handle.value == 5001, "native images operation assigns first opaque handle");
    require(operation.images[2].image_id.value == 3, "native images operation assigns last image id");
    require(operation.images[2].handle.value == 5003, "native images operation assigns last opaque handle");
    require(operation.images[0].valid(), "native images operation image binding is valid");
    require(operation.operation.ready_for_frame_lifecycle_setup(), "native images summary is frame-ready");
    require(
        operation.operation.entrypoint_name == "vkGetSwapchainImagesKHR",
        "native images summary names the stable entrypoint");
    require(
        operation.operation.selected_extent.width == 1280,
        "native images summary preserves selected width");
    require(
        operation.operation.selected_present_mode
            == vulkan_backend::vulkan_swapchain_present_mode::mailbox,
        "native images summary preserves selected present mode");
}

void test_native_swapchain_images_operation_blocks_create_and_swapchain_prerequisites()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_swapchain_images_operation_request request =
        make_ready_native_swapchain_images_operation_request();
    request.create_operation = {};
    const vulkan_backend::vulkan_native_swapchain_images_operation_result missing_create =
        vulkan_backend::build_vulkan_native_swapchain_images_operation_plan(request);
    require(
        missing_create.status
            == vulkan_backend::vulkan_native_swapchain_images_operation_status::create_operation_unavailable,
        "native images operation blocks unchecked create operation");
    require(missing_create.blocked(), "unchecked create operation reports blocked");

    request = make_ready_native_swapchain_images_operation_request();
    request.create_operation.device = {};
    request.create_operation.device_valid = false;
    const vulkan_backend::vulkan_native_swapchain_images_operation_result invalid_device =
        vulkan_backend::build_vulkan_native_swapchain_images_operation_plan(request);
    require(
        invalid_device.status
            == vulkan_backend::vulkan_native_swapchain_images_operation_status::invalid_device,
        "native images operation blocks invalid device");
    require(!invalid_device.device_valid, "native images operation records invalid device");

    request = make_ready_native_swapchain_images_operation_request();
    request.swapchain = {};
    const vulkan_backend::vulkan_native_swapchain_images_operation_result missing_swapchain =
        vulkan_backend::build_vulkan_native_swapchain_images_operation_plan(request);
    require(
        missing_swapchain.status
            == vulkan_backend::vulkan_native_swapchain_images_operation_status::missing_swapchain_handle,
        "native images operation blocks missing swapchain handle");
    require(!missing_swapchain.swapchain_valid, "native images operation records missing swapchain");
}

void test_native_swapchain_images_operation_blocks_native_extension_and_entrypoint()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_swapchain_images_operation_request request =
        make_ready_native_swapchain_images_operation_request();
    request.native_entrypoints.required_extensions_ready = false;
    request.native_entrypoints.missing_required_extension = "VK_KHR_swapchain";
    request.native_entrypoints.diagnostic = "missing VK_KHR_swapchain";
    const vulkan_backend::vulkan_native_swapchain_images_operation_result missing_extension =
        vulkan_backend::build_vulkan_native_swapchain_images_operation_plan(request);
    require(
        missing_extension.status
            == vulkan_backend::vulkan_native_swapchain_images_operation_status::required_extension_unavailable,
        "native images operation blocks missing required extension");
    require(
        missing_extension.missing_required_extension == "VK_KHR_swapchain",
        "native images operation records missing extension");

    request = make_ready_native_swapchain_images_operation_request();
    request.native_entrypoints.get_swapchain_images_ready = false;
    request.native_entrypoints.missing_symbol_name = "vkGetSwapchainImagesKHR";
    const vulkan_backend::vulkan_native_swapchain_images_operation_result missing_symbol =
        vulkan_backend::build_vulkan_native_swapchain_images_operation_plan(request);
    require(
        missing_symbol.status
            == vulkan_backend::vulkan_native_swapchain_images_operation_status::missing_images_symbol,
        "native images operation blocks missing get-images entrypoint");
    require(
        missing_symbol.missing_symbol_name == "vkGetSwapchainImagesKHR",
        "native images operation records missing get-images symbol");
}

void test_native_swapchain_images_operation_blocks_missing_image_count()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_swapchain_images_operation_request request =
        make_ready_native_swapchain_images_operation_request();
    request.create_operation.selected_image_count = 0;
    const vulkan_backend::vulkan_native_swapchain_images_operation_result operation =
        vulkan_backend::build_vulkan_native_swapchain_images_operation_plan(request);
    require(
        operation.status
            == vulkan_backend::vulkan_native_swapchain_images_operation_status::image_count_unavailable,
        "native images operation blocks missing image count");
    require(operation.expected_image_count == 0, "native images operation records zero expected count");
}

void test_swapchain_create_plan_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::swapchain_create_plan_status_name(
            vulkan_backend::vulkan_swapchain_create_plan_status::ready)
            == std::string_view{"ready"},
        "swapchain create plan ready status name is stable");
    require(
        vulkan_backend::swapchain_create_plan_status_name(
            vulkan_backend::vulkan_swapchain_create_plan_status::missing_surface_format)
            == std::string_view{"missing_surface_format"},
        "swapchain create plan missing format status name is stable");
    require(
        vulkan_backend::swapchain_image_format_name(
            vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm)
            == std::string_view{"bgra8_unorm"},
        "swapchain image format name is stable");
    require(
        vulkan_backend::swapchain_color_space_name(
            vulkan_backend::vulkan_swapchain_color_space::srgb_nonlinear)
            == std::string_view{"srgb_nonlinear"},
        "swapchain color space name is stable");
    require(
        vulkan_backend::swapchain_image_sharing_mode_name(
            vulkan_backend::vulkan_swapchain_image_sharing_mode::concurrent)
            == std::string_view{"concurrent"},
        "swapchain sharing mode name is stable");
    require(
        vulkan_backend::native_swapchain_create_operation_status_name(
            vulkan_backend::vulkan_native_swapchain_create_operation_status::ready)
            == std::string_view{"ready"},
        "native swapchain create operation ready status name is stable");
    require(
        vulkan_backend::native_swapchain_create_operation_status_name(
            vulkan_backend::vulkan_native_swapchain_create_operation_status::missing_images_symbol)
            == std::string_view{"missing_images_symbol"},
        "native swapchain create operation missing images status name is stable");
    require(
        vulkan_backend::native_swapchain_create_operation_status_name(
            vulkan_backend::vulkan_native_swapchain_create_operation_status::recreate_incompatible)
            == std::string_view{"recreate_incompatible"},
        "native swapchain create operation recreate status name is stable");
    require(
        vulkan_backend::native_swapchain_images_operation_status_name(
            vulkan_backend::vulkan_native_swapchain_images_operation_status::ready)
            == std::string_view{"ready"},
        "native swapchain images operation ready status name is stable");
    require(
        vulkan_backend::native_swapchain_images_operation_status_name(
            vulkan_backend::vulkan_native_swapchain_images_operation_status::missing_swapchain_handle)
            == std::string_view{"missing_swapchain_handle"},
        "native swapchain images operation missing handle status name is stable");
    require(
        vulkan_backend::native_swapchain_images_operation_status_name(
            vulkan_backend::vulkan_native_swapchain_images_operation_status::missing_images_symbol)
            == std::string_view{"missing_images_symbol"},
        "native swapchain images operation missing symbol status name is stable");
}

} // namespace

int main()
{
    test_swapchain_create_plan_selects_requested_supported_values();
    test_swapchain_create_plan_selects_vsync_fifo_and_clamps_extent_and_image_count();
    test_swapchain_create_plan_reports_missing_format_and_present_mode();
    test_swapchain_create_plan_reports_unsupported_zero_extent();
    test_swapchain_create_plan_reports_recreate_compatibility();
    test_native_swapchain_create_operation_reports_ready_operation_summary();
    test_native_swapchain_create_operation_blocks_plan_and_recreate_compatibility();
    test_native_swapchain_create_operation_blocks_invalid_device_and_surface();
    test_native_swapchain_create_operation_blocks_native_extension_and_symbols();
    test_native_swapchain_images_operation_reports_ready_operation_summary();
    test_native_swapchain_images_operation_blocks_create_and_swapchain_prerequisites();
    test_native_swapchain_images_operation_blocks_native_extension_and_entrypoint();
    test_native_swapchain_images_operation_blocks_missing_image_count();
    test_swapchain_create_plan_names_are_stable();
    return 0;
}
