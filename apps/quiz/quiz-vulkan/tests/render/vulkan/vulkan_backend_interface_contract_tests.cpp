#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"

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

static_assert(requires(render::vulkan_backend::vulkan_loader_probe_status status) {
    { render::vulkan_backend::loader_probe_status_name(status) } -> std::same_as<std::string_view>;
});

static_assert(requires(render::vulkan_backend::vulkan_loader_readiness_status status) {
    { render::vulkan_backend::loader_readiness_status_name(status) } -> std::same_as<std::string_view>;
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

static_assert(requires(render::vulkan_backend::fake_vulkan_loader_options options) {
    { options.library_name } -> std::same_as<std::string&>;
    { options.library_available } -> std::same_as<bool&>;
    { options.required_symbol_available } -> std::same_as<bool&>;
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
    { lifecycle.loader } -> std::same_as<render::vulkan_backend::vulkan_loader_readiness_state&>;
    { lifecycle.instance } -> std::same_as<render::vulkan_backend::vulkan_instance_create_result&>;
    { lifecycle.device } -> std::same_as<render::vulkan_backend::vulkan_device_create_result&>;
    { lifecycle.effective_instance_ready() } -> std::same_as<bool>;
    { lifecycle.effective_device_ready() } -> std::same_as<bool>;
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
    { pipeline.compatibility } -> std::same_as<render::vulkan_backend::vulkan_pipeline_compatibility_key_summary&>;
    { pipeline.shader_bindings }
        -> std::same_as<render::vulkan_backend::vulkan_backend_shader_binding_readiness_state&>;
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
    { result.command_recorder } -> std::same_as<render::vulkan_backend::vulkan_backend_command_recorder_state&>;
    { result.command_buffer_submit }
        -> std::same_as<render::vulkan_backend::vulkan_backend_command_buffer_submit_state&>;
    { result.fallback_summary } -> std::same_as<render::vulkan_backend::vulkan_backend_frame_fallback_summary&>;
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
