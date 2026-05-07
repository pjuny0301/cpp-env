#include "render/text/font_backend_capabilities.h"
#include "render/text/font_shaping_backend.h"

#include <cassert>
#include <cmath>
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

quiz_vulkan::render::render_text_style shaping_style()
{
    return quiz_vulkan::render::render_text_style{
        .id = "body",
        .font_family = "Shape Sans",
        .font_size = 20.0f,
        .line_height = 24.0f,
        .letter_spacing = 0.0f,
        .font_weight = 400,
        .italic = false,
    };
}

std::vector<quiz_vulkan::render::render_text_font_shaping_codepoint_selection> selections_for(
    const std::vector<quiz_vulkan::render::utf8_text_codepoint>& codepoints,
    quiz_vulkan::render::font_face_id face_id,
    bool glyph_supported = true)
{
    std::vector<quiz_vulkan::render::render_text_font_shaping_codepoint_selection> selections;
    selections.reserve(codepoints.size());
    for (std::size_t index = 0; index < codepoints.size(); ++index) {
        selections.push_back(quiz_vulkan::render::render_text_font_shaping_codepoint_selection{
            .requested_face_id = face_id,
            .resolved_face_id = face_id,
            .glyph_supported = glyph_supported,
            .used_codepoint_fallback = false,
        });
    }
    return selections;
}

quiz_vulkan::render::render_text_font_shaping_request request_for(
    const std::string& text,
    std::vector<quiz_vulkan::render::render_text_font_shaping_codepoint_selection> selections = {})
{
    using namespace quiz_vulkan::render;

    std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(text);
    if (selections.empty()) {
        selections = selections_for(codepoints, 41);
    }

    return render_text_font_shaping_request{
        .run_index = 2,
        .style_token = "body",
        .style = shaping_style(),
        .codepoints = codepoints,
        .clusters = cluster_utf8_text_run(codepoints),
        .font_selections = selections,
    };
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

quiz_vulkan::render::render_text_font_backend_component harfbuzz_component(
    quiz_vulkan::render::render_text_font_backend_version version =
        quiz_vulkan::render::render_text_font_backend_version{.major = 8, .minor = 3, .patch = 0})
{
    using namespace quiz_vulkan::render;

    return render_text_font_backend_component{
        .library = render_text_font_backend_library::harfbuzz,
        .name = "HarfBuzz",
        .available = true,
        .version = version,
        .features = {
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_shaping,
            render_text_font_backend_feature::complex_script_shaping,
        },
        .diagnostic = "HarfBuzz probe fixture is available",
    };
}

void test_font_backend_probe_reports_available_real_stack()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_backend_capability_probe probe({
        freetype_component(),
        harfbuzz_component(),
    });
    const render_text_font_backend_capability_snapshot capability = probe.probe(
        render_text_font_backend_capability_probe_request{
            .required_libraries = {
                render_text_font_backend_library::freetype,
                render_text_font_backend_library::harfbuzz,
            },
            .required_features = {
                render_text_font_backend_feature::font_file_loading,
                render_text_font_backend_feature::glyph_rasterization,
                render_text_font_backend_feature::glyph_shaping,
                render_text_font_backend_feature::complex_script_shaping,
            },
            .minimum_versions = {
                render_text_font_backend_minimum_version{
                    .library = render_text_font_backend_library::freetype,
                    .version = render_text_font_backend_version{.major = 2, .minor = 12, .patch = 0},
                },
                render_text_font_backend_minimum_version{
                    .library = render_text_font_backend_library::harfbuzz,
                    .version = render_text_font_backend_version{.major = 8, .minor = 0, .patch = 0},
                },
            },
        });

    require(capability.ok(), "available real stack reports ok");
    require(
        capability.status == render_text_font_backend_capability_status::available,
        "available real stack preserves available status");
    require(capability.missing_libraries.empty(), "available real stack reports no missing libraries");
    require(capability.missing_features.empty(), "available real stack reports no missing features");
    require(capability.version_mismatches.empty(), "available real stack reports no version mismatch");
    require(
        capability.supports_feature(render_text_font_backend_feature::complex_script_shaping),
        "available real stack supports complex shaping");
    require(
        render_text_font_backend_library_name(render_text_font_backend_library::harfbuzz) == "harfbuzz",
        "library names keep backend dependency out of app code");
    require(
        render_text_font_backend_feature_name(render_text_font_backend_feature::glyph_rasterization)
            == "glyph_rasterization",
        "feature names are stable for diagnostics");

    const render_text_font_backend_shaping_capability shaping =
        render_text_font_backend_capability_to_shaping(capability);
    require(shaping.backend_available, "available real stack enables shaping backend");
    require(shaping.support_complex_scripts, "available real stack enables complex script shaping");
    require(!shaping.fallback_only, "available real stack is not fallback-only");

    const deterministic_fake_font_shaping_backend backend;
    const render_text_font_shaping_result result =
        backend.shape(apply_render_text_font_backend_capability(request_for("\xd8\xa7"), capability));
    require(result.ok(), "capability bridge lets fake backend shape complex script when real support is present");
}

void test_font_backend_probe_reports_unavailable_directwrite()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_backend_capability_probe probe({
        render_text_font_backend_component{
            .library = render_text_font_backend_library::directwrite,
            .name = "DirectWrite",
            .available = false,
            .version = render_text_font_backend_version{.major = 0, .minor = 0, .patch = 0},
            .features = {
                render_text_font_backend_feature::glyph_shaping,
                render_text_font_backend_feature::complex_script_shaping,
            },
            .diagnostic = "DirectWrite is not available on this fixture platform",
        },
    });
    const render_text_font_backend_capability_snapshot capability = probe.probe(
        render_text_font_backend_capability_probe_request{
            .required_libraries = {
                render_text_font_backend_library::directwrite,
            },
            .required_features = {
                render_text_font_backend_feature::glyph_shaping,
            },
        });

    require(!capability.ok(), "unavailable DirectWrite stack does not report ok");
    require(
        capability.status == render_text_font_backend_capability_status::unavailable,
        "unavailable DirectWrite stack preserves unavailable status");
    require(capability.missing_libraries.size() == 1U, "unavailable DirectWrite stack reports one missing library");
    require(
        capability.missing_libraries.front() == render_text_font_backend_library::directwrite,
        "unavailable DirectWrite stack names missing library");

    const render_text_font_backend_shaping_capability shaping =
        render_text_font_backend_capability_to_shaping(capability);
    require(!shaping.backend_available, "unavailable DirectWrite stack disables shaping backend");
    require(!shaping.support_complex_scripts, "unavailable DirectWrite stack disables complex shaping");

    const deterministic_fake_font_shaping_backend backend;
    const render_text_font_shaping_result result =
        backend.shape(apply_render_text_font_backend_capability(request_for("A"), capability));
    require(
        result.status == render_text_font_shaping_backend_status::backend_unavailable,
        "capability bridge maps unavailable backend into shaping diagnostic");
}

void test_font_backend_probe_reports_version_mismatch_and_unsupported_feature()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_backend_capability_probe old_harfbuzz_probe({
        freetype_component(),
        harfbuzz_component(render_text_font_backend_version{.major = 7, .minor = 1, .patch = 0}),
    });
    const render_text_font_backend_capability_snapshot version_mismatch =
        old_harfbuzz_probe.probe(render_text_font_backend_capability_probe_request{
            .required_libraries = {
                render_text_font_backend_library::harfbuzz,
            },
            .required_features = {
                render_text_font_backend_feature::glyph_shaping,
            },
            .minimum_versions = {
                render_text_font_backend_minimum_version{
                    .library = render_text_font_backend_library::harfbuzz,
                    .version = render_text_font_backend_version{.major = 8, .minor = 0, .patch = 0},
                },
            },
        });

    require(!version_mismatch.ok(), "old HarfBuzz stack does not report ok");
    require(
        version_mismatch.status == render_text_font_backend_capability_status::version_mismatch,
        "old HarfBuzz stack preserves version mismatch status");
    require(version_mismatch.version_mismatches.size() == 1U, "old HarfBuzz stack records one mismatch");
    require(
        version_mismatch.version_mismatches.front().library == render_text_font_backend_library::harfbuzz,
        "old HarfBuzz stack records mismatched library");
    require(
        version_mismatch.version_mismatches.front().actual.display_label() == "7.1.0",
        "old HarfBuzz stack records actual version");

    const deterministic_fake_font_backend_capability_probe limited_probe({
        freetype_component(),
        harfbuzz_component(),
    });
    const render_text_font_backend_capability_snapshot unsupported_feature =
        limited_probe.probe(render_text_font_backend_capability_probe_request{
            .required_libraries = {
                render_text_font_backend_library::freetype,
                render_text_font_backend_library::harfbuzz,
            },
            .required_features = {
                render_text_font_backend_feature::variable_fonts,
            },
        });

    require(!unsupported_feature.ok(), "unsupported feature stack does not report ok");
    require(
        unsupported_feature.status == render_text_font_backend_capability_status::unsupported_feature,
        "unsupported feature stack preserves unsupported feature status");
    require(unsupported_feature.missing_features.size() == 1U, "unsupported feature stack records missing feature");
    require(
        unsupported_feature.missing_features.front() == render_text_font_backend_feature::variable_fonts,
        "unsupported feature stack names missing variable font support");
    require(
        render_text_font_backend_capability_status_name(unsupported_feature.status) == "unsupported_feature",
        "capability status names are stable for diagnostics");
}

void test_font_backend_probe_reports_fallback_only_mode()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_backend_capability_probe probe;
    const render_text_font_backend_capability_snapshot capability = probe.probe(
        render_text_font_backend_capability_probe_request{
            .required_libraries = {
                render_text_font_backend_library::deterministic_fake,
            },
            .required_features = {
                render_text_font_backend_feature::glyph_id_mapping,
                render_text_font_backend_feature::glyph_shaping,
            },
        });

    require(!capability.ok(), "fallback-only backend is intentionally not a real backend ok state");
    require(capability.can_shape_with_fallback(), "fallback-only backend can still drive deterministic tests");
    require(
        capability.status == render_text_font_backend_capability_status::fallback_only,
        "fallback-only backend preserves fallback-only status");
    require(capability.fallback_only, "fallback-only backend records fallback-only mode");
    require(capability.missing_features.empty(), "fallback-only backend satisfies requested fake features");

    const render_text_font_backend_shaping_capability shaping =
        render_text_font_backend_capability_to_shaping(capability);
    require(shaping.backend_available, "fallback-only backend keeps fake shaping path available");
    require(!shaping.support_complex_scripts, "fallback-only backend does not claim complex script shaping");
    require(shaping.fallback_only, "fallback-only bridge records fallback-only mode");

    const deterministic_fake_font_shaping_backend backend;
    render_text_font_shaping_request request =
        apply_render_text_font_backend_capability(request_for("\xd8\xa7"), capability);
    request.fallback_glyph_id = 0;
    const render_text_font_shaping_result result = backend.shape(request);
    require(
        result.status == render_text_font_shaping_backend_status::unsupported_script,
        "fallback-only backend keeps complex scripts on diagnostic fallback path");
}

void test_fake_backend_shapes_successful_latin_hangul_and_non_bmp_run()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_shaping_backend backend;
    const std::string text = "A" "\xed\x95\x9c" "\xf0\x9f\x98\x80";
    const render_text_font_shaping_result result = backend.shape(request_for(text));

    require(result.ok(), "successful shaping run reports ok");
    require(result.status == render_text_font_shaping_backend_status::shaped, "successful run has shaped status");
    require(result.has_diagnostic(render_text_font_shaping_backend_status::shaped), "successful run is diagnosed");
    require(result.glyphs.size() == 3U, "successful run emits three glyphs");
    require(result.policy.run_count == 1U, "successful run counts one backend request");
    require(result.policy.shaped_run_count == 1U, "successful run counts one shaped run");
    require(result.policy.codepoint_count == 3U, "successful run counts codepoints");
    require(result.policy.supported_glyph_count == 3U, "successful run counts supported glyphs");

    require(result.glyphs[0].byte_offset == 0U && result.glyphs[0].byte_count == 1U, "Latin byte range is stable");
    require(result.glyphs[0].glyph_id == U'A', "Latin glyph id follows codepoint in fake backend");
    require(near(result.glyphs[0].advance_x, 10.0f), "Latin advance is deterministic");

    require(result.glyphs[1].byte_offset == 1U && result.glyphs[1].byte_count == 3U, "Hangul byte range is stable");
    require(result.glyphs[1].glyph_id == 0xd55cU, "Hangul glyph id follows codepoint");
    require(near(result.glyphs[1].advance_x, 20.0f), "Hangul advance is full width");

    require(result.glyphs[2].byte_offset == 4U && result.glyphs[2].byte_count == 4U, "non-BMP byte range is stable");
    require(result.glyphs[2].glyph_id == 0x1f600U, "non-BMP glyph id follows codepoint");
    require(near(result.glyphs[2].advance_x, 20.0f), "non-BMP fake advance is full width");
}

void test_fake_backend_reports_backend_unavailable()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_shaping_backend backend;
    render_text_font_shaping_request request = request_for("A");
    request.backend_available = false;
    const render_text_font_shaping_result result = backend.shape(request);

    require(!result.ok(), "unavailable backend does not report ok");
    require(
        result.status == render_text_font_shaping_backend_status::backend_unavailable,
        "unavailable backend status is preserved");
    require(result.glyphs.empty(), "unavailable backend emits no glyphs");
    require(
        result.has_diagnostic(render_text_font_shaping_backend_status::backend_unavailable),
        "unavailable backend diagnostic is recorded");
    require(result.policy.backend_unavailable_count == 1U, "unavailable backend count is recorded");
}

void test_fake_backend_reports_unsupported_script_and_fallback_glyph_id()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_shaping_backend backend;
    render_text_font_shaping_request request = request_for("\xd8\xa7");
    request.support_complex_scripts = false;
    request.fallback_glyph_id = 0;
    const render_text_font_shaping_result result = backend.shape(request);

    require(!result.ok(), "unsupported script does not report ok");
    require(
        result.status == render_text_font_shaping_backend_status::unsupported_script,
        "unsupported script status is preserved");
    require(result.glyphs.size() == 1U, "unsupported script still emits a diagnostic glyph");
    require(!result.glyphs[0].glyph_supported, "unsupported script does not claim glyph support");
    require(result.glyphs[0].used_fallback_glyph_id, "unsupported script uses fallback glyph id");
    require(result.glyphs[0].glyph_id == 0U, "unsupported script fallback glyph id is stable");
    require(
        result.has_diagnostic(render_text_font_shaping_backend_status::unsupported_script),
        "unsupported script diagnostic is recorded");
    require(
        result.has_diagnostic(render_text_font_shaping_backend_status::fallback_glyph_id),
        "fallback glyph id diagnostic is recorded");
    require(result.policy.unsupported_script_count == 1U, "unsupported script count is recorded");
    require(result.policy.fallback_glyph_id_count == 1U, "fallback glyph id count is recorded");
}

void test_fake_backend_reports_unsupported_glyph_and_fallback_glyph_id()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_shaping_backend backend;
    std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run("?");
    render_text_font_shaping_request request = request_for("?", selections_for(codepoints, 41, false));
    request.fallback_glyph_id = 0;
    const render_text_font_shaping_result result = backend.shape(request);

    require(!result.ok(), "unsupported glyph does not report ok");
    require(
        result.status == render_text_font_shaping_backend_status::unsupported_glyph,
        "unsupported glyph status is preserved");
    require(result.glyphs.size() == 1U, "unsupported glyph emits one fallback glyph");
    require(!result.glyphs[0].glyph_supported, "unsupported glyph does not claim support");
    require(result.glyphs[0].glyph_id == 0U, "unsupported glyph uses fallback glyph id");
    require(
        result.has_diagnostic(render_text_font_shaping_backend_status::unsupported_glyph),
        "unsupported glyph diagnostic is recorded");
    require(
        result.has_diagnostic(render_text_font_shaping_backend_status::fallback_glyph_id),
        "fallback glyph id diagnostic is recorded for unsupported glyph");
    require(result.policy.unsupported_glyph_count == 1U, "unsupported glyph count is recorded");
}

void test_fake_backend_consumes_pre_resolved_glyph_ids()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_shaping_backend backend;
    std::vector<render_text_font_shaping_codepoint_selection> selections;
    selections.push_back(render_text_font_shaping_codepoint_selection{
        .requested_face_id = 41,
        .resolved_face_id = 41,
        .glyph_id = 1234,
        .has_glyph_id = true,
        .glyph_supported = true,
    });
    selections.push_back(render_text_font_shaping_codepoint_selection{
        .requested_face_id = 41,
        .resolved_face_id = 41,
        .glyph_id = 99,
        .has_glyph_id = true,
        .glyph_supported = false,
    });

    const render_text_font_shaping_result result = backend.shape(request_for("A?", selections));

    require(!result.ok(), "unsupported pre-resolved glyph keeps shaping result diagnostic");
    require(result.glyphs.size() == 2U, "pre-resolved glyph fixture emits two glyphs");
    require(result.glyphs[0].glyph_id == 1234U, "fake backend consumes supported pre-resolved glyph id");
    require(result.glyphs[0].glyph_supported, "supported pre-resolved glyph claims support");
    require(result.glyphs[1].glyph_id == 99U, "fake backend consumes unsupported fallback glyph id from selection");
    require(!result.glyphs[1].glyph_supported, "unsupported pre-resolved glyph does not claim support");
    require(result.glyphs[1].used_fallback_glyph_id, "unsupported pre-resolved glyph records fallback glyph use");
    require(result.policy.fallback_glyph_id_count == 1U, "pre-resolved fallback glyph id is counted");
}

void test_fake_backend_reports_zero_advance_combining_mark_with_cluster_range()
{
    using namespace quiz_vulkan::render;

    const deterministic_fake_font_shaping_backend backend;
    const render_text_font_shaping_result result = backend.shape(request_for("e\xcc\x81"));

    require(result.ok(), "combining mark run remains shapeable");
    require(result.glyphs.size() == 2U, "combining mark run emits base and mark glyphs");
    require(result.glyphs[1].combining_mark, "second glyph records combining mark");
    require(result.glyphs[1].zero_advance, "combining mark records zero advance");
    require(near(result.glyphs[1].advance_x, 0.0f), "combining mark advance is zero");
    require(result.glyphs[1].cluster_byte_offset == 0U, "combining mark stays in base cluster");
    require(result.glyphs[1].cluster_byte_count == 3U, "combining mark cluster spans base and mark bytes");
    require(result.glyphs[1].cluster_codepoint_offset == 0U, "combining mark cluster starts at base codepoint");
    require(result.glyphs[1].cluster_codepoint_count == 2U, "combining mark cluster spans two codepoints");
    require(
        result.has_diagnostic(render_text_font_shaping_backend_status::zero_advance_combining_mark),
        "zero advance combining mark diagnostic is recorded");
    require(
        result.policy.zero_advance_combining_mark_count == 1U,
        "zero advance combining mark count is recorded");
    require(result.policy.shaped_run_count == 1U, "combining run still counts as successfully shaped");
}

} // namespace

int main()
{
    test_font_backend_probe_reports_available_real_stack();
    test_font_backend_probe_reports_unavailable_directwrite();
    test_font_backend_probe_reports_version_mismatch_and_unsupported_feature();
    test_font_backend_probe_reports_fallback_only_mode();
    test_fake_backend_shapes_successful_latin_hangul_and_non_bmp_run();
    test_fake_backend_reports_backend_unavailable();
    test_fake_backend_reports_unsupported_script_and_fallback_glyph_id();
    test_fake_backend_reports_unsupported_glyph_and_fallback_glyph_id();
    test_fake_backend_consumes_pre_resolved_glyph_ids();
    test_fake_backend_reports_zero_advance_combining_mark_with_cluster_range();
    return 0;
}
