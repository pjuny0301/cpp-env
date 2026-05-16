#include "render/vulkan/vulkan_backend_adapter.h"

#include <array>
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

quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_snapshot make_binding()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_snapshot{
        .set = 0,
        .binding = 0,
        .kind = quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_kind::batch_uniform,
        .resource_id = "batch_uniform:packet",
        .required = true,
        .available = true,
    };
}

std::array<quiz_vulkan::render::vulkan_backend::vulkan_quad_vertex, 4>
make_packet_vertices()
{
    return {
        quiz_vulkan::render::vulkan_backend::vulkan_quad_vertex{.x = 0.0f, .y = 0.0f},
        quiz_vulkan::render::vulkan_backend::vulkan_quad_vertex{.x = 10.0f, .y = 0.0f},
        quiz_vulkan::render::vulkan_backend::vulkan_quad_vertex{.x = 10.0f, .y = 10.0f},
        quiz_vulkan::render::vulkan_backend::vulkan_quad_vertex{.x = 0.0f, .y = 10.0f},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_packet make_packet(
    quiz_vulkan::render::vulkan_backend::vulkan_command_packet_category category,
    quiz_vulkan::render::vulkan_backend::vulkan_batch_kind batch_kind,
    std::size_t packet_index,
    std::size_t command_index)
{
    return quiz_vulkan::render::vulkan_backend::vulkan_command_packet{
        .category = category,
        .batch_kind = batch_kind,
        .command_index = command_index,
        .packet_index = packet_index,
        .node_id = "packet-" + std::to_string(packet_index),
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 10.0f, 10.0f},
        .clipped_bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 10.0f, 10.0f},
        .scissor = quiz_vulkan::render::vulkan_backend::vulkan_scissor_rect{
            .x = 0,
            .y = 0,
            .width = 10,
            .height = 10,
        },
        .vertices = make_packet_vertices(),
        .descriptor_set_count = 1,
        .binding_count = 1,
        .bindings = {make_binding()},
    };
}

void append_packet(
    quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result& bridge,
    quiz_vulkan::render::vulkan_backend::vulkan_command_packet packet)
{
    using namespace quiz_vulkan::render;

    switch (packet.category) {
    case vulkan_backend::vulkan_command_packet_category::rect:
        ++bridge.rect_packet_count;
        break;
    case vulkan_backend::vulkan_command_packet_category::text:
        ++bridge.text_packet_count;
        break;
    case vulkan_backend::vulkan_command_packet_category::image:
        ++bridge.image_packet_count;
        break;
    case vulkan_backend::vulkan_command_packet_category::debug_bounds:
        ++bridge.debug_bounds_packet_count;
        break;
    }

    bridge.packets.push_back(std::move(packet));
    bridge.packet_count = bridge.packets.size();
    bridge.planned_batch_count = bridge.packets.size();
}

quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result make_ready_bridge()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_command_packet_bridge_result bridge{
        .checked = true,
        .status = vulkan_backend::vulkan_command_packet_bridge_status::ready,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::none,
        .pipeline_checked = true,
        .pipeline_ready = true,
        .resource_bindings_checked = true,
        .resource_bindings_ready = true,
        .resource_registry_checked = true,
        .resource_registry_ready = true,
    };

    append_packet(
        bridge,
        make_packet(
            vulkan_backend::vulkan_command_packet_category::rect,
            vulkan_backend::vulkan_batch_kind::quad,
            0,
            10));
    append_packet(
        bridge,
        make_packet(
            vulkan_backend::vulkan_command_packet_category::text,
            vulkan_backend::vulkan_batch_kind::text,
            1,
            11));
    append_packet(
        bridge,
        make_packet(
            vulkan_backend::vulkan_command_packet_category::image,
            vulkan_backend::vulkan_batch_kind::image,
            2,
            12));
    append_packet(
        bridge,
        make_packet(
            vulkan_backend::vulkan_command_packet_category::debug_bounds,
            vulkan_backend::vulkan_batch_kind::debug_bounds,
            3,
            13));
    return bridge;
}

quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result make_empty_bridge()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_status::ready,
        .fallback_reason = quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason::none,
        .pipeline_checked = true,
        .pipeline_ready = true,
        .resource_bindings_checked = true,
        .resource_bindings_ready = true,
        .resource_registry_checked = true,
        .resource_registry_ready = true,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_render_pass_scope_record_result
make_ready_render_pass_scope(std::size_t selected_index = 1)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_render_pass_scope_record_result{
        .checked = true,
        .status = vulkan_backend::vulkan_native_render_pass_scope_record_status::recorded,
        .selected_framebuffer_target_index = selected_index,
        .framebuffer_target_count = 2,
        .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = selected_index + 1},
        .command_buffer =
            vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 909},
        .render_pass = vulkan_backend::vulkan_render_pass_handle{.value = 300},
        .framebuffer = vulkan_backend::vulkan_framebuffer_handle{.value = 13000 + selected_index + 1},
        .extent = vulkan_backend::vulkan_surface_extent{.width = 1280, .height = 720},
        .begin_render_pass_symbol = vulkan_backend::vulkan_native_function_pointer{.value = 1401},
        .end_render_pass_symbol = vulkan_backend::vulkan_native_function_pointer{.value = 1402},
        .framebuffer_targets_ready = true,
        .command_buffer_ready = true,
        .render_pass_ready = true,
        .framebuffer_ready = true,
        .extent_ready = true,
        .extent_matches = true,
        .dispatch_table_ready = true,
        .vk_cmd_begin_render_pass_called = true,
        .vk_cmd_end_render_pass_called = true,
        .diagnostic = "Native Vulkan render pass scope recorded",
    };
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

quiz_vulkan::render::vulkan_backend::vulkan_native_command_packet_executor_evidence
make_native_packet_evidence(
    quiz_vulkan::render::vulkan_backend::vulkan_native_function_table_diagnostics
        native_functions = make_native_functions())
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_command_packet_executor_evidence{
        .native_functions = std::move(native_functions),
        .command_buffer =
            vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 909},
        .pipeline = vulkan_backend::vulkan_graphics_pipeline_handle{.value = 4400},
        .pipeline_layout = vulkan_backend::vulkan_pipeline_layout_handle{.value = 5500},
        .viewport = quiz_vulkan::render::render_rect{0.0f, 0.0f, 1280.0f, 720.0f},
        .viewport_available = true,
        .descriptor_sets = {
            vulkan_backend::vulkan_native_command_packet_descriptor_set{
                .packet_index = 0,
                .set = 0,
                .descriptor_set =
                    vulkan_backend::vulkan_native_descriptor_set_handle{.value = 7000},
                .required = true,
                .available = true,
            },
            vulkan_backend::vulkan_native_command_packet_descriptor_set{
                .packet_index = 1,
                .set = 0,
                .descriptor_set =
                    vulkan_backend::vulkan_native_descriptor_set_handle{.value = 7001},
                .required = true,
                .available = true,
            },
            vulkan_backend::vulkan_native_command_packet_descriptor_set{
                .packet_index = 2,
                .set = 0,
                .descriptor_set =
                    vulkan_backend::vulkan_native_descriptor_set_handle{.value = 7002},
                .required = true,
                .available = true,
            },
            vulkan_backend::vulkan_native_command_packet_descriptor_set{
                .packet_index = 3,
                .set = 0,
                .descriptor_set =
                    vulkan_backend::vulkan_native_descriptor_set_handle{.value = 7003},
                .required = true,
                .available = true,
            },
        },
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_result
make_native_packet_frame_without_descriptor_handles()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_backend_frame_result frame;
    frame.surface = vulkan_backend::vulkan_surface_extent{.width = 1280, .height = 720};
    frame.viewport = quiz_vulkan::render::render_rect{4.0f, 8.0f, 640.0f, 360.0f};
    frame.command_buffer_recording.command_buffer =
        vulkan_backend::vulkan_command_buffer_id{.value = 44};
    frame.pipeline.pipeline_layout.pipeline_layout =
        vulkan_backend::vulkan_pipeline_layout_handle{.value = 5500};
    frame.pipeline.graphics_pipeline.pipeline =
        vulkan_backend::vulkan_graphics_pipeline_handle{.value = 4400};
    frame.command_packets = make_ready_bridge();
    return frame;
}

void test_vulkan_command_packet_execution_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::not_checked)
            == std::string_view{"not_checked"},
        "execution status name for not checked is stable");
    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::completed)
            == std::string_view{"completed"},
        "execution status name for completed is stable");
    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::packet_bridge_unavailable)
            == std::string_view{"packet_bridge_unavailable"},
        "execution status name for packet bridge unavailable is stable");
    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::begin_failed)
            == std::string_view{"begin_failed"},
        "execution status name for begin failed is stable");
    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::packet_failed)
            == std::string_view{"packet_failed"},
        "execution status name for packet failed is stable");
    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::end_failed)
            == std::string_view{"end_failed"},
        "execution status name for end failed is stable");
    require(
        vulkan_backend::command_packet_execution_event_name(
            vulkan_backend::vulkan_command_packet_execution_event::begin)
            == std::string_view{"begin"},
        "execution event name for begin is stable");
    require(
        vulkan_backend::command_packet_execution_event_name(
            vulkan_backend::vulkan_command_packet_execution_event::packet)
            == std::string_view{"packet"},
        "execution event name for packet is stable");
    require(
        vulkan_backend::command_packet_execution_event_name(
            vulkan_backend::vulkan_command_packet_execution_event::end)
            == std::string_view{"end"},
        "execution event name for end is stable");
    require(
        vulkan_backend::native_command_packet_execution_status_name(
            vulkan_backend::vulkan_native_command_packet_execution_status::completed)
            == std::string_view{"completed"},
        "native packet execution status name for completed is stable");
    require(
        vulkan_backend::native_command_packet_execution_status_name(
            vulkan_backend::vulkan_native_command_packet_execution_status::native_command_symbol_unavailable)
            == std::string_view{"native_command_symbol_unavailable"},
        "native packet execution status name for missing symbol is stable");
    require(
        vulkan_backend::native_command_packet_execution_status_name(
            vulkan_backend::vulkan_native_command_packet_execution_status::invalid_packet_data)
            == std::string_view{"invalid_packet_data"},
        "native packet execution status name for invalid packet data is stable");
    require(
        vulkan_backend::native_command_packet_call_kind_name(
            vulkan_backend::vulkan_native_command_packet_call_kind::bind_pipeline)
            == std::string_view{"bind_pipeline"},
        "native packet call kind name for bind pipeline is stable");
    require(
        vulkan_backend::native_command_packet_call_kind_name(
            vulkan_backend::vulkan_native_command_packet_call_kind::draw)
            == std::string_view{"draw"},
        "native packet call kind name for draw is stable");
    require(
        vulkan_backend::scoped_command_packet_execution_status_name(
            vulkan_backend::vulkan_scoped_command_packet_execution_status::completed)
            == std::string_view{"completed"},
        "scoped execution status name for completed is stable");
    require(
        vulkan_backend::scoped_command_packet_execution_status_name(
            vulkan_backend::vulkan_scoped_command_packet_execution_status::render_pass_scope_unavailable)
            == std::string_view{"render_pass_scope_unavailable"},
        "scoped execution status name for unavailable render pass scope is stable");
    require(
        vulkan_backend::scoped_command_packet_execution_status_name(
            vulkan_backend::vulkan_scoped_command_packet_execution_status::packet_failed)
            == std::string_view{"packet_failed"},
        "scoped execution status name for packet failure is stable");
}

void test_vulkan_command_packet_execution_records_successful_lifecycle()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);

    require(result.checked, "packet execution result is checked");
    require(result.completed(), "packet execution completes");
    require(!result.failed(), "packet execution reports no failure");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::completed,
        "packet execution status is completed");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "packet execution has no fallback");
    require(result.packet_bridge_checked, "packet execution records checked bridge");
    require(result.packet_bridge_ready, "packet execution records ready bridge");
    require(result.begin_attempted, "packet execution attempts begin");
    require(result.begin_completed, "packet execution completes begin");
    require(result.end_attempted, "packet execution attempts end");
    require(result.end_completed, "packet execution completes end");
    require(!result.has_failed_packet, "packet execution has no failed packet");
    require(result.planned_packet_count == 4, "packet execution records planned count");
    require(result.attempted_packet_count == 4, "packet execution records attempted packet count");
    require(result.executed_packet_count == 4, "packet execution records executed packet count");
    require(result.rect_packet_count == 1, "packet execution counts rect packets");
    require(result.text_packet_count == 1, "packet execution counts text packets");
    require(result.image_packet_count == 1, "packet execution counts image packets");
    require(result.debug_bounds_packet_count == 1, "packet execution counts debug bounds packets");
    require(result.events.size() == 6, "packet execution records begin, packets, and end events");
    require(
        result.events[0].event == vulkan_backend::vulkan_command_packet_execution_event::begin,
        "packet execution first event is begin");
    require(
        result.events[1].event == vulkan_backend::vulkan_command_packet_execution_event::packet,
        "packet execution second event is packet");
    require(result.events[1].packet_index == 0, "packet execution records first packet index");
    require(
        result.events[2].category == vulkan_backend::vulkan_command_packet_category::text,
        "packet execution records second packet category");
    require(
        result.events[3].category == vulkan_backend::vulkan_command_packet_category::image,
        "packet execution records third packet category");
    require(
        result.events[4].category == vulkan_backend::vulkan_command_packet_category::debug_bounds,
        "packet execution records fourth packet category");
    require(
        result.events[5].event == vulkan_backend::vulkan_command_packet_execution_event::end,
        "packet execution final event is end");

    for (const vulkan_backend::vulkan_command_packet_execution_snapshot& event : result.events) {
        require(event.successful(), "successful packet execution event reports success");
    }
    require(executor.execution_result().completed(), "retained fake execution result completes");
}

void test_vulkan_command_packet_execution_records_first_packet_failure()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    vulkan_backend::fake_vulkan_command_packet_executor executor(
        vulkan_backend::fake_vulkan_command_packet_executor_options{
            .fail_packet = true,
            .fail_packet_index = 2,
        });
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);

    require(result.checked, "failed packet execution result is checked");
    require(!result.completed(), "failed packet execution does not complete");
    require(result.failed(), "failed packet execution reports failure");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::packet_failed,
        "packet execution status records packet failure");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "packet execution maps packet failure to record commands fallback");
    require(result.begin_attempted, "failed packet execution attempts begin");
    require(result.begin_completed, "failed packet execution completes begin before packet failure");
    require(!result.end_attempted, "failed packet execution does not attempt end after packet failure");
    require(!result.end_completed, "failed packet execution does not complete end after packet failure");
    require(result.has_failed_packet, "failed packet execution records failed packet");
    require(
        result.first_failed_category == vulkan_backend::vulkan_command_packet_category::image,
        "failed packet execution records failed packet category");
    require(
        result.first_failed_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "failed packet execution records failed packet batch kind");
    require(result.first_failed_packet_index == 2, "failed packet execution records failed packet index");
    require(result.first_failed_command_index == 12, "failed packet execution records failed command index");
    require(result.planned_packet_count == 4, "failed packet execution preserves planned count");
    require(result.attempted_packet_count == 3, "failed packet execution records attempted packets through failure");
    require(result.executed_packet_count == 2, "failed packet execution records successfully executed packets");
    require(result.rect_packet_count == 1, "failed packet execution counts completed rect packet");
    require(result.text_packet_count == 1, "failed packet execution counts completed text packet");
    require(result.image_packet_count == 0, "failed packet execution does not count failed image packet as executed");
    require(result.debug_bounds_packet_count == 0, "failed packet execution stops before debug packet");
    require(result.events.size() == 4, "failed packet execution records begin, two packets, and failed packet");
    require(result.events[3].failed, "failed packet execution marks first failed packet event");
    require(
        result.events[3].category == vulkan_backend::vulkan_command_packet_category::image,
        "failed packet execution marks failed packet category");
}

void test_vulkan_command_packet_execution_records_successful_empty_lifecycle()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_empty_bridge();
    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);

    require(result.checked, "empty packet execution result is checked");
    require(result.completed(), "empty packet execution completes");
    require(!result.failed(), "empty packet execution does not fail");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::completed,
        "empty packet execution status is completed");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "empty packet execution has no fallback");
    require(result.begin_attempted, "empty packet execution attempts begin");
    require(result.begin_completed, "empty packet execution completes begin");
    require(result.end_attempted, "empty packet execution attempts end");
    require(result.end_completed, "empty packet execution completes end");
    require(result.planned_packet_count == 0, "empty packet execution records no planned packets");
    require(result.attempted_packet_count == 0, "empty packet execution records no attempted packets");
    require(result.executed_packet_count == 0, "empty packet execution records no executed packets");
    require(result.rect_packet_count == 0, "empty packet execution records no rect packets");
    require(result.text_packet_count == 0, "empty packet execution records no text packets");
    require(result.image_packet_count == 0, "empty packet execution records no image packets");
    require(result.debug_bounds_packet_count == 0, "empty packet execution records no debug packets");
    require(result.events.size() == 2, "empty packet execution records begin and end only");
    require(
        result.events[0].event == vulkan_backend::vulkan_command_packet_execution_event::begin,
        "empty packet execution first event is begin");
    require(
        result.events[1].event == vulkan_backend::vulkan_command_packet_execution_event::end,
        "empty packet execution final event is end");
}

void test_vulkan_native_command_packet_executor_translates_packets_to_native_calls()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    vulkan_backend::vulkan_native_command_packet_executor executor(
        make_native_packet_evidence());
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(result.checked, "native packet executor result is checked");
    require(result.completed(), "native packet executor completes interface execution");
    require(result.events.size() == 6, "native packet executor records begin, packets, and end");
    require(
        result.events.front().event == vulkan_backend::vulkan_command_packet_execution_event::begin,
        "native packet executor first interface event is begin");
    require(
        result.events.back().event == vulkan_backend::vulkan_command_packet_execution_event::end,
        "native packet executor final interface event is end");
    require(native_result.checked, "native packet translation result is checked");
    require(native_result.completed(), "native packet translation completes");
    require(!native_result.failed(), "native packet translation reports no failure");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::completed,
        "native packet translation status is completed");
    require(
        native_result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "native packet translation has no fallback");
    require(native_result.packet_bridge_checked, "native packet translation records bridge check");
    require(native_result.packet_bridge_ready, "native packet translation records ready bridge");
    require(
        native_result.native_function_table_checked,
        "native packet translation records checked function table");
    require(
        native_result.native_command_symbols_ready,
        "native packet translation records native command symbols ready");
    require(native_result.command_buffer_ready, "native packet translation records command buffer ready");
    require(native_result.pipeline_ready, "native packet translation records pipeline ready");
    require(
        native_result.pipeline_layout_ready,
        "native packet translation records pipeline layout ready");
    require(native_result.viewport_ready, "native packet translation records viewport ready");
    require(
        native_result.descriptor_sets_ready,
        "native packet translation records descriptor set handles ready");
    require(native_result.planned_packet_count == 4, "native packet translation records planned packets");
    require(
        native_result.attempted_packet_count == 4,
        "native packet translation records attempted packets");
    require(
        native_result.translated_packet_count == 4,
        "native packet translation records translated packets");
    require(
        native_result.attempted_native_call_count == 20,
        "native packet translation attempts five native calls per packet");
    require(
        native_result.completed_native_call_count == 20,
        "native packet translation completes five native calls per packet");
    require(native_result.calls.size() == 20, "native packet translation records native call evidence");
    require(
        native_result.calls[0].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::bind_pipeline,
        "native packet translation starts each packet by binding the pipeline");
    require(
        native_result.calls[1].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::bind_descriptor_sets,
        "native packet translation binds descriptor sets");
    require(
        native_result.calls[2].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::set_viewport,
        "native packet translation sets the viewport");
    require(
        native_result.calls[3].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::set_scissor,
        "native packet translation sets the packet scissor");
    require(
        native_result.calls[4].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::draw,
        "native packet translation emits a draw call");
    require(native_result.calls[4].symbol_name == "vkCmdDraw", "draw call records symbol name");
    require(native_result.calls[4].vertex_count == 4, "draw call records quad vertex count");
    require(native_result.calls[4].packet_index == 0, "draw call records packet index");
    for (const vulkan_backend::vulkan_native_command_packet_call_evidence& call :
         native_result.calls) {
        require(call.successful(), "each native command packet call reports success");
    }
    require(
        executor.execution_result().completed(),
        "native packet executor retains completed interface result");
}

void test_vulkan_native_command_packet_evidence_preserves_descriptor_handle_gap()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    const vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            make_native_functions());

    require(evidence.native_functions.checked, "native packet evidence keeps function diagnostics");
    require(
        evidence.command_buffer.value == 9044,
        "native packet evidence derives scoped command buffer from frame recording");
    require(evidence.pipeline.value == 4400, "native packet evidence keeps graphics pipeline handle");
    require(
        evidence.pipeline_layout.value == 5500,
        "native packet evidence keeps pipeline layout handle");
    require(evidence.viewport_available, "native packet evidence keeps visible viewport");
    require(evidence.viewport.x == 4.0f, "native packet evidence keeps viewport x");
    require(evidence.viewport.y == 8.0f, "native packet evidence keeps viewport y");
    require(evidence.viewport.width == 640.0f, "native packet evidence keeps viewport width");
    require(evidence.viewport.height == 360.0f, "native packet evidence keeps viewport height");
    require(
        evidence.descriptor_sets.size() == 4,
        "native packet evidence records one pending descriptor set per packet");
    require(
        evidence.descriptor_sets.front().packet_index == 0,
        "native packet evidence records first packet descriptor owner");
    require(evidence.descriptor_sets.front().set == 0, "native packet evidence records set index");
    require(
        evidence.descriptor_sets.front().required,
        "native packet evidence records descriptor set requirement");
    require(
        !evidence.descriptor_sets.front().available,
        "native packet evidence does not fabricate descriptor set availability");
    require(
        !evidence.descriptor_sets.front().descriptor_set.valid(),
        "native packet evidence does not fabricate native descriptor set handles");

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(frame.command_packets);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(result.checked, "descriptor gap result is checked");
    require(!result.completed(), "descriptor gap does not complete native packet execution");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::packet_failed,
        "descriptor gap fails inside packet execution");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::descriptor_sets_unavailable,
        "descriptor gap reports unavailable native descriptor sets");
    require(!native_result.descriptor_sets_ready, "descriptor gap keeps descriptor sets unready");
    require(native_result.first_failed_packet_index == 0, "descriptor gap blocks first packet");
}

void test_vulkan_native_command_packet_executor_blocks_missing_draw_symbol()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_native_command_packet_executor executor(
        make_native_packet_evidence(make_native_functions({"vkCmdDraw"})));
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(make_ready_bridge());
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(result.checked, "missing draw result is checked");
    require(!result.completed(), "missing draw does not complete interface execution");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::begin_failed,
        "missing draw fails before packet translation begins");
    require(result.begin_attempted, "missing draw records begin attempt");
    require(!result.begin_completed, "missing draw does not complete begin");
    require(result.events.size() == 1, "missing draw records only failed begin event");
    require(result.events.front().failed, "missing draw marks begin event failed");
    require(native_result.failed(), "missing draw reports native failure");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::native_command_symbol_unavailable,
        "missing draw reports native command symbol unavailable");
    require(
        native_result.missing_native_symbol_name == "vkCmdDraw",
        "missing draw records missing symbol name");
    require(!native_result.native_command_symbols_ready, "missing draw records symbols not ready");
    require(native_result.calls.empty(), "missing draw records no native calls");
    require(
        native_result.diagnostic.find("vkCmdDraw") != std::string::npos,
        "missing draw diagnostic names missing draw symbol");
}

void test_vulkan_native_command_packet_executor_blocks_invalid_command_buffer()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        make_native_packet_evidence();
    evidence.command_buffer = {};
    vulkan_backend::vulkan_native_command_packet_executor executor(std::move(evidence));
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(make_ready_bridge());
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "invalid command buffer does not complete interface execution");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::begin_failed,
        "invalid command buffer maps to begin failure");
    require(native_result.failed(), "invalid command buffer reports native failure");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::command_buffer_unavailable,
        "invalid command buffer records unavailable command buffer status");
    require(!native_result.command_buffer_ready, "invalid command buffer records command buffer not ready");
    require(native_result.calls.empty(), "invalid command buffer records no native calls");
    require(
        native_result.diagnostic.find("command buffer") != std::string::npos,
        "invalid command buffer diagnostic names command buffer");
}

void test_vulkan_native_command_packet_executor_blocks_invalid_packet_scissor()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    bridge.packets[1].scissor = {};
    vulkan_backend::vulkan_native_command_packet_executor executor(
        make_native_packet_evidence());
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "invalid scissor does not complete interface execution");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::packet_failed,
        "invalid scissor maps to packet failure");
    require(result.has_failed_packet, "invalid scissor records failed packet");
    require(
        result.first_failed_category == vulkan_backend::vulkan_command_packet_category::text,
        "invalid scissor records failed packet category");
    require(result.attempted_packet_count == 2, "invalid scissor attempts packets through failure");
    require(result.executed_packet_count == 1, "invalid scissor translates only prior packet");
    require(native_result.failed(), "invalid scissor reports native failure");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::invalid_packet_data,
        "invalid scissor records invalid packet data status");
    require(
        native_result.calls.size() == 5,
        "invalid scissor keeps native call evidence for completed prior packet");
    require(
        native_result.diagnostic.find("scissor") != std::string::npos,
        "invalid scissor diagnostic names scissor data");
}

void test_vulkan_scoped_command_packet_execution_records_scope_and_packets()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_scoped_command_packet_execution_result result =
        vulkan_backend::execute_vulkan_scoped_command_packets(
            executor,
            vulkan_backend::vulkan_scoped_command_packet_execution_request{
                .render_pass_scope = make_ready_render_pass_scope(1),
                .packet_bridge = make_ready_bridge(),
            });

    require(result.checked, "scoped packet execution is checked");
    require(result.completed(), "scoped packet execution completes");
    require(!result.failed(), "scoped packet execution reports no failure");
    require(
        result.status == vulkan_backend::vulkan_scoped_command_packet_execution_status::completed,
        "scoped packet execution reports completed");
    require(result.render_pass_scope_id == 2, "scoped packet execution records scope id");
    require(result.selected_framebuffer_target_index == 1, "scoped packet execution records target index");
    require(result.image_id.value == 2, "scoped packet execution records image id");
    require(result.framebuffer.value == 13002, "scoped packet execution records framebuffer handle");
    require(result.command_buffer.value == 909, "scoped packet execution records command buffer");
    require(result.render_pass_scope_checked, "scoped packet execution records checked scope");
    require(result.render_pass_scope_ready, "scoped packet execution records ready scope");
    require(result.command_buffer_ready, "scoped packet execution records command buffer readiness");
    require(result.packet_bridge_checked, "scoped packet execution records checked packet bridge");
    require(result.packet_bridge_ready, "scoped packet execution records ready packet bridge");
    require(!result.scoped_execution_empty, "scoped packet execution records non-empty scope");
    require(result.render_pass_begin_attempted, "scoped packet execution records render pass begin attempt");
    require(result.render_pass_begin_completed, "scoped packet execution records render pass begin completion");
    require(result.render_pass_end_attempted, "scoped packet execution records render pass end attempt");
    require(result.render_pass_end_completed, "scoped packet execution records render pass end completion");
    require(!result.render_pass_end_skipped, "scoped packet execution does not skip render pass end");
    require(result.packet_execution_ready, "scoped packet execution records packet execution readiness");
    require(result.operation_plan_ready, "scoped packet execution records operation plan readiness");
    require(result.planned_packet_count == 4, "scoped packet execution records planned packets");
    require(result.attempted_packet_count == 4, "scoped packet execution records attempted packets");
    require(result.executed_packet_count == 4, "scoped packet execution records executed packets");
    require(result.rect_packet_count == 1, "scoped packet execution counts rect packets");
    require(result.text_packet_count == 1, "scoped packet execution counts text packets");
    require(result.image_packet_count == 1, "scoped packet execution counts image packets");
    require(result.debug_bounds_packet_count == 1, "scoped packet execution counts debug packets");
    require(result.packet_execution.events.size() == 6, "scoped packet execution keeps packet events");
    require(result.operation_plan.operation_count == 4, "scoped packet execution builds recorder operations");
    require(executor.execution_result().completed(), "scoped packet execution uses packet executor");
}

void test_vulkan_scoped_command_packet_execution_accepts_empty_scope()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_scoped_command_packet_execution_result result =
        vulkan_backend::execute_vulkan_scoped_command_packets(
            executor,
            vulkan_backend::vulkan_scoped_command_packet_execution_request{
                .render_pass_scope = make_ready_render_pass_scope(0),
                .packet_bridge = make_empty_bridge(),
            });

    require(result.checked, "empty scoped packet execution is checked");
    require(result.completed(), "empty scoped packet execution completes");
    require(result.scoped_execution_empty, "empty scoped packet execution records empty scope");
    require(result.render_pass_scope_id == 1, "empty scoped packet execution records scope id");
    require(result.planned_packet_count == 0, "empty scoped packet execution records no planned packets");
    require(result.attempted_packet_count == 0, "empty scoped packet execution records no attempted packets");
    require(result.executed_packet_count == 0, "empty scoped packet execution records no executed packets");
    require(result.packet_execution.events.size() == 2, "empty scoped packet execution keeps begin and end events");
    require(result.operation_plan.completed(), "empty scoped packet execution builds an empty operation plan");
}

void test_vulkan_scoped_command_packet_execution_reports_scope_and_bridge_blockers()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_command_packet_executor executor;

    vulkan_backend::vulkan_native_render_pass_scope_record_result begin_failed_scope =
        make_ready_render_pass_scope(0);
    begin_failed_scope.status =
        vulkan_backend::vulkan_native_render_pass_scope_record_status::begin_failed;
    begin_failed_scope.vk_cmd_end_render_pass_called = false;
    begin_failed_scope.diagnostic = "Native Vulkan render pass scope begin failed";
    const vulkan_backend::vulkan_scoped_command_packet_execution_result begin_failed =
        vulkan_backend::execute_vulkan_scoped_command_packets(
            executor,
            vulkan_backend::vulkan_scoped_command_packet_execution_request{
                .render_pass_scope = begin_failed_scope,
                .packet_bridge = make_ready_bridge(),
            });
    require(
        begin_failed.status == vulkan_backend::vulkan_scoped_command_packet_execution_status::begin_failed,
        "scoped packet execution reports render pass begin failure");
    require(begin_failed.render_pass_end_skipped, "scoped packet execution skips render pass end after begin failure");
    require(!begin_failed.packet_execution_checked, "scoped packet execution does not execute packets after begin failure");

    const vulkan_backend::vulkan_command_packet_bridge_result unavailable_bridge{
        .checked = true,
        .status = vulkan_backend::vulkan_command_packet_bridge_status::pipeline_unavailable,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        .pipeline_checked = true,
        .pipeline_ready = false,
        .planned_batch_count = 4,
    };
    const vulkan_backend::vulkan_scoped_command_packet_execution_result bridge_blocked =
        vulkan_backend::execute_vulkan_scoped_command_packets(
            executor,
            vulkan_backend::vulkan_scoped_command_packet_execution_request{
                .render_pass_scope = make_ready_render_pass_scope(0),
                .packet_bridge = unavailable_bridge,
            });
    require(
        bridge_blocked.status
            == vulkan_backend::vulkan_scoped_command_packet_execution_status::packet_bridge_unavailable,
        "scoped packet execution reports unavailable packet bridge");
    require(
        bridge_blocked.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        "scoped packet execution preserves bridge fallback");
}

void test_vulkan_scoped_command_packet_execution_reports_first_packet_failure_inside_scope()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_command_packet_executor executor(
        vulkan_backend::fake_vulkan_command_packet_executor_options{
            .fail_packet = true,
            .fail_packet_index = 2,
        });
    const vulkan_backend::vulkan_scoped_command_packet_execution_result result =
        vulkan_backend::execute_vulkan_scoped_command_packets(
            executor,
            vulkan_backend::vulkan_scoped_command_packet_execution_request{
                .render_pass_scope = make_ready_render_pass_scope(1),
                .packet_bridge = make_ready_bridge(),
            });

    require(result.checked, "failed scoped packet execution is checked");
    require(!result.completed(), "failed scoped packet execution does not complete");
    require(result.failed(), "failed scoped packet execution reports failure");
    require(
        result.status == vulkan_backend::vulkan_scoped_command_packet_execution_status::packet_failed,
        "scoped packet execution reports packet failure");
    require(result.has_failed_packet, "scoped packet execution records failed packet");
    require(
        result.first_failed_category == vulkan_backend::vulkan_command_packet_category::image,
        "scoped packet execution records first failed category");
    require(
        result.first_failed_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "scoped packet execution records first failed batch kind");
    require(result.first_failed_packet_index == 2, "scoped packet execution records first failed packet index");
    require(result.first_failed_command_index == 12, "scoped packet execution records first failed command index");
    require(result.attempted_packet_count == 3, "scoped packet execution records attempted packets through failure");
    require(result.executed_packet_count == 2, "scoped packet execution records packets before failure");
    require(result.rect_packet_count == 1, "scoped packet execution counts completed rect packet");
    require(result.text_packet_count == 1, "scoped packet execution counts completed text packet");
    require(result.image_packet_count == 0, "scoped packet execution does not count failed packet as executed");
    require(result.render_pass_begin_completed, "scoped packet execution records render pass scope began");
    require(result.render_pass_end_completed, "scoped packet execution preserves render pass scope end evidence");
    require(!result.operation_plan_ready, "scoped packet execution blocks operation plan after packet failure");
}

} // namespace

int main()
{
    test_vulkan_command_packet_execution_names_are_stable();
    test_vulkan_command_packet_execution_records_successful_lifecycle();
    test_vulkan_command_packet_execution_records_first_packet_failure();
    test_vulkan_command_packet_execution_records_successful_empty_lifecycle();
    test_vulkan_native_command_packet_executor_translates_packets_to_native_calls();
    test_vulkan_native_command_packet_evidence_preserves_descriptor_handle_gap();
    test_vulkan_native_command_packet_executor_blocks_missing_draw_symbol();
    test_vulkan_native_command_packet_executor_blocks_invalid_command_buffer();
    test_vulkan_native_command_packet_executor_blocks_invalid_packet_scissor();
    test_vulkan_scoped_command_packet_execution_records_scope_and_packets();
    test_vulkan_scoped_command_packet_execution_accepts_empty_scope();
    test_vulkan_scoped_command_packet_execution_reports_scope_and_bridge_blockers();
    test_vulkan_scoped_command_packet_execution_reports_first_packet_failure_inside_scope();
    return 0;
}
