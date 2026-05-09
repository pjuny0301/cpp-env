#pragma once

#include "render/text/font_backend_selection.h"

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
    if (!capability.missing_features.empty()) {
        return render_text_font_backend_adapter_readiness_status::unsupported_feature;
    }
    if (!adapter_unavailable_dependencies.empty()) {
        return render_text_font_backend_adapter_readiness_status::adapter_unavailable;
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
