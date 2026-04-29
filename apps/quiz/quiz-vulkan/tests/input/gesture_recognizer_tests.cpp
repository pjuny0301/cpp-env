#include "core/input/gesture_recognizer.h"

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

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 1610, 20.0f, 20.0f));
    require(gestures.empty(), "long press suppresses following tap");

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

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::move, 140, 15.0f, 7.0f, 4));
    require(gestures.size() == 1, "drag update emits one gesture");
    require(gestures[0].kind == gesture_kind::drag_update, "drag update kind is emitted");
    require(gestures[0].delta_x == 6.0f, "drag update delta x is measured from previous pointer");
    require(gestures[0].delta_y == 3.0f, "drag update delta y is measured from previous pointer");

    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 150, 20.0f, 10.0f, 4));
    require(gestures.size() == 1, "drag end emits one gesture");
    require(gestures[0].kind == gesture_kind::drag_end, "drag end kind is emitted");
    require(gestures[0].delta_x == 5.0f, "drag end delta x is measured from previous pointer");
    require(gestures[0].delta_y == 3.0f, "drag end delta y is measured from previous pointer");

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
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::up, 230, 30.0f, 42.0f, 6)),
        "up after drag cancel emits no stale gesture");
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
    test_tap_and_long_press_suppression();
    test_touch_cancel_and_multi_pointer_edges();
    test_unknown_pointer_reset_and_timing_edges();
    test_pointer_id_reuse_replaces_pending_state();
    test_drag_gesture_lifecycle_and_cancel();
    test_drag_start_slop_is_configurable();
    test_multi_pointer_long_press_order_is_stable();

    std::cout << "gesture_recognizer_tests passed\n";
    return 0;
}
