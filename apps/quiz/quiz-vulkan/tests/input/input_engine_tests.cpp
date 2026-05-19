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

void require_modifiers(
    quiz_vulkan::input::input_modifier_state modifiers,
    bool alt,
    bool ctrl,
    bool shift,
    bool meta,
    const char* message)
{
    require(modifiers.alt == alt, message);
    require(modifiers.ctrl == ctrl, message);
    require(modifiers.shift == shift, message);
    require(modifiers.meta == meta, message);
}

void require_text_route_state(
    const quiz_vulkan::input::text_focus_route_state& state,
    bool has_focus,
    const std::string& target_id,
    std::size_t text_byte_count,
    quiz_vulkan::input::text_range caret,
    bool has_selection,
    quiz_vulkan::input::text_range selection,
    const char* message)
{
    require(state.has_focus == has_focus, message);
    require(state.target_id == target_id, message);
    require(state.text_byte_count == text_byte_count, message);
    require(state.caret.start_byte == caret.start_byte, message);
    require(state.caret.end_byte == caret.end_byte, message);
    require(state.has_selection == has_selection, message);
    require(state.selection.start_byte == selection.start_byte, message);
    require(state.selection.end_byte == selection.end_byte, message);
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
    quiz_vulkan::input::scroll_delta_unit unit,
    quiz_vulkan::input::input_modifier_state modifiers = {})
{
    return quiz_vulkan::input::raw_scroll_event{
        .timestamp_ms = timestamp_ms,
        .x = x,
        .y = y,
        .delta_x = delta_x,
        .delta_y = delta_y,
        .unit = unit,
        .modifiers = modifiers,
    };
}

quiz_vulkan::raw_platform_input_event platform_scroll(
    std::int64_t timestamp_ms,
    float x,
    float y,
    float delta_x,
    float delta_y,
    quiz_vulkan::raw_platform_scroll_delta_unit unit,
    bool alt = false,
    bool ctrl = false,
    bool shift = false,
    bool meta = false)
{
    return quiz_vulkan::raw_platform_scroll_event{
        .timestamp_ms = timestamp_ms,
        .x = x,
        .y = y,
        .delta_x = delta_x,
        .delta_y = delta_y,
        .unit = unit,
        .alt = alt,
        .ctrl = ctrl,
        .shift = shift,
        .meta = meta,
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

void test_touch_pointer_does_not_corrupt_mouse_drag_capture()
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
        "mixed capture mouse down emits no event");

    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::move,
        120,
        9.0f,
        0.0f,
        raw_platform_pointer_button::primary,
        1));
    require(events.size() == 1, "mixed capture mouse starts drag");
    const gesture_event& start = require_event<gesture_event>(events, 0);
    require(start.kind == gesture_kind::drag_start, "mixed capture mouse emits drag start");
    const action_route_policy_diagnostic& start_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "mixed capture drag start route is emitted");
    require(start_policy.pointer_contact == pointer_contact_kind::mouse_like,
        "mixed capture drag start records mouse contact");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::captured,
        true,
        1,
        1,
        "mixed capture mouse owns pointer capture");

    require(engine.process_raw_event(pointer(
                raw_platform_pointer_phase::down,
                130,
                30.0f,
                0.0f,
                raw_platform_pointer_button::none,
                44))
                .empty(),
        "mixed capture touch down is diagnostic only while mouse captured");
    const action_route_policy_diagnostic& ignored_touch_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::pointer_capture_arbitration,
        "mixed capture touch down emits arbitration route");
    require(!ignored_touch_policy.emits_input_event,
        "mixed capture ignored touch emits no normalized input event");
    require(ignored_touch_policy.pointer_decision == pointer_arbitration_decision::ignored_by_capture,
        "mixed capture touch records ignored-by-capture decision");
    require(ignored_touch_policy.pointer_contact == pointer_contact_kind::touch_like,
        "mixed capture touch route records touch contact");
    require(ignored_touch_policy.pointer_id == 44, "mixed capture touch route preserves touch id");
    require_capture_snapshot(
        ignored_touch_policy.pointer_capture_before,
        pointer_capture_lifecycle::captured,
        true,
        1,
        1,
        "mixed capture touch route records mouse capture before");
    require_capture_snapshot(
        ignored_touch_policy.pointer_capture_after,
        pointer_capture_lifecycle::captured,
        true,
        1,
        1,
        "mixed capture touch route leaves mouse capture after");
    require(engine.routing_diagnostics().summary.normalized_event_count == 0,
        "mixed capture ignored touch has no normalized event summary");
    require(engine.routing_diagnostics().summary.routes.pointer == 1,
        "mixed capture ignored touch counts one pointer route");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::move,
        140,
        13.0f,
        2.0f,
        raw_platform_pointer_button::primary,
        1));
    require(events.size() == 1, "mixed capture mouse drag update still emits");
    const gesture_event& update = require_event<gesture_event>(events, 0);
    require(update.kind == gesture_kind::drag_update, "mixed capture mouse update kind is routed");
    require(update.pointer_id == 1, "mixed capture mouse update preserves pointer id");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        150,
        16.0f,
        4.0f,
        raw_platform_pointer_button::primary,
        1));
    require(events.size() == 1, "mixed capture mouse drag end still emits");
    const gesture_event& end = require_event<gesture_event>(events, 0);
    require(end.kind == gesture_kind::drag_end, "mixed capture mouse end kind is routed");
    require(end.pointer_id == 1, "mixed capture mouse end preserves pointer id");
    require_capture_snapshot(
        engine.routing_diagnostics().pointer_capture,
        pointer_capture_lifecycle::idle,
        false,
        0,
        0,
        "mixed capture releases mouse capture cleanly");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        160,
        30.0f,
        0.0f,
        raw_platform_pointer_button::none,
        44));
    require(events.empty(), "mixed capture ignored touch up has no stale tap");
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

void test_wheel_modifiers_normalize_without_pointer_text_or_domain_routes()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    std::vector<input_event> events = engine.process_scroll_event(scroll(
        300,
        12.0f,
        24.0f,
        0.0f,
        -5.0f,
        scroll_delta_unit::lines,
        input_modifier_state{
            .alt = true,
            .ctrl = false,
            .shift = true,
            .meta = false,
        }));
    require(events.size() == 1, "direct modified wheel emits one input event");
    const scroll_event& direct = require_event<scroll_event>(events, 0);
    require(direct.timestamp_ms == 300, "direct modified wheel preserves timestamp");
    require(direct.x == 12.0f, "direct modified wheel preserves x");
    require(direct.y == 24.0f, "direct modified wheel preserves y");
    require(direct.line_delta_y == -5.0f, "direct modified wheel preserves line delta");
    require_modifiers(direct.modifiers, true, false, true, false,
        "direct modified wheel preserves modifier state");

    const input_routing_diagnostics& direct_diagnostics = engine.routing_diagnostics();
    require(direct_diagnostics.normalized_events.size() == 1,
        "direct modified wheel emits one normalized summary");
    require_modifiers(
        direct_diagnostics.normalized_events[0].modifiers,
        true,
        false,
        true,
        false,
        "direct modified wheel summary preserves modifiers");
    require(direct_diagnostics.action_routes.size() == 1,
        "direct modified wheel emits one route policy");
    const action_route_policy_diagnostic& direct_policy = require_policy(
        direct_diagnostics,
        0,
        action_route_policy_kind::wheel_summary,
        "direct modified wheel route is wheel summary");
    require(direct_policy.emits_input_event, "direct modified wheel route emits normalized input event");
    require(direct_policy.normalized_event.kind == input_event_summary_kind::wheel,
        "direct modified wheel route carries wheel summary");
    require_modifiers(direct_policy.normalized_event.modifiers, true, false, true, false,
        "direct modified wheel route carries modifier evidence");
    require(direct_policy.pointer_contact == pointer_contact_kind::unknown,
        "direct modified wheel route has no pointer contact semantics");
    require(direct_policy.pointer_decision == pointer_arbitration_decision::none,
        "direct modified wheel route has no pointer arbitration");
    const input_diagnostic_summary& direct_summary = direct_diagnostics.summary;
    require(direct_summary.normalized_event_count == 1,
        "direct modified wheel summary counts one normalized event");
    require(direct_summary.normalized_events.wheel == 1,
        "direct modified wheel summary counts wheel event");
    require(direct_summary.routes.wheel == 1,
        "direct modified wheel summary counts one wheel route");
    require(direct_summary.routes.pointer == 0,
        "direct modified wheel summary counts no pointer route");
    require(direct_summary.routes.text == 0,
        "direct modified wheel summary counts no text route");
    require(direct_summary.routes.ime == 0,
        "direct modified wheel summary counts no ime route");
    require(direct_summary.routes.focus == 0,
        "direct modified wheel summary counts no focus route");
    require(direct_summary.routes.total == 1,
        "direct modified wheel summary counts only the wheel route");
    require(direct_summary.pointer_capture_ended_cleanly,
        "direct modified wheel keeps pointer capture clean");
    require(direct_summary.focus_ended_cleanly,
        "direct modified wheel keeps focus clean");
    require(direct_summary.preedit_ended_cleanly,
        "direct modified wheel keeps preedit clean");

    events = engine.process_raw_event(platform_scroll(
        320,
        30.0f,
        40.0f,
        9.0f,
        -18.0f,
        raw_platform_scroll_delta_unit::pixels,
        false,
        true,
        false,
        true));
    require(events.size() == 1, "raw modified wheel emits one input event");
    const scroll_event& raw = require_event<scroll_event>(events, 0);
    require(raw.pixel_delta_x == 9.0f, "raw modified wheel preserves x pixel delta");
    require(raw.pixel_delta_y == -18.0f, "raw modified wheel preserves y pixel delta");
    require_modifiers(raw.modifiers, false, true, false, true,
        "raw modified wheel preserves modifier state");
    require_modifiers(
        engine.routing_diagnostics().action_routes[0].normalized_event.modifiers,
        false,
        true,
        false,
        true,
        "raw modified wheel route carries modifier evidence");
}

void test_text_focus_caret_route_state_tracks_edits_ime_and_tap_focus()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    std::vector<input_event> events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::down,
        100,
        12.0f,
        24.0f));
    require(events.empty(), "text route tap down emits no gesture yet");
    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        120,
        12.0f,
        24.0f));
    require(events.size() == 1, "text route tap up emits tap gesture");
    const gesture_event& tap = require_event<gesture_event>(events, 0);
    require(tap.kind == gesture_kind::tap, "text route focus source is tap gesture");
    require(!engine.routing_diagnostics().text_route_state.has_focus,
        "text route tap does not focus text by itself");
    require(engine.routing_diagnostics().summary.routes.text == 0,
        "text route tap emits no text route");
    require(engine.routing_diagnostics().summary.routes.focus == 0,
        "text route tap emits no focus route");

    engine.focus_text_target("tap-target");
    require_text_route_state(
        engine.routing_diagnostics().text_route_state,
        true,
        "tap-target",
        0,
        text_range{},
        false,
        text_range{},
        "tap target focus updates current text route state");
    require(engine.routing_diagnostics().action_routes.empty(),
        "initial tap target focus emits no app/domain action route");

    const std::string initial = std::string("A") + utf8(u8"한");
    events = engine.process_raw_event(text(130, initial));
    require(events.size() == 1, "text route insert emits text event");
    const text_event& commit = require_event<text_event>(events, 0);
    require(commit.kind == text_event_kind::commit, "text route insert emits commit kind");
    const action_route_policy_diagnostic& commit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_commit_boundary,
        "text route insert emits commit boundary route");
    require_text_route_state(
        commit_policy.text_route_before,
        true,
        "tap-target",
        0,
        text_range{},
        false,
        text_range{},
        "text route insert records focused empty route state before");
    require_text_route_state(
        commit_policy.text_route_after,
        true,
        "tap-target",
        initial.size(),
        text_range{.start_byte = initial.size(), .end_byte = initial.size()},
        false,
        text_range{},
        "text route insert records caret after utf8 insert");
    require_text_route_state(
        engine.routing_diagnostics().text_route_state,
        true,
        "tap-target",
        initial.size(),
        text_range{.start_byte = initial.size(), .end_byte = initial.size()},
        false,
        text_range{},
        "text route insert updates current route state");

    events = engine.process_raw_event(key(140, "Backspace"));
    require(events.size() == 1, "text route backspace emits text event");
    const text_event& backspace = require_event<text_event>(events, 0);
    require(backspace.kind == text_event_kind::backspace,
        "text route backspace emits backspace kind");
    const action_route_policy_diagnostic& backspace_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_backspace_boundary,
        "text route backspace emits backspace route");
    require_text_route_state(
        backspace_policy.text_route_before,
        true,
        "tap-target",
        initial.size(),
        text_range{.start_byte = initial.size(), .end_byte = initial.size()},
        false,
        text_range{},
        "text route backspace records route state before deletion");
    require_text_route_state(
        backspace_policy.text_route_after,
        true,
        "tap-target",
        1,
        text_range{.start_byte = 1, .end_byte = 1},
        false,
        text_range{},
        "text route backspace records utf8-safe caret after deletion");
    require_text_route_state(
        engine.routing_diagnostics().text_route_state,
        true,
        "tap-target",
        1,
        text_range{.start_byte = 1, .end_byte = 1},
        false,
        text_range{},
        "text route backspace updates current route state");

    const std::string preedit = utf8(u8"ㄱ");
    events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 150, preedit));
    require(events.size() == 1, "text route ime preedit emits ime event");
    const ime_event& preedit_event = require_event<ime_event>(events, 0);
    require(preedit_event.kind == ime_event_kind::preedit,
        "text route ime preedit emits preedit kind");
    const action_route_policy_diagnostic& preedit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "text route ime preedit emits preedit route");
    require_text_route_state(
        preedit_policy.text_route_before,
        true,
        "tap-target",
        1,
        text_range{.start_byte = 1, .end_byte = 1},
        false,
        text_range{},
        "text route ime preedit records committed caret before");
    require_text_route_state(
        preedit_policy.text_route_after,
        true,
        "tap-target",
        1,
        text_range{.start_byte = 1 + preedit.size(), .end_byte = 1 + preedit.size()},
        false,
        text_range{},
        "text route ime preedit records display caret after");
    require(preedit_policy.text_route_after.composition.active,
        "text route ime preedit route state records active composition");
    require(preedit_policy.text_route_after.composition.preedit_text == preedit,
        "text route ime preedit route state records preedit text");
    require(engine.routing_diagnostics().text_route_state.composition.active,
        "text route ime preedit updates current active composition");

    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::down,
        160,
        42.0f,
        24.0f));
    require(events.empty(), "text route second tap down emits no gesture yet");
    events = engine.process_raw_event(pointer(
        raw_platform_pointer_phase::up,
        180,
        42.0f,
        24.0f));
    require(events.size() == 1, "text route second tap emits tap gesture");
    require(require_event<gesture_event>(events, 0).kind == gesture_kind::tap,
        "text route second focus source is tap gesture");

    engine.focus_text_target("second-target");
    const input_routing_diagnostics& focus_diagnostics = engine.routing_diagnostics();
    require_text_route_state(
        focus_diagnostics.text_route_state,
        true,
        "second-target",
        1,
        text_range{.start_byte = 1, .end_byte = 1},
        false,
        text_range{},
        "text route tap focus target change updates current route state");
    require(!focus_diagnostics.text_route_state.composition.active,
        "text route tap focus target change clears active preedit");
    require(focus_diagnostics.action_routes.size() == 2,
        "text route tap focus target change emits ime cleanup and focus cleanup routes");
    const action_route_policy_diagnostic& cancel_policy = require_policy(
        focus_diagnostics,
        0,
        action_route_policy_kind::ime_cancel,
        "text route target change first route cancels ime");
    require_text_route_state(
        cancel_policy.text_route_before,
        true,
        "tap-target",
        1,
        text_range{.start_byte = 1 + preedit.size(), .end_byte = 1 + preedit.size()},
        false,
        text_range{},
        "text route target change cancel records old focused preedit state before");
    require(cancel_policy.text_route_before.composition.active,
        "text route target change cancel records active preedit before");
    require_text_route_state(
        cancel_policy.text_route_after,
        true,
        "second-target",
        1,
        text_range{.start_byte = 1, .end_byte = 1},
        false,
        text_range{},
        "text route target change cancel records new focused state after");
    const action_route_policy_diagnostic& focus_policy = require_policy(
        focus_diagnostics,
        1,
        action_route_policy_kind::focus_loss,
        "text route target change second route records old focus loss");
    require_text_route_state(
        focus_policy.text_route_before,
        true,
        "tap-target",
        1,
        text_range{.start_byte = 1 + preedit.size(), .end_byte = 1 + preedit.size()},
        false,
        text_range{},
        "text route target change focus records old route state before");
    require_text_route_state(
        focus_policy.text_route_after,
        true,
        "second-target",
        1,
        text_range{.start_byte = 1, .end_byte = 1},
        false,
        text_range{},
        "text route target change focus records new route state after");
    require(focus_diagnostics.summary.routes.ime == 1,
        "text route target change summary counts ime cleanup");
    require(focus_diagnostics.summary.routes.focus == 1,
        "text route target change summary counts focus cleanup");
    require(focus_diagnostics.summary.routes.text == 0,
        "text route target change emits no text submit route");
    require(!engine.text_model().has_submit_text(),
        "text route target change does not dispatch app/domain action");
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
    require(swipe_policy.gesture_policy.decision == gesture_policy_decision::swipe_accepted,
        "diagnostic swipe policy records accepted decision");
    require(swipe_policy.gesture_policy.direction == gesture_direction::right,
        "diagnostic swipe policy records right direction");
    require(swipe_policy.gesture_policy.distance == 70.0f,
        "diagnostic swipe policy records distance");
    require(swipe_policy.gesture_policy.duration_ms == 80,
        "diagnostic swipe policy records duration");
    require(swipe_policy.gesture_policy.swipe_min_dx == 60.0f,
        "diagnostic swipe policy records dx threshold");
    require(swipe_policy.gesture_policy.swipe_max_dy == 40.0f,
        "diagnostic swipe policy records dy threshold");
    require(swipe_policy.gesture_policy.swipe_max_duration_ms == 800,
        "diagnostic swipe policy records duration threshold");
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
    require(long_press_policy.gesture_policy.decision == gesture_policy_decision::long_press_accepted,
        "diagnostic long press policy records accepted decision");
    require(long_press_policy.gesture_policy.emitted_kind == gesture_kind::long_press,
        "diagnostic long press policy records emitted kind");
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
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "diagnostic long press suppressed release emits one policy");
    const action_route_policy_diagnostic& suppressed_release_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "diagnostic long press suppressed release is gesture policy");
    require(!suppressed_release_policy.emits_input_event,
        "diagnostic long press suppressed release emits no input event");
    require(suppressed_release_policy.gesture_policy.decision == gesture_policy_decision::release_suppressed,
        "diagnostic long press release records suppression decision");

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
    require(drag_start_policy.gesture_policy.decision == gesture_policy_decision::drag_started,
        "diagnostic drag start policy records drag started decision");
    require(drag_start_policy.gesture_policy.direction == gesture_direction::right,
        "diagnostic drag start policy records direction");
    require(drag_start_policy.gesture_policy.drag_start_slop == 8.0f,
        "diagnostic drag start policy records drag threshold");
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
    require(drag_update_policy.gesture_policy.decision == gesture_policy_decision::drag_updated,
        "diagnostic drag update policy records update decision");
    require(drag_update_policy.gesture_policy.delta_x == 12.0f,
        "diagnostic drag update policy records start-relative dx");

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
    require(drag_end_policy.gesture_policy.decision == gesture_policy_decision::drag_released,
        "diagnostic drag end policy records release decision");
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

void test_gesture_policy_diagnostics_record_rejected_swipes()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 100, 0.0f, 0.0f)).empty(),
        "short swipe diagnostic down emits no gesture");
    std::vector<input_event> events =
        engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 180, 59.0f, 0.0f));
    require(events.empty(), "short swipe diagnostic emits no gesture");
    require(engine.routing_diagnostics().normalized_events.empty(),
        "short swipe diagnostic emits no normalized summary");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "short swipe diagnostic emits one route policy");
    const action_route_policy_diagnostic& short_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "short swipe diagnostic route is gesture snapshot");
    require(!short_policy.emits_input_event, "short swipe policy emits no input event");
    require(short_policy.gesture_policy.decision == gesture_policy_decision::swipe_rejected_distance,
        "short swipe policy records distance rejection");
    require(short_policy.gesture_policy.direction == gesture_direction::right,
        "short swipe policy records deterministic direction");
    require(short_policy.gesture_policy.delta_x == 59.0f, "short swipe policy records dx");
    require(short_policy.gesture_policy.swipe_min_dx == 60.0f,
        "short swipe policy records dx threshold");
    require(short_policy.pointer_capture_before.lifecycle == pointer_capture_lifecycle::tracking,
        "short swipe policy records tracked pointer before release");
    require(short_policy.pointer_capture_after.lifecycle == pointer_capture_lifecycle::idle,
        "short swipe policy records idle pointer after release");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 200, 0.0f, 0.0f)).empty(),
        "cross-axis swipe diagnostic down emits no gesture");
    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 260, 60.0f, 41.0f));
    require(events.empty(), "cross-axis swipe diagnostic emits no gesture");
    const action_route_policy_diagnostic& cross_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "cross-axis swipe diagnostic route is gesture snapshot");
    require(cross_policy.gesture_policy.decision == gesture_policy_decision::swipe_rejected_cross_axis,
        "cross-axis swipe policy records cross-axis rejection");
    require(cross_policy.gesture_policy.direction == gesture_direction::right,
        "cross-axis swipe policy records dominant direction");
    require(cross_policy.gesture_policy.delta_y == 41.0f, "cross-axis swipe policy records dy");
    require(cross_policy.gesture_policy.swipe_max_dy == 40.0f,
        "cross-axis swipe policy records dy threshold");

    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::down, 300, 0.0f, 0.0f)).empty(),
        "slow swipe diagnostic down emits no gesture");
    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 1101, 60.0f, 0.0f));
    require(events.empty(), "slow swipe diagnostic emits no gesture");
    const action_route_policy_diagnostic& slow_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "slow swipe diagnostic route is gesture snapshot");
    require(slow_policy.gesture_policy.decision == gesture_policy_decision::swipe_rejected_duration,
        "slow swipe policy records duration rejection");
    require(slow_policy.gesture_policy.duration_ms == 801, "slow swipe policy records duration");
    require(slow_policy.gesture_policy.swipe_max_duration_ms == 800,
        "slow swipe policy records duration threshold");
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
    require(cancel_policy.gesture_policy.decision == gesture_policy_decision::drag_canceled,
        "diagnostic cancel policy records drag canceled decision");
    require(cancel_policy.gesture_policy.emitted_kind == gesture_kind::drag_cancel,
        "diagnostic cancel policy records emitted kind");
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
    require(cancel_reset_policy.gesture_policy.decision == gesture_policy_decision::drag_canceled,
        "diagnostic cancel reset policy carries drag canceled decision");
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
    require(engine.routing_diagnostics().action_routes.size() == 2,
        "non-emitting cancel emits gesture suppression and pointer reset policies");
    const action_route_policy_diagnostic& cancel_route_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "non-emitting cancel route policy records suppression");
    require(!cancel_route_policy.emits_input_event, "non-emitting cancel route emits no input event");
    require(cancel_route_policy.gesture_policy.decision == gesture_policy_decision::release_suppressed,
        "non-emitting cancel route records suppression decision");
    require(cancel_route_policy.gesture_policy.direction == gesture_direction::right,
        "non-emitting cancel route records deterministic direction");
    require(cancel_route_policy.gesture_policy.distance == 70.0f,
        "non-emitting cancel route records canceled distance");
    const action_route_policy_diagnostic& cancel_policy = require_policy(
        engine.routing_diagnostics(),
        1,
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
    require(engine.routing_diagnostics().action_routes.size() == 2,
        "isolated cancel emits gesture suppression and pointer reset policies");
    const action_route_policy_diagnostic& isolated_route_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "isolated cancel route policy records suppression");
    require(isolated_route_policy.gesture_policy.decision == gesture_policy_decision::release_suppressed,
        "isolated cancel route records suppression");
    const action_route_policy_diagnostic& isolated_policy = require_policy(
        engine.routing_diagnostics(),
        1,
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
    require(engine.routing_diagnostics().action_routes.size() == 2,
        "long-pressed pointer cancel emits suppression and pointer reset policies");
    const action_route_policy_diagnostic& long_press_cancel_route_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::gesture_route_snapshot,
        "long-pressed pointer cancel route records suppression");
    require(long_press_cancel_route_policy.gesture_policy.decision == gesture_policy_decision::release_suppressed,
        "long-pressed cancel route records release suppression");
    const action_route_policy_diagnostic& long_press_cancel_policy = require_policy(
        engine.routing_diagnostics(),
        1,
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
    const input_diagnostic_summary& focus_summary = engine.routing_diagnostics().summary;
    require(focus_summary.normalized_event_count == 0, "focus loss summary emits no normalized events");
    require(focus_summary.routes.pointer == 1, "focus loss summary counts pointer reset route");
    require(focus_summary.routes.ime == 1, "focus loss summary counts ime cancel route");
    require(focus_summary.routes.focus == 1, "focus loss summary counts focus loss route");
    require(focus_summary.routes.text == 0, "focus loss summary counts no text edit route");
    require(focus_summary.routes.total == 3, "focus loss summary counts all focus loss routes");
    require(focus_summary.pointer_capture_ended_cleanly, "focus loss summary clears pointer capture");
    require(focus_summary.focus_ended_cleanly, "focus loss summary clears focus state cleanly");
    require(focus_summary.preedit_ended_cleanly, "focus loss summary clears preedit cleanly");

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
    test_touch_pointer_does_not_corrupt_mouse_drag_capture();
    test_drag_start_slop_routes_from_engine_thresholds();
    test_multi_pointer_long_press_order_routes_stably();
    test_scroll_events_normalize_line_and_pixel_deltas();
    test_raw_platform_scroll_routes_through_input_engine();
    test_wheel_modifiers_normalize_without_pointer_text_or_domain_routes();
    test_text_focus_caret_route_state_tracks_edits_ime_and_tap_focus();
    test_gesture_routing_diagnostics_summarize_gestures_and_wheel();
    test_gesture_policy_diagnostics_record_rejected_swipes();
    test_gesture_routing_diagnostics_cancel_and_focus_loss();
    test_pointer_cancel_policy_records_non_emitting_stream_cleanup();
    test_pointer_capture_arbitration_diagnostics();
    test_reset_clears_text_ime_and_pointer_state();
    test_unfocused_ime_and_focus_gained_edges();
    test_focus_loss_cancels_composition_and_pointer_state();

    std::cout << "input_engine_tests passed\n";
    return 0;
}
