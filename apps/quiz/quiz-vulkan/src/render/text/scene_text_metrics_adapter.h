#pragma once

#include "core/layout/layout_placer.h"
#include "render/text/text_engine.h"

#include <utility>
#include <vector>

namespace quiz_vulkan::scene {

class render_text_metrics final : public text_metrics_interface {
public:
    render_text_metrics(
        render::text_engine_interface& text_engine,
        render::render_text_style_catalog style_catalog,
        render::render_text_options options)
        : text_engine_(text_engine)
        , style_catalog_(std::move(style_catalog))
        , options_(options)
    {
    }

    scene_size measure_text(
        const std::vector<scene_text_run>& text_runs,
        const scene_style& style,
        float max_width) const override
    {
        render::render_text_request request;
        request.text_runs.reserve(text_runs.size());
        for (const scene_text_run& run : text_runs) {
            request.text_runs.push_back(render::render_text_run{
                run.text,
                run.style_token.empty() ? style.token : run.style_token});
        }
        request.bounds = render::render_rect{0.0f, 0.0f, max_width, 0.0f};
        request.style_catalog = style_catalog_;
        request.options = options_;

        const render::render_text_measure measure = text_engine_.measure_text(request);
        return scene_size{measure.width, measure.height};
    }

private:
    render::text_engine_interface& text_engine_;
    render::render_text_style_catalog style_catalog_;
    render::render_text_options options_;
};

} // namespace quiz_vulkan::scene
