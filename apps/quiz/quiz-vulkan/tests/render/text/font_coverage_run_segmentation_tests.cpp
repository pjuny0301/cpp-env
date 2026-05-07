#include "render/text/font_coverage_run_segmentation.h"
#include "render/text/font_unicode_coverage.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
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

quiz_vulkan::render::render_text_font_unicode_coverage_snapshot make_coverage(
    std::vector<quiz_vulkan::render::render_text_font_cmap_range> ranges)
{
    return quiz_vulkan::render::render_text_font_unicode_coverage_snapshot{
        .source_label = "test-coverage",
        .status = quiz_vulkan::render::render_text_font_unicode_coverage_status::valid,
        .ranges = std::move(ranges),
        .diagnostic = "valid test coverage",
    };
}

quiz_vulkan::render::render_text_style primary_style()
{
    return quiz_vulkan::render::render_text_style{
        .id = "primary",
        .font_family = "Primary Sans",
        .font_weight = 400,
    };
}

quiz_vulkan::render::font_face_catalog make_adapted_catalog()
{
    using namespace quiz_vulkan::render;

    const font_unicode_coverage_catalog_adapter adapter;
    font_face_catalog catalog;
    catalog.add_face(adapter.apply_to_descriptor(
        font_face_descriptor{
            .id = 101,
            .family = "Primary Sans",
            .source_uri = "fixture://fonts/primary-latin",
            .version = "fixture-1",
            .license = "test-fixture",
            .weight = 400,
        },
        make_coverage({
            render_text_font_cmap_range{
                .first_codepoint = U'A',
                .last_codepoint = U'Z',
            },
        })));
    catalog.add_face(adapter.apply_to_descriptor(
        font_face_descriptor{
            .id = 102,
            .family = "Fallback Sans",
            .source_uri = "fixture://fonts/fallback-hangul-emoji",
            .version = "fixture-1",
            .license = "test-fixture",
            .weight = 400,
            .fallback = true,
        },
        make_coverage({
            render_text_font_cmap_range{
                .first_codepoint = static_cast<char32_t>(0xac00U),
                .last_codepoint = static_cast<char32_t>(0xac02U),
            },
            render_text_font_cmap_range{
                .first_codepoint = static_cast<char32_t>(0x1f600U),
                .last_codepoint = static_cast<char32_t>(0x1f64fU),
            },
        })));
    return catalog;
}

void test_latin_stays_on_requested_face_and_merges_contiguous_codepoints()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_adapted_catalog();
    const render_text_font_coverage_run_segmentation segmentation =
        segment_font_coverage_runs("ABC", catalog, primary_style());

    require(segmentation.ok(), "Latin segmentation resolves without errors");
    require(segmentation.codepoint_count == 3U, "Latin segmentation records codepoint count");
    require(segmentation.supported_codepoint_count == 3U, "Latin codepoints are supported");
    require(segmentation.fallback_codepoint_count == 0U, "Latin does not use fallback coverage");
    require(segmentation.segments.size() == 1U, "contiguous Latin requested-face codepoints merge into one run");

    const render_text_font_coverage_run_segment& segment = segmentation.segments.front();
    require(segment.ok(), "Latin segment reports ok");
    require(segment.byte_offset == 0U && segment.byte_count == 3U, "Latin segment covers all bytes");
    require(segment.codepoint_offset == 0U && segment.codepoint_count == 3U, "Latin segment covers all codepoints");
    require(segment.requested_face_id == 101U, "Latin segment records requested face");
    require(segment.resolved_face_id == 101U, "Latin segment resolves to requested face");
    require(!segment.used_fallback, "Latin segment does not use fallback");
    require(segment.glyph_supported, "Latin segment claims glyph support");
}

void test_hangul_and_non_bmp_fall_back_to_adapted_coverage_face()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_adapted_catalog();
    const std::string text = std::string("\xEA\xB0\x80", 3) + std::string("\xF0\x9F\x98\x80", 4);
    const render_text_font_coverage_run_segmentation segmentation =
        segment_font_coverage_runs(text, catalog, primary_style());

    require(segmentation.ok(), "fallback segmentation resolves without errors");
    require(segmentation.codepoint_count == 2U, "fallback segmentation records two codepoints");
    require(segmentation.supported_codepoint_count == 2U, "Hangul and non-BMP codepoints are supported");
    require(segmentation.fallback_codepoint_count == 2U, "Hangul and non-BMP use fallback coverage");
    require(segmentation.segments.size() == 1U, "same fallback face codepoints merge into one run");

    const render_text_font_coverage_run_segment& segment = segmentation.segments.front();
    require(segment.byte_offset == 0U && segment.byte_count == 7U, "fallback segment covers Hangul and emoji bytes");
    require(segment.codepoint_offset == 0U && segment.codepoint_count == 2U, "fallback segment covers both codepoints");
    require(segment.requested_face_id == 101U, "fallback segment records requested face");
    require(segment.resolved_face_id == 102U, "fallback segment resolves to adapted fallback face");
    require(segment.resolved_family == "Fallback Sans", "fallback segment records resolved family");
    require(segment.used_fallback, "fallback segment reports fallback");
    require(segment.glyph_supported, "fallback segment claims glyph support");
}

void test_invalid_utf8_produces_unsupported_segment_diagnostic()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_adapted_catalog();
    const std::string text(1U, static_cast<char>(0xc3U));
    const font_coverage_run_segmenter segmenter;
    const render_text_font_coverage_run_segmentation segmentation = segmenter.segment(
        render_text_font_coverage_run_segmentation_request{
            .text = text,
            .style = primary_style(),
        },
        catalog);

    require(!segmentation.ok(), "invalid UTF-8 segmentation reports not ok");
    require(segmentation.invalid_utf8_count == 1U, "invalid UTF-8 count is recorded");
    require(segmentation.unsupported_codepoint_count == 0U, "invalid UTF-8 is tracked separately from unsupported scalar coverage");
    require(segmentation.segments.size() == 1U, "invalid UTF-8 produces one segment");

    const render_text_font_coverage_run_segment& segment = segmentation.segments.front();
    require(segment.status == render_text_font_coverage_run_segment_status::invalid_utf8, "invalid segment has invalid UTF-8 status");
    require(!segment.ok(), "invalid segment is not ok");
    require(!segment.valid_utf8, "invalid segment records invalid UTF-8");
    require(!segment.glyph_supported, "invalid segment does not claim glyph support");
    require(segment.diagnostic.find("invalid UTF-8") != std::string::npos, "invalid segment includes diagnostic");
}

void test_unsupported_codepoint_produces_unsupported_segment_diagnostic()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_adapted_catalog();
    const render_text_font_coverage_run_segmentation segmentation =
        segment_font_coverage_runs("?", catalog, primary_style());

    require(!segmentation.ok(), "unsupported codepoint segmentation reports not ok");
    require(segmentation.invalid_utf8_count == 0U, "unsupported codepoint does not count as invalid UTF-8");
    require(segmentation.unsupported_codepoint_count == 1U, "unsupported codepoint count is recorded");
    require(segmentation.segments.size() == 1U, "unsupported codepoint produces one segment");

    const render_text_font_coverage_run_segment& segment = segmentation.segments.front();
    require(
        segment.status == render_text_font_coverage_run_segment_status::unsupported_codepoint,
        "unsupported segment has unsupported status");
    require(segment.valid_utf8, "unsupported segment can still be valid UTF-8");
    require(!segment.glyph_supported, "unsupported segment does not claim glyph support");
    require(segment.resolved_face_id == 101U, "unsupported segment falls back to unresolved requested face");
    require(segment.diagnostic.find("U+003F") != std::string::npos, "unsupported segment names codepoint");
}

void test_adjacent_requested_and_fallback_faces_form_separate_merged_runs()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_adapted_catalog();
    const std::string text = std::string("AB") + std::string("\xEA\xB0\x80", 3)
        + std::string("\xF0\x9F\x98\x80", 4);
    const render_text_font_coverage_run_segmentation segmentation =
        segment_font_coverage_runs(text, catalog, primary_style());

    require(segmentation.ok(), "mixed face segmentation resolves without errors");
    require(segmentation.segments.size() == 2U, "mixed requested/fallback text produces two merged runs");
    require(segmentation.segments[0].resolved_face_id == 101U, "first run resolves to requested face");
    require(segmentation.segments[0].byte_offset == 0U && segmentation.segments[0].byte_count == 2U, "first run merges Latin bytes");
    require(segmentation.segments[0].codepoint_count == 2U, "first run merges Latin codepoints");
    require(segmentation.segments[1].resolved_face_id == 102U, "second run resolves to fallback face");
    require(segmentation.segments[1].byte_offset == 2U && segmentation.segments[1].byte_count == 7U, "second run merges fallback bytes");
    require(segmentation.segments[1].codepoint_count == 2U, "second run merges fallback codepoints");
}

} // namespace

int main()
{
    test_latin_stays_on_requested_face_and_merges_contiguous_codepoints();
    test_hangul_and_non_bmp_fall_back_to_adapted_coverage_face();
    test_invalid_utf8_produces_unsupported_segment_diagnostic();
    test_unsupported_codepoint_produces_unsupported_segment_diagnostic();
    test_adjacent_requested_and_fallback_faces_form_separate_merged_runs();
    return 0;
}
