#include "core/input/input_engine.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "input_engine_touch_arbitration_tests failed: " << message << '\n';
    std::exit(1);
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

quiz_vulkan::raw_platform_input_event pointer(
    quiz_vulkan::raw_platform_pointer_phase phase,
    std::int64_t timestamp_ms,
    float x,
    float y,
    quiz_vulkan::raw_platform_pointer_button button = quiz_vulkan::raw_platform_pointer_button::primary,
    std::int32_t pointer_id = 1)
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

void test_touch_like_multipointer_arbitration_diagnostics()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine captured_engine;
    require(captured_engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                100,
                0.0f,
                0.0f,
                raw_platform_pointer_button::none,
                1))
                .empty(),
        "touch arbitration first pointer down emits no input event");
    const action_route_policy_diagnostic& first_track_policy = require_policy(
        captured_engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_arbitration,
        "touch arbitration first pointer tracks");
    require(first_track_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch arbitration first pointer records touch-like contact");
    require(first_track_policy.tracked_pointer_count_before == 0,
        "touch arbitration first pointer records zero tracked before");
    require(first_track_policy.tracked_pointer_count_after == 1,
        "touch arbitration first pointer records one tracked after");
    require(first_track_policy.gesture_policy.decision == gesture_policy_decision::tracking_started,
        "touch arbitration first pointer records tracking gesture policy");

    std::vector<input_event> events = captured_engine.process_raw_event(pointer(
        raw_platform_pointer_phase::move,
        120,
        9.0f,
        0.0f,
        raw_platform_pointer_button::none,
        1));
    require(events.size() == 1, "touch arbitration first pointer starts drag");
    const action_route_policy_diagnostic& capture_policy = require_policy(
        captured_engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "touch arbitration drag start policy is emitted");
    require(capture_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch arbitration drag start records touch-like contact");
    require(capture_policy.pointer_decision == pointer_arbitration_decision::captured,
        "touch arbitration drag start records captured decision");
    require(capture_policy.tracked_pointer_count_before == 1,
        "touch arbitration drag start records one tracked before");
    require(capture_policy.tracked_pointer_count_after == 1,
        "touch arbitration drag start records one captured pointer after");
    require(capture_policy.gesture_policy.decision == gesture_policy_decision::drag_started,
        "touch arbitration drag start records gesture policy");
    require(capture_policy.gesture_policy.direction == gesture_direction::right,
        "touch arbitration drag start records direction");

    events = captured_engine.process_raw_event(pointer(
        raw_platform_pointer_phase::down,
        130,
        40.0f,
        0.0f,
        raw_platform_pointer_button::none,
        2));
    require(events.empty(), "touch arbitration second pointer down is ignored by capture");
    const action_route_policy_diagnostic& ignored_down_policy = require_policy(
        captured_engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_arbitration,
        "touch arbitration second pointer down emits arbitration policy");
    require(ignored_down_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch arbitration ignored down records touch-like contact");
    require(ignored_down_policy.pointer_decision == pointer_arbitration_decision::ignored_by_capture,
        "touch arbitration ignored down records ignored decision");
    require(ignored_down_policy.pointer_event_phase == pointer_phase::down,
        "touch arbitration ignored down records phase");
    require(ignored_down_policy.pointer_id == 2, "touch arbitration ignored down records second pointer id");
    require(ignored_down_policy.tracked_pointer_count_before == 1,
        "touch arbitration ignored down records captured pointer count before");
    require(ignored_down_policy.tracked_pointer_count_after == 1,
        "touch arbitration ignored down preserves captured pointer count after");
    require(ignored_down_policy.gesture_policy.decision == gesture_policy_decision::ignored_by_capture,
        "touch arbitration ignored down carries ignored gesture policy");
    require_capture_snapshot(
        ignored_down_policy.pointer_capture_before,
        pointer_capture_lifecycle::captured,
        true,
        1,
        1,
        "touch arbitration ignored down records first pointer captured before");
    require_capture_snapshot(
        ignored_down_policy.pointer_capture_after,
        pointer_capture_lifecycle::captured,
        true,
        1,
        1,
        "touch arbitration ignored down preserves first pointer capture");

    events = captured_engine.process_raw_event(pointer(
        raw_platform_pointer_phase::cancel,
        140,
        40.0f,
        0.0f,
        raw_platform_pointer_button::none,
        2));
    require(events.empty(), "touch arbitration second pointer cancel is ignored by capture");
    const action_route_policy_diagnostic& ignored_cancel_policy = require_policy(
        captured_engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_arbitration,
        "touch arbitration second pointer cancel emits arbitration policy");
    require(ignored_cancel_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch arbitration ignored cancel records touch-like contact");
    require(ignored_cancel_policy.pointer_decision == pointer_arbitration_decision::ignored_by_capture,
        "touch arbitration ignored cancel records ignored decision");
    require(ignored_cancel_policy.pointer_event_phase == pointer_phase::cancel,
        "touch arbitration ignored cancel records cancel phase");
    require(ignored_cancel_policy.gesture_policy.decision == gesture_policy_decision::ignored_by_capture,
        "touch arbitration ignored cancel carries ignored gesture policy");

    events = captured_engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        160,
        12.0f,
        0.0f,
        raw_platform_pointer_button::none,
        1));
    require(events.size() == 1, "touch arbitration captured pointer releases drag");
    const action_route_policy_diagnostic& release_policy = require_policy(
        captured_engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "touch arbitration captured release emits gesture policy");
    require(release_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch arbitration release records touch-like contact");
    require(release_policy.pointer_decision == pointer_arbitration_decision::released,
        "touch arbitration release records released decision");
    require(release_policy.tracked_pointer_count_before == 1,
        "touch arbitration release records one tracked before");
    require(release_policy.tracked_pointer_count_after == 0,
        "touch arbitration release records zero tracked after");
    require(release_policy.gesture_policy.decision == gesture_policy_decision::drag_released,
        "touch arbitration release records drag release policy");

    input_engine cancel_engine;
    require(cancel_engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                200,
                0.0f,
                0.0f,
                raw_platform_pointer_button::none,
                10))
                .empty(),
        "touch cancel first pointer down emits no event");
    require(cancel_engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                210,
                50.0f,
                0.0f,
                raw_platform_pointer_button::none,
                20))
                .empty(),
        "touch cancel second pointer down emits no event");
    const action_route_policy_diagnostic& second_track_policy = require_policy(
        cancel_engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_arbitration,
        "touch cancel second pointer emits tracking policy");
    require(second_track_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch cancel second pointer records touch-like contact");
    require(second_track_policy.tracked_pointer_count_before == 1,
        "touch cancel second pointer records one tracked before");
    require(second_track_policy.tracked_pointer_count_after == 2,
        "touch cancel second pointer records two tracked after");
    require_capture_snapshot(
        second_track_policy.pointer_capture_after,
        pointer_capture_lifecycle::tracking,
        false,
        10,
        2,
        "touch cancel second pointer preserves deterministic lowest pointer id");

    events = cancel_engine.process_raw_event(pointer(
        raw_platform_pointer_phase::cancel,
        220,
        0.0f,
        0.0f,
        raw_platform_pointer_button::none,
        10));
    require(events.empty(), "touch cancel first tracked pointer emits no input event");
    require(cancel_engine.routing_diagnostics().action_routes.size() == 2,
        "touch cancel emits route suppression and reset policies");
    const action_route_policy_diagnostic& cancel_route_policy = require_policy(
        cancel_engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "touch cancel first route records suppression");
    require(cancel_route_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch cancel route records touch-like contact");
    require(cancel_route_policy.pointer_decision == pointer_arbitration_decision::canceled,
        "touch cancel route records canceled decision");
    require(cancel_route_policy.tracked_pointer_count_before == 2,
        "touch cancel route records two tracked before");
    require(cancel_route_policy.tracked_pointer_count_after == 1,
        "touch cancel route records one tracked after");
    require(cancel_route_policy.gesture_policy.decision == gesture_policy_decision::release_suppressed,
        "touch cancel route records release suppression");
    const action_route_policy_diagnostic& cancel_reset_policy = require_policy(
        cancel_engine.routing_diagnostics(),
        1,
        action_route_policy_kind::pointer_capture_reset,
        "touch cancel reset policy is emitted");
    require(cancel_reset_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch cancel reset records touch-like contact");
    require(cancel_reset_policy.pointer_decision == pointer_arbitration_decision::canceled,
        "touch cancel reset records canceled decision");
    require(cancel_reset_policy.tracked_pointer_count_before == 2,
        "touch cancel reset records two tracked before");
    require(cancel_reset_policy.tracked_pointer_count_after == 1,
        "touch cancel reset records one tracked after");
    require_capture_snapshot(
        cancel_reset_policy.pointer_capture_after,
        pointer_capture_lifecycle::tracking,
        false,
        20,
        1,
        "touch cancel reset records remaining pointer deterministically");

    events = cancel_engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        240,
        51.0f,
        1.0f,
        raw_platform_pointer_button::none,
        20));
    require(events.size() == 1, "touch cancel remaining pointer can still tap");
    const action_route_policy_diagnostic& remaining_tap_policy = require_policy(
        cancel_engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "touch cancel remaining pointer emits tap route policy");
    require(remaining_tap_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch cancel remaining tap records touch-like contact");
    require(remaining_tap_policy.tracked_pointer_count_before == 1,
        "touch cancel remaining tap records one tracked before");
    require(remaining_tap_policy.tracked_pointer_count_after == 0,
        "touch cancel remaining tap records zero tracked after");
    require(remaining_tap_policy.gesture_policy.decision == gesture_policy_decision::tap_accepted,
        "touch cancel remaining tap records accepted tap");
}

void test_touch_like_swipe_threshold_route_diagnostics()
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
                42))
                .empty(),
        "touch swipe down emits no event");
    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        180,
        70.0f,
        0.0f,
        raw_platform_pointer_button::none,
        42));
    require(events.size() == 1, "touch swipe emits one gesture");
    const action_route_policy_diagnostic& swipe_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "touch swipe emits route policy");
    require(swipe_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch swipe records touch-like contact");
    require(swipe_policy.gesture_policy.decision == gesture_policy_decision::swipe_accepted,
        "touch swipe records accepted swipe");
    require(swipe_policy.gesture_policy.direction == gesture_direction::right,
        "touch swipe records right direction");
    require(swipe_policy.gesture_policy.distance == 70.0f,
        "touch swipe records distance");
    require(swipe_policy.gesture_policy.swipe_min_dx == 60.0f,
        "touch swipe records dx threshold");

    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                200,
                0.0f,
                0.0f,
                raw_platform_pointer_button::none,
                43))
                .empty(),
        "touch short swipe down emits no event");
    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        260,
        59.0f,
        0.0f,
        raw_platform_pointer_button::none,
        43));
    require(events.empty(), "touch short swipe emits no gesture");
    const action_route_policy_diagnostic& short_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "touch short swipe emits route policy");
    require(short_policy.pointer_contact == pointer_contact_kind::touch_like,
        "touch short swipe records touch-like contact");
    require(short_policy.gesture_policy.decision == gesture_policy_decision::swipe_rejected_distance,
        "touch short swipe records distance rejection");
    require(short_policy.gesture_policy.direction == gesture_direction::right,
        "touch short swipe records deterministic direction");
    require(short_policy.gesture_policy.delta_x == 59.0f,
        "touch short swipe records dx");
    require(short_policy.tracked_pointer_count_before == 1,
        "touch short swipe records one tracked before");
    require(short_policy.tracked_pointer_count_after == 0,
        "touch short swipe records zero tracked after");
}

} // namespace

int main()
{
    test_touch_like_multipointer_arbitration_diagnostics();
    test_touch_like_swipe_threshold_route_diagnostics();

    std::cout << "input_engine_touch_arbitration_tests passed\n";
    return 0;
}
