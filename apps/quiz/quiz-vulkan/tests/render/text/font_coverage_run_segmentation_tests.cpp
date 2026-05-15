#include "render/text/font_fallback_shaping_handoff.h"
#include "render/text/font_fallback_run_planning_diagnostics.h"
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

quiz_vulkan::render::font_face_catalog make_cluster_fallback_catalog()
{
    using namespace quiz_vulkan::render;

    const font_unicode_coverage_catalog_adapter adapter;
    font_face_catalog catalog;
    catalog.add_face(adapter.apply_to_descriptor(
        font_face_descriptor{
            .id = 211,
            .family = "Primary Sans",
            .source_uri = "fixture://fonts/primary-latin",
            .version = "fixture-1",
            .license = "test-fixture",
            .weight = 400,
        },
        make_coverage({
            render_text_font_cmap_range{
                .first_codepoint = U'A',
                .last_codepoint = U'A',
            },
        })));
    catalog.add_face(adapter.apply_to_descriptor(
        font_face_descriptor{
            .id = 212,
            .family = "Cluster Fallback",
            .source_uri = "fixture://fonts/cluster-fallback",
            .version = "fixture-1",
            .license = "test-fixture",
            .weight = 400,
            .fallback = true,
        },
        make_coverage({
            render_text_font_cmap_range{
                .first_codepoint = U'A',
                .last_codepoint = U'A',
            },
            render_text_font_cmap_range{
                .first_codepoint = static_cast<char32_t>(0x0301U),
                .last_codepoint = static_cast<char32_t>(0x0301U),
            },
        })));
    return catalog;
}

quiz_vulkan::render::font_face_catalog make_alternate_hangul_fallback_catalog()
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
            .id = 204,
            .family = "Alternate Hangul Fallback",
            .source_uri = "fixture://fonts/alternate-hangul",
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

quiz_vulkan::render::render_text_font_fallback_shaped_glyph_execution_snapshot
make_fallback_execution_snapshot(
    const std::string& text,
    const quiz_vulkan::render::font_face_catalog& catalog,
    const std::string& source_label,
    const std::size_t item_index)
{
    using namespace quiz_vulkan::render;

    render_text_font_fallback_run_plan_request request;
    request.items.push_back(make_render_text_font_fallback_chain_plan_item(
        std::vector<render_text_run>{
            render_text_run{
                .text = text,
                .style_token = "primary",
            },
        },
        fallback_chain_style_catalog(),
        source_label,
        item_index));

    const render_text_font_fallback_run_plan_snapshot plan =
        plan_render_text_font_fallback_runs(request, catalog);
    const render_text_font_fallback_shaping_handoff_snapshot handoff =
        make_render_text_font_fallback_shaping_handoff(plan);
    const render_text_font_fallback_shaped_glyph_input_snapshot inputs =
        make_render_text_font_fallback_shaped_glyph_inputs(
            render_text_font_fallback_shaped_glyph_input_request{
                .handoff = handoff,
                .items = request.items,
                .font_catalog = catalog,
            });
    return execute_render_text_font_fallback_shaped_glyph_inputs(inputs);
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

void test_combining_cluster_uses_one_fallback_face_for_coverage()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_cluster_fallback_catalog();
    const std::string text = std::string("A") + std::string("\xCC\x81", 2);
    const render_text_font_coverage_run_segmentation segmentation =
        segment_font_coverage_runs(text, catalog, primary_style());

    require(segmentation.ok(), "cluster-aware segmentation resolves combining sequence");
    require(segmentation.codepoint_count == 2U, "combining sequence records two codepoints");
    require(segmentation.supported_codepoint_count == 2U, "combining sequence is fully supported");
    require(segmentation.fallback_codepoint_count == 2U, "entire combining cluster moves to fallback face");
    require(segmentation.segments.size() == 1U, "combining sequence stays in one coverage segment");

    const render_text_font_coverage_run_segment& segment = segmentation.segments.front();
    require(segment.byte_offset == 0U && segment.byte_count == 3U, "cluster segment spans base and mark bytes");
    require(segment.codepoint_offset == 0U && segment.codepoint_count == 2U, "cluster segment spans base and mark scalars");
    require(segment.requested_face_id == 211U, "cluster segment records requested face");
    require(segment.resolved_face_id == 212U, "cluster segment selects the fallback face that covers the whole cluster");
    require(segment.used_fallback, "cluster segment records fallback use");
    require(segment.glyph_supported, "cluster segment reports supported glyph coverage");
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

void test_fallback_run_plan_splits_latin_hangul_and_missing_ranges()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_fallback_chain_catalog(false);
    const std::string text = std::string("A") + std::string("\xEA\xB0\x80", 3)
        + std::string("\xF0\x9F\x98\x80", 4) + std::string("B");
    const render_text_font_fallback_run_plan_snapshot plan =
        plan_render_text_font_fallback_runs(text, catalog, primary_style());

    require(!plan.ok(), "missing emoji keeps fallback run plan incomplete");
    require(plan.policy.fallback_run_count == 4U, "mixed Latin/Hangul/missing text forms four ranges");
    require(plan.policy.codepoint_count == 4U, "fallback run plan counts all scalars");
    require(plan.policy.covered_codepoint_count == 3U, "Latin and Hangul are covered");
    require(plan.policy.fallback_codepoint_count == 1U, "Hangul uses fallback face");
    require(plan.policy.missing_glyph_count == 1U, "emoji is missing");
    require(plan.policy.missing_run_count == 1U, "missing emoji range is counted");
    require(plan.policy.unique_selected_face_count == 2U, "selected face order excludes missing glyph");
    require(plan.selected_face_order.size() == 2U, "selected face order records deterministic fallback order");
    require(plan.selected_face_order[0] == 201U, "Latin requested face is selected first");
    require(plan.selected_face_order[1] == 202U, "Hangul fallback face is selected second");

    require(plan.runs[0].ok(), "first Latin run is covered");
    require(plan.runs[0].selected_face_id == 201U, "Latin run selects requested face");
    require(!plan.runs[0].used_fallback, "Latin run does not use fallback");
    require(plan.runs[0].byte_offset == 0U && plan.runs[0].byte_count == 1U, "Latin run records byte range");
    require(!plan.runs[0].stable_run_key.empty(), "Latin run has stable key");

    require(plan.runs[1].ok(), "Hangul run is covered");
    require(plan.runs[1].selected_face_id == 202U, "Hangul run selects fallback face");
    require(plan.runs[1].fallback_order == 1U, "Hangul run records fallback order");
    require(plan.runs[1].used_fallback, "Hangul run records fallback selection");
    require(plan.runs[1].attempted_face_ids.size() == 2U, "Hangul run records attempted faces");
    require(plan.runs[1].attempted_face_ids[0] == 201U, "requested face is attempted first");
    require(plan.runs[1].attempted_face_ids[1] == 202U, "fallback face is attempted second");

    require(plan.runs[2].missing(), "emoji run is missing");
    require(
        plan.runs[2].status == render_text_font_fallback_run_status::missing_glyph,
        "emoji run reports missing glyph status");
    require(plan.runs[2].selected_face_id == 0U, "missing emoji does not claim selected face");
    require(!plan.runs[2].glyph_supported, "missing emoji does not claim glyph support");
    require(plan.missing_runs.size() == 1U, "missing run is copied to missing range summary");
    require(plan.missing_runs.front().stable_run_key == plan.runs[2].stable_run_key, "missing summary preserves stable key");

    require(plan.runs[3].ok(), "trailing Latin run is covered");
    require(plan.runs[3].selected_face_id == 201U, "trailing Latin returns to requested face");
}

void test_fallback_run_plan_merges_contiguous_ranges_for_same_selected_face()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_fallback_chain_catalog(true);
    const std::string text = std::string("AB") + std::string("\xEA\xB0\x80", 3)
        + std::string("\xEA\xB0\x81", 3);
    const render_text_font_fallback_run_plan_snapshot plan =
        plan_render_text_font_fallback_runs(text, catalog, primary_style());

    require(plan.ok(), "Latin and Hangul fallback ranges resolve");
    require(plan.policy.fallback_run_count == 2U, "contiguous same-face ranges merge");
    require(plan.runs[0].selected_face_id == 201U, "first merged run uses requested Latin face");
    require(plan.runs[0].byte_count == 2U, "Latin run merges two ASCII bytes");
    require(plan.runs[0].codepoint_count == 2U, "Latin run merges two codepoints");
    require(plan.runs[1].selected_face_id == 202U, "second merged run uses Hangul fallback");
    require(plan.runs[1].byte_count == 6U, "Hangul run merges two UTF-8 scalars");
    require(plan.runs[1].codepoint_count == 2U, "Hangul run merges two codepoints");
    require(plan.runs[1].first_codepoint == 0xac00U, "Hangul run records first codepoint");
    require(plan.runs[1].last_codepoint == 0xac01U, "Hangul run records last codepoint");

    const render_text_font_fallback_run_plan_snapshot repeated =
        plan_render_text_font_fallback_runs(text, catalog, primary_style());
    require(
        repeated.runs[1].stable_run_key == plan.runs[1].stable_run_key,
        "stable run key repeats for identical fallback ranges");
}

void test_fallback_run_plan_keeps_combining_cluster_on_one_face()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_cluster_fallback_catalog();
    const std::string text = std::string("A") + std::string("\xCC\x81", 2);
    const render_text_font_fallback_run_plan_snapshot plan =
        plan_render_text_font_fallback_runs(text, catalog, primary_style());

    require(plan.ok(), "fallback run planner resolves combining cluster");
    require(plan.policy.fallback_run_count == 1U, "combining cluster produces one fallback run");
    require(plan.policy.covered_codepoint_count == 2U, "fallback run covers both combining codepoints");
    require(plan.policy.fallback_codepoint_count == 2U, "fallback policy counts both codepoints as fallback");
    require(plan.policy.unique_selected_face_count == 1U, "one face is selected for the cluster");
    require(plan.selected_face_order.size() == 1U && plan.selected_face_order.front() == 212U, "cluster fallback face is selected once");

    const render_text_font_fallback_run_snapshot& run = plan.runs.front();
    require(run.selected_face_id == 212U, "combining fallback run selects whole-cluster fallback face");
    require(run.byte_offset == 0U && run.byte_count == 3U, "combining fallback run spans all cluster bytes");
    require(run.codepoint_offset == 0U && run.codepoint_count == 2U, "combining fallback run spans all cluster codepoints");
    require(run.first_codepoint == U'A' && run.last_codepoint == 0x0301U, "combining fallback run records cluster endpoints");
    require(run.used_fallback, "combining fallback run records fallback use");
    require(run.ok(), "combining fallback run is ready for shaping");
}

void test_fallback_run_plan_diff_reports_catalog_coverage_change()
{
    using namespace quiz_vulkan::render;

    const std::string text = std::string("\xF0\x9F\x98\x80", 4);
    const render_text_font_fallback_run_plan_snapshot missing =
        plan_render_text_font_fallback_runs(text, make_fallback_chain_catalog(false), primary_style());
    const render_text_font_fallback_run_plan_snapshot covered =
        plan_render_text_font_fallback_runs(text, make_fallback_chain_catalog(true), primary_style());
    const render_text_font_fallback_run_plan_diff_snapshot diff =
        diff_render_text_font_fallback_run_plans(missing, covered);

    require(!missing.ok(), "baseline plan is missing emoji coverage");
    require(covered.ok(), "updated catalog covers emoji");
    require(diff.has_changes(), "fallback run diff detects catalog coverage change");
    require(diff.changed_run_count == 1U, "same UTF-8 range is reported as changed");
    require(diff.added_run_count == 0U && diff.removed_run_count == 0U, "coverage change keeps stable run key");
    require(diff.policy.covered_codepoint_count_delta == 1, "diff records covered codepoint gain");
    require(diff.policy.missing_glyph_count_delta == -1, "diff records missing glyph recovery");
    require(diff.policy.unique_selected_face_count_delta == 1, "diff records selected face gain");
    require(diff.policy.selected_face_order_changed, "diff records selected face order change");
    require(diff.policy.missing_state_changed, "diff records missing state change");
    require(diff.run_diffs.front().status_changed, "run diff records status change");
    require(diff.run_diffs.front().selected_face_changed, "run diff records selected face change");
    require(diff.run_diffs.front().missing_changed, "run diff records missing recovery");
    require(diff.run_diffs.front().previous_selected_face_id == 0U, "previous run did not claim selected face");
    require(diff.run_diffs.front().current_selected_face_id == 203U, "current run selects emoji fallback face");
}

void test_fallback_shaping_handoff_summarizes_ready_and_blocked_runs()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_fallback_chain_catalog(false);
    const std::string text = std::string("A") + std::string("\xEA\xB0\x80", 3)
        + std::string("\xF0\x9F\x98\x80", 4) + std::string("B");
    const render_text_font_fallback_run_plan_snapshot plan =
        plan_render_text_font_fallback_runs(text, catalog, primary_style());
    const render_text_font_fallback_shaping_handoff_snapshot handoff =
        make_render_text_font_fallback_shaping_handoff(plan);

    require(!handoff.ok(), "missing emoji blocks shaping handoff");
    require(handoff.has_blocked_runs(), "handoff exposes blocked run summaries");
    require(handoff.policy.run_count == 4U, "handoff preserves fallback run count");
    require(handoff.policy.ready_run_count == 3U, "Latin and Hangul runs are ready to shape");
    require(handoff.policy.blocked_run_count == 1U, "emoji run is blocked");
    require(handoff.policy.missing_glyph_run_count == 1U, "missing emoji is counted as missing glyph");
    require(handoff.policy.invalid_utf8_run_count == 0U, "valid emoji is not invalid UTF-8");
    require(handoff.policy.no_selected_face_run_count == 0U, "missing glyph accounts for absent selected face");
    require(handoff.policy.ready_codepoint_count == 3U, "ready runs total supported codepoints");
    require(handoff.policy.blocked_codepoint_count == 1U, "blocked run totals missing emoji codepoint");
    require(handoff.policy.fallback_ready_run_count == 1U, "Hangul ready run records fallback use");
    require(handoff.policy.unique_page_key_count == 2U, "ready runs expose requested and fallback page evidence");
    require(handoff.policy.unique_style_token_count == 1U, "handoff records stable style evidence");

    require(handoff.stable_run_keys.size() == 4U, "handoff exposes every fallback run key");
    require(handoff.stable_page_keys.size() == 2U, "handoff exposes unique ready page keys");
    require(handoff.style_tokens.size() == 1U && handoff.style_tokens.front() == "primary", "handoff records style token");
    require(
        handoff.ready_runs[0].stable_run_key == plan.runs[0].stable_run_key,
        "first ready run preserves fallback run key");
    require(
        handoff.ready_runs[1].stable_page_key
            == font_fallback_shaping_handoff_stable_page_key_for(plan.runs[1]),
        "fallback run exposes stable page key for later atlas shaping");
    require(handoff.ready_runs[1].selected_face_id == 202U, "handoff preserves selected fallback face");
    require(handoff.ready_runs[1].style_token == "primary", "handoff preserves style token");
    require(handoff.ready_runs[1].ready_to_shape(), "Hangul run is marked ready to shape");

    const render_text_font_fallback_shaping_handoff_run_snapshot& blocked = handoff.blocked_runs.front();
    require(blocked.blocked(), "blocked emoji run is not ready");
    require(blocked.stable_run_key == plan.runs[2].stable_run_key, "blocked run preserves stable fallback key");
    require(blocked.stable_page_key.empty(), "blocked run does not claim page evidence");
    require(
        blocked.handoff_status == render_text_font_fallback_shaping_handoff_status::missing_glyph,
        "blocked run records missing glyph handoff status");
    require(blocked.selected_face_id == 0U, "blocked missing glyph has no selected face");
    require(blocked.diagnostic.find("missing_glyph") != std::string::npos, "blocked diagnostic names reason");
}

void test_fallback_shaping_handoff_reports_invalid_utf8_and_no_selected_face()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_fallback_chain_catalog(true);
    const std::string invalid_text(1U, static_cast<char>(0xc3U));
    const render_text_font_fallback_run_plan_snapshot invalid_plan =
        plan_render_text_font_fallback_runs(invalid_text, catalog, primary_style());
    const render_text_font_fallback_shaping_handoff_snapshot invalid_handoff =
        make_render_text_font_fallback_shaping_handoff(invalid_plan);

    require(!invalid_handoff.ok(), "invalid UTF-8 blocks shaping handoff");
    require(invalid_handoff.policy.invalid_utf8_run_count == 1U, "invalid UTF-8 blocker is counted");
    require(
        invalid_handoff.blocked_runs.front().handoff_status
            == render_text_font_fallback_shaping_handoff_status::invalid_utf8,
        "invalid handoff run records invalid UTF-8 status");
    require(invalid_handoff.blocked_runs.front().stable_page_key.empty(), "invalid UTF-8 has no page evidence");

    render_text_font_fallback_run_plan_snapshot no_face_plan =
        plan_render_text_font_fallback_runs("A", catalog, primary_style());
    no_face_plan.runs.front().selected_face_id = 0U;
    const render_text_font_fallback_shaping_handoff_snapshot no_face_handoff =
        make_render_text_font_fallback_shaping_handoff(no_face_plan);

    require(!no_face_handoff.ok(), "covered run without selected face blocks handoff");
    require(no_face_handoff.policy.no_selected_face_run_count == 1U, "no selected face blocker is counted");
    require(no_face_handoff.policy.missing_glyph_run_count == 0U, "no selected face is distinct from missing glyph");
    require(
        no_face_handoff.blocked_runs.front().handoff_status
            == render_text_font_fallback_shaping_handoff_status::no_selected_face,
        "blocked run records no selected face handoff status");
    require(no_face_handoff.blocked_runs.front().stable_page_key.empty(), "no selected face has no page evidence");
}

void test_fallback_shaped_glyph_inputs_materialize_ready_runs_only()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_fallback_chain_catalog(false);
    const std::string text = std::string("A") + std::string("\xEA\xB0\x80", 3)
        + std::string("\xF0\x9F\x98\x80", 4) + std::string("B");
    render_text_font_fallback_run_plan_request request;
    request.items.push_back(make_render_text_font_fallback_chain_plan_item(
        std::vector<render_text_run>{
            render_text_run{
                .text = text,
                .style_token = "primary",
            },
        },
        fallback_chain_style_catalog(),
        "shaped-input-fixture",
        11U));

    const render_text_font_fallback_run_plan_snapshot plan =
        plan_render_text_font_fallback_runs(request, catalog);
    const render_text_font_fallback_shaping_handoff_snapshot handoff =
        make_render_text_font_fallback_shaping_handoff(plan);
    const render_text_font_fallback_shaped_glyph_input_snapshot inputs =
        make_render_text_font_fallback_shaped_glyph_inputs(
            render_text_font_fallback_shaped_glyph_input_request{
                .handoff = handoff,
                .items = request.items,
                .font_catalog = catalog,
            });

    require(!inputs.ok(), "missing emoji keeps shaped glyph input snapshot incomplete");
    require(inputs.has_inputs(), "ready fallback runs produce shaped glyph input records");
    require(inputs.inputs.size() == 3U, "Latin, Hangul, and trailing Latin produce inputs");
    require(inputs.blocked_runs.size() == 1U, "missing emoji remains a blocked run");
    require(inputs.policy.handoff_run_count == 4U, "input policy preserves handoff run count");
    require(inputs.policy.ready_run_count == 3U, "input policy preserves ready run count");
    require(inputs.policy.blocked_run_count == 1U, "input policy preserves blocked run count");
    require(inputs.policy.input_count == 3U, "input policy counts emitted input records");
    require(inputs.policy.cacheable_input_count == 3U, "all supported fixture glyphs are cacheable");
    require(inputs.policy.uncacheable_input_count == 0U, "no ready fixture glyph is uncacheable");
    require(inputs.policy.fallback_input_count == 1U, "only Hangul uses a fallback face");
    require(inputs.policy.unique_page_key_count == 2U, "requested and fallback page keys are stable evidence");
    require(inputs.policy.unique_style_token_count == 1U, "style token evidence is stable");
    require(inputs.stable_input_keys.size() == 3U, "each shaped input exposes a stable key");

    const render_text_font_fallback_shaped_glyph_input_record& latin = inputs.inputs[0];
    require(latin.ready_for_shaping(), "Latin input is ready for fake shaping");
    require(latin.ready_for_glyph_atlas(), "Latin input is ready for fake atlas handoff");
    require(latin.stable_run_key == plan.runs[0].stable_run_key, "Latin input preserves fallback run key");
    require(latin.stable_page_key == handoff.ready_runs[0].stable_page_key, "Latin input preserves page key");
    require(latin.item_index == 11U && latin.source_run_index == 0U, "Latin input preserves item/run identity");
    require(latin.byte_offset == 0U && latin.byte_count == 1U, "Latin input preserves byte span");
    require(latin.source_codepoint_index == 0U, "Latin input records source scalar index");
    require(latin.codepoint == U'A', "Latin input keeps codepoint");
    require(latin.glyph_id == U'A', "Latin input maps glyph id deterministically");
    require(latin.selected_face_id == 201U, "Latin input selects requested face");
    require(latin.cache_key.face_id == 201U, "Latin atlas key uses selected face");
    require(latin.cache_key.glyph_id == U'A', "Latin atlas key uses glyph id");
    require(latin.cache_key.pixel_size == 16U, "Latin atlas key uses line-height pixel size");
    require(latin.font_selection.glyph_id == latin.glyph_id, "Latin shaping selection carries glyph id");
    require(latin.font_selection.resolved_face_id == latin.selected_face_id, "Latin shaping selection carries face");

    const render_text_font_fallback_shaped_glyph_input_record& hangul = inputs.inputs[1];
    require(hangul.ready_for_glyph_atlas(), "Hangul input is ready for fake atlas handoff");
    require(hangul.stable_run_key == plan.runs[1].stable_run_key, "Hangul input preserves fallback run key");
    require(
        hangul.stable_page_key == font_fallback_shaping_handoff_stable_page_key_for(plan.runs[1]),
        "Hangul input preserves stable page key");
    require(hangul.byte_offset == 1U && hangul.byte_count == 3U, "Hangul input preserves UTF-8 byte span");
    require(hangul.source_codepoint_index == 1U, "Hangul input records source scalar index");
    require(hangul.codepoint == 0xac00U, "Hangul input keeps codepoint");
    require(hangul.glyph_id == 0xac00U, "Hangul input maps glyph id deterministically");
    require(hangul.selected_face_id == 202U, "Hangul input selects fallback face");
    require(hangul.used_fallback, "Hangul input records fallback use");
    require(hangul.font_selection.used_codepoint_fallback, "Hangul shaping selection records fallback use");
    require(hangul.advance_x == 16.0f, "Hangul fake advance is deterministic");
    require(hangul.line_height == 16.0f, "Hangul line height falls back to font size");

    const render_text_font_fallback_shaped_glyph_input_record& trailing = inputs.inputs[2];
    require(trailing.stable_run_key == plan.runs[3].stable_run_key, "trailing Latin preserves final run key");
    require(trailing.byte_offset == 8U && trailing.byte_count == 1U, "trailing Latin preserves byte span");
    require(trailing.source_codepoint_index == 3U, "trailing Latin records source scalar index");
    require(trailing.selected_face_id == 201U, "trailing Latin returns to requested face");
}

void test_fallback_shaped_glyph_inputs_apply_face_local_glyph_mapping()
{
    using namespace quiz_vulkan::render;

    font_face_catalog catalog;
    catalog.add_face(font_face_descriptor{
        .id = 301,
        .family = "Mapped Sans",
        .source_uri = "fixture://fonts/mapped-sans",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'A'},
        },
        .weight = 400,
        .glyph_id_offset = 1000U,
    });

    render_text_style mapped_style = primary_style();
    mapped_style.id = "mapped";
    mapped_style.font_family = "Mapped Sans";

    render_text_style_catalog styles;
    styles.fallback_style = mapped_style;
    styles.styles.push_back(mapped_style);

    render_text_font_fallback_run_plan_request request;
    request.items.push_back(make_render_text_font_fallback_chain_plan_item(
        std::vector<render_text_run>{
            render_text_run{
                .text = "A",
                .style_token = "mapped",
            },
        },
        styles,
        "mapped-glyph-fixture",
        12U));

    const render_text_font_fallback_run_plan_snapshot plan =
        plan_render_text_font_fallback_runs(request, catalog);
    const render_text_font_fallback_shaping_handoff_snapshot handoff =
        make_render_text_font_fallback_shaping_handoff(plan);
    const render_text_font_fallback_shaped_glyph_input_snapshot inputs =
        make_render_text_font_fallback_shaped_glyph_inputs(
            render_text_font_fallback_shaped_glyph_input_request{
                .handoff = handoff,
                .items = request.items,
                .font_catalog = catalog,
            });

    require(inputs.ok(), "mapped glyph fixture produces complete shaped input snapshot");
    require(inputs.inputs.size() == 1U, "mapped glyph fixture emits one shaped input");
    require(inputs.policy.glyph_id_offset_input_count == 1U, "input policy counts face-local glyph mapping");

    const render_text_font_fallback_shaped_glyph_input_record& input = inputs.inputs.front();
    require(input.glyph_id == U'A' + 1000U, "shaped input applies face-local glyph id offset");
    require(input.cache_key.glyph_id == input.glyph_id, "atlas key uses mapped glyph id");
    require(input.glyph_id_from_selection, "input records selection-sourced glyph id");
    require(!input.glyph_id_matches_codepoint, "input records glyph id/codepoint mismatch");
    require(input.glyph_id_offset == 1000U, "input records glyph id offset");
    require(input.font_selection.glyph_id == input.glyph_id, "font selection uses mapped glyph id");
    require(input.font_selection.glyph_id_offset == 1000U, "font selection records glyph id offset");
    require(!input.font_selection.glyph_id_matches_codepoint, "font selection records mapped glyph id");
}

void test_fallback_shaped_glyph_execution_shapes_ready_inputs_and_keeps_blockers()
{
    using namespace quiz_vulkan::render;

    const font_face_catalog catalog = make_fallback_chain_catalog(false);
    const std::string text = std::string("A") + std::string("\xEA\xB0\x80", 3)
        + std::string("\xF0\x9F\x98\x80", 4) + std::string("B");
    render_text_font_fallback_run_plan_request request;
    request.items.push_back(make_render_text_font_fallback_chain_plan_item(
        std::vector<render_text_run>{
            render_text_run{
                .text = text,
                .style_token = "primary",
            },
        },
        fallback_chain_style_catalog(),
        "execution-fixture",
        13U));

    const render_text_font_fallback_run_plan_snapshot plan =
        plan_render_text_font_fallback_runs(request, catalog);
    const render_text_font_fallback_shaping_handoff_snapshot handoff =
        make_render_text_font_fallback_shaping_handoff(plan);
    const render_text_font_fallback_shaped_glyph_input_snapshot inputs =
        make_render_text_font_fallback_shaped_glyph_inputs(
            render_text_font_fallback_shaped_glyph_input_request{
                .handoff = handoff,
                .items = request.items,
                .font_catalog = catalog,
            });
    const render_text_font_fallback_shaped_glyph_execution_snapshot executions =
        execute_render_text_font_fallback_shaped_glyph_inputs(inputs);

    require(!executions.ok(), "blocked emoji input keeps execution snapshot incomplete");
    require(executions.has_executions(), "ready inputs produce deterministic execution records");
    require(executions.executions.size() == 3U, "ready Latin and Hangul inputs execute");
    require(executions.blocked_runs.size() == 1U, "blocked emoji run is carried to execution snapshot");
    require(executions.policy.input_count == 3U, "execution policy records input count");
    require(executions.policy.execution_count == 3U, "execution policy records execution count");
    require(executions.policy.shaped_count == 3U, "all emitted inputs shape deterministically");
    require(executions.policy.blocked_input_count == 1U, "blocked handoff input is counted");
    require(executions.policy.atlas_ready_count == 3U, "all executed fixture glyphs are atlas-ready");
    require(executions.policy.missing_cache_key_count == 0U, "cacheable fixture glyphs keep cache keys");
    require(executions.policy.fallback_execution_count == 1U, "only Hangul execution uses fallback");
    require(executions.policy.unique_input_key_count == 3U, "execution snapshot records unique input keys");
    require(executions.policy.unique_page_key_count == 2U, "execution snapshot records page evidence");
    require(executions.policy.unique_cache_key_count == 3U, "execution snapshot records cache key evidence");
    require(executions.policy.unique_selected_face_count == 2U, "execution snapshot records selected faces");

    const render_text_font_fallback_shaped_glyph_execution_record& latin = executions.executions[0];
    require(
        latin.status == render_text_font_fallback_shaped_glyph_execution_status::shaped,
        "Latin execution status is shaped");
    require(latin.executed(), "Latin execution reports executed");
    require(latin.ready_for_glyph_atlas(), "Latin execution is ready for atlas diagnostics");
    require(latin.stable_input_key == inputs.inputs[0].stable_input_key, "Latin execution preserves input key");
    require(!latin.stable_execution_key.empty(), "Latin execution has stable execution key");
    require(latin.shaped_glyph.run_index == 0U, "Latin shaped glyph records source run index");
    require(latin.shaped_glyph.glyph_index == 0U, "Latin shaped glyph records source scalar index");
    require(latin.shaped_glyph.byte_offset == 0U, "Latin shaped glyph records byte offset");
    require(latin.shaped_glyph.glyph_id == U'A', "Latin shaped glyph records glyph id");
    require(latin.shaped_glyph.resolved_face_id == 201U, "Latin shaped glyph records selected face");
    require(latin.shaped_glyph.glyph_supported, "Latin shaped glyph is supported");
    require(!latin.shaped_glyph.used_codepoint_fallback, "Latin shaped glyph does not use fallback");

    const render_text_font_fallback_shaped_glyph_execution_record& hangul = executions.executions[1];
    require(hangul.used_fallback, "Hangul execution records fallback use");
    require(hangul.shaped_glyph.codepoint == 0xac00U, "Hangul shaped glyph records codepoint");
    require(hangul.shaped_glyph.glyph_id == 0xac00U, "Hangul shaped glyph records glyph id");
    require(hangul.shaped_glyph.resolved_face_id == 202U, "Hangul shaped glyph records fallback face");
    require(hangul.shaped_glyph.used_codepoint_fallback, "Hangul shaped glyph records codepoint fallback");
    require(hangul.shaped_glyph.advance_x == 16.0f, "Hangul shaped glyph records deterministic fake advance");
}

void test_fallback_shaped_glyph_execution_records_uncacheable_combining_mark()
{
    using namespace quiz_vulkan::render;

    font_face_catalog catalog;
    catalog.add_face(font_face_descriptor{
        .id = 401,
        .family = "Primary Sans",
        .source_uri = "fixture://fonts/primary-combining",
        .version = "fixture-1",
        .license = "test-fixture",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'A'},
            font_codepoint_range{.first = 0x0301U, .last = 0x0301U},
        },
        .weight = 400,
    });

    const std::string text = std::string("A") + std::string("\xCC\x81", 2);
    render_text_font_fallback_run_plan_request request;
    request.items.push_back(make_render_text_font_fallback_chain_plan_item(
        std::vector<render_text_run>{
            render_text_run{
                .text = text,
                .style_token = "primary",
            },
        },
        fallback_chain_style_catalog(),
        "combining-execution-fixture",
        14U));

    const render_text_font_fallback_run_plan_snapshot plan =
        plan_render_text_font_fallback_runs(request, catalog);
    const render_text_font_fallback_shaping_handoff_snapshot handoff =
        make_render_text_font_fallback_shaping_handoff(plan);
    const render_text_font_fallback_shaped_glyph_input_snapshot inputs =
        make_render_text_font_fallback_shaped_glyph_inputs(
            render_text_font_fallback_shaped_glyph_input_request{
                .handoff = handoff,
                .items = request.items,
                .font_catalog = catalog,
            });
    const render_text_font_fallback_shaped_glyph_execution_snapshot executions =
        execute_render_text_font_fallback_shaped_glyph_inputs(inputs);

    require(inputs.ok(), "combining fixture has no blocked shaped inputs");
    require(executions.ok(), "combining fixture executes despite missing atlas key");
    require(executions.executions.size() == 2U, "base and combining mark both execute");
    require(executions.policy.shaped_count == 2U, "execution policy counts both shaped glyphs");
    require(executions.policy.atlas_ready_count == 1U, "only the base glyph is atlas-ready");
    require(executions.policy.missing_cache_key_count == 1U, "zero-advance combining mark is counted without cache key");
    require(executions.policy.unique_cache_key_count == 1U, "only the base glyph contributes cache key evidence");

    const render_text_font_fallback_shaped_glyph_execution_record& mark = executions.executions[1];
    require(mark.executed(), "combining mark execution reports shaped");
    require(!mark.ready_for_glyph_atlas(), "combining mark execution is not atlas-ready");
    require(!mark.has_cache_key, "combining mark execution records missing cache key");
    require(mark.shaped_glyph.codepoint == 0x0301U, "combining mark shaped glyph keeps codepoint");
    require(mark.shaped_glyph.glyph_id == 0x0301U, "combining mark shaped glyph keeps glyph id");
    require(mark.shaped_glyph.zero_advance, "combining mark shaped glyph records zero advance");
    require(mark.shaped_glyph.combining_mark, "combining mark shaped glyph records combining status");
    require(mark.shaped_glyph.cluster_codepoint_offset == 1U, "combining mark shaped glyph records source scalar offset");
}

void test_fallback_shaped_glyph_execution_diff_reports_missing_to_covered_transition()
{
    using namespace quiz_vulkan::render;

    const std::string text = std::string("A") + std::string("\xEA\xB0\x80", 3)
        + std::string("\xF0\x9F\x98\x80", 4) + std::string("B");
    const render_text_font_fallback_shaped_glyph_execution_snapshot before =
        make_fallback_execution_snapshot(
            text,
            make_fallback_chain_catalog(false),
            "execution-diff-fixture",
            15U);
    const render_text_font_fallback_shaped_glyph_execution_snapshot after =
        make_fallback_execution_snapshot(
            text,
            make_fallback_chain_catalog(true),
            "execution-diff-fixture",
            15U);

    const render_text_font_fallback_shaped_glyph_execution_diff_snapshot diff =
        diff_render_text_font_fallback_shaped_glyph_execution_snapshots(before, after);

    require(!before.ok(), "baseline execution snapshot keeps the missing emoji blocked");
    require(after.ok(), "covered execution snapshot has no blocked runs");
    require(before.executions.size() == 3U, "baseline execution snapshot shapes only covered runs");
    require(after.executions.size() == 4U, "covered execution snapshot shapes the emoji run");
    require(diff.has_changes(), "execution diff reports the coverage transition");
    require(diff.policy.input_count_delta == 1, "execution diff reports the added shaped input");
    require(diff.policy.execution_count_delta == 1, "execution diff reports the added execution");
    require(diff.policy.shaped_count_delta == 1, "execution diff reports the added shaped glyph");
    require(diff.policy.glyph_count_delta == 1, "execution diff mirrors shaped glyph count changes");
    require(diff.policy.blocked_input_count_delta == -1, "execution diff reports one fewer blocked input");
    require(diff.policy.blocked_run_count_delta == -1, "execution diff reports one fewer blocked run");
    require(diff.policy.added_execution_count == 1U, "execution diff counts the added emoji execution");
    require(diff.policy.removed_execution_count == 0U, "execution diff does not remove ready executions");
    require(diff.policy.changed_execution_count == 0U, "execution diff keeps existing executions unchanged");
    require(diff.policy.added_blocked_run_count == 0U, "execution diff does not add blocked runs");
    require(diff.policy.removed_blocked_run_count == 1U, "execution diff counts the removed blocked run");
    require(diff.policy.status_counts_changed, "execution diff reports status-count changes");
    require(diff.policy.blocked_runs_changed, "execution diff reports blocked-run changes");
    require(diff.policy.glyph_count_changed, "execution diff reports glyph-count changes");
    require(diff.policy.unique_selected_face_count_delta == 1, "execution diff reports new selected face evidence");
    require(diff.policy.unique_cache_key_count_delta == 1, "execution diff reports new cache key evidence");
    require(diff.policy.unique_page_key_count_delta == 1, "execution diff reports new page key evidence");
    require(diff.added_execution_keys.size() == 1U, "execution diff exposes the added execution key");
    require(diff.removed_blocked_run_keys.size() == 1U, "execution diff exposes the removed blocked-run key");
    require(!diff.diagnostic_reasons.empty(), "execution diff records stable diagnostic reasons");

    const render_text_font_fallback_shaped_glyph_execution_record_diff* added = nullptr;
    for (const render_text_font_fallback_shaped_glyph_execution_record_diff& execution_diff :
         diff.execution_diffs) {
        if (execution_diff.added) {
            added = &execution_diff;
            break;
        }
    }

    require(added != nullptr, "execution diff includes an added execution record");
    require(added->current_selected_face_id == 203U, "added execution records emoji selected face evidence");
    require(
        added->current_status == render_text_font_fallback_shaped_glyph_execution_status::shaped,
        "added execution records shaped status");
    require(
        added->diagnostic_reason.find("added execution") != std::string::npos,
        "added execution records stable diagnostic reason");
}

void test_fallback_shaped_glyph_execution_diff_reports_selected_face_cache_and_page_changes()
{
    using namespace quiz_vulkan::render;

    const std::string text = std::string("\xEA\xB0\x80", 3);
    const render_text_font_fallback_shaped_glyph_execution_snapshot before =
        make_fallback_execution_snapshot(
            text,
            make_fallback_chain_catalog(false),
            "execution-face-diff-fixture",
            16U);
    const render_text_font_fallback_shaped_glyph_execution_snapshot after =
        make_fallback_execution_snapshot(
            text,
            make_alternate_hangul_fallback_catalog(),
            "execution-face-diff-fixture",
            16U);

    const render_text_font_fallback_shaped_glyph_execution_diff_snapshot diff =
        diff_render_text_font_fallback_shaped_glyph_execution_snapshots(before, after);

    require(before.ok(), "baseline Hangul execution snapshot is ready");
    require(after.ok(), "alternate Hangul execution snapshot is ready");
    require(before.executions.size() == 1U, "baseline Hangul fixture has one execution");
    require(after.executions.size() == 1U, "alternate Hangul fixture has one execution");
    require(diff.has_changes(), "execution diff reports selected face evidence changes");
    require(diff.policy.added_execution_count == 0U, "execution diff does not add the stable Hangul execution");
    require(diff.policy.removed_execution_count == 0U, "execution diff does not remove the stable Hangul execution");
    require(diff.policy.changed_execution_count == 1U, "execution diff counts the changed Hangul execution");
    require(diff.policy.selected_face_changed_count == 1U, "execution diff counts selected face changes");
    require(diff.policy.cache_key_changed_count == 1U, "execution diff counts cache key changes");
    require(diff.policy.page_key_changed_count == 1U, "execution diff counts page key changes");
    require(diff.policy.style_token_changed_count == 0U, "execution diff does not report style changes");
    require(diff.policy.glyph_id_changed_count == 0U, "execution diff keeps glyph id stable");
    require(diff.policy.status_changed_count == 0U, "execution diff keeps execution status stable");
    require(diff.policy.selected_face_set_changed, "execution diff reports selected face set changes");
    require(diff.policy.cache_key_set_changed, "execution diff reports cache key set changes");
    require(diff.policy.page_key_set_changed, "execution diff reports page key set changes");
    require(!diff.policy.style_token_set_changed, "execution diff keeps style token set stable");
    require(!diff.policy.status_counts_changed, "execution diff keeps status counts stable");
    require(!diff.policy.glyph_count_changed, "execution diff keeps glyph count stable");
    require(diff.changed_execution_keys.size() == 1U, "execution diff exposes the changed execution key");

    const render_text_font_fallback_shaped_glyph_execution_record_diff& execution_diff =
        diff.execution_diffs.front();
    require(execution_diff.changed, "Hangul execution diff is marked changed");
    require(execution_diff.previous_selected_face_id == 202U, "execution diff records previous selected face");
    require(execution_diff.current_selected_face_id == 204U, "execution diff records current selected face");
    require(
        !(execution_diff.previous_cache_key == execution_diff.current_cache_key),
        "execution diff records changed cache key evidence");
    require(
        execution_diff.previous_page_key != execution_diff.current_page_key,
        "execution diff records changed page key evidence");
    require(
        execution_diff.diagnostic_reason.find("selected face changed") != std::string::npos,
        "execution diff records stable selected-face diagnostic reason");
}

void test_fallback_shaped_glyph_execution_diff_reports_style_token_changes()
{
    using namespace quiz_vulkan::render;

    const render_text_font_fallback_shaped_glyph_execution_snapshot before =
        make_fallback_execution_snapshot(
            "A",
            make_fallback_chain_catalog(false),
            "execution-style-diff-fixture",
            17U);
    render_text_font_fallback_shaped_glyph_execution_snapshot after = before;
    after.executions.front().style_token = "alternate-style";
    after.style_tokens = {"alternate-style"};

    const render_text_font_fallback_shaped_glyph_execution_diff_snapshot diff =
        diff_render_text_font_fallback_shaped_glyph_execution_snapshots(before, after);

    require(before.ok(), "baseline style fixture is ready");
    require(after.ok(), "mutated style fixture remains ready");
    require(diff.has_changes(), "execution diff reports style token evidence changes");
    require(diff.policy.changed_execution_count == 1U, "execution diff counts the changed style execution");
    require(diff.policy.style_token_changed_count == 1U, "execution diff counts style token changes");
    require(diff.policy.style_token_set_changed, "execution diff reports style token set changes");
    require(diff.policy.added_execution_count == 0U, "execution diff does not add executions for style mutation");
    require(diff.policy.removed_execution_count == 0U, "execution diff does not remove executions for style mutation");
    require(!diff.policy.status_counts_changed, "execution diff keeps style-only status counts stable");
    require(!diff.policy.glyph_count_changed, "execution diff keeps style-only glyph count stable");
    require(diff.execution_diffs.front().previous_style_token == "primary", "execution diff records old style token");
    require(
        diff.execution_diffs.front().current_style_token == "alternate-style",
        "execution diff records new style token");
    require(
        diff.execution_diffs.front().diagnostic_reason.find("style token changed") != std::string::npos,
        "execution diff records stable style-token diagnostic reason");
}

} // namespace

int main()
{
    test_latin_stays_on_requested_face_and_merges_contiguous_codepoints();
    test_hangul_and_non_bmp_fall_back_to_adapted_coverage_face();
    test_invalid_utf8_produces_unsupported_segment_diagnostic();
    test_unsupported_codepoint_produces_unsupported_segment_diagnostic();
    test_adjacent_requested_and_fallback_faces_form_separate_merged_runs();
    test_combining_cluster_uses_one_fallback_face_for_coverage();
    test_fallback_chain_plans_mixed_batch_selected_face_order_before_shaping();
    test_fallback_chain_reports_missing_emoji_without_claiming_support();
    test_fallback_run_plan_splits_latin_hangul_and_missing_ranges();
    test_fallback_run_plan_merges_contiguous_ranges_for_same_selected_face();
    test_fallback_run_plan_keeps_combining_cluster_on_one_face();
    test_fallback_run_plan_diff_reports_catalog_coverage_change();
    test_fallback_shaping_handoff_summarizes_ready_and_blocked_runs();
    test_fallback_shaping_handoff_reports_invalid_utf8_and_no_selected_face();
    test_fallback_shaped_glyph_inputs_materialize_ready_runs_only();
    test_fallback_shaped_glyph_inputs_apply_face_local_glyph_mapping();
    test_fallback_shaped_glyph_execution_shapes_ready_inputs_and_keeps_blockers();
    test_fallback_shaped_glyph_execution_records_uncacheable_combining_mark();
    test_fallback_shaped_glyph_execution_diff_reports_missing_to_covered_transition();
    test_fallback_shaped_glyph_execution_diff_reports_selected_face_cache_and_page_changes();
    test_fallback_shaped_glyph_execution_diff_reports_style_token_changes();
    return 0;
}
