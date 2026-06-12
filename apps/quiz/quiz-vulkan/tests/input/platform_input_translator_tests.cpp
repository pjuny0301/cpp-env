#include "core/input/input_engine.h"
#include "core/input/platform_input_translator.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
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

    std::cerr << "platform_input_translator_tests failed: " << message << '\n';
    std::exit(1);
}

void require_accepted(
    const quiz_vulkan::input::platform_input_translation_result& result,
    quiz_vulkan::input::platform_input_source_kind source,
    const char* message)
{
    require(result.event.has_value(), message);
    require(result.diagnostic.source == source, message);
    require(result.diagnostic.status == quiz_vulkan::input::platform_input_translation_status::accepted, message);
    require(result.diagnostic.rejection_reason == quiz_vulkan::input::platform_input_rejection_reason::none, message);
    require(result.diagnostic.emitted_event, message);
}

void require_rejected(
    const quiz_vulkan::input::platform_input_translation_result& result,
    quiz_vulkan::input::platform_input_source_kind source,
    quiz_vulkan::input::platform_input_rejection_reason reason,
    const char* message)
{
    require(!result.event.has_value(), message);
    require(result.diagnostic.source == source, message);
    require(result.diagnostic.status == quiz_vulkan::input::platform_input_translation_status::rejected, message);
    require(result.diagnostic.rejection_reason == reason, message);
    require(!result.diagnostic.emitted_event, message);
}

void require_modifiers(
    quiz_vulkan::input::input_modifier_state modifiers,
    bool alt,
    bool ctrl,
    bool shift,
    bool meta,
    const char* message)
{
    require(modifiers.alt == alt, message);
    require(modifiers.ctrl == ctrl, message);
    require(modifiers.shift == shift, message);
    require(modifiers.meta == meta, message);
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

template <typename T>
const T& require_translated_event(
    const quiz_vulkan::input::platform_input_translation_result& result,
    const char* message)
{
    require(result.event.has_value(), message);
    require(std::holds_alternative<T>(*result.event), message);
    return std::get<T>(*result.event);
}

template <typename T>
const T& require_input_event(const std::vector<quiz_vulkan::input::input_event>& events, std::size_t index)
{
    require(index < events.size(), "input event index exists");
    require(std::holds_alternative<T>(events[index]), "input event has expected type");
    return std::get<T>(events[index]);
}

quiz_vulkan::input::platform_input_translation_result translate(
    const quiz_vulkan::input::platform_input_translator& translator,
    quiz_vulkan::input::platform_input_sample sample)
{
    return translator.translate(quiz_vulkan::input::platform_input_translation_request{
        .sample = std::move(sample),
    });
}

void test_mouse_click_translates_to_pointer_events_and_tap()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    platform_input_translator translator;
    input_engine engine;

    platform_input_translation_result down = translate(
        translator,
        platform_mouse_sample{
            .timestamp_ms = 100,
            .pointer_id = 7,
            .phase = platform_pointer_sample_phase::down,
            .button = platform_mouse_button::primary,
            .x = 12.0f,
            .y = 24.0f,
        });
    require_accepted(down, platform_input_source_kind::mouse, "mouse down is accepted");
    const raw_platform_pointer_event& raw_down = require_translated_event<raw_platform_pointer_event>(
        down,
        "mouse down translates to raw pointer");
    require(raw_down.phase == raw_platform_pointer_phase::down, "mouse down preserves phase");
    require(raw_down.button == raw_platform_pointer_button::primary, "mouse down preserves primary button");
    require(raw_down.pointer_id == 7, "mouse down preserves pointer id");
    require(raw_down.x == 12.0f, "mouse down preserves x");
    require(engine.process_raw_event(*down.event).empty(), "mouse down emits no gesture yet");

    platform_input_translation_result up = translate(
        translator,
        platform_mouse_sample{
            .timestamp_ms = 140,
            .pointer_id = 7,
            .phase = platform_pointer_sample_phase::up,
            .button = platform_mouse_button::primary,
            .x = 13.0f,
            .y = 25.0f,
        });
    require_accepted(up, platform_input_source_kind::mouse, "mouse up is accepted");
    const raw_platform_pointer_event& raw_up = require_translated_event<raw_platform_pointer_event>(
        up,
        "mouse up translates to raw pointer");
    require(raw_up.phase == raw_platform_pointer_phase::up, "mouse up preserves phase");
    require(raw_up.pointer_id == 7, "mouse up preserves pointer id");

    std::vector<input_event> events = engine.process_raw_event(*up.event);
    require(events.size() == 1, "translated mouse click emits one tap");
    const gesture_event& tap = require_input_event<gesture_event>(events, 0);
    require(tap.kind == gesture_kind::tap, "translated mouse click emits tap");
    require(tap.pointer_id == 7, "translated mouse click preserves pointer id");
    require(engine.routing_diagnostics().action_routes.size() == 1,
        "translated mouse click emits route policy");
    require(engine.routing_diagnostics().action_routes[0].pointer_contact == pointer_contact_kind::mouse_like,
        "translated mouse click records mouse-like contact");
}

void test_touch_contact_translates_to_touch_like_pointer()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    platform_input_translator translator;
    input_engine engine;

    platform_input_translation_result down = translate(
        translator,
        platform_touch_sample{
            .timestamp_ms = 200,
            .contact_id = 42,
            .phase = platform_pointer_sample_phase::down,
            .x = 40.0f,
            .y = 50.0f,
        });
    require_accepted(down, platform_input_source_kind::touch, "touch down is accepted");
    const raw_platform_pointer_event& raw_down = require_translated_event<raw_platform_pointer_event>(
        down,
        "touch down translates to raw pointer");
    require(raw_down.button == raw_platform_pointer_button::none, "touch down maps to touch-like button none");
    require(raw_down.pointer_id == 42, "touch down maps contact id to pointer id");
    require(engine.process_raw_event(*down.event).empty(), "touch down emits no gesture yet");

    platform_input_translation_result up = translate(
        translator,
        platform_touch_sample{
            .timestamp_ms = 230,
            .contact_id = 42,
            .phase = platform_pointer_sample_phase::up,
            .x = 41.0f,
            .y = 51.0f,
        });
    require_accepted(up, platform_input_source_kind::touch, "touch up is accepted");
    std::vector<input_event> events = engine.process_raw_event(*up.event);
    require(events.size() == 1, "translated touch contact emits one tap");
    const gesture_event& tap = require_input_event<gesture_event>(events, 0);
    require(tap.kind == gesture_kind::tap, "translated touch contact emits tap");
    require(tap.pointer_id == 42, "translated touch contact preserves pointer id");
    require(engine.routing_diagnostics().action_routes[0].pointer_contact == pointer_contact_kind::touch_like,
        "translated touch contact records touch-like contact");
}

void test_wheel_translates_to_platform_scroll_and_engine_summary()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    platform_input_translator translator;
    platform_input_translation_result result = translate(
        translator,
        platform_wheel_sample{
            .timestamp_ms = 300,
            .x = 20.0f,
            .y = 30.0f,
            .delta_x = 1.0f,
            .delta_y = -3.0f,
            .unit = platform_scroll_delta_unit::lines,
            .alt = true,
            .ctrl = false,
            .shift = true,
            .meta = false,
        });
    require_accepted(result, platform_input_source_kind::wheel, "wheel is accepted");
    const raw_platform_scroll_event& raw_scroll = require_translated_event<raw_platform_scroll_event>(
        result,
        "wheel translates to raw scroll");
    require(raw_scroll.unit == raw_platform_scroll_delta_unit::lines, "wheel preserves line unit");
    require(raw_scroll.delta_x == 1.0f, "wheel preserves x delta");
    require(raw_scroll.delta_y == -3.0f, "wheel preserves y delta");
    require(raw_scroll.alt, "wheel preserves alt modifier");
    require(!raw_scroll.ctrl, "wheel preserves ctrl modifier");
    require(raw_scroll.shift, "wheel preserves shift modifier");
    require(!raw_scroll.meta, "wheel preserves meta modifier");

    input_engine engine;
    std::vector<input_event> events = engine.process_raw_event(*result.event);
    require(events.size() == 1, "translated wheel emits one scroll event");
    const scroll_event& scroll = require_input_event<scroll_event>(events, 0);
    require(scroll.line_delta_x == 1.0f, "translated wheel normalizes x line delta");
    require(scroll.line_delta_y == -3.0f, "translated wheel normalizes y line delta");
    require(scroll.pixel_delta_x == 0.0f, "translated wheel pixel x is zero");
    require_modifiers(scroll.modifiers, true, false, true, false,
        "translated wheel normalizes modifier state");
    require(engine.routing_diagnostics().normalized_events.size() == 1,
        "translated wheel emits one normalized summary");
    require(engine.routing_diagnostics().normalized_events[0].kind == input_event_summary_kind::wheel,
        "translated wheel summary kind is wheel");
    require(engine.routing_diagnostics().normalized_events[0].line_delta_y == -3.0f,
        "translated wheel summary preserves line delta");
    require_modifiers(
        engine.routing_diagnostics().normalized_events[0].modifiers,
        true,
        false,
        true,
        false,
        "translated wheel summary preserves modifiers");
    require_modifiers(
        engine.routing_diagnostics().action_routes[0].normalized_event.modifiers,
        true,
        false,
        true,
        false,
        "translated wheel route preserves modifiers");
}

void test_translated_wheel_preserves_active_ime_route_context()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    platform_input_translator translator;
    input_engine engine;
    engine.focus_text_target("answer");

    const std::string initial = std::string("A") + utf8(u8"한");
    const platform_input_translation_result character = translate(
        translator,
        platform_character_sample{
            .timestamp_ms = 330,
            .utf8_text = initial,
        });
    require_accepted(character, platform_input_source_kind::character,
        "wheel ime fixture initial character is accepted");
    require(engine.process_raw_event(*character.event).size() == 1,
        "wheel ime fixture initial character commits");

    const std::string preedit = utf8(u8"ㄱ");
    const platform_input_translation_result preedit_result = translate(
        translator,
        platform_ime_composition_sample{
            .timestamp_ms = 340,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = preedit,
        });
    require_accepted(preedit_result, platform_input_source_kind::ime,
        "wheel ime fixture preedit is accepted");
    require(engine.process_raw_event(*preedit_result.event).size() == 1,
        "wheel ime fixture preedit starts composition");
    require(engine.text_model().display_text() == initial + preedit,
        "wheel ime fixture display includes preedit before wheel");

    const platform_input_translation_result wheel = translate(
        translator,
        platform_wheel_sample{
            .timestamp_ms = 350,
            .x = 18.0f,
            .y = 28.0f,
            .delta_x = 0.0f,
            .delta_y = -48.0f,
            .unit = platform_scroll_delta_unit::pixels,
            .alt = false,
            .ctrl = true,
            .shift = true,
            .meta = false,
        });
    require_accepted(wheel, platform_input_source_kind::wheel,
        "wheel ime fixture translated wheel is accepted");

    std::vector<input_event> events = engine.process_raw_event(*wheel.event);
    require(events.size() == 1, "wheel ime fixture emits one scroll event");
    const scroll_event& scroll = require_input_event<scroll_event>(events, 0);
    require(scroll.pixel_delta_y == -48.0f,
        "wheel ime fixture normalizes pixel delta");
    require_modifiers(scroll.modifiers, false, true, true, false,
        "wheel ime fixture preserves wheel modifiers");
    require(engine.text_model().text() == initial,
        "wheel ime fixture preserves committed text");
    require(engine.text_model().preedit_text() == preedit,
        "wheel ime fixture preserves active preedit");

    const input_routing_diagnostics& diagnostics = engine.routing_diagnostics();
    require(diagnostics.action_routes.size() == 1,
        "wheel ime fixture emits one wheel route");
    const action_route_policy_diagnostic& policy = diagnostics.action_routes[0];
    require(policy.kind == action_route_policy_kind::wheel_summary,
        "wheel ime fixture route is wheel summary");
    require(policy.emits_input_event, "wheel ime fixture route emits normalized wheel event");
    require(policy.target_id == "answer",
        "wheel ime fixture route records focused target");
    require(policy.normalized_event.kind == input_event_summary_kind::wheel,
        "wheel ime fixture route carries wheel normalized summary");
    require(policy.normalized_event.pixel_delta_y == -48.0f,
        "wheel ime fixture route records pixel wheel delta");
    require_modifiers(policy.normalized_event.modifiers, false, true, true, false,
        "wheel ime fixture route preserves modifier evidence");
    require(policy.composition.active,
        "wheel ime fixture route carries active composition");
    require(policy.composition.preedit_text == preedit,
        "wheel ime fixture route carries active preedit text");
    require(policy.composition_before.active && policy.composition_after.active,
        "wheel ime fixture route records active composition before and after");
    require(policy.text_byte_count_before == initial.size(),
        "wheel ime fixture route records committed bytes before");
    require(policy.text_byte_count_after == initial.size(),
        "wheel ime fixture route records committed bytes after");
    const std::size_t display_caret = initial.size() + preedit.size();
    require_range(policy.caret_before, display_caret, display_caret,
        "wheel ime fixture route records utf8-safe caret before");
    require_range(policy.caret_after, display_caret, display_caret,
        "wheel ime fixture route records utf8-safe caret after");
    require(diagnostics.summary.routes.wheel == 1,
        "wheel ime fixture summary counts wheel route");
    require(diagnostics.summary.routes.ime == 0,
        "wheel ime fixture summary emits no ime route");
    require(diagnostics.summary.routes.text == 0,
        "wheel ime fixture summary emits no text route");
    require(!diagnostics.summary.preedit_ended_cleanly,
        "wheel ime fixture summary reports preedit still active");
    require(!engine.text_model().has_submit_text(),
        "wheel ime fixture does not submit app/domain action");
}

void test_key_translates_phase_modifiers_and_repeat()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    platform_input_translator translator;
    platform_input_translation_result result = translate(
        translator,
        platform_key_sample{
            .timestamp_ms = 400,
            .phase = platform_key_sample_phase::down,
            .key_code = 65,
            .logical_key = "a",
            .alt = true,
            .ctrl = true,
            .shift = false,
            .meta = true,
            .repeat = true,
        });
    require_accepted(result, platform_input_source_kind::key, "key is accepted");
    const raw_platform_key_event& key = require_translated_event<raw_platform_key_event>(
        result,
        "key translates to raw key");
    require(key.phase == raw_platform_key_phase::down, "key preserves down phase");
    require(key.key_code == 65, "key preserves key code");
    require(key.logical_key == "a", "key preserves logical key");
    require(key.alt, "key preserves alt modifier");
    require(key.ctrl, "key preserves ctrl modifier");
    require(!key.shift, "key preserves shift modifier");
    require(key.meta, "key preserves meta modifier");
    require(key.repeat, "key preserves repeat");
}

void test_character_utf8_translates_to_text_input()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    platform_input_translator translator;
    const std::string committed_text = utf8(u8"한");
    platform_input_translation_result result = translate(
        translator,
        platform_character_sample{
            .timestamp_ms = 500,
            .utf8_text = committed_text,
        });
    require_accepted(result, platform_input_source_kind::character, "character text is accepted");
    const raw_platform_text_event& raw_text = require_translated_event<raw_platform_text_event>(
        result,
        "character text translates to raw text");
    require(raw_text.utf8_text == committed_text, "character text preserves utf8 payload");

    input_engine engine;
    engine.focus_text_target("answer");
    std::vector<input_event> events = engine.process_raw_event(*result.event);
    require(events.size() == 1, "translated character emits one text event");
    const text_event& text = require_input_event<text_event>(events, 0);
    require(text.kind == text_event_kind::commit, "translated character commits text");
    require(text.utf8_text == committed_text, "translated character commit preserves utf8 text");
    require(engine.text_model().text() == committed_text, "translated character updates text model");
}

void test_ime_preedit_commit_and_cancel_translate_to_ime_events()
{
    using namespace quiz_vulkan;
    using namespace quiz_vulkan::input;

    platform_input_translator translator;
    input_engine commit_engine;
    commit_engine.focus_text_target("answer");

    platform_input_translation_result start = translate(
        translator,
        platform_ime_composition_sample{
            .timestamp_ms = 590,
            .phase = platform_ime_sample_phase::composition_start,
            .utf8_text = {},
        });
    require_accepted(start, platform_input_source_kind::ime, "ime composition start is accepted");
    const raw_platform_ime_event& raw_start = require_translated_event<raw_platform_ime_event>(
        start,
        "ime composition start translates to raw ime");
    require(raw_start.phase == raw_platform_ime_phase::composition_start,
        "ime composition start preserves phase");
    std::vector<input_event> events = commit_engine.process_raw_event(*start.event);
    require(events.empty(), "translated ime composition start emits no normalized input event");
    require(commit_engine.text_model().ime_composition().active,
        "translated ime composition start activates composition state");
    require(commit_engine.routing_diagnostics().action_routes.size() == 1,
        "translated ime composition start emits one route diagnostic");
    require(commit_engine.routing_diagnostics().action_routes[0].kind
            == action_route_policy_kind::ime_composition_start,
        "translated ime composition start records state-only route kind");
    require(!commit_engine.routing_diagnostics().action_routes[0].emits_input_event,
        "translated ime composition start route emits no input event");

    platform_input_translation_result preedit = translate(
        translator,
        platform_ime_composition_sample{
            .timestamp_ms = 600,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = utf8(u8"ㅎ"),
        });
    require_accepted(preedit, platform_input_source_kind::ime, "ime preedit is accepted");
    const raw_platform_ime_event& raw_preedit = require_translated_event<raw_platform_ime_event>(
        preedit,
        "ime preedit translates to raw ime");
    require(raw_preedit.phase == raw_platform_ime_phase::preedit_update,
        "ime preedit preserves phase");

    events = commit_engine.process_raw_event(*preedit.event);
    require(events.size() == 1, "translated ime preedit emits one ime event");
    const ime_event& preedit_event = require_input_event<ime_event>(events, 0);
    require(preedit_event.kind == ime_event_kind::preedit, "translated ime preedit emits preedit kind");
    require(preedit_event.utf8_text == utf8(u8"ㅎ"), "translated ime preedit preserves utf8");

    platform_input_translation_result commit = translate(
        translator,
        platform_ime_composition_sample{
            .timestamp_ms = 620,
            .phase = platform_ime_sample_phase::commit,
            .utf8_text = utf8(u8"한"),
        });
    require_accepted(commit, platform_input_source_kind::ime, "ime commit is accepted");
    const raw_platform_ime_event& raw_commit = require_translated_event<raw_platform_ime_event>(
        commit,
        "ime commit translates to raw ime");
    require(raw_commit.phase == raw_platform_ime_phase::commit, "ime commit preserves phase");

    events = commit_engine.process_raw_event(*commit.event);
    require(events.size() == 1, "translated ime commit emits one ime event");
    const ime_event& commit_event = require_input_event<ime_event>(events, 0);
    require(commit_event.kind == ime_event_kind::commit, "translated ime commit emits commit kind");
    require(commit_event.utf8_text == utf8(u8"한"), "translated ime commit preserves utf8");
    require(commit_engine.text_model().text() == utf8(u8"한"), "translated ime commit updates text model");

    input_engine cancel_engine;
    cancel_engine.focus_text_target("answer");
    require(!cancel_engine.process_raw_event(*preedit.event).empty(), "translated ime cancel setup preedits");
    platform_input_translation_result cancel = translate(
        translator,
        platform_ime_composition_sample{
            .timestamp_ms = 700,
            .phase = platform_ime_sample_phase::cancel,
            .utf8_text = {},
        });
    require_accepted(cancel, platform_input_source_kind::ime, "ime cancel is accepted");
    const raw_platform_ime_event& raw_cancel = require_translated_event<raw_platform_ime_event>(
        cancel,
        "ime cancel translates to raw ime");
    require(raw_cancel.phase == raw_platform_ime_phase::cancel, "ime cancel preserves phase");

    events = cancel_engine.process_raw_event(*cancel.event);
    require(events.size() == 1, "translated ime cancel emits one ime event");
    const ime_event& cancel_event = require_input_event<ime_event>(events, 0);
    require(cancel_event.kind == ime_event_kind::cancel, "translated ime cancel emits cancel kind");
    require(cancel_engine.text_model().preedit_text().empty(), "translated ime cancel clears preedit");
}

void test_rejected_malformed_samples_emit_diagnostics_without_events()
{
    using namespace quiz_vulkan::input;

    platform_input_translator translator;
    platform_input_translation_result bad_mouse = translate(
        translator,
        platform_mouse_sample{
            .timestamp_ms = 800,
            .pointer_id = 0,
            .phase = platform_pointer_sample_phase::down,
            .button = platform_mouse_button::primary,
            .x = 0.0f,
            .y = 0.0f,
        });
    require_rejected(
        bad_mouse,
        platform_input_source_kind::mouse,
        platform_input_rejection_reason::invalid_pointer_id,
        "invalid mouse pointer is rejected");

    platform_input_translation_result bad_touch = translate(
        translator,
        platform_touch_sample{
            .timestamp_ms = -1,
            .contact_id = 3,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        });
    require_rejected(
        bad_touch,
        platform_input_source_kind::touch,
        platform_input_rejection_reason::invalid_timestamp,
        "invalid touch timestamp is rejected");

    platform_input_translation_result bad_wheel = translate(
        translator,
        platform_wheel_sample{
            .timestamp_ms = 820,
            .x = 0.0f,
            .y = 0.0f,
            .delta_x = 0.0f,
            .delta_y = 0.0f,
            .unit = platform_scroll_delta_unit::pixels,
        });
    require_rejected(
        bad_wheel,
        platform_input_source_kind::wheel,
        platform_input_rejection_reason::invalid_wheel_delta,
        "zero wheel delta is rejected");

    platform_input_translation_result bad_key = translate(
        translator,
        platform_key_sample{
            .timestamp_ms = 830,
            .phase = platform_key_sample_phase::down,
            .key_code = 0,
            .logical_key = {},
        });
    require_rejected(
        bad_key,
        platform_input_source_kind::key,
        platform_input_rejection_reason::invalid_key,
        "empty key identity is rejected");

    platform_input_translation_result bad_character = translate(
        translator,
        platform_character_sample{
            .timestamp_ms = 840,
            .utf8_text = std::string("\xC0\xAF", 2),
        });
    require_rejected(
        bad_character,
        platform_input_source_kind::character,
        platform_input_rejection_reason::invalid_utf8,
        "invalid character utf8 is rejected");

    platform_input_translation_result empty_character = translate(
        translator,
        platform_character_sample{
            .timestamp_ms = 850,
            .utf8_text = {},
        });
    require_rejected(
        empty_character,
        platform_input_source_kind::character,
        platform_input_rejection_reason::empty_text,
        "empty character text is rejected");

    platform_input_translation_result empty_ime_commit = translate(
        translator,
        platform_ime_composition_sample{
            .timestamp_ms = 860,
            .phase = platform_ime_sample_phase::commit,
            .utf8_text = {},
        });
    require_rejected(
        empty_ime_commit,
        platform_input_source_kind::ime,
        platform_input_rejection_reason::empty_text,
        "empty ime commit is rejected");

    platform_input_translation_result bad_ime_preedit = translate(
        translator,
        platform_ime_composition_sample{
            .timestamp_ms = 870,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = std::string("\xF0\x28\x8C\x28", 4),
        });
    require_rejected(
        bad_ime_preedit,
        platform_input_source_kind::ime,
        platform_input_rejection_reason::invalid_utf8,
        "invalid ime preedit utf8 is rejected");
}

} // namespace

int main()
{
    test_mouse_click_translates_to_pointer_events_and_tap();
    test_touch_contact_translates_to_touch_like_pointer();
    test_wheel_translates_to_platform_scroll_and_engine_summary();
    test_translated_wheel_preserves_active_ime_route_context();
    test_key_translates_phase_modifiers_and_repeat();
    test_character_utf8_translates_to_text_input();
    test_ime_preedit_commit_and_cancel_translate_to_ime_events();
    test_rejected_malformed_samples_emit_diagnostics_without_events();

    std::cout << "platform_input_translator_tests passed\n";
    return 0;
}
