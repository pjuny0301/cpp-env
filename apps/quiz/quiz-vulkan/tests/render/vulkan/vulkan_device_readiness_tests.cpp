#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
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

quiz_vulkan::render::vulkan_backend::vulkan_native_instance_function_table
make_native_instance_function_table()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver;
    return vulkan_backend::collect_vulkan_native_instance_function_table(resolver);
}

quiz_vulkan::render::vulkan_backend::vulkan_native_instance_create_result
make_created_native_instance(
    const quiz_vulkan::render::vulkan_backend::vulkan_native_instance_function_table& table)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_instance_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_native_instance_create_status::created,
        .loader = make_ready_loader(),
        .function_table = table,
        .request = vulkan_backend::vulkan_instance_create_request{},
        .handle = vulkan_backend::vulkan_instance_handle{.value = 84},
        .native_result = 0,
        .diagnostic = "created test native instance",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_physical_device_dispatch_table
make_ready_physical_device_dispatch_table()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_instance_function_table function_table =
        make_native_instance_function_table();
    vulkan_backend::fake_vulkan_native_instance_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_instance_symbol_resolver_options{
            .get_instance_proc_address = function_table.get_instance_proc_address,
        });

    return vulkan_backend::collect_vulkan_native_physical_device_dispatch_table(
        resolver,
        make_created_native_instance(function_table));
}

quiz_vulkan::render::vulkan_backend::vulkan_native_physical_device_enumeration_result
make_physical_device_enumeration(
    std::vector<quiz_vulkan::render::vulkan_backend::vulkan_physical_device_handle>
        physical_devices)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_physical_device_enumerator enumerator(
        vulkan_backend::fake_vulkan_native_physical_device_enumerator_options{
            .physical_devices = std::move(physical_devices),
        });
    return vulkan_backend::enumerate_native_vulkan_physical_devices(
        enumerator,
        make_ready_physical_device_dispatch_table());
}

quiz_vulkan::render::vulkan_backend::vulkan_native_queue_family_query_result
make_queue_family_query(
    const quiz_vulkan::render::vulkan_backend::vulkan_native_physical_device_enumeration_result&
        enumeration,
    std::vector<quiz_vulkan::render::vulkan_backend::vulkan_native_physical_device_queue_family>
        queue_families)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_queue_family_query query(
        vulkan_backend::fake_vulkan_native_queue_family_query_options{
            .queue_families = std::move(queue_families),
        });
    return vulkan_backend::query_native_vulkan_physical_device_queue_families(
        query,
        enumeration);
}

quiz_vulkan::render::vulkan_backend::vulkan_native_physical_device_selection_result
make_selected_physical_device()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_physical_device_handle first_device{.value = 9101};
    const vulkan_backend::vulkan_physical_device_handle selected_device{.value = 9102};
    const vulkan_backend::vulkan_native_physical_device_enumeration_result enumeration =
        make_physical_device_enumeration({first_device, selected_device});
    const vulkan_backend::vulkan_native_queue_family_query_result queue_family_result =
        make_queue_family_query(
            enumeration,
            {
                vulkan_backend::vulkan_native_physical_device_queue_family{
                    .physical_device = first_device,
                    .family_index = 1,
                    .queue_count = 1,
                    .capabilities = {vulkan_backend::vulkan_device_queue_capability::graphics},
                },
                vulkan_backend::vulkan_native_physical_device_queue_family{
                    .physical_device = selected_device,
                    .family_index = 4,
                    .queue_count = 2,
                    .capabilities = {vulkan_backend::vulkan_device_queue_capability::graphics},
                },
                vulkan_backend::vulkan_native_physical_device_queue_family{
                    .physical_device = selected_device,
                    .family_index = 7,
                    .queue_count = 1,
                    .capabilities = {vulkan_backend::vulkan_device_queue_capability::present},
                },
            }
        );
    vulkan_backend::fake_vulkan_native_physical_device_selector selector;
    return vulkan_backend::select_native_vulkan_physical_device(
        selector,
        enumeration,
        queue_family_result,
        make_device_request());
}

quiz_vulkan::render::vulkan_backend::vulkan_native_device_dispatch_table
make_ready_native_device_dispatch_table(
    const quiz_vulkan::render::vulkan_backend::vulkan_native_physical_device_selection_result&
        selection)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_instance_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_instance_symbol_resolver_options{
            .get_instance_proc_address =
                selection.enumeration.dispatch_table.get_instance_proc_address,
        });
    return vulkan_backend::collect_vulkan_native_device_dispatch_table(
        resolver,
        selection);
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
    require(
        result.required_extensions_ready(),
        "created device records required device extensions ready");
    require(
        result.required_extension_count == 1,
        "created device counts required device extensions");
    require(
        result.available_required_extension_count == 1,
        "created device counts available required device extensions");
    require(
        result.missing_required_extension.empty(),
        "created device records no missing required device extension");
    require(
        result.required_extension_diagnostics.size() == 1,
        "created device records one required device extension diagnostic");
    require(
        result.required_extension_diagnostics.front().extension_name == "VK_KHR_swapchain",
        "created device records required swapchain extension diagnostic name");
    require(
        result.required_extension_diagnostics.front().available,
        "created device records required swapchain extension available");
    require(
        result.required_extension_diagnostics.front().selected,
        "created device records required swapchain extension selected");
    require(
        !result.required_extension_diagnostics.front().missing_required(),
        "created device required swapchain diagnostic is not missing");
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
    require(
        result.required_extension_diagnostics.empty(),
        "instance-failure device result skips device extension diagnostics");
    require(
        result.required_extension_count == 0,
        "instance-failure device result leaves required extension count at zero");
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
        !result.required_extensions_ready(),
        "missing-extension device result records required extensions not ready");
    require(
        result.required_extension_count == 1,
        "missing-extension device result counts required extension checks");
    require(
        result.available_required_extension_count == 0,
        "missing-extension device result counts no available required extensions");
    require(
        result.missing_required_extension == "VK_KHR_swapchain",
        "missing-extension device result records missing extension field");
    require(
        result.required_extension_diagnostics.size() == 1,
        "missing-extension device result records one required extension diagnostic");
    require(
        result.required_extension_diagnostics.front().extension_name == "VK_KHR_swapchain",
        "missing-extension diagnostic records swapchain extension name");
    require(
        !result.required_extension_diagnostics.front().available,
        "missing-extension diagnostic records unavailable required extension");
    require(
        !result.required_extension_diagnostics.front().selected,
        "missing-extension diagnostic records unselected required extension");
    require(
        result.required_extension_diagnostics.front().missing_required(),
        "missing-extension diagnostic marks missing required extension");
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
    require(
        result.required_extensions_ready(),
        "missing-queue device result keeps required extension diagnostics ready");
    require(
        result.required_extension_diagnostics.size() == 1,
        "missing-queue device result preserves required extension diagnostic");
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

void test_physical_device_dispatch_reports_no_instance()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_instance_function_table function_table =
        make_native_instance_function_table();
    vulkan_backend::fake_vulkan_native_instance_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_instance_symbol_resolver_options{
            .get_instance_proc_address = function_table.get_instance_proc_address,
        });

    const vulkan_backend::vulkan_native_physical_device_dispatch_table dispatch_table =
        vulkan_backend::collect_vulkan_native_physical_device_dispatch_table(
            resolver,
            vulkan_backend::vulkan_native_instance_create_result{});

    require(dispatch_table.checked, "physical device dispatch no-instance result is checked");
    require(
        dispatch_table.status
            == vulkan_backend::vulkan_native_physical_device_dispatch_table_status::instance_unavailable,
        "physical device dispatch maps missing native instance");
    require(
        !dispatch_table.ready_for_enumeration(),
        "physical device dispatch is not ready without an instance");
    require(
        resolver.state().resolve_call_count == 0,
        "physical device dispatch does not resolve symbols without an instance");
}

void test_physical_device_dispatch_reports_missing_symbol()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_instance_function_table function_table =
        make_native_instance_function_table();
    vulkan_backend::fake_vulkan_native_instance_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_instance_symbol_resolver_options{
            .missing_symbols = {"vkEnumeratePhysicalDevices"},
            .get_instance_proc_address = function_table.get_instance_proc_address,
        });

    const vulkan_backend::vulkan_native_physical_device_dispatch_table dispatch_table =
        vulkan_backend::collect_vulkan_native_physical_device_dispatch_table(
            resolver,
            make_created_native_instance(function_table));

    require(dispatch_table.checked, "physical device dispatch missing-symbol result is checked");
    require(
        dispatch_table.status
            == vulkan_backend::vulkan_native_physical_device_dispatch_table_status::missing_enumerate_physical_devices_symbol,
        "physical device dispatch maps missing vkEnumeratePhysicalDevices");
    require(
        dispatch_table.missing_symbol_name == "vkEnumeratePhysicalDevices",
        "physical device dispatch records the missing enumerate symbol");
    require(
        resolver.state().resolve_call_count == 1,
        "physical device dispatch attempts the missing enumerate symbol");
    require(
        resolver.state().requested_symbols.front() == "vkEnumeratePhysicalDevices",
        "physical device dispatch requests vkEnumeratePhysicalDevices");
}

void test_physical_device_dispatch_reports_missing_queue_family_symbol()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_instance_function_table function_table =
        make_native_instance_function_table();
    vulkan_backend::fake_vulkan_native_instance_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_instance_symbol_resolver_options{
            .missing_symbols = {"vkGetPhysicalDeviceQueueFamilyProperties"},
            .get_instance_proc_address = function_table.get_instance_proc_address,
        });

    const vulkan_backend::vulkan_native_physical_device_dispatch_table dispatch_table =
        vulkan_backend::collect_vulkan_native_physical_device_dispatch_table(
            resolver,
            make_created_native_instance(function_table));

    require(
        dispatch_table.status
            == vulkan_backend::vulkan_native_physical_device_dispatch_table_status::missing_get_physical_device_queue_family_properties_symbol,
        "physical device dispatch maps missing queue-family properties symbol");
    require(
        dispatch_table.missing_symbol_name == "vkGetPhysicalDeviceQueueFamilyProperties",
        "physical device dispatch records the missing queue-family symbol");
    require(
        !dispatch_table.ready_for_queue_family_query(),
        "physical device dispatch is not ready for queue-family query without symbol");
    require(
        resolver.state().resolve_call_count == 2,
        "physical device dispatch resolves enumerate before missing queue-family symbol");
    require(
        resolver.state().requested_symbols.back()
            == "vkGetPhysicalDeviceQueueFamilyProperties",
        "physical device dispatch requests queue-family properties after enumerate");
}

void test_physical_device_enumerator_reports_zero_devices()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_dispatch_table dispatch_table =
        make_ready_physical_device_dispatch_table();
    vulkan_backend::fake_vulkan_native_physical_device_enumerator enumerator(
        vulkan_backend::fake_vulkan_native_physical_device_enumerator_options{
            .physical_devices = {},
        });

    const vulkan_backend::vulkan_native_physical_device_enumeration_result result =
        vulkan_backend::enumerate_native_vulkan_physical_devices(
            enumerator,
            dispatch_table);

    require(result.checked, "physical device zero-device enumeration result is checked");
    require(
        result.status
            == vulkan_backend::vulkan_native_physical_device_enumeration_status::no_devices,
        "physical device enumeration maps zero devices");
    require(result.physical_device_count == 0, "physical device enumeration records zero count");
    require(result.physical_devices.empty(), "physical device enumeration records no handles");
    require(
        !result.ready_for_device_selection(),
        "zero physical devices are not ready for device selection");
    require(
        enumerator.state().enumerate_call_count == 1,
        "physical device enumerator is called once for a ready dispatch table");
}

void test_physical_device_enumerator_reports_usable_devices()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_dispatch_table dispatch_table =
        make_ready_physical_device_dispatch_table();
    vulkan_backend::fake_vulkan_native_physical_device_enumerator enumerator(
        vulkan_backend::fake_vulkan_native_physical_device_enumerator_options{
            .physical_devices = {
                vulkan_backend::vulkan_physical_device_handle{.value = 9001},
                vulkan_backend::vulkan_physical_device_handle{.value = 9002},
            },
        });

    const vulkan_backend::vulkan_native_physical_device_enumeration_result result =
        vulkan_backend::enumerate_native_vulkan_physical_devices(
            enumerator,
            dispatch_table);

    require(result.checked, "physical device enumeration result is checked");
    require(
        result.status
            == vulkan_backend::vulkan_native_physical_device_enumeration_status::ready,
        "physical device enumeration reports ready");
    require(
        result.ready_for_device_selection(),
        "physical device enumeration is ready for device selection");
    require(result.physical_device_count == 2, "physical device enumeration records count");
    require(result.physical_devices.size() == 2, "physical device enumeration records handles");
    require(result.physical_devices[0].value == 9001, "first physical device handle is stable");
    require(result.physical_devices[1].value == 9002, "second physical device handle is stable");
    require(
        enumerator.state().last_instance.value == dispatch_table.instance.value,
        "physical device enumerator records the dispatch instance handle");
    require(
        enumerator.state().last_enumerate_physical_devices.value
            == dispatch_table.enumerate_physical_devices.value,
        "physical device enumerator records the dispatch enumerate pointer");
}

void test_queue_family_query_reports_no_instance()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_instance_function_table function_table =
        make_native_instance_function_table();
    vulkan_backend::fake_vulkan_native_instance_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_instance_symbol_resolver_options{
            .get_instance_proc_address = function_table.get_instance_proc_address,
        });
    const vulkan_backend::vulkan_native_physical_device_dispatch_table dispatch_table =
        vulkan_backend::collect_vulkan_native_physical_device_dispatch_table(
            resolver,
            vulkan_backend::vulkan_native_instance_create_result{});
    vulkan_backend::fake_vulkan_native_physical_device_enumerator enumerator;
    const vulkan_backend::vulkan_native_physical_device_enumeration_result enumeration =
        vulkan_backend::enumerate_native_vulkan_physical_devices(
            enumerator,
            dispatch_table);

    vulkan_backend::fake_vulkan_native_queue_family_query query;
    const vulkan_backend::vulkan_native_queue_family_query_result result =
        vulkan_backend::query_native_vulkan_physical_device_queue_families(
            query,
            enumeration);

    require(result.checked, "queue-family no-instance result is checked");
    require(
        result.status
            == vulkan_backend::vulkan_native_queue_family_query_status::enumeration_unavailable,
        "queue-family query maps unavailable enumeration from missing instance");
    require(
        !result.ready_for_selection(),
        "queue-family query is not ready without a native instance");
    require(
        query.state().query_call_count == 0,
        "queue-family query does not run without ready enumeration");
}

void test_queue_family_query_reports_zero_devices()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_enumeration_result enumeration =
        make_physical_device_enumeration({});
    vulkan_backend::fake_vulkan_native_queue_family_query query;
    const vulkan_backend::vulkan_native_queue_family_query_result result =
        vulkan_backend::query_native_vulkan_physical_device_queue_families(
            query,
            enumeration);

    require(result.checked, "queue-family zero-device result is checked");
    require(
        result.status == vulkan_backend::vulkan_native_queue_family_query_status::no_devices,
        "queue-family query maps zero devices");
    require(result.queue_family_count == 0, "queue-family query records zero count");
    require(result.queue_families.empty(), "queue-family query records no families");
    require(
        !result.ready_for_selection(),
        "queue-family query is not ready when enumeration has no devices");
}

void test_queue_family_query_reports_usable_families()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_physical_device_handle first_device{.value = 9101};
    const vulkan_backend::vulkan_physical_device_handle second_device{.value = 9102};
    const vulkan_backend::vulkan_native_physical_device_enumeration_result enumeration =
        make_physical_device_enumeration({first_device, second_device});
    vulkan_backend::fake_vulkan_native_queue_family_query query(
        vulkan_backend::fake_vulkan_native_queue_family_query_options{
            .queue_families = {
                vulkan_backend::vulkan_native_physical_device_queue_family{
                    .physical_device = first_device,
                    .family_index = 3,
                    .queue_count = 2,
                    .capabilities = {vulkan_backend::vulkan_device_queue_capability::graphics},
                },
                vulkan_backend::vulkan_native_physical_device_queue_family{
                    .physical_device = second_device,
                    .family_index = 5,
                    .queue_count = 1,
                    .capabilities = {vulkan_backend::vulkan_device_queue_capability::present},
                },
            },
        });

    const vulkan_backend::vulkan_native_queue_family_query_result result =
        vulkan_backend::query_native_vulkan_physical_device_queue_families(
            query,
            enumeration);

    require(result.checked, "queue-family result is checked");
    require(
        result.status == vulkan_backend::vulkan_native_queue_family_query_status::ready,
        "queue-family query reports ready");
    require(
        result.ready_for_selection(),
        "queue-family query is ready for physical device selection");
    require(result.queue_family_count == 2, "queue-family query records count");
    require(result.queue_families.size() == 2, "queue-family query records families");
    require(
        result.queue_families.front().physical_device.value == first_device.value,
        "queue-family query preserves first physical device handle");
    require(
        result.queue_families.back().family_index == 5,
        "queue-family query preserves stable queue family index");
    require(
        query.state().query_call_count == 1,
        "queue-family query records one query call");
    require(
        query.state().inspected_physical_devices.size() == 2,
        "queue-family query inspects both enumerated devices");
    require(
        query.state().last_get_physical_device_queue_family_properties.value
            == enumeration.dispatch_table.get_physical_device_queue_family_properties.value,
        "queue-family query records the dispatch queue-family properties pointer");
}

void test_physical_device_selection_reports_no_devices()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_enumeration_result enumeration =
        make_physical_device_enumeration({});
    vulkan_backend::fake_vulkan_native_queue_family_query queue_family_query;
    const vulkan_backend::vulkan_native_queue_family_query_result queue_family_result =
        vulkan_backend::query_native_vulkan_physical_device_queue_families(
            queue_family_query,
            enumeration);
    vulkan_backend::fake_vulkan_native_physical_device_selector selector;
    const vulkan_backend::vulkan_native_physical_device_selection_result result =
        vulkan_backend::select_native_vulkan_physical_device(
            selector,
            enumeration,
            queue_family_result,
            make_device_request());

    require(result.checked, "physical device selection no-device result is checked");
    require(
        result.status == vulkan_backend::vulkan_native_physical_device_selection_status::no_devices,
        "physical device selection maps no devices");
    require(
        !result.ready_for_device_create(),
        "physical device selection is not ready for device create without devices");
    require(
        !result.selected_physical_device.valid(),
        "physical device selection returns no selected handle without devices");
    require(
        result.selected_queue_families.empty(),
        "physical device selection returns no queue families without devices");
    require(
        selector.state().select_call_count == 1,
        "physical device selector records no-device selection call");
    require(
        selector.state().inspected_physical_devices.empty(),
        "physical device selector inspects no handles when enumeration has no devices");
}

void test_physical_device_selection_reports_missing_required_queue()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_physical_device_handle physical_device{.value = 9101};
    const vulkan_backend::vulkan_native_physical_device_enumeration_result enumeration =
        make_physical_device_enumeration({physical_device});
    const vulkan_backend::vulkan_native_queue_family_query_result queue_family_result =
        make_queue_family_query(
            enumeration,
            {
                vulkan_backend::vulkan_native_physical_device_queue_family{
                    .physical_device = physical_device,
                    .family_index = 2,
                    .queue_count = 1,
                    .capabilities = {vulkan_backend::vulkan_device_queue_capability::graphics},
                },
            }
        );
    vulkan_backend::fake_vulkan_native_physical_device_selector selector;

    const vulkan_backend::vulkan_native_physical_device_selection_result result =
        vulkan_backend::select_native_vulkan_physical_device(
            selector,
            enumeration,
            queue_family_result,
            make_device_request());

    require(result.checked, "physical device selection missing-queue result is checked");
    require(
        result.status
            == vulkan_backend::vulkan_native_physical_device_selection_status::missing_required_queue,
        "physical device selection maps missing required queue capability");
    require(
        result.missing_required_queue == "present",
        "physical device selection records missing present queue");
    require(
        result.candidates.size() == 1,
        "physical device selection records one inspected candidate");
    require(
        result.candidates.front().physical_device.value == physical_device.value,
        "physical device selection records candidate physical device handle");
    require(
        result.candidates.front().selected_queue_families.size() == 1,
        "physical device selection records partial graphics queue evidence");
    require(
        result.candidates.front().selected_queue_families.front().family_index == 2,
        "physical device selection records stable graphics queue family index");
    require(
        !result.ready_for_device_create(),
        "physical device selection is not ready when present queue is missing");
}

void test_physical_device_selection_picks_device_with_graphics_and_present()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_physical_device_handle first_device{.value = 9101};
    const vulkan_backend::vulkan_physical_device_handle selected_device{.value = 9102};
    const vulkan_backend::vulkan_native_physical_device_enumeration_result enumeration =
        make_physical_device_enumeration({first_device, selected_device});
    const vulkan_backend::vulkan_native_queue_family_query_result queue_family_result =
        make_queue_family_query(
            enumeration,
            {
                vulkan_backend::vulkan_native_physical_device_queue_family{
                    .physical_device = first_device,
                    .family_index = 1,
                    .queue_count = 1,
                    .capabilities = {vulkan_backend::vulkan_device_queue_capability::graphics},
                },
                vulkan_backend::vulkan_native_physical_device_queue_family{
                    .physical_device = selected_device,
                    .family_index = 4,
                    .queue_count = 2,
                    .capabilities = {vulkan_backend::vulkan_device_queue_capability::graphics},
                },
                vulkan_backend::vulkan_native_physical_device_queue_family{
                    .physical_device = selected_device,
                    .family_index = 7,
                    .queue_count = 1,
                    .capabilities = {vulkan_backend::vulkan_device_queue_capability::present},
                },
            }
        );
    vulkan_backend::fake_vulkan_native_physical_device_selector selector;

    const vulkan_backend::vulkan_native_physical_device_selection_result result =
        vulkan_backend::select_native_vulkan_physical_device(
            selector,
            enumeration,
            queue_family_result,
            make_device_request());

    require(result.checked, "physical device selection result is checked");
    require(
        result.status == vulkan_backend::vulkan_native_physical_device_selection_status::selected,
        "physical device selection reports selected");
    require(
        result.ready_for_device_create(),
        "physical device selection is ready for future vkCreateDevice");
    require(
        result.selected_physical_device.value == selected_device.value,
        "physical device selection records stable selected physical device handle");
    require(
        result.selected_queue_families.size() == 2,
        "physical device selection records graphics and present queue families");
    require(
        result.selected_queue_families[0].capability
            == vulkan_backend::vulkan_device_queue_capability::graphics,
        "physical device selection records graphics capability first");
    require(
        result.selected_queue_families[0].family_index == 4,
        "physical device selection records stable graphics queue family index");
    require(
        result.selected_queue_families[0].queue_count == 2,
        "physical device selection records graphics queue count");
    require(
        result.selected_queue_families[1].capability
            == vulkan_backend::vulkan_device_queue_capability::present,
        "physical device selection records present capability second");
    require(
        result.selected_queue_families[1].family_index == 7,
        "physical device selection records stable present queue family index");
    require(
        result.candidates.size() == 2,
        "physical device selection records skipped and selected candidates");
    require(
        !result.candidates.front().selected,
        "physical device selection records first candidate as unselected");
    require(
        result.candidates.back().required_queues_ready(),
        "physical device selection records selected candidate readiness");
    require(
        selector.state().inspected_physical_devices.size() == 2,
        "physical device selection inspects devices in enumeration order");
    require(
        selector.state().inspected_physical_devices.back().value == selected_device.value,
        "physical device selection stops after selected device");
}

void require_native_device_dispatch_missing_symbol(
    const std::string& symbol_name,
    quiz_vulkan::render::vulkan_backend::vulkan_native_device_dispatch_table_status
        expected_status,
    const char* status_message)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_selection_result selection =
        make_selected_physical_device();
    vulkan_backend::fake_vulkan_native_instance_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_instance_symbol_resolver_options{
            .missing_symbols = {symbol_name},
            .get_instance_proc_address =
                selection.enumeration.dispatch_table.get_instance_proc_address,
        });

    const vulkan_backend::vulkan_native_device_dispatch_table dispatch_table =
        vulkan_backend::collect_vulkan_native_device_dispatch_table(
            resolver,
            selection);

    require(dispatch_table.checked, "native device dispatch missing-symbol result is checked");
    require(dispatch_table.status == expected_status, status_message);
    require(
        dispatch_table.missing_symbol_name == symbol_name,
        "native device dispatch records missing symbol name");
    require(
        !dispatch_table.ready_for_create(),
        "native device dispatch is not ready when a required symbol is missing");
    require(
        resolver.state().requested_symbols.back() == symbol_name,
        "native device dispatch requests the missing symbol");
}

void test_native_device_dispatch_resolves_create_queue_and_destroy()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_selection_result selection =
        make_selected_physical_device();
    vulkan_backend::fake_vulkan_native_instance_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_instance_symbol_resolver_options{
            .get_instance_proc_address =
                selection.enumeration.dispatch_table.get_instance_proc_address,
        });

    const vulkan_backend::vulkan_native_device_dispatch_table dispatch_table =
        vulkan_backend::collect_vulkan_native_device_dispatch_table(
            resolver,
            selection);

    require(dispatch_table.checked, "native device dispatch result is checked");
    require(
        dispatch_table.status == vulkan_backend::vulkan_native_device_dispatch_table_status::ready,
        "native device dispatch reports ready");
    require(
        dispatch_table.ready_for_create(),
        "native device dispatch is ready for logical device creation");
    require(
        dispatch_table.instance.value == selection.enumeration.dispatch_table.instance.value,
        "native device dispatch records the selected instance handle");
    require(
        dispatch_table.physical_device.value == selection.selected_physical_device.value,
        "native device dispatch records the selected physical device handle");
    require(
        resolver.state().resolve_call_count == 3,
        "native device dispatch resolves create, get-queue, and destroy symbols");
    require(
        resolver.state().requested_symbols[0] == "vkCreateDevice",
        "native device dispatch resolves vkCreateDevice first");
    require(
        resolver.state().requested_symbols[1] == "vkGetDeviceQueue",
        "native device dispatch resolves vkGetDeviceQueue second");
    require(
        resolver.state().requested_symbols[2] == "vkDestroyDevice",
        "native device dispatch resolves vkDestroyDevice third");
}

void test_native_device_dispatch_reports_missing_symbols()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require_native_device_dispatch_missing_symbol(
        "vkCreateDevice",
        vulkan_backend::vulkan_native_device_dispatch_table_status::missing_create_device_symbol,
        "native device dispatch maps missing vkCreateDevice");
    require_native_device_dispatch_missing_symbol(
        "vkGetDeviceQueue",
        vulkan_backend::vulkan_native_device_dispatch_table_status::missing_get_device_queue_symbol,
        "native device dispatch maps missing vkGetDeviceQueue");
    require_native_device_dispatch_missing_symbol(
        "vkDestroyDevice",
        vulkan_backend::vulkan_native_device_dispatch_table_status::missing_destroy_device_symbol,
        "native device dispatch maps missing vkDestroyDevice");
}

void test_native_device_create_reports_selection_unavailable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_enumeration_result enumeration =
        make_physical_device_enumeration({});
    vulkan_backend::fake_vulkan_native_queue_family_query queue_family_query;
    const vulkan_backend::vulkan_native_queue_family_query_result queue_family_result =
        vulkan_backend::query_native_vulkan_physical_device_queue_families(
            queue_family_query,
            enumeration);
    vulkan_backend::fake_vulkan_native_physical_device_selector selector;
    const vulkan_backend::vulkan_native_physical_device_selection_result selection =
        vulkan_backend::select_native_vulkan_physical_device(
            selector,
            enumeration,
            queue_family_result,
            make_device_request());

    vulkan_backend::fake_vulkan_native_device_creator creator;
    const vulkan_backend::vulkan_native_device_create_result result =
        vulkan_backend::create_native_vulkan_device(
            creator,
            vulkan_backend::vulkan_native_device_dispatch_table{},
            selection);

    require(result.checked, "native device selection-unavailable result is checked");
    require(
        result.status
            == vulkan_backend::vulkan_native_device_create_status::selection_unavailable,
        "native device create maps unavailable physical-device selection");
    require(
        !result.ready_for_backend(),
        "native device create is not ready without physical-device selection");
    require(
        creator.state().create_call_count == 0,
        "native device creator does not call vkCreateDevice without selection");
}

void test_native_device_create_reports_dispatch_unavailable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_selection_result selection =
        make_selected_physical_device();
    vulkan_backend::fake_vulkan_native_device_creator creator;
    const vulkan_backend::vulkan_native_device_create_result result =
        vulkan_backend::create_native_vulkan_device(
            creator,
            vulkan_backend::vulkan_native_device_dispatch_table{},
            selection);

    require(result.checked, "native device dispatch-unavailable result is checked");
    require(
        result.status
            == vulkan_backend::vulkan_native_device_create_status::dispatch_table_unavailable,
        "native device create maps unavailable dispatch table");
    require(
        creator.state().create_call_count == 0,
        "native device creator does not call vkCreateDevice without dispatch");
}

void test_native_device_create_uses_selected_device_and_queue_families()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_selection_result selection =
        make_selected_physical_device();
    const vulkan_backend::vulkan_native_device_dispatch_table dispatch_table =
        make_ready_native_device_dispatch_table(selection);
    vulkan_backend::fake_vulkan_native_device_creator creator(
        vulkan_backend::fake_vulkan_native_device_creator_options{
            .handle = vulkan_backend::vulkan_device_handle{.value = 9900},
            .queue_handle_base = 10000,
        });

    const vulkan_backend::vulkan_native_device_create_result result =
        vulkan_backend::create_native_vulkan_device(
            creator,
            dispatch_table,
            selection);

    require(result.checked, "native device create result is checked");
    require(
        result.status == vulkan_backend::vulkan_native_device_create_status::created,
        "native device create reports created");
    require(result.ready_for_backend(), "native device create is ready for backend use");
    require(result.handle.value == 9900, "native device create records configured handle");
    require(
        result.selected_extensions.size() == 1,
        "native device create records required device extension selection");
    require(
        result.selected_extensions.front() == "VK_KHR_swapchain",
        "native device create records required swapchain extension");
    require(
        result.queue_create_family_indices.size() == 2,
        "native device create records unique queue family create infos");
    require(
        result.queue_create_family_indices[0] == 4,
        "native device create records graphics queue create family");
    require(
        result.queue_create_family_indices[1] == 7,
        "native device create records present queue create family");
    require(result.selected_queues.size() == 2, "native device create records selected queues");
    require(result.selected_queue_count == 2, "native device create counts selected queues");
    require(
        result.selected_queues[0].family_index == 4,
        "native device create records graphics queue family index");
    require(
        result.selected_queues[0].queue.value == 10005,
        "native device create records stable graphics queue handle");
    require(
        result.selected_queues[1].family_index == 7,
        "native device create records present queue family index");
    require(
        result.selected_queues[1].queue.value == 10008,
        "native device create records stable present queue handle");
    require(
        creator.state().create_call_count == 1,
        "native device creator calls create once");
    require(
        creator.state().get_queue_call_count == 2,
        "native device creator resolves two selected queues");
    require(
        creator.state().destroy_call_count == 0,
        "native device creator does not destroy a successful device");
    require(
        creator.state().requested_physical_device.value
            == selection.selected_physical_device.value,
        "native device creator records selected physical device");
    require(
        creator.state().requested_queue_family_indices.size() == 2,
        "native device creator records requested queue families");
    require(
        creator.state().requested_extensions.front() == "VK_KHR_swapchain",
        "native device creator records required extension request");
    require(
        creator.state().last_create_device.value == dispatch_table.create_device.value,
        "native device creator records vkCreateDevice pointer");
    require(
        creator.state().last_get_device_queue.value == dispatch_table.get_device_queue.value,
        "native device creator records vkGetDeviceQueue pointer");
    require(
        creator.state().last_destroy_device.value == dispatch_table.destroy_device.value,
        "native device creator records vkDestroyDevice pointer");
}

void test_native_device_create_reports_creation_failure()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_selection_result selection =
        make_selected_physical_device();
    const vulkan_backend::vulkan_native_device_dispatch_table dispatch_table =
        make_ready_native_device_dispatch_table(selection);
    vulkan_backend::fake_vulkan_native_device_creator creator(
        vulkan_backend::fake_vulkan_native_device_creator_options{
            .fail_creation = true,
            .failure_result = -7,
        });

    const vulkan_backend::vulkan_native_device_create_result result =
        vulkan_backend::create_native_vulkan_device(
            creator,
            dispatch_table,
            selection);

    require(result.checked, "native device creation-failure result is checked");
    require(
        result.status == vulkan_backend::vulkan_native_device_create_status::creation_failed,
        "native device create maps creation failure");
    require(result.native_result == -7, "native device create records native failure result");
    require(!result.handle.valid(), "native device create returns no handle on failure");
    require(
        creator.state().create_call_count == 1,
        "native device creator attempts create before reporting failure");
    require(
        creator.state().get_queue_call_count == 0,
        "native device creator does not query queues after create failure");
}

void test_native_device_create_destroys_device_when_queue_lookup_fails()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_physical_device_selection_result selection =
        make_selected_physical_device();
    const vulkan_backend::vulkan_native_device_dispatch_table dispatch_table =
        make_ready_native_device_dispatch_table(selection);
    vulkan_backend::fake_vulkan_native_device_creator creator(
        vulkan_backend::fake_vulkan_native_device_creator_options{
            .handle = vulkan_backend::vulkan_device_handle{.value = 9900},
            .queue_handle_base = 10000,
            .unavailable_queue_family_indices = {7},
        });

    const vulkan_backend::vulkan_native_device_create_result result =
        vulkan_backend::create_native_vulkan_device(
            creator,
            dispatch_table,
            selection);

    require(result.checked, "native device queue-failure result is checked");
    require(
        result.status == vulkan_backend::vulkan_native_device_create_status::queue_unavailable,
        "native device create maps queue lookup failure");
    require(
        !result.handle.valid(),
        "native device create clears handle after queue lookup failure");
    require(
        result.selected_queue_count == 1,
        "native device create records queues found before failure");
    require(
        creator.state().get_queue_call_count == 2,
        "native device creator queries queues until the failing family");
    require(
        creator.state().destroy_call_count == 1,
        "native device creator destroys device when queue lookup fails");
    require(
        creator.state().destroyed_device.value == 9900,
        "native device creator records destroyed logical device handle");
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
    test_physical_device_dispatch_reports_no_instance();
    test_physical_device_dispatch_reports_missing_symbol();
    test_physical_device_dispatch_reports_missing_queue_family_symbol();
    test_physical_device_enumerator_reports_zero_devices();
    test_physical_device_enumerator_reports_usable_devices();
    test_queue_family_query_reports_no_instance();
    test_queue_family_query_reports_zero_devices();
    test_queue_family_query_reports_usable_families();
    test_physical_device_selection_reports_no_devices();
    test_physical_device_selection_reports_missing_required_queue();
    test_physical_device_selection_picks_device_with_graphics_and_present();
    test_native_device_dispatch_resolves_create_queue_and_destroy();
    test_native_device_dispatch_reports_missing_symbols();
    test_native_device_create_reports_selection_unavailable();
    test_native_device_create_reports_dispatch_unavailable();
    test_native_device_create_uses_selected_device_and_queue_families();
    test_native_device_create_reports_creation_failure();
    test_native_device_create_destroys_device_when_queue_lookup_fails();
    test_vulkan_device_status_and_queue_names_are_stable();
    return 0;
}
