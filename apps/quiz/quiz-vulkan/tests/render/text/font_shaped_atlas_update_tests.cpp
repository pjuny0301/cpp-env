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

} // namespace

int main()
{
    test_trace_reports_upload_ready_payload_queued();
    test_trace_reports_clean_reuse_when_update_is_absent();
    test_trace_reports_skipped_payload_and_missing_cache_key();
    test_trace_reports_payload_byte_count_mismatch();
    return 0;
}
