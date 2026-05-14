#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_sdk.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
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

quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_state make_missing_loader()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_loader_readiness_state{
        .checked = true,
        .status = vulkan_backend::vulkan_loader_readiness_status::library_missing,
        .probe_status = vulkan_backend::vulkan_loader_probe_status::library_missing,
        .loader_library_available = false,
        .instance_proc_address_available = false,
        .instance_ready = false,
        .loaded_library_name = {},
        .required_symbol_name = std::string{vulkan_backend::vulkan_loader_required_symbol_name()},
        .attempted_library_count = 1,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_instance_create_request make_surface_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_instance_create_request{
        .app_name = "quiz-vulkan-test",
        .engine_name = "quiz-vulkan-renderer-test",
        .api_version = 1,
        .required_instance_extensions = {"VK_KHR_surface"},
        .optional_instance_extensions = {"VK_EXT_debug_utils", "VK_EXT_missing_optional"},
        .requested_layers = {},
        .enable_validation = true,
    };
}

quiz_vulkan::render::vulkan_backend::fake_vulkan_instance_factory make_surface_factory()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::fake_vulkan_instance_factory(
        vulkan_backend::fake_vulkan_instance_factory_options{
            .supported_instance_extensions = {"VK_KHR_surface", "VK_EXT_debug_utils"},
            .supported_layers = {std::string{vulkan_backend::vulkan_validation_layer_name()}},
            .fail_creation = false,
            .handle = vulkan_backend::vulkan_instance_handle{.value = 42},
        });
}

void test_instance_factory_creates_instance_when_loader_and_requirements_are_available()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_instance_factory factory = make_surface_factory();
    const vulkan_backend::vulkan_instance_create_result result =
        vulkan_backend::create_vulkan_instance(factory, make_ready_loader(), make_surface_request());

    require(result.checked, "instance creation records that the request was checked");
    require(
        result.status == vulkan_backend::vulkan_instance_create_status::created,
        "instance creation reports created status");
    require(result.ready_for_device(), "created instance result reaches the device gate");
    require(result.loader.ready_for_instance(), "created instance preserves ready loader snapshot");
    require(result.handle.valid(), "created instance returns an opaque valid handle");
    require(result.handle.value == 42, "created instance returns the configured fake handle");
    require(result.selected_extensions.size() == 2, "created instance selects required and supported optional extensions");
    require(
        result.selected_extensions[0] == "VK_KHR_surface",
        "created instance selects the required surface extension first");
    require(
        result.selected_extensions[1] == "VK_EXT_debug_utils",
        "created instance selects supported optional debug extension");
    require(
        result.required_extensions_ready(),
        "created instance records required instance extensions ready");
    require(
        result.required_extension_count == 1,
        "created instance counts required instance extensions");
    require(
        result.available_required_extension_count == 1,
        "created instance counts available required instance extensions");
    require(
        result.missing_required_extension.empty(),
        "created instance records no missing required extension");
    require(
        result.required_extension_diagnostics.size() == 1,
        "created instance records one required extension diagnostic");
    require(
        result.required_extension_diagnostics.front().extension_name == "VK_KHR_surface",
        "created instance records required surface extension diagnostic name");
    require(
        result.required_extension_diagnostics.front().available,
        "created instance records required surface extension available");
    require(
        result.required_extension_diagnostics.front().selected,
        "created instance records required surface extension selected");
    require(
        !result.required_extension_diagnostics.front().missing_required(),
        "created instance required surface diagnostic is not missing");
    require(result.enabled_layers.size() == 1, "created instance enables validation layer");
    require(
        result.enabled_layers.front() == std::string{vulkan_backend::vulkan_validation_layer_name()},
        "created instance maps validation request to the Khronos validation layer");

    vulkan_backend::null_vulkan_backend_device device(result);
    const vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle =
        device.current_lifecycle_readiness();
    require(lifecycle.instance.checked, "null backend records instance creation result");
    require(lifecycle.instance.ready_for_device(), "null backend keeps instance-created device gate ready");
    require(lifecycle.instance_ready, "null backend maps created instance to instance readiness");
    require(lifecycle.effective_instance_ready(), "null backend effective instance readiness follows instance result");

    const quiz_vulkan::render::render_draw_list draw_list;
    const vulkan_backend::vulkan_backend_frame_result frame_result =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require(
        frame_result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::device_unavailable,
        "instance-created null backend reaches the device readiness gate");
}

void test_instance_factory_reports_loader_unavailable_before_extension_checks()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_instance_factory factory = make_surface_factory();
    const vulkan_backend::vulkan_instance_create_result result =
        vulkan_backend::create_vulkan_instance(factory, make_missing_loader(), make_surface_request());

    require(result.checked, "loader-unavailable instance result is checked");
    require(
        result.status == vulkan_backend::vulkan_instance_create_status::loader_unavailable,
        "loader-unavailable instance result records loader unavailable status");
    require(!result.ready_for_device(), "loader-unavailable instance result does not reach device gate");
    require(!result.handle.valid(), "loader-unavailable instance result has no handle");
    require(result.selected_extensions.empty(), "loader-unavailable instance result selects no extensions");
    require(
        result.required_extension_diagnostics.empty(),
        "loader-unavailable instance result skips extension diagnostics");
    require(
        result.required_extension_count == 0,
        "loader-unavailable instance result leaves required extension count at zero");
    require(result.enabled_layers.empty(), "loader-unavailable instance result enables no layers");
    require(
        result.loader.status == vulkan_backend::vulkan_loader_readiness_status::library_missing,
        "loader-unavailable instance result preserves loader failure snapshot");
}

void test_instance_factory_reports_missing_required_extension()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_instance_factory factory(
        vulkan_backend::fake_vulkan_instance_factory_options{
            .supported_instance_extensions = {"VK_EXT_debug_utils"},
            .supported_layers = {std::string{vulkan_backend::vulkan_validation_layer_name()}},
            .fail_creation = false,
            .handle = vulkan_backend::vulkan_instance_handle{.value = 7},
        });

    const vulkan_backend::vulkan_instance_create_result result =
        vulkan_backend::create_vulkan_instance(factory, make_ready_loader(), make_surface_request());

    require(result.checked, "missing-extension instance result is checked");
    require(
        result.status == vulkan_backend::vulkan_instance_create_status::missing_required_extension,
        "missing-extension instance result reports missing required extension");
    require(!result.ready_for_device(), "missing-extension instance result does not reach device gate");
    require(!result.handle.valid(), "missing-extension instance result has no handle");
    require(result.selected_extensions.empty(), "missing-extension instance result selects no extensions");
    require(
        !result.required_extensions_ready(),
        "missing-extension instance result records required extensions not ready");
    require(
        result.required_extension_count == 1,
        "missing-extension instance result counts required extension checks");
    require(
        result.available_required_extension_count == 0,
        "missing-extension instance result counts no available required extensions");
    require(
        result.missing_required_extension == "VK_KHR_surface",
        "missing-extension instance result records missing extension field");
    require(
        result.required_extension_diagnostics.size() == 1,
        "missing-extension instance result records one required extension diagnostic");
    require(
        result.required_extension_diagnostics.front().extension_name == "VK_KHR_surface",
        "missing-extension diagnostic records surface extension name");
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
        result.diagnostic == "missing required instance extension: VK_KHR_surface",
        "missing-extension instance result records diagnostic extension name");
}

void test_instance_factory_reports_missing_requested_layer()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_instance_factory factory(
        vulkan_backend::fake_vulkan_instance_factory_options{
            .supported_instance_extensions = {"VK_KHR_surface", "VK_EXT_debug_utils"},
            .supported_layers = {},
            .fail_creation = false,
            .handle = vulkan_backend::vulkan_instance_handle{.value = 9},
        });

    const vulkan_backend::vulkan_instance_create_result result =
        vulkan_backend::create_vulkan_instance(factory, make_ready_loader(), make_surface_request());

    require(result.checked, "missing-layer instance result is checked");
    require(
        result.status == vulkan_backend::vulkan_instance_create_status::missing_requested_layer,
        "missing-layer instance result reports missing requested layer");
    require(!result.ready_for_device(), "missing-layer instance result does not reach device gate");
    require(!result.handle.valid(), "missing-layer instance result has no handle");
    require(result.selected_extensions.size() == 2, "missing-layer instance result preserves selected extensions");
    require(
        result.required_extensions_ready(),
        "missing-layer instance result keeps required extension diagnostics ready");
    require(
        result.required_extension_diagnostics.size() == 1,
        "missing-layer instance result preserves required extension diagnostic");
    require(result.enabled_layers.empty(), "missing-layer instance result enables no layers");
    require(
        result.diagnostic == "missing requested instance layer: VK_LAYER_KHRONOS_validation",
        "missing-layer instance result records diagnostic layer name");
}

void test_instance_factory_reports_creation_failure_after_requirements_pass()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_instance_factory factory(
        vulkan_backend::fake_vulkan_instance_factory_options{
            .supported_instance_extensions = {"VK_KHR_surface", "VK_EXT_debug_utils"},
            .supported_layers = {std::string{vulkan_backend::vulkan_validation_layer_name()}},
            .fail_creation = true,
            .handle = vulkan_backend::vulkan_instance_handle{.value = 11},
        });

    const vulkan_backend::vulkan_instance_create_result result =
        vulkan_backend::create_vulkan_instance(factory, make_ready_loader(), make_surface_request());

    require(result.checked, "creation-failure instance result is checked");
    require(
        result.status == vulkan_backend::vulkan_instance_create_status::creation_failed,
        "creation-failure instance result reports creation failure");
    require(!result.ready_for_device(), "creation-failure instance result does not reach device gate");
    require(!result.handle.valid(), "creation-failure instance result has no handle");
    require(result.selected_extensions.size() == 2, "creation-failure instance result preserves selected extensions");
    require(
        result.required_extensions_ready(),
        "creation-failure instance result keeps required extension diagnostics ready");
    require(
        result.available_required_extension_count == result.required_extension_count,
        "creation-failure instance result counts all required extensions available");
    require(result.enabled_layers.size() == 1, "creation-failure instance result preserves enabled layers");

    vulkan_backend::null_vulkan_backend_device device(result);
    const quiz_vulkan::render::render_draw_list draw_list;
    const vulkan_backend::vulkan_backend_frame_result frame_result =
        vulkan_backend::submit_vulkan_backend_frame(
            device,
            draw_list,
            quiz_vulkan::render::render_rect{0.0f, 0.0f, 64.0f, 64.0f});

    require(
        frame_result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable,
        "creation-failure null backend stops at the instance readiness gate");
    require(
        frame_result.lifecycle.instance.status == vulkan_backend::vulkan_instance_create_status::creation_failed,
        "creation-failure frame result preserves instance failure status");
}

void test_vulkan_instance_create_status_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::vulkan_validation_layer_name()
            == std::string_view{"VK_LAYER_KHRONOS_validation"},
        "validation layer helper name is stable");
    require(
        vulkan_backend::instance_create_status_name(
            vulkan_backend::vulkan_instance_create_status::not_requested)
            == std::string_view{"not_requested"},
        "instance create status name for not requested is stable");
    require(
        vulkan_backend::instance_create_status_name(
            vulkan_backend::vulkan_instance_create_status::created)
            == std::string_view{"created"},
        "instance create status name for created is stable");
    require(
        vulkan_backend::instance_create_status_name(
            vulkan_backend::vulkan_instance_create_status::loader_unavailable)
            == std::string_view{"loader_unavailable"},
        "instance create status name for loader unavailable is stable");
    require(
        vulkan_backend::instance_create_status_name(
            vulkan_backend::vulkan_instance_create_status::missing_required_extension)
            == std::string_view{"missing_required_extension"},
        "instance create status name for missing required extension is stable");
    require(
        vulkan_backend::instance_create_status_name(
            vulkan_backend::vulkan_instance_create_status::missing_requested_layer)
            == std::string_view{"missing_requested_layer"},
        "instance create status name for missing requested layer is stable");
    require(
        vulkan_backend::instance_create_status_name(
            vulkan_backend::vulkan_instance_create_status::creation_failed)
            == std::string_view{"creation_failed"},
        "instance create status name for creation failed is stable");
}

void test_native_instance_function_table_collects_global_entrypoints()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver;
    const vulkan_backend::vulkan_native_instance_function_table table =
        vulkan_backend::collect_vulkan_native_instance_function_table(resolver);

    require(table.checked, "native instance function table is checked");
    require(table.ready(), "native instance function table is ready");
    require(
        table.status == vulkan_backend::vulkan_native_instance_function_table_status::ready,
        "native instance function table reports ready");
    require(table.create_instance.valid(), "native instance table records vkCreateInstance");
    require(table.destroy_instance.valid(), "native instance table records vkDestroyInstance");
    require(
        resolver.state().resolve_call_count == 2,
        "native instance table resolves create and destroy symbols");
    require(
        resolver.state().requested_symbols.front() == "vkCreateInstance",
        "native instance table resolves vkCreateInstance first");
    require(
        resolver.state().requested_symbols.back() == "vkDestroyInstance",
        "native instance table resolves vkDestroyInstance second");
}

void test_native_instance_function_table_reports_missing_symbols()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    {
        vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
            vulkan_backend::fake_vulkan_native_symbol_resolver_options{
                .missing_symbols = {"vkCreateInstance"},
            });
        const vulkan_backend::vulkan_native_instance_function_table table =
            vulkan_backend::collect_vulkan_native_instance_function_table(resolver);

        require(!table.ready(), "missing create blocks native instance table");
        require(
            table.status
                == vulkan_backend::vulkan_native_instance_function_table_status::missing_create_instance_symbol,
            "native instance table maps missing vkCreateInstance");
    }

    {
        vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
            vulkan_backend::fake_vulkan_native_symbol_resolver_options{
                .missing_symbols = {"vkDestroyInstance"},
            });
        const vulkan_backend::vulkan_native_instance_function_table table =
            vulkan_backend::collect_vulkan_native_instance_function_table(resolver);

        require(!table.ready(), "missing destroy blocks native instance table");
        require(
            table.status
                == vulkan_backend::vulkan_native_instance_function_table_status::missing_destroy_instance_symbol,
            "native instance table maps missing vkDestroyInstance");
        require(table.create_instance.valid(), "missing destroy table preserves create pointer");
    }
}

void test_native_instance_create_blocks_before_calling_invalid_fake_pointers()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver;
    const vulkan_backend::vulkan_native_instance_function_table table =
        vulkan_backend::collect_vulkan_native_instance_function_table(resolver);

    const vulkan_backend::vulkan_native_instance_create_result missing_loader_result =
        vulkan_backend::create_native_vulkan_instance(
            make_missing_loader(),
            table,
            vulkan_backend::vulkan_instance_create_request{});
    require(missing_loader_result.checked, "native instance missing-loader result is checked");
    require(
        missing_loader_result.status
            == vulkan_backend::vulkan_native_instance_create_status::loader_unavailable,
        "native instance create blocks on loader readiness before function pointers");
    require(!missing_loader_result.created(), "missing-loader native instance is not created");

    vulkan_backend::fake_vulkan_native_symbol_resolver missing_symbol_resolver(
        vulkan_backend::fake_vulkan_native_symbol_resolver_options{
            .missing_symbols = {"vkCreateInstance"},
        });
    const vulkan_backend::vulkan_native_instance_function_table missing_table =
        vulkan_backend::collect_vulkan_native_instance_function_table(missing_symbol_resolver);
    const vulkan_backend::vulkan_native_instance_create_result missing_table_result =
        vulkan_backend::create_native_vulkan_instance(
            make_ready_loader(),
            missing_table,
            vulkan_backend::vulkan_instance_create_request{});
    require(missing_table_result.checked, "native instance missing-table result is checked");
    require(
        missing_table_result.status
            == vulkan_backend::vulkan_native_instance_create_status::function_table_unavailable,
        "native instance create blocks on function table readiness");
    require(!missing_table_result.created(), "missing-table native instance is not created");
}

void test_native_instance_destroy_blocks_invalid_handles()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver;
    const vulkan_backend::vulkan_native_instance_function_table table =
        vulkan_backend::collect_vulkan_native_instance_function_table(resolver);
    const vulkan_backend::vulkan_native_instance_destroy_result result =
        vulkan_backend::destroy_native_vulkan_instance(table, vulkan_backend::vulkan_instance_handle{});

    require(result.checked, "native instance destroy invalid-handle result is checked");
    require(
        result.status == vulkan_backend::vulkan_native_instance_destroy_status::invalid_handle,
        "native instance destroy maps invalid handle");
    require(!result.destroyed(), "native instance destroy does not complete invalid handle");
}

void test_native_instance_create_smoke_uses_system_loader_when_available()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::system_vulkan_loader loader;
    const vulkan_backend::vulkan_loader_probe_result probe =
        vulkan_backend::probe_vulkan_loader(loader);
    require(probe.checked, "native instance smoke checks the system loader");
    if (!probe.available()) {
        return;
    }

    const vulkan_backend::vulkan_loader_readiness_state loader_readiness =
        vulkan_backend::make_vulkan_loader_readiness_state(probe);
    vulkan_backend::system_vulkan_native_symbol_resolver resolver(
        vulkan_backend::system_vulkan_native_symbol_resolver_options{
            .candidate_library_names = {probe.loaded_library_name},
            .use_default_library_names = false,
        });
    const vulkan_backend::vulkan_native_instance_function_table table =
        vulkan_backend::collect_vulkan_native_instance_function_table(resolver);
    require(table.checked, "native instance smoke checks the function table");
    if (!table.ready()) {
        return;
    }

    const vulkan_backend::vulkan_native_instance_create_result create_result =
        vulkan_backend::create_native_vulkan_instance(
            loader_readiness,
            table,
            vulkan_backend::vulkan_instance_create_request{
                .app_name = "quiz-vulkan-native-instance-smoke",
                .engine_name = "quiz-vulkan-renderer-test",
            });
    require(create_result.checked, "native instance smoke create result is checked");
#if QUIZ_VULKAN_HAS_VULKAN_HEADERS
    require(
        create_result.status == vulkan_backend::vulkan_native_instance_create_status::created
            || create_result.status
                == vulkan_backend::vulkan_native_instance_create_status::creation_failed,
        "native instance smoke either creates an instance or records driver failure");
#else
    require(
        create_result.status
            == vulkan_backend::vulkan_native_instance_create_status::headers_unavailable,
        "native instance smoke reports missing compile-time Vulkan headers");
#endif
    if (!create_result.created()) {
        return;
    }

    const vulkan_backend::vulkan_native_instance_destroy_result destroy_result =
        vulkan_backend::destroy_native_vulkan_instance(table, create_result.handle);
    require(destroy_result.checked, "native instance smoke destroy result is checked");
    require(destroy_result.destroyed(), "native instance smoke destroys created instance");
}

} // namespace

int main()
{
    test_instance_factory_creates_instance_when_loader_and_requirements_are_available();
    test_instance_factory_reports_loader_unavailable_before_extension_checks();
    test_instance_factory_reports_missing_required_extension();
    test_instance_factory_reports_missing_requested_layer();
    test_instance_factory_reports_creation_failure_after_requirements_pass();
    test_vulkan_instance_create_status_names_are_stable();
    test_native_instance_function_table_collects_global_entrypoints();
    test_native_instance_function_table_reports_missing_symbols();
    test_native_instance_create_blocks_before_calling_invalid_fake_pointers();
    test_native_instance_destroy_blocks_invalid_handles();
    test_native_instance_create_smoke_uses_system_loader_when_available();
    return 0;
}
