#include "render/vulkan/vulkan_backend_pipeline_layout.h"

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

std::vector<std::uint8_t> make_spirv_bytes()
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(20);
    append_u32_le(bytes, 0x07230203);
    append_u32_le(bytes, 0x00010000);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, 8);
    append_u32_le(bytes, 0);
    return bytes;
}

quiz_vulkan::render::vulkan_backend::vulkan_shader_module_create_request make_shader_request(
    quiz_vulkan::render::vulkan_backend::vulkan_shader_stage stage,
    std::string_view id)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_shader_module_create_request{
        .id = vulkan_backend::vulkan_shader_module_id{.value = std::string{id}},
        .stage = stage,
        .entry_point = "main",
        .spirv_bytes = make_spirv_bytes(),
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_backend_shader_module_readiness_state
make_shader_readiness(bool include_fragment = true)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_shader_module_factory fake;
    std::vector<vulkan_backend::vulkan_shader_module_create_request> requests{
        make_shader_request(vulkan_backend::vulkan_shader_stage::vertex, "quad.vertex"),
    };
    if (include_fragment) {
        requests.push_back(
            make_shader_request(vulkan_backend::vulkan_shader_stage::fragment, "quad.fragment"));
    }

    return vulkan_backend::check_vulkan_shader_module_readiness(fake.adapter(), requests);
}

quiz_vulkan::render::vulkan_backend::vulkan_pipeline_layout_request
make_pipeline_layout_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_pipeline_layout_request{
        .descriptor_sets = {
            vulkan_backend::vulkan_descriptor_set_layout_request{
                .set = 0,
                .bindings = {
                    vulkan_backend::vulkan_descriptor_set_layout_binding_request{
                        .set = 0,
                        .binding = 0,
                        .kind =
                            vulkan_backend::vulkan_descriptor_set_layout_binding_kind::uniform_buffer,
                        .shader_stages = {
                            vulkan_backend::vulkan_shader_stage::vertex,
                            vulkan_backend::vulkan_shader_stage::fragment,
                        },
                        .descriptor_count = 1,
                    },
                    vulkan_backend::vulkan_descriptor_set_layout_binding_request{
                        .set = 0,
                        .binding = 1,
                        .kind =
                            vulkan_backend::vulkan_descriptor_set_layout_binding_kind::sampled_image,
                        .shader_stages = {vulkan_backend::vulkan_shader_stage::fragment},
                        .descriptor_count = 1,
                    },
                    vulkan_backend::vulkan_descriptor_set_layout_binding_request{
                        .set = 0,
                        .binding = 2,
                        .kind =
                            vulkan_backend::vulkan_descriptor_set_layout_binding_kind::sampler,
                        .shader_stages = {vulkan_backend::vulkan_shader_stage::fragment},
                        .descriptor_count = 1,
                    },
                },
            },
        },
        .push_constant_ranges = {
            vulkan_backend::vulkan_push_constant_range{
                .offset = 0,
                .size = 64,
                .shader_stages = {vulkan_backend::vulkan_shader_stage::vertex},
            },
        },
        .required_shader_stages = {
            vulkan_backend::vulkan_shader_stage::vertex,
            vulkan_backend::vulkan_shader_stage::fragment,
        },
        .max_push_constant_bytes = 128,
    };
}

void test_pipeline_layout_readiness_creates_and_destroys_fake_layouts()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_pipeline_layout_factory fake;
    const vulkan_backend::vulkan_pipeline_layout_create_result create =
        vulkan_backend::create_vulkan_pipeline_layout(
            fake.adapter(),
            make_shader_readiness(),
            make_pipeline_layout_request());

    require(create.checked, "pipeline layout create result is checked");
    require(
        create.status == vulkan_backend::vulkan_pipeline_layout_create_status::created,
        "pipeline layout create reports created");
    require(create.ready_for_pipeline(), "pipeline layout is ready for pipeline");
    require(create.pipeline_layout.value == 950, "fake pipeline layout handle is deterministic");
    require(
        create.descriptor_set_layouts.size() == 1,
        "pipeline layout stores descriptor set layout handle");
    require(
        create.descriptor_set_layouts.front().value == 900,
        "fake descriptor set layout handle is deterministic");
    require(
        create.shader_stage_compatibility.size() == 2,
        "pipeline layout checks both required shader stages");
    require(
        create.shader_stage_compatibility.front().completed(),
        "vertex shader stage compatibility completes");
    require(
        create.shader_stage_compatibility.back().completed(),
        "fragment shader stage compatibility completes");
    require(
        create.binding_diagnostics.size() == 3,
        "pipeline layout records descriptor binding diagnostics");
    require(create.binding_diagnostics.front().completed(), "first binding diagnostic completes");
    require(
        create.push_constant_diagnostics.size() == 1,
        "pipeline layout records push constant diagnostics");
    require(
        create.push_constant_diagnostics.front().completed(),
        "push constant diagnostic completes");
    require(
        fake.state().descriptor_set_layout_create_call_count == 1,
        "fake descriptor set layout create called once");
    require(
        fake.state().pipeline_layout_create_call_count == 1,
        "fake pipeline layout create called once");
    require(
        fake.state().last_pipeline_layout_create_call.valid(),
        "fake pipeline layout create call is valid");

    const vulkan_backend::vulkan_pipeline_layout_destroy_result destroy =
        vulkan_backend::destroy_vulkan_pipeline_layout(fake.adapter(), create);
    require(destroy.checked, "pipeline layout destroy result is checked");
    require(
        destroy.status == vulkan_backend::vulkan_pipeline_layout_destroy_status::destroyed,
        "pipeline layout destroy reports destroyed");
    require(destroy.completed(), "pipeline layout destroy completes");
    require(
        destroy.descriptor_set_layout_destroyed_count == 1,
        "pipeline layout destroy counts descriptor set layout destruction");
    require(
        fake.state().pipeline_layout_destroy_call_count == 1,
        "fake pipeline layout destroy called once");
    require(
        fake.state().descriptor_set_layout_destroy_call_count == 1,
        "fake descriptor set layout destroy called once");
    require(
        fake.state().destroyed_descriptor_set_layouts.front().value == 900,
        "fake destroy accounting records descriptor set layout handle");
}

void test_pipeline_layout_readiness_reports_missing_shader_stage()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_pipeline_layout_factory fake;
    const vulkan_backend::vulkan_pipeline_layout_create_result create =
        vulkan_backend::create_vulkan_pipeline_layout(
            fake.adapter(),
            make_shader_readiness(false),
            make_pipeline_layout_request());

    require(
        create.status == vulkan_backend::vulkan_pipeline_layout_create_status::missing_shader_stage,
        "pipeline layout create reports missing shader stage compatibility");
    require(
        create.diagnostic
            == "Vulkan pipeline layout missing shader stage compatibility: fragment",
        "missing shader stage diagnostic is stable");
    require(
        fake.state().descriptor_set_layout_create_call_count == 0,
        "fake pipeline layout factory is not called when shader stage is missing");
}

void test_pipeline_layout_readiness_reports_duplicate_and_invalid_bindings()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_pipeline_layout_factory fake;
    vulkan_backend::vulkan_pipeline_layout_request duplicate = make_pipeline_layout_request();
    duplicate.descriptor_sets.front().bindings.push_back(
        duplicate.descriptor_sets.front().bindings.front());
    const vulkan_backend::vulkan_pipeline_layout_create_result duplicate_result =
        vulkan_backend::create_vulkan_pipeline_layout(
            fake.adapter(),
            make_shader_readiness(),
            duplicate);
    require(
        duplicate_result.status
            == vulkan_backend::vulkan_pipeline_layout_create_status::duplicate_binding,
        "pipeline layout create reports duplicate descriptor binding");
    require(
        duplicate_result.diagnostic
            == "Vulkan pipeline layout descriptor binding is duplicated",
        "duplicate descriptor binding diagnostic is stable");

    vulkan_backend::vulkan_pipeline_layout_request invalid = make_pipeline_layout_request();
    invalid.descriptor_sets.front().bindings.front().set = 1;
    const vulkan_backend::vulkan_pipeline_layout_create_result invalid_result =
        vulkan_backend::create_vulkan_pipeline_layout(
            fake.adapter(),
            make_shader_readiness(),
            invalid);
    require(
        invalid_result.status
            == vulkan_backend::vulkan_pipeline_layout_create_status::invalid_binding,
        "pipeline layout create reports invalid descriptor binding");
    require(
        invalid_result.binding_diagnostics.front().set_matches_layout == false,
        "invalid descriptor binding records set mismatch");
    require(
        fake.state().descriptor_set_layout_create_call_count == 0,
        "fake pipeline layout factory is not called for invalid bindings");
}

void test_pipeline_layout_readiness_reports_push_constant_errors()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_pipeline_layout_factory fake;
    vulkan_backend::vulkan_pipeline_layout_request overlapping = make_pipeline_layout_request();
    overlapping.push_constant_ranges.push_back(vulkan_backend::vulkan_push_constant_range{
        .offset = 32,
        .size = 32,
        .shader_stages = {vulkan_backend::vulkan_shader_stage::fragment},
    });
    const vulkan_backend::vulkan_pipeline_layout_create_result overlap_result =
        vulkan_backend::create_vulkan_pipeline_layout(
            fake.adapter(),
            make_shader_readiness(),
            overlapping);
    require(
        overlap_result.status
            == vulkan_backend::vulkan_pipeline_layout_create_status::push_constant_overlap,
        "pipeline layout create reports push constant overlap");
    require(
        overlap_result.diagnostic == "Vulkan pipeline layout push constant ranges overlap",
        "push constant overlap diagnostic is stable");

    vulkan_backend::vulkan_pipeline_layout_request oversized = make_pipeline_layout_request();
    oversized.push_constant_ranges.front().offset = 96;
    oversized.push_constant_ranges.front().size = 64;
    const vulkan_backend::vulkan_pipeline_layout_create_result oversized_result =
        vulkan_backend::create_vulkan_pipeline_layout(
            fake.adapter(),
            make_shader_readiness(),
            oversized);
    require(
        oversized_result.status
            == vulkan_backend::vulkan_pipeline_layout_create_status::push_constant_size_exceeded,
        "pipeline layout create reports push constant size error");
    require(
        oversized_result.diagnostic == "Vulkan pipeline layout push constant range exceeds limit",
        "push constant size diagnostic is stable");
    require(
        fake.state().pipeline_layout_create_call_count == 0,
        "fake pipeline layout factory is not called for push constant validation errors");
}

void test_pipeline_layout_readiness_maps_recoverable_and_fatal_create_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_pipeline_layout_factory recoverable_fake(
        vulkan_backend::fake_vulkan_pipeline_layout_factory_options{
            .descriptor_set_layout_create_status =
                vulkan_backend::vulkan_pipeline_layout_adapter_call_status::failed_recoverable,
        });
    const vulkan_backend::vulkan_pipeline_layout_create_result recoverable =
        vulkan_backend::create_vulkan_pipeline_layout(
            recoverable_fake.adapter(),
            make_shader_readiness(),
            make_pipeline_layout_request());
    require(
        recoverable.status
            == vulkan_backend::vulkan_pipeline_layout_create_status::create_failed_recoverable,
        "pipeline layout create maps recoverable descriptor set layout failure");
    require(recoverable.recoverable_create_failure(), "recoverable create failure is classified");
    require(!recoverable.fatal_create_failure(), "recoverable create failure is not fatal");
    require(!recoverable.pipeline_layout.valid(), "recoverable create failure has no layout");
    require(
        recoverable_fake.state().pipeline_layout_create_call_count == 0,
        "pipeline layout create is skipped when descriptor set layout creation fails");

    vulkan_backend::fake_vulkan_pipeline_layout_factory fatal_fake(
        vulkan_backend::fake_vulkan_pipeline_layout_factory_options{
            .pipeline_layout_create_status =
                vulkan_backend::vulkan_pipeline_layout_adapter_call_status::failed_fatal,
        });
    const vulkan_backend::vulkan_pipeline_layout_create_result fatal =
        vulkan_backend::create_vulkan_pipeline_layout(
            fatal_fake.adapter(),
            make_shader_readiness(),
            make_pipeline_layout_request());
    require(
        fatal.status == vulkan_backend::vulkan_pipeline_layout_create_status::create_failed_fatal,
        "pipeline layout create maps fatal pipeline layout failure");
    require(fatal.fatal_create_failure(), "fatal create failure is classified");
    require(!fatal.recoverable_create_failure(), "fatal create failure is not recoverable");
    require(!fatal.pipeline_layout.valid(), "fatal create failure has no layout");
    require(
        fatal_fake.state().pipeline_layout_create_call_count == 1,
        "fatal pipeline layout failure still records create call");
}

void test_pipeline_layout_readiness_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::descriptor_set_layout_binding_kind_name(
            vulkan_backend::vulkan_descriptor_set_layout_binding_kind::combined_image_sampler)
            == std::string_view{"combined_image_sampler"},
        "descriptor set layout binding kind name is stable");
    require(
        vulkan_backend::pipeline_layout_adapter_call_status_name(
            vulkan_backend::vulkan_pipeline_layout_adapter_call_status::failed_recoverable)
            == std::string_view{"failed_recoverable"},
        "pipeline layout adapter recoverable status name is stable");
    require(
        vulkan_backend::pipeline_layout_create_status_name(
            vulkan_backend::vulkan_pipeline_layout_create_status::push_constant_overlap)
            == std::string_view{"push_constant_overlap"},
        "pipeline layout create push constant overlap status name is stable");
    require(
        vulkan_backend::pipeline_layout_destroy_status_name(
            vulkan_backend::vulkan_pipeline_layout_destroy_status::destroy_failed_fatal)
            == std::string_view{"destroy_failed_fatal"},
        "pipeline layout destroy fatal status name is stable");
}

} // namespace

int main()
{
    test_pipeline_layout_readiness_creates_and_destroys_fake_layouts();
    test_pipeline_layout_readiness_reports_missing_shader_stage();
    test_pipeline_layout_readiness_reports_duplicate_and_invalid_bindings();
    test_pipeline_layout_readiness_reports_push_constant_errors();
    test_pipeline_layout_readiness_maps_recoverable_and_fatal_create_failures();
    test_pipeline_layout_readiness_names_are_stable();
    return 0;
}
