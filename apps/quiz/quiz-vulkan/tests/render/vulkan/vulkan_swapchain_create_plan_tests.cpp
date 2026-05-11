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
}

} // namespace

int main()
{
    test_swapchain_create_plan_selects_requested_supported_values();
    test_swapchain_create_plan_selects_vsync_fifo_and_clamps_extent_and_image_count();
    test_swapchain_create_plan_reports_missing_format_and_present_mode();
    test_swapchain_create_plan_reports_unsupported_zero_extent();
    test_swapchain_create_plan_reports_recreate_compatibility();
    test_swapchain_create_plan_names_are_stable();
    return 0;
}
