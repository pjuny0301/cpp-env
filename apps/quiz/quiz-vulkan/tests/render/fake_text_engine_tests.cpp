#include "core/layout/layout_placer.h"
#include "render/text/fake_text_engine.h"
#include "render/text/scene_text_metrics_adapter.h"
#include "render/text/text_engine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
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
    test_fake_utf8_hangul_uses_codepoints();
    test_fake_word_wraps_hangul_and_clips_max_lines();
    test_scene_text_metrics_adapter_feeds_layout_placer();
    return 0;
}
