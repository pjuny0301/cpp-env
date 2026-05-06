#pragma once

#include <cstddef>
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

std::string_view loader_probe_status_name(vulkan_loader_probe_status status);
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

} // namespace quiz_vulkan::render::vulkan_backend
