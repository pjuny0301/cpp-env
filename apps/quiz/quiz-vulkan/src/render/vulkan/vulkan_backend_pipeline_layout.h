#pragma once

#include "render/vulkan/vulkan_backend_shader_module.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

enum class vulkan_descriptor_set_layout_binding_kind {
    uniform_buffer,
    storage_buffer,
    sampled_image,
    sampler,
    combined_image_sampler,
};

inline std::string_view descriptor_set_layout_binding_kind_name(
    vulkan_descriptor_set_layout_binding_kind kind)
{
    switch (kind) {
    case vulkan_descriptor_set_layout_binding_kind::uniform_buffer:
        return "uniform_buffer";
    case vulkan_descriptor_set_layout_binding_kind::storage_buffer:
        return "storage_buffer";
    case vulkan_descriptor_set_layout_binding_kind::sampled_image:
        return "sampled_image";
    case vulkan_descriptor_set_layout_binding_kind::sampler:
        return "sampler";
    case vulkan_descriptor_set_layout_binding_kind::combined_image_sampler:
        return "combined_image_sampler";
    }

    return "unknown";
}

struct vulkan_descriptor_set_layout_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_pipeline_layout_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_descriptor_set_layout_binding_request {
    std::size_t set = 0;
    std::size_t binding = 0;
    vulkan_descriptor_set_layout_binding_kind kind =
        vulkan_descriptor_set_layout_binding_kind::uniform_buffer;
    std::vector<vulkan_shader_stage> shader_stages;
    std::size_t descriptor_count = 1;
    bool required = true;

    bool has_shader_stage(vulkan_shader_stage stage) const
    {
        for (vulkan_shader_stage shader_stage : shader_stages) {
            if (shader_stage == stage) {
                return true;
            }
        }
        return false;
    }

    bool valid() const
    {
        return descriptor_count > 0 && !shader_stages.empty();
    }
};

struct vulkan_descriptor_set_layout_request {
    std::size_t set = 0;
    std::vector<vulkan_descriptor_set_layout_binding_request> bindings;

    bool valid() const
    {
        if (bindings.empty()) {
            return false;
        }
        for (const vulkan_descriptor_set_layout_binding_request& binding : bindings) {
            if (binding.set != set || !binding.valid()) {
                return false;
            }
        }
        return true;
    }
};

struct vulkan_push_constant_range {
    std::size_t offset = 0;
    std::size_t size = 0;
    std::vector<vulkan_shader_stage> shader_stages;

    std::size_t end() const
    {
        return offset + size;
    }

    bool overlaps(const vulkan_push_constant_range& other) const
    {
        return size > 0 && other.size > 0
            && offset < other.end() && other.offset < end();
    }

    bool valid(std::size_t max_size) const
    {
        return size > 0 && !shader_stages.empty() && end() <= max_size;
    }
};

struct vulkan_pipeline_layout_request {
    std::vector<vulkan_descriptor_set_layout_request> descriptor_sets;
    std::vector<vulkan_push_constant_range> push_constant_ranges;
    std::vector<vulkan_shader_stage> required_shader_stages{
        vulkan_shader_stage::vertex,
        vulkan_shader_stage::fragment,
    };
    std::size_t max_push_constant_bytes = 128;
};

enum class vulkan_pipeline_layout_adapter_call_status {
    not_called,
    completed,
    failed_recoverable,
    failed_fatal,
};

inline std::string_view pipeline_layout_adapter_call_status_name(
    vulkan_pipeline_layout_adapter_call_status status)
{
    switch (status) {
    case vulkan_pipeline_layout_adapter_call_status::not_called:
        return "not_called";
    case vulkan_pipeline_layout_adapter_call_status::completed:
        return "completed";
    case vulkan_pipeline_layout_adapter_call_status::failed_recoverable:
        return "failed_recoverable";
    case vulkan_pipeline_layout_adapter_call_status::failed_fatal:
        return "failed_fatal";
    }

    return "unknown";
}

enum class vulkan_pipeline_layout_create_status {
    not_requested,
    created,
    missing_shader_stage,
    duplicate_binding,
    invalid_binding,
    push_constant_overlap,
    push_constant_size_exceeded,
    create_failed_recoverable,
    create_failed_fatal,
};

inline std::string_view pipeline_layout_create_status_name(
    vulkan_pipeline_layout_create_status status)
{
    switch (status) {
    case vulkan_pipeline_layout_create_status::not_requested:
        return "not_requested";
    case vulkan_pipeline_layout_create_status::created:
        return "created";
    case vulkan_pipeline_layout_create_status::missing_shader_stage:
        return "missing_shader_stage";
    case vulkan_pipeline_layout_create_status::duplicate_binding:
        return "duplicate_binding";
    case vulkan_pipeline_layout_create_status::invalid_binding:
        return "invalid_binding";
    case vulkan_pipeline_layout_create_status::push_constant_overlap:
        return "push_constant_overlap";
    case vulkan_pipeline_layout_create_status::push_constant_size_exceeded:
        return "push_constant_size_exceeded";
    case vulkan_pipeline_layout_create_status::create_failed_recoverable:
        return "create_failed_recoverable";
    case vulkan_pipeline_layout_create_status::create_failed_fatal:
        return "create_failed_fatal";
    }

    return "unknown";
}

enum class vulkan_pipeline_layout_destroy_status {
    not_requested,
    destroyed,
    invalid_handle,
    destroy_failed_recoverable,
    destroy_failed_fatal,
};

inline std::string_view pipeline_layout_destroy_status_name(
    vulkan_pipeline_layout_destroy_status status)
{
    switch (status) {
    case vulkan_pipeline_layout_destroy_status::not_requested:
        return "not_requested";
    case vulkan_pipeline_layout_destroy_status::destroyed:
        return "destroyed";
    case vulkan_pipeline_layout_destroy_status::invalid_handle:
        return "invalid_handle";
    case vulkan_pipeline_layout_destroy_status::destroy_failed_recoverable:
        return "destroy_failed_recoverable";
    case vulkan_pipeline_layout_destroy_status::destroy_failed_fatal:
        return "destroy_failed_fatal";
    }

    return "unknown";
}

struct vulkan_pipeline_layout_shader_stage_compatibility {
    vulkan_shader_stage stage = vulkan_shader_stage::vertex;
    bool required = false;
    bool shader_module_ready = false;
    vulkan_shader_module_id shader_id;

    bool completed() const
    {
        return !required || (shader_module_ready && !shader_id.empty());
    }
};

struct vulkan_descriptor_set_layout_binding_diagnostics {
    std::size_t set = 0;
    std::size_t binding = 0;
    vulkan_descriptor_set_layout_binding_kind kind =
        vulkan_descriptor_set_layout_binding_kind::uniform_buffer;
    std::size_t descriptor_count = 0;
    bool set_matches_layout = false;
    bool shader_stages_declared = false;
    bool descriptor_count_valid = false;
    bool duplicate_binding = false;

    bool completed() const
    {
        return set_matches_layout && shader_stages_declared && descriptor_count_valid
            && !duplicate_binding;
    }
};

struct vulkan_push_constant_range_diagnostics {
    std::size_t offset = 0;
    std::size_t size = 0;
    std::size_t end = 0;
    bool shader_stages_declared = false;
    bool size_valid = false;
    bool overlaps_previous_range = false;

    bool completed() const
    {
        return shader_stages_declared && size_valid && !overlaps_previous_range;
    }
};

struct vulkan_descriptor_set_layout_create_call {
    std::size_t set = 0;
    std::vector<vulkan_descriptor_set_layout_binding_request> bindings;

    bool valid() const
    {
        return !bindings.empty();
    }
};

struct vulkan_pipeline_layout_create_call {
    std::vector<vulkan_descriptor_set_layout_handle> descriptor_set_layouts;
    std::vector<vulkan_push_constant_range> push_constant_ranges;
    std::vector<vulkan_shader_stage> required_shader_stages;

    bool valid() const
    {
        if (descriptor_set_layouts.empty() || required_shader_stages.empty()) {
            return false;
        }
        for (const vulkan_descriptor_set_layout_handle& handle : descriptor_set_layouts) {
            if (!handle.valid()) {
                return false;
            }
        }
        return true;
    }
};

struct vulkan_descriptor_set_layout_destroy_call {
    vulkan_descriptor_set_layout_handle handle;

    bool valid() const
    {
        return handle.valid();
    }
};

struct vulkan_pipeline_layout_destroy_call {
    vulkan_pipeline_layout_handle handle;

    bool valid() const
    {
        return handle.valid();
    }
};

struct vulkan_descriptor_set_layout_create_operation_result {
    bool checked = false;
    vulkan_pipeline_layout_adapter_call_status status =
        vulkan_pipeline_layout_adapter_call_status::not_called;
    vulkan_descriptor_set_layout_handle handle;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_pipeline_layout_adapter_call_status::completed
            && handle.valid();
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_pipeline_layout_adapter_call_status::failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked && status == vulkan_pipeline_layout_adapter_call_status::failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_pipeline_layout_adapter_call_status::completed;
    }
};

struct vulkan_pipeline_layout_create_operation_result {
    bool checked = false;
    vulkan_pipeline_layout_adapter_call_status status =
        vulkan_pipeline_layout_adapter_call_status::not_called;
    vulkan_pipeline_layout_handle handle;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_pipeline_layout_adapter_call_status::completed
            && handle.valid();
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_pipeline_layout_adapter_call_status::failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked && status == vulkan_pipeline_layout_adapter_call_status::failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_pipeline_layout_adapter_call_status::completed;
    }
};

struct vulkan_pipeline_layout_destroy_operation_result {
    bool checked = false;
    vulkan_pipeline_layout_adapter_call_status status =
        vulkan_pipeline_layout_adapter_call_status::not_called;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_pipeline_layout_adapter_call_status::completed;
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_pipeline_layout_adapter_call_status::failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked && status == vulkan_pipeline_layout_adapter_call_status::failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_pipeline_layout_adapter_call_status::completed;
    }
};

using vulkan_descriptor_set_layout_create_fn =
    vulkan_descriptor_set_layout_create_operation_result (*)(
        void* user_data,
        const vulkan_descriptor_set_layout_create_call& call);

using vulkan_pipeline_layout_create_fn =
    vulkan_pipeline_layout_create_operation_result (*)(
        void* user_data,
        const vulkan_pipeline_layout_create_call& call);

using vulkan_descriptor_set_layout_destroy_fn =
    vulkan_pipeline_layout_destroy_operation_result (*)(
        void* user_data,
        const vulkan_descriptor_set_layout_destroy_call& call);

using vulkan_pipeline_layout_destroy_fn =
    vulkan_pipeline_layout_destroy_operation_result (*)(
        void* user_data,
        const vulkan_pipeline_layout_destroy_call& call);

struct vulkan_pipeline_layout_function_table {
    vulkan_descriptor_set_layout_create_fn create_descriptor_set_layout = nullptr;
    vulkan_descriptor_set_layout_destroy_fn destroy_descriptor_set_layout = nullptr;
    vulkan_pipeline_layout_create_fn create_pipeline_layout = nullptr;
    vulkan_pipeline_layout_destroy_fn destroy_pipeline_layout = nullptr;

    bool valid() const
    {
        return create_descriptor_set_layout != nullptr
            && destroy_descriptor_set_layout != nullptr
            && create_pipeline_layout != nullptr
            && destroy_pipeline_layout != nullptr;
    }
};

struct vulkan_pipeline_layout_factory_adapter {
    void* user_data = nullptr;
    vulkan_pipeline_layout_function_table functions;

    bool valid() const
    {
        return user_data != nullptr && functions.valid();
    }
};

struct vulkan_pipeline_layout_create_result {
    bool checked = false;
    vulkan_pipeline_layout_create_status status =
        vulkan_pipeline_layout_create_status::not_requested;
    vulkan_backend_shader_module_readiness_state shader_modules;
    vulkan_pipeline_layout_request request;
    std::vector<vulkan_pipeline_layout_shader_stage_compatibility> shader_stage_compatibility;
    std::vector<vulkan_descriptor_set_layout_binding_diagnostics> binding_diagnostics;
    std::vector<vulkan_push_constant_range_diagnostics> push_constant_diagnostics;
    std::vector<vulkan_descriptor_set_layout_handle> descriptor_set_layouts;
    vulkan_pipeline_layout_handle pipeline_layout;
    std::vector<vulkan_descriptor_set_layout_create_operation_result>
        descriptor_set_layout_create_results;
    vulkan_pipeline_layout_create_operation_result pipeline_layout_create_result;
    std::string diagnostic;

    bool ready_for_pipeline() const
    {
        return checked && status == vulkan_pipeline_layout_create_status::created
            && pipeline_layout.valid() && pipeline_layout_create_result.completed();
    }

    bool recoverable_create_failure() const
    {
        return checked
            && status == vulkan_pipeline_layout_create_status::create_failed_recoverable;
    }

    bool fatal_create_failure() const
    {
        return checked && status == vulkan_pipeline_layout_create_status::create_failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_pipeline_layout_create_status::created;
    }
};

using vulkan_pipeline_layout_readiness_result = vulkan_pipeline_layout_create_result;

struct vulkan_pipeline_layout_destroy_result {
    bool checked = false;
    vulkan_pipeline_layout_destroy_status status =
        vulkan_pipeline_layout_destroy_status::not_requested;
    vulkan_pipeline_layout_handle pipeline_layout;
    std::vector<vulkan_descriptor_set_layout_handle> descriptor_set_layouts;
    vulkan_pipeline_layout_destroy_operation_result pipeline_layout_destroy_result;
    std::vector<vulkan_pipeline_layout_destroy_operation_result>
        descriptor_set_layout_destroy_results;
    std::size_t descriptor_set_layout_destroyed_count = 0;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_pipeline_layout_destroy_status::destroyed
            && pipeline_layout_destroy_result.completed()
            && descriptor_set_layout_destroyed_count == descriptor_set_layouts.size();
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_pipeline_layout_destroy_status::destroy_failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked && status == vulkan_pipeline_layout_destroy_status::destroy_failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_pipeline_layout_destroy_status::destroyed;
    }
};

struct fake_vulkan_pipeline_layout_factory_state {
    std::size_t descriptor_set_layout_create_call_count = 0;
    std::size_t pipeline_layout_create_call_count = 0;
    std::size_t descriptor_set_layout_destroy_call_count = 0;
    std::size_t pipeline_layout_destroy_call_count = 0;
    vulkan_descriptor_set_layout_create_call last_descriptor_set_layout_create_call;
    vulkan_pipeline_layout_create_call last_pipeline_layout_create_call;
    vulkan_descriptor_set_layout_destroy_call last_descriptor_set_layout_destroy_call;
    vulkan_pipeline_layout_destroy_call last_pipeline_layout_destroy_call;
    std::vector<vulkan_descriptor_set_layout_handle> destroyed_descriptor_set_layouts;
};

struct fake_vulkan_pipeline_layout_factory_options {
    vulkan_pipeline_layout_adapter_call_status descriptor_set_layout_create_status =
        vulkan_pipeline_layout_adapter_call_status::completed;
    vulkan_pipeline_layout_adapter_call_status pipeline_layout_create_status =
        vulkan_pipeline_layout_adapter_call_status::completed;
    vulkan_pipeline_layout_adapter_call_status destroy_status =
        vulkan_pipeline_layout_adapter_call_status::completed;
    std::uintptr_t descriptor_set_layout_handle_base = 900;
    vulkan_pipeline_layout_handle pipeline_layout_handle{.value = 950};
};

class fake_vulkan_pipeline_layout_factory final {
public:
    fake_vulkan_pipeline_layout_factory();
    explicit fake_vulkan_pipeline_layout_factory(
        fake_vulkan_pipeline_layout_factory_options options);

    vulkan_pipeline_layout_factory_adapter adapter();
    const fake_vulkan_pipeline_layout_factory_state& state() const;

private:
    static vulkan_descriptor_set_layout_create_operation_result create_descriptor_set_layout_callback(
        void* user_data,
        const vulkan_descriptor_set_layout_create_call& call);
    static vulkan_pipeline_layout_create_operation_result create_pipeline_layout_callback(
        void* user_data,
        const vulkan_pipeline_layout_create_call& call);
    static vulkan_pipeline_layout_destroy_operation_result destroy_descriptor_set_layout_callback(
        void* user_data,
        const vulkan_descriptor_set_layout_destroy_call& call);
    static vulkan_pipeline_layout_destroy_operation_result destroy_pipeline_layout_callback(
        void* user_data,
        const vulkan_pipeline_layout_destroy_call& call);

    fake_vulkan_pipeline_layout_factory_options options_;
    fake_vulkan_pipeline_layout_factory_state state_;
};

namespace pipeline_layout_detail {

inline vulkan_descriptor_set_layout_create_operation_result make_descriptor_create_result(
    vulkan_pipeline_layout_adapter_call_status status,
    vulkan_descriptor_set_layout_handle handle,
    std::string diagnostic)
{
    return vulkan_descriptor_set_layout_create_operation_result{
        .checked = true,
        .status = status,
        .handle = handle,
        .diagnostic = std::move(diagnostic),
    };
}

inline vulkan_pipeline_layout_create_operation_result make_pipeline_create_result(
    vulkan_pipeline_layout_adapter_call_status status,
    vulkan_pipeline_layout_handle handle,
    std::string diagnostic)
{
    return vulkan_pipeline_layout_create_operation_result{
        .checked = true,
        .status = status,
        .handle = handle,
        .diagnostic = std::move(diagnostic),
    };
}

inline vulkan_pipeline_layout_destroy_operation_result make_destroy_result(
    vulkan_pipeline_layout_adapter_call_status status,
    std::string diagnostic)
{
    return vulkan_pipeline_layout_destroy_operation_result{
        .checked = true,
        .status = status,
        .diagnostic = std::move(diagnostic),
    };
}

inline bool shader_module_ready_for_stage(
    const vulkan_backend_shader_module_readiness_state& shader_modules,
    vulkan_shader_stage stage,
    vulkan_shader_module_id& shader_id)
{
    for (const vulkan_shader_module_create_result& module : shader_modules.modules) {
        if (module.stage == stage && module.ready_for_pipeline()) {
            shader_id = module.id;
            return true;
        }
    }
    return false;
}

inline bool binding_pair_seen(
    const std::vector<vulkan_descriptor_set_layout_binding_diagnostics>& bindings,
    std::size_t set,
    std::size_t binding)
{
    for (const vulkan_descriptor_set_layout_binding_diagnostics& seen : bindings) {
        if (seen.set == set && seen.binding == binding) {
            return true;
        }
    }
    return false;
}

inline vulkan_pipeline_layout_create_status status_for_call_status(
    vulkan_pipeline_layout_adapter_call_status status)
{
    return status == vulkan_pipeline_layout_adapter_call_status::failed_recoverable
        ? vulkan_pipeline_layout_create_status::create_failed_recoverable
        : vulkan_pipeline_layout_create_status::create_failed_fatal;
}

} // namespace pipeline_layout_detail

inline fake_vulkan_pipeline_layout_factory::fake_vulkan_pipeline_layout_factory()
    : fake_vulkan_pipeline_layout_factory(fake_vulkan_pipeline_layout_factory_options{})
{
}

inline fake_vulkan_pipeline_layout_factory::fake_vulkan_pipeline_layout_factory(
    fake_vulkan_pipeline_layout_factory_options options)
    : options_(options)
{
}

inline vulkan_pipeline_layout_factory_adapter fake_vulkan_pipeline_layout_factory::adapter()
{
    return vulkan_pipeline_layout_factory_adapter{
        .user_data = this,
        .functions = vulkan_pipeline_layout_function_table{
            .create_descriptor_set_layout =
                &fake_vulkan_pipeline_layout_factory::create_descriptor_set_layout_callback,
            .destroy_descriptor_set_layout =
                &fake_vulkan_pipeline_layout_factory::destroy_descriptor_set_layout_callback,
            .create_pipeline_layout =
                &fake_vulkan_pipeline_layout_factory::create_pipeline_layout_callback,
            .destroy_pipeline_layout =
                &fake_vulkan_pipeline_layout_factory::destroy_pipeline_layout_callback,
        },
    };
}

inline const fake_vulkan_pipeline_layout_factory_state&
fake_vulkan_pipeline_layout_factory::state() const
{
    return state_;
}

inline vulkan_descriptor_set_layout_create_operation_result
fake_vulkan_pipeline_layout_factory::create_descriptor_set_layout_callback(
    void* user_data,
    const vulkan_descriptor_set_layout_create_call& call)
{
    auto& fake = *static_cast<fake_vulkan_pipeline_layout_factory*>(user_data);
    ++fake.state_.descriptor_set_layout_create_call_count;
    fake.state_.last_descriptor_set_layout_create_call = call;
    const vulkan_descriptor_set_layout_handle handle{
        .value = fake.options_.descriptor_set_layout_handle_base
            + fake.state_.descriptor_set_layout_create_call_count - 1,
    };
    return pipeline_layout_detail::make_descriptor_create_result(
        fake.options_.descriptor_set_layout_create_status,
        fake.options_.descriptor_set_layout_create_status
                == vulkan_pipeline_layout_adapter_call_status::completed
            ? handle
            : vulkan_descriptor_set_layout_handle{},
        fake.options_.descriptor_set_layout_create_status
                == vulkan_pipeline_layout_adapter_call_status::completed
            ? "Vulkan descriptor set layout factory created descriptor set layout"
            : "Vulkan descriptor set layout factory failed descriptor set layout creation");
}

inline vulkan_pipeline_layout_create_operation_result
fake_vulkan_pipeline_layout_factory::create_pipeline_layout_callback(
    void* user_data,
    const vulkan_pipeline_layout_create_call& call)
{
    auto& fake = *static_cast<fake_vulkan_pipeline_layout_factory*>(user_data);
    ++fake.state_.pipeline_layout_create_call_count;
    fake.state_.last_pipeline_layout_create_call = call;
    return pipeline_layout_detail::make_pipeline_create_result(
        fake.options_.pipeline_layout_create_status,
        fake.options_.pipeline_layout_create_status
                == vulkan_pipeline_layout_adapter_call_status::completed
            ? fake.options_.pipeline_layout_handle
            : vulkan_pipeline_layout_handle{},
        fake.options_.pipeline_layout_create_status
                == vulkan_pipeline_layout_adapter_call_status::completed
            ? "Vulkan pipeline layout factory created pipeline layout"
            : "Vulkan pipeline layout factory failed pipeline layout creation");
}

inline vulkan_pipeline_layout_destroy_operation_result
fake_vulkan_pipeline_layout_factory::destroy_descriptor_set_layout_callback(
    void* user_data,
    const vulkan_descriptor_set_layout_destroy_call& call)
{
    auto& fake = *static_cast<fake_vulkan_pipeline_layout_factory*>(user_data);
    ++fake.state_.descriptor_set_layout_destroy_call_count;
    fake.state_.last_descriptor_set_layout_destroy_call = call;
    fake.state_.destroyed_descriptor_set_layouts.push_back(call.handle);
    return pipeline_layout_detail::make_destroy_result(
        fake.options_.destroy_status,
        fake.options_.destroy_status == vulkan_pipeline_layout_adapter_call_status::completed
            ? "Vulkan descriptor set layout factory destroyed descriptor set layout"
            : "Vulkan descriptor set layout factory failed descriptor set layout destroy");
}

inline vulkan_pipeline_layout_destroy_operation_result
fake_vulkan_pipeline_layout_factory::destroy_pipeline_layout_callback(
    void* user_data,
    const vulkan_pipeline_layout_destroy_call& call)
{
    auto& fake = *static_cast<fake_vulkan_pipeline_layout_factory*>(user_data);
    ++fake.state_.pipeline_layout_destroy_call_count;
    fake.state_.last_pipeline_layout_destroy_call = call;
    return pipeline_layout_detail::make_destroy_result(
        fake.options_.destroy_status,
        fake.options_.destroy_status == vulkan_pipeline_layout_adapter_call_status::completed
            ? "Vulkan pipeline layout factory destroyed pipeline layout"
            : "Vulkan pipeline layout factory failed pipeline layout destroy");
}

inline vulkan_pipeline_layout_create_result create_vulkan_pipeline_layout(
    vulkan_pipeline_layout_factory_adapter adapter,
    const vulkan_backend_shader_module_readiness_state& shader_modules,
    const vulkan_pipeline_layout_request& request)
{
    vulkan_pipeline_layout_create_result result;
    result.checked = true;
    result.status = vulkan_pipeline_layout_create_status::not_requested;
    result.shader_modules = shader_modules;
    result.request = request;

    for (vulkan_shader_stage required_stage : request.required_shader_stages) {
        vulkan_pipeline_layout_shader_stage_compatibility compatibility;
        compatibility.stage = required_stage;
        compatibility.required = true;
        compatibility.shader_module_ready =
            pipeline_layout_detail::shader_module_ready_for_stage(
                shader_modules,
                required_stage,
                compatibility.shader_id);
        result.shader_stage_compatibility.push_back(compatibility);
        if (!compatibility.completed()) {
            result.status = vulkan_pipeline_layout_create_status::missing_shader_stage;
            result.diagnostic = "Vulkan pipeline layout missing shader stage compatibility: "
                + std::string{shader_stage_name(required_stage)};
            return result;
        }
    }

    for (const vulkan_descriptor_set_layout_request& descriptor_set : request.descriptor_sets) {
        for (const vulkan_descriptor_set_layout_binding_request& binding : descriptor_set.bindings) {
            vulkan_descriptor_set_layout_binding_diagnostics diagnostics;
            diagnostics.set = binding.set;
            diagnostics.binding = binding.binding;
            diagnostics.kind = binding.kind;
            diagnostics.descriptor_count = binding.descriptor_count;
            diagnostics.set_matches_layout = binding.set == descriptor_set.set;
            diagnostics.shader_stages_declared = !binding.shader_stages.empty();
            diagnostics.descriptor_count_valid = binding.descriptor_count > 0;
            diagnostics.duplicate_binding = pipeline_layout_detail::binding_pair_seen(
                result.binding_diagnostics,
                binding.set,
                binding.binding);
            result.binding_diagnostics.push_back(diagnostics);
            if (diagnostics.duplicate_binding) {
                result.status = vulkan_pipeline_layout_create_status::duplicate_binding;
                result.diagnostic = "Vulkan pipeline layout descriptor binding is duplicated";
                return result;
            }
            if (!diagnostics.completed()) {
                result.status = vulkan_pipeline_layout_create_status::invalid_binding;
                result.diagnostic = "Vulkan pipeline layout descriptor binding is invalid";
                return result;
            }
        }
    }

    for (const vulkan_push_constant_range& range : request.push_constant_ranges) {
        vulkan_push_constant_range_diagnostics diagnostics;
        diagnostics.offset = range.offset;
        diagnostics.size = range.size;
        diagnostics.end = range.end();
        diagnostics.shader_stages_declared = !range.shader_stages.empty();
        diagnostics.size_valid = range.size > 0 && range.end() <= request.max_push_constant_bytes;
        for (const vulkan_push_constant_range_diagnostics& previous :
             result.push_constant_diagnostics) {
            const vulkan_push_constant_range previous_range{
                .offset = previous.offset,
                .size = previous.size,
                .shader_stages = {},
            };
            if (range.overlaps(previous_range)) {
                diagnostics.overlaps_previous_range = true;
                break;
            }
        }
        result.push_constant_diagnostics.push_back(diagnostics);
        if (diagnostics.overlaps_previous_range) {
            result.status = vulkan_pipeline_layout_create_status::push_constant_overlap;
            result.diagnostic = "Vulkan pipeline layout push constant ranges overlap";
            return result;
        }
        if (!diagnostics.size_valid || !diagnostics.shader_stages_declared) {
            result.status = vulkan_pipeline_layout_create_status::push_constant_size_exceeded;
            result.diagnostic = "Vulkan pipeline layout push constant range exceeds limit";
            return result;
        }
    }

    if (!adapter.valid()) {
        result.status = vulkan_pipeline_layout_create_status::create_failed_fatal;
        result.diagnostic = "Vulkan pipeline layout factory function table is unavailable";
        return result;
    }

    for (const vulkan_descriptor_set_layout_request& descriptor_set : request.descriptor_sets) {
        vulkan_descriptor_set_layout_create_call call;
        call.set = descriptor_set.set;
        call.bindings = descriptor_set.bindings;
        vulkan_descriptor_set_layout_create_operation_result create_result =
            adapter.functions.create_descriptor_set_layout(adapter.user_data, call);
        result.descriptor_set_layout_create_results.push_back(create_result);
        result.diagnostic = create_result.diagnostic;
        if (!create_result.completed()) {
            result.status = pipeline_layout_detail::status_for_call_status(create_result.status);
            return result;
        }
        result.descriptor_set_layouts.push_back(create_result.handle);
    }

    vulkan_pipeline_layout_create_call create_call;
    create_call.descriptor_set_layouts = result.descriptor_set_layouts;
    create_call.push_constant_ranges = request.push_constant_ranges;
    create_call.required_shader_stages = request.required_shader_stages;
    result.pipeline_layout_create_result =
        adapter.functions.create_pipeline_layout(adapter.user_data, create_call);
    result.diagnostic = result.pipeline_layout_create_result.diagnostic;
    if (!result.pipeline_layout_create_result.completed()) {
        result.status = pipeline_layout_detail::status_for_call_status(
            result.pipeline_layout_create_result.status);
        return result;
    }

    result.pipeline_layout = result.pipeline_layout_create_result.handle;
    result.status = vulkan_pipeline_layout_create_status::created;
    return result;
}

inline vulkan_pipeline_layout_destroy_result destroy_vulkan_pipeline_layout(
    vulkan_pipeline_layout_factory_adapter adapter,
    const vulkan_pipeline_layout_create_result& create_result)
{
    vulkan_pipeline_layout_destroy_result result;
    result.checked = true;
    result.status = vulkan_pipeline_layout_destroy_status::not_requested;
    result.pipeline_layout = create_result.pipeline_layout;
    result.descriptor_set_layouts = create_result.descriptor_set_layouts;

    if (!create_result.pipeline_layout.valid()) {
        result.status = vulkan_pipeline_layout_destroy_status::invalid_handle;
        result.diagnostic = "Vulkan pipeline layout destroy received an invalid handle";
        return result;
    }
    if (!adapter.valid()) {
        result.status = vulkan_pipeline_layout_destroy_status::destroy_failed_fatal;
        result.diagnostic = "Vulkan pipeline layout factory function table is unavailable";
        return result;
    }

    result.pipeline_layout_destroy_result = adapter.functions.destroy_pipeline_layout(
        adapter.user_data,
        vulkan_pipeline_layout_destroy_call{.handle = create_result.pipeline_layout});
    result.diagnostic = result.pipeline_layout_destroy_result.diagnostic;
    if (!result.pipeline_layout_destroy_result.completed()) {
        result.status = result.pipeline_layout_destroy_result.recoverable_failure()
            ? vulkan_pipeline_layout_destroy_status::destroy_failed_recoverable
            : vulkan_pipeline_layout_destroy_status::destroy_failed_fatal;
        return result;
    }

    for (const vulkan_descriptor_set_layout_handle& handle : create_result.descriptor_set_layouts) {
        vulkan_pipeline_layout_destroy_operation_result destroy_result =
            adapter.functions.destroy_descriptor_set_layout(
                adapter.user_data,
                vulkan_descriptor_set_layout_destroy_call{.handle = handle});
        result.descriptor_set_layout_destroy_results.push_back(destroy_result);
        result.diagnostic = destroy_result.diagnostic;
        if (!destroy_result.completed()) {
            result.status = destroy_result.recoverable_failure()
                ? vulkan_pipeline_layout_destroy_status::destroy_failed_recoverable
                : vulkan_pipeline_layout_destroy_status::destroy_failed_fatal;
            return result;
        }
        ++result.descriptor_set_layout_destroyed_count;
    }

    result.status = vulkan_pipeline_layout_destroy_status::destroyed;
    return result;
}

} // namespace quiz_vulkan::render::vulkan_backend
