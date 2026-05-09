#include "render/vulkan/vulkan_backend_adapter.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_snapshot make_binding(
    std::size_t binding,
    std::string resource_id)
{
    return quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_snapshot{
        .set = 0,
        .binding = binding,
        .kind = quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_kind::batch_uniform,
        .resource_id = std::move(resource_id),
        .required = true,
        .available = true,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_packet make_packet(
    quiz_vulkan::render::vulkan_backend::vulkan_command_packet_category category,
    quiz_vulkan::render::vulkan_backend::vulkan_batch_kind batch_kind,
    std::size_t packet_index,
    std::size_t command_index,
    bool clipped = false)
{
    return quiz_vulkan::render::vulkan_backend::vulkan_command_packet{
        .category = category,
        .batch_kind = batch_kind,
        .command_index = command_index,
        .packet_index = packet_index,
        .node_id = "packet-" + std::to_string(packet_index),
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 10.0f, 10.0f},
        .clipped_bounds = clipped
            ? quiz_vulkan::render::render_rect{1.0f, 1.0f, 8.0f, 8.0f}
            : quiz_vulkan::render::render_rect{0.0f, 0.0f, 10.0f, 10.0f},
        .scissor = quiz_vulkan::render::vulkan_backend::vulkan_scissor_rect{
            .x = clipped ? 1 : 0,
            .y = clipped ? 1 : 0,
            .width = clipped ? 8U : 10U,
            .height = clipped ? 8U : 10U,
        },
        .vertices = {},
        .descriptor_set_count = 1,
        .binding_count = 1,
        .bindings = {make_binding(0, "binding:" + std::to_string(packet_index))},
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

    if (packet.clipped_bounds.x != packet.bounds.x
        || packet.clipped_bounds.y != packet.bounds.y
        || packet.clipped_bounds.width != packet.bounds.width
        || packet.clipped_bounds.height != packet.bounds.height) {
        ++bridge.clipped_packet_count;
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
            12,
            true));
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

void test_vulkan_command_recorder_operation_plan_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::command_recorder_operation_plan_status_name(
            vulkan_backend::vulkan_command_recorder_operation_plan_status::not_checked)
            == std::string_view{"not_checked"},
        "operation plan status name for not checked is stable");
    require(
        vulkan_backend::command_recorder_operation_plan_status_name(
            vulkan_backend::vulkan_command_recorder_operation_plan_status::ready)
            == std::string_view{"ready"},
        "operation plan status name for ready is stable");
    require(
        vulkan_backend::command_recorder_operation_plan_status_name(
            vulkan_backend::vulkan_command_recorder_operation_plan_status::packet_bridge_unavailable)
            == std::string_view{"packet_bridge_unavailable"},
        "operation plan status name for bridge unavailable is stable");
    require(
        vulkan_backend::command_recorder_operation_plan_status_name(
            vulkan_backend::vulkan_command_recorder_operation_plan_status::packet_execution_unavailable)
            == std::string_view{"packet_execution_unavailable"},
        "operation plan status name for execution unavailable is stable");
    require(
        vulkan_backend::command_recorder_operation_kind_name(
            vulkan_backend::vulkan_command_recorder_operation_kind::draw_rect)
            == std::string_view{"draw_rect"},
        "operation kind name for rect draw is stable");
    require(
        vulkan_backend::command_recorder_operation_kind_name(
            vulkan_backend::vulkan_command_recorder_operation_kind::draw_text)
            == std::string_view{"draw_text"},
        "operation kind name for text draw is stable");
    require(
        vulkan_backend::command_recorder_operation_kind_name(
            vulkan_backend::vulkan_command_recorder_operation_kind::draw_image)
            == std::string_view{"draw_image"},
        "operation kind name for image draw is stable");
    require(
        vulkan_backend::command_recorder_operation_kind_name(
            vulkan_backend::vulkan_command_recorder_operation_kind::draw_debug_bounds)
            == std::string_view{"draw_debug_bounds"},
        "operation kind name for debug bounds draw is stable");
}

void test_vulkan_command_recorder_operation_plan_converts_packets_in_order()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_command_packet_execution_result execution =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_command_recorder_operation_plan plan =
        vulkan_backend::build_vulkan_command_recorder_operation_plan(bridge, execution);

    require(plan.checked, "operation plan is checked");
    require(plan.completed(), "operation plan completes");
    require(!plan.blocked(), "operation plan is not blocked");
    require(
        plan.status == vulkan_backend::vulkan_command_recorder_operation_plan_status::ready,
        "operation plan status is ready");
    require(
        plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "operation plan has no fallback");
    require(plan.packet_bridge_checked, "operation plan records checked bridge");
    require(plan.packet_bridge_ready, "operation plan records ready bridge");
    require(plan.packet_execution_checked, "operation plan records checked execution");
    require(plan.packet_execution_ready, "operation plan records ready execution");
    require(plan.planned_packet_count == 4, "operation plan records planned packets");
    require(plan.operation_count == 4, "operation plan records operation count");
    require(plan.rect_operation_count == 1, "operation plan counts rect operation");
    require(plan.text_operation_count == 1, "operation plan counts text operation");
    require(plan.image_operation_count == 1, "operation plan counts image operation");
    require(plan.debug_bounds_operation_count == 1, "operation plan counts debug bounds operation");
    require(plan.clipped_operation_count == 1, "operation plan counts clipped operation");
    require(plan.operations.size() == 4, "operation plan stores four operation summaries");
    require(
        plan.operations[0].kind == vulkan_backend::vulkan_command_recorder_operation_kind::draw_rect,
        "operation plan first operation draws rect");
    require(
        plan.operations[1].kind == vulkan_backend::vulkan_command_recorder_operation_kind::draw_text,
        "operation plan second operation draws text");
    require(
        plan.operations[2].kind == vulkan_backend::vulkan_command_recorder_operation_kind::draw_image,
        "operation plan third operation draws image");
    require(
        plan.operations[3].kind
            == vulkan_backend::vulkan_command_recorder_operation_kind::draw_debug_bounds,
        "operation plan fourth operation draws debug bounds");
    require(plan.operations[2].packet_index == 2, "operation plan preserves packet index");
    require(plan.operations[2].command_index == 12, "operation plan preserves command index");
    require(plan.operations[2].operation_index == 2, "operation plan assigns stable operation index");
    require(plan.operations[2].node_id == "packet-2", "operation plan preserves node id as render data");
    require(plan.operations[2].vertex_count == 4, "operation plan records four vertices");
    require(plan.operations[2].descriptor_set_count == 1, "operation plan preserves descriptor count");
    require(plan.operations[2].binding_count == 1, "operation plan preserves binding count");
    require(plan.operations[2].scissor.width == 8, "operation plan preserves packet scissor");
    for (const vulkan_backend::vulkan_command_recorder_operation_summary& operation :
         plan.operations) {
        require(operation.completed(), "operation summary is complete");
    }
}

void test_vulkan_command_recorder_operation_plan_blocks_without_packet_bridge()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge{
        .checked = true,
        .status = vulkan_backend::vulkan_command_packet_bridge_status::pipeline_unavailable,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        .blocked_batch_kind = vulkan_backend::vulkan_batch_kind::image,
        .blocked_command_index = 42,
    };
    const vulkan_backend::vulkan_command_packet_execution_result execution;
    const vulkan_backend::vulkan_command_recorder_operation_plan plan =
        vulkan_backend::build_vulkan_command_recorder_operation_plan(bridge, execution);

    require(plan.checked, "blocked operation plan is checked");
    require(!plan.completed(), "blocked operation plan does not complete");
    require(plan.blocked(), "blocked operation plan reports blocked");
    require(
        plan.status
            == vulkan_backend::vulkan_command_recorder_operation_plan_status::packet_bridge_unavailable,
        "operation plan records unavailable bridge prerequisite");
    require(
        plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        "operation plan preserves bridge fallback");
    require(plan.packet_bridge_checked, "operation plan records checked bridge");
    require(!plan.packet_bridge_ready, "operation plan records unavailable bridge");
    require(!plan.packet_execution_checked, "operation plan does not require execution after bridge failure");
    require(!plan.packet_execution_ready, "operation plan records missing execution");
    require(
        plan.blocked_category == vulkan_backend::vulkan_command_packet_category::image,
        "operation plan maps blocked batch kind to packet category");
    require(
        plan.blocked_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "operation plan preserves blocked batch kind");
    require(plan.blocked_command_index == 42, "operation plan preserves blocked command index");
    require(plan.operations.empty(), "blocked operation plan stores no operations");
}

void test_vulkan_command_recorder_operation_plan_blocks_on_execution_failure()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    vulkan_backend::fake_vulkan_command_packet_executor executor(
        vulkan_backend::fake_vulkan_command_packet_executor_options{
            .fail_packet = true,
            .fail_packet_index = 2,
        });
    const vulkan_backend::vulkan_command_packet_execution_result execution =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_command_recorder_operation_plan plan =
        vulkan_backend::build_vulkan_command_recorder_operation_plan(bridge, execution);

    require(plan.checked, "execution-blocked operation plan is checked");
    require(!plan.completed(), "execution-blocked operation plan does not complete");
    require(plan.blocked(), "execution-blocked operation plan reports blocked");
    require(
        plan.status
            == vulkan_backend::vulkan_command_recorder_operation_plan_status::packet_execution_unavailable,
        "operation plan records unavailable execution prerequisite");
    require(
        plan.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "operation plan maps packet execution failure to record fallback");
    require(plan.packet_bridge_ready, "operation plan records bridge ready before execution failure");
    require(plan.packet_execution_checked, "operation plan records checked execution");
    require(!plan.packet_execution_ready, "operation plan records unavailable execution");
    require(
        plan.blocked_category == vulkan_backend::vulkan_command_packet_category::image,
        "operation plan records failed packet category");
    require(
        plan.blocked_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "operation plan records failed packet batch kind");
    require(plan.blocked_packet_index == 2, "operation plan records failed packet index");
    require(plan.blocked_command_index == 12, "operation plan records failed command index");
    require(plan.operations.empty(), "execution-blocked operation plan stores no operations");
}

void test_vulkan_command_recorder_operation_plan_accepts_empty_packet_execution()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_empty_bridge();
    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_command_packet_execution_result execution =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_command_recorder_operation_plan plan =
        vulkan_backend::build_vulkan_command_recorder_operation_plan(bridge, execution);

    require(plan.checked, "empty operation plan is checked");
    require(plan.completed(), "empty operation plan completes");
    require(
        plan.status == vulkan_backend::vulkan_command_recorder_operation_plan_status::ready,
        "empty operation plan is ready");
    require(plan.planned_packet_count == 0, "empty operation plan records zero planned packets");
    require(plan.operation_count == 0, "empty operation plan records zero operations");
    require(plan.operations.empty(), "empty operation plan stores no operations");
}

} // namespace

int main()
{
    test_vulkan_command_recorder_operation_plan_names_are_stable();
    test_vulkan_command_recorder_operation_plan_converts_packets_in_order();
    test_vulkan_command_recorder_operation_plan_blocks_without_packet_bridge();
    test_vulkan_command_recorder_operation_plan_blocks_on_execution_failure();
    test_vulkan_command_recorder_operation_plan_accepts_empty_packet_execution();
    return 0;
}
