#include "core/layout/layout_placer.h"
#include "render/text/fake_text_engine.h"
#include "render/text/font_glyph_atlas.h"
#include "render/text/font_resolver.h"
#include "render/text/scene_text_metrics_adapter.h"
#include "render/text/text_engine.h"
#include "render/text/utf8_line_break.h"
#include "render/text/utf8_text_run.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <optional>
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

bool near(float actual, float expected)
{
    return std::fabs(actual - expected) < 0.001f;
}

float line_height_for(const quiz_vulkan::render::render_text_style& style)
{
    return style.line_height > 0.0f ? style.line_height : style.font_size;
}

float glyph_advance_for(const quiz_vulkan::render::render_text_style& style)
{
    return (style.font_size * 0.5f) + style.letter_spacing;
}

void test_utf8_text_run_iterates_codepoints_and_replacements()
{
    using namespace quiz_vulkan::render;

    const std::string text = std::string("A")
        + std::string("\xed\x95\x9c", 3)
        + std::string("\xc0\xaf", 2)
        + std::string("\xe2\x82", 2)
        + std::string("\xf0\x9f\x99\x82", 4);

    const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(text);
    require(codepoints.size() == 7, "UTF-8 helper emits one entry per scalar or replacement");
    require(codepoints[0].code_point == 'A', "UTF-8 helper decodes ASCII scalar");
    require(codepoints[0].byte_offset == 0 && codepoints[0].byte_count == 1, "UTF-8 helper tracks ASCII bytes");
    require(codepoints[0].valid, "UTF-8 helper marks ASCII scalar valid");

    require(codepoints[1].code_point == 0xd55c, "UTF-8 helper decodes Hangul syllable");
    require(codepoints[1].byte_offset == 1 && codepoints[1].byte_count == 3, "UTF-8 helper tracks Hangul bytes");
    require(codepoints[1].valid, "UTF-8 helper marks Hangul scalar valid");
    require(is_utf8_hangul_syllable(codepoints[1].code_point), "UTF-8 helper recognizes Hangul syllables");

    for (std::size_t index = 2; index < 6; ++index) {
        require(codepoints[index].code_point == utf8_replacement_codepoint, "malformed UTF-8 emits replacement");
        require(codepoints[index].byte_count == 1, "malformed UTF-8 consumes one byte per replacement");
        require(!codepoints[index].valid, "malformed UTF-8 replacement is marked invalid");
        require(codepoints[index].cluster_start, "malformed UTF-8 replacement starts a diagnostic cluster");
    }
    require(codepoints[2].byte_offset == 4, "overlong leading byte offset is stable");
    require(codepoints[3].byte_offset == 5, "overlong continuation byte offset is stable");
    require(codepoints[4].byte_offset == 6, "truncated leading byte offset is stable");
    require(codepoints[5].byte_offset == 7, "truncated continuation byte offset is stable");

    require(codepoints[6].code_point == 0x1f642, "UTF-8 helper decodes four-byte scalar after malformed bytes");
    require(codepoints[6].byte_offset == 8 && codepoints[6].byte_count == 4, "UTF-8 helper tracks four-byte scalar");
    require(codepoints[6].valid, "UTF-8 helper marks four-byte scalar valid");
}

void test_utf8_text_run_clusters_ascii_combining_and_hangul()
{
    using namespace quiz_vulkan::render;

    const std::string text = std::string("A")
        + std::string("e")
        + std::string("\xcc\x81", 2)
        + std::string("\xed\x95\x9c", 3)
        + std::string("Z");

    const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(text);
    require(codepoints.size() == 5, "UTF-8 cluster fixture decodes expected codepoints");
    require(codepoints[0].cluster_start, "ASCII codepoint starts its cluster");
    require(codepoints[1].cluster_start, "ASCII base starts combining cluster");
    require(!codepoints[2].cluster_start, "combining mark joins previous base cluster");
    require(codepoints[3].cluster_start, "Hangul syllable starts its own cluster");
    require(codepoints[4].cluster_start, "ASCII after Hangul starts its own cluster");

    const std::vector<utf8_text_cluster> clusters = cluster_utf8_text_run(codepoints);
    require(clusters.size() == 4, "UTF-8 helper emits basic grapheme-ish clusters");
    require(clusters[0].byte_offset == 0 && clusters[0].byte_count == 1, "first ASCII cluster tracks bytes");
    require(clusters[0].codepoint_offset == 0 && clusters[0].codepoint_count == 1, "first ASCII cluster tracks scalar");
    require(clusters[1].byte_offset == 1 && clusters[1].byte_count == 3, "combining cluster spans base and mark bytes");
    require(clusters[1].codepoint_offset == 1 && clusters[1].codepoint_count == 2, "combining cluster spans two scalars");
    require(clusters[1].valid, "combining cluster is valid when both scalars decode");
    require(clusters[2].byte_offset == 4 && clusters[2].byte_count == 3, "Hangul cluster tracks syllable bytes");
    require(clusters[2].codepoint_count == 1, "Hangul syllable remains a single cluster");
    require(clusters[3].byte_offset == 7 && clusters[3].byte_count == 1, "ASCII after Hangul tracks bytes");
}

void test_utf8_line_breaks_ascii_whitespace_and_newlines()
{
    using namespace quiz_vulkan::render;

    const std::vector<utf8_line_fragment> fragments = break_utf8_text_run("ab cd\nef");
    require(fragments.size() == 3, "UTF-8 line breaker splits on ASCII space and newline");

    require(fragments[0].break_reason == utf8_line_break_reason::ascii_whitespace, "first break is ASCII whitespace");
    require(fragments[0].byte_offset == 0 && fragments[0].byte_count == 2, "first fragment tracks byte range");
    require(
        fragments[0].codepoint_offset == 0 && fragments[0].codepoint_count == 2,
        "first fragment tracks codepoint range");
    require(
        fragments[0].separator_byte_offset == 2 && fragments[0].separator_byte_count == 1,
        "first fragment preserves whitespace separator byte range");
    require(
        fragments[0].separator_codepoint_offset == 2 && fragments[0].separator_codepoint_count == 1,
        "first fragment preserves whitespace separator codepoint range");

    require(fragments[1].break_reason == utf8_line_break_reason::explicit_newline, "second break is newline");
    require(fragments[1].byte_offset == 3 && fragments[1].byte_count == 2, "second fragment tracks byte range");
    require(
        fragments[1].codepoint_offset == 3 && fragments[1].codepoint_count == 2,
        "second fragment tracks codepoint range");
    require(
        fragments[1].separator_byte_offset == 5 && fragments[1].separator_byte_count == 1,
        "second fragment preserves newline byte range");
    require(
        fragments[1].separator_codepoint_offset == 5 && fragments[1].separator_codepoint_count == 1,
        "second fragment preserves newline codepoint range");

    require(fragments[2].break_reason == utf8_line_break_reason::end_of_text, "final break is end of text");
    require(fragments[2].byte_offset == 6 && fragments[2].byte_count == 2, "final fragment tracks byte range");
    require(
        fragments[2].codepoint_offset == 6 && fragments[2].codepoint_count == 2,
        "final fragment tracks codepoint range");
    require(fragments[2].separator_byte_count == 0, "final fragment has no separator bytes");
}

void test_utf8_line_breaks_crlf_as_single_explicit_newline()
{
    using namespace quiz_vulkan::render;

    const std::vector<utf8_line_fragment> fragments = break_utf8_text_run("a\r\nb");
    require(fragments.size() == 2, "UTF-8 line breaker treats CRLF as one explicit newline separator");
    require(fragments[0].break_reason == utf8_line_break_reason::explicit_newline, "CRLF break is explicit newline");
    require(fragments[0].byte_offset == 0 && fragments[0].byte_count == 1, "CRLF first fragment tracks content");
    require(
        fragments[0].separator_byte_offset == 1 && fragments[0].separator_byte_count == 2,
        "CRLF separator preserves both bytes");
    require(
        fragments[0].separator_codepoint_offset == 1 && fragments[0].separator_codepoint_count == 2,
        "CRLF separator preserves both codepoints");
    require(fragments[1].byte_offset == 3 && fragments[1].byte_count == 1, "CRLF final fragment starts after newline");
}

void test_utf8_line_breaks_hangul_only_under_width_pressure()
{
    using namespace quiz_vulkan::render;

    const std::string hangul = std::string("\xed\x95\x9c", 3)
        + std::string("\xea\xb8\x80", 3)
        + std::string("\xeb\x82\xa0", 3);

    const std::vector<utf8_line_fragment> unwrapped = break_utf8_text_run(hangul);
    require(unwrapped.size() == 1, "Hangul syllables do not split without width pressure");
    require(unwrapped[0].break_reason == utf8_line_break_reason::end_of_text, "unwrapped Hangul ends normally");
    require(unwrapped[0].byte_offset == 0 && unwrapped[0].byte_count == 9, "unwrapped Hangul preserves byte range");
    require(
        unwrapped[0].codepoint_offset == 0 && unwrapped[0].codepoint_count == 3,
        "unwrapped Hangul preserves codepoint range");

    const std::vector<utf8_line_fragment> wrapped = break_utf8_text_run(hangul, utf8_line_break_options{
        .max_columns = 2,
        .break_hangul_syllables_on_width = true,
    });
    require(wrapped.size() == 3, "Hangul syllables split under width pressure");
    require(wrapped[0].break_reason == utf8_line_break_reason::width_pressure, "first Hangul break uses width pressure");
    require(wrapped[0].byte_offset == 0 && wrapped[0].byte_count == 3, "first Hangul fragment tracks bytes");
    require(wrapped[0].codepoint_offset == 0 && wrapped[0].codepoint_count == 1, "first Hangul fragment tracks scalar");
    require(wrapped[0].separator_byte_offset == 3 && wrapped[0].separator_byte_count == 0, "first pressure break has no separator");
    require(wrapped[1].break_reason == utf8_line_break_reason::width_pressure, "second Hangul break uses width pressure");
    require(wrapped[1].byte_offset == 3 && wrapped[1].byte_count == 3, "second Hangul fragment tracks bytes");
    require(wrapped[1].codepoint_offset == 1 && wrapped[1].codepoint_count == 1, "second Hangul fragment tracks scalar");
    require(wrapped[2].break_reason == utf8_line_break_reason::end_of_text, "final Hangul fragment ends normally");
    require(wrapped[2].byte_offset == 6 && wrapped[2].byte_count == 3, "final Hangul fragment tracks bytes");
    require(wrapped[2].codepoint_offset == 2 && wrapped[2].codepoint_count == 1, "final Hangul fragment tracks scalar");
}

void test_utf8_line_break_keeps_unspaced_ascii_words_together_under_width_pressure()
{
    using namespace quiz_vulkan::render;

    const std::vector<utf8_line_fragment> fragments = break_utf8_text_run("abcd", utf8_line_break_options{
        .max_columns = 2,
        .break_hangul_syllables_on_width = true,
    });
    require(fragments.size() == 1, "ASCII words do not split without whitespace");
    require(fragments[0].break_reason == utf8_line_break_reason::end_of_text, "ASCII word break remains end of text");
    require(fragments[0].byte_offset == 0 && fragments[0].byte_count == 4, "ASCII word preserves byte range");
    require(fragments[0].codepoint_offset == 0 && fragments[0].codepoint_count == 4, "ASCII word preserves scalar range");
}

void test_utf8_line_break_can_fallback_split_long_tokens_on_width()
{
    using namespace quiz_vulkan::render;

    const std::vector<utf8_line_fragment> fragments = break_utf8_text_run("abcd", utf8_line_break_options{
        .max_columns = 2,
        .break_hangul_syllables_on_width = true,
        .break_long_tokens_on_width = true,
    });
    require(fragments.size() == 2, "long-token fallback splits unspaced ASCII under width pressure");
    require(fragments[0].break_reason == utf8_line_break_reason::width_pressure, "long-token fallback uses width pressure");
    require(fragments[0].byte_offset == 0 && fragments[0].byte_count == 2, "long-token fallback tracks first byte range");
    require(
        fragments[0].codepoint_offset == 0 && fragments[0].codepoint_count == 2,
        "long-token fallback tracks first scalar range");
    require(fragments[0].separator_byte_count == 0, "long-token fallback has no separator bytes");
    require(fragments[1].break_reason == utf8_line_break_reason::end_of_text, "long-token fallback ends final fragment");
    require(fragments[1].byte_offset == 2 && fragments[1].byte_count == 2, "long-token fallback tracks final byte range");
}

class recording_text_engine final : public quiz_vulkan::render::text_engine_interface {
public:
    quiz_vulkan::render::render_text_measure measure_text(
        const quiz_vulkan::render::render_text_request& request) const override
    {
        measure_requests.push_back(request);

        float width = 0.0f;
        float height = 0.0f;
        for (const quiz_vulkan::render::render_text_run& run : request.text_runs) {
            const quiz_vulkan::render::render_text_style& style = request.style_catalog.resolve(run.style_token);
            width += static_cast<float>(run.text.size()) * glyph_advance_for(style);
            height = std::max(height, line_height_for(style));
        }

        if (request.options.wrap == quiz_vulkan::render::render_text_wrap_mode::word
            && request.bounds.width > 0.0f) {
            width = std::min(width, request.bounds.width);
        }

        return quiz_vulkan::render::render_text_measure{width, height};
    }

    quiz_vulkan::render::render_text_layout layout_text(
        const quiz_vulkan::render::render_text_request& request) const override
    {
        layout_requests.push_back(request);

        quiz_vulkan::render::render_text_layout layout;
        layout.measure = measure_text(request);
        return layout;
    }

    std::vector<quiz_vulkan::render::render_text_atlas_update> consume_atlas_updates() override
    {
        return {};
    }

    mutable std::vector<quiz_vulkan::render::render_text_request> measure_requests;
    mutable std::vector<quiz_vulkan::render::render_text_request> layout_requests;
};

quiz_vulkan::render::render_text_style_catalog make_style_catalog()
{
    quiz_vulkan::render::render_text_style_catalog catalog;
    catalog.fallback_style = quiz_vulkan::render::render_text_style{
        .id = "fallback",
        .font_family = "Sans",
        .font_size = 12.0f,
        .line_height = 14.0f,
        .letter_spacing = 0.0f,
        .font_weight = 400,
        .italic = false,
    };
    catalog.styles.push_back(quiz_vulkan::render::render_text_style{
        .id = "body",
        .font_family = "Sans",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .letter_spacing = 0.0f,
        .font_weight = 400,
        .italic = false,
    });
    catalog.styles.push_back(quiz_vulkan::render::render_text_style{
        .id = "caption",
        .font_family = "Serif",
        .font_size = 10.0f,
        .line_height = 12.0f,
        .letter_spacing = 1.0f,
        .font_weight = 600,
        .italic = true,
    });
    return catalog;
}

void test_style_catalog_find_and_resolve()
{
    const quiz_vulkan::render::render_text_style_catalog catalog = make_style_catalog();

    const quiz_vulkan::render::render_text_style* body = catalog.find("body");
    require(body != nullptr, "style catalog finds registered style");
    require(body->font_size == 20.0f, "style catalog preserves registered style fields");
    require(catalog.find("missing") == nullptr, "style catalog reports missing style");
    require(catalog.resolve("caption").italic, "style catalog resolves registered style");
    require(catalog.resolve("missing").id == "fallback", "style catalog resolves missing style to fallback");
}

void test_font_face_catalog_resolves_exact_faces_and_fallback()
{
    using namespace quiz_vulkan::render;

    font_face_catalog catalog;
    const font_face_id regular_id = catalog.add_face(font_face_descriptor{
        .family = "Fixture Sans",
        .source_uri = "fixture://fonts/fixture-sans-regular",
        .version = "fixture-1",
        .license = "test-fixture",
        .weight = 400,
        .italic = false,
        .fallback = false,
    }).id;
    const font_face_id bold_id = catalog.add_face(font_face_descriptor{
        .family = "Fixture Sans",
        .source_uri = "fixture://fonts/fixture-sans-bold",
        .version = "fixture-1",
        .license = "test-fixture",
        .weight = 700,
        .italic = false,
        .fallback = false,
    }).id;
    const font_face_id fallback_id = catalog.add_face(font_face_descriptor{
        .family = "Fixture Fallback",
        .source_uri = "fixture://fonts/fixture-fallback",
        .version = "fixture-1",
        .license = "test-fixture",
        .weight = 400,
        .italic = false,
        .fallback = true,
    }).id;

    render_text_style style;
    style.font_family = "Fixture Sans";
    style.font_weight = 400;
    style.italic = false;
    require(catalog.resolve(style)->id == regular_id, "font catalog resolves exact regular face");

    style.font_weight = 700;
    require(catalog.resolve(style)->id == bold_id, "font catalog resolves exact bold face");

    style.font_family = "Missing Sans";
    style.font_weight = 400;
    const font_face_descriptor* resolved_fallback = catalog.resolve(style);
    require(resolved_fallback != nullptr, "font catalog resolves missing family to fallback");
    require(resolved_fallback->id == fallback_id, "font catalog chooses explicit fallback face");
    require(resolved_fallback->source_uri == "fixture://fonts/fixture-fallback", "font catalog preserves source URI");
    require(resolved_fallback->license == "test-fixture", "font catalog preserves license metadata");
}

void test_font_face_catalog_reports_codepoint_fallback_diagnostics()
{
    using namespace quiz_vulkan::render;

    font_face_catalog catalog;
    const font_face_id latin_id = catalog.add_face(font_face_descriptor{
        .family = "Fixture Sans",
        .source_uri = "fixture://fonts/fixture-sans-regular",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {font_codepoint_range{.first = 0x0020, .last = 0x007E}},
        .weight = 400,
        .italic = false,
        .fallback = false,
    }).id;
    const font_face_id hangul_id = catalog.add_face(font_face_descriptor{
        .family = "Fixture Hangul",
        .source_uri = "fixture://fonts/fixture-hangul",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {font_codepoint_range{.first = 0xAC00, .last = 0xD7A3}},
        .weight = 400,
        .italic = false,
        .fallback = true,
    }).id;
    const font_face_id emoji_id = catalog.add_face(font_face_descriptor{
        .family = "Fixture Emoji",
        .source_uri = "fixture://fonts/fixture-emoji",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {font_codepoint_range{.first = 0x1F600, .last = 0x1F64F}},
        .weight = 400,
        .italic = false,
        .fallback = false,
    }).id;

    render_text_style style;
    style.font_family = "Fixture Sans";
    style.font_weight = 400;
    style.italic = false;

    const font_face_resolution latin = catalog.resolve_for_codepoint(style, 0x0041);
    require(latin.requested_face != nullptr, "font coverage diagnostics report requested face");
    require(latin.resolved_face != nullptr, "font coverage diagnostics resolve latin face");
    require(latin.resolved_face->id == latin_id, "font coverage keeps latin glyph on requested face");
    require(!latin.used_fallback, "font coverage does not report fallback for covered latin glyph");
    require(latin.glyph_supported, "font coverage reports latin glyph support");

    const font_face_resolution hangul = catalog.resolve_for_codepoint(style, 0xAC00);
    require(hangul.requested_face != nullptr, "font coverage diagnostics keep original requested face");
    require(hangul.resolved_face != nullptr, "font coverage resolves hangul fallback");
    require(hangul.resolved_face->id == hangul_id, "font coverage prefers explicit hangul fallback");
    require(hangul.used_fallback, "font coverage reports hangul fallback");
    require(hangul.glyph_supported, "font coverage reports hangul glyph support");

    const font_face_resolution emoji = catalog.resolve_for_codepoint(style, 0x1F600);
    require(emoji.resolved_face != nullptr, "font coverage resolves non-explicit covering face");
    require(emoji.resolved_face->id == emoji_id, "font coverage falls through to any covering face");
    require(emoji.used_fallback, "font coverage reports non-requested covering face as fallback");
    require(emoji.glyph_supported, "font coverage reports emoji glyph support");

    const font_face_resolution unsupported = catalog.resolve_for_codepoint(style, 0x0378);
    require(unsupported.resolved_face != nullptr, "font coverage keeps a diagnostic face for unsupported glyph");
    require(unsupported.resolved_face->id == latin_id, "font coverage keeps requested face for missing glyph diagnostics");
    require(!unsupported.used_fallback, "font coverage does not claim fallback when no face covers glyph");
    require(!unsupported.glyph_supported, "font coverage reports unsupported glyph");
}

void test_font_face_catalog_prefers_known_coverage_fallbacks()
{
    using namespace quiz_vulkan::render;

    font_face_catalog catalog;
    const font_face_id catchall_id = catalog.add_face(font_face_descriptor{
        .family = "Fixture Catchall",
        .source_uri = "fixture://fonts/catchall",
        .version = "fixture-1",
        .license = "test-fixture",
        .weight = 400,
        .italic = false,
        .fallback = true,
    }).id;
    const font_face_id latin_id = catalog.add_face(font_face_descriptor{
        .family = "Fixture Latin",
        .source_uri = "fixture://fonts/latin",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {font_codepoint_range{.first = 0x0020, .last = 0x007e}},
        .weight = 400,
        .italic = false,
        .fallback = false,
    }).id;
    const font_face_id hangul_id = catalog.add_face(font_face_descriptor{
        .family = "Fixture Hangul",
        .source_uri = "fixture://fonts/hangul",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {font_codepoint_range{.first = 0xac00, .last = 0xd7a3}},
        .weight = 400,
        .italic = false,
        .fallback = true,
    }).id;

    const font_face_resolution latin = catalog.resolve_for_face_and_codepoint(latin_id, 'A');
    require(latin.resolved_face != nullptr, "coverage fallback fixture resolves latin face");
    require(latin.resolved_face->id == latin_id, "latin glyph stays on requested face");
    require(!latin.used_fallback, "latin glyph does not use fallback");

    const font_face_resolution hangul = catalog.resolve_for_face_and_codepoint(latin_id, 0xd55c);
    require(hangul.resolved_face != nullptr, "coverage fallback fixture resolves Hangul face");
    require(hangul.resolved_face->id == hangul_id, "known Hangul coverage beats catchall fallback");
    require(hangul.used_fallback, "Hangul glyph reports fallback from Latin face");
    require(hangul.glyph_supported, "Hangul glyph is supported by fallback face");

    const font_face_resolution unknown = catalog.resolve_for_face_and_codepoint(latin_id, 0x0378);
    require(unknown.resolved_face != nullptr, "unknown glyph falls through to catchall fallback");
    require(unknown.resolved_face->id == catchall_id, "catchall fallback remains available after known coverage");
    require(unknown.used_fallback, "unknown glyph reports fallback to catchall");
    require(unknown.glyph_supported, "catchall fallback reports glyph support");
}

void test_deterministic_fake_font_resolver_reports_face_fallbacks()
{
    using namespace quiz_vulkan::render;

    deterministic_fake_font_resolver resolver;

    render_text_style style;
    style.font_family = "Sans";
    style.font_weight = 400;
    style.italic = false;

    const font_resolver_result exact = resolver.resolve(style);
    require(exact.exact_face_id == 1, "font resolver reports exact fixture face id");
    require(exact.resolved_face_id == 1, "font resolver resolves exact face id");
    require(exact.resolved_family == "Sans", "font resolver preserves exact family");
    require(!exact.used_fallback(), "font resolver does not report fallback for exact face");

    style.font_weight = 900;
    const font_resolver_result style_fallback = resolver.resolve(style);
    require(style_fallback.exact_face_id == 0, "font resolver reports missing exact style");
    require(style_fallback.resolved_face_id == 2, "font resolver chooses deterministic nearest family face");
    require(style_fallback.resolved_weight == 700, "font resolver reports fallback weight");
    require(!style_fallback.used_family_fallback, "font resolver keeps same family for style fallback");
    require(style_fallback.used_style_fallback, "font resolver reports style fallback");

    style.font_family = "Display";
    style.font_weight = 400;
    const font_resolver_result family_fallback = resolver.resolve(style);
    require(family_fallback.exact_face_id == 0, "font resolver reports missing family exact face");
    require(family_fallback.resolved_face_id == 1, "font resolver chooses deterministic fallback face");
    require(family_fallback.resolved_family == "Sans", "font resolver reports fallback family");
    require(family_fallback.used_family_fallback, "font resolver reports family fallback");
    require(!family_fallback.used_style_fallback, "font resolver does not report style fallback for missing family");
}

void test_glyph_atlas_cache_allocates_rows_pages_and_cached_slots()
{
    using namespace quiz_vulkan::render;

    glyph_atlas_cache cache(glyph_atlas_page_config{
        .width = 16,
        .height = 16,
        .padding = 1,
    });

    const glyph_atlas_key glyph_a{.face_id = 1, .glyph_id = 65, .pixel_size = 20};
    const glyph_atlas_key glyph_b{.face_id = 1, .glyph_id = 66, .pixel_size = 20};
    const glyph_atlas_key glyph_c{.face_id = 1, .glyph_id = 67, .pixel_size = 20};
    const glyph_atlas_key glyph_d{.face_id = 1, .glyph_id = 68, .pixel_size = 20};
    const glyph_atlas_key glyph_e{.face_id = 1, .glyph_id = 69, .pixel_size = 20};

    const std::optional<glyph_atlas_slot> slot_a = cache.cache_glyph(glyph_a, 5, 6);
    require(slot_a.has_value(), "glyph atlas allocates first glyph");
    require(slot_a->newly_allocated, "first glyph reports new allocation");
    require(slot_a->page.id == 1, "first glyph lands on page one");
    require(slot_a->page.revision == 1, "first glyph increments page revision");
    require(near(slot_a->atlas_bounds.x, 1.0f), "first glyph x includes padding");
    require(near(slot_a->atlas_bounds.y, 1.0f), "first glyph y includes padding");

    const std::optional<glyph_atlas_slot> slot_b = cache.cache_glyph(glyph_b, 5, 6);
    require(slot_b.has_value(), "glyph atlas allocates second glyph");
    require(slot_b->page.id == 1, "second glyph stays on page one");
    require(slot_b->page.revision == 2, "second glyph increments page revision");
    require(near(slot_b->atlas_bounds.x, 8.0f), "second glyph packs on first row");
    require(near(slot_b->atlas_bounds.y, 1.0f), "second glyph stays on first row");

    const std::optional<glyph_atlas_slot> slot_c = cache.cache_glyph(glyph_c, 5, 6);
    require(slot_c.has_value(), "glyph atlas allocates third glyph");
    require(slot_c->page.id == 1, "third glyph stays on page one");
    require(near(slot_c->atlas_bounds.x, 1.0f), "third glyph wraps to row start");
    require(near(slot_c->atlas_bounds.y, 9.0f), "third glyph wraps to next row");

    require(cache.cache_glyph(glyph_d, 5, 6).has_value(), "glyph atlas fills remaining row slot");
    const std::optional<glyph_atlas_slot> slot_e = cache.cache_glyph(glyph_e, 5, 6);
    require(slot_e.has_value(), "glyph atlas allocates new page when full");
    require(slot_e->page.id == 2, "overflow glyph lands on page two");
    require(slot_e->page.revision == 1, "new page starts at first revision");

    const std::optional<glyph_atlas_slot> cached_a = cache.cache_glyph(glyph_a, 5, 6);
    require(cached_a.has_value(), "glyph atlas returns cached glyph");
    require(!cached_a->newly_allocated, "cached glyph does not report new allocation");
    require(cached_a->page.id == 1, "cached glyph keeps original page");
    require(cached_a->page.revision == 4, "cached glyph reports latest page revision");
    require(near(cached_a->atlas_bounds.x, slot_a->atlas_bounds.x), "cached glyph keeps atlas x");
    require(near(cached_a->atlas_bounds.y, slot_a->atlas_bounds.y), "cached glyph keeps atlas y");

    const std::optional<glyph_atlas_slot> too_large = cache.cache_glyph(
        glyph_atlas_key{.face_id = 1, .glyph_id = 999, .pixel_size = 20},
        15,
        15);
    require(!too_large.has_value(), "glyph atlas rejects glyph larger than padded page");
    require(cache.pages().size() == 2, "rejected glyph does not create another page");
}

void test_glyph_atlas_cache_consumes_dirty_page_updates_by_revision()
{
    using namespace quiz_vulkan::render;

    glyph_atlas_cache cache(glyph_atlas_page_config{
        .width = 16,
        .height = 16,
        .padding = 1,
    });

    const glyph_atlas_key glyph_a{.face_id = 1, .glyph_id = 65, .pixel_size = 20};
    const glyph_atlas_key glyph_b{.face_id = 1, .glyph_id = 66, .pixel_size = 20};
    const glyph_atlas_key glyph_c{.face_id = 1, .glyph_id = 67, .pixel_size = 20};
    const glyph_atlas_key glyph_d{.face_id = 1, .glyph_id = 68, .pixel_size = 20};
    const glyph_atlas_key glyph_e{.face_id = 1, .glyph_id = 69, .pixel_size = 20};

    require(cache.consume_dirty_page_updates().empty(), "glyph atlas starts with no dirty page updates");

    require(cache.cache_glyph(glyph_a, 5, 6).has_value(), "glyph atlas queues first dirty page");
    std::vector<render_text_atlas_update> first_updates = cache.consume_dirty_page_updates();
    require(first_updates.size() == 1, "glyph atlas emits first dirty page update");
    require(first_updates[0].page.id == 1, "first dirty update uses page one");
    require(first_updates[0].page.revision == 1, "first dirty update reports page revision one");
    require(near(first_updates[0].updated_bounds.x, 1.0f), "first dirty update tracks glyph x");
    require(near(first_updates[0].updated_bounds.y, 1.0f), "first dirty update tracks glyph y");
    require(near(first_updates[0].updated_bounds.width, 5.0f), "first dirty update tracks glyph width");
    require(near(first_updates[0].updated_bounds.height, 6.0f), "first dirty update tracks glyph height");
    require(first_updates[0].rgba.size() == 5U * 6U * 4U, "first dirty update carries tight RGBA payload");
    require(first_updates[0].rgba[0] == 52U, "first dirty payload red channel is deterministic");
    require(first_updates[0].rgba[1] == 2U, "first dirty payload green channel includes atlas x and revision");
    require(first_updates[0].rgba[2] == 2U, "first dirty payload blue channel includes page and atlas y");
    require(first_updates[0].rgba[3] == 255U, "first dirty payload alpha is opaque");
    require(cache.consume_dirty_page_updates().empty(), "glyph atlas consumes dirty page updates once");

    require(cache.cache_glyph(glyph_b, 5, 6).has_value(), "glyph atlas queues second glyph update");
    require(cache.cache_glyph(glyph_c, 5, 6).has_value(), "glyph atlas merges row-wrapped glyph update");
    std::vector<render_text_atlas_update> merged_updates = cache.consume_dirty_page_updates();
    require(merged_updates.size() == 1, "glyph atlas merges dirty bounds per page");
    require(merged_updates[0].page.id == 1, "merged dirty update stays on page one");
    require(merged_updates[0].page.revision == 3, "merged dirty update reports latest page revision");
    require(near(merged_updates[0].updated_bounds.x, 1.0f), "merged dirty bounds keep left edge");
    require(near(merged_updates[0].updated_bounds.y, 1.0f), "merged dirty bounds keep top edge");
    require(near(merged_updates[0].updated_bounds.width, 12.0f), "merged dirty bounds span packed columns");
    require(near(merged_updates[0].updated_bounds.height, 14.0f), "merged dirty bounds span wrapped rows");
    require(merged_updates[0].rgba.size() == 12U * 14U * 4U, "merged dirty update carries RGBA for union bounds");
    require(merged_updates[0].rgba[0] == 114U, "merged dirty payload includes latest page revision");

    require(cache.cache_glyph(glyph_d, 5, 6).has_value(), "glyph atlas queues final page one glyph");
    require(cache.cache_glyph(glyph_e, 5, 6).has_value(), "glyph atlas queues page two glyph");
    std::vector<render_text_atlas_update> page_updates = cache.consume_dirty_page_updates();
    require(page_updates.size() == 2, "glyph atlas emits dirty updates for both touched pages");
    require(page_updates[0].page.id == 1, "first multi-page update keeps page order");
    require(page_updates[0].page.revision == 4, "page one update reports latest revision");
    require(page_updates[1].page.id == 2, "second multi-page update reports page two");
    require(page_updates[1].page.revision == 1, "page two update starts at revision one");
    require(page_updates[0].rgba.size() == 5U * 6U * 4U, "page one dirty update carries glyph payload");
    require(page_updates[1].rgba.size() == 5U * 6U * 4U, "page two dirty update carries glyph payload");
    require(page_updates[1].rgba[0] == 69U, "page two dirty payload includes page id");

    require(cache.cache_glyph(glyph_a, 5, 6).has_value(), "glyph atlas returns cached glyph before dirty check");
    require(cache.consume_dirty_page_updates().empty(), "cached glyph does not dirty atlas page");
    require(
        !cache.cache_glyph(glyph_atlas_key{.face_id = 1, .glyph_id = 999, .pixel_size = 20}, 15, 15).has_value(),
        "glyph atlas rejects oversized glyph before dirtying page");
    require(cache.consume_dirty_page_updates().empty(), "rejected glyph does not dirty atlas page");
}

void test_fake_measure_and_layout_emit_stable_glyphs()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "AB", .style_token = "body"},
        render_text_run{.text = "c", .style_token = "caption"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_measure measure = engine.measure_text(request);
    require(near(measure.width, 26.0f), "fake engine measures stable width");
    require(near(measure.height, 24.0f), "fake engine measures max line height");

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 3, "fake engine emits one glyph per ASCII codepoint");
    require(layout.glyphs[0].glyph_id == 'A', "first glyph id is stable");
    require(layout.glyphs[1].byte_offset == 1, "second glyph byte offset is stable");
    require(layout.glyphs[2].run_index == 1, "third glyph records source run");
    require(near(layout.glyphs[2].bounds.x, 20.0f), "third glyph x advances from prior run");
    require(near(layout.glyphs[2].bounds.width, 6.0f), "third glyph width uses caption style");
}

void test_fake_multirun_layout_tracks_lines_offsets_and_alignment()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A\nB", .style_token = "body"},
        render_text_run{.text = "cd", .style_token = "caption"},
    };
    request.bounds = render_rect{5.0f, 7.0f, 100.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::center,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    require(near(layout.measure.width, 22.0f), "multi-run layout measures widest line");
    require(near(layout.measure.height, 48.0f), "multi-run layout sums line heights");
    require(layout.glyphs.size() == 4, "multi-run layout omits newline glyph");

    require(layout.glyphs[0].run_index == 0, "first line glyph records first run");
    require(layout.glyphs[0].byte_offset == 0, "first line glyph byte offset is stable");
    require(near(layout.glyphs[0].bounds.x, 50.0f), "first line is center aligned independently");
    require(near(layout.glyphs[0].bounds.y, 7.0f), "first line uses request y origin");

    require(layout.glyphs[1].run_index == 0, "second line starts in first run");
    require(layout.glyphs[1].byte_offset == 2, "newline advances source byte offset");
    require(near(layout.glyphs[1].bounds.x, 44.0f), "second line is center aligned by line width");
    require(near(layout.glyphs[1].bounds.y, 31.0f), "second line y advances by first line height");

    require(layout.glyphs[2].run_index == 1, "second line continues into next run");
    require(layout.glyphs[2].byte_offset == 0, "new run byte offsets restart at zero");
    require(near(layout.glyphs[2].bounds.x, 54.0f), "caption glyph advances after body glyph");
    require(near(layout.glyphs[2].bounds.width, 6.0f), "caption glyph keeps run style width");
    require(near(layout.glyphs[2].bounds.height, 12.0f), "caption glyph keeps run style line height");
}

void test_fake_newline_edge_cases_preserve_empty_line_height()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A\n", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_measure trailing_measure = engine.measure_text(request);
    require(near(trailing_measure.width, 10.0f), "trailing newline keeps prior line width");
    require(near(trailing_measure.height, 48.0f), "trailing newline contributes empty line height");

    const render_text_layout trailing_layout = engine.layout_text(request);
    require(trailing_layout.glyphs.size() == 1, "trailing newline does not emit visible glyph");
    require(near(trailing_layout.measure.height, 48.0f), "trailing newline layout keeps empty line height");

    const std::vector<fake_text_engine_caret> trailing_carets = engine.caret_positions(request);
    require(trailing_carets.size() == 3, "trailing newline caret positions include empty line caret");
    require(trailing_carets[0].byte_offset == 0, "trailing newline first caret starts visible glyph");
    require(trailing_carets[1].byte_offset == 1, "trailing newline second caret follows visible glyph");
    require(trailing_carets[2].byte_offset == 2, "trailing newline third caret follows newline byte");
    require(near(trailing_carets[2].bounds.y, 24.0f), "trailing newline empty caret moves to second line");
    require(near(trailing_carets[2].bounds.height, 24.0f), "trailing newline empty caret keeps line height");

    request.text_runs = {
        render_text_run{.text = "\nA", .style_token = "body"},
    };
    const render_text_layout leading_layout = engine.layout_text(request);
    require(near(leading_layout.measure.width, 10.0f), "leading newline measures following line width");
    require(near(leading_layout.measure.height, 48.0f), "leading newline contributes empty line height");
    require(leading_layout.glyphs.size() == 1, "leading newline does not emit visible glyph");
    require(near(leading_layout.glyphs[0].bounds.y, 24.0f), "glyph after leading newline moves to second line");

    const std::vector<fake_text_engine_caret> leading_carets = engine.caret_positions(request);
    require(leading_carets.size() == 3, "leading newline caret positions include empty line caret");
    require(leading_carets[0].byte_offset == 0, "leading newline first caret sits before newline byte");
    require(leading_carets[1].byte_offset == 1, "leading newline second caret starts visible glyph");
    require(leading_carets[2].byte_offset == 2, "leading newline third caret follows visible glyph");
    require(near(leading_carets[0].bounds.y, 0.0f), "leading newline empty caret uses first line y");
    require(near(leading_carets[1].bounds.y, 24.0f), "leading newline visible caret moves to second line");
}

void test_fake_style_fallback_shapes_missing_tokens()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "xy", .style_token = "missing"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_measure measure = engine.measure_text(request);
    require(near(measure.width, 12.0f), "missing style token uses fallback width");
    require(near(measure.height, 14.0f), "missing style token uses fallback line height");
    require(engine.last_diagnostics().used_style_fallback(), "missing style token records fallback diagnostics");
    require(engine.last_diagnostics().style_fallbacks.size() == 1, "one missing style fallback is recorded");
    require(engine.last_diagnostics().style_fallbacks[0].run_index == 0, "fallback diagnostic records run index");
    require(
        engine.last_diagnostics().style_fallbacks[0].requested_style_token == "missing",
        "fallback diagnostic records requested style token");
    require(
        engine.last_diagnostics().style_fallbacks[0].fallback_style_token == "fallback",
        "fallback diagnostic records fallback style token");

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 2, "fallback layout emits requested glyphs");
    require(near(layout.glyphs[0].bounds.width, 6.0f), "fallback glyph width uses fallback font size");
    require(near(layout.glyphs[0].bounds.height, 14.0f), "fallback glyph height uses fallback line height");
    require(near(layout.glyphs[1].bounds.x, 6.0f), "fallback glyph advances from prior fallback glyph");
}

void test_fake_font_resolver_records_family_and_style_fallbacks()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "a", .style_token = "heavy"},
        render_text_run{.text = "b", .style_token = "display"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.style_catalog.styles.push_back(render_text_style{
        .id = "heavy",
        .font_family = "Sans",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .letter_spacing = 0.0f,
        .font_weight = 900,
        .italic = false,
    });
    request.style_catalog.styles.push_back(render_text_style{
        .id = "display",
        .font_family = "Display",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .letter_spacing = 0.0f,
        .font_weight = 400,
        .italic = false,
    });
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 2, "font fallback diagnostics do not change fake glyph emission");
    require(engine.last_diagnostics().has_font_face_selections(), "font resolver records all face selections");
    require(engine.last_diagnostics().font_face_selections.size() == 2, "font resolver records both selected faces");
    require(engine.last_diagnostics().font_face_selections[0].resolved_face_id == 2, "font selection records style fallback face");
    require(
        engine.last_diagnostics().font_face_selections[0].used_style_fallback,
        "font selection records style fallback use");
    require(engine.last_diagnostics().font_face_selections[1].resolved_face_id == 1, "font selection records family fallback face");
    require(
        engine.last_diagnostics().font_face_selections[1].used_family_fallback,
        "font selection records family fallback use");
    require(engine.last_diagnostics().has_font_catalog_policy(), "font resolver records catalog policy");
    require(
        engine.last_diagnostics().font_catalog_policy.style_face_mappings.size() == 2,
        "font resolver records two catalog mappings");
    require(
        engine.last_diagnostics().font_catalog_policy.style_face_mappings[0].style_token == "display",
        "font resolver sorts catalog mappings by style token");
    require(
        engine.last_diagnostics().font_catalog_policy.style_face_mappings[0].resolved_face_id == 1,
        "font resolver maps display to fallback face id");
    require(
        engine.last_diagnostics().font_catalog_policy.style_face_mappings[1].style_token == "heavy",
        "font resolver keeps heavy mapping after sort");
    require(
        engine.last_diagnostics().font_catalog_policy.style_face_mappings[1].resolved_face_id == 2,
        "font resolver maps heavy to style fallback face id");
    require(
        engine.last_diagnostics().font_catalog_policy.missing_face_fallback_count == 2,
        "font resolver counts missing face fallbacks");
    require(
        engine.last_diagnostics().font_catalog_policy.supported_codepoint_count == 2,
        "font resolver counts supported catalog glyphs");
    require(
        engine.last_diagnostics().font_catalog_policy.fallback_codepoint_count == 0,
        "font resolver counts no fallback glyphs for ASCII catalog request");
    require(
        engine.last_diagnostics().font_catalog_policy.missing_glyph_count == 0,
        "font resolver counts no missing glyphs for ASCII catalog request");
    require(engine.last_diagnostics().used_font_fallback(), "font resolver records fallback diagnostics");
    require(engine.last_diagnostics().font_fallbacks.size() == 2, "font resolver records both fallback runs");

    const fake_text_engine_font_fallback& style_fallback = engine.last_diagnostics().font_fallbacks[0];
    require(style_fallback.run_index == 0, "font style fallback records run index");
    require(style_fallback.style_token == "heavy", "font style fallback records style token");
    require(style_fallback.requested_family == "Sans", "font style fallback records requested family");
    require(style_fallback.resolved_family == "Sans", "font style fallback keeps family");
    require(style_fallback.requested_weight == 900, "font style fallback records requested weight");
    require(style_fallback.resolved_weight == 700, "font style fallback records resolved weight");
    require(style_fallback.resolved_face_id == 2, "font style fallback records resolved face id");
    require(!style_fallback.used_family_fallback, "font style fallback does not report family fallback");
    require(style_fallback.used_style_fallback, "font style fallback reports style fallback");

    const fake_text_engine_font_fallback& family_fallback = engine.last_diagnostics().font_fallbacks[1];
    require(family_fallback.run_index == 1, "font family fallback records run index");
    require(family_fallback.style_token == "display", "font family fallback records style token");
    require(family_fallback.requested_family == "Display", "font family fallback records requested family");
    require(family_fallback.resolved_family == "Sans", "font family fallback records resolved family");
    require(family_fallback.resolved_face_id == 1, "font family fallback records fallback face id");
    require(family_fallback.used_family_fallback, "font family fallback reports family fallback");
    require(!family_fallback.used_style_fallback, "font family fallback does not report style fallback");

    request.text_runs = {
        render_text_run{.text = "ok", .style_token = "body"},
    };
    (void)engine.measure_text(request);
    require(!engine.last_diagnostics().used_font_fallback(), "clean font request clears font fallback diagnostics");
}

void test_fake_font_resolution_policy_tracks_codepoint_fallbacks_and_cache_readiness()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    const font_face_id latin_face_id = engine.add_font_face(font_face_descriptor{
        .family = "Coverage Sans",
        .source_uri = "fixture://fonts/coverage-sans-latin",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {font_codepoint_range{.first = 0x0020, .last = 0x007e}},
        .weight = 400,
        .italic = false,
        .fallback = false,
    }).id;
    const font_face_id hangul_face_id = engine.add_font_face(font_face_descriptor{
        .family = "Coverage Hangul",
        .source_uri = "fixture://fonts/coverage-hangul",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {font_codepoint_range{.first = 0xac00, .last = 0xd7a3}},
        .weight = 400,
        .italic = false,
        .fallback = true,
    }).id;

    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A\xed\x95\x9c", .style_token = "coverage"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.style_catalog.styles.push_back(render_text_style{
        .id = "coverage",
        .font_family = "Coverage Sans",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .letter_spacing = 0.0f,
        .font_weight = 400,
        .italic = false,
    });
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 2, "coverage fallback fixture emits two glyphs");
    require(layout.glyphs[0].glyph_id == 'A', "coverage fallback keeps Latin glyph id");
    require(layout.glyphs[1].glyph_id == 0xd55c, "coverage fallback keeps Hangul glyph id");

    require(engine.last_diagnostics().has_font_face_selections(), "coverage fallback records run face selection");
    require(engine.last_diagnostics().font_face_selections.size() == 1, "coverage fallback records one run selection");
    require(
        engine.last_diagnostics().font_face_selections[0].resolved_face_id == latin_face_id,
        "coverage fallback resolves the run to the Latin face");
    require(
        !engine.last_diagnostics().font_face_selections[0].used_family_fallback,
        "coverage fallback does not need family fallback at run level");
    require(engine.last_diagnostics().has_glyph_font_resolutions(), "coverage fallback records glyph face resolutions");
    require(engine.last_diagnostics().glyph_font_resolutions.size() == 2, "coverage fallback records each glyph resolution");

    const render_text_glyph_font_resolution_snapshot& latin = engine.last_diagnostics().glyph_font_resolutions[0];
    const render_text_glyph_font_resolution_snapshot& hangul = engine.last_diagnostics().glyph_font_resolutions[1];
    require(latin.requested_face_id == latin_face_id, "Latin glyph requests Latin face");
    require(latin.resolved_face_id == latin_face_id, "Latin glyph resolves to Latin face");
    require(!latin.used_codepoint_fallback, "Latin glyph does not use codepoint fallback");
    require(latin.glyph_supported, "Latin glyph reports support");
    require(latin.cacheable, "Latin glyph is cache-ready");
    require(hangul.requested_face_id == latin_face_id, "Hangul glyph starts from run Latin face");
    require(hangul.resolved_face_id == hangul_face_id, "Hangul glyph resolves to coverage fallback face");
    require(hangul.used_codepoint_fallback, "Hangul glyph records codepoint fallback");
    require(hangul.glyph_supported, "Hangul glyph reports support through fallback");
    require(hangul.cacheable, "Hangul glyph is cache-ready through fallback");

    require(engine.last_diagnostics().font_resolution_policy.run_request_count == 1, "font policy counts run request");
    require(engine.last_diagnostics().font_resolution_policy.exact_face_match_count == 1, "font policy counts exact run match");
    require(engine.last_diagnostics().font_resolution_policy.glyph_request_count == 2, "font policy counts glyph requests");
    require(engine.last_diagnostics().font_resolution_policy.glyph_supported_count == 2, "font policy counts supported glyphs");
    require(
        engine.last_diagnostics().font_resolution_policy.codepoint_fallback_count == 1,
        "font policy counts glyph-level fallback");
    require(engine.last_diagnostics().font_resolution_policy.cacheable_glyph_count == 2, "font policy counts cacheable glyphs");
    require(
        engine.last_diagnostics().font_resolution_policy.unique_resolved_face_count == 2,
        "font policy counts unique resolved glyph faces");
    require(engine.last_diagnostics().has_font_catalog_policy(), "coverage fallback records catalog policy");
    require(
        engine.last_diagnostics().font_catalog_policy.style_face_mappings.size() == 1,
        "coverage fallback records one catalog mapping");
    require(
        engine.last_diagnostics().font_catalog_policy.style_face_mappings[0].resolved_face_id == latin_face_id,
        "coverage fallback catalog keeps the run on the Latin face");
    require(
        engine.last_diagnostics().font_catalog_policy.missing_face_fallback_count == 0,
        "coverage fallback does not report missing face fallback");
    require(
        engine.last_diagnostics().font_catalog_policy.supported_codepoint_count == 2,
        "coverage fallback counts supported glyphs in catalog policy");
    require(
        engine.last_diagnostics().font_catalog_policy.fallback_codepoint_count == 1,
        "coverage fallback counts codepoint fallback glyphs in catalog policy");
    require(
        engine.last_diagnostics().font_catalog_policy.missing_glyph_count == 0,
        "coverage fallback counts no missing glyphs in catalog policy");

    const std::vector<render_text_glyph_cluster>& clusters = engine.last_diagnostics().glyph_clusters;
    require(clusters.size() == 2, "coverage fallback records one cluster per scalar");
    require(clusters[0].resolved_face_id == latin_face_id, "Latin cluster records Latin face");
    require(clusters[1].resolved_face_id == hangul_face_id, "Hangul cluster records fallback face");

    require(engine.last_diagnostics().has_glyph_cache_readiness(), "coverage fallback records cache readiness");
    require(engine.last_diagnostics().glyph_cache_readiness.size() == 2, "coverage fallback records cluster cache readiness");
    const render_text_glyph_cache_readiness_snapshot& latin_cache =
        engine.last_diagnostics().glyph_cache_readiness[0];
    const render_text_glyph_cache_readiness_snapshot& hangul_cache =
        engine.last_diagnostics().glyph_cache_readiness[1];
    require(latin_cache.requested_face_id == latin_face_id, "Latin cache readiness records requested face");
    require(latin_cache.resolved_face_id == latin_face_id, "Latin cache readiness records resolved face");
    require(latin_cache.cache_key.face_id == latin_face_id, "Latin cache key uses resolved face");
    require(latin_cache.cache_key.glyph_id == 'A', "Latin cache key records glyph id");
    require(latin_cache.atlas_width == 10 && latin_cache.atlas_height == 24, "Latin cache readiness records atlas size");
    require(latin_cache.estimated_rgba_bytes == 10U * 24U * 4U, "Latin cache readiness estimates RGBA bytes");
    require(latin_cache.cacheable, "Latin cluster is cacheable");
    require(latin_cache.has_atlas_slot, "Latin cluster has atlas slot after layout");
    require(hangul_cache.requested_face_id == latin_face_id, "Hangul cache readiness records requested face");
    require(hangul_cache.resolved_face_id == hangul_face_id, "Hangul cache readiness records fallback face");
    require(hangul_cache.cache_key.face_id == hangul_face_id, "Hangul cache key uses fallback face");
    require(hangul_cache.cache_key.glyph_id == 0xd55c, "Hangul cache key records glyph id");
    require(hangul_cache.atlas_width == 20 && hangul_cache.atlas_height == 24, "Hangul cache readiness records atlas size");
    require(hangul_cache.estimated_rgba_bytes == 20U * 24U * 4U, "Hangul cache readiness estimates RGBA bytes");
    require(hangul_cache.used_codepoint_fallback, "Hangul cache readiness records codepoint fallback");
    require(hangul_cache.cacheable, "Hangul fallback cluster is cacheable");
    require(hangul_cache.has_atlas_slot, "Hangul fallback cluster has atlas slot after layout");

    require(
        engine.last_diagnostics().glyph_cache_readiness_policy.cluster_count == 2,
        "cache readiness policy counts clusters");
    require(
        engine.last_diagnostics().glyph_cache_readiness_policy.cacheable_cluster_count == 2,
        "cache readiness policy counts cacheable clusters");
    require(
        engine.last_diagnostics().glyph_cache_readiness_policy.codepoint_fallback_cluster_count == 1,
        "cache readiness policy counts fallback clusters");
    require(
        engine.last_diagnostics().glyph_cache_readiness_policy.unique_cache_key_count == 2,
        "cache readiness policy counts unique cache keys");
    require(
        engine.last_diagnostics().glyph_cache_readiness_policy.unique_face_count == 2,
        "cache readiness policy counts unique cache faces");
    require(
        engine.last_diagnostics().glyph_cache_readiness_policy.estimated_rgba_bytes == (10U * 24U * 4U) + (20U * 24U * 4U),
        "cache readiness policy sums estimated RGBA bytes");
}

void test_fake_atlas_updates_are_revisioned_and_consumed()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_measure measure = engine.measure_text(request);
    require(near(measure.width, 10.0f), "measure works before atlas population");
    require(engine.consume_atlas_updates().empty(), "measure does not enqueue atlas updates");

    const render_text_layout first_layout = engine.layout_text(request);
    require(first_layout.glyphs.size() == 1, "first atlas layout emits one glyph");
    require(first_layout.glyphs[0].atlas_page_id == 1, "fake atlas page id is stable");
    require(first_layout.glyphs[0].atlas_revision == 1, "first new glyph uses atlas revision one");
    require(near(first_layout.glyphs[0].atlas_bounds.x, 1.0f), "first fake glyph atlas x uses padding");
    require(near(first_layout.glyphs[0].atlas_bounds.y, 1.0f), "first fake glyph atlas y uses padding");
    require(near(first_layout.glyphs[0].atlas_bounds.width, 10.0f), "first fake glyph atlas width uses cluster advance");
    require(near(first_layout.glyphs[0].atlas_bounds.height, 24.0f), "first fake glyph atlas height uses cluster height");
    require(engine.last_diagnostics().has_glyph_atlas_placements(), "first layout records atlas placement diagnostics");
    require(
        engine.last_diagnostics().glyph_atlas_placements.size() == 1,
        "first layout records one atlas placement");
    require(
        engine.last_diagnostics().glyph_atlas_placements[0].cluster_index == 0,
        "first atlas placement records cluster index");
    require(
        engine.last_diagnostics().glyph_atlas_placements[0].key.face_id == 1,
        "first atlas placement records resolved face id");
    require(
        engine.last_diagnostics().glyph_atlas_placements[0].key.glyph_id == 'A',
        "first atlas placement records glyph key");
    require(
        engine.last_diagnostics().glyph_atlas_placements[0].key.pixel_size == 24,
        "first atlas placement records pixel size");
    require(
        !engine.last_diagnostics().glyph_atlas_placements[0].cache_hit,
        "first atlas placement is not a cache hit");
    require(
        engine.last_diagnostics().glyph_atlas_placements[0].newly_allocated,
        "first atlas placement records new allocation");
    require(
        engine.last_diagnostics().glyph_atlas_placements[0].created_page,
        "first atlas placement records new page creation");
    require(
        !engine.last_diagnostics().glyph_atlas_placements[0].reused_existing_page,
        "first atlas placement does not claim page reuse");
    require(engine.last_diagnostics().glyph_atlas_metrics.requested_cluster_count == 1, "first atlas requests one cluster");
    require(engine.last_diagnostics().glyph_atlas_metrics.placed_cluster_count == 1, "first atlas places one cluster");
    require(engine.last_diagnostics().glyph_atlas_metrics.cache_hit_count == 0, "first atlas has no cache hits");
    require(engine.last_diagnostics().glyph_atlas_metrics.new_slot_count == 1, "first atlas allocates one slot");
    require(engine.last_diagnostics().glyph_atlas_metrics.new_page_count == 1, "first atlas creates one page");
    require(engine.last_diagnostics().glyph_atlas_metrics.dirty_page_count == 1, "first atlas dirties one page");
    require(engine.last_diagnostics().glyph_atlas_metrics.page_count_after == 1, "first atlas tracks page count");
    require(engine.last_diagnostics().glyph_atlas_metrics.latest_page_revision == 1, "first atlas tracks revision");
    require(engine.last_diagnostics().has_glyph_atlas_pages(), "first atlas records page diagnostics");
    require(engine.last_diagnostics().glyph_atlas_pages.size() == 1, "first atlas records one page snapshot");
    require(engine.last_diagnostics().glyph_atlas_pages[0].page.id == 1, "first page snapshot tracks page id");
    require(engine.last_diagnostics().glyph_atlas_pages[0].page.revision == 1, "first page snapshot tracks revision");
    require(engine.last_diagnostics().glyph_atlas_pages[0].cluster_count == 1, "first page snapshot tracks cluster count");
    require(engine.last_diagnostics().glyph_atlas_pages[0].new_slot_count == 1, "first page snapshot tracks new slot count");
    require(engine.last_diagnostics().glyph_atlas_pages[0].cache_hit_count == 0, "first page snapshot tracks no hits");
    require(engine.last_diagnostics().glyph_atlas_pages[0].created_page_count == 1, "first page snapshot records creation");
    require(engine.last_diagnostics().glyph_atlas_pages[0].reused_page_count == 0, "first page snapshot records no reuse");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_update_count == 1, "first page snapshot records upload");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_cluster_count == 1, "first page snapshot records dirty cluster count");
    require(engine.last_diagnostics().glyph_atlas_pages[0].upload_ready, "first page snapshot marks upload readiness");
    require(
        near(engine.last_diagnostics().glyph_atlas_pages[0].dirty_bounds.x, 1.0f),
        "first page snapshot records dirty bounds x");
    require(
        near(engine.last_diagnostics().glyph_atlas_pages[0].dirty_bounds.width, 10.0f),
        "first page snapshot records dirty bounds width");
    require(engine.last_diagnostics().glyph_atlas_page_policy.page_count == 1, "first page policy tracks one page");
    require(engine.last_diagnostics().glyph_atlas_page_policy.allocated_page_count == 1, "first page policy tracks allocation");
    require(engine.last_diagnostics().glyph_atlas_page_policy.created_page_count == 1, "first page policy tracks created page");
    require(engine.last_diagnostics().glyph_atlas_page_policy.dirty_page_count == 1, "first page policy tracks dirty page");
    require(engine.last_diagnostics().glyph_atlas_page_policy.upload_ready_page_count == 1, "first page policy tracks upload readiness");
    require(engine.last_diagnostics().glyph_atlas_page_policy.cache_hit_page_count == 0, "first page policy tracks no hits");
    require(engine.last_diagnostics().glyph_atlas_page_policy.dirty_cluster_count == 1, "first page policy tracks dirty clusters");
    require(engine.last_diagnostics().glyph_atlas_page_policy.repeated_layout_clean_page_count == 0, "first page policy does not mark clean repeat");
    require(engine.last_diagnostics().has_glyph_cache_faces(), "first atlas records glyph cache face snapshot");
    require(engine.last_diagnostics().glyph_cache_policy.capacity == 8, "first glyph cache records fake capacity");
    require(engine.last_diagnostics().glyph_cache_policy.cached_glyph_count == 1, "first glyph cache stores one glyph");
    require(engine.last_diagnostics().glyph_cache_policy.request_count == 1, "first glyph cache records one lookup");
    require(engine.last_diagnostics().glyph_cache_policy.hit_count == 0, "first glyph cache records no hits");
    require(engine.last_diagnostics().glyph_cache_policy.miss_count == 1, "first glyph cache records one miss");
    require(engine.last_diagnostics().glyph_cache_policy.insert_count == 1, "first glyph cache records one insert");
    require(engine.last_diagnostics().glyph_cache_policy.eviction_count == 0, "first glyph cache records no evictions");
    require(engine.last_diagnostics().glyph_cache_policy.atlas_reuse_count == 0, "first glyph cache records no atlas reuse");
    require(engine.last_diagnostics().glyph_cache_policy.atlas_allocation_count == 1, "first glyph cache records atlas allocation");
    require(
        engine.last_diagnostics().glyph_cache_policy.atlas_page_create_count == 1,
        "first glyph cache records atlas page creation");
    require(engine.last_diagnostics().glyph_cache_faces.size() == 1, "first glyph cache records one face");
    require(engine.last_diagnostics().glyph_cache_faces[0].face_id == 1, "first glyph cache records body face");
    require(
        engine.last_diagnostics().glyph_cache_faces[0].cached_glyph_ids.size() == 1,
        "first glyph cache face records one cached glyph");
    require(engine.last_diagnostics().glyph_cache_faces[0].cached_glyph_ids[0] == 'A', "first glyph cache stores A");
    require(engine.last_diagnostics().glyph_cache_faces[0].miss_count == 1, "first glyph cache face records miss");

    std::vector<render_text_atlas_update> first_updates = engine.consume_atlas_updates();
    require(first_updates.size() == 1, "first new glyph enqueues one atlas update");
    require(first_updates[0].page.id == 1, "atlas update page id is stable");
    require(first_updates[0].page.revision == 1, "first atlas update uses revision one");
    require(near(first_updates[0].updated_bounds.x, 1.0f), "first atlas update records glyph x");
    require(near(first_updates[0].updated_bounds.y, 1.0f), "first atlas update records glyph y");
    require(near(first_updates[0].updated_bounds.width, 10.0f), "first atlas update records glyph width");
    require(near(first_updates[0].updated_bounds.height, 24.0f), "first atlas update records glyph height");
    require(first_updates[0].rgba.size() == 10U * 24U * 4U, "fake atlas update carries cluster RGBA payload");
    require(first_updates[0].rgba[0] == 52U, "fake atlas update red channel is deterministic");
    require(first_updates[0].rgba[3] == 255U, "fake atlas update alpha is opaque");
    require(engine.consume_atlas_updates().empty(), "atlas updates are consumed once");

    const render_text_layout repeated_layout = engine.layout_text(request);
    require(repeated_layout.glyphs[0].atlas_revision == 1, "cached glyph keeps atlas revision");
    require(engine.consume_atlas_updates().empty(), "cached glyph does not enqueue atlas update");
    require(engine.last_diagnostics().glyph_atlas_metrics.cache_hit_count == 1, "cached glyph records one cache hit");
    require(engine.last_diagnostics().glyph_atlas_metrics.new_slot_count == 0, "cached glyph does not allocate");
    require(engine.last_diagnostics().glyph_atlas_metrics.dirty_page_count == 0, "cached glyph dirties no page");
    require(engine.last_diagnostics().glyph_atlas_placements[0].cache_hit, "cached placement records hit");
    require(engine.last_diagnostics().glyph_atlas_pages.size() == 1, "cached glyph keeps one page snapshot");
    require(engine.last_diagnostics().glyph_atlas_pages[0].cache_hit_count == 1, "cached page snapshot records hit");
    require(engine.last_diagnostics().glyph_atlas_pages[0].new_slot_count == 0, "cached page snapshot records no new slot");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_update_count == 0, "cached page snapshot records no upload");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_cluster_count == 0, "cached page snapshot records no dirty clusters");
    require(!engine.last_diagnostics().glyph_atlas_pages[0].upload_ready, "cached page snapshot clears readiness");
    require(engine.last_diagnostics().glyph_atlas_page_policy.cache_hit_page_count == 1, "cached page policy tracks hit page");
    require(engine.last_diagnostics().glyph_atlas_page_policy.upload_ready_page_count == 0, "cached page policy tracks no upload");
    require(engine.last_diagnostics().glyph_atlas_page_policy.repeated_layout_clean_page_count == 1, "cached page policy records clean repeat");
    require(engine.last_diagnostics().glyph_cache_policy.hit_count == 1, "cached glyph policy records one hit");
    require(engine.last_diagnostics().glyph_cache_policy.miss_count == 0, "cached glyph policy records no misses");
    require(engine.last_diagnostics().glyph_cache_policy.insert_count == 0, "cached glyph policy records no inserts");
    require(
        engine.last_diagnostics().glyph_cache_faces[0].hit_count == 1,
        "cached glyph policy face records one hit");

    request.text_runs = {
        render_text_run{.text = "AB", .style_token = "body"},
    };
    const render_text_layout second_layout = engine.layout_text(request);
    require(second_layout.glyphs.size() == 2, "second atlas layout emits cached and new glyphs");
    require(second_layout.glyphs[0].atlas_revision == 2, "cached glyph points at latest atlas revision");
    require(second_layout.glyphs[1].atlas_revision == 2, "new glyph points at latest atlas revision");

    std::vector<render_text_atlas_update> second_updates = engine.consume_atlas_updates();
    require(second_updates.size() == 1, "new glyph enqueues one later atlas update");
    require(second_updates[0].page.revision == 2, "later atlas update increments revision");
    require(near(second_updates[0].updated_bounds.x, 13.0f), "later atlas update records packed glyph x");
    require(engine.last_diagnostics().glyph_atlas_placements.size() == 2, "second layout records two placements");
    require(engine.last_diagnostics().glyph_atlas_placements[0].cache_hit, "second layout hits cached first glyph");
    require(engine.last_diagnostics().glyph_atlas_placements[0].page.revision == 2, "cached placement refreshes page revision");
    require(engine.last_diagnostics().glyph_atlas_placements[1].newly_allocated, "second layout allocates new glyph");
    require(
        engine.last_diagnostics().glyph_atlas_placements[1].reused_existing_page,
        "second layout records page reuse for new glyph");
    require(!engine.last_diagnostics().glyph_atlas_placements[1].created_page, "second glyph reuses page one");
    require(engine.last_diagnostics().glyph_atlas_metrics.requested_cluster_count == 2, "second atlas requests two clusters");
    require(engine.last_diagnostics().glyph_atlas_metrics.cache_hit_count == 1, "second atlas records one hit");
    require(engine.last_diagnostics().glyph_atlas_metrics.new_slot_count == 1, "second atlas records one new slot");
    require(engine.last_diagnostics().glyph_atlas_metrics.reused_page_slot_count == 1, "second atlas records page reuse");
    require(engine.last_diagnostics().glyph_atlas_metrics.new_page_count == 0, "second atlas creates no page");
    require(engine.last_diagnostics().glyph_atlas_metrics.latest_page_revision == 2, "second atlas tracks latest revision");
    require(engine.last_diagnostics().glyph_atlas_pages.size() == 1, "second atlas still records one page");
    require(engine.last_diagnostics().glyph_atlas_pages[0].page.revision == 2, "second page snapshot tracks latest revision");
    require(engine.last_diagnostics().glyph_atlas_pages[0].cluster_count == 2, "second page snapshot tracks two clusters");
    require(engine.last_diagnostics().glyph_atlas_pages[0].cache_hit_count == 1, "second page snapshot tracks cached cluster");
    require(engine.last_diagnostics().glyph_atlas_pages[0].new_slot_count == 1, "second page snapshot tracks new slot");
    require(engine.last_diagnostics().glyph_atlas_pages[0].created_page_count == 0, "second page snapshot records reuse");
    require(engine.last_diagnostics().glyph_atlas_pages[0].reused_page_count == 1, "second page snapshot records reused page");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_update_count == 1, "second page snapshot records upload");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_cluster_count == 1, "second page snapshot records dirty clusters");
    require(engine.last_diagnostics().glyph_atlas_pages[0].upload_ready, "second page snapshot marks upload readiness");
    require(engine.last_diagnostics().glyph_atlas_page_policy.allocated_page_count == 1, "second page policy tracks one allocated page");
    require(engine.last_diagnostics().glyph_atlas_page_policy.dirty_page_count == 1, "second page policy tracks dirty page");
    require(engine.last_diagnostics().glyph_atlas_page_policy.upload_ready_page_count == 1, "second page policy tracks upload readiness");
    require(engine.last_diagnostics().glyph_atlas_page_policy.cache_hit_page_count == 1, "second page policy tracks hit page");
    require(engine.last_diagnostics().glyph_atlas_page_policy.dirty_cluster_count == 1, "second page policy tracks dirty clusters");
    require(engine.last_diagnostics().glyph_atlas_page_policy.repeated_layout_clean_page_count == 0, "second page policy reports not clean");
    require(engine.last_diagnostics().glyph_atlas_page_policy.total_cluster_count == 2, "second page policy tracks total clusters");
    require(engine.last_diagnostics().glyph_cache_policy.request_count == 2, "second glyph cache records two lookups");
    require(engine.last_diagnostics().glyph_cache_policy.hit_count == 1, "second glyph cache records cached A");
    require(engine.last_diagnostics().glyph_cache_policy.miss_count == 1, "second glyph cache records missing B");
    require(engine.last_diagnostics().glyph_cache_policy.insert_count == 1, "second glyph cache records B insert");
    require(engine.last_diagnostics().glyph_cache_policy.eviction_count == 0, "second glyph cache does not evict");
    require(
        engine.last_diagnostics().glyph_cache_policy.atlas_page_reuse_count == 1,
        "second glyph cache records atlas page reuse");
    require(engine.last_diagnostics().glyph_cache_faces[0].cached_glyph_ids.size() == 2, "second glyph cache stores two glyphs");
    require(engine.last_diagnostics().glyph_cache_faces[0].cached_glyph_ids[0] == 'A', "second glyph cache keeps A");
    require(engine.last_diagnostics().glyph_cache_faces[0].cached_glyph_ids[1] == 'B', "second glyph cache inserts B");
}

void test_fake_glyph_atlas_page_diagnostics_track_overflow()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "ABCDEFGHIJK", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 400.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 11, "atlas overflow fixture emits eleven glyphs");
    require(engine.last_diagnostics().glyph_atlas_placements.size() == 11, "atlas overflow records all placements");
    require(engine.last_diagnostics().glyph_atlas_metrics.requested_cluster_count == 11, "atlas overflow requests all clusters");
    require(engine.last_diagnostics().glyph_atlas_metrics.placed_cluster_count == 11, "atlas overflow places all clusters");
    require(engine.last_diagnostics().glyph_atlas_metrics.cache_hit_count == 0, "atlas overflow starts with no hits");
    require(engine.last_diagnostics().glyph_atlas_metrics.new_slot_count == 11, "atlas overflow allocates all slots");
    require(engine.last_diagnostics().glyph_atlas_metrics.reused_page_slot_count == 9, "atlas overflow reuses first page slots");
    require(engine.last_diagnostics().glyph_atlas_metrics.new_page_count == 2, "atlas overflow creates two pages");
    require(engine.last_diagnostics().glyph_atlas_metrics.dirty_page_count == 2, "atlas overflow dirties both pages");
    require(engine.last_diagnostics().glyph_atlas_metrics.page_count_after == 2, "atlas overflow tracks page count");
    require(engine.last_diagnostics().glyph_atlas_metrics.latest_page_revision == 10, "atlas overflow tracks max page revision");
    require(engine.last_diagnostics().has_glyph_atlas_pages(), "atlas overflow records page diagnostics");
    require(engine.last_diagnostics().glyph_atlas_pages.size() == 2, "atlas overflow records two page snapshots");
    require(engine.last_diagnostics().glyph_atlas_pages[0].page.id == 1, "overflow page snapshot keeps page one");
    require(engine.last_diagnostics().glyph_atlas_pages[0].cluster_count == 10, "overflow page one tracks cluster count");
    require(engine.last_diagnostics().glyph_atlas_pages[0].new_slot_count == 10, "overflow page one tracks new slots");
    require(engine.last_diagnostics().glyph_atlas_pages[0].created_page_count == 1, "overflow page one records creation");
    require(engine.last_diagnostics().glyph_atlas_pages[0].reused_page_count == 9, "overflow page one records reused page slots");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_update_count == 1, "overflow page one records dirty update");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_cluster_count == 10, "overflow page one records dirty clusters");
    require(engine.last_diagnostics().glyph_atlas_pages[0].upload_ready, "overflow page one marks upload readiness");
    require(engine.last_diagnostics().glyph_atlas_pages[1].page.id == 2, "overflow page snapshot keeps page two");
    require(engine.last_diagnostics().glyph_atlas_pages[1].cluster_count == 1, "overflow page two tracks cluster count");
    require(engine.last_diagnostics().glyph_atlas_pages[1].new_slot_count == 1, "overflow page two tracks new slot");
    require(engine.last_diagnostics().glyph_atlas_pages[1].created_page_count == 1, "overflow page two records creation");
    require(engine.last_diagnostics().glyph_atlas_pages[1].reused_page_count == 0, "overflow page two records no page reuse");
    require(engine.last_diagnostics().glyph_atlas_pages[1].dirty_update_count == 1, "overflow page two records dirty update");
    require(engine.last_diagnostics().glyph_atlas_pages[1].dirty_cluster_count == 1, "overflow page two records dirty clusters");
    require(engine.last_diagnostics().glyph_atlas_pages[1].upload_ready, "overflow page two marks upload readiness");
    require(engine.last_diagnostics().glyph_atlas_page_policy.page_count == 2, "overflow page policy tracks page count");
    require(engine.last_diagnostics().glyph_atlas_page_policy.allocated_page_count == 2, "overflow page policy tracks allocated pages");
    require(engine.last_diagnostics().glyph_atlas_page_policy.created_page_count == 2, "overflow page policy tracks created pages");
    require(engine.last_diagnostics().glyph_atlas_page_policy.dirty_page_count == 2, "overflow page policy tracks dirty pages");
    require(engine.last_diagnostics().glyph_atlas_page_policy.upload_ready_page_count == 2, "overflow page policy tracks upload readiness");
    require(engine.last_diagnostics().glyph_atlas_page_policy.dirty_cluster_count == 11, "overflow page policy tracks dirty clusters");
    require(engine.last_diagnostics().glyph_atlas_page_policy.total_cluster_count == 11, "overflow page policy tracks total clusters");
    require(engine.last_diagnostics().glyph_cache_policy.capacity == 8, "atlas overflow records glyph cache capacity");
    require(engine.last_diagnostics().glyph_cache_policy.cached_glyph_count == 8, "atlas overflow caps glyph cache size");
    require(engine.last_diagnostics().glyph_cache_policy.request_count == 11, "atlas overflow records cache lookups");
    require(engine.last_diagnostics().glyph_cache_policy.hit_count == 0, "atlas overflow starts with no policy hits");
    require(engine.last_diagnostics().glyph_cache_policy.miss_count == 11, "atlas overflow records all policy misses");
    require(engine.last_diagnostics().glyph_cache_policy.insert_count == 11, "atlas overflow inserts all misses");
    require(engine.last_diagnostics().glyph_cache_policy.eviction_count == 3, "atlas overflow evicts over capacity");
    require(engine.last_diagnostics().glyph_cache_policy.atlas_allocation_count == 11, "atlas overflow allocates atlas slots");
    require(engine.last_diagnostics().glyph_cache_policy.atlas_page_reuse_count == 9, "atlas overflow reuses atlas page slots");
    require(engine.last_diagnostics().glyph_cache_policy.atlas_page_create_count == 2, "atlas overflow creates atlas pages");
    require(engine.last_diagnostics().glyph_cache_faces.size() == 1, "atlas overflow records one cache face");
    require(engine.last_diagnostics().has_glyph_cache_evictions(), "atlas overflow records eviction history");
    require(engine.last_diagnostics().glyph_cache_evictions.size() == 3, "atlas overflow records three evictions");
    require(engine.last_diagnostics().glyph_cache_evictions[0].cache_key.glyph_id == 'A', "atlas overflow evicts A first");
    require(engine.last_diagnostics().glyph_cache_evictions[1].cache_key.glyph_id == 'B', "atlas overflow evicts B second");
    require(engine.last_diagnostics().glyph_cache_evictions[2].cache_key.glyph_id == 'C', "atlas overflow evicts C third");
    require(engine.last_diagnostics().glyph_cache_faces[0].cached_glyph_ids.size() == 8, "atlas overflow keeps capacity entries");
    require(engine.last_diagnostics().glyph_cache_faces[0].cached_glyph_ids[0] == 'D', "atlas overflow evicts oldest glyphs");
    require(engine.last_diagnostics().glyph_cache_faces[0].cached_glyph_ids[7] == 'K', "atlas overflow keeps newest glyph");
    require(engine.last_diagnostics().glyph_cache_faces[0].eviction_count == 3, "atlas overflow records face evictions");

    const render_text_glyph_atlas_placement_snapshot& first =
        engine.last_diagnostics().glyph_atlas_placements.front();
    const render_text_glyph_atlas_placement_snapshot& tenth =
        engine.last_diagnostics().glyph_atlas_placements[9];
    const render_text_glyph_atlas_placement_snapshot& eleventh =
        engine.last_diagnostics().glyph_atlas_placements[10];
    require(first.page.id == 1, "first overflow glyph lands on first page");
    require(first.created_page, "first overflow glyph creates first page");
    require(near(first.atlas_bounds.x, 1.0f), "first overflow glyph records atlas x");
    require(near(tenth.atlas_bounds.x, 49.0f), "tenth overflow glyph packs on second row");
    require(near(tenth.atlas_bounds.y, 27.0f), "tenth overflow glyph records second row y");
    require(tenth.reused_existing_page, "tenth overflow glyph reuses first page");
    require(eleventh.page.id == 2, "eleventh overflow glyph lands on second page");
    require(eleventh.page.revision == 1, "eleventh overflow glyph starts second page revision");
    require(eleventh.created_page, "eleventh overflow glyph records second page creation");
    require(near(eleventh.atlas_bounds.x, 1.0f), "eleventh overflow glyph starts page two row");
    require(near(eleventh.atlas_bounds.y, 1.0f), "eleventh overflow glyph starts page two column");
    require(layout.glyphs[0].atlas_page_id == 1, "first overflow layout glyph uses page one");
    require(layout.glyphs[9].atlas_page_id == 1, "tenth overflow layout glyph stays on page one");
    require(layout.glyphs[10].atlas_page_id == 2, "eleventh overflow layout glyph uses page two");

    const std::vector<render_text_atlas_update> updates = engine.consume_atlas_updates();
    require(updates.size() == 2, "atlas overflow emits dirty updates for both pages");
    require(updates[0].page.id == 1, "first overflow update is page one");
    require(updates[0].page.revision == 10, "first overflow update reports page one revision");
    require(updates[1].page.id == 2, "second overflow update is page two");
    require(updates[1].page.revision == 1, "second overflow update reports page two revision");
    require(near(updates[0].updated_bounds.x, 1.0f), "page one overflow dirty bounds keep left edge");
    require(near(updates[0].updated_bounds.y, 1.0f), "page one overflow dirty bounds keep top edge");
    require(near(updates[0].updated_bounds.width, 58.0f), "page one overflow dirty bounds span columns");
    require(near(updates[0].updated_bounds.height, 50.0f), "page one overflow dirty bounds span rows");
    require(updates[1].rgba.size() == 10U * 24U * 4U, "page two overflow update carries one glyph payload");

    const render_text_layout repeated_layout = engine.layout_text(request);
    require(repeated_layout.glyphs[10].atlas_page_id == 2, "repeated overflow keeps page two glyph");
    require(engine.last_diagnostics().glyph_atlas_metrics.cache_hit_count == 11, "repeated overflow records all hits");
    require(engine.last_diagnostics().glyph_atlas_metrics.new_slot_count == 0, "repeated overflow allocates no slots");
    require(engine.last_diagnostics().glyph_atlas_metrics.dirty_page_count == 0, "repeated overflow dirties no pages");
    require(engine.last_diagnostics().glyph_atlas_pages[0].cache_hit_count == 10, "repeated overflow page one records cache hits");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_update_count == 0, "repeated overflow page one is clean");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_cluster_count == 0, "repeated overflow page one has no dirty clusters");
    require(!engine.last_diagnostics().glyph_atlas_pages[0].upload_ready, "repeated overflow page one does not require upload");
    require(engine.last_diagnostics().glyph_atlas_pages[1].cache_hit_count == 1, "repeated overflow page two records cache hits");
    require(engine.last_diagnostics().glyph_atlas_pages[1].dirty_update_count == 0, "repeated overflow page two is clean");
    require(engine.last_diagnostics().glyph_atlas_pages[1].dirty_cluster_count == 0, "repeated overflow page two has no dirty clusters");
    require(!engine.last_diagnostics().glyph_atlas_pages[1].upload_ready, "repeated overflow page two does not require upload");
    require(engine.last_diagnostics().glyph_atlas_page_policy.dirty_page_count == 0, "repeated overflow page policy tracks no dirty pages");
    require(engine.last_diagnostics().glyph_atlas_page_policy.upload_ready_page_count == 0, "repeated overflow page policy tracks no uploads");
    require(engine.last_diagnostics().glyph_atlas_page_policy.cache_hit_page_count == 2, "repeated overflow page policy tracks hit pages");
    require(engine.last_diagnostics().glyph_atlas_page_policy.repeated_layout_clean_page_count == 2, "repeated overflow page policy tracks clean repeat pages");
    require(engine.last_diagnostics().glyph_cache_policy.hit_count == 0, "repeated overflow policy thrashes with no hits");
    require(engine.last_diagnostics().glyph_cache_policy.miss_count == 11, "repeated overflow policy records misses");
    require(engine.last_diagnostics().glyph_cache_policy.eviction_count == 11, "repeated overflow policy records evictions");
    require(engine.last_diagnostics().glyph_cache_policy.atlas_reuse_count == 11, "repeated overflow reuses atlas after policy miss");
    require(engine.last_diagnostics().glyph_cache_policy.atlas_allocation_count == 0, "repeated overflow allocates no atlas slots");
    require(engine.last_diagnostics().glyph_cache_faces[0].atlas_reuse_count == 11, "repeated overflow face records atlas reuse");
    require(engine.last_diagnostics().glyph_cache_faces[0].cached_glyph_ids[0] == 'D', "repeated overflow cache returns to D");
    require(engine.last_diagnostics().glyph_cache_faces[0].cached_glyph_ids[7] == 'K', "repeated overflow cache returns to K");
    require(engine.consume_atlas_updates().empty(), "repeated overflow enqueues no atlas updates");

    render_text_request reinsertion_request = request;
    reinsertion_request.text_runs = {
        render_text_run{.text = "A", .style_token = "body"},
    };
    const render_text_layout reinsertion_layout = engine.layout_text(reinsertion_request);
    require(reinsertion_layout.glyphs.size() == 1, "reinsertion layout emits one glyph");
    require(reinsertion_layout.glyphs[0].atlas_page_id == 1, "reinsertion layout reuses the first page");
    require(engine.last_diagnostics().glyph_atlas_metrics.cache_hit_count == 1, "reinsertion layout records one atlas hit");
    require(engine.last_diagnostics().glyph_atlas_metrics.new_slot_count == 0, "reinsertion layout allocates no atlas slots");
    require(engine.last_diagnostics().glyph_atlas_metrics.dirty_page_count == 0, "reinsertion layout keeps atlas clean");
    require(engine.last_diagnostics().has_glyph_cache_evictions(), "reinsertion layout records eviction history");
    require(engine.last_diagnostics().glyph_cache_evictions.size() == 1, "reinsertion layout records one eviction");
    require(engine.last_diagnostics().glyph_cache_evictions[0].cache_key.glyph_id == 'D', "reinsertion layout evicts D first");
    require(
        engine.last_diagnostics().glyph_cache_evictions[0].atlas_reused_after_policy_miss,
        "reinsertion layout records atlas reuse after policy miss");
    require(engine.last_diagnostics().glyph_cache_policy.hit_count == 0, "reinsertion layout records no cache-policy hits");
    require(engine.last_diagnostics().glyph_cache_policy.miss_count == 1, "reinsertion layout records one cache-policy miss");
    require(engine.last_diagnostics().glyph_cache_policy.eviction_count == 1, "reinsertion layout records one cache eviction");
    require(engine.last_diagnostics().glyph_cache_policy.atlas_reuse_count == 1, "reinsertion layout records one atlas reuse");
    require(engine.last_diagnostics().glyph_atlas_pages[0].dirty_update_count == 0, "reinsertion layout leaves page one clean");
    require(engine.last_diagnostics().glyph_atlas_pages[1].dirty_update_count == 0, "reinsertion layout leaves page two clean");
    require(engine.last_diagnostics().glyph_atlas_page_policy.repeated_layout_clean_page_count == 2, "reinsertion layout records both pages clean");
}

void test_fake_caret_positions_follow_utf8_runs_and_combining_marks()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A\xed\x95\x9c", .style_token = "body"},
        render_text_run{.text = "e\xcc\x81", .style_token = "caption"},
    };
    request.bounds = render_rect{2.0f, 3.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const std::vector<fake_text_engine_caret> carets = engine.caret_positions(request);
    require(carets.size() == 5, "caret positions include run starts and cluster boundaries");
    require(engine.consume_atlas_updates().empty(), "caret positions do not enqueue atlas updates");
    require(engine.last_diagnostics().has_glyph_clusters(), "caret positions record glyph cluster diagnostics");
    require(engine.last_diagnostics().glyph_clusters.size() == 3, "caret diagnostics keep UTF-8 glyph clusters");
    require(engine.last_diagnostics().has_caret_rects(), "caret positions record caret rect diagnostics");
    require(engine.last_diagnostics().caret_rects.size() == carets.size(), "caret diagnostics mirror returned carets");

    require(carets[0].run_index == 0 && carets[0].byte_offset == 0, "first caret starts first run");
    require(near(carets[0].bounds.x, 2.0f), "first caret uses request x origin");
    require(near(carets[0].bounds.y, 3.0f), "first caret uses request y origin");
    require(near(carets[0].bounds.height, 24.0f), "caret height uses line height");

    require(carets[1].run_index == 0 && carets[1].byte_offset == 1, "second caret follows ASCII byte");
    require(near(carets[1].bounds.x, 12.0f), "second caret advances by ASCII width");
    require(carets[2].run_index == 0 && carets[2].byte_offset == 4, "third caret follows Hangul bytes");
    require(near(carets[2].bounds.x, 32.0f), "third caret advances by Hangul width");

    require(carets[3].run_index == 1 && carets[3].byte_offset == 0, "fourth caret starts second run");
    require(near(carets[3].bounds.x, 32.0f), "second run starts at prior run x");
    require(carets[4].run_index == 1 && carets[4].byte_offset == 3, "fifth caret follows combining cluster bytes");
    require(near(carets[4].bounds.x, 38.0f), "fifth caret advances by caption width");

    const std::vector<render_text_caret_rect_snapshot>& snapshots = engine.last_diagnostics().caret_rects;
    require(snapshots[3].cluster_index == 2, "combining cluster start caret points at third cluster");
    require(!snapshots[3].at_cluster_end, "combining cluster start caret records boundary side");
    require(snapshots[4].cluster_index == 2, "combining cluster end caret points at third cluster");
    require(snapshots[4].at_cluster_end, "combining cluster end caret records boundary side");
    require(snapshots[4].line_index == 0, "combining cluster caret stays on first line");
}

void test_fake_selection_rects_cover_utf8_ranges_without_atlas_updates()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A\xed\x95\x9c", .style_token = "body"},
        render_text_run{.text = "bc", .style_token = "caption"},
    };
    request.bounds = render_rect{2.0f, 3.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const std::vector<render_rect> rects = engine.selection_rects(
        request,
        fake_text_engine_selection_range{
            .start_run_index = 0,
            .start_byte_offset = 1,
            .end_run_index = 1,
            .end_byte_offset = 1,
        });
    require(rects.size() == 1, "selection across UTF-8 and run boundary coalesces on one line");
    require(near(rects[0].x, 12.0f), "selection starts after ASCII caret");
    require(near(rects[0].y, 3.0f), "selection rect uses request y origin");
    require(near(rects[0].width, 26.0f), "selection covers Hangul and one caption glyph");
    require(near(rects[0].height, 24.0f), "selection rect uses line height");
    require(engine.consume_atlas_updates().empty(), "selection rects do not enqueue atlas updates");
    require(engine.last_diagnostics().has_glyph_clusters(), "selection records glyph cluster diagnostics");
    require(engine.last_diagnostics().has_selection_rects(), "selection records range rect diagnostics");
    require(engine.last_diagnostics().selection_rects.size() == 1, "selection diagnostics mirror returned rects");
    require(engine.last_diagnostics().selection_rects[0].cluster_offset == 1, "selection starts at Hangul cluster");
    require(engine.last_diagnostics().selection_rects[0].cluster_count == 2, "selection spans two clusters");
    require(engine.last_diagnostics().selection_rects[0].start_run_index == 0, "selection diagnostic records start run");
    require(engine.last_diagnostics().selection_rects[0].start_byte_offset == 1, "selection diagnostic records start byte");
    require(engine.last_diagnostics().selection_rects[0].end_run_index == 1, "selection diagnostic records end run");
    require(engine.last_diagnostics().selection_rects[0].end_byte_offset == 1, "selection diagnostic records end byte");
    require(engine.last_diagnostics().selection_rects[0].line_index == 0, "selection diagnostic records line index");

    const std::vector<render_rect> reversed_rects = engine.selection_rects(
        request,
        fake_text_engine_selection_range{
            .start_run_index = 1,
            .start_byte_offset = 1,
            .end_run_index = 0,
            .end_byte_offset = 1,
        });
    require(reversed_rects.size() == 1, "selection normalizes reversed ranges");
    require(near(reversed_rects[0].x, rects[0].x), "reversed selection keeps same x");
    require(near(reversed_rects[0].width, rects[0].width), "reversed selection keeps same width");

    const std::vector<render_rect> collapsed_rects = engine.selection_rects(
        request,
        fake_text_engine_selection_range{
            .start_run_index = 1,
            .start_byte_offset = 1,
            .end_run_index = 1,
            .end_byte_offset = 1,
        });
    require(collapsed_rects.empty(), "collapsed selection has no rects");
}

void test_fake_caret_and_selection_clip_to_request_height()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A\nB\nC", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 10.0f, 100.0f, 30.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const std::vector<fake_text_engine_caret> carets = engine.caret_positions(request);
    require(carets.size() == 4, "bounded caret positions omit fully clipped lines");
    require(carets[0].byte_offset == 0, "first clipped caret starts first visible line");
    require(carets[1].byte_offset == 1, "second clipped caret ends first visible line");
    require(carets[2].byte_offset == 2, "third clipped caret starts partially visible line");
    require(carets[3].byte_offset == 3, "fourth clipped caret ends partially visible line");
    require(near(carets[0].bounds.y, 10.0f), "first clipped caret y is request y");
    require(near(carets[0].bounds.height, 24.0f), "first clipped caret keeps full line height");
    require(near(carets[2].bounds.y, 34.0f), "partially clipped caret y advances by line height");
    require(near(carets[2].bounds.height, 6.0f), "partially clipped caret height stops at bounds bottom");

    const std::vector<render_rect> rects = engine.selection_rects(
        request,
        fake_text_engine_selection_range{
            .start_run_index = 0,
            .start_byte_offset = 0,
            .end_run_index = 0,
            .end_byte_offset = 5,
        });
    require(rects.size() == 2, "bounded selection rects omit fully clipped lines");
    require(near(rects[0].x, 0.0f), "first clipped selection starts at request x");
    require(near(rects[0].y, 10.0f), "first clipped selection y is request y");
    require(near(rects[0].width, 10.0f), "first clipped selection covers first glyph");
    require(near(rects[0].height, 24.0f), "first clipped selection keeps full line height");
    require(near(rects[1].y, 34.0f), "second clipped selection y advances by line height");
    require(near(rects[1].height, 6.0f), "second clipped selection height stops at bounds bottom");
}

void test_fake_selection_rects_follow_wrapped_hangul_lines()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xed\x95\x9c\xea\xb8\x80\xeb\x82\xa0", .style_token = "body"},
    };
    request.bounds = render_rect{4.0f, 6.0f, 25.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::word,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const std::vector<render_rect> rects = engine.selection_rects(
        request,
        fake_text_engine_selection_range{
            .start_run_index = 0,
            .start_byte_offset = 0,
            .end_run_index = 0,
            .end_byte_offset = 9,
        });
    require(rects.size() == 3, "wrapped Hangul selection emits one rect per wrapped line");
    require(near(rects[0].x, 4.0f), "first wrapped Hangul selection starts at request x");
    require(near(rects[1].x, 4.0f), "second wrapped Hangul selection starts at request x");
    require(near(rects[2].x, 4.0f), "third wrapped Hangul selection starts at request x");
    require(near(rects[0].y, 6.0f), "first wrapped Hangul selection y is request y");
    require(near(rects[1].y, 30.0f), "second wrapped Hangul selection y advances by line height");
    require(near(rects[2].y, 54.0f), "third wrapped Hangul selection y advances by two line heights");
    require(near(rects[0].width, 20.0f), "first wrapped Hangul selection covers one syllable");
    require(near(rects[1].width, 20.0f), "second wrapped Hangul selection covers one syllable");
    require(near(rects[2].width, 20.0f), "third wrapped Hangul selection covers one syllable");
    require(near(rects[0].height, 24.0f), "wrapped Hangul selection uses line height");
    require(near(rects[1].height, 24.0f), "wrapped Hangul selection keeps line height");
    require(near(rects[2].height, 24.0f), "wrapped Hangul selection keeps final line height");
    require(engine.last_diagnostics().selection_rects.size() == 3, "wrapped selection diagnostics follow lines");
    require(engine.last_diagnostics().selection_rects[0].line_index == 0, "first wrapped selection records first line");
    require(engine.last_diagnostics().selection_rects[1].line_index == 1, "second wrapped selection records second line");
    require(engine.last_diagnostics().selection_rects[2].line_index == 2, "third wrapped selection records third line");
    require(engine.last_diagnostics().selection_rects[0].cluster_offset == 0, "first wrapped selection starts first cluster");
    require(engine.last_diagnostics().selection_rects[1].cluster_offset == 1, "second wrapped selection starts second cluster");
    require(engine.last_diagnostics().selection_rects[2].cluster_offset == 2, "third wrapped selection starts third cluster");
    require(engine.last_diagnostics().selection_rects[0].cluster_count == 1, "first wrapped selection spans one cluster");
    require(engine.last_diagnostics().selection_rects[1].cluster_count == 1, "second wrapped selection spans one cluster");
    require(engine.last_diagnostics().selection_rects[2].cluster_count == 1, "third wrapped selection spans one cluster");
}

void test_fake_caret_positions_follow_wrapped_hangul_lines()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xed\x95\x9c\xea\xb8\x80\xeb\x82\xa0", .style_token = "body"},
    };
    request.bounds = render_rect{4.0f, 6.0f, 25.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::word,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const std::vector<fake_text_engine_caret> carets = engine.caret_positions(request);
    require(carets.size() == 6, "wrapped Hangul carets include start and end for each line");
    require(engine.last_diagnostics().caret_rects.size() == 6, "wrapped Hangul caret diagnostics follow lines");
    require(carets[0].byte_offset == 0, "first wrapped Hangul caret starts first syllable");
    require(carets[1].byte_offset == 3, "second wrapped Hangul caret follows first syllable");
    require(carets[2].byte_offset == 3, "third wrapped Hangul caret starts second syllable");
    require(carets[3].byte_offset == 6, "fourth wrapped Hangul caret follows second syllable");
    require(carets[4].byte_offset == 6, "fifth wrapped Hangul caret starts third syllable");
    require(carets[5].byte_offset == 9, "sixth wrapped Hangul caret follows third syllable");
    require(near(carets[0].bounds.x, 4.0f), "first wrapped Hangul caret starts at request x");
    require(near(carets[1].bounds.x, 24.0f), "second wrapped Hangul caret advances by syllable width");
    require(near(carets[2].bounds.x, 4.0f), "third wrapped Hangul caret resets x on wrapped line");
    require(near(carets[3].bounds.x, 24.0f), "fourth wrapped Hangul caret advances on second line");
    require(near(carets[4].bounds.x, 4.0f), "fifth wrapped Hangul caret resets x on third line");
    require(near(carets[5].bounds.x, 24.0f), "sixth wrapped Hangul caret advances on third line");
    require(near(carets[0].bounds.y, 6.0f), "first wrapped Hangul caret y is request y");
    require(near(carets[2].bounds.y, 30.0f), "third wrapped Hangul caret y advances by line height");
    require(near(carets[4].bounds.y, 54.0f), "fifth wrapped Hangul caret y advances by two line heights");
    require(engine.last_diagnostics().caret_rects[0].cluster_index == 0, "first wrapped caret records first cluster");
    require(engine.last_diagnostics().caret_rects[2].cluster_index == 1, "third wrapped caret records second cluster");
    require(engine.last_diagnostics().caret_rects[4].cluster_index == 2, "fifth wrapped caret records third cluster");
    require(engine.last_diagnostics().caret_rects[0].line_index == 0, "first wrapped caret records first line");
    require(engine.last_diagnostics().caret_rects[2].line_index == 1, "third wrapped caret records second line");
    require(engine.last_diagnostics().caret_rects[4].line_index == 2, "fifth wrapped caret records third line");
}

void test_fake_glyph_clusters_track_utf8_runs_and_font_faces()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A\xed\x95\x9c", .style_token = "body"},
        render_text_run{.text = "e\xcc\x81", .style_token = "caption"},
    };
    request.bounds = render_rect{2.0f, 3.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 4, "glyph cluster fixture emits decoded glyph scalars");
    require(engine.last_diagnostics().has_glyph_clusters(), "layout records glyph cluster diagnostics");

    const std::vector<render_text_glyph_cluster>& clusters = engine.last_diagnostics().glyph_clusters;
    require(clusters.size() == 3, "glyph clusters combine ASCII base with combining mark");

    require(clusters[0].run_index == 0, "first glyph cluster records first run");
    require(clusters[0].byte_offset == 0 && clusters[0].byte_count == 1, "first glyph cluster tracks ASCII bytes");
    require(clusters[0].glyph_offset == 0 && clusters[0].glyph_count == 1, "first glyph cluster tracks glyph range");
    require(near(clusters[0].advance, 10.0f), "first glyph cluster tracks ASCII advance");
    require(near(clusters[0].baseline, 27.0f), "first glyph cluster records line baseline");
    require(clusters[0].line_index == 0, "first glyph cluster records line index");
    require(clusters[0].resolved_face_id == 1, "first glyph cluster records resolved Sans face");

    require(clusters[1].run_index == 0, "Hangul glyph cluster records first run");
    require(clusters[1].byte_offset == 1 && clusters[1].byte_count == 3, "Hangul glyph cluster tracks UTF-8 bytes");
    require(clusters[1].glyph_offset == 1 && clusters[1].glyph_count == 1, "Hangul glyph cluster tracks glyph range");
    require(near(clusters[1].advance, 20.0f), "Hangul glyph cluster tracks full-width advance");
    require(near(clusters[1].baseline, 27.0f), "Hangul glyph cluster keeps first line baseline");
    require(clusters[1].line_index == 0, "Hangul glyph cluster remains on first line");
    require(clusters[1].resolved_face_id == 1, "Hangul glyph cluster records body resolved face");

    require(clusters[2].run_index == 1, "combining glyph cluster records second run");
    require(clusters[2].byte_offset == 0 && clusters[2].byte_count == 3, "combining glyph cluster spans base bytes");
    require(clusters[2].glyph_offset == 2 && clusters[2].glyph_count == 2, "combining glyph cluster spans two glyphs");
    require(near(clusters[2].advance, 6.0f), "combining glyph cluster keeps base advance");
    require(near(clusters[2].baseline, 27.0f), "combining glyph cluster shares first line baseline");
    require(clusters[2].line_index == 0, "combining glyph cluster remains on first line");
    require(clusters[2].resolved_face_id == 3, "combining glyph cluster records resolved Serif face");
}

void test_fake_glyph_clusters_track_wrapped_hangul_lines()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xed\x95\x9c\xea\xb8\x80\xeb\x82\xa0", .style_token = "body"},
    };
    request.bounds = render_rect{3.0f, 5.0f, 25.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::word,
        .alignment = render_text_alignment::end,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 3, "wrapped Hangul cluster fixture emits one glyph per syllable");

    const std::vector<render_text_glyph_cluster>& clusters = engine.last_diagnostics().glyph_clusters;
    require(clusters.size() == 3, "wrapped Hangul records one cluster per syllable");

    require(clusters[0].byte_offset == 0 && clusters[0].byte_count == 3, "first wrapped cluster tracks bytes");
    require(clusters[1].byte_offset == 3 && clusters[1].byte_count == 3, "second wrapped cluster tracks bytes");
    require(clusters[2].byte_offset == 6 && clusters[2].byte_count == 3, "third wrapped cluster tracks bytes");
    require(clusters[0].glyph_offset == 0 && clusters[1].glyph_offset == 1, "wrapped clusters track glyph offsets");
    require(clusters[2].glyph_offset == 2, "final wrapped cluster tracks glyph offset");
    require(clusters[0].glyph_count == 1 && clusters[1].glyph_count == 1, "wrapped clusters keep one glyph each");
    require(clusters[2].glyph_count == 1, "final wrapped cluster keeps one glyph");
    require(near(clusters[0].advance, 20.0f), "first wrapped cluster tracks advance");
    require(near(clusters[1].advance, 20.0f), "second wrapped cluster tracks advance");
    require(near(clusters[2].advance, 20.0f), "third wrapped cluster tracks advance");
    require(clusters[0].line_index == 0, "first wrapped cluster records first line");
    require(clusters[1].line_index == 1, "second wrapped cluster records second line");
    require(clusters[2].line_index == 2, "third wrapped cluster records third line");
    require(near(clusters[0].baseline, 29.0f), "first wrapped cluster records baseline");
    require(near(clusters[1].baseline, 53.0f), "second wrapped cluster records baseline");
    require(near(clusters[2].baseline, 77.0f), "third wrapped cluster records baseline");
    require(clusters[0].resolved_face_id == 1, "first wrapped cluster records resolved face");
    require(clusters[1].resolved_face_id == 1, "second wrapped cluster records resolved face");
    require(clusters[2].resolved_face_id == 1, "third wrapped cluster records resolved face");
}

void test_fake_shaping_diagnostics_track_utf8_clusters_and_wrap_decisions()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xed\x95\x9c \xea\xb8\x80", .style_token = "body"},
    };
    request.bounds = render_rect{4.0f, 6.0f, 25.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::word,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 2, "shaping diagnostics fixture wraps two Hangul syllables");
    require(engine.last_diagnostics().has_utf8_clusters(), "shaping diagnostics record UTF-8 clusters");
    require(engine.last_diagnostics().utf8_clusters.size() == 3, "shaping diagnostics record Hangul space Hangul clusters");
    require(engine.last_diagnostics().utf8_clusters[0].run_index == 0, "first UTF-8 cluster records run");
    require(engine.last_diagnostics().utf8_clusters[0].byte_offset == 0, "first UTF-8 cluster records byte offset");
    require(engine.last_diagnostics().utf8_clusters[0].byte_count == 3, "first UTF-8 cluster records Hangul bytes");
    require(engine.last_diagnostics().utf8_clusters[0].codepoint_offset == 0, "first UTF-8 cluster records scalar offset");
    require(engine.last_diagnostics().utf8_clusters[0].codepoint_count == 1, "first UTF-8 cluster records scalar count");
    require(engine.last_diagnostics().utf8_clusters[0].valid, "first UTF-8 cluster is valid");
    require(engine.last_diagnostics().utf8_clusters[1].byte_offset == 3, "space UTF-8 cluster records separator byte");
    require(engine.last_diagnostics().utf8_clusters[1].byte_count == 1, "space UTF-8 cluster records one byte");
    require(engine.last_diagnostics().utf8_clusters[2].byte_offset == 4, "second Hangul UTF-8 cluster records byte offset");
    require(engine.last_diagnostics().utf8_clusters[2].byte_count == 3, "second Hangul UTF-8 cluster records bytes");

    require(engine.last_diagnostics().has_line_breaks(), "shaping diagnostics record line breaks");
    require(engine.last_diagnostics().line_breaks.size() == 2, "wrapped fixture records two break decisions");
    const render_text_line_break_snapshot& first_break = engine.last_diagnostics().line_breaks[0];
    const render_text_line_break_snapshot& second_break = engine.last_diagnostics().line_breaks[1];
    require(first_break.break_reason == utf8_line_break_reason::ascii_whitespace, "first break records whitespace wrap");
    require(first_break.wrapped, "first break records wrapped decision");
    require(first_break.line_index == 0, "first break records first line");
    require(first_break.utf8_cluster_offset == 0, "first break records starting UTF-8 cluster");
    require(first_break.utf8_cluster_count == 1, "first break records line UTF-8 cluster count");
    require(first_break.start_run_index == 0 && first_break.start_byte_offset == 0, "first break records start byte");
    require(first_break.end_run_index == 0 && first_break.end_byte_offset == 3, "first break records content end");
    require(
        first_break.separator_run_index == 0 && first_break.separator_byte_offset == 3,
        "first break records separator byte");
    require(first_break.separator_byte_count == 1, "first break records separator length");
    require(near(first_break.line_width, 20.0f), "first break records line width");
    require(near(first_break.max_width, 25.0f), "first break records max width");
    require(first_break.caret_safe, "first whitespace break stays caret safe");
    require(first_break.starts_at_utf8_cluster_boundary, "first break starts at a UTF-8 cluster boundary");
    require(first_break.ends_at_utf8_cluster_boundary, "first break ends at a UTF-8 cluster boundary");
    require(!first_break.used_hangul_width_break, "first whitespace break is not a Hangul width break");
    require(!first_break.used_long_token_fallback, "first whitespace break is not a long-token fallback");

    require(second_break.break_reason == utf8_line_break_reason::end_of_text, "second break records end of text");
    require(!second_break.wrapped, "second break is not a wrap decision");
    require(second_break.line_index == 1, "second break records second line");
    require(second_break.utf8_cluster_offset == 2, "second break records second visible UTF-8 cluster offset");
    require(second_break.utf8_cluster_count == 1, "second break records one UTF-8 cluster");
    require(second_break.start_run_index == 0 && second_break.start_byte_offset == 4, "second break records start byte");
    require(second_break.end_run_index == 0 && second_break.end_byte_offset == 7, "second break records end byte");
    require(near(second_break.line_width, 20.0f), "second break records line width");
    require(second_break.caret_safe, "second break stays caret safe");

    require(engine.last_diagnostics().has_line_metrics(), "shaping diagnostics record line metrics");
    require(engine.last_diagnostics().line_metrics.size() == 2, "wrapped fixture records two line metrics");
    require(engine.last_diagnostics().line_layout_metrics.produced_line_count == 2, "line metrics record produced lines");
    require(engine.last_diagnostics().line_layout_metrics.visible_line_count == 2, "line metrics record visible lines");
    require(engine.last_diagnostics().line_layout_metrics.truncated_line_count == 0, "wrapped fixture is not truncated");
    require(!engine.last_diagnostics().line_layout_metrics.overflowed, "wrapped fixture does not overflow");
    require(near(engine.last_diagnostics().line_metrics[0].width, 20.0f), "first line metric records width");
    require(near(engine.last_diagnostics().line_metrics[1].width, 20.0f), "second line metric records width");
    require(engine.last_diagnostics().line_metrics[0].caret_safe, "first line metric records caret safety");
    require(engine.last_diagnostics().line_metrics[0].caret_stop_count == 2, "first line metric records caret stops");
    require(engine.last_diagnostics().has_line_break_policy(), "shaping diagnostics record line-break policy summary");
    require(engine.last_diagnostics().line_break_policy.break_count == 2, "line-break policy counts all fragments");
    require(
        engine.last_diagnostics().line_break_policy.ascii_whitespace_break_count == 1,
        "line-break policy counts whitespace breaks");
    require(engine.last_diagnostics().line_break_policy.caret_safe_break_count == 2, "line-break policy counts safe breaks");
    require(engine.last_diagnostics().line_break_policy.unsafe_break_count == 0, "line-break policy records no unsafe breaks");
}

void test_fake_line_metrics_track_overflow_and_truncation()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "ABC\nD\nE", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 15.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 2,
    };

    const render_text_layout layout = engine.layout_text(request);
    require(near(layout.measure.width, 30.0f), "truncated layout still measures visible overflow width");
    require(near(layout.measure.height, 48.0f), "truncated layout measures visible lines only");
    require(layout.glyphs.size() == 4, "truncated layout emits only visible line glyphs");

    require(engine.last_diagnostics().line_breaks.size() == 3, "line metrics fixture records all line breaks");
    require(engine.last_diagnostics().line_metrics.size() == 3, "line metrics fixture records all produced lines");
    require(engine.last_diagnostics().line_layout_metrics.produced_line_count == 3, "line metrics count produced lines");
    require(engine.last_diagnostics().line_layout_metrics.visible_line_count == 2, "line metrics count visible lines");
    require(engine.last_diagnostics().line_layout_metrics.truncated_line_count == 1, "line metrics count truncated lines");
    require(engine.last_diagnostics().line_layout_metrics.overflow_line_count == 1, "line metrics count overflow lines");
    require(engine.last_diagnostics().line_layout_metrics.truncated, "line metrics mark layout truncated");
    require(engine.last_diagnostics().line_layout_metrics.overflowed, "line metrics mark layout overflowed");

    const render_text_line_metrics_snapshot& first_line = engine.last_diagnostics().line_metrics[0];
    const render_text_line_metrics_snapshot& second_line = engine.last_diagnostics().line_metrics[1];
    const render_text_line_metrics_snapshot& third_line = engine.last_diagnostics().line_metrics[2];
    require(near(first_line.width, 30.0f), "first line metric records overflowing width");
    require(first_line.overflowed, "first line metric records overflow");
    require(near(first_line.overflow_width, 15.0f), "first line metric records overflow amount");
    require(!first_line.truncated, "first visible line is not truncated");
    require(near(second_line.width, 10.0f), "second line metric records width");
    require(!second_line.overflowed, "second line metric does not overflow");
    require(!second_line.truncated, "second visible line is not truncated");
    require(near(third_line.width, 10.0f), "third line metric records hidden line width");
    require(third_line.truncated, "third line metric records truncation");
}

void test_fake_line_break_policy_keeps_combining_clusters_caret_safe()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "Ae\xcc\x81" "B", .style_token = "body"},
    };
    request.bounds = render_rect{2.0f, 4.0f, 15.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::word,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    require(near(layout.measure.width, 10.0f), "long-token fallback measures one caret-safe cluster per line");
    require(near(layout.measure.height, 72.0f), "long-token fallback wraps into three lines");
    require(layout.glyphs.size() == 4, "long-token fallback keeps combining glyph in layout stream");
    require(layout.glyphs[2].glyph_id == 0x0301, "combining mark glyph remains attached to second line");
    require(near(layout.glyphs[2].bounds.width, 0.0f), "combining mark keeps zero advance");
    require(near(layout.glyphs[3].bounds.y, 52.0f), "final glyph moves after combining-cluster line");

    const std::vector<render_text_line_break_snapshot>& breaks = engine.last_diagnostics().line_breaks;
    require(breaks.size() == 3, "long-token fallback records three line fragments");
    require(breaks[0].used_long_token_fallback, "first narrow ASCII break records long-token fallback");
    require(breaks[0].utf8_cluster_offset == 0, "first narrow ASCII break records first cluster offset");
    require(breaks[0].utf8_cluster_count == 1, "first narrow ASCII break records one cluster");
    require(breaks[0].caret_safe, "first narrow ASCII break stays caret safe");
    require(breaks[1].used_long_token_fallback, "second narrow ASCII break records long-token fallback");
    require(breaks[1].codepoint_count == 2, "combining-cluster line spans base and combining scalars");
    require(breaks[1].utf8_cluster_offset == 1, "combining-cluster line records second cluster offset");
    require(breaks[1].utf8_cluster_count == 1, "combining-cluster line counts one UTF-8 cluster");
    require(breaks[1].caret_safe, "combining-cluster line does not split inside the cluster");
    require(breaks[2].break_reason == utf8_line_break_reason::end_of_text, "final narrow ASCII fragment ends text");
    require(breaks[2].utf8_cluster_offset == 2, "final narrow ASCII fragment records final cluster offset");

    require(engine.last_diagnostics().line_break_policy.width_pressure_break_count == 2, "policy counts width breaks");
    require(
        engine.last_diagnostics().line_break_policy.long_token_fallback_break_count == 2,
        "policy counts long-token fallback breaks");
    require(engine.last_diagnostics().line_break_policy.hangul_width_break_count == 0, "policy keeps Hangul count separate");
    require(engine.last_diagnostics().line_break_policy.unsafe_break_count == 0, "policy records no unsafe breaks");

    const std::vector<render_text_line_metrics_snapshot>& metrics = engine.last_diagnostics().line_metrics;
    require(metrics.size() == 3, "long-token fallback records three line metrics");
    require(metrics[1].start_byte_offset == 1, "combining line metric starts at base byte");
    require(metrics[1].end_byte_offset == 4, "combining line metric ends after combining bytes");
    require(metrics[1].caret_stop_count == 2, "combining line metric records start and end caret stops");
    require(metrics[1].caret_safe, "combining line metric records caret-safe cluster boundaries");
}

void test_fake_utf8_hangul_uses_codepoints()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xed\x95\x9c\xea\xb8\x80", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_measure measure = engine.measure_text(request);
    require(near(measure.width, 40.0f), "Hangul text measures by full-width codepoint");
    require(near(measure.height, 24.0f), "Hangul text uses body line height");

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 2, "Hangul UTF-8 emits one glyph per codepoint");
    require(layout.glyphs[0].glyph_id == 0xd55c, "first Hangul glyph id is Unicode scalar");
    require(layout.glyphs[1].glyph_id == 0xae00, "second Hangul glyph id is Unicode scalar");
    require(layout.glyphs[0].byte_offset == 0, "first Hangul byte offset is stable");
    require(layout.glyphs[1].byte_offset == 3, "second Hangul byte offset advances by UTF-8 width");
    require(near(layout.glyphs[1].bounds.x, 20.0f), "second Hangul glyph advances by full width");
}

void test_fake_utf8_handles_wide_combining_and_invalid_sequences()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xf0\x9f\x99\x82" "e" "\xcc\x81" "\xc0\xaf" "\xe2\x82", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_measure measure = engine.measure_text(request);
    require(near(measure.width, 70.0f), "UTF-8 edge cases produce deterministic width");
    require(near(measure.height, 24.0f), "UTF-8 edge cases use body line height");
    require(engine.last_diagnostics().saw_invalid_utf8(), "invalid UTF-8 measure records diagnostics");
    require(
        engine.last_diagnostics().invalid_utf8_sequence_count == 4,
        "invalid UTF-8 measure records each replacement sequence");

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 7, "UTF-8 edge cases emit one glyph per decoded scalar or replacement");
    require(engine.last_diagnostics().saw_invalid_utf8(), "invalid UTF-8 layout records diagnostics");
    require(
        engine.last_diagnostics().invalid_utf8_sequence_count == 4,
        "invalid UTF-8 layout records each replacement sequence");
    require(layout.glyphs[0].glyph_id == 0x1f642, "four-byte UTF-8 decodes to Unicode scalar");
    require(layout.glyphs[0].byte_offset == 0, "four-byte scalar records starting byte");
    require(near(layout.glyphs[0].bounds.width, 20.0f), "wide emoji-like scalar uses full-width advance");

    require(layout.glyphs[1].glyph_id == 'e', "ASCII scalar after four-byte UTF-8 is decoded");
    require(layout.glyphs[1].byte_offset == 4, "ASCII scalar offset follows four-byte UTF-8");
    require(layout.glyphs[2].glyph_id == 0x0301, "combining mark scalar is decoded");
    require(layout.glyphs[2].byte_offset == 5, "combining mark offset follows ASCII byte");
    require(near(layout.glyphs[2].bounds.width, 0.0f), "combining mark has zero advance");
    require(near(layout.glyphs[3].bounds.x, 30.0f), "next glyph starts at combining mark anchor");

    for (std::size_t index = 3; index < layout.glyphs.size(); ++index) {
        require(layout.glyphs[index].glyph_id == 0xfffd, "invalid UTF-8 byte emits replacement glyph");
    }
    require(layout.glyphs[3].byte_offset == 7, "first invalid byte offset is stable");
    require(layout.glyphs[4].byte_offset == 8, "second invalid byte offset is stable");
    require(layout.glyphs[5].byte_offset == 9, "truncated leading byte offset is stable");
    require(layout.glyphs[6].byte_offset == 10, "truncated continuation byte offset is stable");
}

void test_fake_diagnostics_reset_after_clean_request()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xc0", .style_token = "missing"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    (void)engine.measure_text(request);
    require(engine.last_diagnostics().used_style_fallback(), "dirty request records style fallback");
    require(engine.last_diagnostics().saw_invalid_utf8(), "dirty request records invalid UTF-8");

    request.text_runs = {
        render_text_run{.text = "ok", .style_token = "body"},
    };
    (void)engine.layout_text(request);
    require(!engine.last_diagnostics().used_style_fallback(), "clean layout clears style fallback diagnostics");
    require(!engine.last_diagnostics().saw_invalid_utf8(), "clean layout clears invalid UTF-8 diagnostics");

    request.text_runs = {
        render_text_run{.text = "\xc0", .style_token = "missing"},
    };
    (void)engine.caret_positions(request);
    require(engine.last_diagnostics().used_style_fallback(), "dirty caret request records fallback diagnostics");
    require(engine.last_diagnostics().saw_invalid_utf8(), "dirty caret request records invalid UTF-8");

    request.text_runs = {
        render_text_run{.text = "ok", .style_token = "body"},
    };
    (void)engine.selection_rects(
        request,
        fake_text_engine_selection_range{
            .start_run_index = 0,
            .start_byte_offset = 0,
            .end_run_index = 0,
            .end_byte_offset = 2,
        });
    require(!engine.last_diagnostics().used_style_fallback(), "clean selection clears fallback diagnostics");
    require(!engine.last_diagnostics().saw_invalid_utf8(), "clean selection clears invalid UTF-8 diagnostics");
}

void test_fake_word_wraps_hangul_and_clips_max_lines()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xed\x95\x9c \xea\xb8\x80", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 25.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::word,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_measure wrapped_measure = engine.measure_text(request);
    require(near(wrapped_measure.width, 20.0f), "wrapped Hangul line width tracks widest line");
    require(near(wrapped_measure.height, 48.0f), "wrapped Hangul text measures two lines");

    const render_text_layout wrapped_layout = engine.layout_text(request);
    require(wrapped_layout.glyphs.size() == 2, "wrapped Hangul layout omits wrapping separator");
    require(near(wrapped_layout.glyphs[0].bounds.y, 0.0f), "first wrapped Hangul glyph remains on first line");
    require(near(wrapped_layout.glyphs[1].bounds.y, 24.0f), "second wrapped Hangul glyph moves to next line");

    request.options.max_lines = 1;
    const render_text_layout clipped_layout = engine.layout_text(request);
    require(near(clipped_layout.measure.height, 24.0f), "max lines clips measured height");
    require(clipped_layout.glyphs.size() == 1, "max lines clips glyph output");
}

void test_fake_line_break_layout_uses_utf8_helper_fragments()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A B", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout no_wrap_layout = engine.layout_text(request);
    require(near(no_wrap_layout.measure.width, 30.0f), "no-wrap helper layout keeps ASCII spaces in line width");
    require(near(no_wrap_layout.measure.height, 24.0f), "no-wrap helper layout keeps one line");
    require(no_wrap_layout.glyphs.size() == 3, "no-wrap helper layout emits visible space glyph");
    require(no_wrap_layout.glyphs[1].glyph_id == ' ', "no-wrap helper layout preserves space glyph");
    require(near(no_wrap_layout.glyphs[2].bounds.x, 20.0f), "no-wrap helper layout advances after space");

    request.bounds.width = 25.0f;
    request.options.wrap = render_text_wrap_mode::word;
    const render_text_layout wrapped_layout = engine.layout_text(request);
    require(near(wrapped_layout.measure.width, 10.0f), "word-wrap helper layout drops separator from measured lines");
    require(near(wrapped_layout.measure.height, 48.0f), "word-wrap helper layout breaks at ASCII whitespace");
    require(wrapped_layout.glyphs.size() == 2, "word-wrap helper layout omits wrapping separator glyph");
    require(wrapped_layout.glyphs[0].glyph_id == 'A', "word-wrap helper layout keeps first word glyph");
    require(wrapped_layout.glyphs[1].glyph_id == 'B', "word-wrap helper layout keeps second word glyph");
    require(near(wrapped_layout.glyphs[1].bounds.y, 24.0f), "word-wrap helper layout moves second word to next line");

    request.text_runs = {
        render_text_run{.text = "ABC", .style_token = "body"},
    };
    request.bounds.width = 15.0f;
    const render_text_layout unspaced_layout = engine.layout_text(request);
    require(near(unspaced_layout.measure.width, 10.0f), "helper layout falls back to cluster breaks for long ASCII");
    require(near(unspaced_layout.measure.height, 72.0f), "long ASCII fallback emits one glyph per line");
    require(unspaced_layout.glyphs.size() == 3, "long ASCII fallback still emits all glyphs");
    require(near(unspaced_layout.glyphs[0].bounds.y, 0.0f), "first long ASCII glyph uses first line");
    require(near(unspaced_layout.glyphs[1].bounds.y, 24.0f), "second long ASCII glyph wraps to second line");
    require(near(unspaced_layout.glyphs[2].bounds.y, 48.0f), "third long ASCII glyph wraps to third line");
    require(engine.last_diagnostics().line_break_policy.long_token_fallback_break_count == 2, "long ASCII fallback counts two breaks");
    require(engine.last_diagnostics().line_break_policy.unsafe_break_count == 0, "long ASCII fallback stays caret safe");
}

void test_fake_word_wraps_unspaced_hangul_syllables()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xed\x95\x9c\xea\xb8\x80\xeb\x82\xa0", .style_token = "body"},
    };
    request.bounds = render_rect{3.0f, 5.0f, 25.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::word,
        .alignment = render_text_alignment::end,
        .max_lines = 0,
    };

    const render_text_measure measure = engine.measure_text(request);
    require(near(measure.width, 20.0f), "unspaced Hangul wrap measures one syllable per line");
    require(near(measure.height, 72.0f), "unspaced Hangul wrap measures three lines");

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 3, "unspaced Hangul wrap emits three syllable glyphs");
    require(layout.glyphs[0].glyph_id == 0xd55c, "first unspaced Hangul glyph id is stable");
    require(layout.glyphs[1].glyph_id == 0xae00, "second unspaced Hangul glyph id is stable");
    require(layout.glyphs[2].glyph_id == 0xb0a0, "third unspaced Hangul glyph id is stable");
    require(layout.glyphs[0].byte_offset == 0, "first unspaced Hangul byte offset is stable");
    require(layout.glyphs[1].byte_offset == 3, "second unspaced Hangul byte offset is stable");
    require(layout.glyphs[2].byte_offset == 6, "third unspaced Hangul byte offset is stable");
    require(near(layout.glyphs[0].bounds.x, 8.0f), "first unspaced Hangul line end-aligns inside bounds");
    require(near(layout.glyphs[1].bounds.x, 8.0f), "second unspaced Hangul line end-aligns inside bounds");
    require(near(layout.glyphs[2].bounds.x, 8.0f), "third unspaced Hangul line end-aligns inside bounds");
    require(near(layout.glyphs[0].bounds.y, 5.0f), "first unspaced Hangul line uses request y origin");
    require(near(layout.glyphs[1].bounds.y, 29.0f), "second unspaced Hangul line advances by line height");
    require(near(layout.glyphs[2].bounds.y, 53.0f), "third unspaced Hangul line advances by two line heights");

    const std::vector<render_text_line_break_snapshot>& breaks = engine.last_diagnostics().line_breaks;
    require(breaks.size() == 3, "unspaced Hangul diagnostics record each line fragment");
    require(breaks[0].used_hangul_width_break, "first unspaced Hangul break records Hangul width policy");
    require(breaks[1].used_hangul_width_break, "second unspaced Hangul break records Hangul width policy");
    require(!breaks[0].used_long_token_fallback, "Hangul width break does not claim long-token fallback");
    require(breaks[0].utf8_cluster_offset == 0 && breaks[0].utf8_cluster_count == 1, "first Hangul break records cluster range");
    require(breaks[1].utf8_cluster_offset == 1 && breaks[1].utf8_cluster_count == 1, "second Hangul break records cluster range");
    require(breaks[2].utf8_cluster_offset == 2 && breaks[2].utf8_cluster_count == 1, "final Hangul break records cluster range");
    require(breaks[0].caret_safe && breaks[1].caret_safe && breaks[2].caret_safe, "Hangul breaks stay caret safe");
    require(
        engine.last_diagnostics().line_break_policy.hangul_width_break_count == 2,
        "line-break policy counts Hangul width breaks");
    require(
        engine.last_diagnostics().line_break_policy.long_token_fallback_break_count == 0,
        "line-break policy keeps long-token count clear for Hangul");
    require(engine.last_diagnostics().line_break_policy.caret_safe_break_count == 3, "line-break policy counts safe Hangul fragments");
}

void test_scene_text_metrics_adapter_feeds_layout_placer()
{
    using namespace quiz_vulkan::scene;

    recording_text_engine engine;
    render_text_metrics metrics(
        engine,
        make_style_catalog(),
        quiz_vulkan::render::render_text_options{
            .wrap = quiz_vulkan::render::render_text_wrap_mode::word,
            .alignment = quiz_vulkan::render::render_text_alignment::start,
            .max_lines = 2,
        });

    scene_layout_data scene("text");
    scene_layout_rule root_rule;
    root_rule.mode = scene_layout_mode::vertical;
    require(scene.set_bounds_rule(scene.root_node_id(), root_rule), "root layout rule is set");

    scene_node_data label;
    label.id = "label";
    label.kind = scene_node_kind::text;
    label.layout_rule.horizontal_alignment = scene_alignment::start;
    label.style.token = "body";
    label.text_runs.push_back(scene_text_run{.text = "Hi", .style_token = {}});
    require(scene.append_node("", label), "text node is appended");

    const placed_scene placed = layout_placer().place(scene, scene_rect{0.0f, 0.0f, 200.0f, 80.0f}, metrics);
    const placed_scene_node* placed_label = placed.find_node("label");
    require(placed_label != nullptr, "layout placer emits text node");
    require(near(placed_label->bounds.width, 20.0f), "layout placer uses adapter-measured text width");
    require(near(placed_label->bounds.height, 24.0f), "layout placer uses adapter-measured text height");

    require(!engine.measure_requests.empty(), "adapter sent measure request to render text engine");
    const quiz_vulkan::render::render_text_request& first_request = engine.measure_requests.front();
    require(first_request.text_runs.size() == 1, "adapter forwards text runs");
    require(first_request.text_runs.front().style_token == "body", "adapter fills empty run token from scene style");
    require(near(first_request.bounds.width, 200.0f), "adapter forwards max width in render bounds");
    require(near(first_request.bounds.height, 0.0f), "adapter leaves measurement bounds height at zero");
}

} // namespace

int main()
{
    test_utf8_text_run_iterates_codepoints_and_replacements();
    test_utf8_text_run_clusters_ascii_combining_and_hangul();
    test_utf8_line_breaks_ascii_whitespace_and_newlines();
    test_utf8_line_breaks_crlf_as_single_explicit_newline();
    test_utf8_line_breaks_hangul_only_under_width_pressure();
    test_utf8_line_break_keeps_unspaced_ascii_words_together_under_width_pressure();
    test_utf8_line_break_can_fallback_split_long_tokens_on_width();
    test_style_catalog_find_and_resolve();
    test_font_face_catalog_resolves_exact_faces_and_fallback();
    test_font_face_catalog_reports_codepoint_fallback_diagnostics();
    test_font_face_catalog_prefers_known_coverage_fallbacks();
    test_deterministic_fake_font_resolver_reports_face_fallbacks();
    test_glyph_atlas_cache_allocates_rows_pages_and_cached_slots();
    test_glyph_atlas_cache_consumes_dirty_page_updates_by_revision();
    test_fake_measure_and_layout_emit_stable_glyphs();
    test_fake_multirun_layout_tracks_lines_offsets_and_alignment();
    test_fake_newline_edge_cases_preserve_empty_line_height();
    test_fake_style_fallback_shapes_missing_tokens();
    test_fake_font_resolver_records_family_and_style_fallbacks();
    test_fake_font_resolution_policy_tracks_codepoint_fallbacks_and_cache_readiness();
    test_fake_atlas_updates_are_revisioned_and_consumed();
    test_fake_glyph_atlas_page_diagnostics_track_overflow();
    test_fake_caret_positions_follow_utf8_runs_and_combining_marks();
    test_fake_selection_rects_cover_utf8_ranges_without_atlas_updates();
    test_fake_caret_and_selection_clip_to_request_height();
    test_fake_selection_rects_follow_wrapped_hangul_lines();
    test_fake_caret_positions_follow_wrapped_hangul_lines();
    test_fake_glyph_clusters_track_utf8_runs_and_font_faces();
    test_fake_glyph_clusters_track_wrapped_hangul_lines();
    test_fake_shaping_diagnostics_track_utf8_clusters_and_wrap_decisions();
    test_fake_line_metrics_track_overflow_and_truncation();
    test_fake_line_break_policy_keeps_combining_clusters_caret_safe();
    test_fake_utf8_hangul_uses_codepoints();
    test_fake_utf8_handles_wide_combining_and_invalid_sequences();
    test_fake_diagnostics_reset_after_clean_request();
    test_fake_word_wraps_hangul_and_clips_max_lines();
    test_fake_line_break_layout_uses_utf8_helper_fragments();
    test_fake_word_wraps_unspaced_hangul_syllables();
    test_scene_text_metrics_adapter_feeds_layout_placer();
    return 0;
}
