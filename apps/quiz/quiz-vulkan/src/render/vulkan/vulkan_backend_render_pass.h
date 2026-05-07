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

inline vulkan_render_pass_create_result create_vulkan_render_pass(
    vulkan_render_pass_factory_interface& factory,
    const vulkan_swapchain_create_result& swapchain_result,
    const vulkan_render_pass_create_request& request = {})
{
    return factory.create_render_pass(swapchain_result, request);
}

} // namespace quiz_vulkan::render::vulkan_backend
