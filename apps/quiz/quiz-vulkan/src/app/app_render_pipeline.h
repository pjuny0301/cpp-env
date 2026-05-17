#pragma once

#include "app/app_demo.h"
#include "render/text/fake_text_engine.h"

namespace quiz_vulkan {

struct app_render_request {
    const domain::app_snapshot* snapshot = nullptr;
    scene::scene_rect viewport{0.0f, 0.0f, 1280.0f, 720.0f};
    app_render_view_state view_state;

    [[nodiscard]] bool valid() const noexcept
    {
        return snapshot != nullptr;
    }
};

class app_render_pipeline_interface {
public:
    virtual ~app_render_pipeline_interface() = default;

    virtual app_render_frame render(const app_render_request& request) = 0;
};

class default_app_render_pipeline final : public app_render_pipeline_interface {
public:
    app_render_frame render(const app_render_request& request) override
    {
        if (!request.valid()) {
            return {};
        }

        return render_app_frame_with_engines(
            *request.snapshot,
            request.viewport,
            request.view_state,
            text_engine_,
            renderer_);
    }

    [[nodiscard]] const render::fake_text_engine& text_engine() const
    {
        return text_engine_;
    }

    [[nodiscard]] const render::vulkan_renderer& renderer() const
    {
        return renderer_;
    }

private:
    render::fake_text_engine text_engine_;
    render::vulkan_renderer renderer_{render::vulkan_renderer_options{}};
};

} // namespace quiz_vulkan
