#include "core/ui/ui_renderer.h"

#include <concepts>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept UiRendererInterface = requires(
    const T& renderer,
    const scene::placed_scene& placed_scene) {
    { renderer.build_draw_list(placed_scene) } -> std::same_as<ui::ui_draw_list>;
    { renderer.options() } -> std::same_as<const ui::ui_renderer_options&>;
};

static_assert(UiRendererInterface<ui::ui_renderer>);

static_assert(requires(ui::ui_renderer renderer, ui::ui_renderer_options options) {
    { ui::ui_renderer{} };
    { ui::ui_renderer{options} };
    { renderer.set_options(options) } -> std::same_as<void>;
});

static_assert(requires(ui::ui_draw_command command) {
    { command.type } -> std::same_as<ui::ui_draw_command_type&>;
    { command.bounds } -> std::same_as<ui::ui_rect&>;
    { command.paint } -> std::same_as<ui::ui_paint&>;
    { command.text_runs } -> std::same_as<std::vector<ui::ui_text_run>&>;
    { command.image } -> std::same_as<ui::ui_image_ref&>;
});

} // namespace
