#include "render/vulkan/vulkan_backend_adapter.h"

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
make_shader_readiness()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_shader_module_factory fake;
    return vulkan_backend::check_vulkan_shader_module_readiness(
        fake.adapter(),
        {
            make_shader_request(vulkan_backend::vulkan_shader_stage::vertex, "quad.vertex"),
            make_shader_request(vulkan_backend::vulkan_shader_stage::fragment, "quad.fragment"),
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_backend_shader_module_readiness_state
make_missing_shader_readiness()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_shader_module_factory fake;
    vulkan_backend::vulkan_shader_module_create_request failed_fragment =
        make_shader_request(vulkan_backend::vulkan_shader_stage::fragment, "quad.fragment");
    failed_fragment.spirv_bytes.clear();
    return vulkan_backend::check_vulkan_shader_module_readiness(
        fake.adapter(),
        {
            make_shader_request(vulkan_backend::vulkan_shader_stage::vertex, "quad.vertex"),
            failed_fragment,
        });
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
                },
            },
        },
        .push_constant_ranges = {},
        .required_shader_stages = {
            vulkan_backend::vulkan_shader_stage::vertex,
            vulkan_backend::vulkan_shader_stage::fragment,
        },
        .max_push_constant_bytes = 128,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_pipeline_layout_create_result make_pipeline_layout(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_shader_module_readiness_state&
        shader_modules)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_pipeline_layout_factory fake;
    return vulkan_backend::create_vulkan_pipeline_layout(
        fake.adapter(),
        shader_modules,
        make_pipeline_layout_request());
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

quiz_vulkan::render::vulkan_backend::vulkan_graphics_pipeline_create_result
make_graphics_pipeline(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_shader_module_readiness_state&
        shader_modules,
    const quiz_vulkan::render::vulkan_backend::vulkan_pipeline_layout_create_result&
        pipeline_layout,
    quiz_vulkan::render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status status =
        quiz_vulkan::render::vulkan_backend::vulkan_graphics_pipeline_adapter_call_status::completed)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_graphics_pipeline_factory fake(
        vulkan_backend::fake_vulkan_graphics_pipeline_factory_options{
            .create_status = status,
        });
    return vulkan_backend::create_vulkan_graphics_pipeline(
        fake.adapter(),
        shader_modules,
        pipeline_layout,
        make_graphics_pipeline_request());
}

void test_pipeline_readiness_summary_reports_all_ready()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_shader_readiness();
    const vulkan_backend::vulkan_pipeline_layout_create_result pipeline_layout =
        make_pipeline_layout(shader_modules);
    const vulkan_backend::vulkan_graphics_pipeline_create_result graphics_pipeline =
        make_graphics_pipeline(shader_modules, pipeline_layout);

    const vulkan_backend::vulkan_backend_pipeline_readiness_summary summary =
        vulkan_backend::summarize_vulkan_pipeline_readiness(
            shader_modules,
            pipeline_layout,
            graphics_pipeline);

    require(summary.checked, "pipeline readiness summary is checked");
    require(
        summary.status == vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::ready,
        "pipeline readiness summary reports ready");
    require(summary.completed(), "pipeline readiness summary completes when all readiness layers pass");
    require(summary.shader_modules_ready, "pipeline readiness summary reports shader modules ready");
    require(summary.pipeline_layout_ready, "pipeline readiness summary reports pipeline layout ready");
    require(summary.graphics_pipeline_ready, "pipeline readiness summary reports graphics pipeline ready");
    require(summary.requested_shader_module_count == 2, "pipeline readiness summary counts shader requests");
    require(summary.created_shader_module_count == 2, "pipeline readiness summary counts created shaders");
    require(summary.graphics_pipeline_status == vulkan_backend::vulkan_graphics_pipeline_create_status::created,
        "pipeline readiness summary keeps graphics pipeline status");
    require(
        summary.diagnostic == "Vulkan pipeline readiness summary is ready",
        "pipeline readiness summary ready diagnostic is stable");

    vulkan_backend::vulkan_backend_pipeline_state pipeline_state;
    pipeline_state.shader_modules = shader_modules;
    pipeline_state.pipeline_layout = pipeline_layout;
    pipeline_state.graphics_pipeline = graphics_pipeline;
    pipeline_state.pipeline_readiness_summary = summary;
    require(
        pipeline_state.pipeline_readiness_summary.completed(),
        "backend pipeline state exposes readiness summary");
}

void test_pipeline_readiness_summary_reports_missing_shader()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_missing_shader_readiness();
    const vulkan_backend::vulkan_backend_pipeline_readiness_summary summary =
        vulkan_backend::summarize_vulkan_pipeline_readiness(
            shader_modules,
            vulkan_backend::vulkan_pipeline_layout_create_result{},
            vulkan_backend::vulkan_graphics_pipeline_create_result{});

    require(
        summary.status
            == vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::shader_module_unavailable,
        "pipeline readiness summary reports shader module unavailable");
    require(!summary.completed(), "missing shader summary does not complete");
    require(summary.failed_shader_module_count == 1, "pipeline readiness summary counts failed shader");
    require(
        summary.diagnostic
            == "Vulkan pipeline readiness summary shader modules are unavailable",
        "missing shader summary diagnostic is stable");
}

void test_pipeline_readiness_summary_reports_missing_layout()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_shader_readiness();
    const vulkan_backend::vulkan_backend_pipeline_readiness_summary summary =
        vulkan_backend::summarize_vulkan_pipeline_readiness(
            shader_modules,
            vulkan_backend::vulkan_pipeline_layout_create_result{
                .checked = true,
                .status = vulkan_backend::vulkan_pipeline_layout_create_status::invalid_binding,
                .diagnostic = "pipeline layout invalid for summary test",
            },
            vulkan_backend::vulkan_graphics_pipeline_create_result{});

    require(
        summary.status
            == vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::pipeline_layout_unavailable,
        "pipeline readiness summary reports pipeline layout unavailable");
    require(!summary.completed(), "missing layout summary does not complete");
    require(summary.pipeline_layout_checked, "missing layout summary records layout checked");
    require(!summary.pipeline_layout_ready, "missing layout summary records layout not ready");
    require(
        summary.pipeline_layout_status
            == vulkan_backend::vulkan_pipeline_layout_create_status::invalid_binding,
        "missing layout summary records layout status");
    require(
        summary.diagnostic == "pipeline layout invalid for summary test",
        "missing layout summary carries layout diagnostic");
}

void test_pipeline_readiness_summary_reports_graphics_create_failure()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_shader_readiness();
    const vulkan_backend::vulkan_pipeline_layout_create_result pipeline_layout =
        make_pipeline_layout(shader_modules);
    const vulkan_backend::vulkan_graphics_pipeline_create_result graphics_pipeline =
        make_graphics_pipeline(
            shader_modules,
            pipeline_layout,
            vulkan_backend::vulkan_graphics_pipeline_adapter_call_status::failed_recoverable);

    const vulkan_backend::vulkan_backend_pipeline_readiness_summary summary =
        vulkan_backend::summarize_vulkan_pipeline_readiness(
            shader_modules,
            pipeline_layout,
            graphics_pipeline);

    require(
        summary.status
            == vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::graphics_pipeline_unavailable,
        "pipeline readiness summary reports graphics pipeline unavailable");
    require(!summary.completed(), "graphics create failure summary does not complete");
    require(summary.graphics_pipeline_checked, "graphics create failure summary records graphics checked");
    require(summary.graphics_create_recoverable_failure, "graphics create failure summary records recoverable failure");
    require(!summary.graphics_create_fatal_failure, "graphics create failure summary does not record fatal failure");
    require(
        summary.graphics_pipeline_status
            == vulkan_backend::vulkan_graphics_pipeline_create_status::create_failed_recoverable,
        "graphics create failure summary records graphics status");
}

void test_pipeline_readiness_summary_reflects_destroy_accounting()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_shader_module_readiness_state shader_modules =
        make_shader_readiness();
    const vulkan_backend::vulkan_pipeline_layout_create_result pipeline_layout =
        make_pipeline_layout(shader_modules);
    const vulkan_backend::vulkan_graphics_pipeline_create_result graphics_pipeline =
        make_graphics_pipeline(shader_modules, pipeline_layout);

    vulkan_backend::fake_vulkan_pipeline_layout_factory layout_fake;
    const vulkan_backend::vulkan_pipeline_layout_destroy_result layout_destroy =
        vulkan_backend::destroy_vulkan_pipeline_layout(layout_fake.adapter(), pipeline_layout);
    vulkan_backend::fake_vulkan_graphics_pipeline_factory graphics_fake;
    const vulkan_backend::vulkan_graphics_pipeline_destroy_result graphics_destroy =
        vulkan_backend::destroy_vulkan_graphics_pipeline(
            graphics_fake.adapter(),
            graphics_pipeline.pipeline);

    const vulkan_backend::vulkan_backend_pipeline_readiness_summary summary =
        vulkan_backend::summarize_vulkan_pipeline_readiness(
            shader_modules,
            pipeline_layout,
            graphics_pipeline,
            layout_destroy,
            graphics_destroy);

    require(summary.completed(), "destroy accounting summary still completes for ready pipeline");
    require(summary.pipeline_layout_destroy_checked, "summary records pipeline layout destroy checked");
    require(summary.pipeline_layout_destroyed, "summary records pipeline layout destroyed");
    require(
        summary.descriptor_set_layout_destroyed_count == 1,
        "summary records descriptor set layout destroy count");
    require(
        summary.pipeline_layout_destroy_status
            == vulkan_backend::vulkan_pipeline_layout_destroy_status::destroyed,
        "summary records pipeline layout destroy status");
    require(summary.graphics_pipeline_destroy_checked, "summary records graphics destroy checked");
    require(summary.graphics_pipeline_destroyed, "summary records graphics pipeline destroyed");
    require(
        summary.graphics_pipeline_destroyed_count == 1,
        "summary records graphics pipeline destroy count");
    require(
        summary.graphics_pipeline_destroy_status
            == vulkan_backend::vulkan_graphics_pipeline_destroy_status::destroyed,
        "summary records graphics pipeline destroy status");
}

void test_pipeline_readiness_summary_names_are_stable()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    require(
        vulkan_backend::pipeline_readiness_summary_status_name(
            vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::ready)
            == std::string_view{"ready"},
        "pipeline readiness summary ready status name is stable");
    require(
        vulkan_backend::pipeline_readiness_summary_status_name(
            vulkan_backend::vulkan_backend_pipeline_readiness_summary_status::graphics_pipeline_unavailable)
            == std::string_view{"graphics_pipeline_unavailable"},
        "pipeline readiness summary graphics unavailable status name is stable");
}

} // namespace

int main()
{
    test_pipeline_readiness_summary_reports_all_ready();
    test_pipeline_readiness_summary_reports_missing_shader();
    test_pipeline_readiness_summary_reports_missing_layout();
    test_pipeline_readiness_summary_reports_graphics_create_failure();
    test_pipeline_readiness_summary_reflects_destroy_accounting();
    test_pipeline_readiness_summary_names_are_stable();
    return 0;
}
