#include "app/app_render_pipeline.h"

#include <concepts>

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

} // namespace
