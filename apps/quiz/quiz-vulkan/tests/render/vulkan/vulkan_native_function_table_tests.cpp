#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_native_symbols.h"

#include <cassert>
#include <cstdio>
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

void test_native_function_table_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::native_entrypoint_stage_name(
            vulkan_backend::vulkan_native_entrypoint_stage::command_buffer_recording)
            == std::string_view{"command_buffer_recording"},
        "native entrypoint stage name for command buffer recording is stable");
    require(
        vulkan_backend::native_entrypoint_stage_name(
            vulkan_backend::vulkan_native_entrypoint_stage::queue_submit)
            == std::string_view{"queue_submit"},
        "native entrypoint stage name for queue submit is stable");
    require(
        vulkan_backend::native_entrypoint_stage_name(
            vulkan_backend::vulkan_native_entrypoint_stage::queue_present)
            == std::string_view{"queue_present"},
        "native entrypoint stage name for queue present is stable");
    require(
        vulkan_backend::native_function_table_status_name(
            vulkan_backend::vulkan_native_function_table_status::ready)
            == std::string_view{"ready"},
        "native function table status name for ready is stable");
    require(
        vulkan_backend::native_function_table_status_name(
            vulkan_backend::vulkan_native_function_table_status::missing_queue_present_symbol)
            == std::string_view{"missing_queue_present_symbol"},
        "native function table status name for missing queue present is stable");
}

void test_native_function_table_collects_default_backend_entrypoints()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver;
    const vulkan_backend::vulkan_native_function_table_diagnostics diagnostics =
        vulkan_backend::collect_vulkan_native_function_table(
            resolver,
            make_ready_loader());

    require(diagnostics.checked, "native function table diagnostics are checked");
    require(diagnostics.ready_for_backend_path(), "native function table is ready");
    require(!diagnostics.blocked(), "native function table does not block");
    require(
        diagnostics.status == vulkan_backend::vulkan_native_function_table_status::ready,
        "native function table reports ready");
    require(
        diagnostics.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "native function table has no fallback");
    require(diagnostics.loader_ready, "native function table records ready loader");
    require(
        diagnostics.command_buffer_recording_ready,
        "native function table records command buffer recording symbols ready");
    require(diagnostics.queue_submit_ready, "native function table records queue submit symbol ready");
    require(diagnostics.queue_present_ready, "native function table records queue present symbol ready");
    require(diagnostics.requested_symbol_count == 11, "native function table requests default backend symbols");
    require(diagnostics.required_symbol_count == 11, "native function table counts required symbols");
    require(diagnostics.available_symbol_count == 11, "native function table counts available symbols");
    require(diagnostics.missing_required_symbol_count == 0, "native function table has no missing symbols");
    require(
        resolver.state().resolve_call_count == 11,
        "fake resolver is called once per default native symbol");
    require(
        diagnostics.symbols.front().name == "vkBeginCommandBuffer",
        "native function table preserves stable command recording symbol order");
    require(
        diagnostics.symbols.back().name == "vkQueuePresentKHR",
        "native function table preserves stable queue present symbol order");
    for (const vulkan_backend::vulkan_native_entrypoint_symbol_diagnostics& symbol :
         diagnostics.symbols) {
        require(symbol.completed(), "each default native symbol completes");
        require(symbol.pointer.valid(), "each default native symbol has an opaque pointer");
    }
}

void test_native_function_table_blocks_when_loader_is_unavailable()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver;
    const vulkan_backend::vulkan_native_function_table_diagnostics diagnostics =
        vulkan_backend::collect_vulkan_native_function_table(
            resolver,
            vulkan_backend::vulkan_loader_readiness_state{
                .checked = true,
                .status = vulkan_backend::vulkan_loader_readiness_status::library_missing,
                .probe_status = vulkan_backend::vulkan_loader_probe_status::library_missing,
            });

    require(diagnostics.checked, "loader-unavailable native table diagnostics are checked");
    require(diagnostics.blocked(), "loader-unavailable native table blocks");
    require(
        diagnostics.status == vulkan_backend::vulkan_native_function_table_status::loader_unavailable,
        "native function table reports loader unavailable");
    require(
        diagnostics.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable,
        "native function table maps loader unavailable to instance fallback");
    require(!diagnostics.loader_ready, "native function table records unavailable loader");
    require(
        resolver.state().resolve_call_count == 0,
        "native function table does not resolve symbols without loader readiness");
}

void test_native_function_table_maps_missing_command_recording_symbol()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_symbol_resolver_options{
            .missing_symbols = {"vkCmdDraw"},
        });
    const vulkan_backend::vulkan_native_function_table_diagnostics diagnostics =
        vulkan_backend::collect_vulkan_native_function_table(
            resolver,
            make_ready_loader());

    require(diagnostics.blocked(), "missing command recording symbol blocks native table");
    require(
        diagnostics.status
            == vulkan_backend::vulkan_native_function_table_status::missing_command_buffer_recording_symbol,
        "native function table maps missing command recording symbol");
    require(
        diagnostics.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "native function table maps missing command symbol to command recording fallback");
    require(
        diagnostics.missing_symbol_stage
            == vulkan_backend::vulkan_native_entrypoint_stage::command_buffer_recording,
        "native function table records missing command symbol stage");
    require(
        diagnostics.missing_symbol_name == "vkCmdDraw",
        "native function table records missing command symbol name");
    require(!diagnostics.command_buffer_recording_ready, "command recording symbols are not ready");
    require(diagnostics.queue_submit_ready, "queue submit symbol remains ready");
    require(diagnostics.queue_present_ready, "queue present symbol remains ready");
}

void test_native_function_table_maps_missing_queue_symbols()
{
    using namespace quiz_vulkan::render;

    {
        vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
            vulkan_backend::fake_vulkan_native_symbol_resolver_options{
                .missing_symbols = {"vkQueueSubmit"},
            });
        const vulkan_backend::vulkan_native_function_table_diagnostics diagnostics =
            vulkan_backend::collect_vulkan_native_function_table(
                resolver,
                make_ready_loader());

        require(
            diagnostics.status
                == vulkan_backend::vulkan_native_function_table_status::missing_queue_submit_symbol,
            "native function table maps missing queue submit symbol");
        require(
            diagnostics.fallback_reason
                == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
            "native function table maps missing queue submit to submit fallback");
        require(!diagnostics.queue_submit_ready, "queue submit symbol is not ready");
    }

    {
        vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
            vulkan_backend::fake_vulkan_native_symbol_resolver_options{
                .missing_symbols = {"vkQueuePresentKHR"},
            });
        const vulkan_backend::vulkan_native_function_table_diagnostics diagnostics =
            vulkan_backend::collect_vulkan_native_function_table(
                resolver,
                make_ready_loader());

        require(
            diagnostics.status
                == vulkan_backend::vulkan_native_function_table_status::missing_queue_present_symbol,
            "native function table maps missing queue present symbol");
        require(
            diagnostics.fallback_reason
                == vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
            "native function table maps missing queue present to present fallback");
        require(!diagnostics.queue_present_ready, "queue present symbol is not ready");
    }
}

void test_native_function_table_allows_missing_optional_custom_symbol()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_symbol_resolver_options{
            .missing_symbols = {"vkCmdPushConstants"},
        });
    const vulkan_backend::vulkan_native_function_table_diagnostics diagnostics =
        vulkan_backend::collect_vulkan_native_function_table(
            resolver,
            make_ready_loader(),
            vulkan_backend::vulkan_native_function_table_request{
                .symbols = {
                    vulkan_backend::vulkan_native_entrypoint_symbol_request{
                        .stage =
                            vulkan_backend::vulkan_native_entrypoint_stage::command_buffer_recording,
                        .name = "vkCmdPushConstants",
                        .required = false,
                    },
                },
            });

    require(
        diagnostics.status == vulkan_backend::vulkan_native_function_table_status::ready,
        "native function table permits missing optional custom symbol");
    require(diagnostics.ready_for_backend_path(), "optional missing symbol does not block backend path");
    require(diagnostics.requested_symbol_count == 12, "native function table includes optional symbol");
    require(diagnostics.available_symbol_count == 11, "native function table counts optional missing symbol");
    require(
        diagnostics.missing_required_symbol_count == 0,
        "native function table does not count optional missing as required");
    require(!diagnostics.symbols.back().available, "optional missing symbol records missing availability");
    require(diagnostics.symbols.back().completed(), "optional missing symbol remains non-blocking");
}

} // namespace

int main()
{
    test_native_function_table_names_are_stable();
    test_native_function_table_collects_default_backend_entrypoints();
    test_native_function_table_blocks_when_loader_is_unavailable();
    test_native_function_table_maps_missing_command_recording_symbol();
    test_native_function_table_maps_missing_queue_symbols();
    test_native_function_table_allows_missing_optional_custom_symbol();
    return 0;
}
