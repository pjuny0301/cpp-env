#include "core/layout/layout_placer.h"
#include "render/text/fake_text_engine.h"
#include "render/text/scene_text_metrics_adapter.h"
#include "render/text/text_engine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
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

    std::vector<render_text_atlas_update> first_updates = engine.consume_atlas_updates();
    require(first_updates.size() == 1, "first new glyph enqueues one atlas update");
    require(first_updates[0].page.id == 1, "atlas update page id is stable");
    require(first_updates[0].page.revision == 1, "first atlas update uses revision one");
    require(first_updates[0].rgba.size() == 4, "fake atlas update carries one RGBA pixel");
    require(engine.consume_atlas_updates().empty(), "atlas updates are consumed once");

    const render_text_layout repeated_layout = engine.layout_text(request);
    require(repeated_layout.glyphs[0].atlas_revision == 1, "cached glyph keeps atlas revision");
    require(engine.consume_atlas_updates().empty(), "cached glyph does not enqueue atlas update");

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
    require(carets.size() == 6, "caret positions include run starts and scalar boundaries");
    require(engine.consume_atlas_updates().empty(), "caret positions do not enqueue atlas updates");

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
    require(carets[4].run_index == 1 && carets[4].byte_offset == 1, "fifth caret follows second run ASCII");
    require(near(carets[4].bounds.x, 38.0f), "fifth caret advances by caption width");
    require(carets[5].run_index == 1 && carets[5].byte_offset == 3, "sixth caret follows combining mark bytes");
    require(near(carets[5].bounds.x, 38.0f), "combining mark caret keeps previous x");
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
    test_style_catalog_find_and_resolve();
    test_fake_measure_and_layout_emit_stable_glyphs();
    test_fake_multirun_layout_tracks_lines_offsets_and_alignment();
    test_fake_style_fallback_shapes_missing_tokens();
    test_fake_atlas_updates_are_revisioned_and_consumed();
    test_fake_caret_positions_follow_utf8_runs_and_combining_marks();
    test_fake_selection_rects_cover_utf8_ranges_without_atlas_updates();
    test_fake_caret_and_selection_clip_to_request_height();
    test_fake_selection_rects_follow_wrapped_hangul_lines();
    test_fake_utf8_hangul_uses_codepoints();
    test_fake_utf8_handles_wide_combining_and_invalid_sequences();
    test_fake_word_wraps_hangul_and_clips_max_lines();
    test_fake_word_wraps_unspaced_hangul_syllables();
    test_scene_text_metrics_adapter_feeds_layout_placer();
    return 0;
}
