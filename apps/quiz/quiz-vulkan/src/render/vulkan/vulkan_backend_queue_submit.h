#pragma once

#include "render/vulkan/vulkan_backend_command_submit.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace quiz_vulkan::render::vulkan_backend {

enum class vulkan_queue_submit_adapter_call_status {
    not_called,
    completed,
    failed_recoverable,
    failed_fatal,
};

inline std::string_view queue_submit_adapter_call_status_name(
    vulkan_queue_submit_adapter_call_status status)
{
    switch (status) {
    case vulkan_queue_submit_adapter_call_status::not_called:
        return "not_called";
    case vulkan_queue_submit_adapter_call_status::completed:
        return "completed";
    case vulkan_queue_submit_adapter_call_status::failed_recoverable:
        return "failed_recoverable";
    case vulkan_queue_submit_adapter_call_status::failed_fatal:
        return "failed_fatal";
    }

    return "unknown";
}

struct vulkan_queue_submit_adapter_operation_result {
    bool checked = false;
    vulkan_queue_submit_adapter_call_status status =
        vulkan_queue_submit_adapter_call_status::not_called;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_queue_submit_adapter_call_status::completed;
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_queue_submit_adapter_call_status::failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked && status == vulkan_queue_submit_adapter_call_status::failed_fatal;
    }

    bool failed() const
    {
        return recoverable_failure() || fatal_failure();
    }
};

struct vulkan_queue_submit_adapter_submit_call {
    vulkan_queue_handle queue;
    vulkan_command_recording_command_buffer_handle command_buffer;
    vulkan_command_submit_sync_primitives sync_primitives;
    std::size_t batch_count = 0;

    bool valid() const
    {
        return queue.valid() && command_buffer.valid()
            && sync_primitives.image_available_semaphore.valid()
            && sync_primitives.render_finished_semaphore.valid()
            && sync_primitives.frame_fence.valid();
    }
};

struct vulkan_queue_submit_adapter_present_call {
    vulkan_queue_handle queue;
    vulkan_swapchain_handle swapchain;
    vulkan_swapchain_image_id image_id;
    vulkan_command_submit_sync_handle wait_render_finished_semaphore;

    bool valid() const
    {
        return queue.valid() && swapchain.valid() && image_id.value > 0
            && wait_render_finished_semaphore.valid();
    }
};

using vulkan_queue_submit_adapter_submit_fn =
    vulkan_queue_submit_adapter_operation_result (*)(
        void* user_data,
        const vulkan_queue_submit_adapter_submit_call& call);

using vulkan_queue_submit_adapter_present_fn =
    vulkan_queue_submit_adapter_operation_result (*)(
        void* user_data,
        const vulkan_queue_submit_adapter_present_call& call);

struct vulkan_queue_submit_adapter_function_table {
    vulkan_queue_submit_adapter_submit_fn submit = nullptr;
    vulkan_queue_submit_adapter_present_fn present = nullptr;

    bool valid() const
    {
        return submit != nullptr && present != nullptr;
    }
};

struct vulkan_queue_submit_adapter {
    void* user_data = nullptr;
    vulkan_queue_submit_adapter_function_table functions;

    bool valid() const
    {
        return user_data != nullptr && functions.valid();
    }
};

enum class vulkan_queue_submit_present_status {
    not_requested,
    submitted_and_presented,
    command_submit_unavailable,
    command_buffer_unavailable,
    submit_queue_unavailable,
    present_target_unavailable,
    adapter_unavailable,
    submit_failed_recoverable,
    submit_failed_fatal,
    present_failed_recoverable,
    present_failed_fatal,
};

inline std::string_view queue_submit_present_status_name(
    vulkan_queue_submit_present_status status)
{
    switch (status) {
    case vulkan_queue_submit_present_status::not_requested:
        return "not_requested";
    case vulkan_queue_submit_present_status::submitted_and_presented:
        return "submitted_and_presented";
    case vulkan_queue_submit_present_status::command_submit_unavailable:
        return "command_submit_unavailable";
    case vulkan_queue_submit_present_status::command_buffer_unavailable:
        return "command_buffer_unavailable";
    case vulkan_queue_submit_present_status::submit_queue_unavailable:
        return "submit_queue_unavailable";
    case vulkan_queue_submit_present_status::present_target_unavailable:
        return "present_target_unavailable";
    case vulkan_queue_submit_present_status::adapter_unavailable:
        return "adapter_unavailable";
    case vulkan_queue_submit_present_status::submit_failed_recoverable:
        return "submit_failed_recoverable";
    case vulkan_queue_submit_present_status::submit_failed_fatal:
        return "submit_failed_fatal";
    case vulkan_queue_submit_present_status::present_failed_recoverable:
        return "present_failed_recoverable";
    case vulkan_queue_submit_present_status::present_failed_fatal:
        return "present_failed_fatal";
    }

    return "unknown";
}

struct vulkan_queue_submit_present_request {
    bool require_present = true;
    vulkan_swapchain_image_id image_id{.value = 1};
};

struct vulkan_queue_submit_present_result {
    bool checked = false;
    vulkan_queue_submit_present_status status =
        vulkan_queue_submit_present_status::not_requested;
    vulkan_command_submit_readiness_result command_submit;
    vulkan_queue_submit_adapter_submit_call submit_call;
    vulkan_queue_submit_adapter_present_call present_call;
    vulkan_queue_submit_adapter_operation_result submit_result;
    vulkan_queue_submit_adapter_operation_result present_result;
    bool submit_called = false;
    bool present_called = false;
    std::size_t submit_order = 0;
    std::size_t present_order = 0;
    std::string diagnostic;

    bool submit_before_present() const
    {
        return submit_called && present_called && submit_order > 0
            && present_order > submit_order;
    }

    bool completed() const
    {
        return checked && status == vulkan_queue_submit_present_status::submitted_and_presented
            && submit_result.completed() && present_result.completed()
            && submit_before_present();
    }

    bool recoverable_failure() const
    {
        return checked
            && (status == vulkan_queue_submit_present_status::submit_failed_recoverable
                || status == vulkan_queue_submit_present_status::present_failed_recoverable);
    }

    bool fatal_failure() const
    {
        return checked
            && (status == vulkan_queue_submit_present_status::submit_failed_fatal
                || status == vulkan_queue_submit_present_status::present_failed_fatal);
    }

    bool failed() const
    {
        return recoverable_failure() || fatal_failure()
            || status == vulkan_queue_submit_present_status::command_submit_unavailable
            || status == vulkan_queue_submit_present_status::command_buffer_unavailable
            || status == vulkan_queue_submit_present_status::submit_queue_unavailable
            || status == vulkan_queue_submit_present_status::present_target_unavailable
            || status == vulkan_queue_submit_present_status::adapter_unavailable;
    }
};

struct fake_vulkan_queue_submit_adapter_state {
    std::size_t submit_call_count = 0;
    std::size_t present_call_count = 0;
    std::size_t next_order = 0;
    std::size_t submit_order = 0;
    std::size_t present_order = 0;
    bool present_called_before_submit = false;
    vulkan_queue_submit_adapter_submit_call last_submit_call;
    vulkan_queue_submit_adapter_present_call last_present_call;

    bool submit_before_present() const
    {
        return submit_call_count == 1 && present_call_count == 1
            && submit_order > 0 && present_order > submit_order
            && !present_called_before_submit;
    }
};

struct fake_vulkan_queue_submit_adapter_options {
    vulkan_queue_submit_adapter_call_status submit_status =
        vulkan_queue_submit_adapter_call_status::completed;
    vulkan_queue_submit_adapter_call_status present_status =
        vulkan_queue_submit_adapter_call_status::completed;
};

class fake_vulkan_queue_submit_adapter final {
public:
    fake_vulkan_queue_submit_adapter();
    explicit fake_vulkan_queue_submit_adapter(fake_vulkan_queue_submit_adapter_options options);

    vulkan_queue_submit_adapter adapter();
    const fake_vulkan_queue_submit_adapter_state& state() const;

private:
    static vulkan_queue_submit_adapter_operation_result submit_callback(
        void* user_data,
        const vulkan_queue_submit_adapter_submit_call& call);
    static vulkan_queue_submit_adapter_operation_result present_callback(
        void* user_data,
        const vulkan_queue_submit_adapter_present_call& call);

    fake_vulkan_queue_submit_adapter_options options_;
    fake_vulkan_queue_submit_adapter_state state_;
};

namespace queue_submit_detail {

inline vulkan_queue_handle selected_present_queue(
    const vulkan_command_submit_readiness_result& readiness)
{
    for (const vulkan_device_queue_selection& queue :
         readiness.command_recording.render_pass.swapchain.device.selected_queues) {
        if (queue.capability == vulkan_device_queue_capability::present && queue.valid()) {
            return queue.queue;
        }
    }

    return {};
}

inline vulkan_queue_submit_present_result make_queue_submit_present_result(
    const vulkan_command_submit_readiness_result& readiness)
{
    return vulkan_queue_submit_present_result{
        .checked = true,
        .status = vulkan_queue_submit_present_status::not_requested,
        .command_submit = readiness,
        .submit_call = {},
        .present_call = {},
        .submit_result = {},
        .present_result = {},
        .submit_called = false,
        .present_called = false,
        .submit_order = 0,
        .present_order = 0,
        .diagnostic = {},
    };
}

inline vulkan_command_submit_failure_mode failure_mode_for(
    vulkan_queue_submit_adapter_call_status status)
{
    if (status == vulkan_queue_submit_adapter_call_status::failed_recoverable) {
        return vulkan_command_submit_failure_mode::recoverable;
    }
    if (status == vulkan_queue_submit_adapter_call_status::failed_fatal) {
        return vulkan_command_submit_failure_mode::fatal;
    }

    return vulkan_command_submit_failure_mode::none;
}

inline vulkan_queue_submit_adapter_operation_result make_operation_result(
    vulkan_queue_submit_adapter_call_status status,
    std::string diagnostic)
{
    return vulkan_queue_submit_adapter_operation_result{
        .checked = true,
        .status = status,
        .diagnostic = std::move(diagnostic),
    };
}

inline void map_submit_failure(
    vulkan_queue_submit_present_result& result,
    const vulkan_queue_submit_adapter_operation_result& submit_result)
{
    result.command_submit.frame_result.checked = true;
    result.command_submit.frame_result.submit_attempted = true;
    result.command_submit.frame_result.submitted = false;
    result.command_submit.frame_result.present_ready = false;
    result.command_submit.frame_result.failure_mode = failure_mode_for(submit_result.status);
    result.command_submit.failure_mode = result.command_submit.frame_result.failure_mode;
    result.command_submit.submit_attempted = true;
    result.command_submit.diagnostic = submit_result.diagnostic;
    result.diagnostic = submit_result.diagnostic;

    if (submit_result.recoverable_failure()) {
        result.status = vulkan_queue_submit_present_status::submit_failed_recoverable;
        result.command_submit.status =
            vulkan_command_submit_readiness_status::submit_failed_recoverable;
        result.command_submit.frame_result.status =
            vulkan_command_submit_frame_status::submit_failed_recoverable;
        return;
    }

    result.status = vulkan_queue_submit_present_status::submit_failed_fatal;
    result.command_submit.status =
        vulkan_command_submit_readiness_status::submit_failed_fatal;
    result.command_submit.frame_result.status =
        vulkan_command_submit_frame_status::submit_failed_fatal;
}

inline void map_present_failure(
    vulkan_queue_submit_present_result& result,
    const vulkan_queue_submit_adapter_operation_result& present_result)
{
    result.command_submit.frame_result.checked = true;
    result.command_submit.frame_result.submit_attempted = true;
    result.command_submit.frame_result.submitted = true;
    result.command_submit.frame_result.present_ready = false;
    result.command_submit.frame_result.failure_mode = failure_mode_for(present_result.status);
    result.command_submit.failure_mode = result.command_submit.frame_result.failure_mode;
    result.command_submit.diagnostic = present_result.diagnostic;
    result.diagnostic = present_result.diagnostic;

    result.status = present_result.recoverable_failure()
        ? vulkan_queue_submit_present_status::present_failed_recoverable
        : vulkan_queue_submit_present_status::present_failed_fatal;
}

inline bool readiness_has_submit_failure(
    const vulkan_command_submit_readiness_result& readiness)
{
    return readiness.recoverable_submit_failure() || readiness.fatal_submit_failure();
}

} // namespace queue_submit_detail

inline fake_vulkan_queue_submit_adapter::fake_vulkan_queue_submit_adapter()
    : fake_vulkan_queue_submit_adapter(fake_vulkan_queue_submit_adapter_options{})
{
}

inline fake_vulkan_queue_submit_adapter::fake_vulkan_queue_submit_adapter(
    fake_vulkan_queue_submit_adapter_options options)
    : options_(options)
{
}

inline vulkan_queue_submit_adapter fake_vulkan_queue_submit_adapter::adapter()
{
    return vulkan_queue_submit_adapter{
        .user_data = this,
        .functions = vulkan_queue_submit_adapter_function_table{
            .submit = &fake_vulkan_queue_submit_adapter::submit_callback,
            .present = &fake_vulkan_queue_submit_adapter::present_callback,
        },
    };
}

inline const fake_vulkan_queue_submit_adapter_state& fake_vulkan_queue_submit_adapter::state() const
{
    return state_;
}

inline vulkan_queue_submit_adapter_operation_result
fake_vulkan_queue_submit_adapter::submit_callback(
    void* user_data,
    const vulkan_queue_submit_adapter_submit_call& call)
{
    auto& fake = *static_cast<fake_vulkan_queue_submit_adapter*>(user_data);
    ++fake.state_.submit_call_count;
    fake.state_.submit_order = ++fake.state_.next_order;
    fake.state_.last_submit_call = call;
    return queue_submit_detail::make_operation_result(
        fake.options_.submit_status,
        fake.options_.submit_status == vulkan_queue_submit_adapter_call_status::completed
            ? "Vulkan queue submit adapter submitted command buffer"
            : "Vulkan queue submit adapter failed command buffer submit");
}

inline vulkan_queue_submit_adapter_operation_result
fake_vulkan_queue_submit_adapter::present_callback(
    void* user_data,
    const vulkan_queue_submit_adapter_present_call& call)
{
    auto& fake = *static_cast<fake_vulkan_queue_submit_adapter*>(user_data);
    ++fake.state_.present_call_count;
    fake.state_.present_order = ++fake.state_.next_order;
    fake.state_.present_called_before_submit = fake.state_.submit_call_count == 0;
    fake.state_.last_present_call = call;
    if (fake.state_.present_called_before_submit) {
        return queue_submit_detail::make_operation_result(
            vulkan_queue_submit_adapter_call_status::failed_fatal,
            "Vulkan queue submit adapter attempted present before submit");
    }

    return queue_submit_detail::make_operation_result(
        fake.options_.present_status,
        fake.options_.present_status == vulkan_queue_submit_adapter_call_status::completed
            ? "Vulkan queue submit adapter presented image"
            : "Vulkan queue submit adapter failed present");
}

inline vulkan_queue_submit_present_result submit_and_present_vulkan_queue_frame(
    vulkan_queue_submit_adapter adapter,
    const vulkan_command_submit_readiness_result& readiness,
    const vulkan_queue_submit_present_request& request = {})
{
    vulkan_queue_submit_present_result result =
        queue_submit_detail::make_queue_submit_present_result(readiness);

    if (queue_submit_detail::readiness_has_submit_failure(readiness)) {
        result.status = readiness.recoverable_submit_failure()
            ? vulkan_queue_submit_present_status::submit_failed_recoverable
            : vulkan_queue_submit_present_status::submit_failed_fatal;
        result.command_submit = readiness;
        result.diagnostic = readiness.diagnostic;
        return result;
    }

    if (!readiness.checked || !readiness.command_recording_available) {
        result.status = vulkan_queue_submit_present_status::command_submit_unavailable;
        result.diagnostic = "Vulkan command submit readiness is unavailable for queue submit";
        return result;
    }
    if (!readiness.command_buffer_available || !readiness.command_buffer.valid()) {
        result.status = vulkan_queue_submit_present_status::command_buffer_unavailable;
        result.diagnostic = "Vulkan command buffer is unavailable for queue submit";
        return result;
    }
    if (!readiness.sync_primitives_available) {
        result.status = vulkan_queue_submit_present_status::command_submit_unavailable;
        result.diagnostic = "Vulkan command submit sync readiness is unavailable for queue submit";
        return result;
    }
    if (!readiness.submit_queue_available || !readiness.submit_queue.valid()) {
        result.status = vulkan_queue_submit_present_status::submit_queue_unavailable;
        result.diagnostic = "Vulkan submit queue is unavailable for queue submit";
        return result;
    }
    if (!readiness.present_target_available && request.require_present) {
        result.status = vulkan_queue_submit_present_status::present_target_unavailable;
        result.diagnostic = "Vulkan present target is unavailable for queue present";
        return result;
    }
    if (!adapter.valid()) {
        result.status = vulkan_queue_submit_present_status::adapter_unavailable;
        result.diagnostic = "Vulkan queue submit adapter function table is unavailable";
        return result;
    }

    result.submit_call = vulkan_queue_submit_adapter_submit_call{
        .queue = readiness.submit_queue,
        .command_buffer = readiness.command_buffer,
        .sync_primitives = readiness.sync_primitives,
        .batch_count = readiness.planned_batch_count,
    };
    if (!result.submit_call.valid()) {
        result.status = vulkan_queue_submit_present_status::submit_queue_unavailable;
        result.diagnostic = "Vulkan queue submit adapter received an invalid submit call";
        return result;
    }

    result.submit_called = true;
    result.submit_order = 1;
    result.submit_result = adapter.functions.submit(adapter.user_data, result.submit_call);
    if (!result.submit_result.completed()) {
        queue_submit_detail::map_submit_failure(result, result.submit_result);
        return result;
    }

    const vulkan_queue_handle present_queue = queue_submit_detail::selected_present_queue(readiness);
    result.present_call = vulkan_queue_submit_adapter_present_call{
        .queue = present_queue,
        .swapchain = readiness.command_recording.render_pass.swapchain.handle,
        .image_id = request.image_id.value > 0 ? request.image_id : readiness.image_id,
        .wait_render_finished_semaphore =
            readiness.sync_primitives.render_finished_semaphore,
    };
    if (request.require_present && (!readiness.present_target_available || !result.present_call.valid())) {
        result.status = vulkan_queue_submit_present_status::present_target_unavailable;
        result.diagnostic = "Vulkan queue present target is unavailable after submit";
        return result;
    }

    result.present_called = true;
    result.present_order = 2;
    result.present_result = adapter.functions.present(adapter.user_data, result.present_call);
    if (!result.present_result.completed()) {
        queue_submit_detail::map_present_failure(result, result.present_result);
        return result;
    }

    result.status = vulkan_queue_submit_present_status::submitted_and_presented;
    result.command_submit.frame_result.checked = true;
    result.command_submit.frame_result.submit_attempted = true;
    result.command_submit.frame_result.submitted = true;
    result.command_submit.frame_result.present_ready = true;
    result.command_submit.frame_result.failure_mode =
        vulkan_command_submit_failure_mode::none;
    result.command_submit.frame_result.status =
        vulkan_command_submit_frame_status::submitted;
    result.command_submit.submitted_batch_count = readiness.planned_batch_count;
    result.command_submit.submit_attempted = true;
    result.command_submit.status = vulkan_command_submit_readiness_status::ready;
    result.command_submit.diagnostic =
        "Vulkan queue submit adapter submitted command buffer and presented image";
    result.diagnostic = result.command_submit.diagnostic;
    return result;
}

} // namespace quiz_vulkan::render::vulkan_backend
