#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_command_recording.h"
#include "render/vulkan/vulkan_backend_command_submit.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_graphics_pipeline.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_native_symbols.h"
#include "render/vulkan/vulkan_backend_queue_submit.h"
#include "render/vulkan/vulkan_backend_render_pass.h"
#include "render/vulkan/vulkan_backend_pipeline_layout.h"
#include "render/vulkan/vulkan_backend_shader_module.h"
#include "render/vulkan/vulkan_backend_swapchain.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept VulkanLoaderInterface = requires(
    T& loader,
    const render::vulkan_backend::vulkan_loader_probe_request& request) {
    { loader.probe_loader(request) }
        -> std::same_as<render::vulkan_backend::vulkan_loader_probe_result>;
};

static_assert(VulkanLoaderInterface<render::vulkan_backend::vulkan_loader_interface>);
static_assert(VulkanLoaderInterface<render::vulkan_backend::fake_vulkan_loader>);
static_assert(VulkanLoaderInterface<render::vulkan_backend::system_vulkan_loader>);
static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_loader>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_loader,
    render::vulkan_backend::fake_vulkan_loader_options>);
static_assert(std::default_initializable<render::vulkan_backend::system_vulkan_loader>);

template <typename T>
concept VulkanInstanceFactoryInterface = requires(
    T& factory,
    const render::vulkan_backend::vulkan_loader_readiness_state& loader_readiness,
    const render::vulkan_backend::vulkan_instance_create_request& request) {
    { factory.create_instance(loader_readiness, request) }
        -> std::same_as<render::vulkan_backend::vulkan_instance_create_result>;
};

static_assert(VulkanInstanceFactoryInterface<render::vulkan_backend::vulkan_instance_factory_interface>);
static_assert(VulkanInstanceFactoryInterface<render::vulkan_backend::fake_vulkan_instance_factory>);
static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_instance_factory>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_instance_factory,
    render::vulkan_backend::fake_vulkan_instance_factory_options>);

template <typename T>
concept VulkanDeviceFactoryInterface = requires(
    T& factory,
    const render::vulkan_backend::vulkan_instance_create_result& instance_result,
    const render::vulkan_backend::vulkan_device_create_request& request) {
    { factory.create_device(instance_result, request) }
        -> std::same_as<render::vulkan_backend::vulkan_device_create_result>;
};

static_assert(VulkanDeviceFactoryInterface<render::vulkan_backend::vulkan_device_factory_interface>);
static_assert(VulkanDeviceFactoryInterface<render::vulkan_backend::fake_vulkan_device_factory>);
static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_device_factory>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_device_factory,
    render::vulkan_backend::fake_vulkan_device_factory_options>);

template <typename T>
concept VulkanSwapchainFactoryInterface = requires(
    T& factory,
    const render::vulkan_backend::vulkan_device_create_result& device_result,
    const render::vulkan_backend::vulkan_swapchain_create_request& request) {
    { factory.create_swapchain(device_result, request) }
        -> std::same_as<render::vulkan_backend::vulkan_swapchain_create_result>;
};

static_assert(VulkanSwapchainFactoryInterface<render::vulkan_backend::vulkan_swapchain_factory_interface>);
static_assert(VulkanSwapchainFactoryInterface<render::vulkan_backend::fake_vulkan_swapchain_factory>);
static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_swapchain_factory>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_swapchain_factory,
    render::vulkan_backend::fake_vulkan_swapchain_factory_options>);

template <typename T>
concept VulkanRenderPassFactoryInterface = requires(
    T& factory,
    const render::vulkan_backend::vulkan_swapchain_create_result& swapchain_result,
    const render::vulkan_backend::vulkan_render_pass_create_request& request) {
    { factory.create_render_pass(swapchain_result, request) }
        -> std::same_as<render::vulkan_backend::vulkan_render_pass_create_result>;
};

static_assert(VulkanRenderPassFactoryInterface<render::vulkan_backend::vulkan_render_pass_factory_interface>);
static_assert(VulkanRenderPassFactoryInterface<render::vulkan_backend::fake_vulkan_render_pass_factory>);
static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_render_pass_factory>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_render_pass_factory,
    render::vulkan_backend::fake_vulkan_render_pass_factory_options>);

template <typename T>
concept VulkanCommandRecordingReadinessFactoryInterface = requires(
    T& factory,
    const render::vulkan_backend::vulkan_render_pass_create_result& render_pass_result,
    const render::vulkan_backend::vulkan_command_recording_readiness_request& request) {
    { factory.check_command_recording_readiness(render_pass_result, request) }
        -> std::same_as<render::vulkan_backend::vulkan_command_recording_readiness_result>;
};

static_assert(VulkanCommandRecordingReadinessFactoryInterface<
              render::vulkan_backend::vulkan_command_recording_readiness_factory_interface>);
static_assert(VulkanCommandRecordingReadinessFactoryInterface<
              render::vulkan_backend::fake_vulkan_command_recording_readiness_factory>);
static_assert(std::default_initializable<
              render::vulkan_backend::fake_vulkan_command_recording_readiness_factory>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_command_recording_readiness_factory,
    render::vulkan_backend::fake_vulkan_command_recording_readiness_factory_options>);

template <typename T>
concept VulkanCommandSubmitReadinessFactoryInterface = requires(
    T& factory,
    const render::vulkan_backend::vulkan_command_recording_readiness_result& command_recording_result,
    const render::vulkan_backend::vulkan_command_submit_readiness_request& request) {
    { factory.check_command_submit_readiness(command_recording_result, request) }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_readiness_result>;
};

static_assert(VulkanCommandSubmitReadinessFactoryInterface<
              render::vulkan_backend::vulkan_command_submit_readiness_factory_interface>);
static_assert(VulkanCommandSubmitReadinessFactoryInterface<
              render::vulkan_backend::fake_vulkan_command_submit_readiness_factory>);
static_assert(std::default_initializable<
              render::vulkan_backend::fake_vulkan_command_submit_readiness_factory>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_command_submit_readiness_factory,
    render::vulkan_backend::fake_vulkan_command_submit_readiness_factory_options>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_loader_probe_status::not_checked),
    render::vulkan_backend::vulkan_loader_probe_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_loader_probe_status::available),
    render::vulkan_backend::vulkan_loader_probe_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_loader_probe_status::library_missing),
    render::vulkan_backend::vulkan_loader_probe_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_loader_probe_status::required_symbol_missing),
    render::vulkan_backend::vulkan_loader_probe_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_loader_readiness_status::not_checked),
    render::vulkan_backend::vulkan_loader_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_loader_readiness_status::ready),
    render::vulkan_backend::vulkan_loader_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_loader_readiness_status::library_missing),
    render::vulkan_backend::vulkan_loader_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_loader_readiness_status::required_symbol_missing),
    render::vulkan_backend::vulkan_loader_readiness_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_instance_create_status::not_requested),
    render::vulkan_backend::vulkan_instance_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_instance_create_status::created),
    render::vulkan_backend::vulkan_instance_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_instance_create_status::loader_unavailable),
    render::vulkan_backend::vulkan_instance_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_instance_create_status::missing_required_extension),
    render::vulkan_backend::vulkan_instance_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_instance_create_status::missing_requested_layer),
    render::vulkan_backend::vulkan_instance_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_instance_create_status::creation_failed),
    render::vulkan_backend::vulkan_instance_create_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_queue_capability::graphics),
    render::vulkan_backend::vulkan_device_queue_capability>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_queue_capability::present),
    render::vulkan_backend::vulkan_device_queue_capability>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_queue_capability::compute),
    render::vulkan_backend::vulkan_device_queue_capability>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_queue_capability::transfer),
    render::vulkan_backend::vulkan_device_queue_capability>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_create_status::not_requested),
    render::vulkan_backend::vulkan_device_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_create_status::created),
    render::vulkan_backend::vulkan_device_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_create_status::instance_unavailable),
    render::vulkan_backend::vulkan_device_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_create_status::no_suitable_device),
    render::vulkan_backend::vulkan_device_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_create_status::missing_required_device_extension),
    render::vulkan_backend::vulkan_device_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_create_status::missing_required_queue),
    render::vulkan_backend::vulkan_device_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_device_create_status::creation_failed),
    render::vulkan_backend::vulkan_device_create_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_create_status::not_requested),
    render::vulkan_backend::vulkan_swapchain_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_create_status::created),
    render::vulkan_backend::vulkan_swapchain_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_create_status::device_unavailable),
    render::vulkan_backend::vulkan_swapchain_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_create_status::invalid_surface_extent),
    render::vulkan_backend::vulkan_swapchain_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_create_status::missing_present_queue),
    render::vulkan_backend::vulkan_swapchain_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_create_status::unsupported_present_mode),
    render::vulkan_backend::vulkan_swapchain_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_create_status::creation_failed),
    render::vulkan_backend::vulkan_swapchain_create_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_attachment_role::color),
    render::vulkan_backend::vulkan_render_pass_attachment_role>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_attachment_role::depth_stencil),
    render::vulkan_backend::vulkan_render_pass_attachment_role>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_attachment_format::undefined),
    render::vulkan_backend::vulkan_render_pass_attachment_format>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_attachment_format::rgba8_unorm),
    render::vulkan_backend::vulkan_render_pass_attachment_format>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_attachment_format::bgra8_unorm),
    render::vulkan_backend::vulkan_render_pass_attachment_format>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_attachment_format::depth24_stencil8),
    render::vulkan_backend::vulkan_render_pass_attachment_format>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_create_status::not_requested),
    render::vulkan_backend::vulkan_render_pass_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_create_status::created),
    render::vulkan_backend::vulkan_render_pass_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_create_status::swapchain_unavailable),
    render::vulkan_backend::vulkan_render_pass_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_create_status::invalid_extent),
    render::vulkan_backend::vulkan_render_pass_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_create_status::invalid_format),
    render::vulkan_backend::vulkan_render_pass_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_create_status::missing_attachments),
    render::vulkan_backend::vulkan_render_pass_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_render_pass_create_status::creation_failed),
    render::vulkan_backend::vulkan_render_pass_create_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recording_readiness_status::not_requested),
    render::vulkan_backend::vulkan_command_recording_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recording_readiness_status::ready),
    render::vulkan_backend::vulkan_command_recording_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recording_readiness_status::render_pass_unavailable),
    render::vulkan_backend::vulkan_command_recording_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recording_readiness_status::framebuffer_unavailable),
    render::vulkan_backend::vulkan_command_recording_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recording_readiness_status::pipeline_incompatible),
    render::vulkan_backend::vulkan_command_recording_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recording_readiness_status::command_buffer_unavailable),
    render::vulkan_backend::vulkan_command_recording_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recording_readiness_status::unsupported_draw_batch),
    render::vulkan_backend::vulkan_command_recording_readiness_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_failure_mode::none),
    render::vulkan_backend::vulkan_command_submit_failure_mode>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_failure_mode::recoverable),
    render::vulkan_backend::vulkan_command_submit_failure_mode>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_failure_mode::fatal),
    render::vulkan_backend::vulkan_command_submit_failure_mode>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_readiness_status::not_requested),
    render::vulkan_backend::vulkan_command_submit_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_readiness_status::ready),
    render::vulkan_backend::vulkan_command_submit_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_readiness_status::command_recording_unavailable),
    render::vulkan_backend::vulkan_command_submit_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_readiness_status::command_buffer_unavailable),
    render::vulkan_backend::vulkan_command_submit_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_readiness_status::sync_primitives_unavailable),
    render::vulkan_backend::vulkan_command_submit_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_readiness_status::submit_queue_unavailable),
    render::vulkan_backend::vulkan_command_submit_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_readiness_status::present_target_unavailable),
    render::vulkan_backend::vulkan_command_submit_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_readiness_status::submit_failed_recoverable),
    render::vulkan_backend::vulkan_command_submit_readiness_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_readiness_status::submit_failed_fatal),
    render::vulkan_backend::vulkan_command_submit_readiness_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_frame_status::not_requested),
    render::vulkan_backend::vulkan_command_submit_frame_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_frame_status::submit_ready),
    render::vulkan_backend::vulkan_command_submit_frame_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_frame_status::submitted),
    render::vulkan_backend::vulkan_command_submit_frame_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_frame_status::submit_failed_recoverable),
    render::vulkan_backend::vulkan_command_submit_frame_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_submit_frame_status::submit_failed_fatal),
    render::vulkan_backend::vulkan_command_submit_frame_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_adapter_call_status::not_called),
    render::vulkan_backend::vulkan_queue_submit_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_adapter_call_status::completed),
    render::vulkan_backend::vulkan_queue_submit_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_adapter_call_status::failed_recoverable),
    render::vulkan_backend::vulkan_queue_submit_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_adapter_call_status::failed_fatal),
    render::vulkan_backend::vulkan_queue_submit_adapter_call_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::not_requested),
    render::vulkan_backend::vulkan_queue_submit_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::submitted_and_presented),
    render::vulkan_backend::vulkan_queue_submit_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::command_submit_unavailable),
    render::vulkan_backend::vulkan_queue_submit_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::command_buffer_unavailable),
    render::vulkan_backend::vulkan_queue_submit_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::submit_queue_unavailable),
    render::vulkan_backend::vulkan_queue_submit_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::present_target_unavailable),
    render::vulkan_backend::vulkan_queue_submit_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::adapter_unavailable),
    render::vulkan_backend::vulkan_queue_submit_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::submit_failed_recoverable),
    render::vulkan_backend::vulkan_queue_submit_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::submit_failed_fatal),
    render::vulkan_backend::vulkan_queue_submit_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::present_failed_recoverable),
    render::vulkan_backend::vulkan_queue_submit_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_queue_submit_present_status::present_failed_fatal),
    render::vulkan_backend::vulkan_queue_submit_present_status>);

static_assert(requires(render::vulkan_backend::vulkan_loader_probe_status status) {
    { render::vulkan_backend::loader_probe_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_loader_readiness_status status) {
    { render::vulkan_backend::loader_readiness_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_native_entrypoint_stage::command_buffer_recording),
    render::vulkan_backend::vulkan_native_entrypoint_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_native_entrypoint_stage::queue_submit),
    render::vulkan_backend::vulkan_native_entrypoint_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_native_entrypoint_stage::queue_present),
    render::vulkan_backend::vulkan_native_entrypoint_stage>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_native_function_table_status::not_checked),
    render::vulkan_backend::vulkan_native_function_table_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_native_function_table_status::ready),
    render::vulkan_backend::vulkan_native_function_table_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_native_function_table_status::loader_unavailable),
    render::vulkan_backend::vulkan_native_function_table_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_native_function_table_status::missing_command_buffer_recording_symbol),
    render::vulkan_backend::vulkan_native_function_table_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_native_function_table_status::missing_queue_submit_symbol),
    render::vulkan_backend::vulkan_native_function_table_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_native_function_table_status::missing_queue_present_symbol),
    render::vulkan_backend::vulkan_native_function_table_status>);

static_assert(requires(
    render::vulkan_backend::vulkan_native_entrypoint_stage stage,
    render::vulkan_backend::vulkan_native_function_table_status status) {
    { render::vulkan_backend::native_entrypoint_stage_name(stage) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::native_function_table_status_name(status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_instance_create_status status) {
    { render::vulkan_backend::instance_create_status_name(status) } -> std::same_as<std::string_view>;
    { render::vulkan_backend::vulkan_validation_layer_name() } -> std::same_as<std::string_view>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_device_queue_capability capability,
    render::vulkan_backend::vulkan_device_create_status status) {
    { render::vulkan_backend::device_queue_capability_name(capability) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::device_create_status_name(status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_swapchain_create_status status) {
    { render::vulkan_backend::swapchain_create_status_name(status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_render_pass_attachment_role role,
    render::vulkan_backend::vulkan_render_pass_attachment_format format,
    render::vulkan_backend::vulkan_render_pass_create_status status) {
    { render::vulkan_backend::render_pass_attachment_role_name(role) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::render_pass_attachment_format_name(format) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::render_pass_create_status_name(status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_batch_kind kind,
    render::vulkan_backend::vulkan_command_recording_readiness_status status) {
    { render::vulkan_backend::command_recording_batch_kind_name(kind) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::command_recording_readiness_status_name(status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_command_submit_failure_mode failure_mode,
    render::vulkan_backend::vulkan_command_submit_readiness_status readiness_status,
    render::vulkan_backend::vulkan_command_submit_frame_status frame_status) {
    { render::vulkan_backend::command_submit_failure_mode_name(failure_mode) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::command_submit_readiness_status_name(readiness_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::command_submit_frame_status_name(frame_status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_queue_submit_adapter_call_status call_status,
    render::vulkan_backend::vulkan_queue_submit_present_status present_status) {
    { render::vulkan_backend::queue_submit_adapter_call_status_name(call_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::queue_submit_present_status_name(present_status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_loader_probe_request request) {
    { request.candidate_library_names } -> std::same_as<std::vector<std::string>&>;
    { request.required_symbol_name } -> std::same_as<std::string&>;
    { request.use_default_library_names } -> std::same_as<bool&>;
});

static_assert(requires(render::vulkan_backend::vulkan_instance_create_request request) {
    { request.app_name } -> std::same_as<std::string&>;
    { request.engine_name } -> std::same_as<std::string&>;
    { request.api_version } -> std::same_as<std::uint32_t&>;
    { request.required_instance_extensions } -> std::same_as<std::vector<std::string>&>;
    { request.optional_instance_extensions } -> std::same_as<std::vector<std::string>&>;
    { request.requested_layers } -> std::same_as<std::vector<std::string>&>;
    { request.enable_validation } -> std::same_as<bool&>;
});

static_assert(requires(render::vulkan_backend::vulkan_device_create_request request) {
    { request.required_device_extensions } -> std::same_as<std::vector<std::string>&>;
    { request.optional_device_extensions } -> std::same_as<std::vector<std::string>&>;
    { request.required_queue_capabilities }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_device_queue_capability>&>;
});

static_assert(requires(render::vulkan_backend::vulkan_swapchain_create_request request) {
    { request.requested_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { request.requested_present_mode }
        -> std::same_as<render::vulkan_backend::vulkan_swapchain_present_mode&>;
    { request.min_image_count } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::vulkan_backend::vulkan_render_pass_attachment_request attachment) {
    { attachment.role }
        -> std::same_as<render::vulkan_backend::vulkan_render_pass_attachment_role&>;
    { attachment.format }
        -> std::same_as<render::vulkan_backend::vulkan_render_pass_attachment_format&>;
});

static_assert(requires(render::vulkan_backend::vulkan_render_pass_create_request request) {
    { request.framebuffer_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { request.attachments }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_render_pass_attachment_request>&>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_recording_readiness_request request) {
    { request.frame_plan } -> std::same_as<render::vulkan_backend::vulkan_frame_plan&>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_submit_readiness_request request) {
    { request.sync_policy }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_policy&>;
    { request.require_present_target } -> std::same_as<bool&>;
    { request.present_target_available } -> std::same_as<bool&>;
    { request.image_id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
});

static_assert(requires(render::vulkan_backend::vulkan_queue_submit_present_request request) {
    { request.require_present } -> std::same_as<bool&>;
    { request.image_id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
});

static_assert(requires(render::vulkan_backend::vulkan_loader_probe_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_loader_probe_status&>;
    { result.attempted_library_names } -> std::same_as<std::vector<std::string>&>;
    { result.loaded_library_name } -> std::same_as<std::string&>;
    { result.required_symbol_name } -> std::same_as<std::string&>;
    { result.attempted_library_count } -> std::same_as<std::size_t&>;
    { result.library_found } -> std::same_as<bool&>;
    { result.required_symbol_found } -> std::same_as<bool&>;
    { result.available() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_loader_readiness_state readiness) {
    { readiness.checked } -> std::same_as<bool&>;
    { readiness.status } -> std::same_as<render::vulkan_backend::vulkan_loader_readiness_status&>;
    { readiness.probe_status } -> std::same_as<render::vulkan_backend::vulkan_loader_probe_status&>;
    { readiness.loader_library_available } -> std::same_as<bool&>;
    { readiness.instance_proc_address_available } -> std::same_as<bool&>;
    { readiness.instance_ready } -> std::same_as<bool&>;
    { readiness.loaded_library_name } -> std::same_as<std::string&>;
    { readiness.required_symbol_name } -> std::same_as<std::string&>;
    { readiness.attempted_library_count } -> std::same_as<std::size_t&>;
    { readiness.ready_for_instance() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_native_function_pointer pointer) {
    { pointer.value } -> std::same_as<std::uintptr_t&>;
    { pointer.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_native_entrypoint_symbol_request request) {
    { request.stage } -> std::same_as<render::vulkan_backend::vulkan_native_entrypoint_stage&>;
    { request.name } -> std::same_as<std::string&>;
    { request.required } -> std::same_as<bool&>;
});

static_assert(requires(render::vulkan_backend::vulkan_native_function_table_request request) {
    { request.symbols }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_native_entrypoint_symbol_request>&>;
    { request.include_default_backend_entrypoints } -> std::same_as<bool&>;
});

static_assert(requires(render::vulkan_backend::vulkan_native_entrypoint_symbol_diagnostics symbol) {
    { symbol.stage } -> std::same_as<render::vulkan_backend::vulkan_native_entrypoint_stage&>;
    { symbol.name } -> std::same_as<std::string&>;
    { symbol.required } -> std::same_as<bool&>;
    { symbol.pointer } -> std::same_as<render::vulkan_backend::vulkan_native_function_pointer&>;
    { symbol.available } -> std::same_as<bool&>;
    { symbol.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_native_function_table_diagnostics diagnostics) {
    { diagnostics.checked } -> std::same_as<bool&>;
    { diagnostics.status }
        -> std::same_as<render::vulkan_backend::vulkan_native_function_table_status&>;
    { diagnostics.fallback_reason }
        -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { diagnostics.loader } -> std::same_as<render::vulkan_backend::vulkan_loader_readiness_state&>;
    { diagnostics.loader_ready } -> std::same_as<bool&>;
    { diagnostics.command_buffer_recording_ready } -> std::same_as<bool&>;
    { diagnostics.queue_submit_ready } -> std::same_as<bool&>;
    { diagnostics.queue_present_ready } -> std::same_as<bool&>;
    { diagnostics.requested_symbol_count } -> std::same_as<std::size_t&>;
    { diagnostics.required_symbol_count } -> std::same_as<std::size_t&>;
    { diagnostics.available_symbol_count } -> std::same_as<std::size_t&>;
    { diagnostics.missing_required_symbol_count } -> std::same_as<std::size_t&>;
    { diagnostics.missing_symbol_stage }
        -> std::same_as<render::vulkan_backend::vulkan_native_entrypoint_stage&>;
    { diagnostics.missing_symbol_name } -> std::same_as<std::string&>;
    { diagnostics.symbols }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_native_entrypoint_symbol_diagnostics>&>;
    { diagnostics.diagnostic } -> std::same_as<std::string&>;
    { diagnostics.ready_for_backend_path() } -> std::same_as<bool>;
    { diagnostics.blocked() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_instance_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_instance_create_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_instance_create_status&>;
    { result.loader } -> std::same_as<render::vulkan_backend::vulkan_loader_readiness_state&>;
    { result.handle } -> std::same_as<render::vulkan_backend::vulkan_instance_handle&>;
    { result.selected_extensions } -> std::same_as<std::vector<std::string>&>;
    { result.enabled_layers } -> std::same_as<std::vector<std::string>&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ready_for_device() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_device_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_queue_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_device_queue_selection selection) {
    { selection.capability }
        -> std::same_as<render::vulkan_backend::vulkan_device_queue_capability&>;
    { selection.family_index } -> std::same_as<std::size_t&>;
    { selection.queue } -> std::same_as<render::vulkan_backend::vulkan_queue_handle&>;
    { selection.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_device_create_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_device_create_status&>;
    { result.instance } -> std::same_as<render::vulkan_backend::vulkan_instance_create_result&>;
    { result.handle } -> std::same_as<render::vulkan_backend::vulkan_device_handle&>;
    { result.selected_extensions } -> std::same_as<std::vector<std::string>&>;
    { result.selected_queues }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_device_queue_selection>&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ready_for_backend() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_swapchain_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_swapchain_create_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_swapchain_create_status&>;
    { result.device } -> std::same_as<render::vulkan_backend::vulkan_device_create_result&>;
    { result.handle } -> std::same_as<render::vulkan_backend::vulkan_swapchain_handle&>;
    { result.requested_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { result.selected_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { result.requested_present_mode }
        -> std::same_as<render::vulkan_backend::vulkan_swapchain_present_mode&>;
    { result.selected_present_mode }
        -> std::same_as<render::vulkan_backend::vulkan_swapchain_present_mode&>;
    { result.requested_present_mode_supported } -> std::same_as<bool&>;
    { result.fallback_to_fifo } -> std::same_as<bool&>;
    { result.min_image_count } -> std::same_as<std::size_t&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ready_for_frame() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_render_pass_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_framebuffer_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_render_pass_create_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_render_pass_create_status&>;
    { result.swapchain } -> std::same_as<render::vulkan_backend::vulkan_swapchain_create_result&>;
    { result.render_pass } -> std::same_as<render::vulkan_backend::vulkan_render_pass_handle&>;
    { result.framebuffer } -> std::same_as<render::vulkan_backend::vulkan_framebuffer_handle&>;
    { result.requested_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { result.selected_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { result.requested_attachments }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_render_pass_attachment_request>&>;
    { result.selected_attachments }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_render_pass_attachment_request>&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ready_for_pipeline() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_recording_command_buffer_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_command_recording_pipeline_attachment_compatibility compatibility) {
    { compatibility.role }
        -> std::same_as<render::vulkan_backend::vulkan_render_pass_attachment_role&>;
    { compatibility.format }
        -> std::same_as<render::vulkan_backend::vulkan_render_pass_attachment_format&>;
    { compatibility.compatible } -> std::same_as<bool&>;
    { compatibility.completed() } -> std::same_as<bool>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_command_recording_draw_batch_compatibility compatibility) {
    { compatibility.kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { compatibility.command_index } -> std::same_as<std::size_t&>;
    { compatibility.supported } -> std::same_as<bool&>;
    { compatibility.clipped } -> std::same_as<bool&>;
    { compatibility.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_recording_readiness_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_command_recording_readiness_status&>;
    { result.render_pass } -> std::same_as<render::vulkan_backend::vulkan_render_pass_create_result&>;
    { result.command_buffer }
        -> std::same_as<render::vulkan_backend::vulkan_command_recording_command_buffer_handle&>;
    { result.planned_batch_count } -> std::same_as<std::size_t&>;
    { result.recordable_batch_count } -> std::same_as<std::size_t&>;
    { result.clipped_draw_call_count } -> std::same_as<std::size_t&>;
    { result.discarded_draw_call_count } -> std::same_as<std::size_t&>;
    { result.empty_draw_plan } -> std::same_as<bool&>;
    { result.render_pass_available } -> std::same_as<bool&>;
    { result.framebuffer_available } -> std::same_as<bool&>;
    { result.pipeline_compatible } -> std::same_as<bool&>;
    { result.command_buffer_available } -> std::same_as<bool&>;
    { result.unsupported_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { result.unsupported_command_index } -> std::same_as<std::size_t&>;
    { result.attachment_compatibility }
        -> std::same_as<std::vector<
            render::vulkan_backend::vulkan_command_recording_pipeline_attachment_compatibility>&>;
    { result.batch_compatibility }
        -> std::same_as<std::vector<
            render::vulkan_backend::vulkan_command_recording_draw_batch_compatibility>&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.render_pass_ready() } -> std::same_as<bool>;
    { result.framebuffer_ready() } -> std::same_as<bool>;
    { result.pipeline_ready() } -> std::same_as<bool>;
    { result.command_buffer_ready() } -> std::same_as<bool>;
    { result.ready_for_recording() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_submit_sync_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_submit_sync_policy policy) {
    { policy.require_image_available_semaphore } -> std::same_as<bool&>;
    { policy.require_render_finished_semaphore } -> std::same_as<bool&>;
    { policy.require_frame_fence } -> std::same_as<bool&>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_command_submit_sync_primitives sync,
    const render::vulkan_backend::vulkan_command_submit_sync_policy& policy) {
    { sync.image_available_semaphore }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_handle&>;
    { sync.render_finished_semaphore }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_handle&>;
    { sync.frame_fence }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_handle&>;
    { sync.ready_for_submit(policy) } -> std::same_as<bool>;
    { sync.ready_for_present(policy) } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_submit_frame_result frame) {
    { frame.checked } -> std::same_as<bool&>;
    { frame.status } -> std::same_as<render::vulkan_backend::vulkan_command_submit_frame_status&>;
    { frame.submit_attempted } -> std::same_as<bool&>;
    { frame.submitted } -> std::same_as<bool&>;
    { frame.present_ready } -> std::same_as<bool&>;
    { frame.failure_mode } -> std::same_as<render::vulkan_backend::vulkan_command_submit_failure_mode&>;
    { frame.diagnostic } -> std::same_as<std::string&>;
    { frame.completed() } -> std::same_as<bool>;
    { frame.recoverable_failure() } -> std::same_as<bool>;
    { frame.fatal_failure() } -> std::same_as<bool>;
    { frame.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_submit_readiness_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_readiness_status&>;
    { result.command_recording }
        -> std::same_as<render::vulkan_backend::vulkan_command_recording_readiness_result&>;
    { result.command_buffer }
        -> std::same_as<render::vulkan_backend::vulkan_command_recording_command_buffer_handle&>;
    { result.submit_queue } -> std::same_as<render::vulkan_backend::vulkan_queue_handle&>;
    { result.sync_primitives }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_primitives&>;
    { result.image_id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
    { result.planned_batch_count } -> std::same_as<std::size_t&>;
    { result.submitted_batch_count } -> std::same_as<std::size_t&>;
    { result.command_recording_available } -> std::same_as<bool&>;
    { result.command_buffer_available } -> std::same_as<bool&>;
    { result.sync_primitives_available } -> std::same_as<bool&>;
    { result.submit_queue_available } -> std::same_as<bool&>;
    { result.present_target_available } -> std::same_as<bool&>;
    { result.submit_attempted } -> std::same_as<bool&>;
    { result.failure_mode } -> std::same_as<render::vulkan_backend::vulkan_command_submit_failure_mode&>;
    { result.frame_result }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_frame_result&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.preconditions_ready() } -> std::same_as<bool>;
    { result.ready_for_submit() } -> std::same_as<bool>;
    { result.ready_for_present() } -> std::same_as<bool>;
    { result.recoverable_submit_failure() } -> std::same_as<bool>;
    { result.fatal_submit_failure() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_queue_submit_adapter_operation_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_call_status&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_queue_submit_adapter_submit_call call) {
    { call.queue } -> std::same_as<render::vulkan_backend::vulkan_queue_handle&>;
    { call.command_buffer }
        -> std::same_as<render::vulkan_backend::vulkan_command_recording_command_buffer_handle&>;
    { call.sync_primitives }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_primitives&>;
    { call.batch_count } -> std::same_as<std::size_t&>;
    { call.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_queue_submit_adapter_present_call call) {
    { call.queue } -> std::same_as<render::vulkan_backend::vulkan_queue_handle&>;
    { call.swapchain } -> std::same_as<render::vulkan_backend::vulkan_swapchain_handle&>;
    { call.image_id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
    { call.wait_render_finished_semaphore }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_handle&>;
    { call.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_queue_submit_adapter_function_table table) {
    { table.submit } -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_submit_fn&>;
    { table.present } -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_present_fn&>;
    { table.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_queue_submit_adapter adapter) {
    { adapter.user_data } -> std::same_as<void*&>;
    { adapter.functions }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_function_table&>;
    { adapter.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_queue_submit_present_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_queue_submit_present_status&>;
    { result.command_submit }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_readiness_result&>;
    { result.submit_call }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_submit_call&>;
    { result.present_call }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_present_call&>;
    { result.submit_result }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_operation_result&>;
    { result.present_result }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_operation_result&>;
    { result.submit_called } -> std::same_as<bool&>;
    { result.present_called } -> std::same_as<bool&>;
    { result.submit_order } -> std::same_as<std::size_t&>;
    { result.present_order } -> std::same_as<std::size_t&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.submit_before_present() } -> std::same_as<bool>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_queue_submit_adapter_state state) {
    { state.submit_call_count } -> std::same_as<std::size_t&>;
    { state.present_call_count } -> std::same_as<std::size_t&>;
    { state.next_order } -> std::same_as<std::size_t&>;
    { state.submit_order } -> std::same_as<std::size_t&>;
    { state.present_order } -> std::same_as<std::size_t&>;
    { state.present_called_before_submit } -> std::same_as<bool&>;
    { state.last_submit_call }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_submit_call&>;
    { state.last_present_call }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_present_call&>;
    { state.submit_before_present() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_loader_options options) {
    { options.library_name } -> std::same_as<std::string&>;
    { options.library_available } -> std::same_as<bool&>;
    { options.required_symbol_available } -> std::same_as<bool&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_native_symbol_resolver_options options) {
    { options.default_available } -> std::same_as<bool&>;
    { options.available_symbols } -> std::same_as<std::vector<std::string>&>;
    { options.missing_symbols } -> std::same_as<std::vector<std::string>&>;
    { options.pointer_base } -> std::same_as<render::vulkan_backend::vulkan_native_function_pointer&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_native_symbol_resolver_state state) {
    { state.resolve_call_count } -> std::same_as<std::size_t&>;
    { state.requested_symbols } -> std::same_as<std::vector<std::string>&>;
    { state.resolved_symbols } -> std::same_as<std::vector<std::string>&>;
    { state.missing_symbols } -> std::same_as<std::vector<std::string>&>;
});

static_assert(requires(render::vulkan_backend::system_vulkan_native_symbol_resolver_options options) {
    { options.candidate_library_names } -> std::same_as<std::vector<std::string>&>;
    { options.use_default_library_names } -> std::same_as<bool&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_instance_factory_options options) {
    { options.supported_instance_extensions } -> std::same_as<std::vector<std::string>&>;
    { options.supported_layers } -> std::same_as<std::vector<std::string>&>;
    { options.fail_creation } -> std::same_as<bool&>;
    { options.handle } -> std::same_as<render::vulkan_backend::vulkan_instance_handle&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_device_factory_options options) {
    { options.supported_device_extensions } -> std::same_as<std::vector<std::string>&>;
    { options.supported_queue_capabilities }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_device_queue_capability>&>;
    { options.device_available } -> std::same_as<bool&>;
    { options.fail_creation } -> std::same_as<bool&>;
    { options.handle } -> std::same_as<render::vulkan_backend::vulkan_device_handle&>;
    { options.queue_handle_base } -> std::same_as<std::uintptr_t&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_swapchain_factory_options options) {
    { options.supported_present_modes }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_swapchain_present_mode>&>;
    { options.min_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { options.max_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { options.fail_creation } -> std::same_as<bool&>;
    { options.handle } -> std::same_as<render::vulkan_backend::vulkan_swapchain_handle&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_render_pass_factory_options options) {
    { options.supported_formats }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_render_pass_attachment_format>&>;
    { options.fail_creation } -> std::same_as<bool&>;
    { options.render_pass } -> std::same_as<render::vulkan_backend::vulkan_render_pass_handle&>;
    { options.framebuffer } -> std::same_as<render::vulkan_backend::vulkan_framebuffer_handle&>;
});

static_assert(requires(
    render::vulkan_backend::fake_vulkan_command_recording_readiness_factory_options options) {
    { options.supported_batch_kinds }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_batch_kind>&>;
    { options.compatible_attachment_formats }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_render_pass_attachment_format>&>;
    { options.command_buffer_available } -> std::same_as<bool&>;
    { options.command_buffer }
        -> std::same_as<render::vulkan_backend::vulkan_command_recording_command_buffer_handle&>;
});

static_assert(requires(
    render::vulkan_backend::fake_vulkan_command_submit_readiness_factory_options options) {
    { options.sync_primitives_available } -> std::same_as<bool&>;
    { options.submit_queue_available } -> std::same_as<bool&>;
    { options.present_target_available } -> std::same_as<bool&>;
    { options.failure_mode }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_failure_mode&>;
    { options.submit_queue } -> std::same_as<render::vulkan_backend::vulkan_queue_handle&>;
    { options.sync_primitives }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_primitives&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_queue_submit_adapter_options options) {
    { options.submit_status }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_call_status&>;
    { options.present_status }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_call_status&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_shader_module_factory_options options) {
    { options.supported_stages }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_stage>&>;
    { options.create_status }
        -> std::same_as<render::vulkan_backend::vulkan_shader_module_adapter_call_status&>;
    { options.destroy_status }
        -> std::same_as<render::vulkan_backend::vulkan_shader_module_adapter_call_status&>;
    { options.handle } -> std::same_as<render::vulkan_backend::vulkan_shader_module_handle&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_shader_module_factory_state state) {
    { state.create_call_count } -> std::same_as<std::size_t&>;
    { state.destroy_call_count } -> std::same_as<std::size_t&>;
    { state.last_create_call }
        -> std::same_as<render::vulkan_backend::vulkan_shader_module_create_call&>;
    { state.last_destroy_call }
        -> std::same_as<render::vulkan_backend::vulkan_shader_module_destroy_call&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_pipeline_layout_factory_options options) {
    { options.descriptor_set_layout_create_status }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status&>;
    { options.pipeline_layout_create_status }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status&>;
    { options.destroy_status }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status&>;
    { options.descriptor_set_layout_handle_base } -> std::same_as<std::uintptr_t&>;
    { options.pipeline_layout_handle }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_handle&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_pipeline_layout_factory_state state) {
    { state.descriptor_set_layout_create_call_count } -> std::same_as<std::size_t&>;
    { state.pipeline_layout_create_call_count } -> std::same_as<std::size_t&>;
    { state.descriptor_set_layout_destroy_call_count } -> std::same_as<std::size_t&>;
    { state.pipeline_layout_destroy_call_count } -> std::same_as<std::size_t&>;
    { state.last_descriptor_set_layout_create_call }
        -> std::same_as<render::vulkan_backend::vulkan_descriptor_set_layout_create_call&>;
    { state.last_pipeline_layout_create_call }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_create_call&>;
    { state.last_descriptor_set_layout_destroy_call }
        -> std::same_as<render::vulkan_backend::vulkan_descriptor_set_layout_destroy_call&>;
    { state.last_pipeline_layout_destroy_call }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_destroy_call&>;
    { state.destroyed_descriptor_set_layouts }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_descriptor_set_layout_handle>&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_graphics_pipeline_factory_options options) {
    { options.create_status }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status&>;
    { options.destroy_status }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status&>;
    { options.handle } -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_handle&>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_graphics_pipeline_factory_state state) {
    { state.create_call_count } -> std::same_as<std::size_t&>;
    { state.destroy_call_count } -> std::same_as<std::size_t&>;
    { state.last_create_call }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_create_call&>;
    { state.last_destroy_call }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_destroy_call&>;
    { state.destroyed_pipelines }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_graphics_pipeline_handle>&>;
});

static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_queue_submit_adapter>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_queue_submit_adapter,
    render::vulkan_backend::fake_vulkan_queue_submit_adapter_options>);
static_assert(requires(render::vulkan_backend::fake_vulkan_queue_submit_adapter fake) {
    { fake.adapter() } -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter>;
    { fake.state() }
        -> std::same_as<const render::vulkan_backend::fake_vulkan_queue_submit_adapter_state&>;
});

static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_native_symbol_resolver>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_native_symbol_resolver,
    render::vulkan_backend::fake_vulkan_native_symbol_resolver_options>);
static_assert(requires(
    render::vulkan_backend::fake_vulkan_native_symbol_resolver fake,
    std::string_view symbol_name) {
    { fake.resolve_symbol(symbol_name) }
        -> std::same_as<render::vulkan_backend::vulkan_native_function_pointer>;
    { fake.state() }
        -> std::same_as<const render::vulkan_backend::fake_vulkan_native_symbol_resolver_state&>;
});

static_assert(std::default_initializable<render::vulkan_backend::system_vulkan_native_symbol_resolver>);
static_assert(std::constructible_from<
    render::vulkan_backend::system_vulkan_native_symbol_resolver,
    render::vulkan_backend::system_vulkan_native_symbol_resolver_options>);
static_assert(requires(
    render::vulkan_backend::system_vulkan_native_symbol_resolver resolver,
    std::string_view symbol_name) {
    { resolver.resolve_symbol(symbol_name) }
        -> std::same_as<render::vulkan_backend::vulkan_native_function_pointer>;
});

static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_shader_module_factory>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_shader_module_factory,
    render::vulkan_backend::fake_vulkan_shader_module_factory_options>);
static_assert(requires(render::vulkan_backend::fake_vulkan_shader_module_factory fake) {
    { fake.adapter() } -> std::same_as<render::vulkan_backend::vulkan_shader_module_factory_adapter>;
    { fake.state() }
        -> std::same_as<const render::vulkan_backend::fake_vulkan_shader_module_factory_state&>;
});

static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_pipeline_layout_factory>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_pipeline_layout_factory,
    render::vulkan_backend::fake_vulkan_pipeline_layout_factory_options>);
static_assert(requires(render::vulkan_backend::fake_vulkan_pipeline_layout_factory fake) {
    { fake.adapter() } -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_factory_adapter>;
    { fake.state() }
        -> std::same_as<const render::vulkan_backend::fake_vulkan_pipeline_layout_factory_state&>;
});

static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_graphics_pipeline_factory>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_graphics_pipeline_factory,
    render::vulkan_backend::fake_vulkan_graphics_pipeline_factory_options>);
static_assert(requires(render::vulkan_backend::fake_vulkan_graphics_pipeline_factory fake) {
    { fake.adapter() } -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_factory_adapter>;
    { fake.state() }
        -> std::same_as<const render::vulkan_backend::fake_vulkan_graphics_pipeline_factory_state&>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_loader_interface& loader,
    const render::vulkan_backend::vulkan_loader_probe_request& request) {
    { render::vulkan_backend::vulkan_loader_required_symbol_name() } -> std::same_as<std::string_view>;
    { render::vulkan_backend::default_vulkan_loader_library_names() }
        -> std::same_as<std::vector<std::string>>;
    { render::vulkan_backend::probe_vulkan_loader(loader, request) }
        -> std::same_as<render::vulkan_backend::vulkan_loader_probe_result>;
});

static_assert(requires(
    const render::vulkan_backend::vulkan_loader_probe_result& probe_result) {
    { render::vulkan_backend::make_vulkan_loader_readiness_state(probe_result) }
        -> std::same_as<render::vulkan_backend::vulkan_loader_readiness_state>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_native_symbol_resolver_interface& resolver,
    const render::vulkan_backend::vulkan_loader_readiness_state& loader,
    const render::vulkan_backend::vulkan_native_function_table_request& request) {
    { render::vulkan_backend::default_vulkan_native_backend_entrypoints() }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_native_entrypoint_symbol_request>>;
    { render::vulkan_backend::collect_vulkan_native_function_table(
        resolver,
        loader,
        request) } -> std::same_as<render::vulkan_backend::vulkan_native_function_table_diagnostics>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_instance_factory_interface& factory,
    const render::vulkan_backend::vulkan_loader_readiness_state& loader_readiness,
    const render::vulkan_backend::vulkan_instance_create_request& request) {
    { render::vulkan_backend::create_vulkan_instance(
        factory,
        loader_readiness,
        request) } -> std::same_as<render::vulkan_backend::vulkan_instance_create_result>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_device_factory_interface& factory,
    const render::vulkan_backend::vulkan_instance_create_result& instance_result,
    const render::vulkan_backend::vulkan_device_create_request& request) {
    { render::vulkan_backend::create_vulkan_device(
        factory,
        instance_result,
        request) } -> std::same_as<render::vulkan_backend::vulkan_device_create_result>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_swapchain_factory_interface& factory,
    const render::vulkan_backend::vulkan_device_create_result& device_result,
    const render::vulkan_backend::vulkan_swapchain_create_request& request) {
    { render::vulkan_backend::create_vulkan_swapchain(
        factory,
        device_result,
        request) } -> std::same_as<render::vulkan_backend::vulkan_swapchain_create_result>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_render_pass_factory_interface& factory,
    const render::vulkan_backend::vulkan_swapchain_create_result& swapchain_result,
    const render::vulkan_backend::vulkan_render_pass_create_request& request) {
    { render::vulkan_backend::create_vulkan_render_pass(
        factory,
        swapchain_result,
        request) } -> std::same_as<render::vulkan_backend::vulkan_render_pass_create_result>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_command_recording_readiness_factory_interface& factory,
    const render::vulkan_backend::vulkan_render_pass_create_result& render_pass_result,
    const render::vulkan_backend::vulkan_command_recording_readiness_request& request) {
    { render::vulkan_backend::check_vulkan_command_recording_readiness(
        factory,
        render_pass_result,
        request) } -> std::same_as<render::vulkan_backend::vulkan_command_recording_readiness_result>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_command_submit_readiness_factory_interface& factory,
    const render::vulkan_backend::vulkan_command_recording_readiness_result& command_recording_result,
    const render::vulkan_backend::vulkan_command_submit_readiness_request& request) {
    { render::vulkan_backend::check_vulkan_command_submit_readiness(
        factory,
        command_recording_result,
        request) } -> std::same_as<render::vulkan_backend::vulkan_command_submit_readiness_result>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_queue_submit_adapter adapter,
    const render::vulkan_backend::vulkan_command_submit_readiness_result& command_submit,
    const render::vulkan_backend::vulkan_queue_submit_present_request& request) {
    { render::vulkan_backend::submit_and_present_vulkan_queue_frame(
        adapter,
        command_submit,
        request) } -> std::same_as<render::vulkan_backend::vulkan_queue_submit_present_result>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_shader_module_factory_adapter adapter,
    const render::vulkan_backend::vulkan_shader_module_create_request& create_request,
    render::vulkan_backend::vulkan_shader_module_handle handle,
    const std::vector<render::vulkan_backend::vulkan_shader_module_create_request>& requests) {
    { render::vulkan_backend::create_vulkan_shader_module(adapter, create_request) }
        -> std::same_as<render::vulkan_backend::vulkan_shader_module_create_result>;
    { render::vulkan_backend::destroy_vulkan_shader_module(adapter, handle) }
        -> std::same_as<render::vulkan_backend::vulkan_shader_module_destroy_result>;
    { render::vulkan_backend::check_vulkan_shader_module_readiness(adapter, requests) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_shader_module_readiness_state>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_pipeline_layout_factory_adapter adapter,
    const render::vulkan_backend::vulkan_backend_shader_module_readiness_state& shader_modules,
    const render::vulkan_backend::vulkan_pipeline_layout_request& request,
    const render::vulkan_backend::vulkan_pipeline_layout_create_result& create_result) {
    { render::vulkan_backend::create_vulkan_pipeline_layout(
        adapter,
        shader_modules,
        request) } -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_create_result>;
    { render::vulkan_backend::destroy_vulkan_pipeline_layout(adapter, create_result) }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_destroy_result>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_graphics_pipeline_factory_adapter adapter,
    const render::vulkan_backend::vulkan_backend_shader_module_readiness_state& shader_modules,
    const render::vulkan_backend::vulkan_pipeline_layout_create_result& pipeline_layout,
    const render::vulkan_backend::vulkan_graphics_pipeline_request& request,
    render::vulkan_backend::vulkan_graphics_pipeline_handle handle) {
    { render::vulkan_backend::create_vulkan_graphics_pipeline(
        adapter,
        shader_modules,
        pipeline_layout,
        request) } -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_create_result>;
    { render::vulkan_backend::destroy_vulkan_graphics_pipeline(adapter, handle) }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_destroy_result>;
});

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
    { device.acquire_next_image(surface) }
        -> std::same_as<render::vulkan_backend::vulkan_swapchain_acquire_result>;
    { device.record_frame_commands(plan) } -> std::same_as<bool>;
    { device.submit_frame() } -> std::same_as<bool>;
    { device.present_image(render::vulkan_backend::vulkan_swapchain_image_id{}) }
        -> std::same_as<render::vulkan_backend::vulkan_swapchain_present_result>;
    { device.present_frame() } -> std::same_as<bool>;
};

static_assert(VulkanBackendDeviceInterface<render::vulkan_backend::vulkan_backend_device_interface>);
static_assert(VulkanBackendDeviceInterface<render::vulkan_backend::null_vulkan_backend_device>);
static_assert(std::default_initializable<render::vulkan_backend::null_vulkan_backend_device>);
static_assert(std::constructible_from<
    render::vulkan_backend::null_vulkan_backend_device,
    render::vulkan_backend::vulkan_loader_readiness_state>);
static_assert(std::constructible_from<
    render::vulkan_backend::null_vulkan_backend_device,
    const render::vulkan_backend::vulkan_loader_probe_result&>);
static_assert(std::constructible_from<
    render::vulkan_backend::null_vulkan_backend_device,
    render::vulkan_backend::vulkan_instance_create_result>);
static_assert(std::constructible_from<
    render::vulkan_backend::null_vulkan_backend_device,
    render::vulkan_backend::vulkan_device_create_result>);
static_assert(std::constructible_from<
    render::vulkan_backend::null_vulkan_backend_device,
    render::vulkan_backend::vulkan_swapchain_create_result>);
static_assert(std::constructible_from<
    render::vulkan_backend::null_vulkan_backend_device,
    render::vulkan_backend::vulkan_render_pass_create_result>);
static_assert(std::constructible_from<
    render::vulkan_backend::null_vulkan_backend_device,
    render::vulkan_backend::vulkan_command_recording_readiness_result>);
static_assert(std::constructible_from<
    render::vulkan_backend::null_vulkan_backend_device,
    render::vulkan_backend::vulkan_command_submit_readiness_result>);

template <typename T>
concept VulkanPipelineCacheInterface = requires(
    const T& const_cache,
    T& cache,
    const render::vulkan_backend::vulkan_draw_batch& batch) {
    { cache.ensure_pipeline(batch) } -> std::same_as<bool>;
    { const_cache.pipeline_state() }
        -> std::same_as<const render::vulkan_backend::vulkan_backend_pipeline_state&>;
};

static_assert(VulkanPipelineCacheInterface<render::vulkan_backend::vulkan_pipeline_cache_interface>);
static_assert(VulkanPipelineCacheInterface<render::vulkan_backend::diagnostic_vulkan_pipeline_cache>);
static_assert(std::default_initializable<render::vulkan_backend::diagnostic_vulkan_pipeline_cache>);
static_assert(std::constructible_from<
    render::vulkan_backend::diagnostic_vulkan_pipeline_cache,
    render::vulkan_backend::diagnostic_vulkan_pipeline_cache_options>);

static_assert(std::default_initializable<render::vulkan_backend::diagnostic_vulkan_shader_registry>);
static_assert(std::constructible_from<
    render::vulkan_backend::diagnostic_vulkan_shader_registry,
    std::vector<render::vulkan_backend::vulkan_shader_module_descriptor>>);

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
    { lifecycle.render_pass_ready } -> std::same_as<bool&>;
    { lifecycle.pipeline_ready } -> std::same_as<bool&>;
    { lifecycle.command_recorder_ready } -> std::same_as<bool&>;
    { lifecycle.loader } -> std::same_as<render::vulkan_backend::vulkan_loader_readiness_state&>;
    { lifecycle.instance } -> std::same_as<render::vulkan_backend::vulkan_instance_create_result&>;
    { lifecycle.device } -> std::same_as<render::vulkan_backend::vulkan_device_create_result&>;
    { lifecycle.swapchain } -> std::same_as<render::vulkan_backend::vulkan_swapchain_create_result&>;
    { lifecycle.render_pass } -> std::same_as<render::vulkan_backend::vulkan_render_pass_create_result&>;
    { lifecycle.command_recording }
        -> std::same_as<render::vulkan_backend::vulkan_command_recording_readiness_result&>;
    { lifecycle.command_submit }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_readiness_result&>;
    { lifecycle.effective_instance_ready() } -> std::same_as<bool>;
    { lifecycle.effective_device_ready() } -> std::same_as<bool>;
    { lifecycle.effective_swapchain_ready() } -> std::same_as<bool>;
    { lifecycle.effective_render_pass_ready() } -> std::same_as<bool>;
    { lifecycle.effective_pipeline_ready() } -> std::same_as<bool>;
    { lifecycle.effective_command_recorder_ready() } -> std::same_as<bool>;
    { lifecycle.effective_command_submit_ready() } -> std::same_as<bool>;
    { lifecycle.effective_present_target_ready() } -> std::same_as<bool>;
    { lifecycle.ready_for_frame() } -> std::same_as<bool>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle,
    render::vulkan_backend::vulkan_loader_readiness_state loader_readiness) {
    { render::vulkan_backend::apply_vulkan_loader_readiness_to_lifecycle(
        lifecycle,
        loader_readiness) } -> std::same_as<render::vulkan_backend::vulkan_backend_lifecycle_readiness>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle,
    render::vulkan_backend::vulkan_instance_create_result instance_result) {
    { render::vulkan_backend::apply_vulkan_instance_create_result_to_lifecycle(
        lifecycle,
        instance_result) } -> std::same_as<render::vulkan_backend::vulkan_backend_lifecycle_readiness>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle,
    render::vulkan_backend::vulkan_device_create_result device_result) {
    { render::vulkan_backend::apply_vulkan_device_create_result_to_lifecycle(
        lifecycle,
        device_result) } -> std::same_as<render::vulkan_backend::vulkan_backend_lifecycle_readiness>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle,
    render::vulkan_backend::vulkan_swapchain_create_result swapchain_result) {
    { render::vulkan_backend::apply_vulkan_swapchain_create_result_to_lifecycle(
        lifecycle,
        swapchain_result) } -> std::same_as<render::vulkan_backend::vulkan_backend_lifecycle_readiness>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle,
    render::vulkan_backend::vulkan_render_pass_create_result render_pass_result) {
    { render::vulkan_backend::apply_vulkan_render_pass_create_result_to_lifecycle(
        lifecycle,
        render_pass_result) } -> std::same_as<render::vulkan_backend::vulkan_backend_lifecycle_readiness>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle,
    render::vulkan_backend::vulkan_command_recording_readiness_result command_recording_result) {
    { render::vulkan_backend::apply_vulkan_command_recording_readiness_to_lifecycle(
        lifecycle,
        command_recording_result) } -> std::same_as<render::vulkan_backend::vulkan_backend_lifecycle_readiness>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_lifecycle_readiness lifecycle,
    render::vulkan_backend::vulkan_command_submit_readiness_result command_submit_result) {
    { render::vulkan_backend::apply_vulkan_command_submit_readiness_to_lifecycle(
        lifecycle,
        command_submit_result) } -> std::same_as<render::vulkan_backend::vulkan_backend_lifecycle_readiness>;
});

static_assert(requires(render::vulkan_backend::vulkan_swapchain_image_id id) {
    { id.value } -> std::same_as<std::size_t&>;
});

static_assert(requires(render::vulkan_backend::vulkan_swapchain_image_state image) {
    { image.id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
    { image.available } -> std::same_as<bool&>;
    { image.acquired } -> std::same_as<bool&>;
    { image.presented } -> std::same_as<bool&>;
    { image.ready_for_recording() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_acquire_status::not_requested),
    render::vulkan_backend::vulkan_swapchain_acquire_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_acquire_status::acquired),
    render::vulkan_backend::vulkan_swapchain_acquire_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_acquire_status::backpressured),
    render::vulkan_backend::vulkan_swapchain_acquire_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_acquire_status::failed),
    render::vulkan_backend::vulkan_swapchain_acquire_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_present_status::not_requested),
    render::vulkan_backend::vulkan_swapchain_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_present_status::presented),
    render::vulkan_backend::vulkan_swapchain_present_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_present_status::failed),
    render::vulkan_backend::vulkan_swapchain_present_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_present_mode::immediate),
    render::vulkan_backend::vulkan_swapchain_present_mode>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_present_mode::mailbox),
    render::vulkan_backend::vulkan_swapchain_present_mode>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_present_mode::fifo),
    render::vulkan_backend::vulkan_swapchain_present_mode>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_swapchain_present_mode::fifo_relaxed),
    render::vulkan_backend::vulkan_swapchain_present_mode>);

static_assert(requires(
    render::vulkan_backend::vulkan_swapchain_acquire_status acquire_status,
    render::vulkan_backend::vulkan_swapchain_present_status present_status,
    render::vulkan_backend::vulkan_swapchain_present_mode present_mode) {
    { render::vulkan_backend::swapchain_acquire_status_name(acquire_status) } -> std::same_as<std::string_view>;
    { render::vulkan_backend::swapchain_present_status_name(present_status) } -> std::same_as<std::string_view>;
    { render::vulkan_backend::swapchain_present_mode_name(present_mode) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_swapchain_acquire_result acquire) {
    { acquire.status } -> std::same_as<render::vulkan_backend::vulkan_swapchain_acquire_status&>;
    { acquire.image } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_state&>;
    { acquire.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_swapchain_present_result present) {
    { present.status } -> std::same_as<render::vulkan_backend::vulkan_swapchain_present_status&>;
    { present.image_id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
    { present.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_swapchain_lifecycle_state swapchain) {
    { swapchain.acquire_requested } -> std::same_as<bool&>;
    { swapchain.present_requested } -> std::same_as<bool&>;
    { swapchain.acquire } -> std::same_as<render::vulkan_backend::vulkan_swapchain_acquire_result&>;
    { swapchain.present } -> std::same_as<render::vulkan_backend::vulkan_swapchain_present_result&>;
    { swapchain.acquired() } -> std::same_as<bool>;
    { swapchain.presented() } -> std::same_as<bool>;
    { swapchain.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_swapchain_extent_policy_state extent) {
    { extent.checked } -> std::same_as<bool&>;
    { extent.requested_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { extent.selected_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { extent.min_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { extent.max_extent } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { extent.extent_supported } -> std::same_as<bool&>;
    { extent.extent_clamped } -> std::same_as<bool&>;
    { extent.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_swapchain_present_mode_policy_state present_mode) {
    { present_mode.checked } -> std::same_as<bool&>;
    { present_mode.requested_mode } -> std::same_as<render::vulkan_backend::vulkan_swapchain_present_mode&>;
    { present_mode.selected_mode } -> std::same_as<render::vulkan_backend::vulkan_swapchain_present_mode&>;
    { present_mode.requested_mode_supported } -> std::same_as<bool&>;
    { present_mode.fallback_to_fifo } -> std::same_as<bool&>;
    { present_mode.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_swapchain_policy_state policy) {
    { policy.checked } -> std::same_as<bool&>;
    { policy.extent } -> std::same_as<render::vulkan_backend::vulkan_swapchain_extent_policy_state&>;
    { policy.present_mode } -> std::same_as<render::vulkan_backend::vulkan_swapchain_present_mode_policy_state&>;
    { policy.completed() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_acquire_policy_status::not_checked),
    render::vulkan_backend::vulkan_frame_acquire_policy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_acquire_policy_status::not_requested),
    render::vulkan_backend::vulkan_frame_acquire_policy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_acquire_policy_status::acquired),
    render::vulkan_backend::vulkan_frame_acquire_policy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_acquire_policy_status::backpressured),
    render::vulkan_backend::vulkan_frame_acquire_policy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_acquire_policy_status::failed),
    render::vulkan_backend::vulkan_frame_acquire_policy_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_present_result_status::not_checked),
    render::vulkan_backend::vulkan_frame_present_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_present_result_status::not_requested),
    render::vulkan_backend::vulkan_frame_present_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_present_result_status::image_presented),
    render::vulkan_backend::vulkan_frame_present_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_present_result_status::frame_presented),
    render::vulkan_backend::vulkan_frame_present_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_present_result_status::image_failed),
    render::vulkan_backend::vulkan_frame_present_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_present_result_status::frame_failed),
    render::vulkan_backend::vulkan_frame_present_result_status>);

static_assert(requires(
    render::vulkan_backend::vulkan_frame_acquire_policy_status acquire_status,
    render::vulkan_backend::vulkan_frame_present_result_status present_status) {
    { render::vulkan_backend::frame_acquire_policy_status_name(acquire_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::frame_present_result_status_name(present_status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_frame_acquire_policy_diagnostics acquire) {
    { acquire.checked } -> std::same_as<bool&>;
    { acquire.requested } -> std::same_as<bool&>;
    { acquire.swapchain_status } -> std::same_as<render::vulkan_backend::vulkan_swapchain_acquire_status&>;
    { acquire.image_id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
    { acquire.image_available } -> std::same_as<bool&>;
    { acquire.image_acquired } -> std::same_as<bool&>;
    { acquire.backpressured } -> std::same_as<bool&>;
    { acquire.status } -> std::same_as<render::vulkan_backend::vulkan_frame_acquire_policy_status&>;
    { acquire.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { acquire.completed() } -> std::same_as<bool>;
    { acquire.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_frame_present_result_summary present) {
    { present.checked } -> std::same_as<bool&>;
    { present.image_present_requested } -> std::same_as<bool&>;
    { present.frame_present_requested } -> std::same_as<bool&>;
    { present.image_id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
    { present.swapchain_status } -> std::same_as<render::vulkan_backend::vulkan_swapchain_present_status&>;
    { present.image_presented } -> std::same_as<bool&>;
    { present.frame_presented } -> std::same_as<bool&>;
    { present.status } -> std::same_as<render::vulkan_backend::vulkan_frame_present_result_status&>;
    { present.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { present.completed() } -> std::same_as<bool>;
    { present.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_frame_present_policy_state policy) {
    { policy.checked } -> std::same_as<bool&>;
    { policy.acquire_request_count } -> std::same_as<std::size_t&>;
    { policy.present_image_request_count } -> std::same_as<std::size_t&>;
    { policy.backpressure_detected } -> std::same_as<bool&>;
    { policy.acquire } -> std::same_as<render::vulkan_backend::vulkan_frame_acquire_policy_diagnostics&>;
    { policy.present } -> std::same_as<render::vulkan_backend::vulkan_frame_present_result_summary&>;
    { policy.completed() } -> std::same_as<bool>;
    { policy.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_frame_in_flight_id frame) {
    { frame.index } -> std::same_as<std::size_t&>;
    { frame.frame_count } -> std::same_as<std::size_t&>;
    { frame.sequence } -> std::same_as<std::size_t&>;
    { frame.valid() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_sync_token_kind::semaphore),
    render::vulkan_backend::vulkan_frame_sync_token_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_sync_token_kind::fence),
    render::vulkan_backend::vulkan_frame_sync_token_kind>);

static_assert(requires(render::vulkan_backend::vulkan_frame_sync_token_id token) {
    { token.value } -> std::same_as<std::size_t&>;
    { token.kind } -> std::same_as<render::vulkan_backend::vulkan_frame_sync_token_kind&>;
    { token.valid() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_sync_signal_status::not_requested),
    render::vulkan_backend::vulkan_frame_sync_signal_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_sync_signal_status::pending),
    render::vulkan_backend::vulkan_frame_sync_signal_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_sync_signal_status::signaled),
    render::vulkan_backend::vulkan_frame_sync_signal_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_sync_signal_status::failed),
    render::vulkan_backend::vulkan_frame_sync_signal_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_sync_wait_status::not_requested),
    render::vulkan_backend::vulkan_frame_sync_wait_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_sync_wait_status::pending),
    render::vulkan_backend::vulkan_frame_sync_wait_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_sync_wait_status::waited),
    render::vulkan_backend::vulkan_frame_sync_wait_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_sync_wait_status::failed),
    render::vulkan_backend::vulkan_frame_sync_wait_status>);

static_assert(requires(render::vulkan_backend::vulkan_frame_sync_signal_state signal) {
    { signal.token } -> std::same_as<render::vulkan_backend::vulkan_frame_sync_token_id&>;
    { signal.requested } -> std::same_as<bool&>;
    { signal.status } -> std::same_as<render::vulkan_backend::vulkan_frame_sync_signal_status&>;
    { signal.completed() } -> std::same_as<bool>;
    { signal.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_frame_sync_wait_state wait) {
    { wait.token } -> std::same_as<render::vulkan_backend::vulkan_frame_sync_token_id&>;
    { wait.requested } -> std::same_as<bool&>;
    { wait.status } -> std::same_as<render::vulkan_backend::vulkan_frame_sync_wait_status&>;
    { wait.completed() } -> std::same_as<bool>;
    { wait.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_frame_sync_state sync) {
    { sync.frame } -> std::same_as<render::vulkan_backend::vulkan_frame_in_flight_id&>;
    { sync.acquire_signal_image_available_semaphore }
        -> std::same_as<render::vulkan_backend::vulkan_frame_sync_signal_state&>;
    { sync.acquire_signal_fence } -> std::same_as<render::vulkan_backend::vulkan_frame_sync_signal_state&>;
    { sync.submit_wait_image_available_semaphore }
        -> std::same_as<render::vulkan_backend::vulkan_frame_sync_wait_state&>;
    { sync.submit_signal_render_finished_semaphore }
        -> std::same_as<render::vulkan_backend::vulkan_frame_sync_signal_state&>;
    { sync.submit_signal_frame_fence } -> std::same_as<render::vulkan_backend::vulkan_frame_sync_signal_state&>;
    { sync.present_wait_render_finished_semaphore }
        -> std::same_as<render::vulkan_backend::vulkan_frame_sync_wait_state&>;
    { sync.acquire_completed() } -> std::same_as<bool>;
    { sync.submit_completed() } -> std::same_as<bool>;
    { sync.present_completed() } -> std::same_as<bool>;
    { sync.completed() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_resource_kind::swapchain_image),
    render::vulkan_backend::vulkan_frame_resource_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_resource_kind::command_buffer),
    render::vulkan_backend::vulkan_frame_resource_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_resource_kind::descriptor_set),
    render::vulkan_backend::vulkan_frame_resource_kind>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_resource_release_stage::none),
    render::vulkan_backend::vulkan_frame_resource_release_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_resource_release_stage::after_present),
    render::vulkan_backend::vulkan_frame_resource_release_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_resource_release_stage::fallback_cleanup),
    render::vulkan_backend::vulkan_frame_resource_release_stage>);

static_assert(requires(
    render::vulkan_backend::vulkan_frame_resource_kind kind,
    render::vulkan_backend::vulkan_frame_resource_release_stage stage) {
    { render::vulkan_backend::frame_resource_kind_name(kind) } -> std::same_as<std::string_view>;
    { render::vulkan_backend::frame_resource_release_stage_name(stage) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_frame_resource_lifetime_snapshot resource) {
    { resource.kind } -> std::same_as<render::vulkan_backend::vulkan_frame_resource_kind&>;
    { resource.resource_id } -> std::same_as<std::string&>;
    { resource.command_index } -> std::same_as<std::size_t&>;
    { resource.acquired } -> std::same_as<bool&>;
    { resource.released } -> std::same_as<bool&>;
    { resource.release_stage } -> std::same_as<render::vulkan_backend::vulkan_frame_resource_release_stage&>;
    { resource.pending() } -> std::same_as<bool>;
    { resource.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_frame_resource_lifetime_state lifetime) {
    { lifetime.checked } -> std::same_as<bool&>;
    { lifetime.fallback_cleanup } -> std::same_as<bool&>;
    { lifetime.planned_batch_count } -> std::same_as<std::size_t&>;
    { lifetime.tracked_resource_count } -> std::same_as<std::size_t&>;
    { lifetime.acquired_resource_count } -> std::same_as<std::size_t&>;
    { lifetime.released_resource_count } -> std::same_as<std::size_t&>;
    { lifetime.pending_resource_count } -> std::same_as<std::size_t&>;
    { lifetime.fallback_release_count } -> std::same_as<std::size_t&>;
    { lifetime.resources }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_frame_resource_lifetime_snapshot>&>;
    { lifetime.completed() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_step::acquire),
    render::vulkan_backend::vulkan_frame_lifecycle_step>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_step::begin),
    render::vulkan_backend::vulkan_frame_lifecycle_step>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_step::render),
    render::vulkan_backend::vulkan_frame_lifecycle_step>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_step::submit),
    render::vulkan_backend::vulkan_frame_lifecycle_step>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_step::present),
    render::vulkan_backend::vulkan_frame_lifecycle_step>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_order_status::not_started),
    render::vulkan_backend::vulkan_frame_lifecycle_order_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_order_status::started),
    render::vulkan_backend::vulkan_frame_lifecycle_order_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_order_status::completed),
    render::vulkan_backend::vulkan_frame_lifecycle_order_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_order_status::skipped),
    render::vulkan_backend::vulkan_frame_lifecycle_order_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_order_status::failed),
    render::vulkan_backend::vulkan_frame_lifecycle_order_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_order_status::out_of_order),
    render::vulkan_backend::vulkan_frame_lifecycle_order_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_failure_classification::none),
    render::vulkan_backend::vulkan_frame_lifecycle_failure_classification>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_failure_classification::recoverable),
    render::vulkan_backend::vulkan_frame_lifecycle_failure_classification>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_lifecycle_failure_classification::fatal),
    render::vulkan_backend::vulkan_frame_lifecycle_failure_classification>);

static_assert(requires(
    render::vulkan_backend::vulkan_frame_lifecycle_step step,
    render::vulkan_backend::vulkan_frame_lifecycle_order_status status,
    render::vulkan_backend::vulkan_frame_lifecycle_failure_classification classification) {
    { render::vulkan_backend::frame_lifecycle_step_name(step) } -> std::same_as<std::string_view>;
    { render::vulkan_backend::frame_lifecycle_order_status_name(status) } -> std::same_as<std::string_view>;
    { render::vulkan_backend::frame_lifecycle_failure_classification_name(classification) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_frame_lifecycle_step_snapshot snapshot) {
    { snapshot.step } -> std::same_as<render::vulkan_backend::vulkan_frame_lifecycle_step&>;
    { snapshot.expected_order } -> std::same_as<std::size_t&>;
    { snapshot.observed_order } -> std::same_as<std::size_t&>;
    { snapshot.attempted } -> std::same_as<bool&>;
    { snapshot.completed } -> std::same_as<bool&>;
    { snapshot.status } -> std::same_as<render::vulkan_backend::vulkan_frame_lifecycle_order_status&>;
    { snapshot.failure_classification }
        -> std::same_as<render::vulkan_backend::vulkan_frame_lifecycle_failure_classification&>;
    { snapshot.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { snapshot.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_frame_lifecycle_policy_state policy) {
    { policy.checked } -> std::same_as<bool&>;
    { policy.ordering_valid } -> std::same_as<bool&>;
    { policy.recoverable_failure } -> std::same_as<bool&>;
    { policy.fatal_failure } -> std::same_as<bool&>;
    { policy.failed_step } -> std::same_as<render::vulkan_backend::vulkan_frame_lifecycle_step&>;
    { policy.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { policy.attempted_step_count } -> std::same_as<std::size_t&>;
    { policy.completed_step_count } -> std::same_as<std::size_t&>;
    { policy.snapshots }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_frame_lifecycle_step_snapshot>&>;
    { policy.completed() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_resource_binding_kind::batch_uniform),
    render::vulkan_backend::vulkan_resource_binding_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_resource_binding_kind::quad_vertex_buffer),
    render::vulkan_backend::vulkan_resource_binding_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_resource_binding_kind::image_texture),
    render::vulkan_backend::vulkan_resource_binding_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_resource_binding_kind::image_sampler),
    render::vulkan_backend::vulkan_resource_binding_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_resource_binding_kind::text_run_buffer),
    render::vulkan_backend::vulkan_resource_binding_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_resource_binding_kind::text_glyph_atlas),
    render::vulkan_backend::vulkan_resource_binding_kind>);

static_assert(requires(render::vulkan_backend::vulkan_resource_binding_snapshot binding) {
    { binding.set } -> std::same_as<std::size_t&>;
    { binding.binding } -> std::same_as<std::size_t&>;
    { binding.kind } -> std::same_as<render::vulkan_backend::vulkan_resource_binding_kind&>;
    { binding.resource_id } -> std::same_as<std::string&>;
    { binding.required } -> std::same_as<bool&>;
    { binding.available } -> std::same_as<bool&>;
    { binding.bound() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_descriptor_validation_status::not_checked),
    render::vulkan_backend::vulkan_descriptor_validation_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_descriptor_validation_status::valid),
    render::vulkan_backend::vulkan_descriptor_validation_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_descriptor_validation_status::missing_required_resource),
    render::vulkan_backend::vulkan_descriptor_validation_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_descriptor_validation_status::duplicate_binding),
    render::vulkan_backend::vulkan_descriptor_validation_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_descriptor_validation_status::invalid_layout),
    render::vulkan_backend::vulkan_descriptor_validation_status>);

static_assert(requires(render::vulkan_backend::vulkan_descriptor_validation_status status) {
    { render::vulkan_backend::descriptor_validation_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_descriptor_binding_validation_snapshot binding) {
    { binding.set } -> std::same_as<std::size_t&>;
    { binding.binding } -> std::same_as<std::size_t&>;
    { binding.kind } -> std::same_as<render::vulkan_backend::vulkan_resource_binding_kind&>;
    { binding.resource_id } -> std::same_as<std::string&>;
    { binding.required } -> std::same_as<bool&>;
    { binding.available } -> std::same_as<bool&>;
    { binding.bound } -> std::same_as<bool&>;
    { binding.binding_index_matches_order } -> std::same_as<bool&>;
    { binding.duplicate_binding } -> std::same_as<bool&>;
    { binding.status } -> std::same_as<render::vulkan_backend::vulkan_descriptor_validation_status&>;
    { binding.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_descriptor_set_validation_snapshot descriptor_set) {
    { descriptor_set.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { descriptor_set.command_index } -> std::same_as<std::size_t&>;
    { descriptor_set.node_id } -> std::same_as<render::render_node_id&>;
    { descriptor_set.set } -> std::same_as<std::size_t&>;
    { descriptor_set.expected_binding_count } -> std::same_as<std::size_t&>;
    { descriptor_set.actual_binding_count } -> std::same_as<std::size_t&>;
    { descriptor_set.checked } -> std::same_as<bool&>;
    { descriptor_set.descriptor_set_declared } -> std::same_as<bool&>;
    { descriptor_set.binding_count_matches } -> std::same_as<bool&>;
    { descriptor_set.missing_required_resource } -> std::same_as<bool&>;
    { descriptor_set.duplicate_binding } -> std::same_as<bool&>;
    { descriptor_set.invalid_layout } -> std::same_as<bool&>;
    { descriptor_set.failed_binding_kind } -> std::same_as<render::vulkan_backend::vulkan_resource_binding_kind&>;
    { descriptor_set.failed_binding } -> std::same_as<std::size_t&>;
    { descriptor_set.status } -> std::same_as<render::vulkan_backend::vulkan_descriptor_validation_status&>;
    { descriptor_set.bindings }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_descriptor_binding_validation_snapshot>&>;
    { descriptor_set.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_descriptor_validation_state validation) {
    { validation.checked } -> std::same_as<bool&>;
    { validation.missing_required_resource } -> std::same_as<bool&>;
    { validation.duplicate_binding } -> std::same_as<bool&>;
    { validation.invalid_layout } -> std::same_as<bool&>;
    { validation.failed_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { validation.failed_command_index } -> std::same_as<std::size_t&>;
    { validation.failed_binding_kind } -> std::same_as<render::vulkan_backend::vulkan_resource_binding_kind&>;
    { validation.failed_binding } -> std::same_as<std::size_t&>;
    { validation.planned_batch_count } -> std::same_as<std::size_t&>;
    { validation.descriptor_set_count } -> std::same_as<std::size_t&>;
    { validation.valid_descriptor_set_count } -> std::same_as<std::size_t&>;
    { validation.invalid_descriptor_set_count } -> std::same_as<std::size_t&>;
    { validation.requested_binding_count } -> std::same_as<std::size_t&>;
    { validation.valid_binding_count } -> std::same_as<std::size_t&>;
    { validation.invalid_binding_count } -> std::same_as<std::size_t&>;
    { validation.descriptor_sets }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_descriptor_set_validation_snapshot>&>;
    { validation.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_batch_resource_binding_snapshot snapshot) {
    { snapshot.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { snapshot.command_index } -> std::same_as<std::size_t&>;
    { snapshot.node_id } -> std::same_as<render::render_node_id&>;
    { snapshot.descriptor_set_count } -> std::same_as<std::size_t&>;
    { snapshot.binding_count } -> std::same_as<std::size_t&>;
    { snapshot.missing_resource } -> std::same_as<bool&>;
    { snapshot.missing_binding_kind } -> std::same_as<render::vulkan_backend::vulkan_resource_binding_kind&>;
    { snapshot.missing_resource_id } -> std::same_as<std::string&>;
    { snapshot.bindings } -> std::same_as<std::vector<render::vulkan_backend::vulkan_resource_binding_snapshot>&>;
    { snapshot.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_resource_binding_state resources) {
    { resources.checked } -> std::same_as<bool&>;
    { resources.missing_resource } -> std::same_as<bool&>;
    { resources.missing_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { resources.missing_command_index } -> std::same_as<std::size_t&>;
    { resources.missing_binding_kind } -> std::same_as<render::vulkan_backend::vulkan_resource_binding_kind&>;
    { resources.missing_resource_id } -> std::same_as<std::string&>;
    { resources.planned_batch_count } -> std::same_as<std::size_t&>;
    { resources.descriptor_set_count } -> std::same_as<std::size_t&>;
    { resources.binding_count } -> std::same_as<std::size_t&>;
    { resources.descriptor_validation }
        -> std::same_as<render::vulkan_backend::vulkan_backend_descriptor_validation_state&>;
    { resources.batch_snapshots }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_batch_resource_binding_snapshot>&>;
    { resources.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_resource_registry_entry entry) {
    { entry.kind } -> std::same_as<render::vulkan_backend::vulkan_resource_binding_kind&>;
    { entry.resource_id } -> std::same_as<std::string&>;
    { entry.first_command_index } -> std::same_as<std::size_t&>;
    { entry.last_command_index } -> std::same_as<std::size_t&>;
    { entry.use_count } -> std::same_as<std::size_t&>;
    { entry.reused() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_resource_registry_missing_resource missing) {
    { missing.kind } -> std::same_as<render::vulkan_backend::vulkan_resource_binding_kind&>;
    { missing.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { missing.command_index } -> std::same_as<std::size_t&>;
    { missing.set } -> std::same_as<std::size_t&>;
    { missing.binding } -> std::same_as<std::size_t&>;
    { missing.resource_id } -> std::same_as<std::string&>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_resource_registry_state registry) {
    { registry.checked } -> std::same_as<bool&>;
    { registry.planned_batch_count } -> std::same_as<std::size_t&>;
    { registry.descriptor_binding_count } -> std::same_as<std::size_t&>;
    { registry.registered_resource_count } -> std::same_as<std::size_t&>;
    { registry.descriptor_reuse_count } -> std::same_as<std::size_t&>;
    { registry.resource_reuse_count } -> std::same_as<std::size_t&>;
    { registry.missing_resource_count } -> std::same_as<std::size_t&>;
    { registry.resources } -> std::same_as<std::vector<render::vulkan_backend::vulkan_resource_registry_entry>&>;
    { registry.missing_resources }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_resource_registry_missing_resource>&>;
    { registry.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_recorded_draw_batch recorded_batch) {
    { recorded_batch.kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { recorded_batch.command_index } -> std::same_as<std::size_t&>;
    { recorded_batch.recording_index } -> std::same_as<std::size_t&>;
    { recorded_batch.bounds } -> std::same_as<render::render_rect&>;
    { recorded_batch.clipped_bounds } -> std::same_as<render::render_rect&>;
    { recorded_batch.scissor } -> std::same_as<render::vulkan_backend::vulkan_scissor_rect&>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_id id) {
    { id.value } -> std::same_as<std::string&>;
    { id.empty() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_stage::vertex),
    render::vulkan_backend::vulkan_shader_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_stage::fragment),
    render::vulkan_backend::vulkan_shader_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_stage::compute),
    render::vulkan_backend::vulkan_shader_stage>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_adapter_call_status::not_called),
    render::vulkan_backend::vulkan_shader_module_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_adapter_call_status::completed),
    render::vulkan_backend::vulkan_shader_module_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_adapter_call_status::failed_recoverable),
    render::vulkan_backend::vulkan_shader_module_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_adapter_call_status::failed_fatal),
    render::vulkan_backend::vulkan_shader_module_adapter_call_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_create_status::not_requested),
    render::vulkan_backend::vulkan_shader_module_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_create_status::created),
    render::vulkan_backend::vulkan_shader_module_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_create_status::missing_spirv_bytes),
    render::vulkan_backend::vulkan_shader_module_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_create_status::bad_spirv_magic),
    render::vulkan_backend::vulkan_shader_module_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_create_status::bad_spirv_version),
    render::vulkan_backend::vulkan_shader_module_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_create_status::missing_entry_point),
    render::vulkan_backend::vulkan_shader_module_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_create_status::unsupported_stage),
    render::vulkan_backend::vulkan_shader_module_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_create_status::create_failed_recoverable),
    render::vulkan_backend::vulkan_shader_module_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_create_status::create_failed_fatal),
    render::vulkan_backend::vulkan_shader_module_create_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_destroy_status::not_requested),
    render::vulkan_backend::vulkan_shader_module_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_destroy_status::destroyed),
    render::vulkan_backend::vulkan_shader_module_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_destroy_status::invalid_handle),
    render::vulkan_backend::vulkan_shader_module_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_destroy_status::destroy_failed_recoverable),
    render::vulkan_backend::vulkan_shader_module_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_shader_module_destroy_status::destroy_failed_fatal),
    render::vulkan_backend::vulkan_shader_module_destroy_status>);

static_assert(requires(render::vulkan_backend::vulkan_shader_stage stage) {
    { render::vulkan_backend::shader_stage_name(stage) } -> std::same_as<std::string_view>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_shader_module_adapter_call_status call_status,
    render::vulkan_backend::vulkan_shader_module_create_status create_status,
    render::vulkan_backend::vulkan_shader_module_destroy_status destroy_status) {
    { render::vulkan_backend::shader_module_adapter_call_status_name(call_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::shader_module_create_status_name(create_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::shader_module_destroy_status_name(destroy_status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_descriptor descriptor) {
    { descriptor.id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { descriptor.stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { descriptor.entry_point } -> std::same_as<std::string&>;
    { descriptor.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_create_request request) {
    { request.id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { request.stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { request.entry_point } -> std::same_as<std::string&>;
    { request.spirv_bytes } -> std::same_as<std::vector<std::uint8_t>&>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_create_call call) {
    { call.id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { call.stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { call.entry_point } -> std::same_as<std::string&>;
    { call.spirv_byte_count } -> std::same_as<std::size_t&>;
    { call.spirv_word_count } -> std::same_as<std::size_t&>;
    { call.spirv_version } -> std::same_as<std::uint32_t&>;
    { call.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_destroy_call call) {
    { call.handle } -> std::same_as<render::vulkan_backend::vulkan_shader_module_handle&>;
    { call.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_create_operation_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_shader_module_adapter_call_status&>;
    { result.handle } -> std::same_as<render::vulkan_backend::vulkan_shader_module_handle&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_destroy_operation_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_shader_module_adapter_call_status&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_function_table table) {
    { table.create } -> std::same_as<render::vulkan_backend::vulkan_shader_module_create_fn&>;
    { table.destroy } -> std::same_as<render::vulkan_backend::vulkan_shader_module_destroy_fn&>;
    { table.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_factory_adapter adapter) {
    { adapter.user_data } -> std::same_as<void*&>;
    { adapter.functions } -> std::same_as<render::vulkan_backend::vulkan_shader_module_function_table&>;
    { adapter.supported_stages } -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_stage>&>;
    { adapter.valid() } -> std::same_as<bool>;
    { adapter.supports_stage(render::vulkan_backend::vulkan_shader_stage::vertex) }
        -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_create_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_shader_module_create_status&>;
    { result.id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { result.stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { result.entry_point } -> std::same_as<std::string&>;
    { result.spirv_byte_count } -> std::same_as<std::size_t&>;
    { result.spirv_word_count } -> std::same_as<std::size_t&>;
    { result.spirv_magic } -> std::same_as<std::uint32_t&>;
    { result.spirv_version } -> std::same_as<std::uint32_t&>;
    { result.spirv_bytes_present } -> std::same_as<bool&>;
    { result.spirv_magic_valid } -> std::same_as<bool&>;
    { result.spirv_version_supported } -> std::same_as<bool&>;
    { result.entry_point_present } -> std::same_as<bool&>;
    { result.stage_supported } -> std::same_as<bool&>;
    { result.handle } -> std::same_as<render::vulkan_backend::vulkan_shader_module_handle&>;
    { result.create_call } -> std::same_as<render::vulkan_backend::vulkan_shader_module_create_call&>;
    { result.create_result }
        -> std::same_as<render::vulkan_backend::vulkan_shader_module_create_operation_result&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ready_for_pipeline() } -> std::same_as<bool>;
    { result.recoverable_create_failure() } -> std::same_as<bool>;
    { result.fatal_create_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_destroy_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_shader_module_destroy_status&>;
    { result.handle } -> std::same_as<render::vulkan_backend::vulkan_shader_module_handle&>;
    { result.destroy_call } -> std::same_as<render::vulkan_backend::vulkan_shader_module_destroy_call&>;
    { result.destroy_result }
        -> std::same_as<render::vulkan_backend::vulkan_shader_module_destroy_operation_result&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_shader_module_readiness_state state) {
    { state.checked } -> std::same_as<bool&>;
    { state.requested_module_count } -> std::same_as<std::size_t&>;
    { state.created_module_count } -> std::same_as<std::size_t&>;
    { state.failed_module_count } -> std::same_as<std::size_t&>;
    { state.modules }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_module_create_result>&>;
    { state.completed() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind::uniform_buffer),
    render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind::storage_buffer),
    render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind::sampled_image),
    render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind::sampler),
    render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind::combined_image_sampler),
    render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status::not_called),
    render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status::completed),
    render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status::failed_recoverable),
    render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status::failed_fatal),
    render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_create_status::not_requested),
    render::vulkan_backend::vulkan_pipeline_layout_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_create_status::created),
    render::vulkan_backend::vulkan_pipeline_layout_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_create_status::missing_shader_stage),
    render::vulkan_backend::vulkan_pipeline_layout_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_create_status::duplicate_binding),
    render::vulkan_backend::vulkan_pipeline_layout_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_create_status::invalid_binding),
    render::vulkan_backend::vulkan_pipeline_layout_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_create_status::push_constant_overlap),
    render::vulkan_backend::vulkan_pipeline_layout_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_create_status::push_constant_size_exceeded),
    render::vulkan_backend::vulkan_pipeline_layout_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_create_status::create_failed_recoverable),
    render::vulkan_backend::vulkan_pipeline_layout_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_create_status::create_failed_fatal),
    render::vulkan_backend::vulkan_pipeline_layout_create_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_destroy_status::not_requested),
    render::vulkan_backend::vulkan_pipeline_layout_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_destroy_status::destroyed),
    render::vulkan_backend::vulkan_pipeline_layout_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_destroy_status::invalid_handle),
    render::vulkan_backend::vulkan_pipeline_layout_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_destroy_status::destroy_failed_recoverable),
    render::vulkan_backend::vulkan_pipeline_layout_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_layout_destroy_status::destroy_failed_fatal),
    render::vulkan_backend::vulkan_pipeline_layout_destroy_status>);

static_assert(std::same_as<
    render::vulkan_backend::vulkan_pipeline_layout_readiness_result,
    render::vulkan_backend::vulkan_pipeline_layout_create_result>);

static_assert(requires(
    render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind binding_kind,
    render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status call_status,
    render::vulkan_backend::vulkan_pipeline_layout_create_status create_status,
    render::vulkan_backend::vulkan_pipeline_layout_destroy_status destroy_status) {
    { render::vulkan_backend::descriptor_set_layout_binding_kind_name(binding_kind) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::pipeline_layout_adapter_call_status_name(call_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::pipeline_layout_create_status_name(create_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::pipeline_layout_destroy_status_name(destroy_status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_descriptor_set_layout_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_layout_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_descriptor_set_layout_binding_request binding) {
    { binding.set } -> std::same_as<std::size_t&>;
    { binding.binding } -> std::same_as<std::size_t&>;
    { binding.kind }
        -> std::same_as<render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind&>;
    { binding.shader_stages }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_stage>&>;
    { binding.descriptor_count } -> std::same_as<std::size_t&>;
    { binding.required } -> std::same_as<bool&>;
    { binding.has_shader_stage(render::vulkan_backend::vulkan_shader_stage::vertex) }
        -> std::same_as<bool>;
    { binding.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_descriptor_set_layout_request request) {
    { request.set } -> std::same_as<std::size_t&>;
    { request.bindings }
        -> std::same_as<
            std::vector<render::vulkan_backend::vulkan_descriptor_set_layout_binding_request>&>;
    { request.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_push_constant_range range) {
    { range.offset } -> std::same_as<std::size_t&>;
    { range.size } -> std::same_as<std::size_t&>;
    { range.shader_stages }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_stage>&>;
    { range.end() } -> std::same_as<std::size_t>;
    { range.overlaps(range) } -> std::same_as<bool>;
    { range.valid(128) } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_layout_request request) {
    { request.descriptor_sets }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_descriptor_set_layout_request>&>;
    { request.push_constant_ranges }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_push_constant_range>&>;
    { request.required_shader_stages }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_stage>&>;
    { request.max_push_constant_bytes } -> std::same_as<std::size_t&>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_pipeline_layout_shader_stage_compatibility compatibility) {
    { compatibility.stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { compatibility.required } -> std::same_as<bool&>;
    { compatibility.shader_module_ready } -> std::same_as<bool&>;
    { compatibility.shader_id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { compatibility.completed() } -> std::same_as<bool>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_descriptor_set_layout_binding_diagnostics diagnostics) {
    { diagnostics.set } -> std::same_as<std::size_t&>;
    { diagnostics.binding } -> std::same_as<std::size_t&>;
    { diagnostics.kind }
        -> std::same_as<render::vulkan_backend::vulkan_descriptor_set_layout_binding_kind&>;
    { diagnostics.descriptor_count } -> std::same_as<std::size_t&>;
    { diagnostics.set_matches_layout } -> std::same_as<bool&>;
    { diagnostics.shader_stages_declared } -> std::same_as<bool&>;
    { diagnostics.descriptor_count_valid } -> std::same_as<bool&>;
    { diagnostics.duplicate_binding } -> std::same_as<bool&>;
    { diagnostics.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_push_constant_range_diagnostics diagnostics) {
    { diagnostics.offset } -> std::same_as<std::size_t&>;
    { diagnostics.size } -> std::same_as<std::size_t&>;
    { diagnostics.end } -> std::same_as<std::size_t&>;
    { diagnostics.shader_stages_declared } -> std::same_as<bool&>;
    { diagnostics.size_valid } -> std::same_as<bool&>;
    { diagnostics.overlaps_previous_range } -> std::same_as<bool&>;
    { diagnostics.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_descriptor_set_layout_create_call call) {
    { call.set } -> std::same_as<std::size_t&>;
    { call.bindings }
        -> std::same_as<
            std::vector<render::vulkan_backend::vulkan_descriptor_set_layout_binding_request>&>;
    { call.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_layout_create_call call) {
    { call.descriptor_set_layouts }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_descriptor_set_layout_handle>&>;
    { call.push_constant_ranges }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_push_constant_range>&>;
    { call.required_shader_stages }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_stage>&>;
    { call.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_descriptor_set_layout_destroy_call call) {
    { call.handle } -> std::same_as<render::vulkan_backend::vulkan_descriptor_set_layout_handle&>;
    { call.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_layout_destroy_call call) {
    { call.handle } -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_handle&>;
    { call.valid() } -> std::same_as<bool>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_descriptor_set_layout_create_operation_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status&>;
    { result.handle } -> std::same_as<render::vulkan_backend::vulkan_descriptor_set_layout_handle&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_layout_create_operation_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status&>;
    { result.handle } -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_handle&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_layout_destroy_operation_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_adapter_call_status&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_layout_function_table table) {
    { table.create_descriptor_set_layout }
        -> std::same_as<render::vulkan_backend::vulkan_descriptor_set_layout_create_fn&>;
    { table.destroy_descriptor_set_layout }
        -> std::same_as<render::vulkan_backend::vulkan_descriptor_set_layout_destroy_fn&>;
    { table.create_pipeline_layout }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_create_fn&>;
    { table.destroy_pipeline_layout }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_destroy_fn&>;
    { table.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_layout_factory_adapter adapter) {
    { adapter.user_data } -> std::same_as<void*&>;
    { adapter.functions }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_function_table&>;
    { adapter.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_layout_create_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_create_status&>;
    { result.shader_modules }
        -> std::same_as<render::vulkan_backend::vulkan_backend_shader_module_readiness_state&>;
    { result.request } -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_request&>;
    { result.shader_stage_compatibility }
        -> std::same_as<std::vector<
            render::vulkan_backend::vulkan_pipeline_layout_shader_stage_compatibility>&>;
    { result.binding_diagnostics }
        -> std::same_as<
            std::vector<render::vulkan_backend::vulkan_descriptor_set_layout_binding_diagnostics>&>;
    { result.push_constant_diagnostics }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_push_constant_range_diagnostics>&>;
    { result.descriptor_set_layouts }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_descriptor_set_layout_handle>&>;
    { result.pipeline_layout } -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_handle&>;
    { result.descriptor_set_layout_create_results }
        -> std::same_as<std::vector<
            render::vulkan_backend::vulkan_descriptor_set_layout_create_operation_result>&>;
    { result.pipeline_layout_create_result }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_create_operation_result&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ready_for_pipeline() } -> std::same_as<bool>;
    { result.recoverable_create_failure() } -> std::same_as<bool>;
    { result.fatal_create_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_layout_destroy_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_destroy_status&>;
    { result.pipeline_layout } -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_handle&>;
    { result.descriptor_set_layouts }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_descriptor_set_layout_handle>&>;
    { result.pipeline_layout_destroy_result }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_destroy_operation_result&>;
    { result.descriptor_set_layout_destroy_results }
        -> std::same_as<
            std::vector<render::vulkan_backend::vulkan_pipeline_layout_destroy_operation_result>&>;
    { result.descriptor_set_layout_destroyed_count } -> std::same_as<std::size_t&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_vertex_input_rate::vertex),
    render::vulkan_backend::vulkan_vertex_input_rate>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_vertex_input_rate::instance),
    render::vulkan_backend::vulkan_vertex_input_rate>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_primitive_topology::triangle_list),
    render::vulkan_backend::vulkan_primitive_topology>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_primitive_topology::triangle_strip),
    render::vulkan_backend::vulkan_primitive_topology>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_primitive_topology::line_list),
    render::vulkan_backend::vulkan_primitive_topology>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status::not_called),
    render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status::completed),
    render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status::failed_recoverable),
    render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status::failed_fatal),
    render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::not_requested),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::created),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::shader_module_unavailable),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::pipeline_layout_unavailable),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::incompatible_shader_stages),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_vertex_input_state),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_input_assembly_state),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_rasterization_state),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_blend_state),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_depth_state),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::create_failed_recoverable),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_create_status::create_failed_fatal),
    render::vulkan_backend::vulkan_graphics_pipeline_create_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_destroy_status::not_requested),
    render::vulkan_backend::vulkan_graphics_pipeline_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_destroy_status::destroyed),
    render::vulkan_backend::vulkan_graphics_pipeline_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_destroy_status::invalid_handle),
    render::vulkan_backend::vulkan_graphics_pipeline_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_destroy_status::destroy_failed_recoverable),
    render::vulkan_backend::vulkan_graphics_pipeline_destroy_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_graphics_pipeline_destroy_status::destroy_failed_fatal),
    render::vulkan_backend::vulkan_graphics_pipeline_destroy_status>);

static_assert(std::same_as<
    render::vulkan_backend::vulkan_graphics_pipeline_readiness_result,
    render::vulkan_backend::vulkan_graphics_pipeline_create_result>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::not_checked),
    render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::ready),
    render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::shader_module_unavailable),
    render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::pipeline_layout_unavailable),
    render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::graphics_pipeline_unavailable),
    render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status>);

static_assert(requires(
    render::vulkan_backend::vulkan_vertex_input_rate input_rate,
    render::vulkan_backend::vulkan_primitive_topology topology,
    render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status call_status,
    render::vulkan_backend::vulkan_graphics_pipeline_create_status create_status,
    render::vulkan_backend::vulkan_graphics_pipeline_destroy_status destroy_status) {
    { render::vulkan_backend::vertex_input_rate_name(input_rate) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::primitive_topology_name(topology) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::graphics_pipeline_adapter_call_status_name(call_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::graphics_pipeline_create_status_name(create_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::graphics_pipeline_destroy_status_name(destroy_status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status status) {
    { render::vulkan_backend::pipeline_readiness_summary_status_name(status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_graphics_pipeline_handle handle) {
    { handle.value } -> std::same_as<std::uintptr_t&>;
    { handle.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_vertex_input_binding binding) {
    { binding.binding } -> std::same_as<std::size_t&>;
    { binding.stride } -> std::same_as<std::size_t&>;
    { binding.input_rate } -> std::same_as<render::vulkan_backend::vulkan_vertex_input_rate&>;
    { binding.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_vertex_attribute attribute) {
    { attribute.location } -> std::same_as<std::size_t&>;
    { attribute.binding } -> std::same_as<std::size_t&>;
    { attribute.offset } -> std::same_as<std::size_t&>;
    { attribute.byte_size } -> std::same_as<std::size_t&>;
    { attribute.end_offset() } -> std::same_as<std::size_t>;
    { attribute.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_vertex_input_state state) {
    { state.bindings } -> std::same_as<std::vector<render::vulkan_backend::vulkan_vertex_input_binding>&>;
    { state.attributes } -> std::same_as<std::vector<render::vulkan_backend::vulkan_vertex_attribute>&>;
    { state.binding_for(0) }
        -> std::same_as<const render::vulkan_backend::vulkan_vertex_input_binding*>;
    { state.has_duplicate_attribute_location() } -> std::same_as<bool>;
    { state.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_input_assembly_state state) {
    { state.topology } -> std::same_as<render::vulkan_backend::vulkan_primitive_topology&>;
    { state.primitive_restart_enable } -> std::same_as<bool&>;
    { state.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_rasterization_state state) {
    { state.polygon_mode } -> std::same_as<render::vulkan_backend::vulkan_polygon_mode&>;
    { state.cull_mode } -> std::same_as<render::vulkan_backend::vulkan_cull_mode&>;
    { state.front_face } -> std::same_as<render::vulkan_backend::vulkan_front_face&>;
    { state.line_width } -> std::same_as<float&>;
    { state.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_color_blend_attachment_state attachment) {
    { attachment.blend_enable } -> std::same_as<bool&>;
    { attachment.src_color_factor } -> std::same_as<render::vulkan_backend::vulkan_blend_factor&>;
    { attachment.dst_color_factor } -> std::same_as<render::vulkan_backend::vulkan_blend_factor&>;
    { attachment.color_op } -> std::same_as<render::vulkan_backend::vulkan_blend_op&>;
    { attachment.color_write_mask } -> std::same_as<std::uint8_t&>;
    { attachment.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_color_blend_state state) {
    { state.attachments }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_color_blend_attachment_state>&>;
    { state.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_depth_stencil_state state) {
    { state.depth_test_enable } -> std::same_as<bool&>;
    { state.depth_write_enable } -> std::same_as<bool&>;
    { state.compare_op } -> std::same_as<render::vulkan_backend::vulkan_depth_compare_op&>;
    { state.depth_bounds_test_enable } -> std::same_as<bool&>;
    { state.min_depth_bounds } -> std::same_as<float&>;
    { state.max_depth_bounds } -> std::same_as<float&>;
    { state.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_graphics_pipeline_request request) {
    { request.pipeline_id } -> std::same_as<std::string&>;
    { request.required_shader_stages }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_stage>&>;
    { request.require_vertex_input } -> std::same_as<bool&>;
    { request.vertex_input } -> std::same_as<render::vulkan_backend::vulkan_vertex_input_state&>;
    { request.input_assembly } -> std::same_as<render::vulkan_backend::vulkan_input_assembly_state&>;
    { request.rasterization } -> std::same_as<render::vulkan_backend::vulkan_rasterization_state&>;
    { request.color_blend } -> std::same_as<render::vulkan_backend::vulkan_color_blend_state&>;
    { request.depth_stencil } -> std::same_as<render::vulkan_backend::vulkan_depth_stencil_state&>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_graphics_pipeline_shader_stage_diagnostics diagnostics) {
    { diagnostics.stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { diagnostics.required } -> std::same_as<bool&>;
    { diagnostics.shader_module_ready } -> std::same_as<bool&>;
    { diagnostics.pipeline_layout_compatible } -> std::same_as<bool&>;
    { diagnostics.shader_id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { diagnostics.completed() } -> std::same_as<bool>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_graphics_pipeline_fixed_function_diagnostics diagnostics) {
    { diagnostics.checked } -> std::same_as<bool&>;
    { diagnostics.vertex_input_valid } -> std::same_as<bool&>;
    { diagnostics.input_assembly_valid } -> std::same_as<bool&>;
    { diagnostics.rasterization_valid } -> std::same_as<bool&>;
    { diagnostics.blend_valid } -> std::same_as<bool&>;
    { diagnostics.depth_valid } -> std::same_as<bool&>;
    { diagnostics.vertex_binding_count } -> std::same_as<std::size_t&>;
    { diagnostics.vertex_attribute_count } -> std::same_as<std::size_t&>;
    { diagnostics.color_attachment_count } -> std::same_as<std::size_t&>;
    { diagnostics.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_graphics_pipeline_create_call call) {
    { call.pipeline_id } -> std::same_as<std::string&>;
    { call.shader_modules }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_module_id>&>;
    { call.pipeline_layout } -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_handle&>;
    { call.vertex_input } -> std::same_as<render::vulkan_backend::vulkan_vertex_input_state&>;
    { call.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_graphics_pipeline_destroy_call call) {
    { call.handle } -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_handle&>;
    { call.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_graphics_pipeline_create_operation_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status&>;
    { result.handle } -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_handle&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_graphics_pipeline_destroy_operation_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_graphics_pipeline_function_table table) {
    { table.create_graphics_pipeline }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_create_fn&>;
    { table.destroy_graphics_pipeline }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_destroy_fn&>;
    { table.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_graphics_pipeline_factory_adapter adapter) {
    { adapter.user_data } -> std::same_as<void*&>;
    { adapter.functions }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_function_table&>;
    { adapter.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_graphics_pipeline_create_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_create_status&>;
    { result.shader_modules }
        -> std::same_as<render::vulkan_backend::vulkan_backend_shader_module_readiness_state&>;
    { result.pipeline_layout }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_create_result&>;
    { result.request } -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_request&>;
    { result.shader_stage_diagnostics }
        -> std::same_as<std::vector<
            render::vulkan_backend::vulkan_graphics_pipeline_shader_stage_diagnostics>&>;
    { result.fixed_function }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_fixed_function_diagnostics&>;
    { result.pipeline } -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_handle&>;
    { result.create_call } -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_create_call&>;
    { result.create_result }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_create_operation_result&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.ready_for_draw() } -> std::same_as<bool>;
    { result.recoverable_create_failure() } -> std::same_as<bool>;
    { result.fatal_create_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_graphics_pipeline_destroy_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_destroy_status&>;
    { result.pipeline } -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_handle&>;
    { result.destroy_call } -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_destroy_call&>;
    { result.destroy_result }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_destroy_operation_result&>;
    { result.destroyed_pipeline_count } -> std::same_as<std::size_t&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.recoverable_failure() } -> std::same_as<bool>;
    { result.fatal_failure() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_pipeline_readiness_summary summary) {
    { summary.checked } -> std::same_as<bool&>;
    { summary.status }
        -> std::same_as<render::vulkan_backend::vulkan_backend_pipeline_readiness_summary_status&>;
    { summary.shader_modules_checked } -> std::same_as<bool&>;
    { summary.shader_modules_ready } -> std::same_as<bool&>;
    { summary.requested_shader_module_count } -> std::same_as<std::size_t&>;
    { summary.created_shader_module_count } -> std::same_as<std::size_t&>;
    { summary.failed_shader_module_count } -> std::same_as<std::size_t&>;
    { summary.pipeline_layout_checked } -> std::same_as<bool&>;
    { summary.pipeline_layout_ready } -> std::same_as<bool&>;
    { summary.pipeline_layout_status }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_create_status&>;
    { summary.graphics_pipeline_checked } -> std::same_as<bool&>;
    { summary.graphics_pipeline_ready } -> std::same_as<bool&>;
    { summary.graphics_pipeline_status }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_create_status&>;
    { summary.graphics_create_recoverable_failure } -> std::same_as<bool&>;
    { summary.graphics_create_fatal_failure } -> std::same_as<bool&>;
    { summary.pipeline_layout_destroy_checked } -> std::same_as<bool&>;
    { summary.pipeline_layout_destroyed } -> std::same_as<bool&>;
    { summary.descriptor_set_layout_destroyed_count } -> std::same_as<std::size_t&>;
    { summary.pipeline_layout_destroy_status }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_destroy_status&>;
    { summary.graphics_pipeline_destroy_checked } -> std::same_as<bool&>;
    { summary.graphics_pipeline_destroyed } -> std::same_as<bool&>;
    { summary.graphics_pipeline_destroyed_count } -> std::same_as<std::size_t&>;
    { summary.graphics_pipeline_destroy_status }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_destroy_status&>;
    { summary.diagnostic } -> std::same_as<std::string&>;
    { summary.completed() } -> std::same_as<bool>;
    { summary.failed() } -> std::same_as<bool>;
});

static_assert(requires(
    const render::vulkan_backend::vulkan_backend_shader_module_readiness_state& shader_modules,
    const render::vulkan_backend::vulkan_pipeline_layout_create_result& pipeline_layout,
    const render::vulkan_backend::vulkan_graphics_pipeline_create_result& graphics_pipeline,
    const render::vulkan_backend::vulkan_pipeline_layout_destroy_result& pipeline_layout_destroy,
    const render::vulkan_backend::vulkan_graphics_pipeline_destroy_result& graphics_pipeline_destroy) {
    { render::vulkan_backend::summarize_vulkan_pipeline_readiness(
        shader_modules,
        pipeline_layout,
        graphics_pipeline) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_pipeline_readiness_summary>;
    { render::vulkan_backend::summarize_vulkan_pipeline_readiness(
        shader_modules,
        pipeline_layout,
        graphics_pipeline,
        pipeline_layout_destroy,
        graphics_pipeline_destroy) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_pipeline_readiness_summary>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_descriptor descriptor) {
    { descriptor.kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { descriptor.vertex_shader } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { descriptor.fragment_shader } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { descriptor.matches(render::vulkan_backend::vulkan_batch_kind::quad) } -> std::same_as<bool>;
    { descriptor.complete() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_shader_registry_state registry) {
    { registry.registry_checked } -> std::same_as<bool&>;
    { registry.missing_shader } -> std::same_as<bool&>;
    { registry.missing_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { registry.missing_stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { registry.missing_shader_id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { registry.registered_shader_count } -> std::same_as<std::size_t&>;
    { registry.modules } -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_module_descriptor>&>;
    {
        registry.contains(
            render::vulkan_backend::vulkan_shader_module_id{.value = "shader"},
            render::vulkan_backend::vulkan_shader_stage::vertex)
    } -> std::same_as<bool>;
});

static_assert(requires(
    render::vulkan_backend::diagnostic_vulkan_shader_registry registry,
    render::vulkan_backend::vulkan_shader_module_id id) {
    {
        registry.require_shader(
            render::vulkan_backend::vulkan_batch_kind::quad,
            render::vulkan_backend::vulkan_shader_stage::vertex,
            id)
    } -> std::same_as<bool>;
    { registry.registry_state() }
        -> std::same_as<const render::vulkan_backend::vulkan_backend_shader_registry_state&>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_capability capability) {
    { capability.kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { capability.available } -> std::same_as<bool&>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_cache_entry entry) {
    { entry.kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { entry.available } -> std::same_as<bool&>;
    { entry.requested } -> std::same_as<bool&>;
    { entry.request_count } -> std::same_as<std::size_t&>;
    { entry.last_command_index } -> std::same_as<std::size_t&>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_lifecycle_stage::render_pass),
    render::vulkan_backend::vulkan_pipeline_lifecycle_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_lifecycle_stage::shader_stages),
    render::vulkan_backend::vulkan_pipeline_lifecycle_stage>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_lifecycle_stage::pipeline),
    render::vulkan_backend::vulkan_pipeline_lifecycle_stage>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked),
    render::vulkan_backend::vulkan_pipeline_lifecycle_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_lifecycle_status::ready),
    render::vulkan_backend::vulkan_pipeline_lifecycle_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable),
    render::vulkan_backend::vulkan_pipeline_lifecycle_status>);

static_assert(requires(
    render::vulkan_backend::vulkan_pipeline_lifecycle_stage stage,
    render::vulkan_backend::vulkan_pipeline_lifecycle_status status) {
    { render::vulkan_backend::pipeline_lifecycle_stage_name(stage) } -> std::same_as<std::string_view>;
    { render::vulkan_backend::pipeline_lifecycle_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_render_pass_descriptor render_pass) {
    { render_pass.color_attachment_count } -> std::same_as<std::size_t&>;
    { render_pass.has_depth_attachment } -> std::same_as<bool&>;
    { render_pass.surface_compatible } -> std::same_as<bool&>;
    { render_pass.valid() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_shader_stage_snapshot snapshot) {
    { snapshot.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { snapshot.command_index } -> std::same_as<std::size_t&>;
    { snapshot.vertex_shader } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { snapshot.fragment_shader } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { snapshot.vertex_stage_ready } -> std::same_as<bool&>;
    { snapshot.fragment_stage_ready } -> std::same_as<bool&>;
    { snapshot.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_compatibility_key key) {
    { key.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { key.color_attachment_count } -> std::same_as<std::size_t&>;
    { key.has_depth_attachment } -> std::same_as<bool&>;
    { key.surface_compatible } -> std::same_as<bool&>;
    { key.vertex_shader } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { key.fragment_shader } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { key.compatible() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_compatibility_key_summary summary) {
    { summary.checked } -> std::same_as<bool&>;
    { summary.requested_key_count } -> std::same_as<std::size_t&>;
    { summary.compatible_key_count } -> std::same_as<std::size_t&>;
    { summary.incompatible_key_count } -> std::same_as<std::size_t&>;
    { summary.unique_key_count } -> std::same_as<std::size_t&>;
    { summary.keys } -> std::same_as<std::vector<render::vulkan_backend::vulkan_pipeline_compatibility_key>&>;
    { summary.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_binding_readiness binding) {
    { binding.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { binding.command_index } -> std::same_as<std::size_t&>;
    { binding.stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { binding.shader_id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { binding.entry_point } -> std::same_as<std::string&>;
    { binding.descriptor_declared } -> std::same_as<bool&>;
    { binding.registry_checked } -> std::same_as<bool&>;
    { binding.module_registered } -> std::same_as<bool&>;
    { binding.ready } -> std::same_as<bool&>;
    { binding.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_shader_binding_readiness_state state) {
    { state.checked } -> std::same_as<bool&>;
    { state.requested_binding_count } -> std::same_as<std::size_t&>;
    { state.ready_binding_count } -> std::same_as<std::size_t&>;
    { state.missing_binding_count } -> std::same_as<std::size_t&>;
    { state.bindings }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_module_binding_readiness>&>;
    { state.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_pipeline_lifecycle_snapshot snapshot) {
    { snapshot.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { snapshot.command_index } -> std::same_as<std::size_t&>;
    { snapshot.render_pass_status }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_lifecycle_status&>;
    { snapshot.shader_stage_status }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_lifecycle_status&>;
    { snapshot.pipeline_status } -> std::same_as<render::vulkan_backend::vulkan_pipeline_lifecycle_status&>;
    { snapshot.failed_stage } -> std::same_as<render::vulkan_backend::vulkan_pipeline_lifecycle_stage&>;
    { snapshot.missing_shader_stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { snapshot.missing_shader_id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { snapshot.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_pipeline_lifecycle_state lifecycle) {
    { lifecycle.checked } -> std::same_as<bool&>;
    { lifecycle.render_pass } -> std::same_as<render::vulkan_backend::vulkan_render_pass_descriptor&>;
    { lifecycle.missing_render_pass } -> std::same_as<bool&>;
    { lifecycle.missing_shader_stage } -> std::same_as<bool&>;
    { lifecycle.missing_pipeline } -> std::same_as<bool&>;
    { lifecycle.failed_stage } -> std::same_as<render::vulkan_backend::vulkan_pipeline_lifecycle_stage&>;
    { lifecycle.missing_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { lifecycle.missing_command_index } -> std::same_as<std::size_t&>;
    { lifecycle.missing_shader_stage_kind } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { lifecycle.missing_shader_id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { lifecycle.requested_pipeline_count } -> std::same_as<std::size_t&>;
    { lifecycle.shader_stage_snapshots }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_pipeline_shader_stage_snapshot>&>;
    { lifecycle.pipeline_snapshots }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_pipeline_lifecycle_snapshot>&>;
    { lifecycle.render_pass_ready() } -> std::same_as<bool>;
    { lifecycle.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_pipeline_state pipeline) {
    { pipeline.cache_checked } -> std::same_as<bool&>;
    { pipeline.ready } -> std::same_as<bool&>;
    { pipeline.missing_pipeline } -> std::same_as<bool&>;
    { pipeline.missing_descriptor } -> std::same_as<bool&>;
    { pipeline.missing_shader } -> std::same_as<bool&>;
    { pipeline.missing_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { pipeline.missing_command_index } -> std::same_as<std::size_t&>;
    { pipeline.missing_shader_stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { pipeline.missing_shader_id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { pipeline.requested_pipeline_count } -> std::same_as<std::size_t&>;
    { pipeline.capabilities } -> std::same_as<std::vector<render::vulkan_backend::vulkan_pipeline_capability>&>;
    { pipeline.cache_entries } -> std::same_as<std::vector<render::vulkan_backend::vulkan_pipeline_cache_entry>&>;
    { pipeline.pipeline_descriptors } -> std::same_as<std::vector<render::vulkan_backend::vulkan_pipeline_descriptor>&>;
    { pipeline.shader_registry } -> std::same_as<render::vulkan_backend::vulkan_backend_shader_registry_state&>;
    { pipeline.shader_modules }
        -> std::same_as<render::vulkan_backend::vulkan_backend_shader_module_readiness_state&>;
    { pipeline.compatibility } -> std::same_as<render::vulkan_backend::vulkan_pipeline_compatibility_key_summary&>;
    { pipeline.shader_bindings }
        -> std::same_as<render::vulkan_backend::vulkan_backend_shader_binding_readiness_state&>;
    { pipeline.pipeline_layout }
        -> std::same_as<render::vulkan_backend::vulkan_pipeline_layout_create_result&>;
    { pipeline.graphics_pipeline }
        -> std::same_as<render::vulkan_backend::vulkan_graphics_pipeline_create_result&>;
    { pipeline.pipeline_readiness_summary }
        -> std::same_as<render::vulkan_backend::vulkan_backend_pipeline_readiness_summary&>;
    { pipeline.lifecycle } -> std::same_as<render::vulkan_backend::vulkan_backend_pipeline_lifecycle_state&>;
    { pipeline.supports(render::vulkan_backend::vulkan_batch_kind::quad) } -> std::same_as<bool>;
    { pipeline.descriptor_for(render::vulkan_backend::vulkan_batch_kind::quad) }
        -> std::same_as<const render::vulkan_backend::vulkan_pipeline_descriptor*>;
    { pipeline.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::diagnostic_vulkan_pipeline_cache_options options) {
    { options.default_available } -> std::same_as<bool&>;
    { options.use_default_shader_modules } -> std::same_as<bool&>;
    { options.use_default_pipeline_descriptors } -> std::same_as<bool&>;
    { options.render_pass } -> std::same_as<render::vulkan_backend::vulkan_render_pass_descriptor&>;
    { options.overrides } -> std::same_as<std::vector<render::vulkan_backend::vulkan_pipeline_capability>&>;
    { options.shader_modules } -> std::same_as<std::vector<render::vulkan_backend::vulkan_shader_module_descriptor>&>;
    { options.pipeline_descriptors } -> std::same_as<std::vector<render::vulkan_backend::vulkan_pipeline_descriptor>&>;
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

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_gate_status::not_checked),
    render::vulkan_backend::vulkan_command_recorder_gate_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_gate_status::allowed),
    render::vulkan_backend::vulkan_command_recorder_gate_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_gate_status::blocked_by_descriptor_validation),
    render::vulkan_backend::vulkan_command_recorder_gate_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_gate_status::blocked_by_resource_binding),
    render::vulkan_backend::vulkan_command_recorder_gate_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_gate_status::blocked_by_resource_registry),
    render::vulkan_backend::vulkan_command_recorder_gate_status>);

static_assert(requires(render::vulkan_backend::vulkan_command_recorder_gate_status status) {
    { render::vulkan_backend::command_recorder_gate_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_buffer_id id) {
    { id.value } -> std::same_as<std::size_t&>;
    { id.valid() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_recording_status::not_started),
    render::vulkan_backend::vulkan_command_buffer_recording_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_recording_status::recording),
    render::vulkan_backend::vulkan_command_buffer_recording_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_recording_status::recorded),
    render::vulkan_backend::vulkan_command_buffer_recording_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_recording_status::failed),
    render::vulkan_backend::vulkan_command_buffer_recording_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_submit_status::not_requested),
    render::vulkan_backend::vulkan_frame_submit_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_submit_status::pending),
    render::vulkan_backend::vulkan_frame_submit_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_submit_status::submitted),
    render::vulkan_backend::vulkan_frame_submit_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_submit_status::failed),
    render::vulkan_backend::vulkan_frame_submit_status>);

static_assert(requires(
    render::vulkan_backend::vulkan_command_buffer_recording_status recording_status,
    render::vulkan_backend::vulkan_frame_submit_status submit_status) {
    { render::vulkan_backend::command_buffer_recording_status_name(recording_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::frame_submit_status_name(submit_status) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_buffer_recording_diagnostics recording) {
    { recording.command_buffer } -> std::same_as<render::vulkan_backend::vulkan_command_buffer_id&>;
    { recording.status } -> std::same_as<render::vulkan_backend::vulkan_command_buffer_recording_status&>;
    { recording.begin_requested } -> std::same_as<bool&>;
    { recording.finish_requested } -> std::same_as<bool&>;
    { recording.planned_batch_count } -> std::same_as<std::size_t&>;
    { recording.recorded_batch_count } -> std::same_as<std::size_t&>;
    { recording.failure_stage } -> std::same_as<render::vulkan_backend::vulkan_command_recorder_failure_stage&>;
    { recording.failure_recording_index } -> std::same_as<std::size_t&>;
    { recording.completed() } -> std::same_as<bool>;
    { recording.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_frame_submit_diagnostics submit) {
    { submit.command_buffer } -> std::same_as<render::vulkan_backend::vulkan_command_buffer_id&>;
    { submit.frame } -> std::same_as<render::vulkan_backend::vulkan_frame_in_flight_id&>;
    { submit.status } -> std::same_as<render::vulkan_backend::vulkan_frame_submit_status&>;
    { submit.submit_requested } -> std::same_as<bool&>;
    { submit.submitted_batch_count } -> std::same_as<std::size_t&>;
    { submit.wait_image_available_status } -> std::same_as<render::vulkan_backend::vulkan_frame_sync_wait_status&>;
    { submit.signal_render_finished_status }
        -> std::same_as<render::vulkan_backend::vulkan_frame_sync_signal_status&>;
    { submit.signal_frame_fence_status } -> std::same_as<render::vulkan_backend::vulkan_frame_sync_signal_status&>;
    { submit.completed() } -> std::same_as<bool>;
    { submit.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_command_buffer_submit_state command_buffer_submit) {
    { command_buffer_submit.checked } -> std::same_as<bool&>;
    { command_buffer_submit.recording }
        -> std::same_as<render::vulkan_backend::vulkan_command_buffer_recording_diagnostics&>;
    { command_buffer_submit.submit } -> std::same_as<render::vulkan_backend::vulkan_frame_submit_diagnostics&>;
    { command_buffer_submit.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_recorder_gate_state gate) {
    { gate.checked } -> std::same_as<bool&>;
    { gate.recording_allowed } -> std::same_as<bool&>;
    { gate.resource_bindings_checked } -> std::same_as<bool&>;
    { gate.resource_bindings_completed } -> std::same_as<bool&>;
    { gate.descriptor_validation_checked } -> std::same_as<bool&>;
    { gate.descriptor_validation_completed } -> std::same_as<bool&>;
    { gate.resource_registry_checked } -> std::same_as<bool&>;
    { gate.resource_registry_completed } -> std::same_as<bool&>;
    { gate.missing_required_resource } -> std::same_as<bool&>;
    { gate.duplicate_binding } -> std::same_as<bool&>;
    { gate.invalid_layout } -> std::same_as<bool&>;
    { gate.status } -> std::same_as<render::vulkan_backend::vulkan_command_recorder_gate_status&>;
    { gate.descriptor_status } -> std::same_as<render::vulkan_backend::vulkan_descriptor_validation_status&>;
    { gate.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { gate.blocked_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { gate.blocked_command_index } -> std::same_as<std::size_t&>;
    { gate.blocked_binding_kind } -> std::same_as<render::vulkan_backend::vulkan_resource_binding_kind&>;
    { gate.blocked_binding } -> std::same_as<std::size_t&>;
    { gate.blocked_resource_id } -> std::same_as<std::string&>;
    { gate.planned_batch_count } -> std::same_as<std::size_t&>;
    { gate.descriptor_set_count } -> std::same_as<std::size_t&>;
    { gate.invalid_descriptor_set_count } -> std::same_as<std::size_t&>;
    { gate.missing_resource_count } -> std::same_as<std::size_t&>;
    { gate.completed() } -> std::same_as<bool>;
    { gate.blocked() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_command_recorder_state recorder) {
    { recorder.ready } -> std::same_as<bool&>;
    { recorder.frame_open } -> std::same_as<bool&>;
    { recorder.command_buffer_recorded } -> std::same_as<bool&>;
    { recorder.failure_stage } -> std::same_as<render::vulkan_backend::vulkan_command_recorder_failure_stage&>;
    { recorder.failure_recording_index } -> std::same_as<std::size_t&>;
    { recorder.planned_batch_count } -> std::same_as<std::size_t&>;
    { recorder.recorded_batch_count } -> std::same_as<std::size_t&>;
    { recorder.gate } -> std::same_as<render::vulkan_backend::vulkan_command_recorder_gate_state&>;
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

static_assert(requires(render::vulkan_backend::vulkan_backend_frame_fallback_summary fallback) {
    { fallback.checked } -> std::same_as<bool&>;
    { fallback.required } -> std::same_as<bool&>;
    { fallback.reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { fallback.reached_stage } -> std::same_as<render::vulkan_backend::vulkan_backend_frame_stage&>;
    { fallback.recoverable } -> std::same_as<bool&>;
    { fallback.fatal } -> std::same_as<bool&>;
    { fallback.reason_count } -> std::same_as<std::size_t&>;
    { fallback.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_queue_submit_adapter_summary summary) {
    { summary.checked } -> std::same_as<bool&>;
    { summary.status } -> std::same_as<render::vulkan_backend::vulkan_queue_submit_present_status&>;
    { summary.submit_called } -> std::same_as<bool&>;
    { summary.present_called } -> std::same_as<bool&>;
    { summary.submit_before_present } -> std::same_as<bool&>;
    { summary.recoverable_failure } -> std::same_as<bool&>;
    { summary.fatal_failure } -> std::same_as<bool&>;
    { summary.diagnostic } -> std::same_as<std::string&>;
    { summary.completed() } -> std::same_as<bool>;
    { summary.failed() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_category::rect),
    render::vulkan_backend::vulkan_command_packet_category>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_category::text),
    render::vulkan_backend::vulkan_command_packet_category>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_category::image),
    render::vulkan_backend::vulkan_command_packet_category>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_category::debug_bounds),
    render::vulkan_backend::vulkan_command_packet_category>);
static_assert(requires(render::vulkan_backend::vulkan_command_packet_category category) {
    { render::vulkan_backend::command_packet_category_name(category) } -> std::same_as<std::string_view>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_bridge_status::not_checked),
    render::vulkan_backend::vulkan_command_packet_bridge_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_bridge_status::ready),
    render::vulkan_backend::vulkan_command_packet_bridge_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_bridge_status::pipeline_unavailable),
    render::vulkan_backend::vulkan_command_packet_bridge_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_bridge_status::resource_binding_unavailable),
    render::vulkan_backend::vulkan_command_packet_bridge_status>);
static_assert(requires(render::vulkan_backend::vulkan_command_packet_bridge_status status) {
    { render::vulkan_backend::command_packet_bridge_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_packet packet) {
    { packet.category } -> std::same_as<render::vulkan_backend::vulkan_command_packet_category&>;
    { packet.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { packet.command_index } -> std::same_as<std::size_t&>;
    { packet.packet_index } -> std::same_as<std::size_t&>;
    { packet.node_id } -> std::same_as<render::render_node_id&>;
    { packet.bounds } -> std::same_as<render::render_rect&>;
    { packet.clipped_bounds } -> std::same_as<render::render_rect&>;
    { packet.scissor } -> std::same_as<render::vulkan_backend::vulkan_scissor_rect&>;
    { packet.vertices } -> std::same_as<std::array<render::vulkan_backend::vulkan_quad_vertex, 4>&>;
    { packet.descriptor_set_count } -> std::same_as<std::size_t&>;
    { packet.binding_count } -> std::same_as<std::size_t&>;
    { packet.bindings } -> std::same_as<std::vector<render::vulkan_backend::vulkan_resource_binding_snapshot>&>;
    { packet.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_packet_bridge_result bridge) {
    { bridge.checked } -> std::same_as<bool&>;
    { bridge.status } -> std::same_as<render::vulkan_backend::vulkan_command_packet_bridge_status&>;
    { bridge.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { bridge.pipeline_checked } -> std::same_as<bool&>;
    { bridge.pipeline_ready } -> std::same_as<bool&>;
    { bridge.resource_bindings_checked } -> std::same_as<bool&>;
    { bridge.resource_bindings_ready } -> std::same_as<bool&>;
    { bridge.resource_registry_checked } -> std::same_as<bool&>;
    { bridge.resource_registry_ready } -> std::same_as<bool&>;
    { bridge.blocked_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { bridge.blocked_command_index } -> std::same_as<std::size_t&>;
    { bridge.blocked_resource_id } -> std::same_as<std::string&>;
    { bridge.planned_batch_count } -> std::same_as<std::size_t&>;
    { bridge.packet_count } -> std::same_as<std::size_t&>;
    { bridge.rect_packet_count } -> std::same_as<std::size_t&>;
    { bridge.text_packet_count } -> std::same_as<std::size_t&>;
    { bridge.image_packet_count } -> std::same_as<std::size_t&>;
    { bridge.debug_bounds_packet_count } -> std::same_as<std::size_t&>;
    { bridge.clipped_packet_count } -> std::same_as<std::size_t&>;
    { bridge.discarded_draw_call_count } -> std::same_as<std::size_t&>;
    { bridge.packets } -> std::same_as<std::vector<render::vulkan_backend::vulkan_command_packet>&>;
    { bridge.completed() } -> std::same_as<bool>;
    { bridge.blocked() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_execution_status::not_checked),
    render::vulkan_backend::vulkan_command_packet_execution_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_execution_status::completed),
    render::vulkan_backend::vulkan_command_packet_execution_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_execution_status::packet_bridge_unavailable),
    render::vulkan_backend::vulkan_command_packet_execution_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_execution_status::begin_failed),
    render::vulkan_backend::vulkan_command_packet_execution_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_execution_status::packet_failed),
    render::vulkan_backend::vulkan_command_packet_execution_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_execution_status::end_failed),
    render::vulkan_backend::vulkan_command_packet_execution_status>);
static_assert(requires(render::vulkan_backend::vulkan_command_packet_execution_status status) {
    { render::vulkan_backend::command_packet_execution_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_execution_event::begin),
    render::vulkan_backend::vulkan_command_packet_execution_event>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_execution_event::packet),
    render::vulkan_backend::vulkan_command_packet_execution_event>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_packet_execution_event::end),
    render::vulkan_backend::vulkan_command_packet_execution_event>);
static_assert(requires(render::vulkan_backend::vulkan_command_packet_execution_event event) {
    { render::vulkan_backend::command_packet_execution_event_name(event) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_packet_execution_snapshot snapshot) {
    { snapshot.event } -> std::same_as<render::vulkan_backend::vulkan_command_packet_execution_event&>;
    { snapshot.category } -> std::same_as<render::vulkan_backend::vulkan_command_packet_category&>;
    { snapshot.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { snapshot.packet_index } -> std::same_as<std::size_t&>;
    { snapshot.command_index } -> std::same_as<std::size_t&>;
    { snapshot.attempted } -> std::same_as<bool&>;
    { snapshot.completed } -> std::same_as<bool&>;
    { snapshot.failed } -> std::same_as<bool&>;
    { snapshot.successful() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_packet_execution_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_command_packet_execution_status&>;
    { result.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { result.packet_bridge_checked } -> std::same_as<bool&>;
    { result.packet_bridge_ready } -> std::same_as<bool&>;
    { result.begin_attempted } -> std::same_as<bool&>;
    { result.begin_completed } -> std::same_as<bool&>;
    { result.end_attempted } -> std::same_as<bool&>;
    { result.end_completed } -> std::same_as<bool&>;
    { result.has_failed_packet } -> std::same_as<bool&>;
    { result.first_failed_category } -> std::same_as<render::vulkan_backend::vulkan_command_packet_category&>;
    { result.first_failed_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { result.first_failed_packet_index } -> std::same_as<std::size_t&>;
    { result.first_failed_command_index } -> std::same_as<std::size_t&>;
    { result.planned_packet_count } -> std::same_as<std::size_t&>;
    { result.attempted_packet_count } -> std::same_as<std::size_t&>;
    { result.executed_packet_count } -> std::same_as<std::size_t&>;
    { result.rect_packet_count } -> std::same_as<std::size_t&>;
    { result.text_packet_count } -> std::same_as<std::size_t&>;
    { result.image_packet_count } -> std::same_as<std::size_t&>;
    { result.debug_bounds_packet_count } -> std::same_as<std::size_t&>;
    { result.events }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_command_packet_execution_snapshot>&>;
    { result.completed() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_command_packet_executor_options options) {
    { options.fail_begin } -> std::same_as<bool&>;
    { options.fail_end } -> std::same_as<bool&>;
    { options.fail_packet } -> std::same_as<bool&>;
    { options.fail_packet_index } -> std::same_as<std::size_t&>;
});

template <typename T>
concept VulkanCommandPacketExecutorInterface = requires(
    T& executor,
    const render::vulkan_backend::vulkan_command_packet_bridge_result& bridge) {
    { executor.execute_packets(bridge) }
        -> std::same_as<render::vulkan_backend::vulkan_command_packet_execution_result>;
    { executor.execution_result() }
        -> std::same_as<const render::vulkan_backend::vulkan_command_packet_execution_result&>;
};

static_assert(VulkanCommandPacketExecutorInterface<
              render::vulkan_backend::vulkan_command_packet_executor_interface>);
static_assert(VulkanCommandPacketExecutorInterface<
              render::vulkan_backend::fake_vulkan_command_packet_executor>);
static_assert(std::default_initializable<render::vulkan_backend::fake_vulkan_command_packet_executor>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_command_packet_executor,
    render::vulkan_backend::fake_vulkan_command_packet_executor_options>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_operation_plan_status::not_checked),
    render::vulkan_backend::vulkan_command_recorder_operation_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_operation_plan_status::ready),
    render::vulkan_backend::vulkan_command_recorder_operation_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_operation_plan_status::packet_bridge_unavailable),
    render::vulkan_backend::vulkan_command_recorder_operation_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_operation_plan_status::packet_execution_unavailable),
    render::vulkan_backend::vulkan_command_recorder_operation_plan_status>);
static_assert(requires(render::vulkan_backend::vulkan_command_recorder_operation_plan_status status) {
    { render::vulkan_backend::command_recorder_operation_plan_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_operation_kind::draw_rect),
    render::vulkan_backend::vulkan_command_recorder_operation_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_operation_kind::draw_text),
    render::vulkan_backend::vulkan_command_recorder_operation_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_operation_kind::draw_image),
    render::vulkan_backend::vulkan_command_recorder_operation_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_recorder_operation_kind::draw_debug_bounds),
    render::vulkan_backend::vulkan_command_recorder_operation_kind>);
static_assert(requires(render::vulkan_backend::vulkan_command_recorder_operation_kind kind) {
    { render::vulkan_backend::command_recorder_operation_kind_name(kind) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_recorder_operation_summary operation) {
    { operation.kind } -> std::same_as<render::vulkan_backend::vulkan_command_recorder_operation_kind&>;
    { operation.category } -> std::same_as<render::vulkan_backend::vulkan_command_packet_category&>;
    { operation.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { operation.operation_index } -> std::same_as<std::size_t&>;
    { operation.packet_index } -> std::same_as<std::size_t&>;
    { operation.command_index } -> std::same_as<std::size_t&>;
    { operation.node_id } -> std::same_as<render::render_node_id&>;
    { operation.bounds } -> std::same_as<render::render_rect&>;
    { operation.clipped_bounds } -> std::same_as<render::render_rect&>;
    { operation.scissor } -> std::same_as<render::vulkan_backend::vulkan_scissor_rect&>;
    { operation.vertex_count } -> std::same_as<std::size_t&>;
    { operation.descriptor_set_count } -> std::same_as<std::size_t&>;
    { operation.binding_count } -> std::same_as<std::size_t&>;
    { operation.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_recorder_operation_plan plan) {
    { plan.checked } -> std::same_as<bool&>;
    { plan.status } -> std::same_as<render::vulkan_backend::vulkan_command_recorder_operation_plan_status&>;
    { plan.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { plan.packet_bridge_checked } -> std::same_as<bool&>;
    { plan.packet_bridge_ready } -> std::same_as<bool&>;
    { plan.packet_execution_checked } -> std::same_as<bool&>;
    { plan.packet_execution_ready } -> std::same_as<bool&>;
    { plan.blocked_category } -> std::same_as<render::vulkan_backend::vulkan_command_packet_category&>;
    { plan.blocked_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { plan.blocked_packet_index } -> std::same_as<std::size_t&>;
    { plan.blocked_command_index } -> std::same_as<std::size_t&>;
    { plan.planned_packet_count } -> std::same_as<std::size_t&>;
    { plan.operation_count } -> std::same_as<std::size_t&>;
    { plan.rect_operation_count } -> std::same_as<std::size_t&>;
    { plan.text_operation_count } -> std::same_as<std::size_t&>;
    { plan.image_operation_count } -> std::same_as<std::size_t&>;
    { plan.debug_bounds_operation_count } -> std::same_as<std::size_t&>;
    { plan.clipped_operation_count } -> std::same_as<std::size_t&>;
    { plan.discarded_draw_call_count } -> std::same_as<std::size_t&>;
    { plan.operations }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_command_recorder_operation_summary>&>;
    { plan.completed() } -> std::same_as<bool>;
    { plan.blocked() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_result_status::not_checked),
    render::vulkan_backend::vulkan_command_buffer_record_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_result_status::recorded),
    render::vulkan_backend::vulkan_command_buffer_record_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_result_status::operation_plan_unavailable),
    render::vulkan_backend::vulkan_command_buffer_record_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_result_status::native_entrypoint_unavailable),
    render::vulkan_backend::vulkan_command_buffer_record_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_result_status::command_buffer_unavailable),
    render::vulkan_backend::vulkan_command_buffer_record_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_result_status::begin_failed),
    render::vulkan_backend::vulkan_command_buffer_record_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_result_status::operation_failed),
    render::vulkan_backend::vulkan_command_buffer_record_result_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_result_status::end_failed),
    render::vulkan_backend::vulkan_command_buffer_record_result_status>);
static_assert(requires(render::vulkan_backend::vulkan_command_buffer_record_result_status status) {
    { render::vulkan_backend::command_buffer_record_result_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_event_kind::begin),
    render::vulkan_backend::vulkan_command_buffer_record_event_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_event_kind::operation),
    render::vulkan_backend::vulkan_command_buffer_record_event_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_command_buffer_record_event_kind::end),
    render::vulkan_backend::vulkan_command_buffer_record_event_kind>);
static_assert(requires(render::vulkan_backend::vulkan_command_buffer_record_event_kind kind) {
    { render::vulkan_backend::command_buffer_record_event_kind_name(kind) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_buffer_record_event event) {
    { event.event } -> std::same_as<render::vulkan_backend::vulkan_command_buffer_record_event_kind&>;
    { event.command_buffer } -> std::same_as<render::vulkan_backend::vulkan_command_buffer_id&>;
    { event.operation_kind } -> std::same_as<render::vulkan_backend::vulkan_command_recorder_operation_kind&>;
    { event.category } -> std::same_as<render::vulkan_backend::vulkan_command_packet_category&>;
    { event.batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { event.operation_index } -> std::same_as<std::size_t&>;
    { event.packet_index } -> std::same_as<std::size_t&>;
    { event.command_index } -> std::same_as<std::size_t&>;
    { event.attempted } -> std::same_as<bool&>;
    { event.completed } -> std::same_as<bool&>;
    { event.failed } -> std::same_as<bool&>;
    { event.successful() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_command_buffer_record_result result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status } -> std::same_as<render::vulkan_backend::vulkan_command_buffer_record_result_status&>;
    { result.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { result.command_buffer } -> std::same_as<render::vulkan_backend::vulkan_command_buffer_id&>;
    { result.operation_plan_checked } -> std::same_as<bool&>;
    { result.operation_plan_ready } -> std::same_as<bool&>;
    { result.native_function_table_checked } -> std::same_as<bool&>;
    { result.native_command_buffer_recording_ready } -> std::same_as<bool&>;
    { result.native_function_table_status }
        -> std::same_as<render::vulkan_backend::vulkan_native_function_table_status&>;
    { result.missing_native_symbol_name } -> std::same_as<std::string&>;
    { result.begin_attempted } -> std::same_as<bool&>;
    { result.begin_completed } -> std::same_as<bool&>;
    { result.end_attempted } -> std::same_as<bool&>;
    { result.end_completed } -> std::same_as<bool&>;
    { result.has_failed_operation } -> std::same_as<bool&>;
    { result.first_failed_operation_kind }
        -> std::same_as<render::vulkan_backend::vulkan_command_recorder_operation_kind&>;
    { result.first_failed_category } -> std::same_as<render::vulkan_backend::vulkan_command_packet_category&>;
    { result.first_failed_batch_kind } -> std::same_as<render::vulkan_backend::vulkan_batch_kind&>;
    { result.first_failed_operation_index } -> std::same_as<std::size_t&>;
    { result.first_failed_packet_index } -> std::same_as<std::size_t&>;
    { result.first_failed_command_index } -> std::same_as<std::size_t&>;
    { result.planned_operation_count } -> std::same_as<std::size_t&>;
    { result.attempted_operation_count } -> std::same_as<std::size_t&>;
    { result.recorded_operation_count } -> std::same_as<std::size_t&>;
    { result.rect_operation_count } -> std::same_as<std::size_t&>;
    { result.text_operation_count } -> std::same_as<std::size_t&>;
    { result.image_operation_count } -> std::same_as<std::size_t&>;
    { result.debug_bounds_operation_count } -> std::same_as<std::size_t&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.events }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_command_buffer_record_event>&>;
    { result.completed() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::fake_vulkan_command_buffer_operation_recorder_options options) {
    { options.fail_begin } -> std::same_as<bool&>;
    { options.fail_end } -> std::same_as<bool&>;
    { options.fail_operation } -> std::same_as<bool&>;
    { options.fail_operation_index } -> std::same_as<std::size_t&>;
});

template <typename T>
concept VulkanCommandBufferOperationRecorderInterface = requires(
    T& recorder,
    render::vulkan_backend::vulkan_command_buffer_id command_buffer,
    const render::vulkan_backend::vulkan_command_recorder_operation_plan& operation_plan) {
    { recorder.record_operations(command_buffer, operation_plan) }
        -> std::same_as<render::vulkan_backend::vulkan_command_buffer_record_result>;
    { recorder.record_result() }
        -> std::same_as<const render::vulkan_backend::vulkan_command_buffer_record_result&>;
};

static_assert(VulkanCommandBufferOperationRecorderInterface<
              render::vulkan_backend::vulkan_command_buffer_operation_recorder_interface>);
static_assert(VulkanCommandBufferOperationRecorderInterface<
              render::vulkan_backend::fake_vulkan_command_buffer_operation_recorder>);
static_assert(std::default_initializable<
              render::vulkan_backend::fake_vulkan_command_buffer_operation_recorder>);
static_assert(std::constructible_from<
    render::vulkan_backend::fake_vulkan_command_buffer_operation_recorder,
    render::vulkan_backend::fake_vulkan_command_buffer_operation_recorder_options>);
static_assert(requires(
    render::vulkan_backend::vulkan_command_buffer_operation_recorder_interface& recorder,
    render::vulkan_backend::vulkan_command_buffer_id command_buffer,
    render::vulkan_backend::vulkan_command_recorder_operation_plan operation_plan,
    render::vulkan_backend::vulkan_native_function_table_diagnostics native_functions) {
    { render::vulkan_backend::record_vulkan_command_buffer_operations(
        recorder,
        command_buffer,
        operation_plan,
        native_functions) }
        -> std::same_as<render::vulkan_backend::vulkan_command_buffer_record_result>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::not_checked),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::ready),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::command_buffer_recording_unavailable),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::native_queue_submit_unavailable),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::command_submit_unavailable),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::command_buffer_unavailable),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::sync_primitives_unavailable),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::submit_queue_unavailable),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::present_target_unavailable),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::submit_failed_recoverable),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_plan_status::submit_failed_fatal),
    render::vulkan_backend::vulkan_submit_batch_plan_status>);
static_assert(requires(render::vulkan_backend::vulkan_submit_batch_plan_status status) {
    { render::vulkan_backend::submit_batch_plan_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_sync_intent_kind::wait_image_available),
    render::vulkan_backend::vulkan_submit_batch_sync_intent_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_sync_intent_kind::signal_render_finished),
    render::vulkan_backend::vulkan_submit_batch_sync_intent_kind>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_submit_batch_sync_intent_kind::signal_frame_fence),
    render::vulkan_backend::vulkan_submit_batch_sync_intent_kind>);
static_assert(requires(render::vulkan_backend::vulkan_submit_batch_sync_intent_kind kind) {
    { render::vulkan_backend::submit_batch_sync_intent_kind_name(kind) } -> std::same_as<std::string_view>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_present_completion_plan_status::not_checked),
    render::vulkan_backend::vulkan_present_completion_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_present_completion_plan_status::ready),
    render::vulkan_backend::vulkan_present_completion_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_present_completion_plan_status::submit_batch_unavailable),
    render::vulkan_backend::vulkan_present_completion_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_present_completion_plan_status::native_queue_present_unavailable),
    render::vulkan_backend::vulkan_present_completion_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_present_completion_plan_status::present_request_unavailable),
    render::vulkan_backend::vulkan_present_completion_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_present_completion_plan_status::present_adapter_unavailable),
    render::vulkan_backend::vulkan_present_completion_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_present_completion_plan_status::submit_failed_recoverable),
    render::vulkan_backend::vulkan_present_completion_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_present_completion_plan_status::submit_failed_fatal),
    render::vulkan_backend::vulkan_present_completion_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_present_completion_plan_status::present_failed_recoverable),
    render::vulkan_backend::vulkan_present_completion_plan_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_present_completion_plan_status::present_failed_fatal),
    render::vulkan_backend::vulkan_present_completion_plan_status>);

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_completion_status::not_checked),
    render::vulkan_backend::vulkan_frame_completion_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_completion_status::ready_for_present),
    render::vulkan_backend::vulkan_frame_completion_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_completion_status::completed),
    render::vulkan_backend::vulkan_frame_completion_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_completion_status::submit_unavailable),
    render::vulkan_backend::vulkan_frame_completion_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_completion_status::present_unavailable),
    render::vulkan_backend::vulkan_frame_completion_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_completion_status::submit_failed_recoverable),
    render::vulkan_backend::vulkan_frame_completion_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_completion_status::submit_failed_fatal),
    render::vulkan_backend::vulkan_frame_completion_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_completion_status::present_failed_recoverable),
    render::vulkan_backend::vulkan_frame_completion_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_frame_completion_status::present_failed_fatal),
    render::vulkan_backend::vulkan_frame_completion_status>);

static_assert(requires(
    render::vulkan_backend::vulkan_present_completion_plan_status present_status,
    render::vulkan_backend::vulkan_frame_completion_status frame_status) {
    { render::vulkan_backend::present_completion_plan_status_name(present_status) }
        -> std::same_as<std::string_view>;
    { render::vulkan_backend::frame_completion_status_name(frame_status) }
        -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_submit_batch_sync_intent intent) {
    { intent.kind } -> std::same_as<render::vulkan_backend::vulkan_submit_batch_sync_intent_kind&>;
    { intent.handle } -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_handle&>;
    { intent.required } -> std::same_as<bool&>;
    { intent.available } -> std::same_as<bool&>;
    { intent.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_submit_batch_present_intent intent) {
    { intent.requested } -> std::same_as<bool&>;
    { intent.target_available } -> std::same_as<bool&>;
    { intent.image_id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
    { intent.wait_render_finished_semaphore }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_handle&>;
    { intent.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_submit_batch_record batch) {
    { batch.batch_index } -> std::same_as<std::size_t&>;
    { batch.recorded_command_buffer } -> std::same_as<render::vulkan_backend::vulkan_command_buffer_id&>;
    { batch.submit_command_buffer }
        -> std::same_as<render::vulkan_backend::vulkan_command_recording_command_buffer_handle&>;
    { batch.submit_queue } -> std::same_as<render::vulkan_backend::vulkan_queue_handle&>;
    { batch.recorded_operation_count } -> std::same_as<std::size_t&>;
    { batch.wait_intent_count } -> std::same_as<std::size_t&>;
    { batch.signal_intent_count } -> std::same_as<std::size_t&>;
    { batch.present_intent_count } -> std::same_as<std::size_t&>;
    { batch.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_submit_batch_plan_result plan) {
    { plan.checked } -> std::same_as<bool&>;
    { plan.status } -> std::same_as<render::vulkan_backend::vulkan_submit_batch_plan_status&>;
    { plan.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { plan.command_buffer_recording_checked } -> std::same_as<bool&>;
    { plan.command_buffer_recording_ready } -> std::same_as<bool&>;
    { plan.native_function_table_checked } -> std::same_as<bool&>;
    { plan.native_queue_submit_ready } -> std::same_as<bool&>;
    { plan.native_function_table_status }
        -> std::same_as<render::vulkan_backend::vulkan_native_function_table_status&>;
    { plan.missing_native_symbol_name } -> std::same_as<std::string&>;
    { plan.command_submit_readiness_checked } -> std::same_as<bool&>;
    { plan.command_submit_readiness_ready } -> std::same_as<bool&>;
    { plan.command_buffer_available } -> std::same_as<bool&>;
    { plan.sync_primitives_available } -> std::same_as<bool&>;
    { plan.submit_queue_available } -> std::same_as<bool&>;
    { plan.present_target_available } -> std::same_as<bool&>;
    { plan.submit_ready } -> std::same_as<bool&>;
    { plan.present_ready } -> std::same_as<bool&>;
    { plan.recorded_operation_count } -> std::same_as<std::size_t&>;
    { plan.submit_batch_count } -> std::same_as<std::size_t&>;
    { plan.wait_intent_count } -> std::same_as<std::size_t&>;
    { plan.signal_intent_count } -> std::same_as<std::size_t&>;
    { plan.present_intent_count } -> std::same_as<std::size_t&>;
    { plan.recorded_command_buffer } -> std::same_as<render::vulkan_backend::vulkan_command_buffer_id&>;
    { plan.submit_command_buffer }
        -> std::same_as<render::vulkan_backend::vulkan_command_recording_command_buffer_handle&>;
    { plan.submit_queue } -> std::same_as<render::vulkan_backend::vulkan_queue_handle&>;
    { plan.sync_primitives } -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_primitives&>;
    { plan.image_id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
    { plan.submit_batches }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_submit_batch_record>&>;
    { plan.wait_intents }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_submit_batch_sync_intent>&>;
    { plan.signal_intents }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_submit_batch_sync_intent>&>;
    { plan.present_intents }
        -> std::same_as<std::vector<render::vulkan_backend::vulkan_submit_batch_present_intent>&>;
    { plan.diagnostic } -> std::same_as<std::string&>;
    { plan.completed() } -> std::same_as<bool>;
    { plan.blocked() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_present_request_summary request) {
    { request.requested } -> std::same_as<bool&>;
    { request.source_adapter_checked } -> std::same_as<bool&>;
    { request.present_queue } -> std::same_as<render::vulkan_backend::vulkan_queue_handle&>;
    { request.swapchain } -> std::same_as<render::vulkan_backend::vulkan_swapchain_handle&>;
    { request.image_id } -> std::same_as<render::vulkan_backend::vulkan_swapchain_image_id&>;
    { request.wait_render_finished_semaphore }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_sync_handle&>;
    { request.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_present_result_summary result) {
    { result.checked } -> std::same_as<bool&>;
    { result.status }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_adapter_call_status&>;
    { result.present_called } -> std::same_as<bool&>;
    { result.submit_before_present } -> std::same_as<bool&>;
    { result.recoverable_failure } -> std::same_as<bool&>;
    { result.fatal_failure } -> std::same_as<bool&>;
    { result.diagnostic } -> std::same_as<std::string&>;
    { result.completed() } -> std::same_as<bool>;
    { result.failed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_present_completion_plan_result plan) {
    { plan.checked } -> std::same_as<bool&>;
    { plan.status }
        -> std::same_as<render::vulkan_backend::vulkan_present_completion_plan_status&>;
    { plan.frame_status }
        -> std::same_as<render::vulkan_backend::vulkan_frame_completion_status&>;
    { plan.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { plan.submit_batch_checked } -> std::same_as<bool&>;
    { plan.submit_batch_ready } -> std::same_as<bool&>;
    { plan.native_function_table_checked } -> std::same_as<bool&>;
    { plan.native_queue_present_ready } -> std::same_as<bool&>;
    { plan.native_function_table_status }
        -> std::same_as<render::vulkan_backend::vulkan_native_function_table_status&>;
    { plan.missing_native_symbol_name } -> std::same_as<std::string&>;
    { plan.queue_present_adapter_checked } -> std::same_as<bool&>;
    { plan.queue_present_adapter_ready } -> std::same_as<bool&>;
    { plan.present_request_ready } -> std::same_as<bool&>;
    { plan.present_result_ready } -> std::same_as<bool&>;
    { plan.frame_completion_ready } -> std::same_as<bool&>;
    { plan.recoverable_failure } -> std::same_as<bool&>;
    { plan.fatal_failure } -> std::same_as<bool&>;
    { plan.submit_batch_count } -> std::same_as<std::size_t&>;
    { plan.present_intent_count } -> std::same_as<std::size_t&>;
    { plan.request } -> std::same_as<render::vulkan_backend::vulkan_present_request_summary&>;
    { plan.result } -> std::same_as<render::vulkan_backend::vulkan_present_result_summary&>;
    { plan.diagnostic } -> std::same_as<std::string&>;
    { plan.completed() } -> std::same_as<bool>;
    { plan.blocked() } -> std::same_as<bool>;
});

static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::not_checked),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::ready),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::instance_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::device_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::swapchain_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::render_pass_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::surface_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::viewport_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::pipeline_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::resource_binding_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::command_recording_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::frame_lifecycle_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::submit_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status::present_unavailable),
    render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status>);
static_assert(requires(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status status) {
    { render::vulkan_backend::frame_pipeline_handoff_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_frame_pipeline_handoff handoff) {
    { handoff.checked } -> std::same_as<bool&>;
    { handoff.status } -> std::same_as<render::vulkan_backend::vulkan_backend_frame_pipeline_handoff_status&>;
    { handoff.fallback_reason } -> std::same_as<render::vulkan_backend::vulkan_backend_fallback_reason&>;
    { handoff.reached_stage } -> std::same_as<render::vulkan_backend::vulkan_backend_frame_stage&>;
    { handoff.cpu_fallback_available } -> std::same_as<bool&>;
    { handoff.loader_checked } -> std::same_as<bool&>;
    { handoff.loader_ready } -> std::same_as<bool&>;
    { handoff.instance_ready } -> std::same_as<bool&>;
    { handoff.device_ready } -> std::same_as<bool&>;
    { handoff.swapchain_ready } -> std::same_as<bool&>;
    { handoff.render_pass_ready } -> std::same_as<bool&>;
    { handoff.surface_ready } -> std::same_as<bool&>;
    { handoff.frame_plan_ready } -> std::same_as<bool&>;
    { handoff.pipeline_required } -> std::same_as<bool&>;
    { handoff.pipeline_checked } -> std::same_as<bool&>;
    { handoff.pipeline_completed } -> std::same_as<bool&>;
    { handoff.pipeline_readiness_summary_checked } -> std::same_as<bool&>;
    { handoff.pipeline_readiness_summary_completed } -> std::same_as<bool&>;
    { handoff.shader_modules_ready } -> std::same_as<bool&>;
    { handoff.pipeline_layout_ready } -> std::same_as<bool&>;
    { handoff.graphics_pipeline_ready } -> std::same_as<bool&>;
    { handoff.resource_bindings_checked } -> std::same_as<bool&>;
    { handoff.resource_bindings_completed } -> std::same_as<bool&>;
    { handoff.resource_registry_checked } -> std::same_as<bool&>;
    { handoff.resource_registry_completed } -> std::same_as<bool&>;
    { handoff.command_packets_checked } -> std::same_as<bool&>;
    { handoff.command_packets_completed } -> std::same_as<bool&>;
    { handoff.command_packet_execution_checked } -> std::same_as<bool&>;
    { handoff.command_packet_execution_completed } -> std::same_as<bool&>;
    { handoff.command_recorder_operations_checked } -> std::same_as<bool&>;
    { handoff.command_recorder_operations_completed } -> std::same_as<bool&>;
    { handoff.command_buffer_recording_checked } -> std::same_as<bool&>;
    { handoff.command_buffer_recording_completed } -> std::same_as<bool&>;
    { handoff.command_buffer_ready_for_submit } -> std::same_as<bool&>;
    { handoff.submit_batch_planning_checked } -> std::same_as<bool&>;
    { handoff.submit_batch_planning_completed } -> std::same_as<bool&>;
    { handoff.submit_batch_ready_for_queue } -> std::same_as<bool&>;
    { handoff.present_completion_planning_checked } -> std::same_as<bool&>;
    { handoff.present_completion_planning_completed } -> std::same_as<bool&>;
    { handoff.frame_completion_ready } -> std::same_as<bool&>;
    { handoff.command_recorder_lifecycle_ready } -> std::same_as<bool&>;
    { handoff.command_recorder_gate_checked } -> std::same_as<bool&>;
    { handoff.command_recorder_gate_allowed } -> std::same_as<bool&>;
    { handoff.command_recording_ready } -> std::same_as<bool&>;
    { handoff.command_submit_readiness_checked } -> std::same_as<bool&>;
    { handoff.command_submit_readiness_ready } -> std::same_as<bool&>;
    { handoff.frame_submit_completed } -> std::same_as<bool&>;
    { handoff.present_completed } -> std::same_as<bool&>;
    { handoff.frame_lifecycle_checked } -> std::same_as<bool&>;
    { handoff.frame_lifecycle_completed } -> std::same_as<bool&>;
    { handoff.frame_lifecycle_attempted_step_count } -> std::same_as<std::size_t&>;
    { handoff.frame_lifecycle_completed_step_count } -> std::same_as<std::size_t&>;
    { handoff.planned_batch_count } -> std::same_as<std::size_t&>;
    { handoff.recorded_batch_count } -> std::same_as<std::size_t&>;
    { handoff.quad_batch_count } -> std::same_as<std::size_t&>;
    { handoff.text_batch_count } -> std::same_as<std::size_t&>;
    { handoff.image_batch_count } -> std::same_as<std::size_t&>;
    { handoff.debug_bounds_batch_count } -> std::same_as<std::size_t&>;
    { handoff.clipped_draw_call_count } -> std::same_as<std::size_t&>;
    { handoff.discarded_draw_call_count } -> std::same_as<std::size_t&>;
    { handoff.completed() } -> std::same_as<bool>;
    { handoff.blocked() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::vulkan_backend_frame_result result) {
    { result.surface } -> std::same_as<render::vulkan_backend::vulkan_surface_extent&>;
    { result.lifecycle } -> std::same_as<render::vulkan_backend::vulkan_backend_lifecycle_readiness&>;
    { result.swapchain } -> std::same_as<render::vulkan_backend::vulkan_backend_swapchain_lifecycle_state&>;
    { result.swapchain_policy } -> std::same_as<render::vulkan_backend::vulkan_backend_swapchain_policy_state&>;
    { result.frame_sync } -> std::same_as<render::vulkan_backend::vulkan_backend_frame_sync_state&>;
    { result.frame_resources }
        -> std::same_as<render::vulkan_backend::vulkan_backend_frame_resource_lifetime_state&>;
    { result.lifecycle_policy }
        -> std::same_as<render::vulkan_backend::vulkan_backend_frame_lifecycle_policy_state&>;
    { result.present_policy }
        -> std::same_as<render::vulkan_backend::vulkan_backend_frame_present_policy_state&>;
    { result.resource_bindings } -> std::same_as<render::vulkan_backend::vulkan_backend_resource_binding_state&>;
    { result.resource_registry } -> std::same_as<render::vulkan_backend::vulkan_backend_resource_registry_state&>;
    { result.pipeline } -> std::same_as<render::vulkan_backend::vulkan_backend_pipeline_state&>;
    { result.command_packets } -> std::same_as<render::vulkan_backend::vulkan_command_packet_bridge_result&>;
    { result.command_packet_execution }
        -> std::same_as<render::vulkan_backend::vulkan_command_packet_execution_result&>;
    { result.command_recorder_operations }
        -> std::same_as<render::vulkan_backend::vulkan_command_recorder_operation_plan&>;
    { result.command_buffer_recording }
        -> std::same_as<render::vulkan_backend::vulkan_command_buffer_record_result&>;
    { result.submit_batch_plan }
        -> std::same_as<render::vulkan_backend::vulkan_submit_batch_plan_result&>;
    { result.present_completion_plan }
        -> std::same_as<render::vulkan_backend::vulkan_present_completion_plan_result&>;
    { result.command_recorder } -> std::same_as<render::vulkan_backend::vulkan_backend_command_recorder_state&>;
    { result.command_submit }
        -> std::same_as<render::vulkan_backend::vulkan_command_submit_readiness_result&>;
    { result.queue_submit }
        -> std::same_as<render::vulkan_backend::vulkan_queue_submit_present_result&>;
    { result.queue_submit_adapter }
        -> std::same_as<render::vulkan_backend::vulkan_backend_queue_submit_adapter_summary&>;
    { result.command_buffer_submit }
        -> std::same_as<render::vulkan_backend::vulkan_backend_command_buffer_submit_state&>;
    { result.fallback_summary } -> std::same_as<render::vulkan_backend::vulkan_backend_frame_fallback_summary&>;
    { result.pipeline_handoff } -> std::same_as<render::vulkan_backend::vulkan_backend_frame_pipeline_handoff&>;
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
    render::vulkan_backend::vulkan_pipeline_cache_interface& pipeline_cache,
    render::vulkan_backend::vulkan_command_recorder_interface& command_recorder,
    const render::render_draw_list& draw_list,
    render::render_rect viewport) {
    { render::vulkan_backend::submit_vulkan_backend_frame(device, draw_list, viewport) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_frame_result>;
    { render::vulkan_backend::submit_vulkan_backend_frame(device, command_recorder, draw_list, viewport) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_frame_result>;
    { render::vulkan_backend::submit_vulkan_backend_frame(device, pipeline_cache, command_recorder, draw_list, viewport) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_frame_result>;
    { render::vulkan_backend::build_vulkan_resource_binding_state(
        draw_list,
        render::vulkan_backend::vulkan_frame_plan{}) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_resource_binding_state>;
    { render::vulkan_backend::build_vulkan_resource_registry_state(
        draw_list,
        render::vulkan_backend::vulkan_frame_plan{},
        render::vulkan_backend::vulkan_backend_resource_binding_state{}) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_resource_registry_state>;
    { render::vulkan_backend::build_vulkan_command_packet_bridge(
        render::vulkan_backend::vulkan_frame_plan{},
        render::vulkan_backend::vulkan_backend_pipeline_state{},
        render::vulkan_backend::vulkan_backend_resource_binding_state{},
        render::vulkan_backend::vulkan_backend_resource_registry_state{}) }
        -> std::same_as<render::vulkan_backend::vulkan_command_packet_bridge_result>;
    { render::vulkan_backend::build_vulkan_command_recorder_operation_plan(
        render::vulkan_backend::vulkan_command_packet_bridge_result{},
        render::vulkan_backend::vulkan_command_packet_execution_result{}) }
        -> std::same_as<render::vulkan_backend::vulkan_command_recorder_operation_plan>;
    { render::vulkan_backend::record_vulkan_command_buffer_operations(
        *static_cast<render::vulkan_backend::vulkan_command_buffer_operation_recorder_interface*>(nullptr),
        render::vulkan_backend::vulkan_command_buffer_id{},
        render::vulkan_backend::vulkan_command_recorder_operation_plan{},
        render::vulkan_backend::vulkan_native_function_table_diagnostics{}) }
        -> std::same_as<render::vulkan_backend::vulkan_command_buffer_record_result>;
    { render::vulkan_backend::build_vulkan_submit_batch_plan(
        render::vulkan_backend::vulkan_command_buffer_record_result{},
        render::vulkan_backend::vulkan_command_submit_readiness_result{}) }
        -> std::same_as<render::vulkan_backend::vulkan_submit_batch_plan_result>;
    { render::vulkan_backend::build_vulkan_submit_batch_plan(
        render::vulkan_backend::vulkan_command_buffer_record_result{},
        render::vulkan_backend::vulkan_command_submit_readiness_result{},
        render::vulkan_backend::vulkan_native_function_table_diagnostics{}) }
        -> std::same_as<render::vulkan_backend::vulkan_submit_batch_plan_result>;
    { render::vulkan_backend::build_vulkan_present_completion_plan(
        render::vulkan_backend::vulkan_submit_batch_plan_result{},
        render::vulkan_backend::vulkan_queue_submit_present_result{}) }
        -> std::same_as<render::vulkan_backend::vulkan_present_completion_plan_result>;
    { render::vulkan_backend::build_vulkan_present_completion_plan(
        render::vulkan_backend::vulkan_submit_batch_plan_result{},
        render::vulkan_backend::vulkan_queue_submit_present_result{},
        render::vulkan_backend::vulkan_native_function_table_diagnostics{}) }
        -> std::same_as<render::vulkan_backend::vulkan_present_completion_plan_result>;
});

static_assert(requires(
    render::vulkan_backend::vulkan_backend_frame_result frame,
    render::vulkan_backend::vulkan_queue_submit_present_result queue_submit) {
    { render::vulkan_backend::summarize_vulkan_queue_submit_adapter_result(queue_submit) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_queue_submit_adapter_summary>;
    { render::vulkan_backend::apply_vulkan_queue_submit_adapter_result_to_frame(
        frame,
        queue_submit) } -> std::same_as<render::vulkan_backend::vulkan_backend_frame_result>;
    { render::vulkan_backend::summarize_vulkan_frame_pipeline_handoff(frame) }
        -> std::same_as<render::vulkan_backend::vulkan_backend_frame_pipeline_handoff>;
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
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::render_pass_unavailable),
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
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::acquire_image_failed),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::resource_binding_unavailable),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::submit_frame_failed),
    render::vulkan_backend::vulkan_backend_fallback_reason>);
static_assert(std::same_as<
    decltype(render::vulkan_backend::vulkan_backend_fallback_reason::present_image_failed),
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
