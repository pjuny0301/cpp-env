#pragma once

#include "core/input/text_input_types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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

class text_input_model {
public:
    void focus(std::string target_id);
    void clear_focus();

    [[nodiscard]] bool has_focus() const;
    [[nodiscard]] const std::string& focus_id() const;
    [[nodiscard]] const std::string& text() const;
    [[nodiscard]] const std::string& preedit_text() const;
    [[nodiscard]] std::string display_text() const;
    [[nodiscard]] std::size_t caret_byte_offset() const;
    [[nodiscard]] text_range caret_range() const;
    [[nodiscard]] std::optional<text_range> preedit_range() const;
    [[nodiscard]] std::optional<text_range> selection_range() const;
    [[nodiscard]] ime_composition_state ime_composition() const;
    [[nodiscard]] const text_edit_transaction_diagnostics& last_transaction_diagnostics() const;

    bool move_caret_to_start();
    bool move_caret_to_end();
    bool move_caret_left();
    bool move_caret_right();
    bool extend_selection_left();
    bool extend_selection_right();
    bool select_all();
    bool clear_selection();
    bool set_selection(text_range range);
    bool commit_utf8(std::string_view utf8_text);
    bool backspace();
    bool delete_forward();
    bool set_preedit(std::string_view utf8_text);
    bool commit_ime(std::string_view utf8_text);
    bool cancel_ime();
    bool submit();

    [[nodiscard]] bool has_submit_text() const;
    [[nodiscard]] std::optional<std::string> consume_submit_text();

private:
    bool focused_ = false;
    std::string focus_id_;
    std::string text_;
    std::string preedit_text_;
    bool ime_composition_active_ = false;
    std::size_t caret_byte_offset_ = 0;
    std::optional<text_range> selection_range_;
    std::optional<std::size_t> selection_anchor_byte_offset_;
    std::optional<std::string> submit_text_;
    text_edit_transaction_diagnostics last_transaction_;

    [[nodiscard]] std::size_t preedit_anchor_byte_offset() const;
    [[nodiscard]] text_range ime_replacement_range() const;
    [[nodiscard]] text_edit_transaction_snapshot transaction_snapshot() const;
    void record_transaction(
        text_edit_transaction_operation operation,
        const text_edit_transaction_snapshot& before,
        bool accepted,
        text_edit_transaction_rejection_reason rejection_reason =
            text_edit_transaction_rejection_reason::none);
    void set_selection_from_anchor(std::size_t anchor_byte, std::size_t active_byte);
    void clear_preedit();
    void reset_selection();
    void replace_selection_with(std::string_view utf8_text);
    bool erase_selected_text();
};

} // namespace quiz_vulkan::input
