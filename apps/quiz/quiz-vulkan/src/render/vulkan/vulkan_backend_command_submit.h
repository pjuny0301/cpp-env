#pragma once

#include "render/vulkan/vulkan_backend_command_recording.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace quiz_vulkan::render::vulkan_backend {

struct vulkan_command_submit_sync_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_command_submit_sync_policy {
    bool require_image_available_semaphore = true;
    bool require_render_finished_semaphore = true;
    bool require_frame_fence = true;
};

struct vulkan_command_submit_sync_primitives {
    vulkan_command_submit_sync_handle image_available_semaphore;
    vulkan_command_submit_sync_handle render_finished_semaphore;
    vulkan_command_submit_sync_handle frame_fence;

    bool ready_for_submit(const vulkan_command_submit_sync_policy& policy) const
    {
        return (!policy.require_image_available_semaphore || image_available_semaphore.valid())
            && (!policy.require_render_finished_semaphore || render_finished_semaphore.valid())
            && (!policy.require_frame_fence || frame_fence.valid());
    }

    bool ready_for_present(const vulkan_command_submit_sync_policy& policy) const
    {
        return !policy.require_render_finished_semaphore || render_finished_semaphore.valid();
    }
};

enum class vulkan_command_submit_failure_mode {
    none,
    recoverable,
    fatal,
};

inline std::string_view command_submit_failure_mode_name(
    vulkan_command_submit_failure_mode mode)
{
    switch (mode) {
    case vulkan_command_submit_failure_mode::none:
        return "none";
    case vulkan_command_submit_failure_mode::recoverable:
        return "recoverable";
    case vulkan_command_submit_failure_mode::fatal:
        return "fatal";
    }

    return "unknown";
}

enum class vulkan_command_submit_readiness_status {
    not_requested,
    ready,
    command_recording_unavailable,
    command_buffer_unavailable,
    sync_primitives_unavailable,
    submit_queue_unavailable,
    present_target_unavailable,
    submit_failed_recoverable,
    submit_failed_fatal,
};

inline std::string_view command_submit_readiness_status_name(
    vulkan_command_submit_readiness_status status)
{
    switch (status) {
    case vulkan_command_submit_readiness_status::not_requested:
        return "not_requested";
    case vulkan_command_submit_readiness_status::ready:
        return "ready";
    case vulkan_command_submit_readiness_status::command_recording_unavailable:
        return "command_recording_unavailable";
    case vulkan_command_submit_readiness_status::command_buffer_unavailable:
        return "command_buffer_unavailable";
    case vulkan_command_submit_readiness_status::sync_primitives_unavailable:
        return "sync_primitives_unavailable";
    case vulkan_command_submit_readiness_status::submit_queue_unavailable:
        return "submit_queue_unavailable";
    case vulkan_command_submit_readiness_status::present_target_unavailable:
        return "present_target_unavailable";
    case vulkan_command_submit_readiness_status::submit_failed_recoverable:
        return "submit_failed_recoverable";
    case vulkan_command_submit_readiness_status::submit_failed_fatal:
        return "submit_failed_fatal";
    }

    return "unknown";
}

enum class vulkan_command_submit_frame_status {
    not_requested,
    submit_ready,
    submitted,
    submit_failed_recoverable,
    submit_failed_fatal,
};

inline std::string_view command_submit_frame_status_name(
    vulkan_command_submit_frame_status status)
{
    switch (status) {
    case vulkan_command_submit_frame_status::not_requested:
        return "not_requested";
    case vulkan_command_submit_frame_status::submit_ready:
        return "submit_ready";
    case vulkan_command_submit_frame_status::submitted:
        return "submitted";
    case vulkan_command_submit_frame_status::submit_failed_recoverable:
        return "submit_failed_recoverable";
    case vulkan_command_submit_frame_status::submit_failed_fatal:
        return "submit_failed_fatal";
    }

    return "unknown";
}

struct vulkan_command_submit_frame_result {
    bool checked = false;
    vulkan_command_submit_frame_status status =
        vulkan_command_submit_frame_status::not_requested;
    bool submit_attempted = false;
    bool submitted = false;
    bool present_ready = false;
    vulkan_command_submit_failure_mode failure_mode =
        vulkan_command_submit_failure_mode::none;
    std::string diagnostic;

    bool completed() const
    {
        return checked && submit_attempted && submitted && present_ready
            && status == vulkan_command_submit_frame_status::submitted
            && failure_mode == vulkan_command_submit_failure_mode::none;
    }

    bool recoverable_failure() const
    {
        return checked && status == vulkan_command_submit_frame_status::submit_failed_recoverable
            && failure_mode == vulkan_command_submit_failure_mode::recoverable;
    }

    bool fatal_failure() const
    {
        return checked && status == vulkan_command_submit_frame_status::submit_failed_fatal
            && failure_mode == vulkan_command_submit_failure_mode::fatal;
    }

    bool failed() const
    {
        return recoverable_failure() || fatal_failure();
    }
};

struct vulkan_command_submit_readiness_request {
    vulkan_command_submit_sync_policy sync_policy;
    bool require_present_target = true;
    bool present_target_available = true;
    vulkan_swapchain_image_id image_id{.value = 1};
};

struct vulkan_command_submit_readiness_result {
    bool checked = false;
    vulkan_command_submit_readiness_status status =
        vulkan_command_submit_readiness_status::not_requested;
    vulkan_command_recording_readiness_result command_recording;
    vulkan_command_recording_command_buffer_handle command_buffer;
    vulkan_queue_handle submit_queue;
    vulkan_command_submit_sync_primitives sync_primitives;
    vulkan_swapchain_image_id image_id;
    std::size_t planned_batch_count = 0;
    std::size_t submitted_batch_count = 0;
    bool command_recording_available = false;
    bool command_buffer_available = false;
    bool sync_primitives_available = false;
    bool submit_queue_available = false;
    bool present_target_available = false;
    bool submit_attempted = false;
    vulkan_command_submit_failure_mode failure_mode =
        vulkan_command_submit_failure_mode::none;
    vulkan_command_submit_frame_result frame_result;
    std::string diagnostic;

    bool preconditions_ready() const
    {
        return checked && command_recording_available && command_buffer_available
            && sync_primitives_available && submit_queue_available && present_target_available;
    }

    bool ready_for_submit() const
    {
        return checked && status == vulkan_command_submit_readiness_status::ready
            && preconditions_ready() && frame_result.completed()
            && submitted_batch_count == planned_batch_count;
    }

    bool ready_for_present() const
    {
        return checked && present_target_available && frame_result.present_ready;
    }

    bool recoverable_submit_failure() const
    {
        return checked
            && status == vulkan_command_submit_readiness_status::submit_failed_recoverable
            && frame_result.recoverable_failure();
    }

    bool fatal_submit_failure() const
    {
        return checked && status == vulkan_command_submit_readiness_status::submit_failed_fatal
            && frame_result.fatal_failure();
    }
};

class vulkan_command_submit_readiness_factory_interface {
public:
    virtual ~vulkan_command_submit_readiness_factory_interface() = default;

    virtual vulkan_command_submit_readiness_result check_command_submit_readiness(
        const vulkan_command_recording_readiness_result& command_recording_result,
        const vulkan_command_submit_readiness_request& request) = 0;
};

struct fake_vulkan_command_submit_readiness_factory_options {
    bool sync_primitives_available = true;
    bool submit_queue_available = true;
    bool present_target_available = true;
    vulkan_command_submit_failure_mode failure_mode =
        vulkan_command_submit_failure_mode::none;
    vulkan_queue_handle submit_queue;
    vulkan_command_submit_sync_primitives sync_primitives{
        .image_available_semaphore = vulkan_command_submit_sync_handle{.value = 1},
        .render_finished_semaphore = vulkan_command_submit_sync_handle{.value = 2},
        .frame_fence = vulkan_command_submit_sync_handle{.value = 3},
    };
};

class fake_vulkan_command_submit_readiness_factory final
    : public vulkan_command_submit_readiness_factory_interface {
public:
    fake_vulkan_command_submit_readiness_factory();
    explicit fake_vulkan_command_submit_readiness_factory(
        fake_vulkan_command_submit_readiness_factory_options options);

    vulkan_command_submit_readiness_result check_command_submit_readiness(
        const vulkan_command_recording_readiness_result& command_recording_result,
        const vulkan_command_submit_readiness_request& request) override;

private:
    fake_vulkan_command_submit_readiness_factory_options options_;
};

namespace command_submit_detail {

inline vulkan_queue_handle selected_submit_queue(
    const vulkan_command_recording_readiness_result& command_recording_result,
    const fake_vulkan_command_submit_readiness_factory_options& options)
{
    if (options.submit_queue.valid()) {
        return options.submit_queue;
    }

    for (const vulkan_device_queue_selection& queue :
         command_recording_result.render_pass.swapchain.device.selected_queues) {
        if (queue.capability == vulkan_device_queue_capability::graphics && queue.valid()) {
            return queue.queue;
        }
    }

    return {};
}

inline vulkan_command_submit_readiness_result make_command_submit_readiness_result(
    const vulkan_command_recording_readiness_result& command_recording_result,
    const vulkan_command_submit_readiness_request& request)
{
    return vulkan_command_submit_readiness_result{
        .checked = true,
        .status = vulkan_command_submit_readiness_status::not_requested,
        .command_recording = command_recording_result,
        .command_buffer = command_recording_result.command_buffer,
        .submit_queue = {},
        .sync_primitives = {},
        .image_id = request.image_id,
        .planned_batch_count = command_recording_result.planned_batch_count,
        .submitted_batch_count = 0,
        .command_recording_available = false,
        .command_buffer_available = false,
        .sync_primitives_available = false,
        .submit_queue_available = false,
        .present_target_available = false,
        .submit_attempted = false,
        .failure_mode = vulkan_command_submit_failure_mode::none,
        .frame_result = vulkan_command_submit_frame_result{
            .checked = true,
            .status = vulkan_command_submit_frame_status::not_requested,
            .submit_attempted = false,
            .submitted = false,
            .present_ready = false,
            .failure_mode = vulkan_command_submit_failure_mode::none,
            .diagnostic = {},
        },
        .diagnostic = {},
    };
}

inline void mark_submit_failure(
    vulkan_command_submit_readiness_result& result,
    vulkan_command_submit_failure_mode failure_mode,
    const vulkan_command_submit_sync_policy& sync_policy)
{
    result.submit_attempted = true;
    result.failure_mode = failure_mode;
    result.frame_result.submit_attempted = true;
    result.frame_result.failure_mode = failure_mode;
    result.frame_result.present_ready = result.present_target_available
        && result.sync_primitives.ready_for_present(sync_policy);

    if (failure_mode == vulkan_command_submit_failure_mode::recoverable) {
        result.status = vulkan_command_submit_readiness_status::submit_failed_recoverable;
        result.frame_result.status =
            vulkan_command_submit_frame_status::submit_failed_recoverable;
        result.frame_result.diagnostic = "Vulkan command buffer submit failed recoverably";
        result.diagnostic = result.frame_result.diagnostic;
        return;
    }

    result.status = vulkan_command_submit_readiness_status::submit_failed_fatal;
    result.frame_result.status = vulkan_command_submit_frame_status::submit_failed_fatal;
    result.frame_result.diagnostic = "Vulkan command buffer submit failed fatally";
    result.diagnostic = result.frame_result.diagnostic;
}

} // namespace command_submit_detail

inline fake_vulkan_command_submit_readiness_factory::
    fake_vulkan_command_submit_readiness_factory()
    : fake_vulkan_command_submit_readiness_factory(
        fake_vulkan_command_submit_readiness_factory_options{})
{
}

inline fake_vulkan_command_submit_readiness_factory::
    fake_vulkan_command_submit_readiness_factory(
        fake_vulkan_command_submit_readiness_factory_options options)
    : options_(std::move(options))
{
}

inline vulkan_command_submit_readiness_result
fake_vulkan_command_submit_readiness_factory::check_command_submit_readiness(
    const vulkan_command_recording_readiness_result& command_recording_result,
    const vulkan_command_submit_readiness_request& request)
{
    vulkan_command_submit_readiness_result result =
        command_submit_detail::make_command_submit_readiness_result(
            command_recording_result,
            request);

    result.command_recording_available =
        command_recording_result.checked
        && command_recording_result.status == vulkan_command_recording_readiness_status::ready
        && command_recording_result.render_pass_ready()
        && command_recording_result.framebuffer_ready()
        && command_recording_result.pipeline_ready();
    if (!result.command_recording_available) {
        result.status =
            vulkan_command_submit_readiness_status::command_recording_unavailable;
        result.diagnostic =
            "Vulkan command recording is not ready for command buffer submit";
        result.frame_result.diagnostic = result.diagnostic;
        return result;
    }

    result.command_buffer_available =
        command_recording_result.command_buffer_ready()
        && command_recording_result.command_buffer.valid();
    if (!result.command_buffer_available) {
        result.status = vulkan_command_submit_readiness_status::command_buffer_unavailable;
        result.diagnostic = "Vulkan command buffer is not available for submit";
        result.frame_result.diagnostic = result.diagnostic;
        return result;
    }

    result.sync_primitives = options_.sync_primitives_available
        ? options_.sync_primitives
        : vulkan_command_submit_sync_primitives{};
    result.sync_primitives_available =
        options_.sync_primitives_available
        && result.sync_primitives.ready_for_submit(request.sync_policy);
    if (!result.sync_primitives_available) {
        result.status =
            vulkan_command_submit_readiness_status::sync_primitives_unavailable;
        result.diagnostic =
            "Vulkan frame sync primitives are not available for command buffer submit";
        result.frame_result.diagnostic = result.diagnostic;
        return result;
    }

    result.submit_queue =
        command_submit_detail::selected_submit_queue(command_recording_result, options_);
    result.submit_queue_available = options_.submit_queue_available && result.submit_queue.valid();
    if (!result.submit_queue_available) {
        result.status = vulkan_command_submit_readiness_status::submit_queue_unavailable;
        result.diagnostic = "Vulkan submit queue is not available for command buffer submit";
        result.frame_result.diagnostic = result.diagnostic;
        return result;
    }

    result.present_target_available =
        !request.require_present_target
        || (options_.present_target_available && request.present_target_available
            && request.image_id.value > 0
            && command_recording_result.render_pass.swapchain.ready_for_frame()
            && result.sync_primitives.ready_for_present(request.sync_policy));
    if (!result.present_target_available) {
        result.status = vulkan_command_submit_readiness_status::present_target_unavailable;
        result.diagnostic = "Vulkan present target is not available after command buffer submit";
        result.frame_result.diagnostic = result.diagnostic;
        return result;
    }

    if (options_.failure_mode != vulkan_command_submit_failure_mode::none) {
        command_submit_detail::mark_submit_failure(
            result,
            options_.failure_mode,
            request.sync_policy);
        return result;
    }

    result.status = vulkan_command_submit_readiness_status::ready;
    result.submit_attempted = true;
    result.submitted_batch_count = result.planned_batch_count;
    result.failure_mode = vulkan_command_submit_failure_mode::none;
    result.frame_result.status = vulkan_command_submit_frame_status::submitted;
    result.frame_result.submit_attempted = true;
    result.frame_result.submitted = true;
    result.frame_result.present_ready = true;
    result.frame_result.failure_mode = vulkan_command_submit_failure_mode::none;
    result.frame_result.diagnostic =
        "Vulkan command buffer submit is ready and frame submit completed";
    result.diagnostic = result.frame_result.diagnostic;
    return result;
}

inline vulkan_command_submit_readiness_result check_vulkan_command_submit_readiness(
    vulkan_command_submit_readiness_factory_interface& factory,
    const vulkan_command_recording_readiness_result& command_recording_result,
    const vulkan_command_submit_readiness_request& request = {})
{
    return factory.check_command_submit_readiness(command_recording_result, request);
}

} // namespace quiz_vulkan::render::vulkan_backend
