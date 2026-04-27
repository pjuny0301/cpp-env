#include "app/app_demo.h"

#include "core/layout/layout_placer.h"
#include "core/ui/quiz_screens.h"

#include <algorithm>
#include <sstream>
#include <string_view>
#include <vector>

namespace quiz_vulkan {
namespace {

class demo_text_metrics final : public scene::text_metrics_interface {
public:
    scene::scene_size measure_text(
        const std::vector<scene::scene_text_run>& text_runs,
        const scene::scene_style&,
        float max_width) const override
    {
        std::size_t character_count = 0;
        for (const scene::scene_text_run& run : text_runs) {
            character_count += run.text.size();
        }

        const float raw_width = static_cast<float>(character_count * 8);
        const float width = max_width > 0.0f ? std::min(raw_width, max_width) : raw_width;
        const float line_count = max_width > 0.0f && raw_width > max_width
            ? static_cast<float>(static_cast<int>((raw_width / max_width) + 1.0f))
            : 1.0f;
        return scene::scene_size{width, 20.0f * line_count};
    }
};

} // namespace

domain::deck make_demo_deck()
{
    using namespace domain;

    question git_question;
    git_question.id = "demo_git_vcs";
    git_question.prompt = "Git에서 커밋이 하는 역할은?";
    git_question.type = question_type::answer;
    git_question.options = {
        option{"파일 변경 이력을 고정된 스냅샷으로 남긴다", true},
        option{"운영체제 커널을 직접 교체한다", false},
        option{"네트워크 DNS 캐시를 비운다", false},
    };
    git_question.explanation = answer_explanation{
        "커밋은 변경 이력을 재현 가능한 단위로 저장하므로 이후 비교, 복구, 협업의 기준이 된다.",
        {"git-commit"},
    };

    question blank_question;
    blank_question.id = "demo_vulkan_contract";
    blank_question.prompt = "Renderer는 domain state를 직접 소유하지 않고 _____ snapshot만 소비한다.";
    blank_question.type = question_type::blank;
    blank_question.accepted_answers = {"scene", "scene snapshot"};

    day demo_day;
    demo_day.id = "day_1";
    demo_day.title = "Day 1";
    demo_day.questions = {git_question, blank_question};

    deck demo_deck;
    demo_deck.id = "demo_deck";
    demo_deck.title = "Native Quiz Demo";
    demo_deck.source_uri = "built-in-demo";
    demo_deck.concepts = {
        concept_ref{"git-commit", "Git commit", {}},
        concept_ref{"renderer-contract", "Renderer contract", {}},
    };
    demo_deck.days = {demo_day};
    return demo_deck;
}

app_render_report render_app_snapshot(const domain::app_snapshot& snapshot, scene::scene_rect viewport)
{
    scene::scene_layout_data scene_data("quiz_app");
    const scene::scene_layout_patch patch = ui::make_quiz_screen_patch(snapshot);
    const scene::scene_layout_apply_result apply_result = patch.apply_to(scene_data);

    app_render_report report;
    report.screen_id = scene_data.route_state().screen_id;
    if (!apply_result.applied()) {
        return report;
    }

    scene::scene_layout_environment environment;
    environment.viewport = viewport;
    environment.safe_area = {24.0f, 24.0f, 24.0f, 24.0f};
    if (snapshot.active_session.has_value()
        && snapshot.active_session->current_question.has_value()
        && snapshot.active_session->current_question->type == domain::question_type::blank) {
        environment.keyboard.visible = true;
        environment.keyboard.bottom_inset = 260.0f;
    }

    const demo_text_metrics text_metrics;
    const scene::placed_scene placed_scene = scene::layout_placer{}.place_with_environment(
        scene_data,
        environment,
        text_metrics);

    render::vulkan_renderer_options renderer_options;
    renderer_options.viewport = environment.viewport;
    render::vulkan_renderer renderer(renderer_options);
    renderer.submit(placed_scene);

    report.node_count = placed_scene.nodes.size();
    report.input_region_count = placed_scene.input_regions.size();
    report.frame_stats = renderer.last_frame_stats();
    report.frame_summary = renderer.last_frame_summary();
    return report;
}

std::string format_render_report(std::string_view label, const app_render_report& report)
{
    std::ostringstream stream;
    stream << label
           << " screen=" << report.screen_id
           << " nodes=" << report.node_count
           << " inputs=" << report.input_region_count
           << " commands=" << report.frame_stats.command_count
           << " draw_calls=" << report.frame_stats.draw_call_count
           << " shaded_pixels=" << report.frame_summary.shaded_pixel_count
           << " nonblank=" << (report.frame_summary.nonblank() ? "true" : "false");
    return stream.str();
}

} // namespace quiz_vulkan
