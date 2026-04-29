#include "core/layout/layout_placer.h"

#include <concepts>
#include <cstddef>
#include <vector>

namespace {

namespace scene = quiz_vulkan::scene;

template <typename T>
concept TextMetricsInterface = requires(
    const T& metrics,
    const std::vector<scene::scene_text_run>& text_runs,
    const scene::scene_style& style,
    float max_width) {
    { metrics.measure_text(text_runs, style, max_width) } -> std::same_as<scene::scene_size>;
};

template <typename T>
concept LayoutPlacerInterface = requires(
    const T& placer,
    const scene::scene_layout_data& layout_data,
    const scene::scene_layout_environment& environment,
    scene::scene_rect viewport,
    const scene::text_metrics_interface& text_metrics) {
    { placer.place(layout_data, viewport, text_metrics) } -> std::same_as<scene::placed_scene>;
    { placer.place_with_environment(layout_data, environment, text_metrics) } -> std::same_as<scene::placed_scene>;
};

static_assert(TextMetricsInterface<scene::text_metrics_interface>);
static_assert(LayoutPlacerInterface<scene::layout_placer>);

static_assert(requires(scene::placed_scene_node node) {
    { node.id } -> std::same_as<scene::scene_node_id&>;
    { node.parent_id } -> std::same_as<scene::scene_node_id&>;
    { node.kind } -> std::same_as<scene::scene_node_kind&>;
    { node.bounds } -> std::same_as<scene::scene_rect&>;
    { node.content_bounds } -> std::same_as<scene::scene_rect&>;
    { node.layout_rule } -> std::same_as<scene::scene_layout_rule&>;
    { node.style } -> std::same_as<scene::scene_style&>;
    { node.text_runs } -> std::same_as<std::vector<scene::scene_text_run>&>;
    { node.image } -> std::same_as<scene::scene_image_ref&>;
    { node.has_image } -> std::same_as<bool&>;
    { node.action_binding } -> std::same_as<scene::scene_action_binding&>;
    { node.has_action_binding } -> std::same_as<bool&>;
    { node.semantics } -> std::same_as<scene::scene_node_semantics&>;
    { node.depth } -> std::same_as<std::size_t&>;
    { node.visible } -> std::same_as<bool&>;
    { node.input_enabled } -> std::same_as<bool&>;
});

static_assert(requires(
    scene::placed_scene placed,
    const scene::placed_scene& const_placed,
    const scene::scene_node_id& node_id) {
    { placed.environment } -> std::same_as<scene::scene_layout_environment&>;
    { placed.usable_bounds } -> std::same_as<scene::scene_rect&>;
    { placed.nodes } -> std::same_as<std::vector<scene::placed_scene_node>&>;
    { placed.input_regions } -> std::same_as<std::vector<scene::scene_input_region>&>;
    { const_placed.find_node(node_id) } -> std::same_as<const scene::placed_scene_node*>;
});

} // namespace
