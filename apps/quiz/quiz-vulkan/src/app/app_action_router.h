#pragma once

#include "core/domain/app_action.hpp"
#include "core/scene/scene_layout_data.h"

#include <optional>
#include <string>
#include <string_view>

namespace quiz_vulkan {

struct app_action_route_result {
    std::optional<domain::app_action> action;
    std::string error;

    bool ok() const;
};

app_action_route_result route_scene_action(
    const scene::scene_action_binding& binding,
    std::optional<std::string_view> submitted_text = std::nullopt);

}  // namespace quiz_vulkan
