#pragma once

#include "core/input/input_engine.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace quiz_vulkan::input::detail {

struct text_edit_snapshot {
    std::size_t text_byte_count = 0;
    text_range caret;
    bool has_selection = false;
    text_range selection;
};

[[nodiscard]] inline text_edit_snapshot capture_text_edit(const text_input_model& model)
{
    text_edit_snapshot snapshot{};
    snapshot.text_byte_count = model.text().size();
    snapshot.caret = model.caret_range();
    if (const std::optional<text_range> selection = model.selection_range()) {
        snapshot.has_selection = true;
        snapshot.selection = *selection;
    }
    return snapshot;
}

inline void apply_text_edit_boundary(
    action_route_policy_diagnostic& diagnostic,
    text_edit_snapshot before,
    text_edit_snapshot after)
{
    diagnostic.text_byte_count_before = before.text_byte_count;
    diagnostic.text_byte_count_after = after.text_byte_count;
    diagnostic.caret_before = before.caret;
    diagnostic.caret_after = after.caret;
    diagnostic.had_selection_before = before.has_selection;
    diagnostic.has_selection_after = after.has_selection;
    diagnostic.selection_before = before.selection;
    diagnostic.selection_after = after.selection;
}

[[nodiscard]] inline std::size_t removed_text_byte_count(text_edit_snapshot before, text_edit_snapshot after)
{
    return before.text_byte_count >= after.text_byte_count
        ? before.text_byte_count - after.text_byte_count
        : 0;
}

[[nodiscard]] inline action_route_policy_diagnostic make_text_event_policy(
    action_route_policy_kind kind,
    std::int64_t timestamp_ms,
    std::size_t event_index,
    const std::string& target_id,
    text_edit_snapshot before,
    text_edit_snapshot after,
    pointer_capture_snapshot pointer_capture_before,
    pointer_capture_snapshot pointer_capture_after)
{
    action_route_policy_diagnostic diagnostic{};
    diagnostic.kind = kind;
    diagnostic.timestamp_ms = timestamp_ms;
    diagnostic.pointer_capture_before = pointer_capture_before;
    diagnostic.pointer_capture_after = pointer_capture_after;
    diagnostic.tracked_pointer_count_before = pointer_capture_before.tracked_pointer_count;
    diagnostic.tracked_pointer_count_after = pointer_capture_after.tracked_pointer_count;
    diagnostic.emits_input_event = true;
    diagnostic.event_index = event_index;
    diagnostic.target_id = target_id;
    apply_text_edit_boundary(diagnostic, before, after);
    return diagnostic;
}

[[nodiscard]] inline action_route_policy_diagnostic make_text_state_policy(
    action_route_policy_kind kind,
    std::int64_t timestamp_ms,
    const std::string& target_id,
    text_edit_snapshot before,
    text_edit_snapshot after,
    pointer_capture_snapshot pointer_capture_before,
    pointer_capture_snapshot pointer_capture_after)
{
    action_route_policy_diagnostic diagnostic{};
    diagnostic.kind = kind;
    diagnostic.timestamp_ms = timestamp_ms;
    diagnostic.pointer_capture_before = pointer_capture_before;
    diagnostic.pointer_capture_after = pointer_capture_after;
    diagnostic.tracked_pointer_count_before = pointer_capture_before.tracked_pointer_count;
    diagnostic.tracked_pointer_count_after = pointer_capture_after.tracked_pointer_count;
    diagnostic.target_id = target_id;
    apply_text_edit_boundary(diagnostic, before, after);
    return diagnostic;
}

} // namespace quiz_vulkan::input::detail
