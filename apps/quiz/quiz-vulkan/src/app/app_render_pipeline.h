#pragma once

#include "app/app_demo.h"
#include "platform/platform_shell.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_pipeline.h"
#include "render/text/fake_text_engine.h"

#include <filesystem>
#include <utility>

namespace quiz_vulkan {

struct default_app_render_pipeline_config {
    std::filesystem::path image_base_directory;
    platform_native_window_handle native_window;
    render::vulkan_renderer_options renderer_options;
};

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
    default_app_render_pipeline() = default;

    explicit default_app_render_pipeline(default_app_render_pipeline_config config)
        : renderer_(make_renderer_options(std::move(config.renderer_options), config.native_window))
        , native_window_(config.native_window)
    {
        image_source_bytes_loader_.set_base_directory(std::move(config.image_base_directory));
    }

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
            renderer_,
            &image_texture_pipeline_,
            &image_resolver_);
    }

    [[nodiscard]] const render::fake_text_engine& text_engine() const
    {
        return text_engine_;
    }

    [[nodiscard]] const render::vulkan_renderer& renderer() const
    {
        return renderer_;
    }

    [[nodiscard]] const render::standard_image_texture_pipeline& image_texture_pipeline() const
    {
        return image_texture_pipeline_;
    }

    [[nodiscard]] const render::filesystem_image_source_bytes_loader& image_source_bytes_loader() const
    {
        return image_source_bytes_loader_;
    }

    [[nodiscard]] platform_native_window_handle native_window() const
    {
        return native_window_;
    }

private:
    [[nodiscard]] static render::vulkan_renderer_native_window_target to_renderer_native_window_target(
        platform_native_window_handle native_window)
    {
        switch (native_window.kind) {
        case platform_native_window_kind::win32_hwnd:
            return render::vulkan_renderer_native_window_target{
                .kind = render::vulkan_renderer_native_window_kind::win32_hwnd,
                .window = native_window.value,
                .display = native_window.display,
            };
        case platform_native_window_kind::none:
            return {};
        }

        return {};
    }

    [[nodiscard]] static render::vulkan_renderer_options make_renderer_options(
        render::vulkan_renderer_options options,
        platform_native_window_handle native_window)
    {
        options.native_window = to_renderer_native_window_target(native_window);
        return options;
    }

    render::fake_text_engine text_engine_;
    render::vulkan_renderer renderer_{render::vulkan_renderer_options{}};
    platform_native_window_handle native_window_;
    render::normalizing_image_resolver image_resolver_;
    render::filesystem_image_source_bytes_loader image_source_bytes_loader_;
    render::standard_image_texture_pipeline image_texture_pipeline_{
        image_resolver_,
        image_source_bytes_loader_,
    };
};

} // namespace quiz_vulkan
