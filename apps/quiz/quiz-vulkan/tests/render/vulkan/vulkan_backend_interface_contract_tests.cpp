#include "render/vulkan/vulkan_backend_adapter.h"

#include <concepts>
#include <cstddef>
#include <string_view>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept VulkanBackendDeviceInterface = requires(
    const T& const_device,
    T& device,
    const render::vulkan_backend::vulkan_frame_plan& plan,
    render::vulkan_backend::vulkan_surface_extent surface) {
    { const_device.current_lifecycle_readiness() }
        -> std::same_as<render::vulkan_backend::vulkan_backend_lifecycle_readiness>;
    { const_device.current_surface_extent() } -> std::same_as<render::vulkan_backend::vulkan_surface_extent>;
    { device.begin_frame(surface) } -> std::same_as<bool>;
    { device.record_frame_commands(plan) } -> std::same_as<bool>;
    { device.submit_frame() } -> std::same_as<bool>;
    { device.present_frame() } -> std::same_as<bool>;
};

static_assert(VulkanBackendDeviceInterface<render::vulkan_backend::vulkan_backend_device_interface>);
static_assert(VulkanBackendDeviceInterface<render::vulkan_backend::null_vulkan_backend_device>);

template <typename T>
concept VulkanCommandRecorderInterface = requires(
    const T& const_recorder,
    T& recorder,
    const render::vulkan_backend::vulkan_draw_batch& batch,
    render::vulkan_backend::vulkan_surface_extent surface,
    std::size_t planned_batch_count) {
    { recorder.begin_recording(surface, planned_batch_count) } -> std::same_as<bool>;
    { recorder.record_draw_batch(batch) } -> std::same_as<bool>;
    { recorder.finish_recording() } -> std::same_as<bool>;
    { const_recorder.recorder_state() }
        -> std::same_as<const render::vulkan_backend::vulkan_backend_command_recorder_state&>;
};

static_assert(VulkanCommandRecorderInterface<render::vulkan_backend::vulkan_command_recorder_interface>);
static_assert(VulkanCommandRecorderInterface<render::vulkan_backend::diagnostic_vulkan_command_recorder>);
static_assert(std::constructible_from<render::vulkan_backend::diagnostic_vulkan_command_recorder, bool>);
static_assert(std::constructible_from<
    render::vulkan_backend::diagnostic_vulkan_command_recorder,
    render::vulkan_backend::diagnostic_vulkan_command_recorder_options>);

static_assert(requires(render::vulkan_backend::vulkan_surface_extent surface) {
    { surface.width } -> std::same_as<std::size_t&>;
    { surface.height } -> std::same_as<std::size_t&>;
    { surface.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle) {
    { lifecycle.instance_ready } -> std::same_as<bool&>;
    { lifecycle.device_ready } -> std::same_as<bool&>;
    { lifecycle.swapchain_ready } -> std::same_as<bool&>;
    { lifecycle.pipeline_ready } -> std::same_as<bool&>;
    { lifecycle.command_recorder_ready } -> std::same_as<bool&>;
    { lifecycle.ready_for_frame() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_recorded_draw_batch recorded_batch) {
    { recorded_batch.kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { recorded_batch.command_index } -> std::same_as<std::size_t&>;
    { recorded_batch.recording_index } -> std::same_as<std::size_t&>;
    { recorded_batch.bounds } -> std::same_as<render::render_rect&>;
    { recorded_batch.clipped_bounds } -> std::same_as<render::render_rect&>;
    { recorded_batch.scissor } -> std::same_as<render::vulkan_backend::vulkan_scissor_rect&>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_failure_stage::none),
    render::vulkan_backend::vulkan_command_recorder_failure_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_failure_stage::begin_recording),
    render::vulkan_backend::vulkan_command_recorder_failure_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_failure_stage::record_draw_batch),
    render::vulkan_backend::vulkan_command_recorder_failure_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_failure_stage::finish_recording),
    render::vulkan_backend::vulkan_command_recorder_failure_stage>);

static_assert(requires(render::vulkan_backend::vulkan_command_recorder_failure_stage stage) {
    { render::vulkan_backend::command_recorder_failure_stage_name(stage) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_command_recorder_state recorder) {
    { recorder.ready } -> std::same_as<bool&>;
    { recorder.frame_open } -> std::same_as<bool&>;
    { recorder.command_buffer_recorded } -> std::same_as<bool&>;
    { recorder.failure_stage } -> std::same_as<render::vulkan_backend::vulkan_command_recorder_failure_stage&>;
    { recorder.failure_recording_index } -> std::same_as<std::size_t&>;
    { recorder.planned_batch_count } -> std::same_as<std::size_t&>;
    { recorder.recorded_batch_count } -> std::same_as<std::size_t&>;
    { recorder.recorded_batches } -> std::same_as<std::vector<render::vulkan_backend::vulkan_recorded_draw_batch>&>;
    { recorder.empty() } -> std::same_as<bool>;
    { recorder.completed() } -> std::same_as<bool>;
    { recorder.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::diagnostic_vulkan_command_recorder_options options) {
    { options.ready } -> std::same_as<bool&>;
    { options.fail_at } -> std::same_as<render::vulkan_backend::vulkan_command_recorder_failure_stage&>;
    { options.fail_recording_index } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_frame_result result) {
    { result.surface } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { result.lifecycle } -> std::same_as<render::vulkan_backend::vulkan_backend_lifecycle_readiness&>;
    { result.command_recorder } -> std::same_as<render::vulkan_backend::vulkan_backend_command_recorder_state&>;
    { result.reached_stage } -> std::same_as<render::vulkan_backend::vulkan_backend_frame_stage&>;
    { result.lifecycle_ready } -> std::same_as<bool&>;
    { result.surface_ready } -> std::same_as<bool&>;
    { result.frame_begun } -> std::same_as<bool&>;
    { result.commands_recorded } -> std::same_as<bool&>;
    { result.frame_submitted } -> std::same_as<bool&>;
    { result.frame_presented } -> std::same_as<bool&>;
    { result.attempted } -> std::same_as<bool&>;
    { result.fallback_required } -> std::same_as<bool&>;
    { result.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { result.planned_batch_count } -> std::same_as<std::size_t&>;
    { result.recorded_batch_count } -> std::same_as<std::size_t&>;
    { result.clipped_draw_call_count } -> std::same_as<std::size_t&>;
    { result.discarded_draw_call_count } -> std::same_as<std::size_t&>;
    { result.completed() } -> std::same_as<bool>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_device_interface& device,
    render::vulkan_backend::vulkan_command_recorder_interface& command_recorder,
    const render::render_draw_list& draw_list,
    render::render_rect viewport) {
    { render::vulkan_backend::submit_vulkan_backend_frame(device, draw_list, viewport) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_frame_result>;
    { render::vulkan_backend::submit_vulkan_backend_frame(device, command_recorder, draw_list, viewport) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_frame_result>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::none),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::not_requested),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::instance_unavailable),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::device_unavailable),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::swapchain_unavailable),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::command_recorder_unavailable),
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

static_assert(requires(render::vulkan_backend::vulkan_backend_fallback_reason reason) {
    { render::vulkan_backend::fallback_reason_name(reason) } -> std::same_as<std::string_view>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_stage::not_started),
    render::vulkan_backend::vulkan_backend_frame_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_stage::backend_attempted),
    render::vulkan_backend::vulkan_backend_frame_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_stage::lifecycle_ready),
    render::vulkan_backend::vulkan_backend_frame_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_stage::surface_extent_ready),
    render::vulkan_backend::vulkan_backend_frame_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_stage::frame_plan_ready),
    render::vulkan_backend::vulkan_backend_frame_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_stage::frame_begun),
    render::vulkan_backend::vulkan_backend_frame_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_stage::commands_recorded),
    render::vulkan_backend::vulkan_backend_frame_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_stage::frame_submitted),
    render::vulkan_backend::vulkan_backend_frame_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_stage::frame_presented),
    render::vulkan_backend::vulkan_backend_frame_stage>);

static_assert(requires(render::vulkan_backend::vulkan_backend_frame_stage stage) {
    { render::vulkan_backend::frame_stage_name(stage) } -> std::same_as<std::string_view>;
});

} // namespace
