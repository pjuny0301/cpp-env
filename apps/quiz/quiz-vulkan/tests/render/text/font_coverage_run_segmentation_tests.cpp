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

quiz_vulkan::render::render_text_style_catalog fallback_chain_style_catalog()
{
    quiz_vulkan::render::render_text_style_catalog catalog;
    catalog.fallback_style = primary_style();
    catalog.styles.push_back(primary_style());
    return catalog;
}

quiz_vulkan::render::font_face_catalog make_fallback_chain_catalog(const bool include_emoji)
{
    using namespace quiz_vulkan::render;

    const font_unicode_coverage_catalog_adapter adapter;
    font_face_catalog catalog;
    catalog.add_face(adapter.apply_to_descriptor(
        font_face_descriptor{
            .id = 201,
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
            .id = 202,
            .family = "Hangul Fallback",
            .source_uri = "fixture://fonts/hangul",
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
        })));

    if (include_emoji) {
        catalog.add_face(adapter.apply_to_descriptor(
            font_face_descriptor{
                .id = 203,
                .family = "Emoji Color",
                .source_uri = "fixture://fonts/emoji",
                .version = "fixture-1",
                .license = "test-fixture",
                .weight = 400,
                .fallback = true,
            },
            make_coverage({
                render_text_font_cmap_range{
                    .first_codepoint = static_cast<char32_t>(0x1f600U),
                    .last_codepoint = static_cast<char32_t>(0x1f64fU),
                },
            })));
    }

    return catalog;
}

quiz_vulkan::render::render_text_font_backend_selection_result selected_harfbuzz_shaping()
{
    using namespace quiz_vulkan::render;
    return select_render_text_font_backend(render_text_font_backend_selection_request{
        .purpose = render_text_font_backend_selection_purpose::shaping,
        .candidates = {
            make_render_text_harfbuzz_backend_candidate(true),
            make_render_text_deterministic_fake_backend_candidate(),
        },
    });
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

void test_fallback_chain_plans_mixed_batch_selected_face_order_before_shaping()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_fallback_chain_catalog(true);
    render_text_font_fallback_chain_plan_request request;
    request.shaping_selection = selected_harfbuzz_shaping();
    request.items.push_back(make_render_text_font_fallback_chain_plan_item(
        std::vector<render_text_run>{
            render_text_run{
                .text = "A",
                .style_token = "primary",
            },
            render_text_run{
                .text = std::string("\xEA\xB0\x80", 3) + std::string("\xF0\x9F\x98\x80", 4),
                .style_token = "primary",
            },
        },
        fallback_chain_style_catalog(),
        "mixed-batch",
        7U));

    const render_text_font_fallback_chain_plan_snapshot plan =
        plan_render_text_font_fallback_chains(request, catalog);

    require(plan.ok(), "fallback chain resolves Latin, Hangul, and emoji coverage");
    require(!plan.has_missing_glyphs(), "resolved fallback chain has no missing glyph summaries");
    require(plan.policy.item_count == 1U, "fallback chain records item count");
    require(plan.policy.run_count == 2U, "fallback chain records per-run snapshots");
    require(plan.policy.codepoint_count == 3U, "fallback chain records batch codepoint count");
    require(plan.policy.supported_codepoint_count == 3U, "fallback chain records supported glyph count");
    require(plan.policy.fallback_codepoint_count == 2U, "fallback chain records Hangul and emoji fallback count");
    require(plan.policy.unique_selected_face_count == 3U, "fallback chain records unique selected faces");
    require(plan.deterministic_selected_face_order.size() == 3U, "selected face order has three entries");
    require(plan.deterministic_selected_face_order[0] == 201U, "Latin requested face is selected first");
    require(plan.deterministic_selected_face_order[1] == 202U, "Hangul fallback face is selected second");
    require(plan.deterministic_selected_face_order[2] == 203U, "emoji fallback face is selected third");
    require(
        plan.shaping_selection.selected.library == render_text_font_backend_library::harfbuzz,
        "injected HarfBuzz shaping selection reaches plan diagnostics");

    require(plan.runs[0].item_index == 7U && plan.runs[0].run_index == 0U, "first run keeps item/run identity");
    require(plan.runs[0].selected_face_ids.size() == 1U, "Latin run selects one face");
    require(plan.runs[0].selected_face_ids.front() == 201U, "Latin run selects requested face");
    require(plan.runs[0].entries.size() == 3U, "Latin run records requested and fallback chain entries");
    require(plan.runs[0].entries[0].requested_face, "first chain entry is requested face");
    require(plan.runs[0].entries[0].covered_codepoint_count == 1U, "requested face covers Latin codepoint");
    require(plan.runs[0].entries[0].selected_codepoint_count == 1U, "requested face is selected for Latin");
    require(plan.runs[0].shaping_backend == render_text_font_backend_library::harfbuzz, "run records selected shaping backend");
    require(!plan.runs[0].shaping_used_deterministic_fallback, "run records real backend selection");

    require(plan.runs[1].selected_face_ids.size() == 2U, "fallback run selects Hangul and emoji faces");
    require(plan.runs[1].selected_face_ids[0] == 202U, "fallback run selects Hangul fallback first");
    require(plan.runs[1].selected_face_ids[1] == 203U, "fallback run selects emoji fallback second");
    require(plan.runs[1].entries[1].family == "Hangul Fallback", "fallback chain records Hangul face metadata");
    require(plan.runs[1].entries[1].covered_codepoint_count == 1U, "Hangul face covers one codepoint");
    require(plan.runs[1].entries[1].selected_codepoint_count == 1U, "Hangul face is selected once");
    require(plan.runs[1].entries[2].family == "Emoji Color", "fallback chain records emoji face metadata");
    require(plan.runs[1].entries[2].covered_codepoint_count == 1U, "emoji face covers one codepoint");
    require(plan.runs[1].entries[2].selected_codepoint_count == 1U, "emoji face is selected once");
}

void test_fallback_chain_reports_missing_emoji_without_claiming_support()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_fallback_chain_catalog(false);
    const std::string text = std::string("A") + std::string("\xEA\xB0\x80", 3)
        + std::string("\xF0\x9F\x98\x80", 4);
    const render_text_font_fallback_chain_plan_snapshot plan =
        plan_render_text_font_fallback_chains(
            render_text_font_fallback_chain_plan_request{
                .items = {
                    make_render_text_font_fallback_chain_plan_item(
                        std::vector<render_text_run>{
                            render_text_run{
                                .text = text,
                                .style_token = "primary",
                            },
                        },
                        fallback_chain_style_catalog(),
                        "missing-emoji",
                        3U),
                },
            },
            catalog);

    require(!plan.ok(), "missing emoji prevents fallback chain plan from being ok");
    require(plan.has_missing_glyphs(), "missing emoji produces glyph summary");
    require(plan.policy.supported_codepoint_count == 2U, "Latin and Hangul remain supported");
    require(plan.policy.missing_glyph_count == 1U, "only emoji is missing");
    require(plan.policy.invalid_utf8_count == 0U, "emoji scalar is valid UTF-8");
    require(plan.policy.deterministic_backend_selected, "default fallback chain uses deterministic backend metadata");
    require(plan.deterministic_selected_face_order.size() == 2U, "missing emoji is not added to selected face order");
    require(plan.deterministic_selected_face_order[0] == 201U, "Latin requested face remains first");
    require(plan.deterministic_selected_face_order[1] == 202U, "Hangul fallback face remains second");

    const render_text_font_fallback_chain_missing_glyph_snapshot& missing = plan.missing_glyphs.front();
    require(missing.item_index == 3U && missing.run_index == 0U, "missing glyph records item/run identity");
    require(missing.valid_utf8, "missing emoji summary records valid UTF-8");
    require(missing.codepoint == 0x1f600U, "missing glyph summary records emoji codepoint");
    require(missing.requested_face_id == 201U, "missing glyph records requested face");
    require(missing.attempted_face_ids.size() == 2U, "missing glyph records attempted chain faces");
    require(missing.attempted_face_ids[0] == 201U, "missing glyph attempted requested face first");
    require(missing.attempted_face_ids[1] == 202U, "missing glyph attempted fallback face second");
    require(missing.diagnostic.find("U+1F600") != std::string::npos, "missing glyph diagnostic names emoji");

    const render_text_font_fallback_chain_run_snapshot& run = plan.runs.front();
    require(!run.ok(), "run with missing emoji is not ok");
    require(run.coverage.unsupported_codepoint_count == 1U, "coverage segmentation preserves unsupported count");
    require(run.entries.size() == 2U, "chain omits absent emoji face");
    require(run.entries[0].selected_codepoint_count == 1U, "requested face selects Latin only");
    require(run.entries[1].selected_codepoint_count == 1U, "Hangul fallback selects Hangul only");
}

} // namespace

int main()
{
    test_latin_stays_on_requested_face_and_merges_contiguous_codepoints();
    test_hangul_and_non_bmp_fall_back_to_adapted_coverage_face();
    test_invalid_utf8_produces_unsupported_segment_diagnostic();
    test_unsupported_codepoint_produces_unsupported_segment_diagnostic();
    test_adjacent_requested_and_fallback_faces_form_separate_merged_runs();
    test_fallback_chain_plans_mixed_batch_selected_face_order_before_shaping();
    test_fallback_chain_reports_missing_emoji_without_claiming_support();
    return 0;
}
