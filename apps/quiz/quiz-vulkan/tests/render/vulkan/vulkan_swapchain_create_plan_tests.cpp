#include "render/vulkan/vulkan_backend_swapchain.h"

#include <cassert>
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

quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_state
make_ready_loader()
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
        .required_symbol_name = "vkGetInstanceProcAddr",
        .attempted_library_count = 1,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_function_table_diagnostics
make_swapchain_function_table(
    const std::vector<std::string>& missing_symbols = {},
    bool include_swapchain_extension = true)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_symbol_resolver_options{
            .missing_symbols = missing_symbols,
            .pointer_base = vulkan_backend::vulkan_native_function_pointer{.value = 900},
        });
    return vulkan_backend::collect_vulkan_native_function_table(
        resolver,
        make_ready_loader(),
        vulkan_backend::vulkan_native_function_table_request{
            .symbols = vulkan_backend::default_vulkan_native_swapchain_entrypoints(),
            .include_default_backend_entrypoints = false,
            .available_extensions =
                include_swapchain_extension
                ? std::vector<std::string>{"VK_KHR_swapchain"}
                : std::vector<std::string>{},
            .include_default_swapchain_extension = true,
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_operation_dispatch_table
make_ready_native_swapchain_operation_dispatch_table()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::collect_vulkan_native_swapchain_operation_dispatch_table(
        make_swapchain_function_table());
}

quiz_vulkan::render::vulkan_backend::vulkan_native_physical_device_selection_result
make_ready_native_physical_device_selection()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_instance_handle instance{.value = 44};
    const vulkan_backend::vulkan_physical_device_handle physical_device{.value = 55};
    const vulkan_backend::vulkan_native_physical_device_dispatch_table physical_dispatch{
        .checked = true,
        .status = vulkan_backend::vulkan_native_physical_device_dispatch_table_status::ready,
        .instance = instance,
        .get_instance_proc_address = vulkan_backend::vulkan_native_function_pointer{.value = 100},
        .enumerate_physical_devices = vulkan_backend::vulkan_native_function_pointer{.value = 101},
        .get_physical_device_queue_family_properties =
            vulkan_backend::vulkan_native_function_pointer{.value = 102},
        .diagnostic = "physical device dispatch ready",
    };
    const vulkan_backend::vulkan_native_physical_device_enumeration_result enumeration{
        .checked = true,
        .status = vulkan_backend::vulkan_native_physical_device_enumeration_status::ready,
        .dispatch_table = physical_dispatch,
        .physical_devices = {physical_device},
        .physical_device_count = 1,
        .diagnostic = "physical device enumeration ready",
    };
    const vulkan_backend::vulkan_native_queue_family_query_result queue_query{
        .checked = true,
        .status = vulkan_backend::vulkan_native_queue_family_query_status::ready,
        .enumeration = enumeration,
        .queue_families = {
            vulkan_backend::vulkan_native_physical_device_queue_family{
                .physical_device = physical_device,
                .family_index = 3,
                .queue_count = 1,
                .capabilities = {vulkan_backend::vulkan_device_queue_capability::graphics},
            },
            vulkan_backend::vulkan_native_physical_device_queue_family{
                .physical_device = physical_device,
                .family_index = 7,
                .queue_count = 1,
                .capabilities = {vulkan_backend::vulkan_device_queue_capability::present},
            },
        },
        .queue_family_count = 2,
        .diagnostic = "queue families ready",
    };
    return vulkan_backend::vulkan_native_physical_device_selection_result{
        .checked = true,
        .status = vulkan_backend::vulkan_native_physical_device_selection_status::selected,
        .enumeration = enumeration,
        .queue_family_query = queue_query,
        .request = vulkan_backend::vulkan_device_create_request{
            .required_device_extensions = {"VK_KHR_swapchain"},
            .required_queue_capabilities = {
                vulkan_backend::vulkan_device_queue_capability::graphics,
                vulkan_backend::vulkan_device_queue_capability::present,
            },
        },
        .selected_physical_device = physical_device,
        .selected_queue_families = {
            vulkan_backend::vulkan_native_physical_device_queue_family_selection{
                .capability = vulkan_backend::vulkan_device_queue_capability::graphics,
                .physical_device = physical_device,
                .family_index = 3,
                .queue_count = 1,
            },
            vulkan_backend::vulkan_native_physical_device_queue_family_selection{
                .capability = vulkan_backend::vulkan_device_queue_capability::present,
                .physical_device = physical_device,
                .family_index = 7,
                .queue_count = 1,
            },
        },
        .diagnostic = "physical device selected",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_device_create_result
make_ready_native_device()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_selection_result selection =
        make_ready_native_physical_device_selection();
    const vulkan_backend::vulkan_native_device_dispatch_table dispatch{
        .checked = true,
        .status = vulkan_backend::vulkan_native_device_dispatch_table_status::ready,
        .instance = selection.enumeration.dispatch_table.instance,
        .physical_device = selection.selected_physical_device,
        .get_instance_proc_address =
            selection.enumeration.dispatch_table.get_instance_proc_address,
        .enumerate_device_extension_properties =
            vulkan_backend::vulkan_native_function_pointer{.value = 201},
        .create_device = vulkan_backend::vulkan_native_function_pointer{.value = 202},
        .get_device_queue = vulkan_backend::vulkan_native_function_pointer{.value = 203},
        .destroy_device = vulkan_backend::vulkan_native_function_pointer{.value = 204},
        .diagnostic = "device dispatch ready",
    };
    const vulkan_backend::vulkan_native_device_extension_query_result extension_query{
        .checked = true,
        .status = vulkan_backend::vulkan_native_device_extension_query_status::ready,
        .dispatch_table = dispatch,
        .selection = selection,
        .physical_device = selection.selected_physical_device,
        .available_extensions = {"VK_KHR_swapchain"},
        .selected_extensions = {"VK_KHR_swapchain"},
        .required_extension_diagnostics = {
            vulkan_backend::vulkan_device_extension_diagnostic{
                .extension_name = "VK_KHR_swapchain",
                .required = true,
                .available = true,
                .selected = true,
            },
        },
        .available_extension_count = 1,
        .required_extension_count = 1,
        .available_required_extension_count = 1,
        .diagnostic = "device extensions ready",
    };
    return vulkan_backend::vulkan_native_device_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_native_device_create_status::created,
        .dispatch_table = dispatch,
        .extension_query = extension_query,
        .selection = selection,
        .handle = vulkan_backend::vulkan_device_handle{.value = 77},
        .selected_extensions = {"VK_KHR_swapchain"},
        .queue_create_family_indices = {3, 7},
        .selected_queues = {
            vulkan_backend::vulkan_device_queue_selection{
                .capability = vulkan_backend::vulkan_device_queue_capability::graphics,
                .family_index = 3,
                .queue = vulkan_backend::vulkan_queue_handle{.value = 301},
            },
            vulkan_backend::vulkan_device_queue_selection{
                .capability = vulkan_backend::vulkan_device_queue_capability::present,
                .family_index = 7,
                .queue = vulkan_backend::vulkan_queue_handle{.value = 302},
            },
        },
        .queue_create_family_count = 2,
        .selected_queue_count = 2,
        .diagnostic = "native device ready",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_surface_query_dispatch_table
make_ready_native_surface_query_dispatch_table(
    const quiz_vulkan::render::vulkan_backend::vulkan_native_device_create_result& device)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_surface_query_dispatch_table{
        .checked = true,
        .status = vulkan_backend::vulkan_native_surface_query_dispatch_table_status::ready,
        .instance = device.dispatch_table.instance,
        .physical_device = device.selection.selected_physical_device,
        .get_instance_proc_address = device.dispatch_table.get_instance_proc_address,
        .get_physical_device_surface_support =
            vulkan_backend::vulkan_native_function_pointer{.value = 401},
        .get_physical_device_surface_capabilities =
            vulkan_backend::vulkan_native_function_pointer{.value = 402},
        .get_physical_device_surface_formats =
            vulkan_backend::vulkan_native_function_pointer{.value = 403},
        .get_physical_device_surface_present_modes =
            vulkan_backend::vulkan_native_function_pointer{.value = 404},
        .diagnostic = "surface query dispatch ready",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_surface_capability_query_result
make_ready_native_surface_query(
    const quiz_vulkan::render::vulkan_backend::vulkan_native_device_create_result& device,
    quiz_vulkan::render::vulkan_backend::vulkan_surface_handle surface =
        quiz_vulkan::render::vulkan_backend::vulkan_surface_handle{.value = 88})
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_surface_capability_query query(
        vulkan_backend::fake_vulkan_native_surface_capability_query_options{
            .capabilities = make_capabilities(),
        });
    return vulkan_backend::query_native_vulkan_surface_capabilities(
        query,
        make_ready_native_surface_query_dispatch_table(device),
        device,
        surface,
        7);
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_create_operation_request
make_ready_native_swapchain_create_operation_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;
    const vulkan_backend::vulkan_native_device_create_result device =
        make_ready_native_device();

    return vulkan_backend::vulkan_native_swapchain_create_operation_request{
        .create_plan = make_ready_create_plan(),
        .native_entrypoints = make_ready_native_swapchain_entrypoints(),
        .device = device.handle,
        .surface = vulkan_backend::vulkan_surface_handle{.value = 88},
        .surface_query = make_ready_native_surface_query(
            device,
            vulkan_backend::vulkan_surface_handle{.value = 88}),
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

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_images_operation_result
make_ready_native_swapchain_images_operation()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::build_vulkan_native_swapchain_images_operation_plan(
        make_ready_native_swapchain_images_operation_request());
}

quiz_vulkan::render::vulkan_backend::vulkan_swapchain_image_acquire_plan_result
make_acquire_plan(
    quiz_vulkan::render::vulkan_backend::vulkan_swapchain_acquire_status status,
    std::size_t image_id = 2)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const bool recordable =
        status == vulkan_backend::vulkan_swapchain_acquire_status::acquired
        || status == vulkan_backend::vulkan_swapchain_acquire_status::suboptimal;
    return vulkan_backend::build_vulkan_swapchain_image_acquire_plan(
        vulkan_backend::vulkan_swapchain_image_acquire_request{
            .requested = true,
            .lifecycle_ready = true,
            .swapchain_ready = true,
            .swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 123},
            .image_count = 3,
            .timeout_nanoseconds = 16000000,
            .image_available_semaphore_ready = true,
            .fence_ready = true,
            .allow_suboptimal = true,
        },
        vulkan_backend::vulkan_swapchain_acquire_result{
            .status = status,
            .image = vulkan_backend::vulkan_swapchain_image_state{
                .id = vulkan_backend::vulkan_swapchain_image_id{.value = image_id},
                .available = recordable,
                .acquired = recordable,
                .presented = false,
            },
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_native_swapchain_acquire_operation_request
make_ready_native_swapchain_acquire_operation_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_swapchain_acquire_operation_request{
        .images_operation = make_ready_native_swapchain_images_operation(),
        .acquire_plan =
            make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::acquired),
        .native_entrypoints = make_ready_native_swapchain_entrypoints(),
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

void test_native_surface_query_dispatch_resolves_surface_symbols()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_device_create_result device =
        make_ready_native_device();
    vulkan_backend::fake_vulkan_native_instance_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_instance_symbol_resolver_options{
            .get_instance_proc_address = device.dispatch_table.get_instance_proc_address,
        });

    const vulkan_backend::vulkan_native_surface_query_dispatch_table dispatch =
        vulkan_backend::collect_vulkan_native_surface_query_dispatch_table(
            resolver,
            device);

    require(dispatch.checked, "surface query dispatch result is checked");
    require(
        dispatch.status
            == vulkan_backend::vulkan_native_surface_query_dispatch_table_status::ready,
        "surface query dispatch reports ready");
    require(dispatch.ready_for_query(), "surface query dispatch reaches query gate");
    require(
        resolver.state().resolve_call_count == 4,
        "surface query dispatch resolves four surface symbols");
    require(
        resolver.state().requested_symbols[0]
            == "vkGetPhysicalDeviceSurfaceSupportKHR",
        "surface query dispatch resolves support first");
    require(
        resolver.state().requested_symbols[1]
            == "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
        "surface query dispatch resolves capabilities second");
    require(
        resolver.state().requested_symbols[2]
            == "vkGetPhysicalDeviceSurfaceFormatsKHR",
        "surface query dispatch resolves formats third");
    require(
        resolver.state().requested_symbols[3]
            == "vkGetPhysicalDeviceSurfacePresentModesKHR",
        "surface query dispatch resolves present modes fourth");
}

void require_native_surface_query_dispatch_missing_symbol(
    std::string_view symbol_name,
    quiz_vulkan::render::vulkan_backend::vulkan_native_surface_query_dispatch_table_status
        expected_status,
    const char* message)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_device_create_result device =
        make_ready_native_device();
    vulkan_backend::fake_vulkan_native_instance_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_instance_symbol_resolver_options{
            .missing_symbols = {std::string{symbol_name}},
            .get_instance_proc_address = device.dispatch_table.get_instance_proc_address,
        });

    const vulkan_backend::vulkan_native_surface_query_dispatch_table dispatch =
        vulkan_backend::collect_vulkan_native_surface_query_dispatch_table(
            resolver,
            device);

    require(dispatch.status == expected_status, message);
    require(
        dispatch.missing_symbol_name == symbol_name,
        "surface query dispatch records missing symbol name");
    require(!dispatch.ready_for_query(), "missing surface symbol blocks query gate");
}

void test_native_surface_query_dispatch_reports_missing_symbols()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require_native_surface_query_dispatch_missing_symbol(
        "vkGetPhysicalDeviceSurfaceSupportKHR",
        vulkan_backend::vulkan_native_surface_query_dispatch_table_status::missing_surface_support_symbol,
        "surface query dispatch maps missing support symbol");
    require_native_surface_query_dispatch_missing_symbol(
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
        vulkan_backend::vulkan_native_surface_query_dispatch_table_status::missing_surface_capabilities_symbol,
        "surface query dispatch maps missing capabilities symbol");
    require_native_surface_query_dispatch_missing_symbol(
        "vkGetPhysicalDeviceSurfaceFormatsKHR",
        vulkan_backend::vulkan_native_surface_query_dispatch_table_status::missing_surface_formats_symbol,
        "surface query dispatch maps missing formats symbol");
    require_native_surface_query_dispatch_missing_symbol(
        "vkGetPhysicalDeviceSurfacePresentModesKHR",
        vulkan_backend::vulkan_native_surface_query_dispatch_table_status::missing_surface_present_modes_symbol,
        "surface query dispatch maps missing present modes symbol");
}

void test_native_surface_capability_query_records_ready_evidence()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_device_create_result device =
        make_ready_native_device();
    const vulkan_backend::vulkan_native_surface_query_dispatch_table dispatch =
        make_ready_native_surface_query_dispatch_table(device);
    vulkan_backend::fake_vulkan_native_surface_capability_query query(
        vulkan_backend::fake_vulkan_native_surface_capability_query_options{
            .capabilities = make_capabilities(),
        });

    const vulkan_backend::vulkan_native_surface_capability_query_result result =
        vulkan_backend::query_native_vulkan_surface_capabilities(
            query,
            dispatch,
            device,
            vulkan_backend::vulkan_surface_handle{.value = 88},
            7);

    require(result.checked, "surface capability query result is checked");
    require(
        result.status
            == vulkan_backend::vulkan_native_surface_capability_query_status::ready,
        "surface capability query reports ready");
    require(result.ready_for_swapchain_create(), "surface query reaches swapchain create gate");
    require(result.support_query_checked, "surface query records support query");
    require(result.capabilities_query_checked, "surface query records capabilities query");
    require(result.formats_query_checked, "surface query records format query");
    require(result.present_modes_query_checked, "surface query records present mode query");
    require(result.present_queue_supported, "surface query records present queue support");
    require(result.surface_format_count == 2, "surface query records surface format count");
    require(result.present_mode_count == 2, "surface query records present mode count");
    require(
        result.capabilities.min_image_count == 2,
        "surface query preserves capability min image count");
    require(
        query.state().requested_surface.value == 88,
        "surface query records requested opaque surface handle");
    require(
        query.state().requested_present_queue_family_index == 7,
        "surface query records requested present queue family index");
    require(
        query.state().last_get_physical_device_surface_support.value
            == dispatch.get_physical_device_surface_support.value,
        "surface query records support function pointer");
}

void test_native_surface_capability_query_reports_blockers()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_device_create_result device =
        make_ready_native_device();
    const vulkan_backend::vulkan_native_surface_query_dispatch_table dispatch =
        make_ready_native_surface_query_dispatch_table(device);

    vulkan_backend::fake_vulkan_native_surface_capability_query query;
    const vulkan_backend::vulkan_native_surface_capability_query_result missing_surface =
        vulkan_backend::query_native_vulkan_surface_capabilities(
            query,
            dispatch,
            device,
            {},
            7);
    require(
        missing_surface.status
            == vulkan_backend::vulkan_native_surface_capability_query_status::missing_surface_handle,
        "surface query reports missing surface handle");
    require(!missing_surface.ready_for_swapchain_create(), "missing surface blocks query readiness");

    vulkan_backend::fake_vulkan_native_surface_capability_query unsupported_query(
        vulkan_backend::fake_vulkan_native_surface_capability_query_options{
            .capabilities = make_capabilities(),
            .present_queue_supported = false,
        });
    const vulkan_backend::vulkan_native_surface_capability_query_result unsupported =
        vulkan_backend::query_native_vulkan_surface_capabilities(
            unsupported_query,
            dispatch,
            device,
            vulkan_backend::vulkan_surface_handle{.value = 88},
            7);
    require(
        unsupported.status
            == vulkan_backend::vulkan_native_surface_capability_query_status::unsupported_present_queue,
        "surface query reports unsupported present queue");
    require(!unsupported.present_queue_supported, "surface query records unsupported present queue");

    vulkan_backend::vulkan_swapchain_surface_capabilities_snapshot no_formats =
        make_capabilities();
    no_formats.surface_formats = {};
    vulkan_backend::fake_vulkan_native_surface_capability_query format_query(
        vulkan_backend::fake_vulkan_native_surface_capability_query_options{
            .capabilities = no_formats,
        });
    const vulkan_backend::vulkan_native_surface_capability_query_result formats =
        vulkan_backend::query_native_vulkan_surface_capabilities(
            format_query,
            dispatch,
            device,
            vulkan_backend::vulkan_surface_handle{.value = 88},
            7);
    require(
        formats.status
            == vulkan_backend::vulkan_native_surface_capability_query_status::zero_surface_formats,
        "surface query reports zero surface formats");

    vulkan_backend::vulkan_swapchain_surface_capabilities_snapshot no_modes =
        make_capabilities();
    no_modes.present_modes = {};
    vulkan_backend::fake_vulkan_native_surface_capability_query mode_query(
        vulkan_backend::fake_vulkan_native_surface_capability_query_options{
            .capabilities = no_modes,
        });
    const vulkan_backend::vulkan_native_surface_capability_query_result modes =
        vulkan_backend::query_native_vulkan_surface_capabilities(
            mode_query,
            dispatch,
            device,
            vulkan_backend::vulkan_surface_handle{.value = 88},
            7);
    require(
        modes.status
            == vulkan_backend::vulkan_native_surface_capability_query_status::zero_present_modes,
        "surface query reports zero present modes");
}

void test_native_swapchain_create_operation_blocks_missing_surface_query()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_swapchain_create_operation_request request =
        make_ready_native_swapchain_create_operation_request();
    request.surface_query = {};
    const vulkan_backend::vulkan_native_swapchain_create_operation_result unchecked =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(request);
    require(
        unchecked.status
            == vulkan_backend::vulkan_native_swapchain_create_operation_status::surface_query_unavailable,
        "native operation blocks unchecked surface query");
    require(!unchecked.surface_query_checked, "native operation records unchecked surface query");
    require(!unchecked.can_call_vk_create_swapchain(), "unchecked surface query blocks Vulkan create");

    const vulkan_backend::vulkan_native_device_create_result device =
        make_ready_native_device();
    vulkan_backend::fake_vulkan_native_surface_capability_query query(
        vulkan_backend::fake_vulkan_native_surface_capability_query_options{
            .capabilities = make_capabilities(),
            .present_queue_supported = false,
        });
    request = make_ready_native_swapchain_create_operation_request();
    request.surface_query = vulkan_backend::query_native_vulkan_surface_capabilities(
        query,
        make_ready_native_surface_query_dispatch_table(device),
        device,
        request.surface,
        7);
    const vulkan_backend::vulkan_native_swapchain_create_operation_result unsupported =
        vulkan_backend::build_vulkan_native_swapchain_create_operation_plan(request);
    require(
        unsupported.status
            == vulkan_backend::vulkan_native_swapchain_create_operation_status::surface_query_unavailable,
        "native operation blocks unsupported surface query");
    require(unsupported.surface_query_checked, "native operation records checked surface query");
    require(!unsupported.surface_query_ready, "native operation records blocked surface query");
    require(
        !unsupported.present_queue_supported,
        "native operation preserves unsupported present queue evidence");
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
    require(operation.surface_query_checked, "native operation records surface query check");
    require(operation.surface_query_ready, "native operation records ready surface query");
    require(operation.present_queue_supported, "native operation records present queue support");
    require(operation.surface_format_count == 2, "native operation records surface format count");
    require(operation.present_mode_count == 2, "native operation records present mode count");
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
    require(
        operation.operation.surface_query_ready,
        "native operation summary preserves surface query readiness");
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

void test_native_swapchain_operation_dispatch_table_resolves_create_destroy_symbols()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_swapchain_operation_dispatch_table dispatch =
        make_ready_native_swapchain_operation_dispatch_table();

    require(dispatch.checked, "swapchain operation dispatch table is checked");
    require(
        dispatch.status
            == vulkan_backend::vulkan_native_swapchain_operation_dispatch_table_status::ready,
        "swapchain operation dispatch table reports ready");
    require(dispatch.ready_for_create(), "swapchain operation dispatch reaches create gate");
    require(dispatch.ready_for_destroy(), "swapchain operation dispatch reaches destroy gate");
    require(
        dispatch.create_swapchain.value == 901,
        "swapchain operation dispatch records create symbol pointer");
    require(
        dispatch.destroy_swapchain.value == 902,
        "swapchain operation dispatch records destroy symbol pointer");

    const vulkan_backend::vulkan_native_swapchain_operation_dispatch_table missing_extension =
        vulkan_backend::collect_vulkan_native_swapchain_operation_dispatch_table(
            make_swapchain_function_table({}, false));
    require(
        missing_extension.status
            == vulkan_backend::vulkan_native_swapchain_operation_dispatch_table_status::required_extension_unavailable,
        "swapchain operation dispatch reports missing required extension");
    require(
        missing_extension.missing_required_extension == "VK_KHR_swapchain",
        "swapchain operation dispatch records missing extension name");

    const vulkan_backend::vulkan_native_swapchain_operation_dispatch_table missing_create =
        vulkan_backend::collect_vulkan_native_swapchain_operation_dispatch_table(
            make_swapchain_function_table({"vkCreateSwapchainKHR"}));
    require(
        missing_create.status
            == vulkan_backend::vulkan_native_swapchain_operation_dispatch_table_status::missing_create_symbol,
        "swapchain operation dispatch reports missing create symbol");
    require(
        missing_create.missing_symbol_name == "vkCreateSwapchainKHR",
        "swapchain operation dispatch records missing create symbol");

    const vulkan_backend::vulkan_native_swapchain_operation_dispatch_table missing_destroy =
        vulkan_backend::collect_vulkan_native_swapchain_operation_dispatch_table(
            make_swapchain_function_table({"vkDestroySwapchainKHR"}));
    require(
        missing_destroy.status
            == vulkan_backend::vulkan_native_swapchain_operation_dispatch_table_status::missing_destroy_symbol,
        "swapchain operation dispatch reports missing destroy symbol");
    require(
        missing_destroy.missing_symbol_name == "vkDestroySwapchainKHR",
        "swapchain operation dispatch records missing destroy symbol");
}

void test_native_swapchain_create_destroy_fake_operation_records_handoff()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_swapchain_operation operation(
        vulkan_backend::fake_vulkan_native_swapchain_operation_options{
            .created_swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 222},
        });
    const vulkan_backend::vulkan_native_swapchain_operation_dispatch_table dispatch =
        make_ready_native_swapchain_operation_dispatch_table();
    const vulkan_backend::vulkan_native_swapchain_create_operation_result create_operation =
        make_ready_native_swapchain_create_operation();

    const vulkan_backend::vulkan_native_swapchain_create_execution_result created =
        vulkan_backend::create_native_vulkan_swapchain(
            operation,
            vulkan_backend::vulkan_native_swapchain_create_execution_request{
                .create_operation = create_operation,
                .dispatch_table = dispatch,
            });

    require(created.checked, "native swapchain fake create execution is checked");
    require(
        created.status
            == vulkan_backend::vulkan_native_swapchain_create_execution_status::created,
        "native swapchain fake create execution reports created");
    require(created.ready_for_images(), "native swapchain fake create reaches image gate");
    require(!created.blocked(), "native swapchain fake create is not blocked");
    require(created.created_swapchain.value == 222, "fake create returns stable swapchain handle");
    require(created.old_swapchain.value == 99, "fake create preserves old swapchain handle");
    require(created.old_swapchain_handoff, "fake create records old swapchain handoff");
    require(created.selected_extent.width == 1280, "fake create preserves selected extent width");
    require(
        created.selected_surface_format.format
            == vulkan_backend::vulkan_swapchain_image_format::bgra8_unorm,
        "fake create preserves selected surface format");
    require(
        created.selected_present_mode == vulkan_backend::vulkan_swapchain_present_mode::mailbox,
        "fake create preserves selected present mode");
    require(created.selected_image_count == 3, "fake create preserves selected image count");
    require(
        created.selected_queue_family_count == 2,
        "fake create records selected queue family count");
    require(
        created.selected_queue_family_indices[0] == 0
            && created.selected_queue_family_indices[1] == 1,
        "fake create records graphics and present queue families");
    require(created.create_swapchain_symbol.value == 901, "fake create records create pointer");
    require(created.destroy_swapchain_symbol.value == 902, "fake create records destroy pointer");
    require(operation.state().create_call_count == 1, "fake create was called once");
    require(
        operation.state().requested_old_swapchain.value == 99,
        "fake create state records old swapchain handoff");
    require(
        operation.state().selected_present_mode
            == vulkan_backend::vulkan_swapchain_present_mode::mailbox,
        "fake create state records selected present mode");

    const vulkan_backend::vulkan_native_swapchain_destroy_execution_result destroyed =
        vulkan_backend::destroy_native_vulkan_swapchain(
            operation,
            vulkan_backend::vulkan_native_swapchain_destroy_execution_request{
                .dispatch_table = dispatch,
                .device = created.device,
                .swapchain = created.created_swapchain,
            });

    require(destroyed.checked, "native swapchain fake destroy execution is checked");
    require(
        destroyed.status
            == vulkan_backend::vulkan_native_swapchain_destroy_execution_status::destroyed,
        "native swapchain fake destroy execution reports destroyed");
    require(destroyed.destroyed(), "native swapchain fake destroy reaches destroyed gate");
    require(destroyed.destroy_swapchain_symbol.value == 902, "fake destroy records destroy pointer");
    require(operation.state().destroy_call_count == 1, "fake destroy was called once");
    require(
        operation.state().destroyed_swapchain.value == 222,
        "fake destroy state records destroyed swapchain");
}

void test_native_swapchain_create_destroy_fake_operation_reports_blockers()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_swapchain_operation_dispatch_table dispatch =
        make_ready_native_swapchain_operation_dispatch_table();
    vulkan_backend::fake_vulkan_native_swapchain_operation operation;

    const vulkan_backend::vulkan_native_swapchain_create_execution_result missing_operation =
        vulkan_backend::create_native_vulkan_swapchain(
            operation,
            vulkan_backend::vulkan_native_swapchain_create_execution_request{
                .create_operation = {},
                .dispatch_table = dispatch,
            });
    require(
        missing_operation.status
            == vulkan_backend::vulkan_native_swapchain_create_execution_status::create_operation_unavailable,
        "fake create blocks missing create operation");
    require(
        operation.state().create_call_count == 0,
        "fake create does not call native boundary when operation is missing");

    const vulkan_backend::vulkan_native_swapchain_create_execution_result missing_dispatch =
        vulkan_backend::create_native_vulkan_swapchain(
            operation,
            vulkan_backend::vulkan_native_swapchain_create_execution_request{
                .create_operation = make_ready_native_swapchain_create_operation(),
                .dispatch_table = {},
            });
    require(
        missing_dispatch.status
            == vulkan_backend::vulkan_native_swapchain_create_execution_status::dispatch_table_unavailable,
        "fake create blocks missing dispatch table");

    vulkan_backend::fake_vulkan_native_swapchain_operation failing_create(
        vulkan_backend::fake_vulkan_native_swapchain_operation_options{
            .fail_create = true,
            .create_failure_result = -7,
        });
    const vulkan_backend::vulkan_native_swapchain_create_execution_result failed_create =
        vulkan_backend::create_native_vulkan_swapchain(
            failing_create,
            vulkan_backend::vulkan_native_swapchain_create_execution_request{
                .create_operation = make_ready_native_swapchain_create_operation(),
                .dispatch_table = dispatch,
            });
    require(
        failed_create.status
            == vulkan_backend::vulkan_native_swapchain_create_execution_status::creation_failed,
        "fake create reports create failure");
    require(failed_create.native_result == -7, "fake create records native failure result");

    const vulkan_backend::vulkan_native_swapchain_destroy_execution_result missing_swapchain =
        vulkan_backend::destroy_native_vulkan_swapchain(
            operation,
            vulkan_backend::vulkan_native_swapchain_destroy_execution_request{
                .dispatch_table = dispatch,
                .device = vulkan_backend::vulkan_device_handle{.value = 77},
                .swapchain = {},
            });
    require(
        missing_swapchain.status
            == vulkan_backend::vulkan_native_swapchain_destroy_execution_status::missing_swapchain_handle,
        "fake destroy blocks missing swapchain handle");

    vulkan_backend::fake_vulkan_native_swapchain_operation failing_destroy(
        vulkan_backend::fake_vulkan_native_swapchain_operation_options{
            .fail_destroy = true,
        });
    const vulkan_backend::vulkan_native_swapchain_destroy_execution_result failed_destroy =
        vulkan_backend::destroy_native_vulkan_swapchain(
            failing_destroy,
            vulkan_backend::vulkan_native_swapchain_destroy_execution_request{
                .dispatch_table = dispatch,
                .device = vulkan_backend::vulkan_device_handle{.value = 77},
                .swapchain = vulkan_backend::vulkan_swapchain_handle{.value = 222},
            });
    require(
        failed_destroy.status
            == vulkan_backend::vulkan_native_swapchain_destroy_execution_status::destroy_failed,
        "fake destroy reports destroy failure");
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

void test_native_swapchain_acquire_operation_reports_ready_recordable_image()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result operation =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(
            make_ready_native_swapchain_acquire_operation_request());

    require(operation.checked, "native acquire operation is checked");
    require(operation.ready_for_command_recording(), "native acquire operation is recordable");
    require(!operation.blocked(), "ready native acquire operation is not blocked");
    require(
        operation.status == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::ready,
        "native acquire operation status is ready");
    require(operation.images_operation_checked, "native acquire operation records images check");
    require(operation.images_operation_ready, "native acquire operation records images readiness");
    require(operation.acquire_plan_checked, "native acquire operation records acquire plan check");
    require(
        operation.acquire_plan_ready_for_command_recording,
        "native acquire operation records acquire plan readiness");
    require(operation.native_entrypoints_checked, "native acquire operation records native check");
    require(operation.required_extensions_ready, "native acquire operation records extension readiness");
    require(operation.acquire_symbol_ready, "native acquire operation records acquire symbol readiness");
    require(operation.device_valid, "native acquire operation records valid device");
    require(operation.swapchain_valid, "native acquire operation records valid swapchain");
    require(operation.image_binding_ready, "native acquire operation records image binding readiness");
    require(operation.vk_acquire_next_image_callable, "native acquire operation exposes call gate");
    require(
        operation.command_recording_may_consume_acquired_image,
        "native acquire operation exposes command recording handoff");
    require(operation.selected_image_index == 2, "native acquire operation preserves selected index");
    require(operation.image_id.value == 2, "native acquire operation preserves selected image id");
    require(operation.image_handle.value == 5002, "native acquire operation selects opaque image handle");
    require(operation.timeout_nanoseconds == 16000000, "native acquire operation preserves timeout");
    require(operation.image_available, "native acquire operation records image availability");
    require(operation.image_acquired, "native acquire operation records image acquired");
    require(operation.operation.ready_for_command_recording(), "native acquire summary is recordable");
    require(
        operation.operation.entrypoint_name == "vkAcquireNextImageKHR",
        "native acquire summary names the stable entrypoint");
}

void test_native_swapchain_acquire_operation_reports_timeout_outdated_suboptimal_and_error()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_swapchain_acquire_operation_request request =
        make_ready_native_swapchain_acquire_operation_request();
    request.acquire_plan =
        make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::timeout, 0);
    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result timeout =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(request);
    require(
        timeout.status == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::timeout,
        "native acquire operation reports timeout");
    require(timeout.vk_acquire_next_image_callable, "timeout still records callable native acquire");
    require(timeout.timed_out, "native acquire operation records timeout flag");
    require(!timeout.ready_for_command_recording(), "timeout is not recordable");

    request = make_ready_native_swapchain_acquire_operation_request();
    request.acquire_plan =
        make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::out_of_date, 0);
    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result out_of_date =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(request);
    require(
        out_of_date.status
            == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::out_of_date,
        "native acquire operation reports out-of-date");
    require(out_of_date.out_of_date, "native acquire operation records out-of-date flag");

    request = make_ready_native_swapchain_acquire_operation_request();
    request.acquire_plan =
        make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::suboptimal, 3);
    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result suboptimal =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(request);
    require(
        suboptimal.status
            == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::suboptimal,
        "native acquire operation reports suboptimal");
    require(suboptimal.suboptimal, "native acquire operation records suboptimal flag");
    require(
        suboptimal.ready_for_command_recording(),
        "recordable suboptimal acquire may feed command recording");
    require(suboptimal.image_handle.value == 5003, "suboptimal acquire keeps selected image handle");

    request = make_ready_native_swapchain_acquire_operation_request();
    request.acquire_plan =
        make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::error, 0);
    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result error =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(request);
    require(
        error.status == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::error,
        "native acquire operation reports error");
    require(error.error, "native acquire operation records error flag");
}

void test_native_swapchain_acquire_operation_blocks_image_and_handle_prerequisites()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_swapchain_acquire_operation_request request =
        make_ready_native_swapchain_acquire_operation_request();
    request.images_operation = {};
    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result missing_images =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(request);
    require(
        missing_images.status
            == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::images_operation_unavailable,
        "native acquire operation blocks missing images operation");

    request = make_ready_native_swapchain_acquire_operation_request();
    request.images_operation.device = {};
    request.images_operation.device_valid = false;
    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result invalid_device =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(request);
    require(
        invalid_device.status
            == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::invalid_device,
        "native acquire operation blocks invalid device");

    request = make_ready_native_swapchain_acquire_operation_request();
    request.images_operation.swapchain = {};
    request.images_operation.swapchain_valid = false;
    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result missing_swapchain =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(request);
    require(
        missing_swapchain.status
            == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::missing_swapchain_handle,
        "native acquire operation blocks missing swapchain handle");

    request = make_ready_native_swapchain_acquire_operation_request();
    request.acquire_plan =
        make_acquire_plan(vulkan_backend::vulkan_swapchain_acquire_status::acquired, 9);
    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result missing_binding =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(request);
    require(
        missing_binding.status
            == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::image_binding_unavailable,
        "native acquire operation blocks missing selected image binding");
    require(!missing_binding.image_binding_ready, "native acquire operation records missing binding");
}

void test_native_swapchain_acquire_operation_blocks_native_extension_and_entrypoint()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_native_swapchain_acquire_operation_request request =
        make_ready_native_swapchain_acquire_operation_request();
    request.native_entrypoints.required_extensions_ready = false;
    request.native_entrypoints.missing_required_extension = "VK_KHR_swapchain";
    request.native_entrypoints.diagnostic = "missing VK_KHR_swapchain";
    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result missing_extension =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(request);
    require(
        missing_extension.status
            == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::required_extension_unavailable,
        "native acquire operation blocks missing required extension");
    require(
        missing_extension.missing_required_extension == "VK_KHR_swapchain",
        "native acquire operation records missing extension");

    request = make_ready_native_swapchain_acquire_operation_request();
    request.native_entrypoints.acquire_next_image_ready = false;
    request.native_entrypoints.missing_symbol_name = "vkAcquireNextImageKHR";
    const vulkan_backend::vulkan_native_swapchain_acquire_operation_result missing_symbol =
        vulkan_backend::build_vulkan_native_swapchain_acquire_operation_plan(request);
    require(
        missing_symbol.status
            == vulkan_backend::vulkan_native_swapchain_acquire_operation_status::missing_acquire_symbol,
        "native acquire operation blocks missing acquire entrypoint");
    require(
        missing_symbol.missing_symbol_name == "vkAcquireNextImageKHR",
        "native acquire operation records missing acquire symbol");
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
            vulkan_backend::vulkan_native_swapchain_create_operation_status::surface_query_unavailable)
            == std::string_view{"surface_query_unavailable"},
        "native swapchain create operation surface query status name is stable");
    require(
        vulkan_backend::native_swapchain_create_operation_status_name(
            vulkan_backend::vulkan_native_swapchain_create_operation_status::recreate_incompatible)
            == std::string_view{"recreate_incompatible"},
        "native swapchain create operation recreate status name is stable");
    require(
        vulkan_backend::native_swapchain_operation_dispatch_table_status_name(
            vulkan_backend::vulkan_native_swapchain_operation_dispatch_table_status::missing_create_symbol)
            == std::string_view{"missing_create_symbol"},
        "native swapchain operation dispatch missing create status name is stable");
    require(
        vulkan_backend::native_swapchain_create_execution_status_name(
            vulkan_backend::vulkan_native_swapchain_create_execution_status::created)
            == std::string_view{"created"},
        "native swapchain create execution created status name is stable");
    require(
        vulkan_backend::native_swapchain_destroy_execution_status_name(
            vulkan_backend::vulkan_native_swapchain_destroy_execution_status::destroyed)
            == std::string_view{"destroyed"},
        "native swapchain destroy execution destroyed status name is stable");
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
    require(
        vulkan_backend::native_swapchain_acquire_operation_status_name(
            vulkan_backend::vulkan_native_swapchain_acquire_operation_status::ready)
            == std::string_view{"ready"},
        "native swapchain acquire operation ready status name is stable");
    require(
        vulkan_backend::native_swapchain_acquire_operation_status_name(
            vulkan_backend::vulkan_native_swapchain_acquire_operation_status::timeout)
            == std::string_view{"timeout"},
        "native swapchain acquire operation timeout status name is stable");
    require(
        vulkan_backend::native_swapchain_acquire_operation_status_name(
            vulkan_backend::vulkan_native_swapchain_acquire_operation_status::missing_acquire_symbol)
            == std::string_view{"missing_acquire_symbol"},
        "native swapchain acquire operation missing symbol status name is stable");
}

} // namespace

int main()
{
    test_swapchain_create_plan_selects_requested_supported_values();
    test_swapchain_create_plan_selects_vsync_fifo_and_clamps_extent_and_image_count();
    test_swapchain_create_plan_reports_missing_format_and_present_mode();
    test_swapchain_create_plan_reports_unsupported_zero_extent();
    test_swapchain_create_plan_reports_recreate_compatibility();
    test_native_surface_query_dispatch_resolves_surface_symbols();
    test_native_surface_query_dispatch_reports_missing_symbols();
    test_native_surface_capability_query_records_ready_evidence();
    test_native_surface_capability_query_reports_blockers();
    test_native_swapchain_create_operation_blocks_missing_surface_query();
    test_native_swapchain_create_operation_reports_ready_operation_summary();
    test_native_swapchain_create_operation_blocks_plan_and_recreate_compatibility();
    test_native_swapchain_create_operation_blocks_invalid_device_and_surface();
    test_native_swapchain_create_operation_blocks_native_extension_and_symbols();
    test_native_swapchain_operation_dispatch_table_resolves_create_destroy_symbols();
    test_native_swapchain_create_destroy_fake_operation_records_handoff();
    test_native_swapchain_create_destroy_fake_operation_reports_blockers();
    test_native_swapchain_images_operation_reports_ready_operation_summary();
    test_native_swapchain_images_operation_blocks_create_and_swapchain_prerequisites();
    test_native_swapchain_images_operation_blocks_native_extension_and_entrypoint();
    test_native_swapchain_images_operation_blocks_missing_image_count();
    test_native_swapchain_acquire_operation_reports_ready_recordable_image();
    test_native_swapchain_acquire_operation_reports_timeout_outdated_suboptimal_and_error();
    test_native_swapchain_acquire_operation_blocks_image_and_handle_prerequisites();
    test_native_swapchain_acquire_operation_blocks_native_extension_and_entrypoint();
    test_swapchain_create_plan_names_are_stable();
    return 0;
}
