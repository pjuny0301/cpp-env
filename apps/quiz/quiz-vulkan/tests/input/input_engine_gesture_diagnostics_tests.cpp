#include "core/input/input_engine.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <variant>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "input_engine_gesture_diagnostics_tests failed: " << message << '\n';
    std::exit(1);
}

template <typename T>
const T& require_event(const std::vector<quiz_vulkan::input::input_event>& events, std::size_t index)
{
    require(index < events.size(), "event index exists");
    require(std::holds_alternative<T>(events[index]), "event has expected type");
    return std::get<T>(events[index]);
}

const quiz_vulkan::input::action_route_policy_diagnostic& require_policy(
    const quiz_vulkan::input::input_routing_diagnostics& diagnostics,
    std::size_t index,
    quiz_vulkan::input::action_route_policy_kind kind,
    const char* message)
{
    require(index < diagnostics.action_routes.size(), message);
    const quiz_vulkan::input::action_route_policy_diagnostic& policy = diagnostics.action_routes[index];
    require(policy.kind == kind, message);
    return policy;
}

void require_capture_snapshot(
    quiz_vulkan::input::pointer_capture_snapshot snapshot,
    quiz_vulkan::input::pointer_capture_lifecycle lifecycle,
    bool active,
    std::int32_t pointer_id,
    std::size_t tracked_pointer_count,
    const char* message)
{
    require(snapshot.lifecycle == lifecycle, message);
    require(snapshot.active == active, message);
    require(snapshot.pointer_id == pointer_id, message);
    require(snapshot.tracked_pointer_count == tracked_pointer_count, message);
}

quiz_vulkan::raw_platform_input_event pointer(
    quiz_vulkan::raw_platform_pointer_phase phase,
    std::int64_t timestamp_ms,
    float x,
    float y,
    quiz_vulkan::raw_platform_pointer_button button,
    std::int32_t pointer_id)
{
    return quiz_vulkan::raw_platform_pointer_event{
        .timestamp_ms = timestamp_ms,
        .pointer_id = pointer_id,
        .phase = phase,
        .button = button,
        .x = x,
        .y = y,
    };
}

quiz_vulkan::raw_platform_input_event platform_scroll(
    std::int64_t timestamp_ms,
    float x,
    float y,
    float delta_x,
    float delta_y,
    quiz_vulkan::raw_platform_scroll_delta_unit unit)
{
    return quiz_vulkan::raw_platform_scroll_event{
        .timestamp_ms = timestamp_ms,
        .x = x,
        .y = y,
        .delta_x = delta_x,
        .delta_y = delta_y,
        .unit = unit,
    };
}

quiz_vulkan::input::raw_scroll_event scroll(
    std::int64_t timestamp_ms,
    float x,
    float y,
    float delta_x,
    float delta_y,
    quiz_vulkan::input::scroll_delta_unit unit)
{
    return quiz_vulkan::input::raw_scroll_event{
        .timestamp_ms = timestamp_ms,
        .x = x,
        .y = y,
        .delta_x = delta_x,
        .delta_y = delta_y,
        .unit = unit,
    };
}

void require_swipe_parity(
    quiz_vulkan::raw_platform_pointer_button button,
    quiz_vulkan::input::pointer_contact_kind contact,
    std::int32_t pointer_id,
    const char* label)
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                100,
                4.0f,
                8.0f,
                button,
                pointer_id))
                .empty(),
        label);
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::tracking,
        false,
        pointer_id,
        1,
        "swipe parity down tracks pointer");

    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        340,
        84.0f,
        18.0f,
        button,
        pointer_id));
    require(events.size() == 1, "swipe parity emits one event");
    const gesture_event& swipe = require_event<gesture_event>(events, 0);
    require(swipe.kind == gesture_kind::swipe_right, "swipe parity emits right swipe");
    require(swipe.pointer_id == pointer_id, "swipe parity preserves pointer id");
    require(swipe.duration_ms == 240, "swipe parity preserves duration");
    require(swipe.start_x == 4.0f, "swipe parity preserves start x");
    require(swipe.x == 84.0f, "swipe parity preserves end x");

    const input_routing_diagnostics& diagnostics = engine.routing_diagnostics();
    require(diagnostics.normalized_events.size() == 1, "swipe parity emits one normalized summary");
    const normalized_input_event_summary& summary = diagnostics.normalized_events[0];
    require(summary.kind == input_event_summary_kind::swipe_right, "swipe parity summary records kind");
    require(summary.pointer_id == pointer_id, "swipe parity summary records pointer id");
    require(summary.duration_ms == 240, "swipe parity summary records duration");
    require(summary.start_x == 4.0f, "swipe parity summary records start x");
    require(summary.x == 84.0f, "swipe parity summary records end x");

    const action_route_policy_diagnostic& policy = require_policy(
        diagnostics,
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "swipe parity route policy is emitted");
    require(policy.emits_input_event, "swipe parity route emits input event");
    require(policy.event_index == 0, "swipe parity route points at emitted event");
    require(policy.pointer_contact == contact, "swipe parity route records contact kind");
    require(policy.pointer_id == pointer_id, "swipe parity route records pointer id");
    require(policy.pointer_event_phase == pointer_phase::up, "swipe parity route records up phase");
    require(policy.tracked_pointer_count_before == 1, "swipe parity route records tracked before");
    require(policy.tracked_pointer_count_after == 0, "swipe parity route records cleared after");
    require(policy.normalized_event.kind == input_event_summary_kind::swipe_right,
        "swipe parity route carries normalized summary");
    require(policy.normalized_event.pointer_id == pointer_id,
        "swipe parity route carries normalized pointer id");
    require(policy.gesture_policy.decision == gesture_policy_decision::swipe_accepted,
        "swipe parity route records accepted classifier decision");
    require(policy.gesture_policy.pointer_id == pointer_id,
        "swipe parity gesture policy records pointer id");
    require(policy.gesture_policy.direction == gesture_direction::right,
        "swipe parity gesture policy records direction");
    require(policy.gesture_policy.delta_x == 80.0f, "swipe parity gesture policy records dx");
    require(policy.gesture_policy.delta_y == 10.0f, "swipe parity gesture policy records dy");
    require(policy.gesture_policy.duration_ms == 240,
        "swipe parity gesture policy records classifier duration");
    require(policy.gesture_policy.swipe_min_dx == 60.0f,
        "swipe parity gesture policy records dx threshold");
    require(policy.gesture_policy.swipe_max_dy == 40.0f,
        "swipe parity gesture policy records dy threshold");
    require(policy.gesture_policy.swipe_max_duration_ms == 800,
        "swipe parity gesture policy records duration threshold");
    require_capture_snapshot(
        policy.pointer_capture_before,
        pointer_capture_lifecycle::tracking,
        false,
        pointer_id,
        1,
        "swipe parity route records tracked pointer before release");
    require_capture_snapshot(
        policy.pointer_capture_after,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "swipe parity route records idle pointer after release");
}

void test_mouse_and_touch_swipe_classifier_diagnostics_match()
{
    require_swipe_parity(
        quiz_vulkan::raw_platform_pointer_button::primary,
        quiz_vulkan::input::pointer_contact_kind::mouse_like,
        11,
        "mouse swipe down emits no event");
    require_swipe_parity(
        quiz_vulkan::raw_platform_pointer_button::none,
        quiz_vulkan::input::pointer_contact_kind::touch_like,
        12,
        "touch swipe down emits no event");
}

void test_custom_swipe_threshold_rejections_are_diagnostic_only()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    gesture_thresholds thresholds{
        .swipe_min_dx = 80.0f,
        .swipe_max_dy = 10.0f,
        .swipe_max_duration_ms = 300,
        .long_press_min_duration_ms = 600,
        .tap_slop = 8.0f,
        .drag_start_slop = 8.0f,
    };
    input_engine engine(thresholds);

    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                100,
                0.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                21))
                .empty(),
        "custom short swipe down emits no event");
    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        220,
        79.0f,
        0.0f,
        raw_platform_pointer_button::primary,
        21));
    require(events.empty(), "custom short swipe emits no input event");
    require(engine.routing_diagnostics().normalized_events.empty(),
        "custom short swipe emits no normalized summary");
    const action_route_policy_diagnostic& short_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "custom short swipe emits diagnostic route");
    require(!short_policy.emits_input_event, "custom short swipe route emits no input");
    require(short_policy.gesture_policy.decision == gesture_policy_decision::swipe_rejected_distance,
        "custom short swipe records distance rejection");
    require(short_policy.gesture_policy.delta_x == 79.0f,
        "custom short swipe records dx");
    require(short_policy.gesture_policy.swipe_min_dx == 80.0f,
        "custom short swipe records custom dx threshold");

    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                300,
                0.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                22))
                .empty(),
        "custom cross-axis swipe down emits no event");
    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        420,
        80.0f,
        11.0f,
        raw_platform_pointer_button::primary,
        22));
    require(events.empty(), "custom cross-axis swipe emits no input event");
    const action_route_policy_diagnostic& cross_axis_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "custom cross-axis swipe emits diagnostic route");
    require(cross_axis_policy.gesture_policy.decision == gesture_policy_decision::swipe_rejected_cross_axis,
        "custom cross-axis swipe records cross-axis rejection");
    require(cross_axis_policy.gesture_policy.delta_y == 11.0f,
        "custom cross-axis swipe records dy");
    require(cross_axis_policy.gesture_policy.swipe_max_dy == 10.0f,
        "custom cross-axis swipe records custom dy threshold");

    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                500,
                0.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                23))
                .empty(),
        "custom slow swipe down emits no event");
    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        801,
        80.0f,
        0.0f,
        raw_platform_pointer_button::primary,
        23));
    require(events.empty(), "custom slow swipe emits no input event");
    const action_route_policy_diagnostic& slow_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "custom slow swipe emits diagnostic route");
    require(slow_policy.gesture_policy.decision == gesture_policy_decision::swipe_rejected_duration,
        "custom slow swipe records duration rejection");
    require(slow_policy.gesture_policy.duration_ms == 301,
        "custom slow swipe records duration");
    require(slow_policy.gesture_policy.swipe_max_duration_ms == 300,
        "custom slow swipe records custom duration threshold");

    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                900,
                0.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                24))
                .empty(),
        "custom boundary swipe down emits no event");
    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        1200,
        80.0f,
        10.0f,
        raw_platform_pointer_button::primary,
        24));
    require(events.size() == 1, "custom boundary swipe emits one input event");
    const gesture_event& boundary_swipe = require_event<gesture_event>(events, 0);
    require(boundary_swipe.kind == gesture_kind::swipe_right,
        "custom boundary swipe accepts exact thresholds");
    const action_route_policy_diagnostic& boundary_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "custom boundary swipe emits route policy");
    require(boundary_policy.gesture_policy.decision == gesture_policy_decision::swipe_accepted,
        "custom boundary swipe records accepted decision");
    require(boundary_policy.gesture_policy.delta_x == 80.0f,
        "custom boundary swipe records exact dx");
    require(boundary_policy.gesture_policy.delta_y == 10.0f,
        "custom boundary swipe records exact dy");
    require(boundary_policy.gesture_policy.duration_ms == 300,
        "custom boundary swipe records exact duration");
}

void test_touch_drag_cancel_diagnostics_keep_pointer_id_consistent()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                100,
                10.0f,
                20.0f,
                raw_platform_pointer_button::none,
                31))
                .empty(),
        "touch drag down emits no event");

    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::move,
        130,
        19.0f,
        20.0f,
        raw_platform_pointer_button::none,
        31));
    require(events.size() == 1, "touch drag start emits one event");
    const gesture_event& start = require_event<gesture_event>(events, 0);
    require(start.kind == gesture_kind::drag_start, "touch drag start kind is emitted");
    require(start.pointer_id == 31, "touch drag start preserves pointer id");
    require(start.delta_x == 9.0f, "touch drag start summary delta is from down");
    const action_route_policy_diagnostic& start_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "touch drag start emits route policy");
    require(start_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch drag start records touch contact");
    require(start_policy.pointer_decision == pointer_arbitration_decision::captured,
        "touch drag start records captured decision");
    require(start_policy.pointer_id == 31, "touch drag start route records pointer id");
    require(start_policy.normalized_event.pointer_id == 31,
        "touch drag start normalized route records pointer id");
    require(start_policy.gesture_policy.pointer_id == 31,
        "touch drag start gesture policy records pointer id");
    require(start_policy.gesture_policy.decision == gesture_policy_decision::drag_started,
        "touch drag start gesture policy records classifier decision");
    require(start_policy.gesture_policy.delta_x == 9.0f,
        "touch drag start gesture policy records start-relative dx");
    require_capture_snapshot(
        start_policy.pointer_capture_after,
        pointer_capture_lifecycle::captured,
        true,
        31,
        1,
        "touch drag start captures pointer");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::move,
        150,
        25.0f,
        24.0f,
        raw_platform_pointer_button::none,
        31));
    require(events.size() == 1, "touch drag update emits one event");
    const gesture_event& update = require_event<gesture_event>(events, 0);
    require(update.kind == gesture_kind::drag_update, "touch drag update kind is emitted");
    require(update.pointer_id == 31, "touch drag update preserves pointer id");
    require(update.delta_x == 6.0f, "touch drag update event delta is previous-relative");
    require(update.delta_y == 4.0f, "touch drag update event y delta is previous-relative");
    const action_route_policy_diagnostic& update_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "touch drag update emits route policy");
    require(update_policy.pointer_decision == pointer_arbitration_decision::none,
        "touch drag update keeps capture without new arbitration decision");
    require(update_policy.gesture_policy.delta_x == 15.0f,
        "touch drag update classifier dx remains start-relative");
    require(update_policy.normalized_event.delta_x == 6.0f,
        "touch drag update normalized dx is previous-relative");
    require(update_policy.pointer_id == 31, "touch drag update route records pointer id");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::cancel,
        170,
        28.0f,
        26.0f,
        raw_platform_pointer_button::none,
        31));
    require(events.size() == 1, "touch drag cancel emits one event");
    const gesture_event& cancel = require_event<gesture_event>(events, 0);
    require(cancel.kind == gesture_kind::drag_cancel, "touch drag cancel kind is emitted");
    require(cancel.pointer_id == 31, "touch drag cancel preserves pointer id");
    require(cancel.delta_x == 3.0f, "touch drag cancel event dx is previous-relative");
    require(cancel.delta_y == 2.0f, "touch drag cancel event dy is previous-relative");
    require(engine.routing_diagnostics().action_routes.size() == 2,
        "touch drag cancel emits route and reset policies");
    const action_route_policy_diagnostic& cancel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "touch drag cancel route policy is emitted");
    require(cancel_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch drag cancel records touch contact");
    require(cancel_policy.pointer_decision == pointer_arbitration_decision::canceled,
        "touch drag cancel records canceled decision");
    require(cancel_policy.pointer_id == 31, "touch drag cancel route records pointer id");
    require(cancel_policy.normalized_event.pointer_id == 31,
        "touch drag cancel normalized route records pointer id");
    require(cancel_policy.gesture_policy.pointer_id == 31,
        "touch drag cancel gesture policy records pointer id");
    require(cancel_policy.gesture_policy.decision == gesture_policy_decision::drag_canceled,
        "touch drag cancel records canceled classifier decision");
    require(cancel_policy.gesture_policy.delta_x == 18.0f,
        "touch drag cancel classifier dx remains start-relative");
    const action_route_policy_diagnostic& reset_policy = require_policy(
        engine.routing_diagnostics(),
        1,
        action_route_policy_kind::pointer_capture_reset,
        "touch drag cancel reset policy is emitted");
    require(reset_policy.pointer_id == 31, "touch drag cancel reset records pointer id");
    require(reset_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch drag cancel reset records touch contact");
    require(reset_policy.gesture_policy.decision == gesture_policy_decision::drag_canceled,
        "touch drag cancel reset carries classifier decision");
    require_capture_snapshot(
        reset_policy.pointer_capture_before,
        pointer_capture_lifecycle::captured,
        true,
        31,
        1,
        "touch drag cancel reset records captured pointer before");
    require_capture_snapshot(
        reset_policy.pointer_capture_after,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "touch drag cancel reset records idle pointer after");
}

void test_wheel_after_touch_cancel_has_idle_capture_context()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                100,
                0.0f,
                0.0f,
                raw_platform_pointer_button::none,
                41))
                .empty(),
        "wheel after cancel touch down emits no event");
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::move,
                120,
                10.0f,
                0.0f,
                raw_platform_pointer_button::none,
                41))
                .size() == 1,
        "wheel after cancel touch drag captures pointer");

    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::cancel,
        140,
        12.0f,
        0.0f,
        raw_platform_pointer_button::none,
        41));
    require(events.size() == 1, "wheel after cancel touch cancel emits drag cancel");
    require(engine.routing_diagnostics().action_routes.size() == 2,
        "wheel after cancel touch cancel emits route and reset policies");
    const action_route_policy_diagnostic& cancel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "wheel after cancel gesture route is emitted");
    require(cancel_policy.pointer_contact == pointer_contact_kind::touch_like,
        "wheel after cancel gesture route records touch contact");
    require(cancel_policy.pointer_decision == pointer_arbitration_decision::canceled,
        "wheel after cancel gesture route records canceled decision");
    require(cancel_policy.tracked_pointer_count_before == 1,
        "wheel after cancel gesture route records tracked pointer before");
    require(cancel_policy.tracked_pointer_count_after == 0,
        "wheel after cancel gesture route records no tracked pointer after");
    const action_route_policy_diagnostic& reset_policy = require_policy(
        engine.routing_diagnostics(),
        1,
        action_route_policy_kind::pointer_capture_reset,
        "wheel after cancel reset route is emitted");
    require(reset_policy.pointer_contact == pointer_contact_kind::touch_like,
        "wheel after cancel reset route records touch contact");
    require(reset_policy.tracked_pointer_count_before == 1,
        "wheel after cancel reset route records tracked pointer before");
    require(reset_policy.tracked_pointer_count_after == 0,
        "wheel after cancel reset route records no tracked pointer after");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "wheel after cancel leaves diagnostics idle after cancel");
    const input_diagnostic_summary& cancel_summary = engine.routing_diagnostics().summary;
    require(cancel_summary.normalized_event_count == 1, "wheel after cancel summary counts drag cancel event");
    require(cancel_summary.normalized_events.drag_cancel == 1, "wheel after cancel summary counts drag cancel kind");
    require(cancel_summary.routes.pointer == 2, "wheel after cancel summary counts gesture and reset routes");
    require(cancel_summary.routes.total == 2, "wheel after cancel summary counts all cancel routes");
    require(cancel_summary.pointer_capture_ended_cleanly, "wheel after cancel summary clears capture");
    require(cancel_summary.focus_ended_cleanly, "wheel after cancel summary leaves focus clean");
    require(cancel_summary.preedit_ended_cleanly, "wheel after cancel summary leaves preedit clean");

    events = engine.process_scroll_event(scroll(
        160,
        20.0f,
        30.0f,
        0.0f,
        -2.0f,
        scroll_delta_unit::lines));
    require(events.size() == 1, "wheel after cancel emits one wheel event");
    const action_route_policy_diagnostic& wheel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::wheel_summary,
        "wheel after cancel emits wheel summary policy");
    require(wheel_policy.normalized_event.kind == input_event_summary_kind::wheel,
        "wheel after cancel policy carries wheel summary");
    require(wheel_policy.pointer_decision == pointer_arbitration_decision::none,
        "wheel after cancel policy has no pointer arbitration");
    require(wheel_policy.pointer_contact == pointer_contact_kind::unknown,
        "wheel after cancel policy has no pointer contact semantics");
    require(wheel_policy.tracked_pointer_count_before == 0,
        "wheel after cancel policy records no tracked pointers before");
    require(wheel_policy.tracked_pointer_count_after == 0,
        "wheel after cancel policy records no tracked pointers after");
    require_capture_snapshot(
        wheel_policy.pointer_capture_before,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "wheel after cancel policy records idle capture before");
    require_capture_snapshot(
        wheel_policy.pointer_capture_after,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "wheel after cancel policy records idle capture after");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "wheel after cancel leaves engine capture idle");
    const input_diagnostic_summary& wheel_summary = engine.routing_diagnostics().summary;
    require(wheel_summary.normalized_event_count == 1, "wheel after cancel summary counts wheel event");
    require(wheel_summary.normalized_events.wheel == 1, "wheel after cancel summary counts wheel kind");
    require(wheel_summary.routes.wheel == 1, "wheel after cancel summary counts wheel route");
    require(wheel_summary.routes.pointer == 0, "wheel after cancel summary has no pointer route");
    require(wheel_summary.routes.total == 1, "wheel after cancel summary counts one route");
    require(wheel_summary.pointer_capture_ended_cleanly, "wheel after cancel summary keeps capture clean");
    require(wheel_summary.focus_ended_cleanly, "wheel after cancel summary keeps focus clean");
    require(wheel_summary.preedit_ended_cleanly, "wheel after cancel summary keeps preedit clean");
}

void require_release_restart_parity(
    quiz_vulkan::raw_platform_pointer_button button,
    quiz_vulkan::input::pointer_contact_kind contact,
    std::int32_t pointer_id)
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine release_engine;
    require(release_engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                100,
                0.0f,
                0.0f,
                button,
                pointer_id))
                .empty(),
        "release parity down emits no event");
    require(release_engine.process_raw_event(pointer(
                raw_platform_pointer_phase::move,
                120,
                9.0f,
                0.0f,
                button,
                pointer_id))
                .size() == 1,
        "release parity move captures pointer");
    std::vector<input_event> events = release_engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        140,
        14.0f,
        0.0f,
        button,
        pointer_id));
    require(events.size() == 1, "release parity up emits drag end");
    const gesture_event& end = require_event<gesture_event>(events, 0);
    require(end.kind == gesture_kind::drag_end, "release parity emits drag end");
    require(end.pointer_id == pointer_id, "release parity event preserves pointer id");
    const action_route_policy_diagnostic& release_policy = require_policy(
        release_engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "release parity route is emitted");
    require(release_policy.pointer_decision == pointer_arbitration_decision::released,
        "release parity records released decision");
    require(release_policy.pointer_contact == contact, "release parity records contact kind");
    require(release_policy.pointer_id == pointer_id, "release parity route records pointer id");
    require(release_policy.tracked_pointer_count_before == 1,
        "release parity route records one tracked pointer before");
    require(release_policy.tracked_pointer_count_after == 0,
        "release parity route records no tracked pointer after");
    require_capture_snapshot(
        release_policy.pointer_capture_before,
        pointer_capture_lifecycle::captured,
        true,
        pointer_id,
        1,
        "release parity records captured pointer before");
    require_capture_snapshot(
        release_policy.pointer_capture_after,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "release parity records idle pointer after");
    const input_diagnostic_summary& release_summary = release_engine.routing_diagnostics().summary;
    require(release_summary.normalized_event_count == 1, "release parity summary counts drag end event");
    require(release_summary.normalized_events.drag_end == 1, "release parity summary counts drag end kind");
    require(release_summary.routes.pointer == 1, "release parity summary counts pointer route");
    require(release_summary.routes.total == 1, "release parity summary counts one route");
    require(release_summary.pointer_capture_ended_cleanly, "release parity summary clears pointer capture");
    require(release_summary.preedit_ended_cleanly, "release parity summary has clean preedit");

    input_engine restart_engine;
    require(restart_engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                200,
                0.0f,
                0.0f,
                button,
                pointer_id))
                .empty(),
        "restart parity down emits no event");
    require(restart_engine.process_raw_event(pointer(
                raw_platform_pointer_phase::move,
                220,
                10.0f,
                0.0f,
                button,
                pointer_id))
                .size() == 1,
        "restart parity move captures pointer");
    events = restart_engine.process_raw_event(pointer(
        raw_platform_pointer_phase::down,
        240,
        2.0f,
        2.0f,
        button,
        pointer_id));
    require(events.size() == 1, "restart parity repeated down emits drag cancel");
    const gesture_event& cancel = require_event<gesture_event>(events, 0);
    require(cancel.kind == gesture_kind::drag_cancel, "restart parity emits drag cancel");
    require(cancel.pointer_id == pointer_id, "restart parity event preserves pointer id");
    const action_route_policy_diagnostic& restart_policy = require_policy(
        restart_engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "restart parity route is emitted");
    require(restart_policy.pointer_decision == pointer_arbitration_decision::restarted,
        "restart parity records restarted decision");
    require(restart_policy.pointer_contact == contact, "restart parity records contact kind");
    require(restart_policy.pointer_id == pointer_id, "restart parity route records pointer id");
    require(restart_policy.tracked_pointer_count_before == 1,
        "restart parity route records one tracked pointer before");
    require(restart_policy.tracked_pointer_count_after == 1,
        "restart parity route records replacement tracked pointer after");
    require(restart_policy.normalized_event.kind == input_event_summary_kind::drag_cancel,
        "restart parity route carries normalized drag cancel");
    require(restart_policy.gesture_policy.decision == gesture_policy_decision::drag_canceled,
        "restart parity route records drag canceled classifier decision");
    require_capture_snapshot(
        restart_policy.pointer_capture_before,
        pointer_capture_lifecycle::captured,
        true,
        pointer_id,
        1,
        "restart parity records captured pointer before");
    require_capture_snapshot(
        restart_policy.pointer_capture_after,
        pointer_capture_lifecycle::tracking,
        false,
        pointer_id,
        1,
        "restart parity records replacement tracking after");
}

void test_pointer_capture_release_and_restart_are_deterministic_for_mouse_and_touch()
{
    require_release_restart_parity(
        quiz_vulkan::raw_platform_pointer_button::primary,
        quiz_vulkan::input::pointer_contact_kind::mouse_like,
        51);
    require_release_restart_parity(
        quiz_vulkan::raw_platform_pointer_button::none,
        quiz_vulkan::input::pointer_contact_kind::touch_like,
        52);
}

void test_long_press_timing_and_policy_order_are_deterministic()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                100,
                90.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                9))
                .empty(),
        "long press high pointer down emits no event");
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                150,
                10.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                1))
                .empty(),
        "long press low pointer down emits no event");

    require(engine.update_time(699).empty(), "long press emits nothing before first threshold");
    require(engine.routing_diagnostics().normalized_events.empty(),
        "pre-threshold long press emits no normalized summary");
    require(engine.routing_diagnostics().action_routes.empty(),
        "pre-threshold long press emits no action route");

    std::vector<input_event> events = engine.update_time(700);
    require(events.size() == 1, "first long press threshold emits one event");
    const gesture_event& high_press = require_event<gesture_event>(events, 0);
    require(high_press.kind == gesture_kind::long_press, "first threshold emits long press");
    require(high_press.pointer_id == 9, "first threshold emits older high pointer id");
    require(high_press.duration_ms == 600, "first threshold duration is exact");
    const input_routing_diagnostics& high_diagnostics = engine.routing_diagnostics();
    require(high_diagnostics.normalized_events.size() == 1,
        "first threshold emits one normalized summary");
    require(high_diagnostics.normalized_events[0].kind == input_event_summary_kind::long_press,
        "first threshold summary kind is long press");
    require(high_diagnostics.normalized_events[0].pointer_id == 9,
        "first threshold summary records pointer id");
    const action_route_policy_diagnostic& high_policy = require_policy(
        high_diagnostics,
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "first threshold emits long press route");
    require(high_policy.gesture_policy.decision == gesture_policy_decision::long_press_accepted,
        "first threshold records accepted long press");
    require(high_policy.gesture_policy.pointer_id == 9,
        "first threshold gesture policy records pointer id");
    require(high_policy.gesture_policy.duration_ms == 600,
        "first threshold gesture policy records duration");
    require(high_policy.normalized_event.pointer_id == 9,
        "first threshold route carries normalized pointer id");
    require_capture_snapshot(
        high_policy.pointer_capture_before,
        pointer_capture_lifecycle::tracking,
        false,
        1,
        2,
        "first threshold route records deterministic capture snapshot before");
    require_capture_snapshot(
        high_policy.pointer_capture_after,
        pointer_capture_lifecycle::tracking,
        false,
        1,
        2,
        "first threshold route records deterministic capture snapshot after");

    require(engine.update_time(749).empty(), "second pointer emits nothing one millisecond before threshold");
    events = engine.update_time(750);
    require(events.size() == 1, "second long press threshold emits one event");
    const gesture_event& low_press = require_event<gesture_event>(events, 0);
    require(low_press.kind == gesture_kind::long_press, "second threshold emits long press");
    require(low_press.pointer_id == 1, "second threshold emits low pointer id");
    require(low_press.duration_ms == 600, "second threshold duration is exact");
    const action_route_policy_diagnostic& low_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "second threshold emits long press route");
    require(low_policy.gesture_policy.pointer_id == 1,
        "second threshold gesture policy records pointer id");
    require(low_policy.normalized_event.pointer_id == 1,
        "second threshold route carries normalized pointer id");

    require(engine.update_time(900).empty(), "long press updates do not duplicate");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        920,
        90.0f,
        0.0f,
        raw_platform_pointer_button::primary,
        9));
    require(events.empty(), "long-pressed high pointer release is suppressed");
    const action_route_policy_diagnostic& high_release_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "long-pressed high release emits suppression route");
    require(high_release_policy.gesture_policy.decision == gesture_policy_decision::release_suppressed,
        "long-pressed high release records suppression");
    require(high_release_policy.gesture_policy.pointer_id == 9,
        "long-pressed high release policy records pointer id");
    require(high_release_policy.tracked_pointer_count_before == 2,
        "long-pressed high release records two tracked before");
    require(high_release_policy.tracked_pointer_count_after == 1,
        "long-pressed high release records one tracked after");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        930,
        10.0f,
        0.0f,
        raw_platform_pointer_button::primary,
        1));
    require(events.empty(), "long-pressed low pointer release is suppressed");
    const action_route_policy_diagnostic& low_release_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "long-pressed low release emits suppression route");
    require(low_release_policy.gesture_policy.decision == gesture_policy_decision::release_suppressed,
        "long-pressed low release records suppression");
    require(low_release_policy.gesture_policy.pointer_id == 1,
        "long-pressed low release policy records pointer id");
    require(low_release_policy.tracked_pointer_count_before == 1,
        "long-pressed low release records one tracked before");
    require(low_release_policy.tracked_pointer_count_after == 0,
        "long-pressed low release records zero tracked after");
}

void test_wheel_delta_normalization_updates_summaries_and_action_routes()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    std::vector<input_event> events = engine.process_scroll_event(scroll(
        100,
        12.0f,
        24.0f,
        3.0f,
        -4.0f,
        scroll_delta_unit::lines));
    require(events.size() == 1, "direct line wheel emits one event");
    const scroll_event& direct_line = require_event<scroll_event>(events, 0);
    require(direct_line.line_delta_x == 3.0f, "direct line wheel preserves x line delta");
    require(direct_line.line_delta_y == -4.0f, "direct line wheel preserves y line delta");
    require(direct_line.pixel_delta_x == 0.0f, "direct line wheel keeps pixel x zero");
    require(direct_line.pixel_delta_y == 0.0f, "direct line wheel keeps pixel y zero");
    const action_route_policy_diagnostic& direct_line_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::wheel_summary,
        "direct line wheel emits summary route");
    require(direct_line_policy.normalized_event.kind == input_event_summary_kind::wheel,
        "direct line wheel route carries wheel summary");
    require(direct_line_policy.normalized_event.line_delta_x == 3.0f,
        "direct line wheel route preserves x line delta");
    require(direct_line_policy.normalized_event.line_delta_y == -4.0f,
        "direct line wheel route preserves y line delta");
    require(direct_line_policy.normalized_event.pixel_delta_x == 0.0f,
        "direct line wheel route keeps pixel x zero");
    require(direct_line_policy.normalized_event.pixel_delta_y == 0.0f,
        "direct line wheel route keeps pixel y zero");

    events = engine.process_raw_event(platform_scroll(
        120,
        40.0f,
        50.0f,
        7.5f,
        -15.0f,
        raw_platform_scroll_delta_unit::pixels));
    require(events.size() == 1, "raw pixel wheel emits one event");
    const scroll_event& raw_pixel = require_event<scroll_event>(events, 0);
    require(raw_pixel.pixel_delta_x == 7.5f, "raw pixel wheel preserves x pixel delta");
    require(raw_pixel.pixel_delta_y == -15.0f, "raw pixel wheel preserves y pixel delta");
    require(raw_pixel.line_delta_x == 0.0f, "raw pixel wheel keeps line x zero");
    require(raw_pixel.line_delta_y == 0.0f, "raw pixel wheel keeps line y zero");
    const input_routing_diagnostics& raw_pixel_diagnostics = engine.routing_diagnostics();
    require(raw_pixel_diagnostics.normalized_events.size() == 1,
        "raw pixel wheel emits one normalized summary");
    require(raw_pixel_diagnostics.normalized_events[0].pixel_delta_x == 7.5f,
        "raw pixel wheel normalized summary preserves x pixel delta");
    require(raw_pixel_diagnostics.normalized_events[0].pixel_delta_y == -15.0f,
        "raw pixel wheel normalized summary preserves y pixel delta");
    const action_route_policy_diagnostic& raw_pixel_policy = require_policy(
        raw_pixel_diagnostics,
        0,
        action_route_policy_kind::wheel_summary,
        "raw pixel wheel emits summary route");
    require(raw_pixel_policy.emits_input_event, "raw pixel wheel route emits input event");
    require(raw_pixel_policy.event_index == 0, "raw pixel wheel route points at event index");
    require(raw_pixel_policy.normalized_event.pixel_delta_x == 7.5f,
        "raw pixel wheel route preserves x pixel delta");
    require(raw_pixel_policy.normalized_event.line_delta_x == 0.0f,
        "raw pixel wheel route keeps line x zero");

    events = engine.process_raw_event(platform_scroll(
        130,
        40.0f,
        50.0f,
        0.0f,
        0.0f,
        raw_platform_scroll_delta_unit::pixels));
    require(events.empty(), "raw zero wheel emits no event");
    require(engine.routing_diagnostics().normalized_events.empty(),
        "raw zero wheel clears previous normalized summary");
    require(engine.routing_diagnostics().action_routes.empty(),
        "raw zero wheel emits no stale action route");
}

} // namespace

int main()
{
    test_mouse_and_touch_swipe_classifier_diagnostics_match();
    test_custom_swipe_threshold_rejections_are_diagnostic_only();
    test_touch_drag_cancel_diagnostics_keep_pointer_id_consistent();
    test_wheel_after_touch_cancel_has_idle_capture_context();
    test_pointer_capture_release_and_restart_are_deterministic_for_mouse_and_touch();
    test_long_press_timing_and_policy_order_are_deterministic();
    test_wheel_delta_normalization_updates_summaries_and_action_routes();

    std::cout << "input_engine_gesture_diagnostics_tests passed\n";
    return 0;
}
