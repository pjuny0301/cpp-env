#include "core/input/normalized_input_replay.h"

#include <array>
#include <cstddef>
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

void require_range(
    quiz_vulkan::input::text_range range,
    std::size_t start_byte,
    std::size_t end_byte,
    const char* message)
{
    require(range.start_byte == start_byte, message);
    require(range.end_byte == end_byte, message);
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

quiz_vulkan::raw_platform_input_event key(
    std::int64_t timestamp_ms,
    std::string logical_key,
    bool repeat = false,
    quiz_vulkan::raw_platform_key_phase phase = quiz_vulkan::raw_platform_key_phase::down,
    bool ctrl = false,
    bool shift = false,
    bool meta = false,
    bool alt = false)
{
    return quiz_vulkan::raw_platform_key_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
        .key_code = 0,
        .logical_key = std::move(logical_key),
        .alt = alt,
        .ctrl = ctrl,
        .shift = shift,
        .meta = meta,
        .repeat = repeat,
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
    require(recording.final_state.text == utf8(u8"한"), "mixed replay final state records committed text");
    require(recording.final_state.display_text == utf8(u8"한"), "mixed replay final state records display text");
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

void test_keyboard_shortcut_replay_summarizes_chords_and_final_state()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    const std::array steps{
        step("text-initial", text(600, initial)),
        step("home", key(610, "Home")),
        step("delete", key(620, "Delete")),
        step("arrow-right", key(630, "ArrowRight")),
        step("shift-arrow-left", key(640, "ArrowLeft", false, raw_platform_key_phase::down, false, true)),
        step("ctrl-a", key(650, "a", false, raw_platform_key_phase::down, true)),
        step("repeat-backspace", key(660, "Backspace", true)),
        step("text-ok", text(670, "ok")),
        step("enter", key(680, "Enter")),
        step("repeat-enter", key(690, "Enter", true)),
        step("alt-escape", key(700, "Escape", false, raw_platform_key_phase::down, false, false, false, true)),
        step("tab", key(710, "Tab")),
        step("repeat-shift-tab", key(720, "Tab", true, raw_platform_key_phase::down, false, true)),
    };

    const normalized_input_replay_recording recording = replay_normalized_input_fixture(
        engine,
        steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    require(recording.batches.size() == steps.size(), "keyboard replay records one batch per step");
    require(recording.summary.routes.text == 11, "keyboard replay aggregate counts text routes");
    require(recording.summary.routes.focus == 2, "keyboard replay aggregate counts focus traversal routes");
    require(recording.summary.routes.total == 13, "keyboard replay aggregate counts all routes");

    const normalized_input_replay_keyboard_summary& keyboard = recording.keyboard;
    require(keyboard.total == 11, "keyboard replay aggregate counts all keyboard chords");
    require(keyboard.chords.size() == 11, "keyboard replay aggregate stores keyboard chord snapshots");
    require(keyboard.emitted_input_event_routes == 8,
        "keyboard replay aggregate counts key routes with emitted input events");
    require(keyboard.diagnostic_only_routes == 3,
        "keyboard replay aggregate counts diagnostic-only key routes");
    require(keyboard.intents.focus_traversal_next == 1,
        "keyboard replay aggregate counts next traversal intent");
    require(keyboard.intents.focus_traversal_previous == 1,
        "keyboard replay aggregate counts previous traversal intent");
    require(keyboard.intents.submit == 2, "keyboard replay aggregate counts submit and suppressed submit");
    require(keyboard.intents.cancel == 1, "keyboard replay aggregate counts cancel intent");
    require(keyboard.intents.caret_next == 1, "keyboard replay aggregate counts caret next intent");
    require(keyboard.intents.caret_home == 1, "keyboard replay aggregate counts caret home intent");
    require(keyboard.intents.selection_previous == 1,
        "keyboard replay aggregate counts selection previous intent");
    require(keyboard.intents.select_all == 1, "keyboard replay aggregate counts select all intent");
    require(keyboard.intents.delete_backward == 1, "keyboard replay aggregate counts delete backward intent");
    require(keyboard.intents.delete_forward == 1, "keyboard replay aggregate counts delete forward intent");
    require(keyboard.intents.none == 0, "keyboard replay aggregate has no unknown shortcut intent");
    require(keyboard.modifiers.unmodified == 7, "keyboard replay aggregate counts unmodified chords");
    require(keyboard.modifiers.alt == 1, "keyboard replay aggregate counts alt chord");
    require(keyboard.modifiers.ctrl == 1, "keyboard replay aggregate counts ctrl chord");
    require(keyboard.modifiers.shift == 2, "keyboard replay aggregate counts shift chords");
    require(keyboard.modifiers.meta == 0, "keyboard replay aggregate counts no meta chords");
    require(keyboard.repeat_policies.not_repeat == 8,
        "keyboard replay aggregate counts non-repeat key policies");
    require(keyboard.repeat_policies.allowed == 1,
        "keyboard replay aggregate counts allowed repeat key policy");
    require(keyboard.repeat_policies.ignored == 2,
        "keyboard replay aggregate counts ignored repeat key policies");

    require(keyboard.chords[0].logical_key == "Home", "keyboard replay stores first chord logical key");
    require(keyboard.chords[0].intent == keyboard_shortcut_intent::caret_home,
        "keyboard replay stores first chord intent");
    require(recording.batches[2].keyboard.intents.delete_forward == 1,
        "keyboard replay delete batch counts delete-forward intent");
    require(recording.batches[2].end_state.text == std::string(utf8(u8"한")) + "B",
        "keyboard replay delete batch records post-delete text state");
    require(recording.batches[2].end_state.caret_byte_offset == 0,
        "keyboard replay delete batch records caret at delete position");
    require(recording.batches[4].keyboard.modifiers.shift == 1,
        "keyboard replay shift-arrow batch records shift modifier");
    require(recording.batches[5].keyboard.modifiers.ctrl == 1,
        "keyboard replay ctrl-a batch records ctrl modifier");
    require(recording.batches[6].keyboard.repeat_policies.allowed == 1,
        "keyboard replay repeat backspace batch records allowed repeat policy");
    require(recording.batches[9].input_events.empty(), "keyboard replay repeat enter emits no event");
    require(recording.batches[9].keyboard.repeat_policies.ignored == 1,
        "keyboard replay repeat enter batch records ignored repeat policy");
    require(recording.batches[10].keyboard.modifiers.alt == 1,
        "keyboard replay escape batch records alt modifier");
    require(recording.batches[10].keyboard.intents.cancel == 1,
        "keyboard replay escape batch counts cancel intent");
    require(recording.batches[12].keyboard.intents.focus_traversal_previous == 1,
        "keyboard replay repeat shift-tab batch counts previous traversal intent");
    require(recording.batches[12].keyboard.repeat_policies.ignored == 1,
        "keyboard replay repeat shift-tab batch records ignored repeat policy");

    require(recording.final_state.has_text_focus, "keyboard replay final state keeps text focus");
    require(recording.final_state.focus_id == "answer", "keyboard replay final state preserves focus id");
    require(recording.final_state.text.empty(), "keyboard replay final state records cleared committed text");
    require(recording.final_state.display_text.empty(), "keyboard replay final state records cleared display text");
    require(recording.final_state.caret_byte_offset == 0, "keyboard replay final state records caret offset");
    require(!recording.final_state.has_selection, "keyboard replay final state records no selection");
    require_range(recording.final_state.selection, 0, 0, "keyboard replay final selection is collapsed");
    require(recording.final_state.preedit_text.empty(), "keyboard replay final state records empty preedit");
    require(recording.final_state.preedit_clean, "keyboard replay final state has clean preedit");
}

void test_ime_replay_timeline_records_composition_lifecycle()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    const std::array steps{
        step("text-initial", text(800, initial)),
        step("home", key(810, "Home")),
        step("select-first", key(820, "ArrowRight", false, raw_platform_key_phase::down, false, true)),
        step("composition-start", ime(raw_platform_ime_phase::composition_start, 830)),
        step("preedit-jamo", ime(raw_platform_ime_phase::preedit_update, 840, utf8(u8"ㅎ"))),
        step("preedit-syllable", ime(raw_platform_ime_phase::preedit_update, 850, utf8(u8"하"))),
        step("commit", ime(raw_platform_ime_phase::commit, 860, utf8(u8"가"))),
        step("second-composition-start", ime(raw_platform_ime_phase::composition_start, 870)),
        step("second-preedit", ime(raw_platform_ime_phase::preedit_update, 880, "x")),
        step("focus-lost", focus(raw_platform_focus_phase::lost, 890)),
    };

    const normalized_input_replay_recording recording = replay_normalized_input_fixture(
        engine,
        steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    const normalized_input_replay_ime_summary& ime_summary = recording.ime;
    require(ime_summary.total == 7, "ime replay timeline counts all ime route entries");
    require(ime_summary.timeline.size() == 7, "ime replay timeline stores all ime route entries");
    require(ime_summary.phases.composition_start == 2, "ime replay timeline counts composition starts");
    require(ime_summary.phases.preedit_update == 3, "ime replay timeline counts preedit updates");
    require(ime_summary.phases.commit == 1, "ime replay timeline counts commits");
    require(ime_summary.phases.cancel == 1, "ime replay timeline counts cancels");
    require(ime_summary.emitted_input_event_routes == 5, "ime replay timeline counts emitted ime routes");
    require(ime_summary.diagnostic_only_routes == 2, "ime replay timeline counts diagnostic-only starts");
    require(ime_summary.all_preedit_text_valid, "ime replay timeline validates all preedit text");
    require(ime_summary.all_preedit_ranges_valid, "ime replay timeline validates all preedit ranges");
    require(ime_summary.stale_preedit_cleared, "ime replay timeline records stale preedit cleared");
    require(ime_summary.final_committed_text == std::string(utf8(u8"가")) + utf8(u8"한") + "B",
        "ime replay timeline records final committed text");
    require(ime_summary.final_display_text == ime_summary.final_committed_text,
        "ime replay timeline records final display text");
    require(ime_summary.final_preedit_text.empty(), "ime replay timeline records empty final preedit");
    require(ime_summary.final_preedit_clean, "ime replay timeline records clean final preedit");
    require(!ime_summary.final_has_selection, "ime replay timeline records no final selection");
    require_range(ime_summary.final_caret, 3, 3, "ime replay timeline records final caret");

    const normalized_input_replay_ime_timeline_entry& start = ime_summary.timeline[0];
    require(start.phase == normalized_input_replay_ime_timeline_phase::composition_start,
        "ime replay timeline first entry is composition start");
    require(!start.emits_input_event, "ime replay composition start is diagnostic-only");
    require(start.target_id == "answer", "ime replay composition start preserves target id");
    require(start.composition.active, "ime replay composition start records active composition");
    require(start.composition.preedit_text.empty(), "ime replay composition start records empty preedit");
    require_range(start.composition.replacement_range, 0, 1,
        "ime replay composition start records selected replacement range");
    require_range(start.composition.preedit_range, 0, 0,
        "ime replay composition start records collapsed preedit range");
    require(start.had_selection_before, "ime replay composition start records selected state before");
    require(start.has_selection_after, "ime replay composition start keeps replacement selection after");
    require_range(start.selection_before, 0, 1, "ime replay composition start records selection before");
    require_range(start.selection_after, 0, 1, "ime replay composition start records selection after");
    require(start.preedit_text_valid, "ime replay composition start preedit is valid");
    require(start.preedit_range_valid, "ime replay composition start preedit range is valid");

    const normalized_input_replay_ime_timeline_entry& first_preedit = ime_summary.timeline[1];
    require(first_preedit.phase == normalized_input_replay_ime_timeline_phase::preedit_update,
        "ime replay second entry is preedit update");
    require(first_preedit.emits_input_event, "ime replay preedit emits ime event");
    require(first_preedit.utf8_text == utf8(u8"ㅎ"), "ime replay preedit records emitted utf8 text");
    require(first_preedit.composition.preedit_text == utf8(u8"ㅎ"),
        "ime replay preedit records composition preedit text");
    require_range(first_preedit.composition.preedit_range, 0, std::string(utf8(u8"ㅎ")).size(),
        "ime replay preedit records preedit range");
    require_range(first_preedit.caret_after,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "ime replay preedit records display caret after update");
    require(first_preedit.display_text_after == std::string(utf8(u8"ㅎ")) + utf8(u8"한") + "B",
        "ime replay preedit batch records display text with preedit");

    const normalized_input_replay_ime_timeline_entry& commit = ime_summary.timeline[3];
    require(commit.phase == normalized_input_replay_ime_timeline_phase::commit,
        "ime replay fourth entry is commit");
    require(commit.emits_input_event, "ime replay commit emits ime event");
    require(commit.utf8_text == utf8(u8"가"), "ime replay commit records emitted commit text");
    require(commit.committed_text == utf8(u8"가"), "ime replay commit records committed text payload");
    require(commit.composition.preedit_text == utf8(u8"하"),
        "ime replay commit records previous preedit snapshot");
    require(commit.text_byte_count_before == initial.size(),
        "ime replay commit records committed text bytes before replacement");
    require(commit.text_byte_count_after == std::string(utf8(u8"가")).size()
            + std::string(utf8(u8"한")).size() + 1,
        "ime replay commit records committed text bytes after replacement");
    require(commit.stale_preedit_cleared_after, "ime replay commit clears stale preedit after route");
    require(commit.committed_text_after == std::string(utf8(u8"가")) + utf8(u8"한") + "B",
        "ime replay commit records committed text after route");
    require(commit.preedit_text_after.empty(), "ime replay commit records cleared preedit after route");

    const normalized_input_replay_ime_timeline_entry& cancel = ime_summary.timeline[6];
    require(cancel.phase == normalized_input_replay_ime_timeline_phase::cancel,
        "ime replay final ime entry is focus-loss cancel");
    require(cancel.emits_input_event, "ime replay focus loss cancel emits ime event");
    require(cancel.composition.active, "ime replay focus loss cancel records active pre-cancel composition");
    require(cancel.composition.preedit_text == "x", "ime replay focus loss cancel records stale preedit text");
    require(cancel.stale_preedit_cleared_after, "ime replay focus loss cancel clears stale preedit");
    require(cancel.preedit_text_after.empty(), "ime replay focus loss cancel records empty preedit after route");
    require(recording.final_state.text == ime_summary.final_committed_text,
        "ime replay final state matches timeline final committed text");
    require(!recording.final_state.has_text_focus, "ime replay focus loss clears final focus");
    require(recording.final_state.preedit_clean, "ime replay final state clears stale preedit");
}

void test_ime_replay_timeline_flags_invalid_preedit_text()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    const std::string invalid_preedit = std::string("\xC3(", 2);
    const std::array steps{
        step("invalid-preedit", ime(raw_platform_ime_phase::preedit_update, 900, invalid_preedit)),
        step("cancel", ime(raw_platform_ime_phase::cancel, 910)),
    };

    const normalized_input_replay_recording recording = replay_normalized_input_fixture(
        engine,
        steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    require(recording.ime.total == 2, "invalid ime replay records preedit and cancel entries");
    require(!recording.ime.all_preedit_text_valid, "invalid ime replay flags invalid preedit text");
    require(recording.ime.all_preedit_ranges_valid, "invalid ime replay keeps byte ranges internally valid");
    require(recording.ime.stale_preedit_cleared, "invalid ime replay clears stale invalid preedit");
    require(!recording.ime.timeline[0].preedit_text_valid,
        "invalid ime replay preedit entry records invalid text");
    require(!recording.ime.timeline[1].preedit_text_valid,
        "invalid ime replay cancel entry records invalid stale preedit snapshot");
    require(recording.ime.timeline[1].stale_preedit_cleared_after,
        "invalid ime replay cancel clears stale preedit after route");
    require(recording.final_state.preedit_clean, "invalid ime replay final state has clean preedit");
    require(recording.final_state.text.empty(), "invalid ime replay final committed text remains empty");
}

} // namespace

int main()
{
    test_mixed_fixture_records_stable_summary_counts();
    test_focus_loss_and_ime_cancel_replay_clean_preedit();
    test_pointer_cancel_and_release_replay_have_no_stale_capture();
    test_keyboard_shortcut_replay_summarizes_chords_and_final_state();
    test_ime_replay_timeline_records_composition_lifecycle();
    test_ime_replay_timeline_flags_invalid_preedit_text();

    std::cout << "normalized_input_replay_tests passed\n";
    return 0;
}
