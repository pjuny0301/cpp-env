#include "core/input/text_input_model.h"

#include <cstddef>
#include <utility>

namespace quiz_vulkan::input {
namespace {

void pop_last_utf8_codepoint(std::string& value)
{
    if (value.empty()) {
        return;
    }

    auto is_continuation_byte = [](unsigned char byte) {
        return (byte & 0xc0U) == 0x80U;
    };
    auto expected_codepoint_length = [](unsigned char lead) -> std::size_t {
        if (lead < 0x80U) {
            return 1;
        }
        if (lead >= 0xc2U && lead <= 0xdfU) {
            return 2;
        }
        if (lead >= 0xe0U && lead <= 0xefU) {
            return 3;
        }
        if (lead >= 0xf0U && lead <= 0xf4U) {
            return 4;
        }
        return 0;
    };

    const std::size_t end = value.size();
    std::size_t start = end - 1;
    while (start > 0 && is_continuation_byte(static_cast<unsigned char>(value[start]))) {
        --start;
    }

    const unsigned char lead = static_cast<unsigned char>(value[start]);
    const std::size_t actual_length = end - start;
    if (expected_codepoint_length(lead) == actual_length) {
        value.erase(start);
        return;
    }

    value.erase(end - 1);
}

} // namespace

void text_input_model::focus(std::string target_id)
{
    focused_ = true;
    focus_id_ = std::move(target_id);
    preedit_text_.clear();
}

void text_input_model::clear_focus()
{
    focused_ = false;
    focus_id_.clear();
    preedit_text_.clear();
}

bool text_input_model::has_focus() const
{
    return focused_;
}

const std::string& text_input_model::focus_id() const
{
    return focus_id_;
}

const std::string& text_input_model::text() const
{
    return text_;
}

const std::string& text_input_model::preedit_text() const
{
    return preedit_text_;
}

std::string text_input_model::display_text() const
{
    std::string display = text_;
    display.append(preedit_text_);
    return display;
}

text_range text_input_model::caret_range() const
{
    const std::size_t caret = text_.size() + preedit_text_.size();
    return text_range{
        .start_byte = caret,
        .end_byte = caret,
    };
}

std::optional<text_range> text_input_model::preedit_range() const
{
    if (preedit_text_.empty()) {
        return std::nullopt;
    }

    return text_range{
        .start_byte = text_.size(),
        .end_byte = text_.size() + preedit_text_.size(),
    };
}

bool text_input_model::commit_utf8(std::string_view utf8_text)
{
    if (!focused_ || utf8_text.empty()) {
        return false;
    }

    text_.append(utf8_text);
    preedit_text_.clear();
    return true;
}

bool text_input_model::backspace()
{
    if (!focused_) {
        return false;
    }

    if (!preedit_text_.empty()) {
        preedit_text_.clear();
        return true;
    }

    if (text_.empty()) {
        return false;
    }

    pop_last_utf8_codepoint(text_);
    return true;
}

bool text_input_model::set_preedit(std::string_view utf8_text)
{
    if (!focused_) {
        return false;
    }

    preedit_text_.assign(utf8_text);
    return true;
}

bool text_input_model::commit_ime(std::string_view utf8_text)
{
    if (!focused_) {
        return false;
    }

    preedit_text_.clear();
    if (utf8_text.empty()) {
        return false;
    }

    text_.append(utf8_text);
    return true;
}

bool text_input_model::cancel_ime()
{
    if (!focused_ || preedit_text_.empty()) {
        return false;
    }

    preedit_text_.clear();
    return true;
}

bool text_input_model::submit()
{
    if (!focused_) {
        return false;
    }

    submit_text_ = text_;
    text_.clear();
    preedit_text_.clear();
    return true;
}

bool text_input_model::has_submit_text() const
{
    return submit_text_.has_value();
}

std::optional<std::string> text_input_model::consume_submit_text()
{
    if (!submit_text_.has_value()) {
        return std::nullopt;
    }

    std::optional<std::string> consumed = std::move(submit_text_);
    submit_text_.reset();
    return consumed;
}

} // namespace quiz_vulkan::input
