#include "core/domain/app_snapshot.hpp"
#include "core/layout/layout_placer.h"
#include "core/ui/quiz_screens.h"
#include "core/ui/ui_renderer.h"
#include "render/vulkan/vulkan_renderer.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
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
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

std::size_t framebuffer_pixel_offset(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer,
    std::size_t x,
    std::size_t y)
{
    return (y * framebuffer.width + x) * 4;
}

std::size_t count_nonzero_framebuffer_pixels(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer)
{
    std::size_t count = 0;
    for (std::size_t offset = 0; offset + 3 < framebuffer.rgba.size(); offset += 4) {
        if (framebuffer.rgba[offset] != 0 || framebuffer.rgba[offset + 1] != 0
            || framebuffer.rgba[offset + 2] != 0 || framebuffer.rgba[offset + 3] != 0) {
            ++count;
        }
    }
    return count;
}

bool framebuffer_pixel_is(
    const quiz_vulkan::render::vulkan_renderer_framebuffer& framebuffer,
    std::size_t x,
    std::size_t y,
    unsigned char red,
    unsigned char green,
    unsigned char blue,
    unsigned char alpha)
{
    if (x >= framebuffer.width || y >= framebuffer.height) {
        return false;
    }

    const std::size_t offset = framebuffer_pixel_offset(framebuffer, x, y);
    return offset + 3 < framebuffer.rgba.size()
        && framebuffer.rgba[offset] == red
        && framebuffer.rgba[offset + 1] == green
        && framebuffer.rgba[offset + 2] == blue
        && framebuffer.rgba[offset + 3] == alpha;
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

quiz_vulkan::scene::placed_scene_node make_semantic_draw_node(
    std::string id,
    float y,
    quiz_vulkan::scene::scene_node_role role,
    std::string label)
{
    using namespace quiz_vulkan;

    scene::placed_scene_node node;
    node.id = std::move(id);
    node.bounds = scene::scene_rect{24.0f, y, 272.0f, 44.0f};
    node.content_bounds = scene::scene_rect{36.0f, y + 10.0f, 248.0f, 24.0f};
    node.style.background_color = "#203447";
    node.style.foreground_color = "#ffffff";
    node.style.border_radius = 6.0f;
    node.text_runs.push_back(scene::scene_text_run{std::move(label), "semantic"});
    node.semantics.role = role;
    return node;
}

quiz_vulkan::scene::placed_scene make_quiz_semantic_scene()
{
    using namespace quiz_vulkan;

    scene::placed_scene placed_scene;
    placed_scene.environment.viewport = scene::scene_rect{0.0f, 0.0f, 320.0f, 320.0f};
    placed_scene.usable_bounds = placed_scene.environment.viewport;

    scene::placed_scene_node enabled_option = make_semantic_draw_node(
        "semantic_option_enabled",
        24.0f,
        scene::scene_node_role::quiz_option,
        "Enabled option");
    enabled_option.has_action_binding = true;
    enabled_option.action_binding.action_type = "submit_option";
    enabled_option.action_binding.payload = "0";
    enabled_option.input_enabled = true;
    enabled_option.semantics.quiz.stage = scene::scene_quiz_stage::question;
    enabled_option.semantics.quiz.option_index = 0;

    scene::placed_scene_node disabled_option = make_semantic_draw_node(
        "semantic_option_disabled",
        80.0f,
        scene::scene_node_role::quiz_option,
        "Disabled option");
    disabled_option.input_enabled = false;
    disabled_option.semantics.quiz.stage = scene::scene_quiz_stage::feedback;
    disabled_option.semantics.quiz.option_index = 1;
    disabled_option.semantics.quiz.option_state = scene::scene_quiz_option_state::disabled;

    scene::placed_scene_node feedback = make_semantic_draw_node(
        "semantic_feedback",
        136.0f,
        scene::scene_node_role::quiz_feedback,
        "Correct");
    feedback.style.background_color = "#28503a";
    feedback.semantics.quiz.stage = scene::scene_quiz_stage::feedback;
    feedback.semantics.quiz.feedback = scene::scene_quiz_feedback_state::correct;

    scene::placed_scene_node answer_dock;
    answer_dock.id = "semantic_answer_dock";
    answer_dock.bounds = scene::scene_rect{24.0f, 192.0f, 272.0f, 68.0f};
    answer_dock.content_bounds = scene::scene_rect{36.0f, 204.0f, 248.0f, 44.0f};
    answer_dock.style.background_color = "#1c2936";
    answer_dock.style.border_radius = 6.0f;
    answer_dock.semantics.role = scene::scene_node_role::quiz_answer_dock;
    answer_dock.semantics.quiz.stage = scene::scene_quiz_stage::question;

    scene::placed_scene_node answer_input = make_semantic_draw_node(
        "semantic_answer_input",
        204.0f,
        scene::scene_node_role::quiz_answer_input,
        "Type answer");
    answer_input.parent_id = answer_dock.id;
    answer_input.depth = 1;
    answer_input.bounds = scene::scene_rect{36.0f, 204.0f, 248.0f, 44.0f};
    answer_input.content_bounds = scene::scene_rect{48.0f, 214.0f, 224.0f, 24.0f};
    answer_input.has_action_binding = true;
    answer_input.action_binding.trigger = scene::scene_action_trigger::change;
    answer_input.action_binding.action_type = "submit_text_answer";
    answer_input.action_binding.payload = "q-semantic";
    answer_input.input_enabled = true;
    answer_input.semantics.quiz.stage = scene::scene_quiz_stage::question;
    answer_input.semantics.quiz.accepts_keyboard_input = true;

    placed_scene.nodes.push_back(std::move(enabled_option));
    placed_scene.nodes.push_back(std::move(disabled_option));
    placed_scene.nodes.push_back(std::move(feedback));
    placed_scene.nodes.push_back(std::move(answer_dock));
    placed_scene.nodes.push_back(std::move(answer_input));
    return placed_scene;
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
    // The Vulkan boundary intentionally consumes UI draw commands, not placed_scene.
    renderer.submit(expected_draw_list);

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
    assert(renderer.last_framebuffer().width == 0);
    assert(renderer.last_framebuffer().height == 0);
    assert(renderer.last_framebuffer().rgba.empty());
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
    assert(stats.input_enabled_draw_node_count == 0);
    assert(stats.disabled_draw_node_count == 0);
    assert(stats.quiz_option_draw_command_count == 0);
    assert(stats.quiz_feedback_draw_command_count == 0);
    assert(stats.quiz_answer_input_draw_command_count == 0);
    assert(stats.quiz_answer_dock_draw_command_count == 0);

    const render::vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    assert(summary.used_cpu_fallback());
    assert(summary.nonblank());
    assert(summary.clipped_draw_call_count == 1);
    assert(summary.discarded_draw_call_count == 1);
    assert(summary.shaded_pixel_count == 9);

    const render::vulkan_renderer_framebuffer& framebuffer = renderer.last_framebuffer();
    require(framebuffer.width == options.fallback_surface_width, "framebuffer records fallback width");
    require(framebuffer.height == options.fallback_surface_height, "framebuffer records fallback height");
    require(framebuffer.rgba.size() == framebuffer.width * framebuffer.height * 4, "framebuffer stores RGBA bytes");
    require(count_nonzero_framebuffer_pixels(framebuffer) == 9, "only clipped visible quad pixels are colored");
    require(framebuffer_pixel_is(framebuffer, 2, 2, 0, 255, 0, 255), "visible clipped quad writes green RGBA");
    require(framebuffer_pixel_is(framebuffer, 1, 1, 0, 0, 0, 0), "transparent command does not color pixels");
    require(framebuffer_pixel_is(framebuffer, 5, 5, 0, 0, 0, 0), "clipped command does not color out-of-clip pixels");
}

void test_semantic_scene_stats_distinguish_quiz_draw_work()
{
    using namespace quiz_vulkan;

    const scene::placed_scene placed_scene = make_quiz_semantic_scene();
    const ui::ui_draw_list draw_list = ui::ui_renderer{}.build_draw_list(placed_scene);
    require(!draw_list.empty(), "semantic scene produces UI draw commands before Vulkan submission");

    render::vulkan_renderer_options options;
    options.viewport = placed_scene.environment.viewport;
    options.fallback_surface_width = 64;
    options.fallback_surface_height = 64;

    render::vulkan_renderer renderer(options);
    // Renderer callers must submit ui_draw_list; placed_scene remains owned by layout/UI.
    renderer.submit(draw_list);

    const render::vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    require(stats.command_count == 9, "semantic scene emits expected draw command count");
    require(stats.draw_call_count == 9, "semantic scene draw commands are all draw calls");
    require(stats.visible_draw_call_count == 9, "semantic scene draw calls are visible");
    require(stats.input_enabled_draw_node_count == 2, "enabled option and answer input count as input-enabled draw nodes");
    require(stats.disabled_draw_node_count == 1, "disabled option counts once as a disabled draw node");
    require(stats.quiz_option_draw_command_count == 4, "option backgrounds and labels are counted as option draw commands");
    require(stats.quiz_feedback_draw_command_count == 2, "feedback banner background and label are counted");
    require(stats.quiz_answer_input_draw_command_count == 2, "answer input background and placeholder are counted");
    require(stats.quiz_answer_dock_draw_command_count == 1, "answer dock background is counted");

    const render::vulkan_renderer_frame_summary& summary = renderer.last_frame_summary();
    require(summary.used_cpu_fallback(), "semantic scene uses current CPU fallback backend");
    require(summary.nonblank(), "semantic scene shades fallback pixels");
    require(summary.discarded_draw_call_count == 0, "semantic scene draw calls are not discarded");
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
    // The Vulkan renderer receives only the explicit UI draw list built above.
    renderer.submit(expected_draw_list);

    const render::vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    require(stats.command_count == expected_draw_list.size(), "renderer consumed the explicit UI draw list");
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

    const render::vulkan_renderer_framebuffer& framebuffer = renderer.last_framebuffer();
    require(framebuffer.width == options.fallback_surface_width, "app framebuffer records fallback width");
    require(framebuffer.height == options.fallback_surface_height, "app framebuffer records fallback height");
    require(framebuffer.rgba.size() == fallback_surface_area * 4, "app framebuffer stores RGBA bytes");
    require(count_nonzero_framebuffer_pixels(framebuffer) > 0, "app framebuffer has nonzero pixels");
}

} // namespace

int main()
{
    test_scene_snapshot_submission();
    test_cpu_fallback_clips_and_discards();
    test_semantic_scene_stats_distinguish_quiz_draw_work();
    test_app_snapshot_renders_nonblank_frame();
    return 0;
}
