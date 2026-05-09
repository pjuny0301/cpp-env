#pragma once

#include "render/vulkan/vulkan_backend_pipeline_layout.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

enum class vulkan_vertex_input_rate {
    vertex,
    instance,
};

inline std::string_view vertex_input_rate_name(vulkan_vertex_input_rate rate)
{
    switch (rate) {
    case vulkan_vertex_input_rate::vertex:
        return "vertex";
    case vulkan_vertex_input_rate::instance:
        return "instance";
    }

    return "unknown";
}

enum class vulkan_primitive_topology {
    triangle_list,
    triangle_strip,
    line_list,
};

inline std::string_view primitive_topology_name(vulkan_primitive_topology topology)
{
    switch (topology) {
    case vulkan_primitive_topology::triangle_list:
        return "triangle_list";
    case vulkan_primitive_topology::triangle_strip:
        return "triangle_strip";
    case vulkan_primitive_topology::line_list:
        return "line_list";
    }

    return "unknown";
}

enum class vulkan_polygon_mode {
    fill,
    line,
};

enum class vulkan_cull_mode {
    none,
    front,
    back,
};

enum class vulkan_front_face {
    clockwise,
    counter_clockwise,
};

enum class vulkan_blend_factor {
    zero,
    one,
    src_alpha,
    one_minus_src_alpha,
};

enum class vulkan_blend_op {
    add,
};

enum class vulkan_depth_compare_op {
    less,
    less_or_equal,
    always,
};

struct vulkan_graphics_pipeline_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_vertex_input_binding {
    std::size_t binding = 0;
    std::size_t stride = 0;
    vulkan_vertex_input_rate input_rate = vulkan_vertex_input_rate::vertex;

    bool valid() const
    {
        return stride > 0;
    }
};

struct vulkan_vertex_attribute {
    std::size_t location = 0;
    std::size_t binding = 0;
    std::size_t offset = 0;
    std::size_t byte_size = 0;

    std::size_t end_offset() const
    {
        return offset + byte_size;
    }

    bool valid() const
    {
        return byte_size > 0;
    }
};

struct vulkan_vertex_input_state {
    std::vector<vulkan_vertex_input_binding> bindings;
    std::vector<vulkan_vertex_attribute> attributes;

    const vulkan_vertex_input_binding* binding_for(std::size_t binding) const
    {
        for (const vulkan_vertex_input_binding& candidate : bindings) {
            if (candidate.binding == binding) {
                return &candidate;
            }
        }
        return nullptr;
    }

    bool has_duplicate_attribute_location() const
    {
        for (std::size_t outer = 0; outer < attributes.size(); ++outer) {
            for (std::size_t inner = outer + 1; inner < attributes.size(); ++inner) {
                if (attributes[outer].location == attributes[inner].location) {
                    return true;
                }
            }
        }
        return false;
    }

    bool valid() const
    {
        if (bindings.empty() || attributes.empty() || has_duplicate_attribute_location()) {
            return false;
        }
        for (const vulkan_vertex_input_binding& binding : bindings) {
            if (!binding.valid()) {
                return false;
            }
        }
        for (const vulkan_vertex_attribute& attribute : attributes) {
            const vulkan_vertex_input_binding* binding = binding_for(attribute.binding);
            if (!attribute.valid() || binding == nullptr || attribute.end_offset() > binding->stride) {
                return false;
            }
        }
        return true;
    }
};

struct vulkan_input_assembly_state {
    vulkan_primitive_topology topology = vulkan_primitive_topology::triangle_list;
    bool primitive_restart_enable = false;

    bool valid() const
    {
        return !primitive_restart_enable
            || topology == vulkan_primitive_topology::triangle_strip;
    }
};

struct vulkan_rasterization_state {
    vulkan_polygon_mode polygon_mode = vulkan_polygon_mode::fill;
    vulkan_cull_mode cull_mode = vulkan_cull_mode::back;
    vulkan_front_face front_face = vulkan_front_face::counter_clockwise;
    float line_width = 1.0F;

    bool valid() const
    {
        return line_width > 0.0F;
    }
};

struct vulkan_color_blend_attachment_state {
    bool blend_enable = false;
    vulkan_blend_factor src_color_factor = vulkan_blend_factor::one;
    vulkan_blend_factor dst_color_factor = vulkan_blend_factor::zero;
    vulkan_blend_op color_op = vulkan_blend_op::add;
    vulkan_blend_factor src_alpha_factor = vulkan_blend_factor::one;
    vulkan_blend_factor dst_alpha_factor = vulkan_blend_factor::zero;
    vulkan_blend_op alpha_op = vulkan_blend_op::add;
    std::uint8_t color_write_mask = 0x0f;

    bool valid() const
    {
        return color_write_mask != 0
            && (!blend_enable
                || src_color_factor != vulkan_blend_factor::zero
                || dst_color_factor != vulkan_blend_factor::zero
                || src_alpha_factor != vulkan_blend_factor::zero
                || dst_alpha_factor != vulkan_blend_factor::zero);
    }
};

struct vulkan_color_blend_state {
    std::vector<vulkan_color_blend_attachment_state> attachments;

    bool valid() const
    {
        if (attachments.empty()) {
            return false;
        }
        for (const vulkan_color_blend_attachment_state& attachment : attachments) {
            if (!attachment.valid()) {
                return false;
            }
        }
        return true;
    }
};

struct vulkan_depth_stencil_state {
    bool depth_test_enable = false;
    bool depth_write_enable = false;
    vulkan_depth_compare_op compare_op = vulkan_depth_compare_op::less_or_equal;
    bool depth_bounds_test_enable = false;
    float min_depth_bounds = 0.0F;
    float max_depth_bounds = 1.0F;

    bool valid() const
    {
        if (depth_write_enable && !depth_test_enable) {
            return false;
        }
        if (depth_bounds_test_enable
            && (min_depth_bounds < 0.0F || max_depth_bounds > 1.0F
                || min_depth_bounds > max_depth_bounds)) {
            return false;
        }
        return true;
    }
};

struct vulkan_graphics_pipeline_request {
    std::string pipeline_id = "default.graphics";
    std::vector<vulkan_shader_stage> required_shader_stages{
        vulkan_shader_stage::vertex,
        vulkan_shader_stage::fragment,
    };
    bool require_vertex_input = true;
    vulkan_vertex_input_state vertex_input;
    vulkan_input_assembly_state input_assembly;
    vulkan_rasterization_state rasterization;
    vulkan_color_blend_state color_blend;
    vulkan_depth_stencil_state depth_stencil;
};

enum class vulkan_graphics_pipeline_adapter_call_status {
    not_called,
    completed,
    failed_recoverable,
    failed_fatal,
};

inline std::string_view graphics_pipeline_adapter_call_status_name(
    vulkan_graphics_pipeline_adapter_call_status status)
{
    switch (status) {
    case vulkan_graphics_pipeline_adapter_call_status::not_called:
        return "not_called";
    case vulkan_graphics_pipeline_adapter_call_status::completed:
        return "completed";
    case vulkan_graphics_pipeline_adapter_call_status::failed_recoverable:
        return "failed_recoverable";
    case vulkan_graphics_pipeline_adapter_call_status::failed_fatal:
        return "failed_fatal";
    }

    return "unknown";
}

enum class vulkan_graphics_pipeline_create_status {
    not_requested,
    created,
    shader_module_unavailable,
    pipeline_layout_unavailable,
    incompatible_shader_stages,
    invalid_vertex_input_state,
    invalid_input_assembly_state,
    invalid_rasterization_state,
    invalid_blend_state,
    invalid_depth_state,
    create_failed_recoverable,
    create_failed_fatal,
};

inline std::string_view graphics_pipeline_create_status_name(
    vulkan_graphics_pipeline_create_status status)
{
    switch (status) {
    case vulkan_graphics_pipeline_create_status::not_requested:
        return "not_requested";
    case vulkan_graphics_pipeline_create_status::created:
        return "created";
    case vulkan_graphics_pipeline_create_status::shader_module_unavailable:
        return "shader_module_unavailable";
    case vulkan_graphics_pipeline_create_status::pipeline_layout_unavailable:
        return "pipeline_layout_unavailable";
    case vulkan_graphics_pipeline_create_status::incompatible_shader_stages:
        return "incompatible_shader_stages";
    case vulkan_graphics_pipeline_create_status::invalid_vertex_input_state:
        return "invalid_vertex_input_state";
    case vulkan_graphics_pipeline_create_status::invalid_input_assembly_state:
        return "invalid_input_assembly_state";
    case vulkan_graphics_pipeline_create_status::invalid_rasterization_state:
        return "invalid_rasterization_state";
    case vulkan_graphics_pipeline_create_status::invalid_blend_state:
        return "invalid_blend_state";
    case vulkan_graphics_pipeline_create_status::invalid_depth_state:
        return "invalid_depth_state";
    case vulkan_graphics_pipeline_create_status::create_failed_recoverable:
        return "create_failed_recoverable";
    case vulkan_graphics_pipeline_create_status::create_failed_fatal:
        return "create_failed_fatal";
    }

    return "unknown";
}

enum class vulkan_graphics_pipeline_destroy_status {
    not_requested,
    destroyed,
    invalid_handle,
    destroy_failed_recoverable,
    destroy_failed_fatal,
};

inline std::string_view graphics_pipeline_destroy_status_name(
    vulkan_graphics_pipeline_destroy_status status)
{
    switch (status) {
    case vulkan_graphics_pipeline_destroy_status::not_requested:
        return "not_requested";
    case vulkan_graphics_pipeline_destroy_status::destroyed:
        return "destroyed";
    case vulkan_graphics_pipeline_destroy_status::invalid_handle:
        return "invalid_handle";
    case vulkan_graphics_pipeline_destroy_status::destroy_failed_recoverable:
        return "destroy_failed_recoverable";
    case vulkan_graphics_pipeline_destroy_status::destroy_failed_fatal:
        return "destroy_failed_fatal";
    }

    return "unknown";
}

struct vulkan_graphics_pipeline_shader_stage_diagnostics {
    vulkan_shader_stage stage = vulkan_shader_stage::vertex;
    bool required = false;
    bool shader_module_ready = false;
    bool pipeline_layout_compatible = false;
    vulkan_shader_module_id shader_id;

    bool completed() const
    {
        return !required
            || (shader_module_ready && pipeline_layout_compatible && !shader_id.empty());
    }
};

struct vulkan_graphics_pipeline_fixed_function_diagnostics {
    bool checked = false;
    bool vertex_input_valid = false;
    bool input_assembly_valid = false;
    bool rasterization_valid = false;
    bool blend_valid = false;
    bool depth_valid = false;
    std::size_t vertex_binding_count = 0;
    std::size_t vertex_attribute_count = 0;
    std::size_t color_attachment_count = 0;

    bool completed() const
    {
        return checked && vertex_input_valid && input_assembly_valid && rasterization_valid
            && blend_valid && depth_valid;
    }
};

struct vulkan_graphics_pipeline_create_call {
    std::string pipeline_id;
    std::vector<vulkan_shader_module_id> shader_modules;
    vulkan_pipeline_layout_handle pipeline_layout;
    vulkan_vertex_input_state vertex_input;
    vulkan_input_assembly_state input_assembly;
    vulkan_rasterization_state rasterization;
    vulkan_color_blend_state color_blend;
    vulkan_depth_stencil_state depth_stencil;

    bool valid() const
    {
        return !pipeline_id.empty() && !shader_modules.empty() && pipeline_layout.valid()
            && vertex_input.valid() && input_assembly.valid() && rasterization.valid()
            && color_blend.valid() && depth_stencil.valid();
    }
};

struct vulkan_graphics_pipeline_destroy_call {
    vulkan_graphics_pipeline_handle handle;

    bool valid() const
    {
        return handle.valid();
    }
};

struct vulkan_graphics_pipeline_create_operation_result {
    bool checked = false;
    vulkan_graphics_pipeline_adapter_call_status status =
        vulkan_graphics_pipeline_adapter_call_status::not_called;
    vulkan_graphics_pipeline_handle handle;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_graphics_pipeline_adapter_call_status::completed
            && handle.valid();
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_graphics_pipeline_adapter_call_status::failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked && status == vulkan_graphics_pipeline_adapter_call_status::failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_graphics_pipeline_adapter_call_status::completed;
    }
};

struct vulkan_graphics_pipeline_destroy_operation_result {
    bool checked = false;
    vulkan_graphics_pipeline_adapter_call_status status =
        vulkan_graphics_pipeline_adapter_call_status::not_called;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_graphics_pipeline_adapter_call_status::completed;
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_graphics_pipeline_adapter_call_status::failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked && status == vulkan_graphics_pipeline_adapter_call_status::failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_graphics_pipeline_adapter_call_status::completed;
    }
};

using vulkan_graphics_pipeline_create_fn =
    vulkan_graphics_pipeline_create_operation_result (*)(
        void* user_data,
        const vulkan_graphics_pipeline_create_call& call);

using vulkan_graphics_pipeline_destroy_fn =
    vulkan_graphics_pipeline_destroy_operation_result (*)(
        void* user_data,
        const vulkan_graphics_pipeline_destroy_call& call);

struct vulkan_graphics_pipeline_function_table {
    vulkan_graphics_pipeline_create_fn create_graphics_pipeline = nullptr;
    vulkan_graphics_pipeline_destroy_fn destroy_graphics_pipeline = nullptr;

    bool valid() const
    {
        return create_graphics_pipeline != nullptr && destroy_graphics_pipeline != nullptr;
    }
};

struct vulkan_graphics_pipeline_factory_adapter {
    void* user_data = nullptr;
    vulkan_graphics_pipeline_function_table functions;

    bool valid() const
    {
        return user_data != nullptr && functions.valid();
    }
};

struct vulkan_graphics_pipeline_create_result {
    bool checked = false;
    vulkan_graphics_pipeline_create_status status =
        vulkan_graphics_pipeline_create_status::not_requested;
    vulkan_backend_shader_module_readiness_state shader_modules;
    vulkan_pipeline_layout_create_result pipeline_layout;
    vulkan_graphics_pipeline_request request;
    std::vector<vulkan_graphics_pipeline_shader_stage_diagnostics> shader_stage_diagnostics;
    vulkan_graphics_pipeline_fixed_function_diagnostics fixed_function;
    vulkan_graphics_pipeline_handle pipeline;
    vulkan_graphics_pipeline_create_call create_call;
    vulkan_graphics_pipeline_create_operation_result create_result;
    std::string diagnostic;

    bool ready_for_draw() const
    {
        return checked && status == vulkan_graphics_pipeline_create_status::created
            && pipeline.valid() && fixed_function.completed() && create_result.completed();
    }

    bool recoverable_create_failure() const
    {
        return checked
            && status == vulkan_graphics_pipeline_create_status::create_failed_recoverable;
    }

    bool fatal_create_failure() const
    {
        return checked && status == vulkan_graphics_pipeline_create_status::create_failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_graphics_pipeline_create_status::created;
    }
};

using vulkan_graphics_pipeline_readiness_result = vulkan_graphics_pipeline_create_result;

struct vulkan_graphics_pipeline_destroy_result {
    bool checked = false;
    vulkan_graphics_pipeline_destroy_status status =
        vulkan_graphics_pipeline_destroy_status::not_requested;
    vulkan_graphics_pipeline_handle pipeline;
    vulkan_graphics_pipeline_destroy_call destroy_call;
    vulkan_graphics_pipeline_destroy_operation_result destroy_result;
    std::size_t destroyed_pipeline_count = 0;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_graphics_pipeline_destroy_status::destroyed
            && destroy_result.completed() && destroyed_pipeline_count == 1;
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_graphics_pipeline_destroy_status::destroy_failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked && status == vulkan_graphics_pipeline_destroy_status::destroy_failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_graphics_pipeline_destroy_status::destroyed;
    }
};

enum class vulkan_backend_pipeline_readiness_summary_status {
    not_checked,
    ready,
    shader_module_unavailable,
    pipeline_layout_unavailable,
    graphics_pipeline_unavailable,
};

inline std::string_view pipeline_readiness_summary_status_name(
    vulkan_backend_pipeline_readiness_summary_status status)
{
    switch (status) {
    case vulkan_backend_pipeline_readiness_summary_status::not_checked:
        return "not_checked";
    case vulkan_backend_pipeline_readiness_summary_status::ready:
        return "ready";
    case vulkan_backend_pipeline_readiness_summary_status::shader_module_unavailable:
        return "shader_module_unavailable";
    case vulkan_backend_pipeline_readiness_summary_status::pipeline_layout_unavailable:
        return "pipeline_layout_unavailable";
    case vulkan_backend_pipeline_readiness_summary_status::graphics_pipeline_unavailable:
        return "graphics_pipeline_unavailable";
    }

    return "unknown";
}

struct vulkan_backend_pipeline_readiness_summary {
    bool checked = false;
    vulkan_backend_pipeline_readiness_summary_status status =
        vulkan_backend_pipeline_readiness_summary_status::not_checked;
    bool shader_modules_checked = false;
    bool shader_modules_ready = false;
    std::size_t requested_shader_module_count = 0;
    std::size_t created_shader_module_count = 0;
    std::size_t failed_shader_module_count = 0;
    bool pipeline_layout_checked = false;
    bool pipeline_layout_ready = false;
    vulkan_pipeline_layout_create_status pipeline_layout_status =
        vulkan_pipeline_layout_create_status::not_requested;
    bool graphics_pipeline_checked = false;
    bool graphics_pipeline_ready = false;
    vulkan_graphics_pipeline_create_status graphics_pipeline_status =
        vulkan_graphics_pipeline_create_status::not_requested;
    bool graphics_create_recoverable_failure = false;
    bool graphics_create_fatal_failure = false;
    bool pipeline_layout_destroy_checked = false;
    bool pipeline_layout_destroyed = false;
    std::size_t descriptor_set_layout_destroyed_count = 0;
    vulkan_pipeline_layout_destroy_status pipeline_layout_destroy_status =
        vulkan_pipeline_layout_destroy_status::not_requested;
    bool graphics_pipeline_destroy_checked = false;
    bool graphics_pipeline_destroyed = false;
    std::size_t graphics_pipeline_destroyed_count = 0;
    vulkan_graphics_pipeline_destroy_status graphics_pipeline_destroy_status =
        vulkan_graphics_pipeline_destroy_status::not_requested;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_backend_pipeline_readiness_summary_status::ready
            && shader_modules_ready && pipeline_layout_ready && graphics_pipeline_ready
            && !graphics_create_recoverable_failure && !graphics_create_fatal_failure;
    }

    bool failed() const
    {
        return checked && status != vulkan_backend_pipeline_readiness_summary_status::ready;
    }
};

struct fake_vulkan_graphics_pipeline_factory_state {
    std::size_t create_call_count = 0;
    std::size_t destroy_call_count = 0;
    vulkan_graphics_pipeline_create_call last_create_call;
    vulkan_graphics_pipeline_destroy_call last_destroy_call;
    std::vector<vulkan_graphics_pipeline_handle> destroyed_pipelines;
};

struct fake_vulkan_graphics_pipeline_factory_options {
    vulkan_graphics_pipeline_adapter_call_status create_status =
        vulkan_graphics_pipeline_adapter_call_status::completed;
    vulkan_graphics_pipeline_adapter_call_status destroy_status =
        vulkan_graphics_pipeline_adapter_call_status::completed;
    vulkan_graphics_pipeline_handle handle{.value = 1100};
};

class fake_vulkan_graphics_pipeline_factory final {
public:
    fake_vulkan_graphics_pipeline_factory();
    explicit fake_vulkan_graphics_pipeline_factory(
        fake_vulkan_graphics_pipeline_factory_options options);

    vulkan_graphics_pipeline_factory_adapter adapter();
    const fake_vulkan_graphics_pipeline_factory_state& state() const;

private:
    static vulkan_graphics_pipeline_create_operation_result create_callback(
        void* user_data,
        const vulkan_graphics_pipeline_create_call& call);
    static vulkan_graphics_pipeline_destroy_operation_result destroy_callback(
        void* user_data,
        const vulkan_graphics_pipeline_destroy_call& call);

    fake_vulkan_graphics_pipeline_factory_options options_;
    fake_vulkan_graphics_pipeline_factory_state state_;
};

namespace graphics_pipeline_detail {

inline vulkan_graphics_pipeline_create_operation_result make_create_result(
    vulkan_graphics_pipeline_adapter_call_status status,
    vulkan_graphics_pipeline_handle handle,
    std::string diagnostic)
{
    return vulkan_graphics_pipeline_create_operation_result{
        .checked = true,
        .status = status,
        .handle = handle,
        .diagnostic = std::move(diagnostic),
    };
}

inline vulkan_graphics_pipeline_destroy_operation_result make_destroy_result(
    vulkan_graphics_pipeline_adapter_call_status status,
    std::string diagnostic)
{
    return vulkan_graphics_pipeline_destroy_operation_result{
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

inline bool pipeline_layout_compatible_with_stage(
    const vulkan_pipeline_layout_create_result& pipeline_layout,
    vulkan_shader_stage stage)
{
    for (const vulkan_pipeline_layout_shader_stage_compatibility& compatibility :
         pipeline_layout.shader_stage_compatibility) {
        if (compatibility.stage == stage && compatibility.completed()) {
            return true;
        }
    }
    return false;
}

inline vulkan_graphics_pipeline_create_status status_for_call_status(
    vulkan_graphics_pipeline_adapter_call_status status)
{
    return status == vulkan_graphics_pipeline_adapter_call_status::failed_recoverable
        ? vulkan_graphics_pipeline_create_status::create_failed_recoverable
        : vulkan_graphics_pipeline_create_status::create_failed_fatal;
}

inline std::vector<vulkan_shader_module_id> ready_shader_ids(
    const std::vector<vulkan_graphics_pipeline_shader_stage_diagnostics>& diagnostics)
{
    std::vector<vulkan_shader_module_id> ids;
    ids.reserve(diagnostics.size());
    for (const vulkan_graphics_pipeline_shader_stage_diagnostics& diagnostic : diagnostics) {
        if (diagnostic.shader_module_ready) {
            ids.push_back(diagnostic.shader_id);
        }
    }
    return ids;
}

} // namespace graphics_pipeline_detail

inline fake_vulkan_graphics_pipeline_factory::fake_vulkan_graphics_pipeline_factory()
    : fake_vulkan_graphics_pipeline_factory(fake_vulkan_graphics_pipeline_factory_options{})
{
}

inline fake_vulkan_graphics_pipeline_factory::fake_vulkan_graphics_pipeline_factory(
    fake_vulkan_graphics_pipeline_factory_options options)
    : options_(options)
{
}

inline vulkan_graphics_pipeline_factory_adapter fake_vulkan_graphics_pipeline_factory::adapter()
{
    return vulkan_graphics_pipeline_factory_adapter{
        .user_data = this,
        .functions = vulkan_graphics_pipeline_function_table{
            .create_graphics_pipeline = &fake_vulkan_graphics_pipeline_factory::create_callback,
            .destroy_graphics_pipeline = &fake_vulkan_graphics_pipeline_factory::destroy_callback,
        },
    };
}

inline const fake_vulkan_graphics_pipeline_factory_state&
fake_vulkan_graphics_pipeline_factory::state() const
{
    return state_;
}

inline vulkan_graphics_pipeline_create_operation_result
fake_vulkan_graphics_pipeline_factory::create_callback(
    void* user_data,
    const vulkan_graphics_pipeline_create_call& call)
{
    auto& fake = *static_cast<fake_vulkan_graphics_pipeline_factory*>(user_data);
    ++fake.state_.create_call_count;
    fake.state_.last_create_call = call;
    return graphics_pipeline_detail::make_create_result(
        fake.options_.create_status,
        fake.options_.create_status == vulkan_graphics_pipeline_adapter_call_status::completed
            ? fake.options_.handle
            : vulkan_graphics_pipeline_handle{},
        fake.options_.create_status == vulkan_graphics_pipeline_adapter_call_status::completed
            ? "Vulkan graphics pipeline factory created graphics pipeline"
            : "Vulkan graphics pipeline factory failed graphics pipeline creation");
}

inline vulkan_graphics_pipeline_destroy_operation_result
fake_vulkan_graphics_pipeline_factory::destroy_callback(
    void* user_data,
    const vulkan_graphics_pipeline_destroy_call& call)
{
    auto& fake = *static_cast<fake_vulkan_graphics_pipeline_factory*>(user_data);
    ++fake.state_.destroy_call_count;
    fake.state_.last_destroy_call = call;
    fake.state_.destroyed_pipelines.push_back(call.handle);
    return graphics_pipeline_detail::make_destroy_result(
        fake.options_.destroy_status,
        fake.options_.destroy_status == vulkan_graphics_pipeline_adapter_call_status::completed
            ? "Vulkan graphics pipeline factory destroyed graphics pipeline"
            : "Vulkan graphics pipeline factory failed graphics pipeline destroy");
}

inline vulkan_graphics_pipeline_create_result create_vulkan_graphics_pipeline(
    vulkan_graphics_pipeline_factory_adapter adapter,
    const vulkan_backend_shader_module_readiness_state& shader_modules,
    const vulkan_pipeline_layout_create_result& pipeline_layout,
    const vulkan_graphics_pipeline_request& request)
{
    vulkan_graphics_pipeline_create_result result;
    result.checked = true;
    result.status = vulkan_graphics_pipeline_create_status::not_requested;
    result.shader_modules = shader_modules;
    result.pipeline_layout = pipeline_layout;
    result.request = request;

    if (!shader_modules.checked || !shader_modules.completed()) {
        result.status = vulkan_graphics_pipeline_create_status::shader_module_unavailable;
        result.diagnostic = "Vulkan graphics pipeline shader module readiness is unavailable";
        return result;
    }
    if (!pipeline_layout.checked || !pipeline_layout.ready_for_pipeline()) {
        result.status = vulkan_graphics_pipeline_create_status::pipeline_layout_unavailable;
        result.diagnostic = "Vulkan graphics pipeline layout readiness is unavailable";
        return result;
    }

    for (vulkan_shader_stage stage : request.required_shader_stages) {
        vulkan_graphics_pipeline_shader_stage_diagnostics diagnostics;
        diagnostics.stage = stage;
        diagnostics.required = true;
        diagnostics.shader_module_ready =
            graphics_pipeline_detail::shader_module_ready_for_stage(
                shader_modules,
                stage,
                diagnostics.shader_id);
        diagnostics.pipeline_layout_compatible =
            graphics_pipeline_detail::pipeline_layout_compatible_with_stage(
                pipeline_layout,
                stage);
        result.shader_stage_diagnostics.push_back(diagnostics);
        if (!diagnostics.shader_module_ready) {
            result.status = vulkan_graphics_pipeline_create_status::shader_module_unavailable;
            result.diagnostic = "Vulkan graphics pipeline missing shader module for stage: "
                + std::string{shader_stage_name(stage)};
            return result;
        }
        if (!diagnostics.pipeline_layout_compatible) {
            result.status = vulkan_graphics_pipeline_create_status::incompatible_shader_stages;
            result.diagnostic =
                "Vulkan graphics pipeline shader stage is incompatible with pipeline layout: "
                + std::string{shader_stage_name(stage)};
            return result;
        }
    }

    result.fixed_function = vulkan_graphics_pipeline_fixed_function_diagnostics{
        .checked = true,
        .vertex_input_valid = !request.require_vertex_input || request.vertex_input.valid(),
        .input_assembly_valid = request.input_assembly.valid(),
        .rasterization_valid = request.rasterization.valid(),
        .blend_valid = request.color_blend.valid(),
        .depth_valid = request.depth_stencil.valid(),
        .vertex_binding_count = request.vertex_input.bindings.size(),
        .vertex_attribute_count = request.vertex_input.attributes.size(),
        .color_attachment_count = request.color_blend.attachments.size(),
    };
    if (!result.fixed_function.vertex_input_valid) {
        result.status = vulkan_graphics_pipeline_create_status::invalid_vertex_input_state;
        result.diagnostic = "Vulkan graphics pipeline vertex input state is invalid";
        return result;
    }
    if (!result.fixed_function.input_assembly_valid) {
        result.status = vulkan_graphics_pipeline_create_status::invalid_input_assembly_state;
        result.diagnostic = "Vulkan graphics pipeline input assembly state is invalid";
        return result;
    }
    if (!result.fixed_function.rasterization_valid) {
        result.status = vulkan_graphics_pipeline_create_status::invalid_rasterization_state;
        result.diagnostic = "Vulkan graphics pipeline rasterization state is invalid";
        return result;
    }
    if (!result.fixed_function.blend_valid) {
        result.status = vulkan_graphics_pipeline_create_status::invalid_blend_state;
        result.diagnostic = "Vulkan graphics pipeline blend state is invalid";
        return result;
    }
    if (!result.fixed_function.depth_valid) {
        result.status = vulkan_graphics_pipeline_create_status::invalid_depth_state;
        result.diagnostic = "Vulkan graphics pipeline depth state is invalid";
        return result;
    }

    if (!adapter.valid()) {
        result.status = vulkan_graphics_pipeline_create_status::create_failed_fatal;
        result.diagnostic = "Vulkan graphics pipeline factory function table is unavailable";
        return result;
    }

    result.create_call = vulkan_graphics_pipeline_create_call{
        .pipeline_id = request.pipeline_id,
        .shader_modules = graphics_pipeline_detail::ready_shader_ids(
            result.shader_stage_diagnostics),
        .pipeline_layout = pipeline_layout.pipeline_layout,
        .vertex_input = request.vertex_input,
        .input_assembly = request.input_assembly,
        .rasterization = request.rasterization,
        .color_blend = request.color_blend,
        .depth_stencil = request.depth_stencil,
    };
    result.create_result = adapter.functions.create_graphics_pipeline(
        adapter.user_data,
        result.create_call);
    result.diagnostic = result.create_result.diagnostic;
    if (!result.create_result.completed()) {
        result.status = graphics_pipeline_detail::status_for_call_status(
            result.create_result.status);
        return result;
    }

    result.pipeline = result.create_result.handle;
    result.status = vulkan_graphics_pipeline_create_status::created;
    return result;
}

inline vulkan_graphics_pipeline_destroy_result destroy_vulkan_graphics_pipeline(
    vulkan_graphics_pipeline_factory_adapter adapter,
    vulkan_graphics_pipeline_handle pipeline)
{
    vulkan_graphics_pipeline_destroy_result result;
    result.checked = true;
    result.status = vulkan_graphics_pipeline_destroy_status::not_requested;
    result.pipeline = pipeline;

    if (!pipeline.valid()) {
        result.status = vulkan_graphics_pipeline_destroy_status::invalid_handle;
        result.diagnostic = "Vulkan graphics pipeline destroy received an invalid handle";
        return result;
    }
    if (!adapter.valid()) {
        result.status = vulkan_graphics_pipeline_destroy_status::destroy_failed_fatal;
        result.diagnostic = "Vulkan graphics pipeline factory function table is unavailable";
        return result;
    }

    result.destroy_call = vulkan_graphics_pipeline_destroy_call{.handle = pipeline};
    result.destroy_result = adapter.functions.destroy_graphics_pipeline(
        adapter.user_data,
        result.destroy_call);
    result.diagnostic = result.destroy_result.diagnostic;
    if (!result.destroy_result.completed()) {
        result.status = result.destroy_result.recoverable_failure()
            ? vulkan_graphics_pipeline_destroy_status::destroy_failed_recoverable
            : vulkan_graphics_pipeline_destroy_status::destroy_failed_fatal;
        return result;
    }

    result.destroyed_pipeline_count = 1;
    result.status = vulkan_graphics_pipeline_destroy_status::destroyed;
    return result;
}

inline vulkan_backend_pipeline_readiness_summary summarize_vulkan_pipeline_readiness(
    const vulkan_backend_shader_module_readiness_state& shader_modules,
    const vulkan_pipeline_layout_create_result& pipeline_layout,
    const vulkan_graphics_pipeline_create_result& graphics_pipeline,
    const vulkan_pipeline_layout_destroy_result& pipeline_layout_destroy,
    const vulkan_graphics_pipeline_destroy_result& graphics_pipeline_destroy)
{
    vulkan_backend_pipeline_readiness_summary summary;
    summary.checked = true;
    summary.shader_modules_checked = shader_modules.checked;
    summary.shader_modules_ready = shader_modules.checked && shader_modules.completed();
    summary.requested_shader_module_count = shader_modules.requested_module_count;
    summary.created_shader_module_count = shader_modules.created_module_count;
    summary.failed_shader_module_count = shader_modules.failed_module_count;
    summary.pipeline_layout_checked = pipeline_layout.checked;
    summary.pipeline_layout_ready = pipeline_layout.checked && pipeline_layout.ready_for_pipeline();
    summary.pipeline_layout_status = pipeline_layout.status;
    summary.graphics_pipeline_checked = graphics_pipeline.checked;
    summary.graphics_pipeline_ready =
        graphics_pipeline.checked && graphics_pipeline.ready_for_draw();
    summary.graphics_pipeline_status = graphics_pipeline.status;
    summary.graphics_create_recoverable_failure =
        graphics_pipeline.recoverable_create_failure();
    summary.graphics_create_fatal_failure = graphics_pipeline.fatal_create_failure();
    summary.pipeline_layout_destroy_checked = pipeline_layout_destroy.checked;
    summary.pipeline_layout_destroyed =
        pipeline_layout_destroy.checked && pipeline_layout_destroy.completed();
    summary.descriptor_set_layout_destroyed_count =
        pipeline_layout_destroy.descriptor_set_layout_destroyed_count;
    summary.pipeline_layout_destroy_status = pipeline_layout_destroy.status;
    summary.graphics_pipeline_destroy_checked = graphics_pipeline_destroy.checked;
    summary.graphics_pipeline_destroyed =
        graphics_pipeline_destroy.checked && graphics_pipeline_destroy.completed();
    summary.graphics_pipeline_destroyed_count =
        graphics_pipeline_destroy.destroyed_pipeline_count;
    summary.graphics_pipeline_destroy_status = graphics_pipeline_destroy.status;

    if (!summary.shader_modules_ready) {
        summary.status =
            vulkan_backend_pipeline_readiness_summary_status::shader_module_unavailable;
        summary.diagnostic = "Vulkan pipeline readiness summary shader modules are unavailable";
        return summary;
    }
    if (!summary.pipeline_layout_ready) {
        summary.status =
            vulkan_backend_pipeline_readiness_summary_status::pipeline_layout_unavailable;
        summary.diagnostic = !pipeline_layout.diagnostic.empty()
            ? pipeline_layout.diagnostic
            : "Vulkan pipeline readiness summary pipeline layout is unavailable";
        return summary;
    }
    if (!summary.graphics_pipeline_ready) {
        summary.status =
            vulkan_backend_pipeline_readiness_summary_status::graphics_pipeline_unavailable;
        summary.diagnostic = !graphics_pipeline.diagnostic.empty()
            ? graphics_pipeline.diagnostic
            : "Vulkan pipeline readiness summary graphics pipeline is unavailable";
        return summary;
    }

    summary.status = vulkan_backend_pipeline_readiness_summary_status::ready;
    summary.diagnostic = "Vulkan pipeline readiness summary is ready";
    return summary;
}

inline vulkan_backend_pipeline_readiness_summary summarize_vulkan_pipeline_readiness(
    const vulkan_backend_shader_module_readiness_state& shader_modules,
    const vulkan_pipeline_layout_create_result& pipeline_layout,
    const vulkan_graphics_pipeline_create_result& graphics_pipeline)
{
    return summarize_vulkan_pipeline_readiness(
        shader_modules,
        pipeline_layout,
        graphics_pipeline,
        vulkan_pipeline_layout_destroy_result{},
        vulkan_graphics_pipeline_destroy_result{});
}

} // namespace quiz_vulkan::render::vulkan_backend
