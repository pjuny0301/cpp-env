#include "app/app_demo.h"

#include "core/layout/layout_placer.h"
#include "core/scene/modifier_interface.h"
#include "app/app_quiz_screens.h"
#include "core/ui/ui_renderer.h"
#include "render/text/fake_text_engine.h"
#include "render/text/scene_text_metrics_adapter.h"

#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace quiz_vulkan {
namespace {

render::render_rect to_render_rect(scene::scene_rect rect)
{
    return render::render_rect{rect.x, rect.y, rect.width, rect.height};
}

render::render_text_style make_text_style(
    render::render_style_id id,
    float font_size,
    float line_height,
    int font_weight = 400)
{
    return render::render_text_style{
        .id = std::move(id),
        .font_family = "DemoSans",
        .font_size = font_size,
        .line_height = line_height,
        .letter_spacing = 0.0f,
        .font_weight = font_weight,
        .italic = false,
    };
}

render::render_text_style_catalog make_demo_text_style_catalog()
{
    render::render_text_style_catalog catalog;
    catalog.fallback_style = make_text_style("body", 16.0f, 20.0f);
    catalog.styles = {
        make_text_style("body", 16.0f, 20.0f),
        make_text_style("muted", 14.0f, 18.0f),
        make_text_style("heading", 24.0f, 30.0f, 700),
        make_text_style("button", 16.0f, 20.0f, 600),
        make_text_style("section", 16.0f, 20.0f, 600),
        make_text_style("summary_row", 16.0f, 20.0f),
        make_text_style("text_input", 16.0f, 20.0f),
        make_text_style("error", 16.0f, 20.0f, 600),
        make_text_style("feedback_correct", 16.0f, 20.0f, 600),
        make_text_style("feedback_needs_work", 16.0f, 20.0f, 600),
        make_text_style("option_correct", 16.0f, 20.0f, 600),
        make_text_style("option_incorrect", 16.0f, 20.0f, 600),
    };
    return catalog;
}

render::render_text_options make_demo_text_options()
{
    return render::render_text_options{
        .wrap = render::render_text_wrap_mode::word,
        .alignment = render::render_text_alignment::start,
        .max_lines = 0,
    };
}

class app_render_view_state_modifier final : public scene::scene_modifier {
public:
    explicit app_render_view_state_modifier(app_render_view_state view_state)
        : typed_text_answer_(view_state.typed_text_answer)
    {
    }

    void modify(const scene::scene_modifier_context& context, scene::scene_layout_edit_data& edit_data) override
    {
        if (typed_text_answer_.empty() || context.current_scene == nullptr) {
            return;
        }

        for (const auto& [node_id, node] : context.current_scene->nodes()) {
            if (node.semantics.role != scene::scene_node_role::quiz_answer_input
                || !node.semantics.quiz.accepts_keyboard_input) {
                continue;
            }

            edit_data.set_text(node_id, {scene::scene_text_run{typed_text_answer_, "text_input"}});
            edit_data.set_focus(node_id);
        }
    }

private:
    std::string typed_text_answer_;
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

app_render_frame render_app_frame(
    const domain::app_snapshot& snapshot,
    scene::scene_rect viewport,
    app_render_view_state view_state)
{
    scene::scene_layout_data scene_data("quiz_app");

    app_render_frame frame;

    scene::scene_layout_environment environment;
    environment.viewport = viewport;
    environment.safe_area = {24.0f, 24.0f, 24.0f, 24.0f};
    if (snapshot.active_session.has_value()
        && snapshot.active_session->current_question.has_value()
        && snapshot.active_session->current_question->type == domain::question_type::blank) {
        environment.keyboard.visible = true;
        environment.keyboard.bottom_inset = 260.0f;
    }

    scene::scene_layout_data_modifier scene_modifiers;
    scene_modifiers.add_modifier(presentation::make_app_snapshot_screen_modifier(snapshot));
    scene_modifiers.add_modifier(std::make_shared<app_render_view_state_modifier>(view_state));

    const scene::scene_layout_apply_result apply_result = scene_modifiers.apply(
        scene_data,
        scene::scene_modifier_context{
            .current_scene = nullptr,
            .viewport = viewport,
            .layout_environment = environment,
            .route_state = scene_data.route_state(),
        });
    frame.report.screen_id = scene_data.route_state().screen_id;
    if (!apply_result.applied()) {
        return frame;
    }

    if (scene_data.has_focus()) {
        environment.keyboard.focused_node_id = scene_data.focus_id();
    }

    render::fake_text_engine text_engine;
    scene::render_text_metrics text_metrics(
        text_engine,
        make_demo_text_style_catalog(),
        make_demo_text_options());
    frame.placed_scene = scene::layout_placer{}.place_with_environment(
        scene_data,
        environment,
        text_metrics);
    const ui::ui_draw_list draw_list = ui::ui_renderer{}.build_draw_list(frame.placed_scene);

    render::vulkan_renderer_options renderer_options{.viewport = to_render_rect(environment.viewport)};
    render::vulkan_renderer renderer(renderer_options);
    renderer.submit(draw_list);

    frame.report.node_count = frame.placed_scene.nodes.size();
    frame.report.input_region_count = frame.placed_scene.input_regions.size();
    frame.report.frame_stats = renderer.last_frame_stats();
    frame.report.frame_summary = renderer.last_frame_summary();
    frame.framebuffer = renderer.last_framebuffer();
    return frame;
}

app_render_report render_app_snapshot(const domain::app_snapshot& snapshot, scene::scene_rect viewport)
{
    return render_app_frame(snapshot, viewport).report;
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
