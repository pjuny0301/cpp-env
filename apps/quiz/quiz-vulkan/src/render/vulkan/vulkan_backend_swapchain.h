#pragma once

#include "render/vulkan/vulkan_backend_device.h"

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
    failed,
};

std::string_view swapchain_present_status_name(vulkan_swapchain_present_status status);

enum class vulkan_swapchain_present_mode {
    immediate,
    mailbox,
    fifo,
    fifo_relaxed,
};

std::string_view swapchain_present_mode_name(vulkan_swapchain_present_mode mode);

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
        return status == vulkan_swapchain_present_status::presented;
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
