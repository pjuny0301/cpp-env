#include "core/scene/modifier_interface.h"

#include <concepts>
#include <type_traits>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept SceneModifierInterface = requires(
    T& modifier,
    const scene::scene_modifier_context& context,
    scene::scene_layout_edit_data& edit_data) {
    { modifier.modify(context, edit_data) } -> std::same_as<void>;
};

static_assert(SceneModifierInterface<scene::scene_modifier>);

static_assert(requires(const scene::scene_modifier_context& context) {
    { context.current_scene } -> std::same_as<const scene::scene_layout_data* const&>;
    { context.viewport } -> std::same_as<const scene::scene_rect&>;
    { context.layout_environment } -> std::same_as<const scene::scene_layout_environment&>;
    { context.route_state } -> std::same_as<const scene::scene_route_state&>;
});

static_assert(std::is_const_v<std::remove_pointer_t<decltype(scene::scene_modifier_context::current_scene)>>);

static_assert(requires(
    scene::scene_layout_data_modifier modifier_stack,
    scene::scene_layout_data& layout_data,
    scene::scene_modifier_context context) {
    { modifier_stack.apply(layout_data, context) } -> std::same_as<scene::scene_layout_apply_result>;
    { modifier_stack.build_patch(context) } -> std::same_as<scene::scene_layout_patch>;
});

} // namespace
