#include "core/input/input_engine.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
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
    quiz_vulkan::raw_platform_key_phase phase = quiz_vulkan::raw_platform_key_phase::down)
{
    return quiz_vulkan::raw_platform_key_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
        .key_code = 0,
        .logical_key = std::move(logical_key),
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
    require(engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 1710, 30.0f, 30.0f, raw_platform_pointer_button::primary, 6))
                .empty(),
        "engine long press suppresses release after duplicate update check");
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

    require(engine.process_raw_event(key(120, "Backspace", false, raw_platform_key_phase::up)).empty(),
        "backspace keyup is ignored");
    events = engine.process_raw_event(key(130, "Backspace", true));
    require(events.size() == 1, "repeat backspace deletes text");
    const text_event& backspace = require_event<text_event>(events, 0);
    require(backspace.kind == text_event_kind::backspace, "backspace event is emitted");
    require(engine.text_model().text().empty(), "backspace removes utf8 codepoint");

    require(engine.process_raw_event(text(140, "ok")).size() == 1, "second text commit succeeds");
    events = engine.process_raw_event(key(150, "Enter"));
    require(events.size() == 1, "enter submits once");
    const text_event& submit = require_event<text_event>(events, 0);
    require(submit.kind == text_event_kind::submit, "submit event is emitted");
    require(submit.utf8_text == "ok", "submit carries committed buffer");
    require(engine.text_model().text().empty(), "submit clears editing buffer");
    require(engine.text_model().has_submit_text(), "text model retains consumable submit text");

    require(engine.process_raw_event(key(151, "Enter", true)).empty(), "repeat enter is ignored");
}

void test_ime_composition_suppresses_text_and_key_events()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::composition_start, 100)).empty(),
        "composition start is state only");
    std::vector<input_event> events =
        engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 110, utf8(u8"ㅎ")));
    require(events.size() == 1, "preedit emits one ime event");
    const ime_event& preedit = require_event<ime_event>(events, 0);
    require(preedit.kind == ime_event_kind::preedit, "preedit event is emitted");
    require(preedit.utf8_text == utf8(u8"ㅎ"), "preedit text is preserved");
    require(engine.text_model().display_text() == utf8(u8"ㅎ"), "display text includes preedit");

    require(engine.process_raw_event(key(120, "Enter")).empty(), "enter is suppressed during composition");
    require(engine.process_raw_event(text(130, "duplicate")).empty(), "raw text is suppressed during composition");
    require(engine.text_model().text().empty(), "suppressed raw text does not commit");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::composition_end, 140, utf8(u8"한")));
    require(events.size() == 1, "composition end with text commits ime text");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "ime commit event is emitted");
    require(commit.utf8_text == utf8(u8"한"), "ime commit text is preserved");
    require(engine.text_model().text() == utf8(u8"한"), "ime commit updates text model");
    require(engine.text_model().preedit_text().empty(), "ime commit clears preedit");
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
    require(require_event<ime_event>(events, 0).utf8_text == utf8(u8"ㅎ"), "first preedit text is emitted");
    require(engine.text_model().display_text() == utf8(u8"ㅎ"), "first preedit is displayed");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 110, utf8(u8"하")));
    require(events.size() == 1, "replacement preedit emits one event");
    require(require_event<ime_event>(events, 0).utf8_text == utf8(u8"하"), "replacement preedit text is emitted");
    require(engine.text_model().text().empty(), "replacement preedit does not commit text");
    require(engine.text_model().display_text() == utf8(u8"하"), "replacement preedit replaces displayed preedit");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::commit, 120, utf8(u8"한")));
    require(events.size() == 1, "explicit ime commit emits one event");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "explicit ime commit emits commit kind");
    require(commit.utf8_text == utf8(u8"한"), "explicit ime commit preserves final text");
    require(engine.text_model().text() == utf8(u8"한"), "explicit ime commit updates committed text");
    require(engine.text_model().preedit_text().empty(), "explicit ime commit clears preedit");

    events = engine.process_raw_event(text(130, "x"));
    require(events.size() == 1, "raw text resumes after explicit ime commit");
    require(engine.text_model().text() == std::string(utf8(u8"한")) + "x", "raw text appends after ime commit");
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

    events = engine.process_raw_event(text(120, "a"));
    require(events.size() == 1, "raw text resumes after empty composition commit");
    require(engine.text_model().text() == "a", "raw text commits after empty composition commit");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 130));
    require(events.size() == 1, "empty preedit update emits preedit event");
    const ime_event& empty_preedit = require_event<ime_event>(events, 0);
    require(empty_preedit.kind == ime_event_kind::preedit, "empty preedit event uses preedit kind");
    require(empty_preedit.utf8_text.empty(), "empty preedit event carries no text");
    require(engine.text_model().display_text() == "a", "empty preedit displays committed text only");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::composition_end, 140));
    require(events.size() == 1, "empty composition end after empty preedit emits cancel");
    const ime_event& empty_end = require_event<ime_event>(events, 0);
    require(empty_end.kind == ime_event_kind::cancel, "empty composition end after empty preedit is cancel kind");
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
    require(commit_only_engine.text_model().text() == utf8(u8"한"), "commit-only ime flow updates text model");

    events = commit_only_engine.process_raw_event(text(210, "x"));
    require(events.size() == 1, "raw text resumes after commit-only ime flow");
    require(commit_only_engine.text_model().text() == std::string(utf8(u8"한")) + "x",
        "raw text appends after commit-only ime flow");
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
    const text_event& lost = require_event<text_event>(events, 1);
    require(lost.kind == text_event_kind::focus_lost, "focus loss emits text focus lost");
    require(lost.target_id == "answer", "focus loss preserves target id");
    require(!engine.has_text_focus(), "focus loss clears text focus");
    require(engine.text_model().preedit_text().empty(), "focus loss clears preedit");

    events = engine.process_raw_event(pointer(raw_platform_pointer_phase::up, 130, 0.0f, 0.0f));
    require(events.empty(), "focus loss resets pending pointer state");
}

} // namespace

int main()
{
    test_primary_pointer_gestures_and_secondary_filter();
    test_touch_pointer_cancel_and_multi_pointer_edges();
    test_pointer_filter_and_timing_edges();
    test_text_key_flow();
    test_ime_composition_suppresses_text_and_key_events();
    test_ime_preedit_commit_edges();
    test_empty_ime_commit_and_end_cancel_preedit();
    test_ime_empty_preedit_and_commit_only_edges();
    test_focus_loss_cancels_composition_and_pointer_state();

    std::cout << "input_engine_tests passed\n";
    return 0;
}
