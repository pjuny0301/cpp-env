#pragma once

#include "render/vulkan/vulkan_backend_loader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

struct vulkan_instance_create_request {
    std::string app_name = "quiz-vulkan";
    std::string engine_name = "quiz-vulkan-renderer";
    std::uint32_t api_version = 0;
    std::vector<std::string> required_instance_extensions;
    std::vector<std::string> optional_instance_extensions;
    std::vector<std::string> requested_layers;
    bool enable_validation = false;
};

struct vulkan_instance_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

enum class vulkan_instance_create_status {
    not_requested,
    created,
    loader_unavailable,
    missing_required_extension,
    missing_requested_layer,
    creation_failed,
};

inline std::string_view instance_create_status_name(vulkan_instance_create_status status)
{
    switch (status) {
    case vulkan_instance_create_status::not_requested:
        return "not_requested";
    case vulkan_instance_create_status::created:
        return "created";
    case vulkan_instance_create_status::loader_unavailable:
        return "loader_unavailable";
    case vulkan_instance_create_status::missing_required_extension:
        return "missing_required_extension";
    case vulkan_instance_create_status::missing_requested_layer:
        return "missing_requested_layer";
    case vulkan_instance_create_status::creation_failed:
        return "creation_failed";
    }

    return "unknown";
}

inline std::string_view vulkan_validation_layer_name()
{
    return "VK_LAYER_KHRONOS_validation";
}

struct vulkan_instance_extension_diagnostic {
    std::string extension_name;
    bool required = true;
    bool available = false;
    bool selected = false;

    bool missing_required() const
    {
        return required && !available;
    }
};

struct vulkan_instance_create_result {
    bool checked = false;
    vulkan_instance_create_status status = vulkan_instance_create_status::not_requested;
    vulkan_loader_readiness_state loader;
    vulkan_instance_handle handle;
    std::vector<std::string> selected_extensions;
    std::vector<vulkan_instance_extension_diagnostic> required_extension_diagnostics;
    std::size_t required_extension_count = 0;
    std::size_t available_required_extension_count = 0;
    std::string missing_required_extension;
    std::vector<std::string> enabled_layers;
    std::string diagnostic;

    bool required_extensions_ready() const
    {
        return checked && required_extension_count == available_required_extension_count
            && missing_required_extension.empty();
    }

    bool ready_for_device() const
    {
        return checked && status == vulkan_instance_create_status::created
            && loader.ready_for_instance() && handle.valid()
            && required_extensions_ready();
    }
};

class vulkan_instance_factory_interface {
public:
    virtual ~vulkan_instance_factory_interface() = default;

    virtual vulkan_instance_create_result create_instance(
        const vulkan_loader_readiness_state& loader_readiness,
        const vulkan_instance_create_request& request) = 0;
};

struct fake_vulkan_instance_factory_options {
    std::vector<std::string> supported_instance_extensions;
    std::vector<std::string> supported_layers;
    bool fail_creation = false;
    vulkan_instance_handle handle{.value = 1};
};

class fake_vulkan_instance_factory final : public vulkan_instance_factory_interface {
public:
    fake_vulkan_instance_factory();
    explicit fake_vulkan_instance_factory(fake_vulkan_instance_factory_options options);

    vulkan_instance_create_result create_instance(
        const vulkan_loader_readiness_state& loader_readiness,
        const vulkan_instance_create_request& request) override;

private:
    fake_vulkan_instance_factory_options options_;
};

namespace instance_detail {

inline bool contains_string(
    const std::vector<std::string>& values,
    const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

inline void append_unique_string(
    std::vector<std::string>& values,
    const std::string& value)
{
    if (value.empty() || contains_string(values, value)) {
        return;
    }

    values.push_back(value);
}

inline vulkan_instance_create_result make_instance_create_result(
    const vulkan_loader_readiness_state& loader_readiness)
{
    return vulkan_instance_create_result{
        .checked = true,
        .status = vulkan_instance_create_status::not_requested,
        .loader = loader_readiness,
        .handle = {},
        .selected_extensions = {},
        .required_extension_diagnostics = {},
        .required_extension_count = 0,
        .available_required_extension_count = 0,
        .missing_required_extension = {},
        .enabled_layers = {},
        .diagnostic = {},
    };
}

inline std::vector<std::string> requested_instance_layers(
    const vulkan_instance_create_request& request)
{
    std::vector<std::string> layers;
    layers.reserve(request.requested_layers.size() + (request.enable_validation ? 1 : 0));
    for (const std::string& layer : request.requested_layers) {
        append_unique_string(layers, layer);
    }
    if (request.enable_validation) {
        append_unique_string(layers, std::string{vulkan_validation_layer_name()});
    }

    return layers;
}

inline std::string make_missing_required_extension_diagnostic(
    const std::string& extension_name)
{
    return "missing required instance extension: " + extension_name;
}

inline std::string make_missing_requested_layer_diagnostic(
    const std::string& layer_name)
{
    return "missing requested instance layer: " + layer_name;
}

inline void record_required_extension_diagnostic(
    vulkan_instance_create_result& result,
    const std::string& extension_name,
    bool available)
{
    result.required_extension_diagnostics.push_back(
        vulkan_instance_extension_diagnostic{
            .extension_name = extension_name,
            .required = true,
            .available = available,
            .selected = available,
        });
    result.required_extension_count = result.required_extension_diagnostics.size();
    if (available) {
        ++result.available_required_extension_count;
    } else if (result.missing_required_extension.empty()) {
        result.missing_required_extension = extension_name;
    }
}

} // namespace instance_detail

inline fake_vulkan_instance_factory::fake_vulkan_instance_factory()
    : fake_vulkan_instance_factory(fake_vulkan_instance_factory_options{})
{
}

inline fake_vulkan_instance_factory::fake_vulkan_instance_factory(
    fake_vulkan_instance_factory_options options)
    : options_(std::move(options))
{
}

inline vulkan_instance_create_result fake_vulkan_instance_factory::create_instance(
    const vulkan_loader_readiness_state& loader_readiness,
    const vulkan_instance_create_request& request)
{
    vulkan_instance_create_result result =
        instance_detail::make_instance_create_result(loader_readiness);

    if (!loader_readiness.ready_for_instance()) {
        result.status = vulkan_instance_create_status::loader_unavailable;
        result.diagnostic = "Vulkan loader is not ready for instance creation";
        return result;
    }

    for (const std::string& extension_name : request.required_instance_extensions) {
        const bool extension_available = instance_detail::contains_string(
            options_.supported_instance_extensions,
            extension_name);
        instance_detail::record_required_extension_diagnostic(
            result,
            extension_name,
            extension_available);
        if (!extension_available) {
            result.status = vulkan_instance_create_status::missing_required_extension;
            result.diagnostic =
                instance_detail::make_missing_required_extension_diagnostic(extension_name);
            return result;
        }
        instance_detail::append_unique_string(result.selected_extensions, extension_name);
    }

    for (const std::string& extension_name : request.optional_instance_extensions) {
        if (instance_detail::contains_string(
                options_.supported_instance_extensions,
                extension_name)) {
            instance_detail::append_unique_string(result.selected_extensions, extension_name);
        }
    }

    for (const std::string& layer_name : instance_detail::requested_instance_layers(request)) {
        if (!instance_detail::contains_string(options_.supported_layers, layer_name)) {
            result.status = vulkan_instance_create_status::missing_requested_layer;
            result.diagnostic =
                instance_detail::make_missing_requested_layer_diagnostic(layer_name);
            return result;
        }
        instance_detail::append_unique_string(result.enabled_layers, layer_name);
    }

    if (options_.fail_creation || !options_.handle.valid()) {
        result.status = vulkan_instance_create_status::creation_failed;
        result.diagnostic = options_.fail_creation
            ? "Vulkan instance creation failed"
            : "Vulkan instance creation returned an invalid handle";
        return result;
    }

    result.status = vulkan_instance_create_status::created;
    result.handle = options_.handle;
    result.diagnostic = "Vulkan instance created";
    return result;
}

inline vulkan_instance_create_result create_vulkan_instance(
    vulkan_instance_factory_interface& factory,
    const vulkan_loader_readiness_state& loader_readiness,
    const vulkan_instance_create_request& request = {})
{
    return factory.create_instance(loader_readiness, request);
}

} // namespace quiz_vulkan::render::vulkan_backend
