#pragma once

#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_native_symbols.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

struct vulkan_surface_extent {
    std::size_t width = 0;
    std::size_t height = 0;

    bool valid() const
    {
        return width > 0 && height > 0;
    }
};

struct vulkan_surface_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_swapchain_image_id {
    std::size_t value = 0;
};

struct vulkan_swapchain_image_state {
    vulkan_swapchain_image_id id;
    bool available = false;
    bool acquired = false;
    bool presented = false;

    bool ready_for_recording() const
    {
        return available && acquired;
    }
};

enum class vulkan_swapchain_acquire_status {
    not_requested,
    acquired,
    backpressured,
    timeout,
    out_of_date,
    suboptimal,
    failed,
    error,
};

std::string_view swapchain_acquire_status_name(vulkan_swapchain_acquire_status status);

enum class vulkan_swapchain_image_acquire_plan_status {
    not_checked,
    not_requested,
    ready,
    lifecycle_unavailable,
    swapchain_unavailable,
    sync_unavailable,
    no_images_available,
    backpressured,
    timeout,
    out_of_date,
    suboptimal,
    error,
};

std::string_view swapchain_image_acquire_plan_status_name(
    vulkan_swapchain_image_acquire_plan_status status);

enum class vulkan_swapchain_present_status {
    not_requested,
    presented,
    out_of_date,
    suboptimal,
    failed,
    error,
};

std::string_view swapchain_present_status_name(vulkan_swapchain_present_status status);

enum class vulkan_swapchain_present_mode {
    immediate,
    mailbox,
    fifo,
    fifo_relaxed,
};

std::string_view swapchain_present_mode_name(vulkan_swapchain_present_mode mode);

enum class vulkan_swapchain_image_format {
    undefined,
    rgba8_unorm,
    bgra8_unorm,
};

inline std::string_view swapchain_image_format_name(vulkan_swapchain_image_format format)
{
    switch (format) {
    case vulkan_swapchain_image_format::undefined:
        return "undefined";
    case vulkan_swapchain_image_format::rgba8_unorm:
        return "rgba8_unorm";
    case vulkan_swapchain_image_format::bgra8_unorm:
        return "bgra8_unorm";
    }

    return "unknown";
}

enum class vulkan_swapchain_color_space {
    srgb_nonlinear,
    display_p3_nonlinear,
};

inline std::string_view swapchain_color_space_name(vulkan_swapchain_color_space color_space)
{
    switch (color_space) {
    case vulkan_swapchain_color_space::srgb_nonlinear:
        return "srgb_nonlinear";
    case vulkan_swapchain_color_space::display_p3_nonlinear:
        return "display_p3_nonlinear";
    }

    return "unknown";
}

enum class vulkan_swapchain_surface_transform {
    identity,
    rotate_90,
    rotate_180,
    rotate_270,
};

inline std::string_view swapchain_surface_transform_name(
    vulkan_swapchain_surface_transform transform)
{
    switch (transform) {
    case vulkan_swapchain_surface_transform::identity:
        return "identity";
    case vulkan_swapchain_surface_transform::rotate_90:
        return "rotate_90";
    case vulkan_swapchain_surface_transform::rotate_180:
        return "rotate_180";
    case vulkan_swapchain_surface_transform::rotate_270:
        return "rotate_270";
    }

    return "unknown";
}

enum class vulkan_swapchain_composite_alpha {
    opaque,
    pre_multiplied,
    post_multiplied,
    inherit,
};

inline std::string_view swapchain_composite_alpha_name(
    vulkan_swapchain_composite_alpha alpha)
{
    switch (alpha) {
    case vulkan_swapchain_composite_alpha::opaque:
        return "opaque";
    case vulkan_swapchain_composite_alpha::pre_multiplied:
        return "pre_multiplied";
    case vulkan_swapchain_composite_alpha::post_multiplied:
        return "post_multiplied";
    case vulkan_swapchain_composite_alpha::inherit:
        return "inherit";
    }

    return "unknown";
}

enum class vulkan_swapchain_image_sharing_mode {
    exclusive,
    concurrent,
};

inline std::string_view swapchain_image_sharing_mode_name(
    vulkan_swapchain_image_sharing_mode mode)
{
    switch (mode) {
    case vulkan_swapchain_image_sharing_mode::exclusive:
        return "exclusive";
    case vulkan_swapchain_image_sharing_mode::concurrent:
        return "concurrent";
    }

    return "unknown";
}

struct vulkan_swapchain_surface_format {
    vulkan_swapchain_image_format format = vulkan_swapchain_image_format::bgra8_unorm;
    vulkan_swapchain_color_space color_space =
        vulkan_swapchain_color_space::srgb_nonlinear;

    bool valid() const
    {
        return format != vulkan_swapchain_image_format::undefined;
    }
};

struct vulkan_swapchain_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_swapchain_acquire_result {
    vulkan_swapchain_acquire_status status = vulkan_swapchain_acquire_status::not_requested;
    vulkan_swapchain_image_state image;

    bool completed() const
    {
        return (status == vulkan_swapchain_acquire_status::acquired
                || status == vulkan_swapchain_acquire_status::suboptimal)
            && image.ready_for_recording();
    }
};

struct vulkan_swapchain_image_acquire_request {
    bool requested = false;
    bool lifecycle_ready = false;
    bool swapchain_ready = false;
    vulkan_swapchain_handle swapchain;
    std::size_t image_count = 0;
    std::uint64_t timeout_nanoseconds = 0;
    bool image_available_semaphore_ready = false;
    bool fence_ready = false;
    bool allow_suboptimal = true;
};

struct vulkan_swapchain_image_acquire_plan_result {
    bool checked = false;
    vulkan_swapchain_image_acquire_plan_status status =
        vulkan_swapchain_image_acquire_plan_status::not_checked;
    vulkan_swapchain_image_acquire_request request;
    vulkan_swapchain_acquire_status acquire_status =
        vulkan_swapchain_acquire_status::not_requested;
    std::size_t selected_image_index = 0;
    vulkan_swapchain_image_id image_id;
    bool lifecycle_ready = false;
    bool swapchain_ready = false;
    bool sync_primitives_ready = false;
    bool acquire_requested = false;
    bool acquire_completed = false;
    bool image_available = false;
    bool image_acquired = false;
    bool timed_out = false;
    bool out_of_date = false;
    bool suboptimal = false;
    bool error = false;
    std::string diagnostic;

    bool ready_for_command_recording() const
    {
        return checked && acquire_completed && image_available && image_acquired
            && (status == vulkan_swapchain_image_acquire_plan_status::ready
                || status == vulkan_swapchain_image_acquire_plan_status::suboptimal);
    }

    bool blocked() const
    {
        return checked && !ready_for_command_recording();
    }
};

struct vulkan_swapchain_present_result {
    vulkan_swapchain_present_status status = vulkan_swapchain_present_status::not_requested;
    vulkan_swapchain_image_id image_id;

    bool completed() const
    {
        return status == vulkan_swapchain_present_status::presented
            || status == vulkan_swapchain_present_status::suboptimal;
    }
};

enum class vulkan_swapchain_recreate_policy_action {
    not_checked,
    keep_rendering,
    recreate_immediately,
    recreate_after_frame,
    skip_submit,
    fatal_error,
};

std::string_view swapchain_recreate_policy_action_name(
    vulkan_swapchain_recreate_policy_action action);

struct vulkan_swapchain_recreate_policy_result {
    bool checked = false;
    vulkan_swapchain_recreate_policy_action action =
        vulkan_swapchain_recreate_policy_action::not_checked;
    bool acquire_plan_checked = false;
    vulkan_swapchain_image_acquire_plan_status acquire_plan_status =
        vulkan_swapchain_image_acquire_plan_status::not_checked;
    vulkan_swapchain_acquire_status acquire_status =
        vulkan_swapchain_acquire_status::not_requested;
    bool present_checked = false;
    vulkan_swapchain_present_status present_status =
        vulkan_swapchain_present_status::not_requested;
    bool acquired_image_ready = false;
    bool acquire_out_of_date = false;
    bool acquire_suboptimal = false;
    bool acquire_timed_out = false;
    bool acquire_error = false;
    bool present_out_of_date = false;
    bool present_suboptimal = false;
    bool present_error = false;
    bool should_keep_rendering = false;
    bool should_recreate_immediately = false;
    bool should_recreate_after_frame = false;
    bool should_skip_submit = false;
    bool fatal = false;
    std::string diagnostic;

    bool keep_rendering() const
    {
        return checked && should_keep_rendering
            && action == vulkan_swapchain_recreate_policy_action::keep_rendering;
    }

    bool recreate_immediately() const
    {
        return checked && should_recreate_immediately
            && action == vulkan_swapchain_recreate_policy_action::recreate_immediately;
    }

    bool recreate_after_frame() const
    {
        return checked && should_recreate_after_frame
            && action == vulkan_swapchain_recreate_policy_action::recreate_after_frame;
    }

    bool skip_submit() const
    {
        return checked && should_skip_submit
            && action == vulkan_swapchain_recreate_policy_action::skip_submit;
    }

    bool fatal_error() const
    {
        return checked && fatal
            && action == vulkan_swapchain_recreate_policy_action::fatal_error;
    }

    bool allows_frame_progress() const
    {
        return keep_rendering() || recreate_after_frame();
    }
};

struct vulkan_backend_swapchain_lifecycle_state {
    bool acquire_requested = false;
    bool present_requested = false;
    vulkan_swapchain_acquire_result acquire;
    vulkan_swapchain_present_result present;

    bool acquired() const
    {
        return acquire_requested && acquire.completed();
    }

    bool presented() const
    {
        return present_requested && present.completed();
    }

    bool completed() const
    {
        return acquired() && presented();
    }
};

struct vulkan_swapchain_extent_policy_state {
    bool checked = false;
    vulkan_surface_extent requested_extent;
    vulkan_surface_extent selected_extent;
    vulkan_surface_extent min_extent{.width = 1, .height = 1};
    vulkan_surface_extent max_extent{.width = 4096, .height = 4096};
    bool extent_supported = false;
    bool extent_clamped = false;

    bool completed() const
    {
        return checked && extent_supported && selected_extent.valid();
    }
};

struct vulkan_swapchain_present_mode_policy_state {
    bool checked = false;
    vulkan_swapchain_present_mode requested_mode = vulkan_swapchain_present_mode::fifo;
    vulkan_swapchain_present_mode selected_mode = vulkan_swapchain_present_mode::fifo;
    bool requested_mode_supported = true;
    bool fallback_to_fifo = false;

    bool completed() const
    {
        return checked && requested_mode_supported && !fallback_to_fifo;
    }
};

struct vulkan_backend_swapchain_policy_state {
    bool checked = false;
    vulkan_swapchain_extent_policy_state extent;
    vulkan_swapchain_present_mode_policy_state present_mode;

    bool completed() const
    {
        return checked && extent.completed() && present_mode.completed();
    }
};

struct vulkan_swapchain_surface_capabilities_snapshot {
    bool checked = true;
    std::size_t min_image_count = 2;
    std::size_t max_image_count = 0;
    vulkan_surface_extent current_extent;
    vulkan_surface_extent min_extent{.width = 1, .height = 1};
    vulkan_surface_extent max_extent{.width = 4096, .height = 4096};
    std::vector<vulkan_swapchain_surface_transform> supported_transforms{
        vulkan_swapchain_surface_transform::identity,
    };
    vulkan_swapchain_surface_transform current_transform =
        vulkan_swapchain_surface_transform::identity;
    std::vector<vulkan_swapchain_composite_alpha> supported_composite_alpha{
        vulkan_swapchain_composite_alpha::opaque,
    };
    std::vector<vulkan_swapchain_surface_format> surface_formats{
        vulkan_swapchain_surface_format{},
    };
    std::vector<vulkan_swapchain_present_mode> present_modes{
        vulkan_swapchain_present_mode::fifo,
    };
};

struct vulkan_swapchain_create_plan_intent {
    bool desired_vsync = true;
    vulkan_surface_extent desired_extent{.width = 1, .height = 1};
    std::size_t desired_image_count = 2;
    vulkan_swapchain_surface_format desired_surface_format;
    vulkan_swapchain_surface_transform desired_transform =
        vulkan_swapchain_surface_transform::identity;
    vulkan_swapchain_composite_alpha desired_composite_alpha =
        vulkan_swapchain_composite_alpha::opaque;
    bool graphics_queue_family_available = true;
    bool present_queue_family_available = true;
    std::size_t graphics_queue_family_index = 0;
    std::size_t present_queue_family_index = 0;
};

struct vulkan_swapchain_recreate_compatibility_snapshot {
    bool checked = false;
    vulkan_surface_extent previous_extent;
    std::size_t previous_image_count = 0;
    vulkan_swapchain_surface_format previous_surface_format;
    vulkan_swapchain_present_mode previous_present_mode =
        vulkan_swapchain_present_mode::fifo;
};

struct vulkan_swapchain_create_plan_request {
    vulkan_swapchain_surface_capabilities_snapshot capabilities;
    vulkan_swapchain_create_plan_intent intent;
    vulkan_swapchain_recreate_compatibility_snapshot recreate_reference;
};

enum class vulkan_swapchain_create_plan_status {
    not_checked,
    ready,
    missing_surface_format,
    missing_present_mode,
    unsupported_zero_extent,
    missing_transform,
    missing_composite_alpha,
};

inline std::string_view swapchain_create_plan_status_name(
    vulkan_swapchain_create_plan_status status)
{
    switch (status) {
    case vulkan_swapchain_create_plan_status::not_checked:
        return "not_checked";
    case vulkan_swapchain_create_plan_status::ready:
        return "ready";
    case vulkan_swapchain_create_plan_status::missing_surface_format:
        return "missing_surface_format";
    case vulkan_swapchain_create_plan_status::missing_present_mode:
        return "missing_present_mode";
    case vulkan_swapchain_create_plan_status::unsupported_zero_extent:
        return "unsupported_zero_extent";
    case vulkan_swapchain_create_plan_status::missing_transform:
        return "missing_transform";
    case vulkan_swapchain_create_plan_status::missing_composite_alpha:
        return "missing_composite_alpha";
    }

    return "unknown";
}

struct vulkan_swapchain_create_plan_result {
    bool checked = false;
    vulkan_swapchain_create_plan_status status =
        vulkan_swapchain_create_plan_status::not_checked;
    vulkan_swapchain_surface_capabilities_snapshot capabilities;
    vulkan_swapchain_create_plan_intent intent;
    vulkan_swapchain_surface_format selected_surface_format;
    vulkan_swapchain_present_mode selected_present_mode =
        vulkan_swapchain_present_mode::fifo;
    std::size_t selected_image_count = 0;
    vulkan_surface_extent selected_extent;
    vulkan_swapchain_surface_transform selected_transform =
        vulkan_swapchain_surface_transform::identity;
    vulkan_swapchain_composite_alpha selected_composite_alpha =
        vulkan_swapchain_composite_alpha::opaque;
    vulkan_swapchain_image_sharing_mode selected_sharing_mode =
        vulkan_swapchain_image_sharing_mode::exclusive;
    bool capabilities_checked = false;
    bool desired_format_supported = false;
    bool fallback_format_selected = false;
    bool desired_present_mode_supported = false;
    bool fallback_present_mode_selected = false;
    bool extent_clamped = false;
    bool image_count_clamped = false;
    bool min_image_count_clamped = false;
    bool max_image_count_clamped = false;
    bool zero_extent_unsupported = false;
    bool transform_supported = false;
    bool fallback_transform_selected = false;
    bool composite_alpha_supported = false;
    bool fallback_composite_alpha_selected = false;
    bool sharing_concurrent = false;
    bool recreate_reference_checked = false;
    bool recreate_compatible = true;
    bool recreate_extent_compatible = true;
    bool recreate_format_compatible = true;
    bool recreate_present_mode_compatible = true;
    bool recreate_image_count_compatible = true;
    std::string diagnostic;

    bool ready_for_create() const
    {
        return checked && status == vulkan_swapchain_create_plan_status::ready
            && selected_surface_format.valid() && selected_extent.valid()
            && selected_image_count > 0;
    }

    bool blocked() const
    {
        return checked && !ready_for_create();
    }
};

enum class vulkan_native_swapchain_create_operation_status {
    not_checked,
    ready,
    create_plan_unavailable,
    native_entrypoints_unavailable,
    invalid_device,
    invalid_surface,
    required_extension_unavailable,
    missing_create_symbol,
    missing_destroy_symbol,
    missing_images_symbol,
    recreate_incompatible,
};

inline std::string_view native_swapchain_create_operation_status_name(
    vulkan_native_swapchain_create_operation_status status)
{
    switch (status) {
    case vulkan_native_swapchain_create_operation_status::not_checked:
        return "not_checked";
    case vulkan_native_swapchain_create_operation_status::ready:
        return "ready";
    case vulkan_native_swapchain_create_operation_status::create_plan_unavailable:
        return "create_plan_unavailable";
    case vulkan_native_swapchain_create_operation_status::native_entrypoints_unavailable:
        return "native_entrypoints_unavailable";
    case vulkan_native_swapchain_create_operation_status::invalid_device:
        return "invalid_device";
    case vulkan_native_swapchain_create_operation_status::invalid_surface:
        return "invalid_surface";
    case vulkan_native_swapchain_create_operation_status::required_extension_unavailable:
        return "required_extension_unavailable";
    case vulkan_native_swapchain_create_operation_status::missing_create_symbol:
        return "missing_create_symbol";
    case vulkan_native_swapchain_create_operation_status::missing_destroy_symbol:
        return "missing_destroy_symbol";
    case vulkan_native_swapchain_create_operation_status::missing_images_symbol:
        return "missing_images_symbol";
    case vulkan_native_swapchain_create_operation_status::recreate_incompatible:
        return "recreate_incompatible";
    }

    return "unknown";
}

struct vulkan_native_swapchain_create_operation_request {
    vulkan_swapchain_create_plan_result create_plan;
    vulkan_native_swapchain_entrypoint_readiness native_entrypoints;
    vulkan_device_handle device;
    vulkan_surface_handle surface;
    vulkan_swapchain_handle old_swapchain;
    bool require_recreate_compatibility = false;
};

struct vulkan_native_swapchain_create_operation_summary {
    bool checked = false;
    vulkan_native_swapchain_create_operation_status status =
        vulkan_native_swapchain_create_operation_status::not_checked;
    std::string entrypoint_name = "vkCreateSwapchainKHR";
    bool vk_create_swapchain_callable = false;
    bool create_plan_ready = false;
    bool native_entrypoints_ready_for_create = false;
    bool device_valid = false;
    bool surface_valid = false;
    vulkan_device_handle device;
    vulkan_surface_handle surface;
    vulkan_swapchain_handle old_swapchain;
    vulkan_surface_extent selected_extent;
    vulkan_swapchain_surface_format selected_surface_format;
    vulkan_swapchain_present_mode selected_present_mode =
        vulkan_swapchain_present_mode::fifo;
    std::size_t selected_image_count = 0;
    vulkan_swapchain_surface_transform selected_transform =
        vulkan_swapchain_surface_transform::identity;
    vulkan_swapchain_composite_alpha selected_composite_alpha =
        vulkan_swapchain_composite_alpha::opaque;
    vulkan_swapchain_image_sharing_mode selected_sharing_mode =
        vulkan_swapchain_image_sharing_mode::exclusive;
    bool recreate_reference_checked = false;
    bool recreate_compatible = true;
    bool recreate_compatibility_required = false;
    bool recreate_compatibility_blocked = false;
    bool required_extensions_ready = false;
    bool create_symbol_ready = false;
    bool destroy_symbol_ready = false;
    bool get_images_symbol_ready = false;
    std::string missing_required_extension;
    std::string missing_symbol_name;
    std::string diagnostic;

    bool ready() const
    {
        return checked && status == vulkan_native_swapchain_create_operation_status::ready
            && vk_create_swapchain_callable;
    }
};

struct vulkan_native_swapchain_create_operation_result {
    bool checked = false;
    vulkan_native_swapchain_create_operation_status status =
        vulkan_native_swapchain_create_operation_status::not_checked;
    vulkan_swapchain_create_plan_result create_plan;
    vulkan_native_swapchain_entrypoint_readiness native_entrypoints;
    vulkan_device_handle device;
    vulkan_surface_handle surface;
    vulkan_swapchain_handle old_swapchain;
    vulkan_surface_extent selected_extent;
    vulkan_swapchain_surface_format selected_surface_format;
    vulkan_swapchain_present_mode selected_present_mode =
        vulkan_swapchain_present_mode::fifo;
    std::size_t selected_image_count = 0;
    vulkan_swapchain_surface_transform selected_transform =
        vulkan_swapchain_surface_transform::identity;
    vulkan_swapchain_composite_alpha selected_composite_alpha =
        vulkan_swapchain_composite_alpha::opaque;
    vulkan_swapchain_image_sharing_mode selected_sharing_mode =
        vulkan_swapchain_image_sharing_mode::exclusive;
    bool create_plan_checked = false;
    bool create_plan_ready = false;
    bool native_entrypoints_checked = false;
    bool native_entrypoints_ready_for_create = false;
    bool device_valid = false;
    bool surface_valid = false;
    bool required_extensions_ready = false;
    bool create_symbol_ready = false;
    bool destroy_symbol_ready = false;
    bool get_images_symbol_ready = false;
    bool recreate_reference_checked = false;
    bool recreate_compatible = true;
    bool recreate_compatibility_required = false;
    bool recreate_compatibility_blocked = false;
    bool vk_create_swapchain_callable = false;
    std::string missing_required_extension;
    std::string missing_symbol_name;
    std::string diagnostic;
    vulkan_native_swapchain_create_operation_summary operation;

    bool can_call_vk_create_swapchain() const
    {
        return checked && status == vulkan_native_swapchain_create_operation_status::ready
            && vk_create_swapchain_callable && operation.ready();
    }

    bool blocked() const
    {
        return checked && !can_call_vk_create_swapchain();
    }
};

struct vulkan_swapchain_create_request {
    vulkan_surface_extent requested_extent;
    vulkan_swapchain_present_mode requested_present_mode = vulkan_swapchain_present_mode::fifo;
    std::size_t min_image_count = 2;
};

enum class vulkan_swapchain_create_status {
    not_requested,
    created,
    device_unavailable,
    invalid_surface_extent,
    missing_present_queue,
    unsupported_present_mode,
    creation_failed,
};

inline std::string_view swapchain_create_status_name(vulkan_swapchain_create_status status)
{
    switch (status) {
    case vulkan_swapchain_create_status::not_requested:
        return "not_requested";
    case vulkan_swapchain_create_status::created:
        return "created";
    case vulkan_swapchain_create_status::device_unavailable:
        return "device_unavailable";
    case vulkan_swapchain_create_status::invalid_surface_extent:
        return "invalid_surface_extent";
    case vulkan_swapchain_create_status::missing_present_queue:
        return "missing_present_queue";
    case vulkan_swapchain_create_status::unsupported_present_mode:
        return "unsupported_present_mode";
    case vulkan_swapchain_create_status::creation_failed:
        return "creation_failed";
    }

    return "unknown";
}

struct vulkan_swapchain_create_result {
    bool checked = false;
    vulkan_swapchain_create_status status = vulkan_swapchain_create_status::not_requested;
    vulkan_device_create_result device;
    vulkan_swapchain_handle handle;
    vulkan_surface_extent requested_extent;
    vulkan_surface_extent selected_extent;
    vulkan_swapchain_present_mode requested_present_mode = vulkan_swapchain_present_mode::fifo;
    vulkan_swapchain_present_mode selected_present_mode = vulkan_swapchain_present_mode::fifo;
    bool requested_present_mode_supported = false;
    bool fallback_to_fifo = false;
    std::size_t min_image_count = 0;
    std::string diagnostic;

    bool ready_for_frame() const
    {
        return checked && status == vulkan_swapchain_create_status::created
            && device.ready_for_backend() && handle.valid() && selected_extent.valid();
    }
};

class vulkan_swapchain_factory_interface {
public:
    virtual ~vulkan_swapchain_factory_interface() = default;

    virtual vulkan_swapchain_create_result create_swapchain(
        const vulkan_device_create_result& device_result,
        const vulkan_swapchain_create_request& request) = 0;
};

struct fake_vulkan_swapchain_factory_options {
    std::vector<vulkan_swapchain_present_mode> supported_present_modes{
        vulkan_swapchain_present_mode::fifo,
    };
    vulkan_surface_extent min_extent{.width = 1, .height = 1};
    vulkan_surface_extent max_extent{.width = 4096, .height = 4096};
    bool fail_creation = false;
    vulkan_swapchain_handle handle{.value = 1};
};

class fake_vulkan_swapchain_factory final : public vulkan_swapchain_factory_interface {
public:
    fake_vulkan_swapchain_factory();
    explicit fake_vulkan_swapchain_factory(fake_vulkan_swapchain_factory_options options);

    vulkan_swapchain_create_result create_swapchain(
        const vulkan_device_create_result& device_result,
        const vulkan_swapchain_create_request& request) override;

private:
    fake_vulkan_swapchain_factory_options options_;
};

namespace swapchain_detail {

inline bool contains_present_mode(
    const std::vector<vulkan_swapchain_present_mode>& modes,
    vulkan_swapchain_present_mode mode)
{
    return std::find(modes.begin(), modes.end(), mode) != modes.end();
}

inline bool same_surface_format(
    vulkan_swapchain_surface_format left,
    vulkan_swapchain_surface_format right)
{
    return left.format == right.format && left.color_space == right.color_space;
}

inline bool contains_surface_format(
    const std::vector<vulkan_swapchain_surface_format>& formats,
    vulkan_swapchain_surface_format format)
{
    return std::find_if(
        formats.begin(),
        formats.end(),
        [format](vulkan_swapchain_surface_format candidate) {
            return candidate.valid() && same_surface_format(candidate, format);
        }) != formats.end();
}

template <typename T>
inline bool contains_value(const std::vector<T>& values, T value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

inline bool same_extent(vulkan_surface_extent left, vulkan_surface_extent right)
{
    return left.width == right.width && left.height == right.height;
}

inline bool contains_queue_capability(
    const std::vector<vulkan_device_queue_selection>& queues,
    vulkan_device_queue_capability capability)
{
    return std::find_if(
        queues.begin(),
        queues.end(),
        [capability](const vulkan_device_queue_selection& queue) {
            return queue.capability == capability && queue.valid();
        }) != queues.end();
}

inline std::size_t clamp_extent_value(
    std::size_t value,
    std::size_t min_value,
    std::size_t max_value)
{
    return std::clamp(value, min_value, max_value);
}

inline vulkan_surface_extent clamp_extent(
    vulkan_surface_extent requested_extent,
    vulkan_surface_extent min_extent,
    vulkan_surface_extent max_extent)
{
    return vulkan_surface_extent{
        .width = clamp_extent_value(requested_extent.width, min_extent.width, max_extent.width),
        .height = clamp_extent_value(requested_extent.height, min_extent.height, max_extent.height),
    };
}

inline std::size_t clamp_image_count(
    std::size_t desired,
    std::size_t minimum,
    std::size_t maximum)
{
    std::size_t selected = std::max(desired, minimum);
    if (maximum > 0) {
        selected = std::min(selected, maximum);
    }
    return selected;
}

inline vulkan_swapchain_create_result make_swapchain_create_result(
    const vulkan_device_create_result& device_result,
    const vulkan_swapchain_create_request& request)
{
    return vulkan_swapchain_create_result{
        .checked = true,
        .status = vulkan_swapchain_create_status::not_requested,
        .device = device_result,
        .handle = {},
        .requested_extent = request.requested_extent,
        .selected_extent = {},
        .requested_present_mode = request.requested_present_mode,
        .selected_present_mode = vulkan_swapchain_present_mode::fifo,
        .requested_present_mode_supported = false,
        .fallback_to_fifo = false,
        .min_image_count = request.min_image_count,
        .diagnostic = {},
    };
}

inline std::size_t selected_image_index_for(
    const vulkan_swapchain_image_acquire_request& request,
    const vulkan_swapchain_acquire_result& acquire)
{
    static_cast<void>(request);
    return acquire.image.id.value;
}

} // namespace swapchain_detail

inline vulkan_swapchain_create_plan_result build_vulkan_swapchain_create_plan(
    const vulkan_swapchain_create_plan_request& request)
{
    const vulkan_swapchain_surface_capabilities_snapshot& capabilities =
        request.capabilities;
    const vulkan_swapchain_create_plan_intent& intent = request.intent;

    vulkan_swapchain_create_plan_result result{
        .checked = true,
        .status = vulkan_swapchain_create_plan_status::not_checked,
        .capabilities = capabilities,
        .intent = intent,
        .selected_surface_format = {},
        .selected_present_mode = vulkan_swapchain_present_mode::fifo,
        .selected_image_count = 0,
        .selected_extent = {},
        .selected_transform = vulkan_swapchain_surface_transform::identity,
        .selected_composite_alpha = vulkan_swapchain_composite_alpha::opaque,
        .selected_sharing_mode = vulkan_swapchain_image_sharing_mode::exclusive,
        .capabilities_checked = capabilities.checked,
        .recreate_reference_checked = request.recreate_reference.checked,
        .diagnostic = {},
    };

    if (capabilities.surface_formats.empty()) {
        result.status = vulkan_swapchain_create_plan_status::missing_surface_format;
        result.diagnostic = "Vulkan swapchain create plan has no supported surface formats";
        return result;
    }

    if (swapchain_detail::contains_surface_format(
            capabilities.surface_formats,
            intent.desired_surface_format)) {
        result.selected_surface_format = intent.desired_surface_format;
        result.desired_format_supported = true;
    } else {
        const auto fallback = std::find_if(
            capabilities.surface_formats.begin(),
            capabilities.surface_formats.end(),
            [](vulkan_swapchain_surface_format candidate) {
                return candidate.valid();
            });
        if (fallback == capabilities.surface_formats.end()) {
            result.status = vulkan_swapchain_create_plan_status::missing_surface_format;
            result.diagnostic =
                "Vulkan swapchain create plan has no usable surface format";
            return result;
        }
        result.selected_surface_format = *fallback;
        result.fallback_format_selected = true;
    }

    if (capabilities.present_modes.empty()) {
        result.status = vulkan_swapchain_create_plan_status::missing_present_mode;
        result.diagnostic = "Vulkan swapchain create plan has no supported present modes";
        return result;
    }

    const std::vector<vulkan_swapchain_present_mode> present_mode_candidates =
        intent.desired_vsync
        ? std::vector<vulkan_swapchain_present_mode>{
              vulkan_swapchain_present_mode::fifo,
              vulkan_swapchain_present_mode::fifo_relaxed,
              vulkan_swapchain_present_mode::mailbox,
              vulkan_swapchain_present_mode::immediate,
          }
        : std::vector<vulkan_swapchain_present_mode>{
              vulkan_swapchain_present_mode::mailbox,
              vulkan_swapchain_present_mode::immediate,
              vulkan_swapchain_present_mode::fifo,
              vulkan_swapchain_present_mode::fifo_relaxed,
          };
    const vulkan_swapchain_present_mode desired_present_mode =
        present_mode_candidates.front();
    for (vulkan_swapchain_present_mode candidate : present_mode_candidates) {
        if (swapchain_detail::contains_present_mode(capabilities.present_modes, candidate)) {
            result.selected_present_mode = candidate;
            result.desired_present_mode_supported = candidate == desired_present_mode;
            result.fallback_present_mode_selected = candidate != desired_present_mode;
            break;
        }
    }
    if (!result.desired_present_mode_supported && !result.fallback_present_mode_selected) {
        result.selected_present_mode = capabilities.present_modes.front();
        result.fallback_present_mode_selected = result.selected_present_mode != desired_present_mode;
    }

    result.selected_extent = capabilities.current_extent.valid()
        ? capabilities.current_extent
        : swapchain_detail::clamp_extent(
              intent.desired_extent,
              capabilities.min_extent,
              capabilities.max_extent);
    result.extent_clamped =
        !capabilities.current_extent.valid()
        && !swapchain_detail::same_extent(result.selected_extent, intent.desired_extent);
    if (!result.selected_extent.valid()) {
        result.status = vulkan_swapchain_create_plan_status::unsupported_zero_extent;
        result.zero_extent_unsupported = true;
        result.diagnostic = "Vulkan swapchain create plan selected an unsupported zero extent";
        return result;
    }

    result.selected_image_count = swapchain_detail::clamp_image_count(
        intent.desired_image_count,
        capabilities.min_image_count,
        capabilities.max_image_count);
    result.min_image_count_clamped =
        intent.desired_image_count < capabilities.min_image_count;
    result.max_image_count_clamped =
        capabilities.max_image_count > 0
        && intent.desired_image_count > capabilities.max_image_count;
    result.image_count_clamped =
        result.min_image_count_clamped || result.max_image_count_clamped;

    if (capabilities.supported_transforms.empty()) {
        result.status = vulkan_swapchain_create_plan_status::missing_transform;
        result.diagnostic = "Vulkan swapchain create plan has no supported surface transforms";
        return result;
    }
    if (swapchain_detail::contains_value(
            capabilities.supported_transforms,
            intent.desired_transform)) {
        result.selected_transform = intent.desired_transform;
        result.transform_supported = true;
    } else if (swapchain_detail::contains_value(
                   capabilities.supported_transforms,
                   capabilities.current_transform)) {
        result.selected_transform = capabilities.current_transform;
        result.fallback_transform_selected = true;
    } else {
        result.selected_transform = capabilities.supported_transforms.front();
        result.fallback_transform_selected = true;
    }

    if (capabilities.supported_composite_alpha.empty()) {
        result.status = vulkan_swapchain_create_plan_status::missing_composite_alpha;
        result.diagnostic = "Vulkan swapchain create plan has no supported composite alpha";
        return result;
    }
    if (swapchain_detail::contains_value(
            capabilities.supported_composite_alpha,
            intent.desired_composite_alpha)) {
        result.selected_composite_alpha = intent.desired_composite_alpha;
        result.composite_alpha_supported = true;
    } else if (swapchain_detail::contains_value(
                   capabilities.supported_composite_alpha,
                   vulkan_swapchain_composite_alpha::opaque)) {
        result.selected_composite_alpha = vulkan_swapchain_composite_alpha::opaque;
        result.fallback_composite_alpha_selected = true;
    } else {
        result.selected_composite_alpha = capabilities.supported_composite_alpha.front();
        result.fallback_composite_alpha_selected = true;
    }

    result.sharing_concurrent =
        intent.graphics_queue_family_available && intent.present_queue_family_available
        && intent.graphics_queue_family_index != intent.present_queue_family_index;
    result.selected_sharing_mode = result.sharing_concurrent
        ? vulkan_swapchain_image_sharing_mode::concurrent
        : vulkan_swapchain_image_sharing_mode::exclusive;

    if (request.recreate_reference.checked) {
        result.recreate_extent_compatible = swapchain_detail::same_extent(
            result.selected_extent,
            request.recreate_reference.previous_extent);
        result.recreate_format_compatible = swapchain_detail::same_surface_format(
            result.selected_surface_format,
            request.recreate_reference.previous_surface_format);
        result.recreate_present_mode_compatible =
            result.selected_present_mode == request.recreate_reference.previous_present_mode;
        result.recreate_image_count_compatible =
            result.selected_image_count == request.recreate_reference.previous_image_count;
        result.recreate_compatible =
            result.recreate_extent_compatible && result.recreate_format_compatible
            && result.recreate_present_mode_compatible
            && result.recreate_image_count_compatible;
    }

    result.status = vulkan_swapchain_create_plan_status::ready;
    result.diagnostic = result.recreate_compatible
        ? "Vulkan swapchain create plan is ready"
        : "Vulkan swapchain create plan is ready but changes recreate compatibility";
    return result;
}

inline vulkan_native_swapchain_create_operation_result
build_vulkan_native_swapchain_create_operation_plan(
    const vulkan_native_swapchain_create_operation_request& request)
{
    vulkan_native_swapchain_create_operation_result result{
        .checked = true,
        .status = vulkan_native_swapchain_create_operation_status::not_checked,
        .create_plan = request.create_plan,
        .native_entrypoints = request.native_entrypoints,
        .device = request.device,
        .surface = request.surface,
        .old_swapchain = request.old_swapchain,
        .selected_extent = request.create_plan.selected_extent,
        .selected_surface_format = request.create_plan.selected_surface_format,
        .selected_present_mode = request.create_plan.selected_present_mode,
        .selected_image_count = request.create_plan.selected_image_count,
        .selected_transform = request.create_plan.selected_transform,
        .selected_composite_alpha = request.create_plan.selected_composite_alpha,
        .selected_sharing_mode = request.create_plan.selected_sharing_mode,
        .create_plan_checked = request.create_plan.checked,
        .create_plan_ready = request.create_plan.ready_for_create(),
        .native_entrypoints_checked = request.native_entrypoints.checked,
        .native_entrypoints_ready_for_create =
            request.native_entrypoints.ready_for_swapchain_create(),
        .device_valid = request.device.valid(),
        .surface_valid = request.surface.valid(),
        .required_extensions_ready = request.native_entrypoints.required_extensions_ready,
        .create_symbol_ready = request.native_entrypoints.create_swapchain_ready,
        .destroy_symbol_ready = request.native_entrypoints.destroy_swapchain_ready,
        .get_images_symbol_ready = request.native_entrypoints.get_swapchain_images_ready,
        .recreate_reference_checked = request.create_plan.recreate_reference_checked,
        .recreate_compatible = request.create_plan.recreate_compatible,
        .recreate_compatibility_required = request.require_recreate_compatibility,
        .recreate_compatibility_blocked =
            request.require_recreate_compatibility
            && !request.create_plan.recreate_compatible,
        .missing_required_extension = request.native_entrypoints.missing_required_extension,
        .missing_symbol_name = request.native_entrypoints.missing_symbol_name,
        .diagnostic = {},
        .operation = {},
    };

    if (!result.create_plan_ready) {
        result.status =
            vulkan_native_swapchain_create_operation_status::create_plan_unavailable;
        result.diagnostic = request.create_plan.diagnostic.empty()
            ? "Native Vulkan swapchain create operation is missing a ready create plan"
            : request.create_plan.diagnostic;
    } else if (!result.device_valid) {
        result.status = vulkan_native_swapchain_create_operation_status::invalid_device;
        result.diagnostic =
            "Native Vulkan swapchain create operation has no valid device handle";
    } else if (!result.surface_valid) {
        result.status = vulkan_native_swapchain_create_operation_status::invalid_surface;
        result.diagnostic =
            "Native Vulkan swapchain create operation has no valid surface handle";
    } else if (!result.native_entrypoints_checked) {
        result.status =
            vulkan_native_swapchain_create_operation_status::native_entrypoints_unavailable;
        result.diagnostic =
            "Native Vulkan swapchain create operation has unchecked native entrypoints";
    } else if (!result.required_extensions_ready) {
        result.status =
            vulkan_native_swapchain_create_operation_status::required_extension_unavailable;
        result.diagnostic = request.native_entrypoints.diagnostic.empty()
            ? "Native Vulkan swapchain create operation is missing VK_KHR_swapchain"
            : request.native_entrypoints.diagnostic;
    } else if (!result.create_symbol_ready) {
        result.status = vulkan_native_swapchain_create_operation_status::missing_create_symbol;
        result.diagnostic = request.native_entrypoints.diagnostic.empty()
            ? "Native Vulkan swapchain create operation is missing vkCreateSwapchainKHR"
            : request.native_entrypoints.diagnostic;
    } else if (!result.destroy_symbol_ready) {
        result.status = vulkan_native_swapchain_create_operation_status::missing_destroy_symbol;
        result.diagnostic = request.native_entrypoints.diagnostic.empty()
            ? "Native Vulkan swapchain create operation is missing vkDestroySwapchainKHR"
            : request.native_entrypoints.diagnostic;
    } else if (!result.get_images_symbol_ready) {
        result.status = vulkan_native_swapchain_create_operation_status::missing_images_symbol;
        result.diagnostic = request.native_entrypoints.diagnostic.empty()
            ? "Native Vulkan swapchain create operation is missing vkGetSwapchainImagesKHR"
            : request.native_entrypoints.diagnostic;
    } else if (result.recreate_compatibility_blocked) {
        result.status = vulkan_native_swapchain_create_operation_status::recreate_incompatible;
        result.diagnostic =
            "Native Vulkan swapchain create operation blocks incompatible recreate";
    } else {
        result.status = vulkan_native_swapchain_create_operation_status::ready;
        result.vk_create_swapchain_callable = true;
        result.diagnostic =
            "Native Vulkan swapchain create operation can call vkCreateSwapchainKHR";
    }

    result.operation = vulkan_native_swapchain_create_operation_summary{
        .checked = result.checked,
        .status = result.status,
        .entrypoint_name = "vkCreateSwapchainKHR",
        .vk_create_swapchain_callable = result.vk_create_swapchain_callable,
        .create_plan_ready = result.create_plan_ready,
        .native_entrypoints_ready_for_create = result.native_entrypoints_ready_for_create,
        .device_valid = result.device_valid,
        .surface_valid = result.surface_valid,
        .device = result.device,
        .surface = result.surface,
        .old_swapchain = result.old_swapchain,
        .selected_extent = result.selected_extent,
        .selected_surface_format = result.selected_surface_format,
        .selected_present_mode = result.selected_present_mode,
        .selected_image_count = result.selected_image_count,
        .selected_transform = result.selected_transform,
        .selected_composite_alpha = result.selected_composite_alpha,
        .selected_sharing_mode = result.selected_sharing_mode,
        .recreate_reference_checked = result.recreate_reference_checked,
        .recreate_compatible = result.recreate_compatible,
        .recreate_compatibility_required = result.recreate_compatibility_required,
        .recreate_compatibility_blocked = result.recreate_compatibility_blocked,
        .required_extensions_ready = result.required_extensions_ready,
        .create_symbol_ready = result.create_symbol_ready,
        .destroy_symbol_ready = result.destroy_symbol_ready,
        .get_images_symbol_ready = result.get_images_symbol_ready,
        .missing_required_extension = result.missing_required_extension,
        .missing_symbol_name = result.missing_symbol_name,
        .diagnostic = result.diagnostic,
    };

    return result;
}

inline vulkan_swapchain_image_acquire_plan_result build_vulkan_swapchain_image_acquire_plan(
    const vulkan_swapchain_image_acquire_request& request,
    const vulkan_swapchain_acquire_result& acquire)
{
    vulkan_swapchain_image_acquire_plan_result result{
        .checked = true,
        .status = vulkan_swapchain_image_acquire_plan_status::not_checked,
        .request = request,
        .acquire_status = acquire.status,
        .selected_image_index = swapchain_detail::selected_image_index_for(request, acquire),
        .image_id = acquire.image.id,
        .lifecycle_ready = request.lifecycle_ready,
        .swapchain_ready = request.swapchain_ready,
        .sync_primitives_ready =
            request.image_available_semaphore_ready && request.fence_ready,
        .acquire_requested = request.requested,
        .acquire_completed = false,
        .image_available = acquire.image.available,
        .image_acquired = acquire.image.acquired,
        .timed_out = false,
        .out_of_date = false,
        .suboptimal = false,
        .error = false,
        .diagnostic = {},
    };

    if (!request.requested) {
        result.status = vulkan_swapchain_image_acquire_plan_status::not_requested;
        result.diagnostic = "Vulkan swapchain image acquire was not requested";
        return result;
    }
    if (!request.lifecycle_ready) {
        result.status = vulkan_swapchain_image_acquire_plan_status::lifecycle_unavailable;
        result.diagnostic = "Vulkan frame lifecycle is not ready for image acquire";
        return result;
    }
    if (!request.swapchain_ready) {
        result.status = vulkan_swapchain_image_acquire_plan_status::swapchain_unavailable;
        result.diagnostic = "Vulkan swapchain is not ready for image acquire";
        return result;
    }
    if (!result.sync_primitives_ready) {
        result.status = vulkan_swapchain_image_acquire_plan_status::sync_unavailable;
        result.diagnostic = "Vulkan acquire sync primitives are unavailable";
        return result;
    }
    if (request.image_count == 0) {
        result.status = vulkan_swapchain_image_acquire_plan_status::no_images_available;
        result.diagnostic = "Vulkan swapchain has no images available for acquire";
        return result;
    }

    switch (acquire.status) {
    case vulkan_swapchain_acquire_status::not_requested:
        result.status = vulkan_swapchain_image_acquire_plan_status::not_requested;
        result.diagnostic = "Vulkan swapchain image acquire result was not requested";
        return result;
    case vulkan_swapchain_acquire_status::acquired:
        if (!acquire.completed()) {
            result.status = vulkan_swapchain_image_acquire_plan_status::backpressured;
            result.diagnostic = "Vulkan swapchain image acquire did not produce a recordable image";
            return result;
        }
        result.status = vulkan_swapchain_image_acquire_plan_status::ready;
        result.acquire_completed = true;
        result.diagnostic = "Vulkan swapchain image acquire is ready for command recording";
        return result;
    case vulkan_swapchain_acquire_status::backpressured:
        result.status = vulkan_swapchain_image_acquire_plan_status::backpressured;
        result.diagnostic = "Vulkan swapchain image acquire is backpressured";
        return result;
    case vulkan_swapchain_acquire_status::timeout:
        result.status = vulkan_swapchain_image_acquire_plan_status::timeout;
        result.timed_out = true;
        result.diagnostic = "Vulkan swapchain image acquire timed out";
        return result;
    case vulkan_swapchain_acquire_status::out_of_date:
        result.status = vulkan_swapchain_image_acquire_plan_status::out_of_date;
        result.out_of_date = true;
        result.diagnostic = "Vulkan swapchain image acquire found an out-of-date swapchain";
        return result;
    case vulkan_swapchain_acquire_status::suboptimal:
        result.suboptimal = true;
        if (!request.allow_suboptimal || !acquire.completed()) {
            result.status = vulkan_swapchain_image_acquire_plan_status::suboptimal;
            result.diagnostic =
                "Vulkan swapchain image acquire returned suboptimal without a usable image";
            return result;
        }
        result.status = vulkan_swapchain_image_acquire_plan_status::suboptimal;
        result.acquire_completed = true;
        result.diagnostic =
            "Vulkan swapchain image acquire returned suboptimal with a recordable image";
        return result;
    case vulkan_swapchain_acquire_status::failed:
    case vulkan_swapchain_acquire_status::error:
        result.status = vulkan_swapchain_image_acquire_plan_status::error;
        result.error = true;
        result.diagnostic = "Vulkan swapchain image acquire failed";
        return result;
    }

    result.status = vulkan_swapchain_image_acquire_plan_status::error;
    result.error = true;
    result.diagnostic = "Vulkan swapchain image acquire returned an unknown status";
    return result;
}

namespace swapchain_detail {

inline void mark_recreate_policy_action(
    vulkan_swapchain_recreate_policy_result& result,
    vulkan_swapchain_recreate_policy_action action,
    std::string diagnostic)
{
    result.action = action;
    result.should_keep_rendering =
        action == vulkan_swapchain_recreate_policy_action::keep_rendering;
    result.should_recreate_immediately =
        action == vulkan_swapchain_recreate_policy_action::recreate_immediately;
    result.should_recreate_after_frame =
        action == vulkan_swapchain_recreate_policy_action::recreate_after_frame;
    result.should_skip_submit =
        action == vulkan_swapchain_recreate_policy_action::skip_submit
        || action == vulkan_swapchain_recreate_policy_action::recreate_immediately;
    result.fatal = action == vulkan_swapchain_recreate_policy_action::fatal_error;
    result.diagnostic = std::move(diagnostic);
}

} // namespace swapchain_detail

inline vulkan_swapchain_recreate_policy_result evaluate_vulkan_swapchain_recreate_policy(
    const vulkan_swapchain_image_acquire_plan_result& acquire_plan,
    const vulkan_swapchain_present_result& present = {})
{
    vulkan_swapchain_recreate_policy_result result{
        .checked = acquire_plan.checked
            || present.status != vulkan_swapchain_present_status::not_requested,
        .action = vulkan_swapchain_recreate_policy_action::not_checked,
        .acquire_plan_checked = acquire_plan.checked,
        .acquire_plan_status = acquire_plan.status,
        .acquire_status = acquire_plan.acquire_status,
        .present_checked = present.status != vulkan_swapchain_present_status::not_requested,
        .present_status = present.status,
        .acquired_image_ready = acquire_plan.ready_for_command_recording(),
        .acquire_out_of_date = acquire_plan.out_of_date
            || acquire_plan.status == vulkan_swapchain_image_acquire_plan_status::out_of_date,
        .acquire_suboptimal = acquire_plan.suboptimal,
        .acquire_timed_out = acquire_plan.timed_out
            || acquire_plan.status == vulkan_swapchain_image_acquire_plan_status::timeout,
        .acquire_error = acquire_plan.error
            || acquire_plan.status == vulkan_swapchain_image_acquire_plan_status::error,
        .present_out_of_date = present.status == vulkan_swapchain_present_status::out_of_date,
        .present_suboptimal = present.status == vulkan_swapchain_present_status::suboptimal,
        .present_error = present.status == vulkan_swapchain_present_status::failed
            || present.status == vulkan_swapchain_present_status::error,
        .diagnostic = {},
    };

    if (!result.checked) {
        result.diagnostic = "Vulkan swapchain recreate policy was not checked";
        return result;
    }

    if (result.acquire_error || result.present_error) {
        swapchain_detail::mark_recreate_policy_action(
            result,
            vulkan_swapchain_recreate_policy_action::fatal_error,
            result.acquire_error
                ? "Vulkan swapchain acquire reported a fatal error"
                : "Vulkan swapchain present reported a fatal error");
        return result;
    }

    if (result.acquire_out_of_date) {
        swapchain_detail::mark_recreate_policy_action(
            result,
            vulkan_swapchain_recreate_policy_action::recreate_immediately,
            "Vulkan swapchain acquire is out of date and requires immediate recreate");
        return result;
    }

    if (result.acquire_timed_out
        || acquire_plan.status == vulkan_swapchain_image_acquire_plan_status::backpressured
        || acquire_plan.status == vulkan_swapchain_image_acquire_plan_status::no_images_available) {
        swapchain_detail::mark_recreate_policy_action(
            result,
            vulkan_swapchain_recreate_policy_action::skip_submit,
            "Vulkan swapchain acquire cannot provide an image for submit this frame");
        return result;
    }

    if (result.present_out_of_date || result.present_suboptimal || result.acquire_suboptimal) {
        swapchain_detail::mark_recreate_policy_action(
            result,
            vulkan_swapchain_recreate_policy_action::recreate_after_frame,
            result.present_out_of_date
                ? "Vulkan swapchain present is out of date and should recreate after frame"
                : "Vulkan swapchain is suboptimal and should recreate after frame");
        return result;
    }

    if (!result.acquired_image_ready
        && acquire_plan.status != vulkan_swapchain_image_acquire_plan_status::not_checked
        && acquire_plan.status != vulkan_swapchain_image_acquire_plan_status::not_requested) {
        swapchain_detail::mark_recreate_policy_action(
            result,
            vulkan_swapchain_recreate_policy_action::skip_submit,
            "Vulkan swapchain acquire is not ready, so submit should be skipped");
        return result;
    }

    swapchain_detail::mark_recreate_policy_action(
        result,
        vulkan_swapchain_recreate_policy_action::keep_rendering,
        "Vulkan swapchain can keep rendering with the current images");
    return result;
}

inline fake_vulkan_swapchain_factory::fake_vulkan_swapchain_factory()
    : fake_vulkan_swapchain_factory(fake_vulkan_swapchain_factory_options{})
{
}

inline fake_vulkan_swapchain_factory::fake_vulkan_swapchain_factory(
    fake_vulkan_swapchain_factory_options options)
    : options_(std::move(options))
{
}

inline vulkan_swapchain_create_result fake_vulkan_swapchain_factory::create_swapchain(
    const vulkan_device_create_result& device_result,
    const vulkan_swapchain_create_request& request)
{
    vulkan_swapchain_create_result result =
        swapchain_detail::make_swapchain_create_result(device_result, request);

    if (!device_result.ready_for_backend()) {
        result.status = vulkan_swapchain_create_status::device_unavailable;
        result.diagnostic = "Vulkan device is not ready for swapchain creation";
        return result;
    }
    if (!swapchain_detail::contains_queue_capability(
            device_result.selected_queues,
            vulkan_device_queue_capability::present)) {
        result.status = vulkan_swapchain_create_status::missing_present_queue;
        result.diagnostic = "Vulkan device is missing a present queue for swapchain creation";
        return result;
    }
    if (!request.requested_extent.valid()) {
        result.status = vulkan_swapchain_create_status::invalid_surface_extent;
        result.diagnostic = "Vulkan swapchain surface extent is invalid";
        return result;
    }

    result.selected_extent = swapchain_detail::clamp_extent(
        request.requested_extent,
        options_.min_extent,
        options_.max_extent);
    result.requested_present_mode_supported = swapchain_detail::contains_present_mode(
        options_.supported_present_modes,
        request.requested_present_mode);
    if (result.requested_present_mode_supported) {
        result.selected_present_mode = request.requested_present_mode;
    } else if (swapchain_detail::contains_present_mode(
                   options_.supported_present_modes,
                   vulkan_swapchain_present_mode::fifo)) {
        result.selected_present_mode = vulkan_swapchain_present_mode::fifo;
        result.fallback_to_fifo = true;
    } else {
        result.status = vulkan_swapchain_create_status::unsupported_present_mode;
        result.diagnostic = "Vulkan swapchain present mode is unsupported";
        return result;
    }

    if (options_.fail_creation || !options_.handle.valid()) {
        result.status = vulkan_swapchain_create_status::creation_failed;
        result.diagnostic = options_.fail_creation
            ? "Vulkan swapchain creation failed"
            : "Vulkan swapchain creation returned an invalid handle";
        return result;
    }

    result.status = vulkan_swapchain_create_status::created;
    result.handle = options_.handle;
    result.diagnostic = "Vulkan swapchain created";
    return result;
}

inline vulkan_swapchain_create_result create_vulkan_swapchain(
    vulkan_swapchain_factory_interface& factory,
    const vulkan_device_create_result& device_result,
    const vulkan_swapchain_create_request& request = {})
{
    return factory.create_swapchain(device_result, request);
}

} // namespace quiz_vulkan::render::vulkan_backend
