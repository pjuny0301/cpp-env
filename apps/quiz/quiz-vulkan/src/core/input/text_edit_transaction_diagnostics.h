#pragma once

#include "core/input/text_input_types.h"

#include <cstdint>
#include <string>

namespace quiz_vulkan::input {

enum class text_edit_transaction_operation {
    none,
    focus,
    clear_focus,
    move_caret_to_start,
    move_caret_to_end,
    move_caret_left,
    move_caret_right,
    extend_selection_left,
    extend_selection_right,
    select_all,
    clear_selection,
    set_selection,
    commit_utf8,
    backspace,
    delete_forward,
    set_preedit,
    commit_ime,
    cancel_ime,
    submit,
};

enum class text_edit_transaction_rejection_reason {
    none,
    not_focused,
    empty_text,
    no_selection,
    no_active_ime,
    no_text_before_caret,
    no_text_after_caret,
    empty_ime_commit,
    unchanged,
};

struct text_edit_transaction_snapshot {
    bool has_focus = false;
    std::string target_id;
    std::string text;
    std::string display_text;
    std::size_t caret_byte_offset = 0;
    text_range caret_range;
    bool has_selection = false;
    text_range selection;
    bool has_preedit = false;
    std::string preedit_text;
    text_range preedit_range;
    std::size_t preedit_anchor_byte_offset = 0;
    ime_composition_state composition;
};

struct text_edit_transaction_byte_diagnostics {
    std::size_t committed_text_bytes_before = 0;
    std::size_t committed_text_bytes_after = 0;
    std::size_t display_text_bytes_before = 0;
    std::size_t display_text_bytes_after = 0;
    std::int64_t committed_byte_delta = 0;
    std::int64_t display_byte_delta = 0;
    std::size_t inserted_byte_count = 0;
    std::size_t deleted_byte_count = 0;
    std::size_t replaced_byte_count = 0;
};

struct text_edit_transaction_diagnostics {
    text_edit_transaction_operation operation = text_edit_transaction_operation::none;
    bool accepted = false;
    bool rejected = false;
    text_edit_transaction_rejection_reason rejection_reason =
        text_edit_transaction_rejection_reason::none;
    text_edit_transaction_snapshot before;
    text_edit_transaction_snapshot after;
    text_edit_transaction_byte_diagnostics bytes;
    bool text_changed = false;
    bool display_text_changed = false;
    bool caret_changed = false;
    bool selection_changed = false;
    bool preedit_changed = false;
    bool before_utf8_boundary_safe = true;
    bool after_utf8_boundary_safe = true;
    bool utf8_boundary_safe = true;
    bool selection_was_active = false;
    bool selection_replaced_committed_text = false;
    bool selection_replaced_display_text = false;
    bool selection_deleted = false;
    bool selection_cleared = false;
    bool ime_preedit_started = false;
    bool ime_preedit_updated = false;
    bool ime_preedit_cleared = false;
    bool ime_committed = false;
    bool ime_canceled = false;
    bool invalid_edit_rejected = false;
    bool changed = false;
};

} // namespace quiz_vulkan::input
