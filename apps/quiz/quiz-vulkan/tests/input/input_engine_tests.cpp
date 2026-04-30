#include "core/input/input_engine.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

std::string utf8(const char8_t* text)
{
    return reinterpret_cast<const char*>(text);
}

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "input_engine_tests failed: " << message << '\n';
    std::exit(1);
}

void require_range(
    quiz_vulkan::input::text_range range,
    std::size_t start_byte,
    std::size_t end_byte,
    const char* message)
{
    require(range.start_byte == start_byte, message);
    require(range.end_byte == end_byte, message);
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

template <typename T>
const T& require_event(const std::vector<quiz_vulkan::input::input_event>& events, std::size_t index)
{
    require(index < events.size(), "event index exists");
    require(std::holds_alternative<T>(events[index]), "event has expected type");
    return std::get<T>(events[index]);
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

quiz_vulkan::raw_platform_input_event text(std::int64_t timestamp_ms, std::string value)
{
    return quiz_vulkan::raw_platform_text_event{
        .timestamp_ms = timestamp_ms,
        .utf8_text = std::move(value),
    };
}

quiz_vulkan::raw_platform_input_event ime(
    quiz_vulkan::raw_platform_ime_phase phase,
    std::int64_t timestamp_ms,
    std::string value = {})
{
    return quiz_vulkan::raw_platform_ime_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
        .utf8_text = std::move(value),
    };
}

quiz_vulkan::raw_platform_input_event key(
    std::int64_t timestamp_ms,
    std::string logical_key,
    bool repeat = false,
    quiz_vulkan::raw_platform_key_phase phase = quiz_vulkan::raw_platform_key_phase::down,
    bool ctrl = false,
    bool shift = false,
    bool meta = false)
{
    return quiz_vulkan::raw_platform_key_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
        .key_code = 0,
        .logical_key = std::move(logical_key),
        .ctrl = ctrl,
        .shift = shift,
        .meta = meta,
        .repeat = repeat,
    };
}

quiz_vulkan::raw_platform_input_event key_code(
    std::int64_t timestamp_ms,
    std::int32_t code,
    quiz_vulkan::raw_platform_key_phase phase = quiz_vulkan::raw_platform_key_phase::down)
{
    return quiz_vulkan::raw_platform_key_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
        .key_code = code,
        .logical_key = {},
    };
}

quiz_vulkan::raw_platform_input_event focus(
    quiz_vulkan::raw_platform_focus_phase phase,
    std::int64_t timestamp_ms)
{
    return quiz_vulkan::raw_platform_focus_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
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

void test_primary_pointer_gestures_and_secondary_filter()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                100,
                10.0f,
                10.0f,
                raw_platform_pointer_button::secondary))
                .empty(),
        "secondary down is ignored");
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::up,
                130,
                10.0f,
                10.0f,
                raw_platform_pointer_button::secondary))
                .empty(),
        "secondary up is ignored");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 200, 20.0f, 30.0f)).empty(),
        "primary down emits no gesture");
    std::vector<input_event> events =
        engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 240, 24.0f, 32.0f));

    require(events.size() == 1, "primary tap emits one event");
    const gesture_event& tap = require_event<gesture_event>(events, 0);
    require(tap.kind == gesture_kind::tap, "primary tap emits tap gesture");
    require(tap.pointer_id == 1, "pointer id is preserved");
}

void test_touch_pointer_cancel_and_multi_pointer_edges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                100,
                20.0f,
                30.0f,
                raw_platform_pointer_button::none,
                42))
                .empty(),
        "touch-style pointer down emits no gesture");
    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        140,
        21.0f,
        31.0f,
        raw_platform_pointer_button::none,
        42));
    require(events.size() == 1, "touch-style pointer button none emits tap");
    const gesture_event& touch_tap = require_event<gesture_event>(events, 0);
    require(touch_tap.kind == gesture_kind::tap, "touch-style pointer tap kind is emitted");
    require(touch_tap.pointer_id == 42, "touch-style pointer id is preserved");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 200, 0.0f, 0.0f, raw_platform_pointer_button::primary, 7))
                .empty(),
        "cancel edge down emits no gesture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::cancel, 220, 0.0f, 0.0f, raw_platform_pointer_button::primary, 7))
                .empty(),
        "cancel edge cancel emits no gesture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 240, 0.0f, 0.0f, raw_platform_pointer_button::primary, 7))
                .empty(),
        "up after cancel emits no stale tap");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 1000, 5.0f, 5.0f, raw_platform_pointer_button::primary, 1))
                .empty(),
        "first concurrent touch down emits no gesture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 1010, 90.0f, 10.0f, raw_platform_pointer_button::primary, 2))
                .empty(),
        "second concurrent touch down emits no gesture");
    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 1040, 91.0f, 11.0f, raw_platform_pointer_button::primary, 2));
    require(events.size() == 1, "second concurrent touch can tap independently");
    const gesture_event& second_touch_tap = require_event<gesture_event>(events, 0);
    require(second_touch_tap.kind == gesture_kind::tap, "second concurrent touch emits tap kind");
    require(second_touch_tap.pointer_id == 2, "second concurrent touch preserves pointer id");

    events = engine.update_time(1600);
    require(events.size() == 1, "first concurrent touch remains active for long press");
    const gesture_event& first_touch_long_press = require_event<gesture_event>(events, 0);
    require(first_touch_long_press.kind == gesture_kind::long_press, "first concurrent touch emits long press kind");
    require(first_touch_long_press.pointer_id == 1, "first concurrent touch preserves pointer id");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 1610, 5.0f, 5.0f, raw_platform_pointer_button::primary, 1))
                .empty(),
        "long-pressed concurrent touch suppresses release");
}

void test_pointer_filter_and_timing_edges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 100, 5.0f, 5.0f)).empty(),
        "unknown raw pointer move emits no gesture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 110, 5.0f, 5.0f)).empty(),
        "unknown raw pointer up emits no gesture");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 200, 10.0f, 10.0f, raw_platform_pointer_button::primary, 5))
                .empty(),
        "primary pointer down emits no gesture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::cancel, 210, 10.0f, 10.0f, raw_platform_pointer_button::secondary, 5))
                .empty(),
        "secondary cancel with matching id is filtered");
    std::vector<input_event> events =
        engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 230, 10.0f, 10.0f, raw_platform_pointer_button::primary, 5));
    require(events.size() == 1, "filtered secondary cancel does not disturb primary tap");
    const gesture_event& tap = require_event<gesture_event>(events, 0);
    require(tap.kind == gesture_kind::tap, "primary tap survives filtered secondary cancel");
    require(tap.pointer_id == 5, "primary tap preserves pointer id after filtered cancel");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 1000, 30.0f, 30.0f, raw_platform_pointer_button::primary, 6))
                .empty(),
        "long press timing down emits no gesture");
    events = engine.update_time(1600);
    require(events.size() == 1, "engine long press update emits once");
    const gesture_event& long_press = require_event<gesture_event>(events, 0);
    require(long_press.kind == gesture_kind::long_press, "engine long press update kind is emitted");
    require(long_press.pointer_id == 6, "engine long press preserves pointer id");
    require(engine.update_time(1700).empty(), "engine long press update emits no duplicate");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 1705, 80.0f, 30.0f, raw_platform_pointer_button::primary, 6))
                .empty(),
        "engine long press suppresses later drag start");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 1710, 30.0f, 30.0f, raw_platform_pointer_button::primary, 6))
                .empty(),
        "engine long press suppresses release after duplicate update check");
}

void test_pointer_id_reuse_routes_replacement_state()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 100, 0.0f, 0.0f, raw_platform_pointer_button::primary, 8))
                .empty(),
        "reused raw pointer first down emits no gesture");
    std::vector<input_event> events =
        engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 150, 100.0f, 0.0f, raw_platform_pointer_button::primary, 8));
    require(events.size() == 1, "reused raw pointer first move starts drag");
    require(require_event<gesture_event>(events, 0).kind == gesture_kind::drag_start,
        "reused raw pointer first move emits drag start");
    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 200, 20.0f, 20.0f, raw_platform_pointer_button::primary, 8));
    require(events.size() == 1, "reused raw pointer second down cancels first drag");
    const gesture_event& cancel = require_event<gesture_event>(events, 0);
    require(cancel.kind == gesture_kind::drag_cancel, "reused raw pointer second down emits drag cancel");
    require(cancel.duration_ms == 100, "reused raw pointer drag cancel duration uses old state");
    require(cancel.x == 100.0f, "reused raw pointer drag cancel uses old last x");
    require(cancel.y == 0.0f, "reused raw pointer drag cancel uses old last y");
    require(engine.update_time(799).empty(), "reused raw pointer old long press state is discarded");

    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 740, 21.0f, 21.0f, raw_platform_pointer_button::primary, 8));
    require(events.size() == 1, "reused raw pointer emits one replacement tap");
    const gesture_event& tap = require_event<gesture_event>(events, 0);
    require(tap.kind == gesture_kind::tap, "reused raw pointer emits tap kind");
    require(tap.pointer_id == 8, "reused raw pointer preserves pointer id");
    require(tap.duration_ms == 540, "reused raw pointer duration starts at replacement down");
    require(tap.start_x == 20.0f, "reused raw pointer start x is replacement down");
    require(tap.start_y == 20.0f, "reused raw pointer start y is replacement down");
}

void test_drag_gestures_route_from_raw_pointer()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 100, 0.0f, 0.0f, raw_platform_pointer_button::primary, 10))
                .empty(),
        "raw drag down emits no gesture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 110, 8.0f, 8.0f, raw_platform_pointer_button::primary, 10))
                .empty(),
        "raw drag move at slop boundary emits no gesture");

    std::vector<input_event> events =
        engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 120, 9.0f, 4.0f, raw_platform_pointer_button::primary, 10));
    require(events.size() == 1, "raw drag start emits one event");
    const gesture_event& start = require_event<gesture_event>(events, 0);
    require(start.kind == gesture_kind::drag_start, "raw drag start kind is routed");
    require(start.pointer_id == 10, "raw drag start preserves pointer id");
    require(start.delta_x == 9.0f, "raw drag start delta x is from down");
    require(start.delta_y == 4.0f, "raw drag start delta y is from down");

    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 140, 15.0f, 7.0f, raw_platform_pointer_button::primary, 10));
    require(events.size() == 1, "raw drag update emits one event");
    const gesture_event& update = require_event<gesture_event>(events, 0);
    require(update.kind == gesture_kind::drag_update, "raw drag update kind is routed");
    require(update.delta_x == 6.0f, "raw drag update delta x is from previous pointer");
    require(update.delta_y == 3.0f, "raw drag update delta y is from previous pointer");

    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 150, 20.0f, 10.0f, raw_platform_pointer_button::primary, 10));
    require(events.size() == 1, "raw drag end emits one event");
    const gesture_event& end = require_event<gesture_event>(events, 0);
    require(end.kind == gesture_kind::drag_end, "raw drag end kind is routed");
    require(end.delta_x == 5.0f, "raw drag end delta x is from previous pointer");
    require(end.delta_y == 3.0f, "raw drag end delta y is from previous pointer");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 200, 30.0f, 30.0f, raw_platform_pointer_button::none, 11))
                .empty(),
        "raw touch drag down emits no gesture");
    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 210, 30.0f, 39.0f, raw_platform_pointer_button::none, 11));
    require(events.size() == 1, "raw touch drag starts");
    require(require_event<gesture_event>(events, 0).kind == gesture_kind::drag_start, "raw touch drag start kind is routed");

    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::cancel, 220, 30.0f, 42.0f, raw_platform_pointer_button::none, 11));
    require(events.size() == 1, "raw touch drag cancel emits one event");
    const gesture_event& cancel = require_event<gesture_event>(events, 0);
    require(cancel.kind == gesture_kind::drag_cancel, "raw touch drag cancel kind is routed");
    require(cancel.delta_y == 3.0f, "raw touch drag cancel delta y is from previous pointer");
}

void test_pointer_capture_routes_drag_and_ignores_other_pointers()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                100,
                0.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                1))
                .empty(),
        "capture route first pointer down emits no gesture");
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                105,
                50.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                2))
                .empty(),
        "capture route second pointer before drag emits no gesture");

    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::move,
        120,
        9.0f,
        0.0f,
        raw_platform_pointer_button::primary,
        1));
    require(events.size() == 1, "capture route first pointer starts drag");
    const gesture_event& start = require_event<gesture_event>(events, 0);
    require(start.kind == gesture_kind::drag_start, "capture route first pointer emits drag start");
    require(start.pointer_id == 1, "capture route drag start preserves first pointer id");

    require(engine.update_time(705).empty(), "capture route suppresses other pointer long press");
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::up,
                710,
                50.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                2))
                .empty(),
        "capture route ignores other pointer up");
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                720,
                60.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                2))
                .empty(),
        "capture route ignores new other pointer down");
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::up,
                730,
                60.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                2))
                .empty(),
        "capture route ignores new other pointer up");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::cancel,
        740,
        12.0f,
        3.0f,
        raw_platform_pointer_button::primary,
        1));
    require(events.size() == 1, "capture route first pointer cancel emits drag cancel");
    const gesture_event& cancel = require_event<gesture_event>(events, 0);
    require(cancel.kind == gesture_kind::drag_cancel, "capture route cancel kind is routed");
    require(cancel.pointer_id == 1, "capture route cancel preserves first pointer id");
    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::up,
                750,
                12.0f,
                3.0f,
                raw_platform_pointer_button::primary,
                1))
                .empty(),
        "capture route up after drag cancel emits no stale gesture");

    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                800,
                60.0f,
                0.0f,
                raw_platform_pointer_button::primary,
                2))
                .empty(),
        "capture route second pointer is accepted after release");
    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        820,
        60.0f,
        0.0f,
        raw_platform_pointer_button::primary,
        2));
    require(events.size() == 1, "capture route second pointer taps after release");
    const gesture_event& tap = require_event<gesture_event>(events, 0);
    require(tap.kind == gesture_kind::tap, "capture route second pointer tap kind is routed");
    require(tap.pointer_id == 2, "capture route second pointer tap preserves id");
}

void test_drag_start_slop_routes_from_engine_thresholds()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    gesture_thresholds thresholds;
    thresholds.drag_start_slop = 12.0f;
    input_engine engine(thresholds);

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 100, 0.0f, 0.0f)).empty(),
        "engine custom drag slop down emits no gesture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 120, 9.0f, 0.0f)).empty(),
        "engine move outside tap slop but inside drag slop emits no drag");
    require(engine.update_time(700).empty(), "engine move outside tap slop still prevents long press");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 720, 9.0f, 0.0f)).empty(),
        "engine release inside custom drag slop emits no tap or drag");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 1000, 0.0f, 0.0f)).empty(),
        "engine custom drag slop second down emits no gesture");
    std::vector<input_event> events =
        engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 1020, 13.0f, 0.0f));
    require(events.size() == 1, "engine move outside custom drag slop starts drag");
    const gesture_event& drag = require_event<gesture_event>(events, 0);
    require(drag.kind == gesture_kind::drag_start, "engine custom drag slop emits drag start");
    require(drag.delta_x == 13.0f, "engine custom drag slop delta is preserved");
}

void test_multi_pointer_long_press_order_routes_stably()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 100, 90.0f, 0.0f, raw_platform_pointer_button::primary, 9))
                .empty(),
        "engine stable order high id down emits no gesture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 100, 10.0f, 0.0f, raw_platform_pointer_button::primary, 1))
                .empty(),
        "engine stable order low id down emits no gesture");

    std::vector<input_event> events = engine.update_time(700);
    require(events.size() == 2, "engine simultaneous long presses emit two events");
    const gesture_event& first = require_event<gesture_event>(events, 0);
    const gesture_event& second = require_event<gesture_event>(events, 1);
    require(first.kind == gesture_kind::long_press, "engine first stable gesture is long press");
    require(second.kind == gesture_kind::long_press, "engine second stable gesture is long press");
    require(first.pointer_id == 1, "engine stable long press order starts with lower pointer id");
    require(second.pointer_id == 9, "engine stable long press order ends with higher pointer id");
}

void test_scroll_events_normalize_line_and_pixel_deltas()
{
    using namespace quiz_vulkan::input;

    input_engine engine;
    std::vector<input_event> events =
        engine.process_scroll_event(scroll(100, 10.0f, 20.0f, 0.0f, -120.0f, scroll_delta_unit::pixels));
    require(events.size() == 1, "pixel scroll emits one input event");
    const scroll_event& pixel = require_event<scroll_event>(events, 0);
    require(pixel.timestamp_ms == 100, "pixel scroll preserves timestamp");
    require(pixel.x == 10.0f, "pixel scroll preserves x");
    require(pixel.y == 20.0f, "pixel scroll preserves y");
    require(pixel.pixel_delta_x == 0.0f, "pixel scroll x delta is normalized");
    require(pixel.pixel_delta_y == -120.0f, "pixel scroll y delta is normalized");
    require(pixel.line_delta_x == 0.0f, "pixel scroll line x is zero");
    require(pixel.line_delta_y == 0.0f, "pixel scroll line y is zero");

    events = engine.process_scroll_event(scroll(110, 30.0f, 40.0f, 1.0f, -3.0f, scroll_delta_unit::lines));
    require(events.size() == 1, "line scroll emits one input event");
    const scroll_event& line = require_event<scroll_event>(events, 0);
    require(line.timestamp_ms == 110, "line scroll preserves timestamp");
    require(line.x == 30.0f, "line scroll preserves x");
    require(line.y == 40.0f, "line scroll preserves y");
    require(line.pixel_delta_x == 0.0f, "line scroll pixel x is zero");
    require(line.pixel_delta_y == 0.0f, "line scroll pixel y is zero");
    require(line.line_delta_x == 1.0f, "line scroll x delta is normalized");
    require(line.line_delta_y == -3.0f, "line scroll y delta is normalized");

    events = engine.process_scroll_event(scroll(120, 50.0f, 60.0f, 0.0f, 0.0f, scroll_delta_unit::pixels));
    require(events.empty(), "zero scroll delta emits no input event");
}

void test_raw_platform_scroll_routes_through_input_engine()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    std::vector<input_event> events = engine.process_raw_event(platform_scroll(
        200,
        12.0f,
        24.0f,
        0.0f,
        -2.0f,
        raw_platform_scroll_delta_unit::lines));
    require(events.size() == 1, "raw platform line scroll emits one input event");
    const scroll_event& line = require_event<scroll_event>(events, 0);
    require(line.timestamp_ms == 200, "raw platform line scroll preserves timestamp");
    require(line.x == 12.0f, "raw platform line scroll preserves x");
    require(line.y == 24.0f, "raw platform line scroll preserves y");
    require(line.line_delta_x == 0.0f, "raw platform line scroll x delta is normalized");
    require(line.line_delta_y == -2.0f, "raw platform line scroll y delta is normalized");
    require(line.pixel_delta_x == 0.0f, "raw platform line scroll pixel x is zero");
    require(line.pixel_delta_y == 0.0f, "raw platform line scroll pixel y is zero");

    events = engine.process_raw_event(platform_scroll(
        210,
        30.0f,
        40.0f,
        15.0f,
        -30.0f,
        raw_platform_scroll_delta_unit::pixels));
    require(events.size() == 1, "raw platform pixel scroll emits one input event");
    const scroll_event& pixel = require_event<scroll_event>(events, 0);
    require(pixel.timestamp_ms == 210, "raw platform pixel scroll preserves timestamp");
    require(pixel.pixel_delta_x == 15.0f, "raw platform pixel scroll x delta is normalized");
    require(pixel.pixel_delta_y == -30.0f, "raw platform pixel scroll y delta is normalized");
    require(pixel.line_delta_x == 0.0f, "raw platform pixel scroll line x is zero");
    require(pixel.line_delta_y == 0.0f, "raw platform pixel scroll line y is zero");

    events = engine.process_raw_event(platform_scroll(
        220,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        raw_platform_scroll_delta_unit::pixels));
    require(events.empty(), "raw platform zero scroll emits no input event");
}

void test_gesture_routing_diagnostics_summarize_gestures_and_wheel()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "routing diagnostics start with idle capture");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 100, 0.0f, 0.0f)).empty(),
        "diagnostic swipe down emits no gesture");
    require(engine.routing_diagnostics().normalized_events.empty(), "diagnostic swipe down has no summaries");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::tracking,
        false,
        1,
        1,
        "diagnostic swipe down tracks pointer");

    std::vector<input_event> events =
        engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 180, 70.0f, 0.0f));
    require(events.size() == 1, "diagnostic swipe emits one event");
    const auto& swipe_summaries = engine.routing_diagnostics().normalized_events;
    require(swipe_summaries.size() == 1, "diagnostic swipe emits one summary");
    require(swipe_summaries[0].kind == input_event_summary_kind::swipe_right,
        "diagnostic swipe summary kind is swipe right");
    require(swipe_summaries[0].timestamp_ms == 180, "diagnostic swipe summary timestamp is preserved");
    require(swipe_summaries[0].duration_ms == 80, "diagnostic swipe summary duration is preserved");
    require(swipe_summaries[0].pointer_id == 1, "diagnostic swipe summary pointer id is preserved");
    require(swipe_summaries[0].start_x == 0.0f, "diagnostic swipe summary start x is preserved");
    require(swipe_summaries[0].x == 70.0f, "diagnostic swipe summary end x is preserved");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "diagnostic swipe emits one gesture policy");
    const action_route_policy_diagnostic& swipe_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "diagnostic swipe policy is gesture snapshot");
    require(swipe_policy.normalized_event.kind == input_event_summary_kind::swipe_right,
        "diagnostic swipe policy carries normalized swipe");
    require(swipe_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::tracking,
        "diagnostic swipe policy records tracked pointer before release");
    require(swipe_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::idle,
        "diagnostic swipe policy records idle pointer after release");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "diagnostic swipe release clears pointer capture");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 200, 10.0f, 10.0f)).empty(),
        "diagnostic long press down emits no gesture");
    events = engine.update_time(800);
    require(events.size() == 1, "diagnostic long press emits one event");
    const auto& long_press_summaries = engine.routing_diagnostics().normalized_events;
    require(long_press_summaries.size() == 1, "diagnostic long press emits one summary");
    require(long_press_summaries[0].kind == input_event_summary_kind::long_press,
        "diagnostic long press summary kind is long press");
    require(long_press_summaries[0].duration_ms == 600, "diagnostic long press duration is preserved");
    require(long_press_summaries[0].x == 10.0f, "diagnostic long press x is preserved");
    const action_route_policy_diagnostic& long_press_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "diagnostic long press policy is gesture snapshot");
    require(long_press_policy.normalized_event.kind == input_event_summary_kind::long_press,
        "diagnostic long press policy carries normalized long press");
    require(long_press_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::tracking,
        "diagnostic long press policy records tracked pointer before update");
    require(long_press_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::tracking,
        "diagnostic long press policy records tracked pointer after update");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::tracking,
        false,
        1,
        1,
        "diagnostic long press keeps pointer tracked until release");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 810, 10.0f, 10.0f)).empty(),
        "diagnostic long press release is suppressed");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 900, 0.0f, 0.0f)).empty(),
        "diagnostic drag down emits no gesture");
    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 920, 9.0f, 4.0f));
    require(events.size() == 1, "diagnostic drag start emits one event");
    const auto& drag_start_summaries = engine.routing_diagnostics().normalized_events;
    require(drag_start_summaries.size() == 1, "diagnostic drag start emits one summary");
    require(drag_start_summaries[0].kind == input_event_summary_kind::drag_start,
        "diagnostic drag start summary kind is drag start");
    require(drag_start_summaries[0].delta_x == 9.0f, "diagnostic drag start delta x is preserved");
    require(drag_start_summaries[0].delta_y == 4.0f, "diagnostic drag start delta y is preserved");
    const action_route_policy_diagnostic& drag_start_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "diagnostic drag start policy is gesture snapshot");
    require(drag_start_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::tracking,
        "diagnostic drag start policy records tracked pointer before capture");
    require(drag_start_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::captured,
        "diagnostic drag start policy records captured pointer after capture");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::captured,
        true,
        1,
        1,
        "diagnostic drag start captures pointer");

    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 940, 12.0f, 8.0f));
    require(events.size() == 1, "diagnostic drag update emits one event");
    const auto& drag_update_summaries = engine.routing_diagnostics().normalized_events;
    require(drag_update_summaries.size() == 1, "diagnostic drag update emits one summary");
    require(drag_update_summaries[0].kind == input_event_summary_kind::drag_update,
        "diagnostic drag update summary kind is drag update");
    require(drag_update_summaries[0].delta_x == 3.0f, "diagnostic drag update delta x is previous-relative");
    require(drag_update_summaries[0].delta_y == 4.0f, "diagnostic drag update delta y is previous-relative");
    const action_route_policy_diagnostic& drag_update_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "diagnostic drag update policy is gesture snapshot");
    require(drag_update_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::captured,
        "diagnostic drag update policy records captured pointer before update");
    require(drag_update_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::captured,
        "diagnostic drag update policy records captured pointer after update");

    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 960, 14.0f, 10.0f));
    require(events.size() == 1, "diagnostic drag end emits one event");
    const auto& drag_end_summaries = engine.routing_diagnostics().normalized_events;
    require(drag_end_summaries.size() == 1, "diagnostic drag end emits one summary");
    require(drag_end_summaries[0].kind == input_event_summary_kind::drag_end,
        "diagnostic drag end summary kind is drag end");
    const action_route_policy_diagnostic& drag_end_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "diagnostic drag end policy is gesture snapshot");
    require(drag_end_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::captured,
        "diagnostic drag end policy records captured pointer before release");
    require(drag_end_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::idle,
        "diagnostic drag end policy records idle pointer after release");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "diagnostic drag end releases capture");

    events = engine.process_scroll_event(scroll(1000, 40.0f, 50.0f, 0.0f, -3.0f, scroll_delta_unit::lines));
    require(events.size() == 1, "diagnostic wheel emits one event");
    const auto& wheel_summaries = engine.routing_diagnostics().normalized_events;
    require(wheel_summaries.size() == 1, "diagnostic wheel emits one summary");
    require(wheel_summaries[0].kind == input_event_summary_kind::wheel, "diagnostic wheel summary kind is wheel");
    require(wheel_summaries[0].x == 40.0f, "diagnostic wheel summary x is preserved");
    require(wheel_summaries[0].y == 50.0f, "diagnostic wheel summary y is preserved");
    require(wheel_summaries[0].line_delta_y == -3.0f, "diagnostic wheel line delta is preserved");
    require(wheel_summaries[0].pixel_delta_y == 0.0f, "diagnostic wheel pixel delta defaults to zero");
    const action_route_policy_diagnostic& wheel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::wheel_summary,
        "diagnostic wheel action route policy is emitted");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "diagnostic wheel emits one action route policy");
    require(wheel_policy.emits_input_event, "diagnostic wheel policy marks emitted input event");
    require(wheel_policy.event_index == 0, "diagnostic wheel policy points at first emitted event");
    require(wheel_policy.normalized_event.kind == input_event_summary_kind::wheel,
        "diagnostic wheel policy includes normalized wheel summary");
    require(wheel_policy.normalized_event.line_delta_y == -3.0f,
        "diagnostic wheel policy preserves line delta");
}

void test_gesture_routing_diagnostics_cancel_and_focus_loss()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 100, 0.0f, 0.0f)).empty(),
        "diagnostic cancel down emits no gesture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 120, 9.0f, 0.0f)).size() == 1,
        "diagnostic cancel setup starts drag");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::captured,
        true,
        1,
        1,
        "diagnostic cancel setup captures pointer");

    std::vector<input_event> events =
        engine.process_raw_event(pointer(raw_platform_pointer_phase::cancel, 130, 12.0f, 0.0f));
    require(events.size() == 1, "diagnostic cancel emits one event");
    const auto& cancel_summaries = engine.routing_diagnostics().normalized_events;
    require(cancel_summaries.size() == 1, "diagnostic cancel emits one summary");
    require(cancel_summaries[0].kind == input_event_summary_kind::drag_cancel,
        "diagnostic cancel summary kind is drag cancel");
    require(cancel_summaries[0].delta_x == 3.0f, "diagnostic cancel delta x is previous-relative");
    const action_route_policy_diagnostic& cancel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "diagnostic cancel policy is gesture snapshot");
    require(cancel_policy.normalized_event.kind == input_event_summary_kind::drag_cancel,
        "diagnostic cancel policy carries normalized drag cancel");
    require(cancel_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::captured,
        "diagnostic cancel policy records captured pointer before cancel");
    require(cancel_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::idle,
        "diagnostic cancel policy records idle pointer after cancel");
    require(engine.routing_diagnostics().action_routes.size() == 2,
        "diagnostic cancel emits gesture and pointer reset policies");
    const action_route_policy_diagnostic& cancel_reset_policy = require_policy(
        engine.routing_diagnostics(),
        1,
        action_route_policy_kind::pointer_capture_reset,
        "diagnostic cancel pointer reset policy is emitted");
    require(!cancel_reset_policy.emits_input_event, "diagnostic cancel reset policy emits no input event");
    require(cancel_reset_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::captured,
        "diagnostic cancel reset policy records captured pointer before cancel");
    require(cancel_reset_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::idle,
        "diagnostic cancel reset policy records idle pointer after cancel");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "diagnostic cancel releases pointer capture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 140, 12.0f, 0.0f)).empty(),
        "diagnostic up after cancel emits no stale event");
    require(engine.routing_diagnostics().normalized_events.empty(),
        "diagnostic up after cancel clears stale summaries");
    require(engine.routing_diagnostics().action_routes.empty(),
        "diagnostic up after cancel emits no stale policies");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 200, 0.0f, 0.0f)).empty(),
        "diagnostic focus loss down emits no gesture");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 220, 9.0f, 0.0f)).size() == 1,
        "diagnostic focus loss setup starts drag");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::captured,
        true,
        1,
        1,
        "diagnostic focus loss setup captures pointer");

    events = engine.process_raw_event(focus(raw_platform_focus_phase::lost, 230));
    require(events.empty(), "diagnostic focus loss without text focus emits no input events");
    require(engine.routing_diagnostics().normalized_events.empty(),
        "diagnostic focus loss clears gesture summaries");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "diagnostic focus loss resets pointer capture snapshot");
    require(engine.routing_diagnostics().action_routes.size() == 2,
        "diagnostic focus loss emits pointer reset and focus loss policies");
    const action_route_policy_diagnostic& reset_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_reset,
        "diagnostic focus loss first policy resets pointer capture");
    require(!reset_policy.emits_input_event, "diagnostic pointer reset policy emits no input event");
    require(reset_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::captured,
        "diagnostic pointer reset policy records captured state before reset");
    require(reset_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::idle,
        "diagnostic pointer reset policy records idle state after reset");
    const action_route_policy_diagnostic& focus_policy = require_policy(
        engine.routing_diagnostics(),
        1,
        action_route_policy_kind::focus_loss,
        "diagnostic focus loss second policy records focus loss");
    require(!focus_policy.emits_input_event, "diagnostic focus loss without text focus emits no input event");
    require(focus_policy.event_index == 0, "diagnostic focus loss without text focus points at zero event index");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 240, 9.0f, 0.0f)).empty(),
        "diagnostic up after focus loss emits no stale event");
}

void test_pointer_cancel_policy_records_non_emitting_stream_cleanup()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 100, 0.0f, 0.0f, raw_platform_pointer_button::none, 41))
                .empty(),
        "non-emitting cancel down starts tracking");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::tracking,
        false,
        41,
        1,
        "non-emitting cancel down records tracked pointer");

    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::cancel,
        120,
        70.0f,
        0.0f,
        raw_platform_pointer_button::none,
        41));
    require(events.empty(), "non-emitting cancel produces no gesture event");
    require(engine.routing_diagnostics().normalized_events.empty(),
        "non-emitting cancel produces no normalized gesture summary");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "non-emitting cancel emits one pointer reset policy");
    const action_route_policy_diagnostic& cancel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_reset,
        "non-emitting cancel policy records pointer reset");
    require(!cancel_policy.emits_input_event, "non-emitting cancel policy emits no input event");
    require(cancel_policy.timestamp_ms == 120, "non-emitting cancel policy preserves timestamp");
    require(cancel_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::tracking,
        "non-emitting cancel policy records tracked pointer before cancel");
    require(cancel_policy.pointer_capture_before.pointer_id == 41,
        "non-emitting cancel policy records canceled pointer id before cancel");
    require(cancel_policy.pointer_capture_before.tracked_pointer_count == 1,
        "non-emitting cancel policy records one tracked pointer before cancel");
    require(cancel_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::idle,
        "non-emitting cancel policy records idle pointer after cancel");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "non-emitting cancel clears pointer tracking");
    require(engine.update_time(800).empty(), "non-emitting cancel clears pending long press");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 810, 70.0f, 0.0f, raw_platform_pointer_button::none, 41))
                .empty(),
        "non-emitting cancel suppresses stale release");
    require(engine.routing_diagnostics().action_routes.empty(),
        "stale release after non-emitting cancel emits no policies");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 1000, 0.0f, 0.0f, raw_platform_pointer_button::none, 3))
                .empty(),
        "isolated cancel first pointer down starts tracking");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 1010, 40.0f, 0.0f, raw_platform_pointer_button::none, 9))
                .empty(),
        "isolated cancel second pointer down starts tracking");
    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::cancel,
        1020,
        0.0f,
        0.0f,
        raw_platform_pointer_button::none,
        3));
    require(events.empty(), "isolated cancel emits no gesture event");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "isolated cancel emits one pointer reset policy");
    const action_route_policy_diagnostic& isolated_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_reset,
        "isolated cancel policy records pointer reset");
    require(isolated_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::tracking,
        "isolated cancel policy records tracking before cancel");
    require(isolated_policy.pointer_capture_before.pointer_id == 3,
        "isolated cancel policy records lowest tracked pointer before cancel");
    require(isolated_policy.pointer_capture_before.tracked_pointer_count == 2,
        "isolated cancel policy records both tracked pointers before cancel");
    require(isolated_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::tracking,
        "isolated cancel policy records remaining tracked pointer after cancel");
    require(isolated_policy.pointer_capture_after.pointer_id == 9,
        "isolated cancel policy records remaining pointer id after cancel");
    require(isolated_policy.pointer_capture_after.tracked_pointer_count == 1,
        "isolated cancel policy records one remaining pointer after cancel");

    events = engine.update_time(1610);
    require(events.size() == 1, "isolated cancel keeps uncanceled pointer eligible for long press");
    const gesture_event& long_press = require_event<gesture_event>(events, 0);
    require(long_press.kind == gesture_kind::long_press,
        "isolated cancel long press emits for remaining pointer");
    require(long_press.pointer_id == 9, "isolated cancel long press preserves remaining pointer id");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::cancel,
        1620,
        40.0f,
        0.0f,
        raw_platform_pointer_button::none,
        9));
    require(events.empty(), "long-pressed pointer cancel emits no gesture event");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "long-pressed pointer cancel emits pointer reset policy");
    const action_route_policy_diagnostic& long_press_cancel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_reset,
        "long-pressed pointer cancel policy records pointer reset");
    require(long_press_cancel_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::tracking,
        "long-pressed cancel policy records tracked state before cancel");
    require(long_press_cancel_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::idle,
        "long-pressed cancel policy records idle state after cancel");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 1630, 40.0f, 0.0f, raw_platform_pointer_button::none, 9))
                .empty(),
        "long-pressed pointer cancel suppresses stale release");
}

void test_pointer_capture_arbitration_diagnostics()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 100, 0.0f, 0.0f, raw_platform_pointer_button::none, 10))
                .empty(),
        "arbitration first pointer down emits no input event");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "arbitration first pointer down emits tracking policy");
    const action_route_policy_diagnostic& first_track_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_arbitration,
        "arbitration first pointer down policy is emitted");
    require(first_track_policy.pointer_decision == pointer_arbitration_decision::tracked,
        "arbitration first pointer down records tracked decision");
    require(first_track_policy.pointer_event_phase == pointer_phase::down,
        "arbitration first pointer down records down phase");
    require(first_track_policy.pointer_id == 10, "arbitration first pointer down records pointer id");
    require_capture_snapshot(
        first_track_policy.pointer_capture_before,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "arbitration first pointer down records idle before");
    require_capture_snapshot(
        first_track_policy.pointer_capture_after,
        pointer_capture_lifecycle::tracking,
        false,
        10,
        1,
        "arbitration first pointer down records tracked after");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 110, 50.0f, 0.0f, raw_platform_pointer_button::none, 20))
                .empty(),
        "arbitration second pointer down emits no input event");
    const action_route_policy_diagnostic& second_track_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_arbitration,
        "arbitration second pointer down policy is emitted");
    require(second_track_policy.pointer_decision == pointer_arbitration_decision::tracked,
        "arbitration second pointer down records tracked decision");
    require(second_track_policy.pointer_id == 20, "arbitration second pointer down records pointer id");
    require_capture_snapshot(
        second_track_policy.pointer_capture_before,
        pointer_capture_lifecycle::tracking,
        false,
        10,
        1,
        "arbitration second pointer down records first tracked pointer before");
    require_capture_snapshot(
        second_track_policy.pointer_capture_after,
        pointer_capture_lifecycle::tracking,
        false,
        10,
        2,
        "arbitration second pointer down records deterministic lowest pointer id after");

    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::move,
        130,
        59.0f,
        0.0f,
        raw_platform_pointer_button::none,
        20));
    require(events.size() == 1, "arbitration second pointer starts captured drag");
    const gesture_event& start = require_event<gesture_event>(events, 0);
    require(start.kind == gesture_kind::drag_start, "arbitration second pointer emits drag start");
    const action_route_policy_diagnostic& capture_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "arbitration drag start policy is gesture snapshot");
    require(capture_policy.pointer_decision == pointer_arbitration_decision::captured,
        "arbitration drag start records captured decision");
    require(capture_policy.pointer_event_phase == pointer_phase::move,
        "arbitration drag start records move phase");
    require(capture_policy.pointer_id == 20, "arbitration drag start records pointer id");
    require_capture_snapshot(
        capture_policy.pointer_capture_before,
        pointer_capture_lifecycle::tracking,
        false,
        10,
        2,
        "arbitration drag start records both tracked pointers before capture");
    require_capture_snapshot(
        capture_policy.pointer_capture_after,
        pointer_capture_lifecycle::captured,
        true,
        20,
        1,
        "arbitration drag start records captured pointer after");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::cancel,
        140,
        0.0f,
        0.0f,
        raw_platform_pointer_button::none,
        10));
    require(events.empty(), "arbitration cancel for noncaptured pointer is ignored");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "arbitration ignored cancel emits one diagnostic policy");
    const action_route_policy_diagnostic& ignored_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_arbitration,
        "arbitration ignored cancel policy is emitted");
    require(ignored_policy.pointer_decision == pointer_arbitration_decision::ignored_by_capture,
        "arbitration ignored cancel records ignored decision");
    require(ignored_policy.pointer_event_phase == pointer_phase::cancel,
        "arbitration ignored cancel records cancel phase");
    require(ignored_policy.pointer_id == 10, "arbitration ignored cancel records ignored pointer id");
    require_capture_snapshot(
        ignored_policy.pointer_capture_before,
        pointer_capture_lifecycle::captured,
        true,
        20,
        1,
        "arbitration ignored cancel records captured pointer before");
    require_capture_snapshot(
        ignored_policy.pointer_capture_after,
        pointer_capture_lifecycle::captured,
        true,
        20,
        1,
        "arbitration ignored cancel keeps captured pointer after");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        160,
        65.0f,
        2.0f,
        raw_platform_pointer_button::none,
        20));
    require(events.size() == 1, "arbitration captured pointer up emits drag end");
    const action_route_policy_diagnostic& release_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "arbitration drag end policy is gesture snapshot");
    require(release_policy.pointer_decision == pointer_arbitration_decision::released,
        "arbitration drag end records released decision");
    require(release_policy.pointer_event_phase == pointer_phase::up,
        "arbitration drag end records up phase");
    require(release_policy.pointer_id == 20, "arbitration drag end records pointer id");
    require_capture_snapshot(
        release_policy.pointer_capture_before,
        pointer_capture_lifecycle::captured,
        true,
        20,
        1,
        "arbitration drag end records captured before release");
    require_capture_snapshot(
        release_policy.pointer_capture_after,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "arbitration drag end records idle after release");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 200, 0.0f, 0.0f, raw_platform_pointer_button::none, 30))
                .empty(),
        "arbitration restart down emits no input event");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 220, 10.0f, 0.0f, raw_platform_pointer_button::none, 30))
                .size() == 1,
        "arbitration restart setup captures pointer");
    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::down,
        240,
        2.0f,
        2.0f,
        raw_platform_pointer_button::none,
        30));
    require(events.size() == 1, "arbitration repeated down cancels captured drag");
    const gesture_event& restart_cancel = require_event<gesture_event>(events, 0);
    require(restart_cancel.kind == gesture_kind::drag_cancel,
        "arbitration repeated down emits drag cancel");
    const action_route_policy_diagnostic& restart_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "arbitration repeated down policy is gesture snapshot");
    require(restart_policy.pointer_decision == pointer_arbitration_decision::restarted,
        "arbitration repeated down records restarted decision");
    require(restart_policy.pointer_event_phase == pointer_phase::down,
        "arbitration repeated down records down phase");
    require(restart_policy.pointer_id == 30, "arbitration repeated down records pointer id");
    require_capture_snapshot(
        restart_policy.pointer_capture_before,
        pointer_capture_lifecycle::captured,
        true,
        30,
        1,
        "arbitration repeated down records captured before restart");
    require_capture_snapshot(
        restart_policy.pointer_capture_after,
        pointer_capture_lifecycle::tracking,
        false,
        30,
        1,
        "arbitration repeated down records replacement tracking after restart");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::move, 260, 11.0f, 2.0f, raw_platform_pointer_button::none, 30))
                .size() == 1,
        "arbitration restarted pointer captures again");
    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::cancel,
        270,
        11.0f,
        4.0f,
        raw_platform_pointer_button::none,
        30));
    require(events.size() == 1, "arbitration captured pointer cancel emits drag cancel");
    require(engine.routing_diagnostics().action_routes.size() == 2,
        "arbitration captured pointer cancel emits gesture and reset policies");
    const action_route_policy_diagnostic& cancel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "arbitration captured cancel first policy is gesture snapshot");
    require(cancel_policy.pointer_decision == pointer_arbitration_decision::canceled,
        "arbitration captured cancel gesture records canceled decision");
    require(cancel_policy.pointer_event_phase == pointer_phase::cancel,
        "arbitration captured cancel gesture records cancel phase");
    const action_route_policy_diagnostic& cancel_reset_policy = require_policy(
        engine.routing_diagnostics(),
        1,
        action_route_policy_kind::pointer_capture_reset,
        "arbitration captured cancel second policy is reset");
    require(cancel_reset_policy.pointer_decision == pointer_arbitration_decision::canceled,
        "arbitration captured cancel reset records canceled decision");
    require(cancel_reset_policy.pointer_event_phase == pointer_phase::cancel,
        "arbitration captured cancel reset records cancel phase");
    require(cancel_reset_policy.pointer_id == 30,
        "arbitration captured cancel reset records pointer id");
    require_capture_snapshot(
        cancel_reset_policy.pointer_capture_before,
        pointer_capture_lifecycle::captured,
        true,
        30,
        1,
        "arbitration captured cancel reset records captured before");
    require_capture_snapshot(
        cancel_reset_policy.pointer_capture_after,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "arbitration captured cancel reset records idle after");
}

void test_text_key_flow()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(text(100, "ignored")).empty(), "unfocused text is ignored");

    engine.focus_text_target("answer");
    std::vector<input_event> events = engine.process_raw_event(text(110, utf8(u8"한")));
    require(events.size() == 1, "focused text commit emits one event");
    const text_event& commit = require_event<text_event>(events, 0);
    require(commit.kind == text_event_kind::commit, "text event is commit");
    require(commit.target_id == "answer", "text target is preserved");
    require(commit.utf8_text == utf8(u8"한"), "utf8 commit text is preserved");
    require(engine.text_model().text() == utf8(u8"한"), "text model stores committed utf8");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "utf8 text commit emits one edit boundary policy");
    const action_route_policy_diagnostic& text_commit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_commit_boundary,
        "utf8 text commit policy is emitted");
    require(text_commit_policy.text_byte_count == std::string(utf8(u8"한")).size(),
        "utf8 text commit policy records committed byte count");
    require(text_commit_policy.text_byte_count_before == 0, "utf8 text commit policy records empty before text");
    require(text_commit_policy.text_byte_count_after == std::string(utf8(u8"한")).size(),
        "utf8 text commit policy records after text byte count");
    require_range(text_commit_policy.caret_before, 0, 0, "utf8 text commit policy records caret before commit");
    require_range(text_commit_policy.caret_after,
        std::string(utf8(u8"한")).size(),
        std::string(utf8(u8"한")).size(),
        "utf8 text commit policy records caret after commit");

    require(engine.process_raw_event(key(120, "Backspace", false, raw_platform_key_phase::up)).empty(),
        "backspace keyup is ignored");
    events = engine.process_raw_event(key(130, "Backspace", true));
    require(events.size() == 1, "repeat backspace deletes text");
    const text_event& backspace = require_event<text_event>(events, 0);
    require(backspace.kind == text_event_kind::backspace, "backspace event is emitted");
    require(engine.text_model().text().empty(), "backspace removes utf8 codepoint");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "utf8 backspace emits one edit boundary policy");
    const action_route_policy_diagnostic& backspace_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_backspace_boundary,
        "utf8 backspace policy is emitted");
    require(backspace_policy.text_byte_count == std::string(utf8(u8"한")).size(),
        "utf8 backspace policy records deleted codepoint byte count");
    require(backspace_policy.text_byte_count_before == std::string(utf8(u8"한")).size(),
        "utf8 backspace policy records before byte count");
    require(backspace_policy.text_byte_count_after == 0, "utf8 backspace policy records after byte count");
    require_range(backspace_policy.caret_before,
        std::string(utf8(u8"한")).size(),
        std::string(utf8(u8"한")).size(),
        "utf8 backspace policy records caret before delete");
    require_range(backspace_policy.caret_after, 0, 0, "utf8 backspace policy records caret after delete");

    require(engine.process_raw_event(text(140, "ok")).size() == 1, "second text commit succeeds");
    events = engine.process_raw_event(key(150, "Enter"));
    require(events.size() == 1, "enter submits once");
    const text_event& submit = require_event<text_event>(events, 0);
    require(submit.kind == text_event_kind::submit, "submit event is emitted");
    require(submit.utf8_text == "ok", "submit carries committed buffer");
    require(engine.text_model().text().empty(), "submit clears editing buffer");
    require(engine.text_model().has_submit_text(), "text model retains consumable submit text");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "submit emits one action route policy");
    const action_route_policy_diagnostic& submit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_submit_boundary,
        "submit action route policy is emitted");
    require(submit_policy.emits_input_event, "submit policy marks emitted event");
    require(submit_policy.event_index == 0, "submit policy points at submit event index");
    require(submit_policy.target_id == "answer", "submit policy preserves target id");
    require(submit_policy.text_byte_count == 2, "submit policy records submitted byte count before clear");
    require(submit_policy.text_byte_count_before == 2, "submit policy records before byte count");
    require(submit_policy.text_byte_count_after == 0, "submit policy records cleared after byte count");
    require_range(submit_policy.caret_before, 2, 2, "submit policy records caret before submit");
    require_range(submit_policy.caret_after, 0, 0, "submit policy records caret after submit");

    require(engine.process_raw_event(key(151, "Enter", true)).empty(), "repeat enter is ignored");
    require(engine.routing_diagnostics().action_routes.empty(), "repeat enter emits no submit policy");
}

void test_key_code_fallback_edges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(key_code(100, 8)).empty(), "unfocused key-code backspace is ignored");

    engine.focus_text_target("answer");
    require(engine.process_raw_event(text(110, utf8(u8"한"))).size() == 1, "text before key-code backspace commits");
    std::vector<input_event> events = engine.process_raw_event(key_code(120, 8));
    require(events.size() == 1, "key-code backspace emits one text event");
    const text_event& backspace = require_event<text_event>(events, 0);
    require(backspace.kind == text_event_kind::backspace, "key-code backspace emits backspace kind");
    require(backspace.target_id == "answer", "key-code backspace preserves target id");
    require(engine.text_model().text().empty(), "key-code backspace removes utf8 codepoint");

    require(engine.process_raw_event(text(130, "ok")).size() == 1, "text before key-code enter commits");
    require(engine.process_raw_event(key_code(140, 13, raw_platform_key_phase::up)).empty(),
        "key-code enter keyup is ignored");
    events = engine.process_raw_event(key_code(150, 13));
    require(events.size() == 1, "key-code enter emits one submit event");
    const text_event& submit = require_event<text_event>(events, 0);
    require(submit.kind == text_event_kind::submit, "key-code enter emits submit kind");
    require(submit.target_id == "answer", "key-code enter preserves target id");
    require(submit.utf8_text == "ok", "key-code enter submits committed text");
}

void test_text_keyboard_navigation_and_selection()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(key(90, "ArrowLeft")).empty(), "unfocused arrow left is ignored");

    engine.focus_text_target("answer");
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    require(engine.process_raw_event(text(100, initial)).size() == 1, "text before navigation commits");
    require(engine.text_model().caret_byte_offset() == initial.size(), "caret starts at committed end");

    require(engine.process_raw_event(key(110, "ArrowLeft", false, raw_platform_key_phase::up)).empty(),
        "arrow keyup is ignored");
    std::vector<input_event> events = engine.process_raw_event(key(120, "ArrowLeft"));
    require(events.size() == 1, "arrow left emits one caret event");
    const text_event& left = require_event<text_event>(events, 0);
    require(left.kind == text_event_kind::caret_moved, "arrow left emits caret moved kind");
    require(left.target_id == "answer", "arrow left preserves target id");
    require(engine.text_model().caret_byte_offset() == 4, "arrow left moves over trailing ascii");
    require(!engine.text_model().selection_range().has_value(), "plain arrow left leaves no selection");
    const action_route_policy_diagnostic& left_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::caret_moved,
        "arrow left emits caret policy");
    require_range(left_policy.caret_before, initial.size(), initial.size(),
        "arrow left policy records caret before");
    require_range(left_policy.caret_after, 4, 4, "arrow left policy records caret after");
    require(!left_policy.had_selection_before, "arrow left policy records no prior selection");
    require(!left_policy.has_selection_after, "arrow left policy records no resulting selection");

    events = engine.process_raw_event(key(130, "ArrowLeft"));
    require(events.size() == 1, "second arrow left emits one caret event");
    require(require_event<text_event>(events, 0).kind == text_event_kind::caret_moved,
        "second arrow left emits caret moved kind");
    require(engine.text_model().caret_byte_offset() == 1, "second arrow left moves over utf8 codepoint");

    events = engine.process_raw_event(key(140, "ArrowRight"));
    require(events.size() == 1, "arrow right emits one caret event");
    require(require_event<text_event>(events, 0).kind == text_event_kind::caret_moved,
        "arrow right emits caret moved kind");
    require(engine.text_model().caret_byte_offset() == 4, "arrow right moves over utf8 codepoint");

    events = engine.process_raw_event(key(150, "Home"));
    require(events.size() == 1, "home emits one caret event");
    require(require_event<text_event>(events, 0).kind == text_event_kind::caret_moved, "home emits caret moved kind");
    require(engine.text_model().caret_byte_offset() == 0, "home moves caret to start");
    events = engine.process_raw_event(key(151, "Home"));
    require(events.empty(), "home at start emits no event");
    const action_route_policy_diagnostic& home_edge_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::caret_moved,
        "home at start emits caret edge policy");
    require(!home_edge_policy.emits_input_event, "home at start policy emits no input event");
    require_range(home_edge_policy.caret_before, 0, 0, "home at start policy records caret before");
    require_range(home_edge_policy.caret_after, 0, 0, "home at start policy records caret after");
    require(home_edge_policy.text_byte_count_before == initial.size(),
        "home at start policy records before text byte count");
    require(home_edge_policy.text_byte_count_after == initial.size(),
        "home at start policy records after text byte count");

    events = engine.process_raw_event(key(160, "End"));
    require(events.size() == 1, "end emits one caret event");
    require(require_event<text_event>(events, 0).kind == text_event_kind::caret_moved, "end emits caret moved kind");
    require(engine.text_model().caret_byte_offset() == initial.size(), "end moves caret to committed end");
    events = engine.process_raw_event(key(161, "End"));
    require(events.empty(), "end at end emits no event");
    const action_route_policy_diagnostic& end_edge_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::caret_moved,
        "end at end emits caret edge policy");
    require(!end_edge_policy.emits_input_event, "end at end policy emits no input event");
    require_range(end_edge_policy.caret_before, initial.size(), initial.size(),
        "end at end policy records caret before");
    require_range(end_edge_policy.caret_after, initial.size(), initial.size(),
        "end at end policy records caret after");

    events = engine.process_raw_event(key(170, "ArrowLeft", false, raw_platform_key_phase::down, false, true));
    require(events.size() == 1, "shift arrow left emits one selection event");
    const text_event& shift_left = require_event<text_event>(events, 0);
    require(shift_left.kind == text_event_kind::selection_changed, "shift arrow left emits selection changed kind");
    auto selection = engine.text_model().selection_range();
    require(selection.has_value(), "shift arrow left exposes selection");
    require_range(*selection, 4, initial.size(), "shift arrow left selects trailing ascii");
    require(engine.text_model().caret_byte_offset() == 4, "shift arrow left places active caret at selection start");
    const action_route_policy_diagnostic& shift_left_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::selection_changed,
        "shift arrow left emits selection policy");
    require_range(shift_left_policy.caret_before, initial.size(), initial.size(),
        "shift arrow left policy records caret before");
    require_range(shift_left_policy.caret_after, 4, 4, "shift arrow left policy records caret after");
    require(!shift_left_policy.had_selection_before, "shift arrow left policy records no prior selection");
    require(shift_left_policy.has_selection_after, "shift arrow left policy records resulting selection");
    require_range(shift_left_policy.selection_after, 4, initial.size(),
        "shift arrow left policy records selected range");

    events = engine.process_raw_event(key(180, "ArrowLeft", false, raw_platform_key_phase::down, false, true));
    require(events.size() == 1, "second shift arrow left emits one selection event");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "second shift arrow left keeps selection");
    require_range(*selection, 1, initial.size(), "second shift arrow left extends over utf8 codepoint");
    require(engine.text_model().caret_byte_offset() == 1, "second shift arrow left updates active caret");

    events = engine.process_raw_event(key(190, "ArrowRight", false, raw_platform_key_phase::down, false, true));
    require(events.size() == 1, "shift arrow right shrinks selection");
    require(require_event<text_event>(events, 0).kind == text_event_kind::selection_changed,
        "shift arrow right emits selection changed kind");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "shift arrow right keeps selection while not collapsed");
    require_range(*selection, 4, initial.size(), "shift arrow right shrinks over utf8 codepoint");
    require(engine.text_model().caret_byte_offset() == 4, "shift arrow right updates active caret");

    events = engine.process_raw_event(key(200, "ArrowRight"));
    require(events.size() == 1, "plain arrow right collapses selection");
    require(require_event<text_event>(events, 0).kind == text_event_kind::caret_moved,
        "plain arrow collapse emits caret moved kind");
    require(!engine.text_model().selection_range().has_value(), "plain arrow right clears selection");
    require(engine.text_model().caret_byte_offset() == initial.size(), "plain arrow right collapses to selection end");
    const action_route_policy_diagnostic& collapse_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::caret_moved,
        "plain arrow collapse emits caret policy");
    require(collapse_policy.had_selection_before, "plain arrow collapse policy records prior selection");
    require(!collapse_policy.has_selection_after, "plain arrow collapse policy clears selection");
    require_range(collapse_policy.selection_before, 4, initial.size(),
        "plain arrow collapse policy records prior selection range");
    require_range(collapse_policy.caret_after, initial.size(), initial.size(),
        "plain arrow collapse policy records collapsed caret");

    events = engine.process_raw_event(key(210, "a", false, raw_platform_key_phase::down, true));
    require(events.size() == 1, "ctrl+a emits one selection event");
    const text_event& ctrl_a = require_event<text_event>(events, 0);
    require(ctrl_a.kind == text_event_kind::selection_changed, "ctrl+a emits selection changed kind");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "ctrl+a exposes selection");
    require_range(*selection, 0, initial.size(), "ctrl+a selects all committed text");
    const action_route_policy_diagnostic& select_all_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::selection_changed,
        "ctrl+a emits selection policy");
    require(select_all_policy.has_selection_after, "ctrl+a policy records resulting selection");
    require_range(select_all_policy.selection_after, 0, initial.size(), "ctrl+a policy records full selection");
    events = engine.process_raw_event(key(211, "a", false, raw_platform_key_phase::down, true));
    require(events.empty(), "repeat select all without state change emits no event");
    const action_route_policy_diagnostic& select_all_edge_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::selection_changed,
        "repeat select all emits selection edge policy");
    require(!select_all_edge_policy.emits_input_event, "repeat select all policy emits no input event");
    require(select_all_edge_policy.had_selection_before, "repeat select all policy records prior selection");
    require(select_all_edge_policy.has_selection_after, "repeat select all policy records retained selection");
    require_range(select_all_edge_policy.selection_before, 0, initial.size(),
        "repeat select all policy records before range");
    require_range(select_all_edge_policy.selection_after, 0, initial.size(),
        "repeat select all policy records after range");

    require(engine.process_raw_event(key(220, "Home")).size() == 1, "home clears selection before meta+a");
    events = engine.process_raw_event(key(230, "A", false, raw_platform_key_phase::down, false, false, true));
    require(events.size() == 1, "meta+a emits one selection event");
    require(require_event<text_event>(events, 0).kind == text_event_kind::selection_changed,
        "meta+a emits selection changed kind");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "meta+a exposes selection");
    require_range(*selection, 0, initial.size(), "meta+a selects all committed text");
}

void test_keyboard_focus_traversal_diagnostics()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    require(engine.process_raw_event(text(100, initial)).size() == 1,
        "focus traversal initial text commits");

    std::vector<input_event> events = engine.process_raw_event(key(110, "Tab"));
    require(events.empty(), "tab emits no app input event from input engine");
    require(engine.has_text_focus(), "tab diagnostics do not clear text focus");
    require(engine.text_focus_id() == "answer", "tab diagnostics preserve focus id");
    require(engine.text_model().text() == initial, "tab diagnostics preserve text");
    require(engine.text_model().caret_byte_offset() == initial.size(), "tab diagnostics preserve caret");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "tab emits one focus traversal diagnostic policy");
    const action_route_policy_diagnostic& next_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::focus_traversal_next,
        "tab emits forward traversal policy");
    require(!next_policy.emits_input_event, "tab traversal policy emits no input event");
    require(next_policy.target_id == "answer", "tab traversal policy preserves target id");
    require(next_policy.text_byte_count_before == initial.size(),
        "tab traversal policy records before text byte count");
    require(next_policy.text_byte_count_after == initial.size(),
        "tab traversal policy records after text byte count");
    require_range(next_policy.caret_before, initial.size(), initial.size(),
        "tab traversal policy records caret before");
    require_range(next_policy.caret_after, initial.size(), initial.size(),
        "tab traversal policy records caret after");
    require(!next_policy.had_selection_before, "tab traversal policy records no prior selection");
    require(!next_policy.has_selection_after, "tab traversal policy records no resulting selection");

    require(engine.process_raw_event(key(120, "ArrowLeft", false, raw_platform_key_phase::down, false, true)).size() == 1,
        "focus traversal setup selects trailing character");
    std::optional<text_range> selection = engine.text_model().selection_range();
    require(selection.has_value(), "focus traversal setup exposes selection");
    require_range(*selection, 4, initial.size(), "focus traversal setup selected trailing ascii");

    events = engine.process_raw_event(key(130, "Tab", false, raw_platform_key_phase::down, false, true));
    require(events.empty(), "shift tab emits no app input event from input engine");
    require(engine.has_text_focus(), "shift tab diagnostics do not clear text focus");
    require(engine.text_model().text() == initial, "shift tab diagnostics preserve text");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "shift tab diagnostics preserve selection");
    require_range(*selection, 4, initial.size(), "shift tab diagnostics preserve selected range");
    const action_route_policy_diagnostic& previous_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::focus_traversal_previous,
        "shift tab emits reverse traversal policy");
    require(!previous_policy.emits_input_event, "shift tab traversal policy emits no input event");
    require(previous_policy.had_selection_before, "shift tab traversal policy records prior selection");
    require(previous_policy.has_selection_after, "shift tab traversal policy records retained selection");
    require_range(previous_policy.selection_before, 4, initial.size(),
        "shift tab traversal policy records before selection");
    require_range(previous_policy.selection_after, 4, initial.size(),
        "shift tab traversal policy records after selection");
    require_range(previous_policy.caret_before, 4, 4, "shift tab traversal policy records caret before");
    require_range(previous_policy.caret_after, 4, 4, "shift tab traversal policy records caret after");
}

void test_keyboard_navigation_diagnostics_preserve_ime_composition()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    const std::string preedit_text = utf8(u8"ㅎ");
    const std::string committed_text = utf8(u8"한");
    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, preedit_text)).size() == 1,
        "navigation ime setup starts preedit");
    require(engine.text_model().ime_composition().active, "navigation ime setup has active composition");

    std::vector<input_event> events = engine.process_raw_event(key(110, "ArrowLeft"));
    require(events.empty(), "arrow left during ime emits no text event");
    require(engine.text_model().text().empty(), "arrow left during ime preserves committed text");
    require(engine.text_model().display_text() == preedit_text, "arrow left during ime preserves display preedit");
    require(!engine.text_model().selection_range().has_value(), "arrow left during ime creates no selection");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "arrow left during ime emits one navigation diagnostic policy");
    const action_route_policy_diagnostic& caret_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::caret_moved,
        "arrow left during ime emits caret diagnostic policy");
    require(!caret_policy.emits_input_event, "arrow left during ime policy emits no input event");
    require(caret_policy.composition.active, "arrow left during ime policy carries composition");
    require(caret_policy.composition.preedit_text == preedit_text,
        "arrow left during ime policy carries preedit text");
    require_range(caret_policy.caret_before, preedit_text.size(), preedit_text.size(),
        "arrow left during ime policy records display caret before");
    require_range(caret_policy.caret_after, preedit_text.size(), preedit_text.size(),
        "arrow left during ime policy records display caret after");

    events = engine.process_raw_event(key(120, "ArrowRight", false, raw_platform_key_phase::down, false, true));
    require(events.empty(), "shift arrow during ime emits no selection event");
    require(engine.text_model().display_text() == preedit_text, "shift arrow during ime preserves display preedit");
    require(!engine.text_model().selection_range().has_value(), "shift arrow during ime creates no selection");
    const action_route_policy_diagnostic& selection_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::selection_changed,
        "shift arrow during ime emits selection diagnostic policy");
    require(!selection_policy.emits_input_event, "shift arrow during ime policy emits no input event");
    require(selection_policy.composition.active, "shift arrow during ime policy carries composition");
    require(selection_policy.composition.preedit_text == preedit_text,
        "shift arrow during ime policy carries preedit text");

    events = engine.process_raw_event(key(130, "Tab"));
    require(events.empty(), "tab during ime emits no focus traversal event");
    require(engine.has_text_focus(), "tab during ime preserves focus");
    require(engine.text_model().display_text() == preedit_text, "tab during ime preserves display preedit");
    const action_route_policy_diagnostic& traversal_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::focus_traversal_next,
        "tab during ime emits traversal diagnostic policy");
    require(!traversal_policy.emits_input_event, "tab during ime policy emits no input event");
    require(traversal_policy.composition.active, "tab during ime policy carries composition");
    require(traversal_policy.composition.preedit_text == preedit_text,
        "tab during ime policy carries preedit text");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::composition_end, 140, committed_text));
    require(events.size() == 1, "ime commit succeeds after suppressed navigation");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "ime commit after navigation emits commit kind");
    require(engine.text_model().text() == committed_text, "ime commit after navigation updates text");
    require(!engine.text_model().ime_composition().active, "ime commit after navigation clears composition");
}

void test_text_edit_boundary_diagnostics_replace_utf8_selection()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    require(engine.process_raw_event(text(100, initial)).size() == 1,
        "selection replacement initial text commits");

    require(engine.process_raw_event(key(110, "Home")).size() == 1,
        "selection replacement home moves caret");
    require(engine.process_raw_event(key(120, "ArrowRight")).size() == 1,
        "selection replacement arrow right moves after ascii");
    require(engine.process_raw_event(key(130, "ArrowRight", false, raw_platform_key_phase::down, false, true)).size() == 1,
        "selection replacement shift arrow selects utf8 codepoint");
    std::optional<text_range> selection = engine.text_model().selection_range();
    require(selection.has_value(), "selection replacement has selected utf8 codepoint");
    require_range(*selection, 1, 1 + std::string(utf8(u8"한")).size(),
        "selection replacement selected range covers full utf8 codepoint");

    std::vector<input_event> events = engine.process_raw_event(text(140, "Z"));
    require(events.size() == 1, "selection replacement text commit emits one event");
    const text_event& commit = require_event<text_event>(events, 0);
    require(commit.kind == text_event_kind::commit, "selection replacement emits commit kind");
    require(engine.text_model().text() == "AZB", "selection replacement updates text");
    require(engine.text_model().caret_byte_offset() == 2, "selection replacement caret follows inserted text");
    const action_route_policy_diagnostic& policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_commit_boundary,
        "selection replacement emits commit boundary policy");
    require(policy.text_byte_count == 1, "selection replacement policy records inserted byte count");
    require(policy.text_byte_count_before == initial.size(), "selection replacement policy records before byte count");
    require(policy.text_byte_count_after == 3, "selection replacement policy records after byte count");
    require(policy.had_selection_before, "selection replacement policy records prior selection");
    require(!policy.has_selection_after, "selection replacement policy clears selection after commit");
    require_range(policy.selection_before, 1, 1 + std::string(utf8(u8"한")).size(),
        "selection replacement policy records selected utf8 byte range");
    require_range(policy.caret_before,
        1 + std::string(utf8(u8"한")).size(),
        1 + std::string(utf8(u8"한")).size(),
        "selection replacement policy records active caret before commit");
    require_range(policy.caret_after, 2, 2, "selection replacement policy records caret after commit");
}

void test_ime_composition_suppresses_text_and_key_events()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::composition_start, 100)).empty(),
        "composition start is state only");
    require(engine.text_model().ime_composition().active, "composition start creates active empty composition");
    require_range(engine.text_model().ime_composition().preedit_range, 0, 0,
        "composition start preedit range is collapsed");
    std::vector<input_event> events =
        engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 110, utf8(u8"ㅎ")));
    require(events.size() == 1, "preedit emits one ime event");
    const ime_event& preedit = require_event<ime_event>(events, 0);
    require(preedit.kind == ime_event_kind::preedit, "preedit event is emitted");
    require(preedit.utf8_text == utf8(u8"ㅎ"), "preedit text is preserved");
    require(preedit.composition.active, "preedit event carries active composition");
    require(preedit.composition.preedit_text == utf8(u8"ㅎ"), "preedit event carries composition text");
    require_range(preedit.composition.replacement_range, 0, 0, "preedit event replacement range is caret collapsed");
    require_range(preedit.composition.preedit_range, 0, std::string(utf8(u8"ㅎ")).size(),
        "preedit event range covers hangul jamo bytes");
    require_range(preedit.composition.caret_range,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "preedit event caret follows hangul jamo");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "preedit emits one action route policy");
    const action_route_policy_diagnostic& preedit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "preedit action route policy is emitted");
    require(preedit_policy.emits_input_event, "preedit policy marks emitted input event");
    require(preedit_policy.event_index == 0, "preedit policy points at preedit event");
    require(preedit_policy.target_id == "answer", "preedit policy preserves target id");
    require(preedit_policy.text_byte_count == std::string(utf8(u8"ㅎ")).size(),
        "preedit policy records preedit byte count");
    require(preedit_policy.text_byte_count_before == 0, "preedit policy records committed bytes before");
    require(preedit_policy.text_byte_count_after == 0, "preedit policy preserves committed bytes after");
    require_range(preedit_policy.caret_before, 0, 0, "preedit policy records caret before preedit");
    require_range(preedit_policy.caret_after,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "preedit policy records display caret after preedit");
    require(preedit_policy.composition.active, "preedit policy carries active composition");
    require(preedit_policy.composition.preedit_text == utf8(u8"ㅎ"),
        "preedit policy carries composition preedit text");
    require_range(preedit_policy.composition.preedit_range, 0, std::string(utf8(u8"ㅎ")).size(),
        "preedit policy carries composition preedit range");
    require(engine.text_model().display_text() == utf8(u8"ㅎ"), "display text includes preedit");

    require(engine.process_raw_event(key(120, "Enter")).empty(), "enter is suppressed during composition");
    require(engine.process_raw_event(key(121, "ArrowLeft")).empty(), "arrow left is suppressed during composition");
    require(engine.process_raw_event(key(122, "ArrowLeft", false, raw_platform_key_phase::down, false, true)).empty(),
        "shift arrow left is suppressed during composition");
    require(engine.process_raw_event(key(123, "a", false, raw_platform_key_phase::down, true)).empty(),
        "ctrl+a is suppressed during composition");
    require(engine.process_raw_event(text(130, "duplicate")).empty(), "raw text is suppressed during composition");
    require(engine.text_model().text().empty(), "suppressed raw text does not commit");
    require(!engine.text_model().selection_range().has_value(), "suppressed composition navigation leaves no selection");
    require(engine.text_model().display_text() == utf8(u8"ㅎ"), "suppressed composition navigation preserves preedit");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::composition_end, 140, utf8(u8"한")));
    require(events.size() == 1, "composition end with text commits ime text");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "ime commit event is emitted");
    require(commit.utf8_text == utf8(u8"한"), "ime commit text is preserved");
    require(commit.composition.active, "ime commit carries composition snapshot before commit");
    require(commit.composition.preedit_text == utf8(u8"ㅎ"), "ime commit preserves previous preedit snapshot");
    require_range(commit.composition.replacement_range, 0, 0, "ime commit replacement range is original caret");
    require_range(commit.composition.preedit_range, 0, std::string(utf8(u8"ㅎ")).size(),
        "ime commit preedit range is previous hangul jamo range");
    require(engine.text_model().text() == utf8(u8"한"), "ime commit updates text model");
    require(engine.text_model().preedit_text().empty(), "ime commit clears preedit");
    require(!engine.text_model().ime_composition().active, "ime commit clears model composition state");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "ime commit emits one action route policy");
    const action_route_policy_diagnostic& commit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_commit,
        "ime commit action route policy is emitted");
    require(commit_policy.emits_input_event, "ime commit policy marks emitted event");
    require(commit_policy.event_index == 0, "ime commit policy points at first event");
    require(commit_policy.target_id == "answer", "ime commit policy preserves target id");
    require(commit_policy.text_byte_count == std::string(utf8(u8"한")).size(),
        "ime commit policy records utf8 byte count");
    require(commit_policy.composition.active, "ime commit policy carries pre-commit composition");
    require(commit_policy.composition.preedit_text == utf8(u8"ㅎ"),
        "ime commit policy carries pre-commit preedit text");
    require(commit_policy.text_byte_count_before == 0, "ime commit policy records empty committed text before");
    require(commit_policy.text_byte_count_after == std::string(utf8(u8"한")).size(),
        "ime commit policy records committed text after");
    require_range(commit_policy.caret_before,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "ime commit policy records display caret before commit");
    require_range(commit_policy.caret_after,
        std::string(utf8(u8"한")).size(),
        std::string(utf8(u8"한")).size(),
        "ime commit policy records committed caret after commit");
}

void test_ime_preedit_commit_edges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    std::vector<input_event> events =
        engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, utf8(u8"ㅎ")));
    require(events.size() == 1, "first preedit emits one event");
    const ime_event& first_preedit = require_event<ime_event>(events, 0);
    require(first_preedit.utf8_text == utf8(u8"ㅎ"), "first preedit text is emitted");
    require(first_preedit.composition.active, "first preedit carries active composition");
    require_range(first_preedit.composition.replacement_range, 0, 0, "first preedit replacement is collapsed");
    const action_route_policy_diagnostic& first_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "first preedit emits preedit policy");
    require(first_policy.text_byte_count == std::string(utf8(u8"ㅎ")).size(),
        "first preedit policy records jamo byte count");
    require_range(first_policy.caret_before, 0, 0, "first preedit policy records caret before");
    require_range(first_policy.caret_after,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "first preedit policy records caret after");
    require(engine.text_model().display_text() == utf8(u8"ㅎ"), "first preedit is displayed");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 110, utf8(u8"하")));
    require(events.size() == 1, "replacement preedit emits one event");
    const ime_event& replacement_preedit = require_event<ime_event>(events, 0);
    require(replacement_preedit.utf8_text == utf8(u8"하"), "replacement preedit text is emitted");
    require(replacement_preedit.composition.active, "replacement preedit carries active composition");
    require(replacement_preedit.composition.preedit_text == utf8(u8"하"), "replacement preedit composition text is emitted");
    require_range(replacement_preedit.composition.preedit_range, 0, std::string(utf8(u8"하")).size(),
        "replacement preedit range covers hangul syllable bytes");
    const action_route_policy_diagnostic& replacement_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "replacement preedit emits preedit policy");
    require(replacement_policy.text_byte_count == std::string(utf8(u8"하")).size(),
        "replacement preedit policy records syllable byte count");
    require_range(replacement_policy.caret_before,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "replacement preedit policy records previous display caret");
    require_range(replacement_policy.caret_after,
        std::string(utf8(u8"하")).size(),
        std::string(utf8(u8"하")).size(),
        "replacement preedit policy records replacement display caret");
    require(replacement_policy.composition.preedit_text == utf8(u8"하"),
        "replacement preedit policy carries replacement composition");
    require(engine.text_model().text().empty(), "replacement preedit does not commit text");
    require(engine.text_model().display_text() == utf8(u8"하"), "replacement preedit replaces displayed preedit");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::commit, 120, utf8(u8"한")));
    require(events.size() == 1, "explicit ime commit emits one event");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "explicit ime commit emits commit kind");
    require(commit.utf8_text == utf8(u8"한"), "explicit ime commit preserves final text");
    require(commit.composition.active, "explicit ime commit carries active composition snapshot");
    require(commit.composition.preedit_text == utf8(u8"하"), "explicit ime commit carries previous preedit text");
    require(engine.text_model().text() == utf8(u8"한"), "explicit ime commit updates committed text");
    require(engine.text_model().preedit_text().empty(), "explicit ime commit clears preedit");

    events = engine.process_raw_event(text(130, "x"));
    require(events.size() == 1, "raw text resumes after explicit ime commit");
    require(engine.text_model().text() == std::string(utf8(u8"한")) + "x", "raw text appends after ime commit");
}

void test_ime_composition_restart_cancels_visible_preedit()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, "draft")).size() == 1,
        "preedit before restart starts composition");
    require(engine.text_model().preedit_text() == "draft", "preedit before restart is visible");

    std::vector<input_event> events =
        engine.process_raw_event(ime(raw_platform_ime_phase::composition_start, 110));
    require(events.size() == 1, "composition restart emits one event for stale preedit");
    const ime_event& cancel = require_event<ime_event>(events, 0);
    require(cancel.kind == ime_event_kind::cancel, "composition restart emits cancel for stale preedit");
    require(cancel.target_id == "answer", "composition restart cancel preserves target id");
    require(cancel.utf8_text.empty(), "composition restart cancel carries no text");
    require(cancel.composition.active, "composition restart cancel carries stale active composition");
    require(cancel.composition.preedit_text == "draft", "composition restart cancel carries stale preedit text");
    require_range(cancel.composition.preedit_range, 0, 5, "composition restart cancel carries stale preedit range");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "composition restart cancel emits one action route policy");
    const action_route_policy_diagnostic& cancel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_cancel,
        "composition restart cancel action route policy is emitted");
    require(cancel_policy.emits_input_event, "composition restart cancel policy marks emitted event");
    require(cancel_policy.event_index == 0, "composition restart cancel policy points at first event");
    require(cancel_policy.composition.active, "composition restart cancel policy carries stale composition");
    require(cancel_policy.composition.preedit_text == "draft",
        "composition restart cancel policy carries stale preedit text");
    require(cancel_policy.text_byte_count_before == 0, "composition restart cancel policy has no committed text before");
    require(cancel_policy.text_byte_count_after == 0, "composition restart cancel policy has no committed text after");
    require_range(cancel_policy.caret_before, 5, 5, "composition restart cancel policy records preedit caret");
    require_range(cancel_policy.caret_after, 0, 0, "composition restart cancel policy records cleared caret");
    require(engine.text_model().preedit_text().empty(), "composition restart clears stale preedit");

    require(engine.process_raw_event(text(120, "duplicate")).empty(),
        "raw text remains suppressed after composition restart");
    events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 130, "new"));
    require(events.size() == 1, "preedit resumes after composition restart");
    const ime_event& preedit = require_event<ime_event>(events, 0);
    require(preedit.kind == ime_event_kind::preedit, "post-restart preedit kind is emitted");
    require(preedit.utf8_text == "new", "post-restart preedit text is emitted");
}

void test_empty_ime_commit_and_end_cancel_preedit()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, "draft")).size() == 1,
        "preedit before empty commit starts composition");
    std::vector<input_event> events = engine.process_raw_event(ime(raw_platform_ime_phase::commit, 110));
    require(events.size() == 1, "empty ime commit emits cancellation");
    const ime_event& empty_commit = require_event<ime_event>(events, 0);
    require(empty_commit.kind == ime_event_kind::cancel, "empty ime commit emits cancel kind");
    require(empty_commit.utf8_text.empty(), "empty ime commit cancel carries no text");
    require(empty_commit.composition.active, "empty ime commit cancel carries active composition snapshot");
    require(empty_commit.composition.preedit_text == "draft", "empty ime commit cancel carries preedit text");
    require(engine.text_model().text().empty(), "empty ime commit does not append text");
    require(engine.text_model().preedit_text().empty(), "empty ime commit clears preedit");

    require(engine.process_raw_event(text(120, "a")).size() == 1, "raw text resumes after empty ime commit");
    require(engine.text_model().text() == "a", "raw text is committed after empty ime commit");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 130, "draft")).size() == 1,
        "preedit before empty composition end starts composition");
    events = engine.process_raw_event(ime(raw_platform_ime_phase::composition_end, 140));
    require(events.size() == 1, "empty composition end emits cancellation");
    const ime_event& empty_end = require_event<ime_event>(events, 0);
    require(empty_end.kind == ime_event_kind::cancel, "empty composition end emits cancel kind");
    require(empty_end.composition.active, "empty composition end cancel carries active composition snapshot");
    require(empty_end.composition.preedit_text == "draft", "empty composition end cancel carries preedit text");
    require(engine.text_model().text() == "a", "empty composition end preserves committed text");
    require(engine.text_model().preedit_text().empty(), "empty composition end clears preedit");
}

void test_ime_empty_preedit_and_commit_only_edges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::composition_start, 100)).empty(),
        "empty composition start emits no event");
    require(engine.text_model().ime_composition().active, "empty composition start activates composition state");
    require_range(engine.text_model().ime_composition().preedit_range, 0, 0,
        "empty composition start has collapsed preedit range");
    require(engine.process_raw_event(key(105, "Backspace")).empty(),
        "backspace is suppressed during empty composition");
    require(engine.process_raw_event(text(106, "duplicate")).empty(),
        "raw text is suppressed during empty composition");
    require(engine.text_model().text().empty(), "empty composition suppresses duplicate raw text");

    std::vector<input_event> events = engine.process_raw_event(ime(raw_platform_ime_phase::commit, 110));
    require(events.size() == 1, "empty ime commit after composition start emits cancel");
    const ime_event& empty_commit = require_event<ime_event>(events, 0);
    require(empty_commit.kind == ime_event_kind::cancel, "empty commit after composition start is cancel kind");
    require(empty_commit.target_id == "answer", "empty commit cancel preserves target id");
    require(empty_commit.composition.active, "empty commit cancel carries active empty composition snapshot");
    require(empty_commit.composition.preedit_text.empty(), "empty commit cancel carries empty preedit text");
    require_range(empty_commit.composition.preedit_range, 0, 0, "empty commit cancel carries collapsed preedit range");

    events = engine.process_raw_event(text(120, "a"));
    require(events.size() == 1, "raw text resumes after empty composition commit");
    require(engine.text_model().text() == "a", "raw text commits after empty composition commit");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 130));
    require(events.size() == 1, "empty preedit update emits preedit event");
    const ime_event& empty_preedit = require_event<ime_event>(events, 0);
    require(empty_preedit.kind == ime_event_kind::preedit, "empty preedit event uses preedit kind");
    require(empty_preedit.utf8_text.empty(), "empty preedit event carries no text");
    require(empty_preedit.composition.active, "empty preedit event carries active composition");
    require(empty_preedit.composition.preedit_text.empty(), "empty preedit composition text is empty");
    require_range(empty_preedit.composition.replacement_range, 1, 1, "empty preedit replacement range is at caret");
    require_range(empty_preedit.composition.preedit_range, 1, 1, "empty preedit range is collapsed at caret");
    const action_route_policy_diagnostic& empty_preedit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "empty preedit emits preedit policy");
    require(empty_preedit_policy.text_byte_count == 0, "empty preedit policy records zero preedit bytes");
    require(empty_preedit_policy.text_byte_count_before == 1, "empty preedit policy records committed bytes before");
    require(empty_preedit_policy.text_byte_count_after == 1, "empty preedit policy preserves committed bytes after");
    require_range(empty_preedit_policy.caret_before, 1, 1, "empty preedit policy records caret before");
    require_range(empty_preedit_policy.caret_after, 1, 1, "empty preedit policy records collapsed display caret");
    require(empty_preedit_policy.composition.active, "empty preedit policy carries active composition");
    require_range(empty_preedit_policy.composition.replacement_range, 1, 1,
        "empty preedit policy carries collapsed replacement range");
    require(engine.text_model().display_text() == "a", "empty preedit displays committed text only");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::composition_end, 140));
    require(events.size() == 1, "empty composition end after empty preedit emits cancel");
    const ime_event& empty_end = require_event<ime_event>(events, 0);
    require(empty_end.kind == ime_event_kind::cancel, "empty composition end after empty preedit is cancel kind");
    require(empty_end.composition.active, "empty composition end carries active empty composition snapshot");
    require_range(empty_end.composition.replacement_range, 1, 1,
        "empty composition end replacement range is collapsed at caret");
    require(engine.text_model().text() == "a", "empty composition end after empty preedit preserves text");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::cancel, 150)).empty(),
        "cancel without active composition emits no event");

    input_engine commit_only_engine;
    commit_only_engine.focus_text_target("answer");
    events = commit_only_engine.process_raw_event(ime(raw_platform_ime_phase::commit, 200, utf8(u8"한")));
    require(events.size() == 1, "commit-only ime flow emits commit");
    const ime_event& commit_only = require_event<ime_event>(events, 0);
    require(commit_only.kind == ime_event_kind::commit, "commit-only ime flow uses commit kind");
    require(commit_only.utf8_text == utf8(u8"한"), "commit-only ime flow preserves utf8 text");
    require(!commit_only.composition.active, "commit-only ime flow carries inactive composition snapshot");
    require_range(commit_only.composition.replacement_range, 0, 0, "commit-only ime flow replacement range is caret");
    require(commit_only_engine.text_model().text() == utf8(u8"한"), "commit-only ime flow updates text model");

    events = commit_only_engine.process_raw_event(text(210, "x"));
    require(events.size() == 1, "raw text resumes after commit-only ime flow");
    require(commit_only_engine.text_model().text() == std::string(utf8(u8"한")) + "x",
        "raw text appends after commit-only ime flow");
}

void test_ime_hangul_replacement_composition_ranges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    const std::string base = std::string("A") + utf8(u8"한") + "B";
    require(engine.process_raw_event(text(100, base)).size() == 1, "hangul replacement base text commits");

    require(engine.process_raw_event(key(110, "Home")).size() == 1, "home before hangul selection moves caret");
    require(engine.process_raw_event(key(120, "ArrowRight")).size() == 1,
        "arrow right before hangul selection moves past ascii");
    require(engine.process_raw_event(key(130, "ArrowRight", false, raw_platform_key_phase::down, false, true)).size() == 1,
        "shift arrow right selects hangul codepoint");
    const std::optional<text_range> selection = engine.text_model().selection_range();
    require(selection.has_value(), "hangul replacement selection is active");
    require_range(*selection, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement selection spans full utf8 codepoint");

    std::vector<input_event> events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 140, utf8(u8"하")));
    require(events.size() == 1, "hangul replacement preedit emits one event");
    const ime_event& preedit = require_event<ime_event>(events, 0);
    require(preedit.kind == ime_event_kind::preedit, "hangul replacement preedit kind is emitted");
    require(preedit.composition.active, "hangul replacement preedit carries active composition");
    require(preedit.composition.preedit_text == utf8(u8"하"), "hangul replacement preedit carries hangul text");
    require_range(preedit.composition.replacement_range, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement preedit replacement range covers selected hangul");
    require_range(preedit.composition.preedit_range, 1, 1 + std::string(utf8(u8"하")).size(),
        "hangul replacement preedit range covers new hangul");
    require(engine.text_model().display_text() == std::string("A") + utf8(u8"하") + "B",
        "hangul replacement preedit display replaces selected hangul");
    const action_route_policy_diagnostic& preedit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "hangul replacement preedit emits preedit policy");
    require(preedit_policy.text_byte_count == std::string(utf8(u8"하")).size(),
        "hangul replacement preedit policy records preedit byte count");
    require(preedit_policy.text_byte_count_before == base.size(),
        "hangul replacement preedit policy records original committed bytes");
    require(preedit_policy.text_byte_count_after == base.size(),
        "hangul replacement preedit policy does not mutate committed bytes");
    require(preedit_policy.had_selection_before, "hangul replacement preedit policy records prior selection");
    require(preedit_policy.has_selection_after, "hangul replacement preedit policy keeps selection during composition");
    require_range(preedit_policy.selection_before, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement preedit policy records selected hangul before");
    require_range(preedit_policy.selection_after, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement preedit policy records replacement selection after");
    require_range(preedit_policy.caret_before,
        1 + std::string(utf8(u8"한")).size(),
        1 + std::string(utf8(u8"한")).size(),
        "hangul replacement preedit policy records active selection caret before");
    require_range(preedit_policy.caret_after,
        1 + std::string(utf8(u8"하")).size(),
        1 + std::string(utf8(u8"하")).size(),
        "hangul replacement preedit policy records display caret after");
    require_range(preedit_policy.composition.replacement_range, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement preedit policy carries composition replacement range");
    require_range(preedit_policy.composition.preedit_range, 1, 1 + std::string(utf8(u8"하")).size(),
        "hangul replacement preedit policy carries composition preedit range");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::commit, 150, utf8(u8"각")));
    require(events.size() == 1, "hangul replacement commit emits one event");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "hangul replacement commit kind is emitted");
    require(commit.utf8_text == utf8(u8"각"), "hangul replacement commit carries final hangul");
    require(commit.composition.active, "hangul replacement commit carries previous composition snapshot");
    require(commit.composition.preedit_text == utf8(u8"하"), "hangul replacement commit carries previous preedit");
    require_range(commit.composition.replacement_range, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement commit replacement range covers selected hangul");
    require(engine.text_model().text() == std::string("A") + utf8(u8"각") + "B",
        "hangul replacement commit updates committed text");
    require(engine.text_model().caret_byte_offset() == 1 + std::string(utf8(u8"각")).size(),
        "hangul replacement commit places caret after final hangul");
    require(!engine.text_model().selection_range().has_value(), "hangul replacement commit clears selection");
    require(!engine.text_model().ime_composition().active, "hangul replacement commit clears model composition");
}

void test_reset_clears_text_ime_and_pointer_state()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    require(engine.process_raw_event(text(100, "base")).size() == 1, "text before reset commits");
    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 110, "draft")).size() == 1,
        "preedit before reset starts composition");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 120, 4.0f, 4.0f)).empty(),
        "pointer before reset is tracked");

    engine.reset();
    require(!engine.has_text_focus(), "reset clears text focus");
    require(engine.text_focus_id().empty(), "reset clears text focus id");
    require(engine.text_model().text().empty(), "reset clears committed text");
    require(engine.text_model().preedit_text().empty(), "reset clears preedit text");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "reset emits one pointer capture reset policy");
    const action_route_policy_diagnostic& reset_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_reset,
        "reset action route policy records pointer capture reset");
    require(!reset_policy.emits_input_event, "reset pointer policy emits no input event");
    require(reset_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::tracking,
        "reset pointer policy records tracked pointer before reset");
    require(reset_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::idle,
        "reset pointer policy records idle pointer after reset");
    require(engine.process_raw_event(text(130, "ignored")).empty(), "text after reset is ignored without focus");
    require(engine.process_raw_event(key(140, "Backspace")).empty(), "key after reset is ignored without focus");
    require(engine.update_time(800).empty(), "reset clears pending long press state");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 810, 4.0f, 4.0f)).empty(),
        "up after reset emits no stale tap");
}

void test_unfocused_ime_and_focus_gained_edges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(ime(raw_platform_ime_phase::composition_start, 100)).empty(),
        "unfocused composition start is ignored");
    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 110, "draft")).empty(),
        "unfocused preedit is ignored");
    require(engine.process_raw_event(ime(raw_platform_ime_phase::commit, 120, utf8(u8"한"))).empty(),
        "unfocused ime commit is ignored");
    require(engine.process_raw_event(focus(raw_platform_focus_phase::gained, 130)).empty(),
        "focus gained without text target emits no event");

    engine.focus_text_target("answer");
    std::vector<input_event> events = engine.process_raw_event(focus(raw_platform_focus_phase::gained, 140));
    require(events.size() == 1, "focus gained with text target emits one event");
    const text_event& gained = require_event<text_event>(events, 0);
    require(gained.kind == text_event_kind::focus_gained, "focus gained emits focus gained kind");
    require(gained.target_id == "answer", "focus gained preserves target id");

    events = engine.process_raw_event(text(150, "x"));
    require(events.size() == 1, "text commits after ignored unfocused ime events");
    const text_event& commit = require_event<text_event>(events, 0);
    require(commit.kind == text_event_kind::commit, "post-unfocused-ime text emits commit kind");
    require(engine.text_model().text() == "x", "post-unfocused-ime text updates model");
}

void test_focus_loss_cancels_composition_and_pointer_state()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, "draft")).size() == 1,
        "preedit starts composition");
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 110, 0.0f, 0.0f)).empty(),
        "pointer down is tracked before focus loss");

    std::vector<input_event> events = engine.process_raw_event(focus(raw_platform_focus_phase::lost, 120));
    require(events.size() == 2, "focus loss emits ime cancel and focus lost");
    const ime_event& cancel = require_event<ime_event>(events, 0);
    require(cancel.kind == ime_event_kind::cancel, "focus loss cancels composition");
    require(cancel.composition.active, "focus loss cancel carries active composition snapshot");
    require(cancel.composition.preedit_text == "draft", "focus loss cancel carries preedit text");
    require_range(cancel.composition.preedit_range, 0, 5, "focus loss cancel carries preedit range");
    const text_event& lost = require_event<text_event>(events, 1);
    require(lost.kind == text_event_kind::focus_lost, "focus loss emits text focus lost");
    require(lost.target_id == "answer", "focus loss preserves target id");
    require(!engine.has_text_focus(), "focus loss clears text focus");
    require(engine.text_model().preedit_text().empty(), "focus loss clears preedit");
    require(engine.routing_diagnostics().action_routes.size() == 3,
        "focus loss emits pointer reset, ime cancel, and focus loss policies");
    const action_route_policy_diagnostic& reset_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_reset,
        "focus loss first policy records pointer reset");
    require(!reset_policy.emits_input_event, "focus loss pointer reset policy emits no input event");
    require(reset_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::tracking,
        "focus loss pointer reset records tracked pointer before reset");
    require(reset_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::idle,
        "focus loss pointer reset records idle pointer after reset");
    const action_route_policy_diagnostic& ime_cancel_policy = require_policy(
        engine.routing_diagnostics(),
        1,
        action_route_policy_kind::ime_cancel,
        "focus loss second policy records ime cancel");
    require(ime_cancel_policy.emits_input_event, "focus loss ime cancel policy emits input event");
    require(ime_cancel_policy.event_index == 0, "focus loss ime cancel policy points at first event");
    require(ime_cancel_policy.composition.preedit_text == "draft",
        "focus loss ime cancel policy carries composition snapshot");
    require_range(ime_cancel_policy.caret_before, 5, 5,
        "focus loss ime cancel policy records preedit caret before clear");
    require_range(ime_cancel_policy.caret_after, 0, 0,
        "focus loss ime cancel policy records caret after clear");
    const action_route_policy_diagnostic& focus_policy = require_policy(
        engine.routing_diagnostics(),
        2,
        action_route_policy_kind::focus_loss,
        "focus loss third policy records focus loss");
    require(focus_policy.emits_input_event, "focus loss policy marks emitted text event");
    require(focus_policy.event_index == 1, "focus loss policy points after ime cancel event");
    require(focus_policy.target_id == "answer", "focus loss policy preserves target id");
    require(focus_policy.text_byte_count_before == 0, "focus loss policy records before text byte count");
    require(focus_policy.text_byte_count_after == 0, "focus loss policy records after text byte count");

    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 130, 0.0f, 0.0f));
    require(events.empty(), "focus loss resets pending pointer state");
}

} // namespace

int main()
{
    test_primary_pointer_gestures_and_secondary_filter();
    test_touch_pointer_cancel_and_multi_pointer_edges();
    test_pointer_filter_and_timing_edges();
    test_pointer_id_reuse_routes_replacement_state();
    test_drag_gestures_route_from_raw_pointer();
    test_pointer_capture_routes_drag_and_ignores_other_pointers();
    test_drag_start_slop_routes_from_engine_thresholds();
    test_multi_pointer_long_press_order_routes_stably();
    test_scroll_events_normalize_line_and_pixel_deltas();
    test_raw_platform_scroll_routes_through_input_engine();
    test_gesture_routing_diagnostics_summarize_gestures_and_wheel();
    test_gesture_routing_diagnostics_cancel_and_focus_loss();
    test_pointer_cancel_policy_records_non_emitting_stream_cleanup();
    test_pointer_capture_arbitration_diagnostics();
    test_text_key_flow();
    test_key_code_fallback_edges();
    test_text_keyboard_navigation_and_selection();
    test_keyboard_focus_traversal_diagnostics();
    test_keyboard_navigation_diagnostics_preserve_ime_composition();
    test_text_edit_boundary_diagnostics_replace_utf8_selection();
    test_ime_composition_suppresses_text_and_key_events();
    test_ime_preedit_commit_edges();
    test_ime_composition_restart_cancels_visible_preedit();
    test_empty_ime_commit_and_end_cancel_preedit();
    test_ime_empty_preedit_and_commit_only_edges();
    test_ime_hangul_replacement_composition_ranges();
    test_reset_clears_text_ime_and_pointer_state();
    test_unfocused_ime_and_focus_gained_edges();
    test_focus_loss_cancels_composition_and_pointer_state();

    std::cout << "input_engine_tests passed\n";
    return 0;
}
