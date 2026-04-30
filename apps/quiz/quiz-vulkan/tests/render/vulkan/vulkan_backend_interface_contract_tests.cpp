#include "render/vulkan/vulkan_backend_adapter.h"

#include <concepts>
#include <cstddef>
#include <string>
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
    { lifecycle.pipeline_ready } -> std::same_as<bool&>;
    { lifecycle.command_recorder_ready } -> std::same_as<bool&>;
    { lifecycle.ready_for_frame() } -> std::same_as<bool>;
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

static_assert(requires(
    render::vulkan_backend::vulkan_swapchain_acquire_status acquire_status,
    render::vulkan_backend::vulkan_swapchain_present_status present_status) {
    { render::vulkan_backend::swapchain_acquire_status_name(acquire_status) } -> std::same_as<std::string_view>;
    { render::vulkan_backend::swapchain_present_status_name(present_status) } -> std::same_as<std::string_view>;
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

static_assert(requires(render::vulkan_backend::vulkan_shader_stage stage) {
    { render::vulkan_backend::shader_stage_name(stage) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_shader_module_descriptor descriptor) {
    { descriptor.id } -> std::same_as<render::vulkan_backend::vulkan_shader_module_id&>;
    { descriptor.stage } -> std::same_as<render::vulkan_backend::vulkan_shader_stage&>;
    { descriptor.entry_point } -> std::same_as<std::string&>;
    { descriptor.valid() } -> std::same_as<bool>;
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
    { pipeline.supports(render::vulkan_backend::vulkan_batch_kind::quad) } -> std::same_as<bool>;
    { pipeline.descriptor_for(render::vulkan_backend::vulkan_batch_kind::quad) }
        -> std::same_as<const render::vulkan_backend::vulkan_pipeline_descriptor*>;
    { pipeline.completed() } -> std::same_as<bool>;
});

static_assert(requires(render::vulkan_backend::diagnostic_vulkan_pipeline_cache_options options) {
    { options.default_available } -> std::same_as<bool&>;
    { options.use_default_shader_modules } -> std::same_as<bool&>;
    { options.use_default_pipeline_descriptors } -> std::same_as<bool&>;
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
    { result.swapchain } -> std::same_as<render::vulkan_backend::vulkan_backend_swapchain_lifecycle_state&>;
    { result.frame_sync } -> std::same_as<render::vulkan_backend::vulkan_backend_frame_sync_state&>;
    { result.resource_bindings } -> std::same_as<render::vulkan_backend::vulkan_backend_resource_binding_state&>;
    { result.resource_registry } -> std::same_as<render::vulkan_backend::vulkan_backend_resource_registry_state&>;
    { result.pipeline } -> std::same_as<render::vulkan_backend::vulkan_backend_pipeline_state&>;
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
