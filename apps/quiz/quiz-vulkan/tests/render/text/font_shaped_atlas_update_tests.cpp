#include "render/text/font_backend_adapter.h"
#include "render/text/font_shaped_atlas_update.h"
#include "render/text/text_frame_draw_plan.h"
#include "render/text/text_frame_upload_handoff.h"

#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
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

std::filesystem::path quiz_vulkan_app_root_from_this_test()
{
    const std::filesystem::path test_path = std::filesystem::path(__FILE__);
    return test_path.parent_path().parent_path().parent_path().parent_path();
}

std::filesystem::path desktop_external_root()
{
    if (const char* external_root = std::getenv("QUIZ_VULKAN_DESKTOP_EXTERNAL_DIR");
        external_root != nullptr && *external_root != '\0') {
        return std::filesystem::path{external_root};
    }

    const std::filesystem::path app_root = quiz_vulkan_app_root_from_this_test();
    const std::filesystem::path repo_root =
        app_root.parent_path().parent_path().parent_path();
    return repo_root / "build" / "external" / "lib" / "cpp" / "desktop";
}

std::vector<std::filesystem::path> freetype_real_font_fixture_candidates()
{
    const std::filesystem::path external_root = desktop_external_root();
    return {
        external_root / "harfbuzz-14.2.0" / "perf" / "fonts" / "Roboto-Regular.ttf",
        external_root / "harfbuzz-14.2.0" / "test" / "api" / "fonts" / "Mplus1p-Regular.ttf",
    };
}

std::filesystem::path first_available_freetype_real_font_fixture()
{
    for (const std::filesystem::path& candidate : freetype_real_font_fixture_candidates()) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

std::vector<std::byte> read_binary_fixture_bytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    require(input.good(), "real font fixture opens for binary read");

    std::vector<std::byte> bytes;
    for (std::istreambuf_iterator<char> iter{input};
         iter != std::istreambuf_iterator<char>{};
         ++iter) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(*iter)));
    }
    return bytes;
}

quiz_vulkan::render::glyph_atlas_key key_for_a()
{
    return quiz_vulkan::render::glyph_atlas_key{
        .face_id = 7,
        .glyph_id = U'A',
        .pixel_size = 20,
    };
}

quiz_vulkan::render::font_face_descriptor freetype_fixture_face(
    const std::filesystem::path& font_path)
{
    return quiz_vulkan::render::font_face_descriptor{
        .id = 77,
        .family = "FreeType Fixture Sans",
        .source_uri = font_path.generic_string(),
        .version = "external-fixture",
        .license = "external-fixture",
        .coverage = {
            quiz_vulkan::render::font_codepoint_range{
                .first = 0x0041U,
                .last = 0x005aU,
            },
        },
        .weight = 400,
    };
}

quiz_vulkan::render::render_text_font_backend_capability_snapshot freetype_raster_capability()
{
    using namespace quiz_vulkan::render;

    return deterministic_fake_font_backend_capability_probe({
        render_text_font_backend_component{
            .library = render_text_font_backend_library::freetype,
            .name = "FreeType",
            .available = true,
            .version = render_text_font_backend_version{.major = 2, .minor = 14, .patch = 3},
            .features = {
                render_text_font_backend_feature::font_file_loading,
                render_text_font_backend_feature::unicode_cmap,
                render_text_font_backend_feature::glyph_id_mapping,
                render_text_font_backend_feature::glyph_rasterization,
            },
            .diagnostic = "FreeType raster fixture capability is available",
        },
    }).probe(render_text_font_backend_capability_probe_request{
        .required_libraries = {
            render_text_font_backend_library::freetype,
        },
        .required_features = {
            render_text_font_backend_feature::glyph_rasterization,
        },
    });
}

quiz_vulkan::render::render_text_shaped_atlas_update_trace_request upload_ready_request()
{
    using namespace quiz_vulkan::render;

    return render_text_shaped_atlas_update_trace_request{
        .cluster_index = 3,
        .run_index = 1,
        .cluster_byte_offset = 4,
        .cluster_byte_count = 1,
        .shaped_glyph_ids = {U'A'},
        .resolved_face_id = 7,
        .cache_key = key_for_a(),
        .has_cache_key = true,
        .rasterizer_status = render_text_font_rasterizer_status::rasterized,
        .rasterized_payload_skipped = false,
        .payload_upload_ready = true,
        .payload_alpha_bytes = 64,
        .payload_rgba_bytes = 256,
        .has_atlas_placement = true,
        .page = render_text_atlas_page{
            .id = 2,
            .revision = 5,
            .width = 64,
            .height = 64,
        },
        .atlas_bounds = render_rect{1.0f, 2.0f, 8.0f, 8.0f},
        .has_atlas_update = true,
        .atlas_update_bounds = render_rect{1.0f, 2.0f, 8.0f, 8.0f},
        .atlas_update_rgba_bytes = 64U * 64U * 4U,
    };
}

quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot upload_ready_materialization(
    quiz_vulkan::render::glyph_atlas_key key = key_for_a())
{
    using namespace quiz_vulkan::render;

    return make_render_text_glyph_atlas_materialization(
        render_text_glyph_atlas_materialization_request{
            .cluster_index = 3,
            .run_index = 1,
            .cluster_byte_offset = 4,
            .cluster_byte_count = 1,
            .codepoint = key.glyph_id,
            .shaped_glyph_ids = {key.glyph_id},
            .resolved_glyph_id = key.glyph_id,
            .resolved_face_id = key.face_id,
            .cache_key = key,
            .has_cache_key = true,
            .glyph_supported = true,
            .glyph_id_from_selection = true,
            .glyph_id_matches_codepoint = true,
            .layout_bounds = render_rect{10.0f, 20.0f, 8.0f, 8.0f},
            .has_layout_bounds = true,
            .shaping_font_backend_library = render_text_font_backend_library::deterministic_fake,
            .shaping_font_backend_label = "Deterministic fake text backend",
            .shaping_font_backend_capability_status = render_text_font_backend_capability_status::fallback_only,
            .shaping_font_backend_used_deterministic_fallback = true,
            .shaping_font_backend_fallback_only = true,
            .raster_font_backend_library = render_text_font_backend_library::deterministic_fake,
            .raster_font_backend_label = "Deterministic fake text backend",
            .raster_font_backend_capability_status = render_text_font_backend_capability_status::fallback_only,
            .raster_font_backend_used_deterministic_fallback = true,
            .raster_font_backend_fallback_only = true,
            .rasterizer_status = render_text_font_rasterizer_status::rasterized,
            .raster_payload_matches_cache_key = true,
            .rasterized_payload_skipped = false,
            .payload_upload_ready = true,
            .payload_alpha_bytes = 64,
            .payload_rgba_bytes = 256,
            .has_atlas_placement = true,
            .page = render_text_atlas_page{
                .id = 2,
                .revision = 5,
                .width = 64,
                .height = 64,
            },
            .atlas_bounds = render_rect{1.0f, 2.0f, 8.0f, 8.0f},
            .has_atlas_update = true,
            .atlas_update_bounds = render_rect{1.0f, 2.0f, 8.0f, 8.0f},
            .atlas_update_rgba_bytes = 64U * 64U * 4U,
        });
}

quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot clean_reuse_materialization()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_request request{
        .cluster_index = 4,
        .run_index = 1,
        .cluster_byte_offset = 6,
        .cluster_byte_count = 1,
        .codepoint = U'C',
        .shaped_glyph_ids = {U'C'},
        .resolved_glyph_id = U'C',
        .resolved_face_id = 7,
        .cache_key = glyph_atlas_key{
            .face_id = 7,
            .glyph_id = U'C',
            .pixel_size = 20,
        },
        .has_cache_key = true,
        .glyph_supported = true,
        .layout_bounds = render_rect{18.0f, 20.0f, 4.0f, 4.0f},
        .has_layout_bounds = true,
        .rasterizer_status = render_text_font_rasterizer_status::rasterized,
        .raster_payload_matches_cache_key = true,
        .rasterized_payload_skipped = false,
        .payload_upload_ready = true,
        .payload_alpha_bytes = 16,
        .payload_rgba_bytes = 64,
        .has_atlas_placement = true,
        .page = render_text_atlas_page{
            .id = 2,
            .revision = 5,
            .width = 64,
            .height = 64,
        },
        .atlas_bounds = render_rect{3.0f, 4.0f, 4.0f, 4.0f},
        .has_atlas_update = false,
    };
    return make_render_text_glyph_atlas_materialization(std::move(request));
}

quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot mismatched_materialization()
{
    using namespace quiz_vulkan::render;

    return make_render_text_glyph_atlas_materialization(
        render_text_glyph_atlas_materialization_request{
            .cluster_index = 6,
            .run_index = 1,
            .cluster_byte_offset = 8,
            .cluster_byte_count = 1,
            .codepoint = U'D',
            .shaped_glyph_ids = {U'D'},
            .resolved_glyph_id = U'D',
            .resolved_face_id = 7,
            .cache_key = glyph_atlas_key{
                .face_id = 7,
                .glyph_id = U'D',
                .pixel_size = 20,
            },
            .has_cache_key = true,
            .glyph_supported = true,
            .rasterizer_status = render_text_font_rasterizer_status::rasterized,
            .raster_payload_matches_cache_key = true,
            .rasterized_payload_skipped = false,
            .payload_upload_ready = true,
            .payload_alpha_bytes = 4,
            .payload_rgba_bytes = 12,
            .has_atlas_placement = true,
            .page = render_text_atlas_page{
                .id = 3,
                .revision = 1,
                .width = 64,
                .height = 64,
            },
            .atlas_bounds = render_rect{5.0f, 6.0f, 2.0f, 2.0f},
            .has_atlas_update = true,
            .atlas_update_bounds = render_rect{5.0f, 6.0f, 2.0f, 2.0f},
            .atlas_update_rgba_bytes = 16,
        });
}

quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot unsupported_materialization(
    quiz_vulkan::render::glyph_atlas_key key)
{
    using namespace quiz_vulkan::render;

    return make_render_text_glyph_atlas_materialization(
        render_text_glyph_atlas_materialization_request{
            .cluster_index = 7,
            .run_index = 2,
            .cluster_byte_offset = 10,
            .cluster_byte_count = 1,
            .codepoint = key.glyph_id,
            .shaped_glyph_ids = {key.glyph_id},
            .resolved_glyph_id = key.glyph_id,
            .resolved_face_id = key.face_id,
            .cache_key = key,
            .has_cache_key = true,
            .glyph_supported = false,
        });
}

quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot missing_cache_key_materialization()
{
    using namespace quiz_vulkan::render;

    return make_render_text_glyph_atlas_materialization(
        render_text_glyph_atlas_materialization_request{
            .cluster_index = 8,
            .run_index = 2,
            .cluster_byte_offset = 12,
            .cluster_byte_count = 2,
            .codepoint = 0x0301U,
            .shaped_glyph_ids = {0x0301U},
            .resolved_glyph_id = 0x0301U,
            .resolved_face_id = 7,
            .glyph_supported = true,
        });
}

quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot real_backend_materialization(
    quiz_vulkan::render::glyph_atlas_key key)
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot materialization =
        upload_ready_materialization(key);
    materialization.shaping_font_backend_library = render_text_font_backend_library::harfbuzz;
    materialization.shaping_font_backend_label = "HarfBuzz";
    materialization.shaping_font_backend_capability_status =
        render_text_font_backend_capability_status::available;
    materialization.shaping_font_backend_used_deterministic_fallback = false;
    materialization.shaping_font_backend_fallback_only = false;
    materialization.raster_font_backend_library = render_text_font_backend_library::freetype;
    materialization.raster_font_backend_label = "FreeType";
    materialization.raster_font_backend_capability_status =
        render_text_font_backend_capability_status::available;
    materialization.raster_font_backend_used_deterministic_fallback = false;
    materialization.raster_font_backend_fallback_only = false;
    return materialization;
}

quiz_vulkan::render::render_text_glyph_atlas_materialization_policy_snapshot materialization_policy_for(
    const std::vector<quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot>& materializations)
{
    return quiz_vulkan::render::summarize_render_text_glyph_atlas_materialization_policy(materializations);
}

quiz_vulkan::render::render_text_style_catalog batch_style_catalog()
{
    using namespace quiz_vulkan::render;

    render_text_style_catalog catalog;
    catalog.fallback_style = render_text_style{
        .id = "fallback",
        .font_family = "Fallback Sans",
        .font_size = 16.0f,
        .line_height = 18.0f,
        .font_weight = 400,
    };
    catalog.styles.push_back(render_text_style{
        .id = "body",
        .font_family = "  Quiz Sans  ",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .font_weight = 500,
    });
    catalog.styles.push_back(render_text_style{
        .id = "body-duplicate",
        .font_family = "quiz   sans",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .font_weight = 500,
    });
    return catalog;
}

quiz_vulkan::render::render_text_request frame_snapshot_text_request()
{
    using namespace quiz_vulkan::render;

    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 100.0f, 40.0f};
    request.style_catalog = batch_style_catalog();
    return request;
}

quiz_vulkan::render::render_text_font_fallback_chain_plan_snapshot frame_snapshot_fallback_plan(
    quiz_vulkan::render::render_text_request request)
{
    using namespace quiz_vulkan::render;

    font_face_catalog catalog;
    catalog.add_face(font_face_descriptor{
        .id = 7,
        .family = "Quiz Sans",
        .source_uri = "fonts/quiz-sans.ttf",
        .coverage = {font_codepoint_range{.first = U'A', .last = U'Z'}},
        .weight = 500,
    });

    render_text_font_fallback_chain_plan_request fallback_request;
    fallback_request.items.push_back(
        make_render_text_font_fallback_chain_plan_item(std::move(request), "frame-source"));
    return plan_render_text_font_fallback_chains(fallback_request, catalog);
}

quiz_vulkan::render::render_text_frame_snapshot frame_snapshot_for_materializations(
    std::vector<quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot> materializations,
    const bool consumed = false)
{
    using namespace quiz_vulkan::render;

    const render_text_request request = frame_snapshot_text_request();
    render_text_glyph_atlas_materialization_policy_snapshot materialization_policy;
    std::vector<render_text_glyph_atlas_materialization_snapshot> policy_materializations;
    policy_materializations.reserve(materializations.size());
    for (render_text_glyph_atlas_materialization_snapshot materialization : materializations) {
        append_render_text_glyph_atlas_materialization(
            policy_materializations,
            materialization_policy,
            std::move(materialization));
    }

    const render_text_request_batch_plan_snapshot batch_plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(request, policy_materializations, "frame-source", "node-1"),
        });
    render_text_frame_snapshot snapshot =
        make_render_text_frame_snapshot(render_text_frame_snapshot_request{
            .frame_id = "frame-1",
            .source_label = "frame-source",
            .batch_plan = batch_plan,
            .fallback_chain_plan = frame_snapshot_fallback_plan(request),
            .materializations = policy_materializations,
            .materialization_policy = materialization_policy,
            .atlas_upload_bridge = bridge_render_text_atlas_upload_requests(batch_plan),
        });

    if (consumed) {
        snapshot = render_text_frame_snapshot_with_consumed_atlas_updates(
            snapshot,
            snapshot.queued_atlas_upload_request_ids,
            snapshot.queued_atlas_upload_request_ids.size());
    }
    return snapshot;
}

quiz_vulkan::render::render_text_frame_draw_plan_snapshot draw_plan_for_materializations(
    std::vector<quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot> materializations,
    const bool consumed = true)
{
    using namespace quiz_vulkan::render;

    const render_text_frame_snapshot frame =
        frame_snapshot_for_materializations(materializations, consumed);
    return plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
        .frame = frame,
        .materializations = std::move(materializations),
    });
}

quiz_vulkan::render::render_text_glyph_atlas_upload_result_snapshot upload_result_for_materializations(
    const std::vector<quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot>& materializations,
    const quiz_vulkan::render::render_text_frame_draw_plan_snapshot& draw_plan,
    const bool include_upload_request_ids)
{
    using namespace quiz_vulkan::render;

    std::vector<std::string> upload_request_ids;
    if (include_upload_request_ids) {
        for (const render_text_frame_draw_packet_snapshot& packet : draw_plan.packets) {
            if (!packet.atlas_upload_request_id.empty()) {
                upload_request_ids.push_back(packet.atlas_upload_request_id);
            }
        }
    }

    const render_text_glyph_atlas_upload_operation_plan_snapshot operation_plan =
        plan_render_text_glyph_atlas_upload_operations(
            plan_render_text_glyph_atlas_pages(materializations),
            materializations);
    return make_render_text_glyph_atlas_upload_result(render_text_glyph_atlas_upload_result_request{
        .operation_plan = operation_plan,
        .upload_request_ids = std::move(upload_request_ids),
    });
}

quiz_vulkan::render::render_text_frame_upload_handoff_snapshot handoff_for_materializations(
    const std::vector<quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot>& materializations,
    const bool consumed = true,
    const bool include_upload_request_ids = true)
{
    using namespace quiz_vulkan::render;

    const render_text_frame_snapshot frame =
        frame_snapshot_for_materializations(materializations, consumed);
    const render_text_frame_draw_plan_snapshot draw_plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = frame,
            .materializations = materializations,
        });
    const render_text_glyph_atlas_upload_result_snapshot upload_result =
        upload_result_for_materializations(materializations, draw_plan, include_upload_request_ids);
    return make_render_text_frame_upload_handoff(render_text_frame_upload_handoff_request{
        .frame = frame,
        .draw_plan = draw_plan,
        .upload_result = upload_result,
    });
}

void test_trace_reports_upload_ready_payload_queued()
{
    using namespace quiz_vulkan::render;

    const render_text_shaped_atlas_update_trace_snapshot trace =
        make_render_text_shaped_atlas_update_trace(upload_ready_request());

    require(
        trace.status == render_text_shaped_atlas_update_trace_status::upload_ready_payload_queued,
        "upload-ready request reports queued status");
    require(trace.queued, "queued trace marks queued");
    require(!trace.clean_page_reused, "queued trace does not mark clean reuse");
    require(trace.has_cache_key, "queued trace preserves cache key");
    require(trace.cache_key == key_for_a(), "queued trace preserves atlas key");
    require(trace.shaped_glyph_ids.size() == 1U && trace.shaped_glyph_ids.front() == U'A', "queued trace keeps glyph ids");
    require(trace.payload_byte_count_matches, "queued trace validates payload byte count");
    require(trace.expected_payload_rgba_bytes == 256U, "queued trace computes expected RGBA bytes");
    require(trace.atlas_update_rgba_bytes == 64U * 64U * 4U, "queued trace preserves atlas update bytes");

    std::vector<render_text_shaped_atlas_update_trace_snapshot> traces;
    render_text_shaped_atlas_update_trace_policy_snapshot policy;
    append_render_text_shaped_atlas_update_trace(traces, policy, trace);
    require(policy.trace_count == 1U, "trace policy counts one trace");
    require(policy.upload_ready_payload_queued_count == 1U, "trace policy counts queued payload");
    require(policy.traced_shaped_glyph_count == 1U, "trace policy counts shaped glyph ids");
    require(policy.upload_ready_payload_bytes == 256U, "trace policy totals upload-ready payload bytes");
    require(policy.queued_atlas_update_bytes == 64U * 64U * 4U, "trace policy totals queued update bytes");
}

void test_trace_reports_clean_reuse_when_update_is_absent()
{
    using namespace quiz_vulkan::render;

    render_text_shaped_atlas_update_trace_request request = upload_ready_request();
    request.has_atlas_update = false;
    request.atlas_update_rgba_bytes = 0;

    const render_text_shaped_atlas_update_trace_snapshot trace =
        make_render_text_shaped_atlas_update_trace(std::move(request));
    require(
        trace.status == render_text_shaped_atlas_update_trace_status::clean_atlas_page_reused,
        "clean atlas reuse status is reported when no update is queued");
    require(trace.clean_page_reused, "clean trace marks clean page reuse");
    require(!trace.queued, "clean trace does not mark queued");
}

void test_trace_reports_skipped_payload_and_missing_cache_key()
{
    using namespace quiz_vulkan::render;

    render_text_shaped_atlas_update_trace_request skipped = upload_ready_request();
    skipped.rasterizer_status = render_text_font_rasterizer_status::missing_font_bytes;
    skipped.rasterized_payload_skipped = true;
    skipped.payload_upload_ready = false;
    skipped.payload_alpha_bytes = 0;
    skipped.payload_rgba_bytes = 0;
    const render_text_shaped_atlas_update_trace_snapshot skipped_trace =
        make_render_text_shaped_atlas_update_trace(std::move(skipped));
    require(
        skipped_trace.status == render_text_shaped_atlas_update_trace_status::rasterized_payload_skipped,
        "skipped raster payload reports skipped status");
    require(!skipped_trace.payload_upload_ready, "skipped raster payload is not upload-ready");

    render_text_shaped_atlas_update_trace_request no_key = upload_ready_request();
    no_key.has_cache_key = false;
    no_key.payload_upload_ready = false;
    no_key.payload_alpha_bytes = 0;
    no_key.payload_rgba_bytes = 0;
    const render_text_shaped_atlas_update_trace_snapshot no_key_trace =
        make_render_text_shaped_atlas_update_trace(std::move(no_key));
    require(
        no_key_trace.status == render_text_shaped_atlas_update_trace_status::shaped_glyph_without_cache_key,
        "missing cache key reports shaped glyph without cache key");
    require(!no_key_trace.has_cache_key, "missing cache key trace preserves flag");
}

void test_trace_reports_payload_byte_count_mismatch()
{
    using namespace quiz_vulkan::render;

    render_text_shaped_atlas_update_trace_request request = upload_ready_request();
    request.payload_alpha_bytes = 4;
    request.payload_rgba_bytes = 12;
    const render_text_shaped_atlas_update_trace_snapshot trace =
        make_render_text_shaped_atlas_update_trace(std::move(request));
    require(
        trace.status == render_text_shaped_atlas_update_trace_status::payload_byte_count_mismatch,
        "payload byte mismatch status is reported");
    require(!trace.payload_byte_count_matches, "payload byte mismatch is recorded");
    require(trace.expected_payload_rgba_bytes == 16U, "payload byte mismatch records expected count");

    std::vector<render_text_shaped_atlas_update_trace_snapshot> traces;
    render_text_shaped_atlas_update_trace_policy_snapshot policy;
    append_render_text_shaped_atlas_update_trace(traces, policy, trace);
    require(policy.payload_byte_count_mismatch_count == 1U, "trace policy counts byte mismatches");
}

void test_batch_plan_normalizes_style_keys_and_layout_requests()
{
    using namespace quiz_vulkan::render;

    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A", .style_token = "body"},
        render_text_run{.text = "B", .style_token = "missing"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 100.0f, 40.0f};
    request.style_catalog = batch_style_catalog();
    request.options = render_text_options{.wrap = render_text_wrap_mode::word};

    render_text_batch_ref text_ref{
        .node_id = "ref-node",
        .text_runs = {
            render_text_run{.text = "C", .style_token = "body-duplicate"},
        },
        .bounds = render_rect{4.0f, 5.0f, 80.0f, 20.0f},
        .source_label = "ref-source",
    };

    render_text_request_batch_plan_snapshot plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(std::move(request), {}, "request-source", "request-node"),
            make_render_text_request_batch_item(std::move(text_ref), batch_style_catalog()),
        });

    require(plan.has_layout_requests(), "batch plan records layout requests");
    require(plan.layout_requests.size() == 2U, "batch plan keeps two layout items");
    require(plan.policy.item_count == 2U, "batch plan counts items");
    require(plan.policy.layout_request_count == 2U, "batch plan counts layout requests");
    require(plan.policy.text_run_count == 3U, "batch plan counts text runs");
    require(plan.policy.style_key_count == 3U, "batch plan counts style keys");
    require(plan.policy.unique_style_key_count == 2U, "batch plan deduplicates normalized style keys");
    require(plan.policy.fallback_style_count == 1U, "batch plan counts missing style fallback");
    require(
        plan.style_keys[0].normalized_font_family == "quiz sans",
        "style key normalization trims and folds font family");
    require(
        plan.style_keys[0].key == plan.style_keys[2].key,
        "style key normalization deduplicates equivalent font/style properties");
    require(plan.style_keys[1].used_fallback_style, "missing style records fallback key");
    require(plan.layout_requests[1].node_id == "ref-node", "text ref item preserves node id");
    require(plan.layout_requests[1].source_label == "ref-source", "text ref item preserves source label");
}

void test_batch_plan_deduplicates_glyph_atlas_materialization_work()
{
    using namespace quiz_vulkan::render;

    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 100.0f, 40.0f};
    request.style_catalog = batch_style_catalog();

    const render_text_glyph_atlas_materialization_snapshot materialization =
        upload_ready_materialization();

    render_text_request_batch_plan_snapshot plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(request, {materialization}, "first"),
            make_render_text_request_batch_item(std::move(request), {materialization}, "second"),
        });

    require(plan.has_atlas_update_requests(), "batch plan records atlas update requests");
    require(plan.atlas_update_requests.size() == 2U, "batch plan records both materialization requests");
    require(plan.unique_materialization_work.size() == 1U, "batch plan deduplicates identical atlas work");
    require(plan.policy.materialization_count == 2U, "batch plan counts materializations");
    require(plan.policy.unique_atlas_materialization_count == 1U, "batch plan counts unique materialization work");
    require(
        plan.policy.duplicate_atlas_materialization_count == 1U,
        "batch plan counts duplicate materialization work");
    require(!plan.atlas_update_requests[0].duplicate, "first atlas request is unique");
    require(plan.atlas_update_requests[1].duplicate, "second atlas request is duplicate");
    require(
        plan.atlas_update_requests[1].duplicate_of == plan.atlas_update_requests[0].unique_work_index,
        "duplicate atlas request points at unique work");
    require(
        plan.policy.planned_atlas_update_rgba_bytes == materialization.atlas_update_rgba_bytes,
        "batch plan totals queued atlas bytes once for deduped work");
}

void test_batch_plan_reports_fallback_real_backend_and_skipped_materializations()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot fallback_materialization =
        upload_ready_materialization();

    render_text_glyph_atlas_materialization_snapshot real_materialization =
        upload_ready_materialization(glyph_atlas_key{
            .face_id = 9,
            .glyph_id = U'B',
            .pixel_size = 20,
        });
    real_materialization.shaping_font_backend_library = render_text_font_backend_library::harfbuzz;
    real_materialization.shaping_font_backend_label = "HarfBuzz";
    real_materialization.shaping_font_backend_used_deterministic_fallback = false;
    real_materialization.shaping_font_backend_fallback_only = false;
    real_materialization.raster_font_backend_library = render_text_font_backend_library::freetype;
    real_materialization.raster_font_backend_label = "FreeType";
    real_materialization.raster_font_backend_used_deterministic_fallback = false;
    real_materialization.raster_font_backend_fallback_only = false;

    render_text_glyph_atlas_materialization_snapshot skipped =
        make_render_text_glyph_atlas_materialization(
            render_text_glyph_atlas_materialization_request{
                .cluster_index = 5,
                .run_index = 0,
                .codepoint = U'\u0301',
                .shaped_glyph_ids = {0x0301U},
                .resolved_glyph_id = 0x0301U,
                .glyph_supported = true,
                .rasterized_payload_skipped = true,
                .payload_upload_ready = false,
            });

    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "AB", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 100.0f, 40.0f};
    request.style_catalog = batch_style_catalog();

    render_text_request_batch_plan_snapshot plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(
                std::move(request),
                {fallback_materialization, real_materialization, skipped},
                "mixed"),
        });

    require(plan.policy.materialization_count == 3U, "batch plan counts mixed materializations");
    require(plan.policy.unique_atlas_materialization_count == 2U, "batch plan only dedupes materialized cache work");
    require(plan.policy.skipped_materialization_count == 1U, "batch plan counts skipped materialization");
    require(plan.policy.fallback_materialization_count == 1U, "batch plan counts deterministic fallback work");
    require(plan.policy.real_backend_materialization_count == 1U, "batch plan counts real backend work");
    require(plan.atlas_update_requests[0].used_deterministic_fallback, "first request records fallback backend");
    require(plan.atlas_update_requests[1].used_real_backend, "second request records real backend metadata");
    require(plan.atlas_update_requests[2].skipped, "third request records skipped materialization");
}

void test_freetype_raster_payload_materializes_upload_handoff_when_fixture_is_available()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path font_path = first_available_freetype_real_font_fixture();
    if (font_path.empty()) {
        return;
    }

    const std::vector<std::byte> font_bytes = read_binary_fixture_bytes(font_path);
    const font_face_descriptor face = freetype_fixture_face(font_path);
    const glyph_atlas_key requested_key = font_rasterizer_atlas_key_for(face, U'A', 32U);
    const render_text_font_rasterize_request raster_request =
        make_font_rasterize_request(
            face,
            requested_key,
            U'A',
            std::span<const std::byte>{font_bytes},
            font_path.generic_string());
    const function_table_font_backend_adapter adapter(render_text_font_backend_adapter_functions{
        .rasterize = freetype_real_font_backend_rasterize,
        .label = "FreeType memory-face glyph raster adapter",
    });
    const render_text_real_font_raster_adapter_result rasterized = adapter.rasterize(
        render_text_real_font_raster_adapter_request{
            .capability = freetype_raster_capability(),
            .library = render_text_font_backend_library::freetype,
            .rasterize = raster_request,
        });

    if (!render_text_freetype_memory_face_adapter_available()) {
        require(
            rasterized.status == render_text_font_backend_adapter_status::backend_unavailable,
            "FreeType atlas handoff fixture reports unavailable backend without compiled FreeType");
        return;
    }

    require(rasterized.ok(), "FreeType raster callback produces raster evidence for atlas handoff");
    require(
        rasterized.rasterized.key.glyph_id == rasterized.rasterized.glyph_id,
        "FreeType raster evidence keys atlas payloads by resolved glyph index");

    const render_text_font_atlas_glyph_payload payload =
        make_font_rasterizer_atlas_payload(rasterized.rasterized);
    require(payload.upload_ready, "FreeType raster payload is upload-ready before atlas placement");

    glyph_atlas_cache atlas(glyph_atlas_page_config{.width = 128, .height = 128, .padding = 1});
    const std::optional<glyph_atlas_slot> slot =
        atlas.cache_glyph(rasterized.rasterized.key, payload.width, payload.height);
    require(slot.has_value(), "FreeType raster payload receives atlas placement");

    const std::vector<render_text_atlas_update> updates = atlas.consume_dirty_page_updates();
    require(updates.size() == 1U, "FreeType raster payload produces one dirty atlas update");

    const render_text_glyph_atlas_materialization_snapshot materialization =
        make_render_text_glyph_atlas_materialization_from_raster_result(
            render_text_glyph_atlas_materialization_from_raster_request{
                .cluster_index = 0,
                .run_index = 0,
                .cluster_byte_offset = 0,
                .cluster_byte_count = 1,
                .shaped_glyph_ids = {rasterized.rasterized.glyph_id},
                .glyph_id_from_selection = true,
                .layout_bounds = render_rect{
                    10.0f,
                    20.0f,
                    static_cast<float>(payload.width),
                    static_cast<float>(payload.height),
                },
                .has_layout_bounds = true,
                .shaping_font_backend_library = render_text_font_backend_library::harfbuzz,
                .shaping_font_backend_label = "HarfBuzz",
                .shaping_font_backend_capability_status =
                    render_text_font_backend_capability_status::available,
                .raster_font_backend_library = render_text_font_backend_library::freetype,
                .raster_font_backend_label = "FreeType",
                .raster_font_backend_capability_status =
                    render_text_font_backend_capability_status::available,
                .rasterized = rasterized.rasterized,
                .has_atlas_placement = true,
                .page = slot->page,
                .atlas_bounds = slot->atlas_bounds,
                .has_atlas_update = true,
                .atlas_update = updates.front(),
            });

    require(
        materialization.status == render_text_glyph_atlas_materialization_status::materialized_upload_ready,
        "FreeType raster evidence becomes upload-ready atlas materialization");
    require(materialization.materialized, "FreeType atlas materialization is marked materialized");
    require(materialization.queued, "FreeType atlas materialization queues an update");
    require(materialization.raster_payload_matches_cache_key, "FreeType payload matches resolved atlas cache key");
    require(materialization.payload_alpha_bytes == payload.alpha.size(), "materialization records alpha bytes");
    require(materialization.payload_rgba_bytes == payload.rgba.size(), "materialization records RGBA bytes");
    require(
        materialization.atlas_update_rgba_bytes == updates.front().rgba.size(),
        "materialization records atlas update bytes");
    require(
        render_text_glyph_atlas_materialization_uses_real_backend(materialization),
        "materialization records real FreeType backend use");

    const render_text_request_batch_plan_snapshot plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(
                frame_snapshot_text_request(),
                {materialization},
                "freetype-atlas-handoff",
                "node-freetype"),
        });
    require(plan.policy.real_backend_materialization_count == 1U, "batch plan counts FreeType materialization");
    require(plan.policy.unique_atlas_materialization_count == 1U, "batch plan sees one unique FreeType atlas work item");
    require(plan.atlas_update_requests.front().used_real_backend, "batch atlas request keeps FreeType backend evidence");

    const render_text_atlas_upload_request_bridge_snapshot bridge =
        bridge_render_text_atlas_upload_requests(plan);
    require(bridge.has_upload_requests(), "FreeType materialization produces upload bridge work");
    require(bridge.policy.upload_request_count == 1U, "upload bridge counts one FreeType upload request");
    require(
        bridge.policy.total_upload_rgba_bytes == materialization.atlas_update_rgba_bytes,
        "upload bridge preserves FreeType atlas update byte evidence");
    require(
        bridge.requests.front().status == render_text_atlas_upload_request_status::upload_ready,
        "upload bridge marks FreeType atlas update ready for handoff");
}

void test_freetype_raster_payload_handoff_reports_missing_byte_fallback()
{
    using namespace quiz_vulkan::render;

    const font_face_descriptor face{
        .id = 88,
        .family = "Missing FreeType Sans",
        .source_uri = "fonts/missing-freetype.ttf",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'A'},
        },
    };
    const render_text_font_rasterize_request raster_request =
        make_font_rasterize_request(
            face,
            U'A',
            24U,
            std::span<const std::byte>{},
            "missing-freetype.ttf");
    const render_text_real_font_raster_adapter_result rasterized =
        freetype_real_font_backend_rasterize(render_text_real_font_raster_adapter_request{
            .capability = freetype_raster_capability(),
            .library = render_text_font_backend_library::freetype,
            .rasterize = raster_request,
        });

    require(!rasterized.ok(), "missing bytes do not produce FreeType atlas upload evidence");
    require(
        rasterized.rasterized.status == render_text_font_rasterizer_status::missing_font_bytes,
        "missing bytes preserve rasterizer missing-byte status");

    const render_text_glyph_atlas_materialization_snapshot materialization =
        make_render_text_glyph_atlas_materialization_from_raster_result(
            render_text_glyph_atlas_materialization_from_raster_request{
                .cluster_index = 0,
                .run_index = 0,
                .cluster_byte_offset = 0,
                .cluster_byte_count = 1,
                .shaped_glyph_ids = {U'A'},
                .raster_font_backend_library = render_text_font_backend_library::freetype,
                .raster_font_backend_label = "FreeType",
                .raster_font_backend_capability_status =
                    render_text_font_backend_capability_status::available,
                .rasterized = rasterized.rasterized,
            });

    require(
        materialization.status == render_text_glyph_atlas_materialization_status::skipped_raster_payload,
        "missing-byte FreeType raster evidence becomes skipped raster payload materialization");
    require(!materialization.materialized, "missing-byte materialization is not upload-ready");
    require(materialization.payload_alpha_bytes == 0U, "missing-byte materialization has no alpha payload");
    require(materialization.payload_rgba_bytes == 0U, "missing-byte materialization has no RGBA payload");
    require(
        materialization.diagnostic.find("raster payload") != std::string::npos,
        "missing-byte materialization diagnostic names raster payload");

    const render_text_request_batch_plan_snapshot plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(
                frame_snapshot_text_request(),
                {materialization},
                "missing-freetype-atlas-handoff",
                "node-missing-freetype"),
        });
    require(plan.policy.skipped_materialization_count == 1U, "batch plan counts skipped FreeType materialization");

    const render_text_atlas_upload_request_bridge_snapshot bridge =
        bridge_render_text_atlas_upload_requests(plan);
    require(!bridge.has_upload_requests(), "missing-byte FreeType materialization produces no upload request");
    require(
        bridge.policy.skipped_materialization_count == 1U,
        "upload bridge counts skipped FreeType materialization");
}

void test_glyph_atlas_materialization_diff_reports_stable_key_changes()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot upload_ready =
        upload_ready_materialization();
    render_text_glyph_atlas_materialization_snapshot clean_reuse = upload_ready;
    clean_reuse.status = render_text_glyph_atlas_materialization_status::materialized_clean_reuse;
    clean_reuse.has_atlas_update = false;
    clean_reuse.atlas_update_rgba_bytes = 0U;
    clean_reuse.queued = false;
    clean_reuse.clean_reuse = true;

    const render_text_glyph_atlas_materialization_diff_snapshot diff =
        diff_render_text_glyph_atlas_materializations(&upload_ready, &clean_reuse);

    require(
        diff.diff_status == render_text_glyph_atlas_materialization_diff_status::changed,
        "materialization diff reports changed status for stable cache key");
    require(diff.key.has_cache_key, "materialization diff prefers cache key identity");
    require(diff.key.stable_id == "cache:7:65:20", "materialization diff stable id uses cache key");
    require(diff.status_changed, "materialization diff records status change");
    require(diff.upload_ready_changed, "materialization diff records upload-ready transition");
    require(diff.clean_reuse_changed, "materialization diff records clean-reuse transition");
    require(diff.became_clean_reuse, "materialization diff records clean-reuse arrival");
    require(diff.stopped_upload_ready, "materialization diff records upload-ready exit");
    require(diff.atlas_update_rgba_bytes_delta == -16384, "materialization diff records upload byte delta");

    const render_text_glyph_atlas_materialization_snapshot missing =
        missing_cache_key_materialization();
    const render_text_glyph_atlas_materialization_diff_key missing_key =
        render_text_glyph_atlas_materialization_diff_key_for(missing);
    require(!missing_key.has_cache_key, "missing-cache diff key records no cache key");
    require(
        missing_key.stable_id == "run:2:cluster:8:bytes:12:2:glyph:769:face:7",
        "missing-cache diff key falls back to run and cluster identity");
}

void test_glyph_atlas_materialization_policy_diff_reports_byte_and_status_deltas()
{
    using namespace quiz_vulkan::render;

    const std::vector<render_text_glyph_atlas_materialization_snapshot> before = {
        upload_ready_materialization(),
        clean_reuse_materialization(),
    };
    std::vector<render_text_glyph_atlas_materialization_snapshot> after = {
        upload_ready_materialization(),
        mismatched_materialization(),
        missing_cache_key_materialization(),
    };
    after.front().payload_rgba_bytes = 512U;

    const render_text_glyph_atlas_materialization_policy_diff_snapshot diff =
        diff_render_text_glyph_atlas_materialization_policies(
            materialization_policy_for(before),
            materialization_policy_for(after));

    require(diff.has_changes, "materialization policy diff records changed policy");
    require(diff.request_count_delta == 1, "materialization policy diff counts request delta");
    require(diff.materialized_count_delta == -1, "materialization policy diff counts materialized delta");
    require(diff.upload_ready_count_delta == 0, "materialization policy diff keeps upload-ready count stable");
    require(diff.clean_reuse_count_delta == -1, "materialization policy diff counts clean-reuse loss");
    require(diff.skipped_count_delta == 2, "materialization policy diff counts skipped increase");
    require(diff.missing_cache_key_count_delta == 1, "materialization policy diff counts missing-cache increase");
    require(
        diff.payload_byte_count_mismatch_count_delta == 1,
        "materialization policy diff counts payload mismatch increase");
    require(diff.total_rgba_bytes_delta == 204, "materialization policy diff records total RGBA delta");
    require(
        diff.summary == "glyph atlas materialization policy diff: 2 -> 3 requests",
        "materialization policy diff summary is stable");
}

void test_glyph_atlas_materialization_batch_diff_reports_regressions_recoveries_and_backend_transitions()
{
    using namespace quiz_vulkan::render;

    const glyph_atlas_key unsupported_key{
        .face_id = 7,
        .glyph_id = U'E',
        .pixel_size = 20,
    };
    const glyph_atlas_key mismatch_key{
        .face_id = 7,
        .glyph_id = U'D',
        .pixel_size = 20,
    };
    const glyph_atlas_key added_key{
        .face_id = 9,
        .glyph_id = U'B',
        .pixel_size = 20,
    };

    std::vector<render_text_glyph_atlas_materialization_snapshot> before = {
        upload_ready_materialization(),
        unsupported_materialization(unsupported_key),
        mismatched_materialization(),
        clean_reuse_materialization(),
    };
    before[2].cache_key = mismatch_key;
    before[2].resolved_glyph_id = mismatch_key.glyph_id;
    before[2].resolved_face_id = mismatch_key.face_id;

    render_text_glyph_atlas_materialization_snapshot real_a =
        real_backend_materialization(key_for_a());
    real_a.payload_alpha_bytes = 128U;
    real_a.payload_rgba_bytes = 512U;

    std::vector<render_text_glyph_atlas_materialization_snapshot> after = {
        real_a,
        upload_ready_materialization(unsupported_key),
        upload_ready_materialization(mismatch_key),
        missing_cache_key_materialization(),
        upload_ready_materialization(added_key),
    };

    const render_text_glyph_atlas_materialization_batch_diff_snapshot diff =
        diff_render_text_glyph_atlas_materialization_batches(before, after);

    require(diff.has_changes(), "materialization batch diff reports changed batch");
    require(diff.added_count == 2U, "materialization batch diff counts added entries");
    require(diff.removed_count == 1U, "materialization batch diff counts removed entries");
    require(diff.changed_count == 3U, "materialization batch diff counts changed entries");
    require(diff.upload_ready_transition_count == 3U, "materialization batch diff counts upload transitions");
    require(diff.clean_reuse_transition_count == 1U, "materialization batch diff counts clean-reuse transitions");
    require(diff.skipped_regression_count == 1U, "materialization batch diff counts skipped regression");
    require(diff.skipped_recovery_count == 2U, "materialization batch diff counts skipped recoveries");
    require(
        diff.unsupported_glyph_recovery_count == 1U,
        "materialization batch diff counts unsupported glyph recovery");
    require(
        diff.missing_cache_key_regression_count == 1U,
        "materialization batch diff counts missing-cache regression");
    require(
        diff.payload_byte_count_mismatch_recovery_count == 1U,
        "materialization batch diff counts payload mismatch recovery");
    require(
        diff.deterministic_fallback_to_real_backend_count == 1U,
        "materialization batch diff counts fallback-to-real backend transition");
    require(diff.policy_diff.request_count_delta == 1, "batch diff carries policy request delta");
    require(
        diff.policy_diff.missing_cache_key_count_delta == 1,
        "batch diff carries policy missing-cache delta");
    require(
        diff.policy_diff.unsupported_glyph_count_delta == -1,
        "batch diff carries policy unsupported recovery delta");
    require(
        diff.policy_diff.payload_byte_count_mismatch_count_delta == -1,
        "batch diff carries policy payload mismatch recovery delta");
    require(
        diff.summary == "glyph atlas materialization batch diff: 2 added, 1 removed, 3 changed",
        "materialization batch diff summary is stable");
}

void test_glyph_atlas_page_plan_reports_selection_reuse_and_upload_totals()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot first =
        upload_ready_materialization();
    render_text_glyph_atlas_materialization_snapshot second =
        upload_ready_materialization(glyph_atlas_key{
            .face_id = 7,
            .glyph_id = U'B',
            .pixel_size = 20,
        });
    second.page = first.page;
    second.atlas_bounds = render_rect{12.0f, 2.0f, 8.0f, 8.0f};
    second.atlas_update_bounds = render_rect{12.0f, 2.0f, 8.0f, 8.0f};

    const render_text_glyph_atlas_materialization_snapshot clean =
        clean_reuse_materialization();
    const render_text_atlas_update pending_update{
        .page = render_text_atlas_page{
            .id = 9,
            .revision = 1,
            .width = 64,
            .height = 64,
        },
        .updated_bounds = render_rect{0.0f, 0.0f, 2.0f, 4.0f},
        .rgba = std::vector<unsigned char>(32U, 0xffU),
    };

    const render_text_glyph_atlas_page_plan_snapshot plan =
        plan_render_text_glyph_atlas_pages(render_text_glyph_atlas_page_plan_request{
            .materializations = {first, second, clean},
            .pending_updates = {pending_update},
            .constraints = render_text_glyph_atlas_page_plan_constraints{
                .width = 64,
                .height = 64,
                .padding = 1,
            },
        });

    require(plan.ok(), "page plan succeeds for placed materializations");
    require(plan.entries.size() == 3U, "page plan records every materialization entry");
    require(plan.policy.page_count == 2U, "page plan includes pending-update and materialization pages");
    require(plan.policy.allocated_new_page_count == 1U, "page plan records first materialization page allocation");
    require(plan.policy.selected_existing_page_count == 1U, "page plan records existing page selection");
    require(plan.policy.reused_placement_count == 1U, "page plan records clean placement reuse");
    require(plan.policy.pending_update_count == 1U, "page plan counts pending atlas updates");
    require(plan.policy.pending_update_rgba_bytes == 32U, "page plan totals pending update bytes");
    require(
        plan.policy.materialization_upload_rgba_bytes == (64U * 64U * 4U * 2U),
        "page plan totals upload-ready materialization bytes");
    require(
        plan.policy.total_upload_rgba_bytes == (64U * 64U * 4U * 2U) + 32U,
        "page plan combines materialization and pending update bytes");
    require(
        plan.entries[0].status == render_text_glyph_atlas_page_plan_status::allocated_new_page,
        "first materialization allocates a page in the diagnostic plan");
    require(
        plan.entries[1].status == render_text_glyph_atlas_page_plan_status::selected_existing_page,
        "second materialization selects an existing page");
    require(
        plan.entries[2].status == render_text_glyph_atlas_page_plan_status::reused_existing_placement,
        "clean reuse materialization reports reused placement");
    require(plan.policy.estimated_occupancy > 0.0f, "page plan estimates nonzero occupancy");
    require(plan.policy.estimated_fragmentation < 1.0f, "page plan estimates non-full fragmentation");
}

void test_glyph_atlas_page_plan_reports_overflow_and_eviction_hint()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot first =
        upload_ready_materialization();
    render_text_glyph_atlas_materialization_snapshot oversized =
        upload_ready_materialization(glyph_atlas_key{
            .face_id = 7,
            .glyph_id = U'Z',
            .pixel_size = 20,
        });
    oversized.has_atlas_placement = false;
    oversized.page = render_text_atlas_page{};
    oversized.atlas_bounds = render_rect{};
    oversized.atlas_update_bounds = render_rect{0.0f, 0.0f, 32.0f, 32.0f};

    const render_text_glyph_atlas_page_plan_snapshot plan =
        plan_render_text_glyph_atlas_pages(render_text_glyph_atlas_page_plan_request{
            .materializations = {first, oversized},
            .constraints = render_text_glyph_atlas_page_plan_constraints{
                .width = 16,
                .height = 16,
                .padding = 1,
                .max_pages = 1,
            },
        });

    require(!plan.ok(), "page plan reports overflow as not ok");
    require(plan.has_overflow(), "page plan exposes overflow flag");
    require(plan.policy.overflow_count == 1U, "page plan counts overflow");
    require(plan.policy.eviction_candidate_count == 1U, "page plan counts eviction hint");
    require(plan.entries[1].overflow, "oversized entry records overflow");
    require(plan.entries[1].has_eviction_candidate, "oversized entry exposes eviction candidate");
    require(!plan.pages.empty() && plan.pages.front().overflow, "page plan marks an overflow-related page");
    require(
        plan.entries[1].eviction_candidate_key == key_for_a(),
        "overflow eviction candidate uses deterministic first placed glyph");
}

void test_glyph_atlas_page_plan_skips_unsupported_materializations()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot unsupported =
        unsupported_materialization(glyph_atlas_key{
            .face_id = 7,
            .glyph_id = U'?',
            .pixel_size = 20,
        });
    const render_text_glyph_atlas_page_plan_snapshot plan =
        plan_render_text_glyph_atlas_pages({unsupported});

    require(plan.ok(), "unsupported materialization skip is not page overflow");
    require(plan.entries.size() == 1U, "page plan records skipped entry");
    require(
        plan.entries.front().status == render_text_glyph_atlas_page_plan_status::skipped_materialization,
        "unsupported materialization is skipped by page planner");
    require(plan.policy.skipped_count == 1U, "page plan counts skipped materialization");
    require(plan.policy.page_count == 0U, "skipped materialization does not claim atlas page");
    require(plan.policy.total_upload_rgba_bytes == 0U, "skipped materialization does not claim upload bytes");
}

void test_glyph_atlas_upload_operation_plan_emits_upload_and_clean_reuse_packets()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot upload =
        upload_ready_materialization();
    const render_text_glyph_atlas_materialization_snapshot clean =
        clean_reuse_materialization();
    const std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {
        upload,
        clean,
    };
    const render_text_glyph_atlas_page_plan_snapshot page_plan =
        plan_render_text_glyph_atlas_pages(materializations);

    const render_text_glyph_atlas_upload_operation_plan_snapshot upload_plan =
        plan_render_text_glyph_atlas_upload_operations(page_plan, materializations);

    require(upload_plan.operations.size() == 2U, "upload operation plan records upload and reuse packets");
    require(upload_plan.has_uploads(), "upload operation plan reports pending uploads");
    require(upload_plan.ok(), "upload operation plan with clean reuse has no blockers");
    require(upload_plan.policy.upload_ready_count == 1U, "upload operation plan counts upload packet");
    require(upload_plan.policy.clean_reuse_count == 1U, "upload operation plan counts clean reuse packet");
    require(upload_plan.policy.blocked_count == 0U, "upload operation plan has no blockers");
    require(
        upload_plan.policy.total_upload_rgba_bytes == upload.atlas_update_rgba_bytes,
        "upload operation plan totals upload-ready RGBA bytes");
    require(upload_plan.policy.dirty_page_count == 1U, "upload operation plan counts dirty page");
    require(upload_plan.policy.clean_reuse_page_count == 1U, "upload operation plan counts clean reuse page");

    const render_text_glyph_atlas_upload_operation_packet& packet =
        upload_plan.operations.front();
    require(
        packet.status == render_text_glyph_atlas_upload_operation_status::upload_ready,
        "first operation is upload-ready");
    require(packet.uploadable(), "upload-ready packet reports uploadable");
    require(packet.dirty_upload, "upload-ready packet marks dirty upload");
    require(packet.page_id == upload.page.id, "upload-ready packet exposes stable page id");
    require(packet.stable_page_id == "page:2", "upload-ready packet exposes stable page label");
    require(packet.cache_key == upload.cache_key, "upload-ready packet preserves cache key");
    require(packet.has_update_bounds, "upload-ready packet exposes update bounds");
    require(
        packet.update_bounds.x == upload.atlas_update_bounds.x,
        "upload-ready packet preserves update rect");
    require(
        packet.rgba_byte_count == upload.atlas_update_rgba_bytes,
        "upload-ready packet preserves RGBA byte count");

    const render_text_glyph_atlas_upload_operation_packet& clean_packet =
        upload_plan.operations.back();
    require(
        clean_packet.status == render_text_glyph_atlas_upload_operation_status::clean_reuse,
        "second operation is clean reuse");
    require(clean_packet.clean_reuse, "clean reuse packet marks clean reuse");
    require(!clean_packet.dirty_upload, "clean reuse packet is not dirty");
    require(clean_packet.rgba_byte_count == 0U, "clean reuse packet has no upload bytes");
}

void test_glyph_atlas_upload_operation_plan_reports_overflow_and_missing_cache_blockers()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot first =
        upload_ready_materialization();
    render_text_glyph_atlas_materialization_snapshot oversized =
        upload_ready_materialization(glyph_atlas_key{
            .face_id = 7,
            .glyph_id = U'Z',
            .pixel_size = 20,
        });
    oversized.has_atlas_placement = false;
    oversized.page = render_text_atlas_page{};
    oversized.atlas_bounds = render_rect{};
    oversized.atlas_update_bounds = render_rect{0.0f, 0.0f, 32.0f, 32.0f};

    const std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {
        first,
        oversized,
        missing_cache_key_materialization(),
    };
    const render_text_glyph_atlas_page_plan_snapshot page_plan =
        plan_render_text_glyph_atlas_pages(render_text_glyph_atlas_page_plan_request{
            .materializations = materializations,
            .constraints = render_text_glyph_atlas_page_plan_constraints{
                .width = 16,
                .height = 16,
                .padding = 1,
                .max_pages = 1,
            },
        });

    const render_text_glyph_atlas_upload_operation_plan_snapshot upload_plan =
        plan_render_text_glyph_atlas_upload_operations(page_plan, materializations);

    require(!upload_plan.ok(), "upload operation plan with blockers is not ok");
    require(upload_plan.has_blockers(), "upload operation plan exposes blockers");
    require(upload_plan.policy.upload_ready_count == 1U, "upload operation plan keeps uploadable packet");
    require(upload_plan.policy.blocked_count == 2U, "upload operation plan counts blockers");
    require(upload_plan.policy.overflow_count == 1U, "upload operation plan counts overflow blocker");
    require(upload_plan.policy.skipped_count == 1U, "upload operation plan counts skipped blocker");
    require(upload_plan.policy.blocked_page_count == 1U, "upload operation plan counts blocked page");

    require(
        upload_plan.operations[1].status == render_text_glyph_atlas_upload_operation_status::blocked_overflow,
        "oversized operation reports overflow blocker");
    require(upload_plan.operations[1].overflow, "oversized operation marks overflow");
    require(
        upload_plan.operations[1].blocker_reason == "atlas page overflow",
        "oversized operation explains overflow blocker");
    require(
        upload_plan.operations[2].status
            == render_text_glyph_atlas_upload_operation_status::blocked_missing_cache_key,
        "missing-cache operation reports missing cache key blocker");
    require(!upload_plan.operations[2].has_cache_key, "missing-cache operation preserves cache-key flag");
}

void test_glyph_atlas_upload_operation_plan_reports_payload_mismatch_blocker()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot mismatch =
        mismatched_materialization();
    const std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {
        mismatch,
    };
    const render_text_glyph_atlas_page_plan_snapshot page_plan =
        plan_render_text_glyph_atlas_pages(materializations);

    const render_text_glyph_atlas_upload_operation_plan_snapshot upload_plan =
        plan_render_text_glyph_atlas_upload_operations(page_plan, materializations);

    require(upload_plan.operations.size() == 1U, "mismatch creates one diagnostic operation");
    require(!upload_plan.ok(), "payload mismatch blocks upload operation plan");
    require(
        upload_plan.operations.front().status
            == render_text_glyph_atlas_upload_operation_status::blocked_payload_byte_count_mismatch,
        "payload mismatch operation reports byte-count blocker");
    require(
        upload_plan.operations.front().payload_byte_count_mismatch,
        "payload mismatch operation marks byte-count mismatch");
    require(upload_plan.policy.payload_byte_count_mismatch_count == 1U, "policy counts payload mismatch");
    require(upload_plan.policy.total_upload_rgba_bytes == 0U, "blocked payload mismatch does not claim bytes");
}

void test_glyph_atlas_upload_result_accepts_upload_and_clean_reuse_packets()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot upload =
        upload_ready_materialization();
    const render_text_glyph_atlas_materialization_snapshot clean =
        clean_reuse_materialization();
    const std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {
        upload,
        clean,
    };
    const render_text_glyph_atlas_upload_operation_plan_snapshot operation_plan =
        plan_render_text_glyph_atlas_upload_operations(
            plan_render_text_glyph_atlas_pages(materializations),
            materializations);

    const render_text_glyph_atlas_upload_result_snapshot result =
        make_render_text_glyph_atlas_upload_result(render_text_glyph_atlas_upload_result_request{
            .operation_plan = operation_plan,
            .upload_request_ids = {"upload-request:page-2:A"},
        });

    require(result.ok(), "accepted upload result has no rejections");
    require(result.has_uploads(), "accepted upload result reports uploads");
    require(result.policy.operation_count == 2U, "upload result counts operation packets");
    require(result.policy.accepted_packet_count == 2U, "upload result accepts upload and clean reuse");
    require(result.policy.rejected_packet_count == 0U, "upload result has no rejected packets");
    require(result.policy.accepted_upload_count == 1U, "upload result counts accepted upload");
    require(result.policy.accepted_clean_reuse_count == 1U, "upload result counts accepted clean reuse");
    require(result.policy.upload_request_id_count == 1U, "upload result counts upload bridge ids");
    require(result.policy.materialized_glyph_count == 1U, "upload result counts materialized glyphs");
    require(result.policy.reused_glyph_count == 1U, "upload result counts reused glyphs");
    require(result.policy.total_upload_rgba_bytes == upload.atlas_update_rgba_bytes, "upload result totals page bytes");
    require(result.policy.page_count == 1U, "upload result keeps stable page summaries");
    require(result.pages.front().stable_page_id == "page:2", "upload result preserves stable page id");
    require(result.pages.front().upload_rgba_bytes == upload.atlas_update_rgba_bytes, "page summary totals bytes");

    const render_text_glyph_atlas_upload_result_packet_snapshot& upload_packet =
        result.packets.front();
    require(
        upload_packet.result_status == render_text_glyph_atlas_upload_result_status::accepted_upload,
        "upload packet is accepted");
    require(upload_packet.upload_request_id == "upload-request:page-2:A", "upload packet preserves request id");
    require(upload_packet.rgba_byte_count == upload.atlas_update_rgba_bytes, "upload packet preserves bytes");

    const render_text_glyph_atlas_upload_result_packet_snapshot& clean_packet =
        result.packets.back();
    require(
        clean_packet.result_status == render_text_glyph_atlas_upload_result_status::accepted_clean_reuse,
        "clean reuse packet is accepted");
    require(clean_packet.clean_reuse_accepted, "clean reuse packet records reuse");
}

void test_glyph_atlas_upload_result_rejects_blockers_and_missing_upload_ids()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot upload =
        upload_ready_materialization();
    const render_text_glyph_atlas_materialization_snapshot missing =
        missing_cache_key_materialization();
    const std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {
        upload,
        missing,
    };
    const render_text_glyph_atlas_upload_operation_plan_snapshot operation_plan =
        plan_render_text_glyph_atlas_upload_operations(
            plan_render_text_glyph_atlas_pages(materializations),
            materializations);

    const render_text_glyph_atlas_upload_result_snapshot result =
        make_render_text_glyph_atlas_upload_result(render_text_glyph_atlas_upload_result_request{
            .operation_plan = operation_plan,
        });

    require(!result.ok(), "upload result reports missing ids and blockers");
    require(result.has_rejections(), "upload result exposes rejections");
    require(result.policy.accepted_packet_count == 0U, "upload result accepts no packets without ids");
    require(result.policy.rejected_packet_count == 2U, "upload result rejects both packets");
    require(
        result.policy.rejected_missing_upload_request_id_count == 1U,
        "upload result counts missing upload request id");
    require(result.policy.rejected_blocked_packet_count == 1U, "upload result counts blocked packet");
    require(result.policy.blocker_count == 2U, "upload result counts blocker reasons");
    require(result.policy.missing_glyph_count == 1U, "upload result counts missing glyph/cache key");
    require(result.policy.total_upload_rgba_bytes == 0U, "rejected upload result claims no bytes");
    require(
        result.packets.front().result_status
            == render_text_glyph_atlas_upload_result_status::rejected_missing_upload_request_id,
        "upload packet without request id is rejected");
    require(result.packets.back().missing_cache_key, "missing-cache packet records missing glyph");
}

void test_glyph_atlas_upload_result_diff_reports_changed_packet_and_page_summaries()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot upload =
        upload_ready_materialization();
    const std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {
        upload,
    };
    const render_text_glyph_atlas_upload_operation_plan_snapshot operation_plan =
        plan_render_text_glyph_atlas_upload_operations(
            plan_render_text_glyph_atlas_pages(materializations),
            materializations);
    const render_text_glyph_atlas_upload_result_snapshot accepted =
        make_render_text_glyph_atlas_upload_result(render_text_glyph_atlas_upload_result_request{
            .operation_plan = operation_plan,
            .upload_request_ids = {"upload-request:page-2:A"},
        });
    const render_text_glyph_atlas_upload_result_snapshot rejected =
        make_render_text_glyph_atlas_upload_result(render_text_glyph_atlas_upload_result_request{
            .operation_plan = operation_plan,
        });

    const render_text_glyph_atlas_upload_result_diff_snapshot diff =
        diff_render_text_glyph_atlas_upload_results(accepted, rejected);

    require(diff.has_changes(), "upload result diff reports changed result");
    require(diff.changed_packet_count == 1U, "upload result diff counts changed packet");
    require(diff.changed_page_count == 1U, "upload result diff counts changed page");
    require(diff.changed_packet_ids.size() == 1U, "upload result diff records changed packet id");
    require(diff.changed_page_ids.size() == 1U, "upload result diff records changed page id");
    require(diff.policy.accepted_packet_count_delta == -1, "upload result diff records accepted loss");
    require(diff.policy.rejected_packet_count_delta == 1, "upload result diff records rejection increase");
    require(
        diff.policy.total_upload_rgba_bytes_delta
            == -static_cast<std::ptrdiff_t>(upload.atlas_update_rgba_bytes),
        "upload result diff records upload byte loss");
    require(diff.packet_diffs.front().upload_request_id_changed, "packet diff records request id change");
    require(diff.packet_diffs.front().result_status_changed, "packet diff records status change");
    require(diff.page_diffs.front().upload_byte_count_changed, "page diff records byte change");
    require(diff.page_diffs.front().blocker_count_changed, "page diff records blocker change");
}

void test_text_frame_upload_handoff_links_draw_packets_to_upload_results()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot upload =
        upload_ready_materialization();
    const render_text_glyph_atlas_materialization_snapshot clean =
        clean_reuse_materialization();
    const std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {
        upload,
        clean,
    };

    const render_text_frame_upload_handoff_snapshot handoff =
        handoff_for_materializations(materializations);

    require(handoff.ok(), "upload handoff is ready when draw packets and upload results are accepted");
    require(handoff.policy.requested_glyph_packet_count == 2U, "handoff counts requested draw packets");
    require(handoff.policy.ready_glyph_packet_count == 2U, "handoff counts ready packets");
    require(handoff.policy.blocked_glyph_packet_count == 0U, "handoff has no blocked packets");
    require(handoff.policy.uploaded_glyph_count == 1U, "handoff counts uploaded glyph payload");
    require(handoff.policy.clean_reuse_glyph_count == 1U, "handoff counts clean atlas reuse");
    require(handoff.policy.uploaded_page_count == 1U, "handoff records uploaded page ids");
    require(handoff.policy.used_deterministic_fallback, "handoff preserves deterministic fallback flag");
    require(handoff.policy.deterministic_fallback_count == 1U, "handoff counts deterministic fallback packet");
    require(
        handoff.policy.total_upload_rgba_bytes == upload.atlas_update_rgba_bytes,
        "handoff totals uploaded RGBA bytes");
    require(handoff.uploaded_page_ids.size() == 1U, "handoff exposes uploaded page id list");
    require(handoff.ready_packet_ids.size() == 2U, "handoff exposes ready packet ids");
    require(handoff.packets.size() == 2U, "handoff preserves per-glyph packet diagnostics");
    require(
        handoff.packets.front().handoff_status
            == render_text_frame_upload_handoff_packet_status::ready_uploaded,
        "upload packet is ready for handoff");
    require(handoff.packets.front().uploaded, "upload packet records materialized upload");
    require(!handoff.packets.front().upload_request_id.empty(), "upload packet links upload request id");
    require(
        handoff.packets.back().handoff_status
            == render_text_frame_upload_handoff_packet_status::ready_clean_reuse,
        "clean packet is ready through clean reuse");
    require(handoff.packets.back().clean_reuse, "clean packet records atlas reuse");
    require(handoff.pages.size() == 1U, "handoff keeps page-level summary");
    require(handoff.pages.front().ready_packet_count == 2U, "page summary counts ready packets");

    const render_text_frame_upload_handoff_snapshot real_backend_handoff =
        handoff_for_materializations({real_backend_materialization(key_for_a())});
    require(real_backend_handoff.ok(), "real backend handoff remains ready");
    require(real_backend_handoff.policy.used_real_backend, "handoff preserves real backend flag");
    require(real_backend_handoff.policy.real_backend_count == 1U, "handoff counts real backend packet");
}

void test_text_frame_upload_handoff_reports_rejected_and_missing_materialization_blockers()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot upload =
        upload_ready_materialization();
    const render_text_glyph_atlas_materialization_snapshot missing =
        missing_cache_key_materialization();
    const std::vector<render_text_glyph_atlas_materialization_snapshot> upload_only = {
        upload,
    };
    const std::vector<render_text_glyph_atlas_materialization_snapshot> missing_only = {
        missing,
    };

    const render_text_frame_upload_handoff_snapshot rejected =
        handoff_for_materializations(upload_only, true, false);

    require(!rejected.ok(), "handoff with rejected upload result is blocked");
    require(rejected.has_blockers(), "rejected handoff exposes blockers");
    require(rejected.policy.requested_glyph_packet_count == 1U, "rejected handoff counts requested packet");
    require(rejected.policy.ready_glyph_packet_count == 0U, "rejected handoff has no ready packets");
    require(rejected.policy.blocked_glyph_packet_count == 1U, "rejected handoff counts blocker");
    require(rejected.policy.upload_result_rejected_count == 1U, "handoff counts rejected upload result");
    require(rejected.policy.total_upload_rgba_bytes == 0U, "rejected handoff claims no upload bytes");
    require(rejected.blocker_packet_ids.size() == 1U, "rejected handoff records blocked packet id");
    require(
        rejected.packets.front().handoff_status
            == render_text_frame_upload_handoff_packet_status::blocked_upload_rejected,
        "missing upload request id rejects uploadable packet");
    require(!rejected.packets.front().blocker_reason.empty(), "rejected packet carries blocker reason");

    const render_text_frame_upload_handoff_snapshot missing_handoff =
        handoff_for_materializations(missing_only);

    require(!missing_handoff.ok(), "handoff with missing glyph/cache key is blocked");
    require(missing_handoff.policy.requested_glyph_packet_count == 1U, "missing handoff counts requested packet");
    require(missing_handoff.policy.blocked_glyph_packet_count == 1U, "missing handoff counts blocker");
    require(missing_handoff.policy.missing_glyph_count == 1U, "handoff counts missing glyph/cache key blocker");
    require(
        missing_handoff.packets.front().handoff_status
            == render_text_frame_upload_handoff_packet_status::blocked_draw_packet,
        "missing cache key blocks draw packet before upload handoff");
    require(missing_handoff.packets.front().missing_glyph, "missing-cache packet records missing glyph flag");
    require(missing_handoff.pages.front().has_blockers, "page summary marks blockers");
}

void test_text_frame_upload_handoff_distinguishes_missing_upload_result_and_missing_draw_packet()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot upload =
        upload_ready_materialization();
    const std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {
        upload,
    };
    const render_text_frame_snapshot frame =
        frame_snapshot_for_materializations(materializations, true);
    const render_text_frame_draw_plan_snapshot draw_plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = frame,
            .materializations = materializations,
        });

    const render_text_frame_upload_handoff_snapshot missing_upload_result =
        make_render_text_frame_upload_handoff(render_text_frame_upload_handoff_request{
            .frame = frame,
            .draw_plan = draw_plan,
            .upload_result = render_text_glyph_atlas_upload_result_snapshot{},
        });

    require(!missing_upload_result.ok(), "missing upload result blocks a drawable glyph packet");
    require(missing_upload_result.policy.requested_glyph_packet_count == 1U, "missing upload result keeps draw request count");
    require(missing_upload_result.policy.upload_result_missing_count == 1U, "missing upload result is counted separately");
    require(missing_upload_result.policy.draw_packet_missing_count == 0U, "missing upload result does not claim missing draw packet");
    require(
        missing_upload_result.packets.front().handoff_status
            == render_text_frame_upload_handoff_packet_status::blocked_missing_upload_result,
        "drawable packet without result reports missing upload result");
    require(missing_upload_result.packets.front().missing_upload_result, "packet marks missing upload result");
    require(!missing_upload_result.packets.front().missing_draw_packet, "packet does not mark missing draw packet");

    const render_text_glyph_atlas_upload_operation_plan_snapshot operation_plan =
        plan_render_text_glyph_atlas_upload_operations(
            plan_render_text_glyph_atlas_pages(materializations),
            materializations);
    const render_text_glyph_atlas_upload_result_snapshot orphan_result =
        make_render_text_glyph_atlas_upload_result(render_text_glyph_atlas_upload_result_request{
            .operation_plan = operation_plan,
            .upload_request_ids = {"orphan-upload-request"},
        });
    const render_text_frame_snapshot empty_frame =
        frame_snapshot_for_materializations({});
    const render_text_frame_draw_plan_snapshot empty_draw_plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = empty_frame,
            .materializations = {},
        });
    const render_text_frame_upload_handoff_snapshot missing_draw_packet =
        make_render_text_frame_upload_handoff(render_text_frame_upload_handoff_request{
            .frame = empty_frame,
            .draw_plan = empty_draw_plan,
            .upload_result = orphan_result,
        });

    require(!missing_draw_packet.ok(), "orphan upload result blocks handoff");
    require(missing_draw_packet.policy.requested_glyph_packet_count == 0U, "orphan upload result is not counted as a draw request");
    require(missing_draw_packet.policy.blocked_glyph_packet_count == 1U, "orphan upload result is counted as blocked handoff work");
    require(missing_draw_packet.policy.draw_packet_missing_count == 1U, "missing draw packet is counted separately");
    require(missing_draw_packet.policy.upload_result_missing_count == 0U, "missing draw packet does not claim missing upload result");
    require(
        missing_draw_packet.packets.front().handoff_status
            == render_text_frame_upload_handoff_packet_status::blocked_missing_draw_packet,
        "orphan upload result reports missing draw packet");
    require(missing_draw_packet.packets.front().missing_draw_packet, "orphan packet marks missing draw packet");
    require(!missing_draw_packet.packets.front().missing_upload_result, "orphan packet does not mark missing upload result");
}

void test_text_frame_upload_handoff_diff_reports_frame_to_frame_evidence()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot upload =
        upload_ready_materialization();
    const std::vector<render_text_glyph_atlas_materialization_snapshot> materializations = {
        upload,
    };

    const render_text_frame_upload_handoff_snapshot accepted =
        handoff_for_materializations(materializations, true, true);
    const render_text_frame_upload_handoff_snapshot rejected =
        handoff_for_materializations(materializations, true, false);
    const render_text_frame_upload_handoff_diff_snapshot diff =
        diff_render_text_frame_upload_handoffs(accepted, rejected);

    require(diff.has_changes(), "handoff diff reports accepted-to-rejected transition");
    require(diff.changed_packet_count == 1U, "handoff diff counts changed glyph packet");
    require(diff.changed_page_count == 1U, "handoff diff counts changed page summary");
    require(diff.changed_packet_keys.size() == 1U, "handoff diff exposes changed packet key");
    require(diff.changed_page_ids.size() == 1U, "handoff diff exposes changed page id");
    require(diff.policy.ready_glyph_packet_count_delta == -1, "handoff diff records ready packet loss");
    require(diff.policy.blocked_glyph_packet_count_delta == 1, "handoff diff records blocker increase");
    require(
        diff.policy.total_upload_rgba_bytes_delta
            == -static_cast<std::ptrdiff_t>(upload.atlas_update_rgba_bytes),
        "handoff diff records upload byte loss");
    require(diff.packet_diffs.front().status_changed, "packet diff records handoff status change");
    require(diff.packet_diffs.front().readiness_changed, "packet diff records readiness change");
    require(diff.page_diffs.front().blocker_changed, "page diff records blocker transition");
}

void test_text_frame_upload_handoff_diff_reports_backend_and_fallback_flag_changes()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_upload_handoff_snapshot deterministic =
        handoff_for_materializations({upload_ready_materialization()});
    const render_text_frame_upload_handoff_snapshot real_backend =
        handoff_for_materializations({real_backend_materialization(key_for_a())});
    const render_text_frame_upload_handoff_diff_snapshot diff =
        diff_render_text_frame_upload_handoffs(deterministic, real_backend);

    require(deterministic.policy.used_deterministic_fallback, "baseline handoff uses deterministic fallback");
    require(!deterministic.policy.used_real_backend, "baseline handoff does not claim real backend");
    require(!real_backend.policy.used_deterministic_fallback, "real backend handoff clears deterministic fallback");
    require(real_backend.policy.used_real_backend, "real backend handoff reports real backend");
    require(diff.has_changes(), "handoff diff reports backend/fallback transition");
    require(diff.changed_packet_count == 1U, "backend/fallback transition changes the stable packet");
    require(diff.policy.deterministic_fallback_count_delta == -1, "diff records deterministic fallback loss");
    require(diff.policy.real_backend_count_delta == 1, "diff records real backend gain");
    require(diff.policy.deterministic_fallback_changed, "diff marks deterministic fallback flag change");
    require(diff.policy.real_backend_changed, "diff marks real backend flag change");
    require(diff.packet_diffs.front().fallback_changed, "packet diff marks fallback change");
    require(diff.packet_diffs.front().backend_changed, "packet diff marks backend change");
}

void test_text_frame_upload_handoff_diff_reports_page_revision_and_page_id_changes()
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_materialization_snapshot previous =
        upload_ready_materialization();
    render_text_glyph_atlas_materialization_snapshot revised =
        upload_ready_materialization();
    revised.page.revision = previous.page.revision + 1U;

    const render_text_frame_upload_handoff_diff_snapshot revision_diff =
        diff_render_text_frame_upload_handoffs(
            handoff_for_materializations({previous}),
            handoff_for_materializations({revised}));

    require(revision_diff.has_changes(), "page revision change is visible in handoff diff");
    require(revision_diff.changed_packet_count == 1U, "page revision change marks packet changed");
    require(revision_diff.changed_page_count == 1U, "page revision change marks page changed");
    require(revision_diff.packet_diffs.front().page_changed, "packet diff records page revision change");
    require(revision_diff.page_diffs.front().page_revision_changed, "page diff records revision change");

    render_text_glyph_atlas_materialization_snapshot moved =
        upload_ready_materialization();
    moved.page.id = previous.page.id + 7U;

    const render_text_frame_upload_handoff_diff_snapshot page_id_diff =
        diff_render_text_frame_upload_handoffs(
            handoff_for_materializations({previous}),
            handoff_for_materializations({moved}));

    require(page_id_diff.has_changes(), "page id change is visible in handoff diff");
    require(page_id_diff.changed_packet_count == 1U, "page id change marks stable glyph packet changed");
    require(page_id_diff.packet_diffs.front().page_changed, "packet diff records page id change");
    require(page_id_diff.added_page_count == 1U, "page id change records added page id");
    require(page_id_diff.removed_page_count == 1U, "page id change records removed page id");
    require(page_id_diff.policy.total_upload_rgba_bytes_delta == 0, "page id move keeps upload byte total stable");
}

void test_text_frame_upload_handoff_diff_keeps_materialization_blocker_byte_deltas_stable()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_upload_handoff_snapshot before =
        handoff_for_materializations({missing_cache_key_materialization()});
    const render_text_frame_upload_handoff_snapshot after =
        handoff_for_materializations({missing_cache_key_materialization()});
    const render_text_frame_upload_handoff_diff_snapshot diff =
        diff_render_text_frame_upload_handoffs(before, after);

    require(!before.ok(), "missing cache handoff is blocked");
    require(before.policy.missing_glyph_count == 1U, "missing cache handoff counts missing glyph");
    require(before.policy.total_upload_rgba_bytes == 0U, "missing cache handoff claims no upload bytes");
    require(!diff.has_changes(), "identical materialization blocker handoffs remain stable");
    require(diff.unchanged_packet_count == 1U, "blocker diff keeps stable packet key");
    require(diff.policy.blocked_glyph_packet_count_delta == 0, "blocker count delta remains stable");
    require(diff.policy.missing_glyph_count_delta == 0, "missing glyph delta remains stable");
    require(diff.policy.total_upload_rgba_bytes_delta == 0, "upload byte delta remains stable for blockers");
}

void test_atlas_upload_bridge_produces_stable_render_text_atlas_updates()
{
    using namespace quiz_vulkan::render;

    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 100.0f, 40.0f};
    request.style_catalog = batch_style_catalog();

    const render_text_glyph_atlas_materialization_snapshot materialization =
        upload_ready_materialization();
    const render_text_request_batch_plan_snapshot plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(std::move(request), {materialization}, "upload"),
        });
    const render_text_atlas_upload_request_bridge_snapshot bridge =
        bridge_render_text_atlas_upload_requests(plan);
    const render_text_atlas_upload_request_bridge_snapshot repeated_bridge =
        bridge_render_text_atlas_upload_requests(plan);

    require(bridge.has_upload_requests(), "atlas upload bridge emits upload requests");
    require(bridge.ok(), "upload-ready bridge reports ok");
    require(bridge.requests.size() == 1U, "atlas upload bridge records one diagnostic request");
    require(bridge.upload_requests.size() == 1U, "atlas upload bridge emits one render_text_atlas_update");
    require(bridge.stable_request_ids.size() == 1U, "atlas upload bridge records stable request id");
    require(bridge.policy.batch_atlas_request_count == 1U, "atlas upload policy records batch request count");
    require(bridge.policy.upload_request_count == 1U, "atlas upload policy counts upload request");
    require(bridge.policy.unique_upload_request_count == 1U, "atlas upload policy counts unique upload");
    require(
        bridge.policy.total_upload_rgba_bytes == materialization.atlas_update_rgba_bytes,
        "atlas upload policy totals emitted RGBA bytes");

    const render_text_atlas_upload_request_snapshot& upload = bridge.requests.front();
    require(upload.status == render_text_atlas_upload_request_status::upload_ready, "upload request is ready");
    require(upload.ok(), "upload request status is ok");
    require(upload.has_upload_request, "upload request carries render_text_atlas_update payload");
    require(upload.request_id == bridge.stable_request_ids.front(), "upload request id is tracked as stable");
    require(
        upload.request_id == render_text_atlas_upload_request_stable_id_for(plan.atlas_update_requests.front()),
        "upload request id is derived from stable atlas/materialization inputs");
    require(upload.cache_key == materialization.cache_key, "upload request preserves cache key");
    require(upload.upload_request.page.id == materialization.page.id, "upload request preserves page id");
    require(upload.upload_request.updated_bounds.x == materialization.atlas_update_bounds.x, "upload bounds preserve x");
    require(
        upload.upload_request.rgba.size() == materialization.atlas_update_rgba_bytes,
        "upload request carries deterministic RGBA bytes");
    require(
        repeated_bridge.requests.front().request_id == upload.request_id,
        "stable upload request id repeats for identical plan input");
    require(
        repeated_bridge.upload_requests.front().rgba == upload.upload_request.rgba,
        "stable upload request RGBA payload repeats for identical plan input");
}

void test_atlas_upload_bridge_suppresses_duplicates_and_skips_non_uploadable_work()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot skipped =
        make_render_text_glyph_atlas_materialization(
            render_text_glyph_atlas_materialization_request{
                .cluster_index = 5,
                .run_index = 0,
                .codepoint = U'\u0301',
                .shaped_glyph_ids = {0x0301U},
                .resolved_glyph_id = 0x0301U,
                .glyph_supported = true,
                .rasterized_payload_skipped = true,
                .payload_upload_ready = false,
            });

    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "ABCD", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 100.0f, 40.0f};
    request.style_catalog = batch_style_catalog();

    const render_text_glyph_atlas_materialization_snapshot materialization =
        upload_ready_materialization();
    const render_text_request_batch_plan_snapshot plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(
                std::move(request),
                {
                    materialization,
                    materialization,
                    clean_reuse_materialization(),
                    skipped,
                    mismatched_materialization(),
                },
                "mixed-upload"),
        });
    const render_text_atlas_upload_request_bridge_snapshot bridge =
        bridge_render_text_atlas_upload_requests(plan);

    require(!bridge.ok(), "mixed upload bridge reports skipped work");
    require(bridge.requests.size() == 5U, "mixed upload bridge records all source requests");
    require(bridge.upload_requests.size() == 1U, "mixed upload bridge emits only unique upload-ready request");
    require(bridge.policy.upload_request_count == 1U, "mixed upload bridge counts emitted upload");
    require(bridge.policy.duplicate_suppressed_count == 1U, "mixed upload bridge suppresses duplicate request");
    require(bridge.policy.clean_reuse_count == 1U, "mixed upload bridge records clean atlas reuse");
    require(bridge.policy.skipped_materialization_count == 1U, "mixed upload bridge records skipped materialization");
    require(bridge.policy.payload_byte_count_mismatch_count == 1U, "mixed upload bridge records byte mismatch");
    require(
        bridge.stable_request_ids.size() == 4U,
        "mixed upload bridge deduplicates stable request ids for duplicate work");
    require(
        bridge.requests[1].status == render_text_atlas_upload_request_status::duplicate_suppressed,
        "duplicate source request is suppressed");
    require(
        bridge.requests[1].request_id == bridge.requests[0].request_id,
        "duplicate source request keeps same stable id");
    require(
        bridge.requests[2].status == render_text_atlas_upload_request_status::clean_reuse,
        "clean reuse request does not upload");
    require(
        bridge.requests[3].status == render_text_atlas_upload_request_status::skipped_materialization,
        "skipped materialization does not upload");
    require(
        bridge.requests[4].status == render_text_atlas_upload_request_status::payload_byte_count_mismatch,
        "byte mismatch does not upload");
}

void test_text_frame_snapshot_combines_planning_fallback_materialization_and_upload_ids()
{
    using namespace quiz_vulkan::render;

    const render_text_request request = frame_snapshot_text_request();
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations;
    render_text_glyph_atlas_materialization_policy_snapshot materialization_policy;
    append_render_text_glyph_atlas_materialization(
        materializations,
        materialization_policy,
        upload_ready_materialization());

    const render_text_request_batch_plan_snapshot batch_plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(request, materializations, "frame-source", "node-1"),
        });
    const render_text_atlas_upload_request_bridge_snapshot bridge =
        bridge_render_text_atlas_upload_requests(batch_plan);

    const render_text_frame_snapshot pending =
        make_render_text_frame_snapshot(render_text_frame_snapshot_request{
            .frame_id = "frame-1",
            .source_label = "frame-source",
            .batch_plan = batch_plan,
            .fallback_chain_plan = frame_snapshot_fallback_plan(request),
            .materializations = materializations,
            .materialization_policy = materialization_policy,
            .atlas_upload_bridge = bridge,
        });

    require(
        pending.status == render_text_frame_snapshot_status::pending_atlas_updates,
        "frame snapshot remains pending before atlas updates are consumed");
    require(pending.ok(), "pending frame snapshot has no planning errors");
    require(!pending.ready_for_renderer(), "pending frame snapshot is not ready for renderer handoff");
    require(pending.has_atlas_uploads(), "frame snapshot records compact atlas upload entries");
    require(pending.batch_policy.layout_request_count == 1U, "frame snapshot preserves batch layout count");
    require(pending.fallback_chain_policy.run_count == 1U, "frame snapshot preserves fallback-chain run count");
    require(pending.materialization_policy.request_count == 1U, "frame snapshot preserves materialization policy");
    require(pending.atlas_upload_policy.upload_request_count == 1U, "frame snapshot preserves upload policy");
    require(pending.policy.selected_face_count == 1U, "frame snapshot preserves selected fallback face order");
    require(pending.policy.queued_upload_request_id_count == 1U, "frame snapshot derives queued upload ids");
    require(
        pending.queued_atlas_upload_request_ids.front() == bridge.requests.front().request_id,
        "frame snapshot queued id matches atlas upload bridge id");
    require(pending.atlas_uploads.front().queued, "frame snapshot upload is marked queued");
    require(!pending.atlas_uploads.front().consumed, "frame snapshot upload is not consumed yet");
    require(
        pending.atlas_uploads.front().cache_key == materializations.front().cache_key,
        "frame snapshot upload preserves materialization cache key");

    const render_text_frame_snapshot ready =
        render_text_frame_snapshot_with_consumed_atlas_updates(
            pending,
            pending.queued_atlas_upload_request_ids,
            pending.queued_atlas_upload_request_ids.size());

    require(ready.status == render_text_frame_snapshot_status::ready, "matching consumed ids make frame ready");
    require(ready.ready_for_renderer(), "ready frame snapshot reports renderer readiness");
    require(ready.policy.all_queued_uploads_consumed, "ready frame marks all uploads consumed");
    require(
        ready.consumed_atlas_upload_request_ids == pending.queued_atlas_upload_request_ids,
        "ready frame preserves consumed upload ids");
    require(ready.atlas_uploads.front().consumed, "ready frame marks upload consumed");
}

void test_text_frame_snapshot_reports_consumed_upload_id_mismatch()
{
    using namespace quiz_vulkan::render;

    const render_text_request request = frame_snapshot_text_request();
    std::vector<render_text_glyph_atlas_materialization_snapshot> materializations;
    render_text_glyph_atlas_materialization_policy_snapshot materialization_policy;
    append_render_text_glyph_atlas_materialization(
        materializations,
        materialization_policy,
        upload_ready_materialization());

    const render_text_request_batch_plan_snapshot batch_plan =
        plan_render_text_request_batch({
            make_render_text_request_batch_item(request, materializations, "frame-source", "node-1"),
        });
    const render_text_frame_snapshot pending =
        make_render_text_frame_snapshot(render_text_frame_snapshot_request{
            .frame_id = "frame-1",
            .source_label = "frame-source",
            .batch_plan = batch_plan,
            .fallback_chain_plan = frame_snapshot_fallback_plan(request),
            .materializations = materializations,
            .materialization_policy = materialization_policy,
            .atlas_upload_bridge = bridge_render_text_atlas_upload_requests(batch_plan),
        });

    const render_text_frame_snapshot mismatch =
        render_text_frame_snapshot_with_consumed_atlas_updates(
            pending,
            {"text-atlas-upload:v1:wrong"},
            1U);

    require(
        mismatch.status == render_text_frame_snapshot_status::consumed_update_mismatch,
        "frame snapshot detects consumed upload ids that do not match queued bridge ids");
    require(!mismatch.ok(), "mismatched consumed ids are not ok for renderer handoff");
    require(!mismatch.policy.all_queued_uploads_consumed, "mismatched frame does not claim consumed uploads");
}

void test_text_frame_snapshot_diff_reports_readiness_and_consumed_id_transition()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_snapshot pending =
        frame_snapshot_for_materializations({upload_ready_materialization()});
    const render_text_frame_snapshot ready =
        render_text_frame_snapshot_with_consumed_atlas_updates(
            pending,
            pending.queued_atlas_upload_request_ids,
            pending.queued_atlas_upload_request_ids.size());

    const render_text_frame_snapshot_diff diff =
        diff_render_text_frame_snapshots(pending, ready);

    require(
        diff.previous_status == render_text_frame_snapshot_status::pending_atlas_updates,
        "diff records previous frame status");
    require(diff.current_status == render_text_frame_snapshot_status::ready, "diff records current frame status");
    require(!diff.previous_ready_for_renderer, "diff records previous readiness");
    require(diff.current_ready_for_renderer, "diff records current readiness");
    require(diff.policy.status_changed, "diff records status transition");
    require(diff.policy.readiness_changed, "diff records readiness transition");
    require(diff.policy.layout_request_count_delta == 0, "diff records zero layout count delta");
    require(diff.policy.total_upload_rgba_bytes_delta == 0, "diff records stable upload byte count");
    require(diff.added_atlas_upload_request_ids.empty(), "ready transition does not add atlas upload ids");
    require(diff.removed_atlas_upload_request_ids.empty(), "ready transition does not remove atlas upload ids");
    require(diff.changed_atlas_upload_request_ids.size() == 1U, "ready transition changes upload consumed state");
    require(
        diff.changed_atlas_upload_request_ids.front() == pending.queued_atlas_upload_request_ids.front(),
        "changed upload id matches queued id");
    require(
        diff.added_consumed_atlas_upload_request_ids == pending.queued_atlas_upload_request_ids,
        "diff records consumed upload id added");
    require(!diff.has_regression(), "ready transition has no regression");
    require(diff.ok(), "ready transition diff is ok");
}

void test_text_frame_snapshot_diff_reports_added_removed_ids_byte_delta_and_regression()
{
    using namespace quiz_vulkan::render;

    const render_text_frame_snapshot previous =
        frame_snapshot_for_materializations({upload_ready_materialization()}, true);

    render_text_glyph_atlas_materialization_snapshot larger =
        upload_ready_materialization(glyph_atlas_key{
            .face_id = 9,
            .glyph_id = U'B',
            .pixel_size = 20,
        });
    larger.atlas_update_rgba_bytes += 16U;

    render_text_frame_snapshot current =
        frame_snapshot_for_materializations({larger}, true);
    current.status = render_text_frame_snapshot_status::font_fallback_incomplete;
    current.fallback_chain_policy.missing_glyph_count = 2U;
    current.fallback_chain_policy.invalid_utf8_count = 1U;
    current.policy.fallback_chain_missing_glyph_count = 2U;
    current.policy.fallback_chain_invalid_utf8_count = 1U;

    const render_text_frame_snapshot_diff diff =
        diff_render_text_frame_snapshots(previous, current);

    require(diff.has_atlas_upload_id_changes(), "diff records atlas upload id changes");
    require(diff.added_atlas_upload_request_ids.size() == 1U, "diff records added atlas upload id");
    require(diff.removed_atlas_upload_request_ids.size() == 1U, "diff records removed atlas upload id");
    require(diff.changed_atlas_upload_request_ids.empty(), "different upload ids are not reported as changed");
    require(diff.added_queued_atlas_upload_request_ids == current.queued_atlas_upload_request_ids, "queued id added");
    require(diff.removed_queued_atlas_upload_request_ids == previous.queued_atlas_upload_request_ids, "queued id removed");
    require(
        diff.removed_consumed_atlas_upload_request_ids == previous.consumed_atlas_upload_request_ids,
        "consumed id removed");
    require(
        diff.added_consumed_atlas_upload_request_ids == current.consumed_atlas_upload_request_ids,
        "consumed id added");
    require(diff.policy.missing_glyph_count_delta == 2, "diff records missing glyph delta");
    require(diff.policy.invalid_utf8_count_delta == 1, "diff records invalid UTF-8 delta");
    require(diff.policy.total_upload_rgba_bytes_delta == 16, "diff records total upload byte delta");
    require(diff.policy.added_atlas_upload_request_id_count == 1U, "policy counts added upload id");
    require(diff.policy.removed_atlas_upload_request_id_count == 1U, "policy counts removed upload id");
    require(diff.policy.added_queued_upload_request_id_count == 1U, "policy counts added queued id");
    require(diff.policy.added_consumed_upload_request_id_count == 1U, "policy counts added consumed id");
    require(diff.policy.removed_consumed_upload_request_id_count == 1U, "policy counts removed consumed id");
    require(diff.has_queue_or_consume_id_deltas(), "diff records queue/consume id deltas");
    require(diff.has_regression(), "diff records regression");
    require(!diff.ok(), "regression diff is not ok");
    require(diff.regression.readiness_regressed, "diff records readiness regression");
    require(diff.regression.fallback_regressed, "diff records fallback regression");
    require(
        diff.regression.status == render_text_frame_snapshot_regression_status::readiness_regressed,
        "readiness regression is the primary regression status");
    require(diff.regression.issue_count == 2U, "diff counts readiness and fallback regression issues");
}

void test_text_frame_draw_plan_produces_ready_glyph_packet_with_uv_bounds()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot materialization =
        upload_ready_materialization();
    materialization.run_index = 0U;
    const render_text_frame_snapshot frame =
        frame_snapshot_for_materializations({materialization}, true);

    const render_text_frame_draw_plan_snapshot plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = frame,
            .materializations = {materialization},
        });
    const render_text_frame_draw_plan_snapshot repeated_plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = frame,
            .materializations = {materialization},
        });

    require(plan.ok(), "ready frame draw plan is ok");
    require(plan.has_draw_packets(), "ready frame draw plan emits packet diagnostics");
    require(plan.policy.materialization_count == 1U, "draw plan counts materializations");
    require(plan.policy.packet_count == 1U, "draw plan counts packets");
    require(plan.policy.draw_ready_count == 1U, "draw plan counts draw-ready packet");
    require(plan.policy.deterministic_fallback_count == 1U, "draw plan preserves fallback backend status");

    const render_text_frame_draw_packet_snapshot& packet = plan.packets.front();
    require(packet.drawable(), "ready packet reports drawable");
    require(packet.status == render_text_frame_draw_packet_status::draw_ready, "packet status is draw-ready");
    require(packet.frame_ready_for_renderer, "packet records frame readiness");
    require(packet.cache_key == materialization.cache_key, "packet preserves glyph cache key");
    require(packet.resolved_glyph_id == materialization.resolved_glyph_id, "packet preserves resolved glyph id");
    require(packet.page_id == materialization.page.id, "packet preserves atlas page id");
    require(packet.page_revision == materialization.page.revision, "packet preserves atlas revision");
    require(packet.requested_style_token == "body", "packet resolves requested style token");
    require(packet.resolved_style_id == "body", "packet resolves style id");
    require(packet.run_index == 0U, "packet preserves run index");
    require(packet.has_layout_bounds, "packet exposes layout bounds");
    require(packet.layout_bounds.x == materialization.layout_bounds.x, "packet preserves layout bounds");
    require(packet.has_atlas_bounds, "packet exposes atlas bounds");
    require(packet.atlas_bounds.x == materialization.atlas_bounds.x, "packet preserves atlas bounds");
    require(packet.uv_bounds.valid, "packet derives UV bounds from atlas page size");
    require(packet.uv_bounds.u0 == materialization.atlas_bounds.x / 64.0f, "packet derives u0");
    require(packet.uv_bounds.u1 == materialization.atlas_bounds.right() / 64.0f, "packet derives u1");
    require(packet.upload_consumed, "packet records consumed upload id");
    require(packet.stable_cache_key, "packet reports stable cache key");
    require(
        repeated_plan.packets.front().packet_id == packet.packet_id,
        "draw packet stable id repeats for identical inputs");
}

void test_text_frame_draw_plan_reports_pending_and_fallback_blockers()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot materialization =
        upload_ready_materialization();
    materialization.run_index = 0U;

    const render_text_frame_snapshot pending_frame =
        frame_snapshot_for_materializations({materialization});
    const render_text_frame_draw_plan_snapshot pending_plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = pending_frame,
            .materializations = {materialization},
        });

    require(!pending_plan.ok(), "pending frame draw plan is not ready");
    require(
        pending_plan.packets.front().status == render_text_frame_draw_packet_status::frame_not_ready,
        "pending frame blocks draw packet");
    require(pending_plan.policy.frame_not_ready_count == 1U, "policy counts frame-not-ready packet");

    render_text_frame_snapshot fallback_frame =
        frame_snapshot_for_materializations({materialization}, true);
    fallback_frame.status = render_text_frame_snapshot_status::font_fallback_incomplete;
    const render_text_frame_draw_plan_snapshot fallback_plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = fallback_frame,
            .materializations = {materialization},
        });

    require(!fallback_plan.ok(), "fallback-incomplete frame draw plan is not ready");
    require(
        fallback_plan.packets.front().status == render_text_frame_draw_packet_status::fallback_incomplete,
        "fallback incomplete frame blocks draw packet");
    require(fallback_plan.packets.front().fallback_incomplete, "packet records fallback incomplete status");
    require(
        fallback_plan.policy.fallback_incomplete_count == 1U,
        "policy counts fallback-incomplete packet");
}

void test_text_frame_draw_plan_reports_missing_page_extent()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot materialization =
        upload_ready_materialization();
    materialization.run_index = 0U;
    materialization.page.width = 0U;
    materialization.page.height = 0U;

    const render_text_frame_snapshot frame =
        frame_snapshot_for_materializations({materialization}, true);
    const render_text_frame_draw_plan_snapshot plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = frame,
            .materializations = {materialization},
        });

    require(!plan.ok(), "missing page extent draw plan is not ready");
    require(
        plan.packets.front().status == render_text_frame_draw_packet_status::missing_page_extent,
        "draw plan reports missing page extent when UVs cannot be derived");
    require(!plan.packets.front().uv_bounds.valid, "missing page extent leaves UV bounds invalid");
    require(plan.policy.missing_page_extent_count == 1U, "policy counts missing page extent");
}

void test_text_frame_draw_plan_diff_reports_unchanged_packet_set()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot materialization =
        upload_ready_materialization();
    materialization.run_index = 0U;

    const render_text_frame_draw_plan_snapshot previous =
        draw_plan_for_materializations({materialization});
    const render_text_frame_draw_plan_snapshot current =
        draw_plan_for_materializations({materialization});
    const render_text_frame_draw_plan_diff diff =
        diff_render_text_frame_draw_plans(previous, current);

    require(diff.ok(), "unchanged draw plan diff is ok");
    require(!diff.has_packet_changes(), "unchanged draw plan has no packet changes");
    require(!diff.has_readiness_or_fallback_changes(), "unchanged draw plan has no readiness changes");
    require(!diff.has_page_revision_changes(), "unchanged draw plan has no page revision changes");
    require(!diff.has_stable_key_deltas(), "unchanged draw plan has no stable key deltas");
    require(diff.policy.packet_count_delta == 0, "unchanged draw plan has zero packet delta");
    require(diff.policy.glyph_quad_count_delta == 0, "unchanged draw plan has zero glyph quad delta");
    require(diff.policy.unchanged_packet_count == 1U, "unchanged draw plan counts unchanged packet");
    require(diff.policy.stable_no_change, "unchanged draw plan marks stable no-change");
    require(diff.stable_no_change(), "unchanged draw plan exposes stable no-change helper");
    require(diff.policy.stable_glyph_key_count_delta == 0, "unchanged draw plan has zero glyph key delta");
    require(diff.policy.stable_style_key_count_delta == 0, "unchanged draw plan has zero style key delta");
    require(diff.policy.stable_run_key_count_delta == 0, "unchanged draw plan has zero run key delta");
    require(diff.packet_diffs.size() == 1U, "unchanged draw plan exposes compact packet diff");
    require(diff.packet_diffs.front().unchanged, "compact packet diff marks unchanged packet");
    require(diff.packet_diffs.front().current_glyph_quad_count == 1U, "compact packet diff records one glyph quad");
    require(diff.unchanged_packet_ids.size() == 1U, "unchanged packet id is exposed");
}

void test_text_frame_draw_plan_diff_reports_page_revision_changes_as_changed_packets()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot previous_materialization =
        upload_ready_materialization();
    previous_materialization.run_index = 0U;
    render_text_glyph_atlas_materialization_snapshot current_materialization =
        previous_materialization;
    current_materialization.page.revision = previous_materialization.page.revision + 1U;

    const render_text_frame_draw_plan_snapshot previous =
        draw_plan_for_materializations({previous_materialization});
    const render_text_frame_draw_plan_snapshot current =
        draw_plan_for_materializations({current_materialization});
    const render_text_frame_draw_plan_diff diff =
        diff_render_text_frame_draw_plans(previous, current);

    require(diff.ok(), "page revision change does not regress readiness");
    require(diff.has_packet_changes(), "page revision change records a changed packet");
    require(diff.has_page_revision_changes(), "page revision change is reported explicitly");
    require(diff.added_packet_ids.empty(), "page revision change does not add logical packet");
    require(diff.removed_packet_ids.empty(), "page revision change does not remove logical packet");
    require(diff.changed_packet_ids.size() == 1U, "page revision change counts changed packet");
    require(diff.page_revision_changed_packet_ids == diff.changed_packet_ids, "page revision packet id is changed id");
    require(diff.policy.page_revision_changed_packet_count == 1U, "policy counts page revision change");
    require(
        diff.policy.upload_generation_changed_packet_count == 1U,
        "policy counts upload generation change");
    require(
        diff.packet_diffs.front().upload_generation_changed,
        "packet diff marks upload generation change");
    require(
        diff.packet_diffs.front().previous_upload_generation
            != diff.packet_diffs.front().current_upload_generation,
        "packet diff preserves upload generation transition");
    require(diff.policy.stable_glyph_key_count_delta == 0, "page revision keeps stable glyph count");
    require(diff.policy.stable_style_key_count_delta == 0, "page revision keeps stable style count");
    require(diff.policy.stable_run_key_count_delta == 0, "page revision keeps stable run count");
}

void test_text_frame_draw_plan_diff_reports_readiness_and_fallback_changes()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot materialization =
        upload_ready_materialization();
    materialization.run_index = 0U;
    const render_text_frame_draw_plan_snapshot previous =
        draw_plan_for_materializations({materialization});

    render_text_frame_snapshot fallback_frame =
        frame_snapshot_for_materializations({materialization}, true);
    fallback_frame.status = render_text_frame_snapshot_status::font_fallback_incomplete;
    const render_text_frame_draw_plan_snapshot current =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = fallback_frame,
            .materializations = {materialization},
        });

    const render_text_frame_draw_plan_diff diff =
        diff_render_text_frame_draw_plans(previous, current);

    require(!diff.ok(), "readiness regression is not ok");
    require(diff.has_readiness_or_fallback_changes(), "diff records readiness or fallback changes");
    require(diff.policy.frame_status_changed, "policy records frame status change");
    require(diff.policy.frame_readiness_changed, "policy records frame readiness change");
    require(diff.policy.frame_readiness_regressed, "policy records frame readiness regression");
    require(diff.policy.draw_ready_count_delta == -1, "policy records draw-ready loss");
    require(diff.policy.glyph_quad_count_delta == -1, "policy records glyph quad loss");
    require(diff.policy.fallback_incomplete_count_delta == 1, "policy records fallback packet delta");
    require(diff.readiness_changed_packet_ids.size() == 1U, "diff records readiness packet id");
    require(diff.readiness_regressed_packet_ids.size() == 1U, "diff records readiness regression packet id");
    require(diff.fallback_changed_packet_ids.size() == 1U, "diff records fallback packet id");
    require(diff.policy.readiness_changed_packet_count == 1U, "policy counts readiness packet changes");
    require(diff.policy.readiness_regression_count == 1U, "policy counts readiness regression");
    require(diff.policy.glyph_quad_count_changed_packet_count == 1U, "policy counts glyph quad count change");
    require(diff.policy.fallback_changed_packet_count == 1U, "policy counts fallback packet changes");
    require(diff.policy.fallback_or_skipped_packet_count == 1U, "policy counts fallback blocker evidence");
    require(diff.packet_diffs.front().readiness_regressed, "packet diff marks ready-to-blocked regression");
    require(diff.packet_diffs.front().current_glyph_quad_count == 0U, "blocked packet has no glyph quad");
}

void test_text_frame_draw_plan_diff_reports_consumption_blocker_categories()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot a = upload_ready_materialization();
    a.run_index = 0U;
    render_text_glyph_atlas_materialization_snapshot b =
        upload_ready_materialization(glyph_atlas_key{
            .face_id = 7,
            .glyph_id = U'B',
            .pixel_size = 20,
        });
    b.run_index = 1U;

    const render_text_frame_draw_plan_snapshot previous =
        draw_plan_for_materializations({a, b});
    render_text_frame_draw_plan_snapshot current = previous;
    current.packets[0].status = render_text_frame_draw_packet_status::skipped_materialization;
    current.packets[0].atlas_upload_request_id.clear();
    current.packets[0].upload_consumed = false;
    current.packets[0].diagnostic.clear();
    current.packets[1].status = render_text_frame_draw_packet_status::missing_layout_bounds;
    current.packets[1].has_layout_bounds = false;
    current.packets[1].uv_bounds.valid = false;
    current.packets[1].diagnostic.clear();

    const render_text_frame_draw_plan_diff diff =
        diff_render_text_frame_draw_plans(previous, current);

    require(diff.has_packet_changes(), "blocker category changes are packet changes");
    require(diff.policy.status_changed_packet_count == 2U, "status changes are counted");
    require(diff.policy.blocker_changed_packet_count == 2U, "blocker summaries changed");
    require(diff.blocker_changed_packet_ids.size() == 2U, "blocker-changed packet ids are exposed");
    require(diff.policy.missing_atlas_upload_blocker_count == 1U, "missing atlas upload blocker is counted");
    require(diff.policy.missing_materialization_blocker_count == 1U, "missing materialization blocker is counted");
    require(diff.policy.non_upload_ready_payload_blocker_count == 1U, "non-upload-ready payload blocker is counted");
    require(diff.policy.missing_glyph_quad_fact_blocker_count == 1U, "missing glyph quad facts are counted");
    require(diff.policy.fallback_or_skipped_packet_count == 2U, "fallback/skipped packet evidence is counted");
    require(diff.policy.glyph_quad_count_changed_packet_count == 2U, "glyph quad count losses are counted");
    require(diff.policy.glyph_quad_count_delta == -2, "two blocked packets remove two glyph quads");
    require(diff.packet_diffs[0].missing_materialization, "first packet records missing materialization");
    require(diff.packet_diffs[0].missing_atlas_upload, "first packet records missing atlas upload evidence");
    require(diff.packet_diffs[1].missing_glyph_quad_facts, "second packet records missing quad facts");
}

void test_text_frame_draw_plan_diff_reports_added_removed_and_stable_glyph_deltas()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot a = upload_ready_materialization();
    a.run_index = 0U;
    render_text_glyph_atlas_materialization_snapshot b =
        upload_ready_materialization(glyph_atlas_key{
            .face_id = 7,
            .glyph_id = U'B',
            .pixel_size = 20,
        });
    b.run_index = 0U;

    const render_text_frame_draw_plan_snapshot previous =
        draw_plan_for_materializations({a});
    const render_text_frame_draw_plan_snapshot current =
        draw_plan_for_materializations({a, b});

    const render_text_frame_draw_plan_diff added =
        diff_render_text_frame_draw_plans(previous, current);
    require(added.has_packet_changes(), "added glyph records packet changes");
    require(added.added_packet_ids.size() == 1U, "diff records added packet id");
    require(added.removed_packet_ids.empty(), "added glyph has no removed packet");
    require(added.policy.packet_count_delta == 1, "policy records packet count delta");
    require(added.policy.stable_glyph_key_count_delta == 1, "policy records stable glyph count delta");
    require(added.policy.stable_style_key_count_delta == 0, "policy keeps style count stable");
    require(added.policy.stable_run_key_count_delta == 0, "policy keeps run count stable for same run");

    const render_text_frame_draw_plan_diff removed =
        diff_render_text_frame_draw_plans(current, previous);
    require(removed.removed_packet_ids.size() == 1U, "reverse diff records removed packet id");
    require(removed.policy.packet_count_delta == -1, "reverse diff records negative packet delta");
    require(removed.policy.stable_glyph_key_count_delta == -1, "reverse diff records negative glyph delta");
}

void test_text_frame_draw_plan_diff_reports_stable_glyph_style_and_run_changes()
{
    using namespace quiz_vulkan::render;

    render_text_glyph_atlas_materialization_snapshot a = upload_ready_materialization();
    a.run_index = 0U;
    render_text_glyph_atlas_materialization_snapshot b =
        upload_ready_materialization(glyph_atlas_key{
            .face_id = 7,
            .glyph_id = U'B',
            .pixel_size = 20,
        });
    b.run_index = 0U;

    const render_text_frame_draw_plan_snapshot previous =
        draw_plan_for_materializations({a});
    const render_text_frame_draw_plan_snapshot glyph_changed =
        draw_plan_for_materializations({b});
    const render_text_frame_draw_plan_diff glyph_diff =
        diff_render_text_frame_draw_plans(previous, glyph_changed);
    require(glyph_diff.policy.stable_glyph_key_count_delta == 0, "glyph replacement keeps glyph key count");
    require(
        glyph_diff.policy.stable_glyph_changed_packet_count == 1U,
        "glyph replacement counts stable glyph change");
    require(!glyph_diff.stable_glyph_changed_packet_ids.empty(), "glyph replacement exposes packet id");

    render_text_frame_draw_plan_snapshot style_run_changed = previous;
    style_run_changed.packets.front().requested_style_token = "headline";
    style_run_changed.packets.front().resolved_style_id = "headline";
    style_run_changed.packets.front().run_index = 2U;
    const render_text_frame_draw_plan_diff style_run_diff =
        diff_render_text_frame_draw_plans(previous, style_run_changed);

    require(style_run_diff.has_stable_key_deltas(), "style/run change reports stable key deltas");
    require(
        style_run_diff.policy.stable_style_changed_packet_count == 1U,
        "style replacement counts stable style change");
    require(
        style_run_diff.policy.stable_run_changed_packet_count == 1U,
        "run replacement counts stable run change");
    require(style_run_diff.changed_packet_ids.size() == 1U, "style/run change is a changed packet");
}

} // namespace

int main()
{
    test_trace_reports_upload_ready_payload_queued();
    test_trace_reports_clean_reuse_when_update_is_absent();
    test_trace_reports_skipped_payload_and_missing_cache_key();
    test_trace_reports_payload_byte_count_mismatch();
    test_batch_plan_normalizes_style_keys_and_layout_requests();
    test_batch_plan_deduplicates_glyph_atlas_materialization_work();
    test_batch_plan_reports_fallback_real_backend_and_skipped_materializations();
    test_freetype_raster_payload_materializes_upload_handoff_when_fixture_is_available();
    test_freetype_raster_payload_handoff_reports_missing_byte_fallback();
    test_glyph_atlas_materialization_diff_reports_stable_key_changes();
    test_glyph_atlas_materialization_policy_diff_reports_byte_and_status_deltas();
    test_glyph_atlas_materialization_batch_diff_reports_regressions_recoveries_and_backend_transitions();
    test_glyph_atlas_page_plan_reports_selection_reuse_and_upload_totals();
    test_glyph_atlas_page_plan_reports_overflow_and_eviction_hint();
    test_glyph_atlas_page_plan_skips_unsupported_materializations();
    test_glyph_atlas_upload_operation_plan_emits_upload_and_clean_reuse_packets();
    test_glyph_atlas_upload_operation_plan_reports_overflow_and_missing_cache_blockers();
    test_glyph_atlas_upload_operation_plan_reports_payload_mismatch_blocker();
    test_glyph_atlas_upload_result_accepts_upload_and_clean_reuse_packets();
    test_glyph_atlas_upload_result_rejects_blockers_and_missing_upload_ids();
    test_glyph_atlas_upload_result_diff_reports_changed_packet_and_page_summaries();
    test_text_frame_upload_handoff_links_draw_packets_to_upload_results();
    test_text_frame_upload_handoff_reports_rejected_and_missing_materialization_blockers();
    test_text_frame_upload_handoff_distinguishes_missing_upload_result_and_missing_draw_packet();
    test_text_frame_upload_handoff_diff_reports_frame_to_frame_evidence();
    test_text_frame_upload_handoff_diff_reports_backend_and_fallback_flag_changes();
    test_text_frame_upload_handoff_diff_reports_page_revision_and_page_id_changes();
    test_text_frame_upload_handoff_diff_keeps_materialization_blocker_byte_deltas_stable();
    test_atlas_upload_bridge_produces_stable_render_text_atlas_updates();
    test_atlas_upload_bridge_suppresses_duplicates_and_skips_non_uploadable_work();
    test_text_frame_snapshot_combines_planning_fallback_materialization_and_upload_ids();
    test_text_frame_snapshot_reports_consumed_upload_id_mismatch();
    test_text_frame_snapshot_diff_reports_readiness_and_consumed_id_transition();
    test_text_frame_snapshot_diff_reports_added_removed_ids_byte_delta_and_regression();
    test_text_frame_draw_plan_produces_ready_glyph_packet_with_uv_bounds();
    test_text_frame_draw_plan_reports_pending_and_fallback_blockers();
    test_text_frame_draw_plan_reports_missing_page_extent();
    test_text_frame_draw_plan_diff_reports_unchanged_packet_set();
    test_text_frame_draw_plan_diff_reports_page_revision_changes_as_changed_packets();
    test_text_frame_draw_plan_diff_reports_readiness_and_fallback_changes();
    test_text_frame_draw_plan_diff_reports_consumption_blocker_categories();
    test_text_frame_draw_plan_diff_reports_added_removed_and_stable_glyph_deltas();
    test_text_frame_draw_plan_diff_reports_stable_glyph_style_and_run_changes();
    return 0;
}
