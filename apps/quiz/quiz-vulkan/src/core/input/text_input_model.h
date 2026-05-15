#pragma once

#include "core/input/text_edit_transaction_diagnostics.h"

#include <optional>
#include <string>
#include <string_view>

namespace quiz_vulkan::input {

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
