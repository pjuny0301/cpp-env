#include "render/text/font_backend_adapter.h"
#include "render/text/font_backend_capabilities.h"
#include "render/text/font_backend_dependency.h"
#include "render/text/font_backend_selection.h"
#include "render/text/font_shaping_backend.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <iterator>
#include <span>
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

bool near(float actual, float expected)
{
    return std::fabs(actual - expected) < 0.001f;
}

std::string read_text_owned_header(const std::string& header_name)
{
    const std::filesystem::path test_path = std::filesystem::path(__FILE__);
    const std::filesystem::path project_root =
        test_path.parent_path().parent_path().parent_path().parent_path();
    const std::filesystem::path header_path =
        project_root / "src" / "render" / "text" / header_name;
    std::ifstream input(header_path);
    require(input.good(), "text-owned public header can be opened for boundary inspection");
    return std::string(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{});
}

std::vector<quiz_vulkan::render::render_text_external_font_backend_probe_result>
probe_external_backend_stack(
    quiz_vulkan::render::render_text_external_font_backend_manifest manifest,
    std::vector<quiz_vulkan::render::render_text_font_backend_minimum_version> minimum_versions = {})
{
    using namespace quiz_vulkan::render;

    const manifest_font_backend_dependency_probe probe(std::move(manifest));
    return {
        probe.probe(render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::shaping,
            .minimum_versions = minimum_versions,
        }),
        probe.probe(render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::rasterization,
            .minimum_versions = minimum_versions,
        }),
        probe.probe(render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::unicode_processing,
            .minimum_versions = minimum_versions,
        }),
    };
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

std::vector<quiz_vulkan::render::render_text_font_shaping_codepoint_selection> selections_with_glyph_ids_for(
    const std::vector<quiz_vulkan::render::utf8_text_codepoint>& codepoints,
    quiz_vulkan::render::font_face_id face_id,
    std::uint32_t glyph_id_offset = 0,
    bool glyph_supported = true)
{
    std::vector<quiz_vulkan::render::render_text_font_shaping_codepoint_selection> selections;
    selections.reserve(codepoints.size());
    for (const quiz_vulkan::render::utf8_text_codepoint& codepoint : codepoints) {
        const std::uint32_t glyph_id = codepoint.code_point + glyph_id_offset;
        selections.push_back(quiz_vulkan::render::render_text_font_shaping_codepoint_selection{
            .requested_face_id = face_id,
            .resolved_face_id = face_id,
            .glyph_id = glyph_id,
            .has_glyph_id = true,
            .glyph_id_offset = glyph_id_offset,
            .glyph_id_matches_codepoint = glyph_id == codepoint.code_point,
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

quiz_vulkan::render::render_text_real_font_shaping_adapter_request adapter_request_for(
    const std::string& text,
    quiz_vulkan::render::render_text_font_backend_capability_snapshot capability,
    std::vector<quiz_vulkan::render::render_text_font_shaping_codepoint_selection> selections = {})
{
    using namespace quiz_vulkan::render;

    const render_text_font_shaping_request shaping_request = request_for(text, std::move(selections));
    return render_text_real_font_shaping_adapter_request{
        .capability = std::move(capability),
        .library = render_text_font_backend_library::harfbuzz,
        .run_index = shaping_request.run_index,
        .style_token = shaping_request.style_token,
        .style = shaping_request.style,
        .codepoints = shaping_request.codepoints,
        .clusters = shaping_request.clusters,
        .font_selections = shaping_request.font_selections,
        .fallback_glyph_id = shaping_request.fallback_glyph_id,
        .source_label = "adapter-test-font",
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

quiz_vulkan::render::render_text_font_backend_component harfbuzz_shaping_only_component()
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
        },
        .diagnostic = "HarfBuzz probe fixture has no complex script feature",
    };
}

quiz_vulkan::render::render_text_font_backend_capability_snapshot real_backend_capability(
    bool complex_scripts = true,
    bool rasterization = true)
{
    using namespace quiz_vulkan::render;

    std::vector<render_text_font_backend_component> components;
    if (rasterization) {
        components.push_back(freetype_component());
    }
    components.push_back(complex_scripts ? harfbuzz_component() : harfbuzz_shaping_only_component());

    std::vector<render_text_font_backend_library> required_libraries = {
        render_text_font_backend_library::harfbuzz,
    };
    std::vector<render_text_font_backend_feature> required_features = {
        render_text_font_backend_feature::glyph_shaping,
    };
    if (rasterization) {
        required_libraries.push_back(render_text_font_backend_library::freetype);
        required_features.push_back(render_text_font_backend_feature::glyph_rasterization);
    }

    return deterministic_fake_font_backend_capability_probe(std::move(components))
        .probe(render_text_font_backend_capability_probe_request{
            .required_libraries = std::move(required_libraries),
            .required_features = std::move(required_features),
        });
}

quiz_vulkan::render::render_text_font_backend_capability_snapshot fallback_only_capability()
{
    using namespace quiz_vulkan::render;

    return deterministic_fake_font_backend_capability_probe().probe(
        render_text_font_backend_capability_probe_request{
            .required_libraries = {
                render_text_font_backend_library::deterministic_fake,
            },
            .required_features = {
                render_text_font_backend_feature::glyph_id_mapping,
                render_text_font_backend_feature::glyph_shaping,
            },
        });
}

quiz_vulkan::render::render_text_font_backend_capability_snapshot unavailable_directwrite_capability()
{
    using namespace quiz_vulkan::render;

    return deterministic_fake_font_backend_capability_probe({
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
    }).probe(render_text_font_backend_capability_probe_request{
        .required_libraries = {
            render_text_font_backend_library::directwrite,
        },
        .required_features = {
            render_text_font_backend_feature::glyph_shaping,
        },
    });
}

quiz_vulkan::render::render_text_real_font_shaping_adapter_result mismatched_shape_callback(
    const quiz_vulkan::render::render_text_real_font_shaping_adapter_request& request)
{
    quiz_vulkan::render::render_text_real_font_shaping_adapter_result result =
        quiz_vulkan::render::deterministic_fake_real_font_backend_shape(request);
    if (!result.glyphs.empty()) {
        ++result.glyphs.front().glyph_id;
    }
    return result;
}

quiz_vulkan::render::render_text_real_font_shaping_adapter_result recoverable_shape_callback(
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
        .diagnostic = "recoverable fake adapter shaping failure",
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

quiz_vulkan::render::render_text_real_font_shaping_adapter_result fatal_shape_callback(
    const quiz_vulkan::render::render_text_real_font_shaping_adapter_request& request)
{
    using namespace quiz_vulkan::render;

    render_text_real_font_shaping_adapter_result result{
        .status = render_text_font_backend_adapter_status::fatal_backend_failure,
        .library = request.library,
        .capability_status = request.capability.status,
        .run_index = request.run_index,
        .style_token = request.style_token,
        .recoverable = false,
        .fatal = true,
        .diagnostic = "fatal fake adapter shaping failure",
    };
    result.diagnostics.push_back(render_text_font_backend_adapter_diagnostic{
        .status = result.status,
        .library = request.library,
        .capability_status = request.capability.status,
        .run_index = request.run_index,
        .recoverable = false,
        .fatal = true,
        .diagnostic = result.diagnostic,
    });
    return result;
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

void test_font_backend_selection_picks_harfbuzz_for_shaping_only()
{
    using namespace quiz_vulkan::render;

    const render_text_font_backend_selection_result result = select_render_text_font_backend(
        render_text_font_backend_selection_request{
            .purpose = render_text_font_backend_selection_purpose::shaping,
            .candidates = {
                make_render_text_freetype_backend_candidate(true),
                make_render_text_utf8proc_backend_candidate(true),
                make_render_text_harfbuzz_backend_candidate(true),
                make_render_text_deterministic_fake_backend_candidate(),
            },
        });

    require(result.ok(), "shaping selection succeeds");
    require(result.selected_real_backend(), "shaping selection uses a real candidate");
    require(
        result.selected.library == render_text_font_backend_library::harfbuzz,
        "shaping request selects HarfBuzz instead of FreeType or utf8proc");
    require(
        result.status == render_text_font_backend_selection_status::selected,
        "shaping selection records selected status");
    require(
        result.capability.supports_feature(render_text_font_backend_feature::glyph_shaping),
        "shaping selection exposes glyph shaping capability");
    require(
        !result.capability.supports_feature(render_text_font_backend_feature::glyph_rasterization),
        "HarfBuzz shaping selection does not claim rasterization");
    require(result.adapter_functions.shape != nullptr, "shaping selection provides adapter shape callback");
    require(result.adapter_functions.label == "HarfBuzz", "shaping adapter label follows selected candidate");
}

void test_font_backend_selection_picks_freetype_for_rasterization_only()
{
    using namespace quiz_vulkan::render;

    const render_text_font_backend_selection_result result = select_render_text_font_backend(
        render_text_font_backend_selection_request{
            .purpose = render_text_font_backend_selection_purpose::rasterization,
            .candidates = {
                make_render_text_harfbuzz_backend_candidate(true),
                make_render_text_utf8proc_backend_candidate(true),
                make_render_text_freetype_backend_candidate(true),
                make_render_text_deterministic_fake_backend_candidate(),
            },
        });

    require(result.ok(), "rasterization selection succeeds");
    require(result.selected_real_backend(), "rasterization selection uses a real candidate");
    require(
        result.selected.library == render_text_font_backend_library::freetype,
        "rasterization request selects FreeType instead of HarfBuzz or utf8proc");
    require(
        result.capability.supports_feature(render_text_font_backend_feature::glyph_rasterization),
        "rasterization selection exposes glyph rasterization capability");
    require(
        !result.capability.supports_feature(render_text_font_backend_feature::glyph_shaping),
        "FreeType rasterization selection does not claim shaping");
    require(result.adapter_functions.rasterize != nullptr, "rasterization selection provides adapter raster callback");
    require(result.adapter_functions.label == "FreeType", "raster adapter label follows selected candidate");
}

void test_font_backend_selection_picks_utf8proc_for_unicode_processing()
{
    using namespace quiz_vulkan::render;

    const render_text_font_backend_selection_result result = select_render_text_font_backend(
        render_text_font_backend_selection_request{
            .purpose = render_text_font_backend_selection_purpose::unicode_processing,
            .candidates = {
                make_render_text_harfbuzz_backend_candidate(true),
                make_render_text_freetype_backend_candidate(true),
                make_render_text_utf8proc_backend_candidate(true),
                make_render_text_deterministic_fake_backend_candidate(),
            },
        });

    require(result.ok(), "unicode processing selection succeeds");
    require(
        result.selected.library == render_text_font_backend_library::utf8proc,
        "unicode processing request selects utf8proc");
    require(
        result.capability.supports_feature(render_text_font_backend_feature::unicode_normalization),
        "utf8proc selection exposes normalization capability");
    require(
        result.capability.supports_feature(render_text_font_backend_feature::unicode_properties),
        "utf8proc selection exposes codepoint property capability");
}

void test_font_backend_selection_uses_deterministic_fake_when_real_capabilities_are_unavailable()
{
    using namespace quiz_vulkan::render;

    const render_text_font_backend_selection_result result = select_render_text_font_backend(
        render_text_font_backend_selection_request{
            .purpose = render_text_font_backend_selection_purpose::shaping,
            .candidates = make_render_text_known_font_backend_candidates(false),
        });

    require(result.ok(), "unavailable real backends still select deterministic fallback");
    require(
        result.status == render_text_font_backend_selection_status::fallback_selected,
        "unavailable real backend selection records fallback status");
    require(
        result.selected.library == render_text_font_backend_library::deterministic_fake,
        "unavailable real backend selection picks deterministic fake");
    require(result.used_deterministic_fallback, "unavailable real backend selection records fallback use");
    require(result.capability.fallback_only, "deterministic fallback capability is marked fallback-only");
    require(!result.diagnostics.empty(), "deterministic fallback selection records diagnostic");
}

void test_font_backend_selection_metadata_uses_logical_relative_hints()
{
    using namespace quiz_vulkan::render;

    const std::vector<render_text_font_backend_candidate> candidates =
        make_render_text_known_font_backend_candidates(true);
    require(candidates.size() == 4U, "known backend candidates include real text libraries and fallback");
    for (const render_text_font_backend_candidate& candidate : candidates) {
        require(
            render_text_font_backend_candidate_metadata_is_portable(candidate),
            "backend candidate metadata uses portable logical labels and relative hints");
        require(
            candidate.include_hint.find("/mnt/c/aa") == std::string::npos,
            "backend include hint does not hard-code worker absolute path");
        require(
            candidate.library_hint.find("/mnt/c/aa") == std::string::npos,
            "backend library hint does not hard-code worker absolute path");
    }
    require(candidates[0].version.display_label() == "14.2.0", "HarfBuzz logical version is recorded");
    require(candidates[1].license.find("FreeType") != std::string::npos, "FreeType logical license is recorded");
    require(candidates[2].version.display_label() == "v2.11.3", "utf8proc logical version is recorded");

    render_text_font_backend_candidate bad_candidate = make_render_text_harfbuzz_backend_candidate(true);
    bad_candidate.include_hint = "/mnt/c/aa/build/external/lib/cpp/desktop/harfbuzz-14.2.0/src/hb.h";
    require(
        !render_text_font_backend_candidate_metadata_is_portable(bad_candidate),
        "host-specific absolute backend hints are rejected");
}

void test_font_backend_selection_header_keeps_text_boundary()
{
    const std::string header = read_text_owned_header("font_backend_selection.h");
    const std::array<const char*, 10> forbidden_tokens = {
        "render/vulkan",
        "render/image",
        "src/app",
        "app/",
        "domain/",
        "ui/",
        "input/",
        "platform/",
        "/mnt/c/aa",
        "build/external",
    };
    for (const char* token : forbidden_tokens) {
        require(header.find(token) == std::string::npos, "font backend selection header stays text-owned");
    }
}

void test_external_backend_dependency_probe_reports_adapter_ready_stack()
{
    using namespace quiz_vulkan::render;

    const manifest_font_backend_dependency_probe probe(
        make_render_text_known_external_font_backend_manifest(true, true, true));
    const render_text_external_font_backend_probe_result shaping = probe.probe(
        render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::shaping,
            .minimum_versions = {
                render_text_font_backend_minimum_version{
                    .library = render_text_font_backend_library::harfbuzz,
                    .version = render_text_font_backend_version{.major = 14, .minor = 0, .patch = 0},
                },
            },
        });

    require(shaping.ok(), "adapter-ready shaping dependency probe is ok");
    require(
        shaping.status == render_text_font_backend_adapter_readiness_status::adapter_ready,
        "available HarfBuzz dependency reports adapter-ready status");
    require(shaping.adapter_ready, "available dependency marks adapter ready");
    require(shaping.can_attempt_real_backend, "available dependency can attempt real backend");
    require(!shaping.fallback_ready, "available dependency does not select fallback");
    require(!shaping.used_deterministic_fallback, "available dependency does not use deterministic fallback");
    require(shaping.requested_capability.ok(), "available dependency exposes requested capability");
    require(shaping.selection.selected_real_backend(), "available dependency selects a real backend");
    require(
        shaping.selection.selected.library == render_text_font_backend_library::harfbuzz,
        "shaping dependency selects HarfBuzz");
    require(shaping.adapter_functions.shape != nullptr, "shaping dependency exposes adapter shape hook");
    require(shaping.missing_dependencies.empty(), "available dependency has no missing libraries");
    require(shaping.adapter_unavailable_dependencies.empty(), "available dependency has adapter symbols");

    const render_text_external_font_backend_probe_result raster = probe.probe(
        render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::rasterization,
        });
    require(
        raster.selection.selected.library == render_text_font_backend_library::freetype,
        "raster dependency selects FreeType");
    require(raster.adapter_functions.rasterize != nullptr, "raster dependency exposes adapter raster hook");

    const render_text_external_font_backend_probe_result unicode = probe.probe(
        render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::unicode_processing,
        });
    require(
        unicode.selection.selected.library == render_text_font_backend_library::utf8proc,
        "unicode dependency selects utf8proc");
}

void test_external_backend_dependency_probe_reports_missing_dependency_and_fallback()
{
    using namespace quiz_vulkan::render;

    const manifest_font_backend_dependency_probe probe(
        make_render_text_known_external_font_backend_manifest(false, false, false));
    const render_text_external_font_backend_probe_result fallback = probe.probe(
        render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::shaping,
        });

    require(fallback.ok(), "missing real backend still has deterministic fallback");
    require(
        fallback.status == render_text_font_backend_adapter_readiness_status::fallback_ready,
        "missing real backend reports fallback-ready status");
    require(
        fallback.fallback_reason == render_text_font_backend_adapter_readiness_status::missing_dependency,
        "missing real backend records missing dependency reason");
    require(fallback.fallback_ready, "missing real backend marks fallback ready");
    require(fallback.used_deterministic_fallback, "missing real backend uses deterministic fallback");
    require(
        fallback.selection.selected.library == render_text_font_backend_library::deterministic_fake,
        "missing real backend selects deterministic fake");
    require(fallback.missing_dependencies.size() == 1U, "missing dependency list names required library");
    require(
        fallback.missing_dependencies.front() == render_text_font_backend_library::harfbuzz,
        "missing dependency list names HarfBuzz for shaping");

    const render_text_external_font_backend_probe_result blocked = probe.probe(
        render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::shaping,
            .allow_deterministic_fallback = false,
        });
    require(!blocked.ok(), "missing real backend without fallback is not ok");
    require(
        blocked.status == render_text_font_backend_adapter_readiness_status::missing_dependency,
        "missing real backend without fallback reports missing dependency");
    require(!blocked.fallback_ready, "missing real backend without fallback does not claim fallback readiness");
}

void test_external_backend_dependency_probe_reports_unlinked_adapter_fallback()
{
    using namespace quiz_vulkan::render;

    const manifest_font_backend_dependency_probe probe(
        make_render_text_known_external_font_backend_manifest(true, false, false));
    const render_text_external_font_backend_probe_result result = probe.probe(
        render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::rasterization,
        });

    require(result.ok(), "unlinked FreeType dependency can still fall back");
    require(
        result.status == render_text_font_backend_adapter_readiness_status::fallback_ready,
        "unlinked FreeType dependency reports fallback-ready status");
    require(
        result.fallback_reason == render_text_font_backend_adapter_readiness_status::adapter_unavailable,
        "unlinked FreeType dependency records adapter-unavailable reason");
    require(result.missing_dependencies.empty(), "unlinked dependency source is not missing");
    require(
        result.adapter_unavailable_dependencies.size() == 1U,
        "unlinked dependency records adapter-unavailable library");
    require(
        result.adapter_unavailable_dependencies.front() == render_text_font_backend_library::freetype,
        "unlinked dependency records FreeType adapter gap");
    require(!result.adapter_ready, "unlinked dependency is not adapter ready");
    require(result.fallback_ready, "unlinked dependency leaves fallback ready");
}

void test_external_backend_dependency_probe_reports_version_and_feature_mismatch()
{
    using namespace quiz_vulkan::render;

    render_text_external_font_backend_manifest old_manifest =
        make_render_text_known_external_font_backend_manifest(true, true, true);
    bool updated_harfbuzz = false;
    for (render_text_external_font_backend_dependency& dependency : old_manifest.dependencies) {
        if (dependency.library == render_text_font_backend_library::harfbuzz) {
            dependency.version = render_text_font_backend_version{.major = 13, .minor = 0, .patch = 0};
            updated_harfbuzz = true;
        }
    }
    require(updated_harfbuzz, "test manifest includes HarfBuzz");

    const manifest_font_backend_dependency_probe old_probe(std::move(old_manifest));
    const render_text_external_font_backend_probe_result version_mismatch = old_probe.probe(
        render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::shaping,
            .minimum_versions = {
                render_text_font_backend_minimum_version{
                    .library = render_text_font_backend_library::harfbuzz,
                    .version = render_text_font_backend_version{.major = 14, .minor = 0, .patch = 0},
                },
            },
        });
    require(version_mismatch.ok(), "version mismatch can select deterministic fallback");
    require(
        version_mismatch.status == render_text_font_backend_adapter_readiness_status::fallback_ready,
        "version mismatch reports fallback-ready status");
    require(
        version_mismatch.fallback_reason == render_text_font_backend_adapter_readiness_status::version_mismatch,
        "version mismatch records fallback reason");
    require(
        version_mismatch.requested_capability.status
            == render_text_font_backend_capability_status::version_mismatch,
        "version mismatch preserves requested capability status");
    require(version_mismatch.used_deterministic_fallback, "version mismatch uses deterministic fallback");

    const manifest_font_backend_dependency_probe feature_probe(
        make_render_text_known_external_font_backend_manifest(true, true, true));
    const render_text_external_font_backend_probe_result unsupported = feature_probe.probe(
        render_text_external_font_backend_probe_request{
            .purpose = render_text_font_backend_selection_purpose::shaping,
            .required_libraries = {
                render_text_font_backend_library::harfbuzz,
            },
            .required_features = {
                render_text_font_backend_feature::variable_fonts,
            },
        });
    require(unsupported.ok(), "unsupported real feature can select deterministic fallback");
    require(
        unsupported.fallback_reason == render_text_font_backend_adapter_readiness_status::unsupported_feature,
        "unsupported feature records fallback reason");
    require(
        unsupported.requested_capability.status
            == render_text_font_backend_capability_status::unsupported_feature,
        "unsupported feature preserves requested capability status");
}

void test_external_backend_dependency_metadata_and_header_stay_text_owned()
{
    using namespace quiz_vulkan::render;

    const render_text_external_font_backend_manifest manifest =
        make_render_text_known_external_font_backend_manifest(true, true, true);
    require(!manifest.empty(), "known external dependency manifest has entries");
    for (const render_text_external_font_backend_dependency& dependency : manifest.dependencies) {
        require(dependency.metadata_is_portable(), "external dependency metadata uses relative logical hints");
        require(dependency.source_hint.find("build/external") == std::string::npos, "source hint avoids build path");
        require(dependency.include_hint.find("build/external") == std::string::npos, "include hint avoids build path");
        require(dependency.library_hint.find("build/external") == std::string::npos, "library hint avoids build path");
    }

    render_text_external_font_backend_dependency bad_dependency =
        make_render_text_harfbuzz_external_dependency(true, true, true);
    bad_dependency.source_hint = "/absolute/desktop/harfbuzz-14.2.0";
    require(!bad_dependency.metadata_is_portable(), "absolute dependency source hints are rejected");
    require(
        render_text_external_font_backend_hint_uses_absolute_or_host_path(bad_dependency.source_hint),
        "absolute dependency source hint is detected");

    const std::string header = read_text_owned_header("font_backend_dependency.h");
    const std::array<const char*, 9> forbidden_tokens = {
        "render/vulkan",
        "render/image",
        "src/app",
        "app/",
        "domain/",
        "ui/",
        "input/",
        "platform/",
        "build/external",
    };
    for (const char* token : forbidden_tokens) {
        require(header.find(token) == std::string::npos, "font backend dependency header stays text-owned");
    }
}

void test_external_backend_header_probe_reports_compile_time_boundary()
{
    using namespace quiz_vulkan::render;

    const render_text_external_font_backend_header_probe_snapshot snapshot =
        make_render_text_external_font_backend_header_probe_snapshot();
    const std::size_t expected_available_count =
        (QUIZ_VULKAN_HAS_FREETYPE_HEADERS != 0 ? 1U : 0U)
        + (QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS != 0 ? 1U : 0U)
        + (QUIZ_VULKAN_HAS_UTF8PROC_HEADERS != 0 ? 1U : 0U);

    require(snapshot.probes.size() == 3U, "header probe snapshot checks the three approved text headers");
    require(snapshot.fake_fallback_preserved, "header probe snapshot preserves deterministic fake fallback");
    require(
        snapshot.available_header_count == expected_available_count,
        "header probe snapshot mirrors external-header CMake definitions");
    require(
        snapshot.freetype_headers_available == (QUIZ_VULKAN_HAS_FREETYPE_HEADERS != 0),
        "FreeType header availability mirrors CMake definition");
    require(
        snapshot.harfbuzz_headers_available == (QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS != 0),
        "HarfBuzz header availability mirrors CMake definition");
    require(
        snapshot.utf8proc_headers_available == (QUIZ_VULKAN_HAS_UTF8PROC_HEADERS != 0),
        "utf8proc header availability mirrors CMake definition");
    require(
        snapshot.any_real_headers_available() == (expected_available_count != 0U),
        "header probe snapshot reports whether any real header is present");
    require(
        snapshot.all_real_headers_available() == (expected_available_count == 3U),
        "header probe snapshot reports whether all approved headers are present");

    const std::array<render_text_font_backend_library, 3> libraries = {
        render_text_font_backend_library::freetype,
        render_text_font_backend_library::harfbuzz,
        render_text_font_backend_library::utf8proc,
    };
    std::size_t expected_feature_count = 0;
    for (const render_text_font_backend_library library : libraries) {
        const render_text_external_font_backend_header_probe* probe =
            find_render_text_external_font_backend_header_probe(snapshot.probes, library);
        require(probe != nullptr, "header probe snapshot exposes each approved text dependency");
        require(
            probe->header_available == render_text_external_font_backend_header_available(library),
            "header probe availability matches helper");
        require(
            probe->approved_header == render_text_external_font_backend_header_path_for(library),
            "header probe records approved include surface");
        if (probe->header_available) {
            expected_feature_count += render_text_external_font_backend_header_features_for(library).size();
            require(!probe->features.empty(), "available header probe advertises capability evidence");
            require(
                probe->diagnostic.find("available") != std::string::npos,
                "available header probe records availability diagnostic");
        } else {
            require(probe->features.empty(), "unavailable header probe does not advertise real features");
            require(
                probe->diagnostic.find("unavailable") != std::string::npos,
                "unavailable header probe records fallback diagnostic");
        }
    }
    require(
        snapshot.advertised_feature_count == expected_feature_count,
        "header probe snapshot counts advertised feature evidence");
    require(
        snapshot.versioned_header_count <= snapshot.available_header_count,
        "header probe version evidence is bounded by available headers");
}

void test_external_backend_header_backed_manifest_preserves_fake_fallback_without_linking()
{
    using namespace quiz_vulkan::render;

    const render_text_external_font_backend_header_probe_snapshot snapshot =
        make_render_text_external_font_backend_header_probe_snapshot();
    const render_text_external_font_backend_manifest manifest =
        make_render_text_header_backed_external_font_backend_manifest();

    require(!manifest.empty(), "header-backed manifest exposes dependency records");
    require(
        manifest.dependencies.size() == snapshot.probes.size() + 1U,
        "header-backed manifest includes real header probes plus deterministic fallback");
    require(manifest.allow_deterministic_fallback, "header-backed manifest preserves deterministic fallback");

    for (const render_text_external_font_backend_header_probe& probe : snapshot.probes) {
        const render_text_external_font_backend_dependency* dependency =
            find_render_text_external_font_backend_dependency(manifest.dependencies, probe.library);
        require(dependency != nullptr, "header-backed manifest includes each approved dependency");
        require(
            dependency->source_available == probe.header_available,
            "header-backed dependency source availability mirrors header probe");
        require(!dependency->build_linked, "header-backed dependency does not claim linked library");
        require(
            !dependency->adapter_symbols_available,
            "header-backed dependency does not claim adapter symbols");
        require(!dependency->adapter_ready(), "header-backed dependency keeps adapter readiness false");
        require(
            dependency->metadata_is_portable(),
            "header-backed dependency metadata stays portable");
    }

    const manifest_font_backend_dependency_probe probe(manifest);
    const std::array<render_text_font_backend_selection_purpose, 3> purposes = {
        render_text_font_backend_selection_purpose::shaping,
        render_text_font_backend_selection_purpose::rasterization,
        render_text_font_backend_selection_purpose::unicode_processing,
    };
    for (const render_text_font_backend_selection_purpose purpose : purposes) {
        const std::vector<render_text_font_backend_library> required_libraries =
            render_text_external_font_backend_default_libraries_for(purpose);
        require(required_libraries.size() == 1U, "focused text purpose maps to one external dependency");

        const render_text_font_backend_library library = required_libraries.front();
        const bool header_available = render_text_external_font_backend_header_available(library);
        const render_text_external_font_backend_probe_result result = probe.probe(
            render_text_external_font_backend_probe_request{
                .purpose = purpose,
            });

        require(result.ok(), "header-backed manifest preserves deterministic fallback path");
        require(
            result.status == render_text_font_backend_adapter_readiness_status::fallback_ready,
            "header-backed manifest reports fallback-ready status");
        require(result.fallback_ready, "header-backed manifest marks fallback ready");
        require(result.used_deterministic_fallback, "header-backed manifest uses deterministic fallback");
        require(!result.adapter_ready, "header-backed manifest does not claim adapter readiness");
        require(!result.can_attempt_real_backend, "header-backed manifest cannot attempt real backend");
        require(
            result.selection.selected.library == render_text_font_backend_library::deterministic_fake,
            "header-backed manifest selects deterministic fake backend");
        if (header_available) {
            require(
                result.fallback_reason == render_text_font_backend_adapter_readiness_status::adapter_unavailable,
                "header-backed available headers fall back because adapters are unlinked");
            require(result.missing_dependencies.empty(), "available header is not reported as missing source");
            require(
                result.adapter_unavailable_dependencies.size() == 1U,
                "available header reports one adapter-unavailable dependency");
            require(
                result.adapter_unavailable_dependencies.front() == library,
                "available header records the adapter-unavailable library");
        } else {
            require(
                result.fallback_reason == render_text_font_backend_adapter_readiness_status::missing_dependency,
                "missing header falls back because the approved header is unavailable");
            require(
                result.missing_dependencies.size() == 1U,
                "missing header records one missing dependency");
            require(
                result.missing_dependencies.front() == library,
                "missing header records the required library");
        }
    }
}

void test_external_backend_work_readiness_distinguishes_headers_sources_and_linking()
{
    using namespace quiz_vulkan::render;

    const render_text_external_font_backend_header_probe_snapshot headers =
        make_render_text_external_font_backend_header_probe_snapshot();
    const std::array<render_text_font_backend_selection_purpose, 3> purposes = {
        render_text_font_backend_selection_purpose::shaping,
        render_text_font_backend_selection_purpose::rasterization,
        render_text_font_backend_selection_purpose::unicode_processing,
    };

    const std::vector<render_text_external_font_backend_work_readiness> header_only =
        make_render_text_external_font_backend_work_readiness_records(
            headers,
            make_render_text_header_backed_external_font_backend_manifest());
    require(header_only.size() == purposes.size(), "work readiness records each external text purpose");
    for (std::size_t index = 0; index < purposes.size(); ++index) {
        const render_text_external_font_backend_work_readiness& readiness = header_only[index];
        const render_text_font_backend_library library =
            render_text_external_font_backend_default_libraries_for(purposes[index]).front();

        require(readiness.purpose == purposes[index], "header-only readiness preserves purpose");
        require(readiness.library == library, "header-only readiness records required library");
        require(
            readiness.header_available == render_text_external_font_backend_header_available(library),
            "header-only readiness mirrors approved header availability");
        require(!readiness.build_linked, "header-only readiness does not claim linked libraries");
        require(!readiness.adapter_symbols_available, "header-only readiness does not claim adapter symbols");
        require(readiness.fallback_required, "header-only readiness keeps deterministic fallback required");
        require(!readiness.can_attempt_real_backend, "header-only readiness cannot attempt real backend work");
        if (readiness.header_available) {
            require(
                readiness.status == render_text_external_font_backend_work_readiness_status::header_only,
                "available approved headers are classified as header-only without linking");
            require(
                readiness.header_backed_without_library(),
                "header-only readiness reports header-backed state");
        } else {
            require(
                readiness.status
                    == render_text_external_font_backend_work_readiness_status::missing_approved_header,
                "missing approved headers are classified separately");
        }
    }

    const std::vector<render_text_external_font_backend_work_readiness> source_ready =
        make_render_text_external_font_backend_work_readiness_records(
            headers,
            make_render_text_known_external_font_backend_manifest(true, false, false));
    const std::vector<render_text_external_font_backend_work_readiness> library_linked =
        make_render_text_external_font_backend_work_readiness_records(
            headers,
            make_render_text_known_external_font_backend_manifest(true, true, false));
    const std::vector<render_text_external_font_backend_work_readiness> adapter_ready =
        make_render_text_external_font_backend_work_readiness_records(
            headers,
            make_render_text_known_external_font_backend_manifest(true, true, true));

    for (std::size_t index = 0; index < purposes.size(); ++index) {
        require(
            source_ready[index].status == render_text_external_font_backend_work_readiness_status::source_ready,
            "source-ready dependencies are distinct from header-only probes");
        require(source_ready[index].source_available, "source-ready dependency records source availability");
        require(!source_ready[index].build_linked, "source-ready dependency is not linked yet");
        require(source_ready[index].fallback_required, "source-ready dependency still requires fallback");

        require(
            library_linked[index].status == render_text_external_font_backend_work_readiness_status::library_linked,
            "linked dependencies without adapter symbols are classified as linked");
        require(library_linked[index].build_linked, "linked readiness records library linkage");
        require(
            !library_linked[index].adapter_symbols_available,
            "linked readiness distinguishes missing adapter symbols");
        require(!library_linked[index].can_attempt_real_backend, "linked readiness without adapter cannot run backend");

        require(
            adapter_ready[index].status == render_text_external_font_backend_work_readiness_status::adapter_ready,
            "adapter-ready dependencies are classified as runnable real backend work");
        require(adapter_ready[index].ok(), "adapter-ready readiness is ok");
        require(adapter_ready[index].can_attempt_real_backend, "adapter-ready readiness can attempt real backend");
        require(!adapter_ready[index].fallback_required, "adapter-ready readiness does not require fallback");
    }
}

void test_external_backend_dependency_probe_diff_reports_fake_to_adapter_ready()
{
    using namespace quiz_vulkan::render;

    const manifest_font_backend_dependency_probe fake_probe(
        make_render_text_known_external_font_backend_manifest(false, false, false));
    const manifest_font_backend_dependency_probe ready_probe(
        make_render_text_known_external_font_backend_manifest(true, true, true));

    const render_text_external_font_backend_probe_diff_snapshot diff =
        diff_render_text_external_font_backend_probe_result(
            fake_probe.probe(render_text_external_font_backend_probe_request{
                .purpose = render_text_font_backend_selection_purpose::shaping,
            }),
            ready_probe.probe(render_text_external_font_backend_probe_request{
                .purpose = render_text_font_backend_selection_purpose::shaping,
            }));

    require(diff.has_changes, "fake-to-ready backend probe diff records changes");
    require(diff.before.fake_only, "diff before state records fake-only fallback");
    require(diff.before.unavailable, "diff before state records unavailable real dependency");
    require(diff.after.adapter_ready, "diff after state records adapter-ready backend");
    require(diff.became_adapter_ready(), "diff records adapter-ready transition");
    require(diff.fake_only_changed, "diff records fake-only state change");
    require(diff.unavailable_changed, "diff records unavailable state change");
    require(diff.selected_backend_changed, "diff records selected backend change");
    require(diff.missing_dependency_delta == -1, "diff records one fewer missing dependency");
    require(
        diff.after.selected_library == render_text_font_backend_library::harfbuzz,
        "diff after state records selected HarfBuzz backend");
    require(
        diff.summary.find("fake-only unavailable fallback -> adapter-ready") != std::string::npos,
        "diff summary names fake-to-ready transition");
}

void test_external_backend_dependency_probe_diff_reports_unavailable_to_mismatch()
{
    using namespace quiz_vulkan::render;

    render_text_external_font_backend_manifest old_manifest =
        make_render_text_known_external_font_backend_manifest(true, true, true);
    for (render_text_external_font_backend_dependency& dependency : old_manifest.dependencies) {
        if (dependency.library == render_text_font_backend_library::harfbuzz) {
            dependency.version = render_text_font_backend_version{.major = 13, .minor = 0, .patch = 0};
        }
    }

    const manifest_font_backend_dependency_probe unavailable_probe(
        make_render_text_known_external_font_backend_manifest(true, false, false));
    const manifest_font_backend_dependency_probe mismatch_probe(std::move(old_manifest));

    const render_text_external_font_backend_probe_diff_snapshot diff =
        diff_render_text_external_font_backend_probe_result(
            unavailable_probe.probe(render_text_external_font_backend_probe_request{
                .purpose = render_text_font_backend_selection_purpose::shaping,
            }),
            mismatch_probe.probe(render_text_external_font_backend_probe_request{
                .purpose = render_text_font_backend_selection_purpose::shaping,
                .minimum_versions = {
                    render_text_font_backend_minimum_version{
                        .library = render_text_font_backend_library::harfbuzz,
                        .version = render_text_font_backend_version{.major = 14, .minor = 0, .patch = 0},
                    },
                },
            }));

    require(diff.has_changes, "unavailable-to-mismatch backend probe diff records changes");
    require(diff.before.unavailable, "diff before state records unavailable adapter");
    require(!diff.before.mismatch, "diff before state is not a mismatch");
    require(diff.after.mismatch, "diff after state records version mismatch");
    require(diff.became_mismatch(), "diff records mismatch transition");
    require(diff.unavailable_changed, "diff records unavailable state change");
    require(diff.fallback_reason_changed, "diff records fallback reason change");
    require(diff.adapter_unavailable_delta == -1, "diff records one fewer adapter-unavailable dependency");
    require(diff.version_mismatch_delta == 1, "diff records one new version mismatch");
}

void test_external_backend_dependency_probe_diff_summary_compares_backend_snapshots()
{
    using namespace quiz_vulkan::render;

    const std::vector<render_text_external_font_backend_probe_result> fake_only =
        probe_external_backend_stack(make_render_text_known_external_font_backend_manifest(false, false, false));
    const std::vector<render_text_external_font_backend_probe_result> adapter_ready =
        probe_external_backend_stack(make_render_text_known_external_font_backend_manifest(true, true, true));

    const render_text_external_font_backend_probe_diff_summary_snapshot summary =
        diff_render_text_external_font_backend_probe_results(fake_only, adapter_ready);

    require(summary.has_changes(), "backend probe diff summary records changed stack");
    require(summary.diffs.size() == 3U, "backend probe diff summary compares three purposes");
    require(summary.changed_count == 3U, "backend probe diff summary counts three changed purposes");
    require(
        summary.adapter_ready_transition_count == 3U,
        "backend probe diff summary counts adapter-ready transitions");
    require(
        summary.fake_only_transition_count == 0U,
        "backend probe diff summary does not count fake-only transitions for ready stack");
    require(
        summary.selected_backend_change_count == 3U,
        "backend probe diff summary counts selected backend changes");
    require(
        summary.total_missing_dependency_delta == -3,
        "backend probe diff summary aggregates missing dependency delta");
    require(
        summary.summary == "external font backend probe diff: 3 changed of 3 purposes",
        "backend probe diff summary is deterministic");
}

void test_external_backend_dependency_probe_diff_summary_reports_unchanged_state()
{
    using namespace quiz_vulkan::render;

    const std::vector<render_text_external_font_backend_probe_result> adapter_ready =
        probe_external_backend_stack(make_render_text_known_external_font_backend_manifest(true, true, true));
    const render_text_external_font_backend_probe_diff_summary_snapshot summary =
        diff_render_text_external_font_backend_probe_results(adapter_ready, adapter_ready);

    require(!summary.has_changes(), "unchanged backend probe diff summary reports no changes");
    require(summary.changed_count == 0U, "unchanged backend probe diff summary has zero changed purposes");
    require(summary.adapter_ready_transition_count == 0U, "unchanged backend probe diff has no ready transition");
    for (const render_text_external_font_backend_probe_diff_snapshot& diff : summary.diffs) {
        require(!diff.has_changes, "unchanged backend probe purpose diff has no changes");
        require(!diff.became_adapter_ready(), "unchanged backend probe purpose diff has no ready transition");
    }
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

void test_real_backend_adapter_gates_backend_unavailable()
{
    using namespace quiz_vulkan::render;

    const function_table_font_backend_adapter adapter;

    const render_text_real_font_shaping_adapter_result unavailable = adapter.shape(
        adapter_request_for("A", unavailable_directwrite_capability()));
    require(
        unavailable.status == render_text_font_backend_adapter_status::backend_unavailable,
        "real backend adapter gates unavailable capability");
    require(unavailable.can_fallback(), "unavailable real adapter shaping can fall back");
    require(
        unavailable.capability_status == render_text_font_backend_capability_status::unavailable,
        "unavailable adapter preserves capability status");
    require(
        unavailable.has_diagnostic(render_text_font_backend_adapter_status::backend_unavailable),
        "unavailable adapter records diagnostic");

    const render_text_real_font_shaping_adapter_result fallback_only = adapter.shape(
        adapter_request_for("A", fallback_only_capability()));
    require(
        fallback_only.status == render_text_font_backend_adapter_status::backend_unavailable,
        "real backend adapter does not call fallback-only fake capability as a real backend");
    require(
        fallback_only.capability_status == render_text_font_backend_capability_status::fallback_only,
        "fallback-only adapter diagnostic preserves capability mode");
}

void test_real_backend_adapter_reports_unsupported_script_without_complex_feature()
{
    using namespace quiz_vulkan::render;

    const function_table_font_backend_adapter adapter;
    const render_text_real_font_shaping_adapter_result result = adapter.shape(
        adapter_request_for("\xd8\xa7", real_backend_capability(false, false)));

    require(
        result.status == render_text_font_backend_adapter_status::unsupported_script,
        "real backend adapter blocks complex script when capability omits complex shaping");
    require(result.can_fallback(), "unsupported script can fall back");
    require(result.diagnostics.size() == 1U, "unsupported script records one diagnostic");
    require(result.diagnostics.front().codepoint == 0x0627U, "unsupported script diagnostic names Arabic scalar");
    require(
        result.diagnostics.front().capability_status == render_text_font_backend_capability_status::available,
        "unsupported script preserves available capability status");
}

void test_real_backend_adapter_shapes_available_run()
{
    using namespace quiz_vulkan::render;

    const std::string text = "A" "\xed\x95\x9c" "\xf0\x9f\x98\x80";
    const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(text);
    const function_table_font_backend_adapter adapter;
    const render_text_real_font_shaping_adapter_result result = adapter.shape(
        adapter_request_for(text, real_backend_capability(), selections_with_glyph_ids_for(codepoints, 41, 5000)));

    require(result.ok(), "available real backend adapter shapes supported run");
    require(result.glyphs.size() == 3U, "available adapter emits one glyph per codepoint");
    require(result.glyphs.front().glyph_id == U'A' + 5000U, "available adapter consumes resolved glyph id");
    require(result.glyphs.front().glyph_id_from_selection, "available adapter marks glyph id from selection");
    require(!result.glyphs.front().glyph_id_matches_codepoint, "available adapter does not fall back to codepoint");
    require(
        result.has_diagnostic(render_text_font_backend_adapter_status::shaped),
        "available adapter records shaped diagnostic");
}

void test_real_backend_adapter_reports_glyph_id_mismatch()
{
    using namespace quiz_vulkan::render;

    const std::string text = "A";
    const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(text);
    const function_table_font_backend_adapter adapter(render_text_font_backend_adapter_functions{
        .shape = mismatched_shape_callback,
        .rasterize = deterministic_fake_real_font_backend_rasterize,
        .label = "mismatch fixture",
    });
    const render_text_real_font_shaping_adapter_result result = adapter.shape(
        adapter_request_for(text, real_backend_capability(), selections_with_glyph_ids_for(codepoints, 41, 9)));

    require(
        result.status == render_text_font_backend_adapter_status::glyph_id_mismatch,
        "adapter reports glyph id mismatch from function table backend");
    require(result.can_fallback(), "glyph id mismatch can fall back");
    require(result.diagnostics.back().expected_glyph_id == U'A' + 9U, "mismatch records expected glyph id");
    require(result.diagnostics.back().actual_glyph_id == U'A' + 10U, "mismatch records actual glyph id");
}

void test_real_backend_adapter_reports_recoverable_and_fatal_failures()
{
    using namespace quiz_vulkan::render;

    const render_text_real_font_shaping_adapter_request request =
        adapter_request_for("A", real_backend_capability());
    const function_table_font_backend_adapter recoverable(render_text_font_backend_adapter_functions{
        .shape = recoverable_shape_callback,
        .rasterize = deterministic_fake_real_font_backend_rasterize,
        .label = "recoverable fixture",
    });
    const render_text_real_font_shaping_adapter_result recoverable_result = recoverable.shape(request);
    require(
        recoverable_result.status == render_text_font_backend_adapter_status::recoverable_backend_failure,
        "adapter preserves recoverable backend failure");
    require(recoverable_result.can_fallback(), "recoverable backend failure can fall back");
    require(!recoverable_result.fatal, "recoverable backend failure is not fatal");

    const function_table_font_backend_adapter fatal(render_text_font_backend_adapter_functions{
        .shape = fatal_shape_callback,
        .rasterize = deterministic_fake_real_font_backend_rasterize,
        .label = "fatal fixture",
    });
    const render_text_real_font_shaping_adapter_result fatal_result = fatal.shape(request);
    require(
        fatal_result.status == render_text_font_backend_adapter_status::fatal_backend_failure,
        "adapter preserves fatal backend failure");
    require(!fatal_result.can_fallback(), "fatal backend failure does not report fallback-safe");
    require(fatal_result.fatal, "fatal backend failure records fatal flag");
}

void test_real_backend_adapter_diagnostics_are_deterministic()
{
    using namespace quiz_vulkan::render;

    const std::string text = "A" "\xed\x95\x9c";
    const std::vector<utf8_text_codepoint> codepoints = iterate_utf8_text_run(text);
    const render_text_real_font_shaping_adapter_request request =
        adapter_request_for(text, real_backend_capability(), selections_with_glyph_ids_for(codepoints, 41, 20));
    const function_table_font_backend_adapter adapter;

    const render_text_real_font_shaping_adapter_result first = adapter.shape(request);
    const render_text_real_font_shaping_adapter_result second = adapter.shape(request);

    require(first.status == second.status, "adapter status is deterministic");
    require(first.diagnostic == second.diagnostic, "adapter diagnostic string is deterministic");
    require(first.diagnostics.size() == second.diagnostics.size(), "adapter diagnostic count is deterministic");
    require(first.glyphs.size() == second.glyphs.size(), "adapter glyph count is deterministic");
    require(first.glyphs[1].glyph_id == second.glyphs[1].glyph_id, "adapter glyph ids are deterministic");
    require(near(first.glyphs[1].advance_x, second.glyphs[1].advance_x), "adapter advances are deterministic");
}

void test_real_backend_adapter_rasterizes_and_gates_raster_capability()
{
    using namespace quiz_vulkan::render;

    const font_face_descriptor face{
        .id = 41,
        .family = "Adapter Sans",
        .source_uri = "fixture://adapter-sans.ttf",
        .coverage = {
            font_codepoint_range{.first = U'A', .last = U'A'},
        },
    };
    const std::vector<std::byte> font_bytes = {
        std::byte{0x00},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
    };
    const glyph_atlas_key key{
        .face_id = face.id,
        .glyph_id = 8123,
        .pixel_size = 18,
    };
    const render_text_font_rasterize_request raster_request =
        make_font_rasterize_request(
            face,
            key,
            U'A',
            std::span<const std::byte>{font_bytes.data(), font_bytes.size()},
            "adapter-sans");
    const function_table_font_backend_adapter adapter;

    const render_text_real_font_raster_adapter_result rasterized = adapter.rasterize(
        render_text_real_font_raster_adapter_request{
            .capability = real_backend_capability(),
            .library = render_text_font_backend_library::freetype,
            .rasterize = raster_request,
        });
    require(rasterized.ok(), "adapter rasterizes when capability allows rasterization");
    require(rasterized.rasterized.glyph_id == key.glyph_id, "adapter raster result preserves glyph atlas key id");
    require(rasterized.rasterized.has_bitmap(), "adapter raster result has uploadable bitmap");
    require(
        rasterized.diagnostics.front().capability_status == render_text_font_backend_capability_status::available,
        "adapter raster diagnostic records available capability");

    const render_text_real_font_raster_adapter_result gated = adapter.rasterize(
        render_text_real_font_raster_adapter_request{
            .capability = unavailable_directwrite_capability(),
            .library = render_text_font_backend_library::freetype,
            .rasterize = raster_request,
        });
    require(
        gated.status == render_text_font_backend_adapter_status::backend_unavailable,
        "adapter gates raster calls when capability is unavailable");
    require(gated.can_fallback(), "gated raster failure can fall back");
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
    test_font_backend_selection_picks_harfbuzz_for_shaping_only();
    test_font_backend_selection_picks_freetype_for_rasterization_only();
    test_font_backend_selection_picks_utf8proc_for_unicode_processing();
    test_font_backend_selection_uses_deterministic_fake_when_real_capabilities_are_unavailable();
    test_font_backend_selection_metadata_uses_logical_relative_hints();
    test_font_backend_selection_header_keeps_text_boundary();
    test_external_backend_dependency_probe_reports_adapter_ready_stack();
    test_external_backend_dependency_probe_reports_missing_dependency_and_fallback();
    test_external_backend_dependency_probe_reports_unlinked_adapter_fallback();
    test_external_backend_dependency_probe_reports_version_and_feature_mismatch();
    test_external_backend_dependency_metadata_and_header_stay_text_owned();
    test_external_backend_header_probe_reports_compile_time_boundary();
    test_external_backend_header_backed_manifest_preserves_fake_fallback_without_linking();
    test_external_backend_work_readiness_distinguishes_headers_sources_and_linking();
    test_external_backend_dependency_probe_diff_reports_fake_to_adapter_ready();
    test_external_backend_dependency_probe_diff_reports_unavailable_to_mismatch();
    test_external_backend_dependency_probe_diff_summary_compares_backend_snapshots();
    test_external_backend_dependency_probe_diff_summary_reports_unchanged_state();
    test_fake_backend_shapes_successful_latin_hangul_and_non_bmp_run();
    test_fake_backend_reports_backend_unavailable();
    test_fake_backend_reports_unsupported_script_and_fallback_glyph_id();
    test_fake_backend_reports_unsupported_glyph_and_fallback_glyph_id();
    test_fake_backend_consumes_pre_resolved_glyph_ids();
    test_real_backend_adapter_gates_backend_unavailable();
    test_real_backend_adapter_reports_unsupported_script_without_complex_feature();
    test_real_backend_adapter_shapes_available_run();
    test_real_backend_adapter_reports_glyph_id_mismatch();
    test_real_backend_adapter_reports_recoverable_and_fatal_failures();
    test_real_backend_adapter_diagnostics_are_deterministic();
    test_real_backend_adapter_rasterizes_and_gates_raster_capability();
    test_fake_backend_reports_zero_advance_combining_mark_with_cluster_range();
    return 0;
}
