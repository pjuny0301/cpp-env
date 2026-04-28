#pragma once

#include "core/domain/app_action.hpp"
#include "core/input/input_event.h"
#include "core/layout/layout_placer.h"
#include "platform/platform_input_event.h"
#include "platform/platform_shell.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quiz_vulkan {

struct app_input_route_result {
    bool handled = false;
    bool needs_render = false;
    bool clear_text_after_action = false;
    std::optional<domain::app_action> action;
    std::string error;

    [[nodiscard]] bool ok() const;
};

[[nodiscard]] std::vector<raw_platform_input_event> normalize_platform_input_event(
    const platform_input_event& event,
    std::int64_t timestamp_ms);

[[nodiscard]] std::optional<std::string> keyboard_input_target(const scene::placed_scene& placed_scene);

[[nodiscard]] app_input_route_result route_normalized_input_event(
    const input::input_event& event,
    const scene::placed_scene& placed_scene,
    std::string_view committed_text);

} // namespace quiz_vulkan
