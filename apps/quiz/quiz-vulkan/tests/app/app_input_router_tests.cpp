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

template <typename Payload>
const Payload* payload_if(const quiz_vulkan::app_input_route_result& result)
{
    if (!result.action.has_value()) {
        return nullptr;
    }

    return std::get_if<Payload>(&result.action->payload);
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

    const input::gesture_event tap{
        .kind = input::gesture_kind::tap,
        .timestamp_ms = 100,
        .pointer_id = 1,
        .x = 10.0f,
        .y = 10.0f,
    };

    const app_input_route_result result =
        route_normalized_input_event(input::input_event{tap}, scene_with_text_answer(), "button text");
    require(result.ok(), "pointer submit route succeeds");
    require(result.clear_text_after_action, "pointer submit requests text clear");
    const auto* payload = payload_if<domain::submit_text_answer_action>(result);
    require(payload != nullptr, "pointer submit creates text answer action");
    require(payload->answer_text == "button text", "pointer submit uses current committed text");
}

void test_route_errors_stay_in_app_layer()
{
    using namespace quiz_vulkan;

    scene::placed_scene placed;
    placed.input_regions.push_back(region(
        "broken",
        {0.0f, 0.0f, 50.0f, 50.0f},
        action("submit_option", "bad")));

    const input::gesture_event tap{
        .kind = input::gesture_kind::tap,
        .timestamp_ms = 100,
        .pointer_id = 1,
        .x = 10.0f,
        .y = 10.0f,
    };

    const app_input_route_result result = route_normalized_input_event(input::input_event{tap}, placed, {});
    require(!result.ok(), "bad scene action returns app route error");
    require(result.handled, "bad scene action is still handled");
    require(!result.action.has_value(), "bad scene action does not create domain action");
    require(contains(result.error, "submit_option"), "bad scene action explains router error");
}

} // namespace

int main()
{
    test_legacy_pointer_normalizes_to_tap_route();
    test_text_focus_and_commit_route();
    test_submit_key_routes_committed_text();
    test_pointer_text_submit_uses_current_committed_text();
    test_route_errors_stay_in_app_layer();
    return 0;
}
