#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_backend_command_recording.h"
#include "render/vulkan/vulkan_backend_command_submit.h"
#include "render/vulkan/vulkan_backend_device.h"
#include "render/vulkan/vulkan_backend_frame_lifecycle.h"
#include "render/vulkan/vulkan_backend_instance.h"
#include "render/vulkan/vulkan_backend_loader.h"
#include "render/vulkan/vulkan_backend_render_pass.h"
#include "render/vulkan/vulkan_backend_swapchain.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__) || defined(__unix__) || defined(__linux__)
#include <dlfcn.h>
#endif

namespace quiz_vulkan::render::vulkan_backend {
namespace {

constexpr std::string_view k_vulkan_loader_required_symbol_name = "vkGetInstanceProcAddr";

vulkan_backend_fallback_reason fallback_or(
    vulkan_backend_fallback_reason fallback,
    vulkan_backend_fallback_reason replacement)
{
    if (fallback == vulkan_backend_fallback_reason::none
        || fallback == vulkan_backend_fallback_reason::not_requested) {
        return replacement;
    }
    return fallback;
}

std::string required_symbol_name_for(const vulkan_loader_probe_request& request)
{
    if (request.required_symbol_name.empty()) {
        return std::string{k_vulkan_loader_required_symbol_name};
    }

    return request.required_symbol_name;
}

void append_unique_library_name(
    std::vector<std::string>& names,
    const std::string& candidate_name)
{
    if (candidate_name.empty()) {
        return;
    }
    if (std::find(names.begin(), names.end(), candidate_name) != names.end()) {
        return;
    }

    names.push_back(candidate_name);
}

std::vector<std::string> merge_loader_candidate_names(
    const vulkan_loader_probe_request& request,
    const std::vector<std::string>& default_library_names)
{
    std::vector<std::string> candidate_names;
    candidate_names.reserve(
        request.candidate_library_names.size() + default_library_names.size());
    for (const std::string& candidate_name : request.candidate_library_names) {
        append_unique_library_name(candidate_names, candidate_name);
    }
    if (request.use_default_library_names) {
        for (const std::string& candidate_name : default_library_names) {
            append_unique_library_name(candidate_names, candidate_name);
        }
    }

    return candidate_names;
}

vulkan_loader_probe_result make_loader_probe_result(
    const vulkan_loader_probe_request& request)
{
    return vulkan_loader_probe_result{
        .checked = true,
        .status = vulkan_loader_probe_status::library_missing,
        .attempted_library_names = {},
        .candidate_diagnostics = {},
        .loaded_library_name = {},
        .required_symbol_name = required_symbol_name_for(request),
        .attempted_library_count = 0,
        .library_found = false,
        .required_symbol_found = false,
    };
}

void record_loader_attempt(
    vulkan_loader_probe_result& result,
    const std::string& library_name)
{
    result.attempted_library_names.push_back(library_name);
    result.attempted_library_count = result.attempted_library_names.size();
    result.candidate_diagnostics.push_back(vulkan_loader_candidate_diagnostic{
        .library_name = library_name,
        .status = vulkan_loader_candidate_status::library_missing,
        .library_found = false,
        .required_symbol_found = false,
    });
}

void mark_loader_candidate_found(
    vulkan_loader_probe_result& result,
    const std::string& library_name)
{
    for (vulkan_loader_candidate_diagnostic& candidate :
         result.candidate_diagnostics) {
        if (candidate.library_name != library_name) {
            continue;
        }

        candidate.status = vulkan_loader_candidate_status::required_symbol_missing;
        candidate.library_found = true;
        candidate.required_symbol_found = false;
        return;
    }
}

void mark_loader_candidate_usable(
    vulkan_loader_probe_result& result,
    const std::string& library_name)
{
    for (vulkan_loader_candidate_diagnostic& candidate :
         result.candidate_diagnostics) {
        if (candidate.library_name != library_name) {
            continue;
        }

        candidate.status = vulkan_loader_candidate_status::usable;
        candidate.library_found = true;
        candidate.required_symbol_found = true;
        return;
    }
}

void mark_loader_library_found(
    vulkan_loader_probe_result& result,
    const std::string& library_name)
{
    mark_loader_candidate_found(result, library_name);
    result.library_found = true;
    if (result.loaded_library_name.empty()) {
        result.loaded_library_name = library_name;
    }
}

void mark_loader_available(
    vulkan_loader_probe_result& result,
    const std::string& library_name)
{
    mark_loader_candidate_usable(result, library_name);
    result.status = vulkan_loader_probe_status::available;
    result.loaded_library_name = library_name;
    result.library_found = true;
    result.required_symbol_found = true;
}

void mark_loader_probe_completed(vulkan_loader_probe_result& result)
{
    if (result.required_symbol_found) {
        result.status = vulkan_loader_probe_status::available;
        return;
    }

    result.status = result.library_found
        ? vulkan_loader_probe_status::required_symbol_missing
        : vulkan_loader_probe_status::library_missing;
}

vulkan_loader_readiness_status loader_readiness_status_for_probe(
    vulkan_loader_probe_status status)
{
    switch (status) {
    case vulkan_loader_probe_status::not_checked:
        return vulkan_loader_readiness_status::not_checked;
    case vulkan_loader_probe_status::available:
        return vulkan_loader_readiness_status::ready;
    case vulkan_loader_probe_status::library_missing:
        return vulkan_loader_readiness_status::library_missing;
    case vulkan_loader_probe_status::required_symbol_missing:
        return vulkan_loader_readiness_status::required_symbol_missing;
    }

    return vulkan_loader_readiness_status::not_checked;
}

class native_loader_library {
public:
    explicit native_loader_library(const std::string& library_name)
    {
#if defined(_WIN32)
        handle_ = ::LoadLibraryA(library_name.c_str());
#elif defined(__APPLE__) || defined(__unix__) || defined(__linux__)
        int open_flags = RTLD_LAZY;
#if defined(RTLD_LOCAL)
        open_flags |= RTLD_LOCAL;
#endif
        handle_ = ::dlopen(library_name.c_str(), open_flags);
#else
        static_cast<void>(library_name);
#endif
    }

    native_loader_library(const native_loader_library&) = delete;
    native_loader_library& operator=(const native_loader_library&) = delete;

    ~native_loader_library()
    {
#if defined(_WIN32)
        if (handle_ != nullptr) {
            ::FreeLibrary(handle_);
        }
#elif defined(__APPLE__) || defined(__unix__) || defined(__linux__)
        if (handle_ != nullptr) {
            ::dlclose(handle_);
        }
#endif
    }

    bool valid() const
    {
        return handle_ != nullptr;
    }

    bool has_symbol(const std::string& symbol_name) const
    {
        return resolve_symbol(symbol_name).valid();
    }

    vulkan_native_function_pointer resolve_symbol(const std::string& symbol_name) const
    {
        if (handle_ == nullptr) {
            return {};
        }

#if defined(_WIN32)
        return vulkan_native_function_pointer{
            .value = reinterpret_cast<std::uintptr_t>(
                ::GetProcAddress(handle_, symbol_name.c_str())),
        };
#elif defined(__APPLE__) || defined(__unix__) || defined(__linux__)
        return vulkan_native_function_pointer{
            .value = reinterpret_cast<std::uintptr_t>(
                ::dlsym(handle_, symbol_name.c_str())),
        };
#else
        static_cast<void>(symbol_name);
        return {};
#endif
    }

private:
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#elif defined(__APPLE__) || defined(__unix__) || defined(__linux__)
    void* handle_ = nullptr;
#else
    void* handle_ = nullptr;
#endif
};

bool contains_symbol_name(
    const std::vector<std::string>& symbols,
    std::string_view symbol_name)
{
    return std::find(symbols.begin(), symbols.end(), std::string{symbol_name})
        != symbols.end();
}

vulkan_backend_fallback_reason fallback_for_native_stage(
    vulkan_native_entrypoint_stage stage)
{
    switch (stage) {
    case vulkan_native_entrypoint_stage::command_buffer_recording:
        return vulkan_backend_fallback_reason::record_commands_failed;
    case vulkan_native_entrypoint_stage::queue_submit:
        return vulkan_backend_fallback_reason::submit_frame_failed;
    case vulkan_native_entrypoint_stage::swapchain_create:
    case vulkan_native_entrypoint_stage::swapchain_destroy:
    case vulkan_native_entrypoint_stage::swapchain_images:
        return vulkan_backend_fallback_reason::swapchain_unavailable;
    case vulkan_native_entrypoint_stage::swapchain_acquire:
        return vulkan_backend_fallback_reason::acquire_image_failed;
    case vulkan_native_entrypoint_stage::queue_present:
        return vulkan_backend_fallback_reason::present_frame_failed;
    }

    return vulkan_backend_fallback_reason::not_requested;
}

vulkan_native_function_table_status missing_status_for_native_stage(
    vulkan_native_entrypoint_stage stage)
{
    switch (stage) {
    case vulkan_native_entrypoint_stage::command_buffer_recording:
        return vulkan_native_function_table_status::missing_command_buffer_recording_symbol;
    case vulkan_native_entrypoint_stage::queue_submit:
        return vulkan_native_function_table_status::missing_queue_submit_symbol;
    case vulkan_native_entrypoint_stage::swapchain_create:
        return vulkan_native_function_table_status::missing_swapchain_create_symbol;
    case vulkan_native_entrypoint_stage::swapchain_destroy:
        return vulkan_native_function_table_status::missing_swapchain_destroy_symbol;
    case vulkan_native_entrypoint_stage::swapchain_images:
        return vulkan_native_function_table_status::missing_swapchain_images_symbol;
    case vulkan_native_entrypoint_stage::swapchain_acquire:
        return vulkan_native_function_table_status::missing_swapchain_acquire_symbol;
    case vulkan_native_entrypoint_stage::queue_present:
        return vulkan_native_function_table_status::missing_queue_present_symbol;
    }

    return vulkan_native_function_table_status::not_checked;
}

bool required_native_stage_ready(
    const std::vector<vulkan_native_entrypoint_symbol_diagnostics>& symbols,
    vulkan_native_entrypoint_stage stage)
{
    bool has_required_symbol = false;
    for (const vulkan_native_entrypoint_symbol_diagnostics& symbol : symbols) {
        if (symbol.stage != stage || !symbol.required) {
            continue;
        }

        has_required_symbol = true;
        if (!symbol.completed()) {
            return false;
        }
    }

    return has_required_symbol;
}

void record_native_symbol_diagnostics(
    vulkan_native_function_table_diagnostics& diagnostics,
    const vulkan_native_entrypoint_symbol_request& request,
    vulkan_native_function_pointer pointer)
{
    diagnostics.symbols.push_back(vulkan_native_entrypoint_symbol_diagnostics{
        .stage = request.stage,
        .name = request.name,
        .required = request.required,
        .pointer = pointer,
        .available = pointer.valid(),
    });
    diagnostics.requested_symbol_count = diagnostics.symbols.size();
    if (request.required) {
        ++diagnostics.required_symbol_count;
    }
    if (pointer.valid()) {
        ++diagnostics.available_symbol_count;
    } else if (request.required) {
        ++diagnostics.missing_required_symbol_count;
    }
}

std::vector<vulkan_native_entrypoint_symbol_request> make_native_symbol_requests(
    const vulkan_native_function_table_request& request)
{
    std::vector<vulkan_native_entrypoint_symbol_request> symbols;
    if (request.include_default_backend_entrypoints) {
        symbols = default_vulkan_native_backend_entrypoints();
    }
    symbols.insert(symbols.end(), request.symbols.begin(), request.symbols.end());
    return symbols;
}

std::vector<std::string> make_native_required_extensions(
    const vulkan_native_function_table_request& request)
{
    std::vector<std::string> extensions;
    if (request.include_default_swapchain_extension) {
        extensions = default_vulkan_native_swapchain_extensions();
    }
    for (const std::string& extension : request.required_extensions) {
        if (std::find(extensions.begin(), extensions.end(), extension) == extensions.end()) {
            extensions.push_back(extension);
        }
    }
    return extensions;
}

std::vector<std::string> native_symbol_candidate_libraries(
    const system_vulkan_native_symbol_resolver_options& options)
{
    std::vector<std::string> candidate_names;
    if (options.use_default_library_names) {
        candidate_names = default_vulkan_loader_library_names();
    }
    candidate_names.insert(
        candidate_names.end(),
        options.candidate_library_names.begin(),
        options.candidate_library_names.end());
    return candidate_names;
}

vulkan_native_function_pointer resolve_loader_global_symbol(
    vulkan_native_function_pointer instance_proc_address,
    const std::string& symbol_name)
{
    if (!instance_proc_address.valid() || symbol_name.empty()) {
        return {};
    }

    using vk_void_function = void (*)();
#if defined(_WIN32)
    using vk_get_instance_proc_addr = vk_void_function(__stdcall *)(void*, const char*);
#else
    using vk_get_instance_proc_addr = vk_void_function (*)(void*, const char*);
#endif
    const auto get_instance_proc_address =
        reinterpret_cast<vk_get_instance_proc_addr>(instance_proc_address.value);
    if (get_instance_proc_address == nullptr) {
        return {};
    }

    const vk_void_function resolved = get_instance_proc_address(nullptr, symbol_name.c_str());
    return vulkan_native_function_pointer{
        .value = reinterpret_cast<std::uintptr_t>(resolved),
    };
}

vulkan_system_symbol_resolution_result make_system_symbol_resolution_result(
    std::string_view symbol_name)
{
    return vulkan_system_symbol_resolution_result{
        .checked = true,
        .symbol_name = std::string{symbol_name},
        .attempted_library_names = {},
        .loaded_library_name = {},
        .loader_library_available = false,
        .instance_proc_address_available = false,
        .resolved_via_instance_proc_address = false,
        .resolved_via_direct_export = false,
        .pointer = {},
        .diagnostic = {},
    };
}

vulkan_native_instance_function_table make_native_instance_function_table()
{
    return vulkan_native_instance_function_table{
        .checked = true,
        .status = vulkan_native_instance_function_table_status::not_checked,
        .get_instance_proc_address = {},
        .create_instance = {},
        .destroy_instance = {},
        .diagnostic = {},
    };
}

vulkan_native_instance_create_result make_native_instance_create_result(
    const vulkan_loader_readiness_state& loader_readiness,
    const vulkan_native_instance_function_table& function_table,
    const vulkan_instance_create_request& request)
{
    return vulkan_native_instance_create_result{
        .checked = true,
        .status = vulkan_native_instance_create_status::not_requested,
        .loader = loader_readiness,
        .function_table = function_table,
        .request = request,
        .handle = {},
        .native_result = 0,
        .diagnostic = {},
    };
}

vulkan_native_instance_dispatch_table make_native_instance_dispatch_table(
    const vulkan_native_instance_create_result& create_result,
    vulkan_native_instance_symbol_resolver_interface& resolver)
{
    return vulkan_native_instance_dispatch_table{
        .checked = true,
        .status = vulkan_native_instance_dispatch_table_status::not_checked,
        .handle = create_result.handle,
        .get_instance_proc_address = resolver.get_instance_proc_address(),
        .destroy_instance = {},
        .missing_symbol_name = {},
        .diagnostic = {},
    };
}

vulkan_native_instance_destroy_result make_native_instance_destroy_result(
    const vulkan_native_instance_function_table& function_table,
    vulkan_instance_handle handle)
{
    return vulkan_native_instance_destroy_result{
        .checked = true,
        .status = vulkan_native_instance_destroy_status::not_requested,
        .function_table = function_table,
        .dispatch_table = {},
        .handle = handle,
        .used_instance_dispatch = false,
        .diagnostic = {},
    };
}

vulkan_native_physical_device_dispatch_table make_native_physical_device_dispatch_table(
    const vulkan_native_instance_create_result& create_result,
    vulkan_native_instance_symbol_resolver_interface& resolver)
{
    return vulkan_native_physical_device_dispatch_table{
        .checked = true,
        .status = vulkan_native_physical_device_dispatch_table_status::not_checked,
        .instance = create_result.handle,
        .get_instance_proc_address = resolver.get_instance_proc_address(),
        .enumerate_physical_devices = {},
        .missing_symbol_name = {},
        .diagnostic = {},
    };
}

vulkan_native_physical_device_enumeration_result
make_native_physical_device_enumeration_result(
    const vulkan_native_physical_device_dispatch_table& dispatch_table)
{
    return vulkan_native_physical_device_enumeration_result{
        .checked = true,
        .status = vulkan_native_physical_device_enumeration_status::not_checked,
        .dispatch_table = dispatch_table,
        .physical_devices = {},
        .physical_device_count = 0,
        .native_result = 0,
        .diagnostic = {},
    };
}

vulkan_native_physical_device_selection_result
make_native_physical_device_selection_result(
    const vulkan_native_physical_device_enumeration_result& enumeration,
    const vulkan_device_create_request& request)
{
    return vulkan_native_physical_device_selection_result{
        .checked = true,
        .status = vulkan_native_physical_device_selection_status::not_checked,
        .enumeration = enumeration,
        .request = request,
        .selected_physical_device = {},
        .selected_queue_families = {},
        .candidates = {},
        .missing_required_queue = {},
        .diagnostic = {},
    };
}

std::vector<vulkan_native_physical_device_queue_family>
queue_families_for_physical_device(
    const std::vector<vulkan_native_physical_device_queue_family>& queue_families,
    vulkan_physical_device_handle physical_device)
{
    std::vector<vulkan_native_physical_device_queue_family> matching_families;
    for (const vulkan_native_physical_device_queue_family& family : queue_families) {
        if (family.physical_device.value == physical_device.value) {
            matching_families.push_back(family);
        }
    }
    return matching_families;
}

vulkan_native_physical_device_queue_family_selection make_queue_family_selection(
    vulkan_device_queue_capability capability,
    const vulkan_native_physical_device_queue_family& family)
{
    return vulkan_native_physical_device_queue_family_selection{
        .capability = capability,
        .physical_device = family.physical_device,
        .family_index = family.family_index,
        .queue_count = family.queue_count,
    };
}

bool has_visible_area(const render_rect& rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

vulkan_backend_fallback_reason first_unready_reason(
    const vulkan_backend_lifecycle_readiness& lifecycle)
{
    if (!lifecycle.effective_instance_ready()) {
        return vulkan_backend_fallback_reason::instance_unavailable;
    }
    if (!lifecycle.effective_device_ready()) {
        return vulkan_backend_fallback_reason::device_unavailable;
    }
    if (!lifecycle.effective_swapchain_ready()) {
        return vulkan_backend_fallback_reason::swapchain_unavailable;
    }
    if (lifecycle.render_pass.checked && !lifecycle.effective_render_pass_ready()) {
        return vulkan_backend_fallback_reason::render_pass_unavailable;
    }
    if (!lifecycle.effective_pipeline_ready()) {
        return vulkan_backend_fallback_reason::pipeline_unavailable;
    }
    if (!lifecycle.effective_command_recorder_ready()) {
        return vulkan_backend_fallback_reason::command_recorder_unavailable;
    }
    if (lifecycle.command_submit.checked && !lifecycle.effective_command_submit_ready()) {
        if (lifecycle.command_submit.status
            == vulkan_command_submit_readiness_status::present_target_unavailable) {
            return vulkan_backend_fallback_reason::present_frame_failed;
        }
        return vulkan_backend_fallback_reason::submit_frame_failed;
    }
    if (lifecycle.command_submit.checked && !lifecycle.effective_present_target_ready()) {
        return vulkan_backend_fallback_reason::present_frame_failed;
    }

    return vulkan_backend_fallback_reason::none;
}

int frame_stage_order(vulkan_backend_frame_stage stage)
{
    switch (stage) {
    case vulkan_backend_frame_stage::not_started:
        return 0;
    case vulkan_backend_frame_stage::backend_attempted:
        return 1;
    case vulkan_backend_frame_stage::lifecycle_ready:
        return 2;
    case vulkan_backend_frame_stage::surface_extent_ready:
        return 3;
    case vulkan_backend_frame_stage::frame_plan_ready:
        return 4;
    case vulkan_backend_frame_stage::frame_begun:
        return 5;
    case vulkan_backend_frame_stage::commands_recorded:
        return 6;
    case vulkan_backend_frame_stage::frame_submitted:
        return 7;
    case vulkan_backend_frame_stage::frame_presented:
        return 8;
    }

    return 0;
}

bool reached_frame_stage(
    vulkan_backend_frame_stage reached_stage,
    vulkan_backend_frame_stage target_stage)
{
    return frame_stage_order(reached_stage) >= frame_stage_order(target_stage);
}

vulkan_backend_frame_pipeline_handoff_status frame_pipeline_status_for_fallback(
    vulkan_backend_fallback_reason reason)
{
    switch (reason) {
    case vulkan_backend_fallback_reason::none:
        return vulkan_backend_frame_pipeline_handoff_status::ready;
    case vulkan_backend_fallback_reason::not_requested:
        return vulkan_backend_frame_pipeline_handoff_status::not_checked;
    case vulkan_backend_fallback_reason::instance_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::instance_unavailable;
    case vulkan_backend_fallback_reason::device_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::device_unavailable;
    case vulkan_backend_fallback_reason::swapchain_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::swapchain_unavailable;
    case vulkan_backend_fallback_reason::render_pass_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::render_pass_unavailable;
    case vulkan_backend_fallback_reason::pipeline_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::pipeline_unavailable;
    case vulkan_backend_fallback_reason::command_recorder_unavailable:
    case vulkan_backend_fallback_reason::record_commands_failed:
        return vulkan_backend_frame_pipeline_handoff_status::command_recording_unavailable;
    case vulkan_backend_fallback_reason::surface_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::surface_unavailable;
    case vulkan_backend_fallback_reason::viewport_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::viewport_unavailable;
    case vulkan_backend_fallback_reason::begin_frame_failed:
    case vulkan_backend_fallback_reason::acquire_image_failed:
        return vulkan_backend_frame_pipeline_handoff_status::frame_lifecycle_unavailable;
    case vulkan_backend_fallback_reason::resource_binding_unavailable:
        return vulkan_backend_frame_pipeline_handoff_status::resource_binding_unavailable;
    case vulkan_backend_fallback_reason::submit_frame_failed:
        return vulkan_backend_frame_pipeline_handoff_status::submit_unavailable;
    case vulkan_backend_fallback_reason::present_image_failed:
    case vulkan_backend_fallback_reason::present_frame_failed:
        return vulkan_backend_frame_pipeline_handoff_status::present_unavailable;
    }

    return vulkan_backend_frame_pipeline_handoff_status::not_checked;
}

void count_handoff_batch_kind(
    vulkan_backend_frame_pipeline_handoff& handoff,
    vulkan_batch_kind kind)
{
    switch (kind) {
    case vulkan_batch_kind::quad:
        ++handoff.quad_batch_count;
        break;
    case vulkan_batch_kind::text:
        ++handoff.text_batch_count;
        break;
    case vulkan_batch_kind::image:
        ++handoff.image_batch_count;
        break;
    case vulkan_batch_kind::debug_bounds:
        ++handoff.debug_bounds_batch_count;
        break;
    }
}

void count_handoff_batches_from_frame(
    vulkan_backend_frame_pipeline_handoff& handoff,
    const vulkan_backend_frame_result& frame)
{
    if (!frame.resource_bindings.batch_snapshots.empty()) {
        for (const vulkan_batch_resource_binding_snapshot& snapshot :
             frame.resource_bindings.batch_snapshots) {
            count_handoff_batch_kind(handoff, snapshot.batch_kind);
        }
        return;
    }

    if (!frame.pipeline.lifecycle.pipeline_snapshots.empty()) {
        for (const vulkan_pipeline_lifecycle_snapshot& snapshot :
             frame.pipeline.lifecycle.pipeline_snapshots) {
            count_handoff_batch_kind(handoff, snapshot.batch_kind);
        }
        return;
    }

    for (const vulkan_recorded_draw_batch& batch : frame.command_recorder.recorded_batches) {
        count_handoff_batch_kind(handoff, batch.kind);
    }
}

vulkan_command_packet_category command_packet_category_for(vulkan_batch_kind kind)
{
    switch (kind) {
    case vulkan_batch_kind::quad:
        return vulkan_command_packet_category::rect;
    case vulkan_batch_kind::text:
        return vulkan_command_packet_category::text;
    case vulkan_batch_kind::image:
        return vulkan_command_packet_category::image;
    case vulkan_batch_kind::debug_bounds:
        return vulkan_command_packet_category::debug_bounds;
    }

    return vulkan_command_packet_category::rect;
}

void count_command_packet_category(
    vulkan_command_packet_bridge_result& bridge,
    vulkan_command_packet_category category)
{
    switch (category) {
    case vulkan_command_packet_category::rect:
        ++bridge.rect_packet_count;
        break;
    case vulkan_command_packet_category::text:
        ++bridge.text_packet_count;
        break;
    case vulkan_command_packet_category::image:
        ++bridge.image_packet_count;
        break;
    case vulkan_command_packet_category::debug_bounds:
        ++bridge.debug_bounds_packet_count;
        break;
    }
}

void count_packet_execution_category(
    vulkan_command_packet_execution_result& result,
    vulkan_command_packet_category category)
{
    switch (category) {
    case vulkan_command_packet_category::rect:
        ++result.rect_packet_count;
        break;
    case vulkan_command_packet_category::text:
        ++result.text_packet_count;
        break;
    case vulkan_command_packet_category::image:
        ++result.image_packet_count;
        break;
    case vulkan_command_packet_category::debug_bounds:
        ++result.debug_bounds_packet_count;
        break;
    }
}

vulkan_command_recorder_operation_kind command_recorder_operation_kind_for(
    vulkan_command_packet_category category)
{
    switch (category) {
    case vulkan_command_packet_category::rect:
        return vulkan_command_recorder_operation_kind::draw_rect;
    case vulkan_command_packet_category::text:
        return vulkan_command_recorder_operation_kind::draw_text;
    case vulkan_command_packet_category::image:
        return vulkan_command_recorder_operation_kind::draw_image;
    case vulkan_command_packet_category::debug_bounds:
        return vulkan_command_recorder_operation_kind::draw_debug_bounds;
    }

    return vulkan_command_recorder_operation_kind::draw_rect;
}

void count_command_recorder_operation_category(
    vulkan_command_recorder_operation_plan& plan,
    vulkan_command_packet_category category)
{
    switch (category) {
    case vulkan_command_packet_category::rect:
        ++plan.rect_operation_count;
        break;
    case vulkan_command_packet_category::text:
        ++plan.text_operation_count;
        break;
    case vulkan_command_packet_category::image:
        ++plan.image_operation_count;
        break;
    case vulkan_command_packet_category::debug_bounds:
        ++plan.debug_bounds_operation_count;
        break;
    }
}

void count_command_buffer_record_category(
    vulkan_command_buffer_record_result& result,
    vulkan_command_packet_category category)
{
    switch (category) {
    case vulkan_command_packet_category::rect:
        ++result.rect_operation_count;
        break;
    case vulkan_command_packet_category::text:
        ++result.text_operation_count;
        break;
    case vulkan_command_packet_category::image:
        ++result.image_operation_count;
        break;
    case vulkan_command_packet_category::debug_bounds:
        ++result.debug_bounds_operation_count;
        break;
    }
}

vulkan_command_recorder_operation_summary make_command_recorder_operation_summary(
    const vulkan_command_packet& packet,
    std::size_t operation_index)
{
    return vulkan_command_recorder_operation_summary{
        .kind = command_recorder_operation_kind_for(packet.category),
        .category = packet.category,
        .batch_kind = packet.batch_kind,
        .operation_index = operation_index,
        .packet_index = packet.packet_index,
        .command_index = packet.command_index,
        .node_id = packet.node_id,
        .bounds = packet.bounds,
        .clipped_bounds = packet.clipped_bounds,
        .scissor = packet.scissor,
        .vertex_count = packet.vertices.size(),
        .descriptor_set_count = packet.descriptor_set_count,
        .binding_count = packet.binding_count,
    };
}

vulkan_command_buffer_record_event make_command_buffer_record_event(
    vulkan_command_buffer_record_event_kind event,
    vulkan_command_buffer_id command_buffer)
{
    return vulkan_command_buffer_record_event{
        .event = event,
        .command_buffer = command_buffer,
        .attempted = true,
    };
}

vulkan_command_buffer_record_event make_command_buffer_record_operation_event(
    vulkan_command_buffer_id command_buffer,
    const vulkan_command_recorder_operation_summary& operation)
{
    return vulkan_command_buffer_record_event{
        .event = vulkan_command_buffer_record_event_kind::operation,
        .command_buffer = command_buffer,
        .operation_kind = operation.kind,
        .category = operation.category,
        .batch_kind = operation.batch_kind,
        .operation_index = operation.operation_index,
        .packet_index = operation.packet_index,
        .command_index = operation.command_index,
        .attempted = true,
    };
}

vulkan_command_submit_sync_primitives default_submit_batch_sync_primitives()
{
    return vulkan_command_submit_sync_primitives{
        .image_available_semaphore = vulkan_command_submit_sync_handle{.value = 1},
        .render_finished_semaphore = vulkan_command_submit_sync_handle{.value = 2},
        .frame_fence = vulkan_command_submit_sync_handle{.value = 3},
    };
}

vulkan_command_recording_command_buffer_handle submit_command_buffer_for(
    const vulkan_command_buffer_record_result& recording,
    const vulkan_command_submit_readiness_result& command_submit)
{
    if (command_submit.checked && command_submit.command_buffer.valid()) {
        return command_submit.command_buffer;
    }

    return vulkan_command_recording_command_buffer_handle{
        .value = static_cast<std::uintptr_t>(recording.command_buffer.value),
    };
}

vulkan_queue_handle submit_queue_for(
    const vulkan_command_submit_readiness_result& command_submit)
{
    if (command_submit.checked && command_submit.submit_queue.valid()) {
        return command_submit.submit_queue;
    }

    return vulkan_queue_handle{.value = 1};
}

vulkan_command_submit_sync_primitives submit_sync_primitives_for(
    const vulkan_command_submit_readiness_result& command_submit)
{
    if (command_submit.checked) {
        return command_submit.sync_primitives;
    }

    return default_submit_batch_sync_primitives();
}

vulkan_swapchain_image_id submit_image_id_for(
    const vulkan_command_submit_readiness_result& command_submit)
{
    if (command_submit.checked && command_submit.image_id.value > 0) {
        return command_submit.image_id;
    }

    return vulkan_swapchain_image_id{.value = 1};
}

void add_submit_batch_intents(vulkan_submit_batch_plan_result& plan)
{
    plan.wait_intents.push_back(vulkan_submit_batch_sync_intent{
        .kind = vulkan_submit_batch_sync_intent_kind::wait_image_available,
        .handle = plan.sync_primitives.image_available_semaphore,
        .required = true,
        .available = plan.sync_primitives.image_available_semaphore.valid(),
    });
    plan.signal_intents.push_back(vulkan_submit_batch_sync_intent{
        .kind = vulkan_submit_batch_sync_intent_kind::signal_render_finished,
        .handle = plan.sync_primitives.render_finished_semaphore,
        .required = true,
        .available = plan.sync_primitives.render_finished_semaphore.valid(),
    });
    plan.signal_intents.push_back(vulkan_submit_batch_sync_intent{
        .kind = vulkan_submit_batch_sync_intent_kind::signal_frame_fence,
        .handle = plan.sync_primitives.frame_fence,
        .required = true,
        .available = plan.sync_primitives.frame_fence.valid(),
    });
    plan.present_intents.push_back(vulkan_submit_batch_present_intent{
        .requested = true,
        .target_available = plan.present_target_available,
        .image_id = plan.image_id,
        .wait_render_finished_semaphore = plan.sync_primitives.render_finished_semaphore,
    });
    plan.wait_intent_count = plan.wait_intents.size();
    plan.signal_intent_count = plan.signal_intents.size();
    plan.present_intent_count = plan.present_intents.size();
}

vulkan_present_request_summary present_request_from_submit_batch(
    const vulkan_submit_batch_plan_result& submit_batch,
    bool queue_present_checked)
{
    return vulkan_present_request_summary{
        .requested = submit_batch.present_ready,
        .source_adapter_checked = queue_present_checked,
        .present_queue = submit_batch.submit_queue,
        .swapchain = vulkan_swapchain_handle{.value = 1},
        .image_id = submit_batch.image_id,
        .wait_render_finished_semaphore =
            submit_batch.sync_primitives.render_finished_semaphore,
    };
}

vulkan_present_request_summary present_request_from_queue_present(
    const vulkan_queue_submit_present_result& queue_present)
{
    return vulkan_present_request_summary{
        .requested = true,
        .source_adapter_checked = true,
        .present_queue = queue_present.present_call.queue,
        .swapchain = queue_present.present_call.swapchain,
        .image_id = queue_present.present_call.image_id,
        .wait_render_finished_semaphore =
            queue_present.present_call.wait_render_finished_semaphore,
    };
}

vulkan_present_result_summary present_result_from_queue_present(
    const vulkan_queue_submit_present_result& queue_present)
{
    return vulkan_present_result_summary{
        .checked = queue_present.checked,
        .status = queue_present.present_result.status,
        .present_called = queue_present.present_called,
        .submit_before_present = queue_present.submit_before_present(),
        .recoverable_failure = queue_present.status
            == vulkan_queue_submit_present_status::present_failed_recoverable,
        .fatal_failure = queue_present.status
            == vulkan_queue_submit_present_status::present_failed_fatal,
        .diagnostic = queue_present.present_result.diagnostic.empty()
            ? queue_present.diagnostic
            : queue_present.present_result.diagnostic,
    };
}

vulkan_present_result_summary deterministic_present_result_summary()
{
    return vulkan_present_result_summary{
        .checked = true,
        .status = vulkan_queue_submit_adapter_call_status::completed,
        .present_called = true,
        .submit_before_present = true,
        .recoverable_failure = false,
        .fatal_failure = false,
        .diagnostic =
            "Vulkan present completion planned through existing frame present path",
    };
}

void block_present_completion(
    vulkan_present_completion_plan_result& plan,
    vulkan_present_completion_plan_status status,
    vulkan_frame_completion_status frame_status,
    vulkan_backend_fallback_reason fallback,
    std::string diagnostic)
{
    plan.status = status;
    plan.frame_status = frame_status;
    plan.fallback_reason = fallback;
    plan.diagnostic = std::move(diagnostic);
}

bool frame_status_has_submitted_frame(vulkan_frame_completion_status status)
{
    switch (status) {
    case vulkan_frame_completion_status::ready_for_present:
    case vulkan_frame_completion_status::completed:
    case vulkan_frame_completion_status::present_unavailable:
    case vulkan_frame_completion_status::present_failed_recoverable:
    case vulkan_frame_completion_status::present_failed_fatal:
        return true;
    case vulkan_frame_completion_status::not_checked:
    case vulkan_frame_completion_status::submit_unavailable:
    case vulkan_frame_completion_status::submit_failed_recoverable:
    case vulkan_frame_completion_status::submit_failed_fatal:
        return false;
    }

    return false;
}

void finalize_native_queue_present_operation(
    vulkan_native_queue_present_operation_result& result)
{
    result.operation = vulkan_native_queue_present_operation_summary{
        .checked = result.checked,
        .status = result.status,
        .entrypoint_name = "vkQueuePresentKHR",
        .vk_queue_present_callable = result.vk_queue_present_callable,
        .frame_lifecycle_may_complete = result.frame_lifecycle_may_complete,
        .device = result.device,
        .swapchain = result.swapchain,
        .present_queue = result.present_queue,
        .image_id = result.image_id,
        .image_handle = result.image_handle,
        .wait_render_finished_semaphore = result.wait_render_finished_semaphore,
        .present_status = result.present_status,
        .acquire_operation_checked = result.acquire_operation_checked,
        .acquired_image_ready = result.acquired_image_ready,
        .submit_batch_checked = result.submit_batch_checked,
        .submit_batch_ready = result.submit_batch_ready,
        .present_completion_checked = result.present_completion_checked,
        .present_completion_ready = result.present_completion_ready,
        .native_entrypoints_checked = result.native_entrypoints_checked,
        .required_extensions_ready = result.required_extensions_ready,
        .queue_present_symbol_ready = result.queue_present_symbol_ready,
        .present_queue_ready = result.present_queue_ready,
        .swapchain_ready = result.swapchain_ready,
        .submitted_frame_ready = result.submitted_frame_ready,
        .present_request_ready = result.present_request_ready,
        .present_adapter_result_ready = result.present_adapter_result_ready,
        .present_result_checked = result.present_result_checked,
        .present_result_completed = result.present_result_completed,
        .submit_before_present = result.submit_before_present,
        .out_of_date = result.out_of_date,
        .suboptimal = result.suboptimal,
        .recoverable_failure = result.recoverable_failure,
        .fatal_failure = result.fatal_failure,
        .missing_required_extension = result.missing_required_extension,
        .missing_symbol_name = result.missing_symbol_name,
        .diagnostic = result.diagnostic,
    };
}

void block_native_queue_present_operation(
    vulkan_native_queue_present_operation_result& result,
    vulkan_native_queue_present_operation_status status,
    std::string diagnostic)
{
    result.status = status;
    result.diagnostic = std::move(diagnostic);
    finalize_native_queue_present_operation(result);
}

vulkan_backend_fallback_reason fallback_for_native_function_table(
    const vulkan_native_function_table_diagnostics& native_functions)
{
    if (!native_functions.checked) {
        return vulkan_backend_fallback_reason::not_requested;
    }
    if (native_functions.fallback_reason != vulkan_backend_fallback_reason::none
        && native_functions.fallback_reason != vulkan_backend_fallback_reason::not_requested) {
        return native_functions.fallback_reason;
    }

    switch (native_functions.missing_symbol_stage) {
    case vulkan_native_entrypoint_stage::command_buffer_recording:
        return vulkan_backend_fallback_reason::record_commands_failed;
    case vulkan_native_entrypoint_stage::queue_submit:
        return vulkan_backend_fallback_reason::submit_frame_failed;
    case vulkan_native_entrypoint_stage::queue_present:
        return vulkan_backend_fallback_reason::present_frame_failed;
    case vulkan_native_entrypoint_stage::swapchain_create:
    case vulkan_native_entrypoint_stage::swapchain_destroy:
    case vulkan_native_entrypoint_stage::swapchain_images:
        return vulkan_backend_fallback_reason::swapchain_unavailable;
    case vulkan_native_entrypoint_stage::swapchain_acquire:
        return vulkan_backend_fallback_reason::acquire_image_failed;
    }

    return vulkan_backend_fallback_reason::not_requested;
}

vulkan_native_frame_operation_stage_summary make_native_frame_stage_summary(
    vulkan_native_frame_operation_stage stage,
    bool checked,
    bool ready,
    vulkan_backend_fallback_reason fallback_reason,
    std::string diagnostic)
{
    const bool blocked = !ready;
    return vulkan_native_frame_operation_stage_summary{
        .stage = stage,
        .checked = checked,
        .ready = ready,
        .blocked = blocked,
        .fallback_reason = blocked ? fallback_reason : vulkan_backend_fallback_reason::none,
        .diagnostic = blocked ? std::move(diagnostic) : std::string{},
    };
}

std::vector<vulkan_native_frame_operation_stage_summary> build_native_frame_stage_summaries(
    const vulkan_native_frame_operation_result& result)
{
    return {
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::native_function_table,
            result.native_function_table_checked,
            result.native_function_table_ready,
            fallback_for_native_function_table(result.native_functions),
            result.native_functions.diagnostic.empty()
                ? "Native Vulkan function table is unavailable for frame operation"
                : result.native_functions.diagnostic),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::swapchain_create,
            result.swapchain_create_checked,
            result.swapchain_create_ready,
            vulkan_backend_fallback_reason::swapchain_unavailable,
            result.swapchain_create.diagnostic.empty()
                ? "Native Vulkan frame operation has no ready swapchain create operation"
                : result.swapchain_create.diagnostic),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::swapchain_images,
            result.swapchain_images_checked,
            result.swapchain_images_ready,
            vulkan_backend_fallback_reason::swapchain_unavailable,
            result.swapchain_images.diagnostic.empty()
                ? "Native Vulkan frame operation has no enumerated swapchain images"
                : result.swapchain_images.diagnostic),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::acquire,
            result.acquire_checked,
            result.acquire_ready,
            vulkan_backend_fallback_reason::acquire_image_failed,
            result.acquire_operation.diagnostic.empty()
                ? "Native Vulkan frame operation has no acquired image"
                : result.acquire_operation.diagnostic),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::command_recording,
            result.command_recording_checked,
            result.command_recording_ready,
            vulkan_backend_fallback_reason::record_commands_failed,
            result.command_buffer_recording.diagnostic.empty()
                ? "Native Vulkan frame operation has no recorded command buffer"
                : result.command_buffer_recording.diagnostic),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::submit,
            result.submit_batch_checked,
            result.submit_batch_ready,
            vulkan_backend_fallback_reason::submit_frame_failed,
            result.submit_batch.diagnostic.empty()
                ? "Native Vulkan frame operation has no submit batch"
                : result.submit_batch.diagnostic),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::present,
            result.present_operation_checked,
            result.present_operation_ready,
            vulkan_backend_fallback_reason::present_frame_failed,
            result.present_operation.diagnostic.empty()
                ? "Native Vulkan frame operation has no queue present operation"
                : result.present_operation.diagnostic),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::frame_completion,
            result.present_operation_checked,
            result.frame_completion_ready,
            vulkan_backend_fallback_reason::present_frame_failed,
            result.present_operation.diagnostic.empty()
                ? "Native Vulkan frame operation did not complete frame lifecycle"
                : result.present_operation.diagnostic),
    };
}

void finalize_native_frame_operation(vulkan_native_frame_operation_result& result)
{
    result.cpu_fallback_should_remain_active =
        result.cpu_fallback_available
        && result.status != vulkan_native_frame_operation_status::ready;
    result.operation = vulkan_native_frame_operation_summary{
        .checked = result.checked,
        .status = result.status,
        .reached_stage = result.reached_stage,
        .blocker_stage = result.blocker_stage,
        .fallback_reason = result.fallback_reason,
        .native_function_table_checked = result.native_function_table_checked,
        .native_function_table_ready = result.native_function_table_ready,
        .swapchain_create_checked = result.swapchain_create_checked,
        .swapchain_create_ready = result.swapchain_create_ready,
        .swapchain_images_checked = result.swapchain_images_checked,
        .swapchain_images_ready = result.swapchain_images_ready,
        .acquire_checked = result.acquire_checked,
        .acquire_ready = result.acquire_ready,
        .command_recording_checked = result.command_recording_checked,
        .command_recording_ready = result.command_recording_ready,
        .submit_batch_checked = result.submit_batch_checked,
        .submit_batch_ready = result.submit_batch_ready,
        .present_operation_checked = result.present_operation_checked,
        .present_operation_ready = result.present_operation_ready,
        .frame_completion_ready = result.frame_completion_ready,
        .recoverable_failure = result.recoverable_failure,
        .fatal_failure = result.fatal_failure,
        .swapchain_out_of_date = result.swapchain_out_of_date,
        .suboptimal = result.suboptimal,
        .cpu_fallback_available = result.cpu_fallback_available,
        .cpu_fallback_should_remain_active =
            result.cpu_fallback_should_remain_active,
        .native_function_table_status = result.native_function_table_status,
        .missing_required_extension = result.missing_required_extension,
        .missing_symbol_name = result.missing_symbol_name,
        .stages = result.stages,
        .diagnostic = result.diagnostic,
    };
}

void block_native_frame_operation(
    vulkan_native_frame_operation_result& result,
    vulkan_native_frame_operation_status status,
    vulkan_native_frame_operation_stage reached_stage,
    vulkan_native_frame_operation_stage blocker_stage,
    vulkan_backend_fallback_reason fallback_reason,
    std::string diagnostic)
{
    result.status = status;
    result.reached_stage = reached_stage;
    result.blocker_stage = blocker_stage;
    result.fallback_reason = fallback_reason;
    result.diagnostic = std::move(diagnostic);
    finalize_native_frame_operation(result);
}

constexpr std::array<vulkan_native_frame_operation_stage, 8> k_native_frame_operation_stages{
    vulkan_native_frame_operation_stage::native_function_table,
    vulkan_native_frame_operation_stage::swapchain_create,
    vulkan_native_frame_operation_stage::swapchain_images,
    vulkan_native_frame_operation_stage::acquire,
    vulkan_native_frame_operation_stage::command_recording,
    vulkan_native_frame_operation_stage::submit,
    vulkan_native_frame_operation_stage::present,
    vulkan_native_frame_operation_stage::frame_completion,
};

constexpr std::array<vulkan_native_frame_execution_step, 4> k_native_frame_execution_steps{
    vulkan_native_frame_execution_step::acquire,
    vulkan_native_frame_execution_step::record,
    vulkan_native_frame_execution_step::submit,
    vulkan_native_frame_execution_step::present,
};

std::size_t native_frame_operation_stage_order(vulkan_native_frame_operation_stage stage)
{
    switch (stage) {
    case vulkan_native_frame_operation_stage::not_started:
        return 0;
    case vulkan_native_frame_operation_stage::native_function_table:
        return 1;
    case vulkan_native_frame_operation_stage::swapchain_create:
        return 2;
    case vulkan_native_frame_operation_stage::swapchain_images:
        return 3;
    case vulkan_native_frame_operation_stage::acquire:
        return 4;
    case vulkan_native_frame_operation_stage::command_recording:
        return 5;
    case vulkan_native_frame_operation_stage::submit:
        return 6;
    case vulkan_native_frame_operation_stage::present:
        return 7;
    case vulkan_native_frame_operation_stage::frame_completion:
        return 8;
    }

    return 0;
}

vulkan_native_frame_operation_stage_summary native_frame_stage_summary_or_default(
    const vulkan_native_frame_operation_summary& summary,
    vulkan_native_frame_operation_stage stage)
{
    const auto found = std::find_if(
        summary.stages.begin(),
        summary.stages.end(),
        [stage](const vulkan_native_frame_operation_stage_summary& candidate) {
            return candidate.stage == stage;
        });
    if (found != summary.stages.end()) {
        return *found;
    }

    return vulkan_native_frame_operation_stage_summary{
        .stage = stage,
        .checked = false,
        .ready = false,
        .blocked = false,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .diagnostic = {},
    };
}

vulkan_native_frame_operation_stage_diff_diagnostics native_frame_stage_diff_or_default(
    const vulkan_native_frame_operation_diff_diagnostics& diff,
    vulkan_native_frame_operation_stage stage)
{
    const auto found = std::find_if(
        diff.stages.begin(),
        diff.stages.end(),
        [stage](const vulkan_native_frame_operation_stage_diff_diagnostics& candidate) {
            return candidate.stage == stage;
        });
    if (found != diff.stages.end()) {
        return *found;
    }

    return vulkan_native_frame_operation_stage_diff_diagnostics{
        .stage = stage,
        .before_checked = false,
        .after_checked = false,
        .before_ready = false,
        .after_ready = false,
        .before_blocked = false,
        .after_blocked = false,
        .before_fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .after_fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .checked_changed = false,
        .readiness_changed = false,
        .became_ready = false,
        .became_blocked = false,
        .fallback_reason_changed = false,
        .before_diagnostic = {},
        .after_diagnostic = {},
    };
}

vulkan_native_frame_operation_stage_diff_diagnostics build_native_frame_stage_diff(
    const vulkan_native_frame_operation_stage_summary& before,
    const vulkan_native_frame_operation_stage_summary& after)
{
    return vulkan_native_frame_operation_stage_diff_diagnostics{
        .stage = after.stage,
        .before_checked = before.checked,
        .after_checked = after.checked,
        .before_ready = before.ready,
        .after_ready = after.ready,
        .before_blocked = before.blocked,
        .after_blocked = after.blocked,
        .before_fallback_reason = before.fallback_reason,
        .after_fallback_reason = after.fallback_reason,
        .checked_changed = before.checked != after.checked,
        .readiness_changed =
            before.ready != after.ready || before.blocked != after.blocked,
        .became_ready = !before.ready && after.ready,
        .became_blocked = !before.blocked && after.blocked,
        .fallback_reason_changed = before.fallback_reason != after.fallback_reason,
        .before_diagnostic = before.diagnostic,
        .after_diagnostic = after.diagnostic,
    };
}

vulkan_native_frame_operation_stage operation_stage_for_execution_step(
    vulkan_native_frame_execution_step step)
{
    switch (step) {
    case vulkan_native_frame_execution_step::acquire:
        return vulkan_native_frame_operation_stage::acquire;
    case vulkan_native_frame_execution_step::record:
        return vulkan_native_frame_operation_stage::command_recording;
    case vulkan_native_frame_execution_step::submit:
        return vulkan_native_frame_operation_stage::submit;
    case vulkan_native_frame_execution_step::present:
        return vulkan_native_frame_operation_stage::present;
    }

    return vulkan_native_frame_operation_stage::not_started;
}

bool native_frame_execution_step_checked(
    const vulkan_native_frame_operation_summary& summary,
    vulkan_native_frame_execution_step step)
{
    switch (step) {
    case vulkan_native_frame_execution_step::acquire:
        return summary.acquire_checked;
    case vulkan_native_frame_execution_step::record:
        return summary.command_recording_checked;
    case vulkan_native_frame_execution_step::submit:
        return summary.submit_batch_checked;
    case vulkan_native_frame_execution_step::present:
        return summary.present_operation_checked;
    }

    return false;
}

bool native_frame_execution_step_ready(
    const vulkan_native_frame_operation_summary& summary,
    vulkan_native_frame_execution_step step)
{
    switch (step) {
    case vulkan_native_frame_execution_step::acquire:
        return summary.acquire_ready;
    case vulkan_native_frame_execution_step::record:
        return summary.command_recording_ready;
    case vulkan_native_frame_execution_step::submit:
        return summary.submit_batch_ready;
    case vulkan_native_frame_execution_step::present:
        return summary.present_operation_ready && summary.frame_completion_ready;
    }

    return false;
}

vulkan_backend_fallback_reason fallback_reason_for_execution_step(
    vulkan_native_frame_execution_step step)
{
    switch (step) {
    case vulkan_native_frame_execution_step::acquire:
        return vulkan_backend_fallback_reason::acquire_image_failed;
    case vulkan_native_frame_execution_step::record:
        return vulkan_backend_fallback_reason::record_commands_failed;
    case vulkan_native_frame_execution_step::submit:
        return vulkan_backend_fallback_reason::submit_frame_failed;
    case vulkan_native_frame_execution_step::present:
        return vulkan_backend_fallback_reason::present_frame_failed;
    }

    return vulkan_backend_fallback_reason::not_requested;
}

vulkan_backend_fallback_reason native_frame_execution_fallback_reason(
    const vulkan_native_frame_operation_summary& summary,
    vulkan_native_frame_execution_step step)
{
    if (summary.fallback_reason != vulkan_backend_fallback_reason::none
        && summary.fallback_reason != vulkan_backend_fallback_reason::not_requested) {
        return summary.fallback_reason;
    }

    return fallback_reason_for_execution_step(step);
}

std::string native_frame_execution_step_diagnostic(
    vulkan_native_frame_execution_decision decision,
    vulkan_native_frame_execution_step step)
{
    switch (decision) {
    case vulkan_native_frame_execution_decision::not_checked:
        return "Native Vulkan frame execution step has no checked summary";
    case vulkan_native_frame_execution_decision::execute:
        return "Native Vulkan frame execution step is ready to execute";
    case vulkan_native_frame_execution_decision::skip:
        return std::string{"Native Vulkan frame execution step skipped: "}
            + std::string{native_frame_execution_step_name(step)};
    case vulkan_native_frame_execution_decision::fallback:
        return std::string{"Native Vulkan frame execution step falls back: "}
            + std::string{native_frame_execution_step_name(step)};
    }

    return "Native Vulkan frame execution step has unknown decision";
}

vulkan_native_frame_operation_step_execution_decision build_native_frame_step_execution_decision(
    const vulkan_native_frame_operation_summary& summary,
    const vulkan_native_frame_operation_diff_diagnostics& diff,
    vulkan_native_frame_execution_step step,
    bool blocked_by_previous_step)
{
    const vulkan_native_frame_operation_stage operation_stage =
        operation_stage_for_execution_step(step);
    const bool stage_checked = native_frame_execution_step_checked(summary, step);
    const bool stage_ready = native_frame_execution_step_ready(summary, step);
    const vulkan_native_frame_operation_stage_diff_diagnostics stage_diff =
        native_frame_stage_diff_or_default(diff, operation_stage);
    const bool fallback_context =
        summary.cpu_fallback_should_remain_active
        || summary.recoverable_failure
        || summary.fatal_failure
        || (summary.fallback_reason != vulkan_backend_fallback_reason::none
            && summary.fallback_reason != vulkan_backend_fallback_reason::not_requested);

    vulkan_native_frame_execution_decision decision =
        vulkan_native_frame_execution_decision::not_checked;
    if (!summary.checked) {
        decision = vulkan_native_frame_execution_decision::not_checked;
    } else if (blocked_by_previous_step || !stage_checked) {
        decision = vulkan_native_frame_execution_decision::skip;
    } else if (stage_ready) {
        decision = vulkan_native_frame_execution_decision::execute;
    } else if (summary.cpu_fallback_available && fallback_context) {
        decision = vulkan_native_frame_execution_decision::fallback;
    } else {
        decision = vulkan_native_frame_execution_decision::skip;
    }

    const bool cpu_fallback_required =
        decision == vulkan_native_frame_execution_decision::fallback;
    return vulkan_native_frame_operation_step_execution_decision{
        .step = step,
        .operation_stage = operation_stage,
        .decision = decision,
        .summary_checked = summary.checked,
        .stage_checked = stage_checked,
        .stage_ready = stage_ready,
        .diff_checked = diff.checked,
        .diff_changed = stage_diff.changed(),
        .diff_became_ready = stage_diff.became_ready,
        .diff_became_blocked = stage_diff.became_blocked,
        .blocked_by_previous_step = blocked_by_previous_step,
        .cpu_fallback_available = summary.cpu_fallback_available,
        .cpu_fallback_required = cpu_fallback_required,
        .fallback_reason = cpu_fallback_required
            ? native_frame_execution_fallback_reason(summary, step)
            : vulkan_backend_fallback_reason::not_requested,
        .diagnostic = native_frame_execution_step_diagnostic(decision, step),
    };
}

vulkan_native_frame_operation_summary native_frame_operation_summary_from_result(
    const vulkan_native_frame_operation_result& result)
{
    return vulkan_native_frame_operation_summary{
        .checked = result.checked,
        .status = result.status,
        .reached_stage = result.reached_stage,
        .blocker_stage = result.blocker_stage,
        .fallback_reason = result.fallback_reason,
        .native_function_table_checked = result.native_function_table_checked,
        .native_function_table_ready = result.native_function_table_ready,
        .swapchain_create_checked = result.swapchain_create_checked,
        .swapchain_create_ready = result.swapchain_create_ready,
        .swapchain_images_checked = result.swapchain_images_checked,
        .swapchain_images_ready = result.swapchain_images_ready,
        .acquire_checked = result.acquire_checked,
        .acquire_ready = result.acquire_ready,
        .command_recording_checked = result.command_recording_checked,
        .command_recording_ready = result.command_recording_ready,
        .submit_batch_checked = result.submit_batch_checked,
        .submit_batch_ready = result.submit_batch_ready,
        .present_operation_checked = result.present_operation_checked,
        .present_operation_ready = result.present_operation_ready,
        .frame_completion_ready = result.frame_completion_ready,
        .recoverable_failure = result.recoverable_failure,
        .fatal_failure = result.fatal_failure,
        .swapchain_out_of_date = result.swapchain_out_of_date,
        .suboptimal = result.suboptimal,
        .cpu_fallback_available = result.cpu_fallback_available,
        .cpu_fallback_should_remain_active =
            result.cpu_fallback_should_remain_active,
        .native_function_table_status = result.native_function_table_status,
        .missing_required_extension = result.missing_required_extension,
        .missing_symbol_name = result.missing_symbol_name,
        .stages = result.stages,
        .diagnostic = result.diagnostic,
    };
}

template <typename T>
void apply_sdk_native_path_readiness(
    T& result,
    const vulkan_sdk_native_path_readiness& sdk_native_path)
{
    result.sdk_native_path_checked = sdk_native_path.checked;
    result.sdk_adapter_ready = sdk_native_path.ready();
    result.sdk_native_path_status = sdk_native_path.status;
    result.sdk_capability_status = sdk_native_path.sdk_capability_status;
    result.sdk_adapter_fallback_status = sdk_native_path.sdk_adapter_fallback_status;
    result.sdk_diagnostic = sdk_native_path.diagnostic;
}

const vulkan_batch_resource_binding_snapshot* find_binding_snapshot_for_batch(
    const vulkan_backend_resource_binding_state& resource_bindings,
    const vulkan_draw_batch& batch,
    std::size_t batch_index)
{
    if (batch_index < resource_bindings.batch_snapshots.size()) {
        const vulkan_batch_resource_binding_snapshot& snapshot =
            resource_bindings.batch_snapshots[batch_index];
        if (snapshot.command_index == batch.command_index && snapshot.batch_kind == batch.kind) {
            return &snapshot;
        }
    }

    for (const vulkan_batch_resource_binding_snapshot& snapshot :
         resource_bindings.batch_snapshots) {
        if (snapshot.command_index == batch.command_index && snapshot.batch_kind == batch.kind) {
            return &snapshot;
        }
    }
    return nullptr;
}

vulkan_command_packet make_command_packet(
    const vulkan_draw_batch& batch,
    const vulkan_batch_resource_binding_snapshot& bindings,
    std::size_t packet_index)
{
    return vulkan_command_packet{
        .category = command_packet_category_for(batch.kind),
        .batch_kind = batch.kind,
        .command_index = batch.command_index,
        .packet_index = packet_index,
        .node_id = batch.node_id,
        .bounds = batch.bounds,
        .clipped_bounds = batch.clipped_bounds,
        .scissor = batch.scissor,
        .vertices = batch.vertices,
        .descriptor_set_count = bindings.descriptor_set_count,
        .binding_count = bindings.binding_count,
        .bindings = bindings.bindings,
    };
}

vulkan_command_packet_execution_snapshot make_execution_event(
    vulkan_command_packet_execution_event event)
{
    return vulkan_command_packet_execution_snapshot{
        .event = event,
        .attempted = true,
    };
}

vulkan_command_packet_execution_snapshot make_packet_execution_event(
    const vulkan_command_packet& packet)
{
    return vulkan_command_packet_execution_snapshot{
        .event = vulkan_command_packet_execution_event::packet,
        .category = packet.category,
        .batch_kind = packet.batch_kind,
        .packet_index = packet.packet_index,
        .command_index = packet.command_index,
        .attempted = true,
    };
}

vulkan_recorded_draw_batch make_recorded_draw_batch(
    const vulkan_draw_batch& batch,
    std::size_t recording_index)
{
    return vulkan_recorded_draw_batch{
        .kind = batch.kind,
        .command_index = batch.command_index,
        .recording_index = recording_index,
        .bounds = batch.bounds,
        .clipped_bounds = batch.clipped_bounds,
        .scissor = batch.scissor,
    };
}

void mark_recorder_failure(
    vulkan_backend_command_recorder_state& state,
    vulkan_command_recorder_failure_stage stage,
    std::size_t recording_index)
{
    state.failure_stage = stage;
    state.failure_recording_index = recording_index;
}

vulkan_backend_frame_present_policy_state make_frame_present_policy_state()
{
    return vulkan_backend_frame_present_policy_state{
        .checked = true,
        .acquire = vulkan_frame_acquire_policy_diagnostics{
            .checked = true,
            .requested = false,
            .swapchain_status = vulkan_swapchain_acquire_status::not_requested,
            .image_id = {},
            .image_available = false,
            .image_acquired = false,
            .backpressured = false,
            .status = vulkan_frame_acquire_policy_status::not_requested,
            .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        },
        .present = vulkan_frame_present_result_summary{
            .checked = true,
            .image_present_requested = false,
            .frame_present_requested = false,
            .image_id = {},
            .swapchain_status = vulkan_swapchain_present_status::not_requested,
            .image_presented = false,
            .frame_presented = false,
            .status = vulkan_frame_present_result_status::not_requested,
            .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        },
    };
}

void mark_acquire_policy_requested(vulkan_backend_frame_present_policy_state& state)
{
    state.checked = true;
    ++state.acquire_request_count;
    state.acquire.checked = true;
    state.acquire.requested = true;
    state.acquire.status = vulkan_frame_acquire_policy_status::not_requested;
    state.acquire.fallback_reason = vulkan_backend_fallback_reason::not_requested;
}

void mark_acquire_policy_result(
    vulkan_backend_frame_present_policy_state& state,
    const vulkan_swapchain_acquire_result& acquire)
{
    state.acquire.swapchain_status = acquire.status;
    state.acquire.image_id = acquire.image.id;
    state.acquire.image_available = acquire.image.available;
    state.acquire.image_acquired = acquire.image.acquired;

    if (acquire.completed()) {
        state.acquire.status = vulkan_frame_acquire_policy_status::acquired;
        state.acquire.fallback_reason = vulkan_backend_fallback_reason::none;
        return;
    }

    const bool backpressured =
        acquire.status == vulkan_swapchain_acquire_status::backpressured
        || acquire.status == vulkan_swapchain_acquire_status::timeout
        || (acquire.status == vulkan_swapchain_acquire_status::acquired
            && !acquire.image.ready_for_recording());
    state.acquire.backpressured = backpressured;
    state.backpressure_detected = backpressured;
    state.acquire.status = backpressured
        ? vulkan_frame_acquire_policy_status::backpressured
        : vulkan_frame_acquire_policy_status::failed;
    state.acquire.fallback_reason = vulkan_backend_fallback_reason::acquire_image_failed;
}

void mark_present_policy_image_requested(
    vulkan_backend_frame_present_policy_state& state,
    vulkan_swapchain_image_id image_id)
{
    state.checked = true;
    ++state.present_image_request_count;
    state.present.checked = true;
    state.present.image_present_requested = true;
    state.present.image_id = image_id;
    state.present.status = vulkan_frame_present_result_status::not_requested;
    state.present.fallback_reason = vulkan_backend_fallback_reason::not_requested;
}

void mark_present_policy_image_result(
    vulkan_backend_frame_present_policy_state& state,
    const vulkan_swapchain_present_result& present)
{
    state.present.swapchain_status = present.status;
    state.present.image_id = present.image_id;
    state.present.image_presented = present.completed();
    state.present.status = present.completed()
        ? vulkan_frame_present_result_status::image_presented
        : vulkan_frame_present_result_status::image_failed;
    state.present.fallback_reason = present.completed()
        ? vulkan_backend_fallback_reason::none
        : vulkan_backend_fallback_reason::present_image_failed;
}

void mark_present_policy_frame_requested(vulkan_backend_frame_present_policy_state& state)
{
    state.present.frame_present_requested = true;
}

void mark_present_policy_frame_result(
    vulkan_backend_frame_present_policy_state& state,
    bool frame_presented)
{
    state.present.frame_presented = frame_presented;
    state.present.status = frame_presented
        ? vulkan_frame_present_result_status::frame_presented
        : vulkan_frame_present_result_status::frame_failed;
    state.present.fallback_reason = frame_presented
        ? vulkan_backend_fallback_reason::none
        : vulkan_backend_fallback_reason::present_frame_failed;
}

std::size_t clamp_extent_value(std::size_t value, std::size_t min_value, std::size_t max_value)
{
    return std::clamp(value, min_value, max_value);
}

vulkan_backend_swapchain_policy_state make_swapchain_policy_state(vulkan_surface_extent requested_extent)
{
    vulkan_swapchain_extent_policy_state extent;
    extent.checked = true;
    extent.requested_extent = requested_extent;
    extent.extent_supported = requested_extent.valid();
    extent.selected_extent = requested_extent.valid()
        ? vulkan_surface_extent{
            .width = clamp_extent_value(
                requested_extent.width,
                extent.min_extent.width,
                extent.max_extent.width),
            .height = clamp_extent_value(
                requested_extent.height,
                extent.min_extent.height,
                extent.max_extent.height),
        }
        : vulkan_surface_extent{};
    extent.extent_clamped =
        extent.selected_extent.width != requested_extent.width
        || extent.selected_extent.height != requested_extent.height;

    return vulkan_backend_swapchain_policy_state{
        .checked = true,
        .extent = extent,
        .present_mode = vulkan_swapchain_present_mode_policy_state{
            .checked = true,
            .requested_mode = vulkan_swapchain_present_mode::fifo,
            .selected_mode = vulkan_swapchain_present_mode::fifo,
            .requested_mode_supported = true,
            .fallback_to_fifo = false,
        },
    };
}

std::size_t infer_swapchain_image_count_for_acquire(
    const vulkan_backend_lifecycle_readiness& lifecycle,
    const vulkan_swapchain_acquire_result& acquire)
{
    std::size_t image_count = lifecycle.swapchain.checked
        ? lifecycle.swapchain.min_image_count
        : 0U;
    if (image_count == 0 && lifecycle.effective_swapchain_ready()) {
        image_count = 2;
    }
    if (acquire.image.id.value >= image_count) {
        image_count = acquire.image.id.value + 1;
    }

    return image_count;
}

vulkan_swapchain_handle swapchain_handle_for_acquire(
    const vulkan_backend_lifecycle_readiness& lifecycle)
{
    if (lifecycle.swapchain.checked) {
        return lifecycle.swapchain.handle;
    }
    if (lifecycle.effective_swapchain_ready()) {
        return vulkan_swapchain_handle{.value = 1};
    }

    return {};
}

bool ensure_frame_plan_pipelines(
    vulkan_pipeline_cache_interface& pipeline_cache,
    const vulkan_frame_plan& plan,
    vulkan_backend_frame_result& result)
{
    for (const vulkan_draw_batch& batch : plan.batches) {
        if (!pipeline_cache.ensure_pipeline(batch)) {
            result.pipeline = pipeline_cache.pipeline_state();
            frame_lifecycle::mark_fallback(
                result,
                vulkan_backend_fallback_reason::pipeline_unavailable);
            return false;
        }
    }

    result.pipeline = pipeline_cache.pipeline_state();
    return true;
}

vulkan_frame_sync_token_id make_frame_sync_token(
    const vulkan_frame_in_flight_id& frame,
    std::size_t slot,
    vulkan_frame_sync_token_kind kind)
{
    return vulkan_frame_sync_token_id{
        .value = (frame.sequence * 100) + (frame.index * 10) + slot,
        .kind = kind,
    };
}

vulkan_backend_frame_sync_state make_diagnostic_frame_sync_state()
{
    const vulkan_frame_in_flight_id frame{
        .index = 0,
        .frame_count = 2,
        .sequence = 1,
    };

    const vulkan_frame_sync_token_id image_available = make_frame_sync_token(
        frame,
        1,
        vulkan_frame_sync_token_kind::semaphore);
    const vulkan_frame_sync_token_id acquire_fence = make_frame_sync_token(
        frame,
        2,
        vulkan_frame_sync_token_kind::fence);
    const vulkan_frame_sync_token_id render_finished = make_frame_sync_token(
        frame,
        3,
        vulkan_frame_sync_token_kind::semaphore);
    const vulkan_frame_sync_token_id frame_fence = make_frame_sync_token(
        frame,
        4,
        vulkan_frame_sync_token_kind::fence);

    return vulkan_backend_frame_sync_state{
        .frame = frame,
        .acquire_signal_image_available_semaphore = vulkan_frame_sync_signal_state{
            .token = image_available,
        },
        .acquire_signal_fence = vulkan_frame_sync_signal_state{
            .token = acquire_fence,
        },
        .submit_wait_image_available_semaphore = vulkan_frame_sync_wait_state{
            .token = image_available,
        },
        .submit_signal_render_finished_semaphore = vulkan_frame_sync_signal_state{
            .token = render_finished,
        },
        .submit_signal_frame_fence = vulkan_frame_sync_signal_state{
            .token = frame_fence,
        },
        .present_wait_render_finished_semaphore = vulkan_frame_sync_wait_state{
            .token = render_finished,
        },
    };
}

void request_signal(vulkan_frame_sync_signal_state& state)
{
    state.requested = true;
    state.status = vulkan_frame_sync_signal_status::pending;
}

void complete_signal(vulkan_frame_sync_signal_state& state, bool success)
{
    state.status = success
        ? vulkan_frame_sync_signal_status::signaled
        : vulkan_frame_sync_signal_status::failed;
}

void request_wait(vulkan_frame_sync_wait_state& state)
{
    state.requested = true;
    state.status = vulkan_frame_sync_wait_status::pending;
}

void complete_wait(vulkan_frame_sync_wait_state& state, bool success)
{
    state.status = success
        ? vulkan_frame_sync_wait_status::waited
        : vulkan_frame_sync_wait_status::failed;
}

vulkan_command_buffer_id make_command_buffer_id(const vulkan_frame_in_flight_id& frame)
{
    return vulkan_command_buffer_id{
        .value = (frame.sequence * 1000) + (frame.index * 10) + 1,
    };
}

vulkan_backend_frame_resource_lifetime_state make_frame_resource_lifetime_state(
    const vulkan_frame_plan& plan)
{
    return vulkan_backend_frame_resource_lifetime_state{
        .checked = true,
        .fallback_cleanup = false,
        .planned_batch_count = plan.batches.size(),
        .tracked_resource_count = 0,
        .acquired_resource_count = 0,
        .released_resource_count = 0,
        .pending_resource_count = 0,
        .fallback_release_count = 0,
        .resources = {},
    };
}

std::string frame_resource_id(
    std::string prefix,
    std::size_t value)
{
    prefix += ":";
    prefix += std::to_string(value);
    return prefix;
}

std::string descriptor_set_resource_id(
    const vulkan_batch_resource_binding_snapshot& snapshot)
{
    return "descriptor_set:"
        + (snapshot.node_id.empty() ? std::to_string(snapshot.command_index) : snapshot.node_id);
}

void refresh_frame_resource_lifetime_counts(
    vulkan_backend_frame_resource_lifetime_state& state)
{
    state.tracked_resource_count = state.resources.size();
    state.acquired_resource_count = 0;
    state.released_resource_count = 0;
    state.pending_resource_count = 0;
    state.fallback_release_count = 0;

    for (const vulkan_frame_resource_lifetime_snapshot& resource : state.resources) {
        if (resource.acquired) {
            ++state.acquired_resource_count;
        }
        if (resource.released) {
            ++state.released_resource_count;
        }
        if (resource.pending()) {
            ++state.pending_resource_count;
        }
        if (resource.release_stage == vulkan_frame_resource_release_stage::fallback_cleanup) {
            ++state.fallback_release_count;
        }
    }
}

void track_frame_resource(
    vulkan_backend_frame_resource_lifetime_state& state,
    vulkan_frame_resource_lifetime_snapshot resource)
{
    resource.acquired = true;
    state.resources.push_back(std::move(resource));
    refresh_frame_resource_lifetime_counts(state);
}

void track_descriptor_set_resources(
    vulkan_backend_frame_resource_lifetime_state& state,
    const vulkan_backend_resource_binding_state& resource_bindings)
{
    for (const vulkan_batch_resource_binding_snapshot& snapshot : resource_bindings.batch_snapshots) {
        if (!snapshot.completed() || snapshot.descriptor_set_count == 0) {
            continue;
        }
        track_frame_resource(
            state,
            vulkan_frame_resource_lifetime_snapshot{
                .kind = vulkan_frame_resource_kind::descriptor_set,
                .resource_id = descriptor_set_resource_id(snapshot),
                .command_index = snapshot.command_index,
            });
    }
}

void track_swapchain_image_resource(
    vulkan_backend_frame_resource_lifetime_state& state,
    vulkan_swapchain_image_id image_id)
{
    track_frame_resource(
        state,
        vulkan_frame_resource_lifetime_snapshot{
            .kind = vulkan_frame_resource_kind::swapchain_image,
            .resource_id = frame_resource_id("swapchain_image", image_id.value),
        });
}

void track_command_buffer_resource(
    vulkan_backend_frame_resource_lifetime_state& state,
    vulkan_command_buffer_id command_buffer)
{
    if (!command_buffer.valid()) {
        refresh_frame_resource_lifetime_counts(state);
        return;
    }

    track_frame_resource(
        state,
        vulkan_frame_resource_lifetime_snapshot{
            .kind = vulkan_frame_resource_kind::command_buffer,
            .resource_id = frame_resource_id("command_buffer", command_buffer.value),
        });
}

void release_frame_resources(
    vulkan_backend_frame_resource_lifetime_state& state,
    vulkan_frame_resource_release_stage release_stage)
{
    if (!state.checked) {
        return;
    }
    if (release_stage == vulkan_frame_resource_release_stage::fallback_cleanup) {
        state.fallback_cleanup = true;
    }

    for (vulkan_frame_resource_lifetime_snapshot& resource : state.resources) {
        if (!resource.acquired || resource.released) {
            continue;
        }
        resource.released = true;
        resource.release_stage = release_stage;
    }

    refresh_frame_resource_lifetime_counts(state);
}

vulkan_backend_command_buffer_submit_state make_command_buffer_submit_state(
    const vulkan_frame_in_flight_id& frame,
    std::size_t planned_batch_count)
{
    const vulkan_command_buffer_id command_buffer = make_command_buffer_id(frame);
    return vulkan_backend_command_buffer_submit_state{
        .checked = true,
        .recording = vulkan_command_buffer_recording_diagnostics{
            .command_buffer = command_buffer,
            .planned_batch_count = planned_batch_count,
        },
        .submit = vulkan_frame_submit_diagnostics{
            .command_buffer = command_buffer,
            .frame = frame,
        },
    };
}

void mark_command_buffer_recording_started(vulkan_backend_command_buffer_submit_state& state)
{
    state.recording.begin_requested = true;
    state.recording.status = vulkan_command_buffer_recording_status::recording;
}

void mark_command_buffer_recording_failed(
    vulkan_backend_command_buffer_submit_state& state,
    const vulkan_backend_command_recorder_state& recorder)
{
    state.recording.status = vulkan_command_buffer_recording_status::failed;
    state.recording.finish_requested =
        recorder.failure_stage == vulkan_command_recorder_failure_stage::finish_recording;
    state.recording.recorded_batch_count = recorder.recorded_batch_count;
    state.recording.failure_stage = recorder.failure_stage;
    state.recording.failure_recording_index = recorder.failure_recording_index;
}

void mark_command_buffer_recording_finished(
    vulkan_backend_command_buffer_submit_state& state,
    const vulkan_backend_command_recorder_state& recorder)
{
    state.recording.finish_requested = true;
    state.recording.recorded_batch_count = recorder.recorded_batch_count;
    state.recording.failure_stage = recorder.failure_stage;
    state.recording.failure_recording_index = recorder.failure_recording_index;
    state.recording.status = recorder.failed()
        ? vulkan_command_buffer_recording_status::failed
        : vulkan_command_buffer_recording_status::recorded;
}

void mark_frame_submit_requested(
    vulkan_backend_command_buffer_submit_state& state,
    std::size_t submitted_batch_count)
{
    state.submit.submit_requested = true;
    state.submit.submitted_batch_count = submitted_batch_count;
    state.submit.status = vulkan_frame_submit_status::pending;
    state.submit.wait_image_available_status = vulkan_frame_sync_wait_status::pending;
    state.submit.signal_render_finished_status = vulkan_frame_sync_signal_status::pending;
    state.submit.signal_frame_fence_status = vulkan_frame_sync_signal_status::pending;
}

void mark_frame_submit_result(
    vulkan_backend_command_buffer_submit_state& state,
    const vulkan_backend_frame_sync_state& sync,
    bool success)
{
    state.submit.status = success
        ? vulkan_frame_submit_status::submitted
        : vulkan_frame_submit_status::failed;
    state.submit.wait_image_available_status =
        sync.submit_wait_image_available_semaphore.status;
    state.submit.signal_render_finished_status =
        sync.submit_signal_render_finished_semaphore.status;
    state.submit.signal_frame_fence_status = sync.submit_signal_frame_fence.status;
}

vulkan_descriptor_validation_status descriptor_validation_status_for(
    const vulkan_backend_descriptor_validation_state& validation)
{
    if (!validation.checked) {
        return vulkan_descriptor_validation_status::not_checked;
    }
    for (const vulkan_descriptor_set_validation_snapshot& descriptor_set :
         validation.descriptor_sets) {
        if (!descriptor_set.completed()) {
            return descriptor_set.status;
        }
    }
    return validation.completed()
        ? vulkan_descriptor_validation_status::valid
        : vulkan_descriptor_validation_status::invalid_layout;
}

vulkan_command_recorder_gate_state make_command_recorder_gate_state(
    const vulkan_backend_resource_binding_state& resource_bindings,
    const vulkan_backend_resource_registry_state& resource_registry)
{
    const vulkan_backend_descriptor_validation_state& validation =
        resource_bindings.descriptor_validation;
    vulkan_command_recorder_gate_state state{
        .checked = true,
        .recording_allowed = false,
        .resource_bindings_checked = resource_bindings.checked,
        .resource_bindings_completed = resource_bindings.completed(),
        .descriptor_validation_checked = validation.checked,
        .descriptor_validation_completed = validation.completed(),
        .resource_registry_checked = resource_registry.checked,
        .resource_registry_completed = resource_registry.completed(),
        .missing_required_resource = validation.missing_required_resource,
        .duplicate_binding = validation.duplicate_binding,
        .invalid_layout = validation.invalid_layout,
        .status = vulkan_command_recorder_gate_status::not_checked,
        .descriptor_status = descriptor_validation_status_for(validation),
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .blocked_batch_kind = validation.failed_batch_kind,
        .blocked_command_index = validation.failed_command_index,
        .blocked_binding_kind = validation.failed_binding_kind,
        .blocked_binding = validation.failed_binding,
        .blocked_resource_id = resource_bindings.missing_resource_id,
        .planned_batch_count = resource_bindings.planned_batch_count,
        .descriptor_set_count = validation.descriptor_set_count,
        .invalid_descriptor_set_count = validation.invalid_descriptor_set_count,
        .missing_resource_count = resource_registry.missing_resource_count,
    };

    if (resource_bindings.completed() && resource_registry.completed()) {
        state.recording_allowed = true;
        state.status = vulkan_command_recorder_gate_status::allowed;
        state.descriptor_status = vulkan_descriptor_validation_status::valid;
        state.fallback_reason = vulkan_backend_fallback_reason::none;
        return state;
    }

    state.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
    if (validation.checked && !validation.completed()) {
        state.status = vulkan_command_recorder_gate_status::blocked_by_descriptor_validation;
        return state;
    }
    if (resource_bindings.checked && !resource_bindings.completed()) {
        state.status = vulkan_command_recorder_gate_status::blocked_by_resource_binding;
        state.blocked_batch_kind = resource_bindings.missing_batch_kind;
        state.blocked_command_index = resource_bindings.missing_command_index;
        state.blocked_binding_kind = resource_bindings.missing_binding_kind;
        state.blocked_binding = 0;
        return state;
    }

    state.status = vulkan_command_recorder_gate_status::blocked_by_resource_registry;
    if (!resource_registry.missing_resources.empty()) {
        const vulkan_resource_registry_missing_resource& missing =
            resource_registry.missing_resources.front();
        state.blocked_batch_kind = missing.batch_kind;
        state.blocked_command_index = missing.command_index;
        state.blocked_binding_kind = missing.kind;
        state.blocked_binding = missing.binding;
        state.blocked_resource_id = missing.resource_id;
    }
    return state;
}

} // namespace

std::string_view vulkan_loader_required_symbol_name()
{
    return k_vulkan_loader_required_symbol_name;
}

std::vector<std::string> default_vulkan_loader_library_names()
{
#if defined(_WIN32)
    return {"vulkan-1.dll"};
#elif defined(__APPLE__)
    return {"libvulkan.1.dylib", "libvulkan.dylib"};
#else
    return {"libvulkan.so.1", "libvulkan.so"};
#endif
}

fake_vulkan_loader::fake_vulkan_loader()
    : fake_vulkan_loader(fake_vulkan_loader_options{})
{
}

fake_vulkan_loader::fake_vulkan_loader(fake_vulkan_loader_options options)
    : options_(std::move(options))
{
}

vulkan_loader_probe_result fake_vulkan_loader::probe_loader(
    const vulkan_loader_probe_request& request)
{
    vulkan_loader_probe_result result = make_loader_probe_result(request);
    const std::vector<std::string> candidate_names = merge_loader_candidate_names(
        request,
        {options_.library_name});

    for (const std::string& candidate_name : candidate_names) {
        record_loader_attempt(result, candidate_name);

        if (!options_.library_available || candidate_name != options_.library_name) {
            continue;
        }

        mark_loader_library_found(result, candidate_name);
        if (options_.required_symbol_available) {
            mark_loader_available(result, candidate_name);
            return result;
        }
    }

    mark_loader_probe_completed(result);
    return result;
}

vulkan_loader_probe_result system_vulkan_loader::probe_loader(
    const vulkan_loader_probe_request& request)
{
    vulkan_loader_probe_result result = make_loader_probe_result(request);
    const std::vector<std::string> candidate_names = merge_loader_candidate_names(
        request,
        default_vulkan_loader_library_names());

    for (const std::string& candidate_name : candidate_names) {
        record_loader_attempt(result, candidate_name);

        native_loader_library library(candidate_name);
        if (!library.valid()) {
            continue;
        }

        mark_loader_library_found(result, candidate_name);
        if (library.has_symbol(result.required_symbol_name)) {
            mark_loader_available(result, candidate_name);
            return result;
        }
    }

    mark_loader_probe_completed(result);
    return result;
}

vulkan_loader_probe_result probe_vulkan_loader(
    vulkan_loader_interface& loader,
    const vulkan_loader_probe_request& request)
{
    return loader.probe_loader(request);
}

vulkan_loader_readiness_state make_vulkan_loader_readiness_state(
    const vulkan_loader_probe_result& probe_result)
{
    const vulkan_loader_readiness_status status =
        loader_readiness_status_for_probe(probe_result.status);
    const bool instance_ready =
        probe_result.checked
        && status == vulkan_loader_readiness_status::ready
        && probe_result.library_found
        && probe_result.required_symbol_found;

    return vulkan_loader_readiness_state{
        .checked = probe_result.checked,
        .status = status,
        .probe_status = probe_result.status,
        .loader_library_available = probe_result.library_found,
        .instance_proc_address_available = probe_result.required_symbol_found,
        .instance_ready = instance_ready,
        .loaded_library_name = probe_result.loaded_library_name,
        .required_symbol_name = probe_result.required_symbol_name,
        .attempted_library_count = probe_result.attempted_library_count,
    };
}

bool vulkan_native_function_table_diagnostics::ready_for_backend_path() const
{
    return checked && status == vulkan_native_function_table_status::ready
        && fallback_reason == vulkan_backend_fallback_reason::none
        && loader_ready && required_extensions_ready
        && command_buffer_recording_ready && queue_submit_ready
        && swapchain_create_ready && swapchain_destroy_ready
        && swapchain_images_ready && swapchain_acquire_ready && queue_present_ready
        && missing_required_symbol_count == 0;
}

bool vulkan_native_function_table_diagnostics::blocked() const
{
    return checked && status != vulkan_native_function_table_status::ready;
}

bool vulkan_native_swapchain_entrypoint_readiness::ready_for_swapchain_create() const
{
    return checked && function_table_checked && required_extensions_ready
        && create_swapchain_ready && destroy_swapchain_ready
        && get_swapchain_images_ready;
}

bool vulkan_native_swapchain_entrypoint_readiness::ready_for_swapchain_acquire() const
{
    return ready_for_swapchain_create() && acquire_next_image_ready;
}

bool vulkan_native_swapchain_entrypoint_readiness::ready_for_swapchain_present() const
{
    return ready_for_swapchain_acquire() && queue_present_ready;
}

bool vulkan_native_swapchain_entrypoint_readiness::ready_for_swapchain_path() const
{
    return ready_for_swapchain_present()
        && function_table_status == vulkan_native_function_table_status::ready;
}

bool vulkan_native_swapchain_entrypoint_readiness::blocked() const
{
    return checked && !ready_for_swapchain_path();
}

fake_vulkan_native_symbol_resolver::fake_vulkan_native_symbol_resolver()
    : fake_vulkan_native_symbol_resolver(fake_vulkan_native_symbol_resolver_options{})
{
}

fake_vulkan_native_symbol_resolver::fake_vulkan_native_symbol_resolver(
    fake_vulkan_native_symbol_resolver_options options)
    : options_(std::move(options))
{
}

vulkan_native_function_pointer fake_vulkan_native_symbol_resolver::resolve_symbol(
    std::string_view symbol_name)
{
    ++state_.resolve_call_count;
    state_.requested_symbols.push_back(std::string{symbol_name});

    const bool explicitly_missing =
        contains_symbol_name(options_.missing_symbols, symbol_name);
    const bool explicitly_available =
        contains_symbol_name(options_.available_symbols, symbol_name);
    const bool available =
        !explicitly_missing && (options_.default_available || explicitly_available);

    if (!available) {
        state_.missing_symbols.push_back(std::string{symbol_name});
        return {};
    }

    state_.resolved_symbols.push_back(std::string{symbol_name});
    const std::uintptr_t base =
        options_.pointer_base.valid() ? options_.pointer_base.value : 1000;
    return vulkan_native_function_pointer{
        .value = base + state_.resolve_call_count,
    };
}

const fake_vulkan_native_symbol_resolver_state&
fake_vulkan_native_symbol_resolver::state() const
{
    return state_;
}

fake_vulkan_native_instance_symbol_resolver::fake_vulkan_native_instance_symbol_resolver()
    : fake_vulkan_native_instance_symbol_resolver(
        fake_vulkan_native_instance_symbol_resolver_options{})
{
}

fake_vulkan_native_instance_symbol_resolver::fake_vulkan_native_instance_symbol_resolver(
    fake_vulkan_native_instance_symbol_resolver_options options)
    : options_(std::move(options))
{
}

vulkan_native_function_pointer
fake_vulkan_native_instance_symbol_resolver::get_instance_proc_address() const
{
    return options_.get_instance_proc_address;
}

vulkan_native_function_pointer
fake_vulkan_native_instance_symbol_resolver::resolve_instance_symbol(
    vulkan_instance_handle instance,
    std::string_view symbol_name)
{
    ++state_.resolve_call_count;
    state_.requested_instance_handles.push_back(instance.value);
    state_.requested_symbols.push_back(std::string{symbol_name});

    const bool explicitly_missing =
        contains_symbol_name(options_.missing_symbols, symbol_name);
    const bool explicitly_available =
        contains_symbol_name(options_.available_symbols, symbol_name);
    const bool available =
        options_.get_instance_proc_address.valid() && instance.valid() && !explicitly_missing
        && (options_.default_available || explicitly_available);

    if (!available) {
        state_.missing_symbols.push_back(std::string{symbol_name});
        return {};
    }

    state_.resolved_symbols.push_back(std::string{symbol_name});
    const std::uintptr_t base =
        options_.pointer_base.valid() ? options_.pointer_base.value : 2000;
    return vulkan_native_function_pointer{
        .value = base + state_.resolve_call_count,
    };
}

const fake_vulkan_native_instance_symbol_resolver_state&
fake_vulkan_native_instance_symbol_resolver::state() const
{
    return state_;
}

fake_vulkan_native_physical_device_enumerator::fake_vulkan_native_physical_device_enumerator()
    : fake_vulkan_native_physical_device_enumerator(
        fake_vulkan_native_physical_device_enumerator_options{})
{
}

fake_vulkan_native_physical_device_enumerator::fake_vulkan_native_physical_device_enumerator(
    fake_vulkan_native_physical_device_enumerator_options options)
    : options_(std::move(options))
{
}

vulkan_native_physical_device_enumeration_result
fake_vulkan_native_physical_device_enumerator::enumerate_physical_devices(
    const vulkan_native_physical_device_dispatch_table& dispatch_table)
{
    vulkan_native_physical_device_enumeration_result result =
        make_native_physical_device_enumeration_result(dispatch_table);

    if (!dispatch_table.ready_for_enumeration()) {
        result.status =
            vulkan_native_physical_device_enumeration_status::dispatch_table_unavailable;
        result.diagnostic = dispatch_table.diagnostic.empty()
            ? "Native Vulkan physical device dispatch table is unavailable"
            : dispatch_table.diagnostic;
        return result;
    }

    ++state_.enumerate_call_count;
    state_.last_instance = dispatch_table.instance;
    state_.last_enumerate_physical_devices =
        dispatch_table.enumerate_physical_devices;

    if (options_.fail_enumeration) {
        result.status = vulkan_native_physical_device_enumeration_status::enumeration_failed;
        result.native_result = options_.failure_result;
        result.diagnostic = "Native Vulkan physical device enumeration failed";
        return result;
    }

    result.physical_devices = options_.physical_devices;
    result.physical_device_count = result.physical_devices.size();
    if (result.physical_devices.empty()) {
        result.status = vulkan_native_physical_device_enumeration_status::no_devices;
        result.diagnostic = "Native Vulkan physical device enumeration returned no devices";
        return result;
    }

    result.status = vulkan_native_physical_device_enumeration_status::ready;
    result.diagnostic = "Native Vulkan physical devices enumerated";
    return result;
}

const fake_vulkan_native_physical_device_enumerator_state&
fake_vulkan_native_physical_device_enumerator::state() const
{
    return state_;
}

fake_vulkan_native_physical_device_selector::fake_vulkan_native_physical_device_selector()
    : fake_vulkan_native_physical_device_selector(
        fake_vulkan_native_physical_device_selector_options{})
{
}

fake_vulkan_native_physical_device_selector::fake_vulkan_native_physical_device_selector(
    fake_vulkan_native_physical_device_selector_options options)
    : options_(std::move(options))
{
}

vulkan_native_physical_device_selection_result
fake_vulkan_native_physical_device_selector::select_physical_device(
    const vulkan_native_physical_device_enumeration_result& enumeration,
    const vulkan_device_create_request& request)
{
    vulkan_native_physical_device_selection_result result =
        make_native_physical_device_selection_result(enumeration, request);
    ++state_.select_call_count;

    if (!enumeration.ready_for_device_selection()) {
        if (enumeration.status
            == vulkan_native_physical_device_enumeration_status::no_devices) {
            result.status = vulkan_native_physical_device_selection_status::no_devices;
            result.diagnostic = "No native Vulkan physical devices are available";
        } else {
            result.status =
                vulkan_native_physical_device_selection_status::enumeration_unavailable;
            result.diagnostic =
                "Native Vulkan physical device enumeration is unavailable";
        }
        return result;
    }

    const std::vector<vulkan_device_queue_capability> required_capabilities =
        device_detail::requested_queue_capabilities(request);
    for (const vulkan_physical_device_handle physical_device :
         enumeration.physical_devices) {
        state_.inspected_physical_devices.push_back(physical_device);

        vulkan_native_physical_device_selection_candidate candidate{
            .physical_device = physical_device,
            .queue_families = queue_families_for_physical_device(
                options_.queue_families,
                physical_device),
            .selected_queue_families = {},
            .missing_required_queue = {},
            .selected = false,
        };

        for (const vulkan_device_queue_capability capability : required_capabilities) {
            auto matching_family = std::find_if(
                candidate.queue_families.begin(),
                candidate.queue_families.end(),
                [capability](const vulkan_native_physical_device_queue_family& family) {
                    return family.usable() && family.supports(capability);
                });
            if (matching_family == candidate.queue_families.end()) {
                candidate.missing_required_queue =
                    std::string{device_queue_capability_name(capability)};
                break;
            }

            candidate.selected_queue_families.push_back(
                make_queue_family_selection(capability, *matching_family));
        }

        if (candidate.missing_required_queue.empty()) {
            candidate.selected = true;
            result.selected_physical_device = physical_device;
            result.selected_queue_families = candidate.selected_queue_families;
            result.missing_required_queue.clear();
            result.candidates.push_back(candidate);
            result.status = vulkan_native_physical_device_selection_status::selected;
            result.diagnostic =
                "Native Vulkan physical device and queue families selected";
            return result;
        }

        if (result.missing_required_queue.empty()) {
            result.missing_required_queue = candidate.missing_required_queue;
        }
        result.candidates.push_back(candidate);
    }

    result.status = vulkan_native_physical_device_selection_status::missing_required_queue;
    result.diagnostic =
        "No native Vulkan physical device exposes the required queue families";
    return result;
}

const fake_vulkan_native_physical_device_selector_state&
fake_vulkan_native_physical_device_selector::state() const
{
    return state_;
}

vulkan_native_instance_proc_addr_resolver::vulkan_native_instance_proc_addr_resolver(
    vulkan_native_function_pointer get_instance_proc_address)
    : get_instance_proc_address_(get_instance_proc_address)
{
}

vulkan_native_function_pointer
vulkan_native_instance_proc_addr_resolver::get_instance_proc_address() const
{
    return get_instance_proc_address_;
}

vulkan_native_function_pointer
vulkan_native_instance_proc_addr_resolver::resolve_instance_symbol(
    vulkan_instance_handle instance,
    std::string_view symbol_name)
{
    if (!get_instance_proc_address_.valid() || !instance.valid() || symbol_name.empty()) {
        return {};
    }

#if QUIZ_VULKAN_HAS_VULKAN_HEADERS
    const auto get_instance_proc_address =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(get_instance_proc_address_.value);
    if (get_instance_proc_address == nullptr) {
        return {};
    }

    const std::string symbol_name_value{symbol_name};
    const PFN_vkVoidFunction resolved = get_instance_proc_address(
        reinterpret_cast<VkInstance>(instance.value),
        symbol_name_value.c_str());
    return vulkan_native_function_pointer{
        .value = reinterpret_cast<std::uintptr_t>(resolved),
    };
#else
    (void)instance;
    (void)symbol_name;
    return {};
#endif
}

system_vulkan_native_symbol_resolver::system_vulkan_native_symbol_resolver()
    : system_vulkan_native_symbol_resolver(system_vulkan_native_symbol_resolver_options{})
{
}

system_vulkan_native_symbol_resolver::system_vulkan_native_symbol_resolver(
    system_vulkan_native_symbol_resolver_options options)
    : options_(std::move(options))
{
}

vulkan_native_function_pointer system_vulkan_native_symbol_resolver::resolve_symbol(
    std::string_view symbol_name)
{
    return resolve_symbol_with_diagnostics(symbol_name).pointer;
}

vulkan_system_symbol_resolution_result
system_vulkan_native_symbol_resolver::resolve_symbol_with_diagnostics(
    std::string_view symbol_name)
{
    vulkan_system_symbol_resolution_result result =
        make_system_symbol_resolution_result(symbol_name);

    for (const std::string& candidate_library : native_symbol_candidate_libraries(options_)) {
        result.attempted_library_names.push_back(candidate_library);
        native_loader_library library(candidate_library);
        if (!library.valid()) {
            continue;
        }

        result.loader_library_available = true;
        if (result.loaded_library_name.empty()) {
            result.loaded_library_name = candidate_library;
        }

        const vulkan_native_function_pointer instance_proc_address =
            library.resolve_symbol(std::string{k_vulkan_loader_required_symbol_name});
        result.instance_proc_address_available = instance_proc_address.valid();
        if (instance_proc_address.valid()) {
            const vulkan_native_function_pointer pointer =
                resolve_loader_global_symbol(instance_proc_address, result.symbol_name);
            if (pointer.valid()) {
                result.pointer = pointer;
                result.resolved_via_instance_proc_address = true;
                result.diagnostic =
                    "Resolved native Vulkan symbol through vkGetInstanceProcAddr: "
                    + result.symbol_name;
                return result;
            }
        }

        const vulkan_native_function_pointer direct_pointer =
            library.resolve_symbol(result.symbol_name);
        if (direct_pointer.valid()) {
            result.pointer = direct_pointer;
            result.resolved_via_direct_export = true;
            result.diagnostic =
                "Resolved native Vulkan symbol through loader library export: "
                + result.symbol_name;
            return result;
        }
    }

    if (!result.loader_library_available) {
        result.diagnostic =
            "No Vulkan loader library was available for native symbol resolution";
    } else if (!result.instance_proc_address_available) {
        result.diagnostic =
            "Vulkan loader library did not export vkGetInstanceProcAddr";
    } else {
        result.diagnostic = "Native Vulkan symbol was unavailable: " + result.symbol_name;
    }
    return result;
}

vulkan_native_instance_function_table collect_vulkan_native_instance_function_table(
    vulkan_native_symbol_resolver_interface& resolver)
{
    vulkan_native_instance_function_table table = make_native_instance_function_table();
    table.get_instance_proc_address =
        resolver.resolve_symbol(std::string{k_vulkan_loader_required_symbol_name});
    if (!table.get_instance_proc_address.valid()) {
        table.status =
            vulkan_native_instance_function_table_status::missing_get_instance_proc_address_symbol;
        table.diagnostic =
            "Native Vulkan instance table is missing vkGetInstanceProcAddr";
        return table;
    }

    table.create_instance = resolver.resolve_symbol("vkCreateInstance");
    if (!table.create_instance.valid()) {
        table.status =
            vulkan_native_instance_function_table_status::missing_create_instance_symbol;
        table.diagnostic = "Native Vulkan instance table is missing vkCreateInstance";
        return table;
    }

    table.destroy_instance = resolver.resolve_symbol("vkDestroyInstance");
    if (!table.destroy_instance.valid()) {
        table.status =
            vulkan_native_instance_function_table_status::missing_destroy_instance_symbol;
        table.diagnostic = "Native Vulkan instance table is missing vkDestroyInstance";
        return table;
    }

    table.status = vulkan_native_instance_function_table_status::ready;
    table.diagnostic = "Native Vulkan instance function table is ready";
    return table;
}

vulkan_native_instance_dispatch_table collect_vulkan_native_instance_dispatch_table(
    vulkan_native_instance_symbol_resolver_interface& resolver,
    const vulkan_native_instance_create_result& create_result)
{
    vulkan_native_instance_dispatch_table table =
        make_native_instance_dispatch_table(create_result, resolver);

    if (!create_result.created()) {
        table.status = vulkan_native_instance_dispatch_table_status::instance_unavailable;
        table.diagnostic =
            "Native Vulkan instance dispatch table requires a created instance";
        return table;
    }
    if (!table.get_instance_proc_address.valid()) {
        table.status =
            vulkan_native_instance_dispatch_table_status::get_instance_proc_address_unavailable;
        table.diagnostic =
            "Native Vulkan instance dispatch table is missing vkGetInstanceProcAddr";
        return table;
    }

    table.destroy_instance =
        resolver.resolve_instance_symbol(create_result.handle, "vkDestroyInstance");
    if (!table.destroy_instance.valid()) {
        table.status =
            vulkan_native_instance_dispatch_table_status::missing_destroy_instance_symbol;
        table.missing_symbol_name = "vkDestroyInstance";
        table.diagnostic =
            "Native Vulkan instance dispatch table is missing vkDestroyInstance";
        return table;
    }

    table.status = vulkan_native_instance_dispatch_table_status::ready;
    table.diagnostic = "Native Vulkan instance dispatch table is ready";
    return table;
}

vulkan_native_physical_device_dispatch_table
collect_vulkan_native_physical_device_dispatch_table(
    vulkan_native_instance_symbol_resolver_interface& resolver,
    const vulkan_native_instance_create_result& create_result)
{
    vulkan_native_physical_device_dispatch_table table =
        make_native_physical_device_dispatch_table(create_result, resolver);

    if (!create_result.created()) {
        table.status =
            vulkan_native_physical_device_dispatch_table_status::instance_unavailable;
        table.diagnostic =
            "Native Vulkan physical device dispatch table requires a created instance";
        return table;
    }
    if (!table.get_instance_proc_address.valid()) {
        table.status =
            vulkan_native_physical_device_dispatch_table_status::get_instance_proc_address_unavailable;
        table.diagnostic =
            "Native Vulkan physical device dispatch table is missing vkGetInstanceProcAddr";
        return table;
    }

    table.enumerate_physical_devices =
        resolver.resolve_instance_symbol(create_result.handle, "vkEnumeratePhysicalDevices");
    if (!table.enumerate_physical_devices.valid()) {
        table.status =
            vulkan_native_physical_device_dispatch_table_status::missing_enumerate_physical_devices_symbol;
        table.missing_symbol_name = "vkEnumeratePhysicalDevices";
        table.diagnostic =
            "Native Vulkan physical device dispatch table is missing vkEnumeratePhysicalDevices";
        return table;
    }

    table.status = vulkan_native_physical_device_dispatch_table_status::ready;
    table.diagnostic = "Native Vulkan physical device dispatch table is ready";
    return table;
}

vulkan_native_physical_device_enumeration_result
vulkan_native_physical_device_enumerator::enumerate_physical_devices(
    const vulkan_native_physical_device_dispatch_table& dispatch_table)
{
    vulkan_native_physical_device_enumeration_result result =
        make_native_physical_device_enumeration_result(dispatch_table);

    if (!dispatch_table.ready_for_enumeration()) {
        result.status =
            vulkan_native_physical_device_enumeration_status::dispatch_table_unavailable;
        result.diagnostic = dispatch_table.diagnostic.empty()
            ? "Native Vulkan physical device dispatch table is unavailable"
            : dispatch_table.diagnostic;
        return result;
    }

#if QUIZ_VULKAN_HAS_VULKAN_HEADERS
    const auto enumerate_physical_devices =
        reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
            dispatch_table.enumerate_physical_devices.value);
    if (enumerate_physical_devices == nullptr) {
        result.status =
            vulkan_native_physical_device_enumeration_status::dispatch_table_unavailable;
        result.diagnostic =
            "Native Vulkan physical device enumeration pointer is invalid";
        return result;
    }

    std::uint32_t device_count = 0;
    VkResult native_result = enumerate_physical_devices(
        reinterpret_cast<VkInstance>(dispatch_table.instance.value),
        &device_count,
        nullptr);
    result.native_result = static_cast<std::int32_t>(native_result);
    if (native_result != VK_SUCCESS) {
        result.status = vulkan_native_physical_device_enumeration_status::enumeration_failed;
        result.diagnostic = "Native Vulkan physical device count query failed";
        return result;
    }
    if (device_count == 0) {
        result.status = vulkan_native_physical_device_enumeration_status::no_devices;
        result.diagnostic =
            "Native Vulkan physical device enumeration returned no devices";
        return result;
    }

    std::vector<VkPhysicalDevice> native_devices(device_count, VK_NULL_HANDLE);
    native_result = enumerate_physical_devices(
        reinterpret_cast<VkInstance>(dispatch_table.instance.value),
        &device_count,
        native_devices.data());
    result.native_result = static_cast<std::int32_t>(native_result);
    if (native_result != VK_SUCCESS) {
        result.status = vulkan_native_physical_device_enumeration_status::enumeration_failed;
        result.diagnostic = "Native Vulkan physical device list query failed";
        return result;
    }

    result.physical_devices.reserve(device_count);
    for (const VkPhysicalDevice native_device : native_devices) {
        if (native_device == VK_NULL_HANDLE) {
            continue;
        }
        result.physical_devices.push_back(vulkan_physical_device_handle{
            .value = reinterpret_cast<std::uintptr_t>(native_device),
        });
    }
    result.physical_device_count = result.physical_devices.size();
    if (result.physical_devices.empty()) {
        result.status = vulkan_native_physical_device_enumeration_status::no_devices;
        result.diagnostic =
            "Native Vulkan physical device enumeration returned no valid handles";
        return result;
    }

    result.status = vulkan_native_physical_device_enumeration_status::ready;
    result.diagnostic = "Native Vulkan physical devices enumerated";
    return result;
#else
    result.status = vulkan_native_physical_device_enumeration_status::headers_unavailable;
    result.diagnostic =
        "Vulkan headers are unavailable for native physical device enumeration";
    return result;
#endif
}

vulkan_native_physical_device_enumeration_result
enumerate_native_vulkan_physical_devices(
    vulkan_native_physical_device_enumerator_interface& enumerator,
    const vulkan_native_physical_device_dispatch_table& dispatch_table)
{
    return enumerator.enumerate_physical_devices(dispatch_table);
}

vulkan_native_physical_device_selection_result
select_native_vulkan_physical_device(
    vulkan_native_physical_device_selector_interface& selector,
    const vulkan_native_physical_device_enumeration_result& enumeration,
    const vulkan_device_create_request& request)
{
    return selector.select_physical_device(enumeration, request);
}

vulkan_native_instance_create_result create_native_vulkan_instance(
    const vulkan_loader_readiness_state& loader_readiness,
    const vulkan_native_instance_function_table& function_table,
    const vulkan_instance_create_request& request)
{
    vulkan_native_instance_create_result result = make_native_instance_create_result(
        loader_readiness,
        function_table,
        request);

    if (!loader_readiness.ready_for_instance()) {
        result.status = vulkan_native_instance_create_status::loader_unavailable;
        result.diagnostic = "Vulkan loader is not ready for native instance creation";
        return result;
    }
    if (!function_table.ready()) {
        result.status = vulkan_native_instance_create_status::function_table_unavailable;
        result.diagnostic = function_table.diagnostic.empty()
            ? "Native Vulkan instance function table is unavailable"
            : function_table.diagnostic;
        return result;
    }

#if QUIZ_VULKAN_HAS_VULKAN_HEADERS
    const auto create_instance =
        reinterpret_cast<PFN_vkCreateInstance>(function_table.create_instance.value);
    if (create_instance == nullptr) {
        result.status = vulkan_native_instance_create_status::function_table_unavailable;
        result.diagnostic = "Native Vulkan instance create pointer is invalid";
        return result;
    }

    const std::vector<std::string> requested_layers =
        instance_detail::requested_instance_layers(request);
    std::vector<const char*> extension_names;
    extension_names.reserve(request.required_instance_extensions.size());
    for (const std::string& extension_name : request.required_instance_extensions) {
        extension_names.push_back(extension_name.c_str());
    }
    std::vector<const char*> layer_names;
    layer_names.reserve(requested_layers.size());
    for (const std::string& layer_name : requested_layers) {
        layer_names.push_back(layer_name.c_str());
    }

    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName =
        request.app_name.empty() ? nullptr : request.app_name.c_str();
    application_info.applicationVersion = 0;
    application_info.pEngineName =
        request.engine_name.empty() ? nullptr : request.engine_name.c_str();
    application_info.engineVersion = 0;
    application_info.apiVersion =
        request.api_version != 0 ? request.api_version : VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;
    create_info.enabledExtensionCount =
        static_cast<std::uint32_t>(extension_names.size());
    create_info.ppEnabledExtensionNames =
        extension_names.empty() ? nullptr : extension_names.data();
    create_info.enabledLayerCount =
        static_cast<std::uint32_t>(layer_names.size());
    create_info.ppEnabledLayerNames = layer_names.empty() ? nullptr : layer_names.data();

    VkInstance native_instance = VK_NULL_HANDLE;
    const VkResult native_result = create_instance(&create_info, nullptr, &native_instance);
    result.native_result = static_cast<std::int32_t>(native_result);
    if (native_result != VK_SUCCESS || native_instance == VK_NULL_HANDLE) {
        result.status = vulkan_native_instance_create_status::creation_failed;
        result.diagnostic = "Native Vulkan instance creation failed";
        return result;
    }

    result.status = vulkan_native_instance_create_status::created;
    result.handle = vulkan_instance_handle{
        .value = reinterpret_cast<std::uintptr_t>(native_instance),
    };
    result.diagnostic = "Native Vulkan instance created";
    return result;
#else
    result.status = vulkan_native_instance_create_status::headers_unavailable;
    result.diagnostic = "Vulkan headers are unavailable for native instance creation";
    return result;
#endif
}

vulkan_native_instance_destroy_result destroy_native_vulkan_instance(
    const vulkan_native_instance_dispatch_table& dispatch_table)
{
    vulkan_native_instance_destroy_result result =
        make_native_instance_destroy_result({}, dispatch_table.handle);
    result.dispatch_table = dispatch_table;
    result.used_instance_dispatch = true;

    if (!dispatch_table.handle.valid()) {
        result.status = vulkan_native_instance_destroy_status::invalid_handle;
        result.diagnostic =
            "Native Vulkan instance dispatch destroy received an invalid handle";
        return result;
    }
    if (!dispatch_table.ready_for_destroy()) {
        result.status = vulkan_native_instance_destroy_status::dispatch_table_unavailable;
        result.diagnostic = dispatch_table.diagnostic.empty()
            ? "Native Vulkan instance dispatch table is unavailable for destroy"
            : dispatch_table.diagnostic;
        return result;
    }

#if QUIZ_VULKAN_HAS_VULKAN_HEADERS
    const auto destroy_instance =
        reinterpret_cast<PFN_vkDestroyInstance>(dispatch_table.destroy_instance.value);
    if (destroy_instance == nullptr) {
        result.status = vulkan_native_instance_destroy_status::dispatch_table_unavailable;
        result.diagnostic =
            "Native Vulkan instance dispatch destroy pointer is invalid";
        return result;
    }

    destroy_instance(reinterpret_cast<VkInstance>(dispatch_table.handle.value), nullptr);
    result.status = vulkan_native_instance_destroy_status::destroyed;
    result.diagnostic = "Native Vulkan instance destroyed through instance dispatch";
    return result;
#else
    result.status = vulkan_native_instance_destroy_status::headers_unavailable;
    result.diagnostic =
        "Vulkan headers are unavailable for native instance dispatch destroy";
    return result;
#endif
}

vulkan_native_instance_destroy_result destroy_native_vulkan_instance(
    const vulkan_native_instance_function_table& function_table,
    vulkan_instance_handle handle)
{
    vulkan_native_instance_destroy_result result =
        make_native_instance_destroy_result(function_table, handle);

    if (!function_table.ready()) {
        result.status = vulkan_native_instance_destroy_status::function_table_unavailable;
        result.diagnostic = function_table.diagnostic.empty()
            ? "Native Vulkan instance function table is unavailable for destroy"
            : function_table.diagnostic;
        return result;
    }
    if (!handle.valid()) {
        result.status = vulkan_native_instance_destroy_status::invalid_handle;
        result.diagnostic = "Native Vulkan instance destroy received an invalid handle";
        return result;
    }

#if QUIZ_VULKAN_HAS_VULKAN_HEADERS
    const auto destroy_instance =
        reinterpret_cast<PFN_vkDestroyInstance>(function_table.destroy_instance.value);
    if (destroy_instance == nullptr) {
        result.status = vulkan_native_instance_destroy_status::function_table_unavailable;
        result.diagnostic = "Native Vulkan instance destroy pointer is invalid";
        return result;
    }

    destroy_instance(reinterpret_cast<VkInstance>(handle.value), nullptr);
    result.status = vulkan_native_instance_destroy_status::destroyed;
    result.diagnostic = "Native Vulkan instance destroyed";
    return result;
#else
    result.status = vulkan_native_instance_destroy_status::headers_unavailable;
    result.diagnostic = "Vulkan headers are unavailable for native instance destroy";
    return result;
#endif
}

std::vector<vulkan_native_entrypoint_symbol_request>
default_vulkan_native_swapchain_entrypoints()
{
    return {
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::swapchain_create,
            .name = "vkCreateSwapchainKHR",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::swapchain_destroy,
            .name = "vkDestroySwapchainKHR",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::swapchain_images,
            .name = "vkGetSwapchainImagesKHR",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::swapchain_acquire,
            .name = "vkAcquireNextImageKHR",
            .required = true,
        },
    };
}

std::vector<std::string> default_vulkan_native_swapchain_extensions()
{
    return {"VK_KHR_swapchain"};
}

std::vector<vulkan_native_entrypoint_symbol_request>
default_vulkan_native_backend_entrypoints()
{
    std::vector<vulkan_native_entrypoint_symbol_request> entrypoints{
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::command_buffer_recording,
            .name = "vkBeginCommandBuffer",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::command_buffer_recording,
            .name = "vkCmdBeginRenderPass",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::command_buffer_recording,
            .name = "vkCmdBindPipeline",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::command_buffer_recording,
            .name = "vkCmdBindDescriptorSets",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::command_buffer_recording,
            .name = "vkCmdSetViewport",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::command_buffer_recording,
            .name = "vkCmdSetScissor",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::command_buffer_recording,
            .name = "vkCmdDraw",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::command_buffer_recording,
            .name = "vkCmdEndRenderPass",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::command_buffer_recording,
            .name = "vkEndCommandBuffer",
            .required = true,
        },
        vulkan_native_entrypoint_symbol_request{
            .stage = vulkan_native_entrypoint_stage::queue_submit,
            .name = "vkQueueSubmit",
            .required = true,
        },
    };
    const std::vector<vulkan_native_entrypoint_symbol_request> swapchain_entrypoints =
        default_vulkan_native_swapchain_entrypoints();
    entrypoints.insert(
        entrypoints.end(),
        swapchain_entrypoints.begin(),
        swapchain_entrypoints.end());
    entrypoints.push_back(vulkan_native_entrypoint_symbol_request{
        .stage = vulkan_native_entrypoint_stage::queue_present,
        .name = "vkQueuePresentKHR",
        .required = true,
    });
    return entrypoints;
}

vulkan_native_function_table_diagnostics collect_vulkan_native_function_table(
    vulkan_native_symbol_resolver_interface& resolver,
    const vulkan_loader_readiness_state& loader,
    const vulkan_native_function_table_request& request)
{
    vulkan_native_function_table_diagnostics diagnostics;
    diagnostics.checked = true;
    diagnostics.status = vulkan_native_function_table_status::not_checked;
    diagnostics.fallback_reason = vulkan_backend_fallback_reason::not_requested;
    diagnostics.loader = loader;
    diagnostics.loader_ready = loader.ready_for_instance();

    if (!diagnostics.loader_ready) {
        diagnostics.status = vulkan_native_function_table_status::loader_unavailable;
        diagnostics.fallback_reason = vulkan_backend_fallback_reason::instance_unavailable;
        diagnostics.diagnostic =
            "Vulkan loader is unavailable for native function table collection";
        return diagnostics;
    }

    const std::vector<std::string> required_extensions =
        make_native_required_extensions(request);
    diagnostics.required_extension_count = required_extensions.size();
    diagnostics.required_extensions_ready = required_extensions.empty();
    for (const std::string& required_extension : required_extensions) {
        if (!contains_symbol_name(request.available_extensions, required_extension)) {
            diagnostics.status =
                vulkan_native_function_table_status::required_extension_unavailable;
            diagnostics.fallback_reason = vulkan_backend_fallback_reason::swapchain_unavailable;
            diagnostics.missing_required_extension = required_extension;
            diagnostics.diagnostic =
                "Native Vulkan function table is missing required extension: "
                + required_extension;
            return diagnostics;
        }
        ++diagnostics.available_required_extension_count;
    }
    diagnostics.required_extensions_ready =
        diagnostics.available_required_extension_count == diagnostics.required_extension_count;

    for (const vulkan_native_entrypoint_symbol_request& symbol_request :
         make_native_symbol_requests(request)) {
        record_native_symbol_diagnostics(
            diagnostics,
            symbol_request,
            resolver.resolve_symbol(symbol_request.name));
    }

    diagnostics.command_buffer_recording_ready = required_native_stage_ready(
        diagnostics.symbols,
        vulkan_native_entrypoint_stage::command_buffer_recording);
    diagnostics.queue_submit_ready = required_native_stage_ready(
        diagnostics.symbols,
        vulkan_native_entrypoint_stage::queue_submit);
    diagnostics.swapchain_create_ready = required_native_stage_ready(
        diagnostics.symbols,
        vulkan_native_entrypoint_stage::swapchain_create);
    diagnostics.swapchain_destroy_ready = required_native_stage_ready(
        diagnostics.symbols,
        vulkan_native_entrypoint_stage::swapchain_destroy);
    diagnostics.swapchain_images_ready = required_native_stage_ready(
        diagnostics.symbols,
        vulkan_native_entrypoint_stage::swapchain_images);
    diagnostics.swapchain_acquire_ready = required_native_stage_ready(
        diagnostics.symbols,
        vulkan_native_entrypoint_stage::swapchain_acquire);
    diagnostics.queue_present_ready = required_native_stage_ready(
        diagnostics.symbols,
        vulkan_native_entrypoint_stage::queue_present);

    for (const vulkan_native_entrypoint_symbol_diagnostics& symbol :
         diagnostics.symbols) {
        if (symbol.completed()) {
            continue;
        }

        diagnostics.status = missing_status_for_native_stage(symbol.stage);
        diagnostics.fallback_reason = fallback_for_native_stage(symbol.stage);
        diagnostics.missing_symbol_stage = symbol.stage;
        diagnostics.missing_symbol_name = symbol.name;
        diagnostics.diagnostic =
            "Missing native Vulkan entrypoint for "
            + std::string{native_entrypoint_stage_name(symbol.stage)} + ": "
            + symbol.name;
        return diagnostics;
    }

    diagnostics.status = vulkan_native_function_table_status::ready;
    diagnostics.fallback_reason = vulkan_backend_fallback_reason::none;
    diagnostics.diagnostic = "Native Vulkan backend function table is ready";
    return diagnostics;
}

vulkan_native_swapchain_entrypoint_readiness summarize_vulkan_native_swapchain_entrypoints(
    const vulkan_native_function_table_diagnostics& diagnostics)
{
    if (!diagnostics.checked) {
        return vulkan_native_swapchain_entrypoint_readiness{};
    }

    return vulkan_native_swapchain_entrypoint_readiness{
        .checked = true,
        .function_table_checked = diagnostics.checked,
        .required_extensions_ready = diagnostics.required_extensions_ready,
        .create_swapchain_ready = diagnostics.swapchain_create_ready,
        .destroy_swapchain_ready = diagnostics.swapchain_destroy_ready,
        .get_swapchain_images_ready = diagnostics.swapchain_images_ready,
        .acquire_next_image_ready = diagnostics.swapchain_acquire_ready,
        .queue_present_ready = diagnostics.queue_present_ready,
        .function_table_status = diagnostics.status,
        .fallback_reason = diagnostics.fallback_reason,
        .required_extension_count = diagnostics.required_extension_count,
        .available_required_extension_count =
            diagnostics.available_required_extension_count,
        .missing_required_extension = diagnostics.missing_required_extension,
        .missing_symbol_name = diagnostics.missing_symbol_name,
        .diagnostic = diagnostics.diagnostic,
    };
}

vulkan_backend_lifecycle_readiness apply_vulkan_loader_readiness_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_loader_readiness_state loader)
{
    lifecycle.loader = std::move(loader);
    if (lifecycle.loader.checked) {
        lifecycle.instance_ready = lifecycle.loader.ready_for_instance();
    }

    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_instance_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_instance_create_result instance)
{
    if (!instance.checked) {
        return lifecycle;
    }

    lifecycle.loader = instance.loader;
    lifecycle.instance = std::move(instance);
    lifecycle.instance_ready = lifecycle.instance.ready_for_device();
    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_device_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_device_create_result device)
{
    if (!device.checked) {
        return lifecycle;
    }

    lifecycle = apply_vulkan_instance_create_result_to_lifecycle(
        std::move(lifecycle),
        device.instance);
    lifecycle.device = std::move(device);
    lifecycle.device_ready = lifecycle.device.ready_for_backend();
    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_swapchain_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_swapchain_create_result swapchain)
{
    if (!swapchain.checked) {
        return lifecycle;
    }

    lifecycle = apply_vulkan_device_create_result_to_lifecycle(
        std::move(lifecycle),
        swapchain.device);
    lifecycle.swapchain = std::move(swapchain);
    lifecycle.swapchain_ready = lifecycle.swapchain.ready_for_frame();
    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_render_pass_create_result_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_render_pass_create_result render_pass)
{
    if (!render_pass.checked) {
        return lifecycle;
    }

    lifecycle = apply_vulkan_swapchain_create_result_to_lifecycle(
        std::move(lifecycle),
        render_pass.swapchain);
    lifecycle.render_pass = std::move(render_pass);
    lifecycle.render_pass_ready = lifecycle.render_pass.ready_for_pipeline();
    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_command_recording_readiness_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_command_recording_readiness_result command_recording)
{
    if (!command_recording.checked) {
        return lifecycle;
    }

    lifecycle = apply_vulkan_render_pass_create_result_to_lifecycle(
        std::move(lifecycle),
        command_recording.render_pass);
    lifecycle.command_recording = std::move(command_recording);
    lifecycle.pipeline_ready = lifecycle.command_recording.pipeline_ready();
    lifecycle.command_recorder_ready = lifecycle.command_recording.ready_for_recording();
    return lifecycle;
}

vulkan_backend_lifecycle_readiness apply_vulkan_command_submit_readiness_to_lifecycle(
    vulkan_backend_lifecycle_readiness lifecycle,
    vulkan_command_submit_readiness_result command_submit)
{
    if (!command_submit.checked) {
        return lifecycle;
    }

    lifecycle = apply_vulkan_command_recording_readiness_to_lifecycle(
        std::move(lifecycle),
        command_submit.command_recording);
    lifecycle.command_submit = std::move(command_submit);
    return lifecycle;
}

vulkan_backend_queue_submit_adapter_summary summarize_vulkan_queue_submit_adapter_result(
    const vulkan_queue_submit_present_result& queue_submit)
{
    return vulkan_backend_queue_submit_adapter_summary{
        .checked = queue_submit.checked,
        .status = queue_submit.status,
        .submit_called = queue_submit.submit_called,
        .present_called = queue_submit.present_called,
        .submit_before_present = queue_submit.submit_before_present(),
        .recoverable_failure = queue_submit.recoverable_failure(),
        .fatal_failure = queue_submit.fatal_failure(),
        .diagnostic = queue_submit.diagnostic,
    };
}

vulkan_command_packet_bridge_result build_vulkan_command_packet_bridge(
    const vulkan_frame_plan& plan,
    const vulkan_backend_pipeline_state& pipeline,
    const vulkan_backend_resource_binding_state& resource_bindings,
    const vulkan_backend_resource_registry_state& resource_registry)
{
    const bool pipeline_required = !plan.batches.empty();
    const bool pipeline_checked = !pipeline_required
        || pipeline.cache_checked
        || pipeline.requested_pipeline_count > 0
        || pipeline.lifecycle.checked
        || pipeline.pipeline_readiness_summary.checked;
    const bool pipeline_ready = !pipeline_required || pipeline.completed();

    vulkan_command_packet_bridge_result bridge{
        .checked = true,
        .status = vulkan_command_packet_bridge_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .pipeline_checked = pipeline_checked,
        .pipeline_ready = pipeline_ready,
        .resource_bindings_checked = resource_bindings.checked,
        .resource_bindings_ready = resource_bindings.completed(),
        .resource_registry_checked = resource_registry.checked,
        .resource_registry_ready = resource_registry.completed(),
        .blocked_batch_kind = pipeline.missing_batch_kind,
        .blocked_command_index = pipeline.missing_command_index,
        .blocked_resource_id = {},
        .planned_batch_count = plan.batches.size(),
        .packet_count = 0,
        .rect_packet_count = 0,
        .text_packet_count = 0,
        .image_packet_count = 0,
        .debug_bounds_packet_count = 0,
        .clipped_packet_count = 0,
        .discarded_draw_call_count = plan.discarded_draw_call_count,
        .packets = {},
    };

    if (!pipeline_ready) {
        bridge.status = vulkan_command_packet_bridge_status::pipeline_unavailable;
        bridge.fallback_reason = vulkan_backend_fallback_reason::pipeline_unavailable;
        return bridge;
    }

    if (!resource_bindings.completed() || !resource_registry.completed()) {
        bridge.status = vulkan_command_packet_bridge_status::resource_binding_unavailable;
        bridge.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
        if (resource_bindings.checked && !resource_bindings.completed()) {
            bridge.blocked_batch_kind = resource_bindings.missing_batch_kind;
            bridge.blocked_command_index = resource_bindings.missing_command_index;
            bridge.blocked_resource_id = resource_bindings.missing_resource_id;
        } else if (!resource_registry.missing_resources.empty()) {
            const vulkan_resource_registry_missing_resource& missing =
                resource_registry.missing_resources.front();
            bridge.blocked_batch_kind = missing.batch_kind;
            bridge.blocked_command_index = missing.command_index;
            bridge.blocked_resource_id = missing.resource_id;
        }
        return bridge;
    }

    bridge.status = vulkan_command_packet_bridge_status::ready;
    bridge.fallback_reason = vulkan_backend_fallback_reason::none;
    bridge.packets.reserve(plan.batches.size());

    for (std::size_t batch_index = 0; batch_index < plan.batches.size(); ++batch_index) {
        const vulkan_draw_batch& batch = plan.batches[batch_index];
        const vulkan_batch_resource_binding_snapshot* bindings =
            find_binding_snapshot_for_batch(resource_bindings, batch, batch_index);
        if (bindings == nullptr || !bindings->completed()) {
            bridge.status = vulkan_command_packet_bridge_status::resource_binding_unavailable;
            bridge.fallback_reason = vulkan_backend_fallback_reason::resource_binding_unavailable;
            bridge.blocked_batch_kind = batch.kind;
            bridge.blocked_command_index = batch.command_index;
            bridge.blocked_resource_id = bindings == nullptr ? std::string{} : bindings->missing_resource_id;
            bridge.packets.clear();
            bridge.packet_count = 0;
            bridge.rect_packet_count = 0;
            bridge.text_packet_count = 0;
            bridge.image_packet_count = 0;
            bridge.debug_bounds_packet_count = 0;
            bridge.clipped_packet_count = 0;
            return bridge;
        }

        vulkan_command_packet packet = make_command_packet(batch, *bindings, bridge.packets.size());
        count_command_packet_category(bridge, packet.category);
        if (packet.clipped_bounds.x != packet.bounds.x
            || packet.clipped_bounds.y != packet.bounds.y
            || packet.clipped_bounds.width != packet.bounds.width
            || packet.clipped_bounds.height != packet.bounds.height) {
            ++bridge.clipped_packet_count;
        }
        bridge.packets.push_back(std::move(packet));
    }

    bridge.packet_count = bridge.packets.size();
    return bridge;
}

fake_vulkan_command_packet_executor::fake_vulkan_command_packet_executor() = default;

fake_vulkan_command_packet_executor::fake_vulkan_command_packet_executor(
    fake_vulkan_command_packet_executor_options options)
    : options_(options)
{
}

vulkan_command_packet_execution_result fake_vulkan_command_packet_executor::execute_packets(
    const vulkan_command_packet_bridge_result& bridge)
{
    result_ = vulkan_command_packet_execution_result{
        .checked = true,
        .status = vulkan_command_packet_execution_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .packet_bridge_checked = bridge.checked,
        .packet_bridge_ready = bridge.completed(),
        .planned_packet_count = bridge.packet_count,
        .events = {},
    };

    if (!bridge.completed()) {
        result_.status = vulkan_command_packet_execution_status::packet_bridge_unavailable;
        result_.fallback_reason = bridge.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::record_commands_failed
            : bridge.fallback_reason;
        return result_;
    }

    vulkan_command_packet_execution_snapshot begin =
        make_execution_event(vulkan_command_packet_execution_event::begin);
    result_.begin_attempted = true;
    if (options_.fail_begin) {
        begin.failed = true;
        result_.status = vulkan_command_packet_execution_status::begin_failed;
        result_.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        result_.events.push_back(begin);
        return result_;
    }
    begin.completed = true;
    result_.begin_completed = true;
    result_.events.push_back(begin);

    for (const vulkan_command_packet& packet : bridge.packets) {
        vulkan_command_packet_execution_snapshot packet_event =
            make_packet_execution_event(packet);
        ++result_.attempted_packet_count;
        if (options_.fail_packet && packet.packet_index == options_.fail_packet_index) {
            packet_event.failed = true;
            result_.status = vulkan_command_packet_execution_status::packet_failed;
            result_.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
            result_.has_failed_packet = true;
            result_.first_failed_category = packet.category;
            result_.first_failed_batch_kind = packet.batch_kind;
            result_.first_failed_packet_index = packet.packet_index;
            result_.first_failed_command_index = packet.command_index;
            result_.events.push_back(packet_event);
            return result_;
        }

        packet_event.completed = true;
        ++result_.executed_packet_count;
        count_packet_execution_category(result_, packet.category);
        result_.events.push_back(packet_event);
    }

    vulkan_command_packet_execution_snapshot end =
        make_execution_event(vulkan_command_packet_execution_event::end);
    result_.end_attempted = true;
    if (options_.fail_end) {
        end.failed = true;
        result_.status = vulkan_command_packet_execution_status::end_failed;
        result_.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        result_.events.push_back(end);
        return result_;
    }

    end.completed = true;
    result_.end_completed = true;
    result_.status = vulkan_command_packet_execution_status::completed;
    result_.fallback_reason = vulkan_backend_fallback_reason::none;
    result_.events.push_back(end);
    return result_;
}

const vulkan_command_packet_execution_result&
fake_vulkan_command_packet_executor::execution_result() const
{
    return result_;
}

vulkan_command_recorder_operation_plan build_vulkan_command_recorder_operation_plan(
    const vulkan_command_packet_bridge_result& bridge,
    const vulkan_command_packet_execution_result& execution)
{
    vulkan_command_recorder_operation_plan plan{
        .checked = true,
        .status = vulkan_command_recorder_operation_plan_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .packet_bridge_checked = bridge.checked,
        .packet_bridge_ready = bridge.completed(),
        .packet_execution_checked = execution.checked,
        .packet_execution_ready = execution.completed(),
        .planned_packet_count = bridge.packet_count,
        .discarded_draw_call_count = bridge.discarded_draw_call_count,
        .operations = {},
    };

    if (!bridge.completed()) {
        plan.status = vulkan_command_recorder_operation_plan_status::packet_bridge_unavailable;
        plan.fallback_reason = bridge.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::record_commands_failed
            : bridge.fallback_reason;
        plan.blocked_batch_kind = bridge.blocked_batch_kind;
        plan.blocked_command_index = bridge.blocked_command_index;
        plan.blocked_category = command_packet_category_for(bridge.blocked_batch_kind);
        return plan;
    }

    if (!execution.completed()) {
        plan.status = vulkan_command_recorder_operation_plan_status::packet_execution_unavailable;
        plan.fallback_reason = execution.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::record_commands_failed
            : execution.fallback_reason;
        if (execution.has_failed_packet) {
            plan.blocked_category = execution.first_failed_category;
            plan.blocked_batch_kind = execution.first_failed_batch_kind;
            plan.blocked_packet_index = execution.first_failed_packet_index;
            plan.blocked_command_index = execution.first_failed_command_index;
        }
        return plan;
    }

    plan.status = vulkan_command_recorder_operation_plan_status::ready;
    plan.fallback_reason = vulkan_backend_fallback_reason::none;
    plan.operations.reserve(bridge.packets.size());
    for (const vulkan_command_packet& packet : bridge.packets) {
        vulkan_command_recorder_operation_summary operation =
            make_command_recorder_operation_summary(packet, plan.operations.size());
        count_command_recorder_operation_category(plan, operation.category);
        if (operation.clipped_bounds.x != operation.bounds.x
            || operation.clipped_bounds.y != operation.bounds.y
            || operation.clipped_bounds.width != operation.bounds.width
            || operation.clipped_bounds.height != operation.bounds.height) {
            ++plan.clipped_operation_count;
        }
        plan.operations.push_back(std::move(operation));
    }
    plan.operation_count = plan.operations.size();
    return plan;
}

fake_vulkan_command_buffer_operation_recorder::fake_vulkan_command_buffer_operation_recorder() =
    default;

fake_vulkan_command_buffer_operation_recorder::fake_vulkan_command_buffer_operation_recorder(
    fake_vulkan_command_buffer_operation_recorder_options options)
    : options_(options)
{
}

vulkan_command_buffer_record_result
fake_vulkan_command_buffer_operation_recorder::record_operations(
    vulkan_command_buffer_id command_buffer,
    const vulkan_command_recorder_operation_plan& operation_plan)
{
    result_ = vulkan_command_buffer_record_result{
        .checked = true,
        .status = vulkan_command_buffer_record_result_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .command_buffer = command_buffer,
        .operation_plan_checked = operation_plan.checked,
        .operation_plan_ready = operation_plan.completed(),
        .native_function_table_checked = false,
        .native_command_buffer_recording_ready = true,
        .native_function_table_status = vulkan_native_function_table_status::not_checked,
        .missing_native_symbol_name = {},
        .sdk_native_path_checked = false,
        .sdk_adapter_ready = false,
        .sdk_native_path_status = vulkan_sdk_native_path_status::not_checked,
        .sdk_capability_status = vulkan_sdk_capability_status::not_checked,
        .sdk_adapter_fallback_status = vulkan_sdk_adapter_fallback_status::none,
        .sdk_diagnostic = {},
        .planned_operation_count = operation_plan.operation_count,
        .diagnostic = {},
        .events = {},
    };

    if (!operation_plan.completed()) {
        result_.status =
            vulkan_command_buffer_record_result_status::operation_plan_unavailable;
        result_.fallback_reason =
            operation_plan.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::record_commands_failed
            : operation_plan.fallback_reason;
        if (operation_plan.blocked()) {
            result_.first_failed_category = operation_plan.blocked_category;
            result_.first_failed_batch_kind = operation_plan.blocked_batch_kind;
            result_.first_failed_operation_index = operation_plan.blocked_packet_index;
            result_.first_failed_packet_index = operation_plan.blocked_packet_index;
            result_.first_failed_command_index = operation_plan.blocked_command_index;
        }
        return result_;
    }

    if (!command_buffer.valid()) {
        result_.status =
            vulkan_command_buffer_record_result_status::command_buffer_unavailable;
        result_.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        return result_;
    }

    vulkan_command_buffer_record_event begin = make_command_buffer_record_event(
        vulkan_command_buffer_record_event_kind::begin,
        command_buffer);
    result_.begin_attempted = true;
    if (options_.fail_begin) {
        begin.failed = true;
        result_.status = vulkan_command_buffer_record_result_status::begin_failed;
        result_.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        result_.events.push_back(begin);
        return result_;
    }
    begin.completed = true;
    result_.begin_completed = true;
    result_.events.push_back(begin);

    for (const vulkan_command_recorder_operation_summary& operation :
         operation_plan.operations) {
        vulkan_command_buffer_record_event event =
            make_command_buffer_record_operation_event(command_buffer, operation);
        ++result_.attempted_operation_count;
        if (options_.fail_operation
            && operation.operation_index == options_.fail_operation_index) {
            event.failed = true;
            result_.status = vulkan_command_buffer_record_result_status::operation_failed;
            result_.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
            result_.has_failed_operation = true;
            result_.first_failed_operation_kind = operation.kind;
            result_.first_failed_category = operation.category;
            result_.first_failed_batch_kind = operation.batch_kind;
            result_.first_failed_operation_index = operation.operation_index;
            result_.first_failed_packet_index = operation.packet_index;
            result_.first_failed_command_index = operation.command_index;
            result_.events.push_back(event);
            return result_;
        }

        event.completed = true;
        ++result_.recorded_operation_count;
        count_command_buffer_record_category(result_, operation.category);
        result_.events.push_back(event);
    }

    vulkan_command_buffer_record_event end = make_command_buffer_record_event(
        vulkan_command_buffer_record_event_kind::end,
        command_buffer);
    result_.end_attempted = true;
    if (options_.fail_end) {
        end.failed = true;
        result_.status = vulkan_command_buffer_record_result_status::end_failed;
        result_.fallback_reason = vulkan_backend_fallback_reason::record_commands_failed;
        result_.events.push_back(end);
        return result_;
    }

    end.completed = true;
    result_.end_completed = true;
    result_.status = vulkan_command_buffer_record_result_status::recorded;
    result_.fallback_reason = vulkan_backend_fallback_reason::none;
    result_.events.push_back(end);
    return result_;
}

vulkan_command_buffer_record_result record_vulkan_command_buffer_operations(
    vulkan_command_buffer_operation_recorder_interface& recorder,
    vulkan_command_buffer_id command_buffer,
    const vulkan_command_recorder_operation_plan& operation_plan,
    const vulkan_native_function_table_diagnostics& native_functions,
    const vulkan_sdk_capability_result& sdk_capabilities)
{
    const vulkan_native_entrypoint_readiness_snapshot native_readiness =
        summarize_vulkan_command_buffer_recording_native_readiness(
            native_functions,
            sdk_capabilities);

    if (!native_readiness.blocks_fake_execution()) {
        vulkan_command_buffer_record_result result =
            recorder.record_operations(command_buffer, operation_plan);
        result.native_function_table_checked = native_readiness.function_table_checked;
        result.native_command_buffer_recording_ready = native_readiness.entrypoint_ready;
        result.native_function_table_status = native_readiness.function_table_status;
        result.missing_native_symbol_name = native_readiness.missing_symbol_name;
        apply_sdk_native_path_readiness(result, native_readiness.sdk_native_path);
        return result;
    }

    vulkan_command_buffer_record_result result{
        .checked = true,
        .status = vulkan_command_buffer_record_result_status::native_entrypoint_unavailable,
        .fallback_reason = fallback_or(
            native_readiness.function_table_checked && !native_readiness.entrypoint_ready
                ? native_functions.fallback_reason
                : vulkan_backend_fallback_reason::not_requested,
            vulkan_backend_fallback_reason::record_commands_failed),
        .command_buffer = command_buffer,
        .operation_plan_checked = operation_plan.checked,
        .operation_plan_ready = operation_plan.completed(),
        .native_function_table_checked = true,
        .native_command_buffer_recording_ready = false,
        .native_function_table_status = native_readiness.function_table_status,
        .missing_native_symbol_name = native_readiness.missing_symbol_name,
        .sdk_native_path_checked = false,
        .sdk_adapter_ready = false,
        .sdk_native_path_status = vulkan_sdk_native_path_status::not_checked,
        .sdk_capability_status = vulkan_sdk_capability_status::not_checked,
        .sdk_adapter_fallback_status = vulkan_sdk_adapter_fallback_status::none,
        .sdk_diagnostic = {},
        .planned_operation_count = operation_plan.operation_count,
        .diagnostic = native_readiness.diagnostic.empty()
            ? "Vulkan native command buffer recording entrypoint is unavailable"
            : native_readiness.diagnostic,
        .events = {},
    };
    apply_sdk_native_path_readiness(result, native_readiness.sdk_native_path);
    return result;
}

const vulkan_command_buffer_record_result&
fake_vulkan_command_buffer_operation_recorder::record_result() const
{
    return result_;
}

vulkan_submit_batch_plan_result build_vulkan_submit_batch_plan(
    const vulkan_command_buffer_record_result& command_buffer_recording,
    const vulkan_command_submit_readiness_result& command_submit,
    const vulkan_native_function_table_diagnostics& native_functions,
    const vulkan_sdk_capability_result& sdk_capabilities)
{
    const vulkan_native_entrypoint_readiness_snapshot native_submit_readiness =
        summarize_vulkan_queue_submit_native_readiness(
            native_functions,
            sdk_capabilities);
    vulkan_submit_batch_plan_result plan{
        .checked = true,
        .status = vulkan_submit_batch_plan_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .command_buffer_recording_checked = command_buffer_recording.checked,
        .command_buffer_recording_ready = command_buffer_recording.completed(),
        .native_function_table_checked = native_submit_readiness.function_table_checked,
        .native_queue_submit_ready = native_submit_readiness.entrypoint_ready,
        .native_function_table_status = native_submit_readiness.function_table_status,
        .missing_native_symbol_name = native_submit_readiness.missing_symbol_name,
        .sdk_native_path_checked = false,
        .sdk_adapter_ready = false,
        .sdk_native_path_status = vulkan_sdk_native_path_status::not_checked,
        .sdk_capability_status = vulkan_sdk_capability_status::not_checked,
        .sdk_adapter_fallback_status = vulkan_sdk_adapter_fallback_status::none,
        .sdk_diagnostic = {},
        .command_submit_readiness_checked = command_submit.checked,
        .command_submit_readiness_ready =
            command_submit.checked ? command_submit.ready_for_submit() : true,
        .recorded_operation_count = command_buffer_recording.recorded_operation_count,
        .recorded_command_buffer = command_buffer_recording.command_buffer,
        .submit_command_buffer = submit_command_buffer_for(command_buffer_recording, command_submit),
        .submit_queue = submit_queue_for(command_submit),
        .sync_primitives = submit_sync_primitives_for(command_submit),
        .image_id = submit_image_id_for(command_submit),
        .submit_batches = {},
        .wait_intents = {},
        .signal_intents = {},
        .present_intents = {},
        .diagnostic = {},
    };
    apply_sdk_native_path_readiness(plan, native_submit_readiness.sdk_native_path);

    if (!command_buffer_recording.completed()) {
        plan.status = vulkan_submit_batch_plan_status::command_buffer_recording_unavailable;
        plan.fallback_reason =
            command_buffer_recording.fallback_reason == vulkan_backend_fallback_reason::none
            ? vulkan_backend_fallback_reason::record_commands_failed
            : command_buffer_recording.fallback_reason;
        plan.diagnostic = "Vulkan command buffer recording is unavailable for submit planning";
        return plan;
    }

    if (native_submit_readiness.blocks_fake_execution()) {
        plan.status = vulkan_submit_batch_plan_status::native_queue_submit_unavailable;
        plan.fallback_reason = fallback_or(
            native_submit_readiness.function_table_checked
                    && !native_submit_readiness.entrypoint_ready
                ? native_functions.fallback_reason
                : vulkan_backend_fallback_reason::not_requested,
            vulkan_backend_fallback_reason::submit_frame_failed);
        plan.diagnostic = native_submit_readiness.diagnostic.empty()
            ? "Vulkan native queue submit entrypoint is unavailable for submit planning"
            : native_submit_readiness.diagnostic;
        return plan;
    }

    if (command_submit.checked && command_submit.recoverable_submit_failure()) {
        plan.status = vulkan_submit_batch_plan_status::submit_failed_recoverable;
        plan.fallback_reason = vulkan_backend_fallback_reason::submit_frame_failed;
        plan.diagnostic = command_submit.diagnostic;
        return plan;
    }

    if (command_submit.checked && command_submit.fatal_submit_failure()) {
        plan.status = vulkan_submit_batch_plan_status::submit_failed_fatal;
        plan.fallback_reason = vulkan_backend_fallback_reason::submit_frame_failed;
        plan.diagnostic = command_submit.diagnostic;
        return plan;
    }

    if (command_submit.checked && !command_submit.command_recording_available) {
        plan.status = vulkan_submit_batch_plan_status::command_submit_unavailable;
        plan.fallback_reason = vulkan_backend_fallback_reason::submit_frame_failed;
        plan.diagnostic = command_submit.diagnostic.empty()
            ? "Vulkan command submit readiness is unavailable for submit planning"
            : command_submit.diagnostic;
        return plan;
    }

    plan.command_buffer_available =
        plan.recorded_command_buffer.valid() && plan.submit_command_buffer.valid()
        && (!command_submit.checked || command_submit.command_buffer_available);
    if (!plan.command_buffer_available) {
        plan.status = vulkan_submit_batch_plan_status::command_buffer_unavailable;
        plan.fallback_reason = vulkan_backend_fallback_reason::submit_frame_failed;
        plan.diagnostic = "Vulkan command buffer is unavailable for submit batch planning";
        return plan;
    }

    plan.sync_primitives_available =
        (!command_submit.checked || command_submit.sync_primitives_available)
        && plan.sync_primitives.image_available_semaphore.valid()
        && plan.sync_primitives.render_finished_semaphore.valid()
        && plan.sync_primitives.frame_fence.valid();
    if (!plan.sync_primitives_available) {
        plan.status = vulkan_submit_batch_plan_status::sync_primitives_unavailable;
        plan.fallback_reason = vulkan_backend_fallback_reason::submit_frame_failed;
        plan.diagnostic =
            "Vulkan wait/signal primitives are unavailable for submit batch planning";
        return plan;
    }

    plan.submit_queue_available =
        (!command_submit.checked || command_submit.submit_queue_available)
        && plan.submit_queue.valid();
    if (!plan.submit_queue_available) {
        plan.status = vulkan_submit_batch_plan_status::submit_queue_unavailable;
        plan.fallback_reason = vulkan_backend_fallback_reason::submit_frame_failed;
        plan.diagnostic = "Vulkan submit queue is unavailable for submit batch planning";
        return plan;
    }

    plan.present_target_available =
        !command_submit.checked
        || (command_submit.present_target_available && command_submit.ready_for_present());
    if (!plan.present_target_available) {
        plan.status = vulkan_submit_batch_plan_status::present_target_unavailable;
        plan.fallback_reason = vulkan_backend_fallback_reason::present_frame_failed;
        plan.diagnostic = "Vulkan present target is unavailable for submit batch planning";
        return plan;
    }

    add_submit_batch_intents(plan);
    plan.submit_batches.push_back(vulkan_submit_batch_record{
        .batch_index = 0,
        .recorded_command_buffer = plan.recorded_command_buffer,
        .submit_command_buffer = plan.submit_command_buffer,
        .submit_queue = plan.submit_queue,
        .recorded_operation_count = plan.recorded_operation_count,
        .wait_intent_count = plan.wait_intent_count,
        .signal_intent_count = plan.signal_intent_count,
        .present_intent_count = plan.present_intent_count,
    });
    plan.submit_batch_count = plan.submit_batches.size();
    plan.submit_ready = true;
    plan.present_ready = true;
    plan.status = vulkan_submit_batch_plan_status::ready;
    plan.fallback_reason = vulkan_backend_fallback_reason::none;
    plan.diagnostic = "Vulkan submit batch plan is ready for queue submit";
    return plan;
}

vulkan_present_completion_plan_result build_vulkan_present_completion_plan(
    const vulkan_submit_batch_plan_result& submit_batch,
    const vulkan_queue_submit_present_result& queue_present,
    const vulkan_native_function_table_diagnostics& native_functions,
    const vulkan_sdk_capability_result& sdk_capabilities)
{
    const vulkan_native_entrypoint_readiness_snapshot native_present_readiness =
        summarize_vulkan_queue_present_native_readiness(
            native_functions,
            sdk_capabilities);
    vulkan_present_completion_plan_result plan{
        .checked = true,
        .status = vulkan_present_completion_plan_status::not_checked,
        .frame_status = vulkan_frame_completion_status::not_checked,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .submit_batch_checked = submit_batch.checked,
        .submit_batch_ready = submit_batch.completed(),
        .native_function_table_checked = native_present_readiness.function_table_checked,
        .native_queue_present_ready = native_present_readiness.entrypoint_ready,
        .native_function_table_status = native_present_readiness.function_table_status,
        .missing_native_symbol_name = native_present_readiness.missing_symbol_name,
        .sdk_native_path_checked = false,
        .sdk_adapter_ready = false,
        .sdk_native_path_status = vulkan_sdk_native_path_status::not_checked,
        .sdk_capability_status = vulkan_sdk_capability_status::not_checked,
        .sdk_adapter_fallback_status = vulkan_sdk_adapter_fallback_status::none,
        .sdk_diagnostic = {},
        .queue_present_adapter_checked = queue_present.checked,
        .queue_present_adapter_ready = !queue_present.checked || queue_present.completed(),
        .submit_batch_count = submit_batch.submit_batch_count,
        .present_intent_count = submit_batch.present_intent_count,
        .request = queue_present.checked && queue_present.present_call.valid()
            ? present_request_from_queue_present(queue_present)
            : present_request_from_submit_batch(submit_batch, queue_present.checked),
        .result = queue_present.checked
            ? present_result_from_queue_present(queue_present)
            : deterministic_present_result_summary(),
        .diagnostic = {},
    };
    apply_sdk_native_path_readiness(plan, native_present_readiness.sdk_native_path);

    if (!submit_batch.completed()) {
        vulkan_backend_fallback_reason fallback = submit_batch.fallback_reason;
        if (fallback == vulkan_backend_fallback_reason::none
            || fallback == vulkan_backend_fallback_reason::not_requested) {
            fallback = vulkan_backend_fallback_reason::submit_frame_failed;
        }
        const bool present_failure =
            fallback == vulkan_backend_fallback_reason::present_frame_failed
            || fallback == vulkan_backend_fallback_reason::present_image_failed;
        block_present_completion(
            plan,
            vulkan_present_completion_plan_status::submit_batch_unavailable,
            present_failure ? vulkan_frame_completion_status::present_unavailable
                            : vulkan_frame_completion_status::submit_unavailable,
            fallback,
            submit_batch.diagnostic.empty()
                ? "Vulkan submit batch plan is unavailable for present completion planning"
                : submit_batch.diagnostic);
        return plan;
    }

    if (native_present_readiness.blocks_fake_execution()) {
        block_present_completion(
            plan,
            vulkan_present_completion_plan_status::native_queue_present_unavailable,
            vulkan_frame_completion_status::present_unavailable,
            fallback_or(
                native_present_readiness.function_table_checked
                        && !native_present_readiness.entrypoint_ready
                    ? native_functions.fallback_reason
                    : vulkan_backend_fallback_reason::not_requested,
                vulkan_backend_fallback_reason::present_frame_failed),
            native_present_readiness.diagnostic.empty()
                ? "Vulkan native queue present entrypoint is unavailable for present planning"
                : native_present_readiness.diagnostic);
        return plan;
    }

    if (queue_present.checked) {
        switch (queue_present.status) {
        case vulkan_queue_submit_present_status::not_requested:
            block_present_completion(
                plan,
                vulkan_present_completion_plan_status::present_adapter_unavailable,
                vulkan_frame_completion_status::present_unavailable,
                vulkan_backend_fallback_reason::present_frame_failed,
                "Vulkan queue present adapter was not requested for present completion");
            return plan;
        case vulkan_queue_submit_present_status::command_submit_unavailable:
        case vulkan_queue_submit_present_status::command_buffer_unavailable:
        case vulkan_queue_submit_present_status::submit_queue_unavailable:
            block_present_completion(
                plan,
                vulkan_present_completion_plan_status::submit_batch_unavailable,
                vulkan_frame_completion_status::submit_unavailable,
                vulkan_backend_fallback_reason::submit_frame_failed,
                queue_present.diagnostic.empty()
                    ? "Vulkan queue submit adapter cannot complete present after submit prerequisites"
                    : queue_present.diagnostic);
            return plan;
        case vulkan_queue_submit_present_status::present_target_unavailable:
            block_present_completion(
                plan,
                vulkan_present_completion_plan_status::present_request_unavailable,
                vulkan_frame_completion_status::present_unavailable,
                vulkan_backend_fallback_reason::present_frame_failed,
                queue_present.diagnostic.empty()
                    ? "Vulkan present target is unavailable for present completion"
                    : queue_present.diagnostic);
            return plan;
        case vulkan_queue_submit_present_status::adapter_unavailable:
            block_present_completion(
                plan,
                vulkan_present_completion_plan_status::present_adapter_unavailable,
                vulkan_frame_completion_status::present_unavailable,
                vulkan_backend_fallback_reason::present_frame_failed,
                queue_present.diagnostic.empty()
                    ? "Vulkan queue present adapter is unavailable for present completion"
                    : queue_present.diagnostic);
            return plan;
        case vulkan_queue_submit_present_status::submit_failed_recoverable:
            plan.recoverable_failure = true;
            block_present_completion(
                plan,
                vulkan_present_completion_plan_status::submit_failed_recoverable,
                vulkan_frame_completion_status::submit_failed_recoverable,
                vulkan_backend_fallback_reason::submit_frame_failed,
                queue_present.diagnostic.empty()
                    ? "Vulkan queue submit failed recoverably before present completion"
                    : queue_present.diagnostic);
            return plan;
        case vulkan_queue_submit_present_status::submit_failed_fatal:
            plan.fatal_failure = true;
            block_present_completion(
                plan,
                vulkan_present_completion_plan_status::submit_failed_fatal,
                vulkan_frame_completion_status::submit_failed_fatal,
                vulkan_backend_fallback_reason::submit_frame_failed,
                queue_present.diagnostic.empty()
                    ? "Vulkan queue submit failed fatally before present completion"
                    : queue_present.diagnostic);
            return plan;
        case vulkan_queue_submit_present_status::present_failed_recoverable:
            plan.recoverable_failure = true;
            block_present_completion(
                plan,
                vulkan_present_completion_plan_status::present_failed_recoverable,
                vulkan_frame_completion_status::present_failed_recoverable,
                vulkan_backend_fallback_reason::present_frame_failed,
                queue_present.diagnostic.empty()
                    ? "Vulkan queue present failed recoverably"
                    : queue_present.diagnostic);
            return plan;
        case vulkan_queue_submit_present_status::present_failed_fatal:
            plan.fatal_failure = true;
            block_present_completion(
                plan,
                vulkan_present_completion_plan_status::present_failed_fatal,
                vulkan_frame_completion_status::present_failed_fatal,
                vulkan_backend_fallback_reason::present_frame_failed,
                queue_present.diagnostic.empty()
                    ? "Vulkan queue present failed fatally"
                    : queue_present.diagnostic);
            return plan;
        case vulkan_queue_submit_present_status::submitted_and_presented:
            break;
        }
    }

    plan.present_request_ready = plan.request.completed();
    if (!plan.present_request_ready) {
        block_present_completion(
            plan,
            vulkan_present_completion_plan_status::present_request_unavailable,
            vulkan_frame_completion_status::present_unavailable,
            vulkan_backend_fallback_reason::present_frame_failed,
            "Vulkan present request is incomplete for present completion planning");
        return plan;
    }

    plan.present_result_ready = plan.result.completed();
    if (!plan.present_result_ready) {
        block_present_completion(
            plan,
            vulkan_present_completion_plan_status::present_adapter_unavailable,
            vulkan_frame_completion_status::present_unavailable,
            vulkan_backend_fallback_reason::present_frame_failed,
            plan.result.diagnostic.empty()
                ? "Vulkan present adapter result is unavailable for present completion"
                : plan.result.diagnostic);
        return plan;
    }

    plan.queue_present_adapter_ready = true;
    plan.frame_completion_ready = true;
    plan.status = vulkan_present_completion_plan_status::ready;
    plan.frame_status = queue_present.checked
        ? vulkan_frame_completion_status::completed
        : vulkan_frame_completion_status::ready_for_present;
    plan.fallback_reason = vulkan_backend_fallback_reason::none;
    plan.diagnostic = queue_present.checked
        ? "Vulkan present completion confirmed by queue present adapter"
        : "Vulkan present completion plan is ready for existing frame present path";
    return plan;
}

vulkan_native_queue_present_operation_result build_vulkan_native_queue_present_operation_plan(
    const vulkan_native_queue_present_operation_request& request)
{
    const vulkan_present_request_summary& present_request =
        request.present_completion.request;
    const vulkan_swapchain_handle selected_swapchain = present_request.swapchain;
    const vulkan_swapchain_image_id selected_image_id{
        .value = request.acquire_operation.image_id.value > 0
            ? request.acquire_operation.image_id.value
            : present_request.image_id.value,
    };

    vulkan_native_queue_present_operation_result result{
        .checked = true,
        .status = vulkan_native_queue_present_operation_status::not_checked,
        .acquire_operation = request.acquire_operation,
        .submit_batch = request.submit_batch,
        .present_completion = request.present_completion,
        .native_entrypoints = request.native_entrypoints,
        .device = request.acquire_operation.device,
        .swapchain = selected_swapchain,
        .present_queue = present_request.present_queue,
        .image_id = selected_image_id,
        .image_handle = request.acquire_operation.image_handle,
        .wait_render_finished_semaphore =
            present_request.wait_render_finished_semaphore,
        .present_status = request.present_result.status,
        .acquire_operation_checked = request.acquire_operation.checked,
        .acquired_image_ready = request.acquire_operation.ready_for_command_recording(),
        .submit_batch_checked = request.submit_batch.checked,
        .submit_batch_ready = request.submit_batch.completed(),
        .present_completion_checked = request.present_completion.checked,
        .present_completion_ready = request.present_completion.completed(),
        .native_entrypoints_checked = request.native_entrypoints.checked,
        .required_extensions_ready = request.native_entrypoints.required_extensions_ready,
        .queue_present_symbol_ready = request.native_entrypoints.queue_present_ready,
        .present_queue_ready = present_request.present_queue.valid(),
        .swapchain_ready =
            selected_swapchain.valid() && request.acquire_operation.swapchain_valid,
        .submitted_frame_ready =
            request.submit_batch.completed() && request.present_completion.submit_batch_ready
            && frame_status_has_submitted_frame(request.present_completion.frame_status),
        .present_request_ready = present_request.completed(),
        .present_adapter_result_ready = request.present_completion.result.completed(),
        .present_result_checked =
            request.present_result.status != vulkan_swapchain_present_status::not_requested,
        .present_result_completed = request.present_result.completed(),
        .submit_before_present = request.present_completion.result.submit_before_present,
        .out_of_date =
            request.present_result.status == vulkan_swapchain_present_status::out_of_date,
        .suboptimal =
            request.present_result.status == vulkan_swapchain_present_status::suboptimal,
        .recoverable_failure = request.present_completion.recoverable_failure
            || request.present_result.status == vulkan_swapchain_present_status::failed,
        .fatal_failure = request.present_completion.fatal_failure
            || request.present_result.status == vulkan_swapchain_present_status::error,
        .vk_queue_present_callable = false,
        .frame_lifecycle_may_complete = false,
        .missing_required_extension =
            request.native_entrypoints.missing_required_extension,
        .missing_symbol_name = request.native_entrypoints.missing_symbol_name,
        .diagnostic = {},
        .operation = {},
    };

    if (!result.acquire_operation_checked) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::acquire_operation_unavailable,
            "Native Vulkan queue present operation has unchecked acquire operation");
        return result;
    }
    if (request.acquire_operation.out_of_date
        || request.acquire_operation.status
            == vulkan_native_swapchain_acquire_operation_status::out_of_date) {
        result.out_of_date = true;
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::out_of_date,
            "Native Vulkan queue present operation cannot present an out-of-date acquired image");
        return result;
    }
    if (!result.acquired_image_ready) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::acquired_image_unavailable,
            request.acquire_operation.diagnostic.empty()
                ? "Native Vulkan queue present operation has no acquired image"
                : request.acquire_operation.diagnostic);
        return result;
    }
    if (!result.native_entrypoints_checked) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::native_entrypoints_unavailable,
            "Native Vulkan queue present operation has unchecked native entrypoints");
        return result;
    }
    if (!result.required_extensions_ready) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::required_extension_unavailable,
            request.native_entrypoints.diagnostic.empty()
                ? "Native Vulkan queue present operation is missing VK_KHR_swapchain"
                : request.native_entrypoints.diagnostic);
        return result;
    }
    if (!result.queue_present_symbol_ready) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::missing_queue_present_symbol,
            request.native_entrypoints.diagnostic.empty()
                ? "Native Vulkan queue present operation is missing vkQueuePresentKHR"
                : request.native_entrypoints.diagnostic);
        return result;
    }
    if (!result.submit_batch_checked || !result.submit_batch_ready) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::submit_batch_unavailable,
            request.submit_batch.diagnostic.empty()
                ? "Native Vulkan queue present operation has no ready submit batch"
                : request.submit_batch.diagnostic);
        return result;
    }
    if (!result.present_completion_checked) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::present_completion_unavailable,
            "Native Vulkan queue present operation has unchecked present completion plan");
        return result;
    }
    if (!result.present_queue_ready) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::present_queue_unavailable,
            "Native Vulkan queue present operation has no valid present queue");
        return result;
    }
    if (!result.swapchain_ready) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::swapchain_unavailable,
            "Native Vulkan queue present operation has no valid swapchain handle");
        return result;
    }
    if (!result.present_request_ready) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::present_completion_unavailable,
            "Native Vulkan queue present operation has an incomplete present request");
        return result;
    }
    if (!result.submitted_frame_ready) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::submitted_frame_unavailable,
            request.present_completion.diagnostic.empty()
                ? "Native Vulkan queue present operation has no submitted frame"
                : request.present_completion.diagnostic);
        return result;
    }
    if (request.present_completion.status
        == vulkan_present_completion_plan_status::present_failed_recoverable) {
        result.recoverable_failure = true;
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::present_failed_recoverable,
            request.present_completion.diagnostic.empty()
                ? "Native Vulkan queue present operation observed recoverable present failure"
                : request.present_completion.diagnostic);
        return result;
    }
    if (request.present_completion.status
        == vulkan_present_completion_plan_status::present_failed_fatal) {
        result.fatal_failure = true;
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::present_failed_fatal,
            request.present_completion.diagnostic.empty()
                ? "Native Vulkan queue present operation observed fatal present failure"
                : request.present_completion.diagnostic);
        return result;
    }
    if (!result.present_completion_ready) {
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::present_completion_unavailable,
            request.present_completion.diagnostic.empty()
                ? "Native Vulkan queue present operation has no ready present completion plan"
                : request.present_completion.diagnostic);
        return result;
    }

    result.vk_queue_present_callable = true;
    switch (request.present_result.status) {
    case vulkan_swapchain_present_status::not_requested:
        block_native_queue_present_operation(
            result,
            vulkan_native_queue_present_operation_status::present_result_unavailable,
            "Native Vulkan queue present operation has no present result status");
        return result;
    case vulkan_swapchain_present_status::presented:
        result.status = vulkan_native_queue_present_operation_status::ready;
        result.frame_lifecycle_may_complete = true;
        result.diagnostic =
            "Native Vulkan queue present operation completed frame present";
        break;
    case vulkan_swapchain_present_status::suboptimal:
        result.status = vulkan_native_queue_present_operation_status::suboptimal;
        result.suboptimal = true;
        result.frame_lifecycle_may_complete = request.allow_suboptimal;
        result.diagnostic = request.allow_suboptimal
            ? "Native Vulkan queue present operation completed with suboptimal swapchain"
            : "Native Vulkan queue present operation blocked suboptimal present";
        break;
    case vulkan_swapchain_present_status::out_of_date:
        result.status = vulkan_native_queue_present_operation_status::out_of_date;
        result.out_of_date = true;
        result.diagnostic =
            "Native Vulkan queue present operation found an out-of-date swapchain";
        break;
    case vulkan_swapchain_present_status::failed:
        result.status =
            vulkan_native_queue_present_operation_status::present_failed_recoverable;
        result.recoverable_failure = true;
        result.diagnostic =
            "Native Vulkan queue present operation failed recoverably";
        break;
    case vulkan_swapchain_present_status::error:
        result.status = vulkan_native_queue_present_operation_status::present_failed_fatal;
        result.fatal_failure = true;
        result.diagnostic = "Native Vulkan queue present operation failed fatally";
        break;
    }

    finalize_native_queue_present_operation(result);
    return result;
}

vulkan_native_frame_operation_result build_vulkan_native_frame_operation_summary(
    const vulkan_native_frame_operation_request& request)
{
    vulkan_native_frame_operation_result result{
        .checked = true,
        .status = vulkan_native_frame_operation_status::not_checked,
        .native_functions = request.native_functions,
        .swapchain_create = request.swapchain_create,
        .swapchain_images = request.swapchain_images,
        .acquire_operation = request.acquire_operation,
        .command_buffer_recording = request.command_buffer_recording,
        .submit_batch = request.submit_batch,
        .present_operation = request.present_operation,
        .reached_stage = vulkan_native_frame_operation_stage::not_started,
        .blocker_stage = vulkan_native_frame_operation_stage::not_started,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .native_function_table_checked = request.native_functions.checked,
        .native_function_table_ready = request.native_functions.ready_for_backend_path(),
        .swapchain_create_checked = request.swapchain_create.checked,
        .swapchain_create_ready = request.swapchain_create.can_call_vk_create_swapchain(),
        .swapchain_images_checked = request.swapchain_images.checked,
        .swapchain_images_ready =
            request.swapchain_images.can_call_vk_get_swapchain_images(),
        .acquire_checked = request.acquire_operation.checked,
        .acquire_ready = request.acquire_operation.ready_for_command_recording(),
        .command_recording_checked = request.command_buffer_recording.checked,
        .command_recording_ready = request.command_buffer_recording.completed(),
        .submit_batch_checked = request.submit_batch.checked,
        .submit_batch_ready = request.submit_batch.completed(),
        .present_operation_checked = request.present_operation.checked,
        .present_operation_ready = request.present_operation.ready_for_frame_completion(),
        .frame_completion_ready =
            request.present_operation.ready_for_frame_completion()
            && request.present_operation.frame_lifecycle_may_complete,
        .recoverable_failure = request.present_operation.recoverable_failure
            || request.submit_batch.status
                == vulkan_submit_batch_plan_status::submit_failed_recoverable,
        .fatal_failure = request.present_operation.fatal_failure
            || request.submit_batch.status == vulkan_submit_batch_plan_status::submit_failed_fatal,
        .swapchain_out_of_date = request.acquire_operation.out_of_date
            || request.present_operation.out_of_date,
        .suboptimal = request.acquire_operation.suboptimal
            || request.present_operation.suboptimal,
        .cpu_fallback_available = request.cpu_fallback_available,
        .cpu_fallback_should_remain_active = request.cpu_fallback_available,
        .native_function_table_status = request.native_functions.status,
        .missing_required_extension = !request.native_functions.missing_required_extension.empty()
            ? request.native_functions.missing_required_extension
            : !request.acquire_operation.missing_required_extension.empty()
                ? request.acquire_operation.missing_required_extension
                : request.present_operation.missing_required_extension,
        .missing_symbol_name = !request.native_functions.missing_symbol_name.empty()
            ? request.native_functions.missing_symbol_name
            : !request.acquire_operation.missing_symbol_name.empty()
                ? request.acquire_operation.missing_symbol_name
                : request.present_operation.missing_symbol_name,
        .stages = {},
        .diagnostic = {},
        .operation = {},
    };
    result.stages = build_native_frame_stage_summaries(result);

    if (!result.native_function_table_ready) {
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::native_function_table_unavailable,
            vulkan_native_frame_operation_stage::not_started,
            vulkan_native_frame_operation_stage::native_function_table,
            fallback_for_native_function_table(request.native_functions),
            request.native_functions.diagnostic.empty()
                ? "Native Vulkan frame operation has no ready function table"
                : request.native_functions.diagnostic);
        return result;
    }
    if (!result.swapchain_create_ready) {
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::swapchain_create_unavailable,
            vulkan_native_frame_operation_stage::native_function_table,
            vulkan_native_frame_operation_stage::swapchain_create,
            vulkan_backend_fallback_reason::swapchain_unavailable,
            request.swapchain_create.diagnostic.empty()
                ? "Native Vulkan frame operation has no ready swapchain create operation"
                : request.swapchain_create.diagnostic);
        return result;
    }
    if (!result.swapchain_images_ready) {
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::swapchain_images_unavailable,
            vulkan_native_frame_operation_stage::swapchain_create,
            vulkan_native_frame_operation_stage::swapchain_images,
            vulkan_backend_fallback_reason::swapchain_unavailable,
            request.swapchain_images.diagnostic.empty()
                ? "Native Vulkan frame operation has no enumerated swapchain images"
                : request.swapchain_images.diagnostic);
        return result;
    }
    if (!result.acquire_ready) {
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::acquire_unavailable,
            vulkan_native_frame_operation_stage::swapchain_images,
            vulkan_native_frame_operation_stage::acquire,
            vulkan_backend_fallback_reason::acquire_image_failed,
            request.acquire_operation.diagnostic.empty()
                ? "Native Vulkan frame operation has no recordable acquired image"
                : request.acquire_operation.diagnostic);
        return result;
    }
    if (!result.command_recording_ready) {
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::command_recording_unavailable,
            vulkan_native_frame_operation_stage::acquire,
            vulkan_native_frame_operation_stage::command_recording,
            vulkan_backend_fallback_reason::record_commands_failed,
            request.command_buffer_recording.diagnostic.empty()
                ? "Native Vulkan frame operation has no recorded command buffer"
                : request.command_buffer_recording.diagnostic);
        return result;
    }
    if (request.submit_batch.status == vulkan_submit_batch_plan_status::submit_failed_recoverable) {
        result.recoverable_failure = true;
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::recoverable_failure,
            vulkan_native_frame_operation_stage::command_recording,
            vulkan_native_frame_operation_stage::submit,
            vulkan_backend_fallback_reason::submit_frame_failed,
            request.submit_batch.diagnostic.empty()
                ? "Native Vulkan frame operation observed recoverable submit failure"
                : request.submit_batch.diagnostic);
        return result;
    }
    if (request.submit_batch.status == vulkan_submit_batch_plan_status::submit_failed_fatal) {
        result.fatal_failure = true;
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::fatal_failure,
            vulkan_native_frame_operation_stage::command_recording,
            vulkan_native_frame_operation_stage::submit,
            vulkan_backend_fallback_reason::submit_frame_failed,
            request.submit_batch.diagnostic.empty()
                ? "Native Vulkan frame operation observed fatal submit failure"
                : request.submit_batch.diagnostic);
        return result;
    }
    if (!result.submit_batch_ready) {
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::submit_unavailable,
            vulkan_native_frame_operation_stage::command_recording,
            vulkan_native_frame_operation_stage::submit,
            vulkan_backend_fallback_reason::submit_frame_failed,
            request.submit_batch.diagnostic.empty()
                ? "Native Vulkan frame operation has no ready submit batch"
                : request.submit_batch.diagnostic);
        return result;
    }
    if (request.present_operation.recoverable_failure) {
        result.recoverable_failure = true;
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::recoverable_failure,
            vulkan_native_frame_operation_stage::submit,
            vulkan_native_frame_operation_stage::present,
            vulkan_backend_fallback_reason::present_frame_failed,
            request.present_operation.diagnostic.empty()
                ? "Native Vulkan frame operation observed recoverable present failure"
                : request.present_operation.diagnostic);
        return result;
    }
    if (request.present_operation.fatal_failure) {
        result.fatal_failure = true;
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::fatal_failure,
            vulkan_native_frame_operation_stage::submit,
            vulkan_native_frame_operation_stage::present,
            vulkan_backend_fallback_reason::present_frame_failed,
            request.present_operation.diagnostic.empty()
                ? "Native Vulkan frame operation observed fatal present failure"
                : request.present_operation.diagnostic);
        return result;
    }
    if (!result.present_operation_ready) {
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::present_unavailable,
            vulkan_native_frame_operation_stage::submit,
            vulkan_native_frame_operation_stage::present,
            vulkan_backend_fallback_reason::present_frame_failed,
            request.present_operation.diagnostic.empty()
                ? "Native Vulkan frame operation has no ready queue present operation"
                : request.present_operation.diagnostic);
        return result;
    }
    if (!result.frame_completion_ready) {
        block_native_frame_operation(
            result,
            vulkan_native_frame_operation_status::frame_completion_unavailable,
            vulkan_native_frame_operation_stage::present,
            vulkan_native_frame_operation_stage::frame_completion,
            vulkan_backend_fallback_reason::present_frame_failed,
            request.present_operation.diagnostic.empty()
                ? "Native Vulkan frame operation did not complete frame lifecycle"
                : request.present_operation.diagnostic);
        return result;
    }

    result.status = vulkan_native_frame_operation_status::ready;
    result.reached_stage = vulkan_native_frame_operation_stage::frame_completion;
    result.blocker_stage = vulkan_native_frame_operation_stage::not_started;
    result.fallback_reason = vulkan_backend_fallback_reason::none;
    result.diagnostic = "Native Vulkan frame operation completed all data-only stages";
    finalize_native_frame_operation(result);
    return result;
}

vulkan_native_frame_operation_diff_diagnostics build_vulkan_native_frame_operation_summary_diff(
    const vulkan_native_frame_operation_summary& before,
    const vulkan_native_frame_operation_summary& after)
{
    vulkan_native_frame_operation_diff_diagnostics diff{
        .checked = true,
        .changed = false,
        .before_checked = before.checked,
        .after_checked = after.checked,
        .before_status = before.status,
        .after_status = after.status,
        .status_changed = before.status != after.status,
        .before_reached_stage = before.reached_stage,
        .after_reached_stage = after.reached_stage,
        .reached_stage_changed = before.reached_stage != after.reached_stage,
        .before_blocker_stage = before.blocker_stage,
        .after_blocker_stage = after.blocker_stage,
        .blocker_stage_changed = before.blocker_stage != after.blocker_stage,
        .blocker_introduced =
            before.blocker_stage == vulkan_native_frame_operation_stage::not_started
            && after.blocker_stage != vulkan_native_frame_operation_stage::not_started,
        .blocker_cleared =
            before.blocker_stage != vulkan_native_frame_operation_stage::not_started
            && after.blocker_stage == vulkan_native_frame_operation_stage::not_started,
        .blocker_moved_forward =
            before.blocker_stage != vulkan_native_frame_operation_stage::not_started
            && after.blocker_stage != vulkan_native_frame_operation_stage::not_started
            && native_frame_operation_stage_order(after.blocker_stage)
                > native_frame_operation_stage_order(before.blocker_stage),
        .blocker_moved_backward =
            before.blocker_stage != vulkan_native_frame_operation_stage::not_started
            && after.blocker_stage != vulkan_native_frame_operation_stage::not_started
            && native_frame_operation_stage_order(after.blocker_stage)
                < native_frame_operation_stage_order(before.blocker_stage),
        .before_fallback_reason = before.fallback_reason,
        .after_fallback_reason = after.fallback_reason,
        .fallback_reason_changed = before.fallback_reason != after.fallback_reason,
        .before_cpu_fallback_should_remain_active =
            before.cpu_fallback_should_remain_active,
        .after_cpu_fallback_should_remain_active =
            after.cpu_fallback_should_remain_active,
        .fallback_activation_changed =
            before.cpu_fallback_should_remain_active
            != after.cpu_fallback_should_remain_active,
        .fallback_activated =
            !before.cpu_fallback_should_remain_active
            && after.cpu_fallback_should_remain_active,
        .fallback_deactivated =
            before.cpu_fallback_should_remain_active
            && !after.cpu_fallback_should_remain_active,
        .before_recoverable_failure = before.recoverable_failure,
        .after_recoverable_failure = after.recoverable_failure,
        .recoverable_failure_changed =
            before.recoverable_failure != after.recoverable_failure,
        .recoverable_failure_started =
            !before.recoverable_failure && after.recoverable_failure,
        .recoverable_failure_cleared =
            before.recoverable_failure && !after.recoverable_failure,
        .before_fatal_failure = before.fatal_failure,
        .after_fatal_failure = after.fatal_failure,
        .fatal_failure_changed = before.fatal_failure != after.fatal_failure,
        .fatal_failure_started = !before.fatal_failure && after.fatal_failure,
        .fatal_failure_cleared = before.fatal_failure && !after.fatal_failure,
        .before_swapchain_out_of_date = before.swapchain_out_of_date,
        .after_swapchain_out_of_date = after.swapchain_out_of_date,
        .swapchain_out_of_date_changed =
            before.swapchain_out_of_date != after.swapchain_out_of_date,
        .swapchain_out_of_date_started =
            !before.swapchain_out_of_date && after.swapchain_out_of_date,
        .swapchain_out_of_date_cleared =
            before.swapchain_out_of_date && !after.swapchain_out_of_date,
        .before_suboptimal = before.suboptimal,
        .after_suboptimal = after.suboptimal,
        .suboptimal_changed = before.suboptimal != after.suboptimal,
        .suboptimal_started = !before.suboptimal && after.suboptimal,
        .suboptimal_cleared = before.suboptimal && !after.suboptimal,
        .before_frame_completion_ready = before.frame_completion_ready,
        .after_frame_completion_ready = after.frame_completion_ready,
        .frame_completion_readiness_changed =
            before.frame_completion_ready != after.frame_completion_ready,
        .frame_completion_became_ready =
            !before.frame_completion_ready && after.frame_completion_ready,
        .frame_completion_became_unready =
            before.frame_completion_ready && !after.frame_completion_ready,
        .before_completed = before.completed(),
        .after_completed = after.completed(),
        .completion_readiness_changed = before.completed() != after.completed(),
        .completion_became_ready = !before.completed() && after.completed(),
        .completion_became_unready = before.completed() && !after.completed(),
        .stage_count = 0,
        .stage_checked_change_count = 0,
        .stage_readiness_change_count = 0,
        .stage_ready_gain_count = 0,
        .stage_ready_loss_count = 0,
        .stages = {},
    };

    diff.stages.reserve(k_native_frame_operation_stages.size());
    for (const vulkan_native_frame_operation_stage stage : k_native_frame_operation_stages) {
        vulkan_native_frame_operation_stage_diff_diagnostics stage_diff =
            build_native_frame_stage_diff(
                native_frame_stage_summary_or_default(before, stage),
                native_frame_stage_summary_or_default(after, stage));
        if (stage_diff.checked_changed) {
            ++diff.stage_checked_change_count;
        }
        if (stage_diff.readiness_changed) {
            ++diff.stage_readiness_change_count;
        }
        if (stage_diff.became_ready) {
            ++diff.stage_ready_gain_count;
        }
        if (stage_diff.before_ready && !stage_diff.after_ready) {
            ++diff.stage_ready_loss_count;
        }
        diff.stages.push_back(std::move(stage_diff));
    }
    diff.stage_count = diff.stages.size();
    diff.changed = diff.before_checked != diff.after_checked
        || diff.status_changed
        || diff.reached_stage_changed
        || diff.blocker_stage_changed
        || diff.fallback_reason_changed
        || diff.fallback_activation_changed
        || diff.recoverable_failure_changed
        || diff.fatal_failure_changed
        || diff.swapchain_out_of_date_changed
        || diff.suboptimal_changed
        || diff.frame_completion_readiness_changed
        || diff.completion_readiness_changed
        || diff.stage_checked_change_count > 0
        || diff.stage_readiness_change_count > 0;
    return diff;
}

vulkan_native_frame_operation_diff_diagnostics build_vulkan_native_frame_operation_result_diff(
    const vulkan_native_frame_operation_result& before,
    const vulkan_native_frame_operation_result& after)
{
    return build_vulkan_native_frame_operation_summary_diff(
        native_frame_operation_summary_from_result(before),
        native_frame_operation_summary_from_result(after));
}

vulkan_native_frame_operation_execution_plan build_vulkan_native_frame_operation_execution_plan(
    const vulkan_native_frame_operation_execution_request& request)
{
    const vulkan_native_frame_operation_summary& summary = request.summary;
    const vulkan_native_frame_operation_diff_diagnostics& diff = request.diff;
    vulkan_native_frame_operation_execution_plan plan{
        .checked = true,
        .summary_checked = summary.checked,
        .diff_checked = diff.checked,
        .diff_changed = diff.changed,
        .native_execution_ready = false,
        .cpu_fallback_required = false,
        .skip_required = false,
        .fatal_failure = summary.fatal_failure,
        .recoverable_failure = summary.recoverable_failure,
        .swapchain_out_of_date = summary.swapchain_out_of_date,
        .suboptimal = summary.suboptimal,
        .summary_status = summary.status,
        .blocker_stage = summary.blocker_stage,
        .fallback_reason = summary.fallback_reason,
        .step_count = 0,
        .execute_step_count = 0,
        .skip_step_count = 0,
        .fallback_step_count = 0,
        .not_checked_step_count = 0,
        .steps = {},
        .diagnostic = {},
    };

    bool terminal_decision_seen = false;
    plan.steps.reserve(k_native_frame_execution_steps.size());
    for (const vulkan_native_frame_execution_step step : k_native_frame_execution_steps) {
        vulkan_native_frame_operation_step_execution_decision step_decision =
            build_native_frame_step_execution_decision(
                summary,
                diff,
                step,
                terminal_decision_seen);
        switch (step_decision.decision) {
        case vulkan_native_frame_execution_decision::not_checked:
            ++plan.not_checked_step_count;
            break;
        case vulkan_native_frame_execution_decision::execute:
            ++plan.execute_step_count;
            break;
        case vulkan_native_frame_execution_decision::skip:
            ++plan.skip_step_count;
            break;
        case vulkan_native_frame_execution_decision::fallback:
            ++plan.fallback_step_count;
            if (plan.fallback_reason == vulkan_backend_fallback_reason::none
                || plan.fallback_reason == vulkan_backend_fallback_reason::not_requested) {
                plan.fallback_reason = step_decision.fallback_reason;
            }
            break;
        }
        if (step_decision.decision != vulkan_native_frame_execution_decision::execute) {
            terminal_decision_seen = true;
        }
        plan.steps.push_back(std::move(step_decision));
    }

    plan.step_count = plan.steps.size();
    plan.cpu_fallback_required =
        plan.fallback_step_count > 0
        || (summary.checked
            && summary.cpu_fallback_available
            && summary.cpu_fallback_should_remain_active);
    plan.skip_required = plan.skip_step_count > 0 || plan.not_checked_step_count > 0;
    plan.native_execution_ready =
        summary.completed()
        && plan.execute_step_count == plan.step_count
        && plan.fallback_step_count == 0
        && plan.skip_step_count == 0
        && plan.not_checked_step_count == 0;
    if (!summary.checked) {
        plan.diagnostic = "Native Vulkan frame execution skipped because summary is unchecked";
    } else if (plan.native_execution_ready) {
        plan.diagnostic =
            "Native Vulkan frame execution will execute acquire, record, submit, and present";
    } else if (plan.cpu_fallback_required) {
        plan.diagnostic = "Native Vulkan frame execution requires CPU fallback";
    } else {
        plan.diagnostic = "Native Vulkan frame execution skipped by readiness policy";
    }

    return plan;
}

vulkan_native_frame_operation_execution_plan build_vulkan_native_frame_operation_execution_plan(
    const vulkan_native_frame_operation_summary& summary,
    const vulkan_native_frame_operation_diff_diagnostics& diff)
{
    return build_vulkan_native_frame_operation_execution_plan(
        vulkan_native_frame_operation_execution_request{
            .summary = summary,
            .diff = diff,
        });
}

vulkan_native_function_table_status native_function_table_status_from_frame(
    const vulkan_backend_frame_result& frame)
{
    if (frame.command_buffer_recording.native_function_table_checked
        && !frame.command_buffer_recording.native_command_buffer_recording_ready) {
        return frame.command_buffer_recording.native_function_table_status;
    }
    if (frame.submit_batch_plan.native_function_table_checked
        && !frame.submit_batch_plan.native_queue_submit_ready) {
        return frame.submit_batch_plan.native_function_table_status;
    }
    if (frame.present_completion_plan.native_function_table_checked
        && !frame.present_completion_plan.native_queue_present_ready) {
        return frame.present_completion_plan.native_function_table_status;
    }
    if (frame.command_buffer_recording.native_function_table_checked
        || frame.submit_batch_plan.native_function_table_checked
        || frame.present_completion_plan.native_function_table_checked) {
        return vulkan_native_function_table_status::ready;
    }

    return vulkan_native_function_table_status::not_checked;
}

std::string missing_native_symbol_from_frame(const vulkan_backend_frame_result& frame)
{
    if (frame.command_buffer_recording.native_function_table_checked
        && !frame.command_buffer_recording.native_command_buffer_recording_ready) {
        return frame.command_buffer_recording.missing_native_symbol_name;
    }
    if (frame.submit_batch_plan.native_function_table_checked
        && !frame.submit_batch_plan.native_queue_submit_ready) {
        return frame.submit_batch_plan.missing_native_symbol_name;
    }
    if (frame.present_completion_plan.native_function_table_checked
        && !frame.present_completion_plan.native_queue_present_ready) {
        return frame.present_completion_plan.missing_native_symbol_name;
    }

    return {};
}

vulkan_backend_fallback_reason native_frame_pipeline_fallback_reason(
    const vulkan_backend_frame_result& frame,
    vulkan_backend_fallback_reason fallback)
{
    if (frame.fallback_reason != vulkan_backend_fallback_reason::none
        && frame.fallback_reason != vulkan_backend_fallback_reason::not_requested) {
        return frame.fallback_reason;
    }

    return fallback;
}

void block_native_frame_pipeline_summary(
    vulkan_native_frame_operation_summary& summary,
    vulkan_native_frame_operation_status status,
    vulkan_native_frame_operation_stage reached_stage,
    vulkan_native_frame_operation_stage blocker_stage,
    vulkan_backend_fallback_reason fallback_reason,
    std::string diagnostic)
{
    summary.status = status;
    summary.reached_stage = reached_stage;
    summary.blocker_stage = blocker_stage;
    summary.fallback_reason = fallback_reason;
    summary.cpu_fallback_should_remain_active = summary.cpu_fallback_available;
    summary.diagnostic = std::move(diagnostic);
}

void finalize_native_frame_pipeline_summary_stages(
    vulkan_native_frame_operation_summary& summary)
{
    vulkan_native_function_table_diagnostics native_function_table;
    native_function_table.checked = summary.native_function_table_checked;
    native_function_table.status = summary.native_function_table_status;
    native_function_table.fallback_reason = summary.fallback_reason;
    native_function_table.missing_symbol_name = summary.missing_symbol_name;
    const vulkan_backend_fallback_reason native_function_table_fallback =
        fallback_for_native_function_table(native_function_table);

    summary.stages = {
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::native_function_table,
            summary.native_function_table_checked,
            summary.native_function_table_ready,
            native_function_table_fallback,
            summary.native_function_table_ready
                ? "Native Vulkan frame pipeline function table is ready"
                : "Native Vulkan frame pipeline function table is unavailable"),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::swapchain_create,
            summary.swapchain_create_checked,
            summary.swapchain_create_ready,
            vulkan_backend_fallback_reason::swapchain_unavailable,
            summary.swapchain_create_ready
                ? "Native Vulkan frame pipeline swapchain create data is ready"
                : "Native Vulkan frame pipeline swapchain create data is unavailable"),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::swapchain_images,
            summary.swapchain_images_checked,
            summary.swapchain_images_ready,
            vulkan_backend_fallback_reason::swapchain_unavailable,
            summary.swapchain_images_ready
                ? "Native Vulkan frame pipeline swapchain image data is ready"
                : "Native Vulkan frame pipeline swapchain image data is unavailable"),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::acquire,
            summary.acquire_checked,
            summary.acquire_ready,
            vulkan_backend_fallback_reason::acquire_image_failed,
            summary.acquire_ready
                ? "Native Vulkan frame pipeline acquire step would execute"
                : "Native Vulkan frame pipeline acquire step would not execute"),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::command_recording,
            summary.command_recording_checked,
            summary.command_recording_ready,
            vulkan_backend_fallback_reason::record_commands_failed,
            summary.command_recording_ready
                ? "Native Vulkan frame pipeline record step would execute"
                : "Native Vulkan frame pipeline record step would not execute"),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::submit,
            summary.submit_batch_checked,
            summary.submit_batch_ready,
            vulkan_backend_fallback_reason::submit_frame_failed,
            summary.submit_batch_ready
                ? "Native Vulkan frame pipeline submit step would execute"
                : "Native Vulkan frame pipeline submit step would not execute"),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::present,
            summary.present_operation_checked,
            summary.present_operation_ready,
            vulkan_backend_fallback_reason::present_frame_failed,
            summary.present_operation_ready
                ? "Native Vulkan frame pipeline present step would execute"
                : "Native Vulkan frame pipeline present step would not execute"),
        make_native_frame_stage_summary(
            vulkan_native_frame_operation_stage::frame_completion,
            summary.present_operation_checked,
            summary.frame_completion_ready,
            vulkan_backend_fallback_reason::present_frame_failed,
            summary.frame_completion_ready
                ? "Native Vulkan frame pipeline completion is ready"
                : "Native Vulkan frame pipeline completion is unavailable"),
    };
}

vulkan_native_frame_operation_summary build_native_frame_pipeline_operation_summary(
    const vulkan_backend_frame_result& frame,
    bool checked,
    bool native_function_table_checked,
    bool native_command_buffer_recording_ready,
    bool native_queue_submit_ready,
    bool native_queue_present_ready,
    bool submit_batch_ready_for_queue,
    bool present_completed,
    bool frame_completion_ready)
{
    const bool native_function_table_ready =
        native_function_table_checked
        && native_function_table_status_from_frame(frame)
            == vulkan_native_function_table_status::ready
        && native_command_buffer_recording_ready
        && native_queue_submit_ready
        && native_queue_present_ready;
    const bool native_swapchain_ready =
        native_function_table_ready && frame.lifecycle.effective_swapchain_ready();
    const bool acquire_checked = frame.swapchain_image_acquire_plan.checked;
    const bool acquire_ready =
        native_swapchain_ready
        && frame.swapchain_image_acquire_plan.ready_for_command_recording();
    const bool command_recording_checked = frame.command_buffer_recording.checked;
    const bool command_recording_ready =
        acquire_ready && frame.command_buffer_recording.completed()
        && frame.commands_recorded;
    const bool submit_checked = frame.submit_batch_plan.checked;
    const bool submit_ready =
        command_recording_ready && submit_batch_ready_for_queue && frame.frame_submitted;
    const bool present_checked =
        frame.present_completion_plan.checked || frame.present_policy.present.checked;
    const bool present_ready =
        submit_ready && frame.present_completion_plan.completed() && present_completed;
    const bool completed =
        present_ready && frame_completion_ready && frame.lifecycle_policy.completed();

    vulkan_native_frame_operation_summary summary{
        .checked = checked,
        .status = vulkan_native_frame_operation_status::not_checked,
        .reached_stage = vulkan_native_frame_operation_stage::not_started,
        .blocker_stage = vulkan_native_frame_operation_stage::not_started,
        .fallback_reason = vulkan_backend_fallback_reason::not_requested,
        .native_function_table_checked = native_function_table_checked,
        .native_function_table_ready = native_function_table_ready,
        .swapchain_create_checked = checked,
        .swapchain_create_ready = native_swapchain_ready,
        .swapchain_images_checked = checked,
        .swapchain_images_ready = native_swapchain_ready,
        .acquire_checked = acquire_checked,
        .acquire_ready = acquire_ready,
        .command_recording_checked = command_recording_checked,
        .command_recording_ready = command_recording_ready,
        .submit_batch_checked = submit_checked,
        .submit_batch_ready = submit_ready,
        .present_operation_checked = present_checked,
        .present_operation_ready = present_ready,
        .frame_completion_ready = completed,
        .recoverable_failure =
            frame.fallback_reason == vulkan_backend_fallback_reason::submit_frame_failed
            || frame.fallback_reason == vulkan_backend_fallback_reason::present_image_failed
            || frame.fallback_reason == vulkan_backend_fallback_reason::present_frame_failed
            || frame.submit_batch_plan.status
                == vulkan_submit_batch_plan_status::submit_failed_recoverable
            || frame.present_completion_plan.recoverable_failure,
        .fatal_failure =
            frame.swapchain_recreate_policy.fatal
            || frame.submit_batch_plan.status
                == vulkan_submit_batch_plan_status::submit_failed_fatal
            || frame.present_completion_plan.fatal_failure,
        .swapchain_out_of_date =
            frame.swapchain_image_acquire_plan.out_of_date
            || frame.swapchain.present.status == vulkan_swapchain_present_status::out_of_date,
        .suboptimal =
            frame.swapchain_image_acquire_plan.suboptimal
            || frame.swapchain.present.status == vulkan_swapchain_present_status::suboptimal,
        .cpu_fallback_available = true,
        .cpu_fallback_should_remain_active = checked,
        .native_function_table_status = native_function_table_status_from_frame(frame),
        .missing_required_extension = {},
        .missing_symbol_name = missing_native_symbol_from_frame(frame),
        .stages = {},
        .diagnostic = {},
    };

    if (!checked) {
        summary.cpu_fallback_should_remain_active = false;
        summary.diagnostic = "Native Vulkan frame pipeline execution summary is unchecked";
    } else if (!summary.native_function_table_ready) {
        block_native_frame_pipeline_summary(
            summary,
            vulkan_native_frame_operation_status::native_function_table_unavailable,
            vulkan_native_frame_operation_stage::not_started,
            vulkan_native_frame_operation_stage::native_function_table,
            native_frame_pipeline_fallback_reason(
                frame,
                vulkan_backend_fallback_reason::not_requested),
            "Native Vulkan frame pipeline has no ready function table");
    } else if (!summary.swapchain_create_ready) {
        block_native_frame_pipeline_summary(
            summary,
            vulkan_native_frame_operation_status::swapchain_create_unavailable,
            vulkan_native_frame_operation_stage::native_function_table,
            vulkan_native_frame_operation_stage::swapchain_create,
            native_frame_pipeline_fallback_reason(
                frame,
                vulkan_backend_fallback_reason::swapchain_unavailable),
            "Native Vulkan frame pipeline has no ready swapchain create data");
    } else if (!summary.swapchain_images_ready) {
        block_native_frame_pipeline_summary(
            summary,
            vulkan_native_frame_operation_status::swapchain_images_unavailable,
            vulkan_native_frame_operation_stage::swapchain_create,
            vulkan_native_frame_operation_stage::swapchain_images,
            native_frame_pipeline_fallback_reason(
                frame,
                vulkan_backend_fallback_reason::swapchain_unavailable),
            "Native Vulkan frame pipeline has no ready swapchain image data");
    } else if (!summary.acquire_ready) {
        block_native_frame_pipeline_summary(
            summary,
            vulkan_native_frame_operation_status::acquire_unavailable,
            vulkan_native_frame_operation_stage::swapchain_images,
            vulkan_native_frame_operation_stage::acquire,
            native_frame_pipeline_fallback_reason(
                frame,
                vulkan_backend_fallback_reason::acquire_image_failed),
            "Native Vulkan frame pipeline would not acquire a native image");
    } else if (!summary.command_recording_ready) {
        block_native_frame_pipeline_summary(
            summary,
            vulkan_native_frame_operation_status::command_recording_unavailable,
            vulkan_native_frame_operation_stage::acquire,
            vulkan_native_frame_operation_stage::command_recording,
            native_frame_pipeline_fallback_reason(
                frame,
                vulkan_backend_fallback_reason::record_commands_failed),
            "Native Vulkan frame pipeline would not record native commands");
    } else if (!summary.submit_batch_ready) {
        block_native_frame_pipeline_summary(
            summary,
            summary.fatal_failure
                ? vulkan_native_frame_operation_status::fatal_failure
                : summary.recoverable_failure
                    ? vulkan_native_frame_operation_status::recoverable_failure
                    : vulkan_native_frame_operation_status::submit_unavailable,
            vulkan_native_frame_operation_stage::command_recording,
            vulkan_native_frame_operation_stage::submit,
            native_frame_pipeline_fallback_reason(
                frame,
                vulkan_backend_fallback_reason::submit_frame_failed),
            "Native Vulkan frame pipeline would not submit native work");
    } else if (!summary.present_operation_ready) {
        block_native_frame_pipeline_summary(
            summary,
            summary.fatal_failure
                ? vulkan_native_frame_operation_status::fatal_failure
                : summary.recoverable_failure
                    ? vulkan_native_frame_operation_status::recoverable_failure
                    : vulkan_native_frame_operation_status::present_unavailable,
            vulkan_native_frame_operation_stage::submit,
            vulkan_native_frame_operation_stage::present,
            native_frame_pipeline_fallback_reason(
                frame,
                vulkan_backend_fallback_reason::present_frame_failed),
            "Native Vulkan frame pipeline would not present native work");
    } else if (!summary.frame_completion_ready) {
        block_native_frame_pipeline_summary(
            summary,
            vulkan_native_frame_operation_status::frame_completion_unavailable,
            vulkan_native_frame_operation_stage::present,
            vulkan_native_frame_operation_stage::frame_completion,
            native_frame_pipeline_fallback_reason(
                frame,
                vulkan_backend_fallback_reason::present_frame_failed),
            "Native Vulkan frame pipeline would not complete native frame lifecycle");
    } else {
        summary.status = vulkan_native_frame_operation_status::ready;
        summary.reached_stage = vulkan_native_frame_operation_stage::frame_completion;
        summary.blocker_stage = vulkan_native_frame_operation_stage::not_started;
        summary.fallback_reason = vulkan_backend_fallback_reason::none;
        summary.cpu_fallback_should_remain_active = false;
        summary.diagnostic =
            "Native Vulkan frame pipeline would execute acquire, record, submit, and present";
    }

    finalize_native_frame_pipeline_summary_stages(summary);
    return summary;
}

vulkan_backend_frame_native_execution_summary build_native_frame_execution_summary(
    vulkan_native_frame_operation_summary operation)
{
    const vulkan_native_frame_operation_diff_diagnostics diff =
        build_vulkan_native_frame_operation_summary_diff({}, operation);
    vulkan_native_frame_operation_execution_plan plan =
        build_vulkan_native_frame_operation_execution_plan(operation, diff);
    vulkan_backend_frame_native_execution_summary summary{
        .checked = plan.checked,
        .operation = std::move(operation),
        .diff = diff,
        .plan = std::move(plan),
        .acquire_decision = vulkan_native_frame_execution_decision::not_checked,
        .record_decision = vulkan_native_frame_execution_decision::not_checked,
        .submit_decision = vulkan_native_frame_execution_decision::not_checked,
        .present_decision = vulkan_native_frame_execution_decision::not_checked,
        .native_acquire_would_execute = false,
        .native_record_would_execute = false,
        .native_submit_would_execute = false,
        .native_present_would_execute = false,
    };

    for (const vulkan_native_frame_operation_step_execution_decision& step :
         summary.plan.steps) {
        switch (step.step) {
        case vulkan_native_frame_execution_step::acquire:
            summary.acquire_decision = step.decision;
            summary.native_acquire_would_execute = step.should_execute();
            break;
        case vulkan_native_frame_execution_step::record:
            summary.record_decision = step.decision;
            summary.native_record_would_execute = step.should_execute();
            break;
        case vulkan_native_frame_execution_step::submit:
            summary.submit_decision = step.decision;
            summary.native_submit_would_execute = step.should_execute();
            break;
        case vulkan_native_frame_execution_step::present:
            summary.present_decision = step.decision;
            summary.native_present_would_execute = step.should_execute();
            break;
        }
    }

    return summary;
}

vulkan_backend_frame_pipeline_handoff summarize_vulkan_frame_pipeline_handoff(
    const vulkan_backend_frame_result& frame)
{
    const bool checked = frame.attempted || frame.fallback_summary.checked
        || frame.reached_stage != vulkan_backend_frame_stage::not_started;
    const bool pipeline_required = frame.planned_batch_count > 0;
    const bool pipeline_checked = frame.pipeline.cache_checked
        || frame.pipeline.requested_pipeline_count > 0
        || frame.pipeline.lifecycle.checked
        || frame.pipeline.pipeline_readiness_summary.checked;
    const bool pipeline_completed = !pipeline_required || frame.pipeline.completed();
    const bool command_submit_readiness_ready =
        frame.lifecycle.effective_command_submit_ready()
        && frame.lifecycle.effective_present_target_ready();
    const bool frame_submit_completed =
        frame.command_buffer_submit.checked && frame.command_buffer_submit.submit.completed();
    const bool present_completed =
        frame.present_policy.present.completed() || frame.frame_presented;
    const bool resource_bindings_completed = frame.resource_bindings.completed();
    const bool resource_registry_completed = frame.resource_registry.completed();
    const bool command_packets_completed = frame.command_packets.completed();
    const bool command_packet_execution_completed = frame.command_packet_execution.completed();
    const bool command_recorder_operations_completed =
        frame.command_recorder_operations.completed();
    const bool command_buffer_recording_completed =
        frame.command_buffer_recording.completed();
    const bool command_buffer_ready_for_submit = command_buffer_recording_completed;
    const bool native_function_table_checked =
        frame.command_buffer_recording.native_function_table_checked
        || frame.submit_batch_plan.native_function_table_checked
        || frame.present_completion_plan.native_function_table_checked;
    const bool native_command_buffer_recording_ready =
        !frame.command_buffer_recording.native_function_table_checked
        || frame.command_buffer_recording.native_command_buffer_recording_ready;
    const bool native_queue_submit_ready =
        !frame.submit_batch_plan.native_function_table_checked
        || frame.submit_batch_plan.native_queue_submit_ready;
    const bool native_queue_present_ready =
        !frame.present_completion_plan.native_function_table_checked
        || frame.present_completion_plan.native_queue_present_ready;
    const vulkan_sdk_native_path_readiness sdk_native_path =
        summarize_vulkan_sdk_native_path_readiness(frame.sdk_capabilities);
    const bool submit_batch_planning_completed = frame.submit_batch_plan.completed();
    const bool submit_batch_ready_for_queue =
        submit_batch_planning_completed && frame.submit_batch_plan.submit_ready;
    const bool present_completion_planning_completed =
        frame.present_completion_plan.completed();
    const bool frame_completion_ready =
        present_completion_planning_completed
        && frame.present_completion_plan.frame_completion_ready;
    const bool command_recorder_gate_allowed = frame.command_recorder.gate.completed();
    const bool command_recording_ready =
        frame.lifecycle.effective_command_recorder_ready()
        && pipeline_completed
        && resource_bindings_completed
        && resource_registry_completed
        && command_packets_completed
        && command_packet_execution_completed
        && command_recorder_operations_completed
        && command_buffer_recording_completed
        && submit_batch_planning_completed
        && command_recorder_gate_allowed
        && frame.command_recorder.completed()
        && frame.commands_recorded
        && (!frame.command_buffer_submit.checked
            || frame.command_buffer_submit.recording.completed());
    const vulkan_backend_frame_native_execution_summary native_frame_execution =
        build_native_frame_execution_summary(
            build_native_frame_pipeline_operation_summary(
                frame,
                checked,
                native_function_table_checked,
                native_command_buffer_recording_ready,
                native_queue_submit_ready,
                native_queue_present_ready,
                submit_batch_ready_for_queue,
                present_completed,
                frame_completion_ready));

    vulkan_backend_frame_pipeline_handoff handoff{
        .checked = checked,
        .status = checked
            ? frame_pipeline_status_for_fallback(frame.fallback_reason)
            : vulkan_backend_frame_pipeline_handoff_status::not_checked,
        .fallback_reason = frame.fallback_reason,
        .reached_stage = frame.reached_stage,
        .cpu_fallback_available = frame.fallback_required,
        .loader_checked = frame.lifecycle.loader.checked,
        .loader_ready = frame.lifecycle.loader.checked
            ? frame.lifecycle.loader.ready_for_instance()
            : frame.lifecycle.effective_instance_ready(),
        .instance_ready = frame.lifecycle.effective_instance_ready(),
        .device_ready = frame.lifecycle.effective_device_ready(),
        .swapchain_ready = frame.lifecycle.effective_swapchain_ready(),
        .swapchain_recreate_policy_checked = frame.swapchain_recreate_policy.checked,
        .swapchain_recreate_action = frame.swapchain_recreate_policy.action,
        .swapchain_keep_rendering = frame.swapchain_recreate_policy.should_keep_rendering,
        .swapchain_recreate_immediately =
            frame.swapchain_recreate_policy.should_recreate_immediately,
        .swapchain_recreate_after_frame =
            frame.swapchain_recreate_policy.should_recreate_after_frame,
        .swapchain_skip_submit = frame.swapchain_recreate_policy.should_skip_submit,
        .swapchain_fatal_error = frame.swapchain_recreate_policy.fatal,
        .render_pass_ready = frame.lifecycle.effective_render_pass_ready(),
        .surface_ready = frame.surface_ready,
        .frame_plan_ready = reached_frame_stage(
            frame.reached_stage,
            vulkan_backend_frame_stage::frame_plan_ready),
        .pipeline_required = pipeline_required,
        .pipeline_checked = pipeline_checked,
        .pipeline_completed = pipeline_completed,
        .pipeline_readiness_summary_checked = frame.pipeline.pipeline_readiness_summary.checked,
        .pipeline_readiness_summary_completed =
            frame.pipeline.pipeline_readiness_summary.checked
            && frame.pipeline.pipeline_readiness_summary.completed(),
        .shader_modules_ready = !frame.pipeline.shader_modules.checked
            || frame.pipeline.shader_modules.completed(),
        .pipeline_layout_ready = !frame.pipeline.pipeline_layout.checked
            || frame.pipeline.pipeline_layout.ready_for_pipeline(),
        .graphics_pipeline_ready = !frame.pipeline.graphics_pipeline.checked
            || frame.pipeline.graphics_pipeline.ready_for_draw(),
        .resource_bindings_checked = frame.resource_bindings.checked,
        .resource_bindings_completed = resource_bindings_completed,
        .resource_registry_checked = frame.resource_registry.checked,
        .resource_registry_completed = resource_registry_completed,
        .command_packets_checked = frame.command_packets.checked,
        .command_packets_completed = command_packets_completed,
        .command_packet_execution_checked = frame.command_packet_execution.checked,
        .command_packet_execution_completed = command_packet_execution_completed,
        .command_recorder_operations_checked = frame.command_recorder_operations.checked,
        .command_recorder_operations_completed = command_recorder_operations_completed,
        .command_buffer_recording_checked = frame.command_buffer_recording.checked,
        .command_buffer_recording_completed = command_buffer_recording_completed,
        .command_buffer_ready_for_submit = command_buffer_ready_for_submit,
        .native_function_table_checked = native_function_table_checked,
        .native_command_buffer_recording_ready = native_command_buffer_recording_ready,
        .native_queue_submit_ready = native_queue_submit_ready,
        .native_queue_present_ready = native_queue_present_ready,
        .native_function_table_status = native_function_table_status_from_frame(frame),
        .missing_native_symbol_name = missing_native_symbol_from_frame(frame),
        .native_frame_execution = native_frame_execution,
        .sdk_native_path_checked = sdk_native_path.checked,
        .sdk_adapter_ready = sdk_native_path.ready(),
        .sdk_native_path_status = sdk_native_path.status,
        .sdk_capability_status = sdk_native_path.sdk_capability_status,
        .sdk_adapter_fallback_status = sdk_native_path.sdk_adapter_fallback_status,
        .sdk_external_headers = sdk_native_path.external_headers,
        .sdk_external_headers_checked = sdk_native_path.external_headers_checked,
        .sdk_vulkan_headers_available = sdk_native_path.vulkan_headers_available,
        .sdk_vma_headers_available = sdk_native_path.vma_headers_available,
        .sdk_diagnostic = sdk_native_path.diagnostic,
        .submit_batch_planning_checked = frame.submit_batch_plan.checked,
        .submit_batch_planning_completed = submit_batch_planning_completed,
        .submit_batch_ready_for_queue = submit_batch_ready_for_queue,
        .present_completion_planning_checked = frame.present_completion_plan.checked,
        .present_completion_planning_completed = present_completion_planning_completed,
        .frame_completion_ready = frame_completion_ready,
        .command_recorder_lifecycle_ready = frame.lifecycle.effective_command_recorder_ready(),
        .command_recorder_gate_checked = frame.command_recorder.gate.checked,
        .command_recorder_gate_allowed = command_recorder_gate_allowed,
        .command_recording_ready = command_recording_ready,
        .command_submit_readiness_checked = frame.lifecycle.command_submit.checked,
        .command_submit_readiness_ready = command_submit_readiness_ready,
        .frame_submit_completed = frame_submit_completed,
        .present_completed = present_completed,
        .frame_lifecycle_checked = frame.lifecycle_policy.checked,
        .frame_lifecycle_completed = frame.lifecycle_policy.completed(),
        .frame_lifecycle_attempted_step_count = frame.lifecycle_policy.attempted_step_count,
        .frame_lifecycle_completed_step_count = frame.lifecycle_policy.completed_step_count,
        .planned_batch_count = frame.planned_batch_count,
        .recorded_batch_count = frame.recorded_batch_count,
        .clipped_draw_call_count = frame.clipped_draw_call_count,
        .discarded_draw_call_count = frame.discarded_draw_call_count,
    };
    count_handoff_batches_from_frame(handoff, frame);
    return handoff;
}

vulkan_backend_frame_result apply_vulkan_queue_submit_adapter_result_to_frame(
    vulkan_backend_frame_result frame,
    vulkan_queue_submit_present_result queue_submit)
{
    frame.command_submit = queue_submit.command_submit;
    frame.queue_submit_adapter = summarize_vulkan_queue_submit_adapter_result(queue_submit);
    frame.queue_submit = std::move(queue_submit);
    if (frame.submit_batch_plan.checked) {
        frame.present_completion_plan =
            build_vulkan_present_completion_plan(
                frame.submit_batch_plan,
                frame.queue_submit,
                {},
                frame.sdk_capabilities);
    }
    frame.pipeline_handoff = summarize_vulkan_frame_pipeline_handoff(frame);
    return frame;
}

vulkan_backend_frame_result apply_vulkan_sdk_capability_result_to_frame(
    vulkan_backend_frame_result frame,
    vulkan_sdk_capability_result sdk_capabilities)
{
    frame.sdk_capabilities = std::move(sdk_capabilities);
    frame.pipeline_handoff = summarize_vulkan_frame_pipeline_handoff(frame);
    return frame;
}

null_vulkan_backend_device::null_vulkan_backend_device() = default;

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_loader_readiness_state loader_readiness)
    : loader_readiness_(std::move(loader_readiness))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    const vulkan_loader_probe_result& loader_probe)
    : null_vulkan_backend_device(make_vulkan_loader_readiness_state(loader_probe))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_instance_create_result instance_result)
    : loader_readiness_(instance_result.loader)
    , instance_result_(std::move(instance_result))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_device_create_result device_result)
    : loader_readiness_(device_result.instance.loader)
    , instance_result_(device_result.instance)
    , device_result_(std::move(device_result))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_swapchain_create_result swapchain_result)
    : loader_readiness_(swapchain_result.device.instance.loader)
    , instance_result_(swapchain_result.device.instance)
    , device_result_(swapchain_result.device)
    , swapchain_result_(std::move(swapchain_result))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_render_pass_create_result render_pass_result)
    : loader_readiness_(render_pass_result.swapchain.device.instance.loader)
    , instance_result_(render_pass_result.swapchain.device.instance)
    , device_result_(render_pass_result.swapchain.device)
    , swapchain_result_(render_pass_result.swapchain)
    , render_pass_result_(std::move(render_pass_result))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_command_recording_readiness_result command_recording_result)
    : loader_readiness_(command_recording_result.render_pass.swapchain.device.instance.loader)
    , instance_result_(command_recording_result.render_pass.swapchain.device.instance)
    , device_result_(command_recording_result.render_pass.swapchain.device)
    , swapchain_result_(command_recording_result.render_pass.swapchain)
    , render_pass_result_(command_recording_result.render_pass)
    , command_recording_result_(std::move(command_recording_result))
{
}

null_vulkan_backend_device::null_vulkan_backend_device(
    vulkan_command_submit_readiness_result command_submit_result)
    : loader_readiness_(
        command_submit_result.command_recording.render_pass.swapchain.device.instance.loader)
    , instance_result_(
        command_submit_result.command_recording.render_pass.swapchain.device.instance)
    , device_result_(command_submit_result.command_recording.render_pass.swapchain.device)
    , swapchain_result_(command_submit_result.command_recording.render_pass.swapchain)
    , render_pass_result_(command_submit_result.command_recording.render_pass)
    , command_recording_result_(command_submit_result.command_recording)
    , command_submit_result_(std::move(command_submit_result))
{
}

vulkan_backend_lifecycle_readiness null_vulkan_backend_device::current_lifecycle_readiness() const
{
    vulkan_backend_lifecycle_readiness lifecycle =
        apply_vulkan_loader_readiness_to_lifecycle({}, loader_readiness_);
    lifecycle = apply_vulkan_instance_create_result_to_lifecycle(
        std::move(lifecycle),
        instance_result_);
    lifecycle = apply_vulkan_device_create_result_to_lifecycle(
        std::move(lifecycle),
        device_result_);
    lifecycle = apply_vulkan_swapchain_create_result_to_lifecycle(
        std::move(lifecycle),
        swapchain_result_);
    lifecycle = apply_vulkan_render_pass_create_result_to_lifecycle(
        std::move(lifecycle),
        render_pass_result_);
    lifecycle = apply_vulkan_command_recording_readiness_to_lifecycle(
        std::move(lifecycle),
        command_recording_result_);
    return apply_vulkan_command_submit_readiness_to_lifecycle(
        std::move(lifecycle),
        command_submit_result_);
}

vulkan_surface_extent null_vulkan_backend_device::current_surface_extent() const
{
    if (swapchain_result_.ready_for_frame()) {
        return swapchain_result_.selected_extent;
    }

    return {};
}

bool null_vulkan_backend_device::begin_frame(vulkan_surface_extent surface)
{
    static_cast<void>(surface);
    return false;
}

vulkan_swapchain_acquire_result null_vulkan_backend_device::acquire_next_image(vulkan_surface_extent surface)
{
    static_cast<void>(surface);
    return vulkan_swapchain_acquire_result{
        .status = vulkan_swapchain_acquire_status::failed,
        .image = {},
    };
}

bool null_vulkan_backend_device::record_frame_commands(const vulkan_frame_plan& plan)
{
    static_cast<void>(plan);
    return false;
}

bool null_vulkan_backend_device::submit_frame()
{
    return false;
}

vulkan_swapchain_present_result null_vulkan_backend_device::present_image(
    vulkan_swapchain_image_id image_id)
{
    return vulkan_swapchain_present_result{
        .status = vulkan_swapchain_present_status::failed,
        .image_id = image_id,
    };
}

bool null_vulkan_backend_device::present_frame()
{
    return false;
}

diagnostic_vulkan_command_recorder::diagnostic_vulkan_command_recorder(bool ready)
    : diagnostic_vulkan_command_recorder(diagnostic_vulkan_command_recorder_options{.ready = ready})
{
}

diagnostic_vulkan_command_recorder::diagnostic_vulkan_command_recorder(
    diagnostic_vulkan_command_recorder_options options)
    : options_(options)
{
    state_.ready = options_.ready;
}

bool diagnostic_vulkan_command_recorder::begin_recording(
    vulkan_surface_extent surface,
    std::size_t planned_batch_count)
{
    state_ = {};
    state_.ready = options_.ready;
    state_.planned_batch_count = planned_batch_count;
    if (!options_.ready || !surface.valid()
        || options_.fail_at == vulkan_command_recorder_failure_stage::begin_recording) {
        mark_recorder_failure(state_, vulkan_command_recorder_failure_stage::begin_recording, 0);
        return false;
    }

    state_.frame_open = true;
    return true;
}

bool diagnostic_vulkan_command_recorder::record_draw_batch(const vulkan_draw_batch& batch)
{
    const std::size_t recording_index = state_.recorded_batch_count;
    if (state_.failed()) {
        return false;
    }
    if (!state_.ready || !state_.frame_open || state_.command_buffer_recorded) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::record_draw_batch,
            recording_index);
        return false;
    }
    if (options_.fail_at == vulkan_command_recorder_failure_stage::record_draw_batch
        && recording_index == options_.fail_recording_index) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::record_draw_batch,
            recording_index);
        return false;
    }

    state_.recorded_batches.push_back(make_recorded_draw_batch(batch, recording_index));
    state_.recorded_batch_count = state_.recorded_batches.size();
    if (state_.recorded_batch_count > state_.planned_batch_count) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::record_draw_batch,
            recording_index);
        return false;
    }

    return state_.recorded_batch_count <= state_.planned_batch_count;
}

bool diagnostic_vulkan_command_recorder::finish_recording()
{
    if (state_.failed()) {
        return false;
    }
    if (!state_.ready || !state_.frame_open || state_.command_buffer_recorded) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::finish_recording,
            state_.recorded_batch_count);
        return false;
    }
    if (options_.fail_at == vulkan_command_recorder_failure_stage::finish_recording) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::finish_recording,
            state_.recorded_batch_count);
        return false;
    }
    if (state_.recorded_batch_count != state_.planned_batch_count) {
        mark_recorder_failure(
            state_,
            vulkan_command_recorder_failure_stage::finish_recording,
            state_.recorded_batch_count);
        return false;
    }

    state_.command_buffer_recorded = true;
    return true;
}

const vulkan_backend_command_recorder_state& diagnostic_vulkan_command_recorder::recorder_state() const
{
    return state_;
}

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    const render_draw_list& draw_list,
    render_rect viewport)
{
    diagnostic_vulkan_pipeline_cache pipeline_cache;
    diagnostic_vulkan_command_recorder command_recorder;
    return submit_vulkan_backend_frame(device, pipeline_cache, command_recorder, draw_list, viewport);
}

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    vulkan_command_recorder_interface& command_recorder,
    const render_draw_list& draw_list,
    render_rect viewport)
{
    diagnostic_vulkan_pipeline_cache pipeline_cache;
    return submit_vulkan_backend_frame(device, pipeline_cache, command_recorder, draw_list, viewport);
}

vulkan_backend_frame_result submit_vulkan_backend_frame(
    vulkan_backend_device_interface& device,
    vulkan_pipeline_cache_interface& pipeline_cache,
    vulkan_command_recorder_interface& command_recorder,
    const render_draw_list& draw_list,
    render_rect viewport)
{
    vulkan_backend_frame_result result;
    result.attempted = true;
    result.reached_stage = vulkan_backend_frame_stage::backend_attempted;
    const auto finish_frame = [&result]() -> vulkan_backend_frame_result {
        result.pipeline_handoff = summarize_vulkan_frame_pipeline_handoff(result);
        return result;
    };
    result.lifecycle = device.current_lifecycle_readiness();
    result.command_submit = result.lifecycle.command_submit;
    result.command_recorder.ready = result.lifecycle.command_recorder_ready;
    const vulkan_backend_fallback_reason unready_reason = first_unready_reason(result.lifecycle);
    if (unready_reason != vulkan_backend_fallback_reason::none) {
        frame_lifecycle::mark_fallback(result, unready_reason);
        return finish_frame();
    }
    result.lifecycle_ready = true;
    result.reached_stage = vulkan_backend_frame_stage::lifecycle_ready;

    result.surface = device.current_surface_extent();
    result.swapchain_policy = make_swapchain_policy_state(result.surface);
    if (!result.surface.valid()) {
        frame_lifecycle::mark_fallback(result, vulkan_backend_fallback_reason::surface_unavailable);
        return finish_frame();
    }
    result.reached_stage = vulkan_backend_frame_stage::surface_extent_ready;
    if (!has_visible_area(viewport)) {
        frame_lifecycle::mark_fallback(
            result,
            vulkan_backend_fallback_reason::viewport_unavailable);
        return finish_frame();
    }
    result.surface_ready = true;

    const vulkan_frame_plan plan = build_vulkan_frame_plan(
        draw_list,
        vulkan_frame_plan_options{
            .viewport = viewport,
            .surface_width = result.surface.width,
            .surface_height = result.surface.height,
        });
    result.planned_batch_count = plan.batches.size();
    result.command_recorder.planned_batch_count = plan.batches.size();
    result.clipped_draw_call_count = plan.clipped_draw_call_count;
    result.discarded_draw_call_count = plan.discarded_draw_call_count;
    result.reached_stage = vulkan_backend_frame_stage::frame_plan_ready;
    result.frame_resources = make_frame_resource_lifetime_state(plan);

    if (!ensure_frame_plan_pipelines(pipeline_cache, plan, result)) {
        result.command_packets = build_vulkan_command_packet_bridge(
            plan,
            result.pipeline,
            result.resource_bindings,
            result.resource_registry);
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        return finish_frame();
    }

    result.resource_bindings = build_vulkan_resource_binding_state(draw_list, plan);
    result.resource_registry = build_vulkan_resource_registry_state(
        draw_list,
        plan,
        result.resource_bindings);
    result.command_packets = build_vulkan_command_packet_bridge(
        plan,
        result.pipeline,
        result.resource_bindings,
        result.resource_registry);
    result.command_recorder.gate = make_command_recorder_gate_state(
        result.resource_bindings,
        result.resource_registry);
    if (result.command_packets.blocked() || result.command_recorder.gate.blocked()) {
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        frame_lifecycle::mark_fallback(
            result,
            vulkan_backend_fallback_reason::resource_binding_unavailable);
        return finish_frame();
    }
    track_descriptor_set_resources(result.frame_resources, result.resource_bindings);
    fake_vulkan_command_packet_executor packet_executor;
    result.command_packet_execution = packet_executor.execute_packets(result.command_packets);
    result.command_recorder_operations = build_vulkan_command_recorder_operation_plan(
        result.command_packets,
        result.command_packet_execution);
    if (result.command_packet_execution.failed()
        || result.command_recorder_operations.blocked()) {
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        frame_lifecycle::mark_fallback(
            result,
            vulkan_backend_fallback_reason::record_commands_failed);
        return finish_frame();
    }

    result.lifecycle_policy = frame_lifecycle::make_policy_state();
    result.present_policy = make_frame_present_policy_state();
    const auto fail_frame_lifecycle = [&result](
                                          vulkan_frame_lifecycle_step step,
                                          vulkan_backend_fallback_reason reason) {
        release_frame_resources(
            result.frame_resources,
            vulkan_frame_resource_release_stage::fallback_cleanup);
        frame_lifecycle::mark_fallback(result, reason);
        frame_lifecycle::fail_step(result.lifecycle_policy, step, reason);
    };

    result.frame_sync = make_diagnostic_frame_sync_state();
    frame_lifecycle::start_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::acquire);
    request_signal(result.frame_sync.acquire_signal_image_available_semaphore);
    request_signal(result.frame_sync.acquire_signal_fence);
    result.swapchain.acquire_requested = true;
    mark_acquire_policy_requested(result.present_policy);
    result.swapchain.acquire = device.acquire_next_image(result.surface);
    result.swapchain_image_acquire_plan = build_vulkan_swapchain_image_acquire_plan(
        vulkan_swapchain_image_acquire_request{
            .requested = true,
            .lifecycle_ready = result.lifecycle.ready_for_frame(),
            .swapchain_ready = result.lifecycle.effective_swapchain_ready(),
            .swapchain = swapchain_handle_for_acquire(result.lifecycle),
            .image_count = infer_swapchain_image_count_for_acquire(
                result.lifecycle,
                result.swapchain.acquire),
            .timeout_nanoseconds = 0,
            .image_available_semaphore_ready =
                result.frame_sync.acquire_signal_image_available_semaphore.token.valid(),
            .fence_ready = result.frame_sync.acquire_signal_fence.token.valid(),
            .allow_suboptimal = true,
        },
        result.swapchain.acquire);
    mark_acquire_policy_result(result.present_policy, result.swapchain.acquire);
    result.swapchain_recreate_policy = evaluate_vulkan_swapchain_recreate_policy(
        result.swapchain_image_acquire_plan);
    complete_signal(
        result.frame_sync.acquire_signal_image_available_semaphore,
        result.swapchain_image_acquire_plan.ready_for_command_recording());
    complete_signal(
        result.frame_sync.acquire_signal_fence,
        result.swapchain_image_acquire_plan.ready_for_command_recording());
    if (!result.swapchain_image_acquire_plan.ready_for_command_recording()) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::acquire,
            vulkan_backend_fallback_reason::acquire_image_failed);
        return finish_frame();
    }
    track_swapchain_image_resource(result.frame_resources, result.swapchain.acquire.image.id);
    frame_lifecycle::complete_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::acquire);

    frame_lifecycle::start_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::begin);
    result.frame_begun = device.begin_frame(result.surface);
    if (!result.frame_begun) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::begin,
            vulkan_backend_fallback_reason::begin_frame_failed);
        return finish_frame();
    }
    frame_lifecycle::complete_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::begin);
    result.reached_stage = vulkan_backend_frame_stage::frame_begun;

    result.command_buffer_submit = make_command_buffer_submit_state(
        result.frame_sync.frame,
        plan.batches.size());
    track_command_buffer_resource(
        result.frame_resources,
        result.command_buffer_submit.recording.command_buffer);
    frame_lifecycle::start_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::render);
    fake_vulkan_command_buffer_operation_recorder operation_recorder;
    result.command_buffer_recording = record_vulkan_command_buffer_operations(
        operation_recorder,
        result.command_buffer_submit.recording.command_buffer,
        result.command_recorder_operations);
    if (result.command_buffer_recording.failed()) {
        result.command_buffer_submit.recording.status =
            vulkan_command_buffer_recording_status::failed;
        result.command_buffer_submit.recording.begin_requested =
            result.command_buffer_recording.begin_attempted;
        result.command_buffer_submit.recording.finish_requested =
            result.command_buffer_recording.end_attempted;
        result.command_buffer_submit.recording.recorded_batch_count =
            result.command_buffer_recording.recorded_operation_count;
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::render,
            vulkan_backend_fallback_reason::record_commands_failed);
        return finish_frame();
    }
    mark_command_buffer_recording_started(result.command_buffer_submit);
    if (!command_recorder.begin_recording(result.surface, plan.batches.size())) {
        const vulkan_command_recorder_gate_state command_recorder_gate =
            result.command_recorder.gate;
        result.command_recorder = command_recorder.recorder_state();
        result.command_recorder.gate = command_recorder_gate;
        mark_command_buffer_recording_failed(
            result.command_buffer_submit,
            result.command_recorder);
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::render,
            vulkan_backend_fallback_reason::record_commands_failed);
        return finish_frame();
    }
    for (const vulkan_draw_batch& batch : plan.batches) {
        if (!command_recorder.record_draw_batch(batch)) {
            const vulkan_command_recorder_gate_state command_recorder_gate =
                result.command_recorder.gate;
            result.command_recorder = command_recorder.recorder_state();
            result.command_recorder.gate = command_recorder_gate;
            mark_command_buffer_recording_failed(
                result.command_buffer_submit,
                result.command_recorder);
            fail_frame_lifecycle(
                vulkan_frame_lifecycle_step::render,
                vulkan_backend_fallback_reason::record_commands_failed);
            return finish_frame();
        }
    }
    if (!command_recorder.finish_recording()) {
        const vulkan_command_recorder_gate_state command_recorder_gate =
            result.command_recorder.gate;
        result.command_recorder = command_recorder.recorder_state();
        result.command_recorder.gate = command_recorder_gate;
        mark_command_buffer_recording_failed(
            result.command_buffer_submit,
            result.command_recorder);
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::render,
            vulkan_backend_fallback_reason::record_commands_failed);
        return finish_frame();
    }
    const vulkan_command_recorder_gate_state command_recorder_gate =
        result.command_recorder.gate;
    result.command_recorder = command_recorder.recorder_state();
    result.command_recorder.gate = command_recorder_gate;
    mark_command_buffer_recording_finished(
        result.command_buffer_submit,
        result.command_recorder);

    result.commands_recorded = device.record_frame_commands(plan);
    if (!result.commands_recorded) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::render,
            vulkan_backend_fallback_reason::record_commands_failed);
        return finish_frame();
    }
    frame_lifecycle::complete_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::render);
    result.recorded_batch_count = plan.batches.size();
    result.reached_stage = vulkan_backend_frame_stage::commands_recorded;

    result.submit_batch_plan = build_vulkan_submit_batch_plan(
        result.command_buffer_recording,
        result.lifecycle.command_submit);
    if (result.submit_batch_plan.blocked()) {
        const vulkan_frame_lifecycle_step failed_step =
            result.submit_batch_plan.fallback_reason
                == vulkan_backend_fallback_reason::present_frame_failed
            ? vulkan_frame_lifecycle_step::present
            : vulkan_frame_lifecycle_step::submit;
        fail_frame_lifecycle(failed_step, result.submit_batch_plan.fallback_reason);
        return finish_frame();
    }

    result.present_completion_plan = build_vulkan_present_completion_plan(
        result.submit_batch_plan,
        result.queue_submit);
    if (result.present_completion_plan.blocked()) {
        const vulkan_frame_lifecycle_step failed_step =
            result.present_completion_plan.fallback_reason
                == vulkan_backend_fallback_reason::submit_frame_failed
            ? vulkan_frame_lifecycle_step::submit
            : vulkan_frame_lifecycle_step::present;
        fail_frame_lifecycle(failed_step, result.present_completion_plan.fallback_reason);
        return finish_frame();
    }

    frame_lifecycle::start_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::submit);
    request_wait(result.frame_sync.submit_wait_image_available_semaphore);
    request_signal(result.frame_sync.submit_signal_render_finished_semaphore);
    request_signal(result.frame_sync.submit_signal_frame_fence);
    mark_frame_submit_requested(result.command_buffer_submit, result.recorded_batch_count);
    result.frame_submitted = device.submit_frame();
    complete_wait(result.frame_sync.submit_wait_image_available_semaphore, result.frame_submitted);
    complete_signal(result.frame_sync.submit_signal_render_finished_semaphore, result.frame_submitted);
    complete_signal(result.frame_sync.submit_signal_frame_fence, result.frame_submitted);
    mark_frame_submit_result(
        result.command_buffer_submit,
        result.frame_sync,
        result.frame_submitted);
    if (!result.frame_submitted) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::submit,
            vulkan_backend_fallback_reason::submit_frame_failed);
        return finish_frame();
    }
    frame_lifecycle::complete_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::submit);
    result.reached_stage = vulkan_backend_frame_stage::frame_submitted;

    frame_lifecycle::start_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::present);
    request_wait(result.frame_sync.present_wait_render_finished_semaphore);
    result.swapchain.present_requested = true;
    mark_present_policy_image_requested(result.present_policy, result.swapchain.acquire.image.id);
    result.swapchain.present = device.present_image(result.swapchain.acquire.image.id);
    mark_present_policy_image_result(result.present_policy, result.swapchain.present);
    result.swapchain_recreate_policy = evaluate_vulkan_swapchain_recreate_policy(
        result.swapchain_image_acquire_plan,
        result.swapchain.present);
    complete_wait(
        result.frame_sync.present_wait_render_finished_semaphore,
        result.swapchain.present.completed());
    result.swapchain.acquire.image.presented = result.swapchain.present.completed();
    if (!result.swapchain.present.completed()) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::present,
            vulkan_backend_fallback_reason::present_image_failed);
        return finish_frame();
    }

    mark_present_policy_frame_requested(result.present_policy);
    result.frame_presented = device.present_frame();
    mark_present_policy_frame_result(result.present_policy, result.frame_presented);
    if (!result.frame_presented) {
        fail_frame_lifecycle(
            vulkan_frame_lifecycle_step::present,
            vulkan_backend_fallback_reason::present_frame_failed);
        return finish_frame();
    }
    frame_lifecycle::complete_step(result.lifecycle_policy, vulkan_frame_lifecycle_step::present);
    result.reached_stage = vulkan_backend_frame_stage::frame_presented;

    release_frame_resources(
        result.frame_resources,
        vulkan_frame_resource_release_stage::after_present);
    frame_lifecycle::mark_fallback(result, vulkan_backend_fallback_reason::none);
    return finish_frame();
}

} // namespace quiz_vulkan::render::vulkan_backend
