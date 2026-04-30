#include "core/input/text_input_model.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace quiz_vulkan::input {
namespace {

bool is_continuation_byte(unsigned char byte)
{
    return (byte & 0xc0U) == 0x80U;
}

std::size_t expected_codepoint_length(unsigned char lead)
{
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
}

bool is_valid_codepoint_range(const std::string& value, std::size_t start, std::size_t end)
{
    if (start >= end || end > value.size()) {
        return false;
    }

    const std::size_t expected = expected_codepoint_length(static_cast<unsigned char>(value[start]));
    if (expected == 0 || expected != end - start) {
        return false;
    }

    for (std::size_t index = start + 1; index < end; ++index) {
        if (!is_continuation_byte(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

std::size_t next_utf8_boundary(const std::string& value, std::size_t offset)
{
    if (offset >= value.size()) {
        return value.size();
    }

    const std::size_t expected = expected_codepoint_length(static_cast<unsigned char>(value[offset]));
    if (expected > 0 && offset + expected <= value.size()
        && is_valid_codepoint_range(value, offset, offset + expected)) {
        return offset + expected;
    }

    return offset + 1;
}

std::size_t previous_utf8_boundary(const std::string& value, std::size_t offset)
{
    if (value.empty() || offset == 0) {
        return 0;
    }

    const std::size_t clamped = std::min(offset, value.size());
    const std::size_t search_start = clamped > 4 ? clamped - 4 : 0;
    for (std::size_t start = search_start; start < clamped; ++start) {
        if (is_valid_codepoint_range(value, start, clamped)) {
            return start;
        }
    }

    return clamped - 1;
}

void erase_utf8_codepoint_before(std::string& value, std::size_t& offset)
{
    if (value.empty()) {
        return;
    }

    offset = std::min(offset, value.size());
    if (offset == 0) {
        return;
    }

    const std::size_t start = previous_utf8_boundary(value, offset);
    value.erase(start, offset - start);
    offset = start;
}

text_range normalized_range(text_range range, std::size_t text_size)
{
    text_range normalized{
        .start_byte = std::min(range.start_byte, text_size),
        .end_byte = std::min(range.end_byte, text_size),
    };
    if (normalized.end_byte < normalized.start_byte) {
        std::swap(normalized.start_byte, normalized.end_byte);
    }
    return normalized;
}

bool same_range(text_range lhs, text_range rhs)
{
    return lhs.start_byte == rhs.start_byte && lhs.end_byte == rhs.end_byte;
}

} // namespace

void text_input_model::focus(std::string target_id)
{
    focused_ = true;
    focus_id_ = std::move(target_id);
    preedit_text_.clear();
    selection_range_.reset();
    caret_byte_offset_ = text_.size();
}

void text_input_model::clear_focus()
{
    focused_ = false;
    focus_id_.clear();
    preedit_text_.clear();
    selection_range_.reset();
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
    if (preedit_text_.empty()) {
        return text_;
    }

    const std::size_t anchor = preedit_anchor_byte_offset();
    std::string display = text_.substr(0, anchor);
    display.append(preedit_text_);
    display.append(text_.substr(selection_range_.has_value() ? selection_range_->end_byte : anchor));
    return display;
}

std::size_t text_input_model::caret_byte_offset() const
{
    return std::min(caret_byte_offset_, text_.size());
}

text_range text_input_model::caret_range() const
{
    const std::size_t caret = preedit_text_.empty()
        ? caret_byte_offset()
        : preedit_anchor_byte_offset() + preedit_text_.size();
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
        .start_byte = preedit_anchor_byte_offset(),
        .end_byte = preedit_anchor_byte_offset() + preedit_text_.size(),
    };
}

std::optional<text_range> text_input_model::selection_range() const
{
    return selection_range_;
}

bool text_input_model::move_caret_to_start()
{
    if (!focused_) {
        return false;
    }

    const bool had_selection = selection_range_.has_value();
    if (!had_selection && caret_byte_offset() == 0) {
        return false;
    }

    selection_range_.reset();
    caret_byte_offset_ = 0;
    return true;
}

bool text_input_model::move_caret_to_end()
{
    if (!focused_) {
        return false;
    }

    const bool had_selection = selection_range_.has_value();
    if (!had_selection && caret_byte_offset() == text_.size()) {
        return false;
    }

    selection_range_.reset();
    caret_byte_offset_ = text_.size();
    return true;
}

bool text_input_model::move_caret_left()
{
    if (!focused_) {
        return false;
    }

    if (selection_range_.has_value()) {
        caret_byte_offset_ = selection_range_->start_byte;
        selection_range_.reset();
        return true;
    }

    if (caret_byte_offset() == 0) {
        return false;
    }

    caret_byte_offset_ = previous_utf8_boundary(text_, caret_byte_offset());
    return true;
}

bool text_input_model::move_caret_right()
{
    if (!focused_) {
        return false;
    }

    if (selection_range_.has_value()) {
        caret_byte_offset_ = selection_range_->end_byte;
        selection_range_.reset();
        return true;
    }

    if (caret_byte_offset() >= text_.size()) {
        return false;
    }

    caret_byte_offset_ = next_utf8_boundary(text_, caret_byte_offset());
    return true;
}

bool text_input_model::select_all()
{
    if (!focused_ || text_.empty()) {
        return false;
    }

    constexpr std::size_t start = 0;
    const text_range all{
        .start_byte = start,
        .end_byte = text_.size(),
    };
    const bool changed = !selection_range_.has_value()
        || !same_range(*selection_range_, all)
        || caret_byte_offset() != all.end_byte
        || !preedit_text_.empty();
    selection_range_ = all;
    caret_byte_offset_ = all.end_byte;
    preedit_text_.clear();
    return changed;
}

bool text_input_model::clear_selection()
{
    if (!focused_ || !selection_range_.has_value()) {
        return false;
    }

    selection_range_.reset();
    return true;
}

bool text_input_model::set_selection(text_range range)
{
    if (!focused_) {
        return false;
    }

    const text_range normalized = normalized_range(range, text_.size());
    const bool collapsed = normalized.start_byte == normalized.end_byte;
    const bool changed = selection_range_.has_value()
        ? (!collapsed && !same_range(*selection_range_, normalized))
            || collapsed
            || caret_byte_offset() != normalized.end_byte
            || !preedit_text_.empty()
        : !collapsed || caret_byte_offset() != normalized.end_byte || !preedit_text_.empty();

    if (collapsed) {
        selection_range_.reset();
        caret_byte_offset_ = normalized.end_byte;
        preedit_text_.clear();
        return changed;
    }

    selection_range_ = normalized;
    caret_byte_offset_ = normalized.end_byte;
    preedit_text_.clear();
    return changed;
}

bool text_input_model::commit_utf8(std::string_view utf8_text)
{
    if (!focused_ || utf8_text.empty()) {
        return false;
    }

    replace_selection_with(utf8_text);
    preedit_text_.clear();
    return true;
}

bool text_input_model::backspace()
{
    if (!focused_) {
        return false;
    }

    if (erase_selected_text()) {
        preedit_text_.clear();
        return true;
    }

    if (!preedit_text_.empty()) {
        preedit_text_.clear();
        return true;
    }

    if (text_.empty() || caret_byte_offset() == 0) {
        return false;
    }

    erase_utf8_codepoint_before(text_, caret_byte_offset_);
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

    replace_selection_with(utf8_text);
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
    selection_range_.reset();
    caret_byte_offset_ = 0;
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

std::size_t text_input_model::preedit_anchor_byte_offset() const
{
    if (selection_range_.has_value()) {
        return selection_range_->start_byte;
    }

    return caret_byte_offset();
}

void text_input_model::replace_selection_with(std::string_view utf8_text)
{
    const std::size_t insert_byte = preedit_anchor_byte_offset();
    if (selection_range_.has_value()) {
        const text_range selection = *selection_range_;
        text_.erase(selection.start_byte, selection.end_byte - selection.start_byte);
        selection_range_.reset();
    }

    text_.insert(insert_byte, utf8_text);
    caret_byte_offset_ = insert_byte + utf8_text.size();
}

bool text_input_model::erase_selected_text()
{
    if (!selection_range_.has_value()) {
        return false;
    }

    const text_range selection = *selection_range_;
    text_.erase(selection.start_byte, selection.end_byte - selection.start_byte);
    caret_byte_offset_ = selection.start_byte;
    selection_range_.reset();
    return true;
}

} // namespace quiz_vulkan::input
