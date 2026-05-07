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
    quiz_vulkan::render::render_rect bounds)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::quad,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "quad", .color = render_color{0.2f, 0.4f, 0.8f, 1.0f}},
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
        .paint = render_paint{.source = "text", .color = render_color{1.0f, 1.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {render_text_run{.text = std::move(text), .style_token = "body"}},
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

quiz_vulkan::render::render_draw_command make_debug_bounds_command(
    std::string node_id,
    quiz_vulkan::render::render_rect bounds)
{
    using namespace quiz_vulkan::render;

    return render_draw_command{
        .type = render_draw_command_type::debug_bounds,
        .node_id = std::move(node_id),
        .parent_node_id = {},
        .depth = 0,
        .bounds = bounds,
        .content_bounds = bounds,
        .paint = render_paint{.source = "debug", .color = render_color{1.0f, 0.0f, 1.0f, 1.0f}},
        .border_radius = 0.0f,
        .text_runs = {},
        .image = {},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_frame_plan make_plan(
    const quiz_vulkan::render::render_draw_list& draw_list)
{
    return quiz_vulkan::render::vulkan_backend::build_vulkan_frame_plan(
        draw_list,
        quiz_vulkan::render::vulkan_backend::vulkan_frame_plan_options{
            .viewport = quiz_vulkan::render::render_rect{0.0f, 0.0f, 100.0f, 100.0f},
            .surface_width = 100,
            .surface_height = 100,
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_backend_pipeline_state ensure_pipelines(
    const quiz_vulkan::render::vulkan_backend::vulkan_frame_plan& plan,
    quiz_vulkan::render::vulkan_backend::diagnostic_vulkan_pipeline_cache& pipeline_cache)
{
    for (const quiz_vulkan::render::vulkan_backend::vulkan_draw_batch& batch : plan.batches) {
        if (!pipeline_cache.ensure_pipeline(batch)) {
            break;
        }
    }
    return pipeline_cache.pipeline_state();
}

quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result build_bridge(
    const quiz_vulkan::render::render_draw_list& draw_list,
    const quiz_vulkan::render::vulkan_backend::vulkan_frame_plan& plan,
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_pipeline_state& pipeline)
{
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_resource_binding_state bindings =
        quiz_vulkan::render::vulkan_backend::build_vulkan_resource_binding_state(draw_list, plan);
    const quiz_vulkan::render::vulkan_backend::vulkan_backend_resource_registry_state registry =
        quiz_vulkan::render::vulkan_backend::build_vulkan_resource_registry_state(
            draw_list,
            plan,
            bindings);
    return quiz_vulkan::render::vulkan_backend::build_vulkan_command_packet_bridge(
        plan,
        pipeline,
        bindings,
        registry);
}

void test_vulkan_command_packet_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::command_packet_category_name(
            vulkan_backend::vulkan_command_packet_category::rect)
            == std::string_view{"rect"},
        "command packet category name for rect is stable");
    require(
        vulkan_backend::command_packet_category_name(
            vulkan_backend::vulkan_command_packet_category::text)
            == std::string_view{"text"},
        "command packet category name for text is stable");
    require(
        vulkan_backend::command_packet_category_name(
            vulkan_backend::vulkan_command_packet_category::image)
            == std::string_view{"image"},
        "command packet category name for image is stable");
    require(
        vulkan_backend::command_packet_category_name(
            vulkan_backend::vulkan_command_packet_category::debug_bounds)
            == std::string_view{"debug_bounds"},
        "command packet category name for debug bounds is stable");
    require(
        vulkan_backend::command_packet_bridge_status_name(
            vulkan_backend::vulkan_command_packet_bridge_status::not_checked)
            == std::string_view{"not_checked"},
        "command packet bridge status name for not checked is stable");
    require(
        vulkan_backend::command_packet_bridge_status_name(
            vulkan_backend::vulkan_command_packet_bridge_status::ready)
            == std::string_view{"ready"},
        "command packet bridge status name for ready is stable");
    require(
        vulkan_backend::command_packet_bridge_status_name(
            vulkan_backend::vulkan_command_packet_bridge_status::pipeline_unavailable)
            == std::string_view{"pipeline_unavailable"},
        "command packet bridge status name for pipeline unavailable is stable");
    require(
        vulkan_backend::command_packet_bridge_status_name(
            vulkan_backend::vulkan_command_packet_bridge_status::resource_binding_unavailable)
            == std::string_view{"resource_binding_unavailable"},
        "command packet bridge status name for resource binding unavailable is stable");
}

void test_vulkan_command_packet_bridge_builds_ordered_draw_packets()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_quad_command(
        "quad",
        render_rect{0.0f, 0.0f, 20.0f, 20.0f}));
    draw_list.commands.push_back(make_text_command(
        "text",
        render_rect{20.0f, 0.0f, 30.0f, 20.0f},
        "packet text"));
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{50.0f, 0.0f, 20.0f, 20.0f},
        "fixture://renderer/packet.png"));
    draw_list.commands.push_back(make_debug_bounds_command(
        "debug",
        render_rect{70.0f, 0.0f, 20.0f, 20.0f}));

    const vulkan_backend::vulkan_frame_plan plan = make_plan(draw_list);
    vulkan_backend::diagnostic_vulkan_pipeline_cache pipeline_cache;
    const vulkan_backend::vulkan_backend_pipeline_state pipeline =
        ensure_pipelines(plan, pipeline_cache);
    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        build_bridge(draw_list, plan, pipeline);

    require(bridge.checked, "command packet bridge is checked");
    require(bridge.completed(), "command packet bridge completes for prepared draw data");
    require(!bridge.blocked(), "command packet bridge does not block valid draw data");
    require(
        bridge.status == vulkan_backend::vulkan_command_packet_bridge_status::ready,
        "command packet bridge reports ready status");
    require(
        bridge.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "command packet bridge reports no fallback");
    require(bridge.pipeline_checked, "command packet bridge records pipeline check");
    require(bridge.pipeline_ready, "command packet bridge records pipeline ready");
    require(bridge.resource_bindings_checked, "command packet bridge records resource binding check");
    require(bridge.resource_bindings_ready, "command packet bridge records resource binding ready");
    require(bridge.resource_registry_checked, "command packet bridge records resource registry check");
    require(bridge.resource_registry_ready, "command packet bridge records resource registry ready");
    require(bridge.planned_batch_count == 4, "command packet bridge records planned packet count");
    require(bridge.packet_count == 4, "command packet bridge records emitted packet count");
    require(bridge.packets.size() == 4, "command packet bridge stores four packets");
    require(bridge.rect_packet_count == 1, "command packet bridge counts rect packets");
    require(bridge.text_packet_count == 1, "command packet bridge counts text packets");
    require(bridge.image_packet_count == 1, "command packet bridge counts image packets");
    require(bridge.debug_bounds_packet_count == 1, "command packet bridge counts debug bounds packets");
    require(bridge.clipped_packet_count == 0, "command packet bridge records no clipped packets");
    require(bridge.discarded_draw_call_count == 0, "command packet bridge records no discarded draws");

    require(
        bridge.packets[0].category == vulkan_backend::vulkan_command_packet_category::rect,
        "first command packet is rect");
    require(
        bridge.packets[1].category == vulkan_backend::vulkan_command_packet_category::text,
        "second command packet is text");
    require(
        bridge.packets[2].category == vulkan_backend::vulkan_command_packet_category::image,
        "third command packet is image");
    require(
        bridge.packets[3].category == vulkan_backend::vulkan_command_packet_category::debug_bounds,
        "fourth command packet is debug bounds");

    for (std::size_t index = 0; index < bridge.packets.size(); ++index) {
        const vulkan_backend::vulkan_command_packet& packet = bridge.packets[index];
        require(packet.packet_index == index, "command packet preserves stable packet order");
        require(packet.command_index == index, "command packet preserves source command index");
        require(packet.completed(), "command packet carries completed packet data");
        require(packet.descriptor_set_count == 1, "command packet records descriptor set");
        require(packet.binding_count == packet.bindings.size(), "command packet records binding count");
    }
}

void test_vulkan_command_packet_bridge_blocks_missing_pipeline()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{0.0f, 0.0f, 20.0f, 20.0f},
        "fixture://renderer/missing-pipeline.png"));

    const vulkan_backend::vulkan_frame_plan plan = make_plan(draw_list);
    vulkan_backend::diagnostic_vulkan_pipeline_cache pipeline_cache(
        vulkan_backend::diagnostic_vulkan_pipeline_cache_options{
            .default_available = true,
            .overrides = {
                vulkan_backend::vulkan_pipeline_capability{
                    .kind = vulkan_backend::vulkan_batch_kind::image,
                    .available = false,
                },
            },
        });
    const vulkan_backend::vulkan_backend_pipeline_state pipeline =
        ensure_pipelines(plan, pipeline_cache);
    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        vulkan_backend::build_vulkan_command_packet_bridge(
            plan,
            pipeline,
            vulkan_backend::vulkan_backend_resource_binding_state{},
            vulkan_backend::vulkan_backend_resource_registry_state{});

    require(bridge.checked, "command packet bridge is checked for missing pipeline");
    require(bridge.blocked(), "command packet bridge blocks missing pipeline");
    require(!bridge.completed(), "command packet bridge does not complete without pipeline");
    require(
        bridge.status == vulkan_backend::vulkan_command_packet_bridge_status::pipeline_unavailable,
        "command packet bridge reports missing pipeline");
    require(
        bridge.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        "command packet bridge reports pipeline fallback");
    require(bridge.pipeline_checked, "command packet bridge records pipeline check before block");
    require(!bridge.pipeline_ready, "command packet bridge records pipeline not ready");
    require(!bridge.resource_bindings_checked, "command packet bridge does not require resources after pipeline block");
    require(bridge.blocked_batch_kind == vulkan_backend::vulkan_batch_kind::image, "bridge records blocked batch kind");
    require(bridge.blocked_command_index == 0, "bridge records blocked command index");
    require(bridge.packet_count == 0, "bridge emits no packets after pipeline block");
    require(bridge.packets.empty(), "bridge stores no packets after pipeline block");
}

void test_vulkan_command_packet_bridge_blocks_missing_resource_binding()
{
    using namespace quiz_vulkan::render;

    render_draw_list draw_list;
    draw_list.commands.push_back(make_image_command(
        "image",
        render_rect{0.0f, 0.0f, 20.0f, 20.0f},
        ""));

    const vulkan_backend::vulkan_frame_plan plan = make_plan(draw_list);
    vulkan_backend::diagnostic_vulkan_pipeline_cache pipeline_cache;
    const vulkan_backend::vulkan_backend_pipeline_state pipeline =
        ensure_pipelines(plan, pipeline_cache);
    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        build_bridge(draw_list, plan, pipeline);

    require(bridge.checked, "command packet bridge is checked for missing resource");
    require(bridge.blocked(), "command packet bridge blocks missing resource");
    require(!bridge.completed(), "command packet bridge does not complete without resource binding");
    require(
        bridge.status == vulkan_backend::vulkan_command_packet_bridge_status::resource_binding_unavailable,
        "command packet bridge reports resource binding unavailable");
    require(
        bridge.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::resource_binding_unavailable,
        "command packet bridge reports resource binding fallback");
    require(bridge.pipeline_ready, "command packet bridge records pipeline ready before resource block");
    require(bridge.resource_bindings_checked, "command packet bridge checks resource bindings before block");
    require(!bridge.resource_bindings_ready, "command packet bridge records resources not ready");
    require(bridge.resource_registry_checked, "command packet bridge checks resource registry before block");
    require(!bridge.resource_registry_ready, "command packet bridge records registry not ready");
    require(bridge.blocked_batch_kind == vulkan_backend::vulkan_batch_kind::image, "bridge records resource batch kind");
    require(bridge.blocked_command_index == 0, "bridge records resource command index");
    require(bridge.blocked_resource_id == "missing_image_texture:image", "bridge records stable missing resource id");
    require(bridge.packet_count == 0, "bridge emits no packets after resource block");
    require(bridge.packets.empty(), "bridge stores no packets after resource block");
}

void test_vulkan_command_packet_bridge_completes_empty_draw_list()
{
    using namespace quiz_vulkan::render;

    const render_draw_list draw_list;
    const vulkan_backend::vulkan_frame_plan plan = make_plan(draw_list);
    vulkan_backend::diagnostic_vulkan_pipeline_cache pipeline_cache;
    const vulkan_backend::vulkan_backend_pipeline_state pipeline =
        ensure_pipelines(plan, pipeline_cache);
    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        build_bridge(draw_list, plan, pipeline);

    require(bridge.checked, "empty command packet bridge is checked");
    require(bridge.completed(), "empty command packet bridge completes");
    require(!bridge.blocked(), "empty command packet bridge does not block");
    require(
        bridge.status == vulkan_backend::vulkan_command_packet_bridge_status::ready,
        "empty command packet bridge is ready");
    require(
        bridge.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "empty command packet bridge has no fallback");
    require(bridge.pipeline_checked, "empty bridge records pipeline check satisfied");
    require(bridge.pipeline_ready, "empty bridge records pipeline ready");
    require(bridge.resource_bindings_checked, "empty bridge checks resource bindings");
    require(bridge.resource_bindings_ready, "empty bridge records resource bindings ready");
    require(bridge.resource_registry_checked, "empty bridge checks resource registry");
    require(bridge.resource_registry_ready, "empty bridge records resource registry ready");
    require(bridge.planned_batch_count == 0, "empty bridge records no planned batches");
    require(bridge.packet_count == 0, "empty bridge emits no packets");
    require(bridge.packets.empty(), "empty bridge stores no packets");
    require(bridge.rect_packet_count == 0, "empty bridge records no rect packets");
    require(bridge.text_packet_count == 0, "empty bridge records no text packets");
    require(bridge.image_packet_count == 0, "empty bridge records no image packets");
    require(bridge.debug_bounds_packet_count == 0, "empty bridge records no debug packets");
}

} // namespace

int main()
{
    test_vulkan_command_packet_names_are_stable();
    test_vulkan_command_packet_bridge_builds_ordered_draw_packets();
    test_vulkan_command_packet_bridge_blocks_missing_pipeline();
    test_vulkan_command_packet_bridge_blocks_missing_resource_binding();
    test_vulkan_command_packet_bridge_completes_empty_draw_list();
    return 0;
}
