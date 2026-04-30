#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace quiz_vulkan::input {

struct text_range {
    std::size_t start_byte = 0;
    std::size_t end_byte = 0;
};

struct ime_composition_state {
    bool active = false;
    std::string preedit_text;
    text_range replacement_range;
    text_range preedit_range;
    text_range caret_range;
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

    [[nodiscard]] std::size_t preedit_anchor_byte_offset() const;
    [[nodiscard]] text_range ime_replacement_range() const;
    void set_selection_from_anchor(std::size_t anchor_byte, std::size_t active_byte);
    void clear_preedit();
    void reset_selection();
    void replace_selection_with(std::string_view utf8_text);
    bool erase_selected_text();
};

} // namespace quiz_vulkan::input
