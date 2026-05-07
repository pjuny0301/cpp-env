#include "render/text/font_rasterizer.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
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

} // namespace

int main()
{
    test_fake_rasterizer_returns_stable_metrics_and_bitmap();
    test_fake_rasterizer_reports_missing_source_and_bytes();
    test_fake_rasterizer_reports_unsupported_glyph();
    test_fake_rasterizer_reports_invalid_pixel_size();
    test_atlas_key_and_payload_helpers_are_stable();
    test_load_result_request_maps_failure_to_rasterizer_status();
    return 0;
}
