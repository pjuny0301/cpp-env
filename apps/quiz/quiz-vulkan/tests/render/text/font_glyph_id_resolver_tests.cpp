#include "render/text/font_glyph_id_resolver.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
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

quiz_vulkan::render::font_face_descriptor face()
{
    return quiz_vulkan::render::font_face_descriptor{
        .id = 41,
        .family = "Glyph Sans",
        .source_uri = "fixture://fonts/glyph-sans",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            quiz_vulkan::render::font_codepoint_range{.first = U'A', .last = U'Z'},
            quiz_vulkan::render::font_codepoint_range{.first = 0xac00U, .last = 0xac02U},
            quiz_vulkan::render::font_codepoint_range{.first = 0x1f600U, .last = 0x1f64fU},
        },
    };
}

quiz_vulkan::render::render_text_font_unicode_coverage_snapshot coverage()
{
    using namespace quiz_vulkan::render;

    render_text_font_unicode_coverage_snapshot snapshot{
        .source_label = "fixture://fonts/glyph-sans",
        .status = render_text_font_unicode_coverage_status::valid,
        .ranges = {
            render_text_font_cmap_range{.first_codepoint = U'A', .last_codepoint = U'Z'},
            render_text_font_cmap_range{
                .first_codepoint = static_cast<char32_t>(0xac00U),
                .last_codepoint = static_cast<char32_t>(0xac02U),
            },
            render_text_font_cmap_range{
                .first_codepoint = static_cast<char32_t>(0x1f600U),
                .last_codepoint = static_cast<char32_t>(0x1f64fU),
            },
        },
        .diagnostic = "valid test cmap coverage",
    };
    snapshot.cmap = render_text_font_cmap_inspection{
        .status = render_text_font_cmap_inspect_status::valid,
        .selected_platform_id = 3,
        .selected_encoding_id = 10,
        .selected_format = 12,
        .ranges = snapshot.ranges,
        .diagnostic = "valid format 12 coverage",
    };
    return snapshot;
}

quiz_vulkan::render::render_text_font_glyph_id_resolution_request request_for(
    quiz_vulkan::render::utf8_text_codepoint codepoint)
{
    return quiz_vulkan::render::render_text_font_glyph_id_resolution_request{
        .run_index = 2,
        .codepoint_index = 3,
        .codepoint = codepoint,
        .requested_face_id = 41,
        .resolved_face = face(),
        .has_resolved_face = true,
        .coverage = coverage(),
        .has_coverage = true,
        .fallback_glyph_id = 0,
    };
}

void test_resolver_maps_latin_hangul_and_non_bmp_to_deterministic_glyph_ids()
{
    using namespace quiz_vulkan::render;

    const deterministic_font_glyph_id_resolver resolver;
    const render_text_font_glyph_id_resolution_run run = resolver.resolve_run(
        render_text_font_glyph_id_resolution_run_request{
            .requests = {
                request_for(utf8_text_codepoint{.code_point = U'A', .byte_offset = 0, .byte_count = 1, .valid = true}),
                request_for(utf8_text_codepoint{.code_point = 0xac00U, .byte_offset = 1, .byte_count = 3, .valid = true}),
                request_for(utf8_text_codepoint{.code_point = 0x1f600U, .byte_offset = 4, .byte_count = 4, .valid = true}),
            },
        });

    require(run.ok(), "Latin, Hangul, and non-BMP codepoints resolve successfully");
    require(run.resolutions.size() == 3U, "glyph id run records three resolutions");
    require(run.policy.request_count == 3U, "glyph id policy counts requests");
    require(run.policy.resolved_count == 3U, "glyph id policy counts resolved glyphs");
    require(run.policy.supported_glyph_count == 3U, "glyph id policy counts supported glyphs");
    require(run.policy.fallback_glyph_id_count == 0U, "resolved glyphs do not use fallback glyph id");

    require(run.resolutions[0].glyph_id == U'A', "Latin glyph id is deterministic");
    require(run.resolutions[0].glyph_id_matches_codepoint, "default Latin glyph id matches codepoint");
    require(run.resolutions[1].glyph_id == 0xac00U, "Hangul glyph id is deterministic");
    require(run.resolutions[2].glyph_id == 0x1f600U, "non-BMP glyph id is deterministic");
    require(run.resolutions[2].selected_cmap_format == 12U, "resolver preserves cmap format diagnostics");
    require(run.resolutions[2].cmap_status == render_text_font_cmap_inspect_status::valid, "resolver preserves cmap status");
}

void test_resolver_maps_face_local_glyph_id_without_codepoint_fallback()
{
    using namespace quiz_vulkan::render;

    render_text_font_glyph_id_resolution_request request = request_for(utf8_text_codepoint{
        .code_point = U'A',
        .byte_offset = 0,
        .byte_count = 1,
        .valid = true,
    });
    request.resolved_face.glyph_id_offset = 1000;

    const deterministic_font_glyph_id_resolver resolver;
    const render_text_font_glyph_id_resolution_snapshot resolution = resolver.resolve(request);
    const render_text_font_shaping_codepoint_selection selection =
        font_glyph_id_resolution_to_shaping_selection(resolution);

    require(resolution.ok(), "face-local glyph id resolves successfully");
    require(resolution.glyph_id == 1065U, "face-local glyph id applies descriptor offset");
    require(resolution.glyph_id_offset == 1000U, "glyph id resolution records descriptor offset");
    require(!resolution.glyph_id_matches_codepoint, "face-local glyph id proves codepoint was not reused");
    require(!resolution.used_fallback_glyph_id, "face-local glyph id does not use fallback glyph id");
    require(selection.glyph_id == resolution.glyph_id, "shaping selection receives resolved face-local glyph id");
    require(selection.glyph_id_offset == 1000U, "shaping selection preserves glyph id offset");
    require(!selection.glyph_id_matches_codepoint, "shaping selection preserves non-codepoint glyph id diagnostic");
    require(!selection.used_fallback_glyph_id, "shaping selection does not mark fallback glyph id");
}

void test_resolver_rejects_invalid_utf8_without_claiming_glyph_support()
{
    using namespace quiz_vulkan::render;

    const deterministic_font_glyph_id_resolver resolver;
    render_text_font_glyph_id_resolution_request request = request_for(utf8_text_codepoint{
        .code_point = utf8_replacement_codepoint,
        .byte_offset = 0,
        .byte_count = 1,
        .valid = false,
    });
    request.fallback_glyph_id = 9;
    const render_text_font_glyph_id_resolution_snapshot resolution = resolver.resolve(request);

    require(!resolution.ok(), "invalid UTF-8 does not resolve");
    require(
        resolution.status == render_text_font_glyph_id_resolution_status::invalid_utf8,
        "invalid UTF-8 status is preserved");
    require(!resolution.glyph_supported, "invalid UTF-8 does not claim glyph support");
    require(resolution.used_fallback_glyph_id, "invalid UTF-8 records fallback glyph use");
    require(resolution.glyph_id == 9U, "invalid UTF-8 uses configured fallback glyph id");
    require(!resolution.glyph_id_matches_codepoint, "invalid UTF-8 fallback differs from replacement codepoint");
}

void test_resolver_preserves_invalid_cmap_coverage_status()
{
    using namespace quiz_vulkan::render;

    render_text_font_unicode_coverage_snapshot invalid_coverage{
        .source_label = "fixture://fonts/invalid-cmap",
        .status = render_text_font_unicode_coverage_status::cmap_invalid,
        .diagnostic = "unsupported cmap fixture",
    };
    invalid_coverage.cmap = render_text_font_cmap_inspection{
        .status = render_text_font_cmap_inspect_status::unsupported_subtable_format,
        .selected_format = 6,
        .diagnostic = "unsupported format 6",
    };

    render_text_font_glyph_id_resolution_request request = request_for(utf8_text_codepoint{
        .code_point = U'A',
        .byte_offset = 0,
        .byte_count = 1,
        .valid = true,
    });
    request.coverage = invalid_coverage;

    const deterministic_font_glyph_id_resolver resolver;
    const render_text_font_glyph_id_resolution_snapshot resolution = resolver.resolve(request);

    require(!resolution.ok(), "invalid cmap coverage does not resolve");
    require(
        resolution.status == render_text_font_glyph_id_resolution_status::coverage_invalid,
        "invalid cmap coverage reports coverage invalid");
    require(resolution.coverage_status == render_text_font_unicode_coverage_status::cmap_invalid, "coverage status is preserved");
    require(
        resolution.cmap_status == render_text_font_cmap_inspect_status::unsupported_subtable_format,
        "cmap status is preserved");
    require(resolution.selected_cmap_format == 6U, "selected cmap format is preserved");
    require(!resolution.glyph_supported, "invalid cmap coverage does not claim glyph support");
}

void test_resolver_reports_unsupported_codepoint_and_shaping_selection()
{
    using namespace quiz_vulkan::render;

    render_text_font_glyph_id_resolution_request request = request_for(utf8_text_codepoint{
        .code_point = U'?',
        .byte_offset = 0,
        .byte_count = 1,
        .valid = true,
    });
    request.fallback_glyph_id = 99;

    const deterministic_font_glyph_id_resolver resolver;
    const render_text_font_glyph_id_resolution_snapshot resolution = resolver.resolve(request);
    const render_text_font_shaping_codepoint_selection selection =
        font_glyph_id_resolution_to_shaping_selection(resolution);

    require(!resolution.ok(), "unsupported codepoint does not resolve");
    require(
        resolution.status == render_text_font_glyph_id_resolution_status::unsupported_codepoint,
        "unsupported codepoint status is recorded");
    require(!resolution.glyph_supported, "unsupported codepoint does not claim support");
    require(resolution.glyph_id == 99U, "unsupported codepoint uses configured fallback glyph id");
    require(selection.glyph_id == 99U, "shaping selection receives fallback glyph id");
    require(selection.has_glyph_id, "shaping selection marks glyph id availability");
    require(!selection.glyph_supported, "shaping selection preserves unsupported glyph state");
    require(selection.used_fallback_glyph_id, "shaping selection marks unsupported fallback glyph id");
}

void test_descriptor_coverage_snapshot_keeps_known_empty_faces_unsupported()
{
    using namespace quiz_vulkan::render;

    const render_text_font_unicode_coverage_snapshot snapshot = font_glyph_id_coverage_snapshot_for_descriptor(
        font_face_descriptor{
            .id = 7,
            .family = "Known Empty",
            .coverage = {
                font_unicode_coverage_known_empty_codepoint_range(),
            },
        });

    require(snapshot.ok(), "known-empty descriptor coverage is still a valid diagnostic snapshot");
    require(snapshot.ranges.empty(), "known-empty descriptor coverage exposes no supported ranges");
    require(!snapshot.supports_codepoint(U'A'), "known-empty descriptor coverage does not support Latin");
}

} // namespace

int main()
{
    test_resolver_maps_latin_hangul_and_non_bmp_to_deterministic_glyph_ids();
    test_resolver_maps_face_local_glyph_id_without_codepoint_fallback();
    test_resolver_rejects_invalid_utf8_without_claiming_glyph_support();
    test_resolver_preserves_invalid_cmap_coverage_status();
    test_resolver_reports_unsupported_codepoint_and_shaping_selection();
    test_descriptor_coverage_snapshot_keeps_known_empty_faces_unsupported();
    return 0;
}
