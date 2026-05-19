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
    bool meta = false,
    bool alt = false)
{
    return quiz_vulkan::raw_platform_key_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
        .key_code = 0,
        .logical_key = std::move(logical_key),
        .alt = alt,
        .ctrl = ctrl,
        .shift = shift,
        .meta = meta,
        .repeat = repeat,
    };
}

quiz_vulkan::raw_platform_input_event ime(
    quiz_vulkan::raw_platform_ime_phase phase,
    std::int64_t timestamp_ms,
    std::string value = {})
{
    return quiz_vulkan::raw_platform_ime_event{
        .timestamp_ms = timestamp_ms,
        .phase = phase,
        .utf8_text = std::move(value),
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
    require(backspace_policy.keyboard.logical_key == "Backspace",
        "utf8 backspace policy records logical key chord");
    require(backspace_policy.keyboard.intent == keyboard_shortcut_intent::delete_backward,
        "utf8 backspace policy records delete-backward intent");
    require(backspace_policy.keyboard.repeat, "utf8 backspace policy records repeat state");
    require(backspace_policy.keyboard.repeat_policy == keyboard_repeat_policy::allowed,
        "utf8 backspace policy allows repeat deletion");

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
    require(submit_policy.keyboard.intent == keyboard_shortcut_intent::submit,
        "submit policy records submit intent");
    require(submit_policy.keyboard.repeat_policy == keyboard_repeat_policy::not_repeat,
        "submit policy records non-repeat policy");

    require(engine.process_raw_event(key(151, "Enter", true)).empty(), "repeat enter is ignored");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "repeat enter emits one suppressed submit diagnostic");
    const action_route_policy_diagnostic& repeat_submit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_submit_boundary,
        "repeat enter emits submit boundary diagnostic");
    require(!repeat_submit_policy.emits_input_event, "repeat enter diagnostic emits no input event");
    require(repeat_submit_policy.keyboard.intent == keyboard_shortcut_intent::submit,
        "repeat enter diagnostic records submit intent");
    require(repeat_submit_policy.keyboard.repeat, "repeat enter diagnostic records repeat state");
    require(repeat_submit_policy.keyboard.repeat_policy == keyboard_repeat_policy::ignored,
        "repeat enter diagnostic records ignored repeat policy");
}

void test_keyboard_backspace_deletes_composed_hangul_fixture()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    const std::string initial = std::string("A") + utf8(u8"각");
    const std::size_t hangul_bytes = std::string(utf8(u8"각")).size();
    require(engine.process_raw_event(text(200, initial)).size() == 1,
        "hangul fixture initial text commits");

    std::vector<input_event> events = engine.process_raw_event(key(210, "Backspace"));
    require(events.size() == 1, "hangul fixture backspace emits one event");
    const text_event& backspace = require_event<text_event>(events, 0);
    require(backspace.kind == text_event_kind::backspace,
        "hangul fixture emits backspace event");
    require(backspace.target_id == "answer",
        "hangul fixture preserves target id");
    require(engine.text_model().text() == "A",
        "hangul fixture deletes the composed hangul syllable as one utf8 unit");
    require(engine.text_model().caret_byte_offset() == 1,
        "hangul fixture caret lands after ascii prefix");

    const input_routing_diagnostics& diagnostics = engine.routing_diagnostics();
    require(diagnostics.action_routes.size() == 1,
        "hangul fixture emits one backspace route");
    const action_route_policy_diagnostic& policy = require_policy(
        diagnostics,
        0,
        action_route_policy_kind::text_backspace_boundary,
        "hangul fixture emits backspace boundary policy");
    require(policy.emits_input_event, "hangul fixture route emits normalized text event");
    require(policy.event_index == 0, "hangul fixture route points at backspace event");
    require(policy.text_byte_count == hangul_bytes,
        "hangul fixture route records deleted hangul byte count");
    require(policy.text_byte_count_before == initial.size(),
        "hangul fixture route records original byte count");
    require(policy.text_byte_count_after == 1,
        "hangul fixture route records ascii prefix byte count after");
    require_range(policy.caret_before,
        initial.size(),
        initial.size(),
        "hangul fixture route records caret after full composed syllable before");
    require_range(policy.caret_after, 1, 1,
        "hangul fixture route records utf8-safe caret after deletion");
    require(policy.keyboard.intent == keyboard_shortcut_intent::delete_backward,
        "hangul fixture route records delete-backward intent");
    require(policy.keyboard.repeat_policy == keyboard_repeat_policy::not_repeat,
        "hangul fixture route records non-repeat backspace");
    require(diagnostics.summary.routes.text == 1,
        "hangul fixture summary counts text route only");
    require(diagnostics.summary.routes.ime == 0,
        "hangul fixture summary emits no ime route");
    require(diagnostics.summary.routes.pointer == 0,
        "hangul fixture summary emits no pointer route");
    require(diagnostics.summary.routes.wheel == 0,
        "hangul fixture summary emits no wheel route");
    require(!engine.text_model().has_submit_text(),
        "hangul fixture backspace does not submit app/domain action");
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

    const std::string delete_initial = std::string("A") + utf8(u8"한") + "B";
    require(engine.process_raw_event(text(160, delete_initial)).size() == 1,
        "text before delete key commits");
    require(engine.process_raw_event(key(170, "Home")).size() == 1,
        "home before delete moves caret to start");
    events = engine.process_raw_event(key(180, "Delete"));
    require(events.size() == 1, "delete key emits one text edit event");
    const text_event& delete_forward = require_event<text_event>(events, 0);
    require(delete_forward.kind == text_event_kind::delete_forward, "delete key emits delete-forward kind");
    require(delete_forward.target_id == "answer", "delete key preserves target id");
    require(engine.text_model().text() == std::string(utf8(u8"한")) + "B",
        "delete key removes next ascii codepoint");
    const action_route_policy_diagnostic& delete_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_delete_forward_boundary,
        "delete key emits delete-forward boundary policy");
    require(delete_policy.text_byte_count == 1, "delete key records deleted ascii byte count");
    require(delete_policy.keyboard.intent == keyboard_shortcut_intent::delete_forward,
        "delete key records delete-forward intent");
    require(delete_policy.keyboard.repeat_policy == keyboard_repeat_policy::not_repeat,
        "delete key records non-repeat policy");

    events = engine.process_raw_event(key(190, "Delete", true));
    require(events.size() == 1, "repeat delete removes next utf8 codepoint");
    require(require_event<text_event>(events, 0).kind == text_event_kind::delete_forward,
        "repeat delete emits delete-forward kind");
    require(engine.text_model().text() == "B", "repeat delete removes utf8 codepoint on boundary");
    const action_route_policy_diagnostic& repeat_delete_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_delete_forward_boundary,
        "repeat delete emits delete-forward boundary policy");
    require(repeat_delete_policy.text_byte_count == std::string(utf8(u8"한")).size(),
        "repeat delete records deleted utf8 byte count");
    require(repeat_delete_policy.keyboard.repeat, "repeat delete records repeat state");
    require(repeat_delete_policy.keyboard.repeat_policy == keyboard_repeat_policy::allowed,
        "repeat delete records allowed repeat policy");
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
    require(left_policy.keyboard.logical_key == "ArrowLeft", "arrow left policy records logical key");
    require(left_policy.keyboard.intent == keyboard_shortcut_intent::caret_previous,
        "arrow left policy records caret previous intent");
    require(left_policy.keyboard.repeat_policy == keyboard_repeat_policy::not_repeat,
        "arrow left policy records non-repeat policy");

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
    require(shift_left_policy.keyboard.modifiers.shift, "shift arrow left policy records shift modifier");
    require(shift_left_policy.keyboard.intent == keyboard_shortcut_intent::selection_previous,
        "shift arrow left policy records selection previous intent");

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
    require(select_all_policy.keyboard.modifiers.ctrl, "ctrl+a policy records ctrl modifier");
    require(select_all_policy.keyboard.intent == keyboard_shortcut_intent::select_all,
        "ctrl+a policy records select-all intent");
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
    require(engine.routing_diagnostics().normalized_events.empty(),
        "tab diagnostics emit no normalized gesture or wheel summary");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "tab emits one focus traversal diagnostic policy");
    const action_route_policy_diagnostic& next_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::focus_traversal_next,
        "tab emits forward traversal policy");
    require(!next_policy.emits_input_event, "tab traversal policy emits no input event");
    require(next_policy.event_index == 0, "tab traversal policy points at no emitted event");
    require(next_policy.target_id == "answer", "tab traversal policy preserves target id");
    require(next_policy.pointer_decision == pointer_arbitration_decision::none,
        "tab traversal policy has no pointer arbitration");
    require(next_policy.pointer_contact == pointer_contact_kind::unknown,
        "tab traversal policy has no pointer contact semantics");
    require(next_policy.tracked_pointer_count_before == 0,
        "tab traversal policy records no tracked pointers before");
    require(next_policy.tracked_pointer_count_after == 0,
        "tab traversal policy records no tracked pointers after");
    const input_diagnostic_summary& next_summary = engine.routing_diagnostics().summary;
    require(next_summary.normalized_event_count == 0, "tab summary emits no normalized events");
    require(next_summary.routes.focus == 1, "tab summary counts one focus route");
    require(next_summary.routes.text == 0, "tab summary counts no text routes");
    require(next_summary.routes.pointer == 0, "tab summary counts no pointer routes");
    require(next_summary.routes.total == 1, "tab summary counts one total route");
    require(next_summary.pointer_capture_ended_cleanly, "tab summary keeps pointer capture clean");
    require(next_summary.focus_ended_cleanly, "tab summary keeps focus state clean");
    require(next_summary.preedit_ended_cleanly, "tab summary keeps preedit clean");
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
    require(next_policy.keyboard.logical_key == "Tab", "tab traversal policy records logical key");
    require(next_policy.keyboard.intent == keyboard_shortcut_intent::focus_traversal_next,
        "tab traversal policy records next focus intent");
    require(!next_policy.keyboard.modifiers.shift, "tab traversal policy records shift modifier off");
    require(next_policy.keyboard.repeat_policy == keyboard_repeat_policy::not_repeat,
        "tab traversal policy records non-repeat policy");

    require(engine.process_raw_event(key(120, "ArrowLeft", false, raw_platform_key_phase::down, false, true)).size() == 1,
        "focus traversal setup selects trailing character");
    std::optional<text_range> selection = engine.text_model().selection_range();
    require(selection.has_value(), "focus traversal setup exposes selection");
    require_range(*selection, 4, initial.size(), "focus traversal setup selected trailing ascii");

    events = engine.process_raw_event(key(130, "Tab", false, raw_platform_key_phase::down, false, true));
    require(events.empty(), "shift tab emits no app input event from input engine");
    require(engine.has_text_focus(), "shift tab diagnostics do not clear text focus");
    require(engine.text_model().text() == initial, "shift tab diagnostics preserve text");
    require(engine.routing_diagnostics().normalized_events.empty(),
        "shift tab diagnostics emit no normalized gesture or wheel summary");
    selection = engine.text_model().selection_range();
    require(selection.has_value(), "shift tab diagnostics preserve selection");
    require_range(*selection, 4, initial.size(), "shift tab diagnostics preserve selected range");
    const action_route_policy_diagnostic& previous_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::focus_traversal_previous,
        "shift tab emits reverse traversal policy");
    require(!previous_policy.emits_input_event, "shift tab traversal policy emits no input event");
    require(previous_policy.event_index == 0, "shift tab traversal policy points at no emitted event");
    require(previous_policy.pointer_decision == pointer_arbitration_decision::none,
        "shift tab traversal policy has no pointer arbitration");
    require(previous_policy.pointer_contact == pointer_contact_kind::unknown,
        "shift tab traversal policy has no pointer contact semantics");
    require(previous_policy.tracked_pointer_count_before == 0,
        "shift tab traversal policy records no tracked pointers before");
    require(previous_policy.tracked_pointer_count_after == 0,
        "shift tab traversal policy records no tracked pointers after");
    const input_diagnostic_summary& previous_summary = engine.routing_diagnostics().summary;
    require(previous_summary.normalized_event_count == 0, "shift tab summary emits no normalized events");
    require(previous_summary.routes.focus == 1, "shift tab summary counts one focus route");
    require(previous_summary.routes.text == 0, "shift tab summary counts no text routes");
    require(previous_summary.routes.pointer == 0, "shift tab summary counts no pointer routes");
    require(previous_summary.routes.total == 1, "shift tab summary counts one total route");
    require(previous_summary.pointer_capture_ended_cleanly, "shift tab summary keeps pointer capture clean");
    require(previous_summary.focus_ended_cleanly, "shift tab summary keeps focus state clean");
    require(previous_summary.preedit_ended_cleanly, "shift tab summary keeps preedit clean");
    require(previous_policy.had_selection_before, "shift tab traversal policy records prior selection");
    require(previous_policy.has_selection_after, "shift tab traversal policy records retained selection");
    require_range(previous_policy.selection_before, 4, initial.size(),
        "shift tab traversal policy records before selection");
    require_range(previous_policy.selection_after, 4, initial.size(),
        "shift tab traversal policy records after selection");
    require_range(previous_policy.caret_before, 4, 4, "shift tab traversal policy records caret before");
    require_range(previous_policy.caret_after, 4, 4, "shift tab traversal policy records caret after");
    require(previous_policy.keyboard.intent == keyboard_shortcut_intent::focus_traversal_previous,
        "shift tab traversal policy records previous focus intent");
    require(previous_policy.keyboard.modifiers.shift, "shift tab traversal policy records shift modifier");

    events = engine.process_raw_event(key(140, "Tab", true));
    require(events.empty(), "repeat tab emits no app input event from input engine");
    const action_route_policy_diagnostic& repeat_tab_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::focus_traversal_next,
        "repeat tab emits suppressed forward traversal policy");
    require(!repeat_tab_policy.emits_input_event, "repeat tab traversal policy emits no input event");
    require(repeat_tab_policy.keyboard.intent == keyboard_shortcut_intent::focus_traversal_next,
        "repeat tab traversal policy records next focus intent");
    require(repeat_tab_policy.keyboard.repeat, "repeat tab traversal policy records repeat state");
    require(repeat_tab_policy.keyboard.repeat_policy == keyboard_repeat_policy::ignored,
        "repeat tab traversal policy records ignored repeat policy");
}

void test_keyboard_cancel_intent_diagnostics()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    require(engine.process_raw_event(text(100, "draft")).size() == 1,
        "cancel intent setup commits text");

    std::vector<input_event> events = engine.process_raw_event(
        key(110, "Escape", false, raw_platform_key_phase::down, false, false, false, true));
    require(events.size() == 1, "escape emits one semantic-free cancel intent event");
    const text_event& cancel = require_event<text_event>(events, 0);
    require(cancel.kind == text_event_kind::cancel, "escape emits cancel text event kind");
    require(cancel.target_id == "answer", "escape cancel preserves target id");
    require(cancel.utf8_text.empty(), "escape cancel carries no app semantic payload");
    require(engine.text_model().text() == "draft", "escape cancel intent does not mutate text");
    require(engine.has_text_focus(), "escape cancel intent does not clear focus");
    const action_route_policy_diagnostic& cancel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::keyboard_cancel_intent,
        "escape emits keyboard cancel intent policy");
    require(cancel_policy.emits_input_event, "escape cancel policy records emitted input event");
    require(cancel_policy.keyboard.logical_key == "Escape", "escape cancel policy records logical key");
    require(cancel_policy.keyboard.modifiers.alt, "escape cancel policy records alt modifier");
    require(cancel_policy.keyboard.intent == keyboard_shortcut_intent::cancel,
        "escape cancel policy records cancel intent");
    require(cancel_policy.keyboard.repeat_policy == keyboard_repeat_policy::not_repeat,
        "escape cancel policy records non-repeat policy");
    require(cancel_policy.text_byte_count_before == 5, "escape cancel policy records before text count");
    require(cancel_policy.text_byte_count_after == 5, "escape cancel policy records unchanged text count");
    require_range(cancel_policy.caret_before, 5, 5, "escape cancel policy records caret before");
    require_range(cancel_policy.caret_after, 5, 5, "escape cancel policy records caret after");
    const input_diagnostic_summary& cancel_summary = engine.routing_diagnostics().summary;
    require(cancel_summary.normalized_event_count == 0, "escape cancel emits no gesture or wheel summary");
    require(cancel_summary.routes.text == 1, "escape cancel summary counts one text route");
    require(cancel_summary.routes.focus == 0, "escape cancel summary counts no focus route");
    require(cancel_summary.routes.total == 1, "escape cancel summary counts one total route");

    events = engine.process_raw_event(key(120, "Escape", true));
    require(events.empty(), "repeat escape emits no cancel intent event");
    const action_route_policy_diagnostic& repeat_cancel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::keyboard_cancel_intent,
        "repeat escape emits suppressed cancel intent policy");
    require(!repeat_cancel_policy.emits_input_event, "repeat escape policy emits no input event");
    require(repeat_cancel_policy.keyboard.intent == keyboard_shortcut_intent::cancel,
        "repeat escape policy records cancel intent");
    require(repeat_cancel_policy.keyboard.repeat, "repeat escape policy records repeat state");
    require(repeat_cancel_policy.keyboard.repeat_policy == keyboard_repeat_policy::ignored,
        "repeat escape policy records ignored repeat policy");
    require(engine.text_model().text() == "draft", "repeat escape leaves text unchanged");
}

void test_text_input_presentation_snapshot_exposes_read_model()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    text_input_presentation_snapshot snapshot = engine.text_presentation_snapshot();
    require(!snapshot.has_focus, "presentation default has no focus");
    require(snapshot.target_id.empty(), "presentation default target id is empty");
    require(snapshot.committed_text.empty(), "presentation default committed text is empty");
    require(snapshot.display_text.empty(), "presentation default display text is empty");
    require_range(snapshot.caret_range, 0, 0, "presentation default caret range is collapsed");
    require(!snapshot.has_selection, "presentation default has no selection");
    require(!snapshot.has_preedit, "presentation default has no preedit");
    require(snapshot.focus_clean, "presentation default focus is clean");
    require(snapshot.preedit_clean, "presentation default preedit is clean");
    require(!snapshot.route_byte_diagnostics.available,
        "presentation default has no route byte diagnostics");

    engine.focus_text_target("answer");
    const std::string base = std::string("A") + utf8(u8"한") + "B";
    require(engine.process_raw_event(text(100, base)).size() == 1,
        "presentation commit emits text event");
    require(engine.process_raw_event(key(110, "Home")).size() == 1,
        "presentation home emits caret event");
    require(engine.process_raw_event(key(120, "ArrowRight", false, raw_platform_key_phase::down, false, true))
                .size() == 1,
        "presentation shift right emits selection event");

    const std::string preedit = utf8(u8"ㅎ");
    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 130, preedit)).size() == 1,
        "presentation preedit emits ime event");

    snapshot = engine.text_presentation_snapshot();
    require(snapshot.has_focus, "presentation focused snapshot has focus");
    require(snapshot.target_id == "answer", "presentation focused snapshot records target id");
    require(snapshot.committed_text == base, "presentation records committed text");
    require(snapshot.display_text == preedit + utf8(u8"한") + "B",
        "presentation records display text with preedit replacement");
    require(snapshot.caret_byte_offset == 1, "presentation records committed caret byte offset");
    require_range(snapshot.caret_range, preedit.size(), preedit.size(),
        "presentation records display caret range after preedit");
    require(snapshot.has_selection, "presentation records active selection");
    require_range(snapshot.selection_range, 0, 1, "presentation records selection range");
    require(snapshot.has_preedit, "presentation records active preedit");
    require(snapshot.preedit_text == preedit, "presentation records preedit text");
    require_range(snapshot.preedit_range, 0, preedit.size(), "presentation records preedit range");
    require(snapshot.preedit_anchor_byte_offset == 0, "presentation records preedit anchor");
    require(!snapshot.has_submit_text, "presentation has no pending submit before submit");
    require(snapshot.focus_clean, "presentation focused state is clean");
    require(!snapshot.preedit_clean, "presentation active preedit is not clean");
    require(snapshot.byte_counts.committed_text_bytes == base.size(),
        "presentation records committed byte count");
    require(snapshot.byte_counts.display_text_bytes == preedit.size() + std::string(utf8(u8"한")).size() + 1,
        "presentation records display byte count");
    require(snapshot.byte_counts.preedit_text_bytes == preedit.size(),
        "presentation records preedit byte count");
    require(snapshot.byte_counts.selected_text_bytes == 1,
        "presentation records selected byte count");
    require(snapshot.byte_counts.preedit_range_bytes == preedit.size(),
        "presentation records preedit range byte count");
    require(snapshot.byte_counts.caret_byte_offset == 1,
        "presentation byte diagnostics records committed caret offset");
    require(snapshot.byte_counts.preedit_anchor_byte_offset == 0,
        "presentation byte diagnostics records preedit anchor");
    require(snapshot.route_byte_diagnostics.available,
        "presentation records engine route byte diagnostics");
    require(snapshot.route_byte_diagnostics.text_byte_count == preedit.size(),
        "presentation records route text byte count");
    require(snapshot.route_byte_diagnostics.text_byte_count_before == base.size(),
        "presentation records route before byte count");
    require(snapshot.route_byte_diagnostics.text_byte_count_after == base.size(),
        "presentation records route after byte count");
    require(snapshot.route_byte_diagnostics.text_byte_delta == 0,
        "presentation records route byte delta");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::cancel, 140)).size() == 1,
        "presentation cancel emits ime cancel event");
    require(engine.process_raw_event(key(150, "Enter")).size() == 1,
        "presentation submit emits submit event");
    snapshot = engine.text_presentation_snapshot();
    require(snapshot.has_submit_text, "presentation records pending submit text");
    require(snapshot.committed_text.empty(), "presentation submit clears committed text");
    require(snapshot.display_text.empty(), "presentation submit clears display text");
    require(!snapshot.has_preedit, "presentation submit clears preedit");
    require(snapshot.preedit_clean, "presentation submit preedit state is clean");
    require(snapshot.byte_counts.committed_text_bytes == 0,
        "presentation submit records empty committed byte count");
    require(snapshot.route_byte_diagnostics.available,
        "presentation submit keeps route byte diagnostics");
    require(snapshot.route_byte_diagnostics.text_byte_count_before == base.size(),
        "presentation submit route records before byte count");
    require(snapshot.route_byte_diagnostics.text_byte_count_after == 0,
        "presentation submit route records after byte count");
    require(snapshot.route_byte_diagnostics.text_byte_delta == -static_cast<std::int64_t>(base.size()),
        "presentation submit route records negative byte delta");

    text_input_presentation_snapshot direct_snapshot =
        make_text_input_presentation_snapshot(engine.text_model());
    require(!direct_snapshot.route_byte_diagnostics.available,
        "presentation direct text model snapshot has no engine route diagnostics");
    require(direct_snapshot.has_submit_text,
        "presentation direct text model snapshot preserves submit availability");
}

void test_text_input_presentation_diff_reports_review_deltas()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    const text_input_presentation_snapshot initial_snapshot = engine.text_presentation_snapshot();

    engine.focus_text_target("answer");
    const std::string base = std::string("A") + utf8(u8"한") + "B";
    const std::string preedit = utf8(u8"ㅎ");
    require(engine.process_raw_event(text(100, base)).size() == 1,
        "presentation diff commit emits text event");
    require(engine.process_raw_event(key(110, "Home")).size() == 1,
        "presentation diff home emits caret event");
    require(engine.process_raw_event(key(120, "ArrowRight", false, raw_platform_key_phase::down, false, true))
                .size() == 1,
        "presentation diff shift right emits selection event");
    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 130, preedit)).size() == 1,
        "presentation diff preedit emits ime event");

    const text_input_presentation_snapshot preedit_snapshot = engine.text_presentation_snapshot();
    const text_input_presentation_diff preedit_diff =
        diff_text_input_presentation_snapshots(initial_snapshot, preedit_snapshot);
    require(preedit_diff.changed, "presentation diff reports changed snapshot");
    require(preedit_diff.focus_changed, "presentation diff reports focus change");
    require(!preedit_diff.has_focus.before_value && preedit_diff.has_focus.after_value,
        "presentation diff records focus bool values");
    require(preedit_diff.target_changed, "presentation diff reports target id change");
    require(preedit_diff.target_id.after_value == "answer",
        "presentation diff records target id after value");
    require(preedit_diff.committed_text_changed, "presentation diff reports committed text change");
    require(preedit_diff.committed_text.byte_delta == static_cast<std::int64_t>(base.size()),
        "presentation diff records committed text byte delta");
    require(preedit_diff.display_text_changed, "presentation diff reports display text change");
    require(preedit_diff.display_text.after_value == preedit + utf8(u8"한") + "B",
        "presentation diff records display text after value");
    require(preedit_diff.caret_changed, "presentation diff reports caret change");
    require(preedit_diff.caret_byte_offset.after_count == 1,
        "presentation diff records committed caret byte offset");
    require_range(preedit_diff.caret_range.after_range,
        preedit.size(),
        preedit.size(),
        "presentation diff records display caret range");
    require(preedit_diff.selection_changed, "presentation diff reports selection change");
    require(preedit_diff.has_selection.changed && preedit_diff.has_selection.after_value,
        "presentation diff records active selection flag");
    require_range(preedit_diff.selection_range.after_range, 0, 1,
        "presentation diff records selection range");
    require(preedit_diff.preedit_changed, "presentation diff reports preedit change");
    require(preedit_diff.has_preedit.changed && preedit_diff.has_preedit.after_value,
        "presentation diff records active preedit flag");
    require(preedit_diff.preedit_text.after_value == preedit,
        "presentation diff records preedit text after value");
    require(preedit_diff.preedit_text.byte_delta == static_cast<std::int64_t>(preedit.size()),
        "presentation diff records preedit byte delta");
    require_range(preedit_diff.preedit_range.after_range,
        0,
        preedit.size(),
        "presentation diff records preedit range");
    require(!preedit_diff.submit_changed, "presentation diff leaves submit unchanged before submit");
    require(preedit_diff.clean_flags_changed, "presentation diff reports clean flag change");
    require(preedit_diff.preedit_clean.before_value && !preedit_diff.preedit_clean.after_value,
        "presentation diff records preedit clean flag values");
    require(preedit_diff.byte_counts_changed, "presentation diff reports byte count change");
    require(preedit_diff.byte_counts.committed_text_bytes.after_count == base.size(),
        "presentation diff records committed byte count after value");
    require(preedit_diff.byte_counts.display_text_bytes.after_count
                == preedit.size() + std::string(utf8(u8"한")).size() + 1,
        "presentation diff records display byte count after value");
    require(preedit_diff.byte_counts.preedit_text_bytes.after_count == preedit.size(),
        "presentation diff records preedit byte count after value");
    require(preedit_diff.byte_counts.selected_text_bytes.after_count == 1,
        "presentation diff records selected byte count after value");
    require(preedit_diff.byte_counts.caret_byte_offset.after_count == 1,
        "presentation diff records caret byte count after value");
    require(preedit_diff.route_byte_diagnostics_changed,
        "presentation diff reports route diagnostics change");
    require(preedit_diff.route_byte_diagnostics.available.changed
            && preedit_diff.route_byte_diagnostics.available.after_value,
        "presentation diff records route diagnostics availability");
    require(preedit_diff.route_byte_diagnostics.text_byte_count.after_count == preedit.size(),
        "presentation diff records route byte count after value");
    require(preedit_diff.route_byte_diagnostics.text_byte_delta.after_value == 0,
        "presentation diff records route byte delta after value");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::cancel, 140)).size() == 1,
        "presentation diff cancel emits ime event");
    require(engine.process_raw_event(key(150, "Enter")).size() == 1,
        "presentation diff submit emits text event");

    const text_input_presentation_snapshot submit_snapshot = engine.text_presentation_snapshot();
    const text_input_presentation_diff submit_diff =
        diff_text_input_presentation_snapshots(preedit_snapshot, submit_snapshot);
    require(submit_diff.changed, "presentation submit diff reports changed snapshot");
    require(submit_diff.submit_changed, "presentation submit diff reports submit availability change");
    require(!submit_diff.has_submit_text.before_value && submit_diff.has_submit_text.after_value,
        "presentation submit diff records submit availability values");
    require(submit_diff.committed_text_changed, "presentation submit diff reports committed clear");
    require(submit_diff.committed_text.after_value.empty(),
        "presentation submit diff records cleared committed text");
    require(submit_diff.committed_text.byte_delta == -static_cast<std::int64_t>(base.size()),
        "presentation submit diff records committed negative byte delta");
    require(submit_diff.display_text_changed, "presentation submit diff reports display clear");
    require(submit_diff.display_text.after_value.empty(),
        "presentation submit diff records cleared display text");
    require(submit_diff.preedit_changed, "presentation submit diff reports preedit clear");
    require(submit_diff.has_preedit.before_value && !submit_diff.has_preedit.after_value,
        "presentation submit diff records preedit flag clear");
    require(submit_diff.clean_flags_changed, "presentation submit diff reports clean flag change");
    require(!submit_diff.preedit_clean.before_value && submit_diff.preedit_clean.after_value,
        "presentation submit diff records clean preedit after submit");
    require(submit_diff.byte_counts.committed_text_bytes.after_count == 0,
        "presentation submit diff records empty committed byte count");
    require(submit_diff.byte_counts.display_text_bytes.after_count == 0,
        "presentation submit diff records empty display byte count");
    require(submit_diff.byte_counts.preedit_text_bytes.after_count == 0,
        "presentation submit diff records empty preedit byte count");
    require(submit_diff.route_byte_diagnostics.text_byte_delta.changed,
        "presentation submit diff reports route byte delta change");
    require(submit_diff.route_byte_diagnostics.text_byte_delta.after_value
                == -static_cast<std::int64_t>(base.size()),
        "presentation submit diff records submit route negative byte delta");

    const text_input_presentation_diff stable_diff =
        diff_text_input_presentation_snapshots(submit_snapshot, submit_snapshot);
    require(!stable_diff.changed, "presentation diff self comparison is stable");
}

} // namespace

int main()
{
    test_text_key_flow();
    test_keyboard_backspace_deletes_composed_hangul_fixture();
    test_key_code_fallback_edges();
    test_text_keyboard_navigation_and_selection();
    test_keyboard_focus_traversal_diagnostics();
    test_keyboard_cancel_intent_diagnostics();
    test_text_input_presentation_snapshot_exposes_read_model();
    test_text_input_presentation_diff_reports_review_deltas();

    std::cout << "input_engine_keyboard_text_tests passed\n";
    return 0;
}
