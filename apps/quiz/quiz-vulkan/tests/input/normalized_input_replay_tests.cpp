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

bool contains_pointer_id(const std::vector<std::int32_t>& pointer_ids, std::int32_t pointer_id)
{
    for (const std::int32_t existing_id : pointer_ids) {
        if (existing_id == pointer_id) {
            return true;
        }
    }
    return false;
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

quiz_vulkan::input::normalized_input_replay_step step(
    std::string label,
    quiz_vulkan::raw_platform_input_event event)
{
    return quiz_vulkan::input::normalized_input_replay_step{
        .label = std::move(label),
        .action = quiz_vulkan::input::normalized_input_replay_action{std::move(event)},
    };
}

quiz_vulkan::input::normalized_input_replay_step time_step(std::string label, std::int64_t timestamp_ms)
{
    return quiz_vulkan::input::normalized_input_replay_step{
        .label = std::move(label),
        .action = quiz_vulkan::input::normalized_input_replay_action{
            quiz_vulkan::input::normalized_input_replay_time_update{
                .timestamp_ms = timestamp_ms,
            },
        },
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
        step("wheel", platform_scroll(
            140,
            20.0f,
            30.0f,
            0.0f,
            -2.0f,
            raw_platform_scroll_delta_unit::lines,
            true,
            false,
            true)),
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
    require(recording.batches[2].pointer.timeline.size() == 1,
        "mixed replay wheel batch records pointer timeline evidence");
    require_modifiers(recording.batches[2].pointer.timeline[0].modifiers,
        true,
        false,
        true,
        false,
        "mixed replay wheel timeline preserves modifier evidence");
    require_modifiers(recording.batches[2].pointer.timeline[0].normalized_event.modifiers,
        true,
        false,
        true,
        false,
        "mixed replay wheel normalized event preserves modifier evidence");
    require(recording.batches[5].summary.routes.ime == 1,
        "mixed replay preedit batch counts ime route");
    require(!recording.batches[5].end_state.preedit_clean,
        "mixed replay preedit batch records unclean preedit end state");
    require(recording.batches[5].end_state.text_presentation.has_preedit,
        "mixed replay preedit batch exposes presentation preedit");
    require(recording.batches[5].end_state.text_presentation.display_text == utf8(u8"ㅎ"),
        "mixed replay preedit batch exposes presentation display text");
    require(recording.batches[5].text_presentation_diff.preedit_changed,
        "mixed replay preedit batch exposes presentation preedit diff");
    require(recording.batches[5].text_presentation_diff.display_text_changed,
        "mixed replay preedit batch exposes presentation display text diff");
    require(recording.batches[5].text_presentation_diff.route_byte_diagnostics.available.after_value,
        "mixed replay preedit batch exposes presentation route diagnostics");
    require(recording.batches[6].end_state.text_presentation.committed_text == utf8(u8"한"),
        "mixed replay ime commit batch exposes presentation committed text");
    require(!recording.batches[6].end_state.text_presentation.has_preedit,
        "mixed replay ime commit batch exposes cleared presentation preedit");
    require(recording.batches[6].text_presentation_diff.committed_text_changed,
        "mixed replay ime commit batch exposes presentation committed text diff");

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
    require(recording.final_state.text_presentation.target_id == "answer",
        "mixed replay final state exposes presentation target id");
    require(recording.final_state.text_presentation.committed_text == utf8(u8"한"),
        "mixed replay final state exposes presentation committed text");
    require(recording.final_state.text_presentation.display_text == utf8(u8"한"),
        "mixed replay final state exposes presentation display text");
    require(recording.final_state.text_presentation.byte_counts.committed_text_bytes
                == std::string(utf8(u8"한")).size(),
        "mixed replay final state exposes presentation byte counts");
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

void test_focus_caret_replay_timeline_records_navigation_and_final_state()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    const std::array steps{
        step("text-initial", text(950, initial)),
        step("focus-gained", focus(raw_platform_focus_phase::gained, 960)),
        step("home", key(970, "Home")),
        step("arrow-right", key(980, "ArrowRight")),
        step("end", key(990, "End")),
        step("shift-arrow-left", key(1000, "ArrowLeft", false, raw_platform_key_phase::down, false, true)),
        step("tab", key(1010, "Tab")),
        step("shift-tab", key(1020, "Tab", false, raw_platform_key_phase::down, false, true)),
        step("focus-lost", focus(raw_platform_focus_phase::lost, 1030)),
    };

    const normalized_input_replay_recording recording = replay_normalized_input_fixture(
        engine,
        steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    const normalized_input_replay_focus_summary& focus_summary = recording.focus;
    require(focus_summary.total == 8, "focus replay timeline counts focus and caret routes");
    require(focus_summary.timeline.size() == 8, "focus replay timeline stores all focus entries");
    require(focus_summary.kinds.focus_gain == 1, "focus replay timeline counts focus gain");
    require(focus_summary.kinds.focus_loss == 1, "focus replay timeline counts focus loss");
    require(focus_summary.kinds.focus_traversal_next == 1,
        "focus replay timeline counts next traversal");
    require(focus_summary.kinds.focus_traversal_previous == 1,
        "focus replay timeline counts previous traversal");
    require(focus_summary.kinds.caret_moved == 3, "focus replay timeline counts caret moves");
    require(focus_summary.kinds.selection_changed == 1,
        "focus replay timeline counts selection movement");
    require(focus_summary.emitted_input_event_routes == 6,
        "focus replay timeline counts emitted focus/caret events");
    require(focus_summary.diagnostic_only_routes == 2,
        "focus replay timeline counts diagnostic-only traversal routes");
    require(focus_summary.target_transition_count == 1,
        "focus replay timeline counts target transition on focus loss only");
    require(focus_summary.caret_transition_count == 4,
        "focus replay timeline counts caret movement transitions");
    require(focus_summary.selection_transition_count == 2,
        "focus replay timeline counts selection set and clear transitions");
    require(!focus_summary.final_has_focus, "focus replay timeline records final focus cleared");
    require(focus_summary.final_focus_id.empty(), "focus replay timeline records empty final focus id");
    require(focus_summary.final_text_byte_count == initial.size(),
        "focus replay timeline records final text byte count");
    require_range(focus_summary.final_caret, 4, 4, "focus replay timeline records final caret");
    require(!focus_summary.final_has_selection, "focus replay timeline records final selection cleared");
    require_range(focus_summary.final_selection, 0, 0, "focus replay timeline records empty final selection");
    require(focus_summary.final_focus_clean, "focus replay timeline records clean final focus");

    const normalized_input_replay_focus_timeline_entry& gain = focus_summary.timeline[0];
    require(gain.kind == normalized_input_replay_focus_timeline_kind::focus_gain,
        "focus replay first entry records focus gain");
    require(gain.emits_input_event, "focus replay focus gain is emitted event");
    require(gain.target_id == "answer", "focus replay focus gain records target");
    require(gain.target_id_before == "answer", "focus replay focus gain records target before");
    require(gain.target_id_after == "answer", "focus replay focus gain records target after");
    require(gain.had_focus_before, "focus replay focus gain records prior text focus");
    require(gain.has_focus_after, "focus replay focus gain records focus after");
    require(!gain.target_changed, "focus replay focus gain keeps target id stable");
    require_range(gain.caret_before, initial.size(), initial.size(),
        "focus replay focus gain records caret before");
    require_range(gain.caret_after, initial.size(), initial.size(),
        "focus replay focus gain records caret after");

    const normalized_input_replay_focus_timeline_entry& home = focus_summary.timeline[1];
    require(home.kind == normalized_input_replay_focus_timeline_kind::caret_moved,
        "focus replay home records caret movement");
    require(home.keyboard.logical_key == "Home", "focus replay home records logical key");
    require(home.keyboard.intent == keyboard_shortcut_intent::caret_home,
        "focus replay home records caret-home intent");
    require_range(home.caret_before, initial.size(), initial.size(),
        "focus replay home records caret before");
    require_range(home.caret_after, 0, 0, "focus replay home records caret after");
    require(home.caret_changed, "focus replay home flags caret transition");

    const normalized_input_replay_focus_timeline_entry& right = focus_summary.timeline[2];
    require(right.keyboard.logical_key == "ArrowRight", "focus replay right records logical key");
    require(right.keyboard.intent == keyboard_shortcut_intent::caret_next,
        "focus replay right records caret-next intent");
    require_range(right.caret_before, 0, 0, "focus replay right records caret before");
    require_range(right.caret_after, 1, 1, "focus replay right records ASCII boundary after");

    const normalized_input_replay_focus_timeline_entry& end = focus_summary.timeline[3];
    require(end.keyboard.logical_key == "End", "focus replay end records logical key");
    require(end.keyboard.intent == keyboard_shortcut_intent::caret_end,
        "focus replay end records caret-end intent");
    require_range(end.caret_before, 1, 1, "focus replay end records caret before");
    require_range(end.caret_after, initial.size(), initial.size(),
        "focus replay end records caret after");

    const normalized_input_replay_focus_timeline_entry& selection = focus_summary.timeline[4];
    require(selection.kind == normalized_input_replay_focus_timeline_kind::selection_changed,
        "focus replay shift arrow records selection movement");
    require(selection.keyboard.logical_key == "ArrowLeft",
        "focus replay shift arrow records logical key");
    require(selection.keyboard.modifiers.shift, "focus replay shift arrow records shift modifier");
    require(selection.keyboard.intent == keyboard_shortcut_intent::selection_previous,
        "focus replay shift arrow records selection previous intent");
    require_range(selection.caret_before, initial.size(), initial.size(),
        "focus replay selection records caret before");
    require_range(selection.caret_after, 4, 4, "focus replay selection records caret after");
    require(!selection.had_selection_before, "focus replay selection records no selection before");
    require(selection.has_selection_after, "focus replay selection records selection after");
    require_range(selection.selection_after, 4, initial.size(),
        "focus replay selection records selected byte range");
    require(selection.selection_changed, "focus replay selection flags selection transition");

    const normalized_input_replay_focus_timeline_entry& traversal_next = focus_summary.timeline[5];
    require(traversal_next.kind == normalized_input_replay_focus_timeline_kind::focus_traversal_next,
        "focus replay tab records next traversal");
    require(!traversal_next.emits_input_event, "focus replay tab is diagnostic-only");
    require(traversal_next.keyboard.logical_key == "Tab", "focus replay tab records logical key");
    require(traversal_next.keyboard.intent == keyboard_shortcut_intent::focus_traversal_next,
        "focus replay tab records next intent");
    require(!traversal_next.target_changed, "focus replay tab does not mutate target id");

    const normalized_input_replay_focus_timeline_entry& traversal_previous = focus_summary.timeline[6];
    require(traversal_previous.kind == normalized_input_replay_focus_timeline_kind::focus_traversal_previous,
        "focus replay shift tab records previous traversal");
    require(!traversal_previous.emits_input_event, "focus replay shift tab is diagnostic-only");
    require(traversal_previous.keyboard.modifiers.shift, "focus replay shift tab records shift modifier");
    require(traversal_previous.keyboard.intent == keyboard_shortcut_intent::focus_traversal_previous,
        "focus replay shift tab records previous intent");

    const normalized_input_replay_focus_timeline_entry& loss = focus_summary.timeline[7];
    require(loss.kind == normalized_input_replay_focus_timeline_kind::focus_loss,
        "focus replay final entry records focus loss");
    require(loss.emits_input_event, "focus replay focus loss emits focus-lost event");
    require(loss.target_id == "answer", "focus replay focus loss records previous target");
    require(loss.target_id_before == "answer", "focus replay focus loss records target before");
    require(loss.target_id_after.empty(), "focus replay focus loss records empty target after");
    require(loss.had_focus_before, "focus replay focus loss records focus before");
    require(!loss.has_focus_after, "focus replay focus loss records focus cleared");
    require(loss.target_changed, "focus replay focus loss flags target transition");
    require(loss.has_selection_after == false, "focus replay focus loss clears selection");
    require(loss.selection_changed, "focus replay focus loss flags selection clear");
    require(loss.focus_clean_after, "focus replay focus loss ends clean");
    require(recording.final_state.focus_id == focus_summary.final_focus_id,
        "focus replay final state matches focus summary id");
    require(recording.final_state.caret_byte_offset == focus_summary.final_caret.start_byte,
        "focus replay final state matches focus summary caret");
}

void test_replay_diff_diagnostics_compare_recordings_without_semantics()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    const std::string before_text = std::string("A") + utf8(u8"한") + "B";
    input_engine before_engine;
    const std::array before_steps{
        step("before-text", text(1200, before_text)),
        step("before-focus-lost", focus(raw_platform_focus_phase::lost, 1210)),
    };
    const normalized_input_replay_recording before_recording = replay_normalized_input_fixture(
        before_engine,
        before_steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    const std::string after_text = before_text + "C";
    input_engine after_engine;
    const std::array after_steps{
        step("after-text", text(1300, after_text)),
        step("after-home", key(1310, "Home")),
        step("after-arrow-right", key(1320, "ArrowRight")),
        step("after-ime-preedit", ime(raw_platform_ime_phase::preedit_update, 1330, utf8(u8"ㅎ"))),
        step("after-tab", key(1340, "Tab")),
        step("after-drag-down", pointer(raw_platform_pointer_phase::down, 1350, 0.0f, 0.0f, raw_platform_pointer_button::primary, 77)),
        step("after-drag-move", pointer(raw_platform_pointer_phase::move, 1360, 12.0f, 0.0f, raw_platform_pointer_button::primary, 77)),
    };
    const normalized_input_replay_recording after_recording = replay_normalized_input_fixture(
        after_engine,
        after_steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    const normalized_input_replay_diff diff =
        diff_normalized_input_replay_recordings(before_recording, after_recording);

    require(diff.regression.changed, "replay diff summary marks changed recordings");
    require(diff.regression.changed_category_count == 9,
        "replay diff summary counts semantic-free changed categories");
    require(diff.regression.batch_count.delta == 5, "replay diff summary records batch delta");
    require(diff.regression.normalized_event_count.delta == 1,
        "replay diff summary records normalized event delta");
    require(diff.regression.route_count.delta == 5, "replay diff summary records route delta");
    require(diff.regression.final_state_changed, "replay diff summary records final state change");
    require(diff.regression.focus_caret_selection_changed,
        "replay diff summary records focus/caret change");
    require(diff.regression.pointer_capture_changed,
        "replay diff summary records pointer capture change");
    require(diff.regression.pointer_timeline_changed,
        "replay diff summary records pointer timeline change");
    require(diff.regression.gesture_policy_changed,
        "replay diff summary records gesture policy changes");
    require(diff.regression.ime_timeline_changed, "replay diff summary records ime timeline change");
    require(diff.regression.keyboard_changed, "replay diff summary records keyboard changes");
    require(diff.regression.text_or_preedit_changed,
        "replay diff summary records text and preedit changes");
    require(diff.regression.focus_timeline_changed, "replay diff summary records focus timeline change");

    require(diff.final_state.changed, "replay diff final state changed");
    require(diff.final_state.focus_changed, "replay diff final focus changed");
    require(diff.final_state.has_focus.before_value == false,
        "replay diff final focus before is false");
    require(diff.final_state.has_focus.after_value == true,
        "replay diff final focus after is true");
    require(diff.final_state.focus_id.before_value.empty(),
        "replay diff final focus id before is empty");
    require(diff.final_state.focus_id.after_value == "answer",
        "replay diff final focus id after is target");
    require(diff.final_state.caret_changed, "replay diff final caret changed");
    require_range(diff.final_state.caret.before_range, before_text.size(), before_text.size(),
        "replay diff final caret before");
    require_range(diff.final_state.caret.after_range, 1, 1, "replay diff final caret after");
    require(diff.final_state.text_changed, "replay diff final text changed");
    require(diff.final_state.text.byte_delta == 1, "replay diff final text byte delta");
    require(diff.final_state.display_text_changed, "replay diff final display text changed");
    require(diff.final_state.preedit_changed, "replay diff final preedit changed");
    require(diff.final_state.text_presentation_changed,
        "replay diff final presentation snapshot changed");
    require(diff.final_state.text_presentation.target_changed,
        "replay diff presentation target changed");
    require(diff.final_state.text_presentation.target_id.before_value.empty(),
        "replay diff presentation target before is empty");
    require(diff.final_state.text_presentation.target_id.after_value == "answer",
        "replay diff presentation target after is recorded");
    require(diff.final_state.text_presentation.committed_text_changed,
        "replay diff presentation committed text changed");
    require(diff.final_state.text_presentation.committed_text.byte_delta == 1,
        "replay diff presentation committed text byte delta");
    require(diff.final_state.text_presentation.display_text_changed,
        "replay diff presentation display text changed");
    require(diff.final_state.text_presentation.preedit_changed,
        "replay diff presentation preedit changed");
    require(diff.final_state.text_presentation.preedit_text.after_value == utf8(u8"ㅎ"),
        "replay diff presentation preedit after is recorded");
    require(diff.final_state.text_presentation.caret_changed,
        "replay diff presentation caret changed");
    require_range(diff.final_state.text_presentation.caret_range.after_range,
        1 + std::string(utf8(u8"ㅎ")).size(),
        1 + std::string(utf8(u8"ㅎ")).size(),
        "replay diff presentation display caret after");
    require(diff.final_state.text_presentation.byte_counts.preedit_text_bytes.after_count
                == std::string(utf8(u8"ㅎ")).size(),
        "replay diff presentation preedit byte count after");
    require(diff.final_state.preedit_text.before_value.empty(),
        "replay diff preedit before is empty");
    require(diff.final_state.preedit_text.after_value == utf8(u8"ㅎ"),
        "replay diff preedit after is recorded");
    require(diff.final_state.preedit_clean.before_value,
        "replay diff preedit clean before is true");
    require(!diff.final_state.preedit_clean.after_value,
        "replay diff preedit clean after is false");
    require(diff.final_state.pointer_capture_changed,
        "replay diff final pointer capture changed");
    require(diff.final_state.pointer_capture.before_capture.lifecycle == pointer_capture_lifecycle::idle,
        "replay diff pointer capture before is idle");
    require(diff.final_state.pointer_capture.after_capture.lifecycle == pointer_capture_lifecycle::captured,
        "replay diff pointer capture after is captured");
    require(diff.final_state.pointer_capture.before_clean,
        "replay diff pointer capture before is clean");
    require(!diff.final_state.pointer_capture.after_clean,
        "replay diff pointer capture after is active");
    require(!diff.final_state.selection_changed,
        "replay diff primary comparison has no final selection change");

    require(diff.keyboard.changed, "replay diff keyboard counts changed");
    require(diff.keyboard.chord_count.delta == 3, "replay diff keyboard chord count delta");
    require(diff.keyboard.intents.caret_home.delta == 1,
        "replay diff keyboard caret-home delta");
    require(diff.keyboard.intents.caret_next.delta == 1,
        "replay diff keyboard caret-next delta");
    require(diff.keyboard.intents.focus_traversal_next.delta == 1,
        "replay diff keyboard focus traversal delta");
    require(diff.keyboard.modifiers.unmodified.delta == 3,
        "replay diff keyboard unmodified modifier delta");
    require(diff.keyboard.repeat_policies.not_repeat.delta == 3,
        "replay diff keyboard repeat policy delta");

    require(diff.pointer.changed, "replay diff pointer summary changed");
    require(diff.pointer.timeline_changed, "replay diff pointer timeline changed");
    require(diff.pointer.capture_changed, "replay diff pointer capture summary changed");
    require(diff.pointer.timeline_entries.delta == 2, "replay diff pointer timeline delta");
    require(diff.pointer.kinds.pointer_capture_arbitration.delta == 1,
        "replay diff pointer capture arbitration delta");
    require(diff.pointer.kinds.drag_start.delta == 1,
        "replay diff pointer drag start delta");
    require(diff.pointer.capture_transition_count.delta == 2,
        "replay diff pointer capture transition delta");
    require(diff.pointer.pointer_id_count.delta == 1,
        "replay diff pointer id count delta");
    require(diff.pointer.final_capture.after_capture.pointer_id == 77,
        "replay diff pointer capture records pointer id");

    require(diff.gesture_policies.changed, "replay diff gesture policies changed");
    require(diff.gesture_policies.route_count.delta == 2,
        "replay diff gesture policy route count delta");
    require(diff.gesture_policies.unpaired_after_route_count == 2,
        "replay diff gesture policy records unpaired after routes");

    require(diff.ime.changed, "replay diff ime summary changed");
    require(diff.ime.timeline_changed, "replay diff ime timeline changed");
    require(diff.ime.timeline_entries.delta == 1, "replay diff ime timeline delta");
    require(diff.ime.phases.preedit_update.delta == 1,
        "replay diff ime preedit phase delta");
    require(diff.ime.final_preedit_changed,
        "replay diff ime final preedit changed");
    require(diff.ime.final_preedit_text.after_value == utf8(u8"ㅎ"),
        "replay diff ime final preedit after");
    require(diff.ime.final_preedit_clean.before_value,
        "replay diff ime final preedit clean before");
    require(!diff.ime.final_preedit_clean.after_value,
        "replay diff ime final preedit clean after");

    require(diff.focus.changed, "replay diff focus summary changed");
    require(diff.focus.timeline_changed, "replay diff focus timeline changed");
    require(diff.focus.timeline_entries.delta == 2, "replay diff focus timeline delta");
    require(diff.focus.kinds.focus_loss.delta == -1,
        "replay diff focus loss delta");
    require(diff.focus.kinds.focus_traversal_next.delta == 1,
        "replay diff focus traversal next delta");
    require(diff.focus.kinds.caret_moved.delta == 2,
        "replay diff focus caret moved delta");
    require(diff.focus.final_focus_changed, "replay diff focus final focus changed");
    require(diff.focus.final_caret_changed, "replay diff focus final caret changed");
    require(!diff.focus.final_selection_changed,
        "replay diff primary focus final selection unchanged");

    const normalized_input_replay_diff stable_diff =
        diff_normalized_input_replay_recordings(after_recording, after_recording);
    require(!stable_diff.regression.changed, "replay diff self comparison is unchanged");
    require(stable_diff.regression.changed_category_count == 0,
        "replay diff self comparison has no changed categories");
    require(!stable_diff.final_state.changed, "replay diff self final state unchanged");
    require(!stable_diff.final_state.text_presentation_changed,
        "replay diff self presentation unchanged");
    require(!stable_diff.keyboard.changed, "replay diff self keyboard unchanged");
    require(!stable_diff.pointer.changed, "replay diff self pointer unchanged");
    require(!stable_diff.gesture_policies.changed, "replay diff self gesture policy unchanged");
    require(!stable_diff.regression.gesture_policy_changed,
        "replay diff self regression has no gesture policy category");
    require(!stable_diff.ime.changed, "replay diff self ime unchanged");
    require(!stable_diff.focus.changed, "replay diff self focus unchanged");

    input_engine selection_before_engine;
    const std::array selection_before_steps{
        step("selection-before-text", text(1400, "AB")),
        step("selection-before-home", key(1410, "Home")),
    };
    const normalized_input_replay_recording selection_before = replay_normalized_input_fixture(
        selection_before_engine,
        selection_before_steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    input_engine selection_after_engine;
    const std::array selection_after_steps{
        step("selection-after-text", text(1500, "AB")),
        step("selection-after-home", key(1510, "Home")),
        step("selection-after-shift-right", key(1520, "ArrowRight", false, raw_platform_key_phase::down, false, true)),
    };
    const normalized_input_replay_recording selection_after = replay_normalized_input_fixture(
        selection_after_engine,
        selection_after_steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    const normalized_input_replay_diff selection_diff =
        diff_normalized_input_replay_recordings(selection_before, selection_after);
    require(selection_diff.final_state.selection_changed,
        "replay diff final selection changed");
    require(!selection_diff.final_state.has_selection.before_value,
        "replay diff final selection before absent");
    require(selection_diff.final_state.has_selection.after_value,
        "replay diff final selection after present");
    require_range(selection_diff.final_state.selection.after_range, 0, 1,
        "replay diff final selection range after");
    require(selection_diff.final_state.text_presentation.selection_changed,
        "replay diff presentation final selection changed");
    require_range(selection_diff.final_state.text_presentation.selection_range.after_range, 0, 1,
        "replay diff presentation final selection range after");
    require(selection_diff.focus.final_selection_changed,
        "replay diff focus final selection changed");
    require(selection_diff.keyboard.intents.selection_next.delta == 1,
        "replay diff keyboard selection-next delta");
    require(selection_diff.regression.focus_caret_selection_changed,
        "replay diff regression records focus caret selection category");
}

void test_replay_diff_threads_gesture_policy_route_diagnostics()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    const gesture_thresholds before_thresholds{
        .swipe_min_dx = 60.0f,
        .swipe_max_dy = 20.0f,
        .swipe_max_duration_ms = 300,
        .long_press_min_duration_ms = 600,
        .tap_slop = 8.0f,
        .drag_start_slop = 8.0f,
    };
    const gesture_thresholds after_thresholds{
        .swipe_min_dx = 80.0f,
        .swipe_max_dy = 40.0f,
        .swipe_max_duration_ms = 800,
        .long_press_min_duration_ms = 700,
        .tap_slop = 8.0f,
        .drag_start_slop = 6.0f,
    };

    input_engine before_engine{before_thresholds};
    input_engine after_engine{after_thresholds};
    const std::array steps{
        step("accepted-before-down", pointer(raw_platform_pointer_phase::down, 1000, 0.0f, 0.0f, raw_platform_pointer_button::none, 101)),
        step("accepted-before-up", pointer(raw_platform_pointer_phase::up, 1100, 70.0f, 10.0f, raw_platform_pointer_button::none, 101)),
        step("recovered-after-down", pointer(raw_platform_pointer_phase::down, 2000, 0.0f, 0.0f, raw_platform_pointer_button::none, 102)),
        step("recovered-after-up", pointer(raw_platform_pointer_phase::up, 2500, 90.0f, 30.0f, raw_platform_pointer_button::none, 102)),
        step("long-press-down", pointer(raw_platform_pointer_phase::down, 3000, 0.0f, 0.0f, raw_platform_pointer_button::none, 103)),
        time_step("long-press-time", 3750),
        step("long-press-up", pointer(raw_platform_pointer_phase::up, 3760, 0.0f, 0.0f, raw_platform_pointer_button::none, 103)),
        step("drag-down", pointer(raw_platform_pointer_phase::down, 4000, 0.0f, 0.0f, raw_platform_pointer_button::primary, 104)),
        step("drag-move", pointer(raw_platform_pointer_phase::move, 4020, 9.0f, 0.0f, raw_platform_pointer_button::primary, 104)),
        step("drag-up", pointer(raw_platform_pointer_phase::up, 4040, 10.0f, 0.0f, raw_platform_pointer_button::primary, 104)),
    };

    const normalized_input_replay_recording before_recording =
        replay_normalized_input_fixture(before_engine, steps);
    const normalized_input_replay_recording after_recording =
        replay_normalized_input_fixture(after_engine, steps);

    require(before_recording.gesture_policies.total == before_recording.gesture_policies.routes.size(),
        "gesture policy replay before stores one route per summary entry");
    require(after_recording.gesture_policies.total == after_recording.gesture_policies.routes.size(),
        "gesture policy replay after stores one route per summary entry");
    require(before_recording.gesture_policies.total > 0,
        "gesture policy replay records routed gesture policies");
    const input_routing_diagnostics replay_diagnostics =
        normalized_input_replay_gesture_policy_diagnostics(before_recording.gesture_policies);
    require(replay_diagnostics.action_routes.size() == before_recording.gesture_policies.routes.size(),
        "gesture policy replay exposes reusable routing diagnostics");

    const normalized_input_replay_diff diff =
        diff_normalized_input_replay_recordings(before_recording, after_recording);

    require(diff.gesture_policies.changed,
        "replay diff reports gesture policy route diagnostics");
    require(diff.regression.gesture_policy_changed,
        "replay diff regression summary reports gesture policy category");
    require(diff.gesture_policies.route_count.delta == 0,
        "replay diff gesture policy compares stable route counts");
    require(diff.gesture_policies.compared_route_count == before_recording.gesture_policies.routes.size(),
        "replay diff gesture policy compares every routed policy");
    require(diff.gesture_policies.threshold_change_count > 0,
        "replay diff gesture policy reports threshold deltas");
    require(diff.gesture_policies.decision_change_count >= 2,
        "replay diff gesture policy reports decision deltas");
    require(diff.gesture_policies.emitted_kind_change_count >= 2,
        "replay diff gesture policy reports emitted kind deltas");
    require(diff.gesture_policies.accepted_to_suppressed_regression_count >= 1,
        "replay diff gesture policy reports accepted-to-suppressed regression");
    require(diff.gesture_policies.suppressed_to_accepted_recovery_count >= 1,
        "replay diff gesture policy reports suppressed-to-accepted recovery");
    require(diff.gesture_policies.swipe_threshold_tightening_count > 0,
        "replay diff gesture policy reports swipe tightening");
    require(diff.gesture_policies.swipe_threshold_loosening_count > 0,
        "replay diff gesture policy reports swipe loosening");
    require(diff.gesture_policies.long_press_threshold_tightening_count > 0,
        "replay diff gesture policy reports long-press tightening");
    require(diff.gesture_policies.drag_threshold_loosening_count > 0,
        "replay diff gesture policy reports drag loosening");
    require(diff.gesture_policies.pointer_mismatch_count == 0,
        "replay diff gesture policy keeps pointer ids paired");
    require(diff.gesture_policies.contact_mismatch_count == 0,
        "replay diff gesture policy keeps contacts paired");
    require(diff.gesture_policies.phase_mismatch_count == 0,
        "replay diff gesture policy keeps phases paired");

    bool saw_regression_route = false;
    bool saw_recovery_route = false;
    for (const input_routing_gesture_policy_route_diff& route : diff.gesture_policies.routes) {
        saw_regression_route = saw_regression_route || route.accepted_to_suppressed;
        saw_recovery_route = saw_recovery_route || route.suppressed_to_accepted;
    }
    require(saw_regression_route, "replay diff gesture policy preserves regression route evidence");
    require(saw_recovery_route, "replay diff gesture policy preserves recovery route evidence");

    const normalized_input_replay_diff stable_diff =
        diff_normalized_input_replay_recordings(after_recording, after_recording);
    require(!stable_diff.gesture_policies.changed,
        "replay diff gesture policy self comparison is unchanged");
    require(!stable_diff.regression.gesture_policy_changed,
        "replay diff regression self comparison has no gesture policy category");
}

void test_pointer_replay_timeline_records_gestures_capture_and_wheel()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    const std::array steps{
        step("mouse-down", pointer(raw_platform_pointer_phase::down, 1000, 0.0f, 0.0f, raw_platform_pointer_button::primary, 11)),
        step("mouse-up", pointer(raw_platform_pointer_phase::up, 1020, 1.0f, 1.0f, raw_platform_pointer_button::primary, 11)),
        step("long-press-down", pointer(raw_platform_pointer_phase::down, 1100, 4.0f, 4.0f, raw_platform_pointer_button::none, 21)),
        time_step("long-press-time", 1700),
        step("long-press-up", pointer(raw_platform_pointer_phase::up, 1710, 4.0f, 4.0f, raw_platform_pointer_button::none, 21)),
        step("swipe-down", pointer(raw_platform_pointer_phase::down, 2000, 0.0f, 0.0f, raw_platform_pointer_button::none, 31)),
        step("swipe-up", pointer(raw_platform_pointer_phase::up, 2050, 80.0f, 2.0f, raw_platform_pointer_button::none, 31)),
        step("drag-down", pointer(raw_platform_pointer_phase::down, 3000, 0.0f, 0.0f, raw_platform_pointer_button::primary, 41)),
        step("drag-move", pointer(raw_platform_pointer_phase::move, 3020, 12.0f, 0.0f, raw_platform_pointer_button::primary, 41)),
        step("drag-update", pointer(raw_platform_pointer_phase::move, 3040, 18.0f, 3.0f, raw_platform_pointer_button::primary, 41)),
        step("drag-up", pointer(raw_platform_pointer_phase::up, 3060, 20.0f, 5.0f, raw_platform_pointer_button::primary, 41)),
        step("cancel-down", pointer(raw_platform_pointer_phase::down, 4000, 0.0f, 0.0f, raw_platform_pointer_button::none, 51)),
        step("cancel-move", pointer(raw_platform_pointer_phase::move, 4020, 10.0f, 0.0f, raw_platform_pointer_button::none, 51)),
        step("cancel", pointer(raw_platform_pointer_phase::cancel, 4040, 12.0f, 0.0f, raw_platform_pointer_button::none, 51)),
        step("multi-touch-first-down", pointer(raw_platform_pointer_phase::down, 5000, 0.0f, 0.0f, raw_platform_pointer_button::none, 61)),
        step("multi-touch-second-down", pointer(raw_platform_pointer_phase::down, 5010, 1.0f, 1.0f, raw_platform_pointer_button::none, 62)),
        step("multi-touch-first-cancel", pointer(raw_platform_pointer_phase::cancel, 5020, 0.0f, 0.0f, raw_platform_pointer_button::none, 61)),
        step("multi-touch-second-up", pointer(raw_platform_pointer_phase::up, 5030, 1.0f, 1.0f, raw_platform_pointer_button::none, 62)),
        step("wheel", platform_scroll(
            6000,
            20.0f,
            30.0f,
            1.0f,
            -3.0f,
            raw_platform_scroll_delta_unit::lines,
            false,
            true,
            false,
            true)),
    };

    const normalized_input_replay_recording recording = replay_normalized_input_fixture(engine, steps);

    require(recording.batches.size() == steps.size(), "pointer replay records one batch per step");
    require(recording.pointer.timeline.size() == recording.pointer.total,
        "pointer replay aggregate stores one timeline entry per pointer route");
    require(recording.gesture_policies.total == recording.gesture_policies.routes.size(),
        "pointer replay aggregate stores gesture policy route evidence");
    require(recording.gesture_policies.total > 0,
        "pointer replay aggregate records gesture policy routes");
    require(recording.pointer.kinds.pointer_capture_arbitration >= 6,
        "pointer replay timeline counts capture arbitration entries");
    require(recording.pointer.kinds.pointer_capture_reset >= 1,
        "pointer replay timeline counts capture reset entries");
    require(recording.pointer.kinds.tap >= 2, "pointer replay timeline counts tap entries");
    require(recording.pointer.kinds.long_press == 1, "pointer replay timeline counts long press entry");
    require(recording.pointer.kinds.swipe_right == 1, "pointer replay timeline counts swipe entry");
    require(recording.pointer.kinds.drag_start == 2, "pointer replay timeline counts drag start entries");
    require(recording.pointer.kinds.drag_update == 1, "pointer replay timeline counts drag update entry");
    require(recording.pointer.kinds.drag_end == 1, "pointer replay timeline counts drag end entry");
    require(recording.pointer.kinds.drag_cancel == 1, "pointer replay timeline counts drag cancel entry");
    require(recording.pointer.kinds.wheel == 1, "pointer replay timeline counts wheel entry");
    require(recording.pointer.contacts.mouse_like >= 5, "pointer replay timeline counts mouse contact routes");
    require(recording.pointer.contacts.touch_like >= 9, "pointer replay timeline counts touch contact routes");
    require(recording.pointer.wheel_routes == 1, "pointer replay timeline counts wheel routes");
    require(recording.pointer.saw_multipointer_touch, "pointer replay timeline detects multipointer touch ids");
    require(contains_pointer_id(recording.pointer.mouse_pointer_ids, 11),
        "pointer replay timeline records mouse pointer id");
    require(contains_pointer_id(recording.pointer.touch_pointer_ids, 61),
        "pointer replay timeline records first touch pointer id");
    require(contains_pointer_id(recording.pointer.touch_pointer_ids, 62),
        "pointer replay timeline records second touch pointer id");
    require(recording.pointer.capture_transition_count >= 10,
        "pointer replay timeline counts capture lifecycle transitions");
    require(recording.pointer.final_capture_clean, "pointer replay timeline ends with clean capture");
    require(recording.final_state.pointer_capture_clean, "pointer replay final state has clean capture");

    const normalized_input_replay_pointer_summary& mouse_down = recording.batches[0].pointer;
    require(mouse_down.total == 1, "pointer replay mouse down has one pointer route");
    require(mouse_down.kinds.pointer_capture_arbitration == 1,
        "pointer replay mouse down records capture arbitration");
    require(mouse_down.timeline[0].contact == pointer_contact_kind::mouse_like,
        "pointer replay mouse down records mouse contact");
    require(mouse_down.timeline[0].decision == pointer_arbitration_decision::tracked,
        "pointer replay mouse down records tracked decision");
    require(mouse_down.timeline[0].capture_changed,
        "pointer replay mouse down records capture transition");
    require(mouse_down.timeline[0].capture_before.lifecycle == pointer_capture_lifecycle::idle,
        "pointer replay mouse down records idle capture before");
    require(mouse_down.timeline[0].capture_after.lifecycle == pointer_capture_lifecycle::tracking,
        "pointer replay mouse down records tracking capture after");
    require(!mouse_down.final_capture_clean, "pointer replay mouse down batch ends with active tracking");

    const normalized_input_replay_pointer_summary& mouse_up = recording.batches[1].pointer;
    require(mouse_up.kinds.tap == 1, "pointer replay mouse up records tap");
    require(mouse_up.timeline[0].pointer_id == 11, "pointer replay tap records pointer id");
    require(mouse_up.timeline[0].duration_ms == 20, "pointer replay tap records duration");
    require(mouse_up.timeline[0].capture_ended_cleanly_after, "pointer replay tap route releases capture");

    const normalized_input_replay_pointer_summary& long_press = recording.batches[3].pointer;
    require(long_press.kinds.long_press == 1, "pointer replay time update records long press");
    require(long_press.timeline[0].pointer_id == 21, "pointer replay long press records pointer id");
    require(long_press.timeline[0].duration_ms == 600, "pointer replay long press records duration");
    require(long_press.timeline[0].gesture_policy.decision == gesture_policy_decision::long_press_accepted,
        "pointer replay long press records gesture policy");

    const normalized_input_replay_pointer_summary& swipe = recording.batches[6].pointer;
    require(swipe.kinds.swipe_right == 1, "pointer replay swipe up records right swipe");
    require(swipe.timeline[0].pointer_id == 31, "pointer replay swipe records pointer id");
    require(swipe.timeline[0].delta_x == 0.0f,
        "pointer replay swipe keeps existing normalized swipe delta semantics");
    require(swipe.timeline[0].x == 80.0f, "pointer replay swipe records end x");

    const normalized_input_replay_pointer_summary& drag_start = recording.batches[8].pointer;
    require(drag_start.kinds.drag_start == 1, "pointer replay drag move records drag start");
    require(drag_start.timeline[0].capture_after.lifecycle == pointer_capture_lifecycle::captured,
        "pointer replay drag start captures pointer");
    require(drag_start.timeline[0].delta_x == 12.0f, "pointer replay drag start records delta");

    const normalized_input_replay_pointer_summary& drag_update = recording.batches[9].pointer;
    require(drag_update.kinds.drag_update == 1, "pointer replay drag update records update");
    require(drag_update.timeline[0].delta_x == 6.0f, "pointer replay drag update records incremental x delta");
    require(drag_update.timeline[0].delta_y == 3.0f, "pointer replay drag update records incremental y delta");

    const normalized_input_replay_pointer_summary& drag_end = recording.batches[10].pointer;
    require(drag_end.kinds.drag_end == 1, "pointer replay drag up records drag end");
    require(drag_end.timeline[0].capture_ended_cleanly_after, "pointer replay drag end releases capture");

    const normalized_input_replay_pointer_summary& drag_cancel = recording.batches[13].pointer;
    require(drag_cancel.kinds.drag_cancel == 1, "pointer replay cancel records drag cancel");
    require(drag_cancel.kinds.pointer_capture_reset == 1,
        "pointer replay cancel records capture reset diagnostic");
    require(drag_cancel.timeline.size() == 2, "pointer replay cancel stores drag and reset entries");
    require(drag_cancel.final_capture_clean, "pointer replay cancel batch ends with clean capture");

    const normalized_input_replay_pointer_summary& second_touch_down = recording.batches[15].pointer;
    require(second_touch_down.saw_multipointer_touch,
        "pointer replay second touch down batch detects multiple tracked touch ids");
    require(second_touch_down.timeline[0].tracked_pointer_count_after == 2,
        "pointer replay second touch down records two tracked pointers");

    const normalized_input_replay_pointer_summary& wheel = recording.batches[18].pointer;
    require(wheel.kinds.wheel == 1, "pointer replay wheel batch records wheel timeline");
    require(wheel.timeline[0].kind == normalized_input_replay_pointer_timeline_kind::wheel,
        "pointer replay wheel entry has wheel kind");
    require(wheel.timeline[0].line_delta_x == 1.0f, "pointer replay wheel records line x delta");
    require(wheel.timeline[0].line_delta_y == -3.0f, "pointer replay wheel records line y delta");
    require(wheel.timeline[0].pixel_delta_y == 0.0f, "pointer replay wheel records pixel delta");
    require_modifiers(wheel.timeline[0].modifiers,
        false,
        true,
        false,
        true,
        "pointer replay wheel records modifier evidence");
}

void test_combined_focus_ime_text_pointer_replay_evidence_stays_input_owned()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    const std::string initial = std::string("A") + utf8(u8"한");
    const std::array steps{
        step("text-initial", text(7000, initial)),
        step("backspace-hangul", key(7010, "Backspace")),
        step("focus-gained", focus(raw_platform_focus_phase::gained, 7020)),
        step("home", key(7030, "Home")),
        step("select-ascii", key(7040, "ArrowRight", false, raw_platform_key_phase::down, false, true)),
        step("composition-start", ime(raw_platform_ime_phase::composition_start, 7050)),
        step("preedit-jamo", ime(raw_platform_ime_phase::preedit_update, 7060, utf8(u8"ㅎ"))),
        step("preedit-syllable", ime(raw_platform_ime_phase::preedit_update, 7070, utf8(u8"하"))),
        step("commit-replacement", ime(raw_platform_ime_phase::commit, 7080, utf8(u8"가"))),
        step("second-composition-start", ime(raw_platform_ime_phase::composition_start, 7090)),
        step("second-preedit", ime(raw_platform_ime_phase::preedit_update, 7100, "x")),
        step("cancel-second", ime(raw_platform_ime_phase::cancel, 7110)),
        step("touch-down", pointer(raw_platform_pointer_phase::down, 7120, 0.0f, 0.0f, raw_platform_pointer_button::none, 90)),
        step("touch-drag", pointer(raw_platform_pointer_phase::move, 7140, 12.0f, 0.0f, raw_platform_pointer_button::none, 90)),
        step("touch-cancel", pointer(raw_platform_pointer_phase::cancel, 7160, 12.0f, 0.0f, raw_platform_pointer_button::none, 90)),
        step("wheel", platform_scroll(
            7170,
            2.0f,
            3.0f,
            0.0f,
            -1.0f,
            raw_platform_scroll_delta_unit::lines,
            false,
            true,
            true)),
        step("focus-lost", focus(raw_platform_focus_phase::lost, 7180)),
    };

    const normalized_input_replay_recording recording = replay_normalized_input_fixture(
        engine,
        steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});

    require(recording.batches.size() == steps.size(), "combined replay records one batch per step");
    require(recording.summary.routes.text == 4,
        "combined replay counts text commit, backspace, caret, and selection routes");
    require(recording.summary.routes.ime == 7,
        "combined replay counts ime start update commit cancel routes");
    require(recording.summary.routes.focus == 1, "combined replay counts focus loss route");
    require(recording.summary.routes.pointer == 4,
        "combined replay counts touch down drag cancel and reset pointer routes");
    require(recording.summary.routes.wheel == 1, "combined replay counts wheel route");
    require(recording.summary.normalized_events.drag_start == 1,
        "combined replay counts drag start normalized event");
    require(recording.summary.normalized_events.drag_cancel == 1,
        "combined replay counts drag cancel normalized event");
    require(recording.summary.normalized_events.wheel == 1,
        "combined replay counts wheel normalized event");
    require(recording.summary.pointer_capture_ended_cleanly,
        "combined replay aggregate ends with clean pointer capture");
    require(recording.summary.preedit_ended_cleanly,
        "combined replay aggregate ends with clean preedit");
    require(recording.summary.focus_ended_cleanly,
        "combined replay aggregate ends with clean focus state");

    const normalized_input_replay_batch& backspace_batch = recording.batches[1];
    require(backspace_batch.keyboard.intents.delete_backward == 1,
        "combined replay backspace batch records delete-backward intent");
    require(backspace_batch.keyboard.repeat_policies.not_repeat == 1,
        "combined replay backspace batch records non-repeat policy");
    require(backspace_batch.end_state.text == "A",
        "combined replay backspace removes exactly the trailing multibyte character");
    require(backspace_batch.end_state.caret_byte_offset == 1,
        "combined replay backspace lands caret on an ASCII boundary");
    require(backspace_batch.text_presentation_diff.committed_text.byte_delta
                == -static_cast<std::int64_t>(std::string(utf8(u8"한")).size()),
        "combined replay backspace records multibyte byte delta");
    require(backspace_batch.text_presentation_diff.route_byte_diagnostics.available.after_value,
        "combined replay backspace exposes route byte diagnostics");
    require(backspace_batch.text_presentation_diff.route_byte_diagnostics.text_byte_delta.after_value
                == -static_cast<std::int64_t>(std::string(utf8(u8"한")).size()),
        "combined replay backspace route records multibyte deletion delta");

    const normalized_input_replay_focus_summary& focus_summary = recording.focus;
    require(focus_summary.kinds.focus_gain == 1, "combined replay distinguishes focus gain");
    require(focus_summary.kinds.focus_loss == 1, "combined replay distinguishes focus loss");
    require(focus_summary.kinds.caret_moved == 1, "combined replay distinguishes caret movement");
    require(focus_summary.kinds.selection_changed == 1,
        "combined replay distinguishes selection movement");
    require(focus_summary.target_transition_count == 1,
        "combined replay records target transition only on focus loss");
    require(focus_summary.caret_transition_count >= 2,
        "combined replay records caret movement and focus-loss caret transition evidence");
    require(focus_summary.selection_transition_count >= 1,
        "combined replay records focus-owned selection movement evidence");
    require(!focus_summary.final_has_focus, "combined replay focus final state is cleared");
    require(focus_summary.final_focus_id.empty(), "combined replay focus final id is empty");
    require(focus_summary.final_focus_clean, "combined replay focus final state is clean");

    const normalized_input_replay_focus_timeline_entry& selection =
        recording.batches[4].focus.timeline.front();
    require(selection.kind == normalized_input_replay_focus_timeline_kind::selection_changed,
        "combined replay selection batch records selection timeline kind");
    require(selection.keyboard.intent == keyboard_shortcut_intent::selection_next,
        "combined replay selection batch records shortcut intent");
    require(selection.keyboard.modifiers.shift, "combined replay selection batch records shift modifier");
    require_range(selection.selection_after, 0, 1,
        "combined replay selection batch records selected byte range");

    const normalized_input_replay_ime_summary& ime_summary = recording.ime;
    require(ime_summary.phases.composition_start == 2,
        "combined replay distinguishes ime composition starts");
    require(ime_summary.phases.preedit_update == 3,
        "combined replay distinguishes ime preedit updates");
    require(ime_summary.phases.commit == 1, "combined replay distinguishes ime commit");
    require(ime_summary.phases.cancel == 1, "combined replay distinguishes ime cancel");
    require(ime_summary.emitted_input_event_routes == 5,
        "combined replay counts emitted ime update commit cancel events");
    require(ime_summary.diagnostic_only_routes == 2,
        "combined replay counts diagnostic-only ime starts");
    require(ime_summary.all_preedit_text_valid,
        "combined replay keeps preedit text valid");
    require(ime_summary.all_preedit_ranges_valid,
        "combined replay keeps preedit ranges valid");
    require(ime_summary.stale_preedit_cleared,
        "combined replay records cancel clearing stale preedit");
    require(ime_summary.final_committed_text == utf8(u8"가"),
        "combined replay ime final committed text is semantic-free text state");
    require(ime_summary.final_preedit_text.empty(),
        "combined replay ime final preedit is empty");
    require(ime_summary.final_preedit_clean,
        "combined replay ime final preedit state is clean");

    const normalized_input_replay_ime_timeline_entry& ime_start =
        recording.batches[5].ime.timeline.front();
    require(ime_start.phase == normalized_input_replay_ime_timeline_phase::composition_start,
        "combined replay start batch records composition-start phase");
    require(!ime_start.emits_input_event,
        "combined replay composition start is diagnostic-only");
    require_range(ime_start.composition.replacement_range, 0, 1,
        "combined replay composition start records selected replacement range");

    const normalized_input_replay_ime_timeline_entry& ime_update =
        recording.batches[7].ime.timeline.front();
    require(ime_update.phase == normalized_input_replay_ime_timeline_phase::preedit_update,
        "combined replay update batch records preedit-update phase");
    require(ime_update.utf8_text == utf8(u8"하"),
        "combined replay update batch records preedit text");
    require(ime_update.display_text_after == std::string(utf8(u8"하")),
        "combined replay update batch records display text with preedit");

    const normalized_input_replay_ime_timeline_entry& ime_commit =
        recording.batches[8].ime.timeline.front();
    require(ime_commit.phase == normalized_input_replay_ime_timeline_phase::commit,
        "combined replay commit batch records commit phase");
    require(ime_commit.committed_text == utf8(u8"가"),
        "combined replay commit batch records committed text payload");
    require(ime_commit.stale_preedit_cleared_after,
        "combined replay commit batch clears preedit state");

    const normalized_input_replay_ime_timeline_entry& ime_cancel =
        recording.batches[11].ime.timeline.front();
    require(ime_cancel.phase == normalized_input_replay_ime_timeline_phase::cancel,
        "combined replay cancel batch records cancel phase");
    require(ime_cancel.composition.preedit_text == "x",
        "combined replay cancel batch records stale preedit snapshot");
    require(ime_cancel.stale_preedit_cleared_after,
        "combined replay cancel batch clears stale preedit");

    const normalized_input_replay_pointer_summary& pointer_summary = recording.pointer;
    require(pointer_summary.kinds.pointer_capture_arbitration == 1,
        "combined replay distinguishes pointer arbitration");
    require(pointer_summary.kinds.drag_start == 1, "combined replay distinguishes drag start");
    require(pointer_summary.kinds.drag_cancel == 1, "combined replay distinguishes drag cancel");
    require(pointer_summary.kinds.pointer_capture_reset == 1,
        "combined replay distinguishes pointer capture reset");
    require(pointer_summary.kinds.wheel == 1, "combined replay distinguishes wheel timeline");
    require(pointer_summary.decisions.tracked == 1,
        "combined replay records tracked pointer decision");
    require(pointer_summary.decisions.captured == 1,
        "combined replay records captured pointer decision");
    require(pointer_summary.decisions.canceled == 2,
        "combined replay records canceled pointer decisions for drag and reset");
    require(pointer_summary.final_capture_clean,
        "combined replay pointer summary ends with clean capture");
    require(contains_pointer_id(pointer_summary.touch_pointer_ids, 90),
        "combined replay records touch pointer id");

    const normalized_input_replay_pointer_timeline_entry& drag_start =
        recording.batches[13].pointer.timeline.front();
    require(drag_start.kind == normalized_input_replay_pointer_timeline_kind::drag_start,
        "combined replay drag batch records drag-start timeline kind");
    require(drag_start.capture_after.lifecycle == pointer_capture_lifecycle::captured,
        "combined replay drag batch records captured lifecycle");

    const normalized_input_replay_pointer_summary& cancel_summary = recording.batches[14].pointer;
    require(cancel_summary.timeline.size() == 2,
        "combined replay cancel batch records drag cancel plus capture reset");
    require(cancel_summary.timeline[1].kind == normalized_input_replay_pointer_timeline_kind::pointer_capture_reset,
        "combined replay cancel batch records reset timeline kind");
    require(cancel_summary.final_capture_clean,
        "combined replay cancel batch ends with clean capture");

    const normalized_input_replay_pointer_summary& wheel_summary = recording.batches[15].pointer;
    require(wheel_summary.kinds.wheel == 1, "combined replay wheel batch records wheel");
    require(wheel_summary.timeline[0].line_delta_y == -1.0f,
        "combined replay wheel batch records normalized line delta");
    require_modifiers(wheel_summary.timeline[0].modifiers,
        false,
        true,
        true,
        false,
        "combined replay wheel batch records modifier evidence");

    require(!recording.final_state.has_text_focus,
        "combined replay final state has no app-level focus dispatch");
    require(recording.final_state.focus_id.empty(),
        "combined replay final state clears focus id");
    require(recording.final_state.text == utf8(u8"가"),
        "combined replay final state keeps committed input text");
    require(recording.final_state.display_text == utf8(u8"가"),
        "combined replay final state keeps display input text");
    require(recording.final_state.preedit_clean,
        "combined replay final state has clean preedit");
    require(recording.final_state.pointer_capture_clean,
        "combined replay final state has clean pointer capture");
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
    test_focus_caret_replay_timeline_records_navigation_and_final_state();
    test_replay_diff_diagnostics_compare_recordings_without_semantics();
    test_replay_diff_threads_gesture_policy_route_diagnostics();
    test_pointer_replay_timeline_records_gestures_capture_and_wheel();
    test_combined_focus_ime_text_pointer_replay_evidence_stays_input_owned();

    std::cout << "normalized_input_replay_tests passed\n";
    return 0;
}
