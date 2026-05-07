#pragma once

#include "render/text/font_shaping_backend.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_backend_library {
    deterministic_fake,
    freetype,
    harfbuzz,
    directwrite,
};

inline std::string render_text_font_backend_library_name(
    const render_text_font_backend_library library)
{
    switch (library) {
    case render_text_font_backend_library::deterministic_fake:
        return "deterministic_fake";
    case render_text_font_backend_library::freetype:
        return "freetype";
    case render_text_font_backend_library::harfbuzz:
        return "harfbuzz";
    case render_text_font_backend_library::directwrite:
        return "directwrite";
    }

    return "unknown";
}

enum class render_text_font_backend_feature {
    font_file_loading,
    unicode_cmap,
    glyph_id_mapping,
    glyph_rasterization,
    glyph_shaping,
    complex_script_shaping,
    color_glyphs,
    variable_fonts,
};

inline std::string render_text_font_backend_feature_name(
    const render_text_font_backend_feature feature)
{
    switch (feature) {
    case render_text_font_backend_feature::font_file_loading:
        return "font_file_loading";
    case render_text_font_backend_feature::unicode_cmap:
        return "unicode_cmap";
    case render_text_font_backend_feature::glyph_id_mapping:
        return "glyph_id_mapping";
    case render_text_font_backend_feature::glyph_rasterization:
        return "glyph_rasterization";
    case render_text_font_backend_feature::glyph_shaping:
        return "glyph_shaping";
    case render_text_font_backend_feature::complex_script_shaping:
        return "complex_script_shaping";
    case render_text_font_backend_feature::color_glyphs:
        return "color_glyphs";
    case render_text_font_backend_feature::variable_fonts:
        return "variable_fonts";
    }

    return "unknown";
}

enum class render_text_font_backend_capability_status {
    available,
    unavailable,
    version_mismatch,
    unsupported_feature,
    fallback_only,
};

inline std::string render_text_font_backend_capability_status_name(
    const render_text_font_backend_capability_status status)
{
    switch (status) {
    case render_text_font_backend_capability_status::available:
        return "available";
    case render_text_font_backend_capability_status::unavailable:
        return "unavailable";
    case render_text_font_backend_capability_status::version_mismatch:
        return "version_mismatch";
    case render_text_font_backend_capability_status::unsupported_feature:
        return "unsupported_feature";
    case render_text_font_backend_capability_status::fallback_only:
        return "fallback_only";
    }

    return "unknown";
}

struct render_text_font_backend_version {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;
    std::string label;

    std::string display_label() const
    {
        if (!label.empty()) {
            return label;
        }
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};

inline int compare_render_text_font_backend_versions(
    const render_text_font_backend_version& lhs,
    const render_text_font_backend_version& rhs)
{
    if (lhs.major != rhs.major) {
        return lhs.major < rhs.major ? -1 : 1;
    }
    if (lhs.minor != rhs.minor) {
        return lhs.minor < rhs.minor ? -1 : 1;
    }
    if (lhs.patch != rhs.patch) {
        return lhs.patch < rhs.patch ? -1 : 1;
    }
    return 0;
}

inline bool render_text_font_backend_version_satisfies(
    const render_text_font_backend_version& actual,
    const render_text_font_backend_version& minimum)
{
    return compare_render_text_font_backend_versions(actual, minimum) >= 0;
}

struct render_text_font_backend_component {
    render_text_font_backend_library library =
        render_text_font_backend_library::deterministic_fake;
    std::string name;
    bool available = false;
    render_text_font_backend_version version;
    std::vector<render_text_font_backend_feature> features;
    bool fallback_only = false;
    std::string diagnostic;

    bool supports_feature(const render_text_font_backend_feature feature) const
    {
        return std::find(features.begin(), features.end(), feature) != features.end();
    }
};

struct render_text_font_backend_minimum_version {
    render_text_font_backend_library library =
        render_text_font_backend_library::deterministic_fake;
    render_text_font_backend_version version;
};

struct render_text_font_backend_version_mismatch {
    render_text_font_backend_library library =
        render_text_font_backend_library::deterministic_fake;
    render_text_font_backend_version required;
    render_text_font_backend_version actual;
};

struct render_text_font_backend_capability_probe_request {
    std::vector<render_text_font_backend_library> required_libraries;
    std::vector<render_text_font_backend_feature> required_features;
    std::vector<render_text_font_backend_minimum_version> minimum_versions;
    bool allow_fallback_only = true;
};

struct render_text_font_backend_capability_snapshot {
    render_text_font_backend_capability_status status =
        render_text_font_backend_capability_status::unavailable;
    std::vector<render_text_font_backend_component> components;
    std::vector<render_text_font_backend_library> missing_libraries;
    std::vector<render_text_font_backend_feature> missing_features;
    std::vector<render_text_font_backend_version_mismatch> version_mismatches;
    bool fallback_only = false;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_backend_capability_status::available;
    }

    bool can_shape_with_fallback() const
    {
        return ok() || status == render_text_font_backend_capability_status::fallback_only;
    }

    bool supports_feature(const render_text_font_backend_feature feature) const
    {
        return std::any_of(
            components.begin(),
            components.end(),
            [&](const render_text_font_backend_component& component) {
                return component.available && component.supports_feature(feature);
            });
    }
};

class font_backend_capability_probe_interface {
public:
    virtual ~font_backend_capability_probe_interface() = default;

    virtual render_text_font_backend_capability_snapshot probe(
        const render_text_font_backend_capability_probe_request& request) const = 0;
};

inline const render_text_font_backend_component* find_render_text_font_backend_component(
    const std::vector<render_text_font_backend_component>& components,
    const render_text_font_backend_library library)
{
    const auto match = std::find_if(
        components.begin(),
        components.end(),
        [&](const render_text_font_backend_component& component) {
            return component.library == library;
        });
    return match == components.end() ? nullptr : &*match;
}

inline bool render_text_font_backend_feature_is_supported(
    const std::vector<render_text_font_backend_component>& components,
    const render_text_font_backend_feature feature)
{
    return std::any_of(
        components.begin(),
        components.end(),
        [&](const render_text_font_backend_component& component) {
            return component.available && component.supports_feature(feature);
        });
}

inline std::vector<render_text_font_backend_library> missing_render_text_font_backend_libraries(
    const std::vector<render_text_font_backend_component>& components,
    const std::vector<render_text_font_backend_library>& required_libraries)
{
    std::vector<render_text_font_backend_library> missing;
    for (const render_text_font_backend_library library : required_libraries) {
        const render_text_font_backend_component* component =
            find_render_text_font_backend_component(components, library);
        if (component == nullptr || !component->available) {
            missing.push_back(library);
        }
    }
    return missing;
}

inline std::vector<render_text_font_backend_feature> missing_render_text_font_backend_features(
    const std::vector<render_text_font_backend_component>& components,
    const std::vector<render_text_font_backend_feature>& required_features)
{
    std::vector<render_text_font_backend_feature> missing;
    for (const render_text_font_backend_feature feature : required_features) {
        if (!render_text_font_backend_feature_is_supported(components, feature)) {
            missing.push_back(feature);
        }
    }
    return missing;
}

inline std::vector<render_text_font_backend_version_mismatch> mismatched_render_text_font_backend_versions(
    const std::vector<render_text_font_backend_component>& components,
    const std::vector<render_text_font_backend_minimum_version>& minimum_versions)
{
    std::vector<render_text_font_backend_version_mismatch> mismatches;
    for (const render_text_font_backend_minimum_version& minimum : minimum_versions) {
        const render_text_font_backend_component* component =
            find_render_text_font_backend_component(components, minimum.library);
        if (component == nullptr || !component->available) {
            continue;
        }
        if (!render_text_font_backend_version_satisfies(component->version, minimum.version)) {
            mismatches.push_back(render_text_font_backend_version_mismatch{
                .library = minimum.library,
                .required = minimum.version,
                .actual = component->version,
            });
        }
    }
    return mismatches;
}

inline bool render_text_font_backend_components_are_fallback_only(
    const std::vector<render_text_font_backend_component>& components)
{
    bool has_available_component = false;
    for (const render_text_font_backend_component& component : components) {
        if (!component.available) {
            continue;
        }
        has_available_component = true;
        if (!component.fallback_only) {
            return false;
        }
    }
    return has_available_component;
}

inline bool render_text_font_backend_has_available_component(
    const std::vector<render_text_font_backend_component>& components)
{
    return std::any_of(
        components.begin(),
        components.end(),
        [](const render_text_font_backend_component& component) {
            return component.available;
        });
}

inline render_text_font_backend_capability_snapshot make_render_text_font_backend_capability_snapshot(
    std::vector<render_text_font_backend_component> components,
    const render_text_font_backend_capability_probe_request& request)
{
    render_text_font_backend_capability_snapshot snapshot{
        .components = std::move(components),
    };
    snapshot.missing_libraries =
        missing_render_text_font_backend_libraries(snapshot.components, request.required_libraries);
    snapshot.version_mismatches =
        mismatched_render_text_font_backend_versions(snapshot.components, request.minimum_versions);
    snapshot.missing_features =
        missing_render_text_font_backend_features(snapshot.components, request.required_features);
    snapshot.fallback_only =
        render_text_font_backend_components_are_fallback_only(snapshot.components);

    if (!render_text_font_backend_has_available_component(snapshot.components)
        || !snapshot.missing_libraries.empty()) {
        snapshot.status = render_text_font_backend_capability_status::unavailable;
        snapshot.diagnostic = "required font backend library is unavailable";
        return snapshot;
    }
    if (!snapshot.version_mismatches.empty()) {
        snapshot.status = render_text_font_backend_capability_status::version_mismatch;
        snapshot.diagnostic = "font backend library version is below the requested minimum";
        return snapshot;
    }
    if (!snapshot.missing_features.empty()) {
        snapshot.status = render_text_font_backend_capability_status::unsupported_feature;
        snapshot.diagnostic = "font backend does not support a required text feature";
        return snapshot;
    }
    if (snapshot.fallback_only) {
        snapshot.status = render_text_font_backend_capability_status::fallback_only;
        snapshot.diagnostic = request.allow_fallback_only
            ? "font backend can only provide deterministic fallback behavior"
            : "font backend fallback-only mode is not allowed by this request";
        return snapshot;
    }

    snapshot.status = render_text_font_backend_capability_status::available;
    snapshot.diagnostic = "font backend capability probe satisfied all requirements";
    return snapshot;
}

class deterministic_fake_font_backend_capability_probe final : public font_backend_capability_probe_interface {
public:
    deterministic_fake_font_backend_capability_probe()
        : components_{
              render_text_font_backend_component{
                  .library = render_text_font_backend_library::deterministic_fake,
                  .name = "Deterministic fake text backend",
                  .available = true,
                  .version = render_text_font_backend_version{.major = 1, .minor = 0, .patch = 0},
                  .features = {
                      render_text_font_backend_feature::font_file_loading,
                      render_text_font_backend_feature::unicode_cmap,
                      render_text_font_backend_feature::glyph_id_mapping,
                      render_text_font_backend_feature::glyph_rasterization,
                      render_text_font_backend_feature::glyph_shaping,
                  },
                  .fallback_only = true,
                  .diagnostic = "deterministic fake backend is a text-owned fallback implementation",
              },
          }
    {
    }

    explicit deterministic_fake_font_backend_capability_probe(
        std::vector<render_text_font_backend_component> components)
        : components_(std::move(components))
    {
    }

    render_text_font_backend_capability_snapshot probe(
        const render_text_font_backend_capability_probe_request& request) const override
    {
        return make_render_text_font_backend_capability_snapshot(components_, request);
    }

private:
    std::vector<render_text_font_backend_component> components_;
};

struct render_text_font_backend_shaping_capability {
    bool backend_available = false;
    bool support_complex_scripts = false;
    bool fallback_only = false;
    std::string diagnostic;
};

inline render_text_font_backend_shaping_capability render_text_font_backend_capability_to_shaping(
    const render_text_font_backend_capability_snapshot& capability)
{
    return render_text_font_backend_shaping_capability{
        .backend_available = capability.ok() || capability.can_shape_with_fallback(),
        .support_complex_scripts =
            capability.ok()
            && capability.supports_feature(render_text_font_backend_feature::complex_script_shaping),
        .fallback_only = capability.status == render_text_font_backend_capability_status::fallback_only,
        .diagnostic = capability.diagnostic,
    };
}

inline render_text_font_shaping_request apply_render_text_font_backend_capability(
    render_text_font_shaping_request request,
    const render_text_font_backend_capability_snapshot& capability)
{
    const render_text_font_backend_shaping_capability shaping =
        render_text_font_backend_capability_to_shaping(capability);
    request.backend_available = shaping.backend_available;
    request.support_complex_scripts = shaping.support_complex_scripts;
    return request;
}

} // namespace quiz_vulkan::render
