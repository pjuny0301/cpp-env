#pragma once

#include "render/vulkan/vulkan_backend_instance.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render::vulkan_backend {

enum class vulkan_device_queue_capability {
    graphics,
    present,
    compute,
    transfer,
};

inline std::string_view device_queue_capability_name(
    vulkan_device_queue_capability capability)
{
    switch (capability) {
    case vulkan_device_queue_capability::graphics:
        return "graphics";
    case vulkan_device_queue_capability::present:
        return "present";
    case vulkan_device_queue_capability::compute:
        return "compute";
    case vulkan_device_queue_capability::transfer:
        return "transfer";
    }

    return "unknown";
}

struct vulkan_device_create_request {
    std::vector<std::string> required_device_extensions;
    std::vector<std::string> optional_device_extensions;
    std::vector<vulkan_device_queue_capability> required_queue_capabilities{
        vulkan_device_queue_capability::graphics,
    };
};

struct vulkan_device_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_queue_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_physical_device_handle {
    std::uintptr_t value = 0;

    bool valid() const
    {
        return value != 0;
    }
};

struct vulkan_device_queue_selection {
    vulkan_device_queue_capability capability = vulkan_device_queue_capability::graphics;
    std::size_t family_index = 0;
    vulkan_queue_handle queue;

    bool valid() const
    {
        return queue.valid();
    }
};

struct vulkan_device_extension_diagnostic {
    std::string extension_name;
    bool required = true;
    bool available = false;
    bool selected = false;

    bool missing_required() const
    {
        return required && !available;
    }
};

enum class vulkan_device_create_status {
    not_requested,
    created,
    instance_unavailable,
    no_suitable_device,
    missing_required_device_extension,
    missing_required_queue,
    creation_failed,
};

inline std::string_view device_create_status_name(vulkan_device_create_status status)
{
    switch (status) {
    case vulkan_device_create_status::not_requested:
        return "not_requested";
    case vulkan_device_create_status::created:
        return "created";
    case vulkan_device_create_status::instance_unavailable:
        return "instance_unavailable";
    case vulkan_device_create_status::no_suitable_device:
        return "no_suitable_device";
    case vulkan_device_create_status::missing_required_device_extension:
        return "missing_required_device_extension";
    case vulkan_device_create_status::missing_required_queue:
        return "missing_required_queue";
    case vulkan_device_create_status::creation_failed:
        return "creation_failed";
    }

    return "unknown";
}

enum class vulkan_native_physical_device_dispatch_table_status {
    not_checked,
    ready,
    instance_unavailable,
    get_instance_proc_address_unavailable,
    missing_enumerate_physical_devices_symbol,
    missing_get_physical_device_queue_family_properties_symbol,
};

struct vulkan_native_physical_device_dispatch_table {
    bool checked = false;
    vulkan_native_physical_device_dispatch_table_status status =
        vulkan_native_physical_device_dispatch_table_status::not_checked;
    vulkan_instance_handle instance;
    vulkan_native_function_pointer get_instance_proc_address;
    vulkan_native_function_pointer enumerate_physical_devices;
    vulkan_native_function_pointer get_physical_device_queue_family_properties;
    std::string missing_symbol_name;
    std::string diagnostic;

    bool ready_for_enumeration() const
    {
        return checked
            && status == vulkan_native_physical_device_dispatch_table_status::ready
            && instance.valid() && get_instance_proc_address.valid()
            && enumerate_physical_devices.valid()
            && get_physical_device_queue_family_properties.valid();
    }

    bool ready_for_queue_family_query() const
    {
        return ready_for_enumeration()
            && get_physical_device_queue_family_properties.valid();
    }
};

enum class vulkan_native_physical_device_enumeration_status {
    not_checked,
    ready,
    dispatch_table_unavailable,
    headers_unavailable,
    enumeration_failed,
    no_devices,
};

struct vulkan_native_physical_device_enumeration_result {
    bool checked = false;
    vulkan_native_physical_device_enumeration_status status =
        vulkan_native_physical_device_enumeration_status::not_checked;
    vulkan_native_physical_device_dispatch_table dispatch_table;
    std::vector<vulkan_physical_device_handle> physical_devices;
    std::size_t physical_device_count = 0;
    std::int32_t native_result = 0;
    std::string diagnostic;

    bool ready_for_device_selection() const
    {
        return checked && status == vulkan_native_physical_device_enumeration_status::ready
            && dispatch_table.ready_for_enumeration() && physical_device_count > 0
            && physical_device_count == physical_devices.size()
            && std::all_of(
                physical_devices.begin(),
                physical_devices.end(),
                [](vulkan_physical_device_handle handle) { return handle.valid(); });
    }
};

class vulkan_native_physical_device_enumerator_interface {
public:
    virtual ~vulkan_native_physical_device_enumerator_interface() = default;

    virtual vulkan_native_physical_device_enumeration_result enumerate_physical_devices(
        const vulkan_native_physical_device_dispatch_table& dispatch_table) = 0;
};

struct fake_vulkan_native_physical_device_enumerator_options {
    std::vector<vulkan_physical_device_handle> physical_devices{
        vulkan_physical_device_handle{.value = 3000},
    };
    bool fail_enumeration = false;
    std::int32_t failure_result = -1;
};

struct fake_vulkan_native_physical_device_enumerator_state {
    std::size_t enumerate_call_count = 0;
    vulkan_instance_handle last_instance;
    vulkan_native_function_pointer last_enumerate_physical_devices;
};

class fake_vulkan_native_physical_device_enumerator final
    : public vulkan_native_physical_device_enumerator_interface {
public:
    fake_vulkan_native_physical_device_enumerator();
    explicit fake_vulkan_native_physical_device_enumerator(
        fake_vulkan_native_physical_device_enumerator_options options);

    vulkan_native_physical_device_enumeration_result enumerate_physical_devices(
        const vulkan_native_physical_device_dispatch_table& dispatch_table) override;
    const fake_vulkan_native_physical_device_enumerator_state& state() const;

private:
    fake_vulkan_native_physical_device_enumerator_options options_;
    fake_vulkan_native_physical_device_enumerator_state state_;
};

class vulkan_native_physical_device_enumerator final
    : public vulkan_native_physical_device_enumerator_interface {
public:
    vulkan_native_physical_device_enumeration_result enumerate_physical_devices(
        const vulkan_native_physical_device_dispatch_table& dispatch_table) override;
};

struct vulkan_native_physical_device_queue_family {
    vulkan_physical_device_handle physical_device;
    std::size_t family_index = 0;
    std::size_t queue_count = 0;
    std::vector<vulkan_device_queue_capability> capabilities;

    bool supports(vulkan_device_queue_capability capability) const
    {
        return std::find(capabilities.begin(), capabilities.end(), capability)
            != capabilities.end();
    }

    bool usable() const
    {
        return physical_device.valid() && queue_count > 0 && !capabilities.empty();
    }
};

struct vulkan_native_physical_device_queue_family_selection {
    vulkan_device_queue_capability capability = vulkan_device_queue_capability::graphics;
    vulkan_physical_device_handle physical_device;
    std::size_t family_index = 0;
    std::size_t queue_count = 0;

    bool valid() const
    {
        return physical_device.valid() && queue_count > 0;
    }
};

enum class vulkan_native_queue_family_query_status {
    not_checked,
    ready,
    enumeration_unavailable,
    dispatch_table_unavailable,
    headers_unavailable,
    no_devices,
    no_queue_families,
};

struct vulkan_native_queue_family_query_result {
    bool checked = false;
    vulkan_native_queue_family_query_status status =
        vulkan_native_queue_family_query_status::not_checked;
    vulkan_native_physical_device_enumeration_result enumeration;
    std::vector<vulkan_native_physical_device_queue_family> queue_families;
    std::size_t queue_family_count = 0;
    std::string diagnostic;

    bool ready_for_selection() const
    {
        return checked && status == vulkan_native_queue_family_query_status::ready
            && enumeration.ready_for_device_selection()
            && enumeration.dispatch_table.ready_for_queue_family_query()
            && queue_family_count > 0 && queue_family_count == queue_families.size()
            && std::all_of(
                queue_families.begin(),
                queue_families.end(),
                [](const vulkan_native_physical_device_queue_family& family) {
                    return family.usable();
                });
    }
};

class vulkan_native_queue_family_query_interface {
public:
    virtual ~vulkan_native_queue_family_query_interface() = default;

    virtual vulkan_native_queue_family_query_result query_queue_families(
        const vulkan_native_physical_device_enumeration_result& enumeration) = 0;
};

struct fake_vulkan_native_queue_family_query_options {
    std::vector<vulkan_native_physical_device_queue_family> queue_families;
};

struct fake_vulkan_native_queue_family_query_state {
    std::size_t query_call_count = 0;
    std::vector<vulkan_physical_device_handle> inspected_physical_devices;
    vulkan_native_function_pointer last_get_physical_device_queue_family_properties;
};

class fake_vulkan_native_queue_family_query final
    : public vulkan_native_queue_family_query_interface {
public:
    fake_vulkan_native_queue_family_query();
    explicit fake_vulkan_native_queue_family_query(
        fake_vulkan_native_queue_family_query_options options);

    vulkan_native_queue_family_query_result query_queue_families(
        const vulkan_native_physical_device_enumeration_result& enumeration) override;
    const fake_vulkan_native_queue_family_query_state& state() const;

private:
    fake_vulkan_native_queue_family_query_options options_;
    fake_vulkan_native_queue_family_query_state state_;
};

class vulkan_native_queue_family_query final
    : public vulkan_native_queue_family_query_interface {
public:
    vulkan_native_queue_family_query_result query_queue_families(
        const vulkan_native_physical_device_enumeration_result& enumeration) override;
};

struct vulkan_native_physical_device_selection_candidate {
    vulkan_physical_device_handle physical_device;
    std::vector<vulkan_native_physical_device_queue_family> queue_families;
    std::vector<vulkan_native_physical_device_queue_family_selection>
        selected_queue_families;
    std::string missing_required_queue;
    bool selected = false;

    bool required_queues_ready() const
    {
        return selected && physical_device.valid() && missing_required_queue.empty()
            && !selected_queue_families.empty()
            && std::all_of(
                selected_queue_families.begin(),
                selected_queue_families.end(),
                [](const vulkan_native_physical_device_queue_family_selection& selection) {
                    return selection.valid();
                });
    }
};

enum class vulkan_native_physical_device_selection_status {
    not_checked,
    selected,
    enumeration_unavailable,
    no_devices,
    missing_required_queue,
};

struct vulkan_native_physical_device_selection_result {
    bool checked = false;
    vulkan_native_physical_device_selection_status status =
        vulkan_native_physical_device_selection_status::not_checked;
    vulkan_native_physical_device_enumeration_result enumeration;
    vulkan_native_queue_family_query_result queue_family_query;
    vulkan_device_create_request request;
    vulkan_physical_device_handle selected_physical_device;
    std::vector<vulkan_native_physical_device_queue_family_selection>
        selected_queue_families;
    std::vector<vulkan_native_physical_device_selection_candidate> candidates;
    std::string missing_required_queue;
    std::string diagnostic;

    bool ready_for_device_create() const
    {
        return checked && status == vulkan_native_physical_device_selection_status::selected
            && enumeration.ready_for_device_selection()
            && queue_family_query.ready_for_selection()
            && selected_physical_device.valid() && missing_required_queue.empty()
            && !selected_queue_families.empty()
            && std::all_of(
                selected_queue_families.begin(),
                selected_queue_families.end(),
                [](const vulkan_native_physical_device_queue_family_selection& selection) {
                    return selection.valid();
                });
    }
};

enum class vulkan_native_device_dispatch_table_status {
    not_checked,
    ready,
    selection_unavailable,
    get_instance_proc_address_unavailable,
    missing_enumerate_device_extension_properties_symbol,
    missing_create_device_symbol,
    missing_get_device_queue_symbol,
    missing_destroy_device_symbol,
};

struct vulkan_native_device_dispatch_table {
    bool checked = false;
    vulkan_native_device_dispatch_table_status status =
        vulkan_native_device_dispatch_table_status::not_checked;
    vulkan_instance_handle instance;
    vulkan_physical_device_handle physical_device;
    vulkan_native_function_pointer get_instance_proc_address;
    vulkan_native_function_pointer enumerate_device_extension_properties;
    vulkan_native_function_pointer create_device;
    vulkan_native_function_pointer get_device_queue;
    vulkan_native_function_pointer destroy_device;
    std::string missing_symbol_name;
    std::string diagnostic;

    bool ready_for_create() const
    {
        return checked && status == vulkan_native_device_dispatch_table_status::ready
            && instance.valid() && physical_device.valid()
            && get_instance_proc_address.valid()
            && enumerate_device_extension_properties.valid()
            && create_device.valid() && get_device_queue.valid()
            && destroy_device.valid();
    }
};

enum class vulkan_native_device_extension_query_status {
    not_checked,
    ready,
    selection_unavailable,
    dispatch_table_unavailable,
    headers_unavailable,
    enumeration_failed,
    missing_required_extension,
};

struct vulkan_native_device_extension_query_result {
    bool checked = false;
    vulkan_native_device_extension_query_status status =
        vulkan_native_device_extension_query_status::not_checked;
    vulkan_native_device_dispatch_table dispatch_table;
    vulkan_native_physical_device_selection_result selection;
    vulkan_physical_device_handle physical_device;
    std::vector<std::string> available_extensions;
    std::vector<std::string> selected_extensions;
    std::vector<vulkan_device_extension_diagnostic> required_extension_diagnostics;
    std::size_t available_extension_count = 0;
    std::size_t required_extension_count = 0;
    std::size_t available_required_extension_count = 0;
    std::string missing_required_extension;
    std::int32_t native_result = 0;
    std::string diagnostic;

    bool required_extensions_ready() const
    {
        return checked && required_extension_count == available_required_extension_count
            && missing_required_extension.empty();
    }

    bool ready_for_create() const
    {
        return checked && status == vulkan_native_device_extension_query_status::ready
            && dispatch_table.ready_for_create() && selection.ready_for_device_create()
            && physical_device.valid() && required_extensions_ready();
    }
};

class vulkan_native_device_extension_query_interface {
public:
    virtual ~vulkan_native_device_extension_query_interface() = default;

    virtual vulkan_native_device_extension_query_result query_device_extensions(
        const vulkan_native_device_dispatch_table& dispatch_table,
        const vulkan_native_physical_device_selection_result& selection) = 0;
};

struct fake_vulkan_native_device_extension_query_options {
    std::vector<std::string> available_extensions{"VK_KHR_swapchain"};
    bool fail_enumeration = false;
    std::int32_t failure_result = -1;
};

struct fake_vulkan_native_device_extension_query_state {
    std::size_t query_call_count = 0;
    vulkan_physical_device_handle requested_physical_device;
    vulkan_native_function_pointer last_enumerate_device_extension_properties;
};

class fake_vulkan_native_device_extension_query final
    : public vulkan_native_device_extension_query_interface {
public:
    fake_vulkan_native_device_extension_query();
    explicit fake_vulkan_native_device_extension_query(
        fake_vulkan_native_device_extension_query_options options);

    vulkan_native_device_extension_query_result query_device_extensions(
        const vulkan_native_device_dispatch_table& dispatch_table,
        const vulkan_native_physical_device_selection_result& selection) override;
    const fake_vulkan_native_device_extension_query_state& state() const;

private:
    fake_vulkan_native_device_extension_query_options options_;
    fake_vulkan_native_device_extension_query_state state_;
};

class vulkan_native_device_extension_query final
    : public vulkan_native_device_extension_query_interface {
public:
    vulkan_native_device_extension_query_result query_device_extensions(
        const vulkan_native_device_dispatch_table& dispatch_table,
        const vulkan_native_physical_device_selection_result& selection) override;
};

enum class vulkan_native_device_create_status {
    not_requested,
    created,
    selection_unavailable,
    dispatch_table_unavailable,
    extension_query_unavailable,
    missing_required_extension,
    headers_unavailable,
    creation_failed,
    queue_unavailable,
};

struct vulkan_native_device_create_result {
    bool checked = false;
    vulkan_native_device_create_status status =
        vulkan_native_device_create_status::not_requested;
    vulkan_native_device_dispatch_table dispatch_table;
    vulkan_native_device_extension_query_result extension_query;
    vulkan_native_physical_device_selection_result selection;
    vulkan_device_handle handle;
    std::vector<std::string> selected_extensions;
    std::vector<std::size_t> queue_create_family_indices;
    std::vector<vulkan_device_queue_selection> selected_queues;
    std::size_t queue_create_family_count = 0;
    std::size_t selected_queue_count = 0;
    std::int32_t native_result = 0;
    std::string diagnostic;

    bool ready_for_backend() const
    {
        return checked && status == vulkan_native_device_create_status::created
            && dispatch_table.ready_for_create()
            && extension_query.ready_for_create()
            && selection.ready_for_device_create() && handle.valid()
            && !queue_create_family_indices.empty()
            && queue_create_family_count == queue_create_family_indices.size()
            && selected_queue_count == selected_queues.size()
            && !selected_queues.empty()
            && std::all_of(
                selected_queues.begin(),
                selected_queues.end(),
                [](const vulkan_device_queue_selection& selection) {
                    return selection.valid();
                });
    }
};

class vulkan_native_device_creator_interface {
public:
    virtual ~vulkan_native_device_creator_interface() = default;

    virtual vulkan_native_device_create_result create_device(
        const vulkan_native_device_dispatch_table& dispatch_table,
        const vulkan_native_device_extension_query_result& extension_query,
        const vulkan_native_physical_device_selection_result& selection) = 0;
};

struct fake_vulkan_native_device_creator_options {
    bool fail_creation = false;
    std::int32_t failure_result = -1;
    vulkan_device_handle handle{.value = 5000};
    std::uintptr_t queue_handle_base = 6000;
    std::vector<std::size_t> unavailable_queue_family_indices;
};

struct fake_vulkan_native_device_creator_state {
    std::size_t create_call_count = 0;
    std::size_t get_queue_call_count = 0;
    std::size_t destroy_call_count = 0;
    vulkan_physical_device_handle requested_physical_device;
    vulkan_device_handle destroyed_device;
    vulkan_native_function_pointer last_create_device;
    vulkan_native_function_pointer last_get_device_queue;
    vulkan_native_function_pointer last_destroy_device;
    std::vector<std::size_t> requested_queue_family_indices;
    std::vector<std::string> requested_extensions;
};

class fake_vulkan_native_device_creator final
    : public vulkan_native_device_creator_interface {
public:
    fake_vulkan_native_device_creator();
    explicit fake_vulkan_native_device_creator(
        fake_vulkan_native_device_creator_options options);

    vulkan_native_device_create_result create_device(
        const vulkan_native_device_dispatch_table& dispatch_table,
        const vulkan_native_device_extension_query_result& extension_query,
        const vulkan_native_physical_device_selection_result& selection) override;
    const fake_vulkan_native_device_creator_state& state() const;

private:
    fake_vulkan_native_device_creator_options options_;
    fake_vulkan_native_device_creator_state state_;
};

class vulkan_native_device_creator final : public vulkan_native_device_creator_interface {
public:
    vulkan_native_device_create_result create_device(
        const vulkan_native_device_dispatch_table& dispatch_table,
        const vulkan_native_device_extension_query_result& extension_query,
        const vulkan_native_physical_device_selection_result& selection) override;
};

class vulkan_native_physical_device_selector_interface {
public:
    virtual ~vulkan_native_physical_device_selector_interface() = default;

    virtual vulkan_native_physical_device_selection_result select_physical_device(
        const vulkan_native_physical_device_enumeration_result& enumeration,
        const vulkan_native_queue_family_query_result& queue_family_query,
        const vulkan_device_create_request& request) = 0;
};

struct fake_vulkan_native_physical_device_selector_options {};

struct fake_vulkan_native_physical_device_selector_state {
    std::size_t select_call_count = 0;
    std::vector<vulkan_physical_device_handle> inspected_physical_devices;
};

class fake_vulkan_native_physical_device_selector final
    : public vulkan_native_physical_device_selector_interface {
public:
    fake_vulkan_native_physical_device_selector();
    explicit fake_vulkan_native_physical_device_selector(
        fake_vulkan_native_physical_device_selector_options options);

    vulkan_native_physical_device_selection_result select_physical_device(
        const vulkan_native_physical_device_enumeration_result& enumeration,
        const vulkan_native_queue_family_query_result& queue_family_query,
        const vulkan_device_create_request& request) override;
    const fake_vulkan_native_physical_device_selector_state& state() const;

private:
    fake_vulkan_native_physical_device_selector_options options_;
    fake_vulkan_native_physical_device_selector_state state_;
};

struct vulkan_device_create_result {
    bool checked = false;
    vulkan_device_create_status status = vulkan_device_create_status::not_requested;
    vulkan_instance_create_result instance;
    vulkan_device_handle handle;
    std::vector<std::string> selected_extensions;
    std::vector<vulkan_device_extension_diagnostic> required_extension_diagnostics;
    std::size_t required_extension_count = 0;
    std::size_t available_required_extension_count = 0;
    std::string missing_required_extension;
    std::vector<vulkan_device_queue_selection> selected_queues;
    std::string diagnostic;

    bool required_extensions_ready() const
    {
        return checked && required_extension_count == available_required_extension_count
            && missing_required_extension.empty();
    }

    bool ready_for_backend() const
    {
        return checked && status == vulkan_device_create_status::created
            && instance.ready_for_device() && handle.valid()
            && required_extensions_ready() && !selected_queues.empty();
    }
};

class vulkan_device_factory_interface {
public:
    virtual ~vulkan_device_factory_interface() = default;

    virtual vulkan_device_create_result create_device(
        const vulkan_instance_create_result& instance_result,
        const vulkan_device_create_request& request) = 0;
};

struct fake_vulkan_device_factory_options {
    std::vector<std::string> supported_device_extensions;
    std::vector<vulkan_device_queue_capability> supported_queue_capabilities{
        vulkan_device_queue_capability::graphics,
    };
    bool device_available = true;
    bool fail_creation = false;
    vulkan_device_handle handle{.value = 1};
    std::uintptr_t queue_handle_base = 100;
};

class fake_vulkan_device_factory final : public vulkan_device_factory_interface {
public:
    fake_vulkan_device_factory();
    explicit fake_vulkan_device_factory(fake_vulkan_device_factory_options options);

    vulkan_device_create_result create_device(
        const vulkan_instance_create_result& instance_result,
        const vulkan_device_create_request& request) override;

private:
    fake_vulkan_device_factory_options options_;
};

namespace device_detail {

inline bool contains_string(
    const std::vector<std::string>& values,
    const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

inline bool contains_queue_capability(
    const std::vector<vulkan_device_queue_capability>& capabilities,
    vulkan_device_queue_capability capability)
{
    return std::find(capabilities.begin(), capabilities.end(), capability)
        != capabilities.end();
}

inline void append_unique_string(
    std::vector<std::string>& values,
    const std::string& value)
{
    if (value.empty() || contains_string(values, value)) {
        return;
    }

    values.push_back(value);
}

inline void append_unique_queue_capability(
    std::vector<vulkan_device_queue_capability>& capabilities,
    vulkan_device_queue_capability capability)
{
    if (contains_queue_capability(capabilities, capability)) {
        return;
    }

    capabilities.push_back(capability);
}

inline vulkan_device_create_result make_device_create_result(
    const vulkan_instance_create_result& instance_result)
{
    return vulkan_device_create_result{
        .checked = true,
        .status = vulkan_device_create_status::not_requested,
        .instance = instance_result,
        .handle = {},
        .selected_extensions = {},
        .required_extension_diagnostics = {},
        .required_extension_count = 0,
        .available_required_extension_count = 0,
        .missing_required_extension = {},
        .selected_queues = {},
        .diagnostic = {},
    };
}

inline std::string make_missing_required_device_extension_diagnostic(
    const std::string& extension_name)
{
    return "missing required device extension: " + extension_name;
}

inline void record_required_extension_diagnostic(
    vulkan_device_create_result& result,
    const std::string& extension_name,
    bool available)
{
    result.required_extension_diagnostics.push_back(
        vulkan_device_extension_diagnostic{
            .extension_name = extension_name,
            .required = true,
            .available = available,
            .selected = available,
        });
    result.required_extension_count = result.required_extension_diagnostics.size();
    if (available) {
        ++result.available_required_extension_count;
    } else if (result.missing_required_extension.empty()) {
        result.missing_required_extension = extension_name;
    }
}

inline std::string make_missing_required_queue_diagnostic(
    vulkan_device_queue_capability capability)
{
    return "missing required device queue: "
        + std::string{device_queue_capability_name(capability)};
}

inline std::vector<vulkan_device_queue_capability> requested_queue_capabilities(
    const vulkan_device_create_request& request)
{
    std::vector<vulkan_device_queue_capability> capabilities;
    capabilities.reserve(request.required_queue_capabilities.size());
    for (const vulkan_device_queue_capability capability :
         request.required_queue_capabilities) {
        append_unique_queue_capability(capabilities, capability);
    }

    return capabilities;
}

inline vulkan_device_queue_selection make_queue_selection(
    vulkan_device_queue_capability capability,
    std::size_t family_index,
    std::uintptr_t queue_handle_base)
{
    return vulkan_device_queue_selection{
        .capability = capability,
        .family_index = family_index,
        .queue = vulkan_queue_handle{.value = queue_handle_base + family_index + 1},
    };
}

} // namespace device_detail

inline fake_vulkan_device_factory::fake_vulkan_device_factory()
    : fake_vulkan_device_factory(fake_vulkan_device_factory_options{})
{
}

inline fake_vulkan_device_factory::fake_vulkan_device_factory(
    fake_vulkan_device_factory_options options)
    : options_(std::move(options))
{
}

inline vulkan_device_create_result fake_vulkan_device_factory::create_device(
    const vulkan_instance_create_result& instance_result,
    const vulkan_device_create_request& request)
{
    vulkan_device_create_result result =
        device_detail::make_device_create_result(instance_result);

    if (!instance_result.ready_for_device()) {
        result.status = vulkan_device_create_status::instance_unavailable;
        result.diagnostic = "Vulkan instance is not ready for device creation";
        return result;
    }
    if (!options_.device_available) {
        result.status = vulkan_device_create_status::no_suitable_device;
        result.diagnostic = "No suitable Vulkan physical device is available";
        return result;
    }

    for (const std::string& extension_name : request.required_device_extensions) {
        const bool extension_available = device_detail::contains_string(
            options_.supported_device_extensions,
            extension_name);
        device_detail::record_required_extension_diagnostic(
            result,
            extension_name,
            extension_available);
        if (!extension_available) {
            result.status = vulkan_device_create_status::missing_required_device_extension;
            result.diagnostic =
                device_detail::make_missing_required_device_extension_diagnostic(extension_name);
            return result;
        }
        device_detail::append_unique_string(result.selected_extensions, extension_name);
    }

    for (const std::string& extension_name : request.optional_device_extensions) {
        if (device_detail::contains_string(
                options_.supported_device_extensions,
                extension_name)) {
            device_detail::append_unique_string(result.selected_extensions, extension_name);
        }
    }

    const std::vector<vulkan_device_queue_capability> requested_capabilities =
        device_detail::requested_queue_capabilities(request);
    for (const vulkan_device_queue_capability capability : requested_capabilities) {
        if (!device_detail::contains_queue_capability(
                options_.supported_queue_capabilities,
                capability)) {
            result.status = vulkan_device_create_status::missing_required_queue;
            result.diagnostic = device_detail::make_missing_required_queue_diagnostic(capability);
            return result;
        }
        result.selected_queues.push_back(device_detail::make_queue_selection(
            capability,
            result.selected_queues.size(),
            options_.queue_handle_base));
    }

    if (options_.fail_creation || !options_.handle.valid()) {
        result.status = vulkan_device_create_status::creation_failed;
        result.diagnostic = options_.fail_creation
            ? "Vulkan device creation failed"
            : "Vulkan device creation returned an invalid handle";
        return result;
    }

    result.status = vulkan_device_create_status::created;
    result.handle = options_.handle;
    result.diagnostic = "Vulkan device created";
    return result;
}

inline vulkan_device_create_result create_vulkan_device(
    vulkan_device_factory_interface& factory,
    const vulkan_instance_create_result& instance_result,
    const vulkan_device_create_request& request = {})
{
    return factory.create_device(instance_result, request);
}

vulkan_native_physical_device_dispatch_table
collect_vulkan_native_physical_device_dispatch_table(
    vulkan_native_instance_symbol_resolver_interface& resolver,
    const vulkan_native_instance_create_result& create_result);

vulkan_native_physical_device_enumeration_result
enumerate_native_vulkan_physical_devices(
    vulkan_native_physical_device_enumerator_interface& enumerator,
    const vulkan_native_physical_device_dispatch_table& dispatch_table);

vulkan_native_queue_family_query_result
query_native_vulkan_physical_device_queue_families(
    vulkan_native_queue_family_query_interface& query,
    const vulkan_native_physical_device_enumeration_result& enumeration);

vulkan_native_physical_device_selection_result
select_native_vulkan_physical_device(
    vulkan_native_physical_device_selector_interface& selector,
    const vulkan_native_physical_device_enumeration_result& enumeration,
    const vulkan_native_queue_family_query_result& queue_family_query,
    const vulkan_device_create_request& request = {});

vulkan_native_device_dispatch_table
collect_vulkan_native_device_dispatch_table(
    vulkan_native_instance_symbol_resolver_interface& resolver,
    const vulkan_native_physical_device_selection_result& selection);

vulkan_native_device_extension_query_result
query_native_vulkan_device_extensions(
    vulkan_native_device_extension_query_interface& query,
    const vulkan_native_device_dispatch_table& dispatch_table,
    const vulkan_native_physical_device_selection_result& selection);

vulkan_native_device_create_result
create_native_vulkan_device(
    vulkan_native_device_creator_interface& creator,
    const vulkan_native_device_dispatch_table& dispatch_table,
    const vulkan_native_device_extension_query_result& extension_query,
    const vulkan_native_physical_device_selection_result& selection);

} // namespace quiz_vulkan::render::vulkan_backend
