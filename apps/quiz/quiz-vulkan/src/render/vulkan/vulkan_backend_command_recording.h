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

enum class vulkan_native_render_pass_scope_dispatch_table_status {
    not_checked,
    ready,
    function_table_unavailable,
    missing_begin_render_pass_symbol,
    missing_end_render_pass_symbol,
};

inline std::string_view native_render_pass_scope_dispatch_table_status_name(
    vulkan_native_render_pass_scope_dispatch_table_status status)
{
    switch (status) {
    case vulkan_native_render_pass_scope_dispatch_table_status::not_checked:
        return "not_checked";
    case vulkan_native_render_pass_scope_dispatch_table_status::ready:
        return "ready";
    case vulkan_native_render_pass_scope_dispatch_table_status::function_table_unavailable:
        return "function_table_unavailable";
    case vulkan_native_render_pass_scope_dispatch_table_status::missing_begin_render_pass_symbol:
        return "missing_begin_render_pass_symbol";
    case vulkan_native_render_pass_scope_dispatch_table_status::missing_end_render_pass_symbol:
        return "missing_end_render_pass_symbol";
    }

    return "unknown";
}

struct vulkan_native_render_pass_scope_dispatch_table {
    bool checked = false;
    vulkan_native_render_pass_scope_dispatch_table_status status =
        vulkan_native_render_pass_scope_dispatch_table_status::not_checked;
    vulkan_native_function_table_status function_table_status =
        vulkan_native_function_table_status::not_checked;
    bool function_table_checked = false;
    vulkan_native_function_pointer begin_render_pass;
    vulkan_native_function_pointer end_render_pass;
    std::string missing_symbol_name;
    std::string diagnostic;

    bool ready_for_render_pass_scope() const
    {
        return checked
            && status == vulkan_native_render_pass_scope_dispatch_table_status::ready
            && begin_render_pass.valid() && end_render_pass.valid();
    }
};

vulkan_native_render_pass_scope_dispatch_table
collect_vulkan_native_render_pass_scope_dispatch_table(
    const vulkan_native_function_table_diagnostics& diagnostics);

enum class vulkan_native_render_pass_scope_record_status {
    not_requested,
    recorded,
    framebuffer_targets_unavailable,
    target_index_unavailable,
    command_buffer_unavailable,
    dispatch_table_unavailable,
    missing_render_pass,
    missing_framebuffer,
    missing_extent,
    extent_mismatch,
    begin_failed,
    end_failed,
    headers_unavailable,
};

inline std::string_view native_render_pass_scope_record_status_name(
    vulkan_native_render_pass_scope_record_status status)
{
    switch (status) {
    case vulkan_native_render_pass_scope_record_status::not_requested:
        return "not_requested";
    case vulkan_native_render_pass_scope_record_status::recorded:
        return "recorded";
    case vulkan_native_render_pass_scope_record_status::framebuffer_targets_unavailable:
        return "framebuffer_targets_unavailable";
    case vulkan_native_render_pass_scope_record_status::target_index_unavailable:
        return "target_index_unavailable";
    case vulkan_native_render_pass_scope_record_status::command_buffer_unavailable:
        return "command_buffer_unavailable";
    case vulkan_native_render_pass_scope_record_status::dispatch_table_unavailable:
        return "dispatch_table_unavailable";
    case vulkan_native_render_pass_scope_record_status::missing_render_pass:
        return "missing_render_pass";
    case vulkan_native_render_pass_scope_record_status::missing_framebuffer:
        return "missing_framebuffer";
    case vulkan_native_render_pass_scope_record_status::missing_extent:
        return "missing_extent";
    case vulkan_native_render_pass_scope_record_status::extent_mismatch:
        return "extent_mismatch";
    case vulkan_native_render_pass_scope_record_status::begin_failed:
        return "begin_failed";
    case vulkan_native_render_pass_scope_record_status::end_failed:
        return "end_failed";
    case vulkan_native_render_pass_scope_record_status::headers_unavailable:
        return "headers_unavailable";
    }

    return "unknown";
}

struct vulkan_native_render_pass_scope_record_request {
    vulkan_native_framebuffer_targets_execution_result framebuffer_targets;
    vulkan_command_recording_readiness_result command_recording;
    vulkan_native_render_pass_scope_dispatch_table dispatch_table;
    std::size_t framebuffer_target_index = 0;
};

struct vulkan_native_render_pass_scope_record_result {
    bool checked = false;
    vulkan_native_render_pass_scope_record_status status =
        vulkan_native_render_pass_scope_record_status::not_requested;
    vulkan_native_framebuffer_targets_execution_result framebuffer_targets;
    vulkan_command_recording_readiness_result command_recording;
    vulkan_native_render_pass_scope_dispatch_table dispatch_table;
    std::size_t selected_framebuffer_target_index = 0;
    std::size_t framebuffer_target_count = 0;
    vulkan_swapchain_image_id image_id;
    vulkan_command_recording_command_buffer_handle command_buffer;
    vulkan_render_pass_handle render_pass;
    vulkan_framebuffer_handle framebuffer;
    vulkan_surface_extent extent;
    vulkan_native_function_pointer begin_render_pass_symbol;
    vulkan_native_function_pointer end_render_pass_symbol;
    bool framebuffer_targets_ready = false;
    bool command_buffer_ready = false;
    bool render_pass_ready = false;
    bool framebuffer_ready = false;
    bool extent_ready = false;
    bool extent_matches = false;
    bool dispatch_table_ready = false;
    bool vk_cmd_begin_render_pass_called = false;
    bool vk_cmd_end_render_pass_called = false;
    std::int32_t native_result = 0;
    std::string diagnostic;

    bool ready_for_draw_commands() const
    {
        return checked && status == vulkan_native_render_pass_scope_record_status::recorded
            && framebuffer_targets_ready && command_buffer_ready && render_pass_ready
            && framebuffer_ready && extent_ready && extent_matches && dispatch_table_ready
            && image_id.value > 0 && command_buffer.valid() && render_pass.valid()
            && framebuffer.valid() && extent.valid()
            && vk_cmd_begin_render_pass_called && vk_cmd_end_render_pass_called;
    }

    bool blocked() const
    {
        return checked && !ready_for_draw_commands();
    }
};

class vulkan_native_render_pass_scope_recorder_interface {
public:
    virtual ~vulkan_native_render_pass_scope_recorder_interface() = default;

    virtual vulkan_native_render_pass_scope_record_result record_render_pass_scope(
        const vulkan_native_render_pass_scope_record_request& request) = 0;
};

struct fake_vulkan_native_render_pass_scope_recorder_options {
    bool fail_begin = false;
    bool fail_end = false;
    std::int32_t begin_failure_result = -1;
    std::int32_t end_failure_result = -2;
};

struct fake_vulkan_native_render_pass_scope_recorder_state {
    std::size_t record_call_count = 0;
    std::size_t requested_framebuffer_target_index = 0;
    vulkan_command_recording_command_buffer_handle requested_command_buffer;
    vulkan_render_pass_handle requested_render_pass;
    vulkan_framebuffer_handle requested_framebuffer;
    vulkan_surface_extent requested_extent;
    vulkan_native_function_pointer last_begin_render_pass_symbol;
    vulkan_native_function_pointer last_end_render_pass_symbol;
    vulkan_native_render_pass_scope_record_result last_result;
};

class fake_vulkan_native_render_pass_scope_recorder final
    : public vulkan_native_render_pass_scope_recorder_interface {
public:
    fake_vulkan_native_render_pass_scope_recorder();
    explicit fake_vulkan_native_render_pass_scope_recorder(
        fake_vulkan_native_render_pass_scope_recorder_options options);

    vulkan_native_render_pass_scope_record_result record_render_pass_scope(
        const vulkan_native_render_pass_scope_record_request& request) override;
    const fake_vulkan_native_render_pass_scope_recorder_state& state() const;

private:
    fake_vulkan_native_render_pass_scope_recorder_options options_;
    fake_vulkan_native_render_pass_scope_recorder_state state_;
};

class vulkan_native_render_pass_scope_recorder final
    : public vulkan_native_render_pass_scope_recorder_interface {
public:
    vulkan_native_render_pass_scope_record_result record_render_pass_scope(
        const vulkan_native_render_pass_scope_record_request& request) override;
};

vulkan_native_render_pass_scope_record_result record_native_vulkan_render_pass_scope(
    vulkan_native_render_pass_scope_recorder_interface& recorder,
    const vulkan_native_render_pass_scope_record_request& request);

namespace command_recording_detail {

inline vulkan_native_function_pointer find_native_symbol_pointer_by_name(
    const vulkan_native_function_table_diagnostics& diagnostics,
    std::string_view name)
{
    const auto found = std::find_if(
        diagnostics.symbols.begin(),
        diagnostics.symbols.end(),
        [name](const vulkan_native_entrypoint_symbol_diagnostics& symbol) {
            return symbol.name == name && symbol.completed();
        });
    return found == diagnostics.symbols.end()
        ? vulkan_native_function_pointer{}
        : found->pointer;
}

inline bool same_extent(vulkan_surface_extent lhs, vulkan_surface_extent rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height;
}

inline bool render_pass_scope_extent_matches(
    vulkan_surface_extent selected_extent,
    const vulkan_native_framebuffer_targets_execution_result& framebuffer_targets,
    const vulkan_command_recording_readiness_result& command_recording)
{
    if (!selected_extent.valid()) {
        return false;
    }
    if (framebuffer_targets.extent.valid()
        && !same_extent(selected_extent, framebuffer_targets.extent)) {
        return false;
    }
    if (command_recording.render_pass.selected_extent.valid()
        && !same_extent(selected_extent, command_recording.render_pass.selected_extent)) {
        return false;
    }

    return true;
}

inline vulkan_native_render_pass_scope_record_result make_render_pass_scope_record_result(
    const vulkan_native_render_pass_scope_record_request& request)
{
    vulkan_native_render_pass_scope_record_result result{
        .checked = true,
        .status = vulkan_native_render_pass_scope_record_status::not_requested,
        .framebuffer_targets = request.framebuffer_targets,
        .command_recording = request.command_recording,
        .dispatch_table = request.dispatch_table,
        .selected_framebuffer_target_index = request.framebuffer_target_index,
        .framebuffer_target_count = request.framebuffer_targets.targets.size(),
        .image_id = {},
        .command_buffer = request.command_recording.command_buffer,
        .render_pass = request.framebuffer_targets.render_pass_handle,
        .framebuffer = {},
        .extent = request.framebuffer_targets.extent,
        .begin_render_pass_symbol = request.dispatch_table.begin_render_pass,
        .end_render_pass_symbol = request.dispatch_table.end_render_pass,
        .framebuffer_targets_ready = request.framebuffer_targets.checked
            && request.framebuffer_targets.status
                == vulkan_native_framebuffer_targets_execution_status::ready
            && !request.framebuffer_targets.targets.empty(),
        .command_buffer_ready = request.command_recording.command_buffer_ready(),
        .render_pass_ready = request.framebuffer_targets.render_pass_ready
            && request.framebuffer_targets.render_pass_handle.valid(),
        .framebuffer_ready = false,
        .extent_ready = request.framebuffer_targets.extent.valid(),
        .extent_matches = false,
        .dispatch_table_ready = request.dispatch_table.ready_for_render_pass_scope(),
        .vk_cmd_begin_render_pass_called = false,
        .vk_cmd_end_render_pass_called = false,
        .native_result = 0,
        .diagnostic = {},
    };

    if (request.framebuffer_target_index < request.framebuffer_targets.targets.size()) {
        const vulkan_native_framebuffer_target& target =
            request.framebuffer_targets.targets[request.framebuffer_target_index];
        result.image_id = target.image_id;
        result.render_pass = target.render_pass;
        result.framebuffer = target.framebuffer;
        result.extent = target.extent;
        result.render_pass_ready = target.render_pass_ready && target.render_pass.valid();
        result.framebuffer_ready = target.framebuffer_ready && target.framebuffer.valid();
        result.extent_ready = target.extent_ready && target.extent.valid();
        result.extent_matches = render_pass_scope_extent_matches(
            target.extent,
            request.framebuffer_targets,
            request.command_recording);
    }

    return result;
}

} // namespace command_recording_detail

inline vulkan_native_render_pass_scope_dispatch_table
collect_vulkan_native_render_pass_scope_dispatch_table(
    const vulkan_native_function_table_diagnostics& diagnostics)
{
    vulkan_native_render_pass_scope_dispatch_table dispatch{
        .checked = true,
        .status = vulkan_native_render_pass_scope_dispatch_table_status::not_checked,
        .function_table_status = diagnostics.status,
        .function_table_checked = diagnostics.checked,
        .begin_render_pass = command_recording_detail::find_native_symbol_pointer_by_name(
            diagnostics,
            "vkCmdBeginRenderPass"),
        .end_render_pass = command_recording_detail::find_native_symbol_pointer_by_name(
            diagnostics,
            "vkCmdEndRenderPass"),
        .missing_symbol_name = diagnostics.missing_symbol_name,
        .diagnostic = {},
    };

    if (!diagnostics.checked) {
        dispatch.status =
            vulkan_native_render_pass_scope_dispatch_table_status::function_table_unavailable;
        dispatch.diagnostic = diagnostics.diagnostic.empty()
            ? "Native Vulkan render pass scope dispatch table has unchecked function diagnostics"
            : diagnostics.diagnostic;
        return dispatch;
    }
    if (!dispatch.begin_render_pass.valid()) {
        dispatch.status =
            vulkan_native_render_pass_scope_dispatch_table_status::missing_begin_render_pass_symbol;
        dispatch.missing_symbol_name = dispatch.missing_symbol_name.empty()
            ? "vkCmdBeginRenderPass"
            : dispatch.missing_symbol_name;
        dispatch.diagnostic = diagnostics.diagnostic.empty()
            ? "Native Vulkan render pass scope dispatch table is missing vkCmdBeginRenderPass"
            : diagnostics.diagnostic;
        return dispatch;
    }
    if (!dispatch.end_render_pass.valid()) {
        dispatch.status =
            vulkan_native_render_pass_scope_dispatch_table_status::missing_end_render_pass_symbol;
        dispatch.missing_symbol_name = dispatch.missing_symbol_name.empty()
            ? "vkCmdEndRenderPass"
            : dispatch.missing_symbol_name;
        dispatch.diagnostic = diagnostics.diagnostic.empty()
            ? "Native Vulkan render pass scope dispatch table is missing vkCmdEndRenderPass"
            : diagnostics.diagnostic;
        return dispatch;
    }

    dispatch.status = vulkan_native_render_pass_scope_dispatch_table_status::ready;
    dispatch.diagnostic = "Native Vulkan render pass scope dispatch table is ready";
    return dispatch;
}

inline fake_vulkan_native_render_pass_scope_recorder::
    fake_vulkan_native_render_pass_scope_recorder()
    : fake_vulkan_native_render_pass_scope_recorder(
        fake_vulkan_native_render_pass_scope_recorder_options{})
{
}

inline fake_vulkan_native_render_pass_scope_recorder::
    fake_vulkan_native_render_pass_scope_recorder(
        fake_vulkan_native_render_pass_scope_recorder_options options)
    : options_(options)
{
}

inline vulkan_native_render_pass_scope_record_result
fake_vulkan_native_render_pass_scope_recorder::record_render_pass_scope(
    const vulkan_native_render_pass_scope_record_request& request)
{
    vulkan_native_render_pass_scope_record_result result =
        command_recording_detail::make_render_pass_scope_record_result(request);

    if (!result.framebuffer_targets_ready) {
        result.status =
            vulkan_native_render_pass_scope_record_status::framebuffer_targets_unavailable;
        result.diagnostic = request.framebuffer_targets.diagnostic.empty()
            ? "Native Vulkan render pass scope is missing ready framebuffer targets"
            : request.framebuffer_targets.diagnostic;
        state_.last_result = result;
        return result;
    }
    if (request.framebuffer_target_index >= request.framebuffer_targets.targets.size()) {
        result.status =
            vulkan_native_render_pass_scope_record_status::target_index_unavailable;
        result.diagnostic =
            "Native Vulkan render pass scope selected framebuffer target index is unavailable";
        state_.last_result = result;
        return result;
    }
    if (!result.command_buffer_ready) {
        result.status =
            vulkan_native_render_pass_scope_record_status::command_buffer_unavailable;
        result.diagnostic = "Native Vulkan render pass scope is missing a command buffer";
        state_.last_result = result;
        return result;
    }
    if (!result.render_pass_ready) {
        result.status = vulkan_native_render_pass_scope_record_status::missing_render_pass;
        result.diagnostic = "Native Vulkan render pass scope is missing a render pass";
        state_.last_result = result;
        return result;
    }
    if (!result.framebuffer_ready) {
        result.status = vulkan_native_render_pass_scope_record_status::missing_framebuffer;
        result.diagnostic = "Native Vulkan render pass scope is missing a framebuffer";
        state_.last_result = result;
        return result;
    }
    if (!result.extent_ready) {
        result.status = vulkan_native_render_pass_scope_record_status::missing_extent;
        result.diagnostic = "Native Vulkan render pass scope is missing a framebuffer extent";
        state_.last_result = result;
        return result;
    }
    if (!result.extent_matches) {
        result.status = vulkan_native_render_pass_scope_record_status::extent_mismatch;
        result.diagnostic =
            "Native Vulkan render pass scope extent does not match framebuffer evidence";
        state_.last_result = result;
        return result;
    }
    if (!result.dispatch_table_ready) {
        result.status = vulkan_native_render_pass_scope_record_status::dispatch_table_unavailable;
        result.diagnostic = request.dispatch_table.diagnostic.empty()
            ? "Native Vulkan render pass scope is missing begin/end dispatch"
            : request.dispatch_table.diagnostic;
        state_.last_result = result;
        return result;
    }

    ++state_.record_call_count;
    state_.requested_framebuffer_target_index = request.framebuffer_target_index;
    state_.requested_command_buffer = result.command_buffer;
    state_.requested_render_pass = result.render_pass;
    state_.requested_framebuffer = result.framebuffer;
    state_.requested_extent = result.extent;
    state_.last_begin_render_pass_symbol = result.begin_render_pass_symbol;
    state_.last_end_render_pass_symbol = result.end_render_pass_symbol;

    result.vk_cmd_begin_render_pass_called = true;
    if (options_.fail_begin) {
        result.status = vulkan_native_render_pass_scope_record_status::begin_failed;
        result.native_result = options_.begin_failure_result;
        result.diagnostic = "Native Vulkan render pass scope begin failed";
        state_.last_result = result;
        return result;
    }

    result.vk_cmd_end_render_pass_called = true;
    if (options_.fail_end) {
        result.status = vulkan_native_render_pass_scope_record_status::end_failed;
        result.native_result = options_.end_failure_result;
        result.diagnostic = "Native Vulkan render pass scope end failed";
        state_.last_result = result;
        return result;
    }

    result.status = vulkan_native_render_pass_scope_record_status::recorded;
    result.diagnostic = "Native Vulkan render pass scope recorded";
    state_.last_result = result;
    return result;
}

inline const fake_vulkan_native_render_pass_scope_recorder_state&
fake_vulkan_native_render_pass_scope_recorder::state() const
{
    return state_;
}

inline vulkan_native_render_pass_scope_record_result record_native_vulkan_render_pass_scope(
    vulkan_native_render_pass_scope_recorder_interface& recorder,
    const vulkan_native_render_pass_scope_record_request& request)
{
    return recorder.record_render_pass_scope(request);
}

} // namespace quiz_vulkan::render::vulkan_backend
