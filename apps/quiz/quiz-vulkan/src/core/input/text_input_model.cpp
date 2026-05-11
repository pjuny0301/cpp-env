#include "core/input/text_input_model.h"

#include <algorithm>
#include <cstddef>
#include <optional>
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

std::optional<text_range> containing_utf8_codepoint(const std::string& value, std::size_t offset)
{
    if (offset == 0 || offset >= value.size()) {
        return std::nullopt;
    }

    const std::size_t search_start = offset > 4 ? offset - 4 : 0;
    for (std::size_t start = search_start; start < offset; ++start) {
        const std::size_t expected = expected_codepoint_length(static_cast<unsigned char>(value[start]));
        const std::size_t end = start + expected;
        if (expected > 1 && start < offset && offset < end && is_valid_codepoint_range(value, start, end)) {
            return text_range{
                .start_byte = start,
                .end_byte = end,
            };
        }
    }

    return std::nullopt;
}

std::size_t floor_utf8_boundary(const std::string& value, std::size_t offset)
{
    const std::size_t clamped = std::min(offset, value.size());
    if (const std::optional<text_range> codepoint = containing_utf8_codepoint(value, clamped)) {
        return codepoint->start_byte;
    }

    return clamped;
}

std::size_t ceil_utf8_boundary(const std::string& value, std::size_t offset)
{
    const std::size_t clamped = std::min(offset, value.size());
    if (const std::optional<text_range> codepoint = containing_utf8_codepoint(value, clamped)) {
        return codepoint->end_byte;
    }

    return clamped;
}

std::size_t nearest_utf8_boundary(const std::string& value, std::size_t offset)
{
    const std::size_t clamped = std::min(offset, value.size());
    if (const std::optional<text_range> codepoint = containing_utf8_codepoint(value, clamped)) {
        const std::size_t distance_to_start = clamped - codepoint->start_byte;
        const std::size_t distance_to_end = codepoint->end_byte - clamped;
        return distance_to_start < distance_to_end ? codepoint->start_byte : codepoint->end_byte;
    }

    return clamped;
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

void erase_utf8_codepoint_after(std::string& value, std::size_t& offset)
{
    if (value.empty()) {
        return;
    }

    offset = std::min(offset, value.size());
    if (offset >= value.size()) {
        return;
    }

    const std::size_t end = next_utf8_boundary(value, offset);
    value.erase(offset, end - offset);
}

text_range normalized_range(text_range range, std::size_t text_size)
{
    text_range normalized = range;
    if (normalized.end_byte < normalized.start_byte) {
        std::swap(normalized.start_byte, normalized.end_byte);
    }
    normalized.start_byte = std::min(normalized.start_byte, text_size);
    normalized.end_byte = std::min(normalized.end_byte, text_size);
    return normalized;
}

text_range normalized_utf8_selection_range(const std::string& value, text_range range)
{
    const text_range normalized = normalized_range(range, value.size());
    if (normalized.start_byte == normalized.end_byte) {
        const std::size_t caret = nearest_utf8_boundary(value, normalized.start_byte);
        return text_range{
            .start_byte = caret,
            .end_byte = caret,
        };
    }

    return text_range{
        .start_byte = floor_utf8_boundary(value, normalized.start_byte),
        .end_byte = ceil_utf8_boundary(value, normalized.end_byte),
    };
}

bool same_range(text_range lhs, text_range rhs)
{
    return lhs.start_byte == rhs.start_byte && lhs.end_byte == rhs.end_byte;
}

std::int64_t text_size_delta(std::size_t before_count, std::size_t after_count)
{
    if (after_count >= before_count) {
        return static_cast<std::int64_t>(after_count - before_count);
    }
    return -static_cast<std::int64_t>(before_count - after_count);
}

bool utf8_boundary_safe(const std::string& value, std::size_t offset)
{
    if (offset > value.size()) {
        return false;
    }
    return offset == 0
        || offset == value.size()
        || !is_continuation_byte(static_cast<unsigned char>(value[offset]));
}

bool utf8_range_boundary_safe(const std::string& value, text_range range)
{
    return range.start_byte <= range.end_byte
        && utf8_boundary_safe(value, range.start_byte)
        && utf8_boundary_safe(value, range.end_byte);
}

bool transaction_snapshot_utf8_boundary_safe(const text_edit_transaction_snapshot& snapshot)
{
    const bool committed_caret_safe =
        utf8_boundary_safe(snapshot.text, snapshot.caret_byte_offset);
    const bool display_caret_safe =
        utf8_range_boundary_safe(snapshot.display_text, snapshot.caret_range);
    const bool selection_safe =
        !snapshot.has_selection || utf8_range_boundary_safe(snapshot.text, snapshot.selection);
    const bool preedit_safe =
        !snapshot.has_preedit || utf8_range_boundary_safe(snapshot.display_text, snapshot.preedit_range);
    const bool composition_safe =
        !snapshot.composition.active
        || (utf8_range_boundary_safe(snapshot.text, snapshot.composition.replacement_range)
            && utf8_range_boundary_safe(snapshot.display_text, snapshot.composition.preedit_range)
            && utf8_range_boundary_safe(snapshot.display_text, snapshot.composition.caret_range));
    return committed_caret_safe
        && display_caret_safe
        && selection_safe
        && preedit_safe
        && composition_safe;
}

text_edit_transaction_byte_diagnostics diff_text_edit_transaction_bytes(
    const text_edit_transaction_snapshot& before,
    const text_edit_transaction_snapshot& after)
{
    std::size_t common_prefix = 0;
    const std::size_t before_size = before.text.size();
    const std::size_t after_size = after.text.size();
    while (common_prefix < before_size
        && common_prefix < after_size
        && before.text[common_prefix] == after.text[common_prefix]) {
        ++common_prefix;
    }

    std::size_t common_suffix = 0;
    while (common_suffix + common_prefix < before_size
        && common_suffix + common_prefix < after_size
        && before.text[before_size - common_suffix - 1] == after.text[after_size - common_suffix - 1]) {
        ++common_suffix;
    }

    const std::size_t deleted_bytes = before_size - common_prefix - common_suffix;
    const std::size_t inserted_bytes = after_size - common_prefix - common_suffix;
    return text_edit_transaction_byte_diagnostics{
        .committed_text_bytes_before = before_size,
        .committed_text_bytes_after = after_size,
        .display_text_bytes_before = before.display_text.size(),
        .display_text_bytes_after = after.display_text.size(),
        .committed_byte_delta = text_size_delta(before_size, after_size),
        .display_byte_delta =
            text_size_delta(before.display_text.size(), after.display_text.size()),
        .inserted_byte_count = inserted_bytes,
        .deleted_byte_count = deleted_bytes,
        .replaced_byte_count = deleted_bytes > 0 && inserted_bytes > 0 ? deleted_bytes : 0,
    };
}

bool transaction_rejection_is_invalid(text_edit_transaction_rejection_reason reason)
{
    return reason != text_edit_transaction_rejection_reason::none
        && reason != text_edit_transaction_rejection_reason::unchanged;
}

text_edit_transaction_diagnostics make_text_edit_transaction_diagnostics(
    text_edit_transaction_operation operation,
    text_edit_transaction_snapshot before,
    text_edit_transaction_snapshot after,
    bool accepted,
    text_edit_transaction_rejection_reason rejection_reason)
{
    text_edit_transaction_diagnostics diagnostics{
        .operation = operation,
        .accepted = accepted,
        .rejected = !accepted,
        .rejection_reason = accepted ? text_edit_transaction_rejection_reason::none : rejection_reason,
        .before = std::move(before),
        .after = std::move(after),
        .bytes = {},
    };
    diagnostics.bytes = diff_text_edit_transaction_bytes(diagnostics.before, diagnostics.after);
    diagnostics.text_changed = diagnostics.before.text != diagnostics.after.text;
    diagnostics.display_text_changed = diagnostics.before.display_text != diagnostics.after.display_text;
    diagnostics.caret_changed =
        diagnostics.before.caret_byte_offset != diagnostics.after.caret_byte_offset
        || !same_range(diagnostics.before.caret_range, diagnostics.after.caret_range);
    diagnostics.selection_changed =
        diagnostics.before.has_selection != diagnostics.after.has_selection
        || !same_range(diagnostics.before.selection, diagnostics.after.selection);
    diagnostics.preedit_changed =
        diagnostics.before.has_preedit != diagnostics.after.has_preedit
        || diagnostics.before.preedit_text != diagnostics.after.preedit_text
        || !same_range(diagnostics.before.preedit_range, diagnostics.after.preedit_range)
        || diagnostics.before.preedit_anchor_byte_offset != diagnostics.after.preedit_anchor_byte_offset;
    diagnostics.before_utf8_boundary_safe =
        transaction_snapshot_utf8_boundary_safe(diagnostics.before);
    diagnostics.after_utf8_boundary_safe =
        transaction_snapshot_utf8_boundary_safe(diagnostics.after);
    diagnostics.utf8_boundary_safe =
        diagnostics.before_utf8_boundary_safe && diagnostics.after_utf8_boundary_safe;
    diagnostics.selection_was_active = diagnostics.before.has_selection;
    diagnostics.selection_replaced_committed_text =
        diagnostics.selection_was_active
        && diagnostics.bytes.deleted_byte_count > 0
        && diagnostics.bytes.inserted_byte_count > 0;
    diagnostics.selection_replaced_display_text =
        diagnostics.selection_was_active
        && !diagnostics.text_changed
        && diagnostics.display_text_changed
        && diagnostics.after.has_preedit;
    diagnostics.selection_deleted =
        diagnostics.selection_was_active
        && diagnostics.bytes.deleted_byte_count > 0
        && diagnostics.bytes.inserted_byte_count == 0;
    diagnostics.selection_cleared = diagnostics.before.has_selection && !diagnostics.after.has_selection;
    diagnostics.ime_preedit_started =
        !diagnostics.before.has_preedit && diagnostics.after.has_preedit;
    diagnostics.ime_preedit_updated =
        diagnostics.before.has_preedit
        && diagnostics.after.has_preedit
        && (diagnostics.before.preedit_text != diagnostics.after.preedit_text
            || !same_range(diagnostics.before.preedit_range, diagnostics.after.preedit_range));
    diagnostics.ime_preedit_cleared =
        diagnostics.before.has_preedit && !diagnostics.after.has_preedit;
    diagnostics.ime_committed =
        operation == text_edit_transaction_operation::commit_ime && accepted;
    diagnostics.ime_canceled =
        operation == text_edit_transaction_operation::cancel_ime && accepted;
    diagnostics.invalid_edit_rejected =
        !accepted && transaction_rejection_is_invalid(diagnostics.rejection_reason);
    diagnostics.changed =
        diagnostics.text_changed
        || diagnostics.display_text_changed
        || diagnostics.caret_changed
        || diagnostics.selection_changed
        || diagnostics.preedit_changed;
    return diagnostics;
}

} // namespace

void text_input_model::focus(std::string target_id)
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    focused_ = true;
    focus_id_ = std::move(target_id);
    clear_preedit();
    reset_selection();
    caret_byte_offset_ = text_.size();
    record_transaction(text_edit_transaction_operation::focus, before, true);
}

void text_input_model::clear_focus()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    focused_ = false;
    focus_id_.clear();
    clear_preedit();
    reset_selection();
    record_transaction(text_edit_transaction_operation::clear_focus, before, true);
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
    if (!ime_composition_active_) {
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
    const std::size_t caret = ime_composition_active_
        ? preedit_anchor_byte_offset() + preedit_text_.size()
        : caret_byte_offset();
    return text_range{
        .start_byte = caret,
        .end_byte = caret,
    };
}

std::optional<text_range> text_input_model::preedit_range() const
{
    if (!ime_composition_active_) {
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

ime_composition_state text_input_model::ime_composition() const
{
    const text_range replacement = ime_replacement_range();
    const text_range preedit{
        .start_byte = replacement.start_byte,
        .end_byte = replacement.start_byte + preedit_text_.size(),
    };
    const text_range caret{
        .start_byte = preedit.end_byte,
        .end_byte = preedit.end_byte,
    };
    return ime_composition_state{
        .active = ime_composition_active_,
        .preedit_text = preedit_text_,
        .replacement_range = replacement,
        .preedit_range = preedit,
        .caret_range = caret,
    };
}

const text_edit_transaction_diagnostics& text_input_model::last_transaction_diagnostics() const
{
    return last_transaction_;
}

text_edit_transaction_snapshot text_input_model::transaction_snapshot() const
{
    const ime_composition_state composition = ime_composition();
    const std::optional<text_range> selection = selection_range();
    const std::optional<text_range> active_preedit = preedit_range();
    return text_edit_transaction_snapshot{
        .has_focus = focused_,
        .target_id = focus_id_,
        .text = text_,
        .display_text = display_text(),
        .caret_byte_offset = caret_byte_offset(),
        .caret_range = caret_range(),
        .has_selection = selection.has_value(),
        .selection = selection.value_or(text_range{}),
        .has_preedit = active_preedit.has_value(),
        .preedit_text = preedit_text_,
        .preedit_range = active_preedit.value_or(text_range{}),
        .preedit_anchor_byte_offset = composition.replacement_range.start_byte,
        .composition = composition,
    };
}

void text_input_model::record_transaction(
    text_edit_transaction_operation operation,
    const text_edit_transaction_snapshot& before,
    bool accepted,
    text_edit_transaction_rejection_reason rejection_reason)
{
    last_transaction_ = make_text_edit_transaction_diagnostics(
        operation,
        before,
        transaction_snapshot(),
        accepted,
        rejection_reason);
}

bool text_input_model::move_caret_to_start()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::move_caret_to_start,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    const bool had_selection = selection_range_.has_value();
    const bool had_preedit = ime_composition_active_;
    if (!had_selection && !had_preedit && caret_byte_offset() == 0) {
        record_transaction(
            text_edit_transaction_operation::move_caret_to_start,
            before,
            false,
            text_edit_transaction_rejection_reason::unchanged);
        return false;
    }

    reset_selection();
    clear_preedit();
    caret_byte_offset_ = 0;
    record_transaction(text_edit_transaction_operation::move_caret_to_start, before, true);
    return true;
}

bool text_input_model::move_caret_to_end()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::move_caret_to_end,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    const bool had_selection = selection_range_.has_value();
    const bool had_preedit = ime_composition_active_;
    if (!had_selection && !had_preedit && caret_byte_offset() == text_.size()) {
        record_transaction(
            text_edit_transaction_operation::move_caret_to_end,
            before,
            false,
            text_edit_transaction_rejection_reason::unchanged);
        return false;
    }

    reset_selection();
    clear_preedit();
    caret_byte_offset_ = text_.size();
    record_transaction(text_edit_transaction_operation::move_caret_to_end, before, true);
    return true;
}

bool text_input_model::move_caret_left()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::move_caret_left,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    if (selection_range_.has_value()) {
        caret_byte_offset_ = selection_range_->start_byte;
        reset_selection();
        clear_preedit();
        record_transaction(text_edit_transaction_operation::move_caret_left, before, true);
        return true;
    }

    const bool had_preedit = ime_composition_active_;
    if (caret_byte_offset() == 0) {
        if (had_preedit) {
            clear_preedit();
        }
        record_transaction(
            text_edit_transaction_operation::move_caret_left,
            before,
            had_preedit,
            text_edit_transaction_rejection_reason::no_text_before_caret);
        return had_preedit;
    }

    caret_byte_offset_ = previous_utf8_boundary(text_, caret_byte_offset());
    clear_preedit();
    record_transaction(text_edit_transaction_operation::move_caret_left, before, true);
    return true;
}

bool text_input_model::move_caret_right()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::move_caret_right,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    if (selection_range_.has_value()) {
        caret_byte_offset_ = selection_range_->end_byte;
        reset_selection();
        clear_preedit();
        record_transaction(text_edit_transaction_operation::move_caret_right, before, true);
        return true;
    }

    const bool had_preedit = ime_composition_active_;
    if (caret_byte_offset() >= text_.size()) {
        if (had_preedit) {
            clear_preedit();
        }
        record_transaction(
            text_edit_transaction_operation::move_caret_right,
            before,
            had_preedit,
            text_edit_transaction_rejection_reason::no_text_after_caret);
        return had_preedit;
    }

    caret_byte_offset_ = next_utf8_boundary(text_, caret_byte_offset());
    clear_preedit();
    record_transaction(text_edit_transaction_operation::move_caret_right, before, true);
    return true;
}

bool text_input_model::extend_selection_left()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::extend_selection_left,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    const bool had_preedit = ime_composition_active_;
    if (caret_byte_offset() == 0) {
        if (had_preedit) {
            clear_preedit();
        }
        record_transaction(
            text_edit_transaction_operation::extend_selection_left,
            before,
            had_preedit,
            text_edit_transaction_rejection_reason::no_text_before_caret);
        return had_preedit;
    }

    const std::size_t anchor = selection_anchor_byte_offset_.value_or(caret_byte_offset());
    const std::size_t active = previous_utf8_boundary(text_, caret_byte_offset());
    set_selection_from_anchor(anchor, active);
    clear_preedit();
    record_transaction(text_edit_transaction_operation::extend_selection_left, before, true);
    return true;
}

bool text_input_model::extend_selection_right()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::extend_selection_right,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    const bool had_preedit = ime_composition_active_;
    if (caret_byte_offset() >= text_.size()) {
        if (had_preedit) {
            clear_preedit();
        }
        record_transaction(
            text_edit_transaction_operation::extend_selection_right,
            before,
            had_preedit,
            text_edit_transaction_rejection_reason::no_text_after_caret);
        return had_preedit;
    }

    const std::size_t anchor = selection_anchor_byte_offset_.value_or(caret_byte_offset());
    const std::size_t active = next_utf8_boundary(text_, caret_byte_offset());
    set_selection_from_anchor(anchor, active);
    clear_preedit();
    record_transaction(text_edit_transaction_operation::extend_selection_right, before, true);
    return true;
}

bool text_input_model::select_all()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_ || text_.empty()) {
        record_transaction(
            text_edit_transaction_operation::select_all,
            before,
            false,
            focused_
                ? text_edit_transaction_rejection_reason::empty_text
                : text_edit_transaction_rejection_reason::not_focused);
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
        || ime_composition_active_;
    selection_range_ = all;
    selection_anchor_byte_offset_ = all.start_byte;
    caret_byte_offset_ = all.end_byte;
    clear_preedit();
    record_transaction(
        text_edit_transaction_operation::select_all,
        before,
        changed,
        text_edit_transaction_rejection_reason::unchanged);
    return changed;
}

bool text_input_model::clear_selection()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_ || !selection_range_.has_value()) {
        record_transaction(
            text_edit_transaction_operation::clear_selection,
            before,
            false,
            focused_
                ? text_edit_transaction_rejection_reason::no_selection
                : text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    reset_selection();
    record_transaction(text_edit_transaction_operation::clear_selection, before, true);
    return true;
}

bool text_input_model::set_selection(text_range range)
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::set_selection,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    const text_range normalized = normalized_utf8_selection_range(text_, range);
    const bool collapsed = normalized.start_byte == normalized.end_byte;
    const bool changed = selection_range_.has_value()
        ? (!collapsed && !same_range(*selection_range_, normalized))
            || collapsed
            || caret_byte_offset() != normalized.end_byte
            || ime_composition_active_
        : !collapsed || caret_byte_offset() != normalized.end_byte || ime_composition_active_;

    if (collapsed) {
        reset_selection();
        caret_byte_offset_ = normalized.end_byte;
        clear_preedit();
        record_transaction(
            text_edit_transaction_operation::set_selection,
            before,
            changed,
            text_edit_transaction_rejection_reason::unchanged);
        return changed;
    }

    selection_range_ = normalized;
    selection_anchor_byte_offset_ = normalized.start_byte;
    caret_byte_offset_ = normalized.end_byte;
    clear_preedit();
    record_transaction(
        text_edit_transaction_operation::set_selection,
        before,
        changed,
        text_edit_transaction_rejection_reason::unchanged);
    return changed;
}

bool text_input_model::commit_utf8(std::string_view utf8_text)
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_ || utf8_text.empty()) {
        record_transaction(
            text_edit_transaction_operation::commit_utf8,
            before,
            false,
            focused_
                ? text_edit_transaction_rejection_reason::empty_text
                : text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    replace_selection_with(utf8_text);
    clear_preedit();
    record_transaction(text_edit_transaction_operation::commit_utf8, before, true);
    return true;
}

bool text_input_model::backspace()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::backspace,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    if (erase_selected_text()) {
        clear_preedit();
        record_transaction(text_edit_transaction_operation::backspace, before, true);
        return true;
    }

    if (ime_composition_active_) {
        clear_preedit();
        record_transaction(text_edit_transaction_operation::backspace, before, true);
        return true;
    }

    if (text_.empty() || caret_byte_offset() == 0) {
        record_transaction(
            text_edit_transaction_operation::backspace,
            before,
            false,
            text_edit_transaction_rejection_reason::no_text_before_caret);
        return false;
    }

    erase_utf8_codepoint_before(text_, caret_byte_offset_);
    record_transaction(text_edit_transaction_operation::backspace, before, true);
    return true;
}

bool text_input_model::delete_forward()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::delete_forward,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    if (erase_selected_text()) {
        clear_preedit();
        record_transaction(text_edit_transaction_operation::delete_forward, before, true);
        return true;
    }

    if (ime_composition_active_) {
        clear_preedit();
        record_transaction(text_edit_transaction_operation::delete_forward, before, true);
        return true;
    }

    if (text_.empty() || caret_byte_offset() >= text_.size()) {
        record_transaction(
            text_edit_transaction_operation::delete_forward,
            before,
            false,
            text_edit_transaction_rejection_reason::no_text_after_caret);
        return false;
    }

    erase_utf8_codepoint_after(text_, caret_byte_offset_);
    record_transaction(text_edit_transaction_operation::delete_forward, before, true);
    return true;
}

bool text_input_model::set_preedit(std::string_view utf8_text)
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::set_preedit,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    preedit_text_.assign(utf8_text);
    ime_composition_active_ = true;
    record_transaction(text_edit_transaction_operation::set_preedit, before, true);
    return true;
}

bool text_input_model::commit_ime(std::string_view utf8_text)
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::commit_ime,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    clear_preedit();
    if (utf8_text.empty()) {
        record_transaction(
            text_edit_transaction_operation::commit_ime,
            before,
            false,
            text_edit_transaction_rejection_reason::empty_ime_commit);
        return false;
    }

    replace_selection_with(utf8_text);
    record_transaction(text_edit_transaction_operation::commit_ime, before, true);
    return true;
}

bool text_input_model::cancel_ime()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_ || !ime_composition_active_) {
        record_transaction(
            text_edit_transaction_operation::cancel_ime,
            before,
            false,
            focused_
                ? text_edit_transaction_rejection_reason::no_active_ime
                : text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    clear_preedit();
    record_transaction(text_edit_transaction_operation::cancel_ime, before, true);
    return true;
}

bool text_input_model::submit()
{
    const text_edit_transaction_snapshot before = transaction_snapshot();
    if (!focused_) {
        record_transaction(
            text_edit_transaction_operation::submit,
            before,
            false,
            text_edit_transaction_rejection_reason::not_focused);
        return false;
    }

    submit_text_ = text_;
    text_.clear();
    clear_preedit();
    reset_selection();
    caret_byte_offset_ = 0;
    record_transaction(text_edit_transaction_operation::submit, before, true);
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

text_range text_input_model::ime_replacement_range() const
{
    if (selection_range_.has_value()) {
        return *selection_range_;
    }

    const std::size_t caret = caret_byte_offset();
    return text_range{
        .start_byte = caret,
        .end_byte = caret,
    };
}

void text_input_model::set_selection_from_anchor(std::size_t anchor_byte, std::size_t active_byte)
{
    anchor_byte = std::min(anchor_byte, text_.size());
    active_byte = std::min(active_byte, text_.size());
    caret_byte_offset_ = active_byte;

    if (anchor_byte == active_byte) {
        reset_selection();
        return;
    }

    selection_range_ = text_range{
        .start_byte = std::min(anchor_byte, active_byte),
        .end_byte = std::max(anchor_byte, active_byte),
    };
    selection_anchor_byte_offset_ = anchor_byte;
}

void text_input_model::clear_preedit()
{
    preedit_text_.clear();
    ime_composition_active_ = false;
}

void text_input_model::reset_selection()
{
    selection_range_.reset();
    selection_anchor_byte_offset_.reset();
}

void text_input_model::replace_selection_with(std::string_view utf8_text)
{
    const std::size_t insert_byte = preedit_anchor_byte_offset();
    if (selection_range_.has_value()) {
        const text_range selection = *selection_range_;
        text_.erase(selection.start_byte, selection.end_byte - selection.start_byte);
        reset_selection();
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
    reset_selection();
    return true;
}

} // namespace quiz_vulkan::input
