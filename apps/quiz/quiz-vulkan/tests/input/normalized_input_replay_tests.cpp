#include "core/input/normalized_input_replay.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
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

    std::cerr << "normalized_input_replay_tests failed: " << message << '\n';
    std::exit(1);
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

quiz_vulkan::raw_platform_input_event text(std::int64_t timestamp_ms, std::string value)
{
    return quiz_vulkan::raw_platform_text_event{
        .timestamp_ms = timestamp_ms,
        .utf8_text = std::move(value),
    };
}

quiz_vulkan::raw_platform_input_event key(std::int64_t timestamp_ms, std::string logical_key)
{
    return quiz_vulkan::raw_platform_key_event{
        .timestamp_ms = timestamp_ms,
        .phase = quiz_vulkan::raw_platform_key_phase::down,
        .key_code = 0,
        .logical_key = std::move(logical_key),
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

quiz_vulkan::raw_platform_input_event focus(
    quiz_vulkan::raw_platform_focus_phase phase,
    std::int64_t timestamp_ms)
{
    return quiz_vulkan::raw_platform_focus_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
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

quiz_vulkan::input::normalized_input_replay_step step(
    std::string label,
    quiz_vulkan::raw_platform_input_event event)
{
    return quiz_vulkan::input::normalized_input_replay_step{
        .label = std::move(label),
        .action = quiz_vulkan::input::normalized_input_replay_action{std::move(event)},
    };
}

void test_mixed_fixture_records_stable_summary_counts()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    const std::array steps{
        step("mouse-down", pointer(raw_platform_pointer_phase::down, 100, 4.0f, 6.0f, raw_platform_pointer_button::primary, 3)),
        step("mouse-up", pointer(raw_platform_pointer_phase::up, 130, 5.0f, 7.0f, raw_platform_pointer_button::primary, 3)),
        step("wheel", platform_scroll(140, 20.0f, 30.0f, 0.0f, -2.0f, raw_platform_scroll_delta_unit::lines)),
        step("text", text(150, "x")),
        step("backspace", key(160, "Backspace")),
        step("ime-preedit", ime(raw_platform_ime_phase::preedit_update, 170, utf8(u8"ㅎ"))),
        step("ime-commit", ime(raw_platform_ime_phase::commit, 180, utf8(u8"한"))),
    };

    const normalized_input_replay_recording recording = replay_normalized_input_fixture(
        engine,
        steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    require(recording.batches.size() == steps.size(), "mixed replay records one batch per step");
    require(recording.batches[0].label == "mouse-down", "mixed replay preserves batch labels");
    require(recording.batches[0].input_events.empty(), "mixed replay mouse down has no emitted input event");
    require(recording.batches[0].summary.routes.pointer == 1,
        "mixed replay mouse down batch records pointer arbitration");
    require(!recording.batches[0].end_state.pointer_capture_clean,
        "mixed replay mouse down batch records active pointer capture");
    require(recording.batches[1].input_events.size() == 1,
        "mixed replay mouse up batch captures emitted input event");
    require(recording.batches[1].normalized_events.size() == 1,
        "mixed replay mouse up batch captures normalized event summary");
    require(recording.batches[1].summary.normalized_events.tap == 1,
        "mixed replay mouse up batch counts tap");
    require(recording.batches[2].summary.normalized_events.wheel == 1,
        "mixed replay wheel batch counts wheel");
    require(recording.batches[5].summary.routes.ime == 1,
        "mixed replay preedit batch counts ime route");
    require(!recording.batches[5].end_state.preedit_clean,
        "mixed replay preedit batch records unclean preedit end state");

    const input_diagnostic_summary& summary = recording.summary;
    require(summary.normalized_event_count == 2, "mixed replay aggregate counts normalized events");
    require(summary.normalized_events.tap == 1, "mixed replay aggregate counts tap");
    require(summary.normalized_events.wheel == 1, "mixed replay aggregate counts wheel");
    require(summary.routes.pointer == 2, "mixed replay aggregate counts pointer routes");
    require(summary.routes.wheel == 1, "mixed replay aggregate counts wheel routes");
    require(summary.routes.text == 2, "mixed replay aggregate counts text edit routes");
    require(summary.routes.ime == 2, "mixed replay aggregate counts ime routes");
    require(summary.routes.focus == 0, "mixed replay aggregate has no focus routes");
    require(summary.routes.total == 7, "mixed replay aggregate counts all routes");
    require(summary.pointer_capture_ended_cleanly, "mixed replay aggregate ends with clean pointer capture");
    require(summary.focus_ended_cleanly, "mixed replay aggregate ends with clean focus state");
    require(summary.preedit_ended_cleanly, "mixed replay aggregate ends with clean preedit");

    require(recording.final_state.pointer_capture_clean, "mixed replay final state has no pointer capture");
    require(recording.final_state.has_text_focus, "mixed replay final state keeps focused text target");
    require(recording.final_state.focus_id == "answer", "mixed replay final state preserves focus id");
    require(recording.final_state.preedit_clean, "mixed replay final state clears preedit");
    require(engine.text_model().text() == utf8(u8"한"), "mixed replay leaves committed ime text once");
}

void test_focus_loss_and_ime_cancel_replay_clean_preedit()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine cancel_engine;
    const std::array cancel_steps{
        step("preedit", ime(raw_platform_ime_phase::preedit_update, 200, "draft")),
        step("cancel", ime(raw_platform_ime_phase::cancel, 220)),
    };
    const normalized_input_replay_recording cancel_recording = replay_normalized_input_fixture(
        cancel_engine,
        cancel_steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    require(cancel_recording.batches[0].summary.routes.ime == 1,
        "ime cancel replay preedit batch counts ime route");
    require(!cancel_recording.batches[0].summary.preedit_ended_cleanly,
        "ime cancel replay preedit batch reports active preedit");
    require(cancel_recording.batches[1].summary.routes.ime == 1,
        "ime cancel replay cancel batch counts ime route");
    require(cancel_recording.batches[1].summary.preedit_ended_cleanly,
        "ime cancel replay cancel batch reports clean preedit");
    require(cancel_recording.summary.routes.ime == 2, "ime cancel replay aggregate counts ime routes");
    require(cancel_recording.summary.preedit_ended_cleanly,
        "ime cancel replay aggregate ends with clean preedit");
    require(cancel_recording.final_state.has_text_focus, "ime cancel replay keeps focus");
    require(cancel_recording.final_state.preedit_clean, "ime cancel replay final state clears preedit");

    input_engine focus_engine;
    const std::array focus_steps{
        step("preedit", ime(raw_platform_ime_phase::preedit_update, 300, "draft")),
        step("focus-lost", focus(raw_platform_focus_phase::lost, 320)),
    };
    const normalized_input_replay_recording focus_recording = replay_normalized_input_fixture(
        focus_engine,
        focus_steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    require(focus_recording.batches[1].summary.routes.ime == 1,
        "focus loss replay counts ime cancel route");
    require(focus_recording.batches[1].summary.routes.focus == 1,
        "focus loss replay counts focus route");
    require(focus_recording.summary.routes.ime == 2,
        "focus loss replay aggregate counts preedit and focus-loss cancel ime routes");
    require(focus_recording.summary.routes.focus == 1,
        "focus loss replay aggregate counts focus loss route");
    require(focus_recording.summary.preedit_ended_cleanly,
        "focus loss replay aggregate ends with clean preedit");
    require(focus_recording.summary.focus_ended_cleanly,
        "focus loss replay aggregate ends with clean focus state");
    require(!focus_recording.final_state.has_text_focus, "focus loss replay final state clears focus");
    require(focus_recording.final_state.focus_id.empty(), "focus loss replay final state clears focus id");
    require(focus_recording.final_state.preedit_clean, "focus loss replay final state clears preedit");
}

void test_pointer_cancel_and_release_replay_have_no_stale_capture()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine touch_cancel_engine;
    const std::array touch_cancel_steps{
        step("touch-down", pointer(raw_platform_pointer_phase::down, 400, 0.0f, 0.0f, raw_platform_pointer_button::none, 8)),
        step("touch-drag", pointer(raw_platform_pointer_phase::move, 420, 10.0f, 0.0f, raw_platform_pointer_button::none, 8)),
        step("touch-cancel", pointer(raw_platform_pointer_phase::cancel, 440, 12.0f, 0.0f, raw_platform_pointer_button::none, 8)),
    };
    const normalized_input_replay_recording cancel_recording =
        replay_normalized_input_fixture(touch_cancel_engine, touch_cancel_steps);

    require(cancel_recording.summary.normalized_events.drag_start == 1,
        "touch cancel replay counts drag start");
    require(cancel_recording.summary.normalized_events.drag_cancel == 1,
        "touch cancel replay counts drag cancel");
    require(cancel_recording.summary.routes.pointer == 4,
        "touch cancel replay counts down, drag, cancel, and reset pointer routes");
    require(cancel_recording.summary.pointer_capture_ended_cleanly,
        "touch cancel replay aggregate ends with clean pointer capture");
    require(cancel_recording.final_state.pointer_capture_clean,
        "touch cancel replay final state has no stale capture");
    require(cancel_recording.batches[2].summary.pointer_capture_ended_cleanly,
        "touch cancel replay cancel batch clears pointer capture");

    input_engine mouse_release_engine;
    const std::array mouse_release_steps{
        step("mouse-down", pointer(raw_platform_pointer_phase::down, 500, 0.0f, 0.0f, raw_platform_pointer_button::primary, 9)),
        step("mouse-drag", pointer(raw_platform_pointer_phase::move, 520, 10.0f, 0.0f, raw_platform_pointer_button::primary, 9)),
        step("mouse-up", pointer(raw_platform_pointer_phase::up, 540, 12.0f, 0.0f, raw_platform_pointer_button::primary, 9)),
    };
    const normalized_input_replay_recording release_recording =
        replay_normalized_input_fixture(mouse_release_engine, mouse_release_steps);

    require(release_recording.summary.normalized_events.drag_start == 1,
        "mouse release replay counts drag start");
    require(release_recording.summary.normalized_events.drag_end == 1,
        "mouse release replay counts drag end");
    require(release_recording.summary.routes.pointer == 3,
        "mouse release replay counts down, drag, and release pointer routes");
    require(release_recording.summary.pointer_capture_ended_cleanly,
        "mouse release replay aggregate ends with clean pointer capture");
    require(release_recording.final_state.pointer_capture_clean,
        "mouse release replay final state has no stale capture");
    require(release_recording.batches[2].end_state.pointer_capture_clean,
        "mouse release replay release batch clears pointer capture");
}

} // namespace

int main()
{
    test_mixed_fixture_records_stable_summary_counts();
    test_focus_loss_and_ime_cancel_replay_clean_preedit();
    test_pointer_cancel_and_release_replay_have_no_stale_capture();

    std::cout << "normalized_input_replay_tests passed\n";
    return 0;
}
