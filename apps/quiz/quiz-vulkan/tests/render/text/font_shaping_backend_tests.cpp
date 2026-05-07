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
    test_fake_backend_shapes_successful_latin_hangul_and_non_bmp_run();
    test_fake_backend_reports_backend_unavailable();
    test_fake_backend_reports_unsupported_script_and_fallback_glyph_id();
    test_fake_backend_reports_unsupported_glyph_and_fallback_glyph_id();
    test_fake_backend_consumes_pre_resolved_glyph_ids();
    test_fake_backend_reports_zero_advance_combining_mark_with_cluster_range();
    return 0;
}
