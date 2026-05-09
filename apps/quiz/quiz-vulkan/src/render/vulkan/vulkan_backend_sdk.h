#pragma once

#include "render/vulkan/vulkan_backend_native_symbols.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

struct vulkan_sdk_api_version {
    std::uint32_t variant = 0;
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;

    bool valid() const
    {
        return major > 0;
    }

    std::uint32_t packed() const
    {
        return (variant << 29U) | (major << 22U) | (minor << 12U) | patch;
    }

    bool supports(vulkan_sdk_api_version required) const
    {
        return valid() && packed() >= required.packed();
    }
};

inline vulkan_sdk_api_version make_vulkan_sdk_api_version(
    std::uint32_t major,
    std::uint32_t minor,
    std::uint32_t patch = 0,
    std::uint32_t variant = 0)
{
    return vulkan_sdk_api_version{
        .variant = variant,
        .major = major,
        .minor = minor,
        .patch = patch,
    };
}

inline vulkan_sdk_api_version vulkan_sdk_api_version_1_0()
{
    return make_vulkan_sdk_api_version(1, 0);
}

inline vulkan_sdk_api_version vulkan_sdk_api_version_1_3()
{
    return make_vulkan_sdk_api_version(1, 3);
}

inline vulkan_sdk_api_version vulkan_sdk_api_version_1_4()
{
    return make_vulkan_sdk_api_version(1, 4);
}

enum class vulkan_sdk_header_probe_status {
    not_checked,
    available,
    unavailable,
};

std::string_view sdk_header_probe_status_name(vulkan_sdk_header_probe_status status);

enum class vulkan_sdk_capability_status {
    not_checked,
    ready,
    headers_unavailable,
    api_version_unavailable,
    api_version_too_old,
    missing_required_extension,
    native_function_table_unavailable,
};

std::string_view sdk_capability_status_name(vulkan_sdk_capability_status status);

enum class vulkan_sdk_adapter_fallback_status {
    none,
    headers_unavailable,
    api_version_unavailable,
    extension_unavailable,
    native_function_table_unavailable,
};

std::string_view sdk_adapter_fallback_status_name(
    vulkan_sdk_adapter_fallback_status status);

enum class vulkan_sdk_native_path_status {
    not_checked,
    ready,
    sdk_missing,
    version_mismatch,
    extension_missing,
    function_table_blocked,
};

std::string_view sdk_native_path_status_name(vulkan_sdk_native_path_status status);

struct vulkan_sdk_header_manifest {
    bool headers_available = false;
    vulkan_sdk_api_version api_version;
    std::uint32_t header_version = 0;
    std::string sdk_tag;
    std::string source_label;
    std::vector<std::string> supported_extensions;
    std::string diagnostic;

    bool supports_api_version(vulkan_sdk_api_version required) const
    {
        return headers_available && api_version.supports(required);
    }

    bool supports_extension(std::string_view extension_name) const
    {
        return std::find(
                   supported_extensions.begin(),
                   supported_extensions.end(),
                   extension_name)
            != supported_extensions.end();
    }
};

struct vulkan_sdk_header_probe_request {
    std::string manifest_name = "vulkan-headers";
};

struct vulkan_sdk_header_probe_result {
    bool checked = false;
    vulkan_sdk_header_probe_status status = vulkan_sdk_header_probe_status::not_checked;
    vulkan_sdk_header_manifest manifest;

    bool available() const
    {
        return checked && status == vulkan_sdk_header_probe_status::available
            && manifest.headers_available;
    }
};

class vulkan_sdk_header_probe_interface {
public:
    virtual ~vulkan_sdk_header_probe_interface() = default;

    virtual vulkan_sdk_header_probe_result probe_headers(
        const vulkan_sdk_header_probe_request& request) = 0;
};

struct fake_vulkan_sdk_header_probe_options {
    vulkan_sdk_header_manifest manifest;
};

struct fake_vulkan_sdk_header_probe_state {
    std::size_t probe_call_count = 0;
    std::vector<std::string> requested_manifests;
};

class fake_vulkan_sdk_header_probe final : public vulkan_sdk_header_probe_interface {
public:
    fake_vulkan_sdk_header_probe() = default;
    explicit fake_vulkan_sdk_header_probe(fake_vulkan_sdk_header_probe_options options)
        : options_(std::move(options))
    {
    }

    vulkan_sdk_header_probe_result probe_headers(
        const vulkan_sdk_header_probe_request& request) override
    {
        ++state_.probe_call_count;
        state_.requested_manifests.push_back(request.manifest_name);
        return vulkan_sdk_header_probe_result{
            .checked = true,
            .status = options_.manifest.headers_available
                ? vulkan_sdk_header_probe_status::available
                : vulkan_sdk_header_probe_status::unavailable,
            .manifest = options_.manifest,
        };
    }

    const fake_vulkan_sdk_header_probe_state& state() const
    {
        return state_;
    }

private:
    fake_vulkan_sdk_header_probe_options options_;
    fake_vulkan_sdk_header_probe_state state_;
};

struct vulkan_sdk_capability_request {
    vulkan_sdk_api_version minimum_api_version = vulkan_sdk_api_version_1_0();
    bool include_default_backend_extensions = true;
    std::vector<std::string> required_extensions;
    bool require_native_function_table = true;
};

struct vulkan_sdk_capability_result {
    bool checked = false;
    vulkan_sdk_capability_status status = vulkan_sdk_capability_status::not_checked;
    vulkan_sdk_adapter_fallback_status fallback_status =
        vulkan_sdk_adapter_fallback_status::headers_unavailable;
    vulkan_sdk_header_probe_result headers;
    vulkan_native_function_table_diagnostics native_functions;
    vulkan_sdk_api_version minimum_api_version;
    bool headers_available = false;
    bool api_version_available = false;
    bool api_version_compatible = false;
    bool required_extensions_ready = false;
    bool native_function_table_required = false;
    bool native_function_table_ready = false;
    std::size_t required_extension_count = 0;
    std::size_t available_required_extension_count = 0;
    std::string missing_required_extension;
    std::string diagnostic;

    vulkan_sdk_native_path_status native_path_status() const;

    bool adapter_ready() const
    {
        return checked && status == vulkan_sdk_capability_status::ready
            && fallback_status == vulkan_sdk_adapter_fallback_status::none
            && headers_available && api_version_compatible
            && required_extensions_ready
            && (!native_function_table_required || native_function_table_ready);
    }

    bool blocked() const
    {
        return checked && !adapter_ready();
    }
};

inline vulkan_sdk_native_path_status sdk_native_path_status_for(
    const vulkan_sdk_capability_result& result)
{
    if (!result.checked) {
        return vulkan_sdk_native_path_status::not_checked;
    }
    if (result.adapter_ready()) {
        return vulkan_sdk_native_path_status::ready;
    }

    switch (result.status) {
    case vulkan_sdk_capability_status::not_checked:
        return vulkan_sdk_native_path_status::not_checked;
    case vulkan_sdk_capability_status::ready:
        return vulkan_sdk_native_path_status::ready;
    case vulkan_sdk_capability_status::headers_unavailable:
        return vulkan_sdk_native_path_status::sdk_missing;
    case vulkan_sdk_capability_status::api_version_unavailable:
    case vulkan_sdk_capability_status::api_version_too_old:
        return vulkan_sdk_native_path_status::version_mismatch;
    case vulkan_sdk_capability_status::missing_required_extension:
        return vulkan_sdk_native_path_status::extension_missing;
    case vulkan_sdk_capability_status::native_function_table_unavailable:
        return vulkan_sdk_native_path_status::function_table_blocked;
    }

    return vulkan_sdk_native_path_status::not_checked;
}

inline vulkan_sdk_native_path_status
vulkan_sdk_capability_result::native_path_status() const
{
    return sdk_native_path_status_for(*this);
}

struct vulkan_sdk_native_path_readiness {
    bool checked = false;
    vulkan_sdk_native_path_status status = vulkan_sdk_native_path_status::not_checked;
    bool sdk_capability_checked = false;
    bool sdk_adapter_ready = false;
    vulkan_sdk_capability_status sdk_capability_status =
        vulkan_sdk_capability_status::not_checked;
    vulkan_sdk_adapter_fallback_status sdk_adapter_fallback_status =
        vulkan_sdk_adapter_fallback_status::none;
    bool headers_available = false;
    bool api_version_compatible = false;
    bool required_extensions_ready = false;
    bool native_function_table_required = false;
    bool native_function_table_ready = false;
    vulkan_native_function_table_status native_function_table_status =
        vulkan_native_function_table_status::not_checked;
    std::string missing_required_extension;
    std::string missing_native_symbol_name;
    std::string diagnostic;

    bool ready() const
    {
        return checked && status == vulkan_sdk_native_path_status::ready
            && sdk_adapter_ready;
    }

    bool blocked() const
    {
        return checked && status != vulkan_sdk_native_path_status::ready;
    }
};

inline vulkan_sdk_native_path_readiness summarize_vulkan_sdk_native_path_readiness(
    const vulkan_sdk_capability_result& result)
{
    if (!result.checked) {
        return vulkan_sdk_native_path_readiness{};
    }

    return vulkan_sdk_native_path_readiness{
        .checked = true,
        .status = result.native_path_status(),
        .sdk_capability_checked = result.checked,
        .sdk_adapter_ready = result.adapter_ready(),
        .sdk_capability_status = result.status,
        .sdk_adapter_fallback_status = result.fallback_status,
        .headers_available = result.headers_available,
        .api_version_compatible = result.api_version_compatible,
        .required_extensions_ready = result.required_extensions_ready,
        .native_function_table_required = result.native_function_table_required,
        .native_function_table_ready = result.native_function_table_ready,
        .native_function_table_status = result.native_functions.status,
        .missing_required_extension = result.missing_required_extension,
        .missing_native_symbol_name = result.native_functions.missing_symbol_name,
        .diagnostic = result.diagnostic,
    };
}

inline std::vector<std::string> default_vulkan_sdk_backend_extensions()
{
    return {
        "VK_KHR_surface",
        "VK_KHR_swapchain",
    };
}

inline std::vector<std::string> merge_vulkan_sdk_required_extensions(
    const vulkan_sdk_capability_request& request)
{
    std::vector<std::string> extensions;
    if (request.include_default_backend_extensions) {
        extensions = default_vulkan_sdk_backend_extensions();
    }
    for (const std::string& extension : request.required_extensions) {
        if (std::find(extensions.begin(), extensions.end(), extension) == extensions.end()) {
            extensions.push_back(extension);
        }
    }
    return extensions;
}

inline vulkan_sdk_capability_result collect_vulkan_sdk_capabilities(
    vulkan_sdk_header_probe_interface& header_probe,
    const vulkan_native_function_table_diagnostics& native_functions,
    const vulkan_sdk_capability_request& request = {})
{
    const vulkan_sdk_header_probe_result header_result =
        header_probe.probe_headers(vulkan_sdk_header_probe_request{});
    const std::vector<std::string> required_extensions =
        merge_vulkan_sdk_required_extensions(request);

    vulkan_sdk_capability_result result{
        .checked = true,
        .status = vulkan_sdk_capability_status::not_checked,
        .fallback_status = vulkan_sdk_adapter_fallback_status::headers_unavailable,
        .headers = header_result,
        .native_functions = native_functions,
        .minimum_api_version = request.minimum_api_version,
        .headers_available = header_result.available(),
        .api_version_available = header_result.manifest.api_version.valid(),
        .api_version_compatible =
            header_result.manifest.supports_api_version(request.minimum_api_version),
        .required_extensions_ready = false,
        .native_function_table_required = request.require_native_function_table,
        .native_function_table_ready = !request.require_native_function_table
            || native_functions.ready_for_backend_path(),
        .required_extension_count = required_extensions.size(),
        .available_required_extension_count = 0,
        .missing_required_extension = {},
        .diagnostic = {},
    };

    if (!result.headers_available) {
        result.status = vulkan_sdk_capability_status::headers_unavailable;
        result.fallback_status = vulkan_sdk_adapter_fallback_status::headers_unavailable;
        result.diagnostic = header_result.manifest.diagnostic.empty()
            ? "Vulkan SDK headers are unavailable for native adapter wiring"
            : header_result.manifest.diagnostic;
        return result;
    }
    if (!result.api_version_available) {
        result.status = vulkan_sdk_capability_status::api_version_unavailable;
        result.fallback_status = vulkan_sdk_adapter_fallback_status::api_version_unavailable;
        result.diagnostic = "Vulkan SDK header API version is unavailable";
        return result;
    }
    if (!result.api_version_compatible) {
        result.status = vulkan_sdk_capability_status::api_version_too_old;
        result.fallback_status = vulkan_sdk_adapter_fallback_status::api_version_unavailable;
        result.diagnostic = "Vulkan SDK header API version is older than the adapter requirement";
        return result;
    }

    for (const std::string& required_extension : required_extensions) {
        if (!header_result.manifest.supports_extension(required_extension)) {
            result.status = vulkan_sdk_capability_status::missing_required_extension;
            result.fallback_status = vulkan_sdk_adapter_fallback_status::extension_unavailable;
            result.missing_required_extension = required_extension;
            result.diagnostic = "Vulkan SDK headers do not declare required extension: "
                + required_extension;
            return result;
        }
        ++result.available_required_extension_count;
    }
    result.required_extensions_ready = true;

    if (!result.native_function_table_ready) {
        result.status = vulkan_sdk_capability_status::native_function_table_unavailable;
        result.fallback_status =
            vulkan_sdk_adapter_fallback_status::native_function_table_unavailable;
        result.diagnostic = native_functions.diagnostic.empty()
            ? "Native Vulkan function table is unavailable for SDK-backed adapter wiring"
            : native_functions.diagnostic;
        return result;
    }

    result.status = vulkan_sdk_capability_status::ready;
    result.fallback_status = vulkan_sdk_adapter_fallback_status::none;
    result.diagnostic = "Vulkan SDK headers and native function table are adapter-ready";
    return result;
}

} // namespace quiz_vulkan::render::vulkan_backend
