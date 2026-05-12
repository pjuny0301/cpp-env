#include "core/input/platform_input_replay.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <utility>

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

} // namespace

int main()
{
    test_mixed_platform_replay_summarizes_sources_routes_and_text_transactions();
    test_platform_replay_rejections_are_side_effect_free_and_diffable();
    return 0;
}
