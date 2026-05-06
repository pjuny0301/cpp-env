#include "render/vulkan/vulkan_backend_adapter.h"
#include "render/vulkan/vulkan_frame_plan.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::render::render_draw_command make_quad_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    quiz_vulkan::render::render_color color)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::quad,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "test", .color = color},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
}

quiz_vulkan::render::render_draw_command make_text_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    std::string text)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::text,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "white", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {render_text_run{.text = std::move(text), .style_token = "title"}},
        .image = {},
    };
}

quiz_vulkan::render::render_draw_command make_image_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds,
    std::string uri)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::image,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "image", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = render_image_ref{.uri = std::move(uri), .alt_text = "fixture", .aspect_ratio = 1.0f},
    };
}

void test_vulkan_shader_stage_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::shader_stage_name(vulkan_backend::vulkan_shader_stage::vertex)
            == std::string_view{"vertex"},
        "shader stage name for vertex is stable");
    require(
        vulkan_backend::shader_stage_name(vulkan_backend::vulkan_shader_stage::fragment)
            == std::string_view{"fragment"},
        "shader stage name for fragment is stable");
}

void test_vulkan_pipeline_lifecycle_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::pipeline_lifecycle_stage_name(
            vulkan_backend::vulkan_pipeline_lifecycle_stage::render_pass)
            == std::string_view{"render_pass"},
        "pipeline lifecycle stage name for render pass is stable");
    require(
        vulkan_backend::pipeline_lifecycle_stage_name(
            vulkan_backend::vulkan_pipeline_lifecycle_stage::shader_stages)
            == std::string_view{"shader_stages"},
        "pipeline lifecycle stage name for shader stages is stable");
    require(
        vulkan_backend::pipeline_lifecycle_stage_name(
            vulkan_backend::vulkan_pipeline_lifecycle_stage::pipeline)
            == std::string_view{"pipeline"},
        "pipeline lifecycle stage name for pipeline is stable");
    require(
        vulkan_backend::pipeline_lifecycle_status_name(
            vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked)
            == std::string_view{"not_checked"},
        "pipeline lifecycle status name for not checked is stable");
    require(
        vulkan_backend::pipeline_lifecycle_status_name(
            vulkan_backend::vulkan_pipeline_lifecycle_status::ready)
            == std::string_view{"ready"},
        "pipeline lifecycle status name for ready is stable");
    require(
        vulkan_backend::pipeline_lifecycle_status_name(
            vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable)
            == std::string_view{"unavailable"},
        "pipeline lifecycle status name for unavailable is stable");
}

void test_vulkan_diagnostic_pipeline_cache_reports_batch_capabilities()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{40.0f, 40.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache;
    require(cache.ensure_pipeline(plan.batches[0]), "diagnostic pipeline cache provides quad pipeline");
    require(cache.ensure_pipeline(plan.batches[1]), "diagnostic pipeline cache provides image pipeline");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.cache_checked, "pipeline cache records that pipelines were checked");
    require(state.ready, "pipeline cache remains ready when all requested pipelines exist");
    require(state.completed(), "pipeline cache reports completed state when all requested pipelines exist");
    require(!state.missing_pipeline, "pipeline cache reports no missing pipeline");
    require(state.requested_pipeline_count == 2, "pipeline cache records requested pipeline count");
    require(state.capabilities.size() == 4, "pipeline cache exposes one capability per batch kind");
    require(state.cache_entries.size() == 4, "pipeline cache exposes one cache entry per batch kind");
    require(state.pipeline_descriptors.size() == 4, "pipeline cache exposes one descriptor per batch kind");
    require(state.shader_registry.registered_shader_count == 8, "pipeline cache registers vertex and fragment shaders");
    require(state.shader_registry.registry_checked, "pipeline cache verifies shaders while checking pipelines");
    require(!state.shader_registry.missing_shader, "pipeline cache reports no missing shader on happy path");
    require(state.lifecycle.checked, "pipeline lifecycle is checked");
    require(state.lifecycle.render_pass.valid(), "pipeline lifecycle has a valid render pass descriptor");
    require(state.lifecycle.render_pass_ready(), "pipeline lifecycle render pass is ready");
    require(state.lifecycle.completed(), "pipeline lifecycle completes when requested pipelines are ready");
    require(state.compatibility.checked, "pipeline compatibility summary is checked");
    require(state.compatibility.completed(), "pipeline compatibility summary completes");
    require(state.compatibility.requested_key_count == 2, "pipeline compatibility records requested key count");
    require(state.compatibility.compatible_key_count == 2, "pipeline compatibility records compatible key count");
    require(state.compatibility.incompatible_key_count == 0, "pipeline compatibility records no incompatible keys");
    require(state.compatibility.unique_key_count == 2, "pipeline compatibility records unique key count");
    require(state.compatibility.keys.size() == 2, "pipeline compatibility stores one key per request");
    require(
        state.compatibility.keys[0].batch_kind == vulkan_backend::vulkan_batch_kind::quad,
        "pipeline compatibility first key records quad batch");
    require(
        state.compatibility.keys[0].color_attachment_count == 1,
        "pipeline compatibility first key records color attachment count");
    require(!state.compatibility.keys[0].has_depth_attachment, "pipeline compatibility first key records no depth");
    require(state.compatibility.keys[0].surface_compatible, "pipeline compatibility first key records surface compatibility");
    require(state.compatibility.keys[0].vertex_shader.value == "quad.vertex", "pipeline compatibility first key records vertex shader");
    require(
        state.compatibility.keys[0].fragment_shader.value == "quad.fragment",
        "pipeline compatibility first key records fragment shader");
    require(state.compatibility.keys[0].compatible(), "pipeline compatibility first key is compatible");
    require(
        state.compatibility.keys[1].batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "pipeline compatibility second key records image batch");
    require(state.compatibility.keys[1].vertex_shader.value == "image.vertex", "pipeline compatibility second key records vertex shader");
    require(
        state.compatibility.keys[1].fragment_shader.value == "image.fragment",
        "pipeline compatibility second key records fragment shader");
    require(state.compatibility.keys[1].compatible(), "pipeline compatibility second key is compatible");
    require(state.shader_bindings.checked, "shader binding readiness is checked");
    require(state.shader_bindings.completed(), "shader binding readiness completes");
    require(state.shader_bindings.requested_binding_count == 4, "shader binding readiness records requested bindings");
    require(state.shader_bindings.ready_binding_count == 4, "shader binding readiness records ready bindings");
    require(state.shader_bindings.missing_binding_count == 0, "shader binding readiness records no missing bindings");
    require(state.shader_bindings.bindings.size() == 4, "shader binding readiness stores stage bindings");
    require(
        state.shader_bindings.bindings[0].stage == vulkan_backend::vulkan_shader_stage::vertex,
        "shader binding readiness first binding is vertex");
    require(
        state.shader_bindings.bindings[0].shader_id.value == "quad.vertex",
        "shader binding readiness first binding records quad vertex shader");
    require(state.shader_bindings.bindings[0].completed(), "shader binding readiness first binding completes");
    require(
        state.shader_bindings.bindings[1].stage == vulkan_backend::vulkan_shader_stage::fragment,
        "shader binding readiness second binding is fragment");
    require(
        state.shader_bindings.bindings[1].shader_id.value == "quad.fragment",
        "shader binding readiness second binding records quad fragment shader");
    require(state.shader_bindings.bindings[1].completed(), "shader binding readiness second binding completes");
    require(
        state.shader_bindings.bindings[2].shader_id.value == "image.vertex",
        "shader binding readiness third binding records image vertex shader");
    require(
        state.shader_bindings.bindings[3].shader_id.value == "image.fragment",
        "shader binding readiness fourth binding records image fragment shader");
    require(!state.lifecycle.missing_render_pass, "pipeline lifecycle reports no missing render pass");
    require(!state.lifecycle.missing_shader_stage, "pipeline lifecycle reports no missing shader stage");
    require(!state.lifecycle.missing_pipeline, "pipeline lifecycle reports no missing pipeline");
    require(state.lifecycle.requested_pipeline_count == 2, "pipeline lifecycle records requested pipeline count");
    require(state.lifecycle.pipeline_snapshots.size() == 2, "pipeline lifecycle stores one snapshot per request");
    require(state.lifecycle.shader_stage_snapshots.size() == 2, "pipeline lifecycle stores one shader snapshot per request");
    require(
        state.descriptor_for(vulkan_backend::vulkan_batch_kind::quad) != nullptr,
        "pipeline cache exposes quad descriptor");
    require(
        state.descriptor_for(vulkan_backend::vulkan_batch_kind::image) != nullptr,
        "pipeline cache exposes image descriptor");
    require(state.supports(vulkan_backend::vulkan_batch_kind::quad), "pipeline cache supports quad pipeline");
    require(state.supports(vulkan_backend::vulkan_batch_kind::text), "pipeline cache supports text pipeline");
    require(state.supports(vulkan_backend::vulkan_batch_kind::image), "pipeline cache supports image pipeline");
    require(state.supports(vulkan_backend::vulkan_batch_kind::debug_bounds), "pipeline cache supports debug pipeline");

    require(state.cache_entries[0].kind == vulkan_backend::vulkan_batch_kind::quad, "first pipeline cache entry is quad");
    require(state.cache_entries[0].requested, "quad pipeline cache entry is requested");
    require(state.cache_entries[0].request_count == 1, "quad pipeline cache entry records request count");
    require(state.cache_entries[0].last_command_index == 0, "quad pipeline cache entry records source command index");
    require(state.cache_entries[1].kind == vulkan_backend::vulkan_batch_kind::text, "second pipeline cache entry is text");
    require(!state.cache_entries[1].requested, "text pipeline cache entry is not requested");
    require(state.cache_entries[2].kind == vulkan_backend::vulkan_batch_kind::image, "third pipeline cache entry is image");
    require(state.cache_entries[2].requested, "image pipeline cache entry is requested");
    require(state.cache_entries[2].request_count == 1, "image pipeline cache entry records request count");
    require(state.cache_entries[2].last_command_index == 1, "image pipeline cache entry records source command index");
    require(
        state.cache_entries[3].kind == vulkan_backend::vulkan_batch_kind::debug_bounds,
        "fourth pipeline cache entry is debug bounds");
    require(!state.cache_entries[3].requested, "debug pipeline cache entry is not requested");

    const vulkan_backend::vulkan_pipeline_lifecycle_snapshot& quad_lifecycle =
        state.lifecycle.pipeline_snapshots[0];
    require(quad_lifecycle.batch_kind == vulkan_backend::vulkan_batch_kind::quad, "first lifecycle request is quad");
    require(quad_lifecycle.command_index == 0, "quad lifecycle snapshot records command index");
    require(quad_lifecycle.completed(), "quad lifecycle snapshot completes");
    require(
        quad_lifecycle.render_pass_status == vulkan_backend::vulkan_pipeline_lifecycle_status::ready,
        "quad lifecycle render pass is ready");
    require(
        quad_lifecycle.shader_stage_status == vulkan_backend::vulkan_pipeline_lifecycle_status::ready,
        "quad lifecycle shader stages are ready");
    require(
        quad_lifecycle.pipeline_status == vulkan_backend::vulkan_pipeline_lifecycle_status::ready,
        "quad lifecycle pipeline is ready");

    const vulkan_backend::vulkan_pipeline_shader_stage_snapshot& quad_shaders =
        state.lifecycle.shader_stage_snapshots[0];
    require(quad_shaders.completed(), "quad shader lifecycle snapshot completes");
    require(quad_shaders.vertex_shader.value == "quad.vertex", "quad shader lifecycle records vertex shader id");
    require(quad_shaders.fragment_shader.value == "quad.fragment", "quad shader lifecycle records fragment shader id");
    require(quad_shaders.vertex_stage_ready, "quad vertex stage is ready");
    require(quad_shaders.fragment_stage_ready, "quad fragment stage is ready");

    const vulkan_backend::vulkan_pipeline_lifecycle_snapshot& image_lifecycle =
        state.lifecycle.pipeline_snapshots[1];
    require(image_lifecycle.batch_kind == vulkan_backend::vulkan_batch_kind::image, "second lifecycle request is image");
    require(image_lifecycle.command_index == 1, "image lifecycle snapshot records command index");
    require(image_lifecycle.completed(), "image lifecycle snapshot completes");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_pipeline()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{40.0f, 40.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .default_available = true,
            .overrides = {vulkan_backend::vulkan_pipeline_capability{
                .kind = vulkan_backend::vulkan_batch_kind::image,
                .available = false,
            }},
        });

    require(cache.ensure_pipeline(plan.batches[0]), "pipeline cache provides available quad pipeline");
    require(!cache.ensure_pipeline(plan.batches[1]), "pipeline cache reports missing image pipeline");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.cache_checked, "missing pipeline cache records that pipelines were checked");
    require(!state.ready, "missing pipeline cache is not ready");
    require(!state.completed(), "missing pipeline cache does not complete");
    require(state.missing_pipeline, "missing pipeline cache records missing pipeline");
    require(
        state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "missing pipeline cache records missing image pipeline");
    require(state.missing_command_index == 1, "missing pipeline cache records source command index");
    require(state.requested_pipeline_count == 2, "missing pipeline cache records request count through failure");
    require(state.supports(vulkan_backend::vulkan_batch_kind::quad), "missing pipeline cache still supports quad");
    require(!state.supports(vulkan_backend::vulkan_batch_kind::image), "missing pipeline cache reports unsupported image");
    require(state.cache_entries[0].requested, "missing pipeline cache records quad request");
    require(state.cache_entries[2].requested, "missing pipeline cache records image request");
    require(!state.cache_entries[2].available, "missing pipeline cache entry records unavailable image pipeline");
    require(state.cache_entries[2].last_command_index == 1, "missing pipeline cache entry records failed command index");
    require(state.lifecycle.checked, "missing pipeline lifecycle is checked");
    require(!state.lifecycle.completed(), "missing pipeline lifecycle does not complete");
    require(!state.lifecycle.missing_render_pass, "missing pipeline lifecycle keeps render pass ready");
    require(!state.lifecycle.missing_shader_stage, "missing pipeline lifecycle does not check shaders");
    require(state.lifecycle.missing_pipeline, "missing pipeline lifecycle records pipeline failure");
    require(
        state.lifecycle.failed_stage == vulkan_backend::vulkan_pipeline_lifecycle_stage::pipeline,
        "missing pipeline lifecycle identifies pipeline stage");
    require(
        state.lifecycle.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "missing pipeline lifecycle records image batch");
    require(state.lifecycle.missing_command_index == 1, "missing pipeline lifecycle records command index");
    require(state.lifecycle.pipeline_snapshots.size() == 2, "missing pipeline lifecycle stores snapshots through failure");
    require(state.lifecycle.pipeline_snapshots[0].completed(), "missing pipeline lifecycle preserves completed quad snapshot");
    require(
        state.lifecycle.pipeline_snapshots[1].pipeline_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable,
        "missing pipeline lifecycle marks image pipeline unavailable");
    require(
        state.lifecycle.pipeline_snapshots[1].failed_stage
            == vulkan_backend::vulkan_pipeline_lifecycle_stage::pipeline,
        "missing pipeline lifecycle snapshot records failed stage");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_unavailable_render_pass()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .render_pass = vulkan_backend::vulkan_render_pass_descriptor{
                .color_attachment_count = 0,
                .has_depth_attachment = false,
                .surface_compatible = true,
            },
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports unavailable render pass");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.cache_checked, "render pass failure checks pipeline cache");
    require(!state.ready, "render pass failure marks pipeline cache not ready");
    require(!state.completed(), "render pass failure does not complete pipeline cache");
    require(state.missing_pipeline, "render pass failure marks pipeline unavailable");
    require(state.lifecycle.checked, "render pass lifecycle is checked");
    require(state.compatibility.checked, "render pass failure checks pipeline compatibility");
    require(!state.compatibility.completed(), "render pass failure compatibility does not complete");
    require(state.compatibility.requested_key_count == 1, "render pass failure records one compatibility key");
    require(state.compatibility.compatible_key_count == 0, "render pass failure records no compatible keys");
    require(state.compatibility.incompatible_key_count == 1, "render pass failure records incompatible key");
    require(state.compatibility.unique_key_count == 1, "render pass failure records one unique key");
    require(state.compatibility.keys.size() == 1, "render pass failure stores compatibility key");
    require(!state.compatibility.keys.front().compatible(), "render pass failure key is incompatible");
    require(
        state.compatibility.keys.front().color_attachment_count == 0,
        "render pass failure compatibility key records missing color attachment");
    require(state.shader_bindings.checked, "render pass failure initializes shader binding readiness");
    require(state.shader_bindings.requested_binding_count == 0, "render pass failure performs no shader binding checks");
    require(state.shader_bindings.bindings.empty(), "render pass failure stores no shader bindings");
    require(!state.lifecycle.render_pass.valid(), "render pass lifecycle records invalid descriptor");
    require(!state.lifecycle.render_pass_ready(), "render pass lifecycle is not ready");
    require(state.lifecycle.missing_render_pass, "render pass lifecycle records missing render pass");
    require(!state.lifecycle.missing_shader_stage, "render pass lifecycle does not check shaders");
    require(!state.lifecycle.missing_pipeline, "render pass lifecycle failure is not a pipeline-object miss");
    require(
        state.lifecycle.failed_stage == vulkan_backend::vulkan_pipeline_lifecycle_stage::render_pass,
        "render pass lifecycle identifies render pass stage");
    require(state.lifecycle.missing_command_index == 0, "render pass lifecycle records command index");
    require(state.lifecycle.requested_pipeline_count == 1, "render pass lifecycle records one request");
    require(state.lifecycle.pipeline_snapshots.size() == 1, "render pass lifecycle stores failed request snapshot");
    require(state.lifecycle.shader_stage_snapshots.empty(), "render pass lifecycle stores no shader snapshot");
    require(
        state.lifecycle.pipeline_snapshots.front().render_pass_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable,
        "render pass lifecycle marks render pass unavailable");
    require(
        state.lifecycle.pipeline_snapshots.front().shader_stage_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked,
        "render pass lifecycle does not check shader stages");
    require(
        state.lifecycle.pipeline_snapshots.front().pipeline_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked,
        "render pass lifecycle does not check graphics pipeline");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_vertex_shader()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .use_default_shader_modules = false,
            .shader_modules = {vulkan_backend::vulkan_shader_module_descriptor{
                .id = vulkan_backend::vulkan_shader_module_id{.value = "quad.fragment"},
                .stage = vulkan_backend::vulkan_shader_stage::fragment,
            }},
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports missing vertex shader");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.missing_pipeline, "missing vertex shader marks pipeline missing");
    require(!state.missing_descriptor, "missing vertex shader keeps descriptor available");
    require(state.missing_shader, "missing vertex shader marks shader missing");
    require(
        state.missing_shader_stage == vulkan_backend::vulkan_shader_stage::vertex,
        "missing vertex shader records vertex stage");
    require(state.missing_shader_id.value == "quad.vertex", "missing vertex shader records shader module id");
    require(state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::quad, "missing vertex shader records batch kind");
    require(state.missing_command_index == 0, "missing vertex shader records command index");
    require(state.shader_registry.registry_checked, "missing vertex shader checks shader registry");
    require(state.shader_registry.missing_shader, "shader registry records missing vertex shader");
    require(
        state.shader_registry.missing_stage == vulkan_backend::vulkan_shader_stage::vertex,
        "shader registry records missing vertex stage");
    require(state.shader_registry.missing_shader_id.value == "quad.vertex", "shader registry records vertex id");
    require(state.compatibility.checked, "missing vertex shader checks pipeline compatibility");
    require(state.compatibility.completed(), "missing vertex shader keeps compatibility key complete");
    require(state.compatibility.requested_key_count == 1, "missing vertex shader records one compatibility key");
    require(state.compatibility.compatible_key_count == 1, "missing vertex shader compatibility key is structurally compatible");
    require(state.compatibility.keys.front().vertex_shader.value == "quad.vertex", "missing vertex shader compatibility records vertex id");
    require(state.shader_bindings.checked, "missing vertex shader checks shader binding readiness");
    require(!state.shader_bindings.completed(), "missing vertex shader binding readiness does not complete");
    require(state.shader_bindings.requested_binding_count == 1, "missing vertex shader checks one binding");
    require(state.shader_bindings.ready_binding_count == 0, "missing vertex shader has no ready bindings");
    require(state.shader_bindings.missing_binding_count == 1, "missing vertex shader records one missing binding");
    require(state.shader_bindings.bindings.size() == 1, "missing vertex shader stores one binding");
    require(
        state.shader_bindings.bindings.front().stage == vulkan_backend::vulkan_shader_stage::vertex,
        "missing vertex shader binding records vertex stage");
    require(
        state.shader_bindings.bindings.front().shader_id.value == "quad.vertex",
        "missing vertex shader binding records shader id");
    require(state.shader_bindings.bindings.front().registry_checked, "missing vertex shader binding checks registry");
    require(!state.shader_bindings.bindings.front().module_registered, "missing vertex shader binding records missing module");
    require(!state.shader_bindings.bindings.front().completed(), "missing vertex shader binding does not complete");
    require(!state.lifecycle.completed(), "missing vertex shader lifecycle does not complete");
    require(!state.lifecycle.missing_render_pass, "missing vertex shader lifecycle keeps render pass ready");
    require(state.lifecycle.missing_shader_stage, "missing vertex shader lifecycle records shader stage failure");
    require(!state.lifecycle.missing_pipeline, "missing vertex shader lifecycle is not a pipeline-object miss");
    require(
        state.lifecycle.failed_stage == vulkan_backend::vulkan_pipeline_lifecycle_stage::shader_stages,
        "missing vertex shader lifecycle identifies shader stage");
    require(
        state.lifecycle.missing_shader_stage_kind == vulkan_backend::vulkan_shader_stage::vertex,
        "missing vertex shader lifecycle records vertex stage");
    require(state.lifecycle.missing_shader_id.value == "quad.vertex", "missing vertex shader lifecycle records shader id");
    require(state.lifecycle.pipeline_snapshots.size() == 1, "missing vertex shader lifecycle stores request snapshot");
    require(state.lifecycle.shader_stage_snapshots.size() == 1, "missing vertex shader lifecycle stores shader snapshot");
    require(
        state.lifecycle.pipeline_snapshots.front().shader_stage_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable,
        "missing vertex shader lifecycle marks shader stages unavailable");
    require(
        state.lifecycle.pipeline_snapshots.front().pipeline_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked,
        "missing vertex shader lifecycle does not check graphics pipeline");
    require(
        !state.lifecycle.shader_stage_snapshots.front().vertex_stage_ready,
        "missing vertex shader lifecycle records unavailable vertex stage");
    require(
        !state.lifecycle.shader_stage_snapshots.front().fragment_stage_ready,
        "missing vertex shader lifecycle stops before fragment stage");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_fragment_shader()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        "fixture://renderer/image.png"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .use_default_shader_modules = false,
            .shader_modules = {vulkan_backend::vulkan_shader_module_descriptor{
                .id = vulkan_backend::vulkan_shader_module_id{.value = "image.vertex"},
                .stage = vulkan_backend::vulkan_shader_stage::vertex,
            }},
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports missing fragment shader");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.missing_pipeline, "missing fragment shader marks pipeline missing");
    require(!state.missing_descriptor, "missing fragment shader keeps descriptor available");
    require(state.missing_shader, "missing fragment shader marks shader missing");
    require(
        state.missing_shader_stage == vulkan_backend::vulkan_shader_stage::fragment,
        "missing fragment shader records fragment stage");
    require(state.missing_shader_id.value == "image.fragment", "missing fragment shader records shader module id");
    require(state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::image, "missing fragment shader records batch kind");
    require(state.missing_command_index == 0, "missing fragment shader records command index");
    require(state.shader_registry.registry_checked, "missing fragment shader checks shader registry");
    require(state.shader_registry.missing_shader, "shader registry records missing fragment shader");
    require(
        state.shader_registry.missing_stage == vulkan_backend::vulkan_shader_stage::fragment,
        "shader registry records missing fragment stage");
    require(state.shader_registry.missing_shader_id.value == "image.fragment", "shader registry records fragment id");
    require(state.compatibility.checked, "missing fragment shader checks pipeline compatibility");
    require(state.compatibility.completed(), "missing fragment shader keeps compatibility key complete");
    require(state.compatibility.requested_key_count == 1, "missing fragment shader records one compatibility key");
    require(state.compatibility.compatible_key_count == 1, "missing fragment shader compatibility key is structurally compatible");
    require(
        state.compatibility.keys.front().fragment_shader.value == "image.fragment",
        "missing fragment shader compatibility records fragment id");
    require(state.shader_bindings.checked, "missing fragment shader checks shader binding readiness");
    require(!state.shader_bindings.completed(), "missing fragment shader binding readiness does not complete");
    require(state.shader_bindings.requested_binding_count == 2, "missing fragment shader checks two bindings");
    require(state.shader_bindings.ready_binding_count == 1, "missing fragment shader records ready vertex binding");
    require(state.shader_bindings.missing_binding_count == 1, "missing fragment shader records one missing binding");
    require(state.shader_bindings.bindings.size() == 2, "missing fragment shader stores two bindings");
    require(state.shader_bindings.bindings.front().completed(), "missing fragment shader records completed vertex binding");
    require(
        state.shader_bindings.bindings.back().stage == vulkan_backend::vulkan_shader_stage::fragment,
        "missing fragment shader binding records fragment stage");
    require(
        state.shader_bindings.bindings.back().shader_id.value == "image.fragment",
        "missing fragment shader binding records shader id");
    require(state.shader_bindings.bindings.back().registry_checked, "missing fragment shader binding checks registry");
    require(!state.shader_bindings.bindings.back().module_registered, "missing fragment shader binding records missing module");
    require(!state.shader_bindings.bindings.back().completed(), "missing fragment shader binding does not complete");
    require(!state.lifecycle.completed(), "missing fragment shader lifecycle does not complete");
    require(!state.lifecycle.missing_render_pass, "missing fragment shader lifecycle keeps render pass ready");
    require(state.lifecycle.missing_shader_stage, "missing fragment shader lifecycle records shader stage failure");
    require(!state.lifecycle.missing_pipeline, "missing fragment shader lifecycle is not a pipeline-object miss");
    require(
        state.lifecycle.failed_stage == vulkan_backend::vulkan_pipeline_lifecycle_stage::shader_stages,
        "missing fragment shader lifecycle identifies shader stage");
    require(
        state.lifecycle.missing_shader_stage_kind == vulkan_backend::vulkan_shader_stage::fragment,
        "missing fragment shader lifecycle records fragment stage");
    require(
        state.lifecycle.missing_shader_id.value == "image.fragment",
        "missing fragment shader lifecycle records shader id");
    require(state.lifecycle.pipeline_snapshots.size() == 1, "missing fragment shader lifecycle stores request snapshot");
    require(state.lifecycle.shader_stage_snapshots.size() == 1, "missing fragment shader lifecycle stores shader snapshot");
    require(
        state.lifecycle.pipeline_snapshots.front().shader_stage_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable,
        "missing fragment shader lifecycle marks shader stages unavailable");
    require(
        state.lifecycle.pipeline_snapshots.front().pipeline_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked,
        "missing fragment shader lifecycle does not check graphics pipeline");
    require(
        state.lifecycle.shader_stage_snapshots.front().vertex_stage_ready,
        "missing fragment shader lifecycle records ready vertex stage");
    require(
        !state.lifecycle.shader_stage_snapshots.front().fragment_stage_ready,
        "missing fragment shader lifecycle records unavailable fragment stage");
}

void test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_descriptor()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_text_command(
        "text",
        render_rect{10.0f, 10.0f, 20.0f, 20.0f},
        "missing descriptor"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 10,
            .surface_height = 10,
        });

    vulkan_backend::diagnostic_vulkan_pipeline_cache cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .use_default_pipeline_descriptors = false,
            .pipeline_descriptors = {vulkan_backend::vulkan_pipeline_descriptor{
                .kind = vulkan_backend::vulkan_batch_kind::quad,
                .vertex_shader = vulkan_backend::vulkan_shader_module_id{.value = "quad.vertex"},
                .fragment_shader = vulkan_backend::vulkan_shader_module_id{.value = "quad.fragment"},
            }},
        });

    require(!cache.ensure_pipeline(plan.batches.front()), "pipeline cache reports missing text descriptor");

    const vulkan_backend::vulkan_backend_pipeline_state& state = cache.pipeline_state();
    require(state.missing_pipeline, "missing descriptor marks pipeline missing");
    require(state.missing_descriptor, "missing descriptor flag is set");
    require(!state.missing_shader, "missing descriptor does not report missing shader");
    require(
        state.missing_batch_kind == vulkan_backend::vulkan_batch_kind::text,
        "missing descriptor records text batch kind");
    require(state.missing_command_index == 0, "missing descriptor records command index");
    require(state.pipeline_descriptors.size() == 1, "pipeline cache uses injected descriptor set");
    require(
        state.descriptor_for(vulkan_backend::vulkan_batch_kind::text) == nullptr,
        "pipeline cache exposes no text descriptor");
    require(!state.shader_registry.registry_checked, "missing descriptor stops before shader registry lookup");
    require(!state.lifecycle.completed(), "missing descriptor lifecycle does not complete");
    require(!state.lifecycle.missing_render_pass, "missing descriptor lifecycle keeps render pass ready");
    require(!state.lifecycle.missing_shader_stage, "missing descriptor lifecycle does not check shaders");
    require(state.lifecycle.missing_pipeline, "missing descriptor lifecycle records pipeline-stage failure");
    require(
        state.lifecycle.failed_stage == vulkan_backend::vulkan_pipeline_lifecycle_stage::pipeline,
        "missing descriptor lifecycle identifies pipeline stage");
    require(state.lifecycle.pipeline_snapshots.size() == 1, "missing descriptor lifecycle stores request snapshot");
    require(state.lifecycle.shader_stage_snapshots.empty(), "missing descriptor lifecycle stores no shader snapshot");
    require(
        state.lifecycle.pipeline_snapshots.front().render_pass_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::ready,
        "missing descriptor lifecycle records ready render pass");
    require(
        state.lifecycle.pipeline_snapshots.front().shader_stage_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::not_checked,
        "missing descriptor lifecycle does not check shader stages");
    require(
        state.lifecycle.pipeline_snapshots.front().pipeline_status
            == vulkan_backend::vulkan_pipeline_lifecycle_status::unavailable,
        "missing descriptor lifecycle marks pipeline unavailable");
}

} // namespace

int main()
{
    test_vulkan_shader_stage_names_are_stable();
    test_vulkan_pipeline_lifecycle_names_are_stable();
    test_vulkan_diagnostic_pipeline_cache_reports_batch_capabilities();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_pipeline();
    test_vulkan_diagnostic_pipeline_cache_identifies_unavailable_render_pass();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_vertex_shader();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_fragment_shader();
    test_vulkan_diagnostic_pipeline_cache_identifies_missing_batch_descriptor();
    return 0;
}
