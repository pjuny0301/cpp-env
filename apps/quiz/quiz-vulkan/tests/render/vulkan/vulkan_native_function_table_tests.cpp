#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_native_symbols.h"

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
            vulkan_backend::vulkan_native_entrypoint_stage::swapchain_create)
            == std::string_view{"swapchain_create"},
        "native entrypoint stage name for swapchain create is stable");
    require(
        vulkan_backend::native_entrypoint_stage_name(
            vulkan_backend::vulkan_native_entrypoint_stage::swapchain_acquire)
            == std::string_view{"swapchain_acquire"},
        "native entrypoint stage name for swapchain acquire is stable");
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
            vulkan_backend::vulkan_native_function_table_status::required_extension_unavailable)
            == std::string_view{"required_extension_unavailable"},
        "native function table status name for missing extension is stable");
    require(
        vulkan_backend::native_function_table_status_name(
            vulkan_backend::vulkan_native_function_table_status::missing_swapchain_create_symbol)
            == std::string_view{"missing_swapchain_create_symbol"},
        "native function table status name for missing swapchain create is stable");
    require(
        vulkan_backend::native_function_table_status_name(
            vulkan_backend::vulkan_native_function_table_status::missing_swapchain_acquire_symbol)
            == std::string_view{"missing_swapchain_acquire_symbol"},
        "native function table status name for missing swapchain acquire is stable");
    require(
        vulkan_backend::native_function_table_status_name(
            vulkan_backend::vulkan_native_function_table_status::missing_queue_present_symbol)
            == std::string_view{"missing_queue_present_symbol"},
        "native function table status name for missing queue present is stable");
}

void test_system_native_symbol_resolver_reports_missing_loader_candidate()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::system_vulkan_native_symbol_resolver resolver(
        vulkan_backend::system_vulkan_native_symbol_resolver_options{
            .candidate_library_names = {
                "quiz_vulkan_missing_native_symbol_loader_20260515_97c31d0a.so",
            },
            .use_default_library_names = false,
        });
    const vulkan_backend::vulkan_system_symbol_resolution_result result =
        resolver.resolve_symbol_with_diagnostics("vkCreateInstance");

    require(result.checked, "system native symbol result records checked state");
    require(result.symbol_name == "vkCreateInstance", "system native symbol result records name");
    require(!result.resolved(), "missing loader candidate does not resolve a symbol");
    require(!result.pointer.valid(), "missing loader candidate has no resolved pointer");
    require(!result.loader_library_available, "missing loader candidate records no library");
    require(
        !result.instance_proc_address_available,
        "missing loader candidate records no vkGetInstanceProcAddr");
    require(
        !result.resolved_via_instance_proc_address,
        "missing loader candidate is not resolved through vkGetInstanceProcAddr");
    require(
        !result.resolved_via_direct_export,
        "missing loader candidate is not resolved through direct export");
    require(
        result.attempted_library_names.size() == 1,
        "missing loader candidate records one attempted library");
    require(
        result.attempted_library_names.front()
            == "quiz_vulkan_missing_native_symbol_loader_20260515_97c31d0a.so",
        "missing loader candidate records the attempted library name");
    require(!result.diagnostic.empty(), "missing loader candidate records a diagnostic");
}

void test_system_native_symbol_resolver_uses_loader_proc_address_when_available()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::system_vulkan_loader loader;
    const vulkan_backend::vulkan_loader_probe_result probe =
        vulkan_backend::probe_vulkan_loader(loader);

    require(probe.checked, "system Vulkan loader smoke probe is checked");
    if (!probe.available()) {
        return;
    }

    vulkan_backend::system_vulkan_native_symbol_resolver resolver(
        vulkan_backend::system_vulkan_native_symbol_resolver_options{
            .candidate_library_names = {probe.loaded_library_name},
            .use_default_library_names = false,
        });
    const vulkan_backend::vulkan_system_symbol_resolution_result result =
        resolver.resolve_symbol_with_diagnostics("vkCreateInstance");

    require(result.checked, "system native symbol smoke result records checked state");
    require(result.loader_library_available, "system native symbol smoke found loader library");
    require(
        result.loaded_library_name == probe.loaded_library_name,
        "system native symbol smoke uses the probed loader library");
    require(
        result.instance_proc_address_available,
        "system native symbol smoke sees vkGetInstanceProcAddr");
    require(result.resolved(), "system native symbol smoke resolves vkCreateInstance");
    require(result.pointer.valid(), "system native symbol smoke returns a valid function pointer");
    require(
        result.resolved_via_instance_proc_address,
        "system native symbol smoke resolves vkCreateInstance through vkGetInstanceProcAddr");
    require(
        !result.resolved_via_direct_export,
        "system native symbol smoke does not need direct export fallback");
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
    require(diagnostics.swapchain_create_ready, "native function table records swapchain create ready");
    require(diagnostics.swapchain_destroy_ready, "native function table records swapchain destroy ready");
    require(diagnostics.swapchain_images_ready, "native function table records swapchain images ready");
    require(diagnostics.swapchain_acquire_ready, "native function table records swapchain acquire ready");
    require(diagnostics.queue_present_ready, "native function table records queue present symbol ready");
    require(diagnostics.required_extensions_ready, "native function table has no default extension gate");
    require(diagnostics.required_extension_count == 0, "native function table does not force extension checks");
    require(diagnostics.requested_symbol_count == 16, "native function table requests default backend symbols");
    require(diagnostics.required_symbol_count == 16, "native function table counts required symbols");
    require(diagnostics.available_symbol_count == 16, "native function table counts available symbols");
    require(diagnostics.missing_required_symbol_count == 0, "native function table has no missing symbols");
    require(
        resolver.state().resolve_call_count == 16,
        "fake resolver is called once per default native symbol");
    require(
        diagnostics.symbols.front().name == "vkBeginCommandBuffer",
        "native function table preserves stable command recording symbol order");
    require(
        diagnostics.symbols[4].name == "vkCmdBindVertexBuffers",
        "native function table preserves stable vertex buffer bind symbol order");
    require(
        diagnostics.symbols[11].name == "vkCreateSwapchainKHR",
        "native function table preserves stable swapchain symbol order");
    require(
        diagnostics.symbols[14].name == "vkAcquireNextImageKHR",
        "native function table preserves stable acquire symbol order");
    require(
        diagnostics.symbols.back().name == "vkQueuePresentKHR",
        "native function table preserves stable queue present symbol order");
    const vulkan_backend::vulkan_native_swapchain_entrypoint_readiness swapchain =
        vulkan_backend::summarize_vulkan_native_swapchain_entrypoints(diagnostics);
    require(swapchain.ready_for_swapchain_path(), "swapchain native summary is ready");
    require(swapchain.ready_for_swapchain_create(), "swapchain create summary is ready");
    require(swapchain.ready_for_swapchain_acquire(), "swapchain acquire summary is ready");
    require(swapchain.ready_for_swapchain_present(), "swapchain present summary is ready");
    require(
        vulkan_backend::summarize_vulkan_swapchain_create_native_readiness(diagnostics)
            .entrypoint_ready,
        "swapchain create native entrypoint snapshot is ready");
    require(
        vulkan_backend::summarize_vulkan_swapchain_acquire_native_readiness(diagnostics)
            .entrypoint_ready,
        "swapchain acquire native entrypoint snapshot is ready");
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
    require(diagnostics.swapchain_create_ready, "swapchain create symbol remains ready");
    require(diagnostics.swapchain_acquire_ready, "swapchain acquire symbol remains ready");
    require(diagnostics.queue_present_ready, "queue present symbol remains ready");
}

void test_native_function_table_checks_required_swapchain_extension()
{
    using namespace quiz_vulkan::render;

    {
        vulkan_backend::fake_vulkan_native_symbol_resolver resolver;
        const vulkan_backend::vulkan_native_function_table_diagnostics diagnostics =
            vulkan_backend::collect_vulkan_native_function_table(
                resolver,
                make_ready_loader(),
                vulkan_backend::vulkan_native_function_table_request{
                    .available_extensions = {"VK_KHR_swapchain"},
                    .include_default_swapchain_extension = true,
                });

        require(diagnostics.ready_for_backend_path(), "available swapchain extension keeps native table ready");
        require(diagnostics.required_extensions_ready, "native table records required extension readiness");
        require(diagnostics.required_extension_count == 1, "native table counts required swapchain extension");
        require(
            diagnostics.available_required_extension_count == 1,
            "native table counts available swapchain extension");
    }

    {
        vulkan_backend::fake_vulkan_native_symbol_resolver resolver;
        const vulkan_backend::vulkan_native_function_table_diagnostics diagnostics =
            vulkan_backend::collect_vulkan_native_function_table(
                resolver,
                make_ready_loader(),
                vulkan_backend::vulkan_native_function_table_request{
                    .include_default_swapchain_extension = true,
                });

        require(diagnostics.blocked(), "missing swapchain extension blocks native table");
        require(
            diagnostics.status
                == vulkan_backend::vulkan_native_function_table_status::required_extension_unavailable,
            "native table reports unavailable required extension");
        require(
            diagnostics.fallback_reason
                == vulkan_backend::vulkan_backend_fallback_reason::swapchain_unavailable,
            "native table maps missing swapchain extension to swapchain fallback");
        require(
            diagnostics.missing_required_extension == "VK_KHR_swapchain",
            "native table records missing swapchain extension");
        require(
            resolver.state().resolve_call_count == 0,
            "native table does not resolve swapchain symbols when required extension is absent");

        const vulkan_backend::vulkan_native_swapchain_entrypoint_readiness swapchain =
            vulkan_backend::summarize_vulkan_native_swapchain_entrypoints(diagnostics);
        require(swapchain.blocked(), "swapchain native summary blocks on missing extension");
        require(
            swapchain.missing_required_extension == "VK_KHR_swapchain",
            "swapchain native summary records missing extension");
    }
}

void test_native_function_table_maps_missing_swapchain_symbols()
{
    using namespace quiz_vulkan::render;

    {
        vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
            vulkan_backend::fake_vulkan_native_symbol_resolver_options{
                .missing_symbols = {"vkCreateSwapchainKHR"},
            });
        const vulkan_backend::vulkan_native_function_table_diagnostics diagnostics =
            vulkan_backend::collect_vulkan_native_function_table(
                resolver,
                make_ready_loader());

        require(
            diagnostics.status
                == vulkan_backend::vulkan_native_function_table_status::missing_swapchain_create_symbol,
            "native function table maps missing swapchain create symbol");
        require(
            diagnostics.fallback_reason
                == vulkan_backend::vulkan_backend_fallback_reason::swapchain_unavailable,
            "native function table maps missing swapchain create to swapchain fallback");
        require(!diagnostics.swapchain_create_ready, "swapchain create symbol is not ready");
        const vulkan_backend::vulkan_native_swapchain_entrypoint_readiness swapchain =
            vulkan_backend::summarize_vulkan_native_swapchain_entrypoints(diagnostics);
        require(!swapchain.ready_for_swapchain_create(), "swapchain create summary is blocked");
        require(
            swapchain.missing_symbol_name == "vkCreateSwapchainKHR",
            "swapchain summary records missing create symbol");
    }

    {
        vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
            vulkan_backend::fake_vulkan_native_symbol_resolver_options{
                .missing_symbols = {"vkAcquireNextImageKHR"},
            });
        const vulkan_backend::vulkan_native_function_table_diagnostics diagnostics =
            vulkan_backend::collect_vulkan_native_function_table(
                resolver,
                make_ready_loader());

        require(
            diagnostics.status
                == vulkan_backend::vulkan_native_function_table_status::missing_swapchain_acquire_symbol,
            "native function table maps missing swapchain acquire symbol");
        require(
            diagnostics.fallback_reason
                == vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed,
            "native function table maps missing swapchain acquire to acquire fallback");
        require(!diagnostics.swapchain_acquire_ready, "swapchain acquire symbol is not ready");
        const vulkan_backend::vulkan_native_swapchain_entrypoint_readiness swapchain =
            vulkan_backend::summarize_vulkan_native_swapchain_entrypoints(diagnostics);
        require(
            swapchain.ready_for_swapchain_create(),
            "missing acquire symbol still leaves create summary ready");
        require(!swapchain.ready_for_swapchain_acquire(), "swapchain acquire summary is blocked");
        require(
            vulkan_backend::summarize_vulkan_swapchain_create_native_readiness(diagnostics)
                .entrypoint_ready,
            "missing acquire keeps create entrypoint snapshot ready");
        const vulkan_backend::vulkan_native_entrypoint_readiness_snapshot acquire_snapshot =
            vulkan_backend::summarize_vulkan_swapchain_acquire_native_readiness(diagnostics);
        require(!acquire_snapshot.entrypoint_ready, "missing acquire blocks acquire snapshot");
        require(
            acquire_snapshot.missing_symbol_name == "vkAcquireNextImageKHR",
            "acquire snapshot records missing acquire symbol");
    }
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
    require(diagnostics.requested_symbol_count == 17, "native function table includes optional symbol");
    require(diagnostics.available_symbol_count == 16, "native function table counts optional missing symbol");
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
    test_system_native_symbol_resolver_reports_missing_loader_candidate();
    test_system_native_symbol_resolver_uses_loader_proc_address_when_available();
    test_native_function_table_collects_default_backend_entrypoints();
    test_native_function_table_blocks_when_loader_is_unavailable();
    test_native_function_table_maps_missing_command_recording_symbol();
    test_native_function_table_checks_required_swapchain_extension();
    test_native_function_table_maps_missing_swapchain_symbols();
    test_native_function_table_maps_missing_queue_symbols();
    test_native_function_table_allows_missing_optional_custom_symbol();
    return 0;
}
