#include "core/input/input_action_candidate_plan.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
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

bool has_summary_fragment(
    const quiz_vulkan::input::input_action_resolution_replay_classification_summary& summary,
    quiz_vulkan::input::input_action_resolution_replay_summary_fragment fragment)
{
    for (const quiz_vulkan::input::input_action_resolution_replay_summary_fragment candidate :
        summary.reason_fragments) {
        if (candidate == fragment) {
            return true;
        }
    }
    return false;
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
        step("wheel", platform_scroll(
            260,
            3.0f,
            4.0f,
            0.0f,
            -2.0f,
            raw_platform_scroll_delta_unit::lines,
            false,
            true,
            false,
            true)),
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
    require(wheel_candidate->emits_input_event,
        "wheel scroll candidate records normalized input-event evidence only");
    require(wheel_candidate->line_delta_y == -2.0f,
        "wheel scroll candidate carries normalized line delta");
    require(wheel_candidate->pixel_delta_y == 0.0f,
        "wheel scroll candidate preserves pixel delta evidence");
    require_modifiers(wheel_candidate->modifiers,
        false,
        true,
        false,
        true,
        "wheel scroll candidate carries modifier evidence");
    require_modifiers(wheel_candidate->normalized_event.modifiers,
        false,
        true,
        false,
        true,
        "wheel scroll candidate keeps normalized modifier evidence");

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
        step("wheel", platform_scroll(
            260,
            3.0f,
            4.0f,
            0.0f,
            -2.0f,
            raw_platform_scroll_delta_unit::lines,
            true,
            false,
            true)),
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
    require_modifiers(wheel_result->candidate.modifiers,
        true,
        false,
        true,
        false,
        "wheel result preserves modifier evidence");

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

void test_resolution_replay_summary_groups_handoff_results_by_batch()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    const std::array steps{
        step("text-initial", text(100, std::string{"A"})),
        step("focus-gained", focus(raw_platform_focus_phase::gained, 110)),
        step("ime-preedit", ime(raw_platform_ime_phase::preedit_update, 120, utf8(u8"ㅎ"))),
        step("ime-commit", ime(raw_platform_ime_phase::commit, 130, utf8(u8"한"))),
        step("pointer-down", pointer(raw_platform_pointer_phase::down, 200, 0.0f, 0.0f, raw_platform_pointer_button::primary, 7)),
        step("pointer-drag", pointer(raw_platform_pointer_phase::move, 220, 12.0f, 0.0f, raw_platform_pointer_button::primary, 7)),
        step("wheel", platform_scroll(
            260,
            3.0f,
            4.0f,
            0.0f,
            -2.0f,
            raw_platform_scroll_delta_unit::lines,
            false,
            true,
            true)),
    };

    const normalized_input_replay_recording recording = replay_normalized_input_fixture(
        engine,
        steps,
        normalized_input_replay_options{.initial_focus_target_id = "answer"});
    const input_action_candidate_resolution resolution =
        resolve_input_action_candidates(recording);
    const input_action_resolution_replay_summary summary =
        summarize_input_action_candidate_resolution(resolution);

    require(summary.batches.size() == recording.batches.size(),
        "resolution replay summary keeps one summary batch per replay batch");
    require(summary.counts.total == resolution.counts.total,
        "resolution replay summary preserves total result count");
    require(summary.selected.size() == summary.counts.selected.total,
        "resolution replay summary selected vector matches selected counts");
    require(summary.supporting_evidence.size() == summary.counts.supporting_evidence.total,
        "resolution replay summary support vector matches support counts");
    require(summary.rejected.empty(),
        "valid resolution replay summary has no rejected results");
    require(summary.final_state.text == recording.final_state.text,
        "resolution replay summary preserves final text state");

    const input_action_resolution_batch_summary& text_batch = summary.batches[0];
    require(text_batch.label == "text-initial",
        "resolution replay summary preserves source batch labels");
    require(text_batch.selected.size() == 1,
        "text batch summary has one selected result");
    require(text_batch.selected[0].kind == input_action_candidate_kind::text_edit,
        "text batch summary selects text edit");
    require(text_batch.selected[0].target_id == "answer",
        "text batch summary preserves target id for router handoff");
    require(text_batch.selected[0].text_byte_delta == 1,
        "text batch summary exposes text byte delta");
    require(text_batch.selected[0].has_text_delta,
        "text batch summary carries text delta flag");

    const input_action_resolution_result_summary* ime_commit_summary = nullptr;
    for (const input_action_resolution_result_summary& result : summary.selected) {
        if (result.kind == input_action_candidate_kind::ime_commit) {
            ime_commit_summary = &result;
            break;
        }
    }
    require(ime_commit_summary != nullptr,
        "resolution replay summary exposes ime commit selected summary");
    require(ime_commit_summary->reason == input_action_candidate_result_reason::ime_committed,
        "ime commit summary preserves selected reason");
    require(ime_commit_summary->committed_text_bytes == utf8(u8"한").size(),
        "ime commit summary exposes committed text byte count");
    require(ime_commit_summary->stale_preedit_cleared_after,
        "ime commit summary exposes preedit cleanup flag");

    const input_action_resolution_batch_summary& pointer_batch = summary.batches[5];
    require(pointer_batch.label == "pointer-drag",
        "pointer drag summary preserves batch label");
    require(pointer_batch.has_selected_candidate,
        "pointer drag summary records selected candidate");
    require(pointer_batch.selected_candidate_index != input_action_candidate_npos,
        "pointer drag summary records selected candidate index");
    require(pointer_batch.selected.size() == 1,
        "pointer drag summary has one selected result");
    require(pointer_batch.supporting_evidence.size() == 1,
        "pointer drag summary has one support result");
    require(pointer_batch.selected[0].kind == input_action_candidate_kind::gesture_candidate,
        "pointer drag summary selects gesture candidate");
    require(pointer_batch.selected[0].gesture_decision == gesture_policy_decision::drag_started,
        "pointer drag summary exposes gesture decision");
    require(pointer_batch.selected[0].normalized_event_kind == input_event_summary_kind::drag_start,
        "pointer drag summary exposes normalized event kind");
    require(pointer_batch.supporting_evidence[0].kind == input_action_candidate_kind::pointer_capture,
        "pointer drag summary keeps capture support");
    require(pointer_batch.supporting_evidence[0].capture_before_lifecycle == pointer_capture_lifecycle::tracking,
        "pointer drag support summary exposes capture before lifecycle");
    require(pointer_batch.supporting_evidence[0].capture_after_lifecycle == pointer_capture_lifecycle::captured,
        "pointer drag support summary exposes capture after lifecycle");

    const input_action_resolution_batch_summary& wheel_batch = summary.batches[6];
    require(wheel_batch.selected.size() == 1,
        "wheel batch summary has one selected result");
    require(wheel_batch.selected[0].kind == input_action_candidate_kind::wheel_scroll,
        "wheel batch summary selects wheel scroll");
    require(wheel_batch.selected[0].line_delta_y == -2.0f,
        "wheel batch summary exposes line delta");
    require(wheel_batch.selected[0].has_wheel_delta,
        "wheel batch summary carries wheel delta flag");
    require_modifiers(wheel_batch.selected[0].modifiers,
        false,
        true,
        true,
        false,
        "wheel batch summary exposes modifier evidence");
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

void test_resolution_batch_summary_groups_rejected_and_diagnostic_results()
{
    using namespace quiz_vulkan::input;

    input_action_candidate_batch_plan batch{
        .label = "manual-rejected",
        .candidates = {
            input_action_candidate{
                .kind = input_action_candidate_kind::ime_preedit,
                .utf8_text = "draft",
                .preedit_text_valid = false,
            },
            input_action_candidate{
                .kind = input_action_candidate_kind::gesture_candidate,
                .gesture_policy = gesture_policy_snapshot{
                    .decision = gesture_policy_decision::ignored_by_capture,
                },
            },
            input_action_candidate{
                .kind = input_action_candidate_kind::text_edit,
            },
        },
    };

    const input_action_candidate_batch_resolution resolution =
        resolve_input_action_candidate_batch(batch);
    const input_action_resolution_batch_summary summary =
        summarize_input_action_candidate_batch_resolution(resolution);

    require(!summary.has_selected_candidate,
        "rejected batch summary has no selected candidate");
    require(summary.selected.empty(),
        "rejected batch summary has no selected results");
    require(summary.rejected.size() == 2,
        "rejected batch summary groups rejected results");
    require(summary.diagnostic_only.size() == 1,
        "rejected batch summary groups diagnostic-only results");
    require(summary.counts.rejected.ime_preedit == 1,
        "rejected batch summary counts invalid preedit");
    require(summary.counts.rejected.gesture_candidate == 1,
        "rejected batch summary counts suppressed gesture");
    require(summary.counts.diagnostic_only.text_edit == 1,
        "rejected batch summary counts unchanged text diagnostic");
    require(summary.rejected[0].reason == input_action_candidate_result_reason::ime_preedit_invalid,
        "rejected batch summary preserves invalid preedit reason");
    require(!summary.rejected[0].evidence_clean,
        "rejected batch summary exposes invalid evidence flag");
    require(summary.rejected[1].reason == input_action_candidate_result_reason::gesture_policy_suppressed,
        "rejected batch summary preserves suppressed gesture reason");
    require(summary.diagnostic_only[0].reason == input_action_candidate_result_reason::no_observable_delta,
        "diagnostic-only batch summary preserves no-delta reason");
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

    const input_action_resolution_batch_summary summary =
        summarize_input_action_candidate_batch_resolution(resolution);
    require(summary.selected.size() == 1,
        "manual batch summary has one selected result");
    require(summary.supporting_evidence.size() == 1,
        "manual batch summary has one support result");
    require(summary.selected[0].candidate_index == 1,
        "manual batch summary preserves selected candidate index");
    require(summary.supporting_evidence[0].candidate_index == 0,
        "manual batch summary preserves supporting candidate index");
}

void test_resolution_replay_summary_diff_reports_counts_reasons_and_targets()
{
    using namespace quiz_vulkan::input;

    const input_action_resolution_replay_summary before{
        .batches = {
            input_action_resolution_batch_summary{.label = "before"},
        },
        .selected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::text_edit,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::text_edit_diff,
                .batch_label = "before",
                .has_text_delta = true,
                .target_id = "answer",
            },
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::focus_move,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::focus_transition,
                .batch_label = "before",
                .has_focus_transition = true,
                .target_id = "answer",
                .target_id_after = "answer",
                .has_focus_after = true,
            },
        },
        .counts = input_action_candidate_result_counts{
            .selected = input_action_candidate_counts{
                .text_edit = 1,
                .focus_move = 1,
                .total = 2,
            },
            .total = 2,
        },
    };
    const input_action_resolution_replay_summary after{
        .batches = {
            input_action_resolution_batch_summary{.label = "before"},
            input_action_resolution_batch_summary{.label = "after"},
        },
        .selected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::text_edit,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::text_edit_diff,
                .batch_label = "after",
                .has_text_delta = true,
                .target_id = "answer-alt",
            },
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::focus_move,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::focus_transition,
                .batch_label = "after",
                .has_focus_transition = true,
                .target_id = "next",
                .target_id_before = "answer",
                .target_id_after = "next",
                .target_changed = true,
                .has_focus_after = true,
            },
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::wheel_scroll,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::wheel_delta,
                .batch_label = "after",
                .has_wheel_delta = true,
                .line_delta_y = -2.0f,
            },
        },
        .supporting_evidence = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::pointer_capture,
                .status = input_action_candidate_result_status::supporting_evidence,
                .reason = input_action_candidate_result_reason::pointer_capture_supports_selected,
                .batch_label = "after",
                .supports_selected_candidate = true,
                .has_pointer_capture_transition = true,
            },
        },
        .rejected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::gesture_candidate,
                .status = input_action_candidate_result_status::rejected,
                .reason = input_action_candidate_result_reason::gesture_policy_rejected,
                .batch_label = "after",
                .gesture_decision = gesture_policy_decision::swipe_rejected_distance,
            },
        },
        .counts = input_action_candidate_result_counts{
            .selected = input_action_candidate_counts{
                .text_edit = 1,
                .focus_move = 1,
                .wheel_scroll = 1,
                .total = 3,
            },
            .supporting_evidence = input_action_candidate_counts{
                .pointer_capture = 1,
                .total = 1,
            },
            .rejected = input_action_candidate_counts{
                .gesture_candidate = 1,
                .total = 1,
            },
            .total = 5,
        },
    };

    const input_action_resolution_replay_summary_diff diff =
        diff_input_action_resolution_replay_summaries(before, after);

    require(diff.changed, "resolution replay diff reports changed summaries");
    require(diff.changed_category_count == 6,
        "resolution replay diff counts changed high-level categories");
    require(diff.batch_count.delta == 1,
        "resolution replay diff reports batch count growth");
    require(diff.result_counts.selected.total.delta == 1,
        "resolution replay diff reports selected total delta");
    require(diff.result_counts.supporting_evidence.pointer_capture.delta == 1,
        "resolution replay diff reports pointer capture support delta");
    require(diff.result_counts.rejected.gesture_candidate.delta == 1,
        "resolution replay diff reports rejected gesture delta");
    require(diff.action_kinds.wheel_scroll.delta == 1,
        "resolution replay diff reports aggregate wheel action delta");
    require(diff.action_kinds.pointer_capture.delta == 1,
        "resolution replay diff reports aggregate pointer capture action delta");
    require(diff.action_kinds.gesture_candidate.delta == 1,
        "resolution replay diff reports aggregate gesture action delta");
    require(diff.reasons.wheel_delta.delta == 1,
        "resolution replay diff reports stable wheel reason delta");
    require(diff.reasons.pointer_capture_supports_selected.delta == 1,
        "resolution replay diff reports stable pointer support reason delta");
    require(diff.reasons.gesture_policy_rejected.delta == 1,
        "resolution replay diff reports stable gesture rejection reason delta");
    require(diff.reasons.text_edit_diff.delta == 0,
        "resolution replay diff keeps stable unchanged text reason delta");
    require(diff.focus_target.changed,
        "resolution replay diff reports focus target change");
    require(diff.focus_target.target_id.before_value == "answer",
        "resolution replay diff records before focus target");
    require(diff.focus_target.target_id.after_value == "next",
        "resolution replay diff records after focus target");
    require(diff.text_target.changed,
        "resolution replay diff reports text target change");
    require(diff.text_target.target_id.before_value == "answer",
        "resolution replay diff records before text target");
    require(diff.text_target.target_id.after_value == "answer-alt",
        "resolution replay diff records after text target");

    const input_action_resolution_target_snapshot focus_snapshot =
        input_action_resolution_focus_target_snapshot(after);
    require(focus_snapshot.present,
        "resolution replay focus target snapshot is present when focus evidence exists");
    require(focus_snapshot.target_id == "next",
        "resolution replay focus target snapshot uses latest focus target");

    const input_action_resolution_target_snapshot text_snapshot =
        input_action_resolution_text_target_snapshot(after);
    require(text_snapshot.present,
        "resolution replay text target snapshot is present when text evidence exists");
    require(text_snapshot.target_id == "answer-alt",
        "resolution replay text target snapshot uses latest text target");
}

void test_resolution_replay_summary_diff_classifies_regressions_and_improvements()
{
    using namespace quiz_vulkan::input;

    const input_action_resolution_replay_summary before{
        .selected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::text_edit,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::text_edit_diff,
                .batch_label = "before",
                .has_text_delta = true,
                .target_id = "answer",
            },
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::focus_move,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::focus_transition,
                .batch_label = "before",
                .has_focus_transition = true,
                .target_id = "answer",
                .target_id_after = "answer",
                .has_focus_after = true,
            },
        },
        .supporting_evidence = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::pointer_capture,
                .status = input_action_candidate_result_status::supporting_evidence,
                .reason = input_action_candidate_result_reason::pointer_capture_supports_selected,
                .batch_label = "before",
                .supports_selected_candidate = true,
                .has_pointer_capture_transition = true,
            },
        },
        .counts = input_action_candidate_result_counts{
            .selected = input_action_candidate_counts{
                .text_edit = 1,
                .focus_move = 1,
                .total = 2,
            },
            .supporting_evidence = input_action_candidate_counts{
                .pointer_capture = 1,
                .total = 1,
            },
            .total = 3,
        },
    };
    const input_action_resolution_replay_summary after{
        .selected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::focus_move,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::focus_transition,
                .batch_label = "after",
                .has_focus_transition = true,
                .target_id = "next",
                .target_id_before = "answer",
                .target_id_after = "next",
                .target_changed = true,
                .has_focus_after = true,
            },
        },
        .rejected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::gesture_candidate,
                .status = input_action_candidate_result_status::rejected,
                .reason = input_action_candidate_result_reason::gesture_policy_rejected,
                .batch_label = "after",
                .gesture_decision = gesture_policy_decision::swipe_rejected_distance,
            },
        },
        .diagnostic_only = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::text_edit,
                .status = input_action_candidate_result_status::diagnostic_only,
                .reason = input_action_candidate_result_reason::no_observable_delta,
                .batch_label = "after",
                .target_id = "answer-alt",
            },
        },
        .counts = input_action_candidate_result_counts{
            .selected = input_action_candidate_counts{
                .focus_move = 1,
                .total = 1,
            },
            .diagnostic_only = input_action_candidate_counts{
                .text_edit = 1,
                .total = 1,
            },
            .rejected = input_action_candidate_counts{
                .gesture_candidate = 1,
                .total = 1,
            },
            .total = 3,
        },
    };

    const input_action_resolution_replay_summary_diff regression_diff =
        diff_input_action_resolution_replay_summaries(before, after);
    const input_action_resolution_replay_classification& regression =
        regression_diff.classification;

    require(regression.change_class == input_action_resolution_replay_change_class::regression,
        "resolution replay classification reports regression-only diff");
    require(regression.focus_target_churn,
        "resolution replay classification reports focus target churn");
    require(regression.text_target_churn,
        "resolution replay classification reports text target churn");
    require(regression.selected_action_lost,
        "resolution replay classification reports selected action loss");
    require(!regression.selected_action_gained,
        "resolution replay classification does not report selected action gain");
    require(regression.supporting_evidence_lost,
        "resolution replay classification reports support evidence loss");
    require(!regression.supporting_evidence_gained,
        "resolution replay classification does not report support evidence gain");
    require(regression.rejected_count_changed,
        "resolution replay classification reports rejected count change");
    require(regression.rejected_count_gained,
        "resolution replay classification reports rejected count growth");
    require(!regression.rejected_count_lost,
        "resolution replay classification does not report rejected count loss");
    require(regression.no_observable_delta_changed,
        "resolution replay classification reports no-observable-delta transition");
    require(regression.no_observable_delta_gained,
        "resolution replay classification reports no-observable-delta growth");
    require(!regression.no_observable_delta_lost,
        "resolution replay classification does not report no-observable-delta loss");
    require(regression.has_churn,
        "resolution replay classification records churn presence");
    require(regression.has_regression,
        "resolution replay classification records regression presence");
    require(!regression.has_improvement,
        "resolution replay classification does not record improvement presence");
    require(regression.churn_count == 2,
        "resolution replay classification counts focus and text churn categories");
    require(regression.regression_count == 4,
        "resolution replay classification counts regression categories");
    require(regression.improvement_count == 0,
        "resolution replay classification has zero improvement categories");
    require(regression.changed_category_count == 6,
        "resolution replay classification counts changed categories");

    const input_action_resolution_replay_classification_summary& regression_summary =
        regression_diff.classification_summary;
    require(regression_summary.category == input_action_resolution_replay_summary_category::regression,
        "resolution replay classification summary reports regression category");
    require(regression_summary.change_class == input_action_resolution_replay_change_class::regression,
        "resolution replay classification summary preserves change class");
    require(regression_summary.changed_category_count == 6,
        "resolution replay classification summary preserves changed category count");
    require(regression_summary.churn_count == 2,
        "resolution replay classification summary preserves churn count");
    require(regression_summary.regression_count == 4,
        "resolution replay classification summary preserves regression count");
    require(regression_summary.improvement_count == 0,
        "resolution replay classification summary preserves improvement count");
    require(regression_summary.reason_fragment_count == 6,
        "resolution replay classification summary reports stable reason fragment count");
    require(regression_summary.reason_fragments.size() == regression_summary.reason_fragment_count,
        "resolution replay classification summary fragment count matches vector size");
    require(regression_summary.reason_fragments[0]
            == input_action_resolution_replay_summary_fragment::focus_target_churn,
        "resolution replay classification summary orders focus churn first");
    require(regression_summary.reason_fragments[1]
            == input_action_resolution_replay_summary_fragment::text_target_churn,
        "resolution replay classification summary orders text churn second");
    require(regression_summary.reason_fragments[2]
            == input_action_resolution_replay_summary_fragment::selected_action_lost,
        "resolution replay classification summary orders selected loss after churn");
    require(has_summary_fragment(
            regression_summary,
            input_action_resolution_replay_summary_fragment::supporting_evidence_lost),
        "resolution replay classification summary includes support evidence loss fragment");
    require(has_summary_fragment(
            regression_summary,
            input_action_resolution_replay_summary_fragment::rejected_count_gained),
        "resolution replay classification summary includes rejected count growth fragment");
    require(has_summary_fragment(
            regression_summary,
            input_action_resolution_replay_summary_fragment::no_observable_delta_gained),
        "resolution replay classification summary includes no-observable-delta growth fragment");
    require(input_action_resolution_replay_summary_category_name(regression_summary.category)
            == std::string_view{"regression"},
        "resolution replay classification summary category string is stable");
    require(input_action_resolution_replay_change_class_name(regression_summary.change_class)
            == std::string_view{"regression"},
        "resolution replay classification change class string is stable");
    require(input_action_resolution_replay_summary_fragment_name(
            input_action_resolution_replay_summary_fragment::selected_action_lost)
            == std::string_view{"selected_action_lost"},
        "resolution replay classification selected-loss fragment string is stable");
    require(input_action_resolution_replay_summary_fragment_name(
            input_action_resolution_replay_summary_fragment::rejected_count_gained)
            == std::string_view{"rejected_count_gained"},
        "resolution replay classification rejected-growth fragment string is stable");

    const input_action_resolution_replay_summary_diff improvement_diff =
        diff_input_action_resolution_replay_summaries(after, before);
    const input_action_resolution_replay_classification& improvement =
        improvement_diff.classification;

    require(improvement.change_class == input_action_resolution_replay_change_class::improvement,
        "resolution replay classification reports improvement-only inverse diff");
    require(improvement.focus_target_churn,
        "inverse resolution replay classification keeps focus target churn");
    require(improvement.text_target_churn,
        "inverse resolution replay classification keeps text target churn");
    require(improvement.selected_action_gained,
        "inverse resolution replay classification reports selected action gain");
    require(!improvement.selected_action_lost,
        "inverse resolution replay classification does not report selected action loss");
    require(improvement.supporting_evidence_gained,
        "inverse resolution replay classification reports support evidence gain");
    require(!improvement.supporting_evidence_lost,
        "inverse resolution replay classification does not report support evidence loss");
    require(improvement.rejected_count_lost,
        "inverse resolution replay classification reports rejected count loss");
    require(!improvement.rejected_count_gained,
        "inverse resolution replay classification does not report rejected count gain");
    require(improvement.no_observable_delta_lost,
        "inverse resolution replay classification reports no-observable-delta loss");
    require(!improvement.no_observable_delta_gained,
        "inverse resolution replay classification does not report no-observable-delta gain");
    require(!improvement.has_regression,
        "inverse resolution replay classification has no regression category");
    require(improvement.has_improvement,
        "inverse resolution replay classification has improvement categories");
    require(improvement.churn_count == 2,
        "inverse resolution replay classification counts churn categories");
    require(improvement.regression_count == 0,
        "inverse resolution replay classification has zero regression categories");
    require(improvement.improvement_count == 4,
        "inverse resolution replay classification counts improvement categories");
    require(improvement.changed_category_count == 6,
        "inverse resolution replay classification counts changed categories");

    const input_action_resolution_replay_classification_summary& improvement_summary =
        improvement_diff.classification_summary;
    require(improvement_summary.category
            == input_action_resolution_replay_summary_category::improvement,
        "inverse resolution replay classification summary reports improvement category");
    require(improvement_summary.reason_fragment_count == 6,
        "inverse resolution replay classification summary reports stable reason fragments");
    require(has_summary_fragment(
            improvement_summary,
            input_action_resolution_replay_summary_fragment::selected_action_gained),
        "inverse resolution replay classification summary includes selected gain fragment");
    require(has_summary_fragment(
            improvement_summary,
            input_action_resolution_replay_summary_fragment::supporting_evidence_gained),
        "inverse resolution replay classification summary includes support gain fragment");
    require(has_summary_fragment(
            improvement_summary,
            input_action_resolution_replay_summary_fragment::rejected_count_lost),
        "inverse resolution replay classification summary includes rejected count loss fragment");
    require(has_summary_fragment(
            improvement_summary,
            input_action_resolution_replay_summary_fragment::no_observable_delta_lost),
        "inverse resolution replay classification summary includes no-observable-delta loss fragment");
    require(input_action_resolution_replay_summary_category_name(improvement_summary.category)
            == std::string_view{"improvement"},
        "inverse resolution replay classification summary category string is stable");
}

void test_resolution_replay_summary_diff_classifies_selected_action_kind_churn()
{
    using namespace quiz_vulkan::input;

    const input_action_resolution_replay_summary selected_text{
        .selected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::text_edit,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::text_edit_diff,
                .batch_label = "text",
                .has_text_delta = true,
                .target_id = "answer",
            },
        },
        .counts = input_action_candidate_result_counts{
            .selected = input_action_candidate_counts{
                .text_edit = 1,
                .total = 1,
            },
            .total = 1,
        },
    };
    const input_action_resolution_replay_summary selected_wheel{
        .selected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::wheel_scroll,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::wheel_delta,
                .batch_label = "wheel",
                .has_wheel_delta = true,
                .line_delta_y = -1.0f,
            },
        },
        .counts = input_action_candidate_result_counts{
            .selected = input_action_candidate_counts{
                .wheel_scroll = 1,
                .total = 1,
            },
            .total = 1,
        },
    };

    const input_action_resolution_replay_summary_diff diff =
        diff_input_action_resolution_replay_summaries(selected_text, selected_wheel);
    const input_action_resolution_replay_classification& classification = diff.classification;

    require(diff.result_counts.selected.total.delta == 0,
        "selected kind churn keeps selected total stable");
    require(classification.selected_action_lost,
        "selected kind churn reports selected action loss by kind");
    require(classification.selected_action_gained,
        "selected kind churn reports selected action gain by kind");
    require(classification.change_class == input_action_resolution_replay_change_class::mixed,
        "selected kind churn reports mixed classification");
    require(classification.regression_count == 1,
        "selected kind churn counts selected action loss as one regression category");
    require(classification.improvement_count == 1,
        "selected kind churn counts selected action gain as one improvement category");

    const input_action_resolution_replay_classification_summary& summary =
        diff.classification_summary;
    require(summary.category == input_action_resolution_replay_summary_category::mixed,
        "selected kind churn summary reports mixed category");
    require(summary.reason_fragment_count == 3,
        "selected kind churn summary records churn, lost, and gained fragments");
    require(has_summary_fragment(
            summary,
            input_action_resolution_replay_summary_fragment::text_target_churn),
        "selected kind churn summary records text target churn fragment");
    require(has_summary_fragment(
            summary,
            input_action_resolution_replay_summary_fragment::selected_action_lost),
        "selected kind churn summary records selected loss fragment");
    require(has_summary_fragment(
            summary,
            input_action_resolution_replay_summary_fragment::selected_action_gained),
        "selected kind churn summary records selected gain fragment");
    require(input_action_resolution_replay_summary_category_name(summary.category)
            == std::string_view{"mixed"},
        "selected kind churn summary category string is stable");
}

void test_resolution_replay_classification_summary_reports_churn_category()
{
    using namespace quiz_vulkan::input;

    const input_action_resolution_replay_summary before{
        .selected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::focus_move,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::focus_transition,
                .batch_label = "before",
                .has_focus_transition = true,
                .target_id = "answer",
                .target_id_after = "answer",
                .has_focus_after = true,
            },
        },
        .counts = input_action_candidate_result_counts{
            .selected = input_action_candidate_counts{
                .focus_move = 1,
                .total = 1,
            },
            .total = 1,
        },
    };
    const input_action_resolution_replay_summary after{
        .selected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::focus_move,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::focus_transition,
                .batch_label = "after",
                .has_focus_transition = true,
                .target_id = "next",
                .target_id_before = "answer",
                .target_id_after = "next",
                .target_changed = true,
                .has_focus_after = true,
            },
        },
        .counts = input_action_candidate_result_counts{
            .selected = input_action_candidate_counts{
                .focus_move = 1,
                .total = 1,
            },
            .total = 1,
        },
    };

    const input_action_resolution_replay_summary_diff diff =
        diff_input_action_resolution_replay_summaries(before, after);
    const input_action_resolution_replay_classification_summary summary =
        summarize_input_action_resolution_replay_diff_classification(diff);

    require(diff.classification.change_class
            == input_action_resolution_replay_change_class::behavior_change,
        "focus-only target churn remains behavior-change classification");
    require(summary.category == input_action_resolution_replay_summary_category::churn,
        "focus-only target churn summary reports churn category");
    require(summary.changed,
        "focus-only target churn summary reports changed");
    require(summary.changed_category_count == 1,
        "focus-only target churn summary records one changed category");
    require(summary.churn_count == 1,
        "focus-only target churn summary records one churn fragment");
    require(summary.regression_count == 0,
        "focus-only target churn summary has zero regression count");
    require(summary.improvement_count == 0,
        "focus-only target churn summary has zero improvement count");
    require(summary.reason_fragment_count == 1,
        "focus-only target churn summary records one stable reason fragment");
    require(summary.reason_fragments[0]
            == input_action_resolution_replay_summary_fragment::focus_target_churn,
        "focus-only target churn summary records focus churn fragment");
    require(input_action_resolution_replay_summary_category_name(summary.category)
            == std::string_view{"churn"},
        "focus-only target churn summary category string is stable");
    require(input_action_resolution_replay_change_class_name(diff.classification.change_class)
            == std::string_view{"behavior_change"},
        "focus-only target churn change class string is stable");
}

void test_resolution_replay_summary_diff_is_stable_for_identical_snapshots()
{
    using namespace quiz_vulkan::input;

    const input_action_resolution_replay_summary summary{
        .batches = {
            input_action_resolution_batch_summary{.label = "stable"},
        },
        .selected = {
            input_action_resolution_result_summary{
                .kind = input_action_candidate_kind::ime_commit,
                .status = input_action_candidate_result_status::selected,
                .reason = input_action_candidate_result_reason::ime_committed,
                .batch_label = "stable",
                .has_ime_payload = true,
                .target_id = "answer",
            },
        },
        .counts = input_action_candidate_result_counts{
            .selected = input_action_candidate_counts{
                .ime_commit = 1,
                .total = 1,
            },
            .total = 1,
        },
    };

    const input_action_resolution_replay_summary_diff diff =
        diff_input_action_resolution_replay_summaries(summary, summary);

    require(!diff.changed,
        "stable resolution replay diff reports no change");
    require(diff.changed_category_count == 0,
        "stable resolution replay diff has zero changed categories");
    require(!diff.result_counts.changed,
        "stable resolution replay diff has stable status counts");
    require(!diff.action_kinds.changed,
        "stable resolution replay diff has stable action-kind counts");
    require(!diff.reasons.changed,
        "stable resolution replay diff has stable reason counts");
    require(!diff.text_target.changed,
        "stable resolution replay diff has stable text target");
    require(!diff.focus_target.changed,
        "stable resolution replay diff has stable absent focus target");
    require(diff.classification.change_class == input_action_resolution_replay_change_class::stable,
        "stable resolution replay classification is stable");
    require(!diff.classification.changed,
        "stable resolution replay classification reports unchanged");
    require(diff.classification.changed_category_count == 0,
        "stable resolution replay classification has zero changed categories");
    require(diff.classification_summary.category
            == input_action_resolution_replay_summary_category::stable,
        "stable resolution replay classification summary reports stable category");
    require(!diff.classification_summary.changed,
        "stable resolution replay classification summary reports unchanged");
    require(diff.classification_summary.reason_fragment_count == 0,
        "stable resolution replay classification summary has no reason fragments");
    require(diff.classification_summary.reason_fragments.empty(),
        "stable resolution replay classification summary fragment vector is empty");
    require(input_action_resolution_replay_summary_category_name(diff.classification_summary.category)
            == std::string_view{"stable"},
        "stable resolution replay classification summary category string is stable");
    require(input_action_resolution_replay_change_class_name(diff.classification.change_class)
            == std::string_view{"stable"},
        "stable resolution replay classification change class string is stable");
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
    test_resolution_replay_summary_groups_handoff_results_by_batch();
    test_candidate_resolution_rejects_invalid_and_suppressed_evidence();
    test_resolution_batch_summary_groups_rejected_and_diagnostic_results();
    test_candidate_resolution_uses_deterministic_priority_tiebreak();
    test_resolution_replay_summary_diff_reports_counts_reasons_and_targets();
    test_resolution_replay_summary_diff_classifies_regressions_and_improvements();
    test_resolution_replay_summary_diff_classifies_selected_action_kind_churn();
    test_resolution_replay_classification_summary_reports_churn_category();
    test_resolution_replay_summary_diff_is_stable_for_identical_snapshots();
    test_empty_replay_yields_empty_candidate_plan();

    std::cout << "input_action_candidate_plan_tests passed\n";
    return 0;
}
