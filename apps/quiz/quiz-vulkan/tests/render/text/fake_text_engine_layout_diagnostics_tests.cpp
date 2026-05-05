#include "render/text/fake_text_engine.h"

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
    require(near(first_line.baseline, 24.0f), "first line metric records baseline");
    require(near(first_line.ascent, 24.0f), "first line metric records ascent");
    require(near(first_line.descent, 0.0f), "first line metric records descent");
    require(near(second_line.width, 10.0f), "second line metric records width");
    require(!second_line.overflowed, "second line metric does not overflow");
    require(!second_line.truncated, "second visible line is not truncated");
    require(near(second_line.baseline, 48.0f), "second line metric records second baseline");
    require(near(third_line.width, 10.0f), "third line metric records hidden line width");
    require(third_line.truncated, "third line metric records truncation");
    require(engine.last_diagnostics().has_line_layout_policy(), "line metrics fixture records clipping policy");
    require(engine.last_diagnostics().line_layout_policy.clipped_line_count == 1, "line metrics fixture counts clipped lines");
    require(engine.last_diagnostics().line_layout_policy.clipped_glyph_count == 1, "line metrics fixture counts clipped glyphs");
    require(!engine.last_diagnostics().line_layout_policy.ellipsis_applied, "line metrics fixture keeps ellipsis disabled");
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

} // namespace

int main()
{
    test_fake_glyph_clusters_track_utf8_runs_and_font_faces();
    test_fake_glyph_clusters_track_wrapped_hangul_lines();
    test_fake_shaping_diagnostics_track_utf8_clusters_and_wrap_decisions();
    test_fake_line_metrics_track_overflow_and_truncation();
    test_fake_line_break_policy_keeps_combining_clusters_caret_safe();
    return 0;
}
