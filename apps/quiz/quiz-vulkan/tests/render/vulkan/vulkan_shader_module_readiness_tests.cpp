#include "render/vulkan/vulkan_backend_shader_module.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

void append_u32_le(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffU));
}

std::vector<std::uint8_t> make_spirv_bytes(
    std::uint32_t magic = 0x07230203,
    std::uint32_t version = 0x00010000)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(20);
    append_u32_le(bytes, magic);
    append_u32_le(bytes, version);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, 8);
    append_u32_le(bytes, 0);
    return bytes;
}

quiz_vulkan::render::vulkan_backend::vulkan_shader_module_create_request make_shader_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_shader_module_create_request{
        .id = vulkan_backend::vulkan_shader_module_id{.value = "quad.vertex"},
        .stage = vulkan_backend::vulkan_shader_stage::vertex,
        .entry_point = "main",
        .spirv_bytes = make_spirv_bytes(),
    };
}

void test_shader_module_readiness_creates_and_destroys_fake_module()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_shader_module_factory fake;
    const vulkan_backend::vulkan_shader_module_create_result create =
        vulkan_backend::create_vulkan_shader_module(
            fake.adapter(),
            make_shader_request());

    require(create.checked, "shader module create result is checked");
    require(
        create.status == vulkan_backend::vulkan_shader_module_create_status::created,
        "shader module create result reports created");
    require(create.ready_for_pipeline(), "created shader module is ready for pipeline");
    require(create.handle.value == 700, "fake shader module handle is deterministic");
    require(create.spirv_byte_count == 20, "shader module records SPIR-V byte count");
    require(create.spirv_word_count == 5, "shader module records SPIR-V word count");
    require(create.spirv_magic_valid, "shader module validates SPIR-V magic");
    require(create.spirv_version_supported, "shader module validates SPIR-V version");
    require(create.entry_point_present, "shader module records entry point readiness");
    require(create.stage_supported, "shader module records stage support");
    require(create.create_call.valid(), "shader module create call is valid");
    require(fake.state().create_call_count == 1, "fake shader module factory create called once");
    require(
        fake.state().last_create_call.stage == vulkan_backend::vulkan_shader_stage::vertex,
        "fake shader module factory records stage");
    require(
        fake.state().last_create_call.entry_point == "main",
        "fake shader module factory records entry point");

    const vulkan_backend::vulkan_shader_module_destroy_result destroy =
        vulkan_backend::destroy_vulkan_shader_module(fake.adapter(), create.handle);
    require(destroy.checked, "shader module destroy result is checked");
    require(
        destroy.status == vulkan_backend::vulkan_shader_module_destroy_status::destroyed,
        "shader module destroy result reports destroyed");
    require(destroy.completed(), "shader module destroy completes");
    require(fake.state().destroy_call_count == 1, "fake shader module factory destroy called once");
    require(
        fake.state().last_destroy_call.handle.value == 700,
        "fake shader module factory records destroyed handle");
}

void test_shader_module_readiness_reports_missing_and_bad_spirv_bytes()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_shader_module_factory fake;
    vulkan_backend::vulkan_shader_module_create_request missing = make_shader_request();
    missing.spirv_bytes = {};
    const vulkan_backend::vulkan_shader_module_create_result missing_result =
        vulkan_backend::create_vulkan_shader_module(fake.adapter(), missing);
    require(
        missing_result.status
            == vulkan_backend::vulkan_shader_module_create_status::missing_spirv_bytes,
        "shader module create reports missing SPIR-V bytes");
    require(
        missing_result.diagnostic
            == "Vulkan shader module SPIR-V bytecode is missing or incomplete",
        "shader module missing bytes diagnostic is stable");
    require(fake.state().create_call_count == 0, "fake factory is not called for missing bytes");

    vulkan_backend::vulkan_shader_module_create_request bad_magic = make_shader_request();
    bad_magic.spirv_bytes = make_spirv_bytes(0x00000000, 0x00010000);
    const vulkan_backend::vulkan_shader_module_create_result bad_magic_result =
        vulkan_backend::create_vulkan_shader_module(fake.adapter(), bad_magic);
    require(
        bad_magic_result.status
            == vulkan_backend::vulkan_shader_module_create_status::bad_spirv_magic,
        "shader module create reports bad SPIR-V magic");
    require(
        bad_magic_result.diagnostic == "Vulkan shader module SPIR-V magic is invalid",
        "shader module bad magic diagnostic is stable");
    require(fake.state().create_call_count == 0, "fake factory is not called for bad magic");

    vulkan_backend::vulkan_shader_module_create_request bad_version = make_shader_request();
    bad_version.spirv_bytes = make_spirv_bytes(0x07230203, 0x00020000);
    const vulkan_backend::vulkan_shader_module_create_result bad_version_result =
        vulkan_backend::create_vulkan_shader_module(fake.adapter(), bad_version);
    require(
        bad_version_result.status
            == vulkan_backend::vulkan_shader_module_create_status::bad_spirv_version,
        "shader module create reports bad SPIR-V version");
    require(
        bad_version_result.diagnostic == "Vulkan shader module SPIR-V version is unsupported",
        "shader module bad version diagnostic is stable");
    require(fake.state().create_call_count == 0, "fake factory is not called for bad version");
}

void test_shader_module_readiness_reports_missing_entry_point_and_unsupported_stage()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_shader_module_factory fake;
    vulkan_backend::vulkan_shader_module_create_request missing_entry = make_shader_request();
    missing_entry.entry_point = {};
    const vulkan_backend::vulkan_shader_module_create_result missing_entry_result =
        vulkan_backend::create_vulkan_shader_module(fake.adapter(), missing_entry);
    require(
        missing_entry_result.status
            == vulkan_backend::vulkan_shader_module_create_status::missing_entry_point,
        "shader module create reports missing entry point");
    require(
        missing_entry_result.diagnostic == "Vulkan shader module entry point is missing",
        "shader module missing entry point diagnostic is stable");
    require(fake.state().create_call_count == 0, "fake factory is not called for missing entry point");

    vulkan_backend::vulkan_shader_module_create_request unsupported_stage = make_shader_request();
    unsupported_stage.stage = vulkan_backend::vulkan_shader_stage::compute;
    const vulkan_backend::vulkan_shader_module_create_result unsupported_stage_result =
        vulkan_backend::create_vulkan_shader_module(fake.adapter(), unsupported_stage);
    require(
        unsupported_stage_result.status
            == vulkan_backend::vulkan_shader_module_create_status::unsupported_stage,
        "shader module create reports unsupported stage");
    require(
        unsupported_stage_result.diagnostic
            == "Vulkan shader module stage is unsupported: compute",
        "shader module unsupported stage diagnostic is stable");
    require(fake.state().create_call_count == 0, "fake factory is not called for unsupported stage");
}

void test_shader_module_readiness_maps_recoverable_and_fatal_create_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_shader_module_factory recoverable_fake(
        vulkan_backend::fake_vulkan_shader_module_factory_options{
            .create_status =
                vulkan_backend::vulkan_shader_module_adapter_call_status::failed_recoverable,
        });
    const vulkan_backend::vulkan_shader_module_create_result recoverable =
        vulkan_backend::create_vulkan_shader_module(
            recoverable_fake.adapter(),
            make_shader_request());
    require(
        recoverable.status
            == vulkan_backend::vulkan_shader_module_create_status::create_failed_recoverable,
        "shader module create maps recoverable factory failure");
    require(recoverable.recoverable_create_failure(), "recoverable create failure is classified");
    require(!recoverable.fatal_create_failure(), "recoverable create failure is not fatal");
    require(!recoverable.handle.valid(), "recoverable create failure has no handle");

    vulkan_backend::fake_vulkan_shader_module_factory fatal_fake(
        vulkan_backend::fake_vulkan_shader_module_factory_options{
            .create_status =
                vulkan_backend::vulkan_shader_module_adapter_call_status::failed_fatal,
        });
    const vulkan_backend::vulkan_shader_module_create_result fatal =
        vulkan_backend::create_vulkan_shader_module(
            fatal_fake.adapter(),
            make_shader_request());
    require(
        fatal.status == vulkan_backend::vulkan_shader_module_create_status::create_failed_fatal,
        "shader module create maps fatal factory failure");
    require(fatal.fatal_create_failure(), "fatal create failure is classified");
    require(!fatal.recoverable_create_failure(), "fatal create failure is not recoverable");
    require(!fatal.handle.valid(), "fatal create failure has no handle");
}

void test_shader_module_readiness_aggregates_modules_for_pipeline_diagnostics()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_shader_module_factory fake;
    vulkan_backend::vulkan_shader_module_create_request vertex = make_shader_request();
    vulkan_backend::vulkan_shader_module_create_request fragment = make_shader_request();
    fragment.id = vulkan_backend::vulkan_shader_module_id{.value = "quad.fragment"};
    fragment.stage = vulkan_backend::vulkan_shader_stage::fragment;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state readiness =
        vulkan_backend::check_vulkan_shader_module_readiness(
            fake.adapter(),
            {vertex, fragment});

    require(readiness.checked, "shader module readiness state is checked");
    require(readiness.requested_module_count == 2, "shader module readiness records requested count");
    require(readiness.created_module_count == 2, "shader module readiness records created count");
    require(readiness.failed_module_count == 0, "shader module readiness records no failures");
    require(readiness.completed(), "shader module readiness completes for valid modules");
    require(fake.state().create_call_count == 2, "fake factory creates both shader modules");
}

void test_shader_module_readiness_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::shader_stage_name(vulkan_backend::vulkan_shader_stage::compute)
            == std::string_view{"compute"},
        "shader stage name for compute is stable");
    require(
        vulkan_backend::shader_module_create_status_name(
            vulkan_backend::vulkan_shader_module_create_status::bad_spirv_magic)
            == std::string_view{"bad_spirv_magic"},
        "shader module create status name for bad magic is stable");
    require(
        vulkan_backend::shader_module_create_status_name(
            vulkan_backend::vulkan_shader_module_create_status::create_failed_recoverable)
            == std::string_view{"create_failed_recoverable"},
        "shader module create status name for recoverable failure is stable");
    require(
        vulkan_backend::shader_module_destroy_status_name(
            vulkan_backend::vulkan_shader_module_destroy_status::destroy_failed_fatal)
            == std::string_view{"destroy_failed_fatal"},
        "shader module destroy status name for fatal failure is stable");
}

} // namespace

int main()
{
    test_shader_module_readiness_creates_and_destroys_fake_module();
    test_shader_module_readiness_reports_missing_and_bad_spirv_bytes();
    test_shader_module_readiness_reports_missing_entry_point_and_unsupported_stage();
    test_shader_module_readiness_maps_recoverable_and_fatal_create_failures();
    test_shader_module_readiness_aggregates_modules_for_pipeline_diagnostics();
    test_shader_module_readiness_names_are_stable();
    return 0;
}
