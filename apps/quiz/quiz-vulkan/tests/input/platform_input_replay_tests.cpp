#include "core/input/platform_input_replay.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
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

    std::cerr << "platform_input_replay_tests failed: " << message << '\n';
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

bool contains_pointer_id(const std::vector<std::int32_t>& pointer_ids, std::int32_t pointer_id)
{
    for (const std::int32_t existing_id : pointer_ids) {
        if (existing_id == pointer_id) {
            return true;
        }
    }
    return false;
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

quiz_vulkan::input::platform_input_replay_step request_step(
    std::string label,
    quiz_vulkan::input::platform_input_sample sample)
{
    return quiz_vulkan::input::platform_input_replay_step{
        .label = std::move(label),
        .action = quiz_vulkan::input::platform_input_replay_action{
            quiz_vulkan::input::platform_input_translation_request{
                .sample = std::move(sample),
            },
        },
    };
}

quiz_vulkan::input::platform_input_replay_step focus_step(
    std::string label,
    quiz_vulkan::raw_platform_focus_phase phase,
    std::int64_t timestamp_ms)
{
    return quiz_vulkan::input::platform_input_replay_step{
        .label = std::move(label),
        .action = quiz_vulkan::input::platform_input_replay_action{
            quiz_vulkan::raw_platform_focus_event{
                .timestamp_ms = timestamp_ms,
                .phase = phase,
            },
        },
    };
}

quiz_vulkan::input::platform_input_replay_step time_step(
    std::string label,
    std::int64_t timestamp_ms)
{
    return quiz_vulkan::input::platform_input_replay_step{
        .label = std::move(label),
        .action = quiz_vulkan::input::platform_input_replay_action{
            quiz_vulkan::input::normalized_input_replay_time_update{
                .timestamp_ms = timestamp_ms,
            },
        },
    };
}

void test_mixed_platform_replay_summarizes_sources_routes_and_text_transactions()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    const std::array steps{
        request_step("mouse down", platform_mouse_sample{
            .timestamp_ms = 100,
            .pointer_id = 1,
            .phase = platform_pointer_sample_phase::down,
            .button = platform_mouse_button::primary,
            .x = 4.0f,
            .y = 6.0f,
        }),
        request_step("mouse up", platform_mouse_sample{
            .timestamp_ms = 130,
            .pointer_id = 1,
            .phase = platform_pointer_sample_phase::up,
            .button = platform_mouse_button::primary,
            .x = 5.0f,
            .y = 7.0f,
        }),
        request_step("touch down", platform_touch_sample{
            .timestamp_ms = 200,
            .contact_id = 7,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("touch swipe", platform_touch_sample{
            .timestamp_ms = 250,
            .contact_id = 7,
            .phase = platform_pointer_sample_phase::up,
            .x = 80.0f,
            .y = 0.0f,
        }),
        request_step("wheel", platform_wheel_sample{
            .timestamp_ms = 260,
            .x = 10.0f,
            .y = 20.0f,
            .delta_x = 0.0f,
            .delta_y = -2.0f,
            .unit = platform_scroll_delta_unit::lines,
        }),
        request_step("char", platform_character_sample{
            .timestamp_ms = 270,
            .utf8_text = "A",
        }),
        request_step("backspace", platform_key_sample{
            .timestamp_ms = 280,
            .phase = platform_key_sample_phase::down,
            .key_code = 8,
            .logical_key = "Backspace",
        }),
        request_step("ime preedit", platform_ime_composition_sample{
            .timestamp_ms = 290,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = utf8(u8"ㅎ"),
        }),
        request_step("ime commit", platform_ime_composition_sample{
            .timestamp_ms = 300,
            .phase = platform_ime_sample_phase::commit,
            .utf8_text = utf8(u8"한"),
        }),
        focus_step("focus lost", raw_platform_focus_phase::lost, 310),
    };

    const platform_input_replay_summary replay = replay_platform_input_batch(
        std::span{steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});

    require(replay.steps.size() == steps.size(), "platform replay stores one diagnostic per step");
    require(replay.events.total == 10, "platform replay counts total source events");
    require(replay.events.pointer == 4, "platform replay counts mouse and touch as pointer events");
    require(replay.events.mouse == 2, "platform replay counts mouse source events");
    require(replay.events.touch == 2, "platform replay counts touch source events");
    require(replay.events.wheel == 1, "platform replay counts wheel source events");
    require(replay.events.key == 1, "platform replay counts key source events");
    require(replay.events.character == 1, "platform replay counts character source events");
    require(replay.events.ime == 2, "platform replay counts ime source events");
    require(replay.events.focus == 1, "platform replay counts raw focus source events");
    require(replay.translations.total == 9, "platform replay translates all platform requests");
    require(replay.translations.accepted == 9, "platform replay counts accepted translations");
    require(replay.translations.rejected == 0, "platform replay has no rejected translations");
    require(replay.translations.dispatched_to_engine == 9,
        "platform replay dispatches accepted translations through engine adapter");

    require(replay.route_summary.normalized_event_count == 3,
        "platform replay aggregates normalized pointer and wheel events");
    require(replay.route_summary.normalized_events.tap == 1, "platform replay counts tap event");
    require(replay.route_summary.normalized_events.swipe_right == 1,
        "platform replay counts touch swipe event");
    require(replay.route_summary.normalized_events.wheel == 1, "platform replay counts wheel event");
    require(replay.route_summary.routes.pointer == 4, "platform replay aggregates pointer routes");
    require(replay.route_summary.routes.wheel == 1, "platform replay aggregates wheel routes");
    require(replay.route_summary.routes.text == 2, "platform replay aggregates text edit routes");
    require(replay.route_summary.routes.ime == 2, "platform replay aggregates ime routes");
    require(replay.route_summary.routes.focus == 1, "platform replay aggregates focus route");
    require(replay.route_summary.routes.total == 10, "platform replay aggregates all routes");
    require(replay.route_summary.pointer_capture_ended_cleanly,
        "platform replay ends with clean pointer capture");
    require(replay.route_summary.preedit_ended_cleanly, "platform replay ends with clean preedit");
    require(replay.pointer.final_capture_clean, "platform pointer summary ends with clean capture");
    require(replay.pointer.kinds.tap == 1, "platform pointer summary counts tap");
    require(replay.pointer.kinds.swipe_right == 1, "platform pointer summary counts swipe right");
    require(replay.pointer.wheel_routes == 1, "platform pointer summary counts wheel route");
    require(replay.gesture_policies.total == 4, "platform gesture policy summary counts pointer routes");
    require(replay.focus.total == 1, "platform focus summary counts focus loss");
    require(replay.focus.kinds.focus_loss == 1, "platform focus summary records focus loss kind");
    require(!replay.focus.final_has_focus, "platform focus summary records cleared final focus");

    require(replay.text_edits.counts.step_count == 5,
        "platform replay text edit summary counts only text-model transactions");
    require(replay.text_edits.counts.operations.commit_utf8 == 1,
        "platform replay text summary counts character commit");
    require(replay.text_edits.counts.operations.backspace == 1,
        "platform replay text summary counts backspace");
    require(replay.text_edits.counts.operations.set_preedit == 1,
        "platform replay text summary counts preedit");
    require(replay.text_edits.counts.operations.commit_ime == 1,
        "platform replay text summary counts ime commit");
    require(replay.text_edits.counts.operations.clear_focus == 1,
        "platform replay text summary counts focus clear");
    require(replay.text_edits.counts.inserted_byte_count == 1 + std::string(utf8(u8"한")).size(),
        "platform replay text summary aggregates inserted bytes");
    require(replay.text_edits.counts.deleted_byte_count == 1,
        "platform replay text summary aggregates backspace deleted bytes");
    require(replay.text_edits.counts.ime_preedit_started_count == 1,
        "platform replay text summary counts preedit start");
    require(replay.text_edits.counts.ime_committed_count == 1,
        "platform replay text summary counts ime commit");

    require(!replay.final_text_presentation.has_focus, "platform replay final presentation clears focus");
    require(replay.final_text_presentation.target_id.empty(),
        "platform replay final presentation clears target id");
    require(replay.final_text_presentation.committed_text == utf8(u8"한"),
        "platform replay final presentation keeps committed text");
    require(replay.final_text_presentation.display_text == utf8(u8"한"),
        "platform replay final presentation keeps display text");
    require(!replay.final_text_presentation.has_preedit, "platform replay final presentation clears preedit");
    require(replay.normalized.final_state.text_presentation.committed_text == utf8(u8"한"),
        "platform replay normalized final state carries presentation snapshot");
    require(replay.steps[9].source == platform_input_replay_source_kind::focus,
        "platform replay records raw focus source on focus step");
    require(replay.steps[9].produced_text_edit_transaction,
        "platform replay records focus loss text transaction");
}

void test_platform_replay_rejections_are_side_effect_free_and_diffable()
{
    using namespace quiz_vulkan::input;

    const std::array before_steps{
        request_step("char before", platform_character_sample{
            .timestamp_ms = 400,
            .utf8_text = "A",
        }),
    };
    const std::array after_steps{
        request_step("char after", platform_character_sample{
            .timestamp_ms = 400,
            .utf8_text = "AB",
        }),
        request_step("invalid char", platform_character_sample{
            .timestamp_ms = 410,
            .utf8_text = std::string("\xC0\xAF", 2),
        }),
        request_step("preedit after", platform_ime_composition_sample{
            .timestamp_ms = 420,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = "x",
        }),
    };

    const platform_input_replay_summary before = replay_platform_input_batch(
        std::span{before_steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});
    const platform_input_replay_summary after = replay_platform_input_batch(
        std::span{after_steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});
    const platform_input_replay_diff diff = diff_platform_input_replay_summaries(before, after);

    require(after.steps[1].source == platform_input_replay_source_kind::character,
        "platform replay rejected step records character source");
    require(after.steps[1].has_translation, "platform replay rejected step stores translation diagnostic");
    require(after.steps[1].translation.status == platform_input_translation_status::rejected,
        "platform replay rejected step records rejected translation status");
    require(after.steps[1].translation.rejection_reason == platform_input_rejection_reason::invalid_utf8,
        "platform replay rejected step records invalid utf8 reason");
    require(after.steps[1].rejected_without_side_effect,
        "platform replay rejected step records side-effect-free rejection");
    require(!after.steps[1].dispatched_to_engine,
        "platform replay rejected step is not dispatched to engine");
    require(after.steps[1].normalized_batch.summary.routes.total == 0,
        "platform replay rejected step contributes no route summary");
    require(after.steps[1].before_presentation.committed_text == "AB",
        "platform replay rejected step records before presentation text");
    require(after.steps[1].after_presentation.committed_text == "AB",
        "platform replay rejected step records unchanged after presentation text");
    require(!after.steps[1].text_presentation_diff.changed,
        "platform replay rejected step has no text presentation diff");

    require(after.events.total == 3, "platform replay after counts all source events");
    require(after.events.character == 2, "platform replay after counts valid and invalid character samples");
    require(after.events.ime == 1, "platform replay after counts ime sample");
    require(after.translations.total == 3, "platform replay after counts all translations");
    require(after.translations.accepted == 2, "platform replay after counts accepted translations");
    require(after.translations.rejected == 1, "platform replay after counts rejected translations");
    require(after.translations.rejected_without_side_effect == 1,
        "platform replay after counts side-effect-free rejection");
    require(after.translations.rejections.invalid_utf8 == 1,
        "platform replay after counts invalid utf8 rejection");
    require(after.text_edits.counts.step_count == 2,
        "platform replay after text summary excludes rejected translation");
    require(after.text_edits.counts.operations.commit_utf8 == 1,
        "platform replay after text summary counts valid character commit");
    require(after.text_edits.counts.operations.set_preedit == 1,
        "platform replay after text summary counts ime preedit");
    require(after.final_text_presentation.committed_text == "AB",
        "platform replay after final presentation keeps committed text");
    require(after.final_text_presentation.has_preedit,
        "platform replay after final presentation exposes active preedit");
    require(after.final_text_presentation.preedit_text == "x",
        "platform replay after final presentation exposes preedit text");

    require(diff.changed, "platform replay diff detects changed summaries");
    require(diff.event_counts_changed, "platform replay diff detects source count changes");
    require(diff.translation_counts_changed, "platform replay diff detects translation count changes");
    require(diff.normalized_changed, "platform replay diff detects normalized replay changes");
    require(diff.text_edit_changed, "platform replay diff detects text edit replay changes");
    require(diff.final_text_presentation_changed,
        "platform replay diff detects final presentation changes");
    require(diff.events.character.delta == 1, "platform replay diff records character count delta");
    require(diff.events.ime.delta == 1, "platform replay diff records ime count delta");
    require(diff.translations.rejected.delta == 1,
        "platform replay diff records rejected translation delta");
    require(diff.translations.rejections.invalid_utf8.delta == 1,
        "platform replay diff records invalid utf8 rejection delta");
    require(diff.text_edits.ime_transition_changed,
        "platform replay diff records ime text edit transition delta");
    require(diff.final_text_presentation.committed_text.before_value == "A",
        "platform replay diff records final committed text before");
    require(diff.final_text_presentation.committed_text.after_value == "AB",
        "platform replay diff records final committed text after");
    require(diff.final_text_presentation.preedit_text.after_value == "x",
        "platform replay diff records final preedit text after");

    const platform_input_replay_diff stable = diff_platform_input_replay_summaries(after, after);
    require(!stable.changed, "identical platform replay summaries have unchanged diff");
    require(stable.changed_category_count == 0,
        "identical platform replay summaries have no changed categories");
}

void test_ime_composition_lifecycle_restart_and_cancel_are_summarized()
{
    using namespace quiz_vulkan::input;

    const std::array steps{
        request_step("composition start", platform_ime_composition_sample{
            .timestamp_ms = 500,
            .phase = platform_ime_sample_phase::composition_start,
        }),
        request_step("preedit jamo", platform_ime_composition_sample{
            .timestamp_ms = 510,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = utf8(u8"ㅎ"),
        }),
        request_step("preedit syllable", platform_ime_composition_sample{
            .timestamp_ms = 520,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = utf8(u8"하"),
        }),
        request_step("commit syllable", platform_ime_composition_sample{
            .timestamp_ms = 530,
            .phase = platform_ime_sample_phase::commit,
            .utf8_text = utf8(u8"한"),
        }),
        request_step("second composition start", platform_ime_composition_sample{
            .timestamp_ms = 540,
            .phase = platform_ime_sample_phase::composition_start,
        }),
        request_step("second preedit", platform_ime_composition_sample{
            .timestamp_ms = 550,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = "x",
        }),
        request_step("restart composition", platform_ime_composition_sample{
            .timestamp_ms = 560,
            .phase = platform_ime_sample_phase::composition_start,
        }),
        request_step("restart preedit", platform_ime_composition_sample{
            .timestamp_ms = 570,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = "y",
        }),
        request_step("cancel restarted composition", platform_ime_composition_sample{
            .timestamp_ms = 580,
            .phase = platform_ime_sample_phase::cancel,
        }),
    };

    const platform_input_replay_summary replay = replay_platform_input_batch(
        std::span{steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});

    require(replay.events.ime == steps.size(), "ime lifecycle replay counts all ime source samples");
    require(replay.translations.accepted == steps.size(),
        "ime lifecycle replay accepts all ime source samples");
    require(replay.route_summary.routes.ime == 10,
        "ime lifecycle replay counts restart as cancel plus composition start routes");
    require(replay.ime.total == 10, "ime lifecycle replay stores all ime timeline entries");
    require(replay.ime.phases.composition_start == 3,
        "ime lifecycle replay counts composition starts including restart");
    require(replay.ime.phases.preedit_update == 4,
        "ime lifecycle replay counts preedit updates");
    require(replay.ime.phases.commit == 1, "ime lifecycle replay counts commit");
    require(replay.ime.phases.cancel == 2,
        "ime lifecycle replay counts explicit and restart cancels");
    require(replay.ime.emitted_input_event_routes == 7,
        "ime lifecycle replay counts emitted preedit commit and cancel routes");
    require(replay.ime.diagnostic_only_routes == 3,
        "ime lifecycle replay counts diagnostic-only composition starts");
    require(replay.ime.all_preedit_text_valid, "ime lifecycle replay validates preedit text");
    require(replay.ime.all_preedit_ranges_valid, "ime lifecycle replay validates preedit ranges");
    require(!replay.ime.stale_preedit_cleared,
        "ime lifecycle replay preserves restart stale-preedit evidence");
    require(replay.ime.final_committed_text == utf8(u8"한"),
        "ime lifecycle replay preserves committed text once");
    require(replay.ime.final_preedit_text.empty(), "ime lifecycle replay ends with empty preedit");
    require(replay.ime.final_preedit_clean, "ime lifecycle replay ends with clean preedit");
    require(replay.final_text_presentation.committed_text == utf8(u8"한"),
        "ime lifecycle final presentation exposes committed text");
    require(!replay.final_text_presentation.has_preedit,
        "ime lifecycle final presentation has no stale preedit");

    const platform_input_replay_step_diagnostics& restart = replay.steps[6];
    require(restart.normalized_batch.summary.routes.ime == 2,
        "ime restart step records cancel plus composition-start routes");
    require(restart.normalized_batch.ime.phases.cancel == 1,
        "ime restart step records cancel phase");
    require(restart.normalized_batch.ime.phases.composition_start == 1,
        "ime restart step records replacement composition start phase");
    require(restart.normalized_batch.ime.timeline.size() == 2,
        "ime restart step records two timeline entries");
    require(restart.normalized_batch.ime.timeline[0].phase == normalized_input_replay_ime_timeline_phase::cancel,
        "ime restart first timeline entry is cancel");
    require(restart.normalized_batch.ime.timeline[0].composition.preedit_text == "x",
        "ime restart cancel captures stale preedit text");
    require(!restart.normalized_batch.ime.timeline[0].stale_preedit_cleared_after,
        "ime restart cancel records replacement composition remains active");
    require(restart.normalized_batch.ime.timeline[1].phase
            == normalized_input_replay_ime_timeline_phase::composition_start,
        "ime restart second timeline entry is composition start");
    require(replay.steps.back().normalized_batch.ime.stale_preedit_cleared,
        "ime final cancel step clears stale preedit");

    require(replay.text_edits.counts.operations.set_preedit == 7,
        "ime lifecycle text edit summary counts composition starts and preedit updates");
    require(replay.text_edits.counts.operations.commit_ime == 1,
        "ime lifecycle text edit summary counts ime commit");
    require(replay.text_edits.counts.operations.cancel_ime == 1,
        "ime lifecycle text edit summary counts final explicit cancel");
    require(replay.text_edits.counts.ime_committed_count == 1,
        "ime lifecycle text edit summary counts commit transition");
    require(replay.text_edits.counts.ime_canceled_count == 1,
        "ime lifecycle text edit summary counts explicit cancel transition");
    require(replay.text_edits.counts.utf8_boundary_unsafe_count == 0,
        "ime lifecycle text edit summary stays utf8 boundary safe");
}

void test_utf8_text_and_backspace_replay_stay_boundary_safe()
{
    using namespace quiz_vulkan::input;

    const std::string text = std::string("A") + utf8(u8"한");
    const std::array steps{
        request_step("commit mixed text", platform_character_sample{
            .timestamp_ms = 600,
            .utf8_text = text,
        }),
        request_step("backspace utf8", platform_key_sample{
            .timestamp_ms = 610,
            .phase = platform_key_sample_phase::down,
            .key_code = 8,
            .logical_key = "Backspace",
        }),
        request_step("backspace ascii", platform_key_sample{
            .timestamp_ms = 620,
            .phase = platform_key_sample_phase::down,
            .key_code = 8,
            .logical_key = "Backspace",
        }),
    };

    const platform_input_replay_summary replay = replay_platform_input_batch(
        std::span{steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});

    require(replay.text_edits.counts.step_count == 3, "utf8 replay records three text edit steps");
    require(replay.text_edits.counts.operations.commit_utf8 == 1,
        "utf8 replay counts one commit");
    require(replay.text_edits.counts.operations.backspace == 2,
        "utf8 replay counts two backspaces");
    require(replay.text_edits.counts.utf8_boundary_safe_count == 3,
        "utf8 replay marks every text edit boundary safe");
    require(replay.text_edits.counts.utf8_boundary_unsafe_count == 0,
        "utf8 replay has no unsafe utf8 boundaries");
    require(replay.text_edits.counts.inserted_byte_count == text.size(),
        "utf8 replay records inserted mixed-width bytes");
    require(replay.text_edits.counts.deleted_byte_count == text.size(),
        "utf8 replay records deleted mixed-width bytes");
    require(replay.final_text_presentation.committed_text.empty(),
        "utf8 replay final committed text is empty");
    require(replay.final_text_presentation.display_text.empty(),
        "utf8 replay final display text is empty");
    require_range(replay.final_text_presentation.caret_range, 0, 0,
        "utf8 replay final caret returns to zero");

    const text_edit_transaction_replay_step_diagnostics& utf8_backspace = replay.text_edits.steps[1];
    require(utf8_backspace.operation == text_edit_transaction_operation::backspace,
        "utf8 replay second text edit is backspace");
    require(utf8_backspace.utf8_boundary_safe,
        "utf8 replay backspace over multibyte character is boundary safe");
    require(utf8_backspace.transaction.bytes.deleted_byte_count == std::string(utf8(u8"한")).size(),
        "utf8 replay backspace removes whole multibyte character");
    require_range(utf8_backspace.caret_before, text.size(), text.size(),
        "utf8 replay records caret before multibyte backspace");
    require_range(utf8_backspace.caret_after, 1, 1,
        "utf8 replay records caret after multibyte backspace");
}

void test_pointer_cancel_restart_and_wheel_preserve_capture_and_focus_evidence()
{
    using namespace quiz_vulkan::input;

    const std::array steps{
        request_step("touch down", platform_touch_sample{
            .timestamp_ms = 700,
            .contact_id = 11,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("touch drag", platform_touch_sample{
            .timestamp_ms = 720,
            .contact_id = 11,
            .phase = platform_pointer_sample_phase::move,
            .x = 20.0f,
            .y = 0.0f,
        }),
        request_step("touch cancel", platform_touch_sample{
            .timestamp_ms = 740,
            .contact_id = 11,
            .phase = platform_pointer_sample_phase::cancel,
            .x = 22.0f,
            .y = 0.0f,
        }),
        request_step("wheel after cancel", platform_wheel_sample{
            .timestamp_ms = 750,
            .x = 4.0f,
            .y = 6.0f,
            .delta_x = 0.0f,
            .delta_y = -1.0f,
            .unit = platform_scroll_delta_unit::lines,
        }),
        request_step("restart touch down", platform_touch_sample{
            .timestamp_ms = 760,
            .contact_id = 12,
            .phase = platform_pointer_sample_phase::down,
            .x = 1.0f,
            .y = 1.0f,
        }),
        request_step("restart touch up", platform_touch_sample{
            .timestamp_ms = 780,
            .contact_id = 12,
            .phase = platform_pointer_sample_phase::up,
            .x = 1.0f,
            .y = 1.0f,
        }),
    };

    const platform_input_replay_summary replay = replay_platform_input_batch(
        std::span{steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});

    require(replay.events.touch == 5, "pointer replay counts touch samples");
    require(replay.events.wheel == 1, "pointer replay counts wheel sample");
    require(replay.route_summary.pointer_capture_ended_cleanly,
        "pointer replay aggregate ends with clean capture");
    require(replay.pointer.final_capture_clean, "pointer replay final capture is clean");
    require(replay.pointer.kinds.pointer_capture_reset == 1,
        "pointer replay records capture reset on cancel");
    require(replay.pointer.kinds.drag_start == 1, "pointer replay records drag start");
    require(replay.pointer.kinds.drag_cancel == 1, "pointer replay records drag cancel");
    require(replay.pointer.kinds.tap == 1, "pointer replay records restarted tap");
    require(replay.pointer.kinds.wheel == 1, "pointer replay records wheel");
    require(replay.pointer.wheel_routes == 1, "pointer replay records wheel route count");
    require(contains_pointer_id(replay.pointer.touch_pointer_ids, 11),
        "pointer replay tracks first touch id");
    require(contains_pointer_id(replay.pointer.touch_pointer_ids, 12),
        "pointer replay tracks restarted touch id");
    require(replay.pointer.saw_multipointer_touch,
        "pointer replay records multiple touch ids across replay");
    require(replay.focus.final_has_focus, "pointer replay preserves text focus");
    require(replay.focus.final_focus_id == "answer", "pointer replay preserves focus id");
    require(replay.final_text_presentation.has_focus,
        "pointer replay final presentation preserves focus");
    require(replay.final_text_presentation.target_id == "answer",
        "pointer replay final presentation preserves target id");

    const platform_input_replay_step_diagnostics& cancel = replay.steps[2];
    require(cancel.normalized_batch.pointer.final_capture_clean,
        "pointer replay cancel batch clears capture");
    require(cancel.normalized_batch.pointer.kinds.drag_cancel == 1,
        "pointer replay cancel batch records drag cancel");
    require(cancel.normalized_batch.pointer.kinds.pointer_capture_reset == 1,
        "pointer replay cancel batch records pointer capture reset");

    const platform_input_replay_step_diagnostics& wheel = replay.steps[3];
    require(wheel.normalized_batch.pointer.wheel_routes == 1,
        "pointer replay wheel batch records wheel route");
    require(wheel.normalized_batch.end_state.pointer_capture_clean,
        "pointer replay wheel batch has no stale capture");
    require(wheel.normalized_batch.end_state.has_text_focus,
        "pointer replay wheel batch preserves focus state");
}

void test_platform_replay_diff_flags_ime_utf8_pointer_and_wheel_changes()
{
    using namespace quiz_vulkan::input;

    const std::array before_steps{
        request_step("commit before", platform_character_sample{
            .timestamp_ms = 800,
            .utf8_text = "A",
        }),
    };
    const std::array after_steps{
        request_step("commit after", platform_character_sample{
            .timestamp_ms = 800,
            .utf8_text = std::string("A") + utf8(u8"한"),
        }),
        request_step("backspace after", platform_key_sample{
            .timestamp_ms = 810,
            .phase = platform_key_sample_phase::down,
            .key_code = 8,
            .logical_key = "Backspace",
        }),
        request_step("preedit after", platform_ime_composition_sample{
            .timestamp_ms = 820,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = "x",
        }),
        request_step("cancel after", platform_ime_composition_sample{
            .timestamp_ms = 830,
            .phase = platform_ime_sample_phase::cancel,
        }),
        request_step("touch down after", platform_touch_sample{
            .timestamp_ms = 840,
            .contact_id = 21,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("touch cancel after", platform_touch_sample{
            .timestamp_ms = 850,
            .contact_id = 21,
            .phase = platform_pointer_sample_phase::cancel,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("wheel after", platform_wheel_sample{
            .timestamp_ms = 860,
            .x = 2.0f,
            .y = 3.0f,
            .delta_x = 0.0f,
            .delta_y = -1.0f,
            .unit = platform_scroll_delta_unit::lines,
        }),
    };

    const platform_input_replay_summary before = replay_platform_input_batch(
        std::span{before_steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});
    const platform_input_replay_summary after = replay_platform_input_batch(
        std::span{after_steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});
    const platform_input_replay_diff diff = diff_platform_input_replay_summaries(before, after);

    require(diff.changed, "platform hardening diff detects changed replay");
    require(diff.event_counts_changed, "platform hardening diff flags event count category");
    require(diff.translation_counts_changed,
        "platform hardening diff flags translation count category");
    require(diff.normalized_changed, "platform hardening diff flags normalized replay category");
    require(diff.text_edit_changed, "platform hardening diff flags text edit category");
    require(diff.gesture_capture_focus_changed,
        "platform hardening diff flags gesture capture focus category");
    require(diff.final_text_presentation_changed,
        "platform hardening diff flags final presentation category");
    require(diff.events.touch.delta == 2, "platform hardening diff records touch delta");
    require(diff.events.wheel.delta == 1, "platform hardening diff records wheel delta");
    require(diff.events.ime.delta == 2, "platform hardening diff records ime delta");
    require(diff.events.key.delta == 1, "platform hardening diff records key delta");
    require(diff.normalized.pointer.timeline_changed,
        "platform hardening diff flags pointer timeline change");
    require(diff.normalized.pointer.kinds.pointer_capture_arbitration.delta == 1,
        "platform hardening diff records pointer arbitration delta");
    require(diff.normalized.pointer.kinds.pointer_capture_reset.delta == 1,
        "platform hardening diff records pointer reset delta");
    require(diff.normalized.pointer.kinds.wheel.delta == 1,
        "platform hardening diff records wheel timeline delta");
    require(diff.normalized.ime.timeline_changed,
        "platform hardening diff flags ime timeline change");
    require(diff.normalized.ime.phases.preedit_update.delta == 1,
        "platform hardening diff records ime preedit delta");
    require(diff.normalized.ime.phases.cancel.delta == 1,
        "platform hardening diff records ime cancel delta");
    require(diff.text_edits.counts.operations.backspace.delta == 1,
        "platform hardening diff records backspace text edit delta");
    require(diff.text_edits.counts.utf8_boundary_safe_count.delta > 0,
        "platform hardening diff records utf8 boundary-safe count delta");
    require(!after.final_text_presentation.has_preedit,
        "platform hardening diff fixture ends with clean preedit");
    require(after.final_text_presentation.committed_text == "A",
        "platform hardening diff fixture preserves expected committed text");
}

void test_touch_multipointer_arbitration_replay_preserves_route_evidence()
{
    using namespace quiz_vulkan::input;

    const std::array steps{
        request_step("first touch down", platform_touch_sample{
            .timestamp_ms = 900,
            .contact_id = 10,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("second touch down", platform_touch_sample{
            .timestamp_ms = 910,
            .contact_id = 20,
            .phase = platform_pointer_sample_phase::down,
            .x = 50.0f,
            .y = 0.0f,
        }),
        request_step("first touch cancel", platform_touch_sample{
            .timestamp_ms = 920,
            .contact_id = 10,
            .phase = platform_pointer_sample_phase::cancel,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("second touch up", platform_touch_sample{
            .timestamp_ms = 940,
            .contact_id = 20,
            .phase = platform_pointer_sample_phase::up,
            .x = 51.0f,
            .y = 1.0f,
        }),
    };

    const platform_input_replay_summary replay = replay_platform_input_batch(
        std::span{steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});

    require(replay.events.touch == steps.size(), "multipointer replay counts all touch samples");
    require(replay.pointer.total == 5, "multipointer replay records five pointer route entries");
    require(replay.pointer.kinds.pointer_capture_arbitration == 2,
        "multipointer replay records two tracking arbitration entries");
    require(replay.pointer.kinds.gesture_suppressed == 1,
        "multipointer replay records canceled pointer suppression");
    require(replay.pointer.kinds.pointer_capture_reset == 1,
        "multipointer replay records capture reset for canceled pointer");
    require(replay.pointer.kinds.tap == 1, "multipointer replay records remaining touch tap");
    require(replay.pointer.saw_multipointer_touch, "multipointer replay records multipointer touch evidence");
    require(contains_pointer_id(replay.pointer.touch_pointer_ids, 10),
        "multipointer replay records first touch id");
    require(contains_pointer_id(replay.pointer.touch_pointer_ids, 20),
        "multipointer replay records second touch id");
    require(replay.pointer.final_capture_clean, "multipointer replay ends with clean capture");
    require(replay.route_summary.pointer_capture_ended_cleanly,
        "multipointer route summary ends with clean capture");

    const normalized_input_replay_pointer_timeline_entry& second_down =
        replay.steps[1].normalized_batch.pointer.timeline.front();
    require(second_down.kind == normalized_input_replay_pointer_timeline_kind::pointer_capture_arbitration,
        "multipointer second down records arbitration timeline entry");
    require(second_down.pointer_id == 20, "multipointer second down records second pointer id");
    require(second_down.decision == pointer_arbitration_decision::tracked,
        "multipointer second down records tracked decision");
    require(second_down.tracked_pointer_count_before == 1,
        "multipointer second down records one tracked pointer before");
    require(second_down.tracked_pointer_count_after == 2,
        "multipointer second down records two tracked pointers after");
    require_capture_snapshot(
        second_down.capture_after,
        pointer_capture_lifecycle::tracking,
        false,
        10,
        2,
        "multipointer second down preserves deterministic lowest tracked pointer id");

    const platform_input_replay_step_diagnostics& cancel = replay.steps[2];
    require(cancel.normalized_batch.pointer.timeline.size() == 2,
        "multipointer cancel records suppression plus reset entries");
    const normalized_input_replay_pointer_timeline_entry& cancel_suppression =
        cancel.normalized_batch.pointer.timeline[0];
    require(cancel_suppression.kind == normalized_input_replay_pointer_timeline_kind::gesture_suppressed,
        "multipointer cancel first entry records suppression");
    require(cancel_suppression.pointer_id == 10, "multipointer cancel records canceled pointer id");
    require(cancel_suppression.decision == pointer_arbitration_decision::canceled,
        "multipointer cancel records canceled decision");
    require(cancel_suppression.tracked_pointer_count_before == 2,
        "multipointer cancel records two tracked before");
    require(cancel_suppression.tracked_pointer_count_after == 1,
        "multipointer cancel records one tracked after");

    const normalized_input_replay_pointer_timeline_entry& cancel_reset =
        cancel.normalized_batch.pointer.timeline[1];
    require(cancel_reset.kind == normalized_input_replay_pointer_timeline_kind::pointer_capture_reset,
        "multipointer cancel second entry records capture reset");
    require_capture_snapshot(
        cancel_reset.capture_after,
        pointer_capture_lifecycle::tracking,
        false,
        20,
        1,
        "multipointer cancel reset keeps remaining pointer tracked");

    const normalized_input_replay_pointer_timeline_entry& remaining_tap =
        replay.steps[3].normalized_batch.pointer.timeline.front();
    require(remaining_tap.kind == normalized_input_replay_pointer_timeline_kind::tap,
        "multipointer remaining touch emits tap");
    require(remaining_tap.pointer_id == 20, "multipointer remaining tap preserves pointer id");
    require(remaining_tap.capture_ended_cleanly_after,
        "multipointer remaining tap clears final capture state");
}

void test_platform_replay_records_long_press_swipe_and_drag_threshold_evidence()
{
    using namespace quiz_vulkan::input;

    const std::array steps{
        request_step("long press down", platform_touch_sample{
            .timestamp_ms = 1000,
            .contact_id = 31,
            .phase = platform_pointer_sample_phase::down,
            .x = 4.0f,
            .y = 4.0f,
        }),
        time_step("long press threshold", 1600),
        request_step("long press release", platform_touch_sample{
            .timestamp_ms = 1610,
            .contact_id = 31,
            .phase = platform_pointer_sample_phase::up,
            .x = 4.0f,
            .y = 4.0f,
        }),
        request_step("swipe down", platform_touch_sample{
            .timestamp_ms = 2000,
            .contact_id = 32,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("swipe up", platform_touch_sample{
            .timestamp_ms = 2070,
            .contact_id = 32,
            .phase = platform_pointer_sample_phase::up,
            .x = 70.0f,
            .y = 0.0f,
        }),
        request_step("drag down", platform_touch_sample{
            .timestamp_ms = 3000,
            .contact_id = 33,
            .phase = platform_pointer_sample_phase::down,
            .x = 10.0f,
            .y = 10.0f,
        }),
        request_step("drag move", platform_touch_sample{
            .timestamp_ms = 3010,
            .contact_id = 33,
            .phase = platform_pointer_sample_phase::move,
            .x = 19.0f,
            .y = 10.0f,
        }),
        request_step("drag release", platform_touch_sample{
            .timestamp_ms = 3020,
            .contact_id = 33,
            .phase = platform_pointer_sample_phase::up,
            .x = 21.0f,
            .y = 10.0f,
        }),
    };

    const platform_input_replay_summary replay = replay_platform_input_batch(
        std::span{steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});

    require(replay.events.touch == 7, "threshold replay counts touch samples");
    require(replay.events.time_update == 1, "threshold replay counts long-press time update");
    require(replay.route_summary.normalized_events.long_press == 1,
        "threshold replay counts long press event");
    require(replay.route_summary.normalized_events.swipe_right == 1,
        "threshold replay counts swipe event");
    require(replay.route_summary.normalized_events.drag_start == 1,
        "threshold replay counts drag start event");
    require(replay.route_summary.normalized_events.drag_end == 1,
        "threshold replay counts drag end event");
    require(replay.pointer.kinds.long_press == 1, "threshold replay records long press timeline");
    require(replay.pointer.kinds.swipe_right == 1, "threshold replay records swipe timeline");
    require(replay.pointer.kinds.drag_start == 1, "threshold replay records drag start timeline");
    require(replay.pointer.kinds.drag_end == 1, "threshold replay records drag end timeline");
    require(replay.pointer.kinds.gesture_suppressed == 1,
        "threshold replay records long-press release suppression");
    require(replay.pointer.final_capture_clean, "threshold replay ends with clean capture");

    const normalized_input_replay_pointer_timeline_entry& long_press =
        replay.steps[1].normalized_batch.pointer.timeline.front();
    require(long_press.kind == normalized_input_replay_pointer_timeline_kind::long_press,
        "threshold replay long press step records long press");
    require(long_press.pointer_id == 31, "threshold replay long press preserves touch id");
    require(long_press.duration_ms == 600, "threshold replay records long press duration");
    require(long_press.gesture_policy.long_press_min_duration_ms == 600,
        "threshold replay records long press threshold");
    require(long_press.gesture_policy.tap_slop == 8.0f,
        "threshold replay records tap slop threshold");

    const normalized_input_replay_pointer_timeline_entry& long_release =
        replay.steps[2].normalized_batch.pointer.timeline.front();
    require(long_release.kind == normalized_input_replay_pointer_timeline_kind::gesture_suppressed,
        "threshold replay long press release is suppressed");
    require(long_release.gesture_policy.decision == gesture_policy_decision::release_suppressed,
        "threshold replay long press release records suppression decision");

    const normalized_input_replay_pointer_timeline_entry& swipe =
        replay.steps[4].normalized_batch.pointer.timeline.front();
    require(swipe.kind == normalized_input_replay_pointer_timeline_kind::swipe_right,
        "threshold replay swipe step records swipe right");
    require(swipe.duration_ms == 70, "threshold replay records swipe duration");
    require(swipe.gesture_policy.delta_x == 70.0f,
        "threshold replay records swipe policy delta x");
    require(swipe.gesture_policy.swipe_min_dx == 60.0f,
        "threshold replay records swipe dx threshold");
    require(swipe.gesture_policy.swipe_max_dy == 40.0f,
        "threshold replay records swipe dy threshold");
    require(swipe.gesture_policy.swipe_max_duration_ms == 800,
        "threshold replay records swipe duration threshold");

    const normalized_input_replay_pointer_timeline_entry& drag_start =
        replay.steps[6].normalized_batch.pointer.timeline.front();
    require(drag_start.kind == normalized_input_replay_pointer_timeline_kind::drag_start,
        "threshold replay drag move records drag start");
    require(drag_start.gesture_policy.drag_start_slop == 8.0f,
        "threshold replay records drag threshold");
    require(drag_start.gesture_policy.direction == gesture_direction::right,
        "threshold replay records drag direction");

    const normalized_input_replay_pointer_timeline_entry& drag_end =
        replay.steps[7].normalized_batch.pointer.timeline.front();
    require(drag_end.kind == normalized_input_replay_pointer_timeline_kind::drag_end,
        "threshold replay drag release records drag end");
    require(drag_end.capture_ended_cleanly_after,
        "threshold replay drag release clears capture");
}

void test_cancel_restart_replay_does_not_leak_capture_or_stale_gesture_state()
{
    using namespace quiz_vulkan::input;

    const std::array steps{
        request_step("drag down", platform_touch_sample{
            .timestamp_ms = 4000,
            .contact_id = 55,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("drag start", platform_touch_sample{
            .timestamp_ms = 4010,
            .contact_id = 55,
            .phase = platform_pointer_sample_phase::move,
            .x = 20.0f,
            .y = 0.0f,
        }),
        request_step("reuse pointer down", platform_touch_sample{
            .timestamp_ms = 4020,
            .contact_id = 55,
            .phase = platform_pointer_sample_phase::down,
            .x = 2.0f,
            .y = 2.0f,
        }),
        request_step("reuse pointer cancel", platform_touch_sample{
            .timestamp_ms = 4030,
            .contact_id = 55,
            .phase = platform_pointer_sample_phase::cancel,
            .x = 2.0f,
            .y = 2.0f,
        }),
        request_step("fresh touch down", platform_touch_sample{
            .timestamp_ms = 4040,
            .contact_id = 56,
            .phase = platform_pointer_sample_phase::down,
            .x = 1.0f,
            .y = 1.0f,
        }),
        request_step("fresh touch up", platform_touch_sample{
            .timestamp_ms = 4060,
            .contact_id = 56,
            .phase = platform_pointer_sample_phase::up,
            .x = 1.0f,
            .y = 1.0f,
        }),
    };

    const platform_input_replay_summary replay = replay_platform_input_batch(
        std::span{steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});

    require(replay.pointer.kinds.drag_start == 1, "restart replay records initial drag start");
    require(replay.pointer.kinds.drag_cancel == 1, "restart replay records restart drag cancel");
    require(replay.pointer.kinds.pointer_capture_reset == 1,
        "restart replay records cancel capture reset");
    require(replay.pointer.kinds.tap == 1, "restart replay records fresh touch tap");
    require(replay.pointer.decisions.restarted == 1, "restart replay records restarted decision");
    require(replay.pointer.decisions.canceled == 2,
        "restart replay records canceled decision for suppression and reset");
    require(replay.pointer.final_capture_clean, "restart replay ends with clean capture");
    require(replay.route_summary.pointer_capture_ended_cleanly,
        "restart route summary ends with clean capture");

    const normalized_input_replay_pointer_timeline_entry& restart =
        replay.steps[2].normalized_batch.pointer.timeline.front();
    require(restart.kind == normalized_input_replay_pointer_timeline_kind::drag_cancel,
        "restart replay reused down emits drag cancel");
    require(restart.pointer_id == 55, "restart replay reused down preserves pointer id");
    require(restart.decision == pointer_arbitration_decision::restarted,
        "restart replay reused down records restarted decision");
    require_capture_snapshot(
        restart.capture_before,
        pointer_capture_lifecycle::captured,
        true,
        55,
        1,
        "restart replay reused down records captured pointer before");
    require_capture_snapshot(
        restart.capture_after,
        pointer_capture_lifecycle::tracking,
        false,
        55,
        1,
        "restart replay reused down records replacement tracking state");

    const platform_input_replay_step_diagnostics& cancel = replay.steps[3];
    require(cancel.normalized_batch.pointer.timeline.size() == 2,
        "restart replay cancel records suppression plus reset");
    require(cancel.normalized_batch.pointer.timeline[1].capture_ended_cleanly_after,
        "restart replay cancel reset clears capture");

    const normalized_input_replay_pointer_timeline_entry& fresh_tap =
        replay.steps[5].normalized_batch.pointer.timeline.front();
    require(fresh_tap.kind == normalized_input_replay_pointer_timeline_kind::tap,
        "restart replay fresh touch emits tap");
    require(fresh_tap.pointer_id == 56, "restart replay fresh tap uses new pointer id");
    require(fresh_tap.capture_ended_cleanly_after,
        "restart replay fresh tap has no stale capture");
}

void test_platform_replay_diff_flags_touch_gesture_threshold_and_capture_changes()
{
    using namespace quiz_vulkan::input;

    const std::array threshold_steps{
        request_step("swipe threshold down", platform_touch_sample{
            .timestamp_ms = 5000,
            .contact_id = 70,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("swipe threshold up", platform_touch_sample{
            .timestamp_ms = 5060,
            .contact_id = 70,
            .phase = platform_pointer_sample_phase::up,
            .x = 70.0f,
            .y = 0.0f,
        }),
    };

    input_engine default_engine;
    input_engine tighter_engine{gesture_thresholds{.swipe_min_dx = 80.0f}};
    platform_input_translator translator;
    const platform_input_replay_summary accepted = replay_platform_input_batch(
        default_engine,
        translator,
        std::span{threshold_steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});
    const platform_input_replay_summary suppressed = replay_platform_input_batch(
        tighter_engine,
        translator,
        std::span{threshold_steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});
    const platform_input_replay_diff regression = diff_platform_input_replay_summaries(accepted, suppressed);
    const platform_input_replay_diff recovery = diff_platform_input_replay_summaries(suppressed, accepted);

    require(!regression.event_counts_changed,
        "gesture threshold diff keeps source event counts stable");
    require(!regression.translation_counts_changed,
        "gesture threshold diff keeps translation counts stable");
    require(regression.normalized_changed,
        "gesture threshold diff flags normalized replay changes");
    require(regression.gesture_capture_focus_changed,
        "gesture threshold diff flags gesture capture category");
    require(regression.normalized.gesture_policies.changed,
        "gesture threshold diff flags gesture policy changes");
    require(regression.normalized.gesture_policies.threshold_change_count >= 2,
        "gesture threshold diff records threshold changes for compared routes");
    require(regression.normalized.gesture_policies.swipe_threshold_tightening_count >= 2,
        "gesture threshold diff records swipe threshold tightening");
    require(regression.normalized.gesture_policies.decision_change_count == 1,
        "gesture threshold diff records accepted versus suppressed decision");
    require(regression.normalized.gesture_policies.accepted_to_suppressed_regression_count == 1,
        "gesture threshold diff records accepted-to-suppressed regression");
    require(regression.normalized.pointer.kinds.swipe_right.delta == -1,
        "gesture threshold diff records lost swipe event");
    require(regression.normalized.pointer.kinds.gesture_suppressed.delta == 1,
        "gesture threshold diff records added suppression");
    require(regression.normalized.regression.gesture_policy_changed,
        "gesture threshold diff regression summary flags gesture policy");
    require(regression.normalized.regression.pointer_timeline_changed,
        "gesture threshold diff regression summary flags pointer timeline");

    require(recovery.normalized.gesture_policies.suppressed_to_accepted_recovery_count == 1,
        "gesture threshold recovery diff records suppressed-to-accepted recovery");
    require(recovery.normalized.gesture_policies.swipe_threshold_loosening_count >= 2,
        "gesture threshold recovery diff records swipe threshold loosening");
    require(recovery.normalized.pointer.kinds.swipe_right.delta == 1,
        "gesture threshold recovery diff records restored swipe event");

    const std::array before_steps{
        request_step("single touch down", platform_touch_sample{
            .timestamp_ms = 5100,
            .contact_id = 80,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("single touch up", platform_touch_sample{
            .timestamp_ms = 5120,
            .contact_id = 80,
            .phase = platform_pointer_sample_phase::up,
            .x = 1.0f,
            .y = 1.0f,
        }),
    };
    const std::array after_steps{
        request_step("multi first down", platform_touch_sample{
            .timestamp_ms = 5100,
            .contact_id = 80,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("multi second down", platform_touch_sample{
            .timestamp_ms = 5110,
            .contact_id = 81,
            .phase = platform_pointer_sample_phase::down,
            .x = 40.0f,
            .y = 0.0f,
        }),
        request_step("multi first cancel", platform_touch_sample{
            .timestamp_ms = 5120,
            .contact_id = 80,
            .phase = platform_pointer_sample_phase::cancel,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request_step("multi second up", platform_touch_sample{
            .timestamp_ms = 5140,
            .contact_id = 81,
            .phase = platform_pointer_sample_phase::up,
            .x = 41.0f,
            .y = 1.0f,
        }),
    };

    const platform_input_replay_summary single = replay_platform_input_batch(
        std::span{before_steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});
    const platform_input_replay_summary multi = replay_platform_input_batch(
        std::span{after_steps},
        platform_input_replay_options{.initial_focus_target_id = "answer"});
    const platform_input_replay_diff touch_diff = diff_platform_input_replay_summaries(single, multi);

    require(touch_diff.event_counts_changed, "touch diff flags changed source event counts");
    require(touch_diff.events.touch.delta == 2, "touch diff records added touch samples");
    require(touch_diff.normalized.pointer.saw_multipointer_touch.changed,
        "touch diff flags multipointer touch state change");
    require(touch_diff.normalized.pointer.kinds.pointer_capture_arbitration.delta == 1,
        "touch diff records additional arbitration route");
    require(touch_diff.normalized.pointer.kinds.pointer_capture_reset.delta == 1,
        "touch diff records added capture reset route");
    require(touch_diff.normalized.pointer.kinds.gesture_suppressed.delta == 1,
        "touch diff records added cancel suppression route");
    require(touch_diff.normalized.pointer.touch_pointer_id_count.delta == 1,
        "touch diff records additional touch pointer id");
    require(touch_diff.normalized.regression.pointer_timeline_changed,
        "touch diff regression summary flags pointer timeline");
    require(!multi.pointer.final_capture.active && multi.pointer.final_capture_clean,
        "touch diff fixture after replay ends with clean capture");
}

} // namespace

int main()
{
    test_mixed_platform_replay_summarizes_sources_routes_and_text_transactions();
    test_platform_replay_rejections_are_side_effect_free_and_diffable();
    test_ime_composition_lifecycle_restart_and_cancel_are_summarized();
    test_utf8_text_and_backspace_replay_stay_boundary_safe();
    test_pointer_cancel_restart_and_wheel_preserve_capture_and_focus_evidence();
    test_platform_replay_diff_flags_ime_utf8_pointer_and_wheel_changes();
    test_touch_multipointer_arbitration_replay_preserves_route_evidence();
    test_platform_replay_records_long_press_swipe_and_drag_threshold_evidence();
    test_cancel_restart_replay_does_not_leak_capture_or_stale_gesture_state();
    test_platform_replay_diff_flags_touch_gesture_threshold_and_capture_changes();
    return 0;
}
