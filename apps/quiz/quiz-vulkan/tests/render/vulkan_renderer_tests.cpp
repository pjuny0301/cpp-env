#include "core/domain/app_snapshot.hpp"
#include "core/layout/layout_placer.h"
#include "core/ui/quiz_screens.h"
#include "core/ui/ui_renderer.h"
#include "render/vulkan/vulkan_renderer.h"

#include <cassert>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

class fixed_text_metrics final : public quiz_vulkan::scene::text_metrics_interface {
public:
    quiz_vulkan::scene::scene_size measure_text(
        const std::vector<quiz_vulkan::scene::scene_text_run>& text_runs,
        const quiz_vulkan::scene::scene_style&,
        float) const override
    {
        std::size_t character_count = 0;
        for (const auto& run : text_runs) {
            character_count += run.text.size();
        }
        return quiz_vulkan::scene::scene_size{static_cast<float>(character_count * 8), 18.0f};
    }
};

void require(bool condition, const char* message)
{
    assert((condition) && message);
}

quiz_vulkan::scene::placed_scene make_simple_scene_snapshot()
{
    using namespace quiz_vulkan;

    scene::placed_scene snapshot;

    scene::placed_scene_node panel;
    panel.id = "panel";
    panel.bounds = scene::scene_rect{20.0f, 16.0f, 260.0f, 120.0f};
    panel.content_bounds = scene::scene_rect{32.0f, 28.0f, 236.0f, 96.0f};
    panel.style.background_color = "#24384f";
    panel.style.foreground_color = "#f4f8ff";
    panel.style.border_radius = 6.0f;
    panel.text_runs.push_back(scene::scene_text_run{"Renderer smoke", "title"});
    snapshot.nodes.push_back(std::move(panel));

    return snapshot;
}

quiz_vulkan::domain::deck make_renderer_smoke_deck()
{
    using namespace quiz_vulkan::domain;

    question question;
    question.id = "q-render-contract";
    question.prompt = "Which native path turns an app snapshot into pixels?";
    question.long_text = std::optional<std::string>{
        "The renderer should consume retained scene layout and draw commands without owning quiz state."};
    question.image_uri = std::optional<std::string>{"fixture://renderer/app-snapshot-card.png"};
    question.type = question_type::answer;
    question.options.push_back(option{"Domain snapshot to retained scene", true});
    question.options.push_back(option{"UI state directly owned by Vulkan", false});
    question.options.push_back(option{"Blank frame with no draw commands", false});

    day quiz_day;
    quiz_day.id = "day-render";
    quiz_day.title = "Renderer Contract";
    quiz_day.questions.push_back(std::move(question));

    deck quiz_deck;
    quiz_deck.id = "deck-render";
    quiz_deck.title = "Native Renderer Smoke";
    quiz_deck.source_uri = "fixture://renderer";
    quiz_deck.days.push_back(std::move(quiz_day));
    return quiz_deck;
}

quiz_vulkan::domain::quiz_session make_renderer_smoke_session(const quiz_vulkan::domain::deck& deck)
{
    const quiz_vulkan::domain::learning_state_map learning;
    const quiz_vulkan::domain::previous_answer_map previous_answers;

    quiz_vulkan::domain::quiz_session_options options;
    options.day_id = std::optional<std::string>{"day-render"};
    return quiz_vulkan::domain::start_quiz_session(deck, learning, previous_answers, options);
}

bool has_command(
    const quiz_vulkan::ui::ui_draw_list& draw_list,
    quiz_vulkan::ui::ui_draw_command_type type,
    const std::string& node_id)
{
    for (const quiz_vulkan::ui::ui_draw_command& command : draw_list.commands) {
        if (command.type == type && command.node_id == node_id) {
            return true;
        }
    }
    return false;
}

void test_scene_snapshot_submission()
{
    using namespace quiz_vulkan;

    const scene::placed_scene snapshot = make_simple_scene_snapshot();
    const ui::ui_draw_list expected_draw_list = ui::ui_renderer{}.build_draw_list(snapshot);
    assert(!expected_draw_list.empty());

    render::vulkan_renderer renderer;
    renderer.submit(snapshot);

    const render::vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    assert(stats.command_count == expected_draw_list.size());
    assert(stats.draw_call_count == stats.quad_count + stats.text_count + stats.image_count + stats.debug_bounds_count);
    assert(stats.visible_draw_call_count == stats.draw_call_count);
    assert(stats.quad_count == 1);
    assert(stats.text_count == 1);
    assert(stats.text_run_count == 1);
    assert(stats.text_character_count == std::string{"Renderer smoke"}.size());

    const render::vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    assert(summary.used_cpu_fallback());
    assert(summary.nonblank());
    assert(summary.surface_width == renderer.options().fallback_surface_width);
    assert(summary.surface_height == renderer.options().fallback_surface_height);
    assert(summary.shaded_pixel_count > 0);
    assert(summary.discarded_draw_call_count == 0);

    renderer.clear();
    assert(renderer.last_draw_list().empty());
    assert(renderer.last_frame_stats().empty());
    assert(!renderer.last_frame_summary().nonblank());
}

void test_cpu_fallback_clips_and_discards()
{
    using namespace quiz_vulkan;

    ui::ui_draw_command clip;
    clip.type = ui::ui_draw_command_type::push_clip;
    clip.node_id = "clip";
    clip.bounds = scene::scene_rect{0.0f, 0.0f, 50.0f, 50.0f};
    clip.content_bounds = clip.bounds;

    ui::ui_draw_command quad;
    quad.type = ui::ui_draw_command_type::quad;
    quad.node_id = "quad";
    quad.bounds = scene::scene_rect{25.0f, 25.0f, 50.0f, 50.0f};
    quad.content_bounds = quad.bounds;
    quad.paint = ui::ui_paint{"#00ff00", ui::ui_color{0.0f, 1.0f, 0.0f, 1.0f}};

    ui::ui_draw_command transparent;
    transparent.type = ui::ui_draw_command_type::quad;
    transparent.node_id = "transparent";
    transparent.bounds = scene::scene_rect{10.0f, 10.0f, 10.0f, 10.0f};
    transparent.content_bounds = transparent.bounds;
    transparent.paint = ui::ui_paint{"transparent", ui::ui_color{0.0f, 0.0f, 0.0f, 0.0f}};

    ui::ui_draw_command pop;
    pop.type = ui::ui_draw_command_type::pop_clip;
    pop.node_id = "clip";
    pop.bounds = clip.bounds;
    pop.content_bounds = clip.bounds;

    render::vulkan_renderer_options options;
    options.viewport = scene::scene_rect{0.0f, 0.0f, 100.0f, 100.0f};
    options.fallback_surface_width = 10;
    options.fallback_surface_height = 10;

    render::vulkan_renderer renderer(options);
    renderer.submit(std::vector<ui::ui_draw_command>{clip, quad, transparent, pop});

    const render::vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    assert(stats.command_count == 4);
    assert(stats.draw_call_count == 2);
    assert(stats.visible_draw_call_count == 1);
    assert(stats.push_clip_count == 1);
    assert(stats.pop_clip_count == 1);

    const render::vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    assert(summary.used_cpu_fallback());
    assert(summary.nonblank());
    assert(summary.clipped_draw_call_count == 1);
    assert(summary.discarded_draw_call_count == 1);
    assert(summary.shaded_pixel_count == 9);
}

void test_app_snapshot_renders_nonblank_frame()
{
    using namespace quiz_vulkan;

    domain::deck deck = make_renderer_smoke_deck();
    domain::quiz_session session = make_renderer_smoke_session(deck);
    const domain::learning_state_map learning;
    const std::vector<domain::deck> decks{deck};
    const domain::app_snapshot snapshot = domain::make_app_snapshot(
        decks,
        std::optional<std::string>{"deck-render"},
        std::optional<std::string>{"day-render"},
        &session,
        learning);

    require(snapshot.screen == domain::app_screen::quiz, "active app snapshot selected quiz screen");

    scene::scene_layout_data scene_data("renderer_app_snapshot");
    const scene::scene_layout_apply_result apply_result = ui::make_quiz_screen_patch(snapshot).apply_to(scene_data);
    require(apply_result.applied(), "app snapshot screen patch applies");
    require(scene_data.route_state().screen_id == "quiz_active", "quiz active route emitted");
    require(scene_data.contains_node("quiz_active_prompt"), "prompt node exists before placement");
    require(scene_data.contains_node("quiz_active_option_0"), "first option node exists before placement");
    require(scene_data.contains_node("quiz_active_image"), "image node exists before placement");

    fixed_text_metrics metrics;
    scene::scene_layout_environment environment;
    environment.viewport = scene::scene_rect{0.0f, 0.0f, 420.0f, 720.0f};
    environment.safe_area = scene::scene_edges{0.0f, 24.0f, 0.0f, 16.0f};

    const scene::placed_scene placed_scene = scene::layout_placer{}.place_with_environment(
        scene_data,
        environment,
        metrics);

    const scene::placed_scene_node* screen_root = placed_scene.find_node("quiz_screens_root");
    const scene::placed_scene_node* prompt = placed_scene.find_node("quiz_active_prompt");
    const scene::placed_scene_node* first_option = placed_scene.find_node("quiz_active_option_0");
    const scene::placed_scene_node* image = placed_scene.find_node("quiz_active_image");
    require(screen_root != nullptr && screen_root->bounds.width > 0.0f && screen_root->bounds.height > 0.0f, "screen root placed with visible bounds");
    require(prompt != nullptr && prompt->content_bounds.width > 0.0f && prompt->content_bounds.height > 0.0f, "prompt placed with visible content");
    require(first_option != nullptr && first_option->has_action_binding && first_option->input_enabled, "first option remains interactive");
    require(image != nullptr && image->has_image && image->content_bounds.width > 0.0f && image->content_bounds.height > 0.0f, "image placed with visible content");
    require(placed_scene.input_regions.size() >= 5, "placed scene exposes option and control input regions");

    const ui::ui_draw_list expected_draw_list = ui::ui_renderer{}.build_draw_list(placed_scene);
    require(!expected_draw_list.empty(), "placed app scene produces draw commands");
    require(has_command(expected_draw_list, ui::ui_draw_command_type::text, "quiz_active_prompt"), "draw list includes prompt text");
    require(has_command(expected_draw_list, ui::ui_draw_command_type::image, "quiz_active_image"), "draw list includes question image");
    require(has_command(expected_draw_list, ui::ui_draw_command_type::quad, "quiz_active_option_0"), "draw list includes option background");
    require(has_command(expected_draw_list, ui::ui_draw_command_type::text, "quiz_active_option_0"), "draw list includes option label");

    render::vulkan_renderer_options options;
    options.viewport = environment.viewport;
    options.fallback_surface_width = 84;
    options.fallback_surface_height = 144;

    render::vulkan_renderer renderer(options);
    renderer.submit(placed_scene);

    const render::vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    require(stats.command_count == expected_draw_list.size(), "renderer consumed the placed-scene draw list");
    require(stats.draw_call_count >= 18, "app snapshot produces a substantial draw call set");
    require(stats.visible_draw_call_count == stats.draw_call_count, "all app draw calls are visible");
    require(stats.quad_count >= 7, "app frame includes background and button quads");
    require(stats.text_count >= 9, "app frame includes header, prompt, option, and control text");
    require(stats.image_count == 1, "app frame includes the question image");
    require(stats.push_clip_count == 1 && stats.pop_clip_count == 1, "screen root clip scope is balanced");
    require(stats.text_character_count > 180, "app frame text payload is nontrivial");

    const render::vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    const std::size_t fallback_surface_area = options.fallback_surface_width * options.fallback_surface_height;
    require(summary.used_cpu_fallback(), "renderer used current CPU fallback backend");
    require(summary.nonblank(), "app snapshot renders a nonblank frame");
    require(summary.surface_width == options.fallback_surface_width, "fallback surface width recorded");
    require(summary.surface_height == options.fallback_surface_height, "fallback surface height recorded");
    require(summary.shaded_pixel_count > fallback_surface_area / 2, "app frame shades a meaningful portion of the surface");
    require(summary.discarded_draw_call_count == 0, "no app draw calls were discarded");
}

} // namespace

int main()
{
    test_scene_snapshot_submission();
    test_cpu_fallback_clips_and_discards();
    test_app_snapshot_renders_nonblank_frame();
    return 0;
}
