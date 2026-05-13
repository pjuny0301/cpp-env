#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_sdk.h"

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

quiz_vulkan::render::vulkan_backend::vulkan_native_function_table_diagnostics
make_native_functions(std::string missing_symbol = {})
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
        missing_symbol.empty()
            ? vulkan_backend::fake_vulkan_native_symbol_resolver_options{}
            : vulkan_backend::fake_vulkan_native_symbol_resolver_options{
                .missing_symbols = {missing_symbol},
            });
    return vulkan_backend::collect_vulkan_native_function_table(
        resolver,
        make_ready_loader());
}

quiz_vulkan::render::vulkan_backend::vulkan_sdk_header_manifest make_ready_manifest()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_sdk_header_manifest{
        .headers_available = true,
        .api_version = vulkan_backend::vulkan_sdk_api_version_1_4(),
        .header_version = 341,
        .sdk_tag = "vulkan-sdk-1.4.341.0",
        .source_label = "fake-manifest",
        .supported_extensions = {
            "VK_KHR_surface",
            "VK_KHR_swapchain",
        },
        .diagnostic = "fake Vulkan headers available",
    };
}

void test_sdk_capability_names_and_versions_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::sdk_header_probe_status_name(
            vulkan_backend::vulkan_sdk_header_probe_status::available)
            == std::string_view{"available"},
        "SDK header probe status name for available is stable");
    require(
        vulkan_backend::sdk_capability_status_name(
            vulkan_backend::vulkan_sdk_capability_status::api_version_too_old)
            == std::string_view{"api_version_too_old"},
        "SDK capability status name for old API version is stable");
    require(
        vulkan_backend::sdk_adapter_fallback_status_name(
            vulkan_backend::vulkan_sdk_adapter_fallback_status::extension_unavailable)
            == std::string_view{"extension_unavailable"},
        "SDK adapter fallback status name for extension unavailable is stable");
    require(
        vulkan_backend::sdk_native_path_status_name(
            vulkan_backend::vulkan_sdk_native_path_status::function_table_blocked)
            == std::string_view{"function_table_blocked"},
        "SDK native path status name for function table blocked is stable");

    const vulkan_backend::vulkan_sdk_api_version api_1_4 =
        vulkan_backend::vulkan_sdk_api_version_1_4();
    require(api_1_4.valid(), "Vulkan SDK API version 1.4 is valid");
    require(
        api_1_4.supports(vulkan_backend::vulkan_sdk_api_version_1_3()),
        "Vulkan SDK API version 1.4 supports 1.3");
    require(
        !vulkan_backend::vulkan_sdk_api_version_1_3().supports(api_1_4),
        "Vulkan SDK API version 1.3 does not support 1.4");
}

void test_sdk_external_header_probe_reports_compile_time_boundary()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_sdk_external_header_evidence evidence =
        vulkan_backend::probe_vulkan_sdk_external_headers();
    const vulkan_backend::vulkan_sdk_header_manifest manifest =
        vulkan_backend::make_compile_time_vulkan_sdk_header_manifest();

    require(evidence.checked, "external header evidence is checked");
    require(
        manifest.source_label == "quiz_vulkan_desktop_external_headers",
        "compile-time manifest records external header boundary label");
    require(
        manifest.external_headers.checked,
        "compile-time manifest carries external header evidence");

#if QUIZ_VULKAN_HAS_VULKAN_HEADERS
    require(evidence.vulkan_headers_available(), "Vulkan header evidence is available");
    require(manifest.headers_available, "compile-time manifest records Vulkan headers");
    require(manifest.api_version.valid(), "compile-time manifest records Vulkan API version");
    require(manifest.header_version > 0, "compile-time manifest records Vulkan header version");
    require(evidence.vulkan.instance_handle_size > 0, "Vulkan header probe sees VkInstance");
    require(evidence.vulkan.device_handle_size > 0, "Vulkan header probe sees VkDevice");
    require(evidence.vulkan.result_type_size > 0, "Vulkan header probe sees VkResult");
    require(evidence.vulkan.success_constant_available, "Vulkan header probe sees VK_SUCCESS");
    require(evidence.vulkan.success_value == 0, "Vulkan header probe records VK_SUCCESS value");
    require(
        manifest.supports_extension("VK_KHR_surface"),
        "Vulkan header probe records surface extension constant");
    require(
        manifest.supports_extension("VK_KHR_swapchain"),
        "Vulkan header probe records swapchain extension constant");
#else
    require(!evidence.vulkan_headers_available(), "Vulkan header evidence is unavailable");
    require(!manifest.headers_available, "compile-time manifest records missing Vulkan headers");
    require(!manifest.api_version.valid(), "missing Vulkan headers have no API version");
#endif

#if QUIZ_VULKAN_HAS_VULKAN_HEADERS && QUIZ_VULKAN_HAS_VMA_HEADERS
    require(evidence.vma_headers_available(), "VMA header evidence is available");
    require(evidence.vma.safe_to_include, "VMA header probe uses safe include guard");
    require(evidence.vma.allocator_handle_size > 0, "VMA header probe sees VmaAllocator");
    require(evidence.vma.allocation_handle_size > 0, "VMA header probe sees VmaAllocation");
#else
    require(!evidence.vma_headers_available(), "VMA header evidence is unavailable");
#endif
}

void test_compile_time_sdk_header_probe_uses_external_header_manifest()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::compile_time_vulkan_sdk_header_probe probe;
    const vulkan_backend::vulkan_sdk_header_probe_result header_result =
        probe.probe_headers(vulkan_backend::vulkan_sdk_header_probe_request{});
    const vulkan_backend::vulkan_sdk_capability_result capability =
        vulkan_backend::collect_vulkan_sdk_capabilities(
            probe,
            make_native_functions(),
            vulkan_backend::vulkan_sdk_capability_request{
                .require_native_function_table = false,
            });

    require(header_result.checked, "compile-time header probe is checked");
    require(
        header_result.manifest.source_label == "quiz_vulkan_desktop_external_headers",
        "compile-time header probe records external boundary source");
    require(capability.external_headers.checked, "SDK capability carries external header evidence");

#if QUIZ_VULKAN_HAS_VULKAN_HEADERS
    require(header_result.available(), "compile-time Vulkan headers are available");
    require(capability.headers_available, "SDK capability sees compile-time Vulkan headers");
    require(capability.api_version_available, "SDK capability sees compile-time API version");
#else
    require(!header_result.available(), "compile-time Vulkan headers are unavailable");
    require(!capability.headers_available, "SDK capability records missing compile-time headers");
    require(
        capability.status == vulkan_backend::vulkan_sdk_capability_status::headers_unavailable,
        "SDK capability blocks when compile-time Vulkan headers are missing");
#endif
}

void test_sdk_native_path_readiness_summarizes_capability_states()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_sdk_header_probe ready_probe(
        vulkan_backend::fake_vulkan_sdk_header_probe_options{
            .manifest = make_ready_manifest(),
        });
    const vulkan_backend::vulkan_sdk_capability_result ready =
        vulkan_backend::collect_vulkan_sdk_capabilities(
            ready_probe,
            make_native_functions());
    const vulkan_backend::vulkan_sdk_native_path_readiness ready_path =
        vulkan_backend::summarize_vulkan_sdk_native_path_readiness(ready);
    require(ready_path.ready(), "ready SDK capability summarizes as native-path ready");
    require(
        ready_path.status == vulkan_backend::vulkan_sdk_native_path_status::ready,
        "ready SDK capability maps to ready native-path status");

    vulkan_backend::fake_vulkan_sdk_header_probe missing_probe(
        vulkan_backend::fake_vulkan_sdk_header_probe_options{
            .manifest = vulkan_backend::vulkan_sdk_header_manifest{
                .headers_available = false,
                .diagnostic = "fake Vulkan headers missing",
            },
        });
    const vulkan_backend::vulkan_sdk_capability_result missing =
        vulkan_backend::collect_vulkan_sdk_capabilities(
            missing_probe,
            make_native_functions());
    const vulkan_backend::vulkan_sdk_native_path_readiness missing_path =
        vulkan_backend::summarize_vulkan_sdk_native_path_readiness(missing);
    require(missing_path.blocked(), "missing SDK capability summarizes as blocked");
    require(
        missing_path.status == vulkan_backend::vulkan_sdk_native_path_status::sdk_missing,
        "missing SDK headers map to SDK-missing native-path status");

    vulkan_backend::fake_vulkan_sdk_header_probe version_probe(
        vulkan_backend::fake_vulkan_sdk_header_probe_options{
            .manifest = make_ready_manifest(),
        });
    const vulkan_backend::vulkan_sdk_capability_result version_mismatch =
        vulkan_backend::collect_vulkan_sdk_capabilities(
            version_probe,
            make_native_functions(),
            vulkan_backend::vulkan_sdk_capability_request{
                .minimum_api_version = vulkan_backend::make_vulkan_sdk_api_version(2, 0),
            });
    const vulkan_backend::vulkan_sdk_native_path_readiness version_path =
        vulkan_backend::summarize_vulkan_sdk_native_path_readiness(version_mismatch);
    require(
        version_path.status == vulkan_backend::vulkan_sdk_native_path_status::version_mismatch,
        "old SDK headers map to version-mismatch native-path status");

    vulkan_backend::fake_vulkan_sdk_header_probe function_table_probe(
        vulkan_backend::fake_vulkan_sdk_header_probe_options{
            .manifest = make_ready_manifest(),
        });
    const vulkan_backend::vulkan_sdk_capability_result function_table_blocked =
        vulkan_backend::collect_vulkan_sdk_capabilities(
            function_table_probe,
            make_native_functions("vkQueueSubmit"));
    const vulkan_backend::vulkan_sdk_native_path_readiness function_table_path =
        vulkan_backend::summarize_vulkan_sdk_native_path_readiness(function_table_blocked);
    require(
        function_table_path.status
            == vulkan_backend::vulkan_sdk_native_path_status::function_table_blocked,
        "missing native entrypoint maps to function-table-blocked native-path status");
    require(
        function_table_path.missing_native_symbol_name == "vkQueueSubmit",
        "SDK native-path summary records missing native symbol");
}

void test_sdk_capability_reports_adapter_ready_when_headers_and_native_symbols_match()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_sdk_header_probe probe(
        vulkan_backend::fake_vulkan_sdk_header_probe_options{
            .manifest = make_ready_manifest(),
        });
    const vulkan_backend::vulkan_sdk_capability_result result =
        vulkan_backend::collect_vulkan_sdk_capabilities(
            probe,
            make_native_functions(),
            vulkan_backend::vulkan_sdk_capability_request{
                .minimum_api_version = vulkan_backend::vulkan_sdk_api_version_1_3(),
            });

    require(result.checked, "SDK capability result is checked");
    require(result.adapter_ready(), "SDK capability result is adapter-ready");
    require(!result.blocked(), "SDK capability result does not block");
    require(
        result.status == vulkan_backend::vulkan_sdk_capability_status::ready,
        "SDK capability reports ready");
    require(
        result.fallback_status == vulkan_backend::vulkan_sdk_adapter_fallback_status::none,
        "SDK capability has no fallback status when ready");
    require(result.headers_available, "SDK capability records header availability");
    require(result.api_version_compatible, "SDK capability records API version compatibility");
    require(result.required_extensions_ready, "SDK capability records required extensions ready");
    require(result.native_function_table_ready, "SDK capability records native table ready");
    require(result.required_extension_count == 2, "SDK capability includes default backend extensions");
    require(
        result.available_required_extension_count == 2,
        "SDK capability counts available required extensions");
    require(probe.state().probe_call_count == 1, "SDK header probe is called once");
    require(
        probe.state().requested_manifests.front() == "vulkan-headers",
        "SDK header probe records manifest request name");
}

void test_sdk_capability_blocks_when_headers_are_missing()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_sdk_header_probe probe(
        vulkan_backend::fake_vulkan_sdk_header_probe_options{
            .manifest = vulkan_backend::vulkan_sdk_header_manifest{
                .headers_available = false,
                .diagnostic = "fake Vulkan headers missing",
            },
        });
    const vulkan_backend::vulkan_sdk_capability_result result =
        vulkan_backend::collect_vulkan_sdk_capabilities(probe, make_native_functions());

    require(result.blocked(), "missing SDK headers block native adapter readiness");
    require(
        result.status == vulkan_backend::vulkan_sdk_capability_status::headers_unavailable,
        "SDK capability maps missing headers");
    require(
        result.fallback_status
            == vulkan_backend::vulkan_sdk_adapter_fallback_status::headers_unavailable,
        "SDK capability maps missing headers to header fallback");
    require(!result.headers_available, "SDK capability records unavailable headers");
    require(
        result.diagnostic == "fake Vulkan headers missing",
        "SDK capability preserves fake header diagnostic");
}

void test_sdk_capability_blocks_on_api_version_mismatch()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_sdk_header_manifest manifest = make_ready_manifest();
    manifest.api_version = vulkan_backend::vulkan_sdk_api_version_1_3();
    vulkan_backend::fake_vulkan_sdk_header_probe probe(
        vulkan_backend::fake_vulkan_sdk_header_probe_options{
            .manifest = manifest,
        });
    const vulkan_backend::vulkan_sdk_capability_result result =
        vulkan_backend::collect_vulkan_sdk_capabilities(
            probe,
            make_native_functions(),
            vulkan_backend::vulkan_sdk_capability_request{
                .minimum_api_version = vulkan_backend::vulkan_sdk_api_version_1_4(),
            });

    require(result.blocked(), "old SDK API version blocks native adapter readiness");
    require(
        result.status == vulkan_backend::vulkan_sdk_capability_status::api_version_too_old,
        "SDK capability maps API version mismatch");
    require(
        result.fallback_status
            == vulkan_backend::vulkan_sdk_adapter_fallback_status::api_version_unavailable,
        "SDK capability maps API mismatch to API fallback");
    require(result.api_version_available, "SDK capability records available API version");
    require(!result.api_version_compatible, "SDK capability records incompatible API version");
}

void test_sdk_capability_blocks_on_missing_required_extension()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_sdk_header_manifest manifest = make_ready_manifest();
    manifest.supported_extensions = {"VK_KHR_surface"};
    vulkan_backend::fake_vulkan_sdk_header_probe probe(
        vulkan_backend::fake_vulkan_sdk_header_probe_options{
            .manifest = manifest,
        });
    const vulkan_backend::vulkan_sdk_capability_result result =
        vulkan_backend::collect_vulkan_sdk_capabilities(probe, make_native_functions());

    require(result.blocked(), "missing SDK extension blocks native adapter readiness");
    require(
        result.status == vulkan_backend::vulkan_sdk_capability_status::missing_required_extension,
        "SDK capability maps missing required extension");
    require(
        result.fallback_status
            == vulkan_backend::vulkan_sdk_adapter_fallback_status::extension_unavailable,
        "SDK capability maps missing extension to extension fallback");
    require(
        result.missing_required_extension == "VK_KHR_swapchain",
        "SDK capability records missing required extension name");
    require(!result.required_extensions_ready, "SDK capability records extensions not ready");
}

void test_sdk_capability_blocks_on_missing_native_function_table()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_sdk_header_probe probe(
        vulkan_backend::fake_vulkan_sdk_header_probe_options{
            .manifest = make_ready_manifest(),
        });
    const vulkan_backend::vulkan_sdk_capability_result result =
        vulkan_backend::collect_vulkan_sdk_capabilities(
            probe,
            make_native_functions("vkQueueSubmit"));

    require(result.blocked(), "missing native function table blocks SDK adapter readiness");
    require(
        result.status
            == vulkan_backend::vulkan_sdk_capability_status::native_function_table_unavailable,
        "SDK capability maps missing native function table");
    require(
        result.fallback_status
            == vulkan_backend::vulkan_sdk_adapter_fallback_status::native_function_table_unavailable,
        "SDK capability maps native table failure to native fallback");
    require(result.required_extensions_ready, "SDK capability checks extensions before native table");
    require(!result.native_function_table_ready, "SDK capability records native table not ready");
    require(
        result.native_functions.status
            == vulkan_backend::vulkan_native_function_table_status::missing_queue_submit_symbol,
        "SDK capability preserves native function table diagnostics");
}

} // namespace

int main()
{
    test_sdk_capability_names_and_versions_are_stable();
    test_sdk_external_header_probe_reports_compile_time_boundary();
    test_compile_time_sdk_header_probe_uses_external_header_manifest();
    test_sdk_native_path_readiness_summarizes_capability_states();
    test_sdk_capability_reports_adapter_ready_when_headers_and_native_symbols_match();
    test_sdk_capability_blocks_when_headers_are_missing();
    test_sdk_capability_blocks_on_api_version_mismatch();
    test_sdk_capability_blocks_on_missing_required_extension();
    test_sdk_capability_blocks_on_missing_native_function_table();
    return 0;
}
