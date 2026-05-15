#pragma once

#include "render/vulkan/vulkan_backend_swapchain.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

enum class vulkan_render_pass_attachment_role {
    color,
    depth_stencil,
};

inline std::string_view render_pass_attachment_role_name(
    vulkan_render_pass_attachment_role role)
{
    switch (role) {
    case vulkan_render_pass_attachment_role::color:
        return "color";
    case vulkan_render_pass_attachment_role::depth_stencil:
        return "depth_stencil";
    }

    return "unknown";
}

enum class vulkan_render_pass_attachment_format {
    undefined,
    rgba8_unorm,
    bgra8_unorm,
    depth24_stencil8,
};

inline std::string_view render_pass_attachment_format_name(
    vulkan_render_pass_attachment_format format)
{
    switch (format) {
    case vulkan_render_pass_attachment_format::undefined:
        return "undefined";
    case vulkan_render_pass_attachment_format::rgba8_unorm:
        return "rgba8_unorm";
    case vulkan_render_pass_attachment_format::bgra8_unorm:
        return "bgra8_unorm";
    case vulkan_render_pass_attachment_format::depth24_stencil8:
        return "depth24_stencil8";
    }

    return "unknown";
}

struct vulkan_render_pass_attachment_request {
    vulkan_render_pass_attachment_role role = vulkan_render_pass_attachment_role::color;
    vulkan_render_pass_attachment_format format =
        vulkan_render_pass_attachment_format::bgra8_unorm;
};

struct vulkan_render_pass_create_request {
    vulkan_surface_extent framebuffer_extent;
    std::vector<vulkan_render_pass_attachment_request> attachments{
        vulkan_render_pass_attachment_request{
            .role = vulkan_render_pass_attachment_role::color,
            .format = vulkan_render_pass_attachment_format::bgra8_unorm,
        },
    };
};

struct vulkan_render_pass_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_framebuffer_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

enum class vulkan_render_pass_create_status {
    not_requested,
    created,
    swapchain_unavailable,
    invalid_extent,
    invalid_format,
    missing_attachments,
    creation_failed,
};

inline std::string_view render_pass_create_status_name(
    vulkan_render_pass_create_status status)
{
    switch (status) {
    case vulkan_render_pass_create_status::not_requested:
        return "not_requested";
    case vulkan_render_pass_create_status::created:
        return "created";
    case vulkan_render_pass_create_status::swapchain_unavailable:
        return "swapchain_unavailable";
    case vulkan_render_pass_create_status::invalid_extent:
        return "invalid_extent";
    case vulkan_render_pass_create_status::invalid_format:
        return "invalid_format";
    case vulkan_render_pass_create_status::missing_attachments:
        return "missing_attachments";
    case vulkan_render_pass_create_status::creation_failed:
        return "creation_failed";
    }

    return "unknown";
}

struct vulkan_render_pass_create_result {
    bool checked = false;
    vulkan_render_pass_create_status status =
        vulkan_render_pass_create_status::not_requested;
    vulkan_swapchain_create_result swapchain;
    vulkan_render_pass_handle render_pass;
    vulkan_framebuffer_handle framebuffer;
    vulkan_surface_extent requested_extent;
    vulkan_surface_extent selected_extent;
    std::vector<vulkan_render_pass_attachment_request> requested_attachments;
    std::vector<vulkan_render_pass_attachment_request> selected_attachments;
    std::string diagnostic;

    bool ready_for_pipeline() const
    {
        return checked && status == vulkan_render_pass_create_status::created
            && swapchain.ready_for_frame() && render_pass.valid() && framebuffer.valid()
            && selected_extent.valid() && !selected_attachments.empty();
    }
};

class vulkan_render_pass_factory_interface {
public:
    virtual ~vulkan_render_pass_factory_interface() = default;

    virtual vulkan_render_pass_create_result create_render_pass(
        const vulkan_swapchain_create_result& swapchain_result,
        const vulkan_render_pass_create_request& request) = 0;
};

struct fake_vulkan_render_pass_factory_options {
    std::vector<vulkan_render_pass_attachment_format> supported_formats{
        vulkan_render_pass_attachment_format::bgra8_unorm,
        vulkan_render_pass_attachment_format::rgba8_unorm,
        vulkan_render_pass_attachment_format::depth24_stencil8,
    };
    bool fail_creation = false;
    vulkan_render_pass_handle render_pass{.value = 1};
    vulkan_framebuffer_handle framebuffer{.value = 2};
};

class fake_vulkan_render_pass_factory final : public vulkan_render_pass_factory_interface {
public:
    fake_vulkan_render_pass_factory();
    explicit fake_vulkan_render_pass_factory(fake_vulkan_render_pass_factory_options options);

    vulkan_render_pass_create_result create_render_pass(
        const vulkan_swapchain_create_result& swapchain_result,
        const vulkan_render_pass_create_request& request) override;

private:
    fake_vulkan_render_pass_factory_options options_;
};

enum class vulkan_native_framebuffer_dispatch_table_status {
    not_checked,
    ready,
    function_table_unavailable,
    missing_create_symbol,
    missing_destroy_symbol,
};

inline std::string_view native_framebuffer_dispatch_table_status_name(
    vulkan_native_framebuffer_dispatch_table_status status)
{
    switch (status) {
    case vulkan_native_framebuffer_dispatch_table_status::not_checked:
        return "not_checked";
    case vulkan_native_framebuffer_dispatch_table_status::ready:
        return "ready";
    case vulkan_native_framebuffer_dispatch_table_status::function_table_unavailable:
        return "function_table_unavailable";
    case vulkan_native_framebuffer_dispatch_table_status::missing_create_symbol:
        return "missing_create_symbol";
    case vulkan_native_framebuffer_dispatch_table_status::missing_destroy_symbol:
        return "missing_destroy_symbol";
    }

    return "unknown";
}

struct vulkan_native_framebuffer_dispatch_table {
    bool checked = false;
    vulkan_native_framebuffer_dispatch_table_status status =
        vulkan_native_framebuffer_dispatch_table_status::not_checked;
    vulkan_native_function_table_status function_table_status =
        vulkan_native_function_table_status::not_checked;
    bool function_table_checked = false;
    vulkan_native_function_pointer create_framebuffer;
    vulkan_native_function_pointer destroy_framebuffer;
    std::string missing_symbol_name;
    std::string diagnostic;

    bool ready_for_create() const
    {
        return checked && status == vulkan_native_framebuffer_dispatch_table_status::ready
            && create_framebuffer.valid() && destroy_framebuffer.valid();
    }

    bool ready_for_destroy() const
    {
        return checked && status == vulkan_native_framebuffer_dispatch_table_status::ready
            && destroy_framebuffer.valid();
    }
};

vulkan_native_framebuffer_dispatch_table collect_vulkan_native_framebuffer_dispatch_table(
    const vulkan_native_function_table_diagnostics& diagnostics);

struct vulkan_native_framebuffer_target_intent {
    std::vector<vulkan_render_pass_attachment_request> attachments{
        vulkan_render_pass_attachment_request{
            .role = vulkan_render_pass_attachment_role::color,
            .format = vulkan_render_pass_attachment_format::bgra8_unorm,
        },
    };
    std::uint32_t layers = 1;
};

enum class vulkan_native_framebuffer_target_lifecycle_status {
    not_checked,
    ready,
    image_view_unavailable,
    create_failed,
    destroyed,
    destroy_failed,
    headers_unavailable,
};

inline std::string_view native_framebuffer_target_lifecycle_status_name(
    vulkan_native_framebuffer_target_lifecycle_status status)
{
    switch (status) {
    case vulkan_native_framebuffer_target_lifecycle_status::not_checked:
        return "not_checked";
    case vulkan_native_framebuffer_target_lifecycle_status::ready:
        return "ready";
    case vulkan_native_framebuffer_target_lifecycle_status::image_view_unavailable:
        return "image_view_unavailable";
    case vulkan_native_framebuffer_target_lifecycle_status::create_failed:
        return "create_failed";
    case vulkan_native_framebuffer_target_lifecycle_status::destroyed:
        return "destroyed";
    case vulkan_native_framebuffer_target_lifecycle_status::destroy_failed:
        return "destroy_failed";
    case vulkan_native_framebuffer_target_lifecycle_status::headers_unavailable:
        return "headers_unavailable";
    }

    return "unknown";
}

struct vulkan_native_framebuffer_target {
    std::size_t image_index = 0;
    vulkan_swapchain_image_id image_id;
    vulkan_swapchain_image_view_handle image_view;
    vulkan_render_pass_handle render_pass;
    vulkan_framebuffer_handle framebuffer;
    vulkan_surface_extent extent;
    std::uint32_t layers = 1;
    std::vector<vulkan_render_pass_attachment_request> attachments;
    vulkan_native_framebuffer_target_lifecycle_status lifecycle_status =
        vulkan_native_framebuffer_target_lifecycle_status::not_checked;
    bool render_pass_ready = false;
    bool image_view_ready = false;
    bool extent_ready = false;
    bool attachment_ready = false;
    bool framebuffer_ready = false;
    bool vk_create_framebuffer_called = false;
    bool vk_destroy_framebuffer_called = false;
    std::int32_t native_result = 0;

    bool ready() const
    {
        return lifecycle_status == vulkan_native_framebuffer_target_lifecycle_status::ready
            && render_pass_ready && image_view_ready && extent_ready && attachment_ready
            && framebuffer_ready && image_id.value > 0 && image_view.valid()
            && render_pass.valid() && framebuffer.valid() && extent.valid()
            && layers > 0 && !attachments.empty();
    }
};

enum class vulkan_native_framebuffer_targets_execution_status {
    not_requested,
    ready,
    image_view_targets_unavailable,
    render_pass_unavailable,
    dispatch_table_unavailable,
    invalid_device,
    missing_render_pass,
    missing_image_view,
    missing_extent,
    extent_mismatch,
    missing_attachments,
    create_failed,
    headers_unavailable,
};

inline std::string_view native_framebuffer_targets_execution_status_name(
    vulkan_native_framebuffer_targets_execution_status status)
{
    switch (status) {
    case vulkan_native_framebuffer_targets_execution_status::not_requested:
        return "not_requested";
    case vulkan_native_framebuffer_targets_execution_status::ready:
        return "ready";
    case vulkan_native_framebuffer_targets_execution_status::image_view_targets_unavailable:
        return "image_view_targets_unavailable";
    case vulkan_native_framebuffer_targets_execution_status::render_pass_unavailable:
        return "render_pass_unavailable";
    case vulkan_native_framebuffer_targets_execution_status::dispatch_table_unavailable:
        return "dispatch_table_unavailable";
    case vulkan_native_framebuffer_targets_execution_status::invalid_device:
        return "invalid_device";
    case vulkan_native_framebuffer_targets_execution_status::missing_render_pass:
        return "missing_render_pass";
    case vulkan_native_framebuffer_targets_execution_status::missing_image_view:
        return "missing_image_view";
    case vulkan_native_framebuffer_targets_execution_status::missing_extent:
        return "missing_extent";
    case vulkan_native_framebuffer_targets_execution_status::extent_mismatch:
        return "extent_mismatch";
    case vulkan_native_framebuffer_targets_execution_status::missing_attachments:
        return "missing_attachments";
    case vulkan_native_framebuffer_targets_execution_status::create_failed:
        return "create_failed";
    case vulkan_native_framebuffer_targets_execution_status::headers_unavailable:
        return "headers_unavailable";
    }

    return "unknown";
}

struct vulkan_native_framebuffer_targets_execution_request {
    vulkan_native_swapchain_image_view_targets_execution_result image_view_targets;
    vulkan_render_pass_create_result render_pass;
    vulkan_native_framebuffer_dispatch_table dispatch_table;
    vulkan_native_framebuffer_target_intent intent;
};

struct vulkan_native_framebuffer_targets_execution_result {
    bool checked = false;
    vulkan_native_framebuffer_targets_execution_status status =
        vulkan_native_framebuffer_targets_execution_status::not_requested;
    vulkan_native_swapchain_image_view_targets_execution_result image_view_targets;
    vulkan_render_pass_create_result render_pass;
    vulkan_native_framebuffer_dispatch_table dispatch_table;
    vulkan_device_handle device;
    vulkan_render_pass_handle render_pass_handle;
    vulkan_surface_extent extent;
    std::uint32_t layers = 1;
    std::vector<vulkan_render_pass_attachment_request> attachments;
    std::size_t target_count = 0;
    std::size_t ready_framebuffer_count = 0;
    std::vector<vulkan_native_framebuffer_target> targets;
    vulkan_native_function_pointer create_framebuffer_symbol;
    vulkan_native_function_pointer destroy_framebuffer_symbol;
    bool image_view_targets_ready = false;
    bool render_pass_ready = false;
    bool dispatch_table_ready = false;
    bool extent_matches = false;
    bool vk_create_framebuffer_called = false;
    std::int32_t native_result = 0;
    std::string diagnostic;

    bool ready_for_command_recording() const
    {
        return checked && status == vulkan_native_framebuffer_targets_execution_status::ready
            && image_view_targets_ready && render_pass_ready && dispatch_table_ready
            && device.valid() && render_pass_handle.valid() && extent.valid()
            && layers > 0 && !attachments.empty() && target_count > 0
            && ready_framebuffer_count == target_count && targets.size() == target_count
            && std::all_of(
                targets.begin(),
                targets.end(),
                [](const vulkan_native_framebuffer_target& target) {
                    return target.ready();
                });
    }

    bool blocked() const
    {
        return checked && !ready_for_command_recording();
    }
};

enum class vulkan_native_framebuffer_targets_destroy_status {
    not_requested,
    destroyed,
    targets_unavailable,
    dispatch_table_unavailable,
    invalid_device,
    destroy_failed,
    headers_unavailable,
};

inline std::string_view native_framebuffer_targets_destroy_status_name(
    vulkan_native_framebuffer_targets_destroy_status status)
{
    switch (status) {
    case vulkan_native_framebuffer_targets_destroy_status::not_requested:
        return "not_requested";
    case vulkan_native_framebuffer_targets_destroy_status::destroyed:
        return "destroyed";
    case vulkan_native_framebuffer_targets_destroy_status::targets_unavailable:
        return "targets_unavailable";
    case vulkan_native_framebuffer_targets_destroy_status::dispatch_table_unavailable:
        return "dispatch_table_unavailable";
    case vulkan_native_framebuffer_targets_destroy_status::invalid_device:
        return "invalid_device";
    case vulkan_native_framebuffer_targets_destroy_status::destroy_failed:
        return "destroy_failed";
    case vulkan_native_framebuffer_targets_destroy_status::headers_unavailable:
        return "headers_unavailable";
    }

    return "unknown";
}

struct vulkan_native_framebuffer_targets_destroy_request {
    vulkan_native_framebuffer_targets_execution_result targets;
    vulkan_native_framebuffer_dispatch_table dispatch_table;
};

struct vulkan_native_framebuffer_targets_destroy_result {
    bool checked = false;
    vulkan_native_framebuffer_targets_destroy_status status =
        vulkan_native_framebuffer_targets_destroy_status::not_requested;
    vulkan_native_framebuffer_targets_execution_result targets;
    vulkan_native_framebuffer_dispatch_table dispatch_table;
    vulkan_device_handle device;
    std::size_t requested_framebuffer_count = 0;
    std::size_t destroyed_framebuffer_count = 0;
    vulkan_native_function_pointer destroy_framebuffer_symbol;
    bool targets_ready = false;
    bool dispatch_table_ready = false;
    bool vk_destroy_framebuffer_called = false;
    std::int32_t native_result = 0;
    std::string diagnostic;

    bool destroyed() const
    {
        return checked && status == vulkan_native_framebuffer_targets_destroy_status::destroyed
            && targets_ready && dispatch_table_ready
            && destroyed_framebuffer_count == requested_framebuffer_count;
    }

    bool blocked() const
    {
        return checked && !destroyed();
    }
};

class vulkan_native_framebuffer_operation_interface {
public:
    virtual ~vulkan_native_framebuffer_operation_interface() = default;

    virtual vulkan_native_framebuffer_targets_execution_result create_framebuffer_targets(
        const vulkan_native_framebuffer_targets_execution_request& request) = 0;
    virtual vulkan_native_framebuffer_targets_destroy_result destroy_framebuffer_targets(
        const vulkan_native_framebuffer_targets_destroy_request& request) = 0;
};

struct fake_vulkan_native_framebuffer_operation_options {
    std::vector<vulkan_framebuffer_handle> framebuffer_handles;
    vulkan_native_function_pointer framebuffer_handle_base{.value = 12000};
    bool fail_create = false;
    bool fail_destroy = false;
    std::int32_t create_failure_result = -1;
    std::int32_t destroy_failure_result = -2;
};

struct fake_vulkan_native_framebuffer_operation_state {
    std::size_t create_call_count = 0;
    std::size_t destroy_call_count = 0;
    vulkan_device_handle requested_device;
    vulkan_render_pass_handle requested_render_pass;
    vulkan_surface_extent requested_extent;
    std::uint32_t requested_layers = 0;
    vulkan_native_function_pointer last_create_framebuffer_symbol;
    vulkan_native_function_pointer last_destroy_framebuffer_symbol;
    std::vector<vulkan_native_framebuffer_target> created_targets;
    std::vector<vulkan_framebuffer_handle> destroyed_framebuffers;
};

class fake_vulkan_native_framebuffer_operation final
    : public vulkan_native_framebuffer_operation_interface {
public:
    fake_vulkan_native_framebuffer_operation();
    explicit fake_vulkan_native_framebuffer_operation(
        fake_vulkan_native_framebuffer_operation_options options);

    vulkan_native_framebuffer_targets_execution_result create_framebuffer_targets(
        const vulkan_native_framebuffer_targets_execution_request& request) override;
    vulkan_native_framebuffer_targets_destroy_result destroy_framebuffer_targets(
        const vulkan_native_framebuffer_targets_destroy_request& request) override;
    const fake_vulkan_native_framebuffer_operation_state& state() const;

private:
    fake_vulkan_native_framebuffer_operation_options options_;
    fake_vulkan_native_framebuffer_operation_state state_;
};

class vulkan_native_framebuffer_operation final
    : public vulkan_native_framebuffer_operation_interface {
public:
    vulkan_native_framebuffer_targets_execution_result create_framebuffer_targets(
        const vulkan_native_framebuffer_targets_execution_request& request) override;
    vulkan_native_framebuffer_targets_destroy_result destroy_framebuffer_targets(
        const vulkan_native_framebuffer_targets_destroy_request& request) override;
};

vulkan_native_framebuffer_targets_execution_result
create_native_vulkan_framebuffer_targets(
    vulkan_native_framebuffer_operation_interface& operation,
    const vulkan_native_framebuffer_targets_execution_request& request);

vulkan_native_framebuffer_targets_destroy_result
destroy_native_vulkan_framebuffer_targets(
    vulkan_native_framebuffer_operation_interface& operation,
    const vulkan_native_framebuffer_targets_destroy_request& request);

namespace render_pass_detail {

inline bool same_extent(vulkan_surface_extent lhs, vulkan_surface_extent rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height;
}

inline bool contains_attachment_format(
    const std::vector<vulkan_render_pass_attachment_format>& formats,
    vulkan_render_pass_attachment_format format)
{
    return std::find(formats.begin(), formats.end(), format) != formats.end();
}

inline bool has_color_attachment(
    const std::vector<vulkan_render_pass_attachment_request>& attachments)
{
    return std::find_if(
        attachments.begin(),
        attachments.end(),
        [](const vulkan_render_pass_attachment_request& attachment) {
            return attachment.role == vulkan_render_pass_attachment_role::color;
        }) != attachments.end();
}

inline vulkan_render_pass_create_result make_render_pass_create_result(
    const vulkan_swapchain_create_result& swapchain_result,
    const vulkan_render_pass_create_request& request)
{
    return vulkan_render_pass_create_result{
        .checked = true,
        .status = vulkan_render_pass_create_status::not_requested,
        .swapchain = swapchain_result,
        .render_pass = {},
        .framebuffer = {},
        .requested_extent = request.framebuffer_extent,
        .selected_extent = {},
        .requested_attachments = request.attachments,
        .selected_attachments = {},
        .diagnostic = {},
    };
}

inline std::string make_invalid_attachment_format_diagnostic(
    vulkan_render_pass_attachment_format format)
{
    return "Vulkan render pass attachment format is invalid: "
        + std::string{render_pass_attachment_format_name(format)};
}

inline std::vector<vulkan_framebuffer_handle> make_framebuffer_handles(
    std::size_t target_count,
    vulkan_native_function_pointer framebuffer_handle_base)
{
    const std::uintptr_t base =
        framebuffer_handle_base.valid() ? framebuffer_handle_base.value : 5000;
    std::vector<vulkan_framebuffer_handle> handles;
    handles.reserve(target_count);
    for (std::size_t target_index = 0; target_index < target_count; ++target_index) {
        handles.push_back(vulkan_framebuffer_handle{
            .value = base + target_index + 1,
        });
    }
    return handles;
}

inline vulkan_native_framebuffer_target make_framebuffer_target(
    const vulkan_native_swapchain_image_view_target& image_view_target,
    vulkan_render_pass_handle render_pass,
    vulkan_surface_extent extent,
    std::uint32_t layers,
    std::vector<vulkan_render_pass_attachment_request> attachments)
{
    const bool image_view_ready = image_view_target.ready() && image_view_target.image_view.valid();
    const bool attachment_ready = layers > 0 && !attachments.empty();
    return vulkan_native_framebuffer_target{
        .image_index = image_view_target.image_index,
        .image_id = image_view_target.image_id,
        .image_view = image_view_target.image_view,
        .render_pass = render_pass,
        .framebuffer = {},
        .extent = extent,
        .layers = layers,
        .attachments = std::move(attachments),
        .lifecycle_status = image_view_ready
            ? vulkan_native_framebuffer_target_lifecycle_status::not_checked
            : vulkan_native_framebuffer_target_lifecycle_status::image_view_unavailable,
        .render_pass_ready = render_pass.valid(),
        .image_view_ready = image_view_ready,
        .extent_ready = extent.valid(),
        .attachment_ready = attachment_ready,
        .framebuffer_ready = false,
        .vk_create_framebuffer_called = false,
        .vk_destroy_framebuffer_called = false,
        .native_result = 0,
    };
}

inline vulkan_native_framebuffer_targets_execution_result
make_framebuffer_targets_execution_result(
    const vulkan_native_framebuffer_targets_execution_request& request)
{
    return vulkan_native_framebuffer_targets_execution_result{
        .checked = true,
        .status = vulkan_native_framebuffer_targets_execution_status::not_requested,
        .image_view_targets = request.image_view_targets,
        .render_pass = request.render_pass,
        .dispatch_table = request.dispatch_table,
        .device = request.image_view_targets.device,
        .render_pass_handle = request.render_pass.render_pass,
        .extent = request.image_view_targets.extent,
        .layers = request.intent.layers,
        .attachments = request.intent.attachments,
        .target_count = request.image_view_targets.targets.size(),
        .ready_framebuffer_count = 0,
        .targets = {},
        .create_framebuffer_symbol = request.dispatch_table.create_framebuffer,
        .destroy_framebuffer_symbol = request.dispatch_table.destroy_framebuffer,
        .image_view_targets_ready = request.image_view_targets.checked
            && request.image_view_targets.status
                == vulkan_native_swapchain_image_view_targets_execution_status::ready
            && !request.image_view_targets.targets.empty(),
        .render_pass_ready = request.render_pass.checked
            && request.render_pass.status == vulkan_render_pass_create_status::created
            && request.render_pass.render_pass.valid(),
        .dispatch_table_ready = request.dispatch_table.ready_for_create(),
        .extent_matches = same_extent(
            request.image_view_targets.extent,
            request.render_pass.selected_extent),
        .vk_create_framebuffer_called = false,
        .native_result = 0,
        .diagnostic = {},
    };
}

inline vulkan_native_framebuffer_targets_destroy_result
make_framebuffer_targets_destroy_result(
    const vulkan_native_framebuffer_targets_destroy_request& request)
{
    return vulkan_native_framebuffer_targets_destroy_result{
        .checked = true,
        .status = vulkan_native_framebuffer_targets_destroy_status::not_requested,
        .targets = request.targets,
        .dispatch_table = request.dispatch_table,
        .device = request.targets.device,
        .requested_framebuffer_count = request.targets.targets.size(),
        .destroyed_framebuffer_count = 0,
        .destroy_framebuffer_symbol = request.dispatch_table.destroy_framebuffer,
        .targets_ready = request.targets.ready_for_command_recording(),
        .dispatch_table_ready = request.dispatch_table.ready_for_destroy(),
        .vk_destroy_framebuffer_called = false,
        .native_result = 0,
        .diagnostic = {},
    };
}

} // namespace render_pass_detail

inline fake_vulkan_render_pass_factory::fake_vulkan_render_pass_factory()
    : fake_vulkan_render_pass_factory(fake_vulkan_render_pass_factory_options{})
{
}

inline fake_vulkan_render_pass_factory::fake_vulkan_render_pass_factory(
    fake_vulkan_render_pass_factory_options options)
    : options_(std::move(options))
{
}

inline vulkan_render_pass_create_result fake_vulkan_render_pass_factory::create_render_pass(
    const vulkan_swapchain_create_result& swapchain_result,
    const vulkan_render_pass_create_request& request)
{
    vulkan_render_pass_create_result result =
        render_pass_detail::make_render_pass_create_result(swapchain_result, request);

    if (!swapchain_result.ready_for_frame()) {
        result.status = vulkan_render_pass_create_status::swapchain_unavailable;
        result.diagnostic = "Vulkan swapchain is not ready for render pass creation";
        return result;
    }
    if (!request.framebuffer_extent.valid()
        || !render_pass_detail::same_extent(
            request.framebuffer_extent,
            swapchain_result.selected_extent)) {
        result.status = vulkan_render_pass_create_status::invalid_extent;
        result.diagnostic = "Vulkan framebuffer extent is invalid";
        return result;
    }
    if (request.attachments.empty()
        || !render_pass_detail::has_color_attachment(request.attachments)) {
        result.status = vulkan_render_pass_create_status::missing_attachments;
        result.diagnostic = "Vulkan render pass is missing a color attachment";
        return result;
    }

    for (const vulkan_render_pass_attachment_request& attachment : request.attachments) {
        if (attachment.format == vulkan_render_pass_attachment_format::undefined
            || !render_pass_detail::contains_attachment_format(
                options_.supported_formats,
                attachment.format)) {
            result.status = vulkan_render_pass_create_status::invalid_format;
            result.diagnostic =
                render_pass_detail::make_invalid_attachment_format_diagnostic(attachment.format);
            return result;
        }
    }

    if (options_.fail_creation || !options_.render_pass.valid()
        || !options_.framebuffer.valid()) {
        result.status = vulkan_render_pass_create_status::creation_failed;
        result.diagnostic = options_.fail_creation
            ? "Vulkan render pass/framebuffer creation failed"
            : "Vulkan render pass/framebuffer creation returned an invalid handle";
        return result;
    }

    result.status = vulkan_render_pass_create_status::created;
    result.render_pass = options_.render_pass;
    result.framebuffer = options_.framebuffer;
    result.selected_extent = request.framebuffer_extent;
    result.selected_attachments = request.attachments;
    result.diagnostic = "Vulkan render pass and framebuffer created";
    return result;
}

inline vulkan_native_framebuffer_dispatch_table collect_vulkan_native_framebuffer_dispatch_table(
    const vulkan_native_function_table_diagnostics& diagnostics)
{
    vulkan_native_framebuffer_dispatch_table dispatch{
        .checked = true,
        .status = vulkan_native_framebuffer_dispatch_table_status::not_checked,
        .function_table_status = diagnostics.status,
        .function_table_checked = diagnostics.checked,
        .create_framebuffer = swapchain_detail::find_native_symbol_pointer(
            diagnostics,
            vulkan_native_entrypoint_stage::framebuffer_create),
        .destroy_framebuffer = swapchain_detail::find_native_symbol_pointer(
            diagnostics,
            vulkan_native_entrypoint_stage::framebuffer_destroy),
        .missing_symbol_name = diagnostics.missing_symbol_name,
        .diagnostic = {},
    };

    if (!diagnostics.checked) {
        dispatch.status =
            vulkan_native_framebuffer_dispatch_table_status::function_table_unavailable;
        dispatch.diagnostic = diagnostics.diagnostic.empty()
            ? "Native Vulkan framebuffer dispatch table has unchecked function diagnostics"
            : diagnostics.diagnostic;
        return dispatch;
    }
    if (!dispatch.create_framebuffer.valid()) {
        dispatch.status =
            vulkan_native_framebuffer_dispatch_table_status::missing_create_symbol;
        dispatch.missing_symbol_name = dispatch.missing_symbol_name.empty()
            ? "vkCreateFramebuffer"
            : dispatch.missing_symbol_name;
        dispatch.diagnostic = diagnostics.diagnostic.empty()
            ? "Native Vulkan framebuffer dispatch table is missing vkCreateFramebuffer"
            : diagnostics.diagnostic;
        return dispatch;
    }
    if (!dispatch.destroy_framebuffer.valid()) {
        dispatch.status =
            vulkan_native_framebuffer_dispatch_table_status::missing_destroy_symbol;
        dispatch.missing_symbol_name = dispatch.missing_symbol_name.empty()
            ? "vkDestroyFramebuffer"
            : dispatch.missing_symbol_name;
        dispatch.diagnostic = diagnostics.diagnostic.empty()
            ? "Native Vulkan framebuffer dispatch table is missing vkDestroyFramebuffer"
            : diagnostics.diagnostic;
        return dispatch;
    }
    if (diagnostics.status != vulkan_native_function_table_status::ready) {
        dispatch.status =
            vulkan_native_framebuffer_dispatch_table_status::function_table_unavailable;
        dispatch.diagnostic = diagnostics.diagnostic.empty()
            ? "Native Vulkan framebuffer dispatch table has unavailable function diagnostics"
            : diagnostics.diagnostic;
        return dispatch;
    }

    dispatch.status = vulkan_native_framebuffer_dispatch_table_status::ready;
    dispatch.diagnostic = "Native Vulkan framebuffer dispatch table is ready";
    return dispatch;
}

inline fake_vulkan_native_framebuffer_operation::fake_vulkan_native_framebuffer_operation()
    : fake_vulkan_native_framebuffer_operation(
        fake_vulkan_native_framebuffer_operation_options{})
{
}

inline fake_vulkan_native_framebuffer_operation::fake_vulkan_native_framebuffer_operation(
    fake_vulkan_native_framebuffer_operation_options options)
    : options_(std::move(options))
{
}

inline vulkan_native_framebuffer_targets_execution_result
fake_vulkan_native_framebuffer_operation::create_framebuffer_targets(
    const vulkan_native_framebuffer_targets_execution_request& request)
{
    vulkan_native_framebuffer_targets_execution_result result =
        render_pass_detail::make_framebuffer_targets_execution_result(request);

    if (!result.image_view_targets_ready) {
        result.status =
            vulkan_native_framebuffer_targets_execution_status::image_view_targets_unavailable;
        result.diagnostic = request.image_view_targets.diagnostic.empty()
            ? "Native Vulkan framebuffer target creation is missing ready image views"
            : request.image_view_targets.diagnostic;
        return result;
    }
    if (!request.render_pass.checked
        || request.render_pass.status != vulkan_render_pass_create_status::created) {
        result.status =
            vulkan_native_framebuffer_targets_execution_status::render_pass_unavailable;
        result.diagnostic = request.render_pass.diagnostic.empty()
            ? "Native Vulkan framebuffer target creation is missing a created render pass"
            : request.render_pass.diagnostic;
        return result;
    }
    if (!result.dispatch_table_ready) {
        result.status =
            vulkan_native_framebuffer_targets_execution_status::dispatch_table_unavailable;
        result.diagnostic = request.dispatch_table.diagnostic.empty()
            ? "Native Vulkan framebuffer target creation is missing framebuffer dispatch"
            : request.dispatch_table.diagnostic;
        return result;
    }
    if (!result.device.valid()) {
        result.status = vulkan_native_framebuffer_targets_execution_status::invalid_device;
        result.diagnostic =
            "Native Vulkan framebuffer target creation has no valid device handle";
        return result;
    }
    if (!result.render_pass_handle.valid()) {
        result.status =
            vulkan_native_framebuffer_targets_execution_status::missing_render_pass;
        result.diagnostic =
            "Native Vulkan framebuffer target creation has no valid render pass handle";
        return result;
    }
    if (!result.extent.valid()) {
        result.status = vulkan_native_framebuffer_targets_execution_status::missing_extent;
        result.diagnostic =
            "Native Vulkan framebuffer target creation has no valid framebuffer extent";
        return result;
    }
    if (!result.extent_matches) {
        result.status = vulkan_native_framebuffer_targets_execution_status::extent_mismatch;
        result.diagnostic =
            "Native Vulkan framebuffer target extent does not match the render pass extent";
        return result;
    }
    if (result.layers == 0 || result.attachments.empty()) {
        result.status = vulkan_native_framebuffer_targets_execution_status::missing_attachments;
        result.diagnostic =
            "Native Vulkan framebuffer target creation is missing attachment intent";
        return result;
    }

    ++state_.create_call_count;
    state_.requested_device = result.device;
    state_.requested_render_pass = result.render_pass_handle;
    state_.requested_extent = result.extent;
    state_.requested_layers = result.layers;
    state_.last_create_framebuffer_symbol = result.create_framebuffer_symbol;
    state_.last_destroy_framebuffer_symbol = result.destroy_framebuffer_symbol;
    result.vk_create_framebuffer_called = true;

    if (options_.fail_create) {
        result.status = vulkan_native_framebuffer_targets_execution_status::create_failed;
        result.native_result = options_.create_failure_result;
        result.diagnostic = "Native Vulkan framebuffer target creation failed";
        return result;
    }

    std::vector<vulkan_framebuffer_handle> framebuffers =
        options_.framebuffer_handles.empty()
        ? render_pass_detail::make_framebuffer_handles(
              request.image_view_targets.targets.size(),
              options_.framebuffer_handle_base)
        : options_.framebuffer_handles;
    if (framebuffers.size() != request.image_view_targets.targets.size()) {
        result.status = vulkan_native_framebuffer_targets_execution_status::create_failed;
        result.diagnostic =
            "Native Vulkan framebuffer target creation has mismatched framebuffer evidence";
        return result;
    }

    result.targets.reserve(request.image_view_targets.targets.size());
    for (std::size_t target_index = 0; target_index < request.image_view_targets.targets.size();
         ++target_index) {
        vulkan_native_framebuffer_target target =
            render_pass_detail::make_framebuffer_target(
                request.image_view_targets.targets[target_index],
                result.render_pass_handle,
                result.extent,
                result.layers,
                result.attachments);
        if (!target.image_view_ready) {
            result.status = vulkan_native_framebuffer_targets_execution_status::missing_image_view;
            result.targets.push_back(target);
            result.diagnostic =
                "Native Vulkan framebuffer target creation found a missing image view";
            return result;
        }
        target.framebuffer = framebuffers[target_index];
        target.framebuffer_ready = target.framebuffer.valid();
        target.vk_create_framebuffer_called = true;
        target.lifecycle_status = target.framebuffer_ready
            ? vulkan_native_framebuffer_target_lifecycle_status::ready
            : vulkan_native_framebuffer_target_lifecycle_status::create_failed;
        if (!target.ready()) {
            result.status = vulkan_native_framebuffer_targets_execution_status::create_failed;
            result.targets.push_back(target);
            result.diagnostic =
                "Native Vulkan framebuffer target creation produced an invalid framebuffer";
            return result;
        }
        result.targets.push_back(target);
    }

    result.ready_framebuffer_count = result.targets.size();
    state_.created_targets = result.targets;
    result.status = vulkan_native_framebuffer_targets_execution_status::ready;
    result.diagnostic = "Native Vulkan framebuffer targets are ready";
    return result;
}

inline vulkan_native_framebuffer_targets_destroy_result
fake_vulkan_native_framebuffer_operation::destroy_framebuffer_targets(
    const vulkan_native_framebuffer_targets_destroy_request& request)
{
    vulkan_native_framebuffer_targets_destroy_result result =
        render_pass_detail::make_framebuffer_targets_destroy_result(request);

    if (!result.targets_ready) {
        result.status = vulkan_native_framebuffer_targets_destroy_status::targets_unavailable;
        result.diagnostic = request.targets.diagnostic.empty()
            ? "Native Vulkan framebuffer target destroy is missing ready framebuffers"
            : request.targets.diagnostic;
        return result;
    }
    if (!result.dispatch_table_ready) {
        result.status =
            vulkan_native_framebuffer_targets_destroy_status::dispatch_table_unavailable;
        result.diagnostic = request.dispatch_table.diagnostic.empty()
            ? "Native Vulkan framebuffer target destroy is missing destroy dispatch"
            : request.dispatch_table.diagnostic;
        return result;
    }
    if (!result.device.valid()) {
        result.status = vulkan_native_framebuffer_targets_destroy_status::invalid_device;
        result.diagnostic =
            "Native Vulkan framebuffer target destroy has no valid device handle";
        return result;
    }

    ++state_.destroy_call_count;
    state_.requested_device = result.device;
    state_.last_destroy_framebuffer_symbol = result.destroy_framebuffer_symbol;
    result.vk_destroy_framebuffer_called = true;

    if (options_.fail_destroy) {
        result.status = vulkan_native_framebuffer_targets_destroy_status::destroy_failed;
        result.native_result = options_.destroy_failure_result;
        result.diagnostic = "Native Vulkan framebuffer target destroy failed";
        return result;
    }

    result.destroyed_framebuffer_count = request.targets.targets.size();
    state_.destroyed_framebuffers.clear();
    for (vulkan_native_framebuffer_target& target : result.targets.targets) {
        state_.destroyed_framebuffers.push_back(target.framebuffer);
        target.vk_destroy_framebuffer_called = true;
        target.lifecycle_status = vulkan_native_framebuffer_target_lifecycle_status::destroyed;
    }
    result.status = vulkan_native_framebuffer_targets_destroy_status::destroyed;
    result.diagnostic = "Native Vulkan framebuffer targets destroyed";
    return result;
}

inline const fake_vulkan_native_framebuffer_operation_state&
fake_vulkan_native_framebuffer_operation::state() const
{
    return state_;
}

inline vulkan_native_framebuffer_targets_execution_result
create_native_vulkan_framebuffer_targets(
    vulkan_native_framebuffer_operation_interface& operation,
    const vulkan_native_framebuffer_targets_execution_request& request)
{
    return operation.create_framebuffer_targets(request);
}

inline vulkan_native_framebuffer_targets_destroy_result
destroy_native_vulkan_framebuffer_targets(
    vulkan_native_framebuffer_operation_interface& operation,
    const vulkan_native_framebuffer_targets_destroy_request& request)
{
    return operation.destroy_framebuffer_targets(request);
}

inline vulkan_render_pass_create_result create_vulkan_render_pass(
    vulkan_render_pass_factory_interface& factory,
    const vulkan_swapchain_create_result& swapchain_result,
    const vulkan_render_pass_create_request& request = {})
{
    return factory.create_render_pass(swapchain_result, request);
}

} // namespace quiz_vulkan::render::vulkan_backend
