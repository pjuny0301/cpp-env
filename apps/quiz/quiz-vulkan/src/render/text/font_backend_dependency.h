#pragma once

#include "render/text/font_backend_selection.h"

#ifndef QUIZ_VULKAN_HAS_FREETYPE_HEADERS
#define QUIZ_VULKAN_HAS_FREETYPE_HEADERS 0
#endif

#ifndef QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS
#define QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS 0
#endif

#ifndef QUIZ_VULKAN_HAS_UTF8PROC_HEADERS
#define QUIZ_VULKAN_HAS_UTF8PROC_HEADERS 0
#endif

#if QUIZ_VULKAN_HAS_FREETYPE_HEADERS
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#if QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS
#include <hb.h>
#endif

#if QUIZ_VULKAN_HAS_UTF8PROC_HEADERS
#include <utf8proc.h>
#endif

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_backend_adapter_readiness_status {
    adapter_ready,
    fallback_ready,
    missing_dependency,
    adapter_unavailable,
    version_mismatch,
    unsupported_feature,
};

inline std::string render_text_font_backend_adapter_readiness_status_name(
    const render_text_font_backend_adapter_readiness_status status)
{
    switch (status) {
    case render_text_font_backend_adapter_readiness_status::adapter_ready:
        return "adapter_ready";
    case render_text_font_backend_adapter_readiness_status::fallback_ready:
        return "fallback_ready";
    case render_text_font_backend_adapter_readiness_status::missing_dependency:
        return "missing_dependency";
    case render_text_font_backend_adapter_readiness_status::adapter_unavailable:
        return "adapter_unavailable";
    case render_text_font_backend_adapter_readiness_status::version_mismatch:
        return "version_mismatch";
    case render_text_font_backend_adapter_readiness_status::unsupported_feature:
        return "unsupported_feature";
    }

    return "unknown";
}

inline bool render_text_external_font_backend_hint_uses_absolute_or_host_path(
    const std::string& value)
{
    return !value.empty()
        && (value.front() == '/'
            || value.find("\\") != std::string::npos
            || value.find(":\\") != std::string::npos
            || value.find(":/") != std::string::npos
            || value.find("/mnt/") != std::string::npos
            || value.find("/Users/") != std::string::npos
            || value.find(std::string{"build"} + "/" + "external/") != std::string::npos);
}

struct render_text_external_font_backend_dependency {
    render_text_font_backend_library library =
        render_text_font_backend_library::deterministic_fake;
    std::string label;
    render_text_font_backend_version version;
    std::string license;
    std::string source_hint;
    std::string include_hint;
    std::string library_hint;
    std::vector<render_text_font_backend_feature> features;
    bool source_available = false;
    bool build_linked = false;
    bool adapter_symbols_available = false;
    bool fallback_only = false;
    std::string diagnostic;

    bool supports_feature(const render_text_font_backend_feature feature) const
    {
        return std::find(features.begin(), features.end(), feature) != features.end();
    }

    bool adapter_ready() const
    {
        return source_available && build_linked && adapter_symbols_available;
    }

    bool metadata_is_portable() const
    {
        return !render_text_external_font_backend_hint_uses_absolute_or_host_path(source_hint)
            && !render_text_font_backend_candidate_uses_absolute_or_host_path(candidate());
    }

    render_text_font_backend_candidate candidate() const
    {
        return render_text_font_backend_candidate{
            .library = library,
            .label = label,
            .version = version,
            .license = license,
            .include_hint = include_hint,
            .library_hint = library_hint,
            .features = features,
            .available = adapter_ready(),
            .fallback_only = fallback_only,
            .diagnostic = diagnostic,
        };
    }

    render_text_font_backend_component component() const
    {
        return render_text_font_backend_component{
            .library = library,
            .name = label,
            .available = adapter_ready(),
            .version = version,
            .features = features,
            .fallback_only = fallback_only,
            .diagnostic = diagnostic,
        };
    }
};

struct render_text_external_font_backend_manifest {
    std::string label;
    std::vector<render_text_external_font_backend_dependency> dependencies;
    bool allow_deterministic_fallback = true;

    bool empty() const
    {
        return dependencies.empty();
    }
};

struct render_text_external_font_backend_header_probe {
    render_text_font_backend_library library =
        render_text_font_backend_library::deterministic_fake;
    std::string label;
    std::string approved_header;
    bool header_available = false;
    bool version_available = false;
    render_text_font_backend_version version;
    std::vector<render_text_font_backend_feature> features;
    std::string diagnostic;

    bool supports_feature(const render_text_font_backend_feature feature) const
    {
        return std::find(features.begin(), features.end(), feature) != features.end();
    }

    render_text_external_font_backend_dependency header_dependency() const
    {
        return render_text_external_font_backend_dependency{
            .library = library,
            .label = label,
            .version = version,
            .license = library == render_text_font_backend_library::freetype
                ? "FreeType License or GPL-2.0-or-later"
                : library == render_text_font_backend_library::harfbuzz
                    ? "Old MIT"
                    : "MIT",
            .source_hint = "desktop external header boundary",
            .include_hint = approved_header,
            .library_hint = "not linked by header probe",
            .features = features,
            .source_available = header_available,
            .build_linked = false,
            .adapter_symbols_available = false,
            .fallback_only = false,
            .diagnostic = diagnostic,
        };
    }
};

struct render_text_external_font_backend_header_probe_snapshot {
    std::vector<render_text_external_font_backend_header_probe> probes;
    bool freetype_headers_available = false;
    bool harfbuzz_headers_available = false;
    bool utf8proc_headers_available = false;
    bool fake_fallback_preserved = true;
    std::size_t available_header_count = 0;
    std::size_t versioned_header_count = 0;
    std::size_t advertised_feature_count = 0;
    std::string diagnostic;

    bool any_real_headers_available() const
    {
        return available_header_count != 0U;
    }

    bool all_real_headers_available() const
    {
        return freetype_headers_available && harfbuzz_headers_available && utf8proc_headers_available;
    }
};

struct render_text_external_font_backend_probe_request {
    render_text_font_backend_selection_purpose purpose =
        render_text_font_backend_selection_purpose::shaping;
    std::vector<render_text_font_backend_library> required_libraries;
    std::vector<render_text_font_backend_feature> required_features;
    std::vector<render_text_font_backend_minimum_version> minimum_versions;
    bool allow_deterministic_fallback = true;
};

struct render_text_external_font_backend_probe_result {
    render_text_font_backend_adapter_readiness_status status =
        render_text_font_backend_adapter_readiness_status::missing_dependency;
    render_text_font_backend_adapter_readiness_status fallback_reason =
        render_text_font_backend_adapter_readiness_status::missing_dependency;
    render_text_font_backend_selection_purpose purpose =
        render_text_font_backend_selection_purpose::shaping;
    std::string manifest_label;
    render_text_font_backend_capability_snapshot requested_capability;
    render_text_font_backend_selection_result selection;
    render_text_font_backend_adapter_functions adapter_functions;
    std::vector<render_text_external_font_backend_dependency> dependencies;
    std::vector<render_text_font_backend_library> missing_dependencies;
    std::vector<render_text_font_backend_library> adapter_unavailable_dependencies;
    bool adapter_ready = false;
    bool fallback_ready = false;
    bool can_attempt_real_backend = false;
    bool used_deterministic_fallback = false;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_backend_adapter_readiness_status::adapter_ready
            || status == render_text_font_backend_adapter_readiness_status::fallback_ready;
    }

    bool selected_real_backend() const
    {
        return selection.selected_real_backend();
    }
};

struct render_text_external_font_backend_probe_state_snapshot {
    render_text_font_backend_selection_purpose purpose =
        render_text_font_backend_selection_purpose::shaping;
    render_text_font_backend_adapter_readiness_status status =
        render_text_font_backend_adapter_readiness_status::missing_dependency;
    render_text_font_backend_adapter_readiness_status fallback_reason =
        render_text_font_backend_adapter_readiness_status::missing_dependency;
    render_text_font_backend_selection_status selection_status =
        render_text_font_backend_selection_status::unavailable;
    render_text_font_backend_capability_status capability_status =
        render_text_font_backend_capability_status::unavailable;
    render_text_font_backend_library selected_library =
        render_text_font_backend_library::deterministic_fake;
    std::string selected_label;
    bool fake_only = false;
    bool adapter_ready = false;
    bool fallback_ready = false;
    bool unavailable = false;
    bool mismatch = false;
    bool selected_real_backend = false;
    bool can_attempt_real_backend = false;
    bool used_deterministic_fallback = false;
    std::size_t dependency_count = 0;
    std::size_t missing_dependency_count = 0;
    std::size_t adapter_unavailable_count = 0;
    std::size_t version_mismatch_count = 0;
    std::size_t missing_feature_count = 0;
    std::string diagnostic;
};

struct render_text_external_font_backend_probe_diff_snapshot {
    render_text_font_backend_selection_purpose purpose =
        render_text_font_backend_selection_purpose::shaping;
    render_text_external_font_backend_probe_state_snapshot before;
    render_text_external_font_backend_probe_state_snapshot after;
    bool has_changes = false;
    bool readiness_changed = false;
    bool fallback_reason_changed = false;
    bool selected_backend_changed = false;
    bool capability_status_changed = false;
    bool fake_only_changed = false;
    bool adapter_ready_changed = false;
    bool unavailable_changed = false;
    bool mismatch_changed = false;
    std::ptrdiff_t dependency_count_delta = 0;
    std::ptrdiff_t missing_dependency_delta = 0;
    std::ptrdiff_t adapter_unavailable_delta = 0;
    std::ptrdiff_t version_mismatch_delta = 0;
    std::ptrdiff_t missing_feature_delta = 0;
    std::string summary;

    bool became_fake_only() const
    {
        return !before.fake_only && after.fake_only;
    }

    bool became_adapter_ready() const
    {
        return !before.adapter_ready && after.adapter_ready;
    }

    bool became_unavailable() const
    {
        return !before.unavailable && after.unavailable;
    }

    bool became_mismatch() const
    {
        return !before.mismatch && after.mismatch;
    }
};

struct render_text_external_font_backend_probe_diff_summary_snapshot {
    std::vector<render_text_external_font_backend_probe_diff_snapshot> diffs;
    std::size_t changed_count = 0;
    std::size_t adapter_ready_transition_count = 0;
    std::size_t fake_only_transition_count = 0;
    std::size_t unavailable_transition_count = 0;
    std::size_t mismatch_transition_count = 0;
    std::size_t selected_backend_change_count = 0;
    std::ptrdiff_t total_missing_dependency_delta = 0;
    std::ptrdiff_t total_adapter_unavailable_delta = 0;
    std::ptrdiff_t total_version_mismatch_delta = 0;
    std::ptrdiff_t total_missing_feature_delta = 0;
    std::string summary;

    bool has_changes() const
    {
        return changed_count > 0;
    }
};

class font_backend_dependency_probe_interface {
public:
    virtual ~font_backend_dependency_probe_interface() = default;

    virtual render_text_external_font_backend_probe_result probe(
        const render_text_external_font_backend_probe_request& request) const = 0;
};

inline render_text_external_font_backend_dependency render_text_external_font_backend_dependency_from_candidate(
    const render_text_font_backend_candidate& candidate,
    const bool source_available,
    const bool build_linked,
    const bool adapter_symbols_available)
{
    return render_text_external_font_backend_dependency{
        .library = candidate.library,
        .label = candidate.label,
        .version = candidate.version,
        .license = candidate.license,
        .source_hint = candidate.library_hint,
        .include_hint = candidate.include_hint,
        .library_hint = candidate.library_hint,
        .features = candidate.features,
        .source_available = source_available,
        .build_linked = build_linked,
        .adapter_symbols_available = adapter_symbols_available,
        .fallback_only = candidate.fallback_only,
        .diagnostic = candidate.diagnostic,
    };
}

inline render_text_external_font_backend_dependency make_render_text_harfbuzz_external_dependency(
    const bool source_available = false,
    const bool build_linked = false,
    const bool adapter_symbols_available = false)
{
    return render_text_external_font_backend_dependency_from_candidate(
        make_render_text_harfbuzz_backend_candidate(),
        source_available,
        build_linked,
        adapter_symbols_available);
}

inline render_text_external_font_backend_dependency make_render_text_freetype_external_dependency(
    const bool source_available = false,
    const bool build_linked = false,
    const bool adapter_symbols_available = false)
{
    return render_text_external_font_backend_dependency_from_candidate(
        make_render_text_freetype_backend_candidate(),
        source_available,
        build_linked,
        adapter_symbols_available);
}

inline render_text_external_font_backend_dependency make_render_text_utf8proc_external_dependency(
    const bool source_available = false,
    const bool build_linked = false,
    const bool adapter_symbols_available = false)
{
    return render_text_external_font_backend_dependency_from_candidate(
        make_render_text_utf8proc_backend_candidate(),
        source_available,
        build_linked,
        adapter_symbols_available);
}

inline render_text_external_font_backend_dependency make_render_text_deterministic_fake_external_dependency()
{
    return render_text_external_font_backend_dependency_from_candidate(
        make_render_text_deterministic_fake_backend_candidate(),
        true,
        true,
        true);
}

inline bool render_text_external_font_backend_header_available(
    const render_text_font_backend_library library)
{
    switch (library) {
    case render_text_font_backend_library::freetype:
        return QUIZ_VULKAN_HAS_FREETYPE_HEADERS != 0;
    case render_text_font_backend_library::harfbuzz:
        return QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS != 0;
    case render_text_font_backend_library::utf8proc:
        return QUIZ_VULKAN_HAS_UTF8PROC_HEADERS != 0;
    case render_text_font_backend_library::deterministic_fake:
    case render_text_font_backend_library::directwrite:
        return false;
    }

    return false;
}

inline render_text_font_backend_version render_text_freetype_header_version()
{
#if QUIZ_VULKAN_HAS_FREETYPE_HEADERS
    return render_text_font_backend_version{
        .major = FREETYPE_MAJOR,
        .minor = FREETYPE_MINOR,
        .patch = FREETYPE_PATCH,
    };
#else
    return {};
#endif
}

inline render_text_font_backend_version render_text_harfbuzz_header_version()
{
#if QUIZ_VULKAN_HAS_HARFBUZZ_HEADERS
    return render_text_font_backend_version{
        .major = HB_VERSION_MAJOR,
        .minor = HB_VERSION_MINOR,
        .patch = HB_VERSION_MICRO,
    };
#else
    return {};
#endif
}

inline render_text_font_backend_version render_text_utf8proc_header_version()
{
#if QUIZ_VULKAN_HAS_UTF8PROC_HEADERS \
    && defined(UTF8PROC_VERSION_MAJOR) \
    && defined(UTF8PROC_VERSION_MINOR) \
    && defined(UTF8PROC_VERSION_PATCH)
    return render_text_font_backend_version{
        .major = UTF8PROC_VERSION_MAJOR,
        .minor = UTF8PROC_VERSION_MINOR,
        .patch = UTF8PROC_VERSION_PATCH,
    };
#else
    return {};
#endif
}

inline render_text_font_backend_version render_text_external_font_backend_header_version(
    const render_text_font_backend_library library)
{
    switch (library) {
    case render_text_font_backend_library::freetype:
        return render_text_freetype_header_version();
    case render_text_font_backend_library::harfbuzz:
        return render_text_harfbuzz_header_version();
    case render_text_font_backend_library::utf8proc:
        return render_text_utf8proc_header_version();
    case render_text_font_backend_library::deterministic_fake:
    case render_text_font_backend_library::directwrite:
        return {};
    }

    return {};
}

inline bool render_text_external_font_backend_header_version_available(
    const render_text_font_backend_library library)
{
    const render_text_font_backend_version version =
        render_text_external_font_backend_header_version(library);
    return version.major != 0U || version.minor != 0U || version.patch != 0U || !version.label.empty();
}

inline std::vector<render_text_font_backend_feature> render_text_external_font_backend_header_features_for(
    const render_text_font_backend_library library)
{
    switch (library) {
    case render_text_font_backend_library::freetype:
        return {
            render_text_font_backend_feature::font_file_loading,
            render_text_font_backend_feature::unicode_cmap,
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_rasterization,
        };
    case render_text_font_backend_library::harfbuzz:
        return {
            render_text_font_backend_feature::glyph_id_mapping,
            render_text_font_backend_feature::glyph_shaping,
            render_text_font_backend_feature::complex_script_shaping,
        };
    case render_text_font_backend_library::utf8proc:
        return {
            render_text_font_backend_feature::unicode_normalization,
            render_text_font_backend_feature::unicode_properties,
        };
    case render_text_font_backend_library::deterministic_fake:
    case render_text_font_backend_library::directwrite:
        return {};
    }

    return {};
}

inline std::string render_text_external_font_backend_header_path_for(
    const render_text_font_backend_library library)
{
    switch (library) {
    case render_text_font_backend_library::freetype:
        return "ft2build.h";
    case render_text_font_backend_library::harfbuzz:
        return "hb.h";
    case render_text_font_backend_library::utf8proc:
        return "utf8proc.h";
    case render_text_font_backend_library::deterministic_fake:
    case render_text_font_backend_library::directwrite:
        return {};
    }

    return {};
}

inline std::string render_text_external_font_backend_header_diagnostic_for(
    const render_text_font_backend_library library,
    const bool header_available,
    const bool version_available)
{
    const std::string name = render_text_font_backend_library_name(library);
    if (!header_available) {
        return name + " approved header is unavailable; deterministic fake text fallback remains usable";
    }
    if (!version_available) {
        return name + " approved header is available without version macro evidence";
    }
    return name + " approved header is available with compile-time version evidence";
}

inline render_text_external_font_backend_header_probe make_render_text_external_font_backend_header_probe(
    const render_text_font_backend_library library)
{
    const bool header_available = render_text_external_font_backend_header_available(library);
    const bool version_available =
        header_available && render_text_external_font_backend_header_version_available(library);
    return render_text_external_font_backend_header_probe{
        .library = library,
        .label = render_text_font_backend_library_name(library),
        .approved_header = render_text_external_font_backend_header_path_for(library),
        .header_available = header_available,
        .version_available = version_available,
        .version = render_text_external_font_backend_header_version(library),
        .features = header_available
            ? render_text_external_font_backend_header_features_for(library)
            : std::vector<render_text_font_backend_feature>{},
        .diagnostic = render_text_external_font_backend_header_diagnostic_for(
            library,
            header_available,
            version_available),
    };
}

inline const render_text_external_font_backend_header_probe*
find_render_text_external_font_backend_header_probe(
    const std::vector<render_text_external_font_backend_header_probe>& probes,
    const render_text_font_backend_library library)
{
    const auto match = std::find_if(
        probes.begin(),
        probes.end(),
        [&](const render_text_external_font_backend_header_probe& probe) {
            return probe.library == library;
        });
    return match == probes.end() ? nullptr : &*match;
}

inline void append_render_text_external_font_backend_header_probe(
    render_text_external_font_backend_header_probe_snapshot& snapshot,
    render_text_external_font_backend_header_probe probe)
{
    if (probe.header_available) {
        ++snapshot.available_header_count;
        snapshot.advertised_feature_count += probe.features.size();
    }
    if (probe.version_available) {
        ++snapshot.versioned_header_count;
    }
    switch (probe.library) {
    case render_text_font_backend_library::freetype:
        snapshot.freetype_headers_available = probe.header_available;
        break;
    case render_text_font_backend_library::harfbuzz:
        snapshot.harfbuzz_headers_available = probe.header_available;
        break;
    case render_text_font_backend_library::utf8proc:
        snapshot.utf8proc_headers_available = probe.header_available;
        break;
    case render_text_font_backend_library::deterministic_fake:
    case render_text_font_backend_library::directwrite:
        break;
    }
    snapshot.probes.push_back(std::move(probe));
}

inline render_text_external_font_backend_header_probe_snapshot
make_render_text_external_font_backend_header_probe_snapshot()
{
    render_text_external_font_backend_header_probe_snapshot snapshot;
    append_render_text_external_font_backend_header_probe(
        snapshot,
        make_render_text_external_font_backend_header_probe(render_text_font_backend_library::freetype));
    append_render_text_external_font_backend_header_probe(
        snapshot,
        make_render_text_external_font_backend_header_probe(render_text_font_backend_library::harfbuzz));
    append_render_text_external_font_backend_header_probe(
        snapshot,
        make_render_text_external_font_backend_header_probe(render_text_font_backend_library::utf8proc));
    snapshot.diagnostic = snapshot.any_real_headers_available()
        ? "approved desktop text dependency headers are available through the external header boundary"
        : "approved desktop text dependency headers are unavailable; deterministic fake text fallback is preserved";
    return snapshot;
}

inline std::vector<render_text_external_font_backend_dependency>
render_text_external_font_backend_header_dependencies(
    const render_text_external_font_backend_header_probe_snapshot& snapshot)
{
    std::vector<render_text_external_font_backend_dependency> dependencies;
    dependencies.reserve(snapshot.probes.size() + 1U);
    for (const render_text_external_font_backend_header_probe& probe : snapshot.probes) {
        dependencies.push_back(probe.header_dependency());
    }
    dependencies.push_back(make_render_text_deterministic_fake_external_dependency());
    return dependencies;
}

inline render_text_external_font_backend_manifest make_render_text_header_backed_external_font_backend_manifest()
{
    const render_text_external_font_backend_header_probe_snapshot snapshot =
        make_render_text_external_font_backend_header_probe_snapshot();
    return render_text_external_font_backend_manifest{
        .label = "text-owned external font backend header manifest",
        .dependencies = render_text_external_font_backend_header_dependencies(snapshot),
        .allow_deterministic_fallback = snapshot.fake_fallback_preserved,
    };
}

inline render_text_external_font_backend_manifest make_render_text_known_external_font_backend_manifest(
    const bool real_sources_available = false,
    const bool real_build_linked = false,
    const bool real_adapter_symbols_available = false)
{
    return render_text_external_font_backend_manifest{
        .label = "text-owned external font backend manifest",
        .dependencies = {
            make_render_text_harfbuzz_external_dependency(
                real_sources_available,
                real_build_linked,
                real_adapter_symbols_available),
            make_render_text_freetype_external_dependency(
                real_sources_available,
                real_build_linked,
                real_adapter_symbols_available),
            make_render_text_utf8proc_external_dependency(
                real_sources_available,
                real_build_linked,
                real_adapter_symbols_available),
            make_render_text_deterministic_fake_external_dependency(),
        },
    };
}

inline std::vector<render_text_font_backend_library> render_text_external_font_backend_default_libraries_for(
    const render_text_font_backend_selection_purpose purpose)
{
    switch (purpose) {
    case render_text_font_backend_selection_purpose::shaping:
        return {render_text_font_backend_library::harfbuzz};
    case render_text_font_backend_selection_purpose::rasterization:
        return {render_text_font_backend_library::freetype};
    case render_text_font_backend_selection_purpose::unicode_processing:
        return {render_text_font_backend_library::utf8proc};
    }

    return {};
}

inline const render_text_external_font_backend_dependency* find_render_text_external_font_backend_dependency(
    const std::vector<render_text_external_font_backend_dependency>& dependencies,
    const render_text_font_backend_library library)
{
    const auto match = std::find_if(
        dependencies.begin(),
        dependencies.end(),
        [&](const render_text_external_font_backend_dependency& dependency) {
            return dependency.library == library;
        });
    return match == dependencies.end() ? nullptr : &*match;
}

inline std::vector<render_text_font_backend_library> missing_render_text_external_font_backend_dependencies(
    const std::vector<render_text_external_font_backend_dependency>& dependencies,
    const std::vector<render_text_font_backend_library>& required_libraries)
{
    std::vector<render_text_font_backend_library> missing;
    for (const render_text_font_backend_library library : required_libraries) {
        const render_text_external_font_backend_dependency* dependency =
            find_render_text_external_font_backend_dependency(dependencies, library);
        if (dependency == nullptr || !dependency->source_available) {
            missing.push_back(library);
        }
    }
    return missing;
}

inline std::vector<render_text_font_backend_library> unavailable_render_text_external_font_backend_adapters(
    const std::vector<render_text_external_font_backend_dependency>& dependencies,
    const std::vector<render_text_font_backend_library>& required_libraries)
{
    std::vector<render_text_font_backend_library> unavailable;
    for (const render_text_font_backend_library library : required_libraries) {
        const render_text_external_font_backend_dependency* dependency =
            find_render_text_external_font_backend_dependency(dependencies, library);
        if (dependency != nullptr && dependency->source_available && !dependency->adapter_ready()) {
            unavailable.push_back(library);
        }
    }
    return unavailable;
}

inline std::vector<render_text_font_backend_component> render_text_external_font_backend_components(
    const std::vector<render_text_external_font_backend_dependency>& dependencies)
{
    std::vector<render_text_font_backend_component> components;
    components.reserve(dependencies.size());
    for (const render_text_external_font_backend_dependency& dependency : dependencies) {
        components.push_back(dependency.component());
    }
    return components;
}

inline std::vector<render_text_font_backend_candidate> render_text_external_font_backend_candidates(
    const std::vector<render_text_external_font_backend_dependency>& dependencies)
{
    std::vector<render_text_font_backend_candidate> candidates;
    candidates.reserve(dependencies.size());
    for (const render_text_external_font_backend_dependency& dependency : dependencies) {
        candidates.push_back(dependency.candidate());
    }
    return candidates;
}

inline render_text_font_backend_adapter_readiness_status render_text_external_font_backend_failure_status_for(
    const render_text_font_backend_capability_snapshot& capability,
    const std::vector<render_text_font_backend_library>& missing_dependencies,
    const std::vector<render_text_font_backend_library>& adapter_unavailable_dependencies)
{
    if (!missing_dependencies.empty()) {
        return render_text_font_backend_adapter_readiness_status::missing_dependency;
    }
    if (!capability.version_mismatches.empty()) {
        return render_text_font_backend_adapter_readiness_status::version_mismatch;
    }
    if (!adapter_unavailable_dependencies.empty()) {
        return render_text_font_backend_adapter_readiness_status::adapter_unavailable;
    }
    if (!capability.missing_features.empty()) {
        return render_text_font_backend_adapter_readiness_status::unsupported_feature;
    }
    return render_text_font_backend_adapter_readiness_status::missing_dependency;
}

inline render_text_font_backend_adapter_readiness_status render_text_external_font_backend_readiness_status_for(
    const render_text_font_backend_selection_result& selection,
    const render_text_font_backend_capability_snapshot& capability,
    const std::vector<render_text_font_backend_library>& missing_dependencies,
    const std::vector<render_text_font_backend_library>& adapter_unavailable_dependencies)
{
    if (selection.selected_real_backend() && capability.ok()) {
        return render_text_font_backend_adapter_readiness_status::adapter_ready;
    }
    if (selection.status == render_text_font_backend_selection_status::fallback_selected) {
        return render_text_font_backend_adapter_readiness_status::fallback_ready;
    }
    return render_text_external_font_backend_failure_status_for(
        capability,
        missing_dependencies,
        adapter_unavailable_dependencies);
}

inline std::string render_text_external_font_backend_probe_diagnostic_for(
    const render_text_font_backend_adapter_readiness_status status,
    const render_text_font_backend_adapter_readiness_status fallback_reason)
{
    if (status == render_text_font_backend_adapter_readiness_status::adapter_ready) {
        return "external text font backend adapter is ready for real font work";
    }
    if (status == render_text_font_backend_adapter_readiness_status::fallback_ready) {
        return "external text font backend is not adapter-ready; deterministic fallback remains selected after "
            + render_text_font_backend_adapter_readiness_status_name(fallback_reason);
    }
    return "external text font backend probe failed with "
        + render_text_font_backend_adapter_readiness_status_name(status);
}

inline bool render_text_external_font_backend_probe_result_is_mismatch(
    const render_text_external_font_backend_probe_result& result)
{
    if (result.status == render_text_font_backend_adapter_readiness_status::adapter_ready) {
        return false;
    }
    return result.status == render_text_font_backend_adapter_readiness_status::version_mismatch
        || result.status == render_text_font_backend_adapter_readiness_status::unsupported_feature
        || result.fallback_reason == render_text_font_backend_adapter_readiness_status::version_mismatch
        || result.fallback_reason == render_text_font_backend_adapter_readiness_status::unsupported_feature
        || result.requested_capability.status == render_text_font_backend_capability_status::version_mismatch
        || result.requested_capability.status == render_text_font_backend_capability_status::unsupported_feature
        || !result.requested_capability.version_mismatches.empty()
        || !result.requested_capability.missing_features.empty();
}

inline bool render_text_external_font_backend_probe_result_is_unavailable(
    const render_text_external_font_backend_probe_result& result)
{
    if (result.status == render_text_font_backend_adapter_readiness_status::adapter_ready) {
        return false;
    }
    return result.status == render_text_font_backend_adapter_readiness_status::missing_dependency
        || result.status == render_text_font_backend_adapter_readiness_status::adapter_unavailable
        || result.fallback_reason == render_text_font_backend_adapter_readiness_status::missing_dependency
        || result.fallback_reason == render_text_font_backend_adapter_readiness_status::adapter_unavailable
        || result.requested_capability.status == render_text_font_backend_capability_status::unavailable
        || !result.missing_dependencies.empty()
        || !result.adapter_unavailable_dependencies.empty();
}

inline render_text_external_font_backend_probe_state_snapshot
make_render_text_external_font_backend_probe_state_snapshot(
    const render_text_external_font_backend_probe_result& result)
{
    const bool has_selection = result.selection.has_selection;
    const render_text_font_backend_library selected_library = has_selection
        ? result.selection.selected.library
        : render_text_font_backend_library::deterministic_fake;
    const bool fake_only =
        result.used_deterministic_fallback
        || (has_selection && selected_library == render_text_font_backend_library::deterministic_fake);

    return render_text_external_font_backend_probe_state_snapshot{
        .purpose = result.purpose,
        .status = result.status,
        .fallback_reason = result.fallback_reason,
        .selection_status = result.selection.status,
        .capability_status = result.requested_capability.status,
        .selected_library = selected_library,
        .selected_label = has_selection ? result.selection.selected.label : std::string{},
        .fake_only = fake_only,
        .adapter_ready = result.adapter_ready
            || result.status == render_text_font_backend_adapter_readiness_status::adapter_ready,
        .fallback_ready = result.fallback_ready
            || result.status == render_text_font_backend_adapter_readiness_status::fallback_ready,
        .unavailable = render_text_external_font_backend_probe_result_is_unavailable(result),
        .mismatch = render_text_external_font_backend_probe_result_is_mismatch(result),
        .selected_real_backend = result.selected_real_backend(),
        .can_attempt_real_backend = result.can_attempt_real_backend,
        .used_deterministic_fallback = result.used_deterministic_fallback,
        .dependency_count = result.dependencies.size(),
        .missing_dependency_count = result.missing_dependencies.size(),
        .adapter_unavailable_count = result.adapter_unavailable_dependencies.size(),
        .version_mismatch_count = result.requested_capability.version_mismatches.size(),
        .missing_feature_count = result.requested_capability.missing_features.size(),
        .diagnostic = result.diagnostic,
    };
}

inline std::string render_text_external_font_backend_probe_state_label(
    const render_text_external_font_backend_probe_state_snapshot& state)
{
    if (state.adapter_ready) {
        return "adapter-ready";
    }
    if (state.mismatch) {
        return state.fake_only ? "fake-only mismatch fallback" : "mismatch";
    }
    if (state.unavailable) {
        return state.fake_only ? "fake-only unavailable fallback" : "unavailable";
    }
    if (state.fake_only) {
        return "fake-only";
    }
    if (state.fallback_ready) {
        return "fallback-ready";
    }
    return render_text_font_backend_adapter_readiness_status_name(state.status);
}

inline std::string render_text_external_font_backend_probe_diff_summary_for(
    const render_text_external_font_backend_probe_state_snapshot& before,
    const render_text_external_font_backend_probe_state_snapshot& after)
{
    const std::string purpose = render_text_font_backend_selection_purpose_name(after.purpose);
    const std::string before_label = render_text_external_font_backend_probe_state_label(before);
    const std::string after_label = render_text_external_font_backend_probe_state_label(after);
    if (before_label == after_label
        && before.selected_library == after.selected_library
        && before.capability_status == after.capability_status) {
        return "external font backend probe unchanged for " + purpose;
    }
    return "external font backend probe changed for " + purpose + ": "
        + before_label + " -> " + after_label;
}

inline render_text_external_font_backend_probe_diff_snapshot
diff_render_text_external_font_backend_probe_result(
    const render_text_external_font_backend_probe_result& before_result,
    const render_text_external_font_backend_probe_result& after_result)
{
    const render_text_external_font_backend_probe_state_snapshot before =
        make_render_text_external_font_backend_probe_state_snapshot(before_result);
    const render_text_external_font_backend_probe_state_snapshot after =
        make_render_text_external_font_backend_probe_state_snapshot(after_result);

    render_text_external_font_backend_probe_diff_snapshot diff{
        .purpose = after.purpose,
        .before = before,
        .after = after,
        .readiness_changed = before.status != after.status,
        .fallback_reason_changed = before.fallback_reason != after.fallback_reason,
        .selected_backend_changed = before.selected_library != after.selected_library
            || before.selected_label != after.selected_label,
        .capability_status_changed = before.capability_status != after.capability_status,
        .fake_only_changed = before.fake_only != after.fake_only,
        .adapter_ready_changed = before.adapter_ready != after.adapter_ready,
        .unavailable_changed = before.unavailable != after.unavailable,
        .mismatch_changed = before.mismatch != after.mismatch,
        .dependency_count_delta =
            static_cast<std::ptrdiff_t>(after.dependency_count)
            - static_cast<std::ptrdiff_t>(before.dependency_count),
        .missing_dependency_delta =
            static_cast<std::ptrdiff_t>(after.missing_dependency_count)
            - static_cast<std::ptrdiff_t>(before.missing_dependency_count),
        .adapter_unavailable_delta =
            static_cast<std::ptrdiff_t>(after.adapter_unavailable_count)
            - static_cast<std::ptrdiff_t>(before.adapter_unavailable_count),
        .version_mismatch_delta =
            static_cast<std::ptrdiff_t>(after.version_mismatch_count)
            - static_cast<std::ptrdiff_t>(before.version_mismatch_count),
        .missing_feature_delta =
            static_cast<std::ptrdiff_t>(after.missing_feature_count)
            - static_cast<std::ptrdiff_t>(before.missing_feature_count),
        .summary = render_text_external_font_backend_probe_diff_summary_for(before, after),
    };
    diff.has_changes =
        diff.readiness_changed
        || diff.fallback_reason_changed
        || diff.selected_backend_changed
        || diff.capability_status_changed
        || diff.fake_only_changed
        || diff.adapter_ready_changed
        || diff.unavailable_changed
        || diff.mismatch_changed
        || diff.dependency_count_delta != 0
        || diff.missing_dependency_delta != 0
        || diff.adapter_unavailable_delta != 0
        || diff.version_mismatch_delta != 0
        || diff.missing_feature_delta != 0;
    return diff;
}

inline const render_text_external_font_backend_probe_result*
find_render_text_external_font_backend_probe_result(
    const std::vector<render_text_external_font_backend_probe_result>& results,
    const render_text_font_backend_selection_purpose purpose)
{
    const auto match = std::find_if(
        results.begin(),
        results.end(),
        [&](const render_text_external_font_backend_probe_result& result) {
            return result.purpose == purpose;
        });
    return match == results.end() ? nullptr : &*match;
}

inline render_text_external_font_backend_probe_result
make_render_text_external_font_backend_empty_probe_result(
    const render_text_font_backend_selection_purpose purpose)
{
    return render_text_external_font_backend_probe_result{
        .purpose = purpose,
    };
}

inline render_text_external_font_backend_probe_diff_summary_snapshot
diff_render_text_external_font_backend_probe_results(
    const std::vector<render_text_external_font_backend_probe_result>& before_results,
    const std::vector<render_text_external_font_backend_probe_result>& after_results)
{
    render_text_external_font_backend_probe_diff_summary_snapshot summary;
    const std::vector<render_text_font_backend_selection_purpose> purposes = {
        render_text_font_backend_selection_purpose::shaping,
        render_text_font_backend_selection_purpose::rasterization,
        render_text_font_backend_selection_purpose::unicode_processing,
    };

    summary.diffs.reserve(purposes.size());
    for (const render_text_font_backend_selection_purpose purpose : purposes) {
        const render_text_external_font_backend_probe_result* before =
            find_render_text_external_font_backend_probe_result(before_results, purpose);
        const render_text_external_font_backend_probe_result* after =
            find_render_text_external_font_backend_probe_result(after_results, purpose);
        const render_text_external_font_backend_probe_result empty_before =
            make_render_text_external_font_backend_empty_probe_result(purpose);
        const render_text_external_font_backend_probe_result empty_after =
            make_render_text_external_font_backend_empty_probe_result(purpose);
        summary.diffs.push_back(diff_render_text_external_font_backend_probe_result(
            before == nullptr ? empty_before : *before,
            after == nullptr ? empty_after : *after));
    }

    for (const render_text_external_font_backend_probe_diff_snapshot& diff : summary.diffs) {
        if (diff.has_changes) {
            ++summary.changed_count;
        }
        if (diff.became_adapter_ready()) {
            ++summary.adapter_ready_transition_count;
        }
        if (diff.became_fake_only()) {
            ++summary.fake_only_transition_count;
        }
        if (diff.became_unavailable()) {
            ++summary.unavailable_transition_count;
        }
        if (diff.became_mismatch()) {
            ++summary.mismatch_transition_count;
        }
        if (diff.selected_backend_changed) {
            ++summary.selected_backend_change_count;
        }
        summary.total_missing_dependency_delta += diff.missing_dependency_delta;
        summary.total_adapter_unavailable_delta += diff.adapter_unavailable_delta;
        summary.total_version_mismatch_delta += diff.version_mismatch_delta;
        summary.total_missing_feature_delta += diff.missing_feature_delta;
    }
    summary.summary = "external font backend probe diff: "
        + std::to_string(summary.changed_count)
        + " changed of "
        + std::to_string(summary.diffs.size())
        + " purposes";
    return summary;
}

class manifest_font_backend_dependency_probe final : public font_backend_dependency_probe_interface {
public:
    manifest_font_backend_dependency_probe() = default;

    explicit manifest_font_backend_dependency_probe(render_text_external_font_backend_manifest manifest)
        : manifest_(std::move(manifest))
    {
    }

    render_text_external_font_backend_probe_result probe(
        const render_text_external_font_backend_probe_request& request) const override
    {
        std::vector<render_text_font_backend_feature> required_features =
            request.required_features.empty()
                ? render_text_font_backend_required_features_for(request.purpose)
                : request.required_features;
        std::vector<render_text_font_backend_library> required_libraries =
            request.required_libraries.empty()
                ? render_text_external_font_backend_default_libraries_for(request.purpose)
                : request.required_libraries;

        render_text_font_backend_capability_probe_request capability_request{
            .required_libraries = required_libraries,
            .required_features = required_features,
            .minimum_versions = request.minimum_versions,
            .allow_fallback_only = false,
        };
        const render_text_font_backend_capability_snapshot capability =
            make_render_text_font_backend_capability_snapshot(
                render_text_external_font_backend_components(manifest_.dependencies),
                capability_request);

        std::vector<render_text_font_backend_candidate> candidates =
            render_text_external_font_backend_candidates(manifest_.dependencies);
        if (manifest_.allow_deterministic_fallback
            && std::none_of(
                candidates.begin(),
                candidates.end(),
                [](const render_text_font_backend_candidate& candidate) {
                    return candidate.library == render_text_font_backend_library::deterministic_fake;
                })) {
            candidates.push_back(make_render_text_deterministic_fake_backend_candidate());
        }

        std::vector<render_text_font_backend_candidate> selection_candidates = std::move(candidates);
        render_text_font_backend_selection_result selection = select_render_text_font_backend(
            render_text_font_backend_selection_request{
                .purpose = request.purpose,
                .candidates = selection_candidates,
                .required_features = required_features,
                .allow_deterministic_fallback =
                    request.allow_deterministic_fallback && manifest_.allow_deterministic_fallback,
            });
        if (!capability.ok()
            && request.allow_deterministic_fallback
            && manifest_.allow_deterministic_fallback
            && selection.status == render_text_font_backend_selection_status::selected) {
            render_text_font_backend_candidate fallback = make_render_text_deterministic_fake_backend_candidate();
            for (const render_text_font_backend_candidate& candidate : selection_candidates) {
                if (candidate.library == render_text_font_backend_library::deterministic_fake) {
                    fallback = candidate;
                    break;
                }
            }
            selection = make_render_text_font_backend_selection_result(
                render_text_font_backend_selection_status::fallback_selected,
                request.purpose,
                std::move(fallback),
                required_features,
                {"real backend dependency probe was not adapter-ready; deterministic fallback selected"});
        }

        std::vector<render_text_font_backend_library> missing_dependencies =
            missing_render_text_external_font_backend_dependencies(
                manifest_.dependencies,
                required_libraries);
        std::vector<render_text_font_backend_library> adapter_unavailable_dependencies =
            unavailable_render_text_external_font_backend_adapters(
                manifest_.dependencies,
                required_libraries);
        const render_text_font_backend_adapter_readiness_status fallback_reason =
            render_text_external_font_backend_failure_status_for(
                capability,
                missing_dependencies,
                adapter_unavailable_dependencies);
        const render_text_font_backend_adapter_readiness_status status =
            render_text_external_font_backend_readiness_status_for(
                selection,
                capability,
                missing_dependencies,
                adapter_unavailable_dependencies);

        return render_text_external_font_backend_probe_result{
            .status = status,
            .fallback_reason = fallback_reason,
            .purpose = request.purpose,
            .manifest_label = manifest_.label,
            .requested_capability = capability,
            .selection = selection,
            .adapter_functions = selection.adapter_functions,
            .dependencies = manifest_.dependencies,
            .missing_dependencies = std::move(missing_dependencies),
            .adapter_unavailable_dependencies = std::move(adapter_unavailable_dependencies),
            .adapter_ready = status == render_text_font_backend_adapter_readiness_status::adapter_ready,
            .fallback_ready = status == render_text_font_backend_adapter_readiness_status::fallback_ready,
            .can_attempt_real_backend =
                status == render_text_font_backend_adapter_readiness_status::adapter_ready
                && selection.selected_real_backend(),
            .used_deterministic_fallback = selection.used_deterministic_fallback,
            .diagnostic = render_text_external_font_backend_probe_diagnostic_for(status, fallback_reason),
        };
    }

private:
    render_text_external_font_backend_manifest manifest_ =
        make_render_text_known_external_font_backend_manifest();
};

} // namespace quiz_vulkan::render
