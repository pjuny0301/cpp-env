#include "render/vulkan/vulkan_backend_adapter.h"

#include <concepts>
#include <cstddef>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept VulkanBackendDeviceInterface = requires(
    const T& const_device,
    T& device,
    const render::vulkan_backend::vulkan_frame_plan& plan,
    render::vulkan_backend::vulkan_surface_extent surface) {
    { const_device.current_surface_extent() } -> std::same_as<render::vulkan_backend::vulkan_surface_extent>;
    { device.begin_frame(surface) } -> std::same_as<bool>;
    { device.record_frame_commands(plan) } -> std::same_as<bool>;
    { device.submit_frame() } -> std::same_as<bool>;
    { device.present_frame() } -> std::same_as<bool>;
};

static_assert(VulkanBackendDeviceInterface<render::vulkan_backend::vulkan_backend_device_interface>);
static_assert(VulkanBackendDeviceInterface<render::vulkan_backend::null_vulkan_backend_device>);

static_assert(requires(render::vulkan_backend::vulkan_surface_extent surface) {
    { surface.width } -> std::same_as<std::size_t&>;
    { surface.height } -> std::same_as<std::size_t&>;
    { surface.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_frame_result result) {
    { result.surface } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { result.surface_ready } -> std::same_as<bool&>;
    { result.frame_begun } -> std::same_as<bool&>;
    { result.commands_recorded } -> std::same_as<bool&>;
    { result.frame_submitted } -> std::same_as<bool&>;
    { result.frame_presented } -> std::same_as<bool&>;
    { result.attempted } -> std::same_as<bool&>;
    { result.fallback_required } -> std::same_as<bool&>;
    { result.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { result.planned_batch_count } -> std::same_as<std::size_t&>;
    { result.clipped_draw_call_count } -> std::same_as<std::size_t&>;
    { result.discarded_draw_call_count } -> std::same_as<std::size_t&>;
    { result.completed() } -> std::same_as<bool>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_device_interface& device,
    const render::render_draw_list& draw_list,
    render::render_rect viewport) {
    { render::vulkan_backend::submit_vulkan_backend_frame(device, draw_list, viewport) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_frame_result>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::none),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::not_requested),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::surface_unavailable),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::viewport_unavailable),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::begin_frame_failed),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::present_frame_failed),
    render::vulkan_backend::vulkan_backend_fallback_reason>);

} // namespace
