#pragma once

#include "render/text/font_backend_adapter.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_backend_selection_purpose {
    shaping,
    rasterization,
    unicode_processing,
};

inline std::string render_text_font_backend_selection_purpose_name(
    const render_text_font_backend_selection_purpose purpose)
{
    switch (purpose) {
    case render_text_font_backend_selection_purpose::shaping:
        return "shaping";
    case render_text_font_backend_selection_purpose::rasterization:
        return "rasterization";
    case render_text_font_backend_selection_purpose::unicode_processing:
        return "unicode_processing";
    }

    return "unknown";
}

enum class render_text_font_backend_selection_status {
    selected,
    fallback_selected,
    unavailable,
    unsupported_feature,
};

inline std::string render_text_font_backend_selection_status_name(
    const render_text_font_backend_selection_status status)
{
    switch (status) {
    case render_text_font_backend_selection_status::selected:
        return "selected";
    case render_text_font_backend_selection_status::fallback_selected:
        return "fallback_selected";
    case render_text_font_backend_selection_status::unavailable:
        return "unavailable";
    case render_text_font_backend_selection_status::unsupported_feature:
        return "unsupported_feature";
    }

    return "unknown";
}

struct render_text_font_backend_candidate {
    render_text_font_backend_library library =
        render_text_font_backend_library::deterministic_fake;
    std::string label;
    render_text_font_backend_version version;
    std::string license;
    std::string include_hint;
    std::string library_hint;
    std::vector<render_text_font_backend_feature> features;
    bool available = false;
    bool fallback_only = false;
    std::string diagnostic;

    bool supports_feature(const render_text_font_backend_feature feature) const
    {
        return std::find(features.begin(), features.end(), feature) != features.end();
    }

    render_text_font_backend_component component() const
    {
        return render_text_font_backend_component{
            .library = library,
            .name = label,
            .available = available,
            .version = version,
            .features = features,
            .fallback_only = fallback_only,
            .diagnostic = diagnostic,
        };
    }
};

struct render_text_font_backend_selection_request {
    render_text_font_backend_selection_purpose purpose =
        render_text_font_backend_selection_purpose::shaping;
    std::vector<render_text_font_backend_candidate> candidates;
    std::vector<render_text_font_backend_feature> required_features;
    bool allow_deterministic_fallback = true;
};

struct render_text_font_backend_selection_result {
    render_text_font_backend_selection_status status =
        render_text_font_backend_selection_status::unavailable;
    render_text_font_backend_selection_purpose purpose =
        render_text_font_backend_selection_purpose::shaping;
    render_text_font_backend_candidate selected;
    bool has_selection = false;
    bool used_deterministic_fallback = false;
    std::vector<render_text_font_backend_feature> required_features;
    render_text_font_backend_capability_snapshot capability;
    render_text_font_backend_adapter_functions adapter_functions;
    std::vector<std::string> diagnostics;

    bool ok() const
    {
        return status == render_text_font_backend_selection_status::selected
            || status == render_text_font_backend_selection_status::fallback_selected;
    }

    bool selected_real_backend() const
    {
        return has_selection
            && selected.library != render_text_font_backend_library::deterministic_fake
            && !used_deterministic_fallback;
    }
};

inline std::vector<render_text_font_backend_feature> render_text_font_backend_required_features_for(
    const render_text_font_backend_selection_purpose purpose)
{
    switch (purpose) {
    case render_text_font_backend_selection_purpose::shaping:
        return {
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_shaping,
        };
    case render_text_font_backend_selection_purpose::rasterization:
        return {
            render_text_font_backend_feature::font_file_loading,
            render_text_font_backend_feature::unicode_cmap,
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_rasterization,
        };
    case render_text_font_backend_selection_purpose::unicode_processing:
        return {
            render_text_font_backend_feature::unicode_normalization,
            render_text_font_backend_feature::unicode_properties,
        };
    }

    return {};
}

inline bool render_text_font_backend_candidate_supports_features(
    const render_text_font_backend_candidate& candidate,
    const std::vector<render_text_font_backend_feature>& required_features)
{
    return std::all_of(
        required_features.begin(),
        required_features.end(),
        [&](const render_text_font_backend_feature feature) {
            return candidate.supports_feature(feature);
        });
}

inline bool render_text_font_backend_candidate_uses_absolute_or_host_path(
    const render_text_font_backend_candidate& candidate)
{
    const auto is_host_path = [](const std::string& value) {
        const std::string external_prefix = std::string{"build"} + "/" + "external/";
        return !value.empty()
            && (value.front() == '/'
                || value.find("\\") != std::string::npos
                || value.find(":\\") != std::string::npos
                || value.find(":/") != std::string::npos
                || value.find("/mnt/") != std::string::npos
                || value.find("/Users/") != std::string::npos
                || value.find(external_prefix) != std::string::npos);
    };
    return is_host_path(candidate.include_hint) || is_host_path(candidate.library_hint);
}

inline bool render_text_font_backend_candidate_metadata_is_portable(
    const render_text_font_backend_candidate& candidate)
{
    return !candidate.label.empty()
        && !candidate.license.empty()
        && !render_text_font_backend_candidate_uses_absolute_or_host_path(candidate);
}

inline render_text_font_backend_candidate make_render_text_harfbuzz_backend_candidate(
    const bool available = false)
{
    return render_text_font_backend_candidate{
        .library = render_text_font_backend_library::harfbuzz,
        .label = "HarfBuzz",
        .version = render_text_font_backend_version{.major = 14, .minor = 2, .patch = 0},
        .license = "Old MIT",
        .include_hint = "harfbuzz-14.2.0/src/hb.h",
        .library_hint = "harfbuzz-14.2.0",
        .features = {
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_shaping,
            render_text_font_backend_feature::complex_script_shaping,
        },
        .available = available,
        .diagnostic = available
            ? "HarfBuzz candidate is available to the text backend adapter"
            : "HarfBuzz candidate is described but not linked into this build",
    };
}

inline render_text_font_backend_candidate make_render_text_freetype_backend_candidate(
    const bool available = false)
{
    return render_text_font_backend_candidate{
        .library = render_text_font_backend_library::freetype,
        .label = "FreeType",
        .version = render_text_font_backend_version{.major = 2, .minor = 14, .patch = 3},
        .license = "FreeType License or GPL-2.0-or-later",
        .include_hint = "freetype-2.14.3/include/ft2build.h",
        .library_hint = "freetype-2.14.3",
        .features = {
            render_text_font_backend_feature::font_file_loading,
            render_text_font_backend_feature::unicode_cmap,
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_rasterization,
        },
        .available = available,
        .diagnostic = available
            ? "FreeType candidate is available to the text backend adapter"
            : "FreeType candidate is described but not linked into this build",
    };
}

inline render_text_font_backend_candidate make_render_text_utf8proc_backend_candidate(
    const bool available = false)
{
    return render_text_font_backend_candidate{
        .library = render_text_font_backend_library::utf8proc,
        .label = "utf8proc",
        .version = render_text_font_backend_version{.major = 2, .minor = 11, .patch = 3, .label = "v2.11.3"},
        .license = "MIT",
        .include_hint = "utf8proc-v2.11.3/utf8proc.h",
        .library_hint = "utf8proc-v2.11.3",
        .features = {
            render_text_font_backend_feature::unicode_normalization,
            render_text_font_backend_feature::unicode_properties,
        },
        .available = available,
        .diagnostic = available
            ? "utf8proc candidate is available to the text backend adapter"
            : "utf8proc candidate is described but not linked into this build",
    };
}

inline render_text_font_backend_candidate make_render_text_deterministic_fake_backend_candidate()
{
    return render_text_font_backend_candidate{
        .library = render_text_font_backend_library::deterministic_fake,
        .label = "Deterministic fake text backend",
        .version = render_text_font_backend_version{.major = 1, .minor = 0, .patch = 0},
        .license = "project test fixture",
        .include_hint = "src/render/text",
        .library_hint = "text-owned deterministic fallback",
        .features = {
            render_text_font_backend_feature::font_file_loading,
            render_text_font_backend_feature::unicode_cmap,
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_rasterization,
            render_text_font_backend_feature::glyph_shaping,
            render_text_font_backend_feature::unicode_normalization,
            render_text_font_backend_feature::unicode_properties,
        },
        .available = true,
        .fallback_only = true,
        .diagnostic = "deterministic fake backend remains available for tests and fallback",
    };
}

inline std::vector<render_text_font_backend_candidate> make_render_text_known_font_backend_candidates(
    const bool real_backends_available = false)
{
    return {
        make_render_text_harfbuzz_backend_candidate(real_backends_available),
        make_render_text_freetype_backend_candidate(real_backends_available),
        make_render_text_utf8proc_backend_candidate(real_backends_available),
        make_render_text_deterministic_fake_backend_candidate(),
    };
}

inline render_text_font_backend_capability_probe_request render_text_font_backend_probe_request_for_selection(
    const render_text_font_backend_candidate& candidate,
    std::vector<render_text_font_backend_feature> required_features)
{
    return render_text_font_backend_capability_probe_request{
        .required_libraries = {
            candidate.library,
        },
        .required_features = std::move(required_features),
    };
}

inline render_text_font_backend_capability_snapshot render_text_font_backend_capability_for_selection(
    const render_text_font_backend_candidate& candidate,
    const std::vector<render_text_font_backend_feature>& required_features)
{
    return make_render_text_font_backend_capability_snapshot(
        {candidate.component()},
        render_text_font_backend_probe_request_for_selection(candidate, required_features));
}

inline render_text_font_backend_adapter_functions render_text_font_backend_adapter_functions_for_selection(
    const render_text_font_backend_candidate& candidate)
{
    return render_text_font_backend_adapter_functions{
        .shape = deterministic_fake_real_font_backend_shape,
        .rasterize = deterministic_fake_real_font_backend_rasterize,
        .label = candidate.label,
    };
}

inline render_text_font_backend_selection_result make_render_text_font_backend_selection_result(
    const render_text_font_backend_selection_status status,
    const render_text_font_backend_selection_purpose purpose,
    render_text_font_backend_candidate candidate,
    std::vector<render_text_font_backend_feature> required_features,
    std::vector<std::string> diagnostics = {})
{
    const bool fallback = candidate.library == render_text_font_backend_library::deterministic_fake;
    render_text_font_backend_selection_result result{
        .status = status,
        .purpose = purpose,
        .selected = std::move(candidate),
        .has_selection = status == render_text_font_backend_selection_status::selected
            || status == render_text_font_backend_selection_status::fallback_selected,
        .used_deterministic_fallback = fallback,
        .required_features = std::move(required_features),
    };
    result.capability = render_text_font_backend_capability_for_selection(
        result.selected,
        result.required_features);
    result.adapter_functions = render_text_font_backend_adapter_functions_for_selection(result.selected);
    result.diagnostics = std::move(diagnostics);
    result.diagnostics.push_back(
        render_text_font_backend_selection_status_name(status)
        + " "
        + result.selected.label
        + " for "
        + render_text_font_backend_selection_purpose_name(purpose));
    return result;
}

inline render_text_font_backend_selection_result select_render_text_font_backend(
    render_text_font_backend_selection_request request)
{
    std::vector<render_text_font_backend_feature> required_features =
        request.required_features.empty()
            ? render_text_font_backend_required_features_for(request.purpose)
            : std::move(request.required_features);

    bool saw_available_candidate = false;
    bool saw_feature_capable_candidate = false;
    for (const render_text_font_backend_candidate& candidate : request.candidates) {
        if (candidate.library == render_text_font_backend_library::deterministic_fake) {
            continue;
        }
        if (!candidate.available) {
            continue;
        }
        saw_available_candidate = true;
        if (!render_text_font_backend_candidate_supports_features(candidate, required_features)) {
            continue;
        }
        saw_feature_capable_candidate = true;
        return make_render_text_font_backend_selection_result(
            render_text_font_backend_selection_status::selected,
            request.purpose,
            candidate,
            required_features);
    }

    if (request.allow_deterministic_fallback) {
        render_text_font_backend_candidate fallback = make_render_text_deterministic_fake_backend_candidate();
        for (const render_text_font_backend_candidate& candidate : request.candidates) {
            if (candidate.library == render_text_font_backend_library::deterministic_fake) {
                fallback = candidate;
                break;
            }
        }
        return make_render_text_font_backend_selection_result(
            render_text_font_backend_selection_status::fallback_selected,
            request.purpose,
            std::move(fallback),
            required_features,
            {"real backend candidate was not usable; deterministic fallback selected"});
    }

    const render_text_font_backend_selection_status status =
        saw_available_candidate && !saw_feature_capable_candidate
            ? render_text_font_backend_selection_status::unsupported_feature
            : render_text_font_backend_selection_status::unavailable;
    return render_text_font_backend_selection_result{
        .status = status,
        .purpose = request.purpose,
        .required_features = std::move(required_features),
        .diagnostics = {
            status == render_text_font_backend_selection_status::unsupported_feature
                ? "available backend candidates did not satisfy required text features"
                : "no available backend candidate satisfied the selection request",
        },
    };
}

} // namespace quiz_vulkan::render
