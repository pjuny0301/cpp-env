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
        .vertices = {},
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

} // namespace

int main()
{
    test_vulkan_command_packet_execution_names_are_stable();
    test_vulkan_command_packet_execution_records_successful_lifecycle();
    test_vulkan_command_packet_execution_records_first_packet_failure();
    test_vulkan_command_packet_execution_records_successful_empty_lifecycle();
    return 0;
}
