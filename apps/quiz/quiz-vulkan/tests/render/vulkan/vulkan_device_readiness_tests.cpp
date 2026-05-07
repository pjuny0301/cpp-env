#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"

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
        .handle = vulkan_backend::vulkan_instance_handle{.value = 77},
        .selected_extensions = {"VK_KHR_surface"},
        .enabled_layers = {},
        .diagnostic = "Vulkan instance created",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_instance_create_result make_failed_instance()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_instance_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_instance_create_status::creation_failed,
        .loader = make_ready_loader(),
        .handle = {},
        .selected_extensions = {"VK_KHR_surface"},
        .enabled_layers = {},
        .diagnostic = "Vulkan instance creation failed",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_device_create_request make_device_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_device_create_request{
        .required_device_extensions = {"VK_KHR_swapchain"},
        .optional_device_extensions = {"VK_EXT_memory_budget", "VK_EXT_missing_optional"},
        .required_queue_capabilities = {
            vulkan_backend::vulkan_device_queue_capability::graphics,
            vulkan_backend::vulkan_device_queue_capability::present,
        },
    };
}

quiz_vulkan::render::vulkan_backend::fake_vulkan_device_factory make_device_factory()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::fake_vulkan_device_factory(
        vulkan_backend::fake_vulkan_device_factory_options{
            .supported_device_extensions = {"VK_KHR_swapchain", "VK_EXT_memory_budget"},
            .supported_queue_capabilities = {
                vulkan_backend::vulkan_device_queue_capability::graphics,
                vulkan_backend::vulkan_device_queue_capability::present,
            },
            .device_available = true,
            .fail_creation = false,
            .handle = vulkan_backend::vulkan_device_handle{.value = 99},
            .queue_handle_base = 1000,
        });
}

void test_device_factory_marks_created_instance_device_ready()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_device_factory factory = make_device_factory();
    const vulkan_backend::vulkan_device_create_result result =
        vulkan_backend::create_vulkan_device(
            factory,
            make_created_instance(),
            make_device_request());

    require(result.checked, "device result records that selection was checked");
    require(
        result.status == vulkan_backend::vulkan_device_create_status::created,
        "device result reports created status");
    require(result.ready_for_backend(), "created device result reaches the backend device gate");
    require(result.instance.ready_for_device(), "created device preserves ready instance result");
    require(result.handle.valid(), "created device returns a valid opaque device handle");
    require(result.handle.value == 99, "created device returns configured fake device handle");
    require(result.selected_extensions.size() == 2, "created device selects required and supported optional extension");
    require(
        result.selected_extensions[0] == "VK_KHR_swapchain",
        "created device selects the required swapchain extension first");
    require(
        result.selected_extensions[1] == "VK_EXT_memory_budget",
        "created device selects supported optional memory budget extension");
    require(result.selected_queues.size() == 2, "created device selects graphics and present queues");
    require(
        result.selected_queues[0].capability
            == vulkan_backend::vulkan_device_queue_capability::graphics,
        "created device selects graphics queue first");
    require(result.selected_queues[0].family_index == 0, "graphics queue family index is stable");
    require(result.selected_queues[0].queue.value == 1001, "graphics queue handle is stable");
    require(
        result.selected_queues[1].capability
            == vulkan_backend::vulkan_device_queue_capability::present,
        "created device selects present queue second");
    require(result.selected_queues[1].family_index == 1, "present queue family index is stable");
    require(result.selected_queues[1].queue.value == 1002, "present queue handle is stable");

    const vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle =
        vulkan_backend::apply_vulkan_device_create_result_to_lifecycle({}, result);
    require(lifecycle.instance_ready, "device lifecycle preserves instance readiness");
    require(lifecycle.device_ready, "device lifecycle marks backend device ready");
    require(lifecycle.effective_instance_ready(), "device lifecycle effective instance readiness is true");
    require(lifecycle.effective_device_ready(), "device lifecycle effective device readiness is true");
}

void test_device_factory_maps_instance_failure_to_device_unavailable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_device_factory factory = make_device_factory();
    const vulkan_backend::vulkan_device_create_result result =
        vulkan_backend::create_vulkan_device(
            factory,
            make_failed_instance(),
            make_device_request());

    require(result.checked, "instance-failure device result is checked");
    require(
        result.status == vulkan_backend::vulkan_device_create_status::instance_unavailable,
        "instance-failure device result reports unavailable instance status");
    require(!result.ready_for_backend(), "instance-failure device result does not reach device gate");
    require(!result.handle.valid(), "instance-failure device result has no device handle");
    require(result.selected_extensions.empty(), "instance-failure device result selects no extensions");
    require(result.selected_queues.empty(), "instance-failure device result selects no queues");
    require(
        result.diagnostic == "Vulkan instance is not ready for device creation",
        "instance-failure device result reports instance diagnostic");
}

void test_device_factory_reports_missing_required_device_extension()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_device_factory factory(
        vulkan_backend::fake_vulkan_device_factory_options{
            .supported_device_extensions = {"VK_EXT_memory_budget"},
            .supported_queue_capabilities = {
                vulkan_backend::vulkan_device_queue_capability::graphics,
                vulkan_backend::vulkan_device_queue_capability::present,
            },
            .device_available = true,
            .fail_creation = false,
            .handle = vulkan_backend::vulkan_device_handle{.value = 15},
            .queue_handle_base = 2000,
        });

    const vulkan_backend::vulkan_device_create_result result =
        vulkan_backend::create_vulkan_device(
            factory,
            make_created_instance(),
            make_device_request());

    require(result.checked, "missing-extension device result is checked");
    require(
        result.status == vulkan_backend::vulkan_device_create_status::missing_required_device_extension,
        "missing-extension device result reports missing required device extension");
    require(!result.ready_for_backend(), "missing-extension device result does not reach device gate");
    require(!result.handle.valid(), "missing-extension device result has no device handle");
    require(result.selected_extensions.empty(), "missing-extension device result selects no extensions");
    require(
        result.diagnostic == "missing required device extension: VK_KHR_swapchain",
        "missing-extension device result records diagnostic extension name");
}

void test_device_factory_reports_missing_required_queue_and_keeps_frame_at_device_gate()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_device_factory factory(
        vulkan_backend::fake_vulkan_device_factory_options{
            .supported_device_extensions = {"VK_KHR_swapchain", "VK_EXT_memory_budget"},
            .supported_queue_capabilities = {
                vulkan_backend::vulkan_device_queue_capability::graphics,
            },
            .device_available = true,
            .fail_creation = false,
            .handle = vulkan_backend::vulkan_device_handle{.value = 21},
            .queue_handle_base = 3000,
        });

    const vulkan_backend::vulkan_device_create_result result =
        vulkan_backend::create_vulkan_device(
            factory,
            make_created_instance(),
            make_device_request());

    require(result.checked, "missing-queue device result is checked");
    require(
        result.status == vulkan_backend::vulkan_device_create_status::missing_required_queue,
        "missing-queue device result reports missing required queue");
    require(!result.ready_for_backend(), "missing-queue device result does not reach device gate");
    require(!result.handle.valid(), "missing-queue device result has no device handle");
    require(result.selected_extensions.size() == 2, "missing-queue device result preserves selected extensions");
    require(result.selected_queues.size() == 1, "missing-queue device result preserves earlier queue selection");
    require(
        result.diagnostic == "missing required device queue: present",
        "missing-queue device result records diagnostic queue name");

    vulkan_backend::null_vulkan_backend_device device(result);
    const quiz_vulkan::render::render_draw_list draw_list;
    const vulkan_backend::vulkan_backend_frame_result frame_result =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require(frame_result.lifecycle.instance_ready, "missing-queue frame keeps instance ready");
    require(!frame_result.lifecycle.device_ready, "missing-queue frame keeps device unavailable");
    require(
        frame_result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::device_unavailable,
        "missing-queue frame fallback stays at device unavailable");
}

void test_vulkan_device_status_and_queue_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::device_queue_capability_name(
            vulkan_backend::vulkan_device_queue_capability::graphics)
            == std::string_view{"graphics"},
        "graphics queue capability name is stable");
    require(
        vulkan_backend::device_queue_capability_name(
            vulkan_backend::vulkan_device_queue_capability::present)
            == std::string_view{"present"},
        "present queue capability name is stable");
    require(
        vulkan_backend::device_queue_capability_name(
            vulkan_backend::vulkan_device_queue_capability::compute)
            == std::string_view{"compute"},
        "compute queue capability name is stable");
    require(
        vulkan_backend::device_queue_capability_name(
            vulkan_backend::vulkan_device_queue_capability::transfer)
            == std::string_view{"transfer"},
        "transfer queue capability name is stable");
    require(
        vulkan_backend::device_create_status_name(
            vulkan_backend::vulkan_device_create_status::not_requested)
            == std::string_view{"not_requested"},
        "device create status name for not requested is stable");
    require(
        vulkan_backend::device_create_status_name(
            vulkan_backend::vulkan_device_create_status::created)
            == std::string_view{"created"},
        "device create status name for created is stable");
    require(
        vulkan_backend::device_create_status_name(
            vulkan_backend::vulkan_device_create_status::instance_unavailable)
            == std::string_view{"instance_unavailable"},
        "device create status name for instance unavailable is stable");
    require(
        vulkan_backend::device_create_status_name(
            vulkan_backend::vulkan_device_create_status::no_suitable_device)
            == std::string_view{"no_suitable_device"},
        "device create status name for no suitable device is stable");
    require(
        vulkan_backend::device_create_status_name(
            vulkan_backend::vulkan_device_create_status::missing_required_device_extension)
            == std::string_view{"missing_required_device_extension"},
        "device create status name for missing required device extension is stable");
    require(
        vulkan_backend::device_create_status_name(
            vulkan_backend::vulkan_device_create_status::missing_required_queue)
            == std::string_view{"missing_required_queue"},
        "device create status name for missing required queue is stable");
    require(
        vulkan_backend::device_create_status_name(
            vulkan_backend::vulkan_device_create_status::creation_failed)
            == std::string_view{"creation_failed"},
        "device create status name for creation failed is stable");
}

} // namespace

int main()
{
    test_device_factory_marks_created_instance_device_ready();
    test_device_factory_maps_instance_failure_to_device_unavailable();
    test_device_factory_reports_missing_required_device_extension();
    test_device_factory_reports_missing_required_queue_and_keeps_frame_at_device_gate();
    test_vulkan_device_status_and_queue_names_are_stable();
    return 0;
}
