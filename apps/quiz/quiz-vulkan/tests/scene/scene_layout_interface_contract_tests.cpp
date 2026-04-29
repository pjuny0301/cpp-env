#include "core/scene/scene_layout_edit_data.h"

#include <concepts>
#include <map>
#include <string>
#include <vector>

namespace {

namespace scene = quiz_vulkan::scene;

template <typename T>
concept SceneLayoutDataInterface = requires(
    T& data,
    const T& const_data,
    const scene::scene_node_id& node_id,
    const scene::scene_node_id& parent_id,
    scene::scene_node_data node,
    scene::scene_style style,
    scene::scene_layout_rule bounds_rule,
    scene::scene_image_ref image,
    scene::scene_action_binding action,
    scene::scene_node_semantics semantics,
    scene::scene_route_state route_state,
    scene::scene_animation_state animation_state,
    std::vector<scene::scene_text_run> text_runs,
    std::string* error) {
    { const_data.root_node_id() } -> std::same_as<const scene::scene_node_id&>;
    { const_data.root_screen_id() } -> std::same_as<const scene::scene_screen_id&>;
    { const_data.route_state() } -> std::same_as<const scene::scene_route_state&>;
    { const_data.animation_state() } -> std::same_as<const scene::scene_animation_state&>;
    { const_data.has_focus() } -> std::same_as<bool>;
    { const_data.focus_id() } -> std::same_as<const scene::scene_node_id&>;
    { const_data.nodes() } -> std::same_as<const std::map<scene::scene_node_id, scene::scene_node_data>&>;
    { const_data.style_tokens() } -> std::same_as<const std::map<scene::scene_style_id, scene::scene_style>&>;
    { const_data.contains_node(node_id) } -> std::same_as<bool>;
    { const_data.find_node(node_id) } -> std::same_as<const scene::scene_node_data*>;
    { data.find_node(node_id) } -> std::same_as<scene::scene_node_data*>;
    { const_data.root_node() } -> std::same_as<const scene::scene_node_data&>;
    { data.root_node() } -> std::same_as<scene::scene_node_data&>;
    { const_data.root_children() } -> std::same_as<const std::vector<scene::scene_node_id>&>;
    { data.append_node(parent_id, node, error) } -> std::same_as<bool>;
    { data.remove_node(node_id, error) } -> std::same_as<bool>;
    { data.set_text(node_id, text_runs, error) } -> std::same_as<bool>;
    { data.set_style(node_id, style, error) } -> std::same_as<bool>;
    { data.set_bounds_rule(node_id, bounds_rule, error) } -> std::same_as<bool>;
    { data.set_image(node_id, image, error) } -> std::same_as<bool>;
    { data.bind_action(node_id, action, error) } -> std::same_as<bool>;
    { data.set_semantics(node_id, semantics, error) } -> std::same_as<bool>;
    { data.set_focus(node_id, error) } -> std::same_as<bool>;
    { data.set_route(route_state) } -> std::same_as<bool>;
    { data.start_transition(animation_state) } -> std::same_as<bool>;
};

template <typename T>
concept SceneLayoutEditDataInterface = requires(
    T& edit_data,
    const T& const_edit_data,
    scene::scene_node_id node_id,
    scene::scene_node_id parent_id,
    scene::scene_node_data node,
    scene::scene_style style,
    scene::scene_layout_rule bounds_rule,
    scene::scene_image_ref image,
    scene::scene_action_binding action,
    scene::scene_node_semantics semantics,
    scene::scene_route_state route_state,
    scene::scene_animation_state animation_state,
    std::vector<scene::scene_text_run> text_runs) {
    { const_edit_data.source_name() } -> std::same_as<const std::string&>;
    { const_edit_data.patch() } -> std::same_as<const scene::scene_layout_patch&>;
    { edit_data.finish_patch() } -> std::same_as<scene::scene_layout_patch>;
    { edit_data.append_node(parent_id, node) } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.remove_node(node_id) } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.set_text(node_id, text_runs) } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.set_style(node_id, style) } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.set_bounds_rule(node_id, bounds_rule) } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.set_image(node_id, image) } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.bind_action(node_id, action) } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.set_semantics(node_id, semantics) } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.set_focus(node_id) } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.clear_focus() } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.set_route(route_state) } -> std::same_as<scene::scene_layout_edit_data&>;
    { edit_data.start_transition(animation_state) } -> std::same_as<scene::scene_layout_edit_data&>;
};

template <typename T>
concept SceneLayoutPatchInterface = requires(
    T& patch,
    const T& const_patch,
    scene::scene_layout_data& layout_data,
    scene::scene_node_id node_id,
    scene::scene_node_id parent_id,
    scene::scene_node_data node,
    scene::scene_style style,
    scene::scene_layout_rule bounds_rule,
    scene::scene_image_ref image,
    scene::scene_action_binding action,
    scene::scene_node_semantics semantics,
    scene::scene_route_state route_state,
    scene::scene_animation_state animation_state,
    std::vector<scene::scene_text_run> text_runs) {
    { patch.append_node(parent_id, node) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.remove_node(node_id) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.set_text(node_id, text_runs) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.set_style(node_id, style) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.set_bounds_rule(node_id, bounds_rule) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.set_image(node_id, image) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.bind_action(node_id, action) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.set_semantics(node_id, semantics) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.set_focus(node_id) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.set_route(route_state) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.start_transition(animation_state) } -> std::same_as<scene::scene_layout_patch&>;
    { patch.append_patch(const_patch) } -> std::same_as<scene::scene_layout_patch&>;
    { const_patch.empty() } -> std::same_as<bool>;
    { const_patch.operations() } -> std::same_as<const std::vector<scene::scene_layout_patch_operation>&>;
    { const_patch.apply_to(layout_data) } -> std::same_as<scene::scene_layout_apply_result>;
};

static_assert(SceneLayoutDataInterface<scene::scene_layout_data>);
static_assert(SceneLayoutEditDataInterface<scene::scene_layout_edit_data>);
static_assert(SceneLayoutPatchInterface<scene::scene_layout_patch>);

static_assert(requires(scene::scene_layout_patch_operation operation) {
    { operation.type } -> std::same_as<scene::scene_layout_patch_operation_type&>;
    { operation.parent_id } -> std::same_as<scene::scene_node_id&>;
    { operation.node_id } -> std::same_as<scene::scene_node_id&>;
    { operation.node } -> std::same_as<scene::scene_node_data&>;
    { operation.child_index } -> std::same_as<std::size_t&>;
    { operation.text_runs } -> std::same_as<std::vector<scene::scene_text_run>&>;
    { operation.style } -> std::same_as<scene::scene_style&>;
    { operation.bounds_rule } -> std::same_as<scene::scene_layout_rule&>;
    { operation.image } -> std::same_as<scene::scene_image_ref&>;
    { operation.action } -> std::same_as<scene::scene_action_binding&>;
    { operation.semantics } -> std::same_as<scene::scene_node_semantics&>;
    { operation.route_state } -> std::same_as<scene::scene_route_state&>;
    { operation.animation_state } -> std::same_as<scene::scene_animation_state&>;
});

static_assert(requires(
    scene::scene_layout_apply_result result,
    const scene::scene_layout_apply_result& const_result) {
    { result.errors } -> std::same_as<std::vector<std::string>&>;
    { const_result.applied() } -> std::same_as<bool>;
});

} // namespace
