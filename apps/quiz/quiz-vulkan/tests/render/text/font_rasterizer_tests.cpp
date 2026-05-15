#include "render/text/font_backend_adapter.h"
#include "render/text/font_rasterizer.h"

#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

std::vector<std::byte> fake_font_bytes()
{
    return {
        std::byte{0x00},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
    };
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

quiz_vulkan::render::font_face_descriptor latin_face()
{
    return quiz_vulkan::render::font_face_descriptor{
        .id = 31,
        .family = "Raster Sans",
        .source_uri = "fixture://fonts/raster-sans",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            quiz_vulkan::render::font_codepoint_range{
                .first = 0x0041U,
                .last = 0x005aU,
            },
        },
        .weight = 400,
    };
}

quiz_vulkan::render::font_face_descriptor real_fixture_face(const std::filesystem::path& font_path)
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

void test_fake_rasterizer_returns_stable_metrics_and_bitmap()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_rasterizer rasterizer;
    const font_face_descriptor face = latin_face();
    const std::vector<std::byte> bytes = fake_font_bytes();
    const render_text_font_rasterize_request request =
        make_font_rasterize_request(face, U'A', 16U, std::span<const std::byte>{bytes});
    const render_text_font_rasterize_result result = rasterizer.rasterize(request);

    require(result.ok(), "fake rasterizer reports rasterized status");
    require(result.status == render_text_font_rasterizer_status::rasterized, "rasterized status is stable");
    require(result.face_id == 31U, "raster result records face id");
    require(result.glyph_id == U'A', "raster result records glyph id from atlas key");
    require(result.codepoint == U'A', "raster result records codepoint");
    require(result.pixel_size == 16U, "raster result records pixel size");
    require(result.source_label == "fixture://fonts/raster-sans", "raster result records source label");
    require(result.metrics.advance_x == 10.0f, "fake metrics advance is deterministic");
    require(result.metrics.bearing_x == 2.0f, "fake metrics horizontal bearing is deterministic");
    require(result.metrics.bearing_y == 12.0f, "fake metrics vertical bearing is deterministic");
    require(result.metrics.bitmap_width == 8U, "fake metrics bitmap width is capped");
    require(result.metrics.bitmap_height == 8U, "fake metrics bitmap height is capped");
    require(result.bitmap.width == 8U && result.bitmap.height == 8U, "fake bitmap dimensions are stable");
    require(result.bitmap.row_stride == 8U, "fake bitmap row stride is stable");
    require(result.bitmap.alpha.size() == 64U, "fake bitmap alpha payload is stable");
    require(result.bitmap.alpha.front() == 146U, "fake bitmap first alpha byte is deterministic");
    require(result.bitmap.alpha.back() == 227U, "fake bitmap last alpha byte is deterministic");

    const render_text_font_rasterize_result repeated = rasterizer.rasterize(request);
    require(repeated.bitmap.alpha == result.bitmap.alpha, "fake bitmap is stable across calls");
}

void test_fake_rasterizer_reports_missing_source_and_bytes()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_rasterizer rasterizer;
    const std::vector<std::byte> bytes = fake_font_bytes();
    font_face_descriptor missing_source = latin_face();
    missing_source.source_uri.clear();
    const render_text_font_rasterize_result no_source = rasterizer.rasterize(
        make_font_rasterize_request(
            missing_source,
            U'A',
            16U,
            std::span<const std::byte>{bytes}));
    require(!no_source.ok(), "missing source does not rasterize");
    require(
        no_source.status == render_text_font_rasterizer_status::missing_font_source,
        "missing source reports source status");
    require(no_source.bitmap.alpha.empty(), "missing source returns no bitmap");
    require(no_source.diagnostic.find("source URI") != std::string::npos, "missing source diagnostic names source URI");

    const render_text_font_rasterize_result no_bytes = rasterizer.rasterize(
        make_font_rasterize_request(
            latin_face(),
            U'A',
            16U,
            std::span<const std::byte>{}));
    require(!no_bytes.ok(), "missing bytes do not rasterize");
    require(
        no_bytes.status == render_text_font_rasterizer_status::missing_font_bytes,
        "missing bytes reports byte status");
    require(no_bytes.bitmap.alpha.empty(), "missing bytes returns no bitmap");
    require(no_bytes.diagnostic.find("font bytes") != std::string::npos, "missing bytes diagnostic names bytes");
}

void test_fake_rasterizer_reports_unsupported_glyph()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_rasterizer rasterizer;
    const std::vector<std::byte> bytes = fake_font_bytes();
    const render_text_font_rasterize_result result = rasterizer.rasterize(
        make_font_rasterize_request(
            latin_face(),
            U'?',
            16U,
            std::span<const std::byte>{bytes}));

    require(!result.ok(), "unsupported glyph does not rasterize");
    require(
        result.status == render_text_font_rasterizer_status::unsupported_glyph,
        "unsupported glyph reports unsupported status");
    require(result.glyph_id == U'?', "unsupported glyph preserves glyph id");
    require(result.bitmap.alpha.empty(), "unsupported glyph returns no bitmap");
    require(result.diagnostic.find("coverage") != std::string::npos, "unsupported glyph diagnostic names coverage");

    const render_text_font_atlas_glyph_payload payload = make_font_rasterizer_atlas_payload(result);
    require(!payload.upload_ready, "unsupported glyph does not produce upload-ready atlas payload");
    require(payload.alpha.empty(), "unsupported glyph payload has no alpha bytes");
    require(payload.rgba.empty(), "unsupported glyph payload has no RGBA bytes");
}

void test_fake_rasterizer_reports_invalid_pixel_size()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_rasterizer rasterizer;
    const std::vector<std::byte> bytes = fake_font_bytes();
    const render_text_font_rasterize_result result = rasterizer.rasterize(
        make_font_rasterize_request(
            latin_face(),
            U'A',
            0U,
            std::span<const std::byte>{bytes}));

    require(!result.ok(), "invalid pixel size does not rasterize");
    require(
        result.status == render_text_font_rasterizer_status::invalid_pixel_size,
        "invalid pixel size reports pixel status");
    require(result.pixel_size == 0U, "invalid pixel size preserves requested size");
    require(result.diagnostic.find("pixel size") != std::string::npos, "invalid pixel diagnostic names pixel size");
}

void test_atlas_key_and_payload_helpers_are_stable()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_rasterizer rasterizer;
    const font_face_descriptor face = latin_face();
    const std::vector<std::byte> bytes = fake_font_bytes();
    const glyph_atlas_key key = font_rasterizer_atlas_key_for(face, U'B', 18U);
    require(key.face_id == face.id, "atlas key records face id");
    require(key.glyph_id == U'B', "atlas key records glyph id");
    require(key.pixel_size == 18U, "atlas key records pixel size");

    const render_text_font_rasterize_request request =
        make_font_rasterize_request(face, key, U'B', std::span<const std::byte>{bytes}, "explicit-source");
    require(request.key == key, "request preserves explicit atlas key");
    require(request.pixel_size == key.pixel_size, "request derives pixel size from key");

    const render_text_font_rasterize_result result = rasterizer.rasterize(request);
    const render_text_font_atlas_glyph_payload payload = make_font_rasterizer_atlas_payload(result);
    require(payload.upload_ready, "rasterized payload is upload-ready");
    require(payload.key == key, "payload preserves atlas key");
    require(payload.width == 8U && payload.height == 8U, "payload preserves bitmap dimensions");
    require(payload.alpha == result.bitmap.alpha, "payload preserves alpha bitmap");
    require(payload.rgba.size() == payload.alpha.size() * 4U, "payload expands alpha to RGBA");
    require(payload.rgba[0] == payload.alpha[0], "payload copies grayscale channel from alpha");
    require(payload.rgba[3] == payload.alpha[0], "payload copies alpha channel from alpha");
}

void test_load_result_request_maps_failure_to_rasterizer_status()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_rasterizer rasterizer;
    const render_text_font_source_bytes_load_result missing_load{
        .status = render_text_font_source_bytes_load_status::missing_source,
        .source = font_source_resolution{
            .family = "Raster Sans",
            .source_uri = "fixture://fonts/raster-sans",
        },
        .diagnostic = "missing font source",
    };
    const render_text_font_rasterize_result result = rasterizer.rasterize(
        make_font_rasterize_request(latin_face(), missing_load, U'A', 16U));

    require(!result.ok(), "failed load result does not rasterize");
    require(
        result.status == render_text_font_rasterizer_status::missing_font_source,
        "load result missing source maps to rasterizer missing source");
}

void test_freetype_raster_adapter_returns_metrics_and_grayscale_bitmap_when_fixture_is_available()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path font_path = first_available_freetype_real_font_fixture();
    if (font_path.empty()) {
        return;
    }

    const std::vector<std::byte> font_bytes = read_binary_fixture_bytes(font_path);
    const font_face_descriptor face = real_fixture_face(font_path);
    const glyph_atlas_key key = font_rasterizer_atlas_key_for(face, U'A', 32U);
    const render_text_font_rasterize_request raster_request =
        make_font_rasterize_request(
            face,
            key,
            U'A',
            std::span<const std::byte>{font_bytes},
            font_path.generic_string());
    const function_table_font_backend_adapter adapter(render_text_font_backend_adapter_functions{
        .rasterize = freetype_real_font_backend_rasterize,
        .label = "FreeType memory-face glyph raster adapter",
    });

    const render_text_real_font_raster_adapter_result result = adapter.rasterize(
        render_text_real_font_raster_adapter_request{
            .capability = freetype_raster_capability(),
            .library = render_text_font_backend_library::freetype,
            .rasterize = raster_request,
        });

    if (!render_text_freetype_memory_face_adapter_available()) {
        require(
            result.status == render_text_font_backend_adapter_status::backend_unavailable,
            "FreeType raster adapter reports backend unavailable without compiled FreeType");
        require(result.can_fallback(), "unavailable FreeType raster adapter can fall back");
        return;
    }

    require(result.ok(), "FreeType raster adapter rasterizes a real fixture glyph");
    require(
        result.status == render_text_font_backend_adapter_status::rasterized,
        "FreeType raster adapter reports rasterized adapter status");
    require(result.rasterized.ok(), "FreeType rasterizer result reports rasterized status");
    require(result.rasterized.glyph_id != 0U, "FreeType rasterizer records real glyph index");
    require(result.rasterized.codepoint == U'A', "FreeType rasterizer records requested codepoint");
    require(result.rasterized.pixel_size == 32U, "FreeType rasterizer records requested pixel size");
    require(result.rasterized.metrics.advance_x > 0.0f, "FreeType metrics expose positive horizontal advance");
    require(result.rasterized.metrics.bearing_y > 0.0f, "FreeType metrics expose positive top bearing");
    require(result.rasterized.bitmap.width > 0U, "FreeType rasterizer emits bitmap width");
    require(result.rasterized.bitmap.height > 0U, "FreeType rasterizer emits bitmap height");
    require(
        result.rasterized.metrics.bitmap_width == result.rasterized.bitmap.width,
        "FreeType metrics match bitmap width");
    require(
        result.rasterized.metrics.bitmap_height == result.rasterized.bitmap.height,
        "FreeType metrics match bitmap height");
    require(
        result.rasterized.bitmap.alpha.size()
            == result.rasterized.bitmap.row_stride * result.rasterized.bitmap.height,
        "FreeType rasterizer emits packed grayscale alpha bytes");
    require(
        result.rasterized.has_bitmap(),
        "FreeType rasterizer result has uploadable grayscale bitmap evidence");
    require(
        result.diagnostics.front().actual_glyph_id == result.rasterized.glyph_id,
        "FreeType raster diagnostic records actual glyph index");
}

void test_freetype_raster_adapter_reports_fallback_diagnostics_for_missing_bytes()
{
    using namespace quiz_vulkan::render;

    const font_face_descriptor face = latin_face();
    const render_text_font_rasterize_request raster_request =
        make_font_rasterize_request(
            face,
            U'A',
            18U,
            std::span<const std::byte>{},
            "missing-font-bytes.ttf");
    const render_text_real_font_raster_adapter_result result =
        freetype_real_font_backend_rasterize(render_text_real_font_raster_adapter_request{
            .capability = freetype_raster_capability(),
            .library = render_text_font_backend_library::freetype,
            .rasterize = raster_request,
        });

    require(!result.ok(), "missing bytes do not rasterize through FreeType");
    require(
        result.status == render_text_font_backend_adapter_status::recoverable_backend_failure,
        "missing bytes produce recoverable FreeType adapter failure");
    require(result.can_fallback(), "missing-byte FreeType failure can fall back");
    require(
        result.rasterized.status == render_text_font_rasterizer_status::missing_font_bytes,
        "missing bytes preserve rasterizer missing-byte status");
    require(result.rasterized.bitmap.alpha.empty(), "missing bytes produce no FreeType bitmap");
    require(
        result.diagnostic.find("materialized font bytes") != std::string::npos,
        "missing-byte diagnostic names materialized byte requirement");
}

void test_freetype_raster_adapter_reports_unsupported_codepoint_when_fixture_is_available()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path font_path = first_available_freetype_real_font_fixture();
    if (font_path.empty() || !render_text_freetype_memory_face_adapter_available()) {
        return;
    }

    const std::vector<std::byte> font_bytes = read_binary_fixture_bytes(font_path);
    font_face_descriptor face = real_fixture_face(font_path);
    face.coverage = {
        font_codepoint_range{
            .first = 0x0041U,
            .last = 0x10ffffU,
        },
    };
    const glyph_atlas_key key = font_rasterizer_atlas_key_for(face, 0x10ffffU, 24U);
    const render_text_font_rasterize_request raster_request =
        make_font_rasterize_request(
            face,
            key,
            0x10ffffU,
            std::span<const std::byte>{font_bytes},
            font_path.generic_string());

    const render_text_real_font_raster_adapter_result result =
        freetype_real_font_backend_rasterize(render_text_real_font_raster_adapter_request{
            .capability = freetype_raster_capability(),
            .library = render_text_font_backend_library::freetype,
            .rasterize = raster_request,
        });

    require(!result.ok(), "missing glyph does not rasterize through FreeType");
    require(
        result.status == render_text_font_backend_adapter_status::unsupported_glyph,
        "missing glyph reports unsupported glyph adapter status");
    require(result.can_fallback(), "unsupported glyph can fall back");
    require(
        result.rasterized.status == render_text_font_rasterizer_status::unsupported_glyph,
        "unsupported glyph preserves rasterizer status");
    require(result.rasterized.bitmap.alpha.empty(), "unsupported glyph produces no bitmap");
    require(
        result.diagnostic.find("codepoint") != std::string::npos,
        "unsupported glyph diagnostic names codepoint coverage");
}

} // namespace

int main()
{
    test_fake_rasterizer_returns_stable_metrics_and_bitmap();
    test_fake_rasterizer_reports_missing_source_and_bytes();
    test_fake_rasterizer_reports_unsupported_glyph();
    test_fake_rasterizer_reports_invalid_pixel_size();
    test_atlas_key_and_payload_helpers_are_stable();
    test_load_result_request_maps_failure_to_rasterizer_status();
    test_freetype_raster_adapter_returns_metrics_and_grayscale_bitmap_when_fixture_is_available();
    test_freetype_raster_adapter_reports_fallback_diagnostics_for_missing_bytes();
    test_freetype_raster_adapter_reports_unsupported_codepoint_when_fixture_is_available();
    return 0;
}
