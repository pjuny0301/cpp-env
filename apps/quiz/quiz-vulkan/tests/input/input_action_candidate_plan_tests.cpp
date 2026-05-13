#include "core/input/input_action_candidate_plan.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

    std::cerr << "input_action_candidate_plan_tests failed: " << message << '\n';
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

const quiz_vulkan::input::input_action_candidate* find_first_candidate(
    const quiz_vulkan::input::input_action_candidate_plan& plan,
    quiz_vulkan::input::input_action_candidate_kind kind)
{
    for (const quiz_vulkan::input::input_action_candidate& candidate : plan.candidates) {
        if (candidate.kind == kind) {
            return &candidate;
        }
    }
    return nullptr;
}

void test_replay_evidence_maps_to_semantic_free_candidates()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    const std::array steps{
        step("text-initial", text(100, std::string{"A"})),
        step("focus-gained", focus(raw_platform_focus_phase::gained, 110)),
        step("ime-preedit", ime(raw_platform_ime_phase::preedit_update, 120, utf8(u8"ㅎ"))),
        step("ime-commit", ime(raw_platform_ime_phase::commit, 130, utf8(u8"한"))),
        step("ime-second-preedit", ime(raw_platform_ime_phase::preedit_update, 140, "draft")),
        step("ime-cancel", ime(raw_platform_ime_phase::cancel, 150)),
        step("pointer-down", pointer(raw_platform_pointer_phase::down, 200, 0.0f, 0.0f, raw_platform_pointer_button::primary, 7)),
        step("pointer-drag", pointer(raw_platform_pointer_phase::move, 220, 12.0f, 0.0f, raw_platform_pointer_button::primary, 7)),
        step("pointer-up", pointer(raw_platform_pointer_phase::up, 240, 14.0f, 0.0f, raw_platform_pointer_button::primary, 7)),
        step("wheel", platform_scroll(260, 3.0f, 4.0f, 0.0f, -2.0f, raw_platform_scroll_delta_unit::lines)),
    };

    const normalized_input_replay_recording recording = replay_normalized_input_fixture(
        engine,
        steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});
    const input_action_candidate_plan plan = plan_input_action_candidates(recording);

    require(plan.batches.size() == recording.batches.size(),
        "candidate plan keeps one diagnostic batch per replay batch");
    require(plan.final_state.text == recording.final_state.text,
        "candidate plan preserves final input text state");
    require(plan.counts.text_edit == 1,
        "candidate plan emits one text edit candidate from text replay evidence");
    require(plan.counts.focus_move == 1,
        "candidate plan emits focus movement evidence");
    require(plan.counts.ime_preedit == 2,
        "candidate plan emits preedit evidence for both preedit routes");
    require(plan.counts.ime_commit == 1,
        "candidate plan emits ime commit evidence");
    require(plan.counts.ime_cancel == 1,
        "candidate plan emits ime cancel evidence");
    require(plan.counts.pointer_capture == 3,
        "candidate plan emits pointer capture transition evidence");
    require(plan.counts.gesture_candidate == 2,
        "candidate plan emits drag gesture candidate evidence");
    require(plan.counts.wheel_scroll == 1,
        "candidate plan emits wheel scroll evidence");
    require(plan.counts.total == plan.candidates.size(),
        "candidate plan total count matches candidate vector size");

    const input_action_candidate* text_candidate =
        find_first_candidate(plan, input_action_candidate_kind::text_edit);
    require(text_candidate != nullptr, "text edit candidate exists");
    require(text_candidate->batch_label == "text-initial",
        "text edit candidate preserves source batch label");
    require(text_candidate->target_id == "answer",
        "text edit candidate preserves input target id");
    require(text_candidate->text_presentation_diff.committed_text_changed,
        "text edit candidate carries presentation text diff");
    require(text_candidate->text_byte_count_after == 1,
        "text edit candidate carries byte count evidence");

    const input_action_candidate* focus_candidate =
        find_first_candidate(plan, input_action_candidate_kind::focus_move);
    require(focus_candidate != nullptr, "focus move candidate exists");
    require(focus_candidate->focus_kind == normalized_input_replay_focus_timeline_kind::focus_gain,
        "focus move candidate preserves focus timeline kind");
    require(focus_candidate->target_id == "answer",
        "focus move candidate preserves target id");
    require(focus_candidate->emits_input_event,
        "focus move candidate records emitted input-event evidence");

    const input_action_candidate* ime_commit_candidate =
        find_first_candidate(plan, input_action_candidate_kind::ime_commit);
    require(ime_commit_candidate != nullptr, "ime commit candidate exists");
    require(ime_commit_candidate->committed_text == utf8(u8"한"),
        "ime commit candidate preserves committed payload");
    require(ime_commit_candidate->stale_preedit_cleared_after,
        "ime commit candidate records preedit cleanup evidence");

    const input_action_candidate* ime_cancel_candidate =
        find_first_candidate(plan, input_action_candidate_kind::ime_cancel);
    require(ime_cancel_candidate != nullptr, "ime cancel candidate exists");
    require(ime_cancel_candidate->composition.preedit_text == "draft",
        "ime cancel candidate preserves stale preedit snapshot");
    require(ime_cancel_candidate->stale_preedit_cleared_after,
        "ime cancel candidate records stale preedit cleanup");

    const input_action_candidate* pointer_capture_candidate =
        find_first_candidate(plan, input_action_candidate_kind::pointer_capture);
    require(pointer_capture_candidate != nullptr, "pointer capture candidate exists");
    require(pointer_capture_candidate->pointer_id == 7,
        "pointer capture candidate preserves pointer id");
    require(pointer_capture_candidate->capture_changed,
        "pointer capture candidate records capture transition");
    require(pointer_capture_candidate->capture_before.lifecycle == pointer_capture_lifecycle::idle,
        "pointer capture candidate records before lifecycle");

    const input_action_candidate* gesture_candidate =
        find_first_candidate(plan, input_action_candidate_kind::gesture_candidate);
    require(gesture_candidate != nullptr, "gesture candidate exists");
    require(gesture_candidate->pointer_id == 7,
        "gesture candidate preserves pointer id");
    require(gesture_candidate->pointer_kind == normalized_input_replay_pointer_timeline_kind::drag_start,
        "gesture candidate preserves drag-start evidence");
    require(gesture_candidate->gesture_policy.decision == gesture_policy_decision::drag_started,
        "gesture candidate carries policy decision evidence");

    const input_action_candidate* wheel_candidate =
        find_first_candidate(plan, input_action_candidate_kind::wheel_scroll);
    require(wheel_candidate != nullptr, "wheel scroll candidate exists");
    require(wheel_candidate->line_delta_y == -2.0f,
        "wheel scroll candidate carries normalized line delta");
    require(wheel_candidate->pixel_delta_y == 0.0f,
        "wheel scroll candidate preserves pixel delta evidence");

    const input_action_candidate_batch_plan pointer_batch =
        plan_input_action_candidates_for_batch(recording.batches[7], 7);
    require(pointer_batch.counts.pointer_capture == 1,
        "batch candidate plan records drag capture transition");
    require(pointer_batch.counts.gesture_candidate == 1,
        "batch candidate plan records drag gesture evidence");
    require(pointer_batch.counts.total == pointer_batch.candidates.size(),
        "batch candidate count matches candidate vector size");
}

void test_empty_replay_yields_empty_candidate_plan()
{
    using namespace quiz_vulkan::input;

    input_engine engine;
    const normalized_input_replay_recording recording =
        replay_normalized_input_fixture(engine, std::span<const normalized_input_replay_step>{});
    const input_action_candidate_plan plan = plan_input_action_candidates(recording);

    require(plan.batches.empty(), "empty replay has no candidate batches");
    require(plan.candidates.empty(), "empty replay has no candidates");
    require(plan.counts.total == 0, "empty replay has zero candidate count");
    require(plan.final_state.pointer_capture_clean,
        "empty replay preserves clean final pointer capture");
    require_range(plan.final_state.selection, 0, 0,
        "empty replay preserves default selection range");
}

} // namespace

int main()
{
    test_replay_evidence_maps_to_semantic_free_candidates();
    test_empty_replay_yields_empty_candidate_plan();

    std::cout << "input_action_candidate_plan_tests passed\n";
    return 0;
}
