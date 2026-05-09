#include "app/app_input_router.h"
#include "core/input/input_engine.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "app_input_router_tests failed: " << message << '\n';
    std::exit(1);
}

bool contains(std::string_view value, std::string_view needle)
{
    return value.find(needle) != std::string_view::npos;
}

quiz_vulkan::scene::scene_action_binding action(std::string action_type, std::string payload = {})
{
    quiz_vulkan::scene::scene_action_binding binding;
    binding.action_type = std::move(action_type);
    binding.payload = std::move(payload);
    return binding;
}

quiz_vulkan::scene::scene_input_region region(
    std::string node_id,
    quiz_vulkan::scene::scene_rect bounds,
    quiz_vulkan::scene::scene_action_binding binding)
{
    return quiz_vulkan::scene::scene_input_region{
        .node_id = std::move(node_id),
        .bounds = bounds,
        .action = std::move(binding),
        .enabled = true,
    };
}

quiz_vulkan::scene::placed_scene scene_with_button()
{
    quiz_vulkan::scene::placed_scene placed;
    placed.input_regions.push_back(region(
        "option_2",
        {10.0f, 20.0f, 80.0f, 40.0f},
        action("submit_option", "2")));
    return placed;
}

quiz_vulkan::scene::placed_scene scene_with_text_answer()
{
    quiz_vulkan::scene::placed_scene placed;

    quiz_vulkan::scene::placed_scene_node input_node;
    input_node.id = "answer";
    input_node.visible = true;
    input_node.input_enabled = true;
    input_node.semantics.quiz.accepts_keyboard_input = true;
    placed.nodes.push_back(std::move(input_node));

    placed.input_regions.push_back(region(
        "submit",
        {0.0f, 0.0f, 120.0f, 60.0f},
        action("submit_text_answer", "answer")));
    return placed;
}

quiz_vulkan::input::gesture_event gesture(quiz_vulkan::input::gesture_kind kind)
{
    return quiz_vulkan::input::gesture_event{
        .kind = kind,
        .timestamp_ms = 100,
        .pointer_id = 1,
        .x = 10.0f,
        .y = 10.0f,
    };
}

template <typename Payload>
const Payload* payload_if(const quiz_vulkan::app_input_route_result& result)
{
    if (!result.action.has_value()) {
        return nullptr;
    }

    return std::get_if<Payload>(&result.action->payload);
}

template <typename Payload>
const Payload* raw_payload_if(const quiz_vulkan::raw_platform_input_event& event)
{
    return std::get_if<Payload>(&event);
}

std::vector<quiz_vulkan::input::input_event> normalize_through_engine(
    quiz_vulkan::input::input_engine& engine,
    const quiz_vulkan::platform_input_event& platform_event)
{
    std::vector<quiz_vulkan::input::input_event> events;
    for (const quiz_vulkan::raw_platform_input_event& raw_event :
        quiz_vulkan::normalize_platform_input_event(platform_event, 100)) {
        std::vector<quiz_vulkan::input::input_event> normalized = engine.process_raw_event(raw_event);
        events.insert(events.end(), normalized.begin(), normalized.end());
    }
    return events;
}

void test_platform_pointer_lifecycle_normalizes_without_synthetic_tap()
{
    using namespace quiz_vulkan;

    const std::vector<raw_platform_input_event> down_events = normalize_platform_input_event(
        platform_input_event{
            .type = platform_input_event_type::pointer_down,
            .x = 12.0f,
            .y = 24.0f,
            .pointer_id = 7,
            .pointer_button = raw_platform_pointer_button::secondary,
        },
        100);
    require(down_events.size() == 1, "pointer down normalizes to one raw pointer event");
    const auto* down = raw_payload_if<raw_platform_pointer_event>(down_events.front());
    require(down != nullptr, "pointer down produces raw pointer payload");
    require(down->phase == raw_platform_pointer_phase::down, "pointer down preserves down phase");
    require(down->pointer_id == 7, "pointer down preserves pointer id");
    require(down->button == raw_platform_pointer_button::secondary, "pointer down preserves button");

    const std::vector<raw_platform_input_event> move_events = normalize_platform_input_event(
        platform_input_event{
            .type = platform_input_event_type::pointer_move,
            .x = 30.0f,
            .y = 32.0f,
            .pointer_id = 7,
        },
        120);
    const auto* move = raw_payload_if<raw_platform_pointer_event>(move_events.front());
    require(move != nullptr, "pointer move produces raw pointer payload");
    require(move->phase == raw_platform_pointer_phase::move, "pointer move preserves move phase");

    const std::vector<raw_platform_input_event> up_events = normalize_platform_input_event(
        platform_input_event{
            .type = platform_input_event_type::pointer_up,
            .x = 34.0f,
            .y = 36.0f,
            .pointer_id = 7,
        },
        160);
    const auto* up = raw_payload_if<raw_platform_pointer_event>(up_events.front());
    require(up != nullptr, "pointer up produces raw pointer payload");
    require(up->phase == raw_platform_pointer_phase::up, "pointer up preserves up phase");
}

void test_platform_wheel_and_key_events_normalize_to_raw_input()
{
    using namespace quiz_vulkan;

    const std::vector<raw_platform_input_event> wheel_events = normalize_platform_input_event(
        platform_input_event{
            .type = platform_input_event_type::mouse_wheel,
            .x = 10.0f,
            .y = 20.0f,
            .delta_x = 1.0f,
            .delta_y = -2.0f,
            .scroll_unit = raw_platform_scroll_delta_unit::lines,
        },
        200);
    require(wheel_events.size() == 1, "wheel normalizes to one raw scroll event");
    const auto* wheel = raw_payload_if<raw_platform_scroll_event>(wheel_events.front());
    require(wheel != nullptr, "wheel produces raw scroll payload");
    require(wheel->delta_y == -2.0f, "wheel preserves y delta");
    require(wheel->unit == raw_platform_scroll_delta_unit::lines, "wheel preserves delta unit");

    input::input_engine engine;
    const std::vector<input::input_event> scroll_events = engine.process_raw_event(wheel_events.front());
    require(scroll_events.size() == 1, "wheel reaches input engine as one scroll event");
    require(std::get_if<input::scroll_event>(&scroll_events.front()) != nullptr,
        "wheel reaches input engine as scroll payload");

    const std::vector<raw_platform_input_event> key_events = normalize_platform_input_event(
        platform_input_event{
            .type = platform_input_event_type::key_down,
            .key_code = 46,
            .logical_key = "Delete",
            .shift = true,
            .repeat = true,
        },
        220);
    require(key_events.size() == 1, "key down normalizes to one raw key event");
    const auto* key = raw_payload_if<raw_platform_key_event>(key_events.front());
    require(key != nullptr, "key down produces raw key payload");
    require(key->phase == raw_platform_key_phase::down, "key down preserves down phase");
    require(key->logical_key == "Delete", "key down preserves logical key");
    require(key->shift, "key down preserves shift modifier");
    require(key->repeat, "key down preserves repeat flag");
}

void test_pointer_hold_update_time_routes_long_press()
{
    using namespace quiz_vulkan;

    input::input_engine engine;
    const std::vector<raw_platform_input_event> down_events = normalize_platform_input_event(
        platform_input_event{
            .type = platform_input_event_type::pointer_down,
            .x = 16.0f,
            .y = 24.0f,
        },
        1000);
    require(down_events.size() == 1, "hold starts with one pointer down");
    require(engine.process_raw_event(down_events.front()).empty(), "pointer down alone emits no gesture");

    const std::vector<input::input_event> held_events = engine.update_time(1700);
    require(held_events.size() == 1, "time update emits long press while pointer is held");
    const auto* long_press = std::get_if<input::gesture_event>(&held_events.front());
    require(long_press != nullptr, "held pointer produces gesture payload");
    require(long_press->kind == input::gesture_kind::long_press, "held pointer produces long press");

    const app_input_route_result result = route_normalized_input_event(held_events.front(), {}, {});
    require(result.ok(), "held long press route succeeds");
    require(payload_if<domain::mark_question_unknown_action>(result) != nullptr,
        "held long press routes mark unknown");
}

void test_legacy_pointer_normalizes_to_tap_route()
{
    using namespace quiz_vulkan;

    input::input_engine engine;
    const platform_input_event platform_event{
        .type = platform_input_event_type::pointer_press,
        .x = 20.0f,
        .y = 30.0f,
    };

    const std::vector<input::input_event> events = normalize_through_engine(engine, platform_event);
    require(events.size() == 1, "legacy pointer press normalizes to one tap");

    const app_input_route_result result = route_normalized_input_event(events.front(), scene_with_button(), {});
    require(result.ok(), "tap route succeeds");
    require(result.handled, "tap route is handled");
    require(result.needs_render, "tap route requests render");
    const auto* payload = payload_if<domain::submit_option_action>(result);
    require(payload != nullptr, "tap route creates submit_option action");
    require(payload->option_index == 2, "tap route preserves option index");
}

void test_text_focus_and_commit_route()
{
    using namespace quiz_vulkan;

    const scene::placed_scene placed = scene_with_text_answer();
    const std::optional<std::string> target = keyboard_input_target(placed);
    require(target.has_value(), "keyboard target is found");
    require(*target == "answer", "keyboard target uses input node id");

    input::input_engine engine;
    engine.focus_text_target(*target);
    const platform_input_event platform_event{
        .type = platform_input_event_type::text_input,
        .text = "native",
    };

    const std::vector<input::input_event> events = normalize_through_engine(engine, platform_event);
    require(events.size() == 1, "legacy text input normalizes to one text event");
    require(engine.text_model().text() == "native", "input engine stores committed text");

    const app_input_route_result result =
        route_normalized_input_event(events.front(), placed, engine.text_model().text());
    require(result.ok(), "text commit route succeeds");
    require(result.handled, "text commit is handled");
    require(result.needs_render, "text commit requests render");
    require(!result.action.has_value(), "text commit does not dispatch domain action");
}

void test_submit_key_routes_committed_text()
{
    using namespace quiz_vulkan;

    const scene::placed_scene placed = scene_with_text_answer();
    input::input_engine engine;
    engine.focus_text_target("answer");
    require(!normalize_through_engine(engine, platform_input_event{
                 .type = platform_input_event_type::text_input,
                 .text = "typed answer",
             })
                 .empty(),
        "text input commits before submit");

    const std::vector<input::input_event> events = normalize_through_engine(engine, platform_input_event{
        .type = platform_input_event_type::text_submit,
    });
    require(events.size() == 1, "legacy submit normalizes to one submit event");

    const app_input_route_result result =
        route_normalized_input_event(events.front(), placed, engine.text_model().text());
    require(result.ok(), "submit route succeeds");
    require(result.handled, "submit is handled");
    require(result.clear_text_after_action, "submit requests text clear");
    const auto* payload = payload_if<domain::submit_text_answer_action>(result);
    require(payload != nullptr, "submit route creates text answer action");
    require(payload->answer_text == "typed answer", "submit route uses submitted event text");
}

void test_pointer_text_submit_uses_current_committed_text()
{
    using namespace quiz_vulkan;

    const app_input_route_result result =
        route_normalized_input_event(input::input_event{gesture(input::gesture_kind::tap)}, scene_with_text_answer(), "button text");
    require(result.ok(), "pointer submit route succeeds");
    require(result.clear_text_after_action, "pointer submit requests text clear");
    const auto* payload = payload_if<domain::submit_text_answer_action>(result);
    require(payload != nullptr, "pointer submit creates text answer action");
    require(payload->answer_text == "button text", "pointer submit uses current committed text");
}

void test_swipe_left_routes_previous_question()
{
    using namespace quiz_vulkan;

    const app_input_route_result result =
        route_normalized_input_event(input::input_event{gesture(input::gesture_kind::swipe_left)}, {}, {});
    require(result.ok(), "swipe left route succeeds");
    require(result.handled, "swipe left is handled");
    require(result.needs_render, "swipe left requests render");
    require(!result.clear_text_after_action, "swipe left does not clear text");
    require(payload_if<domain::previous_question_action>(result) != nullptr, "swipe left routes previous question");
}

void test_swipe_right_prefers_scene_continue_then_skip()
{
    using namespace quiz_vulkan;

    scene::placed_scene feedback_scene;
    feedback_scene.input_regions.push_back(region(
        "continue",
        {0.0f, 0.0f, 80.0f, 40.0f},
        action("continue_after_feedback")));
    const app_input_route_result continue_result =
        route_normalized_input_event(input::input_event{gesture(input::gesture_kind::swipe_right)}, feedback_scene, {});
    require(continue_result.ok(), "swipe right continue route succeeds");
    require(payload_if<domain::continue_after_feedback_action>(continue_result) != nullptr,
        "swipe right uses scene continue action when present");

    scene::placed_scene active_scene;
    active_scene.input_regions.push_back(region(
        "skip",
        {0.0f, 0.0f, 80.0f, 40.0f},
        action("skip_question")));
    const app_input_route_result skip_result =
        route_normalized_input_event(input::input_event{gesture(input::gesture_kind::swipe_right)}, active_scene, {});
    require(skip_result.ok(), "swipe right skip route succeeds");
    require(payload_if<domain::skip_question_action>(skip_result) != nullptr,
        "swipe right uses scene skip action when continue is absent");
}

void test_long_press_routes_mark_unknown()
{
    using namespace quiz_vulkan;

    const app_input_route_result result =
        route_normalized_input_event(input::input_event{gesture(input::gesture_kind::long_press)}, {}, {});
    require(result.ok(), "long press route succeeds");
    require(result.handled, "long press is handled");
    require(result.needs_render, "long press requests render");
    require(payload_if<domain::mark_question_unknown_action>(result) != nullptr,
        "long press routes mark question unknown");
}

void test_route_errors_stay_in_app_layer()
{
    using namespace quiz_vulkan;

    scene::placed_scene placed;
    placed.input_regions.push_back(region(
        "broken",
        {0.0f, 0.0f, 50.0f, 50.0f},
        action("submit_option", "bad")));

    const app_input_route_result result =
        route_normalized_input_event(input::input_event{gesture(input::gesture_kind::tap)}, placed, {});
    require(!result.ok(), "bad scene action returns app route error");
    require(result.handled, "bad scene action is still handled");
    require(!result.action.has_value(), "bad scene action does not create domain action");
    require(contains(result.error, "submit_option"), "bad scene action explains router error");
}

} // namespace

int main()
{
    test_platform_pointer_lifecycle_normalizes_without_synthetic_tap();
    test_platform_wheel_and_key_events_normalize_to_raw_input();
    test_pointer_hold_update_time_routes_long_press();
    test_legacy_pointer_normalizes_to_tap_route();
    test_text_focus_and_commit_route();
    test_submit_key_routes_committed_text();
    test_pointer_text_submit_uses_current_committed_text();
    test_swipe_left_routes_previous_question();
    test_swipe_right_prefers_scene_continue_then_skip();
    test_long_press_routes_mark_unknown();
    test_route_errors_stay_in_app_layer();
    return 0;
}
