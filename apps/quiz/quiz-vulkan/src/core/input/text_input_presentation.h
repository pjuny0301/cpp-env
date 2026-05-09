#pragma once

#include "core/input/input_routing_diagnostics.h"
#include "core/input/text_input_model.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

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

} // namespace quiz_vulkan::input
