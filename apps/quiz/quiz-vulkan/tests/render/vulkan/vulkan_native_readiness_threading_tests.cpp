#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_native_symbols.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
make_native_functions(std::vector<std::string> missing_symbols = {})
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_symbol_resolver_options{
            .missing_symbols = std::move(missing_symbols),
        });
    return vulkan_backend::collect_vulkan_native_function_table(
        resolver,
        make_ready_loader());
}

quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_plan
make_ready_operation_plan()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_command_recorder_operation_summary operation{
        .kind = vulkan_backend::vulkan_command_recorder_operation_kind::draw_rect,
        .category = vulkan_backend::vulkan_command_packet_category::rect,
        .batch_kind = vulkan_backend::vulkan_batch_kind::quad,
        .operation_index = 0,
        .packet_index = 0,
        .command_index = 10,
        .node_id = "operation-0",
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 16.0f, 16.0f},
        .clipped_bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 16.0f, 16.0f},
        .scissor = vulkan_backend::vulkan_scissor_rect{.x = 0, .y = 0, .width = 16, .height = 16},
        .vertex_count = 4,
        .descriptor_set_count = 1,
        .binding_count = 1,
    };

    return vulkan_backend::vulkan_command_recorder_operation_plan{
        .checked = true,
        .status = vulkan_backend::vulkan_command_recorder_operation_plan_status::ready,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::none,
        .packet_bridge_checked = true,
        .packet_bridge_ready = true,
        .packet_execution_checked = true,
        .packet_execution_ready = true,
        .planned_packet_count = 1,
        .operation_count = 1,
        .rect_operation_count = 1,
        .operations = {operation},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_submit_sync_primitives make_sync()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_command_submit_sync_primitives{
        .image_available_semaphore =
            vulkan_backend::vulkan_command_submit_sync_handle{.value = 11},
        .render_finished_semaphore =
            vulkan_backend::vulkan_command_submit_sync_handle{.value = 12},
        .frame_fence = vulkan_backend::vulkan_command_submit_sync_handle{.value = 13},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_buffer_record_result
make_recorded_command_buffer(
    const quiz_vulkan::render::vulkan_backend::vulkan_native_function_table_diagnostics&
        native_functions)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_command_buffer_operation_recorder recorder;
    return vulkan_backend::record_vulkan_command_buffer_operations(
        recorder,
        vulkan_backend::vulkan_command_buffer_id{.value = 42},
        make_ready_operation_plan(),
        native_functions);
}

quiz_vulkan::render::vulkan_backend::vulkan_command_submit_readiness_result
make_ready_submit_readiness()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_command_submit_readiness_result{
        .checked = true,
        .status = vulkan_backend::vulkan_command_submit_readiness_status::ready,
        .command_buffer =
            vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 99},
        .submit_queue = vulkan_backend::vulkan_queue_handle{.value = 7},
        .sync_primitives = make_sync(),
        .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = 5},
        .planned_batch_count = 1,
        .submitted_batch_count = 1,
        .command_recording_available = true,
        .command_buffer_available = true,
        .sync_primitives_available = true,
        .submit_queue_available = true,
        .present_target_available = true,
        .submit_attempted = true,
        .failure_mode = vulkan_backend::vulkan_command_submit_failure_mode::none,
        .frame_result = vulkan_backend::vulkan_command_submit_frame_result{
            .checked = true,
            .status = vulkan_backend::vulkan_command_submit_frame_status::submitted,
            .submit_attempted = true,
            .submitted = true,
            .present_ready = true,
            .failure_mode = vulkan_backend::vulkan_command_submit_failure_mode::none,
            .diagnostic = "ready",
        },
        .diagnostic = "ready",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_submit_batch_plan_result make_ready_submit_batch(
    const quiz_vulkan::render::vulkan_backend::vulkan_native_function_table_diagnostics&
        native_functions)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::build_vulkan_submit_batch_plan(
        make_recorded_command_buffer(native_functions),
        make_ready_submit_readiness(),
        native_functions);
}

void test_native_readiness_status_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::command_buffer_record_result_status_name(
            vulkan_backend::vulkan_command_buffer_record_result_status::native_entrypoint_unavailable)
            == std::string_view{"native_entrypoint_unavailable"},
        "command buffer native entrypoint status name is stable");
    require(
        vulkan_backend::submit_batch_plan_status_name(
            vulkan_backend::vulkan_submit_batch_plan_status::native_queue_submit_unavailable)
            == std::string_view{"native_queue_submit_unavailable"},
        "submit batch native queue submit status name is stable");
    require(
        vulkan_backend::present_completion_plan_status_name(
            vulkan_backend::vulkan_present_completion_plan_status::native_queue_present_unavailable)
            == std::string_view{"native_queue_present_unavailable"},
        "present completion native queue present status name is stable");
}

void test_native_command_recording_symbols_gate_fake_recorder()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_function_table_diagnostics native_functions =
        make_native_functions({"vkCmdDraw"});
    vulkan_backend::fake_vulkan_command_buffer_operation_recorder recorder;
    const vulkan_backend::vulkan_command_buffer_record_result result =
        vulkan_backend::record_vulkan_command_buffer_operations(
            recorder,
            vulkan_backend::vulkan_command_buffer_id{.value = 42},
            make_ready_operation_plan(),
            native_functions);

    require(result.checked, "native-gated command buffer recording is checked");
    require(result.failed(), "native-gated command buffer recording fails");
    require(
        result.status
            == vulkan_backend::vulkan_command_buffer_record_result_status::native_entrypoint_unavailable,
        "command buffer recording reports native entrypoint unavailable");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "command buffer recording maps missing native symbol to recording fallback");
    require(result.native_function_table_checked, "command buffer recording records checked native table");
    require(!result.native_command_buffer_recording_ready, "command recording native stage is not ready");
    require(
        result.native_function_table_status
            == vulkan_backend::vulkan_native_function_table_status::missing_command_buffer_recording_symbol,
        "command buffer recording stores native table status");
    require(result.missing_native_symbol_name == "vkCmdDraw", "command buffer recording stores missing symbol");
    require(
        !recorder.record_result().checked,
        "native command buffer gate stops before fake recorder execution");
}

void test_native_queue_submit_symbols_gate_submit_batch_planning()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_function_table_diagnostics ready_native =
        make_native_functions();
    const vulkan_backend::vulkan_native_function_table_diagnostics missing_submit =
        make_native_functions({"vkQueueSubmit"});
    const vulkan_backend::vulkan_submit_batch_plan_result plan =
        vulkan_backend::build_vulkan_submit_batch_plan(
            make_recorded_command_buffer(ready_native),
            make_ready_submit_readiness(),
            missing_submit);

    require(plan.checked, "native-gated submit batch plan is checked");
    require(plan.blocked(), "native-gated submit batch plan blocks");
    require(
        plan.status
            == vulkan_backend::vulkan_submit_batch_plan_status::native_queue_submit_unavailable,
        "submit batch planning reports native queue submit unavailable");
    require(
        plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed,
        "submit batch planning maps missing queue submit to submit fallback");
    require(plan.native_function_table_checked, "submit batch planning records checked native table");
    require(!plan.native_queue_submit_ready, "submit native stage is not ready");
    require(plan.missing_native_symbol_name == "vkQueueSubmit", "submit plan stores missing native symbol");
    require(plan.submit_batches.empty(), "native submit gate stops before submit batch records");
    require(!plan.submit_ready, "native submit gate does not mark submit ready");
}

void test_native_queue_present_symbols_gate_present_completion_planning()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_function_table_diagnostics ready_native =
        make_native_functions();
    const vulkan_backend::vulkan_native_function_table_diagnostics missing_present =
        make_native_functions({"vkQueuePresentKHR"});
    const vulkan_backend::vulkan_present_completion_plan_result plan =
        vulkan_backend::build_vulkan_present_completion_plan(
            make_ready_submit_batch(ready_native),
            vulkan_backend::vulkan_queue_submit_present_result{},
            missing_present);

    require(plan.checked, "native-gated present completion plan is checked");
    require(plan.blocked(), "native-gated present completion plan blocks");
    require(
        plan.status
            == vulkan_backend::vulkan_present_completion_plan_status::native_queue_present_unavailable,
        "present completion planning reports native queue present unavailable");
    require(
        plan.frame_status == vulkan_backend::vulkan_frame_completion_status::present_unavailable,
        "present completion planning maps missing native present to present-unavailable frame");
    require(
        plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed,
        "present completion planning maps missing native present to present fallback");
    require(plan.native_function_table_checked, "present completion records checked native table");
    require(!plan.native_queue_present_ready, "present native stage is not ready");
    require(
        plan.missing_native_symbol_name == "vkQueuePresentKHR",
        "present completion stores missing native symbol");
    require(!plan.present_request_ready, "native present gate stops before present request readiness");
    require(!plan.present_result_ready, "native present gate stops before present result readiness");
}

void test_ready_native_symbols_preserve_fake_planning_paths()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_native_function_table_diagnostics native_functions =
        make_native_functions();
    vulkan_backend::fake_vulkan_command_buffer_operation_recorder recorder;
    const vulkan_backend::vulkan_command_buffer_record_result recording =
        vulkan_backend::record_vulkan_command_buffer_operations(
            recorder,
            vulkan_backend::vulkan_command_buffer_id{.value = 42},
            make_ready_operation_plan(),
            native_functions);
    require(recording.completed(), "ready native symbols allow fake command recording path");
    require(recording.native_function_table_checked, "ready recording stores checked native table");
    require(recording.native_command_buffer_recording_ready, "ready recording stores native command stage");
    require(recorder.record_result().checked, "ready native symbols call fake recorder");

    const vulkan_backend::vulkan_submit_batch_plan_result submit_plan =
        vulkan_backend::build_vulkan_submit_batch_plan(
            recording,
            make_ready_submit_readiness(),
            native_functions);
    require(submit_plan.completed(), "ready native symbols allow submit batch planning");
    require(submit_plan.native_function_table_checked, "submit plan stores checked native table");
    require(submit_plan.native_queue_submit_ready, "submit plan stores ready native queue submit");

    const vulkan_backend::vulkan_present_completion_plan_result present_plan =
        vulkan_backend::build_vulkan_present_completion_plan(
            submit_plan,
            vulkan_backend::vulkan_queue_submit_present_result{},
            native_functions);
    require(present_plan.completed(), "ready native symbols allow present completion planning");
    require(present_plan.native_function_table_checked, "present plan stores checked native table");
    require(present_plan.native_queue_present_ready, "present plan stores ready native queue present");
}

} // namespace

int main()
{
    test_native_readiness_status_names_are_stable();
    test_native_command_recording_symbols_gate_fake_recorder();
    test_native_queue_submit_symbols_gate_submit_batch_planning();
    test_native_queue_present_symbols_gate_present_completion_planning();
    test_ready_native_symbols_preserve_fake_planning_paths();
    return 0;
}
