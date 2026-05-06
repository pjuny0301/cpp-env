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

const quiz_vulkan::render::vulkan_backend::vulkan_resource_registry_entry* find_registry_resource(
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_resource_registry_state& state,
    quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_kind kind,
    std::string_view resource_id)
{
    for (const quiz_vulkan::render::vulkan_backend::vulkan_resource_registry_entry& entry : state.resources) {
        if (entry.kind == kind && entry.resource_id == resource_id) {
            return &entry;
        }
    }
    return nullptr;
}

void test_vulkan_resource_binding_state_builds_batch_snapshots()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{20.0f, 0.0f, 20.0f, 20.0f},
        "fixture://renderer/card.png"));
    draw_list.commands.push_back(make_text_command(
        "text",
        render_rect{40.0f, 0.0f, 40.0f, 20.0f},
        "bindings"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 100,
            .surface_height = 100,
        });
    const vulkan_backend::vulkan_backend_resource_binding_state state =
        vulkan_backend::build_vulkan_resource_binding_state(draw_list, plan);

    require(state.checked, "resource binding state is checked");
    require(!state.missing_resource, "resource binding state reports no missing resources");
    require(state.completed(), "resource binding state completes for quad, image, and text resources");
    require(state.planned_batch_count == 3, "resource binding state tracks planned batch count");
    require(state.descriptor_set_count == 3, "resource binding state creates one descriptor set per batch");
    require(state.binding_count == 8, "resource binding state has stable aggregate bind count");
    require(state.batch_snapshots.size() == 3, "resource binding state stores one snapshot per batch");
    require(state.descriptor_validation.checked, "resource binding state checks descriptor validation");
    require(state.descriptor_validation.completed(), "descriptor validation completes for valid bindings");
    require(!state.descriptor_validation.missing_required_resource, "descriptor validation reports no missing resources");
    require(!state.descriptor_validation.duplicate_binding, "descriptor validation reports no duplicate bindings");
    require(!state.descriptor_validation.invalid_layout, "descriptor validation reports no invalid layouts");
    require(state.descriptor_validation.planned_batch_count == 3, "descriptor validation tracks planned batch count");
    require(state.descriptor_validation.descriptor_set_count == 3, "descriptor validation counts descriptor sets");
    require(
        state.descriptor_validation.valid_descriptor_set_count == 3,
        "descriptor validation counts valid descriptor sets");
    require(
        state.descriptor_validation.invalid_descriptor_set_count == 0,
        "descriptor validation counts no invalid descriptor sets");
    require(state.descriptor_validation.requested_binding_count == 8, "descriptor validation counts requested bindings");
    require(state.descriptor_validation.valid_binding_count == 8, "descriptor validation counts valid bindings");
    require(state.descriptor_validation.invalid_binding_count == 0, "descriptor validation counts no invalid bindings");
    require(state.descriptor_validation.descriptor_sets.size() == 3, "descriptor validation stores one set per batch");

    const vulkan_backend::vulkan_batch_resource_binding_snapshot& quad = state.batch_snapshots[0];
    require(quad.batch_kind == vulkan_backend::vulkan_batch_kind::quad, "first binding snapshot is quad");
    require(quad.command_index == 0, "quad binding snapshot keeps command index");
    require(quad.descriptor_set_count == 1, "quad binding snapshot has one descriptor set");
    require(quad.binding_count == 2, "quad binding snapshot has stable bind count");
    require(quad.completed(), "quad binding snapshot completes");
    require(
        quad.bindings[0].kind == vulkan_backend::vulkan_resource_binding_kind::batch_uniform,
        "quad binding snapshot binds batch uniform first");
    require(
        quad.bindings[1].kind == vulkan_backend::vulkan_resource_binding_kind::quad_vertex_buffer,
        "quad binding snapshot binds quad vertices second");
    require(quad.bindings[0].resource_id == "batch_uniform:quad", "quad uniform resource id is stable");
    require(quad.bindings[1].resource_id == "quad_vertices:quad", "quad vertex resource id is stable");

    const vulkan_backend::vulkan_descriptor_set_validation_snapshot& quad_descriptor =
        state.descriptor_validation.descriptor_sets[0];
    require(quad_descriptor.batch_kind == vulkan_backend::vulkan_batch_kind::quad, "first descriptor set is quad");
    require(quad_descriptor.command_index == 0, "quad descriptor validation keeps command index");
    require(quad_descriptor.expected_binding_count == 2, "quad descriptor expects two bindings");
    require(quad_descriptor.actual_binding_count == 2, "quad descriptor validates two actual bindings");
    require(quad_descriptor.completed(), "quad descriptor validation completes");
    require(
        quad_descriptor.status == vulkan_backend::vulkan_descriptor_validation_status::valid,
        "quad descriptor validation status is valid");
    require(quad_descriptor.bindings[0].binding_index_matches_order, "quad uniform binding order is valid");
    require(quad_descriptor.bindings[1].binding_index_matches_order, "quad vertex binding order is valid");
    require(quad_descriptor.bindings[1].completed(), "quad vertex descriptor binding completes");

    const vulkan_backend::vulkan_batch_resource_binding_snapshot& image = state.batch_snapshots[1];
    require(image.batch_kind == vulkan_backend::vulkan_batch_kind::image, "second binding snapshot is image");
    require(image.binding_count == 3, "image binding snapshot has stable bind count");
    require(image.completed(), "image binding snapshot completes");
    require(
        image.bindings[1].kind == vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "image binding snapshot binds texture resource");
    require(
        image.bindings[2].kind == vulkan_backend::vulkan_resource_binding_kind::image_sampler,
        "image binding snapshot binds sampler resource");
    require(
        image.bindings[1].resource_id == "fixture://renderer/card.png",
        "image binding snapshot uses image URI as texture resource");
    require(
        state.descriptor_validation.descriptor_sets[1].expected_binding_count == 3,
        "image descriptor validation expects texture and sampler bindings");
    require(
        state.descriptor_validation.descriptor_sets[1].bindings[2].kind
            == vulkan_backend::vulkan_resource_binding_kind::image_sampler,
        "image descriptor validation records sampler binding");

    const vulkan_backend::vulkan_batch_resource_binding_snapshot& text = state.batch_snapshots[2];
    require(text.batch_kind == vulkan_backend::vulkan_batch_kind::text, "third binding snapshot is text");
    require(text.binding_count == 3, "text binding snapshot has stable bind count");
    require(text.completed(), "text binding snapshot completes");
    require(
        text.bindings[1].kind == vulkan_backend::vulkan_resource_binding_kind::text_run_buffer,
        "text binding snapshot binds text run buffer");
    require(
        text.bindings[2].kind == vulkan_backend::vulkan_resource_binding_kind::text_glyph_atlas,
        "text binding snapshot binds glyph atlas");
    require(text.bindings[1].resource_id == "text_runs:text", "text run resource id is stable");
    require(text.bindings[2].resource_id == "glyph_atlas:title", "text glyph atlas resource id is stable");
    require(
        state.descriptor_validation.descriptor_sets[2].bindings[2].kind
            == vulkan_backend::vulkan_resource_binding_kind::text_glyph_atlas,
        "text descriptor validation records glyph atlas binding");
}

void test_vulkan_resource_registry_state_tracks_reuse_counts()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 20.0f, 20.0f},
        render_color{1.0f, 0.0f, 0.0f, 1.0f}));
    draw_list.commands.push_back(make_image_command(
        "image_a",
        render_rect{20.0f, 0.0f, 20.0f, 20.0f},
        "fixture://renderer/shared.png"));
    draw_list.commands.push_back(make_image_command(
        "image_b",
        render_rect{40.0f, 0.0f, 20.0f, 20.0f},
        "fixture://renderer/shared.png"));
    draw_list.commands.push_back(make_text_command(
        "text_a",
        render_rect{0.0f, 24.0f, 40.0f, 20.0f},
        "first"));
    draw_list.commands.push_back(make_text_command(
        "text_b",
        render_rect{40.0f, 24.0f, 40.0f, 20.0f},
        "second"));

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 100,
            .surface_height = 100,
        });
    const vulkan_backend::vulkan_backend_resource_binding_state bindings =
        vulkan_backend::build_vulkan_resource_binding_state(draw_list, plan);
    const vulkan_backend::vulkan_backend_resource_registry_state registry =
        vulkan_backend::build_vulkan_resource_registry_state(draw_list, plan, bindings);

    require(registry.checked, "resource registry state is checked");
    require(registry.completed(), "resource registry state completes when all resources are bound");
    require(registry.planned_batch_count == 5, "resource registry tracks planned batch count");
    require(registry.descriptor_binding_count == 14, "resource registry counts descriptor binding slots");
    require(registry.registered_resource_count == 11, "resource registry stores unique resources");
    require(registry.descriptor_reuse_count == 3, "resource registry counts descriptor reuse");
    require(registry.resource_reuse_count == 3, "resource registry counts resource reuse");
    require(registry.missing_resource_count == 0, "resource registry reports no missing resources");
    require(registry.missing_resources.empty(), "resource registry missing-resource list is empty");

    const vulkan_backend::vulkan_resource_registry_entry* image_texture = find_registry_resource(
        registry,
        vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "fixture://renderer/shared.png");
    require(image_texture != nullptr, "resource registry stores shared image texture");
    require(image_texture->use_count == 2, "resource registry records shared image texture use count");
    require(image_texture->reused(), "resource registry marks shared image texture reused");
    require(image_texture->first_command_index == 1, "resource registry records first image command index");
    require(image_texture->last_command_index == 2, "resource registry records last image command index");

    const vulkan_backend::vulkan_resource_registry_entry* image_sampler = find_registry_resource(
        registry,
        vulkan_backend::vulkan_resource_binding_kind::image_sampler,
        "image_sampler:1:1:0:0:0");
    require(image_sampler != nullptr, "resource registry stores shared image sampler");
    require(image_sampler->use_count == 2, "resource registry records shared image sampler use count");

    const vulkan_backend::vulkan_resource_registry_entry* glyph_atlas = find_registry_resource(
        registry,
        vulkan_backend::vulkan_resource_binding_kind::text_glyph_atlas,
        "glyph_atlas:title");
    require(glyph_atlas != nullptr, "resource registry stores shared glyph atlas");
    require(glyph_atlas->use_count == 2, "resource registry records shared glyph atlas use count");

    require(
        registry.resources[0].resource_id == "batch_uniform:quad",
        "resource registry keeps stable first-use resource order");
    require(
        registry.resources[3].resource_id == "fixture://renderer/shared.png",
        "resource registry keeps stable shared image texture order");
}

void test_vulkan_resource_registry_state_reports_missing_resources()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{0.0f, 0.0f, 40.0f, 40.0f},
        ""));
    draw_list.commands.push_back(render_draw_command{
        .type = render_draw_command_type::text,
        .node_id = "text",
        .parent_node_id = {},
        .depth = 0,
        .bounds = render_rect{40.0f, 0.0f, 40.0f, 24.0f},
        .content_bounds = render_rect{40.0f, 0.0f, 40.0f, 24.0f},
        .paint = render_paint{.source = "white", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    });

    const vulkan_backend::vulkan_frame_plan plan = vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        vulkan_backend::vulkan_frame_plan_options{
            .viewport = render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 100,
            .surface_height = 100,
        });
    const vulkan_backend::vulkan_backend_resource_binding_state bindings =
        vulkan_backend::build_vulkan_resource_binding_state(draw_list, plan);
    const vulkan_backend::vulkan_backend_resource_registry_state registry =
        vulkan_backend::build_vulkan_resource_registry_state(draw_list, plan, bindings);

    require(bindings.descriptor_validation.checked, "missing-resource descriptor validation is checked");
    require(!bindings.descriptor_validation.completed(), "missing-resource descriptor validation does not complete");
    require(
        bindings.descriptor_validation.missing_required_resource,
        "missing-resource descriptor validation reports missing required resource");
    require(!bindings.descriptor_validation.duplicate_binding, "missing-resource descriptor validation has no duplicate binding");
    require(!bindings.descriptor_validation.invalid_layout, "missing-resource descriptor validation has valid layout");
    require(bindings.descriptor_validation.planned_batch_count == 2, "missing-resource validation tracks planned batches");
    require(bindings.descriptor_validation.descriptor_set_count == 2, "missing-resource validation counts descriptor sets");
    require(
        bindings.descriptor_validation.valid_descriptor_set_count == 0,
        "missing-resource validation counts no fully valid descriptor sets");
    require(
        bindings.descriptor_validation.invalid_descriptor_set_count == 2,
        "missing-resource validation counts invalid descriptor sets");
    require(
        bindings.descriptor_validation.requested_binding_count == 6,
        "missing-resource validation counts requested bindings");
    require(bindings.descriptor_validation.valid_binding_count == 3, "missing-resource validation counts valid bindings");
    require(
        bindings.descriptor_validation.invalid_binding_count == 3,
        "missing-resource validation counts invalid bindings");
    require(
        bindings.descriptor_validation.failed_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "missing-resource validation records first failed batch kind");
    require(bindings.descriptor_validation.failed_command_index == 0, "missing-resource validation records command index");
    require(
        bindings.descriptor_validation.failed_binding_kind == vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "missing-resource validation records first missing binding kind");
    require(bindings.descriptor_validation.failed_binding == 1, "missing-resource validation records first missing binding slot");

    const vulkan_backend::vulkan_descriptor_set_validation_snapshot& image_descriptor =
        bindings.descriptor_validation.descriptor_sets[0];
    require(image_descriptor.missing_required_resource, "image descriptor validation records missing texture");
    require(
        image_descriptor.status == vulkan_backend::vulkan_descriptor_validation_status::missing_required_resource,
        "image descriptor validation status reports missing resource");
    require(image_descriptor.bindings[1].bound == false, "image descriptor texture binding is not bound");
    require(
        image_descriptor.bindings[1].status
            == vulkan_backend::vulkan_descriptor_validation_status::missing_required_resource,
        "image descriptor texture binding status reports missing resource");

    const vulkan_backend::vulkan_descriptor_set_validation_snapshot& text_descriptor =
        bindings.descriptor_validation.descriptor_sets[1];
    require(text_descriptor.missing_required_resource, "text descriptor validation records missing text resources");
    require(text_descriptor.invalid_layout == false, "text descriptor validation keeps expected layout");
    require(
        text_descriptor.bindings[1].status
            == vulkan_backend::vulkan_descriptor_validation_status::missing_required_resource,
        "text run descriptor binding status reports missing resource");

    require(registry.checked, "missing-resource registry is checked");
    require(!registry.completed(), "missing-resource registry does not complete");
    require(registry.planned_batch_count == 2, "missing-resource registry tracks planned batches");
    require(registry.descriptor_binding_count == 6, "missing-resource registry counts attempted descriptor slots");
    require(registry.registered_resource_count == 3, "missing-resource registry records available resources");
    require(registry.missing_resource_count == 3, "missing-resource registry counts all missing resources");
    require(registry.missing_resources.size() == 3, "missing-resource registry stores all missing resources");
    require(
        registry.missing_resources[0].kind == vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "missing-resource registry records missing image texture first");
    require(registry.missing_resources[0].resource_id == "missing_image_texture:image", "missing image id is stable");
    require(
        registry.missing_resources[1].kind == vulkan_backend::vulkan_resource_binding_kind::text_run_buffer,
        "missing-resource registry records missing text run buffer second");
    require(registry.missing_resources[1].resource_id == "missing_text_run_buffer:text", "missing text run id is stable");
    require(
        registry.missing_resources[2].kind == vulkan_backend::vulkan_resource_binding_kind::text_glyph_atlas,
        "missing-resource registry records missing glyph atlas third");
    require(registry.missing_resources[2].resource_id == "missing_text_glyph_atlas:text", "missing glyph atlas id is stable");
}

} // namespace

int main()
{
    test_vulkan_resource_binding_state_builds_batch_snapshots();
    test_vulkan_resource_registry_state_tracks_reuse_counts();
    test_vulkan_resource_registry_state_reports_missing_resources();
    return 0;
}
