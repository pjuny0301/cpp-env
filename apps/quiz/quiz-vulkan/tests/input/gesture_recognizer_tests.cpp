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
    gestures = recognizer.process_pointer_event(pointer(pointer_phase::up, 2650, 100.0f, 0.0f));
    require(gestures.empty(), "long press suppresses following swipe");

    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::down, 3000, 0.0f, 0.0f)),
        "move test down emits no gesture");
    require_empty(recognizer.process_pointer_event(pointer(pointer_phase::move, 3100, 9.0f, 0.0f)),
        "move beyond slop emits no immediate gesture");
    gestures = recognizer.update_time(3600);
    require(gestures.empty(), "moving beyond tap slop prevents long press");
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

} // namespace

int main()
{
    test_swipe_thresholds();
    test_tap_and_long_press_suppression();
    test_touch_cancel_and_multi_pointer_edges();

    std::cout << "gesture_recognizer_tests passed\n";
    return 0;
}
