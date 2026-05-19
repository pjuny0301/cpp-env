#include "render/text/fake_text_engine.h"
#include "render/text/text_frame_upload_handoff.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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
    if (const char* external_dir = std::getenv("QUIZ_VULKAN_DESKTOP_EXTERNAL_DIR");
        external_dir != nullptr && external_dir[0] != '\0') {
        return std::filesystem::path(external_dir);
    }

    const std::filesystem::path repo_root =
        quiz_vulkan_app_root_from_this_test().parent_path().parent_path().parent_path();
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
    for (const std::filesystem::path& path : freetype_real_font_fixture_candidates()) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return {};
}

std::filesystem::path korean_multiline_font_fixture()
{
    const std::filesystem::path path =
        desktop_external_root() / "harfbuzz-14.2.0" / "test" / "api" / "fonts"
        / "varc-ac00-ac01.ttf";
    return std::filesystem::exists(path) ? path : std::filesystem::path{};
}

quiz_vulkan::render::render_text_style_catalog style_catalog_for(
    const std::string& family,
    const std::string& style_id)
{
    quiz_vulkan::render::render_text_style_catalog catalog;
    catalog.fallback_style = quiz_vulkan::render::render_text_style{
        .id = "fallback",
        .font_family = "Sans",
        .font_size = 12.0f,
        .line_height = 14.0f,
        .font_weight = 400,
    };
    catalog.styles.push_back(quiz_vulkan::render::render_text_style{
        .id = style_id,
        .font_family = family,
        .font_size = 32.0f,
        .line_height = 36.0f,
        .font_weight = 400,
    });
    return catalog;
}

quiz_vulkan::render::render_text_style_catalog multiline_korean_style_catalog()
{
    quiz_vulkan::render::render_text_style_catalog catalog;
    catalog.fallback_style = quiz_vulkan::render::render_text_style{
        .id = "fallback",
        .font_family = "Sans",
        .font_size = 12.0f,
        .line_height = 14.0f,
        .font_weight = 400,
    };
    catalog.styles.push_back(quiz_vulkan::render::render_text_style{
        .id = "payload-korean",
        .font_family = "Payload Korean Varc",
        .font_size = 32.0f,
        .line_height = 36.0f,
        .font_weight = 400,
    });
    catalog.styles.push_back(quiz_vulkan::render::render_text_style{
        .id = "payload-latin",
        .font_family = "Payload Latin Sans",
        .font_size = 32.0f,
        .line_height = 36.0f,
        .font_weight = 400,
    });
    return catalog;
}

quiz_vulkan::render::render_text_request single_glyph_request(
    quiz_vulkan::render::render_text_style_catalog catalog,
    const std::string& style_id)
{
    return quiz_vulkan::render::render_text_request{
        .text_runs = {
            quiz_vulkan::render::render_text_run{
                .text = "A",
                .style_token = style_id,
            },
        },
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 240.0f, 0.0f},
        .style_catalog = std::move(catalog),
        .options = quiz_vulkan::render::render_text_options{
            .wrap = quiz_vulkan::render::render_text_wrap_mode::no_wrap,
            .alignment = quiz_vulkan::render::render_text_alignment::start,
            .max_lines = 0,
        },
    };
}

quiz_vulkan::render::render_text_request latin_residency_request(std::string text)
{
    return quiz_vulkan::render::render_text_request{
        .text_runs = {
            quiz_vulkan::render::render_text_run{
                .text = std::move(text),
                .style_token = "payload-freetype",
            },
        },
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 320.0f, 0.0f},
        .style_catalog = style_catalog_for("Payload FreeType Sans", "payload-freetype"),
        .options = quiz_vulkan::render::render_text_options{
            .wrap = quiz_vulkan::render::render_text_wrap_mode::no_wrap,
            .alignment = quiz_vulkan::render::render_text_alignment::start,
            .max_lines = 0,
        },
    };
}

quiz_vulkan::render::render_text_request multiline_korean_request()
{
    return quiz_vulkan::render::render_text_request{
        .text_runs = {
            quiz_vulkan::render::render_text_run{
                .text = "가각\n",
                .style_token = "payload-korean",
            },
            quiz_vulkan::render::render_text_run{
                .text = "A",
                .style_token = "payload-latin",
            },
        },
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 320.0f, 0.0f},
        .style_catalog = multiline_korean_style_catalog(),
        .options = quiz_vulkan::render::render_text_options{
            .wrap = quiz_vulkan::render::render_text_wrap_mode::no_wrap,
            .alignment = quiz_vulkan::render::render_text_alignment::start,
            .max_lines = 0,
        },
    };
}

quiz_vulkan::render::render_text_request hangul_residency_request(std::string text)
{
    return quiz_vulkan::render::render_text_request{
        .text_runs = {
            quiz_vulkan::render::render_text_run{
                .text = std::move(text),
                .style_token = "payload-korean",
            },
        },
        .bounds = quiz_vulkan::render::render_rect{0.0f, 0.0f, 320.0f, 0.0f},
        .style_catalog = multiline_korean_style_catalog(),
        .options = quiz_vulkan::render::render_text_options{
            .wrap = quiz_vulkan::render::render_text_wrap_mode::no_wrap,
            .alignment = quiz_vulkan::render::render_text_alignment::start,
            .max_lines = 0,
        },
    };
}

std::vector<std::string> upload_request_ids_for_diagnostics(
    const quiz_vulkan::render::fake_text_engine_diagnostics& diagnostics)
{
    std::vector<std::string> upload_request_ids;
    for (const quiz_vulkan::render::render_text_frame_draw_packet_snapshot& packet :
         diagnostics.text_frame_draw_plan.packets) {
        if (!packet.atlas_upload_request_id.empty()) {
            upload_request_ids.push_back(packet.atlas_upload_request_id);
        }
    }
    return upload_request_ids;
}

quiz_vulkan::render::render_text_glyph_atlas_upload_result_snapshot upload_result_for_diagnostics(
    const quiz_vulkan::render::fake_text_engine_diagnostics& diagnostics,
    std::vector<std::string> upload_request_ids)
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_page_plan_snapshot page_plan =
        plan_render_text_glyph_atlas_pages(diagnostics.glyph_atlas_materializations);
    const render_text_glyph_atlas_upload_operation_plan_snapshot operation_plan =
        plan_render_text_glyph_atlas_upload_operations(
            page_plan,
            diagnostics.glyph_atlas_materializations);
    return make_render_text_glyph_atlas_upload_result(render_text_glyph_atlas_upload_result_request{
        .operation_plan = operation_plan,
        .upload_request_ids = std::move(upload_request_ids),
    });
}

quiz_vulkan::render::render_text_glyph_atlas_upload_result_snapshot upload_result_for_diagnostics(
    const quiz_vulkan::render::fake_text_engine_diagnostics& diagnostics)
{
    return upload_result_for_diagnostics(
        diagnostics,
        upload_request_ids_for_diagnostics(diagnostics));
}

quiz_vulkan::render::render_text_frame_resource_packet_materialization
resource_packets_for_diagnostics(
    const quiz_vulkan::render::fake_text_engine_diagnostics& diagnostics)
{
    using namespace quiz_vulkan::render;

    const render_text_glyph_atlas_upload_result_snapshot upload_result =
        upload_result_for_diagnostics(diagnostics);
    const render_text_frame_upload_handoff_snapshot handoff =
        make_render_text_frame_upload_handoff(render_text_frame_upload_handoff_request{
            .frame = diagnostics.text_frame_snapshot,
            .draw_plan = diagnostics.text_frame_draw_plan,
            .upload_result = upload_result,
        });
    return materialize_render_text_frame_resource_packets(
        render_text_frame_resource_packet_materialization_request{
            .draw_plan = diagnostics.text_frame_draw_plan,
            .upload_handoff = handoff,
        });
}

quiz_vulkan::render::render_text_renderer_glyph_quad_packet_snapshot glyph_quads_for_diagnostics(
    const quiz_vulkan::render::fake_text_engine_diagnostics& diagnostics)
{
    using namespace quiz_vulkan::render;

    return make_render_text_renderer_glyph_quad_packets(
        render_text_renderer_glyph_quad_packet_request{
            .resource_packets = resource_packets_for_diagnostics(diagnostics),
        });
}

quiz_vulkan::render::render_text_render_frame_handoff_summary_snapshot
summary_for_diagnostics(
    const quiz_vulkan::render::fake_text_engine_diagnostics& diagnostics,
    const quiz_vulkan::render::render_text_frame_draw_plan_diff& draw_diff = {})
{
    using namespace quiz_vulkan::render;

    const render_text_renderer_glyph_quad_packet_snapshot glyph_quads =
        glyph_quads_for_diagnostics(diagnostics);
    return make_render_text_render_frame_handoff_summary(
        render_text_render_frame_handoff_summary_request{
            .frame = diagnostics.text_frame_snapshot,
            .draw_plan = diagnostics.text_frame_draw_plan,
            .glyph_quads = glyph_quads,
            .draw_packet_diff = draw_diff,
            .has_draw_packet_diff = draw_diff.previous_frame_id != draw_diff.current_frame_id
                || draw_diff.policy.added_packet_count > 0U
                || draw_diff.policy.removed_packet_count > 0U
                || draw_diff.policy.changed_packet_count > 0U
                || draw_diff.policy.readiness_changed_packet_count > 0U,
        });
}

bool payload_has_matching_harfbuzz_handoff(
    const quiz_vulkan::render::fake_text_engine_diagnostics& diagnostics,
    const quiz_vulkan::render::render_text_rasterized_glyph_atlas_payload_snapshot& payload)
{
    return std::any_of(
        diagnostics.shaping_handoffs.begin(),
        diagnostics.shaping_handoffs.end(),
        [&](const quiz_vulkan::render::fake_text_engine_shaping_handoff_snapshot& handoff) {
            return handoff.used_harfbuzz
                && handoff.materialized_font_bytes
                && handoff.glyph_id == payload.glyph_id
                && handoff.glyph_id == payload.cache_key.glyph_id
                && handoff.resolved_face_id == payload.resolved_face_id
                && handoff.run_index == payload.run_index
                && handoff.byte_offset == payload.byte_offset
                && handoff.byte_count == payload.byte_count;
        });
}

bool materialization_preserves_real_shaping_and_raster_stack(
    const quiz_vulkan::render::render_text_glyph_atlas_materialization_snapshot& materialization,
    const quiz_vulkan::render::render_text_rasterized_glyph_atlas_payload_snapshot& payload)
{
    return materialization.cache_key == payload.cache_key
        && materialization.resolved_glyph_id == payload.cache_key.glyph_id
        && std::find(
               materialization.shaped_glyph_ids.begin(),
               materialization.shaped_glyph_ids.end(),
               payload.cache_key.glyph_id)
            != materialization.shaped_glyph_ids.end()
        && materialization.shaping_font_backend_library
            == quiz_vulkan::render::render_text_font_backend_library::harfbuzz
        && materialization.raster_font_backend_library
            == quiz_vulkan::render::render_text_font_backend_library::freetype
        && !materialization.shaping_font_backend_used_deterministic_fallback
        && !materialization.raster_font_backend_used_deterministic_fallback;
}

bool contains_atlas_key(
    const std::vector<quiz_vulkan::render::glyph_atlas_key>& keys,
    const quiz_vulkan::render::glyph_atlas_key& key)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

void select_real_text_stack(quiz_vulkan::render::fake_text_engine& engine)
{
    using namespace quiz_vulkan::render;

    engine.set_font_backend_selection_candidates({
        make_render_text_harfbuzz_backend_candidate(true),
        make_render_text_freetype_backend_candidate(true),
        make_render_text_utf8proc_backend_candidate(true),
        make_render_text_deterministic_fake_backend_candidate(),
    });
}

void test_file_backed_freetype_selection_rasterizes_real_payload()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path font_path = first_available_freetype_real_font_fixture();
    if (font_path.empty() || !render_text_freetype_memory_face_adapter_available()) {
        return;
    }

    fake_text_engine engine;
    select_real_text_stack(engine);
    engine.add_font_face(font_face_descriptor{
        .id = 810,
        .family = "Payload FreeType Sans",
        .source_uri = font_path.generic_string(),
        .version = "external-fixture",
        .license = "harfbuzz-test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'A'},
        },
        .weight = 400,
    });

    const render_text_layout layout = engine.layout_text(single_glyph_request(
        style_catalog_for("Payload FreeType Sans", "payload-freetype"),
        "payload-freetype"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 1U, "file-backed font lays out one glyph");
    if (render_text_harfbuzz_shaping_adapter_available()) {
        require(diagnostics.has_shaping_handoffs(), "file-backed font records shaping handoff");
        require(
            diagnostics.shaping_handoff_policy.harfbuzz_run_count == 1U,
            "file-backed font uses HarfBuzz shaping before raster handoff");
        require(
            diagnostics.shaping_handoffs.front().materialized_font_bytes,
            "HarfBuzz handoff records materialized font bytes");
        require(
            diagnostics.shaping_handoffs.front().used_harfbuzz,
            "HarfBuzz handoff marks real shaping backend");
    }
    require(
        diagnostics.font_backend_rasterization_selection.selected.library
            == render_text_font_backend_library::freetype,
        "raster selection chooses FreeType");
    require(
        diagnostics.rasterized_glyph_atlas_payloads.size() == 1U,
        "file-backed font records one raster payload");

    const render_text_rasterized_glyph_atlas_payload_snapshot& payload =
        diagnostics.rasterized_glyph_atlas_payloads.front();
    require(payload.font_backend_library == render_text_font_backend_library::freetype, "payload records FreeType");
    require(payload.used_freetype_rasterizer, "payload records real FreeType rasterizer use");
    require(!payload.uses_deterministic_rasterizer, "payload bypasses deterministic rasterizer");
    require(payload.materialized_font_bytes, "payload records materialized font bytes");
    require(
        payload.source_bytes_status == render_text_font_source_bytes_load_status::loaded,
        "payload records loaded source bytes");
    require(payload.deterministic_fallback_reason.empty(), "real FreeType payload has no fallback reason");
    require(payload.status == render_text_font_rasterizer_status::rasterized, "payload rasterized");
    require(payload.upload_ready, "payload is upload-ready");
    require(!payload.skipped, "payload is not skipped");
    require(payload.alpha_bytes > 0U, "payload has alpha bytes");
    require(payload.rgba_bytes == payload.alpha_bytes * 4U, "payload expands alpha bytes to RGBA");
    require(payload.diagnostic.find("FreeType") != std::string::npos, "payload diagnostic names FreeType");
    require(payload.source_label == font_path.generic_string(), "payload source label names the font file");

    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.freetype_rasterizer_count == 1U,
        "payload policy counts FreeType rasterizer use");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.deterministic_rasterizer_count == 0U,
        "payload policy records no deterministic rasterizer use");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.materialized_font_bytes_count == 1U,
        "payload policy counts materialized bytes");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.deterministic_fallback_reason_count == 0U,
        "payload policy records no fallback reason");

    require(
        diagnostics.glyph_atlas_materializations.size() == 1U,
        "file-backed font records one atlas materialization");
    const render_text_glyph_atlas_materialization_snapshot& materialization =
        diagnostics.glyph_atlas_materializations.front();
    require(
        materialization.status == render_text_glyph_atlas_materialization_status::materialized_upload_ready,
        "real FreeType payload materializes an upload-ready atlas update");
    require(materialization.payload_upload_ready, "materialization links upload-ready raster payload");
    require(
        materialization.raster_font_backend_library == render_text_font_backend_library::freetype,
        "materialization records FreeType raster backend");
    require(materialization.cache_key == payload.cache_key, "materialization preserves payload atlas key");
    require(materialization.payload_rgba_bytes == payload.rgba_bytes, "materialization preserves payload byte count");

    require(diagnostics.has_line_run_atlas_uploads(), "file-backed font records line/run upload handoff");
    const fake_text_engine_line_run_atlas_upload_snapshot& line_upload =
        diagnostics.line_run_atlas_uploads.front();
    require(line_upload.has_raster_payload, "line/run upload links raster payload");
    require(line_upload.raster_payload_upload_ready, "line/run upload records upload-ready raster payload");
    require(line_upload.upload_ready, "line/run upload is upload-ready");
    require(line_upload.cache_key == materialization.cache_key, "line/run upload preserves atlas key");
    require(
        line_upload.upload_request_id == diagnostics.atlas_upload_request_bridge.requests.front().request_id,
        "line/run upload preserves upload request id");

    require(diagnostics.has_text_frame_draw_plan(), "file-backed font records renderer draw plan evidence");
    require(
        diagnostics.text_frame_draw_plan.policy.packet_count == 1U,
        "pending draw plan records one glyph packet");
    require(
        diagnostics.text_frame_draw_plan.policy.frame_not_ready_count == 1U,
        "pending draw plan blocks before atlas upload consumption");
    require(
        diagnostics.text_frame_draw_plan.policy.real_backend_count == 1U,
        "pending draw plan preserves real backend materialization evidence");
    const render_text_frame_draw_plan_snapshot pending_draw_plan = diagnostics.text_frame_draw_plan;

    const std::string upload_request_id = line_upload.upload_request_id;
    const glyph_atlas_key cache_key = line_upload.cache_key;
    const render_text_atlas_page_id page_id = line_upload.page.id;
    const render_text_revision page_revision = line_upload.page.revision;
    const std::vector<render_text_atlas_update> consumed_updates = engine.consume_atlas_updates();
    require(!consumed_updates.empty(), "file-backed font exposes atlas update for consumption");

    const fake_text_engine_diagnostics& ready_diagnostics = engine.last_diagnostics();
    require(
        ready_diagnostics.text_frame_snapshot.ready_for_renderer(),
        "consumed atlas update makes text frame renderer-ready");
    require(
        ready_diagnostics.text_frame_draw_plan.frame_ready_for_renderer,
        "draw plan sees renderer-ready text frame");
    require(
        ready_diagnostics.text_frame_draw_plan.policy.draw_ready_count == 1U,
        "draw plan exposes one renderer-ready glyph packet");
    require(
        ready_diagnostics.text_frame_draw_plan.policy.upload_consumed_count == 1U,
        "draw plan records consumed upload request");
    require(
        ready_diagnostics.text_frame_draw_plan.policy.real_backend_count == 1U,
        "ready draw plan keeps real backend count");

    const render_text_frame_draw_packet_snapshot& draw_packet =
        ready_diagnostics.text_frame_draw_plan.packets.front();
    require(draw_packet.drawable(), "real font draw packet is drawable after upload consumption");
    require(
        draw_packet.status == render_text_frame_draw_packet_status::draw_ready,
        "real font draw packet is draw-ready");
    require(draw_packet.used_real_backend, "draw packet records real backend use");
    require(!draw_packet.used_deterministic_fallback, "draw packet does not record deterministic fallback");
    require(draw_packet.upload_consumed, "draw packet records consumed upload");
    require(draw_packet.cache_key == cache_key, "draw packet preserves atlas cache key");
    require(draw_packet.atlas_upload_request_id == upload_request_id, "draw packet preserves upload request id");
    require(draw_packet.page_id == page_id, "draw packet preserves atlas page id");
    require(draw_packet.page_revision == page_revision, "draw packet preserves atlas page revision");
    require(draw_packet.uv_bounds.valid, "draw packet derives valid UV bounds");
    require(
        draw_packet.atlas_consumption.cache_key == cache_key,
        "draw packet atlas consumption preserves cache key");
    require(
        draw_packet.atlas_consumption.upload_generation == page_revision,
        "draw packet atlas consumption records upload generation");
    require(!draw_packet.atlas_consumption.missing_glyph, "draw packet atlas consumption has glyph coverage");

    const render_text_frame_draw_plan_diff draw_diff =
        diff_render_text_frame_draw_plans(pending_draw_plan, ready_diagnostics.text_frame_draw_plan);
    require(draw_diff.has_readiness_or_fallback_changes(), "real font draw diff records readiness transition");
    require(draw_diff.policy.readiness_recovery_count == 1U, "real font draw diff counts blocked-to-ready recovery");
    require(draw_diff.policy.glyph_quad_count_delta == 1, "real font draw diff records one new glyph quad");
    require(
        draw_diff.policy.glyph_quad_count_changed_packet_count == 1U,
        "real font draw diff counts glyph quad materialization");
    require(draw_diff.readiness_recovered_packet_ids.size() == 1U, "real font draw diff exposes recovered packet id");
    require(draw_diff.packet_diffs.size() == 1U, "real font draw diff exposes compact packet details");
    const render_text_frame_draw_packet_consumption_diff& packet_diff = draw_diff.packet_diffs.front();
    require(packet_diff.readiness_recovered, "real font packet diff marks readiness recovery");
    require(packet_diff.previous_glyph_quad_count == 0U, "pending real font packet has no glyph quad");
    require(packet_diff.current_glyph_quad_count == 1U, "ready real font packet has one glyph quad");
    require(packet_diff.current_cache_key == cache_key, "real font packet diff preserves atlas key");
    require(packet_diff.current_upload_request_id == upload_request_id, "real font packet diff preserves upload id");
    require(
        packet_diff.current_upload_generation == page_revision,
        "real font packet diff preserves atlas upload generation");
    require(packet_diff.current_line_index == draw_packet.atlas_consumption.line_index, "real font packet diff preserves line index");
    require(packet_diff.current_run_index == draw_packet.run_index, "real font packet diff preserves run index");

    const render_text_render_frame_handoff_summary_snapshot summary =
        summary_for_diagnostics(ready_diagnostics, draw_diff);
    require(summary.ok(), "real font render frame handoff summary is renderer-ready");
    require(summary.policy.draw_packet_count == 1U, "real font summary counts draw packet");
    require(summary.policy.glyph_quad_ready_count == 1U, "real font summary counts ready glyph quad");
    require(summary.policy.glyph_quad_count == 1U, "real font summary counts glyph quad packet");
    require(summary.policy.real_backend_count > 0U, "real font summary preserves real backend evidence");
    require(!summary.policy.used_deterministic_fallback, "real font summary does not mark deterministic fallback");
    if (ready_diagnostics.text_frame_snapshot.policy.total_upload_rgba_bytes > 0U) {
        require(
            summary.policy.total_upload_rgba_bytes
                == ready_diagnostics.text_frame_snapshot.policy.total_upload_rgba_bytes,
            "real font summary preserves frame upload bytes");
    }
    require(summary.policy.upload_request_count == 1U, "real font summary exposes upload request id");
    require(summary.policy.atlas_page_count == 1U, "real font summary exposes atlas page id");
    require(summary.policy.added_packet_count == 0U, "real font summary has no added draw packets");
    require(summary.policy.changed_packet_count == 1U, "real font summary carries changed draw packet diff");
    require(summary.changed_packet_ids.size() == 1U, "real font summary exposes changed draw packet id");
}

void test_repeated_file_backed_freetype_text_reuses_resident_atlas_payload()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path font_path = first_available_freetype_real_font_fixture();
    if (font_path.empty() || !render_text_freetype_memory_face_adapter_available()) {
        return;
    }

    fake_text_engine engine;
    select_real_text_stack(engine);
    engine.add_font_face(font_face_descriptor{
        .id = 815,
        .family = "Payload FreeType Sans",
        .source_uri = font_path.generic_string(),
        .version = "external-fixture",
        .license = "harfbuzz-test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'B'},
        },
        .weight = 400,
    });

    const render_text_layout first_layout = engine.layout_text(latin_residency_request("A"));
    const fake_text_engine_diagnostics first = engine.last_diagnostics();
    require(first_layout.glyphs.size() == 1U, "first FreeType cache frame lays out one glyph");
    require(
        first.rasterized_glyph_atlas_payload_policy.freetype_rasterizer_count == 1U,
        "first FreeType cache frame calls the real rasterizer");
    require(
        first.rasterized_glyph_atlas_payload_policy.materialized_font_bytes_count == 1U,
        "first FreeType cache frame materializes font bytes");
    require(
        first.rasterized_glyph_atlas_payload_policy.atlas_cache_hit_count == 0U,
        "first FreeType cache frame is not an atlas cache hit");
    require(
        first.glyph_atlas_materialization_policy.upload_ready_count == 1U,
        "first FreeType cache frame queues one atlas upload");
    require(
        first.atlas_upload_request_bridge.policy.unique_upload_request_count == 1U,
        "first FreeType cache frame exposes one upload request");

    const glyph_atlas_key cached_key = first.rasterized_glyph_atlas_payloads.front().cache_key;
    const render_text_atlas_page_id cached_page_id = first.glyph_atlas_placements.front().page.id;

    const render_text_layout second_layout = engine.layout_text(latin_residency_request("A"));
    const fake_text_engine_diagnostics second = engine.last_diagnostics();
    require(second_layout.glyphs.size() == 1U, "second FreeType cache frame lays out repeated glyph");
    require(second.rasterized_glyph_atlas_payloads.size() == 1U, "second FreeType cache frame records payload reuse");
    const render_text_rasterized_glyph_atlas_payload_snapshot& reused_payload =
        second.rasterized_glyph_atlas_payloads.front();
    require(
        reused_payload.status == render_text_font_rasterizer_status::atlas_cache_hit,
        "second FreeType cache frame reports atlas cache hit instead of fresh rasterization");
    require(reused_payload.atlas_cache_hit, "second FreeType cache frame marks payload atlas cache hit");
    require(reused_payload.reused_atlas_payload, "second FreeType cache frame marks resident payload reuse");
    require(
        reused_payload.rasterization_skipped_for_atlas_cache_hit,
        "second FreeType cache frame skips rerasterization for resident payload");
    require(!reused_payload.used_freetype_rasterizer, "second FreeType cache frame does not rerun FreeType");
    require(!reused_payload.uses_deterministic_rasterizer, "second FreeType cache frame does not use fake rasterizer");
    require(!reused_payload.materialized_font_bytes, "second FreeType cache frame does not rematerialize font bytes");
    require(reused_payload.cache_key == cached_key, "second FreeType cache frame preserves glyph cache key");
    require(reused_payload.alpha_bytes == 0U, "second FreeType cache frame emits no new alpha payload bytes");
    require(reused_payload.rgba_bytes == 0U, "second FreeType cache frame emits no new RGBA payload bytes");
    require(
        reused_payload.diagnostic.find("resident atlas slot") != std::string::npos,
        "second FreeType cache frame diagnostic names resident atlas reuse");
    require(
        second.rasterized_glyph_atlas_payload_policy.freetype_rasterizer_count == 0U,
        "second FreeType cache frame records no real rasterizer call");
    require(
        second.rasterized_glyph_atlas_payload_policy.materialized_font_bytes_count == 0U,
        "second FreeType cache frame records no fresh font byte materialization");
    require(
        second.rasterized_glyph_atlas_payload_policy.atlas_cache_hit_count == 1U,
        "second FreeType cache frame counts one atlas cache hit");
    require(
        second.rasterized_glyph_atlas_payload_policy.reused_atlas_payload_count == 1U,
        "second FreeType cache frame counts one reused atlas payload");
    require(
        second.rasterized_glyph_atlas_payload_policy.rasterization_skipped_for_atlas_cache_hit_count == 1U,
        "second FreeType cache frame counts one skipped rasterization");
    require(second.glyph_atlas_placements.front().cache_hit, "second FreeType cache frame reuses atlas placement");
    require(
        second.glyph_atlas_placements.front().page.id == cached_page_id,
        "second FreeType cache frame preserves atlas page residency");
    require(
        second.glyph_atlas_materialization_policy.clean_reuse_count == 1U,
        "second FreeType cache frame materializes clean reuse");
    require(
        second.line_run_atlas_upload_policy.clean_reuse_cluster_count == 1U,
        "second FreeType cache frame line/run upload records clean reuse");
    require(
        second.atlas_upload_request_bridge.policy.unique_upload_request_count == 0U,
        "second FreeType cache frame emits no new unique upload request");

    const render_text_layout third_layout = engine.layout_text(latin_residency_request("AB"));
    const fake_text_engine_diagnostics third = engine.last_diagnostics();
    require(third_layout.glyphs.size() == 2U, "third FreeType cache frame lays out repeated and new glyphs");
    require(third.rasterized_glyph_atlas_payloads.size() == 2U, "third FreeType cache frame records two payload decisions");
    require(
        third.rasterized_glyph_atlas_payload_policy.atlas_cache_hit_count == 1U,
        "third FreeType cache frame reuses one resident payload");
    require(
        third.rasterized_glyph_atlas_payload_policy.freetype_rasterizer_count == 1U,
        "third FreeType cache frame rasterizes only the new glyph");
    require(
        third.rasterized_glyph_atlas_payload_policy.materialized_font_bytes_count == 1U,
        "third FreeType cache frame materializes bytes only for the new glyph");
    require(
        third.glyph_atlas_materialization_policy.clean_reuse_count == 1U,
        "third FreeType cache frame records one clean reuse materialization");
    require(
        third.glyph_atlas_materialization_policy.upload_ready_count == 1U,
        "third FreeType cache frame records one new upload-ready materialization");
    require(
        third.line_run_atlas_upload_policy.clean_reuse_cluster_count == 1U,
        "third FreeType cache frame line/run evidence preserves reused glyph");
    require(
        third.line_run_atlas_upload_policy.upload_ready_cluster_count == 1U,
        "third FreeType cache frame line/run evidence exposes new glyph upload");
    require(
        third.atlas_upload_request_bridge.policy.unique_upload_request_count == 1U,
        "third FreeType cache frame emits one new upload request");
}

void test_harfbuzz_shaped_glyph_ids_drive_freetype_atlas_residency()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path font_path = first_available_freetype_real_font_fixture();
    if (font_path.empty()
        || !render_text_harfbuzz_shaping_adapter_available()
        || !render_text_freetype_memory_face_adapter_available()) {
        return;
    }

    fake_text_engine engine;
    select_real_text_stack(engine);
    engine.add_font_face(font_face_descriptor{
        .id = 816,
        .family = "Payload FreeType Sans",
        .source_uri = font_path.generic_string(),
        .version = "external-fixture",
        .license = "harfbuzz-test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'B'},
        },
        .weight = 400,
    });

    const render_text_layout first_layout = engine.layout_text(latin_residency_request("AB"));
    const fake_text_engine_diagnostics first = engine.last_diagnostics();
    require(first_layout.glyphs.size() == 2U, "first HarfBuzz/FreeType frame lays out two glyphs");
    require(first.shaping_handoff_policy.harfbuzz_run_count == 1U, "first frame uses HarfBuzz shaping");
    require(
        first.shaping_handoff_policy.materialized_font_byte_run_count == 1U,
        "first frame materializes font bytes for HarfBuzz");
    require(
        first.shaping_line_run_evidence_policy.harfbuzz_cluster_count == 2U,
        "first frame records two HarfBuzz-shaped clusters");
    require(
        first.rasterized_glyph_atlas_payload_policy.freetype_rasterizer_count == 2U,
        "first frame rasterizes both HarfBuzz glyphs with FreeType");
    require(
        first.rasterized_glyph_atlas_payload_policy.materialized_font_bytes_count == 2U,
        "first frame materializes font bytes for both FreeType payloads");
    require(
        first.glyph_atlas_materialization_policy.upload_ready_count == 2U,
        "first frame queues two upload-ready atlas materializations");
    require(
        first.atlas_upload_request_bridge.policy.unique_upload_request_count == 2U,
        "first frame exposes one upload request per shaped glyph");

    std::vector<glyph_atlas_key> first_keys;
    bool saw_real_font_glyph_id = false;
    for (const render_text_rasterized_glyph_atlas_payload_snapshot& payload :
         first.rasterized_glyph_atlas_payloads) {
        require(payload.used_freetype_rasterizer, "first payload uses real FreeType");
        require(payload.materialized_font_bytes, "first payload uses materialized font bytes");
        require(payload.status == render_text_font_rasterizer_status::rasterized, "first payload rasterizes");
        require(payload.cache_key.glyph_id == payload.glyph_id, "payload cache key uses shaped glyph id");
        require(
            payload_has_matching_harfbuzz_handoff(first, payload),
            "payload glyph id matches a materialized HarfBuzz handoff");
        saw_real_font_glyph_id = saw_real_font_glyph_id || payload.cache_key.glyph_id != payload.codepoint;
        first_keys.push_back(payload.cache_key);

        const auto materialization = std::find_if(
            first.glyph_atlas_materializations.begin(),
            first.glyph_atlas_materializations.end(),
            [&](const render_text_glyph_atlas_materialization_snapshot& candidate) {
                return candidate.cache_key == payload.cache_key;
            });
        require(
            materialization != first.glyph_atlas_materializations.end(),
            "first payload has matching atlas materialization");
        require(
            materialization_preserves_real_shaping_and_raster_stack(*materialization, payload),
            "materialization preserves HarfBuzz shaping and FreeType raster evidence");
        require(
            materialization->status
                == render_text_glyph_atlas_materialization_status::materialized_upload_ready,
            "first materialization is upload-ready");
    }
    require(
        saw_real_font_glyph_id,
        "HarfBuzz path records at least one font glyph id distinct from the Unicode codepoint");

    const std::vector<std::string> first_upload_request_ids =
        upload_request_ids_for_diagnostics(first);
    require(first_upload_request_ids.size() == 2U, "first frame exposes two glyph upload request ids");
    fake_text_engine_diagnostics ready_first = first;
    ready_first.text_frame_snapshot =
        render_text_frame_snapshot_with_consumed_atlas_updates(
            first.text_frame_snapshot,
            first_upload_request_ids,
            first_upload_request_ids.size());
    ready_first.text_frame_draw_plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = ready_first.text_frame_snapshot,
            .materializations = ready_first.glyph_atlas_materializations,
            .item_index = 0U,
        });
    require(ready_first.text_frame_snapshot.ready_for_renderer(), "first frame is ready after upload consumption");
    require(
        ready_first.text_frame_draw_plan.policy.draw_ready_count == 2U,
        "first frame draw plan has two drawable glyph packets after upload consumption");
    require(
        ready_first.text_frame_draw_plan.policy.upload_consumed_count == 2U,
        "first frame draw plan records consumed atlas uploads");

    const render_text_frame_resource_packet_materialization first_resources =
        resource_packets_for_diagnostics(ready_first);
    require(first_resources.ok(), "first real frame resource packets are renderer-boundary ready");
    require(first_resources.policy.ready_packet_count == 2U, "first frame has two ready resource packets");
    require(first_resources.policy.uploaded_packet_count == 2U, "first frame resource packets are upload-backed");
    require(first_resources.policy.clean_reuse_packet_count == 0U, "first frame has no clean resource reuse yet");
    require(first_resources.policy.used_real_backend, "first frame resource packets preserve real backend evidence");

    const render_text_renderer_glyph_quad_packet_snapshot first_quads =
        make_render_text_renderer_glyph_quad_packets(render_text_renderer_glyph_quad_packet_request{
            .resource_packets = first_resources,
        });
    require(first_quads.ok(), "first real frame produces renderer-ready glyph quad packets");
    require(first_quads.policy.quad_packet_count == 2U, "first frame produces one glyph quad per shaped glyph");
    require(first_quads.policy.ready_quad_count == 2U, "first frame glyph quads are ready");
    require(first_quads.policy.uploaded_quad_count == 2U, "first frame glyph quads carry upload evidence");
    require(first_quads.policy.clean_reuse_quad_count == 0U, "first frame glyph quads are not clean reuse");
    require(first_quads.policy.used_real_backend, "first frame glyph quads preserve real backend evidence");
    require(!first_quads.policy.used_deterministic_fallback, "first frame glyph quads avoid fallback evidence");
    for (const render_text_renderer_glyph_quad_packet_record& quad : first_quads.packets) {
        require(quad.drawable(), "first-frame real glyph quad is drawable");
        require(quad.uploaded, "first-frame real glyph quad records uploaded payload");
        require(!quad.clean_reuse, "first-frame real glyph quad is not clean reuse");
        require(quad.used_real_backend, "first-frame real glyph quad preserves real backend flag");
        require(!quad.used_deterministic_fallback, "first-frame real glyph quad avoids deterministic fallback");
        require(quad.upload_consumed, "first-frame real glyph quad records consumed upload");
        require(quad.upload_rgba_bytes > 0U, "first-frame real glyph quad preserves upload byte count");
        require(quad.uv_bounds.valid, "first-frame real glyph quad has valid UVs");
        require(quad.layout_bounds.width > 0.0f, "first-frame real glyph quad preserves layout bounds");
        require(contains_atlas_key(first_keys, quad.cache_key), "first-frame real glyph quad preserves atlas key");
        require(
            quad.atlas_consumption.cache_key == quad.cache_key,
            "first-frame real glyph quad atlas consumption preserves cache key");
        require(
            quad.atlas_consumption.upload_generation == quad.page_revision,
            "first-frame real glyph quad preserves upload generation");
        require(!quad.atlas_consumption.missing_glyph, "first-frame real glyph quad has glyph coverage");
    }

    const render_text_layout second_layout = engine.layout_text(latin_residency_request("AB"));
    const fake_text_engine_diagnostics second = engine.last_diagnostics();
    require(second_layout.glyphs.size() == 2U, "second HarfBuzz/FreeType frame lays out repeated glyphs");
    require(second.shaping_handoff_policy.harfbuzz_run_count == 1U, "second frame still uses HarfBuzz");
    require(
        second.shaping_line_run_evidence_policy.harfbuzz_cluster_count == 2U,
        "second frame keeps HarfBuzz cluster evidence");
    require(
        second.rasterized_glyph_atlas_payload_policy.atlas_cache_hit_count == 2U,
        "second frame reuses resident atlas payloads for both glyphs");
    require(
        second.rasterized_glyph_atlas_payload_policy.freetype_rasterizer_count == 0U,
        "second frame does not rerun FreeType for resident glyphs");
    require(
        second.rasterized_glyph_atlas_payload_policy.materialized_font_bytes_count == 0U,
        "second frame does not rematerialize font bytes for resident glyph payloads");
    require(
        second.glyph_atlas_materialization_policy.clean_reuse_count == 2U,
        "second frame materializes two clean atlas reuses");
    require(
        second.line_run_atlas_upload_policy.clean_reuse_cluster_count == 2U,
        "second frame line/run evidence records two clean reuses");
    require(
        second.atlas_upload_request_bridge.policy.unique_upload_request_count == 0U,
        "second frame emits no new atlas upload requests");

    for (const render_text_rasterized_glyph_atlas_payload_snapshot& payload :
         second.rasterized_glyph_atlas_payloads) {
        require(payload.status == render_text_font_rasterizer_status::atlas_cache_hit, "second payload is a cache hit");
        require(payload.atlas_cache_hit, "second payload marks atlas cache hit");
        require(payload.reused_atlas_payload, "second payload marks resident payload reuse");
        require(
            payload.rasterization_skipped_for_atlas_cache_hit,
            "second payload skips rasterization because residency is available");
        require(!payload.used_freetype_rasterizer, "second payload does not rerun FreeType");
        require(!payload.materialized_font_bytes, "second payload does not reload font bytes");
        require(contains_atlas_key(first_keys, payload.cache_key), "second payload preserves first-frame atlas key");
        require(
            payload_has_matching_harfbuzz_handoff(second, payload),
            "cache-hit payload still matches current HarfBuzz-shaped glyph id");
    }

    const render_text_frame_resource_packet_materialization second_resources =
        resource_packets_for_diagnostics(second);
    require(second_resources.ok(), "second real frame resource packets are renderer-boundary ready");
    require(second_resources.policy.ready_packet_count == 2U, "second frame has two ready resource packets");
    require(second_resources.policy.uploaded_packet_count == 0U, "second frame has no fresh uploaded packets");
    require(second_resources.policy.clean_reuse_packet_count == 2U, "second frame has two clean-reuse packets");
    require(second_resources.policy.used_real_backend, "second frame resource packets preserve real backend evidence");

    const render_text_renderer_glyph_quad_packet_snapshot second_quads =
        make_render_text_renderer_glyph_quad_packets(render_text_renderer_glyph_quad_packet_request{
            .resource_packets = second_resources,
        });
    require(second_quads.ok(), "second real frame produces renderer-ready clean-reuse glyph quads");
    require(second_quads.policy.quad_packet_count == 2U, "second frame produces one clean-reuse quad per glyph");
    require(second_quads.policy.ready_quad_count == 2U, "second frame clean-reuse quads are ready");
    require(second_quads.policy.uploaded_quad_count == 0U, "second frame quads do not carry fresh uploads");
    require(second_quads.policy.clean_reuse_quad_count == 2U, "second frame quads carry clean-reuse evidence");
    require(second_quads.policy.total_upload_rgba_bytes == 0U, "second frame clean-reuse quads add no upload bytes");
    require(second_quads.policy.used_real_backend, "second frame quads preserve real backend evidence");
    require(!second_quads.policy.used_deterministic_fallback, "second frame quads avoid fallback evidence");
    for (const render_text_renderer_glyph_quad_packet_record& quad : second_quads.packets) {
        require(quad.drawable(), "second-frame clean-reuse glyph quad is drawable");
        require(!quad.uploaded, "second-frame clean-reuse glyph quad has no fresh upload");
        require(quad.clean_reuse, "second-frame glyph quad records clean atlas reuse");
        require(quad.used_real_backend, "second-frame clean-reuse quad preserves real backend flag");
        require(!quad.used_deterministic_fallback, "second-frame clean-reuse quad avoids deterministic fallback");
        require(!quad.upload_consumed, "second-frame clean-reuse quad does not consume a fresh upload");
        require(quad.upload_rgba_bytes == 0U, "second-frame clean-reuse quad has no upload byte payload");
        require(quad.uv_bounds.valid, "second-frame clean-reuse quad keeps valid UVs");
        require(contains_atlas_key(first_keys, quad.cache_key), "second-frame clean-reuse quad preserves atlas key");
        require(
            quad.atlas_consumption.clean_reuse,
            "second-frame clean-reuse quad atlas consumption records reuse");
        require(
            quad.atlas_consumption.cache_key == quad.cache_key,
            "second-frame clean-reuse quad atlas consumption preserves cache key");
    }
}

void test_multiline_korean_text_reaches_frame_handoff_with_partial_upload_blocker()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path korean_font_path = korean_multiline_font_fixture();
    const std::filesystem::path latin_font_path = first_available_freetype_real_font_fixture();
    if (korean_font_path.empty() || latin_font_path.empty()
        || !render_text_freetype_memory_face_adapter_available()) {
        return;
    }

    fake_text_engine engine;
    select_real_text_stack(engine);
    engine.add_font_face(font_face_descriptor{
        .id = 812,
        .family = "Payload Korean Varc",
        .source_uri = korean_font_path.generic_string(),
        .version = "external-fixture",
        .license = "harfbuzz-test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'\uAC00', .last = U'\uAC01'},
        },
        .weight = 400,
    });
    engine.add_font_face(font_face_descriptor{
        .id = 813,
        .family = "Payload Latin Sans",
        .source_uri = latin_font_path.generic_string(),
        .version = "external-fixture",
        .license = "harfbuzz-test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'A'},
        },
        .weight = 400,
    });

    const render_text_layout layout = engine.layout_text(multiline_korean_request());
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 3U, "multiline Korean request lays out three visible glyphs");
    require(
        diagnostics.shaping_line_run_evidence_policy.line_count >= 2U,
        "multiline Korean diagnostics preserve two layout lines");
    require(
        diagnostics.shaping_line_run_evidence_policy.cluster_count >= 3U,
        "multiline Korean diagnostics preserve per-glyph cluster evidence");
    const bool has_korean_utf8_cluster = std::any_of(
        diagnostics.shaping_line_run_evidence.begin(),
        diagnostics.shaping_line_run_evidence.end(),
        [](const fake_text_engine_shaping_line_run_evidence_snapshot& cluster) {
            return cluster.cluster_byte_count >= 3U
                && cluster.cluster_codepoint_count == 1U
                && cluster.resolved_face_id == 812U;
        });
    require(has_korean_utf8_cluster, "multiline diagnostics preserve Korean UTF-8 cluster bytes");
    require(
        diagnostics.shaping_line_run_evidence_policy.harfbuzz_cluster_count
            + diagnostics.shaping_line_run_evidence_policy.deterministic_fallback_cluster_count
            >= 3U,
        "multiline diagnostics preserve HarfBuzz-or-fallback shaping evidence");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.freetype_rasterizer_count > 0U,
        "multiline path records a real FreeType raster payload");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.deterministic_rasterizer_count > 0U,
        "multiline path records deterministic fallback raster payloads");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.materialized_font_bytes_count == 3U,
        "multiline path materializes font bytes for all visible glyphs");
    require(
        diagnostics.glyph_atlas_materializations.size() == 3U,
        "multiline path records three atlas materializations");
    require(
        diagnostics.line_run_atlas_upload_policy.line_count >= 2U,
        "line/run atlas upload diagnostics preserve two lines");
    require(
        diagnostics.line_run_atlas_upload_policy.upload_ready_cluster_count == 3U,
        "line/run atlas upload diagnostics mark all glyphs upload-ready before consumption");

    const std::vector<std::string> all_upload_request_ids =
        upload_request_ids_for_diagnostics(diagnostics);
    require(all_upload_request_ids.size() == 3U, "multiline path exposes one upload request per glyph");

    fake_text_engine_diagnostics ready_diagnostics = diagnostics;
    ready_diagnostics.text_frame_snapshot =
        render_text_frame_snapshot_with_consumed_atlas_updates(
            diagnostics.text_frame_snapshot,
            all_upload_request_ids,
            all_upload_request_ids.size());
    ready_diagnostics.text_frame_draw_plan =
        plan_render_text_frame_draw_packets(render_text_frame_draw_plan_request{
            .frame = ready_diagnostics.text_frame_snapshot,
            .materializations = ready_diagnostics.glyph_atlas_materializations,
            .item_index = 0U,
        });
    require(
        ready_diagnostics.text_frame_snapshot.ready_for_renderer(),
        "multiline text frame becomes renderer-ready after upload consumption");
    require(
        ready_diagnostics.text_frame_draw_plan.policy.packet_count == 3U,
        "multiline draw plan records three glyph packets");
    require(
        ready_diagnostics.text_frame_draw_plan.policy.draw_ready_count == 3U,
        "multiline draw plan marks all glyph packets ready after consumption");
    require(
        ready_diagnostics.text_frame_draw_plan.policy.upload_consumed_count == 3U,
        "multiline draw plan records upload consumption for all glyph packets");

    const render_text_renderer_glyph_quad_packet_snapshot full_quads =
        glyph_quads_for_diagnostics(ready_diagnostics);
    require(full_quads.ok(), "multiline glyph quad packet snapshot is renderer-ready");
    require(full_quads.policy.quad_packet_count == 3U, "multiline glyph quads preserve each glyph packet");
    require(full_quads.policy.ready_quad_count == 3U, "multiline glyph quads are ready");
    require(full_quads.policy.real_backend_count == 3U, "multiline glyph quads preserve real backend route");
    const bool has_second_line_quad = std::any_of(
        full_quads.packets.begin(),
        full_quads.packets.end(),
        [&](const render_text_renderer_glyph_quad_packet_record& packet) {
            return packet.atlas_consumption.line_index
                != full_quads.packets.front().atlas_consumption.line_index;
        });
    require(has_second_line_quad, "multiline glyph quads preserve line index boundaries");

    const render_text_render_frame_handoff_summary_snapshot full_summary =
        summary_for_diagnostics(ready_diagnostics);
    require(full_summary.ok(), "multiline render frame handoff summary is renderer-ready");
    require(full_summary.policy.draw_packet_count == 3U, "multiline summary counts all draw packets");
    require(full_summary.policy.glyph_quad_ready_count == 3U, "multiline summary counts ready quads");
    require(full_summary.policy.upload_request_count == 3U, "multiline summary exposes upload requests");
    require(full_summary.policy.real_backend_count == 6U, "multiline summary preserves draw and quad real backend evidence");

    const std::size_t first_line_index =
        ready_diagnostics.text_frame_draw_plan.packets.front().atlas_consumption.line_index;
    std::vector<std::string> first_line_upload_request_ids;
    std::size_t first_line_packet_count = 0U;
    std::size_t later_line_packet_count = 0U;
    for (const render_text_frame_draw_packet_snapshot& packet :
         ready_diagnostics.text_frame_draw_plan.packets) {
        if (packet.atlas_consumption.line_index == first_line_index) {
            ++first_line_packet_count;
            first_line_upload_request_ids.push_back(packet.atlas_upload_request_id);
        } else {
            ++later_line_packet_count;
        }
    }
    require(first_line_packet_count > 0U, "partial upload test finds a first line");
    require(later_line_packet_count > 0U, "partial upload test finds a later line");

    const render_text_glyph_atlas_upload_result_snapshot partial_upload_result =
        upload_result_for_diagnostics(ready_diagnostics, first_line_upload_request_ids);
    const render_text_frame_upload_handoff_snapshot partial_handoff =
        make_render_text_frame_upload_handoff(render_text_frame_upload_handoff_request{
            .frame = ready_diagnostics.text_frame_snapshot,
            .draw_plan = ready_diagnostics.text_frame_draw_plan,
            .upload_result = partial_upload_result,
        });
    require(
        partial_handoff.policy.ready_glyph_packet_count == first_line_packet_count,
        "partial upload handoff keeps first line ready");
    require(
        partial_handoff.policy.upload_result_rejected_count == later_line_packet_count,
        "partial upload handoff rejects the line without upload consumption");

    const render_text_frame_resource_packet_materialization partial_resources =
        materialize_render_text_frame_resource_packets(
            render_text_frame_resource_packet_materialization_request{
                .draw_plan = ready_diagnostics.text_frame_draw_plan,
                .upload_handoff = partial_handoff,
            });
    require(!partial_resources.ok(), "partial resource materialization exposes blockers");
    require(
        partial_resources.policy.ready_packet_count == first_line_packet_count,
        "partial resource materialization keeps first line resource packets ready");
    require(
        partial_resources.policy.rejected_upload_count == later_line_packet_count,
        "partial resource materialization blocks later line upload packets");

    const render_text_renderer_glyph_quad_packet_snapshot partial_quads =
        make_render_text_renderer_glyph_quad_packets(
            render_text_renderer_glyph_quad_packet_request{
                .resource_packets = partial_resources,
            });
    require(!partial_quads.ok(), "partial glyph quad packet snapshot exposes blockers");
    require(
        partial_quads.policy.ready_quad_count == first_line_packet_count,
        "partial glyph quad packets keep first line ready");
    require(
        partial_quads.policy.blocked_quad_count == later_line_packet_count,
        "partial glyph quad packets block the line without upload handoff");

    const render_text_render_frame_handoff_summary_snapshot partial_summary =
        make_render_text_render_frame_handoff_summary(
            render_text_render_frame_handoff_summary_request{
                .frame = ready_diagnostics.text_frame_snapshot,
                .draw_plan = ready_diagnostics.text_frame_draw_plan,
                .glyph_quads = partial_quads,
            });
    require(!partial_summary.ok(), "partial render frame handoff summary exposes blockers");
    require(
        partial_summary.policy.glyph_quad_ready_count == first_line_packet_count,
        "partial summary keeps first line ready");
    require(
        partial_summary.policy.glyph_quad_blocked_count == later_line_packet_count,
        "partial summary blocks later line glyph quads");
    require(
        partial_summary.policy.non_upload_ready_atlas_payload_blocker_count == later_line_packet_count,
        "partial summary counts missing upload consumption blockers");
}

void test_repeated_hangul_text_reuses_atlas_residency_and_uploads_new_glyph()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path korean_font_path = korean_multiline_font_fixture();
    if (korean_font_path.empty() || !render_text_freetype_memory_face_adapter_available()) {
        return;
    }

    fake_text_engine engine;
    select_real_text_stack(engine);
    engine.add_font_face(font_face_descriptor{
        .id = 814,
        .family = "Payload Korean Varc",
        .source_uri = korean_font_path.generic_string(),
        .version = "external-fixture",
        .license = "harfbuzz-test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'\uAC00', .last = U'\uAC01'},
        },
        .weight = 400,
    });

    const render_text_layout first_layout = engine.layout_text(hangul_residency_request("가"));
    const fake_text_engine_diagnostics first = engine.last_diagnostics();
    require(first_layout.glyphs.size() == 1U, "first Hangul frame lays out one glyph");
    require(first.has_font_source_resolutions(), "first Hangul frame records font source identity");
    require(first.has_font_source_bytes(), "first Hangul frame records font source byte readiness");
    require(
        std::any_of(
            first.font_source_resolutions.begin(),
            first.font_source_resolutions.end(),
            [&](const render_text_font_source_resolution_snapshot& source) {
                return source.resolved_face_id == 814U
                    && source.resolved_location == korean_font_path.generic_string();
            }),
        "first Hangul frame preserves file-backed font source location");
    require(
        first.rasterized_glyph_atlas_payload_policy.materialized_font_bytes_count == 1U,
        "first Hangul frame materializes font bytes");
    require(
        first.rasterized_glyph_atlas_payloads.size() == 1U,
        "first Hangul frame records one raster payload decision");
    const render_text_rasterized_glyph_atlas_payload_snapshot& first_hangul_payload =
        first.rasterized_glyph_atlas_payloads.front();
    if (first_hangul_payload.used_freetype_rasterizer) {
        require(
            first.rasterized_glyph_atlas_payload_policy.freetype_rasterizer_count == 1U,
            "first Hangul frame records real FreeType raster coverage when fixture supports it");
    } else {
        require(
            !first_hangul_payload.deterministic_fallback_reason.empty(),
            "first Hangul frame records a fallback reason when FreeType cannot rasterize the fixture glyph");
        require(
            first_hangul_payload.deterministic_fallback_reason.find("FreeType") != std::string::npos,
            "first Hangul fallback diagnostic names the FreeType coverage/raster blocker");
    }
    require(first.glyph_cache_readiness.size() == 1U, "first Hangul frame records one cache readiness item");
    require(
        first.glyph_cache_readiness.front().codepoint == static_cast<std::uint32_t>(U'\uAC00'),
        "first Hangul frame preserves Hangul codepoint identity");
    require(first.glyph_cache_readiness.front().has_atlas_slot, "first Hangul glyph is resident in an atlas slot");
    require(first.glyph_atlas_placements.size() == 1U, "first Hangul frame records one atlas placement");
    require(first.glyph_atlas_placements.front().newly_allocated, "first Hangul glyph allocates a new slot");
    require(!first.glyph_atlas_placements.front().cache_hit, "first Hangul glyph is not an atlas cache hit");
    require(first.glyph_atlas_metrics.new_slot_count == 1U, "first Hangul frame counts one new atlas slot");
    require(first.glyph_atlas_metrics.cache_hit_count == 0U, "first Hangul frame records no atlas cache hit");
    require(
        first.shaping_line_run_evidence_policy.cluster_count == 1U,
        "first Hangul frame records one shaped cluster");
    require(
        first.shaping_line_run_evidence.front().cluster_byte_count == 3U,
        "first Hangul cluster preserves UTF-8 byte width");
    require(
        first.glyph_atlas_materialization_policy.upload_ready_count == 1U,
        "first Hangul glyph queues an atlas upload");
    require(
        first.line_run_atlas_upload_policy.upload_ready_cluster_count == 1U,
        "first Hangul line/run evidence records upload-ready atlas work");
    require(
        first.line_run_atlas_upload_policy.clean_reuse_cluster_count == 0U,
        "first Hangul line/run evidence records no clean reuse");
    require(
        first.atlas_upload_request_bridge.policy.unique_upload_request_count == 1U,
        "first Hangul frame exposes one unique upload request");

    const glyph_atlas_key first_key = first.glyph_atlas_placements.front().key;
    const render_text_atlas_page_id first_page_id = first.glyph_atlas_placements.front().page.id;
    const render_rect first_atlas_bounds = first.glyph_atlas_placements.front().atlas_bounds;

    const render_text_layout second_layout = engine.layout_text(hangul_residency_request("가"));
    const fake_text_engine_diagnostics second = engine.last_diagnostics();
    require(second_layout.glyphs.size() == 1U, "second Hangul frame lays out the repeated glyph");
    require(second.glyph_cache_readiness.size() == 1U, "second Hangul frame records one cache readiness item");
    require(
        second.glyph_cache_readiness.front().has_atlas_slot,
        "second Hangul glyph remains resident in an atlas slot");
    require(second.glyph_atlas_placements.size() == 1U, "second Hangul frame records one atlas placement");
    require(second.glyph_atlas_placements.front().cache_hit, "second Hangul glyph reuses atlas residency");
    require(
        !second.glyph_atlas_placements.front().newly_allocated,
        "second Hangul glyph does not allocate a new slot");
    require(second.glyph_atlas_placements.front().key == first_key, "second Hangul glyph preserves atlas key");
    require(second.glyph_atlas_placements.front().page.id == first_page_id, "second Hangul glyph preserves page id");
    require(
        second.glyph_atlas_placements.front().atlas_bounds.x == first_atlas_bounds.x
            && second.glyph_atlas_placements.front().atlas_bounds.y == first_atlas_bounds.y
            && second.glyph_atlas_placements.front().atlas_bounds.width == first_atlas_bounds.width
            && second.glyph_atlas_placements.front().atlas_bounds.height == first_atlas_bounds.height,
        "second Hangul glyph preserves atlas slot bounds");
    require(second.glyph_atlas_metrics.cache_hit_count == 1U, "second Hangul frame counts one atlas cache hit");
    require(second.glyph_atlas_metrics.new_slot_count == 0U, "second Hangul frame allocates no new atlas slots");
    require(second.glyph_atlas_metrics.dirty_page_count == 0U, "second Hangul frame marks no dirty atlas pages");
    require(
        second.glyph_atlas_page_policy.repeated_layout_clean_page_count > 0U,
        "second Hangul frame records clean atlas page reuse");
    require(
        second.glyph_atlas_materialization_policy.clean_reuse_count == 1U,
        "second Hangul materialization records clean reuse");
    require(
        second.glyph_atlas_materializations.front().status
            == render_text_glyph_atlas_materialization_status::materialized_clean_reuse,
        "second Hangul materialization reports clean atlas reuse");
    require(
        second.line_run_atlas_upload_policy.clean_reuse_cluster_count == 1U,
        "second Hangul line/run evidence records clean reuse");
    require(
        second.line_run_atlas_upload_policy.reused_cluster_count == 1U,
        "second Hangul line/run evidence records atlas data reuse");
    require(
        second.line_run_atlas_upload_policy.upload_ready_cluster_count == 0U,
        "second Hangul frame queues no new upload request");
    require(
        second.atlas_upload_request_bridge.policy.clean_reuse_count == 1U,
        "second Hangul upload bridge records clean reuse");
    require(
        second.atlas_upload_request_bridge.policy.unique_upload_request_count == 0U,
        "second Hangul upload bridge emits no unique upload request");
    require(
        second.shaped_atlas_update_trace_policy.clean_atlas_page_reused_count == 1U,
        "second Hangul shaped-atlas trace records clean page reuse");

    const render_text_layout third_layout = engine.layout_text(hangul_residency_request("가각"));
    const fake_text_engine_diagnostics third = engine.last_diagnostics();
    require(third_layout.glyphs.size() == 2U, "third Hangul frame lays out repeated and new glyphs");
    require(third.glyph_cache_readiness.size() == 2U, "third Hangul frame records two cache readiness items");
    require(
        std::all_of(
            third.glyph_cache_readiness.begin(),
            third.glyph_cache_readiness.end(),
            [](const render_text_glyph_cache_readiness_snapshot& readiness) {
                return readiness.has_atlas_slot;
            }),
        "third Hangul frame keeps both shaped glyphs resident in atlas slots");
    require(
        std::any_of(
            third.glyph_cache_readiness.begin(),
            third.glyph_cache_readiness.end(),
            [](const render_text_glyph_cache_readiness_snapshot& readiness) {
                return readiness.codepoint == static_cast<std::uint32_t>(U'\uAC01');
            }),
        "third Hangul frame records the new Hangul codepoint");
    require(third.glyph_atlas_placements.size() == 2U, "third Hangul frame records two atlas placements");
    const std::size_t third_cache_hits = static_cast<std::size_t>(std::count_if(
        third.glyph_atlas_placements.begin(),
        third.glyph_atlas_placements.end(),
        [](const render_text_glyph_atlas_placement_snapshot& placement) {
            return placement.cache_hit;
        }));
    const std::size_t third_new_slots = static_cast<std::size_t>(std::count_if(
        third.glyph_atlas_placements.begin(),
        third.glyph_atlas_placements.end(),
        [](const render_text_glyph_atlas_placement_snapshot& placement) {
            return placement.newly_allocated;
        }));
    require(third_cache_hits == 1U, "third Hangul frame reuses one existing atlas slot");
    require(third_new_slots == 1U, "third Hangul frame allocates one new atlas slot");
    require(third.glyph_atlas_metrics.cache_hit_count == 1U, "third Hangul metrics count one cache hit");
    require(third.glyph_atlas_metrics.new_slot_count == 1U, "third Hangul metrics count one new slot");
    require(
        third.glyph_atlas_materialization_policy.clean_reuse_count == 1U,
        "third Hangul materialization records one reused glyph");
    require(
        third.glyph_atlas_materialization_policy.upload_ready_count == 1U,
        "third Hangul materialization records one newly upload-ready glyph");
    require(
        third.line_run_atlas_upload_policy.clean_reuse_cluster_count == 1U,
        "third Hangul line/run evidence records one clean reuse");
    require(
        third.line_run_atlas_upload_policy.upload_ready_cluster_count == 1U,
        "third Hangul line/run evidence records one new upload");
    require(
        third.atlas_upload_request_bridge.policy.clean_reuse_count == 1U,
        "third Hangul upload bridge records reused atlas residency");
    require(
        third.atlas_upload_request_bridge.policy.unique_upload_request_count == 1U,
        "third Hangul upload bridge emits one new upload request");
    require(
        third.shaped_atlas_update_trace_policy.shaped_glyph_missing_atlas_slot_count == 0U,
        "third Hangul shaped glyphs all have resident atlas slots");
    require(
        third.shaping_atlas_handoff_policy.missing_atlas_slot_cluster_count == 0U,
        "third Hangul shaping atlas handoff records no missing atlas slot blockers");
}

void test_missing_file_backed_freetype_payload_falls_back_and_skips()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    select_real_text_stack(engine);
    engine.add_font_face(font_face_descriptor{
        .id = 811,
        .family = "Missing Payload Sans",
        .source_uri = "fonts/missing-freetype-payload.ttf",
        .version = "missing-fixture",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'A'},
        },
        .weight = 400,
    });

    const render_text_layout layout = engine.layout_text(single_glyph_request(
        style_catalog_for("Missing Payload Sans", "missing-payload"),
        "missing-payload"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 1U, "missing file-backed font still lays out one glyph");
    require(
        diagnostics.rasterized_glyph_atlas_payloads.size() == 1U,
        "missing file-backed font records one raster payload decision");

    const render_text_rasterized_glyph_atlas_payload_snapshot& payload =
        diagnostics.rasterized_glyph_atlas_payloads.front();
    require(payload.font_backend_library == render_text_font_backend_library::freetype, "payload records selected FreeType");
    require(!payload.used_freetype_rasterizer, "missing bytes do not call the FreeType rasterizer");
    require(payload.uses_deterministic_rasterizer, "missing bytes stay on deterministic fallback");
    require(!payload.materialized_font_bytes, "missing bytes record no materialized font bytes");
    require(
        payload.source_bytes_status == render_text_font_source_bytes_load_status::missing_bytes,
        "payload records missing source bytes");
    require(
        payload.status == render_text_font_rasterizer_status::missing_font_bytes,
        "payload preserves missing-byte raster status");
    require(!payload.upload_ready, "missing-byte payload is not upload-ready");
    require(payload.skipped, "missing-byte payload is skipped");
    require(
        payload.deterministic_fallback_reason.find("materialized font bytes") != std::string::npos,
        "fallback reason names missing materialized bytes");

    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.deterministic_rasterizer_count == 1U,
        "missing-byte policy counts deterministic fallback");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.freetype_rasterizer_count == 0U,
        "missing-byte policy records no FreeType rasterizer use");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.materialized_font_bytes_count == 0U,
        "missing-byte policy records no materialized bytes");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.deterministic_fallback_reason_count == 1U,
        "missing-byte policy counts fallback reason");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.skipped_count == 1U,
        "missing-byte policy counts skipped payload");

    require(
        diagnostics.glyph_atlas_materializations.size() == 1U,
        "missing-byte font records one materialization decision");
    require(
        diagnostics.glyph_atlas_materializations.front().status
            == render_text_glyph_atlas_materialization_status::skipped_raster_payload,
        "missing-byte materialization skips cleanly on raster payload");

    require(diagnostics.has_line_run_atlas_uploads(), "missing bytes record line/run upload blocker");
    const fake_text_engine_line_run_atlas_upload_snapshot& line_upload =
        diagnostics.line_run_atlas_uploads.front();
    require(line_upload.blocked, "missing bytes line/run upload is blocked");
    require(!line_upload.upload_ready, "missing bytes line/run upload is not upload-ready");
    require(
        line_upload.blocker_reason.find("font bytes") != std::string::npos,
        "missing bytes line/run upload blocker names font bytes");

    require(diagnostics.has_text_frame_draw_plan(), "missing bytes record draw packet evidence");
    require(
        diagnostics.text_frame_draw_plan.policy.packet_count == 1U,
        "missing bytes draw plan records one packet");
    require(
        diagnostics.text_frame_draw_plan.policy.skipped_count == 1U,
        "missing bytes draw plan records skipped packet");
    const render_text_frame_draw_packet_snapshot& draw_packet =
        diagnostics.text_frame_draw_plan.packets.front();
    require(!draw_packet.drawable(), "missing bytes draw packet is not drawable");
    require(
        draw_packet.status != render_text_frame_draw_packet_status::draw_ready,
        "missing bytes draw packet records blocked renderer evidence");
    require(
        !draw_packet.diagnostic.empty(),
        "missing bytes draw packet preserves blocker diagnostic");

    const render_text_render_frame_handoff_summary_snapshot summary =
        summary_for_diagnostics(diagnostics);
    require(!summary.ok(), "missing bytes render frame handoff summary is blocked");
    require(summary.has_blockers(), "missing bytes summary exposes blockers");
    require(summary.policy.draw_packet_count == 1U, "missing bytes summary counts draw packet");
    require(summary.policy.draw_blocked_count == 1U, "missing bytes summary counts blocked draw packet");
    require(
        summary.policy.non_upload_ready_atlas_payload_blocker_count == 1U,
        "missing bytes summary counts non-upload-ready atlas payload");
    require(
        summary.policy.missing_materialization_blocker_count == 1U,
        "missing bytes summary counts missing materialization");
    require(
        summary.policy.skipped_fallback_packet_count == 1U,
        "missing bytes summary preserves deterministic fallback route");
    require(summary.policy.used_deterministic_fallback, "missing bytes summary marks deterministic fallback");
    require(!summary.policy.used_real_backend, "missing bytes summary does not mark real backend");
    require(!summary.blocker_packet_ids.empty(), "missing bytes summary exposes blocker packet id");

    const render_text_frame_resource_packet_materialization resources =
        resource_packets_for_diagnostics(diagnostics);
    require(!resources.ok(), "missing bytes resource packet materialization is blocked");
    require(resources.policy.blocked_packet_count == 1U, "missing bytes resource packet is counted as blocked");
    require(resources.has_blockers(), "missing bytes resource packet materialization exposes blockers");
    require(!resources.blocker_summary.empty(), "missing bytes resource packet preserves blocker summary");

    const render_text_renderer_glyph_quad_packet_snapshot quads =
        make_render_text_renderer_glyph_quad_packets(render_text_renderer_glyph_quad_packet_request{
            .resource_packets = resources,
        });
    require(!quads.ok(), "missing bytes glyph quad packet snapshot is blocked");
    require(quads.has_blockers(), "missing bytes glyph quad packet snapshot exposes blockers");
    require(quads.policy.resource_blocked_count == 1U, "missing bytes glyph quad counts blocked resource packet");
    require(quads.policy.blocked_quad_count == 1U, "missing bytes glyph quad counts blocked quad");
    require(
        quads.packets.front().status != render_text_renderer_glyph_quad_packet_status::quad_ready,
        "missing bytes glyph quad preserves blocked status");
    require(!quads.packets.front().drawable(), "missing bytes glyph quad is not drawable");
    require(
        !quads.packets.front().blocker_summary.empty(),
        "missing bytes glyph quad preserves blocker summary");
}

} // namespace

int main()
{
    test_file_backed_freetype_selection_rasterizes_real_payload();
    test_repeated_file_backed_freetype_text_reuses_resident_atlas_payload();
    test_harfbuzz_shaped_glyph_ids_drive_freetype_atlas_residency();
    test_multiline_korean_text_reaches_frame_handoff_with_partial_upload_blocker();
    test_repeated_hangul_text_reuses_atlas_residency_and_uploads_new_glyph();
    test_missing_file_backed_freetype_payload_falls_back_and_skips();
    return 0;
}
