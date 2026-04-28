#include "core/scene/modifier_interface.h"

#include <concepts>

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

static_assert(requires(
    scene::scene_layout_data_modifier modifier_stack,
    scene::scene_layout_data& layout_data,
    scene::scene_modifier_context context) {
    { modifier_stack.apply(layout_data, context) } -> std::same_as<scene::scene_layout_apply_result>;
    { modifier_stack.build_patch(context) } -> std::same_as<scene::scene_layout_patch>;
});

} // namespace
