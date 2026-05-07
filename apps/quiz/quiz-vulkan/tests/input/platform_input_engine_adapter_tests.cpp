#include "core/input/platform_input_engine_adapter.h"

#include <array>
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

    std::cerr << "platform_input_engine_adapter_tests failed: " << message << '\n';
    std::exit(1);
}

template <typename T>
const T& require_event(const std::vector<quiz_vulkan::input::input_event>& events, std::size_t index)
{
    require(index < events.size(), "input event index exists");
    require(std::holds_alternative<T>(events[index]), "input event has expected type");
    return std::get<T>(events[index]);
}

quiz_vulkan::input::platform_input_translation_request request(
    quiz_vulkan::input::platform_input_sample sample)
{
    return quiz_vulkan::input::platform_input_translation_request{
        .sample = std::move(sample),
    };
}

void require_dispatched(
    const quiz_vulkan::input::platform_input_dispatch_result& result,
    quiz_vulkan::input::platform_input_source_kind source,
    const char* message)
{
    require(result.dispatched_to_engine, message);
    require(result.translation.event.has_value(), message);
    require(result.translation.diagnostic.source == source, message);
    require(result.translation.diagnostic.status == quiz_vulkan::input::platform_input_translation_status::accepted,
        message);
}

void test_mouse_click_batch_dispatches_pointer_path()
{
    using namespace quiz_vulkan::input;

    input_engine engine;
    platform_input_translator translator;
    const std::array requests{
        request(platform_mouse_sample{
            .timestamp_ms = 100,
            .pointer_id = 4,
            .phase = platform_pointer_sample_phase::down,
            .button = platform_mouse_button::primary,
            .x = 20.0f,
            .y = 30.0f,
        }),
        request(platform_mouse_sample{
            .timestamp_ms = 140,
            .pointer_id = 4,
            .phase = platform_pointer_sample_phase::up,
            .button = platform_mouse_button::primary,
            .x = 21.0f,
            .y = 31.0f,
        }),
    };

    const platform_input_dispatch_batch_result batch =
        translate_and_dispatch_platform_input_batch(engine, translator, requests);
    require(batch.items.size() == 2, "mouse click batch records two dispatch results");
    require_dispatched(batch.items[0], platform_input_source_kind::mouse, "mouse down dispatches");
    require(batch.items[0].input_events.empty(), "mouse down emits no gesture event");
    require(batch.items[0].routing_diagnostics.pointer_capture.lifecycle == pointer_capture_lifecycle::tracking,
        "mouse down tracks pointer in engine");

    require_dispatched(batch.items[1], platform_input_source_kind::mouse, "mouse up dispatches");
    require(batch.items[1].input_events.size() == 1, "mouse click emits one input event");
    const gesture_event& tap = require_event<gesture_event>(batch.items[1].input_events, 0);
    require(tap.kind == gesture_kind::tap, "mouse click dispatch emits tap");
    require(tap.pointer_id == 4, "mouse click dispatch preserves pointer id");
    require(batch.items[1].routing_diagnostics.action_routes.size() == 1,
        "mouse click dispatch records route policy");
    require(batch.items[1].routing_diagnostics.action_routes[0].pointer_contact == pointer_contact_kind::mouse_like,
        "mouse click dispatch stays mouse-like");
    require(batch.items[1].routing_diagnostics.pointer_capture.lifecycle == pointer_capture_lifecycle::idle,
        "mouse click dispatch releases pointer");
}

void test_touch_swipe_batch_dispatches_gesture_path()
{
    using namespace quiz_vulkan::input;

    input_engine engine;
    platform_input_translator translator;
    const std::array requests{
        request(platform_touch_sample{
            .timestamp_ms = 200,
            .contact_id = 9,
            .phase = platform_pointer_sample_phase::down,
            .x = 0.0f,
            .y = 0.0f,
        }),
        request(platform_touch_sample{
            .timestamp_ms = 260,
            .contact_id = 9,
            .phase = platform_pointer_sample_phase::up,
            .x = 70.0f,
            .y = 0.0f,
        }),
    };

    const platform_input_dispatch_batch_result batch =
        translate_and_dispatch_platform_input_batch(engine, translator, requests);
    require(batch.items.size() == 2, "touch swipe batch records two dispatch results");
    require_dispatched(batch.items[1], platform_input_source_kind::touch, "touch swipe up dispatches");
    require(batch.items[1].input_events.size() == 1, "touch swipe emits one input event");
    const gesture_event& swipe = require_event<gesture_event>(batch.items[1].input_events, 0);
    require(swipe.kind == gesture_kind::swipe_right, "touch swipe dispatch emits swipe right");
    require(swipe.pointer_id == 9, "touch swipe dispatch preserves contact id as pointer id");
    require(batch.items[1].routing_diagnostics.normalized_events.size() == 1,
        "touch swipe dispatch records normalized event");
    require(batch.items[1].routing_diagnostics.normalized_events[0].kind == input_event_summary_kind::swipe_right,
        "touch swipe dispatch records normalized swipe summary");
    require(batch.items[1].routing_diagnostics.action_routes[0].pointer_contact == pointer_contact_kind::touch_like,
        "touch swipe dispatch stays touch-like");
    require(batch.items[1].routing_diagnostics.action_routes[0].gesture_policy.decision
            == gesture_policy_decision::swipe_accepted,
        "touch swipe dispatch records accepted classifier decision");
}

void test_key_and_char_batch_dispatch_text_input_path()
{
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    platform_input_translator translator;
    const std::string committed_text = utf8(u8"한");
    const std::array requests{
        request(platform_character_sample{
            .timestamp_ms = 300,
            .utf8_text = committed_text,
        }),
        request(platform_key_sample{
            .timestamp_ms = 320,
            .phase = platform_key_sample_phase::down,
            .key_code = 8,
            .logical_key = "Backspace",
        }),
    };

    const platform_input_dispatch_batch_result batch =
        translate_and_dispatch_platform_input_batch(engine, translator, requests);
    require(batch.items.size() == 2, "key char batch records two dispatch results");
    require_dispatched(batch.items[0], platform_input_source_kind::character, "character dispatches");
    require(batch.items[0].input_events.size() == 1, "character dispatch emits one event");
    const text_event& commit = require_event<text_event>(batch.items[0].input_events, 0);
    require(commit.kind == text_event_kind::commit, "character dispatch commits text");
    require(commit.utf8_text == committed_text, "character dispatch preserves utf8 text");
    require(batch.items[0].routing_diagnostics.action_routes[0].kind == action_route_policy_kind::text_commit_boundary,
        "character dispatch records commit boundary");

    require_dispatched(batch.items[1], platform_input_source_kind::key, "backspace key dispatches");
    require(batch.items[1].input_events.size() == 1, "backspace dispatch emits one event");
    const text_event& backspace = require_event<text_event>(batch.items[1].input_events, 0);
    require(backspace.kind == text_event_kind::backspace, "backspace dispatch emits text backspace");
    require(engine.text_model().text().empty(), "backspace dispatch mutates only input text model");
    require(batch.items[1].routing_diagnostics.action_routes[0].kind
            == action_route_policy_kind::text_backspace_boundary,
        "backspace dispatch records text backspace boundary");
}

void test_ime_batch_dispatches_existing_ime_path()
{
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    platform_input_translator translator;
    const std::array requests{
        request(platform_ime_composition_sample{
            .timestamp_ms = 400,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = utf8(u8"ㅎ"),
        }),
        request(platform_ime_composition_sample{
            .timestamp_ms = 430,
            .phase = platform_ime_sample_phase::commit,
            .utf8_text = utf8(u8"한"),
        }),
    };

    const platform_input_dispatch_batch_result batch =
        translate_and_dispatch_platform_input_batch(engine, translator, requests);
    require(batch.items.size() == 2, "ime batch records two dispatch results");
    require_dispatched(batch.items[0], platform_input_source_kind::ime, "ime preedit dispatches");
    require(batch.items[0].input_events.size() == 1, "ime preedit dispatch emits one event");
    const ime_event& preedit = require_event<ime_event>(batch.items[0].input_events, 0);
    require(preedit.kind == ime_event_kind::preedit, "ime preedit dispatch emits preedit");
    require(preedit.utf8_text == utf8(u8"ㅎ"), "ime preedit dispatch preserves text");
    require(batch.items[0].routing_diagnostics.action_routes[0].kind == action_route_policy_kind::ime_preedit,
        "ime preedit dispatch records policy");

    require_dispatched(batch.items[1], platform_input_source_kind::ime, "ime commit dispatches");
    require(batch.items[1].input_events.size() == 1, "ime commit dispatch emits one event");
    const ime_event& commit = require_event<ime_event>(batch.items[1].input_events, 0);
    require(commit.kind == ime_event_kind::commit, "ime commit dispatch emits commit");
    require(commit.utf8_text == utf8(u8"한"), "ime commit dispatch preserves text");
    require(engine.text_model().text() == utf8(u8"한"), "ime commit dispatch updates input text model");
    require(batch.items[1].routing_diagnostics.action_routes[0].kind == action_route_policy_kind::ime_commit,
        "ime commit dispatch records policy");
}

void test_rejected_translation_result_has_no_engine_side_effects()
{
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    platform_input_translator translator;
    const platform_input_dispatch_result accepted = translate_and_dispatch_platform_input(
        engine,
        translator,
        request(platform_character_sample{
            .timestamp_ms = 500,
            .utf8_text = "base",
        }));
    require_dispatched(accepted, platform_input_source_kind::character, "side-effect setup character dispatches");
    require(engine.text_model().text() == "base", "side-effect setup commits base text");
    const std::size_t action_route_count_before = engine.routing_diagnostics().action_routes.size();
    const std::size_t normalized_event_count_before = engine.routing_diagnostics().normalized_events.size();

    platform_input_dispatch_result rejected = translate_and_dispatch_platform_input(
        engine,
        translator,
        request(platform_character_sample{
            .timestamp_ms = 520,
            .utf8_text = std::string("\xC0\xAF", 2),
        }));
    require(!rejected.dispatched_to_engine, "rejected translation is not dispatched to engine");
    require(!rejected.translation.event.has_value(), "rejected translation carries no raw event");
    require(rejected.translation.diagnostic.status == platform_input_translation_status::rejected,
        "rejected translation records rejected status");
    require(rejected.translation.diagnostic.rejection_reason == platform_input_rejection_reason::invalid_utf8,
        "rejected translation records invalid utf8 reason");
    require(rejected.input_events.empty(), "rejected translation emits no input events");
    require(engine.text_model().text() == "base", "rejected translation leaves text model unchanged");
    require(engine.routing_diagnostics().action_routes.size() == action_route_count_before,
        "rejected translation leaves action route diagnostics unchanged");
    require(engine.routing_diagnostics().normalized_events.size() == normalized_event_count_before,
        "rejected translation leaves normalized diagnostics unchanged");
    require(rejected.routing_diagnostics.action_routes.size() == action_route_count_before,
        "rejected dispatch result snapshots unchanged action routes");
}

} // namespace

int main()
{
    test_mouse_click_batch_dispatches_pointer_path();
    test_touch_swipe_batch_dispatches_gesture_path();
    test_key_and_char_batch_dispatch_text_input_path();
    test_ime_batch_dispatches_existing_ime_path();
    test_rejected_translation_result_has_no_engine_side_effects();

    std::cout << "platform_input_engine_adapter_tests passed\n";
    return 0;
}
