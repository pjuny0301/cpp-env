#pragma once

#include "render/vulkan/vulkan_backend_sdk.h"

#include <string>

namespace quiz_vulkan::render::vulkan_backend {

struct vulkan_native_entrypoint_readiness_snapshot {
    bool function_table_checked = false;
    bool entrypoint_ready = false;
    vulkan_native_function_table_status function_table_status =
        vulkan_native_function_table_status::not_checked;
    std::string missing_symbol_name;
    std::string diagnostic;
    vulkan_sdk_native_path_readiness sdk_native_path;

    bool blocks_fake_execution() const
    {
        return (function_table_checked && !entrypoint_ready)
            || sdk_native_path.blocked();
    }

    bool native_path_ready() const
    {
        return entrypoint_ready
            && (!sdk_native_path.checked || sdk_native_path.ready());
    }
};

inline bool native_entrypoint_ready_for_stage(
    const vulkan_native_function_table_diagnostics& diagnostics,
    vulkan_native_entrypoint_stage stage)
{
    switch (stage) {
    case vulkan_native_entrypoint_stage::command_buffer_recording:
        return diagnostics.command_buffer_recording_ready;
    case vulkan_native_entrypoint_stage::queue_submit:
        return diagnostics.queue_submit_ready;
    case vulkan_native_entrypoint_stage::queue_present:
        return diagnostics.queue_present_ready;
    }

    return false;
}

inline vulkan_native_entrypoint_readiness_snapshot summarize_vulkan_native_entrypoint_readiness(
    const vulkan_native_function_table_diagnostics& diagnostics,
    vulkan_native_entrypoint_stage stage,
    const vulkan_sdk_capability_result& sdk_capabilities = {})
{
    const bool ready = !diagnostics.checked
        || native_entrypoint_ready_for_stage(diagnostics, stage);
    const vulkan_sdk_native_path_readiness sdk_native_path =
        summarize_vulkan_sdk_native_path_readiness(sdk_capabilities);
    const bool native_blocked = diagnostics.checked && !ready;
    return vulkan_native_entrypoint_readiness_snapshot{
        .function_table_checked = diagnostics.checked,
        .entrypoint_ready = ready,
        .function_table_status = diagnostics.status,
        .missing_symbol_name =
            diagnostics.checked && !ready ? diagnostics.missing_symbol_name : std::string{},
        .diagnostic = native_blocked ? diagnostics.diagnostic
            : sdk_native_path.blocked() ? sdk_native_path.diagnostic
                                        : std::string{},
        .sdk_native_path = sdk_native_path,
    };
}

inline vulkan_native_entrypoint_readiness_snapshot
summarize_vulkan_command_buffer_recording_native_readiness(
    const vulkan_native_function_table_diagnostics& diagnostics,
    const vulkan_sdk_capability_result& sdk_capabilities = {})
{
    return summarize_vulkan_native_entrypoint_readiness(
        diagnostics,
        vulkan_native_entrypoint_stage::command_buffer_recording,
        sdk_capabilities);
}

inline vulkan_native_entrypoint_readiness_snapshot
summarize_vulkan_queue_submit_native_readiness(
    const vulkan_native_function_table_diagnostics& diagnostics,
    const vulkan_sdk_capability_result& sdk_capabilities = {})
{
    return summarize_vulkan_native_entrypoint_readiness(
        diagnostics,
        vulkan_native_entrypoint_stage::queue_submit,
        sdk_capabilities);
}

inline vulkan_native_entrypoint_readiness_snapshot
summarize_vulkan_queue_present_native_readiness(
    const vulkan_native_function_table_diagnostics& diagnostics,
    const vulkan_sdk_capability_result& sdk_capabilities = {})
{
    return summarize_vulkan_native_entrypoint_readiness(
        diagnostics,
        vulkan_native_entrypoint_stage::queue_present,
        sdk_capabilities);
}

} // namespace quiz_vulkan::render::vulkan_backend
