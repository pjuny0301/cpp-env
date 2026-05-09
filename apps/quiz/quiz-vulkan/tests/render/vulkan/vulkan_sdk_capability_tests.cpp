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
    test_sdk_capability_reports_adapter_ready_when_headers_and_native_symbols_match();
    test_sdk_capability_blocks_when_headers_are_missing();
    test_sdk_capability_blocks_on_api_version_mismatch();
    test_sdk_capability_blocks_on_missing_required_extension();
    test_sdk_capability_blocks_on_missing_native_function_table();
    return 0;
}
