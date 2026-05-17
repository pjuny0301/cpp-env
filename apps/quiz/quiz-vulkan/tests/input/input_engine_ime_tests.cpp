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

    std::cerr << "input_engine_ime_tests failed: " << message << '\n';
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

void test_keyboard_navigation_diagnostics_preserve_ime_composition()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    const std::string preedit_text = utf8(u8"ㅎ");
    const std::string committed_text = utf8(u8"한");
    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, preedit_text)).size() == 1,
        "navigation ime setup starts preedit");
    require(engine.text_model().ime_composition().active, "navigation ime setup has active composition");

    std::vector<input_event> events = engine.process_raw_event(key(110, "ArrowLeft"));
    require(events.empty(), "arrow left during ime emits no text event");
    require(engine.text_model().text().empty(), "arrow left during ime preserves committed text");
    require(engine.text_model().display_text() == preedit_text, "arrow left during ime preserves display preedit");
    require(!engine.text_model().selection_range().has_value(), "arrow left during ime creates no selection");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "arrow left during ime emits one navigation diagnostic policy");
    const action_route_policy_diagnostic& caret_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::caret_moved,
        "arrow left during ime emits caret diagnostic policy");
    require(!caret_policy.emits_input_event, "arrow left during ime policy emits no input event");
    require(caret_policy.composition.active, "arrow left during ime policy carries composition");
    require(caret_policy.composition.preedit_text == preedit_text,
        "arrow left during ime policy carries preedit text");
    require_range(caret_policy.caret_before, preedit_text.size(), preedit_text.size(),
        "arrow left during ime policy records display caret before");
    require_range(caret_policy.caret_after, preedit_text.size(), preedit_text.size(),
        "arrow left during ime policy records display caret after");

    events = engine.process_raw_event(key(120, "ArrowRight", false, raw_platform_key_phase::down, false, true));
    require(events.empty(), "shift arrow during ime emits no selection event");
    require(engine.text_model().display_text() == preedit_text, "shift arrow during ime preserves display preedit");
    require(!engine.text_model().selection_range().has_value(), "shift arrow during ime creates no selection");
    const action_route_policy_diagnostic& selection_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::selection_changed,
        "shift arrow during ime emits selection diagnostic policy");
    require(!selection_policy.emits_input_event, "shift arrow during ime policy emits no input event");
    require(selection_policy.composition.active, "shift arrow during ime policy carries composition");
    require(selection_policy.composition.preedit_text == preedit_text,
        "shift arrow during ime policy carries preedit text");

    events = engine.process_raw_event(key(130, "Tab"));
    require(events.empty(), "tab during ime emits no focus traversal event");
    require(engine.has_text_focus(), "tab during ime preserves focus");
    require(engine.text_model().display_text() == preedit_text, "tab during ime preserves display preedit");
    const action_route_policy_diagnostic& traversal_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::focus_traversal_next,
        "tab during ime emits traversal diagnostic policy");
    require(!traversal_policy.emits_input_event, "tab during ime policy emits no input event");
    require(traversal_policy.composition.active, "tab during ime policy carries composition");
    require(traversal_policy.composition.preedit_text == preedit_text,
        "tab during ime policy carries preedit text");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::composition_end, 140, committed_text));
    require(events.size() == 1, "ime commit succeeds after suppressed navigation");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "ime commit after navigation emits commit kind");
    require(engine.text_model().text() == committed_text, "ime commit after navigation updates text");
    require(!engine.text_model().ime_composition().active, "ime commit after navigation clears composition");
}

void test_text_edit_boundary_diagnostics_replace_utf8_selection()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    const std::string initial = std::string("A") + utf8(u8"한") + "B";
    require(engine.process_raw_event(text(100, initial)).size() == 1,
        "selection replacement initial text commits");

    require(engine.process_raw_event(key(110, "Home")).size() == 1,
        "selection replacement home moves caret");
    require(engine.process_raw_event(key(120, "ArrowRight")).size() == 1,
        "selection replacement arrow right moves after ascii");
    require(engine.process_raw_event(key(130, "ArrowRight", false, raw_platform_key_phase::down, false, true)).size() == 1,
        "selection replacement shift arrow selects utf8 codepoint");
    std::optional<text_range> selection = engine.text_model().selection_range();
    require(selection.has_value(), "selection replacement has selected utf8 codepoint");
    require_range(*selection, 1, 1 + std::string(utf8(u8"한")).size(),
        "selection replacement selected range covers full utf8 codepoint");

    std::vector<input_event> events = engine.process_raw_event(text(140, "Z"));
    require(events.size() == 1, "selection replacement text commit emits one event");
    const text_event& commit = require_event<text_event>(events, 0);
    require(commit.kind == text_event_kind::commit, "selection replacement emits commit kind");
    require(engine.text_model().text() == "AZB", "selection replacement updates text");
    require(engine.text_model().caret_byte_offset() == 2, "selection replacement caret follows inserted text");
    const action_route_policy_diagnostic& policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_commit_boundary,
        "selection replacement emits commit boundary policy");
    require(policy.text_byte_count == 1, "selection replacement policy records inserted byte count");
    require(policy.text_byte_count_before == initial.size(), "selection replacement policy records before byte count");
    require(policy.text_byte_count_after == 3, "selection replacement policy records after byte count");
    require(policy.had_selection_before, "selection replacement policy records prior selection");
    require(!policy.has_selection_after, "selection replacement policy clears selection after commit");
    require_range(policy.selection_before, 1, 1 + std::string(utf8(u8"한")).size(),
        "selection replacement policy records selected utf8 byte range");
    require_range(policy.caret_before,
        1 + std::string(utf8(u8"한")).size(),
        1 + std::string(utf8(u8"한")).size(),
        "selection replacement policy records active caret before commit");
    require_range(policy.caret_after, 2, 2, "selection replacement policy records caret after commit");
}

void test_ime_composition_suppresses_text_and_key_events()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::composition_start, 100)).empty(),
        "composition start is state only");
    require(engine.text_model().ime_composition().active, "composition start creates active empty composition");
    require_range(engine.text_model().ime_composition().preedit_range, 0, 0,
        "composition start preedit range is collapsed");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "composition start emits one state diagnostic policy");
    const action_route_policy_diagnostic& start_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_composition_start,
        "composition start emits state diagnostic policy");
    require(!start_policy.emits_input_event, "composition start policy emits no input event");
    require(start_policy.target_id == "answer", "composition start policy preserves target id");
    require(start_policy.composition.active, "composition start policy carries active composition");
    require(start_policy.composition.preedit_text.empty(), "composition start policy carries empty preedit");
    require_range(start_policy.composition.preedit_range, 0, 0,
        "composition start policy carries collapsed preedit range");
    std::vector<input_event> events =
        engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 110, utf8(u8"ㅎ")));
    require(events.size() == 1, "preedit emits one ime event");
    const ime_event& preedit = require_event<ime_event>(events, 0);
    require(preedit.kind == ime_event_kind::preedit, "preedit event is emitted");
    require(preedit.utf8_text == utf8(u8"ㅎ"), "preedit text is preserved");
    require(preedit.composition.active, "preedit event carries active composition");
    require(preedit.composition.preedit_text == utf8(u8"ㅎ"), "preedit event carries composition text");
    require_range(preedit.composition.replacement_range, 0, 0, "preedit event replacement range is caret collapsed");
    require_range(preedit.composition.preedit_range, 0, std::string(utf8(u8"ㅎ")).size(),
        "preedit event range covers hangul jamo bytes");
    require_range(preedit.composition.caret_range,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "preedit event caret follows hangul jamo");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "preedit emits one action route policy");
    const action_route_policy_diagnostic& preedit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "preedit action route policy is emitted");
    require(preedit_policy.emits_input_event, "preedit policy marks emitted input event");
    require(preedit_policy.event_index == 0, "preedit policy points at preedit event");
    require(preedit_policy.target_id == "answer", "preedit policy preserves target id");
    require(preedit_policy.text_byte_count == std::string(utf8(u8"ㅎ")).size(),
        "preedit policy records preedit byte count");
    require(preedit_policy.text_byte_count_before == 0, "preedit policy records committed bytes before");
    require(preedit_policy.text_byte_count_after == 0, "preedit policy preserves committed bytes after");
    require_range(preedit_policy.caret_before, 0, 0, "preedit policy records caret before preedit");
    require_range(preedit_policy.caret_after,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "preedit policy records display caret after preedit");
    require(preedit_policy.composition.active, "preedit policy carries active composition");
    require(preedit_policy.composition.preedit_text == utf8(u8"ㅎ"),
        "preedit policy carries composition preedit text");
    require_range(preedit_policy.composition.preedit_range, 0, std::string(utf8(u8"ㅎ")).size(),
        "preedit policy carries composition preedit range");
    require(engine.text_model().display_text() == utf8(u8"ㅎ"), "display text includes preedit");

    require(engine.process_raw_event(key(120, "Enter")).empty(), "enter is suppressed during composition");
    require(engine.process_raw_event(key(121, "ArrowLeft")).empty(), "arrow left is suppressed during composition");
    require(engine.process_raw_event(key(122, "ArrowLeft", false, raw_platform_key_phase::down, false, true)).empty(),
        "shift arrow left is suppressed during composition");
    require(engine.process_raw_event(key(123, "a", false, raw_platform_key_phase::down, true)).empty(),
        "ctrl+a is suppressed during composition");
    require(engine.process_raw_event(text(130, "duplicate")).empty(), "raw text is suppressed during composition");
    require(engine.text_model().text().empty(), "suppressed raw text does not commit");
    require(!engine.text_model().selection_range().has_value(), "suppressed composition navigation leaves no selection");
    require(engine.text_model().display_text() == utf8(u8"ㅎ"), "suppressed composition navigation preserves preedit");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::composition_end, 140, utf8(u8"한")));
    require(events.size() == 1, "composition end with text commits ime text");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "ime commit event is emitted");
    require(commit.utf8_text == utf8(u8"한"), "ime commit text is preserved");
    require(commit.composition.active, "ime commit carries composition snapshot before commit");
    require(commit.composition.preedit_text == utf8(u8"ㅎ"), "ime commit preserves previous preedit snapshot");
    require_range(commit.composition.replacement_range, 0, 0, "ime commit replacement range is original caret");
    require_range(commit.composition.preedit_range, 0, std::string(utf8(u8"ㅎ")).size(),
        "ime commit preedit range is previous hangul jamo range");
    require(engine.text_model().text() == utf8(u8"한"), "ime commit updates text model");
    require(engine.text_model().preedit_text().empty(), "ime commit clears preedit");
    require(!engine.text_model().ime_composition().active, "ime commit clears model composition state");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "ime commit emits one action route policy");
    const action_route_policy_diagnostic& commit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_commit,
        "ime commit action route policy is emitted");
    require(commit_policy.emits_input_event, "ime commit policy marks emitted event");
    require(commit_policy.event_index == 0, "ime commit policy points at first event");
    require(commit_policy.target_id == "answer", "ime commit policy preserves target id");
    require(commit_policy.text_byte_count == std::string(utf8(u8"한")).size(),
        "ime commit policy records utf8 byte count");
    require(commit_policy.composition.active, "ime commit policy carries pre-commit composition");
    require(commit_policy.composition.preedit_text == utf8(u8"ㅎ"),
        "ime commit policy carries pre-commit preedit text");
    require(commit_policy.text_byte_count_before == 0, "ime commit policy records empty committed text before");
    require(commit_policy.text_byte_count_after == std::string(utf8(u8"한")).size(),
        "ime commit policy records committed text after");
    require_range(commit_policy.caret_before,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "ime commit policy records display caret before commit");
    require_range(commit_policy.caret_after,
        std::string(utf8(u8"한")).size(),
        std::string(utf8(u8"한")).size(),
        "ime commit policy records committed caret after commit");
}

void test_ime_suppressed_text_and_shortcuts_emit_route_diagnostics()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, "draft")).size() == 1,
        "suppressed route setup starts preedit");
    require(engine.text_model().ime_composition().active, "suppressed route setup has active composition");

    const std::string duplicate_text = "duplicate";
    std::vector<input_event> events = engine.process_raw_event(text(110, duplicate_text));
    require(events.empty(), "duplicate raw text during ime emits no text event");
    require(engine.text_model().text().empty(), "duplicate raw text during ime does not commit");
    require(engine.text_model().preedit_text() == "draft", "duplicate raw text during ime preserves preedit");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "duplicate raw text during ime emits one suppressed route policy");
    const action_route_policy_diagnostic& text_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_commit_boundary,
        "duplicate raw text during ime emits commit-boundary diagnostic");
    require(!text_policy.emits_input_event, "duplicate raw text policy emits no input event");
    require(text_policy.target_id == "answer", "duplicate raw text policy preserves target id");
    require(text_policy.text_byte_count == duplicate_text.size(),
        "duplicate raw text policy records suppressed byte count");
    require(text_policy.text_byte_count_before == 0,
        "duplicate raw text policy records committed byte count before suppression");
    require(text_policy.text_byte_count_after == 0,
        "duplicate raw text policy records unchanged committed byte count after suppression");
    require_range(text_policy.caret_before, 5, 5, "duplicate raw text policy records preedit caret before");
    require_range(text_policy.caret_after, 5, 5, "duplicate raw text policy records unchanged preedit caret after");
    require(text_policy.composition.active, "duplicate raw text policy carries active composition");
    require(text_policy.composition.preedit_text == "draft",
        "duplicate raw text policy carries current preedit text");
    require(engine.routing_diagnostics().summary.routes.text == 1,
        "duplicate raw text summary counts one text route");
    require(engine.routing_diagnostics().summary.routes.ime == 0,
        "duplicate raw text summary does not count an ime route");
    require(!engine.routing_diagnostics().summary.preedit_ended_cleanly,
        "duplicate raw text summary reports active preedit remains");

    events = engine.process_raw_event(key(120, "Enter"));
    require(events.empty(), "submit shortcut during ime emits no text event");
    require(engine.text_model().preedit_text() == "draft", "submit shortcut during ime preserves preedit");
    const action_route_policy_diagnostic& submit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_submit_boundary,
        "submit shortcut during ime emits submit diagnostic");
    require(!submit_policy.emits_input_event, "submit shortcut ime policy emits no input event");
    require(submit_policy.keyboard.intent == keyboard_shortcut_intent::submit,
        "submit shortcut ime policy records submit intent");
    require(submit_policy.keyboard.repeat_policy == keyboard_repeat_policy::not_repeat,
        "submit shortcut ime policy records non-repeat policy");
    require(submit_policy.composition.preedit_text == "draft",
        "submit shortcut ime policy carries active preedit");

    events = engine.process_raw_event(key(125, "Enter", true));
    require(events.empty(), "repeat submit shortcut during ime emits no text event");
    const action_route_policy_diagnostic& repeat_submit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_submit_boundary,
        "repeat submit shortcut during ime emits submit diagnostic");
    require(!repeat_submit_policy.emits_input_event, "repeat submit ime policy emits no input event");
    require(repeat_submit_policy.keyboard.intent == keyboard_shortcut_intent::submit,
        "repeat submit ime policy records submit intent");
    require(repeat_submit_policy.keyboard.repeat, "repeat submit ime policy records repeat state");
    require(repeat_submit_policy.keyboard.repeat_policy == keyboard_repeat_policy::ignored,
        "repeat submit ime policy records ignored repeat policy");
    require(repeat_submit_policy.composition.preedit_text == "draft",
        "repeat submit ime policy carries active preedit");

    events = engine.process_raw_event(key(130, "Backspace", true));
    require(events.empty(), "repeat backspace during ime emits no text event");
    require(engine.text_model().preedit_text() == "draft", "repeat backspace during ime preserves preedit");
    const action_route_policy_diagnostic& backspace_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::text_backspace_boundary,
        "repeat backspace during ime emits backspace diagnostic");
    require(!backspace_policy.emits_input_event, "repeat backspace ime policy emits no input event");
    require(backspace_policy.keyboard.intent == keyboard_shortcut_intent::delete_backward,
        "repeat backspace ime policy records delete backward intent");
    require(backspace_policy.keyboard.repeat, "repeat backspace ime policy records repeat state");
    require(backspace_policy.keyboard.repeat_policy == keyboard_repeat_policy::allowed,
        "repeat backspace ime policy records repeat allowance");
    require(backspace_policy.composition.preedit_text == "draft",
        "repeat backspace ime policy carries active preedit");

    events = engine.process_raw_event(key(140, "a", false, raw_platform_key_phase::down, true));
    require(events.empty(), "select-all shortcut during ime emits no selection event");
    require(!engine.text_model().selection_range().has_value(),
        "select-all shortcut during ime does not create selection");
    const action_route_policy_diagnostic& select_all_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::selection_changed,
        "select-all shortcut during ime emits selection diagnostic");
    require(!select_all_policy.emits_input_event, "select-all ime policy emits no input event");
    require(select_all_policy.keyboard.intent == keyboard_shortcut_intent::select_all,
        "select-all ime policy records select-all intent");
    require(select_all_policy.keyboard.modifiers.ctrl, "select-all ime policy records ctrl modifier");
    require(select_all_policy.composition.preedit_text == "draft",
        "select-all ime policy carries active preedit");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::commit, 150, "final"));
    require(events.size() == 1, "ime commit succeeds after suppressed text and shortcuts");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "post-suppression ime event commits");
    require(commit.composition.preedit_text == "draft",
        "post-suppression commit carries original preedit snapshot");
    require(engine.text_model().text() == "final", "post-suppression commit updates text model");
    require(engine.text_model().preedit_text().empty(), "post-suppression commit clears preedit");
}

void test_ime_preedit_commit_edges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    std::vector<input_event> events =
        engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, utf8(u8"ㅎ")));
    require(events.size() == 1, "first preedit emits one event");
    const ime_event& first_preedit = require_event<ime_event>(events, 0);
    require(first_preedit.utf8_text == utf8(u8"ㅎ"), "first preedit text is emitted");
    require(first_preedit.composition.active, "first preedit carries active composition");
    require_range(first_preedit.composition.replacement_range, 0, 0, "first preedit replacement is collapsed");
    const action_route_policy_diagnostic& first_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "first preedit emits preedit policy");
    require(first_policy.text_byte_count == std::string(utf8(u8"ㅎ")).size(),
        "first preedit policy records jamo byte count");
    require_range(first_policy.caret_before, 0, 0, "first preedit policy records caret before");
    require_range(first_policy.caret_after,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "first preedit policy records caret after");
    require(engine.text_model().display_text() == utf8(u8"ㅎ"), "first preedit is displayed");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 110, utf8(u8"하")));
    require(events.size() == 1, "replacement preedit emits one event");
    const ime_event& replacement_preedit = require_event<ime_event>(events, 0);
    require(replacement_preedit.utf8_text == utf8(u8"하"), "replacement preedit text is emitted");
    require(replacement_preedit.composition.active, "replacement preedit carries active composition");
    require(replacement_preedit.composition.preedit_text == utf8(u8"하"), "replacement preedit composition text is emitted");
    require_range(replacement_preedit.composition.preedit_range, 0, std::string(utf8(u8"하")).size(),
        "replacement preedit range covers hangul syllable bytes");
    const action_route_policy_diagnostic& replacement_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "replacement preedit emits preedit policy");
    require(replacement_policy.text_byte_count == std::string(utf8(u8"하")).size(),
        "replacement preedit policy records syllable byte count");
    require_range(replacement_policy.caret_before,
        std::string(utf8(u8"ㅎ")).size(),
        std::string(utf8(u8"ㅎ")).size(),
        "replacement preedit policy records previous display caret");
    require_range(replacement_policy.caret_after,
        std::string(utf8(u8"하")).size(),
        std::string(utf8(u8"하")).size(),
        "replacement preedit policy records replacement display caret");
    require(replacement_policy.composition.preedit_text == utf8(u8"하"),
        "replacement preedit policy carries replacement composition");
    require(engine.text_model().text().empty(), "replacement preedit does not commit text");
    require(engine.text_model().display_text() == utf8(u8"하"), "replacement preedit replaces displayed preedit");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::commit, 120, utf8(u8"한")));
    require(events.size() == 1, "explicit ime commit emits one event");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "explicit ime commit emits commit kind");
    require(commit.utf8_text == utf8(u8"한"), "explicit ime commit preserves final text");
    require(commit.composition.active, "explicit ime commit carries active composition snapshot");
    require(commit.composition.preedit_text == utf8(u8"하"), "explicit ime commit carries previous preedit text");
    require(engine.text_model().text() == utf8(u8"한"), "explicit ime commit updates committed text");
    require(engine.text_model().preedit_text().empty(), "explicit ime commit clears preedit");

    events = engine.process_raw_event(text(130, "x"));
    require(events.size() == 1, "raw text resumes after explicit ime commit");
    require(engine.text_model().text() == std::string(utf8(u8"한")) + "x", "raw text appends after ime commit");
}

void test_ime_composition_restart_cancels_visible_preedit()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, "draft")).size() == 1,
        "preedit before restart starts composition");
    require(engine.text_model().preedit_text() == "draft", "preedit before restart is visible");

    std::vector<input_event> events =
        engine.process_raw_event(ime(raw_platform_ime_phase::composition_start, 110));
    require(events.size() == 1, "composition restart emits one event for stale preedit");
    const ime_event& cancel = require_event<ime_event>(events, 0);
    require(cancel.kind == ime_event_kind::cancel, "composition restart emits cancel for stale preedit");
    require(cancel.target_id == "answer", "composition restart cancel preserves target id");
    require(cancel.utf8_text.empty(), "composition restart cancel carries no text");
    require(cancel.composition.active, "composition restart cancel carries stale active composition");
    require(cancel.composition.preedit_text == "draft", "composition restart cancel carries stale preedit text");
    require_range(cancel.composition.preedit_range, 0, 5, "composition restart cancel carries stale preedit range");
    require(engine.routing_diagnostics().action_routes.size() == 2,
        "composition restart emits cancel and restart action route policies");
    const action_route_policy_diagnostic& cancel_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_cancel,
        "composition restart cancel action route policy is emitted");
    require(cancel_policy.emits_input_event, "composition restart cancel policy marks emitted event");
    require(cancel_policy.event_index == 0, "composition restart cancel policy points at first event");
    require(cancel_policy.composition.active, "composition restart cancel policy carries stale composition");
    require(cancel_policy.composition.preedit_text == "draft",
        "composition restart cancel policy carries stale preedit text");
    require(cancel_policy.text_byte_count_before == 0, "composition restart cancel policy has no committed text before");
    require(cancel_policy.text_byte_count_after == 0, "composition restart cancel policy has no committed text after");
    require_range(cancel_policy.caret_before, 5, 5, "composition restart cancel policy records preedit caret");
    require_range(cancel_policy.caret_after, 0, 0, "composition restart cancel policy records cleared caret");
    require(engine.text_model().preedit_text().empty(), "composition restart clears stale preedit");
    const action_route_policy_diagnostic& restart_policy = require_policy(
        engine.routing_diagnostics(),
        1,
        action_route_policy_kind::ime_composition_start,
        "composition restart emits new composition start policy");
    require(!restart_policy.emits_input_event, "composition restart start policy emits no input event");
    require(restart_policy.composition.active, "composition restart start policy carries active empty composition");
    require(restart_policy.composition.preedit_text.empty(),
        "composition restart start policy carries empty preedit text");
    require_range(restart_policy.composition.preedit_range, 0, 0,
        "composition restart start policy carries collapsed preedit range");
    require(engine.text_model().ime_composition().active, "composition restart leaves empty composition active");

    require(engine.process_raw_event(text(120, "duplicate")).empty(),
        "raw text remains suppressed after composition restart");
    events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 130, "new"));
    require(events.size() == 1, "preedit resumes after composition restart");
    const ime_event& preedit = require_event<ime_event>(events, 0);
    require(preedit.kind == ime_event_kind::preedit, "post-restart preedit kind is emitted");
    require(preedit.utf8_text == "new", "post-restart preedit text is emitted");
}

void test_empty_ime_commit_and_end_cancel_preedit()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, "draft")).size() == 1,
        "preedit before empty commit starts composition");
    std::vector<input_event> events = engine.process_raw_event(ime(raw_platform_ime_phase::commit, 110));
    require(events.size() == 1, "empty ime commit emits cancellation");
    const ime_event& empty_commit = require_event<ime_event>(events, 0);
    require(empty_commit.kind == ime_event_kind::cancel, "empty ime commit emits cancel kind");
    require(empty_commit.utf8_text.empty(), "empty ime commit cancel carries no text");
    require(empty_commit.composition.active, "empty ime commit cancel carries active composition snapshot");
    require(empty_commit.composition.preedit_text == "draft", "empty ime commit cancel carries preedit text");
    require(engine.text_model().text().empty(), "empty ime commit does not append text");
    require(engine.text_model().preedit_text().empty(), "empty ime commit clears preedit");

    require(engine.process_raw_event(text(120, "a")).size() == 1, "raw text resumes after empty ime commit");
    require(engine.text_model().text() == "a", "raw text is committed after empty ime commit");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 130, "draft")).size() == 1,
        "preedit before empty composition end starts composition");
    events = engine.process_raw_event(ime(raw_platform_ime_phase::composition_end, 140));
    require(events.size() == 1, "empty composition end emits cancellation");
    const ime_event& empty_end = require_event<ime_event>(events, 0);
    require(empty_end.kind == ime_event_kind::cancel, "empty composition end emits cancel kind");
    require(empty_end.composition.active, "empty composition end cancel carries active composition snapshot");
    require(empty_end.composition.preedit_text == "draft", "empty composition end cancel carries preedit text");
    require(engine.text_model().text() == "a", "empty composition end preserves committed text");
    require(engine.text_model().preedit_text().empty(), "empty composition end clears preedit");
}

void test_ime_cancel_summary_reports_clean_preedit_state()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 100, "draft")).size() == 1,
        "summary ime cancel setup preedits");
    std::vector<input_event> events = engine.process_raw_event(ime(raw_platform_ime_phase::cancel, 120));
    require(events.size() == 1, "summary ime cancel emits cancel event");
    const ime_event& cancel = require_event<ime_event>(events, 0);
    require(cancel.kind == ime_event_kind::cancel, "summary ime cancel event kind is cancel");
    require(cancel.composition.preedit_text == "draft", "summary ime cancel event carries stale preedit snapshot");
    require(engine.text_model().preedit_text().empty(), "summary ime cancel clears model preedit");
    require(!engine.text_model().ime_composition().active, "summary ime cancel clears composition state");

    const input_diagnostic_summary& summary = engine.routing_diagnostics().summary;
    require(summary.normalized_event_count == 0, "summary ime cancel emits no normalized gesture events");
    require(summary.routes.ime == 1, "summary ime cancel counts one ime route");
    require(summary.routes.pointer == 0, "summary ime cancel counts no pointer routes");
    require(summary.routes.text == 0, "summary ime cancel counts no text routes");
    require(summary.routes.focus == 0, "summary ime cancel counts no focus routes");
    require(summary.routes.total == 1, "summary ime cancel counts one total route");
    require(summary.pointer_capture_ended_cleanly, "summary ime cancel has no pointer capture");
    require(summary.focus_ended_cleanly, "summary ime cancel leaves focus state clean");
    require(summary.preedit_ended_cleanly, "summary ime cancel ends with clean preedit");
}

void test_ime_empty_preedit_and_commit_only_edges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::composition_start, 100)).empty(),
        "empty composition start emits no event");
    require(engine.text_model().ime_composition().active, "empty composition start activates composition state");
    require_range(engine.text_model().ime_composition().preedit_range, 0, 0,
        "empty composition start has collapsed preedit range");
    const action_route_policy_diagnostic& empty_start_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_composition_start,
        "empty composition start emits start policy");
    require(!empty_start_policy.emits_input_event, "empty composition start policy emits no event");
    require(empty_start_policy.composition.active, "empty composition start policy carries active composition");
    require(engine.process_raw_event(key(105, "Backspace")).empty(),
        "backspace is suppressed during empty composition");
    require(engine.process_raw_event(text(106, "duplicate")).empty(),
        "raw text is suppressed during empty composition");
    require(engine.text_model().text().empty(), "empty composition suppresses duplicate raw text");

    std::vector<input_event> events = engine.process_raw_event(ime(raw_platform_ime_phase::commit, 110));
    require(events.size() == 1, "empty ime commit after composition start emits cancel");
    const ime_event& empty_commit = require_event<ime_event>(events, 0);
    require(empty_commit.kind == ime_event_kind::cancel, "empty commit after composition start is cancel kind");
    require(empty_commit.target_id == "answer", "empty commit cancel preserves target id");
    require(empty_commit.composition.active, "empty commit cancel carries active empty composition snapshot");
    require(empty_commit.composition.preedit_text.empty(), "empty commit cancel carries empty preedit text");
    require_range(empty_commit.composition.preedit_range, 0, 0, "empty commit cancel carries collapsed preedit range");

    events = engine.process_raw_event(text(120, "a"));
    require(events.size() == 1, "raw text resumes after empty composition commit");
    require(engine.text_model().text() == "a", "raw text commits after empty composition commit");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 130));
    require(events.size() == 1, "empty preedit update emits preedit event");
    const ime_event& empty_preedit = require_event<ime_event>(events, 0);
    require(empty_preedit.kind == ime_event_kind::preedit, "empty preedit event uses preedit kind");
    require(empty_preedit.utf8_text.empty(), "empty preedit event carries no text");
    require(empty_preedit.composition.active, "empty preedit event carries active composition");
    require(empty_preedit.composition.preedit_text.empty(), "empty preedit composition text is empty");
    require_range(empty_preedit.composition.replacement_range, 1, 1, "empty preedit replacement range is at caret");
    require_range(empty_preedit.composition.preedit_range, 1, 1, "empty preedit range is collapsed at caret");
    const action_route_policy_diagnostic& empty_preedit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "empty preedit emits preedit policy");
    require(empty_preedit_policy.text_byte_count == 0, "empty preedit policy records zero preedit bytes");
    require(empty_preedit_policy.text_byte_count_before == 1, "empty preedit policy records committed bytes before");
    require(empty_preedit_policy.text_byte_count_after == 1, "empty preedit policy preserves committed bytes after");
    require_range(empty_preedit_policy.caret_before, 1, 1, "empty preedit policy records caret before");
    require_range(empty_preedit_policy.caret_after, 1, 1, "empty preedit policy records collapsed display caret");
    require(empty_preedit_policy.composition.active, "empty preedit policy carries active composition");
    require_range(empty_preedit_policy.composition.replacement_range, 1, 1,
        "empty preedit policy carries collapsed replacement range");
    require(engine.text_model().display_text() == "a", "empty preedit displays committed text only");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::composition_end, 140));
    require(events.size() == 1, "empty composition end after empty preedit emits cancel");
    const ime_event& empty_end = require_event<ime_event>(events, 0);
    require(empty_end.kind == ime_event_kind::cancel, "empty composition end after empty preedit is cancel kind");
    require(empty_end.composition.active, "empty composition end carries active empty composition snapshot");
    require_range(empty_end.composition.replacement_range, 1, 1,
        "empty composition end replacement range is collapsed at caret");
    require(engine.text_model().text() == "a", "empty composition end after empty preedit preserves text");

    require(engine.process_raw_event(ime(raw_platform_ime_phase::cancel, 150)).empty(),
        "cancel without active composition emits no event");

    input_engine commit_only_engine;
    commit_only_engine.focus_text_target("answer");
    events = commit_only_engine.process_raw_event(ime(raw_platform_ime_phase::commit, 200, utf8(u8"한")));
    require(events.size() == 1, "commit-only ime flow emits commit");
    const ime_event& commit_only = require_event<ime_event>(events, 0);
    require(commit_only.kind == ime_event_kind::commit, "commit-only ime flow uses commit kind");
    require(commit_only.utf8_text == utf8(u8"한"), "commit-only ime flow preserves utf8 text");
    require(!commit_only.composition.active, "commit-only ime flow carries inactive composition snapshot");
    require_range(commit_only.composition.replacement_range, 0, 0, "commit-only ime flow replacement range is caret");
    require(commit_only_engine.text_model().text() == utf8(u8"한"), "commit-only ime flow updates text model");

    events = commit_only_engine.process_raw_event(text(210, "x"));
    require(events.size() == 1, "raw text resumes after commit-only ime flow");
    require(commit_only_engine.text_model().text() == std::string(utf8(u8"한")) + "x",
        "raw text appends after commit-only ime flow");
}

void test_ime_hangul_replacement_composition_ranges()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    const std::string base = std::string("A") + utf8(u8"한") + "B";
    require(engine.process_raw_event(text(100, base)).size() == 1, "hangul replacement base text commits");

    require(engine.process_raw_event(key(110, "Home")).size() == 1, "home before hangul selection moves caret");
    require(engine.process_raw_event(key(120, "ArrowRight")).size() == 1,
        "arrow right before hangul selection moves past ascii");
    require(engine.process_raw_event(key(130, "ArrowRight", false, raw_platform_key_phase::down, false, true)).size() == 1,
        "shift arrow right selects hangul codepoint");
    const std::optional<text_range> selection = engine.text_model().selection_range();
    require(selection.has_value(), "hangul replacement selection is active");
    require_range(*selection, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement selection spans full utf8 codepoint");

    std::vector<input_event> events = engine.process_raw_event(ime(raw_platform_ime_phase::preedit_update, 140, utf8(u8"하")));
    require(events.size() == 1, "hangul replacement preedit emits one event");
    const ime_event& preedit = require_event<ime_event>(events, 0);
    require(preedit.kind == ime_event_kind::preedit, "hangul replacement preedit kind is emitted");
    require(preedit.composition.active, "hangul replacement preedit carries active composition");
    require(preedit.composition.preedit_text == utf8(u8"하"), "hangul replacement preedit carries hangul text");
    require_range(preedit.composition.replacement_range, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement preedit replacement range covers selected hangul");
    require_range(preedit.composition.preedit_range, 1, 1 + std::string(utf8(u8"하")).size(),
        "hangul replacement preedit range covers new hangul");
    require(engine.text_model().display_text() == std::string("A") + utf8(u8"하") + "B",
        "hangul replacement preedit display replaces selected hangul");
    const action_route_policy_diagnostic& preedit_policy = require_policy(
        engine.routing_diagnostics(),
        0,
        action_route_policy_kind::ime_preedit,
        "hangul replacement preedit emits preedit policy");
    require(preedit_policy.text_byte_count == std::string(utf8(u8"하")).size(),
        "hangul replacement preedit policy records preedit byte count");
    require(preedit_policy.text_byte_count_before == base.size(),
        "hangul replacement preedit policy records original committed bytes");
    require(preedit_policy.text_byte_count_after == base.size(),
        "hangul replacement preedit policy does not mutate committed bytes");
    require(preedit_policy.had_selection_before, "hangul replacement preedit policy records prior selection");
    require(preedit_policy.has_selection_after, "hangul replacement preedit policy keeps selection during composition");
    require_range(preedit_policy.selection_before, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement preedit policy records selected hangul before");
    require_range(preedit_policy.selection_after, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement preedit policy records replacement selection after");
    require_range(preedit_policy.caret_before,
        1 + std::string(utf8(u8"한")).size(),
        1 + std::string(utf8(u8"한")).size(),
        "hangul replacement preedit policy records active selection caret before");
    require_range(preedit_policy.caret_after,
        1 + std::string(utf8(u8"하")).size(),
        1 + std::string(utf8(u8"하")).size(),
        "hangul replacement preedit policy records display caret after");
    require_range(preedit_policy.composition.replacement_range, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement preedit policy carries composition replacement range");
    require_range(preedit_policy.composition.preedit_range, 1, 1 + std::string(utf8(u8"하")).size(),
        "hangul replacement preedit policy carries composition preedit range");

    events = engine.process_raw_event(ime(raw_platform_ime_phase::commit, 150, utf8(u8"각")));
    require(events.size() == 1, "hangul replacement commit emits one event");
    const ime_event& commit = require_event<ime_event>(events, 0);
    require(commit.kind == ime_event_kind::commit, "hangul replacement commit kind is emitted");
    require(commit.utf8_text == utf8(u8"각"), "hangul replacement commit carries final hangul");
    require(commit.composition.active, "hangul replacement commit carries previous composition snapshot");
    require(commit.composition.preedit_text == utf8(u8"하"), "hangul replacement commit carries previous preedit");
    require_range(commit.composition.replacement_range, 1, 1 + std::string(utf8(u8"한")).size(),
        "hangul replacement commit replacement range covers selected hangul");
    require(engine.text_model().text() == std::string("A") + utf8(u8"각") + "B",
        "hangul replacement commit updates committed text");
    require(engine.text_model().caret_byte_offset() == 1 + std::string(utf8(u8"각")).size(),
        "hangul replacement commit places caret after final hangul");
    require(!engine.text_model().selection_range().has_value(), "hangul replacement commit clears selection");
    require(!engine.text_model().ime_composition().active, "hangul replacement commit clears model composition");
}


} // namespace

int main()
{
    test_keyboard_navigation_diagnostics_preserve_ime_composition();
    test_text_edit_boundary_diagnostics_replace_utf8_selection();
    test_ime_composition_suppresses_text_and_key_events();
    test_ime_suppressed_text_and_shortcuts_emit_route_diagnostics();
    test_ime_preedit_commit_edges();
    test_ime_composition_restart_cancels_visible_preedit();
    test_empty_ime_commit_and_end_cancel_preedit();
    test_ime_cancel_summary_reports_clean_preedit_state();
    test_ime_empty_preedit_and_commit_only_edges();
    test_ime_hangul_replacement_composition_ranges();

    std::cout << "input_engine_ime_tests passed\n";
    return 0;
}
