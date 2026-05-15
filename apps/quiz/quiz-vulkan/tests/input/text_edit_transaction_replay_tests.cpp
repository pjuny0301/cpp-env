#include "core/input/text_edit_transaction_replay.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

namespace {

using quiz_vulkan::input::diff_text_edit_transaction_replay_summaries;
using quiz_vulkan::input::replay_text_edit_transactions;
using quiz_vulkan::input::text_edit_transaction_operation;
using quiz_vulkan::input::text_edit_transaction_replay_step;
using quiz_vulkan::input::text_range;

std::string utf8(const char8_t* text)
{
    return reinterpret_cast<const char*>(text);
}

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "text_edit_transaction_replay_tests failed: " << message << '\n';
    std::exit(1);
}

void require_range(text_range range, std::size_t start_byte, std::size_t end_byte, const char* message)
{
    require(range.start_byte == start_byte, message);
    require(range.end_byte == end_byte, message);
}

void test_replay_records_step_and_aggregate_text_diagnostics()
{
    const std::string selected_codepoint = utf8(u8"한");
    const std::string initial = std::string("A") + selected_codepoint + "B";
    const std::string preedit = utf8(u8"ㅎ");
    const std::string updated_preedit = utf8(u8"하");
    const std::string committed = utf8(u8"각");
    const std::string committed_after_ime = std::string("A") + committed + "B";

    const std::array steps{
        text_edit_transaction_replay_step{
            .label = "focus answer",
            .operation = text_edit_transaction_operation::focus,
            .text = "answer",
        },
        text_edit_transaction_replay_step{
            .label = "commit initial",
            .operation = text_edit_transaction_operation::commit_utf8,
            .text = initial,
        },
        text_edit_transaction_replay_step{
            .label = "select middle codepoint",
            .operation = text_edit_transaction_operation::set_selection,
            .range = text_range{.start_byte = 1, .end_byte = 1 + selected_codepoint.size()},
        },
        text_edit_transaction_replay_step{
            .label = "start preedit",
            .operation = text_edit_transaction_operation::set_preedit,
            .text = preedit,
        },
        text_edit_transaction_replay_step{
            .label = "update preedit",
            .operation = text_edit_transaction_operation::set_preedit,
            .text = updated_preedit,
        },
        text_edit_transaction_replay_step{
            .label = "commit ime",
            .operation = text_edit_transaction_operation::commit_ime,
            .text = committed,
        },
        text_edit_transaction_replay_step{
            .label = "reject empty commit",
            .operation = text_edit_transaction_operation::commit_utf8,
        },
        text_edit_transaction_replay_step{
            .label = "submit",
            .operation = text_edit_transaction_operation::submit,
        },
    };

    const auto summary = replay_text_edit_transactions(std::span{steps});

    require(summary.steps.size() == steps.size(), "replay stores one diagnostic per step");
    require(summary.counts.step_count == steps.size(), "aggregate step count is stable");
    require(summary.counts.accepted_count == 7, "aggregate accepted count is stable");
    require(summary.counts.rejected_count == 1, "aggregate rejected count is stable");
    require(summary.counts.invalid_edit_rejected_count == 1, "aggregate invalid edit count is stable");
    require(summary.counts.operations.focus == 1, "focus operation is counted");
    require(summary.counts.operations.commit_utf8 == 2, "commit utf8 operations are counted");
    require(summary.counts.operations.set_selection == 1, "set selection operation is counted");
    require(summary.counts.operations.set_preedit == 2, "preedit operations are counted");
    require(summary.counts.operations.commit_ime == 1, "ime commit operation is counted");
    require(summary.counts.operations.submit == 1, "submit operation is counted");
    require(summary.counts.selection_replaced_display_text_count == 2,
        "display selection replacements are counted across preedit start and update");
    require(summary.counts.selection_replaced_committed_text_count == 1,
        "committed selection replacement is counted");
    require(summary.counts.selection_cleared_count == 1, "selection clear is counted");
    require(summary.counts.ime_preedit_started_count == 1, "ime preedit start is counted");
    require(summary.counts.ime_preedit_updated_count == 1, "ime preedit update is counted");
    require(summary.counts.ime_preedit_cleared_count == 1, "ime preedit clear is counted");
    require(summary.counts.ime_committed_count == 1, "ime commit transition is counted");
    require(summary.counts.ime_canceled_count == 0, "ime cancel transition is not counted");
    require(summary.counts.utf8_boundary_unsafe_count == 0, "all replay steps stay on utf8 boundaries");
    require(summary.counts.inserted_byte_count == initial.size() + committed.size(),
        "aggregate inserted bytes include initial and ime commits");
    require(summary.counts.deleted_byte_count == selected_codepoint.size() + committed_after_ime.size(),
        "aggregate deleted bytes include replaced selection and submit clear");
    require(summary.counts.replaced_byte_count == selected_codepoint.size(),
        "aggregate replaced bytes are stable");
    require(summary.counts.committed_byte_delta == 0, "aggregate committed byte delta returns to empty");

    const auto& initial_commit = summary.steps[1];
    require(initial_commit.label == "commit initial", "step label is preserved");
    require(initial_commit.committed_text_bytes_before == 0, "initial commit before bytes are recorded");
    require(initial_commit.committed_text_bytes_after == initial.size(),
        "initial commit after bytes are recorded");
    require(initial_commit.transaction.bytes.inserted_byte_count == initial.size(),
        "initial commit inserted bytes are recorded");
    require(initial_commit.accepted, "initial commit is accepted");
    require(initial_commit.utf8_boundary_safe, "initial commit is boundary safe");

    const auto& preedit_start = summary.steps[3];
    require(preedit_start.had_selection_before, "preedit step records prior selection");
    require(preedit_start.has_preedit_after, "preedit step records active preedit");
    require(preedit_start.selection_replaced_display_text,
        "preedit step records display selection replacement");
    require(preedit_start.committed_text_bytes_before == initial.size(),
        "preedit step keeps committed bytes before");
    require(preedit_start.committed_text_bytes_after == initial.size(),
        "preedit step keeps committed bytes after");
    require(preedit_start.presentation_diff.display_text_changed,
        "preedit step exposes display presentation change");

    const auto& ime_commit = summary.steps[5];
    require(ime_commit.selection_replaced_committed_text,
        "ime commit records committed selection replacement");
    require(ime_commit.ime_committed, "ime commit records commit transition");
    require(ime_commit.ime_preedit_cleared, "ime commit records preedit clearing");
    require(ime_commit.transaction.bytes.inserted_byte_count == committed.size(),
        "ime commit inserted bytes are recorded");
    require(ime_commit.transaction.bytes.deleted_byte_count == selected_codepoint.size(),
        "ime commit deleted bytes are recorded");
    require(ime_commit.transaction.bytes.replaced_byte_count == selected_codepoint.size(),
        "ime commit replaced bytes are recorded");

    const auto& rejected_commit = summary.steps[6];
    require(rejected_commit.rejected, "invalid empty commit is rejected");
    require(rejected_commit.invalid_edit_rejected, "invalid empty commit records invalid edit evidence");
    require(rejected_commit.committed_text_bytes_before == rejected_commit.committed_text_bytes_after,
        "invalid empty commit has no committed byte side effect");

    require(summary.final_submit_available, "final summary exposes submit availability");
    require(summary.final_utf8_boundary_safe, "final summary exposes utf8 boundary state");
    require(summary.final_presentation.has_focus, "final presentation keeps text focus");
    require(summary.final_presentation.target_id == "answer", "final presentation keeps target id");
    require(summary.final_presentation.committed_text.empty(), "submit clears final committed text");
    require(summary.final_presentation.display_text.empty(), "submit clears final display text");
    require(!summary.final_presentation.has_preedit, "final presentation has no preedit");
    require(summary.final_presentation.has_submit_text, "final presentation exposes submit text");
    require_range(summary.final_presentation.caret_range, 0, 0, "final caret returns to start");
}

void test_replay_diff_reports_semantic_free_deltas()
{
    const std::array before_steps{
        text_edit_transaction_replay_step{
            .label = "focus answer",
            .operation = text_edit_transaction_operation::focus,
            .text = "answer",
        },
        text_edit_transaction_replay_step{
            .label = "commit one byte",
            .operation = text_edit_transaction_operation::commit_utf8,
            .text = "A",
        },
        text_edit_transaction_replay_step{
            .label = "reject empty commit",
            .operation = text_edit_transaction_operation::commit_utf8,
        },
    };
    const std::array after_steps{
        text_edit_transaction_replay_step{
            .label = "focus answer",
            .operation = text_edit_transaction_operation::focus,
            .text = "answer",
        },
        text_edit_transaction_replay_step{
            .label = "commit two bytes",
            .operation = text_edit_transaction_operation::commit_utf8,
            .text = "AB",
        },
        text_edit_transaction_replay_step{
            .label = "start preedit",
            .operation = text_edit_transaction_operation::set_preedit,
            .text = "x",
        },
    };

    const auto before = replay_text_edit_transactions(std::span{before_steps});
    const auto after = replay_text_edit_transactions(std::span{after_steps});
    const auto diff = diff_text_edit_transaction_replay_summaries(before, after);

    require(diff.changed, "different replay summaries produce a changed diff");
    require(diff.counts.changed, "aggregate count deltas are marked changed");
    require(diff.final_presentation_changed, "final presentation delta is marked changed");
    require(diff.invalid_edit_changed, "invalid edit count delta is marked changed");
    require(diff.ime_transition_changed, "ime transition count delta is marked changed");
    require(!diff.replacement_changed, "unrelated replacement deltas stay false");
    require(diff.counts.invalid_edit_rejected_count.before_count == 1,
        "diff records before invalid edit count");
    require(diff.counts.invalid_edit_rejected_count.after_count == 0,
        "diff records after invalid edit count");
    require(diff.counts.invalid_edit_rejected_count.delta == -1,
        "diff records invalid edit count delta");
    require(diff.counts.ime_preedit_started_count.delta == 1,
        "diff records ime preedit start delta");
    require(diff.counts.inserted_byte_count.before_count == 1, "diff records before inserted bytes");
    require(diff.counts.inserted_byte_count.after_count == 2, "diff records after inserted bytes");
    require(diff.counts.inserted_byte_count.delta == 1, "diff records inserted byte delta");
    require(diff.final_presentation.committed_text.before_value == "A",
        "diff records final committed text before value");
    require(diff.final_presentation.committed_text.after_value == "AB",
        "diff records final committed text after value");
    require(diff.final_presentation.preedit_text.after_value == "x",
        "diff records final preedit after value");
    require(diff.final_presentation.preedit_changed, "diff records final preedit change");
    require(diff.changed_category_count >= 3, "diff categories summarize changed areas");

    const auto stable = diff_text_edit_transaction_replay_summaries(after, after);
    require(!stable.changed, "identical replay summaries produce unchanged diff");
    require(stable.changed_category_count == 0, "identical replay summaries have no changed categories");
    require(!stable.counts.changed, "identical replay summaries have no count deltas");
}

} // namespace

int main()
{
    test_replay_records_step_and_aggregate_text_diagnostics();
    test_replay_diff_reports_semantic_free_deltas();
    return 0;
}
