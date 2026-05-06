#pragma once

#include "render/vulkan/vulkan_backend_adapter.h"

namespace quiz_vulkan::render::vulkan_backend::frame_lifecycle {

vulkan_backend_frame_lifecycle_policy_state make_policy_state();

void mark_fallback(
    vulkan_backend_frame_result& result,
    vulkan_backend_fallback_reason reason);

void start_step(
    vulkan_backend_frame_lifecycle_policy_state& state,
    vulkan_frame_lifecycle_step step);

void complete_step(
    vulkan_backend_frame_lifecycle_policy_state& state,
    vulkan_frame_lifecycle_step step);

void fail_step(
    vulkan_backend_frame_lifecycle_policy_state& state,
    vulkan_frame_lifecycle_step step,
    vulkan_backend_fallback_reason reason);

} // namespace quiz_vulkan::render::vulkan_backend::frame_lifecycle
