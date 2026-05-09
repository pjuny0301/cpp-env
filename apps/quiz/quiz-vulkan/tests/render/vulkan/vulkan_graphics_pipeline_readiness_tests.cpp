#include "render/vulkan/vulkan_backend_graphics_pipeline.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
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
make_pipeline_layout_request(
    std::vector<quiz_vulkan::render::vulkan_backend::vulkan_shader_stage> required_stages = {
        quiz_vulkan::render::vulkan_backend::vulkan_shader_stage::vertex,
        quiz_vulkan::render::vulkan_backend::vulkan_shader_stage::fragment,
    })
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
                        .shader_stages = required_stages,
                        .descriptor_count = 1,
                    },
                },
            },
        },
        .push_constant_ranges = {},
        .required_shader_stages = required_stages,
        .max_push_constant_bytes = 128,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_pipeline_layout_create_result
make_pipeline_layout(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_shader_module_readiness_state&
        shader_modules,
    std::vector<quiz_vulkan::render::vulkan_backend::vulkan_shader_stage> required_stages = {
        quiz_vulkan::render::vulkan_backend::vulkan_shader_stage::vertex,
        quiz_vulkan::render::vulkan_backend::vulkan_shader_stage::fragment,
    })
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_pipeline_layout_factory fake;
    return vulkan_backend::create_vulkan_pipeline_layout(
        fake.adapter(),
        shader_modules,
        make_pipeline_layout_request(std::move(required_stages)));
}

quiz_vulkan::render::vulkan_backend::vulkan_graphics_pipeline_request
make_graphics_pipeline_request()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_graphics_pipeline_request{
        .pipeline_id = "quad.graphics",
        .required_shader_stages = {
            vulkan_backend::vulkan_shader_stage::vertex,
            vulkan_backend::vulkan_shader_stage::fragment,
        },
        .require_vertex_input = true,
        .vertex_input = vulkan_backend::vulkan_vertex_input_state{
            .bindings = {
                vulkan_backend::vulkan_vertex_input_binding{
                    .binding = 0,
                    .stride = 20,
                    .input_rate = vulkan_backend::vulkan_vertex_input_rate::vertex,
                },
            },
            .attributes = {
                vulkan_backend::vulkan_vertex_attribute{
                    .location = 0,
                    .binding = 0,
                    .offset = 0,
                    .byte_size = 8,
                },
                vulkan_backend::vulkan_vertex_attribute{
                    .location = 1,
                    .binding = 0,
                    .offset = 8,
                    .byte_size = 12,
                },
            },
        },
        .input_assembly = vulkan_backend::vulkan_input_assembly_state{
            .topology = vulkan_backend::vulkan_primitive_topology::triangle_list,
            .primitive_restart_enable = false,
        },
        .rasterization = vulkan_backend::vulkan_rasterization_state{
            .polygon_mode = vulkan_backend::vulkan_polygon_mode::fill,
            .cull_mode = vulkan_backend::vulkan_cull_mode::back,
            .front_face = vulkan_backend::vulkan_front_face::counter_clockwise,
            .line_width = 1.0F,
        },
        .color_blend = vulkan_backend::vulkan_color_blend_state{
            .attachments = {
                vulkan_backend::vulkan_color_blend_attachment_state{
                    .blend_enable = true,
                    .src_color_factor = vulkan_backend::vulkan_blend_factor::src_alpha,
                    .dst_color_factor =
                        vulkan_backend::vulkan_blend_factor::one_minus_src_alpha,
                    .color_op = vulkan_backend::vulkan_blend_op::add,
                    .src_alpha_factor = vulkan_backend::vulkan_blend_factor::one,
                    .dst_alpha_factor = vulkan_backend::vulkan_blend_factor::zero,
                    .alpha_op = vulkan_backend::vulkan_blend_op::add,
                    .color_write_mask = 0x0f,
                },
            },
        },
        .depth_stencil = vulkan_backend::vulkan_depth_stencil_state{
            .depth_test_enable = false,
            .depth_write_enable = false,
            .compare_op = vulkan_backend::vulkan_depth_compare_op::less_or_equal,
            .depth_bounds_test_enable = false,
            .min_depth_bounds = 0.0F,
            .max_depth_bounds = 1.0F,
        },
    };
}

void test_graphics_pipeline_readiness_creates_and_destroys_fake_pipeline()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_shader_readiness();
    const vulkan_backend::vulkan_pipeline_layout_create_result pipeline_layout =
        make_pipeline_layout(shader_modules);

    vulkan_backend::fake_vulkan_graphics_pipeline_factory fake;
    const vulkan_backend::vulkan_graphics_pipeline_create_result create =
        vulkan_backend::create_vulkan_graphics_pipeline(
            fake.adapter(),
            shader_modules,
            pipeline_layout,
            make_graphics_pipeline_request());

    require(create.checked, "graphics pipeline create result is checked");
    require(
        create.status == vulkan_backend::vulkan_graphics_pipeline_create_status::created,
        "graphics pipeline create reports created");
    require(create.ready_for_draw(), "graphics pipeline is ready for draw");
    require(create.pipeline.value == 1100, "fake graphics pipeline handle is deterministic");
    require(create.shader_stage_diagnostics.size() == 2, "graphics pipeline checks two stages");
    require(
        create.shader_stage_diagnostics.front().completed(),
        "graphics pipeline vertex stage diagnostic completes");
    require(
        create.shader_stage_diagnostics.back().completed(),
        "graphics pipeline fragment stage diagnostic completes");
    require(create.fixed_function.completed(), "graphics pipeline fixed-function diagnostics complete");
    require(create.fixed_function.vertex_binding_count == 1, "graphics pipeline counts vertex bindings");
    require(
        create.fixed_function.vertex_attribute_count == 2,
        "graphics pipeline counts vertex attributes");
    require(
        create.fixed_function.color_attachment_count == 1,
        "graphics pipeline counts color attachments");
    require(create.create_call.valid(), "graphics pipeline create call is valid");
    require(fake.state().create_call_count == 1, "fake graphics pipeline factory create called once");
    require(
        fake.state().last_create_call.pipeline_id == "quad.graphics",
        "fake graphics pipeline factory records pipeline id");
    require(
        fake.state().last_create_call.shader_modules.size() == 2,
        "fake graphics pipeline factory records shader modules");

    const vulkan_backend::vulkan_graphics_pipeline_destroy_result destroy =
        vulkan_backend::destroy_vulkan_graphics_pipeline(fake.adapter(), create.pipeline);
    require(destroy.checked, "graphics pipeline destroy result is checked");
    require(
        destroy.status == vulkan_backend::vulkan_graphics_pipeline_destroy_status::destroyed,
        "graphics pipeline destroy reports destroyed");
    require(destroy.completed(), "graphics pipeline destroy completes");
    require(destroy.destroyed_pipeline_count == 1, "graphics pipeline destroy count is deterministic");
    require(fake.state().destroy_call_count == 1, "fake graphics pipeline factory destroy called once");
    require(
        fake.state().destroyed_pipelines.front().value == 1100,
        "fake graphics pipeline factory records destroyed handle");
}

void test_graphics_pipeline_readiness_reports_missing_shader_module()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_shader_readiness(false);
    const vulkan_backend::vulkan_pipeline_layout_create_result pipeline_layout =
        make_pipeline_layout(
            shader_modules,
            {vulkan_backend::vulkan_shader_stage::vertex});

    vulkan_backend::fake_vulkan_graphics_pipeline_factory fake;
    const vulkan_backend::vulkan_graphics_pipeline_create_result create =
        vulkan_backend::create_vulkan_graphics_pipeline(
            fake.adapter(),
            shader_modules,
            pipeline_layout,
            make_graphics_pipeline_request());

    require(
        create.status
            == vulkan_backend::vulkan_graphics_pipeline_create_status::shader_module_unavailable,
        "graphics pipeline create reports missing shader module");
    require(
        create.diagnostic == "Vulkan graphics pipeline missing shader module for stage: fragment",
        "missing shader module diagnostic is stable");
    require(fake.state().create_call_count == 0, "fake graphics pipeline factory is not called");
}

void test_graphics_pipeline_readiness_reports_missing_pipeline_layout()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_shader_readiness();

    vulkan_backend::fake_vulkan_graphics_pipeline_factory fake;
    const vulkan_backend::vulkan_graphics_pipeline_create_result create =
        vulkan_backend::create_vulkan_graphics_pipeline(
            fake.adapter(),
            shader_modules,
            vulkan_backend::vulkan_pipeline_layout_create_result{
                .checked = true,
                .status =
                    vulkan_backend::vulkan_pipeline_layout_create_status::missing_shader_stage,
            },
            make_graphics_pipeline_request());

    require(
        create.status
            == vulkan_backend::vulkan_graphics_pipeline_create_status::pipeline_layout_unavailable,
        "graphics pipeline create reports missing pipeline layout");
    require(
        create.diagnostic == "Vulkan graphics pipeline layout readiness is unavailable",
        "missing pipeline layout diagnostic is stable");
    require(fake.state().create_call_count == 0, "fake graphics pipeline factory is not called");
}

void test_graphics_pipeline_readiness_reports_incompatible_shader_stages()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_shader_readiness();
    const vulkan_backend::vulkan_pipeline_layout_create_result pipeline_layout =
        make_pipeline_layout(
            shader_modules,
            {vulkan_backend::vulkan_shader_stage::vertex});

    vulkan_backend::fake_vulkan_graphics_pipeline_factory fake;
    const vulkan_backend::vulkan_graphics_pipeline_create_result create =
        vulkan_backend::create_vulkan_graphics_pipeline(
            fake.adapter(),
            shader_modules,
            pipeline_layout,
            make_graphics_pipeline_request());

    require(
        create.status
            == vulkan_backend::vulkan_graphics_pipeline_create_status::incompatible_shader_stages,
        "graphics pipeline create reports incompatible shader stages");
    require(
        create.diagnostic
            == "Vulkan graphics pipeline shader stage is incompatible with pipeline layout: fragment",
        "incompatible shader stage diagnostic is stable");
    require(fake.state().create_call_count == 0, "fake graphics pipeline factory is not called");
}

void test_graphics_pipeline_readiness_reports_invalid_fixed_function_states()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_shader_readiness();
    const vulkan_backend::vulkan_pipeline_layout_create_result pipeline_layout =
        make_pipeline_layout(shader_modules);
    vulkan_backend::fake_vulkan_graphics_pipeline_factory fake;

    vulkan_backend::vulkan_graphics_pipeline_request invalid_vertex =
        make_graphics_pipeline_request();
    invalid_vertex.vertex_input.attributes.front().byte_size = 24;
    const vulkan_backend::vulkan_graphics_pipeline_create_result vertex_result =
        vulkan_backend::create_vulkan_graphics_pipeline(
            fake.adapter(),
            shader_modules,
            pipeline_layout,
            invalid_vertex);
    require(
        vertex_result.status
            == vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_vertex_input_state,
        "graphics pipeline create reports invalid vertex input state");

    vulkan_backend::vulkan_graphics_pipeline_request invalid_assembly =
        make_graphics_pipeline_request();
    invalid_assembly.input_assembly.primitive_restart_enable = true;
    const vulkan_backend::vulkan_graphics_pipeline_create_result assembly_result =
        vulkan_backend::create_vulkan_graphics_pipeline(
            fake.adapter(),
            shader_modules,
            pipeline_layout,
            invalid_assembly);
    require(
        assembly_result.status
            == vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_input_assembly_state,
        "graphics pipeline create reports invalid input assembly state");

    vulkan_backend::vulkan_graphics_pipeline_request invalid_raster =
        make_graphics_pipeline_request();
    invalid_raster.rasterization.line_width = 0.0F;
    const vulkan_backend::vulkan_graphics_pipeline_create_result raster_result =
        vulkan_backend::create_vulkan_graphics_pipeline(
            fake.adapter(),
            shader_modules,
            pipeline_layout,
            invalid_raster);
    require(
        raster_result.status
            == vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_rasterization_state,
        "graphics pipeline create reports invalid rasterization state");

    vulkan_backend::vulkan_graphics_pipeline_request invalid_blend =
        make_graphics_pipeline_request();
    invalid_blend.color_blend.attachments.front().color_write_mask = 0;
    const vulkan_backend::vulkan_graphics_pipeline_create_result blend_result =
        vulkan_backend::create_vulkan_graphics_pipeline(
            fake.adapter(),
            shader_modules,
            pipeline_layout,
            invalid_blend);
    require(
        blend_result.status
            == vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_blend_state,
        "graphics pipeline create reports invalid blend state");

    vulkan_backend::vulkan_graphics_pipeline_request invalid_depth =
        make_graphics_pipeline_request();
    invalid_depth.depth_stencil.depth_write_enable = true;
    const vulkan_backend::vulkan_graphics_pipeline_create_result depth_result =
        vulkan_backend::create_vulkan_graphics_pipeline(
            fake.adapter(),
            shader_modules,
            pipeline_layout,
            invalid_depth);
    require(
        depth_result.status
            == vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_depth_state,
        "graphics pipeline create reports invalid depth state");
    require(
        fake.state().create_call_count == 0,
        "fake graphics pipeline factory is not called for invalid fixed-function states");
}

void test_graphics_pipeline_readiness_maps_recoverable_and_fatal_create_failures()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_shader_readiness();
    const vulkan_backend::vulkan_pipeline_layout_create_result pipeline_layout =
        make_pipeline_layout(shader_modules);

    vulkan_backend::fake_vulkan_graphics_pipeline_factory recoverable_fake(
        vulkan_backend::fake_vulkan_graphics_pipeline_factory_options{
            .create_status =
                vulkan_backend::vulkan_graphics_pipeline_adapter_call_status::failed_recoverable,
        });
    const vulkan_backend::vulkan_graphics_pipeline_create_result recoverable =
        vulkan_backend::create_vulkan_graphics_pipeline(
            recoverable_fake.adapter(),
            shader_modules,
            pipeline_layout,
            make_graphics_pipeline_request());
    require(
        recoverable.status
            == vulkan_backend::vulkan_graphics_pipeline_create_status::create_failed_recoverable,
        "graphics pipeline create maps recoverable fake failure");
    require(recoverable.recoverable_create_failure(), "recoverable create failure is classified");
    require(!recoverable.fatal_create_failure(), "recoverable create failure is not fatal");
    require(!recoverable.pipeline.valid(), "recoverable create failure has no pipeline");

    vulkan_backend::fake_vulkan_graphics_pipeline_factory fatal_fake(
        vulkan_backend::fake_vulkan_graphics_pipeline_factory_options{
            .create_status =
                vulkan_backend::vulkan_graphics_pipeline_adapter_call_status::failed_fatal,
        });
    const vulkan_backend::vulkan_graphics_pipeline_create_result fatal =
        vulkan_backend::create_vulkan_graphics_pipeline(
            fatal_fake.adapter(),
            shader_modules,
            pipeline_layout,
            make_graphics_pipeline_request());
    require(
        fatal.status == vulkan_backend::vulkan_graphics_pipeline_create_status::create_failed_fatal,
        "graphics pipeline create maps fatal fake failure");
    require(fatal.fatal_create_failure(), "fatal create failure is classified");
    require(!fatal.recoverable_create_failure(), "fatal create failure is not recoverable");
    require(!fatal.pipeline.valid(), "fatal create failure has no pipeline");
}

void test_graphics_pipeline_readiness_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::vertex_input_rate_name(vulkan_backend::vulkan_vertex_input_rate::instance)
            == std::string_view{"instance"},
        "vertex input rate name is stable");
    require(
        vulkan_backend::primitive_topology_name(
            vulkan_backend::vulkan_primitive_topology::triangle_strip)
            == std::string_view{"triangle_strip"},
        "primitive topology name is stable");
    require(
        vulkan_backend::graphics_pipeline_adapter_call_status_name(
            vulkan_backend::vulkan_graphics_pipeline_adapter_call_status::failed_recoverable)
            == std::string_view{"failed_recoverable"},
        "graphics pipeline adapter status name is stable");
    require(
        vulkan_backend::graphics_pipeline_create_status_name(
            vulkan_backend::vulkan_graphics_pipeline_create_status::invalid_depth_state)
            == std::string_view{"invalid_depth_state"},
        "graphics pipeline create status name is stable");
    require(
        vulkan_backend::graphics_pipeline_destroy_status_name(
            vulkan_backend::vulkan_graphics_pipeline_destroy_status::destroy_failed_fatal)
            == std::string_view{"destroy_failed_fatal"},
        "graphics pipeline destroy status name is stable");
}

} // namespace

int main()
{
    test_graphics_pipeline_readiness_creates_and_destroys_fake_pipeline();
    test_graphics_pipeline_readiness_reports_missing_shader_module();
    test_graphics_pipeline_readiness_reports_missing_pipeline_layout();
    test_graphics_pipeline_readiness_reports_incompatible_shader_stages();
    test_graphics_pipeline_readiness_reports_invalid_fixed_function_states();
    test_graphics_pipeline_readiness_maps_recoverable_and_fatal_create_failures();
    test_graphics_pipeline_readiness_names_are_stable();
    return 0;
}
