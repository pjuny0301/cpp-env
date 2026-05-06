#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

enum class vulkan_loader_probe_status {
    not_checked,
    available,
    library_missing,
    required_symbol_missing,
};

enum class vulkan_loader_readiness_status {
    not_checked,
    ready,
    library_missing,
    required_symbol_missing,
};

std::string_view loader_probe_status_name(vulkan_loader_probe_status status);
std::string_view loader_readiness_status_name(vulkan_loader_readiness_status status);
std::string_view vulkan_loader_required_symbol_name();

struct vulkan_loader_probe_request {
    std::vector<std::string> candidate_library_names;
    std::string required_symbol_name = "vkGetInstanceProcAddr";
    bool use_default_library_names = true;
};

struct vulkan_loader_probe_result {
    bool checked = false;
    vulkan_loader_probe_status status = vulkan_loader_probe_status::not_checked;
    std::vector<std::string> attempted_library_names;
    std::string loaded_library_name;
    std::string required_symbol_name = "vkGetInstanceProcAddr";
    std::size_t attempted_library_count = 0;
    bool library_found = false;
    bool required_symbol_found = false;

    bool available() const
    {
        return checked && status == vulkan_loader_probe_status::available
            && library_found && required_symbol_found;
    }
};

struct vulkan_loader_readiness_state {
    bool checked = false;
    vulkan_loader_readiness_status status = vulkan_loader_readiness_status::not_checked;
    vulkan_loader_probe_status probe_status = vulkan_loader_probe_status::not_checked;
    bool loader_library_available = false;
    bool instance_proc_address_available = false;
    bool instance_ready = false;
    std::string loaded_library_name;
    std::string required_symbol_name = "vkGetInstanceProcAddr";
    std::size_t attempted_library_count = 0;

    bool ready_for_instance() const
    {
        return checked && status == vulkan_loader_readiness_status::ready
            && loader_library_available && instance_proc_address_available
            && instance_ready;
    }
};

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

std::string_view instance_create_status_name(vulkan_instance_create_status status);
std::string_view vulkan_validation_layer_name();

struct vulkan_instance_create_result {
    bool checked = false;
    vulkan_instance_create_status status = vulkan_instance_create_status::not_requested;
    vulkan_loader_readiness_state loader;
    vulkan_instance_handle handle;
    std::vector<std::string> selected_extensions;
    std::vector<std::string> enabled_layers;
    std::string diagnostic;

    bool ready_for_device() const
    {
        return checked && status == vulkan_instance_create_status::created
            && loader.ready_for_instance() && handle.valid();
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

class vulkan_loader_interface {
public:
    virtual ~vulkan_loader_interface() = default;

    virtual vulkan_loader_probe_result probe_loader(
        const vulkan_loader_probe_request& request) = 0;
};

struct fake_vulkan_loader_options {
    std::string library_name = "fake-vulkan-loader";
    bool library_available = true;
    bool required_symbol_available = true;
};

class fake_vulkan_loader final : public vulkan_loader_interface {
public:
    fake_vulkan_loader();
    explicit fake_vulkan_loader(fake_vulkan_loader_options options);

    vulkan_loader_probe_result probe_loader(
        const vulkan_loader_probe_request& request) override;

private:
    fake_vulkan_loader_options options_;
};

class system_vulkan_loader final : public vulkan_loader_interface {
public:
    vulkan_loader_probe_result probe_loader(
        const vulkan_loader_probe_request& request) override;
};

std::vector<std::string> default_vulkan_loader_library_names();

vulkan_loader_probe_result probe_vulkan_loader(
    vulkan_loader_interface& loader,
    const vulkan_loader_probe_request& request = {});

vulkan_loader_readiness_state make_vulkan_loader_readiness_state(
    const vulkan_loader_probe_result& probe_result);

vulkan_instance_create_result create_vulkan_instance(
    vulkan_instance_factory_interface& factory,
    const vulkan_loader_readiness_state& loader_readiness,
    const vulkan_instance_create_request& request = {});

} // namespace quiz_vulkan::render::vulkan_backend
