#include "app/app_render_pipeline.h"

#include <concepts>
#include <filesystem>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept AppRenderPipelineInterface = requires(
    T& pipeline,
    const app_render_request& request) {
    { pipeline.render(request) } -> std::same_as<app_render_frame>;
};

static_assert(AppRenderPipelineInterface<app_render_pipeline_interface>);
static_assert(AppRenderPipelineInterface<default_app_render_pipeline>);

static_assert(requires(app_render_request request) {
    { request.snapshot } -> std::same_as<const domain::app_snapshot*&>;
    { request.viewport } -> std::same_as<scene::scene_rect&>;
    { request.view_state } -> std::same_as<app_render_view_state&>;
    { request.valid() } -> std::same_as<bool>;
});

static_assert(requires(default_app_render_pipeline_config config) {
    { config.image_base_directory } -> std::same_as<std::filesystem::path&>;
    { config.renderer_options } -> std::same_as<render::vulkan_renderer_options&>;
});

static_assert(requires(default_app_render_pipeline pipeline) {
    { pipeline.image_texture_pipeline() } -> std::same_as<const render::fake_image_texture_pipeline&>;
    { pipeline.image_source_bytes_loader() }
        -> std::same_as<const render::filesystem_image_source_bytes_loader&>;
});

} // namespace
