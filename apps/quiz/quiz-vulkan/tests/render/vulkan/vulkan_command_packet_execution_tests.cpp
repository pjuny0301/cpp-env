#include "render/image/image_texture_frame_resource_packet_materialization.h"
#include "render/vulkan/vulkan_backend_adapter.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
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

quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_snapshot make_binding()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_snapshot{
        .set = 0,
        .binding = 0,
        .kind = quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_kind::batch_uniform,
        .resource_id = "batch_uniform:packet",
        .required = true,
        .available = true,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_snapshot make_binding(
    std::size_t binding,
    quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_kind kind,
    std::string resource_id)
{
    return quiz_vulkan::render::vulkan_backend::vulkan_resource_binding_snapshot{
        .set = 0,
        .binding = binding,
        .kind = kind,
        .resource_id = std::move(resource_id),
        .required = true,
        .available = true,
    };
}

std::array<quiz_vulkan::render::vulkan_backend::vulkan_quad_vertex, 4>
make_packet_vertices()
{
    return {
        quiz_vulkan::render::vulkan_backend::vulkan_quad_vertex{.x = 0.0f, .y = 0.0f},
        quiz_vulkan::render::vulkan_backend::vulkan_quad_vertex{.x = 10.0f, .y = 0.0f},
        quiz_vulkan::render::vulkan_backend::vulkan_quad_vertex{.x = 10.0f, .y = 10.0f},
        quiz_vulkan::render::vulkan_backend::vulkan_quad_vertex{.x = 0.0f, .y = 10.0f},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_command_packet make_packet(
    quiz_vulkan::render::vulkan_backend::vulkan_command_packet_category category,
    quiz_vulkan::render::vulkan_backend::vulkan_batch_kind batch_kind,
    std::size_t packet_index,
    std::size_t command_index)
{
    return quiz_vulkan::render::vulkan_backend::vulkan_command_packet{
        .category = category,
        .batch_kind = batch_kind,
        .command_index = command_index,
        .packet_index = packet_index,
        .node_id = "packet-" + std::to_string(packet_index),
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 10.0f, 10.0f},
        .clipped_bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 10.0f, 10.0f},
        .scissor = quiz_vulkan::render::vulkan_backend::vulkan_scissor_rect{
            .x = 0,
            .y = 0,
            .width = 10,
            .height = 10,
        },
        .vertices = make_packet_vertices(),
        .descriptor_set_count = 1,
        .binding_count = 1,
        .bindings = {make_binding()},
    };
}

void append_packet(
    quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result& bridge,
    quiz_vulkan::render::vulkan_backend::vulkan_command_packet packet)
{
    using namespace quiz_vulkan::render;

    switch (packet.category) {
    case vulkan_backend::vulkan_command_packet_category::rect:
        ++bridge.rect_packet_count;
        break;
    case vulkan_backend::vulkan_command_packet_category::text:
        ++bridge.text_packet_count;
        break;
    case vulkan_backend::vulkan_command_packet_category::image:
        ++bridge.image_packet_count;
        break;
    case vulkan_backend::vulkan_command_packet_category::debug_bounds:
        ++bridge.debug_bounds_packet_count;
        break;
    }

    bridge.packets.push_back(std::move(packet));
    bridge.packet_count = bridge.packets.size();
    bridge.planned_batch_count = bridge.packets.size();
}

quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result make_ready_bridge()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_command_packet_bridge_result bridge{
        .checked = true,
        .status = vulkan_backend::vulkan_command_packet_bridge_status::ready,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::none,
        .pipeline_checked = true,
        .pipeline_ready = true,
        .resource_bindings_checked = true,
        .resource_bindings_ready = true,
        .resource_registry_checked = true,
        .resource_registry_ready = true,
    };

    append_packet(
        bridge,
        make_packet(
            vulkan_backend::vulkan_command_packet_category::rect,
            vulkan_backend::vulkan_batch_kind::quad,
            0,
            10));
    append_packet(
        bridge,
        make_packet(
            vulkan_backend::vulkan_command_packet_category::text,
            vulkan_backend::vulkan_batch_kind::text,
            1,
            11));
    append_packet(
        bridge,
        make_packet(
            vulkan_backend::vulkan_command_packet_category::image,
            vulkan_backend::vulkan_batch_kind::image,
            2,
            12));
    append_packet(
        bridge,
        make_packet(
            vulkan_backend::vulkan_command_packet_category::debug_bounds,
            vulkan_backend::vulkan_batch_kind::debug_bounds,
            3,
            13));
    return bridge;
}

quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result
make_ready_image_resource_bridge(
    std::string image_resource_id = "fixture://renderer/card.png",
    std::string sampler_resource_id = "image_sampler:1:1:0:0:0")
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_command_packet_bridge_result bridge{
        .checked = true,
        .status = vulkan_backend::vulkan_command_packet_bridge_status::ready,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::none,
        .pipeline_checked = true,
        .pipeline_ready = true,
        .resource_bindings_checked = true,
        .resource_bindings_ready = true,
        .resource_registry_checked = true,
        .resource_registry_ready = true,
    };

    vulkan_backend::vulkan_command_packet packet = make_packet(
        vulkan_backend::vulkan_command_packet_category::image,
        vulkan_backend::vulkan_batch_kind::image,
        0,
        20);
    packet.bindings = {
        make_binding(
            0,
            vulkan_backend::vulkan_resource_binding_kind::batch_uniform,
            "batch_uniform:image"),
        make_binding(
            1,
            vulkan_backend::vulkan_resource_binding_kind::image_texture,
            std::move(image_resource_id)),
        make_binding(
            2,
            vulkan_backend::vulkan_resource_binding_kind::image_sampler,
            std::move(sampler_resource_id)),
    };
    packet.binding_count = packet.bindings.size();
    packet.descriptor_set_count = 1;
    append_packet(bridge, std::move(packet));
    return bridge;
}

quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result make_empty_bridge()
{
    return quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result{
        .checked = true,
        .status = quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_status::ready,
        .fallback_reason = quiz_vulkan::render::vulkan_backend::vulkan_backend_fallback_reason::none,
        .pipeline_checked = true,
        .pipeline_ready = true,
        .resource_bindings_checked = true,
        .resource_bindings_ready = true,
        .resource_registry_checked = true,
        .resource_registry_ready = true,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_render_pass_scope_record_result
make_ready_render_pass_scope(std::size_t selected_index = 1)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_render_pass_scope_record_result{
        .checked = true,
        .status = vulkan_backend::vulkan_native_render_pass_scope_record_status::recorded,
        .selected_framebuffer_target_index = selected_index,
        .framebuffer_target_count = 2,
        .image_id = vulkan_backend::vulkan_swapchain_image_id{.value = selected_index + 1},
        .command_buffer =
            vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 909},
        .render_pass = vulkan_backend::vulkan_render_pass_handle{.value = 300},
        .framebuffer = vulkan_backend::vulkan_framebuffer_handle{.value = 13000 + selected_index + 1},
        .extent = vulkan_backend::vulkan_surface_extent{.width = 1280, .height = 720},
        .begin_render_pass_symbol = vulkan_backend::vulkan_native_function_pointer{.value = 1401},
        .end_render_pass_symbol = vulkan_backend::vulkan_native_function_pointer{.value = 1402},
        .framebuffer_targets_ready = true,
        .command_buffer_ready = true,
        .render_pass_ready = true,
        .framebuffer_ready = true,
        .extent_ready = true,
        .extent_matches = true,
        .dispatch_table_ready = true,
        .vk_cmd_begin_render_pass_called = true,
        .vk_cmd_end_render_pass_called = true,
        .diagnostic = "Native Vulkan render pass scope recorded",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_loader_readiness_state make_ready_loader()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_loader_readiness_state{
        .checked = true,
        .status = vulkan_backend::vulkan_loader_readiness_status::ready,
        .probe_status = vulkan_backend::vulkan_loader_probe_status::available,
        .loader_library_available = true,
        .instance_proc_address_available = true,
        .instance_ready = true,
        .loaded_library_name = "fake-vulkan-loader",
        .required_symbol_name = std::string{vulkan_backend::vulkan_loader_required_symbol_name()},
        .attempted_library_count = 1,
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_function_table_diagnostics
make_native_functions(std::vector<std::string> missing_symbols = {})
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::fake_vulkan_native_symbol_resolver resolver(
        vulkan_backend::fake_vulkan_native_symbol_resolver_options{
            .missing_symbols = std::move(missing_symbols),
        });
    return vulkan_backend::collect_vulkan_native_function_table(
        resolver,
        make_ready_loader());
}

quiz_vulkan::render::vulkan_backend::vulkan_native_command_packet_executor_evidence
make_native_packet_evidence(
    quiz_vulkan::render::vulkan_backend::vulkan_native_function_table_diagnostics
        native_functions = make_native_functions())
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::vulkan_native_command_packet_executor_evidence{
        .native_functions = std::move(native_functions),
        .command_buffer =
            vulkan_backend::vulkan_command_recording_command_buffer_handle{.value = 909},
        .pipeline = vulkan_backend::vulkan_graphics_pipeline_handle{.value = 4400},
        .pipeline_layout = vulkan_backend::vulkan_pipeline_layout_handle{.value = 5500},
        .viewport = quiz_vulkan::render::render_rect{0.0f, 0.0f, 1280.0f, 720.0f},
        .viewport_available = true,
        .descriptor_sets = {
            vulkan_backend::vulkan_native_command_packet_descriptor_set{
                .packet_index = 0,
                .set = 0,
                .descriptor_set =
                    vulkan_backend::vulkan_native_descriptor_set_handle{.value = 7000},
                .required = true,
                .available = true,
            },
            vulkan_backend::vulkan_native_command_packet_descriptor_set{
                .packet_index = 1,
                .set = 0,
                .descriptor_set =
                    vulkan_backend::vulkan_native_descriptor_set_handle{.value = 7001},
                .required = true,
                .available = true,
            },
            vulkan_backend::vulkan_native_command_packet_descriptor_set{
                .packet_index = 2,
                .set = 0,
                .descriptor_set =
                    vulkan_backend::vulkan_native_descriptor_set_handle{.value = 7002},
                .required = true,
                .available = true,
            },
            vulkan_backend::vulkan_native_command_packet_descriptor_set{
                .packet_index = 3,
                .set = 0,
                .descriptor_set =
                    vulkan_backend::vulkan_native_descriptor_set_handle{.value = 7003},
                .required = true,
                .available = true,
            },
        },
        .descriptor_write_payloads = {},
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_backend_frame_result
make_native_packet_frame_without_descriptor_handles()
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_backend_frame_result frame;
    frame.surface = vulkan_backend::vulkan_surface_extent{.width = 1280, .height = 720};
    frame.viewport = quiz_vulkan::render::render_rect{4.0f, 8.0f, 640.0f, 360.0f};
    frame.command_buffer_recording.command_buffer =
        vulkan_backend::vulkan_command_buffer_id{.value = 44};
    frame.pipeline.pipeline_layout.pipeline_layout =
        vulkan_backend::vulkan_pipeline_layout_handle{.value = 5500};
    frame.pipeline.graphics_pipeline.pipeline =
        vulkan_backend::vulkan_graphics_pipeline_handle{.value = 4400};
    frame.command_packets = make_ready_bridge();
    return frame;
}

quiz_vulkan::render::vulkan_backend::vulkan_backend_resource_binding_state
make_ready_resource_bindings(
    const quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result& bridge)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    vulkan_backend::vulkan_backend_resource_binding_state state{
        .checked = true,
        .planned_batch_count = bridge.packet_count,
        .descriptor_validation =
            vulkan_backend::vulkan_backend_descriptor_validation_state{
                .checked = true,
                .planned_batch_count = bridge.packet_count,
            },
    };
    state.batch_snapshots.reserve(bridge.packets.size());
    state.descriptor_validation.descriptor_sets.reserve(bridge.packets.size());

    for (const vulkan_backend::vulkan_command_packet& packet : bridge.packets) {
        state.descriptor_set_count += packet.descriptor_set_count;
        state.binding_count += packet.binding_count;
        state.batch_snapshots.push_back(
            vulkan_backend::vulkan_batch_resource_binding_snapshot{
                .batch_kind = packet.batch_kind,
                .command_index = packet.command_index,
                .node_id = packet.node_id,
                .descriptor_set_count = packet.descriptor_set_count,
                .binding_count = packet.binding_count,
                .missing_resource = false,
                .bindings = packet.bindings,
            });

        std::vector<vulkan_backend::vulkan_descriptor_binding_validation_snapshot>
            binding_validations;
        binding_validations.reserve(packet.bindings.size());
        for (const vulkan_backend::vulkan_resource_binding_snapshot& binding : packet.bindings) {
            binding_validations.push_back(
                vulkan_backend::vulkan_descriptor_binding_validation_snapshot{
                    .set = binding.set,
                    .binding = binding.binding,
                    .kind = binding.kind,
                    .resource_id = binding.resource_id,
                    .required = binding.required,
                    .available = binding.available,
                    .bound = binding.bound(),
                    .binding_index_matches_order = true,
                    .duplicate_binding = false,
                    .status = vulkan_backend::vulkan_descriptor_validation_status::valid,
                });
        }

        state.descriptor_validation.descriptor_set_count += packet.descriptor_set_count;
        ++state.descriptor_validation.valid_descriptor_set_count;
        state.descriptor_validation.requested_binding_count += packet.binding_count;
        state.descriptor_validation.valid_binding_count += packet.binding_count;
        state.descriptor_validation.descriptor_sets.push_back(
            vulkan_backend::vulkan_descriptor_set_validation_snapshot{
                .batch_kind = packet.batch_kind,
                .command_index = packet.command_index,
                .node_id = packet.node_id,
                .set = packet.bindings.empty() ? 0 : packet.bindings.front().set,
                .expected_binding_count = packet.binding_count,
                .actual_binding_count = packet.bindings.size(),
                .checked = true,
                .descriptor_set_declared = packet.descriptor_set_count > 0,
                .binding_count_matches = packet.binding_count == packet.bindings.size(),
                .missing_required_resource = false,
                .duplicate_binding = false,
                .invalid_layout = false,
                .status = vulkan_backend::vulkan_descriptor_validation_status::valid,
                .bindings = std::move(binding_validations),
            });
    }

    return state;
}

quiz_vulkan::render::render_image_texture_frame_resource_packet_materialization
make_image_materialization(
    std::string image_resource_id = "fixture://renderer/card.png",
    quiz_vulkan::render::render_image_texture_frame_resource_packet_materialization_status status =
        quiz_vulkan::render::render_image_texture_frame_resource_packet_materialization_status::materialized)
{
    namespace render = quiz_vulkan::render;

    const bool materialized =
        render::render_image_texture_frame_resource_packet_materialization_status_is_materialized(
            status);
    const bool blocked =
        render::render_image_texture_frame_resource_packet_materialization_status_is_blocked(
            status);

    render::render_image_texture_frame_resource_cache_handoff_record cache_record{
        .materialization_index = 0,
        .request_index = 0,
        .render_image_uri = image_resource_id,
        .normalized_uri = image_resource_id,
        .cache_key = image_resource_id,
        .stable_texture_cache_key = image_resource_id + "|image_sampler:1:1:0:0:0",
        .texture_id = materialized ? 7100u : 0u,
        .texture_revision = materialized ? 3u : 0u,
        .texture_width = materialized ? 32u : 0u,
        .texture_height = materialized ? 16u : 0u,
        .renderer_boundary_ready = materialized,
        .diagnostic = materialized
            ? "image frame resource cache handoff is ready"
            : "image frame resource cache handoff is blocked",
    };
    render::render_image_texture_frame_resource_upload_handoff_record upload_record{
        .materialization_index = 0,
        .request_index = 0,
        .upload_request_id = materialized ? 8100u : 0u,
        .upload_generation_id = materialized ? 4u : 0u,
        .mip_level_count = materialized ? 1u : 0u,
        .uploaded_byte_count = materialized ? 2048u : 0u,
        .upload_result_present = materialized,
        .renderer_boundary_ready = materialized,
        .diagnostic = materialized
            ? "image frame resource upload handoff is ready"
            : "image frame resource upload handoff is blocked",
    };
    render::render_image_texture_frame_resource_sampler_handoff_record sampler_record{
        .materialization_index = 0,
        .request_index = 0,
        .sampler_key = "image_sampler:1:1:0:0:0",
        .sampler_summary = "image_sampler:1:1:0:0:0",
        .renderer_boundary_ready = materialized,
        .diagnostic = materialized
            ? "image frame resource sampler handoff is ready"
            : "image frame resource sampler handoff is blocked",
    };
    render::render_image_texture_frame_resource_packet_materialization_entry entry{
        .materialization_index = 0,
        .request_index = 0,
        .status = status,
        .status_name =
            render::render_image_texture_frame_resource_packet_materialization_status_name(status),
        .materialized = materialized,
        .blocked = blocked,
        .missing_upload_result =
            status
            == render::render_image_texture_frame_resource_packet_materialization_status::
                blocked_missing_upload_result,
        .cache_record_present = materialized,
        .upload_record_present = materialized,
        .sampler_record_present = materialized,
        .cache_record = cache_record,
        .upload_record = upload_record,
        .sampler_record = sampler_record,
        .blocker_summary = blocked ? "image materialization blocked" : "",
        .diagnostic = materialized
            ? "image frame resource packet materialized"
            : "image frame resource packet materialization blocked",
    };

    render::render_image_texture_frame_resource_packet_materialization materialization{
        .frame_request_count = 1,
        .planned_packet_count = 1,
        .materialized_packet_count = materialized ? 1u : 0u,
        .blocked_packet_count = blocked ? 1u : 0u,
        .missing_upload_result_count =
            entry.missing_upload_result ? 1u : 0u,
        .cache_record_count = materialized ? 1u : 0u,
        .upload_record_count = materialized ? 1u : 0u,
        .sampler_record_count = materialized ? 1u : 0u,
        .renderer_boundary_ready = materialized,
        .has_blockers = blocked,
        .entries = {entry},
        .cache_handoff_records = materialized
            ? std::vector<render::render_image_texture_frame_resource_cache_handoff_record>{
                cache_record}
            : std::vector<render::render_image_texture_frame_resource_cache_handoff_record>{},
        .upload_handoff_records = materialized
            ? std::vector<render::render_image_texture_frame_resource_upload_handoff_record>{
                upload_record}
            : std::vector<render::render_image_texture_frame_resource_upload_handoff_record>{},
        .sampler_handoff_records = materialized
            ? std::vector<render::render_image_texture_frame_resource_sampler_handoff_record>{
                sampler_record}
            : std::vector<render::render_image_texture_frame_resource_sampler_handoff_record>{},
        .blocker_summary = blocked ? "image materialization blocked" : "no materialization blockers",
        .diagnostic = materialized
            ? "image frame resource packet materialization is ready"
            : "image frame resource packet materialization has missing upload results",
    };
    return materialization;
}

quiz_vulkan::render::vulkan_backend::vulkan_native_image_descriptor_resource_evidence
make_native_image_descriptor_resources(
    std::string texture_resource_id = "fixture://renderer/card.png",
    std::string sampler_resource_id = "image_sampler:1:1:0:0:0",
    std::uintptr_t image_view_handle = 12000,
    std::uintptr_t sampler_handle = 13000,
    std::uint32_t image_layout = 5)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const bool available =
        image_view_handle != 0 && sampler_handle != 0 && image_layout != 0;
    return vulkan_backend::vulkan_native_image_descriptor_resource_evidence{
        .checked = true,
        .resources = {
            vulkan_backend::vulkan_native_image_descriptor_resource{
                .texture_resource_id = std::move(texture_resource_id),
                .sampler_resource_id = std::move(sampler_resource_id),
                .image_view =
                    vulkan_backend::vulkan_native_descriptor_image_view_handle{
                        .value = image_view_handle,
                    },
                .sampler =
                    vulkan_backend::vulkan_native_descriptor_sampler_handle{
                        .value = sampler_handle,
                    },
                .image_layout =
                    vulkan_backend::vulkan_native_descriptor_image_layout{
                        .value = image_layout,
                    },
                .available = available,
                .diagnostic = available
                    ? "native image descriptor resource is ready"
                    : "native image descriptor resource is missing native handles",
            },
        },
        .diagnostic = available
            ? "native image descriptor resource evidence is ready"
            : "native image descriptor resource evidence is incomplete",
    };
}

quiz_vulkan::render::vulkan_backend::vulkan_native_descriptor_set_allocation_result
make_ready_image_descriptor_set_allocation(
    const quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result& bridge)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(bridge);
    const quiz_vulkan::render::render_image_texture_frame_resource_packet_materialization
        materialization = make_image_materialization();
    return vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
        bridge,
        resource_bindings,
        materialization,
        vulkan_backend::vulkan_native_descriptor_set_fake_allocator_options{
            .first_descriptor_set_handle = 9100,
        });
}

quiz_vulkan::render::vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result
make_ready_descriptor_write_payload_handoff(
    const quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result& bridge,
    const quiz_vulkan::render::vulkan_backend::vulkan_native_descriptor_set_allocation_result& allocation)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    return vulkan_backend::build_vulkan_native_descriptor_write_payload_handoff_result(
        bridge,
        allocation,
        make_native_image_descriptor_resources());
}

quiz_vulkan::render::vulkan_backend::vulkan_command_recorder_operation_plan
make_ready_image_command_recorder_operation_plan(
    const quiz_vulkan::render::vulkan_backend::vulkan_command_packet_bridge_result& bridge,
    const quiz_vulkan::render::vulkan_backend::vulkan_native_descriptor_set_allocation_result& allocation)
{
    namespace vulkan_backend = quiz_vulkan::render::vulkan_backend;

    (void)allocation;
    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_command_packet_execution_result execution =
        executor.execute_packets(bridge);
    return vulkan_backend::build_vulkan_command_recorder_operation_plan(bridge, execution);
}

void test_vulkan_command_packet_execution_names_are_stable()
{
    using namespace quiz_vulkan::render;

    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::not_checked)
            == std::string_view{"not_checked"},
        "execution status name for not checked is stable");
    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::completed)
            == std::string_view{"completed"},
        "execution status name for completed is stable");
    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::packet_bridge_unavailable)
            == std::string_view{"packet_bridge_unavailable"},
        "execution status name for packet bridge unavailable is stable");
    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::begin_failed)
            == std::string_view{"begin_failed"},
        "execution status name for begin failed is stable");
    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::packet_failed)
            == std::string_view{"packet_failed"},
        "execution status name for packet failed is stable");
    require(
        vulkan_backend::command_packet_execution_status_name(
            vulkan_backend::vulkan_command_packet_execution_status::end_failed)
            == std::string_view{"end_failed"},
        "execution status name for end failed is stable");
    require(
        vulkan_backend::command_packet_execution_event_name(
            vulkan_backend::vulkan_command_packet_execution_event::begin)
            == std::string_view{"begin"},
        "execution event name for begin is stable");
    require(
        vulkan_backend::command_packet_execution_event_name(
            vulkan_backend::vulkan_command_packet_execution_event::packet)
            == std::string_view{"packet"},
        "execution event name for packet is stable");
    require(
        vulkan_backend::command_packet_execution_event_name(
            vulkan_backend::vulkan_command_packet_execution_event::end)
            == std::string_view{"end"},
        "execution event name for end is stable");
    require(
        vulkan_backend::native_command_packet_execution_status_name(
            vulkan_backend::vulkan_native_command_packet_execution_status::completed)
            == std::string_view{"completed"},
        "native packet execution status name for completed is stable");
    require(
        vulkan_backend::native_command_packet_execution_status_name(
            vulkan_backend::vulkan_native_command_packet_execution_status::native_command_symbol_unavailable)
            == std::string_view{"native_command_symbol_unavailable"},
        "native packet execution status name for missing symbol is stable");
    require(
        vulkan_backend::native_command_packet_execution_status_name(
            vulkan_backend::vulkan_native_command_packet_execution_status::invalid_packet_data)
            == std::string_view{"invalid_packet_data"},
        "native packet execution status name for invalid packet data is stable");
    require(
        vulkan_backend::native_command_packet_execution_status_name(
            vulkan_backend::vulkan_native_command_packet_execution_status::
                descriptor_payloads_unavailable)
            == std::string_view{"descriptor_payloads_unavailable"},
        "native packet execution status name for descriptor payloads unavailable is stable");
    require(
        vulkan_backend::native_command_packet_call_kind_name(
            vulkan_backend::vulkan_native_command_packet_call_kind::bind_pipeline)
            == std::string_view{"bind_pipeline"},
        "native packet call kind name for bind pipeline is stable");
    require(
        vulkan_backend::native_command_packet_call_kind_name(
            vulkan_backend::vulkan_native_command_packet_call_kind::draw)
            == std::string_view{"draw"},
        "native packet call kind name for draw is stable");
    require(
        vulkan_backend::native_descriptor_set_allocation_status_name(
            vulkan_backend::vulkan_native_descriptor_set_allocation_status::ready)
            == std::string_view{"ready"},
        "native descriptor set allocation ready name is stable");
    require(
        vulkan_backend::native_descriptor_set_allocation_status_name(
            vulkan_backend::vulkan_native_descriptor_set_allocation_status::
                resource_binding_mismatch)
            == std::string_view{"resource_binding_mismatch"},
        "native descriptor set allocation mismatch name is stable");
    require(
        vulkan_backend::native_descriptor_set_allocation_status_name(
            vulkan_backend::vulkan_native_descriptor_set_allocation_status::
                image_materialization_unavailable)
            == std::string_view{"image_materialization_unavailable"},
        "native descriptor set allocation image unavailable name is stable");
    require(
        vulkan_backend::native_descriptor_set_allocation_status_name(
            vulkan_backend::vulkan_native_descriptor_set_allocation_status::
                image_materialization_blocked)
            == std::string_view{"image_materialization_blocked"},
        "native descriptor set allocation image blocked name is stable");
    require(
        vulkan_backend::native_descriptor_set_allocation_status_name(
            vulkan_backend::vulkan_native_descriptor_set_allocation_status::
                image_materialization_mismatch)
            == std::string_view{"image_materialization_mismatch"},
        "native descriptor set allocation image mismatch name is stable");
    require(
        vulkan_backend::native_descriptor_write_payload_status_name(
            vulkan_backend::vulkan_native_descriptor_write_payload_status::ready)
            == std::string_view{"ready"},
        "native descriptor write payload ready name is stable");
    require(
        vulkan_backend::native_descriptor_write_payload_status_name(
            vulkan_backend::vulkan_native_descriptor_write_payload_status::
                descriptor_set_allocation_unavailable)
            == std::string_view{"descriptor_set_allocation_unavailable"},
        "native descriptor write payload allocation unavailable name is stable");
    require(
        vulkan_backend::native_descriptor_write_payload_status_name(
            vulkan_backend::vulkan_native_descriptor_write_payload_status::
                image_descriptor_resource_unavailable)
            == std::string_view{"image_descriptor_resource_unavailable"},
        "native descriptor write payload missing resource name is stable");
    require(
        vulkan_backend::native_descriptor_write_payload_status_name(
            vulkan_backend::vulkan_native_descriptor_write_payload_status::
                image_descriptor_texture_mismatch)
            == std::string_view{"image_descriptor_texture_mismatch"},
        "native descriptor write payload texture mismatch name is stable");
    require(
        vulkan_backend::native_descriptor_write_payload_status_name(
            vulkan_backend::vulkan_native_descriptor_write_payload_status::
                image_descriptor_sampler_mismatch)
            == std::string_view{"image_descriptor_sampler_mismatch"},
        "native descriptor write payload sampler mismatch name is stable");
    require(
        vulkan_backend::native_descriptor_write_payload_status_name(
            vulkan_backend::vulkan_native_descriptor_write_payload_status::
                image_descriptor_resource_incomplete)
            == std::string_view{"image_descriptor_resource_incomplete"},
        "native descriptor write payload incomplete resource name is stable");
    require(
        vulkan_backend::native_descriptor_write_payload_status_name(
            vulkan_backend::vulkan_native_descriptor_write_payload_status::duplicate_payload)
            == std::string_view{"duplicate_payload"},
        "native descriptor write payload duplicate name is stable");
    require(
        vulkan_backend::native_descriptor_payload_command_recording_status_name(
            vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::ready)
            == std::string_view{"ready"},
        "native descriptor payload command recording ready name is stable");
    require(
        vulkan_backend::native_descriptor_payload_command_recording_status_name(
            vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::
                descriptor_write_payload_unavailable)
            == std::string_view{"descriptor_write_payload_unavailable"},
        "native descriptor payload command recording unavailable payload name is stable");
    require(
        vulkan_backend::native_descriptor_payload_command_recording_status_name(
            vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::
                missing_descriptor_write_payload)
            == std::string_view{"missing_descriptor_write_payload"},
        "native descriptor payload command recording missing payload name is stable");
    require(
        vulkan_backend::native_descriptor_payload_command_recording_status_name(
            vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::
                duplicate_descriptor_write_payload)
            == std::string_view{"duplicate_descriptor_write_payload"},
        "native descriptor payload command recording duplicate payload name is stable");
    require(
        vulkan_backend::native_descriptor_payload_command_recording_status_name(
            vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::
                incomplete_descriptor_write_payload)
            == std::string_view{"incomplete_descriptor_write_payload"},
        "native descriptor payload command recording incomplete payload name is stable");
    require(
        vulkan_backend::native_descriptor_payload_command_recording_status_name(
            vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::
                descriptor_write_payload_mismatch)
            == std::string_view{"descriptor_write_payload_mismatch"},
        "native descriptor payload command recording mismatch name is stable");
    require(
        vulkan_backend::scoped_command_packet_execution_status_name(
            vulkan_backend::vulkan_scoped_command_packet_execution_status::completed)
            == std::string_view{"completed"},
        "scoped execution status name for completed is stable");
    require(
        vulkan_backend::scoped_command_packet_execution_status_name(
            vulkan_backend::vulkan_scoped_command_packet_execution_status::render_pass_scope_unavailable)
            == std::string_view{"render_pass_scope_unavailable"},
        "scoped execution status name for unavailable render pass scope is stable");
    require(
        vulkan_backend::scoped_command_packet_execution_status_name(
            vulkan_backend::vulkan_scoped_command_packet_execution_status::packet_failed)
            == std::string_view{"packet_failed"},
        "scoped execution status name for packet failure is stable");
}

void test_vulkan_command_packet_execution_records_successful_lifecycle()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);

    require(result.checked, "packet execution result is checked");
    require(result.completed(), "packet execution completes");
    require(!result.failed(), "packet execution reports no failure");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::completed,
        "packet execution status is completed");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "packet execution has no fallback");
    require(result.packet_bridge_checked, "packet execution records checked bridge");
    require(result.packet_bridge_ready, "packet execution records ready bridge");
    require(result.begin_attempted, "packet execution attempts begin");
    require(result.begin_completed, "packet execution completes begin");
    require(result.end_attempted, "packet execution attempts end");
    require(result.end_completed, "packet execution completes end");
    require(!result.has_failed_packet, "packet execution has no failed packet");
    require(result.planned_packet_count == 4, "packet execution records planned count");
    require(result.attempted_packet_count == 4, "packet execution records attempted packet count");
    require(result.executed_packet_count == 4, "packet execution records executed packet count");
    require(result.rect_packet_count == 1, "packet execution counts rect packets");
    require(result.text_packet_count == 1, "packet execution counts text packets");
    require(result.image_packet_count == 1, "packet execution counts image packets");
    require(result.debug_bounds_packet_count == 1, "packet execution counts debug bounds packets");
    require(result.events.size() == 6, "packet execution records begin, packets, and end events");
    require(
        result.events[0].event == vulkan_backend::vulkan_command_packet_execution_event::begin,
        "packet execution first event is begin");
    require(
        result.events[1].event == vulkan_backend::vulkan_command_packet_execution_event::packet,
        "packet execution second event is packet");
    require(result.events[1].packet_index == 0, "packet execution records first packet index");
    require(
        result.events[2].category == vulkan_backend::vulkan_command_packet_category::text,
        "packet execution records second packet category");
    require(
        result.events[3].category == vulkan_backend::vulkan_command_packet_category::image,
        "packet execution records third packet category");
    require(
        result.events[4].category == vulkan_backend::vulkan_command_packet_category::debug_bounds,
        "packet execution records fourth packet category");
    require(
        result.events[5].event == vulkan_backend::vulkan_command_packet_execution_event::end,
        "packet execution final event is end");

    for (const vulkan_backend::vulkan_command_packet_execution_snapshot& event : result.events) {
        require(event.successful(), "successful packet execution event reports success");
    }
    require(executor.execution_result().completed(), "retained fake execution result completes");
}

void test_vulkan_command_packet_execution_records_first_packet_failure()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    vulkan_backend::fake_vulkan_command_packet_executor executor(
        vulkan_backend::fake_vulkan_command_packet_executor_options{
            .fail_packet = true,
            .fail_packet_index = 2,
        });
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);

    require(result.checked, "failed packet execution result is checked");
    require(!result.completed(), "failed packet execution does not complete");
    require(result.failed(), "failed packet execution reports failure");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::packet_failed,
        "packet execution status records packet failure");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::record_commands_failed,
        "packet execution maps packet failure to record commands fallback");
    require(result.begin_attempted, "failed packet execution attempts begin");
    require(result.begin_completed, "failed packet execution completes begin before packet failure");
    require(!result.end_attempted, "failed packet execution does not attempt end after packet failure");
    require(!result.end_completed, "failed packet execution does not complete end after packet failure");
    require(result.has_failed_packet, "failed packet execution records failed packet");
    require(
        result.first_failed_category == vulkan_backend::vulkan_command_packet_category::image,
        "failed packet execution records failed packet category");
    require(
        result.first_failed_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "failed packet execution records failed packet batch kind");
    require(result.first_failed_packet_index == 2, "failed packet execution records failed packet index");
    require(result.first_failed_command_index == 12, "failed packet execution records failed command index");
    require(result.planned_packet_count == 4, "failed packet execution preserves planned count");
    require(result.attempted_packet_count == 3, "failed packet execution records attempted packets through failure");
    require(result.executed_packet_count == 2, "failed packet execution records successfully executed packets");
    require(result.rect_packet_count == 1, "failed packet execution counts completed rect packet");
    require(result.text_packet_count == 1, "failed packet execution counts completed text packet");
    require(result.image_packet_count == 0, "failed packet execution does not count failed image packet as executed");
    require(result.debug_bounds_packet_count == 0, "failed packet execution stops before debug packet");
    require(result.events.size() == 4, "failed packet execution records begin, two packets, and failed packet");
    require(result.events[3].failed, "failed packet execution marks first failed packet event");
    require(
        result.events[3].category == vulkan_backend::vulkan_command_packet_category::image,
        "failed packet execution marks failed packet category");
}

void test_vulkan_command_packet_execution_records_successful_empty_lifecycle()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_empty_bridge();
    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);

    require(result.checked, "empty packet execution result is checked");
    require(result.completed(), "empty packet execution completes");
    require(!result.failed(), "empty packet execution does not fail");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::completed,
        "empty packet execution status is completed");
    require(
        result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "empty packet execution has no fallback");
    require(result.begin_attempted, "empty packet execution attempts begin");
    require(result.begin_completed, "empty packet execution completes begin");
    require(result.end_attempted, "empty packet execution attempts end");
    require(result.end_completed, "empty packet execution completes end");
    require(result.planned_packet_count == 0, "empty packet execution records no planned packets");
    require(result.attempted_packet_count == 0, "empty packet execution records no attempted packets");
    require(result.executed_packet_count == 0, "empty packet execution records no executed packets");
    require(result.rect_packet_count == 0, "empty packet execution records no rect packets");
    require(result.text_packet_count == 0, "empty packet execution records no text packets");
    require(result.image_packet_count == 0, "empty packet execution records no image packets");
    require(result.debug_bounds_packet_count == 0, "empty packet execution records no debug packets");
    require(result.events.size() == 2, "empty packet execution records begin and end only");
    require(
        result.events[0].event == vulkan_backend::vulkan_command_packet_execution_event::begin,
        "empty packet execution first event is begin");
    require(
        result.events[1].event == vulkan_backend::vulkan_command_packet_execution_event::end,
        "empty packet execution final event is end");
}

void test_vulkan_native_command_packet_executor_translates_packets_to_native_calls()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    vulkan_backend::vulkan_native_command_packet_executor executor(
        make_native_packet_evidence());
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(result.checked, "native packet executor result is checked");
    require(result.completed(), "native packet executor completes interface execution");
    require(result.events.size() == 6, "native packet executor records begin, packets, and end");
    require(
        result.events.front().event == vulkan_backend::vulkan_command_packet_execution_event::begin,
        "native packet executor first interface event is begin");
    require(
        result.events.back().event == vulkan_backend::vulkan_command_packet_execution_event::end,
        "native packet executor final interface event is end");
    require(native_result.checked, "native packet translation result is checked");
    require(native_result.completed(), "native packet translation completes");
    require(!native_result.failed(), "native packet translation reports no failure");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::completed,
        "native packet translation status is completed");
    require(
        native_result.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "native packet translation has no fallback");
    require(native_result.packet_bridge_checked, "native packet translation records bridge check");
    require(native_result.packet_bridge_ready, "native packet translation records ready bridge");
    require(
        native_result.native_function_table_checked,
        "native packet translation records checked function table");
    require(
        native_result.native_command_symbols_ready,
        "native packet translation records native command symbols ready");
    require(native_result.command_buffer_ready, "native packet translation records command buffer ready");
    require(native_result.pipeline_ready, "native packet translation records pipeline ready");
    require(
        native_result.pipeline_layout_ready,
        "native packet translation records pipeline layout ready");
    require(native_result.viewport_ready, "native packet translation records viewport ready");
    require(
        native_result.descriptor_sets_ready,
        "native packet translation records descriptor set handles ready");
    require(native_result.planned_packet_count == 4, "native packet translation records planned packets");
    require(
        native_result.attempted_packet_count == 4,
        "native packet translation records attempted packets");
    require(
        native_result.translated_packet_count == 4,
        "native packet translation records translated packets");
    require(
        native_result.attempted_native_call_count == 20,
        "native packet translation attempts five native calls per packet");
    require(
        native_result.completed_native_call_count == 20,
        "native packet translation completes five native calls per packet");
    require(native_result.calls.size() == 20, "native packet translation records native call evidence");
    require(
        native_result.calls[0].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::bind_pipeline,
        "native packet translation starts each packet by binding the pipeline");
    require(
        native_result.calls[1].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::bind_descriptor_sets,
        "native packet translation binds descriptor sets");
    require(
        native_result.calls[2].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::set_viewport,
        "native packet translation sets the viewport");
    require(
        native_result.calls[3].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::set_scissor,
        "native packet translation sets the packet scissor");
    require(
        native_result.calls[4].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::draw,
        "native packet translation emits a draw call");
    require(native_result.calls[4].symbol_name == "vkCmdDraw", "draw call records symbol name");
    require(native_result.calls[4].vertex_count == 4, "draw call records quad vertex count");
    require(native_result.calls[4].packet_index == 0, "draw call records packet index");
    for (const vulkan_backend::vulkan_native_command_packet_call_evidence& call :
         native_result.calls) {
        require(call.successful(), "each native command packet call reports success");
    }
    require(
        executor.execution_result().completed(),
        "native packet executor retains completed interface result");
}

void test_vulkan_native_descriptor_set_allocation_builds_fake_handles()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(bridge);
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            bridge,
            resource_bindings,
            vulkan_backend::vulkan_native_descriptor_set_fake_allocator_options{
                .first_descriptor_set_handle = 8100,
            });

    require(allocation.checked, "descriptor set allocation result is checked");
    require(allocation.completed(), "descriptor set allocation completes");
    require(
        allocation.status == vulkan_backend::vulkan_native_descriptor_set_allocation_status::ready,
        "descriptor set allocation status is ready");
    require(
        allocation.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::none,
        "descriptor set allocation has no fallback");
    require(allocation.packet_bridge_ready, "descriptor set allocation consumes ready bridge");
    require(
        allocation.resource_bindings_ready,
        "descriptor set allocation consumes ready resource bindings");
    require(
        allocation.planned_descriptor_set_count == 4,
        "descriptor set allocation records planned descriptor sets");
    require(
        allocation.allocated_descriptor_set_count == 4,
        "descriptor set allocation records allocated descriptor sets");
    require(allocation.descriptor_sets.size() == 4, "descriptor set allocation stores handles");
    require(
        allocation.descriptor_sets.front().packet_index == 0,
        "descriptor set allocation records packet ownership");
    require(allocation.descriptor_sets.front().set == 0, "descriptor set allocation records set");
    require(
        allocation.descriptor_sets.front().descriptor_set.value == 8100,
        "descriptor set allocation starts from fake allocator base handle");
    require(
        allocation.descriptor_sets.back().descriptor_set.value == 8103,
        "descriptor set allocation assigns stable sequential fake handles");
    for (const vulkan_backend::vulkan_native_command_packet_descriptor_set& descriptor_set :
         allocation.descriptor_sets) {
        require(descriptor_set.completed(), "each allocated descriptor set is complete");
    }
}

void test_vulkan_native_command_packet_executor_completes_with_allocated_descriptor_handles()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(frame.command_packets);
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            frame.command_packets,
            resource_bindings);
    const vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());

    require(allocation.completed(), "allocated descriptor evidence is complete");
    require(
        evidence.descriptor_sets.front().descriptor_set.valid(),
        "merged native packet evidence receives descriptor handles");
    require(
        evidence.descriptor_sets.front().available,
        "merged native packet evidence marks descriptor handles available");

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(frame.command_packets);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(result.completed(), "allocated descriptor evidence completes interface execution");
    require(native_result.completed(), "allocated descriptor evidence completes native execution");
    require(native_result.descriptor_sets_ready, "allocated descriptor evidence is ready");
    require(native_result.calls.size() == 20, "allocated descriptor evidence emits native calls");
    require(
        native_result.calls[1].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::bind_descriptor_sets,
        "allocated descriptor evidence binds descriptor sets");
    require(
        native_result.calls[1].descriptor_set_count == 1,
        "allocated descriptor evidence records descriptor set count");
}

void test_vulkan_native_descriptor_set_allocation_blocks_image_without_materialization()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(bridge);
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            bridge,
            resource_bindings);

    require(!allocation.completed(), "image descriptor allocation needs materialization evidence");
    require(
        allocation.status
            == vulkan_backend::vulkan_native_descriptor_set_allocation_status::
                image_materialization_unavailable,
        "image descriptor allocation reports missing materialization evidence");
    require(!allocation.image_materialization_checked, "missing image materialization is not checked");
    require(!allocation.image_materialization_ready, "missing image materialization is not ready");
    require(
        allocation.required_image_materialization_count == 1,
        "image descriptor allocation records required image materialization count");
    require(
        allocation.failed_resource_id == "fixture://renderer/card.png",
        "image descriptor allocation records blocked image resource id");
}

void test_vulkan_native_descriptor_set_allocation_blocks_blocked_image_materialization()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(bridge);
    const render_image_texture_frame_resource_packet_materialization materialization =
        make_image_materialization(
            "fixture://renderer/card.png",
            render_image_texture_frame_resource_packet_materialization_status::
                blocked_missing_upload_result);
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            bridge,
            resource_bindings,
            materialization);

    require(!materialization.ok(), "blocked image materialization evidence is not ready");
    require(!allocation.completed(), "blocked image materialization does not allocate descriptors");
    require(allocation.image_materialization_checked, "blocked image materialization is checked");
    require(!allocation.image_materialization_ready, "blocked image materialization is not ready");
    require(
        allocation.status
            == vulkan_backend::vulkan_native_descriptor_set_allocation_status::
                image_materialization_blocked,
        "blocked image materialization reports image blocker");
    require(
        allocation.failed_resource_id == "fixture://renderer/card.png",
        "blocked image materialization preserves image resource id");
}

void test_vulkan_native_descriptor_set_allocation_blocks_mismatched_image_materialization()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge("fixture://renderer/card.png");
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(bridge);
    const render_image_texture_frame_resource_packet_materialization materialization =
        make_image_materialization("fixture://renderer/other.png");
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            bridge,
            resource_bindings,
            materialization);

    require(materialization.ok(), "mismatched image materialization evidence can be otherwise ready");
    require(!allocation.completed(), "mismatched image materialization does not allocate descriptors");
    require(
        allocation.status
            == vulkan_backend::vulkan_native_descriptor_set_allocation_status::
                image_materialization_mismatch,
        "mismatched image materialization reports mismatch");
    require(
        allocation.failed_resource_id == "fixture://renderer/card.png",
        "mismatched image materialization records requested image resource id");
}

void test_vulkan_native_descriptor_set_allocation_blocks_mismatched_image_sampler_materialization()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge(
            "fixture://renderer/card.png",
            "image_sampler:mismatched");
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(bridge);
    const render_image_texture_frame_resource_packet_materialization materialization =
        make_image_materialization("fixture://renderer/card.png");
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            bridge,
            resource_bindings,
            materialization);

    require(materialization.ok(), "sampler mismatch materialization evidence can be otherwise ready");
    require(!allocation.completed(), "sampler mismatch does not allocate descriptors");
    require(
        allocation.status
            == vulkan_backend::vulkan_native_descriptor_set_allocation_status::
                image_materialization_mismatch,
        "sampler mismatch reports materialization mismatch");
    require(
        allocation.failed_resource_id == "image_sampler:mismatched",
        "sampler mismatch records requested sampler resource id");
}

void test_vulkan_native_descriptor_set_allocation_uses_image_materialization()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(bridge);
    const render_image_texture_frame_resource_packet_materialization materialization =
        make_image_materialization();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            bridge,
            resource_bindings,
            materialization,
            vulkan_backend::vulkan_native_descriptor_set_fake_allocator_options{
                .first_descriptor_set_handle = 9100,
            });

    require(materialization.ok(), "matching image materialization evidence is ready");
    require(allocation.completed(), "matching image materialization allocates descriptors");
    require(allocation.image_materialization_checked, "matching image materialization is checked");
    require(allocation.image_materialization_ready, "matching image materialization is ready");
    require(
        allocation.required_image_materialization_count == 1,
        "matching image materialization records required count");
    require(
        allocation.materialized_image_resource_count == 1,
        "matching image materialization records materialized count");
    require(
        allocation.descriptor_sets.front().descriptor_set.value == 9100,
        "matching image materialization keeps stable fake descriptor handle");

    vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    frame.command_packets = bridge;
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        make_ready_descriptor_write_payload_handoff(bridge, allocation);
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result
        payload_recording =
            vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
                bridge,
                allocation,
                handoff,
                operation_plan);

    vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());
    evidence = vulkan_backend::merge_vulkan_native_descriptor_write_payload_handoff_result(
        evidence,
        handoff);
    evidence = vulkan_backend::merge_vulkan_native_descriptor_payload_command_recording_result(
        evidence,
        payload_recording);
    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(result.completed(), "image materialization descriptor evidence completes execution");
    require(native_result.completed(), "image materialization descriptor evidence completes native execution");
    require(native_result.descriptor_sets_ready, "image materialization descriptor evidence is ready");
    require(
        native_result.descriptor_payload_binds_ready,
        "image materialization descriptor payload bind evidence is ready");
    require(native_result.calls.size() == 5, "single image packet emits five native calls");
    require(
        native_result.calls[1].descriptor_set_count == 1,
        "single image packet binds one descriptor set");
}

void test_vulkan_native_descriptor_write_payload_blocks_missing_descriptor_allocation()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        vulkan_backend::build_vulkan_native_descriptor_write_payload_handoff_result(
            bridge,
            vulkan_backend::vulkan_native_descriptor_set_allocation_result{},
            make_native_image_descriptor_resources());

    require(!handoff.completed(), "descriptor write payload requires descriptor set allocation");
    require(
        handoff.status
            == vulkan_backend::vulkan_native_descriptor_write_payload_status::
                descriptor_set_allocation_unavailable,
        "missing descriptor set allocation blocks descriptor write payload");
    require(
        !handoff.descriptor_set_allocation_checked,
        "missing descriptor set allocation records unchecked allocation evidence");
}

void test_vulkan_native_descriptor_write_payload_blocks_missing_image_descriptor_evidence()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        vulkan_backend::build_vulkan_native_descriptor_write_payload_handoff_result(
            bridge,
            allocation,
            vulkan_backend::vulkan_native_image_descriptor_resource_evidence{});

    require(allocation.completed(), "descriptor sets are allocated before payload handoff");
    require(!handoff.completed(), "descriptor write payload requires native image descriptor evidence");
    require(
        handoff.status
            == vulkan_backend::vulkan_native_descriptor_write_payload_status::
                image_descriptor_resource_unavailable,
        "missing image descriptor evidence blocks descriptor write payload");
    require(
        handoff.failed_resource_id == "fixture://renderer/card.png",
        "missing image descriptor evidence records texture resource id");
}

void test_vulkan_native_descriptor_write_payload_blocks_mismatched_texture_resource()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        vulkan_backend::build_vulkan_native_descriptor_write_payload_handoff_result(
            bridge,
            allocation,
            make_native_image_descriptor_resources(
                "fixture://renderer/other.png",
                "image_sampler:1:1:0:0:0"));

    require(!handoff.completed(), "texture mismatch blocks descriptor write payload");
    require(
        handoff.status
            == vulkan_backend::vulkan_native_descriptor_write_payload_status::
                image_descriptor_texture_mismatch,
        "texture mismatch reports texture blocker");
    require(
        handoff.failed_resource_id == "fixture://renderer/card.png",
        "texture mismatch records requested texture resource id");
}

void test_vulkan_native_descriptor_write_payload_blocks_mismatched_sampler_resource()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        vulkan_backend::build_vulkan_native_descriptor_write_payload_handoff_result(
            bridge,
            allocation,
            make_native_image_descriptor_resources(
                "fixture://renderer/card.png",
                "image_sampler:other"));

    require(!handoff.completed(), "sampler mismatch blocks descriptor write payload");
    require(
        handoff.status
            == vulkan_backend::vulkan_native_descriptor_write_payload_status::
                image_descriptor_sampler_mismatch,
        "sampler mismatch reports sampler blocker");
    require(
        handoff.failed_resource_id == "image_sampler:1:1:0:0:0",
        "sampler mismatch records requested sampler resource id");
}

void test_vulkan_native_descriptor_write_payload_blocks_duplicate_payloads()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    bridge.packets.front().bindings.push_back(bridge.packets.front().bindings[1]);
    bridge.packets.front().binding_count = bridge.packets.front().bindings.size();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        vulkan_backend::build_vulkan_native_descriptor_write_payload_handoff_result(
            bridge,
            allocation,
            make_native_image_descriptor_resources());

    require(allocation.completed(), "duplicate payload test keeps descriptor allocation complete");
    require(!handoff.completed(), "duplicate packet set/binding blocks descriptor write payload");
    require(
        handoff.status
            == vulkan_backend::vulkan_native_descriptor_write_payload_status::duplicate_payload,
        "duplicate packet set/binding reports duplicate payload");
    require(handoff.failed_binding == 1, "duplicate payload records failed binding");
}

void test_vulkan_native_descriptor_write_payload_blocks_incomplete_native_image_handles()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        vulkan_backend::build_vulkan_native_descriptor_write_payload_handoff_result(
            bridge,
            allocation,
            make_native_image_descriptor_resources(
                "fixture://renderer/card.png",
                "image_sampler:1:1:0:0:0",
                0,
                13000,
                5));

    require(!handoff.completed(), "missing native image view blocks descriptor write payload");
    require(
        handoff.status
            == vulkan_backend::vulkan_native_descriptor_write_payload_status::
                image_descriptor_resource_incomplete,
        "incomplete native image handles report incomplete resource");
    require(
        handoff.failed_resource_id == "fixture://renderer/card.png",
        "incomplete native image handles record texture resource id");
}

void test_vulkan_native_descriptor_write_payload_handoff_builds_stable_payloads()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_image_descriptor_resource_evidence image_resources =
        make_native_image_descriptor_resources();
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        vulkan_backend::build_vulkan_native_descriptor_write_payload_handoff_result(
            bridge,
            allocation,
            image_resources);

    require(allocation.completed(), "descriptor allocation is complete for payload handoff");
    require(image_resources.completed(), "native image descriptor resources are complete");
    require(handoff.completed(), "matching native image descriptor evidence builds payload handoff");
    require(
        handoff.payload_count == 3,
        "image packet descriptor write handoff emits one payload per packet binding");
    require(
        handoff.required_image_descriptor_payload_count == 2,
        "image packet descriptor write handoff tracks image payload count");
    require(
        handoff.ready_image_descriptor_payload_count == 2,
        "image packet descriptor write handoff tracks ready image payload count");

    const vulkan_backend::vulkan_native_descriptor_write_payload& texture_payload =
        handoff.payloads[1];
    require(texture_payload.completed(), "texture descriptor write payload is complete");
    require(texture_payload.descriptor_set.value == 9100, "texture payload carries descriptor set handle");
    require(texture_payload.set == 0, "texture payload carries descriptor set index");
    require(texture_payload.binding == 1, "texture payload carries texture binding");
    require(
        texture_payload.descriptor_kind == vulkan_backend::vulkan_resource_binding_kind::image_texture,
        "texture payload carries descriptor kind");
    require(
        texture_payload.resource_id == "fixture://renderer/card.png",
        "texture payload carries texture resource id");
    require(texture_payload.image_view.value == 12000, "texture payload carries image view handle");
    require(texture_payload.sampler.value == 13000, "texture payload carries sampler handle");
    require(texture_payload.image_layout.value == 5, "texture payload carries image layout");

    const vulkan_backend::vulkan_native_descriptor_write_payload& sampler_payload =
        handoff.payloads[2];
    require(sampler_payload.completed(), "sampler descriptor write payload is complete");
    require(sampler_payload.binding == 2, "sampler payload carries sampler binding");
    require(
        sampler_payload.descriptor_kind == vulkan_backend::vulkan_resource_binding_kind::image_sampler,
        "sampler payload carries descriptor kind");
    require(
        sampler_payload.resource_id == "image_sampler:1:1:0:0:0",
        "sampler payload carries sampler resource id");
    require(sampler_payload.sampler.value == 13000, "sampler payload carries sampler handle");

    vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    frame.command_packets = bridge;
    const vulkan_backend::vulkan_native_command_packet_executor_evidence default_evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());
    require(
        default_evidence.descriptor_write_payloads.empty(),
        "default frame/executor evidence does not attach descriptor write payloads");

    const vulkan_backend::vulkan_native_command_packet_executor_evidence merged_evidence =
        vulkan_backend::merge_vulkan_native_descriptor_write_payload_handoff_result(
            default_evidence,
            handoff);
    require(
        merged_evidence.descriptor_write_payloads.size() == handoff.payloads.size(),
        "explicit payload handoff merge attaches descriptor write payloads");
    require(
        merged_evidence.descriptor_write_payloads[1].image_view.value == 12000,
        "merged payload evidence preserves stable image view handle");
}

void test_vulkan_native_descriptor_payload_command_recording_accepts_ready_image_payloads()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        make_ready_descriptor_write_payload_handoff(bridge, allocation);
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result result =
        vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
            bridge,
            allocation,
            handoff,
            operation_plan);

    require(handoff.completed(), "ready recording evidence starts with ready descriptor payloads");
    require(operation_plan.completed(), "ready recording evidence starts with ready operation plan");
    require(result.completed(), "ready descriptor payload handoff allows command recording evidence");
    require(
        result.status
            == vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::ready,
        "ready descriptor payload command recording reports ready");
    require(result.packet_count == 1, "ready descriptor payload recording tracks packet count");
    require(result.ready_packet_count == 1, "ready descriptor payload recording tracks ready packet");
    require(result.bind_ready_packet_count == 1, "ready descriptor payload recording tracks bind readiness");
    require(result.draw_ready_packet_count == 1, "ready descriptor payload recording tracks draw readiness");
    require(
        result.descriptor_set_count == 1,
        "ready descriptor payload recording can see descriptor set handle count");
    require(
        result.descriptor_write_payload_count == 3,
        "ready descriptor payload recording can see descriptor payload count");
    require(
        result.image_descriptor_write_payload_count == 2,
        "ready descriptor payload recording tracks image descriptor payload count");
    require(
        result.ready_image_descriptor_write_payload_count == 2,
        "ready descriptor payload recording tracks ready image descriptor payload count");

    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_packet& packet =
        result.packets.front();
    require(packet.completed(), "ready descriptor payload packet evidence completes");
    require(packet.bind_ready, "ready descriptor payload packet can bind descriptors");
    require(packet.draw_ready, "ready descriptor payload packet can draw");
    require(packet.descriptor_sets.front().descriptor_set.value == 9100, "packet sees descriptor set handle");
    require(
        packet.descriptor_write_payloads[1].image_view.value == 12000,
        "packet sees texture image view handle");
    require(
        packet.descriptor_write_payloads[1].sampler.value == 13000,
        "packet sees texture sampler handle");
    require(
        packet.descriptor_write_payloads[1].image_layout.value == 5,
        "packet sees texture image layout");
    require(
        packet.descriptor_write_payloads[2].sampler.value == 13000,
        "packet sees sampler payload handle");

    vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    frame.command_packets = bridge;
    const vulkan_backend::vulkan_native_command_packet_executor_evidence default_evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());
    require(
        default_evidence.descriptor_write_payloads.empty(),
        "default executor evidence does not fabricate descriptor write payloads");
    const vulkan_backend::vulkan_native_command_packet_executor_evidence merged_evidence =
        vulkan_backend::merge_vulkan_native_descriptor_write_payload_handoff_result(
            default_evidence,
            handoff);
    require(
        merged_evidence.descriptor_write_payloads.size() == handoff.payloads.size(),
        "explicit merge carries descriptor write payloads into executor evidence");
    require(
        merged_evidence.descriptor_write_payloads[1].image_view.value == 12000,
        "explicit merge preserves native image view handle");
}

void test_vulkan_native_descriptor_payload_command_recording_blocks_blocked_payload_handoff()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result blocked_handoff =
        vulkan_backend::build_vulkan_native_descriptor_write_payload_handoff_result(
            bridge,
            allocation,
            vulkan_backend::vulkan_native_image_descriptor_resource_evidence{});
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result result =
        vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
            bridge,
            allocation,
            blocked_handoff,
            operation_plan);

    require(!blocked_handoff.completed(), "blocked descriptor payload handoff is incomplete");
    require(!result.completed(), "blocked descriptor payload handoff blocks command recording");
    require(
        result.status
            == vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::
                descriptor_write_payload_unavailable,
        "blocked descriptor payload handoff reports unavailable payloads");
    require(
        result.failed_resource_id == "fixture://renderer/card.png",
        "blocked descriptor payload handoff preserves failed resource id");
}

void test_vulkan_native_descriptor_payload_command_recording_blocks_missing_payload()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        make_ready_descriptor_write_payload_handoff(bridge, allocation);
    handoff.payloads.pop_back();
    handoff.payload_count = handoff.payloads.size();
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result result =
        vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
            bridge,
            allocation,
            handoff,
            operation_plan);

    require(!handoff.completed(), "missing descriptor payload makes handoff incomplete");
    require(!result.completed(), "missing descriptor payload blocks command recording");
    require(
        result.status
            == vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::
                missing_descriptor_write_payload,
        "missing descriptor payload reports missing payload status");
    require(result.failed_binding == 2, "missing descriptor payload records failed binding");
    require(
        result.failed_resource_id == "image_sampler:1:1:0:0:0",
        "missing descriptor payload records failed sampler resource");
}

void test_vulkan_native_descriptor_payload_command_recording_blocks_duplicate_payload()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        make_ready_descriptor_write_payload_handoff(bridge, allocation);
    handoff.payloads.push_back(handoff.payloads[1]);
    handoff.payload_count = handoff.payloads.size();
    handoff.planned_payload_count = handoff.payload_count;
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result result =
        vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
            bridge,
            allocation,
            handoff,
            operation_plan);

    require(!handoff.completed(), "duplicate descriptor payload makes handoff incomplete");
    require(!result.completed(), "duplicate descriptor payload blocks command recording");
    require(
        result.status
            == vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::
                duplicate_descriptor_write_payload,
        "duplicate descriptor payload reports duplicate status");
}

void test_vulkan_native_descriptor_payload_command_recording_blocks_incomplete_payload()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        make_ready_descriptor_write_payload_handoff(bridge, allocation);
    handoff.payloads[1].image_view = vulkan_backend::vulkan_native_descriptor_image_view_handle{};
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result result =
        vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
            bridge,
            allocation,
            handoff,
            operation_plan);

    require(!handoff.completed(), "incomplete descriptor payload makes handoff incomplete");
    require(!result.completed(), "incomplete descriptor payload blocks command recording");
    require(
        result.status
            == vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::
                incomplete_descriptor_write_payload,
        "incomplete descriptor payload reports incomplete status");
    require(result.failed_binding == 1, "incomplete descriptor payload records failed texture binding");
}

void test_vulkan_native_descriptor_payload_command_recording_blocks_mismatched_payload()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        make_ready_descriptor_write_payload_handoff(bridge, allocation);
    handoff.payloads[1].resource_id = "fixture://renderer/other.png";
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result result =
        vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
            bridge,
            allocation,
            handoff,
            operation_plan);

    require(handoff.completed(), "mismatched payload can remain internally complete");
    require(!result.completed(), "mismatched descriptor payload blocks command recording");
    require(
        result.status
            == vulkan_backend::vulkan_native_descriptor_payload_command_recording_status::
                descriptor_write_payload_mismatch,
        "mismatched descriptor payload reports mismatch status");
    require(
        result.failed_resource_id == "fixture://renderer/card.png",
        "mismatched descriptor payload records expected texture resource id");
}

void test_vulkan_native_command_packet_executor_binds_descriptor_payloads_for_image_packets()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        make_ready_descriptor_write_payload_handoff(bridge, allocation);
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result
        payload_recording =
            vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
                bridge,
                allocation,
                handoff,
                operation_plan);

    vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    frame.command_packets = bridge;
    vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());
    require(
        evidence.descriptor_payload_binds.empty(),
        "default executor evidence does not fabricate descriptor payload binds");

    evidence = vulkan_backend::merge_vulkan_native_descriptor_write_payload_handoff_result(
        std::move(evidence),
        handoff);
    evidence = vulkan_backend::merge_vulkan_native_descriptor_payload_command_recording_result(
        std::move(evidence),
        payload_recording);

    require(payload_recording.completed(), "payload command recording evidence is complete");
    require(
        evidence.descriptor_payload_binds.size() == 1,
        "executor evidence receives one image descriptor payload bind");
    const vulkan_backend::vulkan_native_command_packet_descriptor_payload_bind& bind =
        evidence.descriptor_payload_binds.front();
    require(bind.completed(), "descriptor payload bind evidence is complete");
    require(bind.bind_ready, "descriptor payload bind evidence is bind-ready");
    require(bind.draw_ready, "descriptor payload bind evidence is draw-ready");
    require(bind.pipeline_layout.value == 5500, "bind evidence owns pipeline layout handle");
    require(
        bind.descriptor_sets.front().descriptor_set.value == 9100,
        "bind evidence owns descriptor set handle");
    require(
        bind.descriptor_payloads.size() == 3,
        "bind evidence owns descriptor payload identities");
    require(
        bind.descriptor_payloads[1].stable_identity
            == "packet:0:set:0:binding:1:kind:image_texture:resource:fixture://renderer/card.png",
        "texture payload identity is stable");

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(result.completed(), "descriptor payload bind evidence completes packet execution");
    require(native_result.completed(), "descriptor payload bind evidence completes native execution");
    require(native_result.descriptor_payload_binds_ready, "descriptor payload bind evidence is ready");
    require(native_result.descriptor_payload_bind_count == 1, "native result records bind count");
    require(
        native_result.descriptor_payload_identity_count == 3,
        "native result records payload identity count");
    require(native_result.calls.size() == 5, "image packet emits five native command calls");
    require(
        native_result.calls[1].kind
            == vulkan_backend::vulkan_native_command_packet_call_kind::bind_descriptor_sets,
        "image packet bind call is recorded");
    require(
        native_result.calls[1].pipeline_layout.value == 5500,
        "bind call records pipeline layout handle");
    require(
        native_result.calls[1].descriptor_sets.front().descriptor_set.value == 9100,
        "bind call records descriptor set handle");
    require(
        native_result.calls[1].descriptor_payloads[1].image_view.value == 12000,
        "bind call records image view payload identity");
    require(
        native_result.calls[1].descriptor_payloads[2].sampler.value == 13000,
        "bind call records sampler payload identity");
}

void test_vulkan_native_command_packet_executor_blocks_missing_descriptor_payload_bind()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    frame.command_packets = bridge;
    const vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "missing descriptor payload bind blocks image execution");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::
                descriptor_payloads_unavailable,
        "missing descriptor payload bind reports descriptor payload unavailable");
    require(
        !native_result.descriptor_payload_binds_ready,
        "missing descriptor payload bind keeps payload bind readiness false");
    require(native_result.calls.empty(), "missing descriptor payload bind records no native calls");
}

void test_vulkan_native_command_packet_executor_blocks_duplicate_descriptor_payload_bind()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        make_ready_descriptor_write_payload_handoff(bridge, allocation);
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result
        payload_recording =
            vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
                bridge,
                allocation,
                handoff,
                operation_plan);
    vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    frame.command_packets = bridge;
    vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());
    evidence = vulkan_backend::merge_vulkan_native_descriptor_payload_command_recording_result(
        std::move(evidence),
        payload_recording);
    evidence.descriptor_payload_binds.push_back(evidence.descriptor_payload_binds.front());

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "duplicate descriptor payload bind blocks image execution");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::
                descriptor_payloads_unavailable,
        "duplicate descriptor payload bind reports descriptor payload unavailable");
    require(
        native_result.diagnostic.find("duplicate") != std::string::npos,
        "duplicate descriptor payload bind diagnostic names duplicate evidence");
}

void test_vulkan_native_command_packet_executor_blocks_incomplete_descriptor_payload_bind()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        make_ready_descriptor_write_payload_handoff(bridge, allocation);
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result
        payload_recording =
            vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
                bridge,
                allocation,
                handoff,
                operation_plan);
    vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    frame.command_packets = bridge;
    vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());
    evidence = vulkan_backend::merge_vulkan_native_descriptor_payload_command_recording_result(
        std::move(evidence),
        payload_recording);
    evidence.descriptor_payload_binds.front().descriptor_payloads[1].image_view = {};

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "incomplete descriptor payload bind blocks image execution");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::
                descriptor_payloads_unavailable,
        "incomplete descriptor payload bind reports descriptor payload unavailable");
    require(
        native_result.diagnostic.find("incomplete") != std::string::npos,
        "incomplete descriptor payload bind diagnostic names incomplete evidence");
}

void test_vulkan_native_command_packet_executor_blocks_mismatched_descriptor_payload_bind()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge =
        make_ready_image_resource_bridge();
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        make_ready_image_descriptor_set_allocation(bridge);
    const vulkan_backend::vulkan_native_descriptor_write_payload_handoff_result handoff =
        make_ready_descriptor_write_payload_handoff(bridge, allocation);
    const vulkan_backend::vulkan_command_recorder_operation_plan operation_plan =
        make_ready_image_command_recorder_operation_plan(bridge, allocation);
    const vulkan_backend::vulkan_native_descriptor_payload_command_recording_result
        payload_recording =
            vulkan_backend::build_vulkan_native_descriptor_payload_command_recording_result(
                bridge,
                allocation,
                handoff,
                operation_plan);
    vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    frame.command_packets = bridge;
    vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());
    evidence = vulkan_backend::merge_vulkan_native_descriptor_payload_command_recording_result(
        std::move(evidence),
        payload_recording);
    evidence.descriptor_payload_binds.front().descriptor_payloads[1].resource_id =
        "fixture://renderer/other.png";

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "mismatched descriptor payload bind blocks image execution");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::
                descriptor_payloads_unavailable,
        "mismatched descriptor payload bind reports descriptor payload unavailable");
    require(
        native_result.diagnostic.find("mismatched") != std::string::npos,
        "mismatched descriptor payload bind diagnostic names mismatched evidence");
}

void test_vulkan_native_command_packet_evidence_preserves_descriptor_handle_gap()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    const vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            make_native_functions());

    require(evidence.native_functions.checked, "native packet evidence keeps function diagnostics");
    require(
        evidence.command_buffer.value == 9044,
        "native packet evidence derives scoped command buffer from frame recording");
    require(evidence.pipeline.value == 4400, "native packet evidence keeps graphics pipeline handle");
    require(
        evidence.pipeline_layout.value == 5500,
        "native packet evidence keeps pipeline layout handle");
    require(evidence.viewport_available, "native packet evidence keeps visible viewport");
    require(evidence.viewport.x == 4.0f, "native packet evidence keeps viewport x");
    require(evidence.viewport.y == 8.0f, "native packet evidence keeps viewport y");
    require(evidence.viewport.width == 640.0f, "native packet evidence keeps viewport width");
    require(evidence.viewport.height == 360.0f, "native packet evidence keeps viewport height");
    require(
        evidence.descriptor_sets.size() == 4,
        "native packet evidence records one pending descriptor set per packet");
    require(
        evidence.descriptor_sets.front().packet_index == 0,
        "native packet evidence records first packet descriptor owner");
    require(evidence.descriptor_sets.front().set == 0, "native packet evidence records set index");
    require(
        evidence.descriptor_sets.front().required,
        "native packet evidence records descriptor set requirement");
    require(
        !evidence.descriptor_sets.front().available,
        "native packet evidence does not fabricate descriptor set availability");
    require(
        !evidence.descriptor_sets.front().descriptor_set.valid(),
        "native packet evidence does not fabricate native descriptor set handles");

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(frame.command_packets);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(result.checked, "descriptor gap result is checked");
    require(!result.completed(), "descriptor gap does not complete native packet execution");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::packet_failed,
        "descriptor gap fails inside packet execution");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::descriptor_sets_unavailable,
        "descriptor gap reports unavailable native descriptor sets");
    require(!native_result.descriptor_sets_ready, "descriptor gap keeps descriptor sets unready");
    require(native_result.first_failed_packet_index == 0, "descriptor gap blocks first packet");
}

void test_vulkan_native_command_packet_executor_blocks_incomplete_descriptor_evidence()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(frame.command_packets);
    vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            frame.command_packets,
            resource_bindings);
    allocation.descriptor_sets.front().available = false;

    const vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());
    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(frame.command_packets);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!allocation.completed(), "incomplete allocation evidence is not complete");
    require(!result.completed(), "incomplete descriptor evidence does not complete");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::descriptor_sets_unavailable,
        "incomplete descriptor evidence reports unavailable descriptor sets");
    require(native_result.first_failed_packet_index == 0, "incomplete descriptor evidence blocks first packet");
}

void test_vulkan_native_command_packet_executor_ignores_incomplete_descriptor_allocation_handles()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(frame.command_packets);
    vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            frame.command_packets,
            resource_bindings);
    allocation.allocated_descriptor_set_count = 0;

    const vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());

    require(!allocation.completed(), "tampered allocation evidence is not complete");
    require(
        !evidence.descriptor_sets.front().available,
        "incomplete allocation handles are not merged into native packet evidence");
    require(
        !evidence.descriptor_sets.front().descriptor_set.valid(),
        "incomplete allocation handles are not accepted as native descriptor handles");

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(frame.command_packets);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "incomplete allocation handles do not complete execution");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::descriptor_sets_unavailable,
        "incomplete allocation handles report unavailable descriptor sets");
    require(
        !native_result.descriptor_sets_ready,
        "incomplete allocation handles keep descriptor readiness false");
}

void test_vulkan_native_command_packet_executor_blocks_duplicate_descriptor_evidence()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(frame.command_packets);
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            frame.command_packets,
            resource_bindings);
    vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());
    evidence.descriptor_sets.push_back(evidence.descriptor_sets.front());

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(frame.command_packets);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "duplicated descriptor evidence does not complete");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::descriptor_sets_unavailable,
        "duplicated descriptor evidence reports unavailable descriptor sets");
    require(native_result.first_failed_packet_index == 0, "duplicated descriptor evidence blocks first packet");
}

void test_vulkan_native_command_packet_executor_blocks_wrong_packet_descriptor_evidence()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_backend_frame_result frame =
        make_native_packet_frame_without_descriptor_handles();
    const vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(frame.command_packets);
    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            frame.command_packets,
            resource_bindings);
    vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        vulkan_backend::build_vulkan_native_command_packet_executor_evidence(
            frame,
            allocation,
            make_native_functions());
    evidence.descriptor_sets.front().packet_index = 99;

    vulkan_backend::vulkan_native_command_packet_executor executor(evidence);
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(frame.command_packets);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "wrong-packet descriptor evidence does not complete");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::descriptor_sets_unavailable,
        "wrong-packet descriptor evidence reports unavailable descriptor sets");
    require(native_result.first_failed_packet_index == 0, "wrong-packet descriptor evidence blocks first packet");
}

void test_vulkan_native_descriptor_set_allocation_blocks_incomplete_resource_bindings()
{
    using namespace quiz_vulkan::render;

    const vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    vulkan_backend::vulkan_backend_resource_binding_state resource_bindings =
        make_ready_resource_bindings(bridge);
    resource_bindings.batch_snapshots.front().bindings.front().available = false;

    const vulkan_backend::vulkan_native_descriptor_set_allocation_result allocation =
        vulkan_backend::build_fake_vulkan_native_descriptor_set_allocation_result(
            bridge,
            resource_bindings);

    require(resource_bindings.completed(), "mutated resource binding evidence still claims completion");
    require(!allocation.completed(), "incomplete resource bindings do not allocate descriptor sets");
    require(
        allocation.status
            == vulkan_backend::vulkan_native_descriptor_set_allocation_status::resource_binding_mismatch,
        "incomplete resource bindings report descriptor allocation mismatch");
    require(
        allocation.fallback_reason
            == vulkan_backend::vulkan_backend_fallback_reason::resource_binding_unavailable,
        "incomplete resource bindings map to resource binding fallback");
}

void test_vulkan_native_command_packet_executor_blocks_missing_draw_symbol()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_native_command_packet_executor executor(
        make_native_packet_evidence(make_native_functions({"vkCmdDraw"})));
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(make_ready_bridge());
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(result.checked, "missing draw result is checked");
    require(!result.completed(), "missing draw does not complete interface execution");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::begin_failed,
        "missing draw fails before packet translation begins");
    require(result.begin_attempted, "missing draw records begin attempt");
    require(!result.begin_completed, "missing draw does not complete begin");
    require(result.events.size() == 1, "missing draw records only failed begin event");
    require(result.events.front().failed, "missing draw marks begin event failed");
    require(native_result.failed(), "missing draw reports native failure");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::native_command_symbol_unavailable,
        "missing draw reports native command symbol unavailable");
    require(
        native_result.missing_native_symbol_name == "vkCmdDraw",
        "missing draw records missing symbol name");
    require(!native_result.native_command_symbols_ready, "missing draw records symbols not ready");
    require(native_result.calls.empty(), "missing draw records no native calls");
    require(
        native_result.diagnostic.find("vkCmdDraw") != std::string::npos,
        "missing draw diagnostic names missing draw symbol");
}

void test_vulkan_native_command_packet_executor_blocks_invalid_command_buffer()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_native_command_packet_executor_evidence evidence =
        make_native_packet_evidence();
    evidence.command_buffer = {};
    vulkan_backend::vulkan_native_command_packet_executor executor(std::move(evidence));
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(make_ready_bridge());
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "invalid command buffer does not complete interface execution");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::begin_failed,
        "invalid command buffer maps to begin failure");
    require(native_result.failed(), "invalid command buffer reports native failure");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::command_buffer_unavailable,
        "invalid command buffer records unavailable command buffer status");
    require(!native_result.command_buffer_ready, "invalid command buffer records command buffer not ready");
    require(native_result.calls.empty(), "invalid command buffer records no native calls");
    require(
        native_result.diagnostic.find("command buffer") != std::string::npos,
        "invalid command buffer diagnostic names command buffer");
}

void test_vulkan_native_command_packet_executor_blocks_invalid_packet_scissor()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::vulkan_command_packet_bridge_result bridge = make_ready_bridge();
    bridge.packets[1].scissor = {};
    vulkan_backend::vulkan_native_command_packet_executor executor(
        make_native_packet_evidence());
    const vulkan_backend::vulkan_command_packet_execution_result result =
        executor.execute_packets(bridge);
    const vulkan_backend::vulkan_native_command_packet_execution_result native_result =
        executor.native_execution_result();

    require(!result.completed(), "invalid scissor does not complete interface execution");
    require(
        result.status == vulkan_backend::vulkan_command_packet_execution_status::packet_failed,
        "invalid scissor maps to packet failure");
    require(result.has_failed_packet, "invalid scissor records failed packet");
    require(
        result.first_failed_category == vulkan_backend::vulkan_command_packet_category::text,
        "invalid scissor records failed packet category");
    require(result.attempted_packet_count == 2, "invalid scissor attempts packets through failure");
    require(result.executed_packet_count == 1, "invalid scissor translates only prior packet");
    require(native_result.failed(), "invalid scissor reports native failure");
    require(
        native_result.status
            == vulkan_backend::vulkan_native_command_packet_execution_status::invalid_packet_data,
        "invalid scissor records invalid packet data status");
    require(
        native_result.calls.size() == 5,
        "invalid scissor keeps native call evidence for completed prior packet");
    require(
        native_result.diagnostic.find("scissor") != std::string::npos,
        "invalid scissor diagnostic names scissor data");
}

void test_vulkan_scoped_command_packet_execution_records_scope_and_packets()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_scoped_command_packet_execution_result result =
        vulkan_backend::execute_vulkan_scoped_command_packets(
            executor,
            vulkan_backend::vulkan_scoped_command_packet_execution_request{
                .render_pass_scope = make_ready_render_pass_scope(1),
                .packet_bridge = make_ready_bridge(),
            });

    require(result.checked, "scoped packet execution is checked");
    require(result.completed(), "scoped packet execution completes");
    require(!result.failed(), "scoped packet execution reports no failure");
    require(
        result.status == vulkan_backend::vulkan_scoped_command_packet_execution_status::completed,
        "scoped packet execution reports completed");
    require(result.render_pass_scope_id == 2, "scoped packet execution records scope id");
    require(result.selected_framebuffer_target_index == 1, "scoped packet execution records target index");
    require(result.image_id.value == 2, "scoped packet execution records image id");
    require(result.framebuffer.value == 13002, "scoped packet execution records framebuffer handle");
    require(result.command_buffer.value == 909, "scoped packet execution records command buffer");
    require(result.render_pass_scope_checked, "scoped packet execution records checked scope");
    require(result.render_pass_scope_ready, "scoped packet execution records ready scope");
    require(result.command_buffer_ready, "scoped packet execution records command buffer readiness");
    require(result.packet_bridge_checked, "scoped packet execution records checked packet bridge");
    require(result.packet_bridge_ready, "scoped packet execution records ready packet bridge");
    require(!result.scoped_execution_empty, "scoped packet execution records non-empty scope");
    require(result.render_pass_begin_attempted, "scoped packet execution records render pass begin attempt");
    require(result.render_pass_begin_completed, "scoped packet execution records render pass begin completion");
    require(result.render_pass_end_attempted, "scoped packet execution records render pass end attempt");
    require(result.render_pass_end_completed, "scoped packet execution records render pass end completion");
    require(!result.render_pass_end_skipped, "scoped packet execution does not skip render pass end");
    require(result.packet_execution_ready, "scoped packet execution records packet execution readiness");
    require(result.operation_plan_ready, "scoped packet execution records operation plan readiness");
    require(result.planned_packet_count == 4, "scoped packet execution records planned packets");
    require(result.attempted_packet_count == 4, "scoped packet execution records attempted packets");
    require(result.executed_packet_count == 4, "scoped packet execution records executed packets");
    require(result.rect_packet_count == 1, "scoped packet execution counts rect packets");
    require(result.text_packet_count == 1, "scoped packet execution counts text packets");
    require(result.image_packet_count == 1, "scoped packet execution counts image packets");
    require(result.debug_bounds_packet_count == 1, "scoped packet execution counts debug packets");
    require(result.packet_execution.events.size() == 6, "scoped packet execution keeps packet events");
    require(result.operation_plan.operation_count == 4, "scoped packet execution builds recorder operations");
    require(executor.execution_result().completed(), "scoped packet execution uses packet executor");
}

void test_vulkan_scoped_command_packet_execution_accepts_empty_scope()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_command_packet_executor executor;
    const vulkan_backend::vulkan_scoped_command_packet_execution_result result =
        vulkan_backend::execute_vulkan_scoped_command_packets(
            executor,
            vulkan_backend::vulkan_scoped_command_packet_execution_request{
                .render_pass_scope = make_ready_render_pass_scope(0),
                .packet_bridge = make_empty_bridge(),
            });

    require(result.checked, "empty scoped packet execution is checked");
    require(result.completed(), "empty scoped packet execution completes");
    require(result.scoped_execution_empty, "empty scoped packet execution records empty scope");
    require(result.render_pass_scope_id == 1, "empty scoped packet execution records scope id");
    require(result.planned_packet_count == 0, "empty scoped packet execution records no planned packets");
    require(result.attempted_packet_count == 0, "empty scoped packet execution records no attempted packets");
    require(result.executed_packet_count == 0, "empty scoped packet execution records no executed packets");
    require(result.packet_execution.events.size() == 2, "empty scoped packet execution keeps begin and end events");
    require(result.operation_plan.completed(), "empty scoped packet execution builds an empty operation plan");
}

void test_vulkan_scoped_command_packet_execution_reports_scope_and_bridge_blockers()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_command_packet_executor executor;

    vulkan_backend::vulkan_native_render_pass_scope_record_result begin_failed_scope =
        make_ready_render_pass_scope(0);
    begin_failed_scope.status =
        vulkan_backend::vulkan_native_render_pass_scope_record_status::begin_failed;
    begin_failed_scope.vk_cmd_end_render_pass_called = false;
    begin_failed_scope.diagnostic = "Native Vulkan render pass scope begin failed";
    const vulkan_backend::vulkan_scoped_command_packet_execution_result begin_failed =
        vulkan_backend::execute_vulkan_scoped_command_packets(
            executor,
            vulkan_backend::vulkan_scoped_command_packet_execution_request{
                .render_pass_scope = begin_failed_scope,
                .packet_bridge = make_ready_bridge(),
            });
    require(
        begin_failed.status == vulkan_backend::vulkan_scoped_command_packet_execution_status::begin_failed,
        "scoped packet execution reports render pass begin failure");
    require(begin_failed.render_pass_end_skipped, "scoped packet execution skips render pass end after begin failure");
    require(!begin_failed.packet_execution_checked, "scoped packet execution does not execute packets after begin failure");

    const vulkan_backend::vulkan_command_packet_bridge_result unavailable_bridge{
        .checked = true,
        .status = vulkan_backend::vulkan_command_packet_bridge_status::pipeline_unavailable,
        .fallback_reason = vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        .pipeline_checked = true,
        .pipeline_ready = false,
        .planned_batch_count = 4,
    };
    const vulkan_backend::vulkan_scoped_command_packet_execution_result bridge_blocked =
        vulkan_backend::execute_vulkan_scoped_command_packets(
            executor,
            vulkan_backend::vulkan_scoped_command_packet_execution_request{
                .render_pass_scope = make_ready_render_pass_scope(0),
                .packet_bridge = unavailable_bridge,
            });
    require(
        bridge_blocked.status
            == vulkan_backend::vulkan_scoped_command_packet_execution_status::packet_bridge_unavailable,
        "scoped packet execution reports unavailable packet bridge");
    require(
        bridge_blocked.fallback_reason == vulkan_backend::vulkan_backend_fallback_reason::pipeline_unavailable,
        "scoped packet execution preserves bridge fallback");
}

void test_vulkan_scoped_command_packet_execution_reports_first_packet_failure_inside_scope()
{
    using namespace quiz_vulkan::render;

    vulkan_backend::fake_vulkan_command_packet_executor executor(
        vulkan_backend::fake_vulkan_command_packet_executor_options{
            .fail_packet = true,
            .fail_packet_index = 2,
        });
    const vulkan_backend::vulkan_scoped_command_packet_execution_result result =
        vulkan_backend::execute_vulkan_scoped_command_packets(
            executor,
            vulkan_backend::vulkan_scoped_command_packet_execution_request{
                .render_pass_scope = make_ready_render_pass_scope(1),
                .packet_bridge = make_ready_bridge(),
            });

    require(result.checked, "failed scoped packet execution is checked");
    require(!result.completed(), "failed scoped packet execution does not complete");
    require(result.failed(), "failed scoped packet execution reports failure");
    require(
        result.status == vulkan_backend::vulkan_scoped_command_packet_execution_status::packet_failed,
        "scoped packet execution reports packet failure");
    require(result.has_failed_packet, "scoped packet execution records failed packet");
    require(
        result.first_failed_category == vulkan_backend::vulkan_command_packet_category::image,
        "scoped packet execution records first failed category");
    require(
        result.first_failed_batch_kind == vulkan_backend::vulkan_batch_kind::image,
        "scoped packet execution records first failed batch kind");
    require(result.first_failed_packet_index == 2, "scoped packet execution records first failed packet index");
    require(result.first_failed_command_index == 12, "scoped packet execution records first failed command index");
    require(result.attempted_packet_count == 3, "scoped packet execution records attempted packets through failure");
    require(result.executed_packet_count == 2, "scoped packet execution records packets before failure");
    require(result.rect_packet_count == 1, "scoped packet execution counts completed rect packet");
    require(result.text_packet_count == 1, "scoped packet execution counts completed text packet");
    require(result.image_packet_count == 0, "scoped packet execution does not count failed packet as executed");
    require(result.render_pass_begin_completed, "scoped packet execution records render pass scope began");
    require(result.render_pass_end_completed, "scoped packet execution preserves render pass scope end evidence");
    require(!result.operation_plan_ready, "scoped packet execution blocks operation plan after packet failure");
}

} // namespace

int main()
{
    test_vulkan_command_packet_execution_names_are_stable();
    test_vulkan_command_packet_execution_records_successful_lifecycle();
    test_vulkan_command_packet_execution_records_first_packet_failure();
    test_vulkan_command_packet_execution_records_successful_empty_lifecycle();
    test_vulkan_native_command_packet_executor_translates_packets_to_native_calls();
    test_vulkan_native_descriptor_set_allocation_builds_fake_handles();
    test_vulkan_native_command_packet_executor_completes_with_allocated_descriptor_handles();
    test_vulkan_native_descriptor_set_allocation_blocks_image_without_materialization();
    test_vulkan_native_descriptor_set_allocation_blocks_blocked_image_materialization();
    test_vulkan_native_descriptor_set_allocation_blocks_mismatched_image_materialization();
    test_vulkan_native_descriptor_set_allocation_blocks_mismatched_image_sampler_materialization();
    test_vulkan_native_descriptor_set_allocation_uses_image_materialization();
    test_vulkan_native_descriptor_write_payload_blocks_missing_descriptor_allocation();
    test_vulkan_native_descriptor_write_payload_blocks_missing_image_descriptor_evidence();
    test_vulkan_native_descriptor_write_payload_blocks_mismatched_texture_resource();
    test_vulkan_native_descriptor_write_payload_blocks_mismatched_sampler_resource();
    test_vulkan_native_descriptor_write_payload_blocks_duplicate_payloads();
    test_vulkan_native_descriptor_write_payload_blocks_incomplete_native_image_handles();
    test_vulkan_native_descriptor_write_payload_handoff_builds_stable_payloads();
    test_vulkan_native_descriptor_payload_command_recording_accepts_ready_image_payloads();
    test_vulkan_native_descriptor_payload_command_recording_blocks_blocked_payload_handoff();
    test_vulkan_native_descriptor_payload_command_recording_blocks_missing_payload();
    test_vulkan_native_descriptor_payload_command_recording_blocks_duplicate_payload();
    test_vulkan_native_descriptor_payload_command_recording_blocks_incomplete_payload();
    test_vulkan_native_descriptor_payload_command_recording_blocks_mismatched_payload();
    test_vulkan_native_command_packet_executor_binds_descriptor_payloads_for_image_packets();
    test_vulkan_native_command_packet_executor_blocks_missing_descriptor_payload_bind();
    test_vulkan_native_command_packet_executor_blocks_duplicate_descriptor_payload_bind();
    test_vulkan_native_command_packet_executor_blocks_incomplete_descriptor_payload_bind();
    test_vulkan_native_command_packet_executor_blocks_mismatched_descriptor_payload_bind();
    test_vulkan_native_command_packet_evidence_preserves_descriptor_handle_gap();
    test_vulkan_native_command_packet_executor_blocks_incomplete_descriptor_evidence();
    test_vulkan_native_command_packet_executor_ignores_incomplete_descriptor_allocation_handles();
    test_vulkan_native_command_packet_executor_blocks_duplicate_descriptor_evidence();
    test_vulkan_native_command_packet_executor_blocks_wrong_packet_descriptor_evidence();
    test_vulkan_native_descriptor_set_allocation_blocks_incomplete_resource_bindings();
    test_vulkan_native_command_packet_executor_blocks_missing_draw_symbol();
    test_vulkan_native_command_packet_executor_blocks_invalid_command_buffer();
    test_vulkan_native_command_packet_executor_blocks_invalid_packet_scissor();
    test_vulkan_scoped_command_packet_execution_records_scope_and_packets();
    test_vulkan_scoped_command_packet_execution_accepts_empty_scope();
    test_vulkan_scoped_command_packet_execution_reports_scope_and_bridge_blockers();
    test_vulkan_scoped_command_packet_execution_reports_first_packet_failure_inside_scope();
    return 0;
}
