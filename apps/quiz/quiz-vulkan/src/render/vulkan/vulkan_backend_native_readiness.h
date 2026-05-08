#pragma once

#include "render/vulkan/vulkan_backend_native_symbols.h"

#include <string>

namespace quiz_vulkan::render::vulkan_backend {

struct vulkan_native_entrypoint_readiness_snapshot {
    bool function_table_checked = false;
    bool entrypoint_ready = false;
    vulkan_native_function_table_status function_table_status =
        vulkan_native_function_table_status::not_checked;
    std::string missing_symbol_name;
    std::string diagnostic;

    bool blocks_fake_execution() const
    {
        return function_table_checked && !entrypoint_ready;
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
    vulkan_native_entrypoint_stage stage)
{
    const bool ready = !diagnostics.checked
        || native_entrypoint_ready_for_stage(diagnostics, stage);
    return vulkan_native_entrypoint_readiness_snapshot{
        .function_table_checked = diagnostics.checked,
        .entrypoint_ready = ready,
        .function_table_status = diagnostics.status,
        .missing_symbol_name =
            diagnostics.checked && !ready ? diagnostics.missing_symbol_name : std::string{},
        .diagnostic = diagnostics.checked && !ready ? diagnostics.diagnostic : std::string{},
    };
}

inline vulkan_native_entrypoint_readiness_snapshot
summarize_vulkan_command_buffer_recording_native_readiness(
    const vulkan_native_function_table_diagnostics& diagnostics)
{
    return summarize_vulkan_native_entrypoint_readiness(
        diagnostics,
        vulkan_native_entrypoint_stage::command_buffer_recording);
}

inline vulkan_native_entrypoint_readiness_snapshot
summarize_vulkan_queue_submit_native_readiness(
    const vulkan_native_function_table_diagnostics& diagnostics)
{
    return summarize_vulkan_native_entrypoint_readiness(
        diagnostics,
        vulkan_native_entrypoint_stage::queue_submit);
}

inline vulkan_native_entrypoint_readiness_snapshot
summarize_vulkan_queue_present_native_readiness(
    const vulkan_native_function_table_diagnostics& diagnostics)
{
    return summarize_vulkan_native_entrypoint_readiness(
        diagnostics,
        vulkan_native_entrypoint_stage::queue_present);
}

} // namespace quiz_vulkan::render::vulkan_backend
