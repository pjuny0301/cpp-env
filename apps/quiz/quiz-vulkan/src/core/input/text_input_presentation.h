#pragma once

#include "core/input/input_routing_diagnostics.h"
#include "core/input/text_input_model.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace quiz_vulkan::input {

struct text_input_presentation_byte_counts {
    std::size_t committed_text_bytes = 0;
    std::size_t display_text_bytes = 0;
    std::size_t preedit_text_bytes = 0;
    std::size_t selected_text_bytes = 0;
    std::size_t preedit_range_bytes = 0;
    std::size_t caret_byte_offset = 0;
    std::size_t preedit_anchor_byte_offset = 0;
};

struct text_input_presentation_route_byte_diagnostics {
    bool available = false;
    std::size_t text_byte_count = 0;
    std::size_t text_byte_count_before = 0;
    std::size_t text_byte_count_after = 0;
    std::int64_t text_byte_delta = 0;
};

struct text_input_presentation_snapshot {
    bool has_focus = false;
    std::string target_id;
    std::string committed_text;
    std::string display_text;
    std::size_t caret_byte_offset = 0;
    text_range caret_range;
    bool has_selection = false;
    text_range selection_range;
    bool has_preedit = false;
    std::string preedit_text;
    text_range preedit_range;
    std::size_t preedit_anchor_byte_offset = 0;
    bool has_submit_text = false;
    bool focus_clean = true;
    bool preedit_clean = true;
    text_input_presentation_byte_counts byte_counts;
    text_input_presentation_route_byte_diagnostics route_byte_diagnostics;
};

struct text_input_presentation_count_delta {
    std::size_t before_count = 0;
    std::size_t after_count = 0;
    std::int64_t delta = 0;
    bool changed = false;
};

struct text_input_presentation_bool_delta {
    bool before_value = false;
    bool after_value = false;
    bool changed = false;
};

struct text_input_presentation_int_delta {
    std::int64_t before_value = 0;
    std::int64_t after_value = 0;
    std::int64_t delta = 0;
    bool changed = false;
};

struct text_input_presentation_range_delta {
    text_range before_range;
    text_range after_range;
    bool changed = false;
};

struct text_input_presentation_string_delta {
    std::string before_value;
    std::string after_value;
    std::int64_t byte_delta = 0;
    bool changed = false;
};

struct text_input_presentation_byte_count_deltas {
    text_input_presentation_count_delta committed_text_bytes;
    text_input_presentation_count_delta display_text_bytes;
    text_input_presentation_count_delta preedit_text_bytes;
    text_input_presentation_count_delta selected_text_bytes;
    text_input_presentation_count_delta preedit_range_bytes;
    text_input_presentation_count_delta caret_byte_offset;
    text_input_presentation_count_delta preedit_anchor_byte_offset;
    bool changed = false;
};

struct text_input_presentation_route_byte_diagnostics_diff {
    text_input_presentation_bool_delta available;
    text_input_presentation_count_delta text_byte_count;
    text_input_presentation_count_delta text_byte_count_before;
    text_input_presentation_count_delta text_byte_count_after;
    text_input_presentation_int_delta text_byte_delta;
    bool changed = false;
};

struct text_input_presentation_diff {
    text_input_presentation_bool_delta has_focus;
    text_input_presentation_string_delta target_id;
    text_input_presentation_string_delta committed_text;
    text_input_presentation_string_delta display_text;
    text_input_presentation_count_delta caret_byte_offset;
    text_input_presentation_range_delta caret_range;
    text_input_presentation_bool_delta has_selection;
    text_input_presentation_range_delta selection_range;
    text_input_presentation_bool_delta has_preedit;
    text_input_presentation_string_delta preedit_text;
    text_input_presentation_range_delta preedit_range;
    text_input_presentation_count_delta preedit_anchor_byte_offset;
    text_input_presentation_bool_delta has_submit_text;
    text_input_presentation_bool_delta focus_clean;
    text_input_presentation_bool_delta preedit_clean;
    text_input_presentation_byte_count_deltas byte_counts;
    text_input_presentation_route_byte_diagnostics_diff route_byte_diagnostics;
    bool focus_changed = false;
    bool target_changed = false;
    bool committed_text_changed = false;
    bool display_text_changed = false;
    bool caret_changed = false;
    bool selection_changed = false;
    bool preedit_changed = false;
    bool submit_changed = false;
    bool clean_flags_changed = false;
    bool byte_counts_changed = false;
    bool route_byte_diagnostics_changed = false;
    bool changed = false;
};

[[nodiscard]] inline std::size_t text_input_presentation_range_byte_count(text_range range)
{
    return range.end_byte >= range.start_byte ? range.end_byte - range.start_byte : 0;
}

[[nodiscard]] inline std::int64_t text_input_presentation_size_delta(
    std::size_t before_count,
    std::size_t after_count)
{
    if (after_count >= before_count) {
        return static_cast<std::int64_t>(after_count - before_count);
    }
    return -static_cast<std::int64_t>(before_count - after_count);
}

[[nodiscard]] inline text_input_presentation_count_delta diff_text_input_presentation_count(
    std::size_t before_count,
    std::size_t after_count)
{
    return text_input_presentation_count_delta{
        .before_count = before_count,
        .after_count = after_count,
        .delta = text_input_presentation_size_delta(before_count, after_count),
        .changed = before_count != after_count,
    };
}

[[nodiscard]] inline text_input_presentation_bool_delta diff_text_input_presentation_bool(
    bool before_value,
    bool after_value)
{
    return text_input_presentation_bool_delta{
        .before_value = before_value,
        .after_value = after_value,
        .changed = before_value != after_value,
    };
}

[[nodiscard]] inline text_input_presentation_int_delta diff_text_input_presentation_int(
    std::int64_t before_value,
    std::int64_t after_value)
{
    return text_input_presentation_int_delta{
        .before_value = before_value,
        .after_value = after_value,
        .delta = after_value - before_value,
        .changed = before_value != after_value,
    };
}

[[nodiscard]] inline bool text_input_presentation_ranges_equal(text_range before_range, text_range after_range)
{
    return before_range.start_byte == after_range.start_byte && before_range.end_byte == after_range.end_byte;
}

[[nodiscard]] inline text_input_presentation_range_delta diff_text_input_presentation_range(
    text_range before_range,
    text_range after_range)
{
    return text_input_presentation_range_delta{
        .before_range = before_range,
        .after_range = after_range,
        .changed = !text_input_presentation_ranges_equal(before_range, after_range),
    };
}

[[nodiscard]] inline text_input_presentation_string_delta diff_text_input_presentation_string(
    std::string before_value,
    std::string after_value)
{
    text_input_presentation_string_delta diff{
        .before_value = std::move(before_value),
        .after_value = std::move(after_value),
    };
    diff.byte_delta =
        text_input_presentation_size_delta(diff.before_value.size(), diff.after_value.size());
    diff.changed = diff.before_value != diff.after_value;
    return diff;
}

[[nodiscard]] inline text_input_presentation_byte_count_deltas diff_text_input_presentation_byte_counts(
    const text_input_presentation_byte_counts& before,
    const text_input_presentation_byte_counts& after)
{
    text_input_presentation_byte_count_deltas diff{
        .committed_text_bytes =
            diff_text_input_presentation_count(before.committed_text_bytes, after.committed_text_bytes),
        .display_text_bytes =
            diff_text_input_presentation_count(before.display_text_bytes, after.display_text_bytes),
        .preedit_text_bytes =
            diff_text_input_presentation_count(before.preedit_text_bytes, after.preedit_text_bytes),
        .selected_text_bytes =
            diff_text_input_presentation_count(before.selected_text_bytes, after.selected_text_bytes),
        .preedit_range_bytes =
            diff_text_input_presentation_count(before.preedit_range_bytes, after.preedit_range_bytes),
        .caret_byte_offset =
            diff_text_input_presentation_count(before.caret_byte_offset, after.caret_byte_offset),
        .preedit_anchor_byte_offset = diff_text_input_presentation_count(
            before.preedit_anchor_byte_offset,
            after.preedit_anchor_byte_offset),
    };
    diff.changed =
        diff.committed_text_bytes.changed
        || diff.display_text_bytes.changed
        || diff.preedit_text_bytes.changed
        || diff.selected_text_bytes.changed
        || diff.preedit_range_bytes.changed
        || diff.caret_byte_offset.changed
        || diff.preedit_anchor_byte_offset.changed;
    return diff;
}

[[nodiscard]] inline text_input_presentation_route_byte_diagnostics_diff
diff_text_input_presentation_route_byte_diagnostics(
    const text_input_presentation_route_byte_diagnostics& before,
    const text_input_presentation_route_byte_diagnostics& after)
{
    text_input_presentation_route_byte_diagnostics_diff diff{
        .available = diff_text_input_presentation_bool(before.available, after.available),
        .text_byte_count =
            diff_text_input_presentation_count(before.text_byte_count, after.text_byte_count),
        .text_byte_count_before =
            diff_text_input_presentation_count(before.text_byte_count_before, after.text_byte_count_before),
        .text_byte_count_after =
            diff_text_input_presentation_count(before.text_byte_count_after, after.text_byte_count_after),
        .text_byte_delta =
            diff_text_input_presentation_int(before.text_byte_delta, after.text_byte_delta),
    };
    diff.changed =
        diff.available.changed
        || diff.text_byte_count.changed
        || diff.text_byte_count_before.changed
        || diff.text_byte_count_after.changed
        || diff.text_byte_delta.changed;
    return diff;
}

[[nodiscard]] inline bool text_input_presentation_route_has_text_byte_diagnostics(
    action_route_policy_kind kind)
{
    switch (kind) {
    case action_route_policy_kind::text_commit_boundary:
    case action_route_policy_kind::text_backspace_boundary:
    case action_route_policy_kind::text_delete_forward_boundary:
    case action_route_policy_kind::caret_moved:
    case action_route_policy_kind::selection_changed:
    case action_route_policy_kind::focus_traversal_next:
    case action_route_policy_kind::focus_traversal_previous:
    case action_route_policy_kind::text_submit_boundary:
    case action_route_policy_kind::keyboard_cancel_intent:
    case action_route_policy_kind::focus_loss:
    case action_route_policy_kind::ime_preedit:
    case action_route_policy_kind::ime_commit:
    case action_route_policy_kind::ime_cancel:
    case action_route_policy_kind::ime_composition_start:
        return true;
    case action_route_policy_kind::pointer_capture_reset:
    case action_route_policy_kind::pointer_capture_arbitration:
    case action_route_policy_kind::wheel_summary:
    case action_route_policy_kind::gesture_route_snapshot:
        return false;
    }

    return false;
}

[[nodiscard]] inline text_input_presentation_route_byte_diagnostics
make_text_input_presentation_route_byte_diagnostics(
    const action_route_policy_diagnostic& route)
{
    return text_input_presentation_route_byte_diagnostics{
        .available = true,
        .text_byte_count = route.text_byte_count,
        .text_byte_count_before = route.text_byte_count_before,
        .text_byte_count_after = route.text_byte_count_after,
        .text_byte_delta =
            text_input_presentation_size_delta(route.text_byte_count_before, route.text_byte_count_after),
    };
}

[[nodiscard]] inline text_input_presentation_route_byte_diagnostics
find_text_input_presentation_route_byte_diagnostics(
    const input_routing_diagnostics& diagnostics)
{
    for (auto route = diagnostics.action_routes.rbegin(); route != diagnostics.action_routes.rend(); ++route) {
        if (text_input_presentation_route_has_text_byte_diagnostics(route->kind)) {
            return make_text_input_presentation_route_byte_diagnostics(*route);
        }
    }
    return {};
}

[[nodiscard]] inline text_input_presentation_snapshot make_text_input_presentation_snapshot(
    const text_input_model& model,
    text_input_presentation_route_byte_diagnostics route_byte_diagnostics = {})
{
    const std::string display_text = model.display_text();
    const std::optional<text_range> selection = model.selection_range();
    const ime_composition_state composition = model.ime_composition();
    const text_range preedit_range = composition.preedit_range;
    const bool has_preedit = composition.active;
    const bool preedit_clean = !composition.active && model.preedit_text().empty();

    text_input_presentation_snapshot snapshot{
        .has_focus = model.has_focus(),
        .target_id = model.focus_id(),
        .committed_text = model.text(),
        .display_text = display_text,
        .caret_byte_offset = model.caret_byte_offset(),
        .caret_range = model.caret_range(),
        .has_selection = selection.has_value(),
        .selection_range = selection.value_or(text_range{}),
        .has_preedit = has_preedit,
        .preedit_text = model.preedit_text(),
        .preedit_range = preedit_range,
        .preedit_anchor_byte_offset = composition.replacement_range.start_byte,
        .has_submit_text = model.has_submit_text(),
        .focus_clean = model.has_focus() || model.focus_id().empty(),
        .preedit_clean = preedit_clean,
        .byte_counts = text_input_presentation_byte_counts{
            .committed_text_bytes = model.text().size(),
            .display_text_bytes = display_text.size(),
            .preedit_text_bytes = model.preedit_text().size(),
            .selected_text_bytes =
                selection.has_value() ? text_input_presentation_range_byte_count(*selection) : 0,
            .preedit_range_bytes = has_preedit ? text_input_presentation_range_byte_count(preedit_range) : 0,
            .caret_byte_offset = model.caret_byte_offset(),
            .preedit_anchor_byte_offset = composition.replacement_range.start_byte,
        },
        .route_byte_diagnostics = route_byte_diagnostics,
    };
    return snapshot;
}

[[nodiscard]] inline text_input_presentation_snapshot make_text_input_presentation_snapshot(
    const text_input_model& model,
    const input_routing_diagnostics& diagnostics)
{
    return make_text_input_presentation_snapshot(
        model,
        find_text_input_presentation_route_byte_diagnostics(diagnostics));
}

[[nodiscard]] inline text_input_presentation_diff diff_text_input_presentation_snapshots(
    const text_input_presentation_snapshot& before,
    const text_input_presentation_snapshot& after)
{
    text_input_presentation_diff diff{
        .has_focus = diff_text_input_presentation_bool(before.has_focus, after.has_focus),
        .target_id = diff_text_input_presentation_string(before.target_id, after.target_id),
        .committed_text =
            diff_text_input_presentation_string(before.committed_text, after.committed_text),
        .display_text = diff_text_input_presentation_string(before.display_text, after.display_text),
        .caret_byte_offset =
            diff_text_input_presentation_count(before.caret_byte_offset, after.caret_byte_offset),
        .caret_range = diff_text_input_presentation_range(before.caret_range, after.caret_range),
        .has_selection = diff_text_input_presentation_bool(before.has_selection, after.has_selection),
        .selection_range =
            diff_text_input_presentation_range(before.selection_range, after.selection_range),
        .has_preedit = diff_text_input_presentation_bool(before.has_preedit, after.has_preedit),
        .preedit_text = diff_text_input_presentation_string(before.preedit_text, after.preedit_text),
        .preedit_range = diff_text_input_presentation_range(before.preedit_range, after.preedit_range),
        .preedit_anchor_byte_offset = diff_text_input_presentation_count(
            before.preedit_anchor_byte_offset,
            after.preedit_anchor_byte_offset),
        .has_submit_text = diff_text_input_presentation_bool(before.has_submit_text, after.has_submit_text),
        .focus_clean = diff_text_input_presentation_bool(before.focus_clean, after.focus_clean),
        .preedit_clean = diff_text_input_presentation_bool(before.preedit_clean, after.preedit_clean),
        .byte_counts = diff_text_input_presentation_byte_counts(before.byte_counts, after.byte_counts),
        .route_byte_diagnostics = diff_text_input_presentation_route_byte_diagnostics(
            before.route_byte_diagnostics,
            after.route_byte_diagnostics),
    };
    diff.focus_changed = diff.has_focus.changed || diff.target_id.changed;
    diff.target_changed = diff.target_id.changed;
    diff.committed_text_changed = diff.committed_text.changed;
    diff.display_text_changed = diff.display_text.changed;
    diff.caret_changed = diff.caret_byte_offset.changed || diff.caret_range.changed;
    diff.selection_changed = diff.has_selection.changed || diff.selection_range.changed;
    diff.preedit_changed =
        diff.has_preedit.changed
        || diff.preedit_text.changed
        || diff.preedit_range.changed
        || diff.preedit_anchor_byte_offset.changed;
    diff.submit_changed = diff.has_submit_text.changed;
    diff.clean_flags_changed = diff.focus_clean.changed || diff.preedit_clean.changed;
    diff.byte_counts_changed = diff.byte_counts.changed;
    diff.route_byte_diagnostics_changed = diff.route_byte_diagnostics.changed;
    diff.changed =
        diff.focus_changed
        || diff.target_changed
        || diff.committed_text_changed
        || diff.display_text_changed
        || diff.caret_changed
        || diff.selection_changed
        || diff.preedit_changed
        || diff.submit_changed
        || diff.clean_flags_changed
        || diff.byte_counts_changed
        || diff.route_byte_diagnostics_changed;
    return diff;
}

} // namespace quiz_vulkan::input
