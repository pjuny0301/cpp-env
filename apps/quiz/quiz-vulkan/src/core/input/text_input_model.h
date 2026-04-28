#pragma once

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
