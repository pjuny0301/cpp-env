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

const quiz_vulkan::input::input_action_candidate_result* find_first_result(
    const quiz_vulkan::input::input_action_candidate_resolution& resolution,
    quiz_vulkan::input::input_action_candidate_kind kind,
    quiz_vulkan::input::input_action_candidate_result_status status)
{
    for (const quiz_vulkan::input::input_action_candidate_result& result : resolution.results) {
        if (result.candidate.kind == kind && result.status == status) {
            return &result;
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

void test_candidate_resolution_selects_primary_results_and_supporting_evidence()
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
    const input_action_candidate_resolution resolution = resolve_input_action_candidate_plan(plan);

    require(resolution.results.size() == plan.candidates.size(),
        "candidate resolution keeps one result per planned candidate");
    require(resolution.final_state.text == recording.final_state.text,
        "candidate resolution preserves final text state");
    require(resolution.counts.total == plan.candidates.size(),
        "candidate resolution counts every candidate result");
    require(resolution.counts.rejected.total == 0,
        "valid replay candidate resolution has no rejected results");
    require(resolution.counts.selected.text_edit == 1,
        "candidate resolution selects text edit result");
    require(resolution.counts.selected.focus_move == 1,
        "candidate resolution selects focus result");
    require(resolution.counts.selected.ime_preedit == 2,
        "candidate resolution selects preedit results");
    require(resolution.counts.selected.ime_commit == 1,
        "candidate resolution selects ime commit result");
    require(resolution.counts.selected.ime_cancel == 1,
        "candidate resolution selects ime cancel result");
    require(resolution.counts.selected.pointer_capture == 1,
        "candidate resolution selects standalone pointer capture result");
    require(resolution.counts.selected.gesture_candidate == 2,
        "candidate resolution selects emitted gesture results");
    require(resolution.counts.selected.wheel_scroll == 1,
        "candidate resolution selects wheel result");
    require(resolution.counts.supporting_evidence.pointer_capture == 2,
        "candidate resolution keeps capture transitions supporting gesture batches");

    const input_action_candidate_result* ime_commit_result = find_first_result(
        resolution,
        input_action_candidate_kind::ime_commit,
        input_action_candidate_result_status::selected);
    require(ime_commit_result != nullptr, "ime commit selected result exists");
    require(ime_commit_result->reason == input_action_candidate_result_reason::ime_committed,
        "ime commit result records semantic-free commit reason");
    require(ime_commit_result->has_ime_payload,
        "ime commit result carries payload evidence");
    require(ime_commit_result->evidence_clean,
        "ime commit result records clean evidence");

    const input_action_candidate_result* wheel_result = find_first_result(
        resolution,
        input_action_candidate_kind::wheel_scroll,
        input_action_candidate_result_status::selected);
    require(wheel_result != nullptr, "wheel selected result exists");
    require(wheel_result->reason == input_action_candidate_result_reason::wheel_delta,
        "wheel result records delta reason");
    require(wheel_result->has_wheel_delta,
        "wheel result carries delta evidence");

    const input_action_candidate_batch_plan pointer_batch =
        plan_input_action_candidates_for_batch(recording.batches[7], 7);
    const input_action_candidate_batch_resolution pointer_resolution =
        resolve_input_action_candidate_batch(pointer_batch);
    require(pointer_resolution.has_selected_candidate,
        "pointer drag batch resolution selects a primary result");
    require(pointer_resolution.selected_candidate_index != input_action_candidate_npos,
        "pointer drag batch resolution records selected index");
    const input_action_candidate_result& selected =
        pointer_resolution.results[pointer_resolution.selected_candidate_index];
    require(selected.candidate.kind == input_action_candidate_kind::gesture_candidate,
        "pointer drag batch selects gesture over capture support");
    require(selected.reason == input_action_candidate_result_reason::gesture_emitted,
        "pointer drag selected result records emitted gesture reason");
    require(pointer_resolution.counts.selected.gesture_candidate == 1,
        "pointer drag batch selected count records gesture result");
    require(pointer_resolution.counts.supporting_evidence.pointer_capture == 1,
        "pointer drag batch support count records capture result");

    const input_action_candidate_result* capture_support = nullptr;
    for (const input_action_candidate_result& result : pointer_resolution.results) {
        if (result.candidate.kind == input_action_candidate_kind::pointer_capture) {
            capture_support = &result;
            break;
        }
    }
    require(capture_support != nullptr, "pointer drag capture support exists");
    require(capture_support->status == input_action_candidate_result_status::supporting_evidence,
        "pointer drag capture is supporting evidence");
    require(capture_support->supports_selected_candidate,
        "pointer drag capture marks selected-candidate support");
    require(capture_support->reason == input_action_candidate_result_reason::pointer_capture_supports_selected,
        "pointer drag capture records support reason");
}

void test_candidate_resolution_rejects_invalid_and_suppressed_evidence()
{
    using namespace quiz_vulkan::input;

    const input_action_candidate_result invalid_preedit = resolve_input_action_candidate(
        input_action_candidate{
            .kind = input_action_candidate_kind::ime_preedit,
            .utf8_text = "draft",
            .preedit_text_valid = false,
        });
    require(invalid_preedit.status == input_action_candidate_result_status::rejected,
        "invalid preedit resolves to rejected result");
    require(invalid_preedit.reason == input_action_candidate_result_reason::ime_preedit_invalid,
        "invalid preedit records invalid reason");
    require(!invalid_preedit.evidence_clean,
        "invalid preedit records unclean evidence");

    const input_action_candidate_result rejected_gesture = resolve_input_action_candidate(
        input_action_candidate{
            .kind = input_action_candidate_kind::gesture_candidate,
            .gesture_policy = gesture_policy_snapshot{
                .decision = gesture_policy_decision::swipe_rejected_distance,
            },
        });
    require(rejected_gesture.status == input_action_candidate_result_status::rejected,
        "rejected gesture policy resolves to rejected result");
    require(rejected_gesture.reason == input_action_candidate_result_reason::gesture_policy_rejected,
        "rejected gesture policy records rejection reason");

    const input_action_candidate_result suppressed_gesture = resolve_input_action_candidate(
        input_action_candidate{
            .kind = input_action_candidate_kind::gesture_candidate,
            .gesture_policy = gesture_policy_snapshot{
                .decision = gesture_policy_decision::ignored_by_capture,
            },
        });
    require(suppressed_gesture.status == input_action_candidate_result_status::rejected,
        "suppressed gesture policy resolves to rejected result");
    require(suppressed_gesture.reason == input_action_candidate_result_reason::gesture_policy_suppressed,
        "suppressed gesture policy records suppression reason");

    const input_action_candidate_result unchanged_text = resolve_input_action_candidate(
        input_action_candidate{.kind = input_action_candidate_kind::text_edit});
    require(unchanged_text.status == input_action_candidate_result_status::diagnostic_only,
        "unchanged text candidate remains diagnostic-only");
    require(unchanged_text.reason == input_action_candidate_result_reason::no_observable_delta,
        "unchanged text candidate records no-delta reason");
}

void test_candidate_resolution_uses_deterministic_priority_tiebreak()
{
    using namespace quiz_vulkan::input;

    input_action_candidate_batch_plan batch{
        .label = "manual-mixed",
        .candidates = {
            input_action_candidate{
                .kind = input_action_candidate_kind::wheel_scroll,
                .timestamp_ms = 10,
                .line_delta_y = -1.0f,
            },
            input_action_candidate{
                .kind = input_action_candidate_kind::text_edit,
                .timestamp_ms = 20,
                .text_presentation_diff = text_input_presentation_diff{
                    .committed_text_changed = true,
                    .changed = true,
                },
                .text_byte_count_before = 0,
                .text_byte_count_after = 1,
            },
        },
    };

    const input_action_candidate_batch_resolution resolution =
        resolve_input_action_candidate_batch(batch);

    require(resolution.has_selected_candidate,
        "manual batch resolution selects a primary result");
    require(resolution.selected_candidate_index == 1,
        "manual batch resolution chooses higher-priority text candidate deterministically");
    require(resolution.results[1].selected,
        "manual batch marks selected text result");
    require(resolution.results[1].reason == input_action_candidate_result_reason::text_edit_diff,
        "manual selected text result records text diff reason");
    require(resolution.results[0].status == input_action_candidate_result_status::supporting_evidence,
        "manual lower-priority wheel result becomes supporting evidence");
    require(resolution.results[0].reason == input_action_candidate_result_reason::coalesced_by_selected_candidate,
        "manual lower-priority result records coalescing reason");
    require(resolution.counts.selected.text_edit == 1,
        "manual resolution counts selected text result");
    require(resolution.counts.supporting_evidence.wheel_scroll == 1,
        "manual resolution counts supporting wheel result");
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
    test_candidate_resolution_selects_primary_results_and_supporting_evidence();
    test_candidate_resolution_rejects_invalid_and_suppressed_evidence();
    test_candidate_resolution_uses_deterministic_priority_tiebreak();
    test_empty_replay_yields_empty_candidate_plan();

    std::cout << "input_action_candidate_plan_tests passed\n";
    return 0;
}
