#include "render/text/font_shaped_atlas_update.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
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

quiz_vulkan::render::glyph_atlas_key key_for_a()
{
    return quiz_vulkan::render::glyph_atlas_key{
        .face_id = 7,
        .glyph_id = U'A',
        .pixel_size = 20,
    };
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
    test_atlas_upload_bridge_produces_stable_render_text_atlas_updates();
    test_atlas_upload_bridge_suppresses_duplicates_and_skips_non_uploadable_work();
    return 0;
}
