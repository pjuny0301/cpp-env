#pragma once

#include "render/vulkan/vulkan_backend_loader.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

enum class vulkan_backend_fallback_reason;

struct vulkan_native_function_pointer {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

enum class vulkan_native_entrypoint_stage {
    command_buffer_recording,
    queue_submit,
    swapchain_create,
    swapchain_destroy,
    swapchain_images,
    swapchain_acquire,
    queue_present,
};

std::string_view native_entrypoint_stage_name(vulkan_native_entrypoint_stage stage);

enum class vulkan_native_function_table_status {
    not_checked,
    ready,
    loader_unavailable,
    required_extension_unavailable,
    missing_command_buffer_recording_symbol,
    missing_queue_submit_symbol,
    missing_swapchain_create_symbol,
    missing_swapchain_destroy_symbol,
    missing_swapchain_images_symbol,
    missing_swapchain_acquire_symbol,
    missing_queue_present_symbol,
};

std::string_view native_function_table_status_name(
    vulkan_native_function_table_status status);

struct vulkan_native_entrypoint_symbol_request {
    vulkan_native_entrypoint_stage stage =
        vulkan_native_entrypoint_stage::command_buffer_recording;
    std::string name;
    bool required = true;
};

struct vulkan_native_function_table_request {
    std::vector<vulkan_native_entrypoint_symbol_request> symbols;
    bool include_default_backend_entrypoints = true;
    std::vector<std::string> required_extensions;
    std::vector<std::string> available_extensions;
    bool include_default_swapchain_extension = false;
};

struct vulkan_native_entrypoint_symbol_diagnostics {
    vulkan_native_entrypoint_stage stage =
        vulkan_native_entrypoint_stage::command_buffer_recording;
    std::string name;
    bool required = true;
    vulkan_native_function_pointer pointer;
    bool available = false;

    bool completed() const
    {
        return !required || (available && pointer.valid());
    }
};

struct vulkan_native_function_table_diagnostics {
    bool checked = false;
    vulkan_native_function_table_status status =
        vulkan_native_function_table_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason =
        static_cast<vulkan_backend_fallback_reason>(0);
    vulkan_loader_readiness_state loader;
    bool loader_ready = false;
    bool required_extensions_ready = true;
    bool command_buffer_recording_ready = false;
    bool queue_submit_ready = false;
    bool swapchain_create_ready = false;
    bool swapchain_destroy_ready = false;
    bool swapchain_images_ready = false;
    bool swapchain_acquire_ready = false;
    bool queue_present_ready = false;
    std::size_t required_extension_count = 0;
    std::size_t available_required_extension_count = 0;
    std::size_t requested_symbol_count = 0;
    std::size_t required_symbol_count = 0;
    std::size_t available_symbol_count = 0;
    std::size_t missing_required_symbol_count = 0;
    std::string missing_required_extension;
    vulkan_native_entrypoint_stage missing_symbol_stage =
        vulkan_native_entrypoint_stage::command_buffer_recording;
    std::string missing_symbol_name;
    std::vector<vulkan_native_entrypoint_symbol_diagnostics> symbols;
    std::string diagnostic;

    bool ready_for_backend_path() const;
    bool blocked() const;
};

struct vulkan_native_swapchain_entrypoint_readiness {
    bool checked = false;
    bool function_table_checked = false;
    bool required_extensions_ready = false;
    bool create_swapchain_ready = false;
    bool destroy_swapchain_ready = false;
    bool get_swapchain_images_ready = false;
    bool acquire_next_image_ready = false;
    bool queue_present_ready = false;
    vulkan_native_function_table_status function_table_status =
        vulkan_native_function_table_status::not_checked;
    vulkan_backend_fallback_reason fallback_reason =
        static_cast<vulkan_backend_fallback_reason>(0);
    std::size_t required_extension_count = 0;
    std::size_t available_required_extension_count = 0;
    std::string missing_required_extension;
    std::string missing_symbol_name;
    std::string diagnostic;

    bool ready_for_swapchain_create() const;
    bool ready_for_swapchain_acquire() const;
    bool ready_for_swapchain_present() const;
    bool ready_for_swapchain_path() const;
    bool blocked() const;
};

class vulkan_native_symbol_resolver_interface {
public:
    virtual ~vulkan_native_symbol_resolver_interface() = default;

    virtual vulkan_native_function_pointer resolve_symbol(std::string_view symbol_name) = 0;
};

struct fake_vulkan_native_symbol_resolver_options {
    bool default_available = true;
    std::vector<std::string> available_symbols;
    std::vector<std::string> missing_symbols;
    vulkan_native_function_pointer pointer_base{.value = 1000};
};

struct fake_vulkan_native_symbol_resolver_state {
    std::size_t resolve_call_count = 0;
    std::vector<std::string> requested_symbols;
    std::vector<std::string> resolved_symbols;
    std::vector<std::string> missing_symbols;
};

class fake_vulkan_native_symbol_resolver final
    : public vulkan_native_symbol_resolver_interface {
public:
    fake_vulkan_native_symbol_resolver();
    explicit fake_vulkan_native_symbol_resolver(
        fake_vulkan_native_symbol_resolver_options options);

    vulkan_native_function_pointer resolve_symbol(std::string_view symbol_name) override;
    const fake_vulkan_native_symbol_resolver_state& state() const;

private:
    fake_vulkan_native_symbol_resolver_options options_;
    fake_vulkan_native_symbol_resolver_state state_;
};

struct system_vulkan_native_symbol_resolver_options {
    std::vector<std::string> candidate_library_names;
    bool use_default_library_names = true;
};

struct vulkan_system_symbol_resolution_result {
    bool checked = false;
    std::string symbol_name;
    std::vector<std::string> attempted_library_names;
    std::string loaded_library_name;
    bool loader_library_available = false;
    bool instance_proc_address_available = false;
    bool resolved_via_instance_proc_address = false;
    bool resolved_via_direct_export = false;
    vulkan_native_function_pointer pointer;
    std::string diagnostic;

    bool resolved() const
    {
        return checked && pointer.valid()
            && (resolved_via_instance_proc_address || resolved_via_direct_export);
    }
};

class system_vulkan_native_symbol_resolver final
    : public vulkan_native_symbol_resolver_interface {
public:
    system_vulkan_native_symbol_resolver();
    explicit system_vulkan_native_symbol_resolver(
        system_vulkan_native_symbol_resolver_options options);

    vulkan_native_function_pointer resolve_symbol(std::string_view symbol_name) override;
    vulkan_system_symbol_resolution_result resolve_symbol_with_diagnostics(
        std::string_view symbol_name);

private:
    system_vulkan_native_symbol_resolver_options options_;
};

std::vector<vulkan_native_entrypoint_symbol_request>
default_vulkan_native_backend_entrypoints();

std::vector<vulkan_native_entrypoint_symbol_request>
default_vulkan_native_swapchain_entrypoints();

std::vector<std::string> default_vulkan_native_swapchain_extensions();

vulkan_native_function_table_diagnostics collect_vulkan_native_function_table(
    vulkan_native_symbol_resolver_interface& resolver,
    const vulkan_loader_readiness_state& loader,
    const vulkan_native_function_table_request& request = {});

vulkan_native_swapchain_entrypoint_readiness summarize_vulkan_native_swapchain_entrypoints(
    const vulkan_native_function_table_diagnostics& diagnostics);

} // namespace quiz_vulkan::render::vulkan_backend
