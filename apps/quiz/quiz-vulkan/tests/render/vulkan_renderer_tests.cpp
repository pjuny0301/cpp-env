#include "core/domain/app_snapshot.hpp"
#include "core/layout/layout_placer.h"
#include "core/ui/quiz_screens.h"
#include "core/ui/ui_renderer.h"
#include "render/vulkan/vulkan_renderer.h"

#include <algorithm>
#include <cassert>
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
        float max_width) const override
    {
        std::size_t character_count = 0;
        for (const auto& run : text_runs) {
            character_count += run.text.size();
        }

        const float measured_width = static_cast<float>(character_count) * 8.0f;
        return quiz_vulkan::scene::scene_size{std::min(max_width, measured_width), 20.0f};
    }
};

quiz_vulkan::domain::deck make_test_deck()
{
    using namespace quiz_vulkan::domain;

    question quiz_question;
    quiz_question.id = "q1";
    quiz_question.prompt = "2 + 2";
    quiz_question.type = question_type::answer;
    quiz_question.options.push_back(option{"4", true});
    quiz_question.options.push_back(option{"5", false});

    day quiz_day;
    quiz_day.id = "day1";
    quiz_day.title = "Day 1";
    quiz_day.questions.push_back(std::move(quiz_question));

    deck quiz_deck;
    quiz_deck.id = "math";
    quiz_deck.title = "Math";
    quiz_deck.days.push_back(std::move(quiz_day));
    return quiz_deck;
}

} // namespace

int main()
{
    using namespace quiz_vulkan;

    std::vector<domain::deck> decks;
    decks.push_back(make_test_deck());

    const domain::learning_state_map learning;
    const domain::app_snapshot snapshot = domain::make_app_snapshot(
        decks,
        std::nullopt,
        std::nullopt,
        nullptr,
        learning);

    scene::scene_layout_data data("render_test");
    const scene::scene_layout_apply_result apply_result = ui::make_quiz_screen_patch(snapshot).apply_to(data);
    assert(apply_result.applied());

    const fixed_text_metrics metrics;
    const scene::placed_scene placed = scene::layout_placer{}.place(
        data,
        scene::scene_rect{0.0f, 0.0f, 1280.0f, 720.0f},
        metrics);
    assert(!placed.nodes.empty());

    const ui::ui_draw_list draw_list = ui::ui_renderer{}.build_draw_list(placed);
    assert(!draw_list.empty());

    render::vulkan_renderer renderer;
    renderer.submit(draw_list);

    const render::vulkan_renderer_frame_stats& stats = renderer.last_frame_stats();
    assert(stats.command_count == draw_list.size());
    assert(stats.quad_count > 0);
    assert(stats.text_count > 0);
    assert(!stats.empty());

    renderer.clear();
    assert(renderer.last_draw_list().empty());
    assert(renderer.last_frame_stats().empty());

    return 0;
}
