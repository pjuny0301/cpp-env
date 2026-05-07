#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

struct vulkan_shader_module_id {
    std::string value;

    bool empty() const
    {
        return value.empty();
    }
};

enum class vulkan_shader_stage {
    vertex,
    fragment,
    compute,
};

std::string_view shader_stage_name(vulkan_shader_stage stage);

struct vulkan_shader_module_descriptor {
    vulkan_shader_module_id id;
    vulkan_shader_stage stage = vulkan_shader_stage::vertex;
    std::string entry_point = "main";

    bool valid() const
    {
        return !id.empty() && !entry_point.empty();
    }
};

struct vulkan_shader_module_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

enum class vulkan_shader_module_adapter_call_status {
    not_called,
    completed,
    failed_recoverable,
    failed_fatal,
};

inline std::string_view shader_module_adapter_call_status_name(
    vulkan_shader_module_adapter_call_status status)
{
    switch (status) {
    case vulkan_shader_module_adapter_call_status::not_called:
        return "not_called";
    case vulkan_shader_module_adapter_call_status::completed:
        return "completed";
    case vulkan_shader_module_adapter_call_status::failed_recoverable:
        return "failed_recoverable";
    case vulkan_shader_module_adapter_call_status::failed_fatal:
        return "failed_fatal";
    }

    return "unknown";
}

enum class vulkan_shader_module_create_status {
    not_requested,
    created,
    missing_spirv_bytes,
    bad_spirv_magic,
    bad_spirv_version,
    missing_entry_point,
    unsupported_stage,
    create_failed_recoverable,
    create_failed_fatal,
};

inline std::string_view shader_module_create_status_name(
    vulkan_shader_module_create_status status)
{
    switch (status) {
    case vulkan_shader_module_create_status::not_requested:
        return "not_requested";
    case vulkan_shader_module_create_status::created:
        return "created";
    case vulkan_shader_module_create_status::missing_spirv_bytes:
        return "missing_spirv_bytes";
    case vulkan_shader_module_create_status::bad_spirv_magic:
        return "bad_spirv_magic";
    case vulkan_shader_module_create_status::bad_spirv_version:
        return "bad_spirv_version";
    case vulkan_shader_module_create_status::missing_entry_point:
        return "missing_entry_point";
    case vulkan_shader_module_create_status::unsupported_stage:
        return "unsupported_stage";
    case vulkan_shader_module_create_status::create_failed_recoverable:
        return "create_failed_recoverable";
    case vulkan_shader_module_create_status::create_failed_fatal:
        return "create_failed_fatal";
    }

    return "unknown";
}

enum class vulkan_shader_module_destroy_status {
    not_requested,
    destroyed,
    invalid_handle,
    destroy_failed_recoverable,
    destroy_failed_fatal,
};

inline std::string_view shader_module_destroy_status_name(
    vulkan_shader_module_destroy_status status)
{
    switch (status) {
    case vulkan_shader_module_destroy_status::not_requested:
        return "not_requested";
    case vulkan_shader_module_destroy_status::destroyed:
        return "destroyed";
    case vulkan_shader_module_destroy_status::invalid_handle:
        return "invalid_handle";
    case vulkan_shader_module_destroy_status::destroy_failed_recoverable:
        return "destroy_failed_recoverable";
    case vulkan_shader_module_destroy_status::destroy_failed_fatal:
        return "destroy_failed_fatal";
    }

    return "unknown";
}

struct vulkan_shader_module_create_request {
    vulkan_shader_module_id id;
    vulkan_shader_stage stage = vulkan_shader_stage::vertex;
    std::string entry_point = "main";
    std::vector<std::uint8_t> spirv_bytes;
};

struct vulkan_shader_module_create_call {
    vulkan_shader_module_id id;
    vulkan_shader_stage stage = vulkan_shader_stage::vertex;
    std::string entry_point = "main";
    std::size_t spirv_byte_count = 0;
    std::size_t spirv_word_count = 0;
    std::uint32_t spirv_version = 0;

    bool valid() const
    {
        return !id.empty() && !entry_point.empty()
            && spirv_byte_count >= 20 && spirv_word_count >= 5;
    }
};

struct vulkan_shader_module_destroy_call {
    vulkan_shader_module_handle handle;

    bool valid() const
    {
        return handle.valid();
    }
};

struct vulkan_shader_module_create_operation_result {
    bool checked = false;
    vulkan_shader_module_adapter_call_status status =
        vulkan_shader_module_adapter_call_status::not_called;
    vulkan_shader_module_handle handle;
    std::string diagnostic;

    bool completed() const
    {
        return checked
            && status == vulkan_shader_module_adapter_call_status::completed
            && handle.valid();
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_shader_module_adapter_call_status::failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked
            && status == vulkan_shader_module_adapter_call_status::failed_fatal;
    }

    bool failed() const
    {
        return recoverable_failure() || fatal_failure();
    }
};

struct vulkan_shader_module_destroy_operation_result {
    bool checked = false;
    vulkan_shader_module_adapter_call_status status =
        vulkan_shader_module_adapter_call_status::not_called;
    std::string diagnostic;

    bool completed() const
    {
        return checked
            && status == vulkan_shader_module_adapter_call_status::completed;
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_shader_module_adapter_call_status::failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked
            && status == vulkan_shader_module_adapter_call_status::failed_fatal;
    }

    bool failed() const
    {
        return recoverable_failure() || fatal_failure();
    }
};

using vulkan_shader_module_create_fn =
    vulkan_shader_module_create_operation_result (*)(
        void* user_data,
        const vulkan_shader_module_create_call& call);

using vulkan_shader_module_destroy_fn =
    vulkan_shader_module_destroy_operation_result (*)(
        void* user_data,
        const vulkan_shader_module_destroy_call& call);

struct vulkan_shader_module_function_table {
    vulkan_shader_module_create_fn create = nullptr;
    vulkan_shader_module_destroy_fn destroy = nullptr;

    bool valid() const
    {
        return create != nullptr && destroy != nullptr;
    }
};

struct vulkan_shader_module_factory_adapter {
    void* user_data = nullptr;
    vulkan_shader_module_function_table functions;
    std::vector<vulkan_shader_stage> supported_stages{
        vulkan_shader_stage::vertex,
        vulkan_shader_stage::fragment,
    };

    bool valid() const
    {
        return user_data != nullptr && functions.valid();
    }

    bool supports_stage(vulkan_shader_stage stage) const
    {
        for (vulkan_shader_stage supported_stage : supported_stages) {
            if (supported_stage == stage) {
                return true;
            }
        }
        return false;
    }
};

struct vulkan_shader_module_create_result {
    bool checked = false;
    vulkan_shader_module_create_status status =
        vulkan_shader_module_create_status::not_requested;
    vulkan_shader_module_id id;
    vulkan_shader_stage stage = vulkan_shader_stage::vertex;
    std::string entry_point;
    std::size_t spirv_byte_count = 0;
    std::size_t spirv_word_count = 0;
    std::uint32_t spirv_magic = 0;
    std::uint32_t spirv_version = 0;
    bool spirv_bytes_present = false;
    bool spirv_magic_valid = false;
    bool spirv_version_supported = false;
    bool entry_point_present = false;
    bool stage_supported = false;
    vulkan_shader_module_handle handle;
    vulkan_shader_module_create_call create_call;
    vulkan_shader_module_create_operation_result create_result;
    std::string diagnostic;

    bool ready_for_pipeline() const
    {
        return checked && status == vulkan_shader_module_create_status::created
            && handle.valid() && create_result.completed()
            && spirv_bytes_present && spirv_magic_valid && spirv_version_supported
            && entry_point_present && stage_supported;
    }

    bool recoverable_create_failure() const
    {
        return checked
            && status == vulkan_shader_module_create_status::create_failed_recoverable;
    }

    bool fatal_create_failure() const
    {
        return checked
            && status == vulkan_shader_module_create_status::create_failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_shader_module_create_status::created;
    }
};

struct vulkan_shader_module_destroy_result {
    bool checked = false;
    vulkan_shader_module_destroy_status status =
        vulkan_shader_module_destroy_status::not_requested;
    vulkan_shader_module_handle handle;
    vulkan_shader_module_destroy_call destroy_call;
    vulkan_shader_module_destroy_operation_result destroy_result;
    std::string diagnostic;

    bool completed() const
    {
        return checked && status == vulkan_shader_module_destroy_status::destroyed
            && destroy_result.completed();
    }

    bool recoverable_failure() const
    {
        return checked
            && status == vulkan_shader_module_destroy_status::destroy_failed_recoverable;
    }

    bool fatal_failure() const
    {
        return checked
            && status == vulkan_shader_module_destroy_status::destroy_failed_fatal;
    }

    bool failed() const
    {
        return checked && status != vulkan_shader_module_destroy_status::destroyed;
    }
};

struct vulkan_backend_shader_module_readiness_state {
    bool checked = false;
    std::size_t requested_module_count = 0;
    std::size_t created_module_count = 0;
    std::size_t failed_module_count = 0;
    std::vector<vulkan_shader_module_create_result> modules;

    bool completed() const
    {
        return checked && requested_module_count == modules.size()
            && created_module_count == modules.size()
            && failed_module_count == 0;
    }
};

struct fake_vulkan_shader_module_factory_state {
    std::size_t create_call_count = 0;
    std::size_t destroy_call_count = 0;
    vulkan_shader_module_create_call last_create_call;
    vulkan_shader_module_destroy_call last_destroy_call;
};

struct fake_vulkan_shader_module_factory_options {
    std::vector<vulkan_shader_stage> supported_stages{
        vulkan_shader_stage::vertex,
        vulkan_shader_stage::fragment,
    };
    vulkan_shader_module_adapter_call_status create_status =
        vulkan_shader_module_adapter_call_status::completed;
    vulkan_shader_module_adapter_call_status destroy_status =
        vulkan_shader_module_adapter_call_status::completed;
    vulkan_shader_module_handle handle{.value = 700};
};

class fake_vulkan_shader_module_factory final {
public:
    fake_vulkan_shader_module_factory();
    explicit fake_vulkan_shader_module_factory(
        fake_vulkan_shader_module_factory_options options);

    vulkan_shader_module_factory_adapter adapter();
    const fake_vulkan_shader_module_factory_state& state() const;

private:
    static vulkan_shader_module_create_operation_result create_callback(
        void* user_data,
        const vulkan_shader_module_create_call& call);
    static vulkan_shader_module_destroy_operation_result destroy_callback(
        void* user_data,
        const vulkan_shader_module_destroy_call& call);

    fake_vulkan_shader_module_factory_options options_;
    fake_vulkan_shader_module_factory_state state_;
};

namespace shader_module_detail {

inline constexpr std::uint32_t k_spirv_magic = 0x07230203;
inline constexpr std::uint32_t k_min_spirv_version = 0x00010000;
inline constexpr std::uint32_t k_max_spirv_version = 0x00010600;

inline std::uint32_t read_u32_le(
    const std::vector<std::uint8_t>& bytes,
    std::size_t byte_offset)
{
    return static_cast<std::uint32_t>(bytes[byte_offset])
        | (static_cast<std::uint32_t>(bytes[byte_offset + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[byte_offset + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[byte_offset + 3]) << 24);
}

inline bool supported_spirv_version(std::uint32_t version)
{
    return version >= k_min_spirv_version && version <= k_max_spirv_version
        && ((version >> 16) & 0xffU) == 1U;
}

inline vulkan_shader_module_create_result make_create_result(
    const vulkan_shader_module_create_request& request)
{
    return vulkan_shader_module_create_result{
        .checked = true,
        .status = vulkan_shader_module_create_status::not_requested,
        .id = request.id,
        .stage = request.stage,
        .entry_point = request.entry_point,
        .spirv_byte_count = request.spirv_bytes.size(),
        .spirv_word_count = request.spirv_bytes.size() / 4,
        .spirv_magic = 0,
        .spirv_version = 0,
        .spirv_bytes_present = false,
        .spirv_magic_valid = false,
        .spirv_version_supported = false,
        .entry_point_present = !request.entry_point.empty(),
        .stage_supported = false,
        .handle = {},
        .create_call = {},
        .create_result = {},
        .diagnostic = {},
    };
}

inline vulkan_shader_module_create_operation_result make_create_operation_result(
    vulkan_shader_module_adapter_call_status status,
    vulkan_shader_module_handle handle,
    std::string diagnostic)
{
    return vulkan_shader_module_create_operation_result{
        .checked = true,
        .status = status,
        .handle = handle,
        .diagnostic = std::move(diagnostic),
    };
}

inline vulkan_shader_module_destroy_operation_result make_destroy_operation_result(
    vulkan_shader_module_adapter_call_status status,
    std::string diagnostic)
{
    return vulkan_shader_module_destroy_operation_result{
        .checked = true,
        .status = status,
        .diagnostic = std::move(diagnostic),
    };
}

} // namespace shader_module_detail

inline fake_vulkan_shader_module_factory::fake_vulkan_shader_module_factory()
    : fake_vulkan_shader_module_factory(fake_vulkan_shader_module_factory_options{})
{
}

inline fake_vulkan_shader_module_factory::fake_vulkan_shader_module_factory(
    fake_vulkan_shader_module_factory_options options)
    : options_(std::move(options))
{
}

inline vulkan_shader_module_factory_adapter fake_vulkan_shader_module_factory::adapter()
{
    return vulkan_shader_module_factory_adapter{
        .user_data = this,
        .functions = vulkan_shader_module_function_table{
            .create = &fake_vulkan_shader_module_factory::create_callback,
            .destroy = &fake_vulkan_shader_module_factory::destroy_callback,
        },
        .supported_stages = options_.supported_stages,
    };
}

inline const fake_vulkan_shader_module_factory_state&
fake_vulkan_shader_module_factory::state() const
{
    return state_;
}

inline vulkan_shader_module_create_operation_result
fake_vulkan_shader_module_factory::create_callback(
    void* user_data,
    const vulkan_shader_module_create_call& call)
{
    auto& fake = *static_cast<fake_vulkan_shader_module_factory*>(user_data);
    ++fake.state_.create_call_count;
    fake.state_.last_create_call = call;
    return shader_module_detail::make_create_operation_result(
        fake.options_.create_status,
        fake.options_.create_status == vulkan_shader_module_adapter_call_status::completed
            ? fake.options_.handle
            : vulkan_shader_module_handle{},
        fake.options_.create_status == vulkan_shader_module_adapter_call_status::completed
            ? "Vulkan shader module factory created shader module"
            : "Vulkan shader module factory failed shader module creation");
}

inline vulkan_shader_module_destroy_operation_result
fake_vulkan_shader_module_factory::destroy_callback(
    void* user_data,
    const vulkan_shader_module_destroy_call& call)
{
    auto& fake = *static_cast<fake_vulkan_shader_module_factory*>(user_data);
    ++fake.state_.destroy_call_count;
    fake.state_.last_destroy_call = call;
    return shader_module_detail::make_destroy_operation_result(
        fake.options_.destroy_status,
        fake.options_.destroy_status == vulkan_shader_module_adapter_call_status::completed
            ? "Vulkan shader module factory destroyed shader module"
            : "Vulkan shader module factory failed shader module destroy");
}

inline vulkan_shader_module_create_result create_vulkan_shader_module(
    vulkan_shader_module_factory_adapter adapter,
    const vulkan_shader_module_create_request& request)
{
    vulkan_shader_module_create_result result =
        shader_module_detail::make_create_result(request);

    if (request.spirv_bytes.size() < 20 || request.spirv_bytes.size() % 4 != 0) {
        result.status = vulkan_shader_module_create_status::missing_spirv_bytes;
        result.diagnostic =
            "Vulkan shader module SPIR-V bytecode is missing or incomplete";
        return result;
    }

    result.spirv_bytes_present = true;
    result.spirv_magic = shader_module_detail::read_u32_le(request.spirv_bytes, 0);
    result.spirv_version = shader_module_detail::read_u32_le(request.spirv_bytes, 4);
    if (result.spirv_magic != shader_module_detail::k_spirv_magic) {
        result.status = vulkan_shader_module_create_status::bad_spirv_magic;
        result.diagnostic = "Vulkan shader module SPIR-V magic is invalid";
        return result;
    }

    result.spirv_magic_valid = true;
    if (!shader_module_detail::supported_spirv_version(result.spirv_version)) {
        result.status = vulkan_shader_module_create_status::bad_spirv_version;
        result.diagnostic = "Vulkan shader module SPIR-V version is unsupported";
        return result;
    }

    result.spirv_version_supported = true;
    if (request.entry_point.empty()) {
        result.status = vulkan_shader_module_create_status::missing_entry_point;
        result.diagnostic = "Vulkan shader module entry point is missing";
        return result;
    }

    if (!adapter.supports_stage(request.stage)) {
        result.status = vulkan_shader_module_create_status::unsupported_stage;
        result.diagnostic = "Vulkan shader module stage is unsupported: "
            + std::string{shader_stage_name(request.stage)};
        return result;
    }

    result.stage_supported = true;
    if (!adapter.valid()) {
        result.status = vulkan_shader_module_create_status::create_failed_fatal;
        result.diagnostic = "Vulkan shader module factory function table is unavailable";
        return result;
    }

    result.create_call = vulkan_shader_module_create_call{
        .id = request.id,
        .stage = request.stage,
        .entry_point = request.entry_point,
        .spirv_byte_count = request.spirv_bytes.size(),
        .spirv_word_count = request.spirv_bytes.size() / 4,
        .spirv_version = result.spirv_version,
    };
    result.create_result = adapter.functions.create(adapter.user_data, result.create_call);
    result.diagnostic = result.create_result.diagnostic;
    if (result.create_result.completed()) {
        result.status = vulkan_shader_module_create_status::created;
        result.handle = result.create_result.handle;
        return result;
    }

    if (result.create_result.recoverable_failure()) {
        result.status = vulkan_shader_module_create_status::create_failed_recoverable;
        return result;
    }

    result.status = vulkan_shader_module_create_status::create_failed_fatal;
    return result;
}

inline vulkan_shader_module_destroy_result destroy_vulkan_shader_module(
    vulkan_shader_module_factory_adapter adapter,
    vulkan_shader_module_handle handle)
{
    vulkan_shader_module_destroy_result result{
        .checked = true,
        .status = vulkan_shader_module_destroy_status::not_requested,
        .handle = handle,
        .destroy_call = {},
        .destroy_result = {},
        .diagnostic = {},
    };

    if (!handle.valid()) {
        result.status = vulkan_shader_module_destroy_status::invalid_handle;
        result.diagnostic = "Vulkan shader module destroy received an invalid handle";
        return result;
    }
    if (!adapter.valid()) {
        result.status = vulkan_shader_module_destroy_status::destroy_failed_fatal;
        result.diagnostic = "Vulkan shader module factory function table is unavailable";
        return result;
    }

    result.destroy_call = vulkan_shader_module_destroy_call{.handle = handle};
    result.destroy_result = adapter.functions.destroy(adapter.user_data, result.destroy_call);
    result.diagnostic = result.destroy_result.diagnostic;
    if (result.destroy_result.completed()) {
        result.status = vulkan_shader_module_destroy_status::destroyed;
        return result;
    }
    if (result.destroy_result.recoverable_failure()) {
        result.status = vulkan_shader_module_destroy_status::destroy_failed_recoverable;
        return result;
    }

    result.status = vulkan_shader_module_destroy_status::destroy_failed_fatal;
    return result;
}

inline vulkan_backend_shader_module_readiness_state check_vulkan_shader_module_readiness(
    vulkan_shader_module_factory_adapter adapter,
    const std::vector<vulkan_shader_module_create_request>& requests)
{
    vulkan_backend_shader_module_readiness_state state{
        .checked = true,
        .requested_module_count = requests.size(),
        .created_module_count = 0,
        .failed_module_count = 0,
        .modules = {},
    };
    state.modules.reserve(requests.size());
    for (const vulkan_shader_module_create_request& request : requests) {
        vulkan_shader_module_create_result result =
            create_vulkan_shader_module(adapter, request);
        if (result.ready_for_pipeline()) {
            ++state.created_module_count;
        } else {
            ++state.failed_module_count;
        }
        state.modules.push_back(std::move(result));
    }
    return state;
}

} // namespace quiz_vulkan::render::vulkan_backend
