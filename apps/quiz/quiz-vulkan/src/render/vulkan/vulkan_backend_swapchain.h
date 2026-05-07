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
    failed,
};

std::string_view swapchain_acquire_status_name(vulkan_swapchain_acquire_status status);

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

struct vulkan_swapchain_acquire_result {
    vulkan_swapchain_acquire_status status = vulkan_swapchain_acquire_status::not_requested;
    vulkan_swapchain_image_state image;

    bool completed() const
    {
        return status == vulkan_swapchain_acquire_status::acquired && image.ready_for_recording();
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

struct vulkan_swapchain_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
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

} // namespace swapchain_detail

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
