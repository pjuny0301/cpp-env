#include "core/input/gesture_recognizer.h"

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

    std::cerr << "gesture_recognizer_tests failed: " << message << '\n';
    std::exit(1);
}

quiz_vulkan::input::pointer_event pointer(
    quiz_vulkan::input::pointer_phase phase,
    std::int64_t timestamp_ms,
    float x,
    float y,
    std::int32_t pointer_id = 1)
{
    return quiz_vulkan::input::pointer_event{
        .timestamp_ms = timestamp_ms,
        .pointer_id = pointer_id,
        .phase = phase,
        .x = x,
        .y = y,
    };
}

void require_empty(const std::vector<quiz_vulkan::input::gesture_event>& gestures, const char* message)
{
    require(gestures.empty(), message);
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

const quiz_vulkan::input::gesture_policy_snapshot& require_single_policy(
    const quiz_vulkan::input::gesture_recognizer& recognizer,
    quiz_vulkan::input::gesture_policy_decision decision,
    const char* message)
{
    const std::vector<quiz_vulkan::input::gesture_policy_snapshot>& snapshots = recognizer.policy_snapshots();
    require(snapshots.size() == 1, message);
    require(snapshots[0].decision == decision, message);
    return snapshots[0];
}

void test_swipe_thresholds()
{
    using namespace quiz_vulkan::input;

    gesture_recognizer recognizer;
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 10.0f, 10.0f)),
        "down emits no gesture");

    std::vector<gesture_event> gestures =
        recognizer.process_pointer_event(pointer(pointer_phase::up, 400, 70.0f, 50.0f));
    require(gestures.size() == 1, "threshold swipe emits one gesture");
    require(gestures[0].kind == gesture_kind::swipe_right, "dx 60 dy 40 is swipe right");
    require(gestures[0].duration_ms == 300, "swipe duration is preserved");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 1000, 70.0f, 20.0f)),
        "second down emits no gesture");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 1200, 9.0f, 60.0f));
    require(gestures.size() == 1, "negative threshold swipe emits one gesture");
    require(gestures[0].kind == gesture_kind::swipe_left, "negative dx is swipe left");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 2000, 0.0f, 0.0f)),
        "third down emits no gesture");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 2100, 59.0f, 0.0f));
    require(gestures.empty(), "dx below swipe threshold does not swipe");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 3000, 0.0f, 0.0f)),
        "fourth down emits no gesture");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 3100, 60.0f, 41.0f));
    require(gestures.empty(), "dy above swipe threshold does not swipe");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 4000, 0.0f, 0.0f)),
        "fifth down emits no gesture");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 4801, 60.0f, 0.0f));
    require(gestures.empty(), "swipe duration above threshold does not swipe");
}

void test_swipe_policy_snapshots()
{
    using namespace quiz_vulkan::input;

    gesture_recognizer recognizer;
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 0.0f, 0.0f)),
        "policy swipe down emits no gesture");
    const gesture_policy_snapshot& tracking = require_single_policy(
        recognizer,
        gesture_policy_decision::tracking_started,
        "policy swipe down records tracking");
    require(tracking.direction == gesture_direction::none, "policy tracking has no direction");
    require(tracking.pointer_id == 1, "policy tracking preserves pointer id");
    require(tracking.swipe_min_dx == 60.0f, "policy tracking records swipe dx threshold");
    require(tracking.swipe_max_dy == 40.0f, "policy tracking records swipe dy threshold");
    require(tracking.swipe_max_duration_ms == 800, "policy tracking records swipe duration threshold");

    std::vector<gesture_event> gestures =
        recognizer.process_pointer_event(pointer(pointer_phase::up, 180, 70.0f, 0.0f));
    require(gestures.size() == 1, "policy swipe emits one gesture");
    const gesture_policy_snapshot& accepted = require_single_policy(
        recognizer,
        gesture_policy_decision::swipe_accepted,
        "policy swipe accepted snapshot is recorded");
    require(accepted.direction == gesture_direction::right, "policy swipe direction is right");
    require(accepted.duration_ms == 80, "policy swipe duration is recorded");
    require(accepted.delta_x == 70.0f, "policy swipe delta x is recorded");
    require(accepted.delta_y == 0.0f, "policy swipe delta y is recorded");
    require(accepted.distance == 70.0f, "policy swipe distance is recorded");
    require(accepted.emitted_input_event, "policy swipe marks emitted input");
    require(accepted.emitted_kind == gesture_kind::swipe_right, "policy swipe emitted kind is recorded");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 200, 0.0f, 0.0f)),
        "policy short swipe down emits no gesture");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 280, 59.0f, 0.0f));
    require(gestures.empty(), "policy short swipe emits no gesture");
    const gesture_policy_snapshot& short_swipe = require_single_policy(
        recognizer,
        gesture_policy_decision::swipe_rejected_distance,
        "policy short swipe records distance rejection");
    require(short_swipe.direction == gesture_direction::right, "policy short swipe direction is right");
    require(short_swipe.delta_x == 59.0f, "policy short swipe delta x is recorded");
    require(!short_swipe.emitted_input_event, "policy short swipe emits no input");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 300, 0.0f, 0.0f)),
        "policy cross-axis swipe down emits no gesture");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 360, 60.0f, 41.0f));
    require(gestures.empty(), "policy cross-axis swipe emits no gesture");
    const gesture_policy_snapshot& cross_axis = require_single_policy(
        recognizer,
        gesture_policy_decision::swipe_rejected_cross_axis,
        "policy cross-axis swipe records rejection");
    require(cross_axis.direction == gesture_direction::right, "policy cross-axis direction is deterministic");
    require(cross_axis.delta_y == 41.0f, "policy cross-axis delta y is recorded");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 400, 0.0f, 0.0f)),
        "policy slow swipe down emits no gesture");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 1201, 60.0f, 0.0f));
    require(gestures.empty(), "policy slow swipe emits no gesture");
    const gesture_policy_snapshot& slow_swipe = require_single_policy(
        recognizer,
        gesture_policy_decision::swipe_rejected_duration,
        "policy slow swipe records duration rejection");
    require(slow_swipe.duration_ms == 801, "policy slow swipe duration is recorded");
    require(slow_swipe.direction == gesture_direction::right, "policy slow swipe direction is right");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 1300, 10.0f, 10.0f)),
        "policy tap down emits no gesture");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 1340, 12.0f, 13.0f));
    require(gestures.size() == 1, "policy tap emits one gesture");
    const gesture_policy_snapshot& tap = require_single_policy(
        recognizer,
        gesture_policy_decision::tap_accepted,
        "policy tap records accepted tap");
    require(tap.emitted_input_event, "policy tap marks emitted input");
    require(tap.emitted_kind == gesture_kind::tap, "policy tap emitted kind is recorded");
    require(tap.direction == gesture_direction::down, "policy tap direction uses dominant axis");
}

void test_tap_and_long_press_suppression()
{
    using namespace quiz_vulkan::input;

    gesture_recognizer recognizer;
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 20.0f, 20.0f)),
        "tap down emits no gesture");
    std::vector<gesture_event> gestures =
        recognizer.process_pointer_event(pointer(pointer_phase::up, 180, 27.0f, 25.0f));
    require(gestures.size() == 1, "tap inside slop emits one gesture");
    require(gestures[0].kind == gesture_kind::tap, "tap kind is emitted");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 1000, 20.0f, 20.0f)),
        "long press down emits no gesture");
    gestures = recognizer.update_time(1599);
    require(gestures.empty(), "long press is not emitted before threshold");

    gestures = recognizer.update_time(1600);
    require(gestures.size() == 1, "long press emits at threshold");
    require(gestures[0].kind == gesture_kind::long_press, "long press kind is emitted");
    const gesture_policy_snapshot& long_press_policy = require_single_policy(
        recognizer,
        gesture_policy_decision::long_press_accepted,
        "long press records accepted policy");
    require(long_press_policy.emitted_input_event, "long press policy marks emitted input");
    require(long_press_policy.emitted_kind == gesture_kind::long_press,
        "long press policy records emitted kind");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 1610, 20.0f, 20.0f));
    require(gestures.empty(), "long press suppresses following tap");
    const gesture_policy_snapshot& suppressed_release = require_single_policy(
        recognizer,
        gesture_policy_decision::release_suppressed,
        "long press release records suppression policy");
    require(!suppressed_release.emitted_input_event, "suppressed release emits no input");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 2000, 0.0f, 0.0f)),
        "swipe suppression down emits no gesture");
    require(!recognizer.update_time(2600).empty(), "long press emits before suppressed swipe");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::move, 2620, 100.0f, 0.0f)),
        "long press suppresses later drag start");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 2650, 100.0f, 0.0f));
    require(gestures.empty(), "long press suppresses following swipe");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 3000, 0.0f, 0.0f)),
        "move test down emits no gesture");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::move, 3100, 9.0f, 0.0f));
    require(gestures.size() == 1, "move beyond slop starts drag");
    require(gestures[0].kind == gesture_kind::drag_start, "move beyond slop emits drag start");
    gestures = recognizer.update_time(3600);
    require(gestures.empty(), "moving beyond tap slop prevents long press");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 3610, 9.0f, 0.0f));
    require(gestures.size() == 1, "drag release emits drag end");
    require(gestures[0].kind == gesture_kind::drag_end, "drag release emits drag end kind");
}

void test_touch_cancel_and_multi_pointer_edges()
{
    using namespace quiz_vulkan::input;

    gesture_recognizer recognizer;
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 0.0f, 0.0f, 10)),
        "cancel test down emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::cancel, 120, 0.0f, 0.0f, 10)),
        "cancel emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::up, 140, 0.0f, 0.0f, 10)),
        "up after cancel emits no stale gesture");
    require_empty(recognizer.update_time(1000), "canceled pointer is not retained for long press");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 2000, 10.0f, 10.0f, 1)),
        "first touch down emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 2010, 80.0f, 20.0f, 2)),
        "second touch down emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::cancel, 2020, 10.0f, 10.0f, 1)),
        "canceling first touch emits no gesture");

    std::vector<gesture_event> gestures = recognizer.update_time(2610);
    require(gestures.size() == 1, "only uncanceled second touch emits long press");
    require(gestures[0].kind == gesture_kind::long_press, "second touch emits long press kind");
    require(gestures[0].pointer_id == 2, "long press preserves uncanceled pointer id");
    require(gestures[0].duration_ms == 600, "long press duration is measured from second touch down");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 2620, 80.0f, 20.0f, 2));
    require(gestures.empty(), "long press suppresses second touch release");

    gesture_recognizer slop_recognizer;
    require_empty(slop_recognizer.process_pointer_event(pointer(pointer_phase::down, 3000, 20.0f, 20.0f)),
        "slop boundary down emits no gesture");
    require_empty(slop_recognizer.process_pointer_event(pointer(pointer_phase::move, 3100, 28.0f, 28.0f)),
        "move exactly at tap slop emits no gesture");
    gestures = slop_recognizer.update_time(3600);
    require(gestures.size() == 1, "move exactly at tap slop still allows long press");
    require(gestures[0].kind == gesture_kind::long_press, "slop boundary long press kind is emitted");
}

void test_unknown_pointer_reset_and_timing_edges()
{
    using namespace quiz_vulkan::input;

    gesture_recognizer recognizer;
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::move, 100, 10.0f, 10.0f, 99)),
        "unknown move emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::up, 110, 10.0f, 10.0f, 99)),
        "unknown up emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::cancel, 120, 10.0f, 10.0f, 99)),
        "unknown cancel emits no gesture");
    require_empty(recognizer.update_time(1000), "unknown pointer phases retain no pending state");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 2000, 0.0f, 0.0f)),
        "max duration swipe down emits no gesture");
    std::vector<gesture_event> gestures =
        recognizer.process_pointer_event(pointer(pointer_phase::up, 2800, 60.0f, 40.0f));
    require(gestures.size() == 1, "swipe at max duration emits one gesture");
    require(gestures[0].kind == gesture_kind::swipe_right, "swipe at max duration keeps swipe kind");
    require(gestures[0].duration_ms == 800, "swipe at max duration preserves duration");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 3000, 20.0f, 20.0f)),
        "idempotent long press down emits no gesture");
    gestures = recognizer.update_time(3600);
    require(gestures.size() == 1, "first long press update emits one gesture");
    require(gestures[0].kind == gesture_kind::long_press, "first long press update kind is emitted");
    require_empty(recognizer.update_time(3700), "second long press update emits no duplicate gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::move, 3710, 20.0f, 20.0f)),
        "move after long press emits no duplicate gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::up, 3720, 20.0f, 20.0f)),
        "release after idempotent long press is suppressed");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 4000, 1.0f, 1.0f)),
        "reset edge down emits no gesture");
    recognizer.reset();
    require_empty(recognizer.update_time(4600), "reset clears pending long press state");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::up, 4610, 1.0f, 1.0f)),
        "up after reset emits no stale tap");
}

void test_pointer_id_reuse_replaces_pending_state()
{
    using namespace quiz_vulkan::input;

    gesture_recognizer recognizer;
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 0.0f, 0.0f, 5)),
        "reused pointer first down emits no gesture");
    std::vector<gesture_event> gestures =
        recognizer.process_pointer_event(pointer(pointer_phase::move, 150, 100.0f, 0.0f, 5));
    require(gestures.size() == 1, "reused pointer first move starts drag");
    require(gestures[0].kind == gesture_kind::drag_start, "reused pointer first move emits drag start");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::down, 200, 10.0f, 10.0f, 5));
    require(gestures.size() == 1, "reused pointer second down cancels first drag");
    require(gestures[0].kind == gesture_kind::drag_cancel, "reused pointer second down emits drag cancel");
    require(gestures[0].duration_ms == 100, "reused pointer drag cancel duration uses old state");
    require(gestures[0].x == 100.0f, "reused pointer drag cancel uses old last x");
    require(gestures[0].y == 0.0f, "reused pointer drag cancel uses old last y");
    require_empty(recognizer.update_time(799), "reused pointer old long press state is discarded");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 740, 11.0f, 11.0f, 5));
    require(gestures.size() == 1, "reused pointer emits tap from replacement state");
    require(gestures[0].kind == gesture_kind::tap, "reused pointer replacement emits tap kind");
    require(gestures[0].duration_ms == 540, "reused pointer duration starts at replacement down");
    require(gestures[0].start_x == 10.0f, "reused pointer start x is replacement down");
    require(gestures[0].start_y == 10.0f, "reused pointer start y is replacement down");
}

void test_drag_gesture_lifecycle_and_cancel()
{
    using namespace quiz_vulkan::input;

    gesture_recognizer recognizer;
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 0.0f, 0.0f, 4)),
        "drag down emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::move, 110, 8.0f, 8.0f, 4)),
        "drag move at slop boundary emits no gesture");

    std::vector<gesture_event> gestures =
        recognizer.process_pointer_event(pointer(pointer_phase::move, 120, 9.0f, 4.0f, 4));
    require(gestures.size() == 1, "drag start emits one gesture");
    require(gestures[0].kind == gesture_kind::drag_start, "drag start kind is emitted");
    require(gestures[0].duration_ms == 20, "drag start duration is measured from down");
    require(gestures[0].delta_x == 9.0f, "drag start delta x is measured from down");
    require(gestures[0].delta_y == 4.0f, "drag start delta y is measured from down");
    const gesture_policy_snapshot& drag_start_policy = require_single_policy(
        recognizer,
        gesture_policy_decision::drag_started,
        "drag start records policy");
    require(drag_start_policy.emitted_input_event, "drag start policy marks emitted input");
    require(drag_start_policy.emitted_kind == gesture_kind::drag_start,
        "drag start policy records emitted kind");
    require(drag_start_policy.direction == gesture_direction::right, "drag start policy records direction");
    require(drag_start_policy.distance > 9.0f, "drag start policy records distance from start");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::move, 140, 15.0f, 7.0f, 4));
    require(gestures.size() == 1, "drag update emits one gesture");
    require(gestures[0].kind == gesture_kind::drag_update, "drag update kind is emitted");
    require(gestures[0].delta_x == 6.0f, "drag update delta x is measured from previous pointer");
    require(gestures[0].delta_y == 3.0f, "drag update delta y is measured from previous pointer");
    const gesture_policy_snapshot& drag_update_policy = require_single_policy(
        recognizer,
        gesture_policy_decision::drag_updated,
        "drag update records policy");
    require(drag_update_policy.delta_x == 15.0f, "drag update policy delta x is from drag start");
    require(drag_update_policy.delta_y == 7.0f, "drag update policy delta y is from drag start");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 150, 20.0f, 10.0f, 4));
    require(gestures.size() == 1, "drag end emits one gesture");
    require(gestures[0].kind == gesture_kind::drag_end, "drag end kind is emitted");
    require(gestures[0].delta_x == 5.0f, "drag end delta x is measured from previous pointer");
    require(gestures[0].delta_y == 3.0f, "drag end delta y is measured from previous pointer");
    const gesture_policy_snapshot& drag_end_policy = require_single_policy(
        recognizer,
        gesture_policy_decision::drag_released,
        "drag end records policy");
    require(drag_end_policy.emitted_kind == gesture_kind::drag_end, "drag end policy records emitted kind");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::cancel, 160, 20.0f, 10.0f, 4)),
        "cancel after drag end emits no stale gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::up, 170, 20.0f, 10.0f, 4)),
        "up after drag end emits no stale gesture");
    require_empty(recognizer.update_time(800), "drag end clears pending long press state");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 200, 30.0f, 30.0f, 6)),
        "drag cancel down emits no gesture");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::move, 210, 30.0f, 39.0f, 6));
    require(gestures.size() == 1, "drag cancel setup starts drag");
    require(gestures[0].kind == gesture_kind::drag_start, "drag cancel setup emits drag start");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::cancel, 220, 30.0f, 42.0f, 6));
    require(gestures.size() == 1, "drag cancel emits one gesture");
    require(gestures[0].kind == gesture_kind::drag_cancel, "drag cancel kind is emitted");
    require(gestures[0].delta_x == 0.0f, "drag cancel delta x is measured from previous pointer");
    require(gestures[0].delta_y == 3.0f, "drag cancel delta y is measured from previous pointer");
    const gesture_policy_snapshot& drag_cancel_policy = require_single_policy(
        recognizer,
        gesture_policy_decision::drag_canceled,
        "drag cancel records policy");
    require(drag_cancel_policy.emitted_kind == gesture_kind::drag_cancel,
        "drag cancel policy records emitted kind");
    require(drag_cancel_policy.phase == pointer_phase::cancel, "drag cancel policy records cancel phase");
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "drag cancel releases capture snapshot");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::up, 230, 30.0f, 42.0f, 6)),
        "up after drag cancel emits no stale gesture");
    require_empty(recognizer.update_time(900), "drag cancel clears pending long press state");
}

void test_drag_capture_ignores_other_pointers_until_release()
{
    using namespace quiz_vulkan::input;

    gesture_recognizer recognizer;
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "capture snapshot starts idle");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 0.0f, 0.0f, 1)),
        "captured drag first pointer down emits no gesture");
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::tracking,
        false,
        1,
        1,
        "capture snapshot tracks first pointer before drag");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 105, 40.0f, 0.0f, 2)),
        "second pointer before capture emits no gesture");
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::tracking,
        false,
        1,
        2,
        "capture snapshot tracks multiple pending pointers deterministically");

    std::vector<gesture_event> gestures =
        recognizer.process_pointer_event(pointer(pointer_phase::move, 120, 9.0f, 0.0f, 1));
    require(gestures.size() == 1, "first pointer starts captured drag");
    require(gestures[0].kind == gesture_kind::drag_start, "captured drag start kind is emitted");
    require(gestures[0].pointer_id == 1, "captured drag start preserves first pointer id");
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::captured,
        true,
        1,
        1,
        "capture snapshot marks first pointer captured");

    require_empty(recognizer.update_time(705), "captured drag drops other pending long press state");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::up, 710, 40.0f, 0.0f, 2)),
        "second pointer up is ignored while first pointer is captured");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 720, 50.0f, 0.0f, 2)),
        "new second pointer down is ignored while first pointer is captured");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::up, 730, 50.0f, 0.0f, 2)),
        "new second pointer up is ignored while first pointer is captured");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::move, 740, 15.0f, 2.0f, 1));
    require(gestures.size() == 1, "captured first pointer continues drag updates");
    require(gestures[0].kind == gesture_kind::drag_update, "captured first pointer emits drag update");
    require(gestures[0].pointer_id == 1, "captured drag update preserves first pointer id");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 760, 18.0f, 3.0f, 1));
    require(gestures.size() == 1, "captured first pointer releases with drag end");
    require(gestures[0].kind == gesture_kind::drag_end, "captured drag releases with drag end kind");
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "capture snapshot returns idle after drag release");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 800, 60.0f, 0.0f, 2)),
        "second pointer down after capture release is accepted");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 820, 60.0f, 0.0f, 2));
    require(gestures.size() == 1, "second pointer taps after capture release");
    require(gestures[0].kind == gesture_kind::tap, "second pointer tap kind is emitted after release");
    require(gestures[0].pointer_id == 2, "second pointer tap preserves pointer id after release");
}

void test_capture_arbitration_restart_and_ignored_cancel()
{
    using namespace quiz_vulkan::input;

    gesture_recognizer recognizer;
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 0.0f, 0.0f, 4)),
        "arbitration first down emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 105, 40.0f, 0.0f, 9)),
        "arbitration second down emits no gesture");
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::tracking,
        false,
        4,
        2,
        "arbitration pending snapshot uses lowest pointer id");

    std::vector<gesture_event> gestures =
        recognizer.process_pointer_event(pointer(pointer_phase::move, 120, 50.0f, 0.0f, 9));
    require(gestures.size() == 1, "arbitration second pointer starts drag");
    require(gestures[0].kind == gesture_kind::drag_start, "arbitration second pointer emits drag start");
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::captured,
        true,
        9,
        1,
        "arbitration capture drops noncaptured pending pointer");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::cancel, 130, 0.0f, 0.0f, 4)),
        "arbitration noncaptured cancel is ignored while another pointer is captured");
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::captured,
        true,
        9,
        1,
        "arbitration ignored cancel preserves capture");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::down, 140, 45.0f, 2.0f, 9));
    require(gestures.size() == 1, "arbitration repeated captured down cancels existing drag");
    require(gestures[0].kind == gesture_kind::drag_cancel,
        "arbitration repeated captured down emits drag cancel");
    require(gestures[0].pointer_id == 9, "arbitration repeated captured down preserves pointer id");
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::tracking,
        false,
        9,
        1,
        "arbitration repeated captured down restarts pointer as tracked");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::move, 150, 55.0f, 2.0f, 9));
    require(gestures.size() == 1, "arbitration restarted pointer starts drag again");
    require(gestures[0].kind == gesture_kind::drag_start,
        "arbitration restarted pointer emits drag start");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::cancel, 160, 55.0f, 4.0f, 9));
    require(gestures.size() == 1, "arbitration captured cancel emits drag cancel");
    require(gestures[0].kind == gesture_kind::drag_cancel,
        "arbitration captured cancel emits drag cancel kind");
    require_capture_snapshot(
        recognizer.capture_snapshot(),
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "arbitration captured cancel releases capture");
}

void test_drag_suppresses_long_press_when_drag_slop_is_inside_tap_slop()
{
    using namespace quiz_vulkan::input;

    gesture_thresholds thresholds;
    thresholds.tap_slop = 10.0f;
    thresholds.drag_start_slop = 4.0f;
    gesture_recognizer recognizer(thresholds);

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 0.0f, 0.0f)),
        "inside-tap-slop drag down emits no gesture");
    std::vector<gesture_event> gestures =
        recognizer.process_pointer_event(pointer(pointer_phase::move, 120, 5.0f, 0.0f));
    require(gestures.size() == 1, "move outside smaller drag slop starts drag");
    require(gestures[0].kind == gesture_kind::drag_start, "inside-tap-slop drag start kind is emitted");

    require_empty(recognizer.update_time(700), "active drag suppresses long press even inside tap slop");
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 710, 5.0f, 0.0f));
    require(gestures.size() == 1, "inside-tap-slop drag still releases as drag end");
    require(gestures[0].kind == gesture_kind::drag_end, "inside-tap-slop drag end kind is emitted");
}

void test_drag_start_slop_is_configurable()
{
    using namespace quiz_vulkan::input;

    gesture_thresholds thresholds;
    thresholds.drag_start_slop = 12.0f;
    gesture_recognizer recognizer(thresholds);

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 0.0f, 0.0f)),
        "custom drag slop down emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::move, 120, 9.0f, 0.0f)),
        "move outside tap slop but inside drag slop emits no drag");
    require_empty(recognizer.update_time(700), "move outside tap slop still prevents long press");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::up, 720, 9.0f, 0.0f)),
        "release inside custom drag slop emits no tap or drag");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 1000, 0.0f, 0.0f)),
        "custom drag slop second down emits no gesture");
    std::vector<gesture_event> gestures =
        recognizer.process_pointer_event(pointer(pointer_phase::move, 1020, 13.0f, 0.0f));
    require(gestures.size() == 1, "move outside custom drag slop starts drag");
    require(gestures[0].kind == gesture_kind::drag_start, "custom drag slop emits drag start");
    require(gestures[0].delta_x == 13.0f, "custom drag slop drag start delta is preserved");
}

void test_multi_pointer_long_press_order_is_stable()
{
    using namespace quiz_vulkan::input;

    gesture_recognizer recognizer;
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 90.0f, 0.0f, 9)),
        "stable order high id down emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 100, 10.0f, 0.0f, 1)),
        "stable order low id down emits no gesture");

    std::vector<gesture_event> gestures = recognizer.update_time(700);
    require(gestures.size() == 2, "simultaneous long presses emit two gestures");
    require(gestures[0].kind == gesture_kind::long_press, "first stable gesture is long press");
    require(gestures[1].kind == gesture_kind::long_press, "second stable gesture is long press");
    require(gestures[0].pointer_id == 1, "stable long press order starts with lower pointer id");
    require(gestures[1].pointer_id == 9, "stable long press order ends with higher pointer id");
}

} // namespace

int main()
{
    test_swipe_thresholds();
    test_swipe_policy_snapshots();
    test_tap_and_long_press_suppression();
    test_touch_cancel_and_multi_pointer_edges();
    test_unknown_pointer_reset_and_timing_edges();
    test_pointer_id_reuse_replaces_pending_state();
    test_drag_gesture_lifecycle_and_cancel();
    test_drag_capture_ignores_other_pointers_until_release();
    test_capture_arbitration_restart_and_ignored_cancel();
    test_drag_suppresses_long_press_when_drag_slop_is_inside_tap_slop();
    test_drag_start_slop_is_configurable();
    test_multi_pointer_long_press_order_is_stable();

    std::cout << "gesture_recognizer_tests passed\n";
    return 0;
}
