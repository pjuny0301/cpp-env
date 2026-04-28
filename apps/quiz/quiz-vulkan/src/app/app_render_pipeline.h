#pragma once

#include "app/app_demo.h"

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

        return render_app_frame(*request.snapshot, request.viewport, request.view_state);
    }
};

} // namespace quiz_vulkan
