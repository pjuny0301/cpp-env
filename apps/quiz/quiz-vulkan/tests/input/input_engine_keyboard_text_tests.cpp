#include "core/input/input_engine.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

std::string utf8(const char8_t* text)
{
    return reinterpret_cast<const char*>(text);
}

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "input_engine_keyboard_text_tests failed: " << message << '\n';
    std::exit(1);
}

void require_range(
    quiz_vulkan::input::text_range range,
    std::size_t start_byte,
    std::size_t end_byte,
    const char* message)
{
    require(range.start_byte == start_byte, message);
    require(range.end_byte == end_byte, message);
}

const quiz_vulkan::input::action_route_policy_diagnostic& require_policy(
    const quiz_vulkan::input::input_routing_diagnostics& diagnostics,
    std::size_t index,
    quiz_vulkan::input::action_route_policy_kind kind,
    const char* message)
{
    require(index < diagnostics.action_routes.size(), message);
    const quiz_vulkan::input::action_route_policy_diagnostic& policy = diagnostics.action_routes[index];
    require(policy.kind == kind, message);
    return policy;
}

template <typename T>
const T& require_event(const std::vector<quiz_vulkan::input::input_event>& events, std::size_t index)
{
    require(index < events.size(), "event index exists");
    require(std::holds_alternative<T>(events[index]), "event has expected type");
    return std::get<T>(events[index]);
}

quiz_vulkan::raw_platform_input_event text(std::int64_t timestamp_ms, std::string value)
{
    return quiz_vulkan::raw_platform_text_event{
        .timestamp_ms = timestamp_ms,
        .utf8_text = std::move(value),
    };
}

quiz_vulkan::raw_platform_input_event key(
    std::int64_t timestamp_ms,
    std::string logical_key,
    bool repeat = false,
    quiz_vulkan::raw_platform_key_phase phase = quiz_vulkan::raw_platform_key_phase::down,
    bool ctrl = false,
    bool shift = false,
    bool meta = false)
{
    return quiz_vulkan::raw_platform_key_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
        .key_code = 0,
        .logical_key = std::move(logical_key),
        .ctrl = ctrl,
        .shift = shift,
        .meta = meta,
        .repeat = repeat,
    };
}

quiz_vulkan::raw_platform_input_event key_code(
    std::int64_t timestamp_ms,
    std::int32_t code,
    quiz_vulkan::raw_platform_key_phase phase = quiz_vulkan::raw_platform_key_phase::down)
{
    return quiz_vulkan::raw_platform_key_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
        .key_code = code,
        .logical_key = {},
    };
}

void test_text_key_flow()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(text(100, "ignored")).empty(), "unfocused text is ignored");

    engine.focus_text_target("answer");
    std::vector<input_event> events = engine.process_raw_event(text(110, utf8(u8"한")));
    require(events.size() == 1, "focused text commit emits one event");
    const text_event& commit = require_event<text_event>(events, 0);
    require(commit.kind == text_event_kind::commit, "text event is commit");
    require(commit.target_id == "answer", "text target is preserved");
    require(commit.utf8_text == utf8(u8"한"), "utf8 commit text is preserved");
    require(engine.text_model().text() == utf8(u8"한"), "text model stores committed utf8");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "utf8 text commit emits one edit boundary policy");
    const action_route_policy_diagnostic& text_commit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_commit_boundary,
        "utf8 text commit policy is emitted");
    require(text_commit_policy.text_byte_count == std::string(utf8(u8"한")).size(),
        "utf8 text commit policy records committed byte count");
    require(text_commit_policy.text_byte_count_before == 0, "utf8 text commit policy records empty before text");
    require(text_commit_policy.text_byte_count_after == std::string(utf8(u8"한")).size(),
        "utf8 text commit policy records after text byte count");
    require_range(text_commit_policy.caret_before, 0, 0, "utf8 text commit policy records caret before commit");
    require_range(text_commit_policy.caret_after,
        std::string(utf8(u8"한")).size(),
        std::string(utf8(u8"한")).size(),
        "utf8 text commit policy records caret after commit");

    require(engine.process_raw_event(key(120, "Backspace", false, raw_platform_key_phase::up)).empty(),
        "backspace keyup is ignored");
    events = engine.process_raw_event(key(130, "Backspace", true));
    require(events.size() == 1, "repeat backspace deletes text");
    const text_event& backspace = require_event<text_event>(events, 0);
    require(backspace.kind == text_event_kind::backspace, "backspace event is emitted");
    require(engine.text_model().text().empty(), "backspace removes utf8 codepoint");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "utf8 backspace emits one edit boundary policy");
    const action_route_policy_diagnostic& backspace_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_backspace_boundary,
        "utf8 backspace policy is emitted");
    require(backspace_policy.text_byte_count == std::string(utf8(u8"한")).size(),
        "utf8 backspace policy records deleted codepoint byte count");
    require(backspace_policy.text_byte_count_before == std::string(utf8(u8"한")).size(),
        "utf8 backspace policy records before byte count");
    require(backspace_policy.text_byte_count_after == 0, "utf8 backspace policy records after byte count");
    require_range(backspace_policy.caret_before,
        std::string(utf8(u8"한")).size(),
        std::string(utf8(u8"한")).size(),
        "utf8 backspace policy records caret before delete");
    require_range(backspace_policy.caret_after, 0, 0, "utf8 backspace policy records caret after delete");

    require(engine.process_raw_event(text(140, "ok")).size() == 1, "second text commit succeeds");
    events = engine.process_raw_event(key(150, "Enter"));
    require(events.size() == 1, "enter submits once");
    const text_event& submit = require_event<text_event>(events, 0);
    require(submit.kind == text_event_kind::submit, "submit event is emitted");
    require(submit.utf8_text == "ok", "submit carries committed buffer");
    require(engine.text_model().text().empty(), "submit clears editing buffer");
    require(engine.text_model().has_submit_text(), "text model retains consumable submit text");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "submit emits one action route policy");
    const action_route_policy_diagnostic& submit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_submit_boundary,
        "submit action route policy is emitted");
    require(submit_policy.emits_input_event, "submit policy marks emitted event");
    require(submit_policy.event_index == 0, "submit policy points at submit event index");
    require(submit_policy.target_id == "answer", "submit policy preserves target id");
    require(submit_policy.text_byte_count == 2, "submit policy records submitted byte count before clear");
    require(submit_policy.text_byte_count_before == 2, "submit policy records before byte count");
    require(submit_policy.text_byte_count_after == 0, "submit policy records cleared after byte count");
    require_range(submit_policy.caret_before, 2, 2, "submit policy records caret before submit");
    require_range(submit_policy.caret_after, 0, 0, "submit policy records caret after submit");

    require(engine.process_raw_event(key(151, "Enter", true)).empty(), "repeat enter is ignored");
    require(engine.routing_diagnostics().action_routes.empty(), "repeat enter emits no submit policy");
}

void test_key_code_fallback_edges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(key_code(100, 8)).empty(), "unfocused key-code backspace is ignored");

    engine.focus_text_target("answer");
    require(engine.process_raw_event(text(110, utf8(u8"한"))).size() == 1, "text before key-code backspace commits");
    std::vector<input_event> events = engine.process_raw_event(key_code(120, 8));
    require(events.size() == 1, "key-code backspace emits one text event");
    const text_event& backspace = require_event<text_event>(events, 0);
    require(backspace.kind == text_event_kind::backspace, "key-code backspace emits backspace kind");
    require(backspace.target_id == "answer", "key-code backspace preserves target id");
    require(engine.text_model().text().empty(), "key-code backspace removes utf8 codepoint");

    require(engine.process_raw_event(text(130, "ok")).size() == 1, "text before key-code enter commits");
    require(engine.process_raw_event(key_code(140, 13, raw_platform_key_phase::up)).empty(),
        "key-code enter keyup is ignored");
    events = engine.process_raw_event(key_code(150, 13));
    require(events.size() == 1, "key-code enter emits one submit event");
    const text_event& submit = require_event<text_event>(events, 0);
    require(submit.kind == text_event_kind::submit, "key-code enter emits submit kind");
    require(submit.target_id == "answer", "key-code enter preserves target id");
    require(submit.utf8_text == "ok", "key-code enter submits committed text");
}

void test_text_keyboard_navigation_and_selection()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    require(engine.process_raw_event(key(90, "ArrowLeft")).empty(), "unfocused arrow left is ignored");

    engine.focus_text_target("answer");
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    require(engine.process_raw_event(text(100, initial)).size() == 1, "text before navigation commits");
    require(engine.text_model().caret_byte_offset() == initial.size(), "caret starts at committed end");

    require(engine.process_raw_event(key(110, "ArrowLeft", false, raw_platform_key_phase::up)).empty(),
        "arrow keyup is ignored");
    std::vector<input_event> events = engine.process_raw_event(key(120, "ArrowLeft"));
    require(events.size() == 1, "arrow left emits one caret event");
    const text_event& left = require_event<text_event>(events, 0);
    require(left.kind == text_event_kind::caret_moved, "arrow left emits caret moved kind");
    require(left.target_id == "answer", "arrow left preserves target id");
    require(engine.text_model().caret_byte_offset() == 4, "arrow left moves over trailing ascii");
    require(!engine.text_model().selection_range().has_value(), "plain arrow left leaves no selection");
    const action_route_policy_diagnostic& left_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::caret_moved,
        "arrow left emits caret policy");
    require_range(left_policy.caret_before, initial.size(), initial.size(),
        "arrow left policy records caret before");
    require_range(left_policy.caret_after, 4, 4, "arrow left policy records caret after");
    require(!left_policy.had_selection_before, "arrow left policy records no prior selection");
    require(!left_policy.has_selection_after, "arrow left policy records no resulting selection");

    events = engine.process_raw_event(key(130, "ArrowLeft"));
    require(events.size() == 1, "second arrow left emits one caret event");
    require(require_event<text_event>(events, 0).kind == text_event_kind::caret_moved,
        "second arrow left emits caret moved kind");
    require(engine.text_model().caret_byte_offset() == 1, "second arrow left moves over utf8 codepoint");

    events = engine.process_raw_event(key(140, "ArrowRight"));
    require(events.size() == 1, "arrow right emits one caret event");
    require(require_event<text_event>(events, 0).kind == text_event_kind::caret_moved,
        "arrow right emits caret moved kind");
    require(engine.text_model().caret_byte_offset() == 4, "arrow right moves over utf8 codepoint");

    events = engine.process_raw_event(key(150, "Home"));
    require(events.size() == 1, "home emits one caret event");
    require(require_event<text_event>(events, 0).kind == text_event_kind::caret_moved, "home emits caret moved kind");
    require(engine.text_model().caret_byte_offset() == 0, "home moves caret to start");
    events = engine.process_raw_event(key(151, "Home"));
    require(events.empty(), "home at start emits no event");
    const action_route_policy_diagnostic& home_edge_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::caret_moved,
        "home at start emits caret edge policy");
    require(!home_edge_policy.emits_input_event, "home at start policy emits no input event");
    require_range(home_edge_policy.caret_before, 0, 0, "home at start policy records caret before");
    require_range(home_edge_policy.caret_after, 0, 0, "home at start policy records caret after");
    require(home_edge_policy.text_byte_count_before == initial.size(),
        "home at start policy records before text byte count");
    require(home_edge_policy.text_byte_count_after == initial.size(),
        "home at start policy records after text byte count");

    events = engine.process_raw_event(key(160, "End"));
    require(events.size() == 1, "end emits one caret event");
    require(require_event<text_event>(events, 0).kind == text_event_kind::caret_moved, "end emits caret moved kind");
    require(engine.text_model().caret_byte_offset() == initial.size(), "end moves caret to committed end");
    events = engine.process_raw_event(key(161, "End"));
    require(events.empty(), "end at end emits no event");
    const action_route_policy_diagnostic& end_edge_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::caret_moved,
        "end at end emits caret edge policy");
    require(!end_edge_policy.emits_input_event, "end at end policy emits no input event");
    require_range(end_edge_policy.caret_before, initial.size(), initial.size(),
        "end at end policy records caret before");
    require_range(end_edge_policy.caret_after, initial.size(), initial.size(),
        "end at end policy records caret after");

    events = engine.process_raw_event(key(170, "ArrowLeft", false, raw_platform_key_phase::down, false, true));
    require(events.size() == 1, "shift arrow left emits one selection event");
    const text_event& shift_left = require_event<text_event>(events, 0);
    require(shift_left.kind == text_event_kind::selection_changed, "shift arrow left emits selection changed kind");
    auto selection = engine.text_model().selection_range();
    require(selection.has_value(), "shift arrow left exposes selection");
    require_range(*selection, 4, initial.size(), "shift arrow left selects trailing ascii");
    require(engine.text_model().caret_byte_offset() == 4, "shift arrow left places active caret at selection start");
    const action_route_policy_diagnostic& shift_left_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::selection_changed,
        "shift arrow left emits selection policy");
    require_range(shift_left_policy.caret_before, initial.size(), initial.size(),
        "shift arrow left policy records caret before");
    require_range(shift_left_policy.caret_after, 4, 4, "shift arrow left policy records caret after");
    require(!shift_left_policy.had_selection_before, "shift arrow left policy records no prior selection");
    require(shift_left_policy.has_selection_after, "shift arrow left policy records resulting selection");
    require_range(shift_left_policy.selection_after, 4, initial.size(),
        "shift arrow left policy records selected range");

    events = engine.process_raw_event(key(180, "ArrowLeft", false, raw_platform_key_phase::down, false, true));
    require(events.size() == 1, "second shift arrow left emits one selection event");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "second shift arrow left keeps selection");
    require_range(*selection, 1, initial.size(), "second shift arrow left extends over utf8 codepoint");
    require(engine.text_model().caret_byte_offset() == 1, "second shift arrow left updates active caret");

    events = engine.process_raw_event(key(190, "ArrowRight", false, raw_platform_key_phase::down, false, true));
    require(events.size() == 1, "shift arrow right shrinks selection");
    require(require_event<text_event>(events, 0).kind == text_event_kind::selection_changed,
        "shift arrow right emits selection changed kind");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "shift arrow right keeps selection while not collapsed");
    require_range(*selection, 4, initial.size(), "shift arrow right shrinks over utf8 codepoint");
    require(engine.text_model().caret_byte_offset() == 4, "shift arrow right updates active caret");

    events = engine.process_raw_event(key(200, "ArrowRight"));
    require(events.size() == 1, "plain arrow right collapses selection");
    require(require_event<text_event>(events, 0).kind == text_event_kind::caret_moved,
        "plain arrow collapse emits caret moved kind");
    require(!engine.text_model().selection_range().has_value(), "plain arrow right clears selection");
    require(engine.text_model().caret_byte_offset() == initial.size(), "plain arrow right collapses to selection end");
    const action_route_policy_diagnostic& collapse_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::caret_moved,
        "plain arrow collapse emits caret policy");
    require(collapse_policy.had_selection_before, "plain arrow collapse policy records prior selection");
    require(!collapse_policy.has_selection_after, "plain arrow collapse policy clears selection");
    require_range(collapse_policy.selection_before, 4, initial.size(),
        "plain arrow collapse policy records prior selection range");
    require_range(collapse_policy.caret_after, initial.size(), initial.size(),
        "plain arrow collapse policy records collapsed caret");

    events = engine.process_raw_event(key(210, "a", false, raw_platform_key_phase::down, true));
    require(events.size() == 1, "ctrl+a emits one selection event");
    const text_event& ctrl_a = require_event<text_event>(events, 0);
    require(ctrl_a.kind == text_event_kind::selection_changed, "ctrl+a emits selection changed kind");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "ctrl+a exposes selection");
    require_range(*selection, 0, initial.size(), "ctrl+a selects all committed text");
    const action_route_policy_diagnostic& select_all_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::selection_changed,
        "ctrl+a emits selection policy");
    require(select_all_policy.has_selection_after, "ctrl+a policy records resulting selection");
    require_range(select_all_policy.selection_after, 0, initial.size(), "ctrl+a policy records full selection");
    events = engine.process_raw_event(key(211, "a", false, raw_platform_key_phase::down, true));
    require(events.empty(), "repeat select all without state change emits no event");
    const action_route_policy_diagnostic& select_all_edge_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::selection_changed,
        "repeat select all emits selection edge policy");
    require(!select_all_edge_policy.emits_input_event, "repeat select all policy emits no input event");
    require(select_all_edge_policy.had_selection_before, "repeat select all policy records prior selection");
    require(select_all_edge_policy.has_selection_after, "repeat select all policy records retained selection");
    require_range(select_all_edge_policy.selection_before, 0, initial.size(),
        "repeat select all policy records before range");
    require_range(select_all_edge_policy.selection_after, 0, initial.size(),
        "repeat select all policy records after range");

    require(engine.process_raw_event(key(220, "Home")).size() == 1, "home clears selection before meta+a");
    events = engine.process_raw_event(key(230, "A", false, raw_platform_key_phase::down, false, false, true));
    require(events.size() == 1, "meta+a emits one selection event");
    require(require_event<text_event>(events, 0).kind == text_event_kind::selection_changed,
        "meta+a emits selection changed kind");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "meta+a exposes selection");
    require_range(*selection, 0, initial.size(), "meta+a selects all committed text");
}

void test_keyboard_focus_traversal_diagnostics()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    require(engine.process_raw_event(text(100, initial)).size() == 1,
        "focus traversal initial text commits");

    std::vector<input_event> events = engine.process_raw_event(key(110, "Tab"));
    require(events.empty(), "tab emits no app input event from input engine");
    require(engine.has_text_focus(), "tab diagnostics do not clear text focus");
    require(engine.text_focus_id() == "answer", "tab diagnostics preserve focus id");
    require(engine.text_model().text() == initial, "tab diagnostics preserve text");
    require(engine.text_model().caret_byte_offset() == initial.size(), "tab diagnostics preserve caret");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "tab emits one focus traversal diagnostic policy");
    const action_route_policy_diagnostic& next_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::focus_traversal_next,
        "tab emits forward traversal policy");
    require(!next_policy.emits_input_event, "tab traversal policy emits no input event");
    require(next_policy.target_id == "answer", "tab traversal policy preserves target id");
    require(next_policy.text_byte_count_before == initial.size(),
        "tab traversal policy records before text byte count");
    require(next_policy.text_byte_count_after == initial.size(),
        "tab traversal policy records after text byte count");
    require_range(next_policy.caret_before, initial.size(), initial.size(),
        "tab traversal policy records caret before");
    require_range(next_policy.caret_after, initial.size(), initial.size(),
        "tab traversal policy records caret after");
    require(!next_policy.had_selection_before, "tab traversal policy records no prior selection");
    require(!next_policy.has_selection_after, "tab traversal policy records no resulting selection");

    require(engine.process_raw_event(key(120, "ArrowLeft", false, raw_platform_key_phase::down, false, true)).size() == 1,
        "focus traversal setup selects trailing character");
    std::optional<text_range> selection = engine.text_model().selection_range();
    require(selection.has_value(), "focus traversal setup exposes selection");
    require_range(*selection, 4, initial.size(), "focus traversal setup selected trailing ascii");

    events = engine.process_raw_event(key(130, "Tab", false, raw_platform_key_phase::down, false, true));
    require(events.empty(), "shift tab emits no app input event from input engine");
    require(engine.has_text_focus(), "shift tab diagnostics do not clear text focus");
    require(engine.text_model().text() == initial, "shift tab diagnostics preserve text");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "shift tab diagnostics preserve selection");
    require_range(*selection, 4, initial.size(), "shift tab diagnostics preserve selected range");
    const action_route_policy_diagnostic& previous_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::focus_traversal_previous,
        "shift tab emits reverse traversal policy");
    require(!previous_policy.emits_input_event, "shift tab traversal policy emits no input event");
    require(previous_policy.had_selection_before, "shift tab traversal policy records prior selection");
    require(previous_policy.has_selection_after, "shift tab traversal policy records retained selection");
    require_range(previous_policy.selection_before, 4, initial.size(),
        "shift tab traversal policy records before selection");
    require_range(previous_policy.selection_after, 4, initial.size(),
        "shift tab traversal policy records after selection");
    require_range(previous_policy.caret_before, 4, 4, "shift tab traversal policy records caret before");
    require_range(previous_policy.caret_after, 4, 4, "shift tab traversal policy records caret after");
}

} // namespace

int main()
{
    test_text_key_flow();
    test_key_code_fallback_edges();
    test_text_keyboard_navigation_and_selection();
    test_keyboard_focus_traversal_diagnostics();

    std::cout << "input_engine_keyboard_text_tests passed\n";
    return 0;
}
