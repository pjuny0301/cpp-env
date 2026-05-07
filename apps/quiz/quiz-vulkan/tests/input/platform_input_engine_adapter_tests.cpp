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

void test_batch_dispatch_preserves_order_and_skips_rejected_items()
{
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    platform_input_translator translator;
    const std::array requests{
        request(platform_character_sample{
            .timestamp_ms = 600,
            .utf8_text = "A",
        }),
        request(platform_character_sample{
            .timestamp_ms = 610,
            .utf8_text = std::string("\xC0\xAF", 2),
        }),
        request(platform_key_sample{
            .timestamp_ms = 620,
            .phase = platform_key_sample_phase::down,
            .key_code = 8,
            .logical_key = "Backspace",
        }),
        request(platform_wheel_sample{
            .timestamp_ms = 630,
            .x = 10.0f,
            .y = 12.0f,
            .delta_x = 0.0f,
            .delta_y = -1.0f,
            .unit = platform_scroll_delta_unit::lines,
        }),
    };

    const platform_input_dispatch_batch_result batch =
        translate_and_dispatch_platform_input_batch(engine, translator, requests);
    require(batch.items.size() == 4, "mixed batch records one result per request");

    require_dispatched(batch.items[0], platform_input_source_kind::character, "mixed batch character dispatches");
    require(batch.items[0].input_events.size() == 1, "mixed batch character emits one event");
    const text_event& commit = require_event<text_event>(batch.items[0].input_events, 0);
    require(commit.kind == text_event_kind::commit, "mixed batch first item is text commit");
    require(commit.utf8_text == "A", "mixed batch first item preserves commit text");

    require(!batch.items[1].dispatched_to_engine, "mixed batch rejected item is not dispatched");
    require(!batch.items[1].translation.event.has_value(), "mixed batch rejected item has no raw event");
    require(batch.items[1].translation.diagnostic.source == platform_input_source_kind::character,
        "mixed batch rejected item preserves source");
    require(batch.items[1].translation.diagnostic.status == platform_input_translation_status::rejected,
        "mixed batch rejected item records rejected status");
    require(batch.items[1].translation.diagnostic.rejection_reason == platform_input_rejection_reason::invalid_utf8,
        "mixed batch rejected item records invalid utf8");
    require(batch.items[1].input_events.empty(), "mixed batch rejected item emits no input events");
    require(batch.items[1].routing_diagnostics.action_routes.size() == 1,
        "mixed batch rejected item snapshots previous route count");

    require_dispatched(batch.items[2], platform_input_source_kind::key, "mixed batch key dispatches after reject");
    require(batch.items[2].input_events.size() == 1, "mixed batch key emits one event");
    const text_event& backspace = require_event<text_event>(batch.items[2].input_events, 0);
    require(backspace.kind == text_event_kind::backspace, "mixed batch third item is backspace");
    require(engine.text_model().text().empty(), "mixed batch rejected item does not alter text before backspace");

    require_dispatched(batch.items[3], platform_input_source_kind::wheel, "mixed batch wheel dispatches last");
    require(batch.items[3].input_events.size() == 1, "mixed batch wheel emits one event");
    const scroll_event& wheel = require_event<scroll_event>(batch.items[3].input_events, 0);
    require(wheel.timestamp_ms == 630, "mixed batch wheel preserves timestamp order");
    require(wheel.line_delta_y == -1.0f, "mixed batch wheel preserves line delta");
    require(batch.items[3].routing_diagnostics.normalized_events.back().kind == input_event_summary_kind::wheel,
        "mixed batch wheel records normalized wheel last");
}

void test_mixed_batch_summarizes_normalized_routes_and_clean_final_state()
{
    using namespace quiz_vulkan::input;

    input_engine engine;
    engine.focus_text_target("answer");
    platform_input_translator translator;
    const std::array requests{
        request(platform_mouse_sample{
            .timestamp_ms = 700,
            .pointer_id = 21,
            .phase = platform_pointer_sample_phase::down,
            .button = platform_mouse_button::primary,
            .x = 2.0f,
            .y = 4.0f,
        }),
        request(platform_mouse_sample{
            .timestamp_ms = 730,
            .pointer_id = 21,
            .phase = platform_pointer_sample_phase::up,
            .button = platform_mouse_button::primary,
            .x = 3.0f,
            .y = 5.0f,
        }),
        request(platform_wheel_sample{
            .timestamp_ms = 740,
            .x = 20.0f,
            .y = 30.0f,
            .delta_x = 0.0f,
            .delta_y = -2.0f,
            .unit = platform_scroll_delta_unit::lines,
        }),
        request(platform_character_sample{
            .timestamp_ms = 750,
            .utf8_text = "x",
        }),
        request(platform_ime_composition_sample{
            .timestamp_ms = 760,
            .phase = platform_ime_sample_phase::preedit_update,
            .utf8_text = utf8(u8"ㅎ"),
        }),
        request(platform_ime_composition_sample{
            .timestamp_ms = 770,
            .phase = platform_ime_sample_phase::commit,
            .utf8_text = utf8(u8"한"),
        }),
    };

    const platform_input_dispatch_batch_result batch =
        translate_and_dispatch_platform_input_batch(engine, translator, requests);
    require(batch.items.size() == 6, "summary mixed batch records all dispatch results");
    require(engine.text_model().text() == std::string("x") + utf8(u8"한"),
        "summary mixed batch commits text and ime once");

    const input_diagnostic_summary& summary = batch.summary;
    require(summary.normalized_event_count == 2, "summary mixed batch counts normalized events");
    require(summary.normalized_events.tap == 1, "summary mixed batch counts tap");
    require(summary.normalized_events.wheel == 1, "summary mixed batch counts wheel");
    require(summary.normalized_events.swipe_left == 0, "summary mixed batch leaves absent swipe count zero");
    require(summary.routes.pointer == 2, "summary mixed batch counts pointer routes");
    require(summary.routes.wheel == 1, "summary mixed batch counts wheel routes");
    require(summary.routes.text == 1, "summary mixed batch counts text routes");
    require(summary.routes.ime == 2, "summary mixed batch counts ime routes");
    require(summary.routes.focus == 0, "summary mixed batch has no focus routes");
    require(summary.routes.total == 6, "summary mixed batch counts all accepted routes");
    require(summary.pointer_capture_ended_cleanly, "summary mixed batch ends with no pointer capture");
    require(summary.focus_ended_cleanly, "summary mixed batch leaves focus state clean");
    require(summary.preedit_ended_cleanly, "summary mixed batch clears preedit after ime commit");
}

} // namespace

int main()
{
    test_mouse_click_batch_dispatches_pointer_path();
    test_touch_swipe_batch_dispatches_gesture_path();
    test_key_and_char_batch_dispatch_text_input_path();
    test_ime_batch_dispatches_existing_ime_path();
    test_rejected_translation_result_has_no_engine_side_effects();
    test_batch_dispatch_preserves_order_and_skips_rejected_items();
    test_mixed_batch_summarizes_normalized_routes_and_clean_final_state();

    std::cout << "platform_input_engine_adapter_tests passed\n";
    return 0;
}
