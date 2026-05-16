#include "render/text/fake_text_engine.h"

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

    require(diagnostics.has_line_run_atlas_uploads(), "file-backed font records line/run upload handoff");
    const fake_text_engine_line_run_atlas_upload_snapshot& line_upload =
        diagnostics.line_run_atlas_uploads.front();
    require(line_upload.has_raster_payload, "line/run upload links raster payload");
    require(line_upload.raster_payload_upload_ready, "line/run upload records upload-ready raster payload");
    require(line_upload.upload_ready, "line/run upload is upload-ready");
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
}

} // namespace

int main()
{
    test_file_backed_freetype_selection_rasterizes_real_payload();
    test_missing_file_backed_freetype_payload_falls_back_and_skips();
    return 0;
}
