#include "render/text/fake_text_engine.h"

#include <cassert>
#include <cmath>
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

quiz_vulkan::render::render_text_font_backend_component freetype_component()
{
    using namespace quiz_vulkan::render;

    return render_text_font_backend_component{
        .library = render_text_font_backend_library::freetype,
        .name = "FreeType",
        .available = true,
        .version = render_text_font_backend_version{.major = 2, .minor = 13, .patch = 2},
        .features = {
            render_text_font_backend_feature::font_file_loading,
            render_text_font_backend_feature::unicode_cmap,
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_rasterization,
        },
        .diagnostic = "FreeType probe fixture is available",
    };
}

quiz_vulkan::render::render_text_font_backend_component harfbuzz_component()
{
    using namespace quiz_vulkan::render;

    return render_text_font_backend_component{
        .library = render_text_font_backend_library::harfbuzz,
        .name = "HarfBuzz",
        .available = true,
        .version = render_text_font_backend_version{.major = 8, .minor = 3, .patch = 0},
        .features = {
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_shaping,
            render_text_font_backend_feature::complex_script_shaping,
        },
        .diagnostic = "HarfBuzz probe fixture is available",
    };
}

quiz_vulkan::render::render_text_request make_single_run_request(const std::string& text)
{
    using namespace quiz_vulkan::render;

    render_text_request request;
    request.text_runs = {
        render_text_run{.text = text, .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };
    return request;
}

const quiz_vulkan::render::render_text_font_fallback_chain_entry_snapshot* find_fallback_chain_entry(
    const std::vector<quiz_vulkan::render::render_text_font_fallback_chain_entry_snapshot>& entries,
    const quiz_vulkan::render::font_face_id face_id)
{
    for (const quiz_vulkan::render::render_text_font_fallback_chain_entry_snapshot& entry : entries) {
        if (entry.face_id == face_id) {
            return &entry;
        }
    }
    return nullptr;
}

bool contains_backend_library(
    const std::vector<quiz_vulkan::render::render_text_font_backend_library>& libraries,
    const quiz_vulkan::render::render_text_font_backend_library library)
{
    for (const quiz_vulkan::render::render_text_font_backend_library candidate : libraries) {
        if (candidate == library) {
            return true;
        }
    }
    return false;
}

std::vector<quiz_vulkan::render::render_text_external_font_backend_probe_result> dependency_probe_results_for(
    const quiz_vulkan::render::fake_text_engine_diagnostics& diagnostics)
{
    return {
        diagnostics.font_backend_shaping_dependency,
        diagnostics.font_backend_rasterization_dependency,
        diagnostics.font_backend_unicode_dependency,
    };
}

void configure_mixed_script_fallback_chain_engine(quiz_vulkan::render::fake_text_engine& engine)
{
    using namespace quiz_vulkan::render;

    engine.set_font_backend_selection_candidates({
        make_render_text_harfbuzz_backend_candidate(true),
        make_render_text_freetype_backend_candidate(true),
        make_render_text_utf8proc_backend_candidate(true),
        make_render_text_deterministic_fake_backend_candidate(),
    });
    engine.add_font_face(font_face_descriptor{
        .id = 501,
        .family = "Primary Chain Sans",
        .source_uri = "fixture://fonts/primary-chain-sans",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'A'},
        },
        .weight = 400,
    });
    engine.add_font_face(font_face_descriptor{
        .id = 502,
        .family = "Hangul Chain Fallback",
        .source_uri = "fixture://fonts/hangul-chain",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{.first = 0xd55cU, .last = 0xd55cU},
        },
        .weight = 400,
        .fallback = true,
    });
    engine.add_font_face(font_face_descriptor{
        .id = 503,
        .family = "Emoji Chain Fallback",
        .source_uri = "fixture://fonts/emoji-chain",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{.first = 0x1f600U, .last = 0x1f600U},
        },
        .weight = 400,
        .fallback = true,
    });
}

quiz_vulkan::render::render_text_request make_mixed_script_fallback_chain_request()
{
    using namespace quiz_vulkan::render;

    render_text_style_catalog catalog = make_style_catalog();
    catalog.styles.push_back(render_text_style{
        .id = "mixed",
        .font_family = "Primary Chain Sans",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .font_weight = 400,
    });

    const std::string mixed_text = std::string("A") + std::string("\xed\x95\x9c", 3)
        + std::string("\xf0\x9f\x98\x80", 4);
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = mixed_text, .style_token = "mixed"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = catalog;
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };
    return request;
}

void require_mixed_script_fallback_chain_diagnostics(
    const quiz_vulkan::render::fake_text_engine_diagnostics& diagnostics)
{
    using namespace quiz_vulkan::render;

    require(diagnostics.has_font_fallback_chain_runs(), "diagnostics record fallback-chain run snapshots");
    require(diagnostics.has_font_fallback_chain_policy(), "diagnostics record fallback-chain policy");
    require(!diagnostics.has_font_fallback_chain_missing_glyphs(), "covered mixed fixture has no missing glyphs");
    require(diagnostics.font_fallback_chain_runs.size() == 1U, "one text run produces one fallback-chain run");
    require(diagnostics.font_fallback_chain_policy.run_count == 1U, "fallback-chain policy counts one run");
    require(diagnostics.font_fallback_chain_policy.codepoint_count == 3U, "fallback-chain policy counts scalars");
    require(
        diagnostics.font_fallback_chain_policy.supported_codepoint_count == 3U,
        "fallback-chain policy counts all mixed glyphs as supported");
    require(
        diagnostics.font_fallback_chain_policy.fallback_codepoint_count == 2U,
        "fallback-chain policy counts Hangul and emoji fallback selections");
    require(
        diagnostics.font_fallback_chain_policy.unique_selected_face_count == 3U,
        "fallback-chain policy records three selected faces");
    require(
        diagnostics.font_fallback_chain_selected_face_order.size() == 3U,
        "fallback-chain selected face order reaches diagnostics");
    require(diagnostics.font_fallback_chain_selected_face_order[0] == 501U, "Latin requested face is first");
    require(diagnostics.font_fallback_chain_selected_face_order[1] == 502U, "Hangul fallback face is second");
    require(diagnostics.font_fallback_chain_selected_face_order[2] == 503U, "emoji fallback face is third");
    require(
        diagnostics.font_fallback_chain_shaping_selection.selected.library
            == render_text_font_backend_library::harfbuzz,
        "fallback-chain diagnostics carry selected shaping backend");

    const render_text_font_fallback_chain_run_snapshot& run = diagnostics.font_fallback_chain_runs.front();
    require(run.style_token == "mixed", "fallback-chain run records style token");
    require(run.source_label == "fake_text_engine", "fallback-chain run records text-engine source");
    require(run.requested_face_id == 501U, "fallback-chain run records requested face");
    require(run.selected_face_ids.size() == 3U, "fallback-chain run records per-run selected faces");
    require(run.selected_face_ids[0] == 501U, "run selected order keeps requested face first");
    require(run.selected_face_ids[1] == 502U, "run selected order keeps Hangul fallback second");
    require(run.selected_face_ids[2] == 503U, "run selected order keeps emoji fallback third");
    require(run.coverage.segments.size() == 3U, "fallback-chain run preserves coverage segmentation");
    require(run.shaping_backend == render_text_font_backend_library::harfbuzz, "run records HarfBuzz shaping");
    require(!run.shaping_used_deterministic_fallback, "run does not claim deterministic shaping fallback");

    const render_text_font_fallback_chain_entry_snapshot* primary =
        find_fallback_chain_entry(run.entries, 501U);
    const render_text_font_fallback_chain_entry_snapshot* hangul =
        find_fallback_chain_entry(run.entries, 502U);
    const render_text_font_fallback_chain_entry_snapshot* emoji =
        find_fallback_chain_entry(run.entries, 503U);
    require(primary != nullptr, "fallback chain records requested face entry");
    require(hangul != nullptr, "fallback chain records Hangul fallback entry");
    require(emoji != nullptr, "fallback chain records emoji fallback entry");
    if (primary != nullptr && hangul != nullptr && emoji != nullptr) {
        require(primary->requested_face, "fallback chain marks requested face entry");
        require(hangul->fallback_face, "fallback chain marks Hangul fallback entry");
        require(emoji->fallback_face, "fallback chain marks emoji fallback entry");
        require(primary->selected_codepoint_count == 1U, "requested face selected for Latin");
        require(hangul->selected_codepoint_count == 1U, "Hangul fallback selected once");
        require(emoji->selected_codepoint_count == 1U, "emoji fallback selected once");
    }
}

quiz_vulkan::render::render_text_real_font_shaping_adapter_result mismatched_adapter_shape(
    const quiz_vulkan::render::render_text_real_font_shaping_adapter_request& request)
{
    quiz_vulkan::render::render_text_real_font_shaping_adapter_result result =
        quiz_vulkan::render::deterministic_fake_real_font_backend_shape(request);
    if (!result.glyphs.empty()) {
        ++result.glyphs.front().glyph_id;
    }
    return result;
}

quiz_vulkan::render::render_text_real_font_shaping_adapter_result recoverable_adapter_shape_failure(
    const quiz_vulkan::render::render_text_real_font_shaping_adapter_request& request)
{
    using namespace quiz_vulkan::render;

    render_text_real_font_shaping_adapter_result result{
        .status = render_text_font_backend_adapter_status::recoverable_backend_failure,
        .library = request.library,
        .capability_status = request.capability.status,
        .run_index = request.run_index,
        .style_token = request.style_token,
        .recoverable = true,
        .fatal = false,
        .diagnostic = "recoverable injected adapter shaping failure",
    };
    result.diagnostics.push_back(render_text_font_backend_adapter_diagnostic{
        .status = result.status,
        .library = request.library,
        .capability_status = request.capability.status,
        .run_index = request.run_index,
        .recoverable = true,
        .fatal = false,
        .diagnostic = result.diagnostic,
    });
    return result;
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

void test_fake_text_engine_records_rasterized_atlas_payloads_for_cacheable_glyphs()
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

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 1, "rasterized payload fixture lays out one glyph");
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();
    require(diagnostics.has_glyph_cache_readiness(), "rasterized payload fixture records cache readiness");
    require(
        diagnostics.has_rasterized_glyph_atlas_payloads(),
        "rasterized payload fixture records rasterizer atlas payloads");
    require(
        diagnostics.has_rasterized_glyph_atlas_payload_policy(),
        "rasterized payload fixture records rasterizer payload policy");
    require(diagnostics.has_glyph_id_resolutions(), "rasterized payload fixture records glyph id resolutions");
    require(
        diagnostics.has_glyph_id_resolution_policy(),
        "rasterized payload fixture records glyph id resolution policy");
    require(diagnostics.glyph_id_resolutions.size() == 1, "one cacheable glyph produces one glyph id resolution");
    const render_text_font_glyph_id_resolution_snapshot& glyph_id_resolution =
        diagnostics.glyph_id_resolutions.front();
    require(
        glyph_id_resolution.status == render_text_font_glyph_id_resolution_status::resolved,
        "cacheable glyph id resolves before shaping");
    require(glyph_id_resolution.glyph_id == U'A', "glyph id resolution maps Latin codepoint deterministically");
    require(glyph_id_resolution.glyph_supported, "glyph id resolution claims supported fixture glyph");
    require(
        diagnostics.glyph_id_resolution_policy.resolved_count == 1,
        "glyph id resolution policy counts resolved fixture glyph");
    require(diagnostics.rasterized_glyph_atlas_payloads.size() == 1, "one cacheable glyph produces one payload");

    const render_text_glyph_cache_readiness_snapshot& readiness = diagnostics.glyph_cache_readiness.front();
    const render_text_rasterized_glyph_atlas_payload_snapshot& payload =
        diagnostics.rasterized_glyph_atlas_payloads.front();
    require(readiness.glyph_id == glyph_id_resolution.glyph_id, "cache readiness uses resolved glyph id");
    require(payload.cluster_index == readiness.cluster_index, "payload records matching cluster index");
    require(payload.run_index == 0 && payload.byte_offset == 0, "payload records glyph run position");
    require(payload.glyph_id == glyph_id_resolution.glyph_id, "raster payload uses resolved glyph id");
    require(payload.cache_key == readiness.cache_key, "payload preserves cache readiness atlas key");
    require(payload.status == render_text_font_rasterizer_status::rasterized, "payload records rasterized status");
    require(payload.cacheable, "payload records cacheable glyph");
    require(payload.upload_ready, "payload is upload-ready for supported fixture glyph");
    require(!payload.skipped, "rasterized payload is not skipped");
    require(payload.bitmap_width == 8U && payload.bitmap_height == 8U, "payload records fake bitmap dimensions");
    require(payload.alpha_bytes == 64U, "payload records fake alpha bytes");
    require(payload.rgba_bytes == 256U, "payload records expanded RGBA bytes");
    require(payload.source_label == "fixture://fonts/sans-regular", "payload records source label");

    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.request_count == 1,
        "rasterizer payload policy counts request");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.rasterized_count == 1,
        "rasterizer payload policy counts rasterized glyph");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.upload_ready_count == 1,
        "rasterizer payload policy counts upload-ready payload");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.skipped_count == 0,
        "rasterizer payload policy records no skipped fixture glyphs");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.total_rgba_bytes == 256U,
        "rasterizer payload policy totals RGBA bytes");
    require(diagnostics.has_shaped_atlas_update_traces(), "rasterized payload fixture records shaped atlas traces");
    require(
        diagnostics.has_shaped_atlas_update_trace_policy(),
        "rasterized payload fixture records shaped atlas trace policy");
    require(
        diagnostics.has_glyph_atlas_materializations(),
        "rasterized payload fixture records atlas materialization diagnostics");
    require(
        diagnostics.has_glyph_atlas_materialization_policy(),
        "rasterized payload fixture records atlas materialization policy");
    require(diagnostics.glyph_atlas_materializations.size() == 1, "one cacheable glyph materializes once");
    const render_text_glyph_atlas_materialization_snapshot& materialization =
        diagnostics.glyph_atlas_materializations.front();
    require(
        materialization.status == render_text_glyph_atlas_materialization_status::materialized_upload_ready,
        "upload-ready payload materializes an atlas update request");
    require(materialization.materialized, "materialization marks upload-ready glyph as materialized");
    require(materialization.queued, "materialization records queued atlas update");
    require(materialization.has_layout_bounds, "materialization carries layout bounds");
    require(materialization.layout_bounds.width > 0.0f, "materialization layout bounds are populated");
    require(materialization.cache_key == readiness.cache_key, "materialization preserves cache readiness key");
    require(materialization.payload_rgba_bytes == 256U, "materialization records payload RGBA bytes");
    require(materialization.payload_byte_count_matches, "materialization validates payload byte count");
    require(
        materialization.shaping_font_backend_library == render_text_font_backend_library::deterministic_fake,
        "materialization records shaping backend metadata");
    require(
        materialization.raster_font_backend_library == render_text_font_backend_library::deterministic_fake,
        "materialization records raster backend metadata");
    require(
        diagnostics.glyph_atlas_materialization_policy.materialized_count == 1,
        "materialization policy counts materialized glyph");
    require(
        diagnostics.glyph_atlas_materialization_policy.upload_ready_count == 1,
        "materialization policy counts upload-ready request");
    require(
        diagnostics.glyph_atlas_materialization_policy.queued_atlas_update_bytes > 0U,
        "materialization policy counts queued atlas update bytes");
    require(diagnostics.has_atlas_upload_request_bridge(), "layout records atlas upload bridge diagnostics");
    require(
        diagnostics.atlas_upload_request_bridge.has_upload_requests(),
        "atlas upload bridge exposes render_text_atlas_update-style requests");
    require(
        diagnostics.atlas_upload_request_bridge.policy.upload_request_count == 1U,
        "atlas upload bridge policy counts queued request");
    require(
        diagnostics.atlas_upload_request_bridge.upload_requests.size() == 1U,
        "atlas upload bridge emits one upload request");
    require(
        diagnostics.atlas_upload_request_bridge.requests.front().status
            == render_text_atlas_upload_request_status::upload_ready,
        "atlas upload bridge marks cacheable glyph upload-ready");
    require(
        diagnostics.atlas_upload_request_bridge.requests.front().cache_key == materialization.cache_key,
        "atlas upload bridge preserves materialization cache key");
    require(
        diagnostics.atlas_upload_request_bridge.upload_requests.front().page.id == materialization.page.id,
        "atlas upload bridge preserves atlas page id");
    require(
        diagnostics.atlas_upload_request_bridge.upload_requests.front().updated_bounds.x
            == materialization.atlas_update_bounds.x,
        "atlas upload bridge preserves atlas update bounds");
    require(
        diagnostics.has_queued_atlas_upload_request_ids(),
        "layout queues stable atlas upload request ids");
    require(
        diagnostics.queued_atlas_upload_request_ids.size() == 1U,
        "layout queues one atlas upload request id");
    require(
        diagnostics.queued_atlas_upload_request_ids.front()
            == diagnostics.atlas_upload_request_bridge.requests.front().request_id,
        "queued atlas upload id matches bridge request id");
    require(diagnostics.has_text_frame_snapshot(), "layout records compact text frame snapshot");
    require(
        diagnostics.text_frame_snapshot.status == render_text_frame_snapshot_status::pending_atlas_updates,
        "text frame snapshot is pending before consume_atlas_updates");
    require(diagnostics.text_frame_snapshot.ok(), "pending text frame snapshot has no planning errors");
    require(
        diagnostics.text_frame_snapshot.policy.layout_request_count == 1U,
        "text frame snapshot carries batch layout request count");
    require(
        diagnostics.text_frame_snapshot.policy.materialization_count == 1U,
        "text frame snapshot carries materialization count");
    require(
        diagnostics.text_frame_snapshot.queued_atlas_upload_request_ids
            == diagnostics.queued_atlas_upload_request_ids,
        "text frame snapshot carries queued upload ids");
    require(
        diagnostics.text_frame_snapshot.atlas_uploads.front().request_id
            == diagnostics.queued_atlas_upload_request_ids.front(),
        "text frame snapshot upload entry matches queued id");

    const std::string queued_upload_request_id = diagnostics.queued_atlas_upload_request_ids.front();
    const render_text_atlas_page_id queued_page_id =
        diagnostics.atlas_upload_request_bridge.upload_requests.front().page.id;
    require(diagnostics.shaped_atlas_update_traces.size() == 1, "one cacheable glyph produces one shaped atlas trace");
    const render_text_shaped_atlas_update_trace_snapshot& trace = diagnostics.shaped_atlas_update_traces.front();
    require(
        trace.status == render_text_shaped_atlas_update_trace_status::upload_ready_payload_queued,
        "upload-ready payload is traced to queued atlas update");
    require(trace.queued, "upload-ready payload trace is queued");
    require(trace.has_cache_key, "upload-ready payload trace records cache key");
    require(trace.cache_key == readiness.cache_key, "upload-ready payload trace preserves cache readiness key");
    require(trace.shaped_glyph_ids.size() == 1 && trace.shaped_glyph_ids.front() == U'A', "trace records shaped glyph id");
    require(trace.resolved_face_id == readiness.resolved_face_id, "trace records resolved face id");
    require(trace.rasterizer_status == render_text_font_rasterizer_status::rasterized, "trace records rasterizer status");
    require(trace.payload_upload_ready, "trace records upload-ready payload");
    require(trace.payload_rgba_bytes == 256U, "trace records payload RGBA bytes");
    require(trace.payload_byte_count_matches, "trace validates payload byte count");
    require(trace.has_atlas_placement, "trace records atlas placement");
    require(trace.has_atlas_update, "trace records queued atlas update");
    require(trace.atlas_update_rgba_bytes > 0U, "trace records queued atlas update bytes");
    require(
        diagnostics.shaped_atlas_update_trace_policy.upload_ready_payload_queued_count == 1,
        "trace policy counts queued payload");
    require(
        diagnostics.shaped_atlas_update_trace_policy.traced_shaped_glyph_count == 1,
        "trace policy counts shaped glyph id");

    const std::vector<render_text_atlas_update> consumed_updates = engine.consume_atlas_updates();
    require(consumed_updates.size() == 1U, "consume returns queued atlas update");
    require(consumed_updates.front().page.id == queued_page_id, "consumed update page matches bridge page");
    require(
        engine.last_diagnostics().consumed_atlas_update_count == consumed_updates.size(),
        "consume records returned atlas update count");
    require(
        engine.last_diagnostics().has_consumed_atlas_upload_request_ids(),
        "consume records consumed atlas upload request ids");
    require(
        engine.last_diagnostics().consumed_atlas_upload_request_ids.size() == consumed_updates.size(),
        "consume records one id per returned atlas update");
    require(
        engine.last_diagnostics().consumed_atlas_upload_request_ids.front() == queued_upload_request_id,
        "consumed atlas upload id matches queued bridge id");
    require(
        engine.last_diagnostics().text_frame_snapshot.status == render_text_frame_snapshot_status::ready,
        "consume marks text frame snapshot ready");
    require(
        engine.last_diagnostics().text_frame_snapshot.ready_for_renderer(),
        "consumed text frame snapshot reports renderer readiness");
    require(
        engine.last_diagnostics().text_frame_snapshot.consumed_atlas_upload_request_ids.front()
            == queued_upload_request_id,
        "consumed text frame snapshot carries matching upload id");
    require(
        engine.last_diagnostics().text_frame_snapshot.atlas_uploads.front().consumed,
        "consumed text frame snapshot marks upload entry consumed");
}

void test_fake_text_engine_records_fallback_only_backend_capability_for_latin_hangul()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    const render_text_layout layout = engine.layout_text(make_single_run_request("A" "\xed\x95\x9c"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 2U, "fallback-only backend keeps Latin and Hangul layout glyphs");
    require(layout.glyphs[0].glyph_id == U'A', "fallback-only backend keeps Latin glyph id");
    require(layout.glyphs[1].glyph_id == 0xd55cU, "fallback-only backend keeps Hangul glyph id");
    require(diagnostics.has_font_backend_capability(), "fallback-only layout records backend capability");
    require(diagnostics.has_font_backend_selection(), "fallback-only layout records backend selection");
    require(
        diagnostics.font_backend_shaping_selection.status == render_text_font_backend_selection_status::fallback_selected,
        "default shaping selection records deterministic fallback");
    require(
        diagnostics.font_backend_shaping_selection.selected.library
            == render_text_font_backend_library::deterministic_fake,
        "default shaping selection identifies deterministic fake backend");
    require(
        diagnostics.font_backend_rasterization_selection.status
            == render_text_font_backend_selection_status::fallback_selected,
        "default raster selection records deterministic fallback");
    require(
        diagnostics.font_backend_unicode_selection.status
            == render_text_font_backend_selection_status::fallback_selected,
        "default unicode selection records deterministic fallback");
    require(
        diagnostics.font_backend_capability.status == render_text_font_backend_capability_status::fallback_only,
        "default fake engine records fallback-only backend mode");
    require(diagnostics.font_backend_capability.fallback_only, "default backend capability records fallback-only flag");
    require(diagnostics.font_backend_shaping_capability.backend_available, "fallback-only backend keeps shaping available");
    require(
        !diagnostics.font_backend_shaping_capability.support_complex_scripts,
        "fallback-only backend does not claim complex script support");
    require(diagnostics.font_backend_uses_deterministic_shaping, "fallback-only backend uses deterministic shaping");
    require(
        diagnostics.font_backend_uses_deterministic_rasterizer,
        "fallback-only backend uses deterministic rasterizer");
    require(diagnostics.shaped_glyphs.size() == 2U, "fallback-only backend still records shaped glyphs");
    require(diagnostics.shaped_glyphs[0].glyph_supported, "fallback-only Latin glyph remains supported");
    require(diagnostics.shaped_glyphs[1].glyph_supported, "fallback-only Hangul glyph remains supported");
    require(diagnostics.has_font_backend_run_selections(), "fallback-only layout records run backend selection");
    require(
        !diagnostics.has_font_backend_dependency_probe(),
        "default fake-only layout does not require external dependency probing");
    require(diagnostics.font_backend_run_selections.size() == 1U, "single run records one backend selection");
    const fake_text_engine_font_backend_run_selection_snapshot& run_selection =
        diagnostics.font_backend_run_selections.front();
    require(
        run_selection.shaping.library == render_text_font_backend_library::deterministic_fake,
        "run shaping selection records deterministic fake backend");
    require(
        run_selection.rasterization.library == render_text_font_backend_library::deterministic_fake,
        "run raster selection records deterministic fake backend");
    require(
        run_selection.unicode_processing.library == render_text_font_backend_library::deterministic_fake,
        "run unicode selection records deterministic fake backend");
    require(run_selection.shaping.fake_only, "run shaping snapshot records fake-only path");
    require(!run_selection.shaping.dependency_probe_configured, "run shaping snapshot records no external probe");
    require(
        run_selection.shaping.dependency_status
            == render_text_font_backend_adapter_readiness_status::fallback_ready,
        "run shaping snapshot has fallback-ready deterministic path");
    require(
        run_selection.shaping.dependency_fallback_reason
            == render_text_font_backend_adapter_readiness_status::missing_dependency,
        "run shaping snapshot keeps a stable fallback reason enum");

    require(
        diagnostics.glyph_font_resolutions.front().font_backend_library
            == render_text_font_backend_library::deterministic_fake,
        "glyph font resolution records fallback shaping backend");
    require(
        diagnostics.glyph_font_resolutions.front().font_backend_used_deterministic_fallback,
        "glyph font resolution records deterministic fallback");
    require(
        diagnostics.glyph_cache_readiness.front().font_backend_library
            == render_text_font_backend_library::deterministic_fake,
        "glyph cache readiness records fallback shaping backend");
    require(
        diagnostics.glyph_cache_readiness.front().font_backend_used_deterministic_fallback,
        "glyph cache readiness records deterministic fallback");

    require(diagnostics.rasterized_glyph_atlas_payloads.size() == 2U, "fallback-only backend rasterizes supported glyphs");
    for (const render_text_rasterized_glyph_atlas_payload_snapshot& payload :
         diagnostics.rasterized_glyph_atlas_payloads) {
        require(
            payload.font_backend_capability_status == render_text_font_backend_capability_status::fallback_only,
            "raster payload records fallback-only backend mode");
        require(payload.font_backend_fallback_only, "raster payload records fallback-only backend flag");
        require(
            payload.font_backend_library == render_text_font_backend_library::deterministic_fake,
            "raster payload records deterministic fake raster backend");
        require(
            payload.font_backend_used_deterministic_fallback,
            "raster payload records deterministic raster fallback");
        require(
            payload.font_backend_supports_rasterization,
            "raster payload records fake rasterization capability");
        require(payload.uses_deterministic_rasterizer, "raster payload records deterministic fake rasterizer use");
    }
    require(
        diagnostics.shaped_atlas_update_traces.front().shaping_font_backend_library
            == render_text_font_backend_library::deterministic_fake,
        "atlas trace records fallback shaping backend");
    require(
        diagnostics.shaped_atlas_update_traces.front().raster_font_backend_library
            == render_text_font_backend_library::deterministic_fake,
        "atlas trace records fallback raster backend");
}

void test_fake_text_engine_dependency_probe_reports_missing_real_backend_fallback()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_dependency_manifest(
        make_render_text_known_external_font_backend_manifest(false, false, false));

    const render_text_layout layout = engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 1U, "missing real backend manifest keeps deterministic layout");
    require(diagnostics.has_font_backend_dependency_probe(), "dependency manifest records probe diagnostics");
    require(diagnostics.font_backend_dependency_policy.configured, "dependency policy records configured manifest");
    require(diagnostics.font_backend_dependency_policy.fake_only, "missing manifest keeps fake-only path selected");
    require(diagnostics.font_backend_dependency_policy.probe_count == 3U, "dependency probe checks three backend purposes");
    require(
        diagnostics.font_backend_dependency_policy.fallback_ready_count == 3U,
        "each backend purpose falls back deterministically");
    require(
        diagnostics.font_backend_dependency_policy.missing_dependency_count == 3U,
        "missing dependency reason is counted per backend purpose");
    require(
        diagnostics.font_backend_shaping_dependency.status
            == render_text_font_backend_adapter_readiness_status::fallback_ready,
        "shaping dependency remains fallback ready");
    require(
        diagnostics.font_backend_shaping_dependency.fallback_reason
            == render_text_font_backend_adapter_readiness_status::missing_dependency,
        "shaping dependency records missing HarfBuzz source");
    require(
        contains_backend_library(
            diagnostics.font_backend_shaping_dependency.missing_dependencies,
            render_text_font_backend_library::harfbuzz),
        "shaping dependency identifies HarfBuzz as missing");
    require(
        diagnostics.font_backend_rasterization_dependency.fallback_reason
            == render_text_font_backend_adapter_readiness_status::missing_dependency,
        "raster dependency records missing FreeType source");
    require(
        contains_backend_library(
            diagnostics.font_backend_rasterization_dependency.missing_dependencies,
            render_text_font_backend_library::freetype),
        "raster dependency identifies FreeType as missing");
    require(
        diagnostics.font_backend_unicode_dependency.fallback_reason
            == render_text_font_backend_adapter_readiness_status::missing_dependency,
        "unicode dependency records missing utf8proc source");
    require(
        contains_backend_library(
            diagnostics.font_backend_unicode_dependency.missing_dependencies,
            render_text_font_backend_library::utf8proc),
        "unicode dependency identifies utf8proc as missing");

    const fake_text_engine_font_backend_run_selection_snapshot& run_selection =
        diagnostics.font_backend_run_selections.front();
    require(run_selection.shaping.dependency_probe_configured, "run shaping records dependency probe");
    require(run_selection.shaping.fake_only, "run shaping records fake fallback selection");
    require(run_selection.shaping.dependency_fallback_ready, "run shaping records fallback-ready dependency path");
    require(
        run_selection.shaping.dependency_fallback_reason
            == render_text_font_backend_adapter_readiness_status::missing_dependency,
        "run shaping records missing dependency fallback reason");
}

void test_fake_text_engine_dependency_probe_reports_adapter_unavailable_fallback()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_dependency_manifest(
        make_render_text_known_external_font_backend_manifest(true, false, false));

    const render_text_layout layout = engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 1U, "unlinked adapter manifest keeps deterministic layout");
    require(diagnostics.has_font_backend_dependency_probe(), "unlinked manifest records probe diagnostics");
    require(diagnostics.font_backend_dependency_policy.fake_only, "unlinked adapter keeps fake path selected");
    require(
        diagnostics.font_backend_dependency_policy.adapter_unavailable_count == 3U,
        "adapter unavailable reason is counted per backend purpose");
    require(
        diagnostics.font_backend_shaping_dependency.fallback_reason
            == render_text_font_backend_adapter_readiness_status::adapter_unavailable,
        "shaping dependency records unavailable adapter symbols");
    require(
        contains_backend_library(
            diagnostics.font_backend_shaping_dependency.adapter_unavailable_dependencies,
            render_text_font_backend_library::harfbuzz),
        "shaping dependency identifies HarfBuzz adapter unavailable");
    require(
        diagnostics.font_backend_rasterization_dependency.fallback_reason
            == render_text_font_backend_adapter_readiness_status::adapter_unavailable,
        "raster dependency records unavailable FreeType adapter symbols");
    require(
        diagnostics.font_backend_unicode_dependency.fallback_reason
            == render_text_font_backend_adapter_readiness_status::adapter_unavailable,
        "unicode dependency records unavailable utf8proc adapter symbols");
}

void test_fake_text_engine_dependency_probe_reports_version_mismatch_fallback()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_dependency_manifest(
        make_render_text_known_external_font_backend_manifest(true, true, true));
    engine.set_font_backend_capability_probe_request(render_text_font_backend_capability_probe_request{
        .minimum_versions = {
            render_text_font_backend_minimum_version{
                .library = render_text_font_backend_library::harfbuzz,
                .version = render_text_font_backend_version{.major = 99, .minor = 0, .patch = 0},
            },
        },
    });

    const render_text_layout layout = engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 1U, "version mismatch manifest keeps deterministic fallback layout");
    require(diagnostics.has_font_backend_dependency_probe(), "version mismatch records dependency probe");
    require(diagnostics.font_backend_dependency_policy.fake_only, "version mismatch falls back to fake path");
    require(
        diagnostics.font_backend_dependency_policy.version_mismatch_count > 0U,
        "version mismatch reason is counted");
    require(
        diagnostics.font_backend_shaping_dependency.fallback_reason
            == render_text_font_backend_adapter_readiness_status::version_mismatch,
        "shaping dependency records HarfBuzz version mismatch");
    require(
        diagnostics.font_backend_shaping_dependency.requested_capability.status
            == render_text_font_backend_capability_status::version_mismatch,
        "shaping dependency preserves capability mismatch status");

    const fake_text_engine_font_backend_run_selection_snapshot& run_selection =
        diagnostics.font_backend_run_selections.front();
    require(run_selection.shaping.fake_only, "run shaping records version-mismatch fallback");
    require(
        run_selection.shaping.dependency_fallback_reason
            == render_text_font_backend_adapter_readiness_status::version_mismatch,
        "run shaping exposes version-mismatch fallback reason");
}

void test_fake_text_engine_dependency_probe_reports_adapter_ready_selection()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_dependency_manifest(
        make_render_text_known_external_font_backend_manifest(true, true, true));

    const render_text_layout layout = engine.layout_text(make_single_run_request("\xd8\xa7"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 1U, "adapter-ready dependency manifest supports complex script layout");
    require(diagnostics.has_font_backend_dependency_probe(), "adapter-ready manifest records dependency probe");
    require(!diagnostics.font_backend_dependency_policy.fake_only, "adapter-ready manifest leaves fake-only path");
    require(diagnostics.font_backend_dependency_policy.adapter_ready, "dependency policy records adapter-ready path");
    require(
        diagnostics.font_backend_dependency_policy.adapter_ready_count == 3U,
        "all three backend purposes are adapter ready");
    require(
        diagnostics.font_backend_dependency_policy.fallback_ready_count == 0U,
        "adapter-ready manifest does not need deterministic fallback");
    require(
        diagnostics.font_backend_shaping_dependency.status
            == render_text_font_backend_adapter_readiness_status::adapter_ready,
        "shaping dependency reports adapter ready");
    require(
        diagnostics.font_backend_shaping_selection.selected.library == render_text_font_backend_library::harfbuzz,
        "adapter-ready selection chooses HarfBuzz shaping");
    require(
        diagnostics.font_backend_rasterization_selection.selected.library == render_text_font_backend_library::freetype,
        "adapter-ready selection chooses FreeType rasterization");
    require(
        diagnostics.font_backend_unicode_selection.selected.library == render_text_font_backend_library::utf8proc,
        "adapter-ready selection chooses utf8proc unicode processing");

    const fake_text_engine_font_backend_run_selection_snapshot& run_selection =
        diagnostics.font_backend_run_selections.front();
    require(run_selection.shaping.dependency_probe_configured, "run shaping records configured probe");
    require(run_selection.shaping.dependency_adapter_ready, "run shaping records adapter-ready dependency path");
    require(!run_selection.shaping.fake_only, "run shaping is not fake-only when HarfBuzz is ready");
    require(
        run_selection.shaping.dependency_status
            == render_text_font_backend_adapter_readiness_status::adapter_ready,
        "run shaping exposes adapter-ready status");
    require(
        run_selection.rasterization.dependency_status
            == render_text_font_backend_adapter_readiness_status::adapter_ready,
        "run rasterization exposes adapter-ready status");
    require(
        run_selection.unicode_processing.dependency_status
            == render_text_font_backend_adapter_readiness_status::adapter_ready,
        "run unicode exposes adapter-ready status");
    require(
        !diagnostics.font_backend_uses_adapter_shaping,
        "dependency readiness does not imply a linked real adapter function table");
}

void test_fake_text_engine_dependency_probe_diff_compares_layout_snapshots()
{
    using namespace quiz_vulkan::render;

    fake_text_engine fallback_engine;
    fallback_engine.set_font_backend_dependency_manifest(
        make_render_text_known_external_font_backend_manifest(false, false, false));
    (void)fallback_engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics fallback_diagnostics = fallback_engine.last_diagnostics();

    fake_text_engine ready_engine;
    ready_engine.set_font_backend_dependency_manifest(
        make_render_text_known_external_font_backend_manifest(true, true, true));
    (void)ready_engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics ready_diagnostics = ready_engine.last_diagnostics();

    const render_text_external_font_backend_probe_diff_summary_snapshot summary =
        diff_render_text_external_font_backend_probe_results(
            dependency_probe_results_for(fallback_diagnostics),
            dependency_probe_results_for(ready_diagnostics));

    require(summary.has_changes(), "layout snapshot backend probe diff records changed stack");
    require(summary.changed_count == 3U, "layout snapshot backend probe diff compares all backend purposes");
    require(
        summary.adapter_ready_transition_count == 3U,
        "layout snapshot backend probe diff records adapter-ready transitions");
    require(
        summary.total_missing_dependency_delta == -3,
        "layout snapshot backend probe diff records missing dependency reduction");
    require(
        summary.diffs.front().before.fake_only,
        "layout snapshot backend probe diff records before fake-only shaping");
    require(
        summary.diffs.front().before.unavailable,
        "layout snapshot backend probe diff records before unavailable shaping backend");
    require(
        summary.diffs.front().after.adapter_ready,
        "layout snapshot backend probe diff records after adapter-ready shaping backend");
    require(
        summary.diffs.front().after.selected_library == render_text_font_backend_library::harfbuzz,
        "layout snapshot backend probe diff records after HarfBuzz shaping selection");
}

void test_fake_text_engine_carries_injected_backend_selection_to_layout_diagnostics()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_selection_candidates({
        make_render_text_harfbuzz_backend_candidate(true),
        make_render_text_freetype_backend_candidate(true),
        make_render_text_utf8proc_backend_candidate(true),
        make_render_text_deterministic_fake_backend_candidate(),
    });

    const render_text_layout layout = engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 1U, "injected selection fixture lays out one Latin glyph");
    require(diagnostics.has_font_backend_selection(), "injected selection reaches top-level diagnostics");
    require(
        diagnostics.font_backend_capability.status == render_text_font_backend_capability_status::available,
        "injected real candidates drive an available capability snapshot");
    require(
        diagnostics.font_backend_shaping_selection.status == render_text_font_backend_selection_status::selected,
        "HarfBuzz is selected for shaping-capable requests");
    require(
        diagnostics.font_backend_shaping_selection.selected.library == render_text_font_backend_library::harfbuzz,
        "shaping selection records HarfBuzz");
    require(
        diagnostics.font_backend_rasterization_selection.status
            == render_text_font_backend_selection_status::selected,
        "FreeType is selected for raster-capable requests");
    require(
        diagnostics.font_backend_rasterization_selection.selected.library
            == render_text_font_backend_library::freetype,
        "raster selection records FreeType");
    require(
        diagnostics.font_backend_unicode_selection.status == render_text_font_backend_selection_status::selected,
        "utf8proc is selected for unicode-capable requests");
    require(
        diagnostics.font_backend_unicode_selection.selected.library == render_text_font_backend_library::utf8proc,
        "unicode selection records utf8proc");

    require(diagnostics.font_backend_run_selections.size() == 1U, "run selection is recorded for one run");
    const fake_text_engine_font_backend_run_selection_snapshot& run_selection =
        diagnostics.font_backend_run_selections.front();
    require(run_selection.shaping.library == render_text_font_backend_library::harfbuzz, "run records HarfBuzz");
    require(run_selection.rasterization.library == render_text_font_backend_library::freetype, "run records FreeType");
    require(
        run_selection.unicode_processing.library == render_text_font_backend_library::utf8proc,
        "run records utf8proc");
    require(!run_selection.shaping.used_deterministic_fallback, "run shaping is not marked as fake fallback");
    require(!run_selection.rasterization.used_deterministic_fallback, "run raster is not marked as fake fallback");

    const render_text_glyph_font_resolution_snapshot& glyph_resolution =
        diagnostics.glyph_font_resolutions.front();
    require(
        glyph_resolution.font_backend_library == render_text_font_backend_library::harfbuzz,
        "glyph font resolution records selected shaping backend");
    require(glyph_resolution.font_backend_label == "HarfBuzz", "glyph font resolution records backend label");
    require(
        glyph_resolution.font_backend_capability_status == render_text_font_backend_capability_status::available,
        "glyph font resolution records selected backend capability");
    require(
        !glyph_resolution.font_backend_used_deterministic_fallback,
        "glyph font resolution does not claim deterministic fallback");

    const render_text_glyph_cache_readiness_snapshot& readiness = diagnostics.glyph_cache_readiness.front();
    require(readiness.font_backend_library == render_text_font_backend_library::harfbuzz, "cache readiness records HarfBuzz");
    require(readiness.font_backend_label == "HarfBuzz", "cache readiness records shaping label");
    require(
        readiness.font_backend_capability_status == render_text_font_backend_capability_status::available,
        "cache readiness records shaping capability");
    require(!readiness.font_backend_used_deterministic_fallback, "cache readiness is not fake fallback");

    const render_text_rasterized_glyph_atlas_payload_snapshot& payload =
        diagnostics.rasterized_glyph_atlas_payloads.front();
    require(payload.font_backend_library == render_text_font_backend_library::freetype, "atlas payload records FreeType");
    require(payload.font_backend_label == "FreeType", "atlas payload records raster backend label");
    require(
        payload.font_backend_capability_status == render_text_font_backend_capability_status::available,
        "atlas payload records available capability");
    require(!payload.font_backend_used_deterministic_fallback, "atlas payload is not marked as fake fallback");
    require(payload.uses_deterministic_rasterizer, "fake engine still uses deterministic rasterizer implementation");

    const render_text_shaped_atlas_update_trace_snapshot& trace = diagnostics.shaped_atlas_update_traces.front();
    require(
        trace.shaping_font_backend_library == render_text_font_backend_library::harfbuzz,
        "atlas trace carries shaping backend selection");
    require(
        trace.raster_font_backend_library == render_text_font_backend_library::freetype,
        "atlas trace carries raster backend selection");
    require(trace.shaping_font_backend_label == "HarfBuzz", "atlas trace carries shaping backend label");
    require(trace.raster_font_backend_label == "FreeType", "atlas trace carries raster backend label");

    const render_text_glyph_atlas_materialization_snapshot& materialization =
        diagnostics.glyph_atlas_materializations.front();
    require(
        materialization.status == render_text_glyph_atlas_materialization_status::materialized_upload_ready,
        "injected selection materializes upload-ready payload");
    require(
        materialization.shaping_font_backend_library == render_text_font_backend_library::harfbuzz,
        "materialization carries injected shaping backend");
    require(
        materialization.raster_font_backend_library == render_text_font_backend_library::freetype,
        "materialization carries injected raster backend");
    require(materialization.shaping_font_backend_label == "HarfBuzz", "materialization carries shaping label");
    require(materialization.raster_font_backend_label == "FreeType", "materialization carries raster label");
    require(
        diagnostics.glyph_atlas_materialization_policy.real_backend_count == 1,
        "materialization policy counts real backend metadata");
}

void test_fake_text_engine_records_font_fallback_chain_for_mixed_script_layout()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_selection_candidates({
        make_render_text_harfbuzz_backend_candidate(true),
        make_render_text_freetype_backend_candidate(true),
        make_render_text_utf8proc_backend_candidate(true),
        make_render_text_deterministic_fake_backend_candidate(),
    });
    engine.add_font_face(font_face_descriptor{
        .id = 501,
        .family = "Primary Chain Sans",
        .source_uri = "fixture://fonts/primary-chain-sans",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'A'},
        },
        .weight = 400,
    });
    engine.add_font_face(font_face_descriptor{
        .id = 502,
        .family = "Hangul Chain Fallback",
        .source_uri = "fixture://fonts/hangul-chain",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{.first = 0xd55cU, .last = 0xd55cU},
        },
        .weight = 400,
        .fallback = true,
    });
    engine.add_font_face(font_face_descriptor{
        .id = 503,
        .family = "Emoji Chain Fallback",
        .source_uri = "fixture://fonts/emoji-chain",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{.first = 0x1f600U, .last = 0x1f600U},
        },
        .weight = 400,
        .fallback = true,
    });

    render_text_style_catalog catalog = make_style_catalog();
    catalog.styles.push_back(render_text_style{
        .id = "mixed",
        .font_family = "Primary Chain Sans",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .font_weight = 400,
    });

    const std::string mixed_text = std::string("A") + std::string("\xed\x95\x9c", 3)
        + std::string("\xf0\x9f\x98\x80", 4);
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = mixed_text, .style_token = "mixed"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = catalog;
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 3U, "mixed fallback chain fixture lays out Latin, Hangul, and emoji glyphs");
    require(diagnostics.has_font_fallback_chain_runs(), "layout records fallback-chain run snapshots");
    require(diagnostics.has_font_fallback_chain_policy(), "layout records fallback-chain policy");
    require(!diagnostics.has_font_fallback_chain_missing_glyphs(), "covered mixed fixture has no missing glyphs");
    require(diagnostics.has_font_backend_selection(), "layout still records backend selection");
    require(diagnostics.has_glyph_id_resolutions(), "layout still records glyph-id diagnostics");
    require(diagnostics.has_glyph_atlas_materializations(), "layout still records atlas materialization diagnostics");
    require(diagnostics.has_shaped_atlas_update_traces(), "layout still records shaped-atlas update traces");
    require(diagnostics.has_line_metrics(), "layout still records line metrics");

    require(diagnostics.font_fallback_chain_runs.size() == 1U, "one text run produces one fallback-chain run");
    require(diagnostics.font_fallback_chain_policy.run_count == 1U, "fallback-chain policy counts one run");
    require(diagnostics.font_fallback_chain_policy.codepoint_count == 3U, "fallback-chain policy counts scalars");
    require(
        diagnostics.font_fallback_chain_policy.supported_codepoint_count == 3U,
        "fallback-chain policy counts all mixed glyphs as supported");
    require(
        diagnostics.font_fallback_chain_policy.fallback_codepoint_count == 2U,
        "fallback-chain policy counts Hangul and emoji fallback selections");
    require(
        diagnostics.font_fallback_chain_policy.unique_selected_face_count == 3U,
        "fallback-chain policy records three selected faces");
    require(
        diagnostics.font_fallback_chain_selected_face_order.size() == 3U,
        "fallback-chain selected face order reaches diagnostics");
    require(diagnostics.font_fallback_chain_selected_face_order[0] == 501U, "Latin requested face is first");
    require(diagnostics.font_fallback_chain_selected_face_order[1] == 502U, "Hangul fallback face is second");
    require(diagnostics.font_fallback_chain_selected_face_order[2] == 503U, "emoji fallback face is third");
    require(
        diagnostics.font_fallback_chain_shaping_selection.selected.library
            == render_text_font_backend_library::harfbuzz,
        "fallback-chain diagnostics carry selected shaping backend");

    const render_text_font_fallback_chain_run_snapshot& run = diagnostics.font_fallback_chain_runs.front();
    require(run.style_token == "mixed", "fallback-chain run records style token");
    require(run.source_label == "fake_text_engine", "fallback-chain run records text-engine source");
    require(run.requested_face_id == 501U, "fallback-chain run records requested face");
    require(run.selected_face_ids.size() == 3U, "fallback-chain run records per-run selected faces");
    require(run.selected_face_ids[0] == 501U, "run selected order keeps requested face first");
    require(run.selected_face_ids[1] == 502U, "run selected order keeps Hangul fallback second");
    require(run.selected_face_ids[2] == 503U, "run selected order keeps emoji fallback third");
    require(run.coverage.segments.size() == 3U, "fallback-chain run preserves coverage segmentation");
    require(run.shaping_backend == render_text_font_backend_library::harfbuzz, "run records HarfBuzz shaping");
    require(!run.shaping_used_deterministic_fallback, "run does not claim deterministic shaping fallback");

    const render_text_font_fallback_chain_entry_snapshot* primary =
        find_fallback_chain_entry(run.entries, 501U);
    const render_text_font_fallback_chain_entry_snapshot* hangul =
        find_fallback_chain_entry(run.entries, 502U);
    const render_text_font_fallback_chain_entry_snapshot* emoji =
        find_fallback_chain_entry(run.entries, 503U);
    require(primary != nullptr, "fallback chain records requested face entry");
    require(hangul != nullptr, "fallback chain records Hangul fallback entry");
    require(emoji != nullptr, "fallback chain records emoji fallback entry");
    if (primary != nullptr && hangul != nullptr && emoji != nullptr) {
        require(primary->requested_face, "fallback chain marks requested face entry");
        require(hangul->fallback_face, "fallback chain marks Hangul fallback entry");
        require(emoji->fallback_face, "fallback chain marks emoji fallback entry");
        require(primary->selected_codepoint_count == 1U, "requested face selected for Latin");
        require(hangul->selected_codepoint_count == 1U, "Hangul fallback selected once");
        require(emoji->selected_codepoint_count == 1U, "emoji fallback selected once");
    }
}

void test_fake_text_engine_records_font_fallback_chain_for_mixed_script_carets()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    configure_mixed_script_fallback_chain_engine(engine);
    const render_text_request request = make_mixed_script_fallback_chain_request();

    const std::vector<fake_text_engine_caret> carets = engine.caret_positions(request);
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(!carets.empty(), "mixed-script caret helper returns caret positions");
    require(diagnostics.has_caret_hit_tests(), "mixed-script caret helper records caret diagnostics");
    require(
        diagnostics.caret_hit_tests.size() == carets.size(),
        "mixed-script caret diagnostics mirror returned carets");
    require(diagnostics.has_font_backend_selection(), "caret helper keeps backend selection diagnostics");
    require(diagnostics.has_glyph_id_resolutions(), "caret helper keeps glyph-id diagnostics");
    require(!diagnostics.has_glyph_atlas_materializations(), "caret helper does not materialize atlas payloads");
    require(!diagnostics.has_shaped_atlas_update_traces(), "caret helper does not queue shaped atlas traces");
    require_mixed_script_fallback_chain_diagnostics(diagnostics);
}

void test_fake_text_engine_records_font_fallback_chain_for_mixed_script_selection()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    configure_mixed_script_fallback_chain_engine(engine);
    const render_text_request request = make_mixed_script_fallback_chain_request();

    const std::vector<render_rect> rects = engine.selection_rects(
        request,
        fake_text_engine_selection_range{
            .start_run_index = 0,
            .start_byte_offset = 1,
            .end_run_index = 0,
            .end_byte_offset = request.text_runs.front().text.size(),
        });
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(!rects.empty(), "mixed-script selection helper returns selection rectangles");
    require(diagnostics.has_selection_rects(), "mixed-script selection helper records selection diagnostics");
    require(
        diagnostics.selection_rects.size() == rects.size(),
        "mixed-script selection diagnostics mirror returned rects");
    require(
        diagnostics.selection_rects.front().cluster_count >= 2U,
        "mixed-script selection spans Hangul and emoji clusters");
    require(diagnostics.has_font_backend_selection(), "selection helper keeps backend selection diagnostics");
    require(diagnostics.has_glyph_id_resolutions(), "selection helper keeps glyph-id diagnostics");
    require(!diagnostics.has_glyph_atlas_materializations(), "selection helper does not materialize atlas payloads");
    require(!diagnostics.has_shaped_atlas_update_traces(), "selection helper does not queue shaped atlas traces");
    require_mixed_script_fallback_chain_diagnostics(diagnostics);
}

void test_fake_text_engine_records_unavailable_backend_capability()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_capability_components({
        render_text_font_backend_component{
            .library = render_text_font_backend_library::directwrite,
            .name = "DirectWrite",
            .available = false,
            .version = render_text_font_backend_version{.major = 0, .minor = 0, .patch = 0},
            .features = {
                render_text_font_backend_feature::glyph_shaping,
                render_text_font_backend_feature::complex_script_shaping,
            },
            .diagnostic = "DirectWrite is unavailable in this deterministic fixture",
        },
    });

    const render_text_layout layout = engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.empty(), "unavailable backend emits no layout glyphs");
    require(diagnostics.has_font_backend_capability(), "unavailable backend records capability diagnostics");
    require(
        diagnostics.font_backend_capability.status == render_text_font_backend_capability_status::unavailable,
        "unavailable backend mode is recorded");
    require(
        !diagnostics.font_backend_shaping_capability.backend_available,
        "unavailable backend disables shaping");
    require(
        diagnostics.font_shaping_policy.backend_unavailable_count == 1U,
        "unavailable backend is counted by shaping diagnostics");
    require(diagnostics.shaped_glyphs.empty(), "unavailable backend records no shaped glyphs");
    require(
        diagnostics.rasterized_glyph_atlas_payloads.empty(),
        "unavailable backend records no raster payloads because no glyphs were shaped");
}

void test_fake_text_engine_keeps_complex_script_on_fallback_until_backend_supports_it()
{
    using namespace quiz_vulkan::render;

    fake_text_engine fallback_engine;
    const render_text_layout fallback_layout =
        fallback_engine.layout_text(make_single_run_request("\xd8\xa7"));
    const fake_text_engine_diagnostics& fallback_diagnostics = fallback_engine.last_diagnostics();

    require(fallback_layout.glyphs.size() == 1U, "fallback-only complex script keeps diagnostic glyph");
    require(
        fallback_diagnostics.font_backend_capability.status == render_text_font_backend_capability_status::fallback_only,
        "complex script fallback fixture starts in fallback-only mode");
    require(
        !fallback_diagnostics.font_backend_shaping_capability.support_complex_scripts,
        "fallback-only mode does not support complex scripts");
    require(
        fallback_diagnostics.shaped_glyphs.front().glyph_id == 0U,
        "fallback-only complex script uses backend fallback glyph id");
    require(
        !fallback_diagnostics.shaped_glyphs.front().glyph_supported,
        "fallback-only complex script does not claim glyph support");
    require(
        fallback_diagnostics.rasterized_glyph_atlas_payloads.front().font_backend_fallback_only,
        "fallback-only skipped payload records backend mode");

    fake_text_engine supported_engine;
    supported_engine.set_font_backend_capability_components({
        freetype_component(),
        harfbuzz_component(),
    });
    const render_text_layout supported_layout =
        supported_engine.layout_text(make_single_run_request("\xd8\xa7"));
    const fake_text_engine_diagnostics& supported_diagnostics = supported_engine.last_diagnostics();

    require(supported_layout.glyphs.size() == 1U, "backend-supported complex script lays out one glyph");
    require(
        supported_diagnostics.font_backend_capability.status == render_text_font_backend_capability_status::available,
        "backend-supported complex script records available backend mode");
    require(
        supported_diagnostics.font_backend_shaping_capability.support_complex_scripts,
        "backend-supported complex script enables complex shaping flag");
    require(
        supported_diagnostics.shaped_glyphs.front().glyph_id == 0x0627U,
        "backend-supported complex script keeps resolved glyph id");
    require(
        supported_diagnostics.shaped_glyphs.front().glyph_supported,
        "backend-supported complex script claims glyph support");
    require(
        !supported_diagnostics.shaped_glyphs.front().used_fallback_glyph_id,
        "backend-supported complex script avoids fallback glyph id");
    require(
        supported_diagnostics.rasterized_glyph_atlas_payloads.front().status
            == render_text_font_rasterizer_status::rasterized,
        "backend-supported complex script still produces deterministic raster payload");
    require(
        supported_diagnostics.rasterized_glyph_atlas_payloads.front().font_backend_capability_status
            == render_text_font_backend_capability_status::available,
        "backend-supported raster payload records available backend mode");
    require(
        !supported_diagnostics.rasterized_glyph_atlas_payloads.front().font_backend_fallback_only,
        "backend-supported raster payload does not claim fallback-only mode");
    require(
        supported_diagnostics.rasterized_glyph_atlas_payloads.front().uses_deterministic_rasterizer,
        "backend-supported fake engine still records deterministic rasterizer use");
}

void test_fake_text_engine_uses_default_deterministic_shaping_without_adapter()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    const render_text_layout layout = engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 1U, "default fake engine lays out deterministic Latin glyph");
    require(diagnostics.font_backend_uses_deterministic_shaping, "default fake engine uses deterministic shaping");
    require(!diagnostics.font_backend_uses_adapter_shaping, "default fake engine does not use adapter shaping");
    require(
        !diagnostics.font_backend_adapter_policy.configured,
        "default fake engine has no adapter configured");
    require(
        diagnostics.font_backend_adapter_policy.shaping_request_count == 0U,
        "default fake engine makes no adapter shaping requests");
    require(
        !diagnostics.has_font_backend_adapter_diagnostics(),
        "default fake engine records no adapter diagnostics");
}

void test_fake_text_engine_injected_adapter_unavailable_drives_shaping_diagnostics()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_capability_components({
        render_text_font_backend_component{
            .library = render_text_font_backend_library::directwrite,
            .name = "DirectWrite",
            .available = false,
            .version = render_text_font_backend_version{.major = 0, .minor = 0, .patch = 0},
            .features = {
                render_text_font_backend_feature::glyph_shaping,
                render_text_font_backend_feature::complex_script_shaping,
            },
            .diagnostic = "DirectWrite is unavailable in this deterministic fixture",
        },
    });
    engine.set_font_backend_adapter_functions(render_text_font_backend_adapter_functions{
        .shape = deterministic_fake_real_font_backend_shape,
        .rasterize = deterministic_fake_real_font_backend_rasterize,
        .label = "injected unavailable adapter",
    });

    const render_text_layout layout = engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.empty(), "unavailable injected adapter emits no layout glyphs");
    require(!diagnostics.font_backend_uses_deterministic_shaping, "injected adapter replaces deterministic shaping");
    require(diagnostics.font_backend_uses_adapter_shaping, "injected adapter path is recorded");
    require(diagnostics.font_backend_adapter_policy.configured, "adapter configuration is recorded");
    require(
        diagnostics.font_backend_adapter_policy.used_for_shaping,
        "adapter shaping use is recorded");
    require(
        diagnostics.font_backend_adapter_policy.backend_unavailable_count == 1U,
        "adapter unavailable status is counted");
    require(
        diagnostics.font_backend_adapter_diagnostics.front().status
            == render_text_font_backend_adapter_status::backend_unavailable,
        "adapter unavailable diagnostic is preserved");
    require(
        diagnostics.font_backend_adapter_diagnostics.front().capability_status
            == render_text_font_backend_capability_status::unavailable,
        "adapter unavailable diagnostic preserves capability status");
    require(
        diagnostics.font_shaping_policy.backend_unavailable_count == 1U,
        "adapter unavailable status drives shaping diagnostics");
    require(diagnostics.shaped_glyphs.empty(), "adapter unavailable produces no shaped glyphs");
}

void test_fake_text_engine_injected_adapter_shapes_available_complex_script()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_capability_components({
        freetype_component(),
        harfbuzz_component(),
    });
    engine.set_font_backend_adapter_functions(render_text_font_backend_adapter_functions{
        .shape = deterministic_fake_real_font_backend_shape,
        .rasterize = deterministic_fake_real_font_backend_rasterize,
        .label = "injected available adapter",
    });

    const render_text_layout layout = engine.layout_text(make_single_run_request("\xd8\xa7"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 1U, "available injected adapter lays out complex script glyph");
    require(!diagnostics.font_backend_uses_deterministic_shaping, "available adapter replaces deterministic shaping");
    require(diagnostics.font_backend_uses_adapter_shaping, "available adapter shaping path is recorded");
    require(
        diagnostics.font_backend_adapter_policy.shaping_request_count == 1U,
        "available adapter records one shaping request");
    require(
        diagnostics.font_backend_adapter_policy.shaped_count == 1U,
        "available adapter records shaped result");
    require(
        diagnostics.font_backend_adapter_diagnostics.front().status == render_text_font_backend_adapter_status::shaped,
        "available adapter records shaped diagnostic");
    require(
        diagnostics.font_shaping_policy.shaped_run_count == 1U,
        "available adapter shaped result drives shaping policy");
    require(diagnostics.shaped_glyphs.front().codepoint == 0x0627U, "adapter shaped glyph keeps Arabic codepoint");
    require(diagnostics.shaped_glyphs.front().glyph_id == 0x0627U, "adapter shaped glyph keeps resolved glyph id");
    require(diagnostics.shaped_glyphs.front().glyph_supported, "adapter shaped glyph claims support");
    require(!diagnostics.shaped_glyphs.front().used_fallback_glyph_id, "adapter shaped glyph avoids fallback id");
}

void test_fake_text_engine_injected_adapter_glyph_mismatch_drives_failure_diagnostics()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_capability_components({
        freetype_component(),
        harfbuzz_component(),
    });
    engine.set_font_backend_adapter_functions(render_text_font_backend_adapter_functions{
        .shape = mismatched_adapter_shape,
        .rasterize = deterministic_fake_real_font_backend_rasterize,
        .label = "injected mismatch adapter",
    });

    const render_text_layout layout = engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.empty(), "glyph id mismatch prevents untrusted adapter glyphs from layout");
    require(diagnostics.font_backend_uses_adapter_shaping, "mismatch adapter path is recorded");
    require(
        diagnostics.font_backend_adapter_policy.glyph_id_mismatch_count == 1U,
        "glyph id mismatch is counted by adapter policy");
    require(
        diagnostics.font_backend_adapter_diagnostics.back().status
            == render_text_font_backend_adapter_status::glyph_id_mismatch,
        "glyph id mismatch adapter diagnostic is preserved");
    require(
        diagnostics.font_backend_adapter_diagnostics.back().expected_glyph_id == U'A',
        "glyph id mismatch diagnostic records expected glyph id");
    require(
        diagnostics.font_backend_adapter_diagnostics.back().actual_glyph_id == U'A' + 1U,
        "glyph id mismatch diagnostic records actual glyph id");
    require(
        diagnostics.font_shaping_policy.unsupported_glyph_count == 1U,
        "glyph id mismatch drives shaping unsupported glyph diagnostic");
    require(diagnostics.shaped_glyphs.empty(), "glyph id mismatch records no trusted shaped glyphs");
}

void test_fake_text_engine_injected_adapter_failure_drives_shaping_diagnostics()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.set_font_backend_capability_components({
        freetype_component(),
        harfbuzz_component(),
    });
    engine.set_font_backend_adapter_functions(render_text_font_backend_adapter_functions{
        .shape = recoverable_adapter_shape_failure,
        .rasterize = deterministic_fake_real_font_backend_rasterize,
        .label = "injected recoverable failure adapter",
    });

    const render_text_layout layout = engine.layout_text(make_single_run_request("A"));
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.empty(), "recoverable adapter failure emits no layout glyphs");
    require(
        diagnostics.font_backend_adapter_policy.recoverable_failure_count == 1U,
        "recoverable adapter failure is counted");
    require(
        diagnostics.font_backend_adapter_diagnostics.front().status
            == render_text_font_backend_adapter_status::recoverable_backend_failure,
        "recoverable adapter failure diagnostic is preserved");
    require(
        diagnostics.font_shaping_policy.backend_unavailable_count == 1U,
        "recoverable adapter failure drives shaping backend diagnostic");
    require(
        diagnostics.font_shaping_diagnostics.front().diagnostic
            == "recoverable injected adapter shaping failure",
        "recoverable adapter failure message is visible through shaping diagnostics");
}

void test_fake_text_engine_wires_resolved_glyph_id_through_atlas_payloads()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.add_font_face(font_face_descriptor{
        .id = 82,
        .family = "Mapped Sans",
        .source_uri = "fixture://fonts/mapped-sans",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'A'},
        },
        .weight = 400,
        .italic = false,
        .glyph_id_offset = 1000,
    });

    render_text_style_catalog catalog = make_style_catalog();
    catalog.styles.push_back(render_text_style{
        .id = "mapped",
        .font_family = "Mapped Sans",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .font_weight = 400,
    });

    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A", .style_token = "mapped"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = catalog;
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();
    require(layout.glyphs.size() == 1, "mapped glyph fixture lays out one glyph");
    require(layout.glyphs.front().glyph_id == 1065U, "layout uses resolved glyph id instead of codepoint");

    const render_text_font_glyph_id_resolution_snapshot& glyph_id = diagnostics.glyph_id_resolutions.front();
    require(glyph_id.codepoint == U'A', "glyph id diagnostic keeps source codepoint");
    require(glyph_id.glyph_id == 1065U, "glyph id diagnostic records mapped glyph id");
    require(glyph_id.glyph_id_offset == 1000U, "glyph id diagnostic records face-local offset");
    require(!glyph_id.glyph_id_matches_codepoint, "glyph id diagnostic proves id differs from codepoint");
    require(!glyph_id.used_fallback_glyph_id, "mapped glyph id is not a fallback glyph id");

    const render_text_shaped_glyph& shaped = diagnostics.shaped_glyphs.front();
    require(shaped.codepoint == U'A', "shaped glyph keeps source codepoint");
    require(shaped.glyph_id == glyph_id.glyph_id, "shaped glyph consumes resolver glyph id");
    require(shaped.glyph_id_from_selection, "shaped glyph records selection-sourced glyph id");
    require(!shaped.glyph_id_matches_codepoint, "shaped glyph records non-codepoint glyph id");
    require(shaped.glyph_id_offset == 1000U, "shaped glyph preserves glyph id offset");

    const render_text_glyph_cache_readiness_snapshot& readiness = diagnostics.glyph_cache_readiness.front();
    require(readiness.codepoint == U'A', "cache readiness keeps source codepoint");
    require(readiness.glyph_id == glyph_id.glyph_id, "cache readiness uses resolver glyph id");
    require(readiness.cache_key.glyph_id == glyph_id.glyph_id, "atlas key uses resolver glyph id");
    require(readiness.glyph_id_from_selection, "cache readiness records selection-sourced glyph id");
    require(!readiness.glyph_id_matches_codepoint, "cache readiness records non-codepoint glyph id");

    const render_text_glyph_atlas_placement_snapshot& placement = diagnostics.glyph_atlas_placements.front();
    require(placement.key == readiness.cache_key, "atlas placement uses readiness cache key");
    require(placement.key.glyph_id == glyph_id.glyph_id, "atlas placement key uses resolver glyph id");

    const render_text_rasterized_glyph_atlas_payload_snapshot& payload =
        diagnostics.rasterized_glyph_atlas_payloads.front();
    require(payload.status == render_text_font_rasterizer_status::rasterized, "mapped glyph rasterizes");
    require(payload.codepoint == U'A', "raster payload keeps source codepoint for coverage checks");
    require(payload.glyph_id == glyph_id.glyph_id, "raster payload uses resolver glyph id");
    require(payload.cache_key == readiness.cache_key, "raster payload keeps atlas key");
    require(payload.glyph_id_from_selection, "raster payload records selection-sourced glyph id");
    require(!payload.glyph_id_matches_codepoint, "raster payload records non-codepoint glyph id");

    const render_text_shaped_atlas_update_trace_snapshot& trace = diagnostics.shaped_atlas_update_traces.front();
    require(trace.status == render_text_shaped_atlas_update_trace_status::upload_ready_payload_queued, "mapped glyph queues upload trace");
    require(trace.codepoint == U'A', "trace keeps source codepoint");
    require(trace.resolved_glyph_id == glyph_id.glyph_id, "trace records resolver glyph id");
    require(trace.shaped_glyph_ids.size() == 1U && trace.shaped_glyph_ids.front() == glyph_id.glyph_id, "trace shaped ids use resolver glyph id");
    require(trace.cache_key.glyph_id == glyph_id.glyph_id, "trace cache key uses resolver glyph id");
    require(trace.cache_key_matches_resolved_glyph_id, "trace proves cache key matches resolver glyph id");
    require(trace.shaped_glyphs_match_cache_key, "trace proves shaped glyph id matches cache key");
    require(trace.raster_payload_matches_cache_key, "trace proves raster payload matches cache key");
    require(trace.glyph_id_from_selection, "trace records selection-sourced glyph id");
    require(!trace.glyph_id_matches_codepoint, "trace records non-codepoint glyph id");

    const render_text_glyph_atlas_materialization_snapshot& materialization =
        diagnostics.glyph_atlas_materializations.front();
    require(
        materialization.status == render_text_glyph_atlas_materialization_status::materialized_upload_ready,
        "mapped glyph materializes upload-ready atlas request");
    require(materialization.resolved_glyph_id == glyph_id.glyph_id, "materialization records resolver glyph id");
    require(materialization.cache_key.glyph_id == glyph_id.glyph_id, "materialization key uses resolver glyph id");
    require(materialization.glyph_id_from_selection, "materialization records selection-sourced glyph id");
    require(!materialization.glyph_id_matches_codepoint, "materialization records non-codepoint glyph id");
    require(materialization.raster_payload_matches_cache_key, "materialization proves payload matches cache key");
}

void test_fake_text_engine_wires_unsupported_shaped_glyph_id_to_skipped_payloads()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xd8\xa7", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    (void)engine.layout_text(request);
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();
    const render_text_font_glyph_id_resolution_snapshot& glyph_id = diagnostics.glyph_id_resolutions.front();
    require(glyph_id.status == render_text_font_glyph_id_resolution_status::resolved, "Arabic codepoint resolves before shaping");
    require(glyph_id.codepoint == 0x0627U, "unsupported shaped glyph diagnostic keeps source codepoint");
    require(glyph_id.glyph_id == 0x0627U, "glyph id resolver does not silently replace supported codepoint");
    require(!glyph_id.used_fallback_glyph_id, "glyph id resolver does not use fallback before shaping");

    const render_text_shaped_glyph& shaped = diagnostics.shaped_glyphs.front();
    require(shaped.glyph_id == 0U, "unsupported shaped glyph uses backend fallback glyph id");
    require(!shaped.glyph_id_from_selection, "unsupported shaped glyph records backend fallback instead of selection id");
    require(!shaped.glyph_id_matches_codepoint, "unsupported shaped glyph proves fallback id differs from codepoint");
    require(shaped.used_fallback_glyph_id, "unsupported shaped glyph records fallback glyph id");
    require(!shaped.glyph_supported, "unsupported shaped glyph does not claim support");

    const render_text_glyph_cache_readiness_snapshot& readiness = diagnostics.glyph_cache_readiness.front();
    require(readiness.codepoint == 0x0627U, "unsupported cache readiness keeps source codepoint");
    require(readiness.glyph_id == shaped.glyph_id, "unsupported cache readiness keeps shaped fallback glyph id");
    require(readiness.cache_key.glyph_id == shaped.glyph_id, "unsupported cache key uses shaped fallback glyph id");
    require(!readiness.cacheable, "unsupported glyph is not cacheable");

    const render_text_rasterized_glyph_atlas_payload_snapshot& payload =
        diagnostics.rasterized_glyph_atlas_payloads.front();
    require(payload.status == render_text_font_rasterizer_status::unsupported_glyph, "unsupported payload is skipped");
    require(payload.codepoint == 0x0627U, "unsupported payload keeps source codepoint");
    require(payload.glyph_id == shaped.glyph_id, "unsupported payload keeps shaped fallback glyph id");
    require(payload.cache_key == readiness.cache_key, "unsupported payload keeps fallback atlas key");
    require(payload.used_fallback_glyph_id, "unsupported payload records fallback glyph id");

    const render_text_shaped_atlas_update_trace_snapshot& trace = diagnostics.shaped_atlas_update_traces.front();
    require(trace.status == render_text_shaped_atlas_update_trace_status::shaped_glyph_without_cache_key, "unsupported glyph has no cache key");
    require(trace.codepoint == 0x0627U, "unsupported trace keeps source codepoint");
    require(trace.resolved_glyph_id == shaped.glyph_id, "unsupported trace keeps shaped fallback glyph id");
    require(trace.shaped_glyph_ids.size() == 1U && trace.shaped_glyph_ids.front() == shaped.glyph_id, "unsupported trace uses fallback glyph id");
    require(trace.cache_key_matches_resolved_glyph_id, "unsupported trace cache key matches fallback glyph id");
    require(trace.raster_payload_matches_cache_key, "unsupported trace payload matches fallback key");
    require(trace.used_fallback_glyph_id, "unsupported trace records fallback glyph id");

    const render_text_glyph_atlas_materialization_snapshot& materialization =
        diagnostics.glyph_atlas_materializations.front();
    require(
        materialization.status == render_text_glyph_atlas_materialization_status::skipped_unsupported_glyph,
        "unsupported glyph materialization records unsupported skip");
    require(!materialization.materialized, "unsupported glyph materialization is not upload-ready");
    require(materialization.used_fallback_glyph_id, "unsupported materialization records fallback glyph id");
    require(
        diagnostics.glyph_atlas_materialization_policy.unsupported_glyph_count == 1,
        "materialization policy counts unsupported glyph skip");
}

void test_fake_text_engine_wires_invalid_utf8_fallback_glyph_id()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = std::string(1U, static_cast<char>(0xc0U)), .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    (void)engine.layout_text(request);
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();
    const render_text_font_glyph_id_resolution_snapshot& glyph_id = diagnostics.glyph_id_resolutions.front();
    require(glyph_id.status == render_text_font_glyph_id_resolution_status::invalid_utf8, "invalid UTF-8 is diagnosed before shaping");
    require(glyph_id.glyph_id == utf8_replacement_codepoint, "invalid UTF-8 uses replacement fallback glyph id");
    require(glyph_id.used_fallback_glyph_id, "invalid UTF-8 records fallback glyph id");

    const render_text_shaped_glyph& shaped = diagnostics.shaped_glyphs.front();
    require(shaped.glyph_id == utf8_replacement_codepoint, "invalid UTF-8 shaped glyph keeps fallback glyph id");
    require(shaped.glyph_id_from_selection, "invalid UTF-8 shaped glyph comes from resolver selection");
    require(shaped.used_fallback_glyph_id, "invalid UTF-8 shaped glyph records fallback glyph id");

    const render_text_rasterized_glyph_atlas_payload_snapshot& payload =
        diagnostics.rasterized_glyph_atlas_payloads.front();
    require(payload.status == render_text_font_rasterizer_status::unsupported_glyph, "invalid UTF-8 payload is skipped");
    require(payload.glyph_id == utf8_replacement_codepoint, "invalid UTF-8 payload keeps fallback glyph id");
    require(payload.used_fallback_glyph_id, "invalid UTF-8 payload records fallback glyph id");

    const render_text_shaped_atlas_update_trace_snapshot& trace = diagnostics.shaped_atlas_update_traces.front();
    require(trace.resolved_glyph_id == utf8_replacement_codepoint, "invalid UTF-8 trace keeps fallback glyph id");
    require(trace.shaped_glyph_ids.size() == 1U && trace.shaped_glyph_ids.front() == utf8_replacement_codepoint, "invalid UTF-8 trace uses fallback glyph id");
    require(trace.used_fallback_glyph_id, "invalid UTF-8 trace records fallback glyph id");

    require(diagnostics.has_font_fallback_chain_runs(), "invalid UTF-8 layout records fallback-chain run");
    require(
        diagnostics.has_font_fallback_chain_missing_glyphs(),
        "invalid UTF-8 layout records fallback-chain missing summary");
    require(
        diagnostics.font_fallback_chain_policy.invalid_utf8_count == 1U,
        "fallback-chain policy counts invalid UTF-8");
    require(
        diagnostics.font_fallback_chain_missing_glyphs.front().valid_utf8 == false,
        "fallback-chain missing summary records invalid UTF-8");
}

void test_fake_text_engine_skips_rasterized_payloads_when_font_bytes_are_missing()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    engine.add_font_face(font_face_descriptor{
        .id = 72,
        .family = "Disk Sans",
        .source_uri = "fonts/missing-disk-sans.ttf",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{
                .first = 0x0041U,
                .last = 0x005aU,
            },
        },
        .weight = 400,
        .italic = false,
    });

    render_text_style_catalog catalog = make_style_catalog();
    catalog.styles.push_back(render_text_style{
        .id = "disk",
        .font_family = "Disk Sans",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .letter_spacing = 0.0f,
        .font_weight = 400,
        .italic = false,
    });

    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "A", .style_token = "disk"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = catalog;
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    require(layout.glyphs.size() == 1, "missing byte fixture still lays out one glyph");
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();
    require(diagnostics.glyph_atlas_placements.size() == 1, "missing byte fixture preserves atlas cache placement");
    require(diagnostics.glyph_cache_readiness.size() == 1, "missing byte fixture records cache readiness");
    require(diagnostics.glyph_cache_readiness.front().cacheable, "missing byte glyph remains cacheable before rasterizer");
    require(
        diagnostics.glyph_cache_readiness.front().has_atlas_slot,
        "missing byte glyph keeps existing atlas readiness behavior");
    require(
        diagnostics.rasterized_glyph_atlas_payloads.size() == 1,
        "missing byte fixture records one rasterizer payload decision");

    const render_text_rasterized_glyph_atlas_payload_snapshot& payload =
        diagnostics.rasterized_glyph_atlas_payloads.front();
    require(
        payload.status == render_text_font_rasterizer_status::missing_font_bytes,
        "missing byte fixture records rasterizer missing bytes");
    require(payload.cacheable, "missing byte payload records cacheable source glyph");
    require(!payload.upload_ready, "missing byte payload is not upload-ready");
    require(payload.skipped, "missing byte payload is skipped");
    require(payload.alpha_bytes == 0U && payload.rgba_bytes == 0U, "missing byte payload has no bitmap data");
    require(payload.source_label == "fonts/missing-disk-sans.ttf", "missing byte payload records source label");
    require(
        payload.diagnostic.find("missing_bytes") != std::string::npos,
        "missing byte payload diagnostic preserves byte load status");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.missing_font_bytes_count == 1,
        "rasterizer payload policy counts missing bytes");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.skipped_count == 1,
        "rasterizer payload policy counts skipped missing-byte payload");
    require(
        diagnostics.rasterized_glyph_atlas_payload_policy.rasterized_count == 0,
        "rasterizer payload policy does not claim rasterized missing-byte glyphs");
    require(
        diagnostics.shaped_atlas_update_traces.size() == 1,
        "missing byte fixture records one shaped atlas trace");
    const render_text_shaped_atlas_update_trace_snapshot& trace = diagnostics.shaped_atlas_update_traces.front();
    require(
        trace.status == render_text_shaped_atlas_update_trace_status::rasterized_payload_skipped,
        "missing byte payload trace records skipped raster payload");
    require(trace.has_cache_key, "missing byte payload trace still records atlas cache key");
    require(trace.rasterizer_status == render_text_font_rasterizer_status::missing_font_bytes, "trace keeps missing byte status");
    require(!trace.payload_upload_ready, "missing byte trace is not upload-ready");
    require(
        diagnostics.shaped_atlas_update_trace_policy.rasterized_payload_skipped_count == 1,
        "trace policy counts skipped raster payload");
    require(
        diagnostics.glyph_atlas_materializations.size() == 1,
        "missing byte fixture records one materialization decision");
    const render_text_glyph_atlas_materialization_snapshot& materialization =
        diagnostics.glyph_atlas_materializations.front();
    require(
        materialization.status == render_text_glyph_atlas_materialization_status::skipped_raster_payload,
        "missing byte materialization records skipped raster payload");
    require(materialization.has_cache_key, "missing byte materialization still has deterministic cache key");
    require(
        materialization.rasterizer_status == render_text_font_rasterizer_status::missing_font_bytes,
        "missing byte materialization keeps rasterizer status");
    require(!materialization.payload_upload_ready, "missing byte materialization is not upload-ready");
    require(
        diagnostics.glyph_atlas_materialization_policy.skipped_raster_payload_count == 1,
        "materialization policy counts skipped raster payload");
}

void test_fake_text_engine_consumes_shaping_backend_glyph_data()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "e\xcc\x81", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();
    require(layout.glyphs.size() == 2, "shaping backend bridge keeps base and mark glyphs");
    require(diagnostics.has_shaped_glyphs(), "shaping backend bridge records shaped glyphs");
    require(diagnostics.has_font_shaping_diagnostics(), "shaping backend bridge records diagnostics");
    require(diagnostics.has_font_shaping_policy(), "shaping backend bridge records shaping policy");
    require(diagnostics.shaped_glyphs.size() == 2, "shaping backend bridge emits two shaped glyph snapshots");
    require(diagnostics.font_shaping_policy.run_count == 1, "shaping backend bridge counts one run");
    require(diagnostics.font_shaping_policy.shaped_run_count == 1, "shaping backend bridge counts shaped run");
    require(diagnostics.font_shaping_policy.glyph_count == 2, "shaping backend bridge counts glyphs");
    require(
        diagnostics.font_shaping_policy.zero_advance_combining_mark_count == 1,
        "shaping backend bridge counts zero-advance combining mark");

    require(
        layout.glyphs[0].glyph_id == diagnostics.shaped_glyphs[0].glyph_id,
        "layout consumes shaped base glyph id");
    require(
        layout.glyphs[1].glyph_id == diagnostics.shaped_glyphs[1].glyph_id,
        "layout consumes shaped mark glyph id");
    require(
        near(layout.glyphs[0].bounds.width, diagnostics.shaped_glyphs[0].advance_x),
        "layout consumes shaped base advance");
    require(
        near(layout.glyphs[1].bounds.width, diagnostics.shaped_glyphs[1].advance_x),
        "layout consumes shaped mark advance");
    require(diagnostics.shaped_glyphs[1].combining_mark, "shaped mark records combining mark");
    require(diagnostics.shaped_glyphs[1].zero_advance, "shaped mark records zero advance");
    require(
        diagnostics.shaped_glyphs[1].cluster_byte_offset == 0
            && diagnostics.shaped_glyphs[1].cluster_byte_count == 3,
        "shaped mark keeps cluster byte range");
}

void test_fake_text_engine_traces_repeated_layout_to_clean_atlas_page()
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

    (void)engine.layout_text(request);
    (void)engine.consume_atlas_updates();
    const render_text_layout repeated_layout = engine.layout_text(request);
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(repeated_layout.glyphs.size() == 1, "repeated layout keeps one glyph");
    require(diagnostics.shaped_atlas_update_traces.size() == 1, "repeated layout records one shaped atlas trace");
    const render_text_shaped_atlas_update_trace_snapshot& trace = diagnostics.shaped_atlas_update_traces.front();
    require(
        trace.status == render_text_shaped_atlas_update_trace_status::clean_atlas_page_reused,
        "repeated layout traces clean atlas page reuse");
    require(trace.clean_page_reused, "repeated layout trace marks clean reuse");
    require(!trace.has_atlas_update, "repeated layout trace has no dirty atlas update");
    require(trace.payload_upload_ready, "repeated layout still has upload-ready payload");
    require(trace.has_atlas_placement, "repeated layout trace keeps atlas placement");
    require(
        diagnostics.shaped_atlas_update_trace_policy.clean_atlas_page_reused_count == 1,
        "trace policy counts clean page reuse");
    require(
        diagnostics.glyph_atlas_materializations.size() == 1,
        "repeated layout records one materialization decision");
    const render_text_glyph_atlas_materialization_snapshot& materialization =
        diagnostics.glyph_atlas_materializations.front();
    require(
        materialization.status == render_text_glyph_atlas_materialization_status::materialized_clean_reuse,
        "repeated layout materialization records clean atlas reuse");
    require(materialization.materialized, "clean reuse still counts as materialized atlas data");
    require(materialization.clean_reuse, "materialization marks clean reuse");
    require(!materialization.has_atlas_update, "clean reuse materialization has no dirty atlas update");
    require(
        diagnostics.glyph_atlas_materialization_policy.clean_reuse_count == 1,
        "materialization policy counts clean reuse");
    require(
        diagnostics.glyph_atlas_page_policy.repeated_layout_clean_page_count > 0,
        "atlas page policy records repeated clean page");
}

void test_fake_text_engine_traces_shaped_glyph_without_cache_key()
{
    using namespace quiz_vulkan::render;

    fake_text_engine engine;
    render_text_request request;
    request.text_runs = {
        render_text_run{.text = "\xcc\x81", .style_token = "body"},
    };
    request.bounds = render_rect{0.0f, 0.0f, 200.0f, 0.0f};
    request.style_catalog = make_style_catalog();
    request.options = render_text_options{
        .wrap = render_text_wrap_mode::no_wrap,
        .alignment = render_text_alignment::start,
        .max_lines = 0,
    };

    const render_text_layout layout = engine.layout_text(request);
    const fake_text_engine_diagnostics& diagnostics = engine.last_diagnostics();

    require(layout.glyphs.size() == 1, "standalone combining mark remains in layout stream");
    require(diagnostics.glyph_cache_readiness.size() == 1, "standalone combining mark records cache readiness");
    require(!diagnostics.glyph_cache_readiness.front().cacheable, "standalone combining mark is not cacheable");
    require(diagnostics.shaped_atlas_update_traces.size() == 1, "standalone combining mark records shaped atlas trace");
    const render_text_shaped_atlas_update_trace_snapshot& trace = diagnostics.shaped_atlas_update_traces.front();
    require(
        trace.status == render_text_shaped_atlas_update_trace_status::shaped_glyph_without_cache_key,
        "standalone combining mark traces missing cache key");
    require(!trace.has_cache_key, "standalone combining mark trace has no cache key");
    require(trace.shaped_glyph_ids.size() == 1 && trace.shaped_glyph_ids.front() == 0x0301U, "trace records mark glyph id");
    require(trace.cluster_byte_offset == 0 && trace.cluster_byte_count == 2, "trace records cluster byte range");
    require(
        diagnostics.shaped_atlas_update_trace_policy.shaped_glyph_without_cache_key_count == 1,
        "trace policy counts missing cache key");
    require(
        diagnostics.glyph_atlas_materializations.size() == 1,
        "standalone combining mark records one materialization decision");
    const render_text_glyph_atlas_materialization_snapshot& materialization =
        diagnostics.glyph_atlas_materializations.front();
    require(
        materialization.status == render_text_glyph_atlas_materialization_status::skipped_missing_cache_key,
        "standalone combining mark materialization records missing cache key");
    require(!materialization.has_cache_key, "standalone combining mark materialization has no cache key");
    require(!materialization.materialized, "missing cache key materialization is skipped");
    require(
        diagnostics.glyph_atlas_materialization_policy.missing_cache_key_count == 1,
        "materialization policy counts missing cache key");
}

} // namespace

int main()
{
    test_fake_glyph_clusters_track_utf8_runs_and_font_faces();
    test_fake_glyph_clusters_track_wrapped_hangul_lines();
    test_fake_shaping_diagnostics_track_utf8_clusters_and_wrap_decisions();
    test_fake_line_metrics_track_overflow_and_truncation();
    test_fake_line_break_policy_keeps_combining_clusters_caret_safe();
    test_fake_text_engine_records_rasterized_atlas_payloads_for_cacheable_glyphs();
    test_fake_text_engine_records_fallback_only_backend_capability_for_latin_hangul();
    test_fake_text_engine_dependency_probe_reports_missing_real_backend_fallback();
    test_fake_text_engine_dependency_probe_reports_adapter_unavailable_fallback();
    test_fake_text_engine_dependency_probe_reports_version_mismatch_fallback();
    test_fake_text_engine_dependency_probe_reports_adapter_ready_selection();
    test_fake_text_engine_dependency_probe_diff_compares_layout_snapshots();
    test_fake_text_engine_carries_injected_backend_selection_to_layout_diagnostics();
    test_fake_text_engine_records_font_fallback_chain_for_mixed_script_layout();
    test_fake_text_engine_records_font_fallback_chain_for_mixed_script_carets();
    test_fake_text_engine_records_font_fallback_chain_for_mixed_script_selection();
    test_fake_text_engine_records_unavailable_backend_capability();
    test_fake_text_engine_keeps_complex_script_on_fallback_until_backend_supports_it();
    test_fake_text_engine_uses_default_deterministic_shaping_without_adapter();
    test_fake_text_engine_injected_adapter_unavailable_drives_shaping_diagnostics();
    test_fake_text_engine_injected_adapter_shapes_available_complex_script();
    test_fake_text_engine_injected_adapter_glyph_mismatch_drives_failure_diagnostics();
    test_fake_text_engine_injected_adapter_failure_drives_shaping_diagnostics();
    test_fake_text_engine_wires_resolved_glyph_id_through_atlas_payloads();
    test_fake_text_engine_wires_unsupported_shaped_glyph_id_to_skipped_payloads();
    test_fake_text_engine_wires_invalid_utf8_fallback_glyph_id();
    test_fake_text_engine_skips_rasterized_payloads_when_font_bytes_are_missing();
    test_fake_text_engine_consumes_shaping_backend_glyph_data();
    test_fake_text_engine_traces_repeated_layout_to_clean_atlas_page();
    test_fake_text_engine_traces_shaped_glyph_without_cache_key();
    return 0;
}
