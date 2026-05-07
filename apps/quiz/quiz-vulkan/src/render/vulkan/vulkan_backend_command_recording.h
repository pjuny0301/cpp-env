#pragma once

#include "render/vulkan/vulkan_backend_render_pass.h"
#include "render/vulkan/vulkan_frame_plan.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

inline std::string_view command_recording_batch_kind_name(vulkan_batch_kind kind)
{
    switch (kind) {
    case vulkan_batch_kind::quad:
        return "quad";
    case vulkan_batch_kind::text:
        return "text";
    case vulkan_batch_kind::image:
        return "image";
    case vulkan_batch_kind::debug_bounds:
        return "debug_bounds";
    }

    return "unknown";
}

struct vulkan_command_recording_command_buffer_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

enum class vulkan_command_recording_readiness_status {
    not_requested,
    ready,
    render_pass_unavailable,
    framebuffer_unavailable,
    pipeline_incompatible,
    command_buffer_unavailable,
    unsupported_draw_batch,
};

inline std::string_view command_recording_readiness_status_name(
    vulkan_command_recording_readiness_status status)
{
    switch (status) {
    case vulkan_command_recording_readiness_status::not_requested:
        return "not_requested";
    case vulkan_command_recording_readiness_status::ready:
        return "ready";
    case vulkan_command_recording_readiness_status::render_pass_unavailable:
        return "render_pass_unavailable";
    case vulkan_command_recording_readiness_status::framebuffer_unavailable:
        return "framebuffer_unavailable";
    case vulkan_command_recording_readiness_status::pipeline_incompatible:
        return "pipeline_incompatible";
    case vulkan_command_recording_readiness_status::command_buffer_unavailable:
        return "command_buffer_unavailable";
    case vulkan_command_recording_readiness_status::unsupported_draw_batch:
        return "unsupported_draw_batch";
    }

    return "unknown";
}

struct vulkan_command_recording_pipeline_attachment_compatibility {
    vulkan_render_pass_attachment_role role = vulkan_render_pass_attachment_role::color;
    vulkan_render_pass_attachment_format format =
        vulkan_render_pass_attachment_format::undefined;
    bool compatible = false;

    bool completed() const
    {
        return compatible && format != vulkan_render_pass_attachment_format::undefined;
    }
};

struct vulkan_command_recording_draw_batch_compatibility {
    vulkan_batch_kind kind = vulkan_batch_kind::quad;
    std::size_t command_index = 0;
    bool supported = false;
    bool clipped = false;

    bool completed() const
    {
        return supported;
    }
};

struct vulkan_command_recording_readiness_request {
    vulkan_frame_plan frame_plan;
};

struct vulkan_command_recording_readiness_result {
    bool checked = false;
    vulkan_command_recording_readiness_status status =
        vulkan_command_recording_readiness_status::not_requested;
    vulkan_render_pass_create_result render_pass;
    vulkan_command_recording_command_buffer_handle command_buffer;
    std::size_t planned_batch_count = 0;
    std::size_t recordable_batch_count = 0;
    std::size_t clipped_draw_call_count = 0;
    std::size_t discarded_draw_call_count = 0;
    bool empty_draw_plan = false;
    bool render_pass_available = false;
    bool framebuffer_available = false;
    bool pipeline_compatible = false;
    bool command_buffer_available = false;
    vulkan_batch_kind unsupported_batch_kind = vulkan_batch_kind::quad;
    std::size_t unsupported_command_index = 0;
    std::vector<vulkan_command_recording_pipeline_attachment_compatibility>
        attachment_compatibility;
    std::vector<vulkan_command_recording_draw_batch_compatibility> batch_compatibility;
    std::string diagnostic;

    bool render_pass_ready() const
    {
        return checked && render_pass_available && render_pass.render_pass.valid();
    }

    bool framebuffer_ready() const
    {
        return checked && framebuffer_available && render_pass.framebuffer.valid()
            && render_pass.selected_extent.valid();
    }

    bool pipeline_ready() const
    {
        return checked && pipeline_compatible;
    }

    bool command_buffer_ready() const
    {
        return checked && command_buffer_available && command_buffer.valid();
    }

    bool ready_for_recording() const
    {
        return checked && status == vulkan_command_recording_readiness_status::ready
            && render_pass_ready() && framebuffer_ready() && pipeline_ready()
            && command_buffer_ready() && recordable_batch_count == planned_batch_count;
    }
};

class vulkan_command_recording_readiness_factory_interface {
public:
    virtual ~vulkan_command_recording_readiness_factory_interface() = default;

    virtual vulkan_command_recording_readiness_result check_command_recording_readiness(
        const vulkan_render_pass_create_result& render_pass_result,
        const vulkan_command_recording_readiness_request& request) = 0;
};

struct fake_vulkan_command_recording_readiness_factory_options {
    std::vector<vulkan_batch_kind> supported_batch_kinds{
        vulkan_batch_kind::quad,
        vulkan_batch_kind::text,
        vulkan_batch_kind::image,
        vulkan_batch_kind::debug_bounds,
    };
    std::vector<vulkan_render_pass_attachment_format> compatible_attachment_formats{
        vulkan_render_pass_attachment_format::bgra8_unorm,
        vulkan_render_pass_attachment_format::rgba8_unorm,
        vulkan_render_pass_attachment_format::depth24_stencil8,
    };
    bool command_buffer_available = true;
    vulkan_command_recording_command_buffer_handle command_buffer{.value = 1};
};

class fake_vulkan_command_recording_readiness_factory final
    : public vulkan_command_recording_readiness_factory_interface {
public:
    fake_vulkan_command_recording_readiness_factory();
    explicit fake_vulkan_command_recording_readiness_factory(
        fake_vulkan_command_recording_readiness_factory_options options);

    vulkan_command_recording_readiness_result check_command_recording_readiness(
        const vulkan_render_pass_create_result& render_pass_result,
        const vulkan_command_recording_readiness_request& request) override;

private:
    fake_vulkan_command_recording_readiness_factory_options options_;
};

namespace command_recording_detail {

inline bool contains_batch_kind(
    const std::vector<vulkan_batch_kind>& kinds,
    vulkan_batch_kind kind)
{
    return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
}

inline bool contains_attachment_format(
    const std::vector<vulkan_render_pass_attachment_format>& formats,
    vulkan_render_pass_attachment_format format)
{
    return std::find(formats.begin(), formats.end(), format) != formats.end();
}

inline vulkan_command_recording_readiness_result make_command_recording_readiness_result(
    const vulkan_render_pass_create_result& render_pass_result,
    const vulkan_command_recording_readiness_request& request)
{
    return vulkan_command_recording_readiness_result{
        .checked = true,
        .status = vulkan_command_recording_readiness_status::not_requested,
        .render_pass = render_pass_result,
        .command_buffer = {},
        .planned_batch_count = request.frame_plan.batches.size(),
        .recordable_batch_count = 0,
        .clipped_draw_call_count = request.frame_plan.clipped_draw_call_count,
        .discarded_draw_call_count = request.frame_plan.discarded_draw_call_count,
        .empty_draw_plan = request.frame_plan.batches.empty(),
        .render_pass_available = false,
        .framebuffer_available = false,
        .pipeline_compatible = false,
        .command_buffer_available = false,
        .unsupported_batch_kind = vulkan_batch_kind::quad,
        .unsupported_command_index = 0,
        .attachment_compatibility = {},
        .batch_compatibility = {},
        .diagnostic = {},
    };
}

inline std::string make_pipeline_incompatible_diagnostic(
    vulkan_render_pass_attachment_format format)
{
    return "Vulkan command recording pipeline is incompatible with selected attachment format: "
        + std::string{render_pass_attachment_format_name(format)};
}

inline std::string make_unsupported_batch_diagnostic(vulkan_batch_kind kind)
{
    return "Vulkan command recording pipeline does not support draw batch: "
        + std::string{command_recording_batch_kind_name(kind)};
}

} // namespace command_recording_detail

inline fake_vulkan_command_recording_readiness_factory::
    fake_vulkan_command_recording_readiness_factory()
    : fake_vulkan_command_recording_readiness_factory(
        fake_vulkan_command_recording_readiness_factory_options{})
{
}

inline fake_vulkan_command_recording_readiness_factory::
    fake_vulkan_command_recording_readiness_factory(
        fake_vulkan_command_recording_readiness_factory_options options)
    : options_(std::move(options))
{
}

inline vulkan_command_recording_readiness_result
fake_vulkan_command_recording_readiness_factory::check_command_recording_readiness(
    const vulkan_render_pass_create_result& render_pass_result,
    const vulkan_command_recording_readiness_request& request)
{
    vulkan_command_recording_readiness_result result =
        command_recording_detail::make_command_recording_readiness_result(
            render_pass_result,
            request);

    result.render_pass_available =
        render_pass_result.checked
        && render_pass_result.status == vulkan_render_pass_create_status::created
        && render_pass_result.render_pass.valid();
    if (!result.render_pass_available) {
        result.status = vulkan_command_recording_readiness_status::render_pass_unavailable;
        result.diagnostic = "Vulkan render pass is not ready for command recording";
        return result;
    }

    result.framebuffer_available =
        render_pass_result.framebuffer.valid() && render_pass_result.selected_extent.valid();
    if (!result.framebuffer_available) {
        result.status = vulkan_command_recording_readiness_status::framebuffer_unavailable;
        result.diagnostic = "Vulkan framebuffer is not available for command recording";
        return result;
    }

    if (render_pass_result.selected_attachments.empty()) {
        result.pipeline_compatible = false;
        result.status = vulkan_command_recording_readiness_status::pipeline_incompatible;
        result.diagnostic =
            "Vulkan command recording pipeline has no selected render pass attachments";
        return result;
    }

    result.attachment_compatibility.reserve(render_pass_result.selected_attachments.size());
    result.pipeline_compatible = true;
    for (const vulkan_render_pass_attachment_request& attachment :
         render_pass_result.selected_attachments) {
        const bool compatible = command_recording_detail::contains_attachment_format(
            options_.compatible_attachment_formats,
            attachment.format);
        result.attachment_compatibility.push_back(
            vulkan_command_recording_pipeline_attachment_compatibility{
                .role = attachment.role,
                .format = attachment.format,
                .compatible = compatible,
            });
        if (!compatible) {
            result.pipeline_compatible = false;
            result.status = vulkan_command_recording_readiness_status::pipeline_incompatible;
            result.diagnostic =
                command_recording_detail::make_pipeline_incompatible_diagnostic(attachment.format);
            return result;
        }
    }

    result.batch_compatibility.reserve(request.frame_plan.batches.size());
    for (const vulkan_draw_batch& batch : request.frame_plan.batches) {
        const bool supported = command_recording_detail::contains_batch_kind(
            options_.supported_batch_kinds,
            batch.kind);
        result.batch_compatibility.push_back(
            vulkan_command_recording_draw_batch_compatibility{
                .kind = batch.kind,
                .command_index = batch.command_index,
                .supported = supported,
                .clipped = batch.scissor.empty(),
            });
        if (!supported) {
            result.pipeline_compatible = false;
            result.status = vulkan_command_recording_readiness_status::unsupported_draw_batch;
            result.unsupported_batch_kind = batch.kind;
            result.unsupported_command_index = batch.command_index;
            result.diagnostic =
                command_recording_detail::make_unsupported_batch_diagnostic(batch.kind);
            return result;
        }
    }

    result.command_buffer_available =
        options_.command_buffer_available && options_.command_buffer.valid();
    if (!result.command_buffer_available) {
        result.status = vulkan_command_recording_readiness_status::command_buffer_unavailable;
        result.diagnostic = "Vulkan command buffer is not available for recording";
        return result;
    }

    result.status = vulkan_command_recording_readiness_status::ready;
    result.command_buffer = options_.command_buffer;
    result.recordable_batch_count = request.frame_plan.batches.size();
    if (result.empty_draw_plan) {
        result.diagnostic = "Vulkan command recording is ready for an empty draw plan";
    } else if (result.clipped_draw_call_count > 0) {
        result.diagnostic = "Vulkan command recording is ready with clipped draw batches";
    } else {
        result.diagnostic = "Vulkan command recording is ready";
    }
    return result;
}

inline vulkan_command_recording_readiness_result check_vulkan_command_recording_readiness(
    vulkan_command_recording_readiness_factory_interface& factory,
    const vulkan_render_pass_create_result& render_pass_result,
    const vulkan_command_recording_readiness_request& request = {})
{
    return factory.check_command_recording_readiness(render_pass_result, request);
}

} // namespace quiz_vulkan::render::vulkan_backend
