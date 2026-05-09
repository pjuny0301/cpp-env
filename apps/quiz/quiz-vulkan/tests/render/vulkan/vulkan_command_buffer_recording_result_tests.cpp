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
    test_vulkan_command_buffer_recording_result_records_successful_operations();
    test_vulkan_command_buffer_recording_result_blocks_without_operation_plan();
    test_vulkan_command_buffer_recording_result_blocks_without_command_buffer();
    test_vulkan_command_buffer_recording_result_records_first_operation_failure();
    test_vulkan_command_buffer_recording_result_accepts_empty_plan();
    return 0;
}
