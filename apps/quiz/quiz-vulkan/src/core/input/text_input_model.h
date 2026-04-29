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

class text_input_model {
public:
    void focus(std::string target_id);
    void clear_focus();

    [[nodiscard]] bool has_focus() const;
    [[nodiscard]] const std::string& focus_id() const;
    [[nodiscard]] const std::string& text() const;
    [[nodiscard]] const std::string& preedit_text() const;
    [[nodiscard]] std::string display_text() const;
    [[nodiscard]] text_range caret_range() const;
    [[nodiscard]] std::optional<text_range> preedit_range() const;

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
    std::optional<std::string> submit_text_;
};

} // namespace quiz_vulkan::input
