#include "render/vulkan/vulkan_backend_adapter.h"

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

quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_summary
make_operation(
    quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_kind kind,
    quiz_vulkan::render::vulkan_backend::vulkan_command_packet_category category,
    quiz_vulkan::render::vulkan_backend::vulkan_batch_kind batch_kind,
    std::size_t operation_index,
    std::size_t command_index)
{
    return quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_summary{
        .kind = kind,
        .category = category,
        .batch_kind = batch_kind,
        .operation_index = operation_index,
        .packet_index = operation_index,
        .command_index = command_index,
        .node_id = "operation-" + std::to_string(operation_index),
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 16.0f, 16.0f},
        .clipped_bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 16.0f, 16.0f},
        .scissor = quiz_vulkan::render::vulkan_backend::vulkan_scissor_rect{
            .x = 0,
            .y = 0,
            .width = 16,
            .height = 16,
        },
        .vertex_count = 4,
        .descriptor_set_count = 1,
        .binding_count = 1,
    };
}

void append_operation(
    quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_plan& plan,
    quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_summary operation)
{
    using namespace quiz_vulkan::render;

    switch (operation.category) {
    case vulkan_backend::vulkan_command_packet_category::rect:
        ++plan.rect_operation_count;
        break;
    case vulkan_backend::vulkan_command_packet_category::text:
        ++plan.text_operation_count;
        break;
    case vulkan_backend::vulkan_command_packet_category::image:
        ++plan.image_operation_count;
        break;
    case vulkan_backend::vulkan_command_packet_category::debug_bounds:
        ++plan.debug_bounds_operation_count;
        break;
    }

    plan.operations.push_back(std::move(operation));
    plan.operation_count = plan.operations.size();
    plan.planned_packet_count = plan.operations.size();
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

quiz_vulkan::render::vulkan_backend::vulkan_surface_extent make_extent()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_surface_extent{
        .width = 1280,
        .height = 720,
    };
}

std::vector<quiz_vulkan::render::vulkan_backend::vulkan_render_pass_attachment_request>
make_color_attachments()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return {
        vulkan_backend::vulkan_render_pass_attachment_request{
            .role = vulkan_backend::vulkan_render_pass_attachment_role::color,
            .format = vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
        },
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_render_pass_create_result make_ready_render_pass()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_render_pass_create_result{
        .checked = true,
        .status = vulkan_backend::vulkan_render_pass_create_status::created,
        .render_pass = vulkan_backend::vulkan_render_pass_handle{.value = 300},
        .framebuffer = vulkan_backend::vulkan_framebuffer_handle{.value = 400},
        .requested_extent = make_extent(),
        .selected_extent = make_extent(),
        .requested_attachments = make_color_attachments(),
        .selected_attachments = make_color_attachments(),
        .diagnostic = "Vulkan render pass and framebuffer created",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_recording_readiness_result
make_ready_command_recording()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_command_recording_readiness_result{
        .checked = true,
        .status = vulkan_backend::vulkan_command_recording_readiness_status::ready,
        .render_pass = make_ready_render_pass(),
        .command_buffer =
            vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 909},
        .planned_batch_count = 0,
        .recordable_batch_count = 0,
        .empty_draw_plan = true,
        .render_pass_available = true,
        .framebuffer_available = true,
        .pipeline_compatible = true,
        .command_buffer_available = true,
        .attachment_compatibility = {
            vulkan_backend::vulkan_command_recording_pipeline_attachment_compatibility{
                .role = vulkan_backend::vulkan_render_pass_attachment_role::color,
                .format = vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm,
                .compatible = true,
            },
        },
        .diagnostic = "Vulkan command recording is ready for an empty draw plan",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_function_table_diagnostics
make_render_pass_scope_function_table(const std::vector<std::string>& missing_symbols = {})
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_symbol_resolver_options{
            .missing_symbols = missing_symbols,
            .pointer_base = vulkan_backend::vulkan_native_function_pointer{.value = 1400},
        });
    return vulkan_backend::collect_vulkan_native_function_table(
        resolver,
        make_ready_loader(),
        vulkan_backend::vulkan_native_function_table_request{
            .symbols = vulkan_backend::default_vulkan_native_render_pass_scope_entrypoints(),
            .include_default_backend_entrypoints = false,
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_native_render_pass_scope_dispatch_table
make_ready_render_pass_scope_dispatch_table()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::collect_vulkan_native_render_pass_scope_dispatch_table(
        make_render_pass_scope_function_table());
}

quiz_vulkan::render::vulkan_backend::vulkan_native_framebuffer_target
make_ready_framebuffer_target(std::size_t target_index)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_framebuffer_target{
        .image_index = target_index,
        .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = target_index + 1},
        .image_view =
            vulkan_backend::vulkan_swapchain_image_view_handle{.value = 8000 + target_index + 1},
        .render_pass = vulkan_backend::vulkan_render_pass_handle{.value = 300},
        .framebuffer = vulkan_backend::vulkan_framebuffer_handle{.value = 13000 + target_index + 1},
        .extent = make_extent(),
        .layers = 1,
        .attachments = make_color_attachments(),
        .lifecycle_status =
            vulkan_backend::vulkan_native_framebuffer_target_lifecycle_status::ready,
        .render_pass_ready = true,
        .image_view_ready = true,
        .extent_ready = true,
        .attachment_ready = true,
        .framebuffer_ready = true,
        .vk_create_framebuffer_called = true,
        .native_result = 0,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_framebuffer_targets_execution_result
make_ready_framebuffer_targets()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_framebuffer_targets_execution_result{
        .checked = true,
        .status = vulkan_backend::vulkan_native_framebuffer_targets_execution_status::ready,
        .device = vulkan_backend::vulkan_device_handle{.value = 80},
        .render_pass_handle = vulkan_backend::vulkan_render_pass_handle{.value = 300},
        .extent = make_extent(),
        .layers = 1,
        .attachments = make_color_attachments(),
        .target_count = 2,
        .ready_framebuffer_count = 2,
        .targets = {
            make_ready_framebuffer_target(0),
            make_ready_framebuffer_target(1),
        },
        .create_framebuffer_symbol = vulkan_backend::vulkan_native_function_pointer{.value = 981},
        .destroy_framebuffer_symbol = vulkan_backend::vulkan_native_function_pointer{.value = 982},
        .image_view_targets_ready = true,
        .render_pass_ready = true,
        .dispatch_table_ready = true,
        .extent_matches = true,
        .vk_create_framebuffer_called = true,
        .diagnostic = "Native Vulkan framebuffer targets are ready",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_plan make_ready_plan()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_command_recorder_operation_plan plan{
        .checked = true,
        .status = vulkan_backend::vulkan_command_recorder_operation_plan_status::ready,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::none,
        .packet_bridge_checked = true,
        .packet_bridge_ready = true,
        .packet_execution_checked = true,
        .packet_execution_ready = true,
    };

    append_operation(
        plan,
        make_operation(
            vulkan_backend::vulkan_command_recorder_operation_kind::draw_rect,
            vulkan_backend::vulkan_command_packet_category::rect,
            vulkan_backend::vulkan_batch_kind::quad,
            0,
            10));
    append_operation(
        plan,
        make_operation(
            vulkan_backend::vulkan_command_recorder_operation_kind::draw_text,
            vulkan_backend::vulkan_command_packet_category::text,
            vulkan_backend::vulkan_batch_kind::text,
            1,
            11));
    append_operation(
        plan,
        make_operation(
            vulkan_backend::vulkan_command_recorder_operation_kind::draw_image,
            vulkan_backend::vulkan_command_packet_category::image,
            vulkan_backend::vulkan_batch_kind::image,
            2,
            12));
    append_operation(
        plan,
        make_operation(
            vulkan_backend::vulkan_command_recorder_operation_kind::draw_debug_bounds,
            vulkan_backend::vulkan_command_packet_category::debug_bounds,
            vulkan_backend::vulkan_batch_kind::debug_bounds,
            3,
            13));
    return plan;
}

quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_plan make_empty_plan()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_plan{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_plan_status::ready,
        .fallback_reason = quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason::none,
        .packet_bridge_checked = true,
        .packet_bridge_ready = true,
        .packet_execution_checked = true,
        .packet_execution_ready = true,
    };
}

void test_vulkan_command_buffer_recording_result_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::command_buffer_record_result_status_name(
            vulkan_backend::vulkan_command_buffer_record_result_status::not_checked)
            == std::string_view{"not_checked"},
        "record result status name for not checked is stable");
    require(
        vulkan_backend::command_buffer_record_result_status_name(
            vulkan_backend::vulkan_command_buffer_record_result_status::recorded)
            == std::string_view{"recorded"},
        "record result status name for recorded is stable");
    require(
        vulkan_backend::command_buffer_record_result_status_name(
            vulkan_backend::vulkan_command_buffer_record_result_status::operation_plan_unavailable)
            == std::string_view{"operation_plan_unavailable"},
        "record result status name for unavailable operation plan is stable");
    require(
        vulkan_backend::command_buffer_record_result_status_name(
            vulkan_backend::vulkan_command_buffer_record_result_status::command_buffer_unavailable)
            == std::string_view{"command_buffer_unavailable"},
        "record result status name for unavailable command buffer is stable");
    require(
        vulkan_backend::command_buffer_record_result_status_name(
            vulkan_backend::vulkan_command_buffer_record_result_status::begin_failed)
            == std::string_view{"begin_failed"},
        "record result status name for begin failure is stable");
    require(
        vulkan_backend::command_buffer_record_result_status_name(
            vulkan_backend::vulkan_command_buffer_record_result_status::operation_failed)
            == std::string_view{"operation_failed"},
        "record result status name for operation failure is stable");
    require(
        vulkan_backend::command_buffer_record_result_status_name(
            vulkan_backend::vulkan_command_buffer_record_result_status::end_failed)
            == std::string_view{"end_failed"},
        "record result status name for end failure is stable");
    require(
        vulkan_backend::command_buffer_record_event_kind_name(
            vulkan_backend::vulkan_command_buffer_record_event_kind::begin)
            == std::string_view{"begin"},
        "record event kind name for begin is stable");
    require(
        vulkan_backend::command_buffer_record_event_kind_name(
            vulkan_backend::vulkan_command_buffer_record_event_kind::operation)
            == std::string_view{"operation"},
        "record event kind name for operation is stable");
    require(
        vulkan_backend::command_buffer_record_event_kind_name(
            vulkan_backend::vulkan_command_buffer_record_event_kind::end)
            == std::string_view{"end"},
        "record event kind name for end is stable");
    require(
        vulkan_backend::native_render_pass_scope_dispatch_table_status_name(
            vulkan_backend::vulkan_native_render_pass_scope_dispatch_table_status::missing_begin_render_pass_symbol)
            == std::string_view{"missing_begin_render_pass_symbol"},
        "render pass scope dispatch status name for missing begin is stable");
    require(
        vulkan_backend::native_render_pass_scope_record_status_name(
            vulkan_backend::vulkan_native_render_pass_scope_record_status::recorded)
            == std::string_view{"recorded"},
        "render pass scope record status name for recorded is stable");
    require(
        vulkan_backend::native_render_pass_scope_record_status_name(
            vulkan_backend::vulkan_native_render_pass_scope_record_status::extent_mismatch)
            == std::string_view{"extent_mismatch"},
        "render pass scope record status name for extent mismatch is stable");
}

void test_native_render_pass_scope_dispatch_table_resolves_begin_end_symbols()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_native_render_pass_scope_dispatch_table dispatch =
        make_ready_render_pass_scope_dispatch_table();
    require(dispatch.checked, "render pass scope dispatch table is checked");
    require(
        dispatch.status
            == vulkan_backend::vulkan_native_render_pass_scope_dispatch_table_status::ready,
        "render pass scope dispatch table reports ready");
    require(
        dispatch.ready_for_render_pass_scope(),
        "render pass scope dispatch table reaches begin/end gate");
    require(dispatch.begin_render_pass.value == 1401, "dispatch records begin render pass pointer");
    require(dispatch.end_render_pass.value == 1402, "dispatch records end render pass pointer");

    const vulkan_backend::vulkan_native_render_pass_scope_dispatch_table missing_begin =
        vulkan_backend::collect_vulkan_native_render_pass_scope_dispatch_table(
            make_render_pass_scope_function_table({"vkCmdBeginRenderPass"}));
    require(
        missing_begin.status
            == vulkan_backend::vulkan_native_render_pass_scope_dispatch_table_status::missing_begin_render_pass_symbol,
        "render pass scope dispatch reports missing begin symbol");
    require(
        missing_begin.missing_symbol_name == "vkCmdBeginRenderPass",
        "render pass scope dispatch records missing begin symbol");

    const vulkan_backend::vulkan_native_render_pass_scope_dispatch_table missing_end =
        vulkan_backend::collect_vulkan_native_render_pass_scope_dispatch_table(
            make_render_pass_scope_function_table({"vkCmdEndRenderPass"}));
    require(
        missing_end.status
            == vulkan_backend::vulkan_native_render_pass_scope_dispatch_table_status::missing_end_render_pass_symbol,
        "render pass scope dispatch reports missing end symbol");
    require(
        missing_end.missing_symbol_name == "vkCmdEndRenderPass",
        "render pass scope dispatch records missing end symbol");
}

void test_native_render_pass_scope_records_selected_framebuffer_target()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_native_render_pass_scope_recorder recorder;
    const vulkan_backend::vulkan_native_render_pass_scope_record_result result =
        vulkan_backend::record_native_vulkan_render_pass_scope(
            recorder,
            vulkan_backend::vulkan_native_render_pass_scope_record_request{
                .framebuffer_targets = make_ready_framebuffer_targets(),
                .command_recording = make_ready_command_recording(),
                .dispatch_table = make_ready_render_pass_scope_dispatch_table(),
                .framebuffer_target_index = 1,
            });

    require(result.checked, "render pass scope result is checked");
    require(
        result.status == vulkan_backend::vulkan_native_render_pass_scope_record_status::recorded,
        "render pass scope records successfully");
    require(result.ready_for_draw_commands(), "render pass scope reaches draw command gate");
    require(!result.blocked(), "ready render pass scope is not blocked");
    require(result.selected_framebuffer_target_index == 1, "render pass scope records selected index");
    require(result.framebuffer_target_count == 2, "render pass scope records target count");
    require(result.image_id.value == 2, "render pass scope records selected image id");
    require(result.command_buffer.value == 909, "render pass scope records command buffer handle");
    require(result.render_pass.value == 300, "render pass scope records render pass handle");
    require(result.framebuffer.value == 13002, "render pass scope records selected framebuffer");
    require(result.extent.width == 1280, "render pass scope records framebuffer width");
    require(result.extent.height == 720, "render pass scope records framebuffer height");
    require(result.begin_render_pass_symbol.value == 1401, "render pass scope records begin symbol");
    require(result.end_render_pass_symbol.value == 1402, "render pass scope records end symbol");
    require(result.framebuffer_targets_ready, "render pass scope records framebuffer target readiness");
    require(result.command_buffer_ready, "render pass scope records command buffer readiness");
    require(result.render_pass_ready, "render pass scope records render pass readiness");
    require(result.framebuffer_ready, "render pass scope records framebuffer readiness");
    require(result.extent_matches, "render pass scope records matching extent");
    require(result.dispatch_table_ready, "render pass scope records dispatch readiness");
    require(result.vk_cmd_begin_render_pass_called, "render pass scope records begin call");
    require(result.vk_cmd_end_render_pass_called, "render pass scope records end call");
    require(
        recorder.state().record_call_count == 1,
        "fake render pass scope recorder records one call");
    require(
        recorder.state().requested_framebuffer_target_index == 1,
        "fake render pass scope recorder stores requested target index");
    require(
        recorder.state().requested_framebuffer.value == 13002,
        "fake render pass scope recorder stores selected framebuffer");
    require(
        recorder.state().last_begin_render_pass_symbol.value == 1401,
        "fake render pass scope recorder stores begin symbol");
    require(
        recorder.state().last_result.ready_for_draw_commands(),
        "fake render pass scope recorder retains ready result");
}

void test_native_render_pass_scope_reports_blockers_and_failures()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_native_render_pass_scope_recorder recorder;
    const vulkan_backend::vulkan_native_render_pass_scope_dispatch_table dispatch =
        make_ready_render_pass_scope_dispatch_table();
    const vulkan_backend::vulkan_command_recording_readiness_result command_recording =
        make_ready_command_recording();
    const vulkan_backend::vulkan_native_framebuffer_targets_execution_result framebuffers =
        make_ready_framebuffer_targets();

    const vulkan_backend::vulkan_native_render_pass_scope_record_result missing_targets =
        vulkan_backend::record_native_vulkan_render_pass_scope(
            recorder,
            vulkan_backend::vulkan_native_render_pass_scope_record_request{
                .framebuffer_targets = {},
                .command_recording = command_recording,
                .dispatch_table = dispatch,
            });
    require(
        missing_targets.status
            == vulkan_backend::vulkan_native_render_pass_scope_record_status::framebuffer_targets_unavailable,
        "render pass scope blocks without framebuffer target stage");

    const vulkan_backend::vulkan_native_render_pass_scope_record_result bad_index =
        vulkan_backend::record_native_vulkan_render_pass_scope(
            recorder,
            vulkan_backend::vulkan_native_render_pass_scope_record_request{
                .framebuffer_targets = framebuffers,
                .command_recording = command_recording,
                .dispatch_table = dispatch,
                .framebuffer_target_index = 4,
            });
    require(
        bad_index.status
            == vulkan_backend::vulkan_native_render_pass_scope_record_status::target_index_unavailable,
        "render pass scope blocks unavailable framebuffer target index");

    vulkan_backend::vulkan_command_recording_readiness_result missing_command_buffer =
        command_recording;
    missing_command_buffer.command_buffer = {};
    missing_command_buffer.command_buffer_available = false;
    const vulkan_backend::vulkan_native_render_pass_scope_record_result missing_command_buffer_result =
        vulkan_backend::record_native_vulkan_render_pass_scope(
            recorder,
            vulkan_backend::vulkan_native_render_pass_scope_record_request{
                .framebuffer_targets = framebuffers,
                .command_recording = missing_command_buffer,
                .dispatch_table = dispatch,
            });
    require(
        missing_command_buffer_result.status
            == vulkan_backend::vulkan_native_render_pass_scope_record_status::command_buffer_unavailable,
        "render pass scope blocks missing command buffer");

    vulkan_backend::vulkan_native_framebuffer_targets_execution_result missing_render_pass =
        framebuffers;
    missing_render_pass.targets[0].render_pass = {};
    missing_render_pass.targets[0].render_pass_ready = false;
    const vulkan_backend::vulkan_native_render_pass_scope_record_result missing_render_pass_result =
        vulkan_backend::record_native_vulkan_render_pass_scope(
            recorder,
            vulkan_backend::vulkan_native_render_pass_scope_record_request{
                .framebuffer_targets = missing_render_pass,
                .command_recording = command_recording,
                .dispatch_table = dispatch,
            });
    require(
        missing_render_pass_result.status
            == vulkan_backend::vulkan_native_render_pass_scope_record_status::missing_render_pass,
        "render pass scope blocks missing render pass handle");

    vulkan_backend::vulkan_native_framebuffer_targets_execution_result missing_framebuffer =
        framebuffers;
    missing_framebuffer.targets[0].framebuffer = {};
    missing_framebuffer.targets[0].framebuffer_ready = false;
    const vulkan_backend::vulkan_native_render_pass_scope_record_result missing_framebuffer_result =
        vulkan_backend::record_native_vulkan_render_pass_scope(
            recorder,
            vulkan_backend::vulkan_native_render_pass_scope_record_request{
                .framebuffer_targets = missing_framebuffer,
                .command_recording = command_recording,
                .dispatch_table = dispatch,
            });
    require(
        missing_framebuffer_result.status
            == vulkan_backend::vulkan_native_render_pass_scope_record_status::missing_framebuffer,
        "render pass scope blocks missing framebuffer handle");

    vulkan_backend::vulkan_native_framebuffer_targets_execution_result mismatched_extent =
        framebuffers;
    mismatched_extent.targets[0].extent =
        vulkan_backend::vulkan_surface_extent{.width = 640, .height = 480};
    const vulkan_backend::vulkan_native_render_pass_scope_record_result extent_mismatch =
        vulkan_backend::record_native_vulkan_render_pass_scope(
            recorder,
            vulkan_backend::vulkan_native_render_pass_scope_record_request{
                .framebuffer_targets = mismatched_extent,
                .command_recording = command_recording,
                .dispatch_table = dispatch,
            });
    require(
        extent_mismatch.status
            == vulkan_backend::vulkan_native_render_pass_scope_record_status::extent_mismatch,
        "render pass scope blocks extent mismatch");

    const vulkan_backend::vulkan_native_render_pass_scope_dispatch_table missing_dispatch =
        vulkan_backend::collect_vulkan_native_render_pass_scope_dispatch_table(
            make_render_pass_scope_function_table({"vkCmdBeginRenderPass"}));
    const vulkan_backend::vulkan_native_render_pass_scope_record_result dispatch_blocked =
        vulkan_backend::record_native_vulkan_render_pass_scope(
            recorder,
            vulkan_backend::vulkan_native_render_pass_scope_record_request{
                .framebuffer_targets = framebuffers,
                .command_recording = command_recording,
                .dispatch_table = missing_dispatch,
            });
    require(
        dispatch_blocked.status
            == vulkan_backend::vulkan_native_render_pass_scope_record_status::dispatch_table_unavailable,
        "render pass scope blocks missing dispatch symbols");

    vulkan_backend::fake_vulkan_native_render_pass_scope_recorder failing_begin(
        vulkan_backend::fake_vulkan_native_render_pass_scope_recorder_options{
            .fail_begin = true,
            .begin_failure_result = -31,
        });
    const vulkan_backend::vulkan_native_render_pass_scope_record_result begin_failed =
        vulkan_backend::record_native_vulkan_render_pass_scope(
            failing_begin,
            vulkan_backend::vulkan_native_render_pass_scope_record_request{
                .framebuffer_targets = framebuffers,
                .command_recording = command_recording,
                .dispatch_table = dispatch,
            });
    require(
        begin_failed.status == vulkan_backend::vulkan_native_render_pass_scope_record_status::begin_failed,
        "render pass scope reports begin failure");
    require(begin_failed.native_result == -31, "render pass scope records begin failure code");

    vulkan_backend::fake_vulkan_native_render_pass_scope_recorder failing_end(
        vulkan_backend::fake_vulkan_native_render_pass_scope_recorder_options{
            .fail_end = true,
            .end_failure_result = -32,
        });
    const vulkan_backend::vulkan_native_render_pass_scope_record_result end_failed =
        vulkan_backend::record_native_vulkan_render_pass_scope(
            failing_end,
            vulkan_backend::vulkan_native_render_pass_scope_record_request{
                .framebuffer_targets = framebuffers,
                .command_recording = command_recording,
                .dispatch_table = dispatch,
            });
    require(
        end_failed.status == vulkan_backend::vulkan_native_render_pass_scope_record_status::end_failed,
        "render pass scope reports end failure");
    require(end_failed.native_result == -32, "render pass scope records end failure code");
}

void test_vulkan_command_buffer_recording_result_records_successful_operations()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_recorder_operation_plan plan = make_ready_plan();
    vulkan_backend::fake_vulkan_command_buffer_operation_recorder recorder;
    const vulkan_backend::vulkan_command_buffer_record_result result =
        recorder.record_operations(vulkan_backend::vulkan_command_buffer_id{.value = 77}, plan);

    require(result.checked, "record result is checked");
    require(result.completed(), "record result completes");
    require(!result.failed(), "record result does not fail");
    require(
        result.status == vulkan_backend::vulkan_command_buffer_record_result_status::recorded,
        "record result status is recorded");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "record result has no fallback");
    require(result.command_buffer.value == 77, "record result preserves command buffer id");
    require(result.operation_plan_checked, "record result checks operation plan");
    require(result.operation_plan_ready, "record result records ready operation plan");
    require(result.begin_attempted, "record result attempts begin");
    require(result.begin_completed, "record result completes begin");
    require(result.end_attempted, "record result attempts end");
    require(result.end_completed, "record result completes end");
    require(!result.has_failed_operation, "record result has no failed operation");
    require(result.planned_operation_count == 4, "record result records planned count");
    require(result.attempted_operation_count == 4, "record result records attempted operations");
    require(result.recorded_operation_count == 4, "record result records completed operations");
    require(result.rect_operation_count == 1, "record result counts rect operation");
    require(result.text_operation_count == 1, "record result counts text operation");
    require(result.image_operation_count == 1, "record result counts image operation");
    require(result.debug_bounds_operation_count == 1, "record result counts debug bounds operation");
    require(result.events.size() == 6, "record result stores begin, operation, and end events");
    require(
        result.events[0].event == vulkan_backend::vulkan_command_buffer_record_event_kind::begin,
        "record result first event begins command buffer");
    require(
        result.events[1].event == vulkan_backend::vulkan_command_buffer_record_event_kind::operation,
        "record result second event records operation");
    require(
        result.events[1].operation_kind
            == vulkan_backend::vulkan_command_recorder_operation_kind::draw_rect,
        "record result records rect operation kind");
    require(result.events[3].operation_index == 2, "record result preserves operation index");
    require(result.events[3].packet_index == 2, "record result preserves packet index");
    require(result.events[3].command_index == 12, "record result preserves command index");
    require(
        result.events[3].category == vulkan_backend::vulkan_command_packet_category::image,
        "record result preserves image operation category");
    require(
        result.events[5].event == vulkan_backend::vulkan_command_buffer_record_event_kind::end,
        "record result final event ends command buffer");
    for (const vulkan_backend::vulkan_command_buffer_record_event& event : result.events) {
        require(event.successful(), "successful record event reports success");
    }
    require(recorder.record_result().completed(), "fake recorder retains completed result");
}

void test_vulkan_command_buffer_recording_result_blocks_without_operation_plan()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_recorder_operation_plan plan{
        .checked = true,
        .status =
            vulkan_backend::vulkan_command_recorder_operation_plan_status::packet_bridge_unavailable,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        .blocked_category = vulkan_backend::vulkan_command_packet_category::image,
        .blocked_batch_kind = vulkan_backend::vulkan_batch_kind::image,
        .blocked_packet_index = 2,
        .blocked_command_index = 12,
    };
    vulkan_backend::fake_vulkan_command_buffer_operation_recorder recorder;
    const vulkan_backend::vulkan_command_buffer_record_result result =
        recorder.record_operations(vulkan_backend::vulkan_command_buffer_id{.value = 77}, plan);

    require(result.checked, "blocked record result is checked");
    require(!result.completed(), "blocked record result does not complete");
    require(result.failed(), "blocked record result reports failure");
    require(
        result.status
            == vulkan_backend::vulkan_command_buffer_record_result_status::operation_plan_unavailable,
        "record result records unavailable operation plan");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        "record result preserves operation plan fallback");
    require(result.operation_plan_checked, "record result records checked operation plan");
    require(!result.operation_plan_ready, "record result records unavailable operation plan");
    require(
        result.first_failed_category == vulkan_backend::vulkan_command_packet_category::image,
        "record result preserves blocked category");
    require(result.first_failed_packet_index == 2, "record result preserves blocked packet index");
    require(result.first_failed_command_index == 12, "record result preserves blocked command index");
    require(result.events.empty(), "blocked record result stores no events");
}

void test_vulkan_command_buffer_recording_result_blocks_without_command_buffer()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_recorder_operation_plan plan = make_ready_plan();
    vulkan_backend::fake_vulkan_command_buffer_operation_recorder recorder;
    const vulkan_backend::vulkan_command_buffer_record_result result =
        recorder.record_operations(vulkan_backend::vulkan_command_buffer_id{}, plan);

    require(result.checked, "missing-buffer record result is checked");
    require(!result.completed(), "missing-buffer record result does not complete");
    require(result.failed(), "missing-buffer record result reports failure");
    require(
        result.status
            == vulkan_backend::vulkan_command_buffer_record_result_status::command_buffer_unavailable,
        "record result records unavailable command buffer");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "record result maps missing command buffer to record fallback");
    require(result.operation_plan_ready, "record result verifies operation plan before command buffer failure");
    require(result.events.empty(), "missing-buffer record result stores no events");
}

void test_vulkan_command_buffer_recording_result_records_first_operation_failure()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_recorder_operation_plan plan = make_ready_plan();
    vulkan_backend::fake_vulkan_command_buffer_operation_recorder recorder(
        vulkan_backend::fake_vulkan_command_buffer_operation_recorder_options{
            .fail_operation = true,
            .fail_operation_index = 2,
        });
    const vulkan_backend::vulkan_command_buffer_record_result result =
        recorder.record_operations(vulkan_backend::vulkan_command_buffer_id{.value = 77}, plan);

    require(result.checked, "failed-operation record result is checked");
    require(!result.completed(), "failed-operation record result does not complete");
    require(result.failed(), "failed-operation record result reports failure");
    require(
        result.status == vulkan_backend::vulkan_command_buffer_record_result_status::operation_failed,
        "record result records operation failure");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "record result maps operation failure to record fallback");
    require(result.begin_completed, "record result completes begin before operation failure");
    require(!result.end_attempted, "record result does not attempt end after operation failure");
    require(result.has_failed_operation, "record result stores failed operation");
    require(
        result.first_failed_operation_kind
            == vulkan_backend::vulkan_command_recorder_operation_kind::draw_image,
        "record result stores failed operation kind");
    require(
        result.first_failed_category == vulkan_backend::vulkan_command_packet_category::image,
        "record result stores failed operation category");
    require(result.first_failed_operation_index == 2, "record result stores failed operation index");
    require(result.first_failed_packet_index == 2, "record result stores failed packet index");
    require(result.first_failed_command_index == 12, "record result stores failed command index");
    require(result.attempted_operation_count == 3, "record result attempts through failed operation");
    require(result.recorded_operation_count == 2, "record result records successful operations before failure");
    require(result.events.size() == 4, "record result stores begin, two successful operations, and failure");
    require(result.events[3].failed, "record result marks failed operation event");
}

void test_vulkan_command_buffer_recording_result_accepts_empty_plan()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_recorder_operation_plan plan = make_empty_plan();
    vulkan_backend::fake_vulkan_command_buffer_operation_recorder recorder;
    const vulkan_backend::vulkan_command_buffer_record_result result =
        recorder.record_operations(vulkan_backend::vulkan_command_buffer_id{.value = 77}, plan);

    require(result.checked, "empty record result is checked");
    require(result.completed(), "empty record result completes");
    require(
        result.status == vulkan_backend::vulkan_command_buffer_record_result_status::recorded,
        "empty record result status is recorded");
    require(result.planned_operation_count == 0, "empty record result records zero planned operations");
    require(result.attempted_operation_count == 0, "empty record result attempts zero operations");
    require(result.recorded_operation_count == 0, "empty record result records zero operations");
    require(result.events.size() == 2, "empty record result stores begin and end events");
    require(
        result.events[0].event == vulkan_backend::vulkan_command_buffer_record_event_kind::begin,
        "empty record result begins command buffer");
    require(
        result.events[1].event == vulkan_backend::vulkan_command_buffer_record_event_kind::end,
        "empty record result ends command buffer");
}

} // namespace

int main()
{
    test_vulkan_command_buffer_recording_result_names_are_stable();
    test_native_render_pass_scope_dispatch_table_resolves_begin_end_symbols();
    test_native_render_pass_scope_records_selected_framebuffer_target();
    test_native_render_pass_scope_reports_blockers_and_failures();
    test_vulkan_command_buffer_recording_result_records_successful_operations();
    test_vulkan_command_buffer_recording_result_blocks_without_operation_plan();
    test_vulkan_command_buffer_recording_result_blocks_without_command_buffer();
    test_vulkan_command_buffer_recording_result_records_first_operation_failure();
    test_vulkan_command_buffer_recording_result_accepts_empty_plan();
    return 0;
}
