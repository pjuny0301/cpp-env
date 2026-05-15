#pragma once

#include "render/image/third_party_image_decoder_contracts.inl"

#if defined(QUIZ_VULKAN_HAS_STB_HEADERS) && QUIZ_VULKAN_HAS_STB_HEADERS
#include <stb_image.h>
#endif

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

inline constexpr bool stb_image_decoder_headers_available()
{
#if defined(QUIZ_VULKAN_HAS_STB_HEADERS) && QUIZ_VULKAN_HAS_STB_HEADERS
    return true;
#else
    return false;
#endif
}

inline std::string stb_image_decoder_header_version()
{
#if defined(QUIZ_VULKAN_HAS_STB_HEADERS) && QUIZ_VULKAN_HAS_STB_HEADERS && defined(STBI_VERSION)
    return std::to_string(STBI_VERSION);
#else
    return {};
#endif
}

enum class stb_image_decoder_dependency_status {
    missing,
    available,
    mismatched_capability,
};

inline std::string stb_image_decoder_dependency_status_name(
    stb_image_decoder_dependency_status status)
{
    switch (status) {
    case stb_image_decoder_dependency_status::missing:
        return "missing";
    case stb_image_decoder_dependency_status::available:
        return "available";
    case stb_image_decoder_dependency_status::mismatched_capability:
        return "mismatched_capability";
    }

    return "unknown";
}

enum class stb_image_decoder_adapter_selection_status {
    ready,
    fallback_missing_dependency,
    fallback_mismatched_capability,
    fallback_internal_decoder_preferred,
    fallback_unsupported_format,
};

inline std::string stb_image_decoder_adapter_selection_status_name(
    stb_image_decoder_adapter_selection_status status)
{
    switch (status) {
    case stb_image_decoder_adapter_selection_status::ready:
        return "ready";
    case stb_image_decoder_adapter_selection_status::fallback_missing_dependency:
        return "fallback_missing_dependency";
    case stb_image_decoder_adapter_selection_status::fallback_mismatched_capability:
        return "fallback_mismatched_capability";
    case stb_image_decoder_adapter_selection_status::fallback_internal_decoder_preferred:
        return "fallback_internal_decoder_preferred";
    case stb_image_decoder_adapter_selection_status::fallback_unsupported_format:
        return "fallback_unsupported_format";
    }

    return "unknown";
}

struct stb_image_decoder_format_matrix_entry {
    render_image_encoded_format format = render_image_encoded_format::unknown;
    std::string format_name;
    std::string mime_type;
    bool dependency_supports = false;
    bool internal_decoder_available = false;
    bool prefer_internal_decoder = false;
    bool external_decode_enabled = false;
    bool declared_by_header = false;
    bool probed_by_header = false;
    std::string diagnostic;

    bool route_to_external() const
    {
        return dependency_supports && external_decode_enabled && !prefer_internal_decoder;
    }
};

inline stb_image_decoder_format_matrix_entry make_stb_image_decoder_format_matrix_entry(
    render_image_encoded_format format,
    bool dependency_supports,
    bool internal_decoder_available,
    bool prefer_internal_decoder,
    bool declared_by_header = false,
    bool probed_by_header = false)
{
    stb_image_decoder_format_matrix_entry entry{
        .format = format,
        .format_name = render_image_encoded_format_name(format),
        .mime_type = render_image_encoded_format_mime_type(format),
        .dependency_supports = dependency_supports,
        .internal_decoder_available = internal_decoder_available,
        .prefer_internal_decoder = prefer_internal_decoder,
        .external_decode_enabled = dependency_supports && !prefer_internal_decoder,
        .declared_by_header = declared_by_header,
        .probed_by_header = probed_by_header,
    };
    if (!entry.dependency_supports) {
        entry.diagnostic = "stb_image matrix entry is not supported by dependency";
    } else if (entry.prefer_internal_decoder) {
        entry.diagnostic = "stb_image matrix entry preserves existing internal decoder";
    } else {
        entry.diagnostic = "stb_image matrix entry routes to external decoder";
    }
    return entry;
}

inline std::vector<stb_image_decoder_format_matrix_entry> make_default_stb_image_decoder_format_matrix()
{
    return {
        make_stb_image_decoder_format_matrix_entry(
            render_image_encoded_format::jpeg,
            true,
            false,
            false),
        make_stb_image_decoder_format_matrix_entry(
            render_image_encoded_format::png,
            true,
            true,
            false),
        make_stb_image_decoder_format_matrix_entry(
            render_image_encoded_format::bmp,
            true,
            true,
            false),
        make_stb_image_decoder_format_matrix_entry(
            render_image_encoded_format::ppm,
            true,
            true,
            false),
    };
}

inline std::vector<stb_image_decoder_format_matrix_entry> make_stb_image_decoder_header_format_matrix()
{
    std::vector<stb_image_decoder_format_matrix_entry> matrix =
        make_default_stb_image_decoder_format_matrix();
    for (stb_image_decoder_format_matrix_entry& entry : matrix) {
        entry.declared_by_header = stb_image_decoder_headers_available();
        entry.probed_by_header = stb_image_decoder_headers_available();
    }
    return matrix;
}

inline const stb_image_decoder_format_matrix_entry* stb_image_decoder_format_matrix_entry_for(
    const std::vector<stb_image_decoder_format_matrix_entry>& matrix,
    render_image_encoded_format format)
{
    for (const stb_image_decoder_format_matrix_entry& entry : matrix) {
        if (entry.format == format) {
            return &entry;
        }
    }
    return nullptr;
}

inline bool stb_image_decoder_format_supported(
    render_image_encoded_format format,
    const std::vector<stb_image_decoder_format_matrix_entry>& matrix =
        make_default_stb_image_decoder_format_matrix())
{
    const stb_image_decoder_format_matrix_entry* entry =
        stb_image_decoder_format_matrix_entry_for(matrix, format);
    return entry != nullptr && entry->dependency_supports;
}

inline std::size_t stb_image_decoder_supported_format_count(
    const std::vector<stb_image_decoder_format_matrix_entry>& matrix)
{
    return static_cast<std::size_t>(std::ranges::count_if(
        matrix,
        [](const stb_image_decoder_format_matrix_entry& entry) {
            return entry.dependency_supports;
        }));
}

inline std::size_t stb_image_decoder_header_format_count(
    const std::vector<stb_image_decoder_format_matrix_entry>& matrix,
    bool stb_image_decoder_format_matrix_entry::* evidence_field)
{
    return static_cast<std::size_t>(std::ranges::count_if(
        matrix,
        [evidence_field](const stb_image_decoder_format_matrix_entry& entry) {
            return entry.dependency_supports && entry.*evidence_field;
        }));
}

inline std::string make_stb_image_decoder_supported_format_summary(
    const std::vector<stb_image_decoder_format_matrix_entry>& matrix)
{
    std::string summary;
    for (const stb_image_decoder_format_matrix_entry& entry : matrix) {
        if (!entry.dependency_supports) {
            continue;
        }
        if (!summary.empty()) {
            summary += ",";
        }
        summary += entry.format_name.empty()
            ? render_image_encoded_format_name(entry.format)
            : entry.format_name;
    }
    return summary.empty() ? "none" : summary;
}

struct stb_image_decoder_dependency_manifest {
    stb_image_decoder_dependency_status status = stb_image_decoder_dependency_status::missing;
    std::string status_name = stb_image_decoder_dependency_status_name(
        stb_image_decoder_dependency_status::missing);
    std::string decoder_id = "stb_image_decoder";
    std::string dependency_name = "stb_image";
    std::string dependency_version;
    bool header_available = false;
    bool implementation_linked = false;
    bool header_probe_used = false;
    std::size_t declared_supported_format_count = 0;
    std::size_t probed_supported_format_count = 0;
    std::string supported_format_summary;
    std::string fallback_reason;
    bool memory_decode_available = false;
    bool info_probe_available = false;
    bool forced_rgba8_decode_available = false;
    std::vector<stb_image_decoder_format_matrix_entry> supported_format_matrix;
    std::string diagnostic = "stb_image dependency is not available";

    bool dependency_available() const
    {
        return status != stb_image_decoder_dependency_status::missing;
    }

    bool capability_ready() const
    {
        return status == stb_image_decoder_dependency_status::available
            && memory_decode_available
            && info_probe_available
            && forced_rgba8_decode_available
            && (!header_probe_used || (header_available && implementation_linked));
    }

    bool ok() const
    {
        return capability_ready();
    }
};

class stb_image_decoder_dependency_probe_interface {
public:
    virtual ~stb_image_decoder_dependency_probe_interface() = default;

    virtual stb_image_decoder_dependency_manifest probe_dependency() const = 0;
};

inline stb_image_decoder_dependency_manifest make_missing_stb_image_decoder_dependency_manifest(
    std::string decoder_id = "stb_image_decoder")
{
    return stb_image_decoder_dependency_manifest{
        .status = stb_image_decoder_dependency_status::missing,
        .status_name = stb_image_decoder_dependency_status_name(stb_image_decoder_dependency_status::missing),
        .decoder_id = std::move(decoder_id),
        .dependency_name = "stb_image",
        .dependency_version = {},
        .header_available = false,
        .implementation_linked = false,
        .header_probe_used = false,
        .declared_supported_format_count = 0,
        .probed_supported_format_count = 0,
        .supported_format_summary = "none",
        .fallback_reason = "stb_image dependency is not available",
        .memory_decode_available = false,
        .info_probe_available = false,
        .forced_rgba8_decode_available = false,
        .supported_format_matrix = make_default_stb_image_decoder_format_matrix(),
        .diagnostic = "stb_image dependency is not available",
    };
}

inline stb_image_decoder_dependency_manifest make_available_stb_image_decoder_dependency_manifest(
    std::string decoder_id = "stb_image_decoder",
    std::string dependency_version = {},
    std::vector<stb_image_decoder_format_matrix_entry> supported_format_matrix =
        make_default_stb_image_decoder_format_matrix())
{
    const std::size_t supported_format_count =
        stb_image_decoder_supported_format_count(supported_format_matrix);
    const std::string supported_format_summary =
        make_stb_image_decoder_supported_format_summary(supported_format_matrix);
    return stb_image_decoder_dependency_manifest{
        .status = stb_image_decoder_dependency_status::available,
        .status_name = stb_image_decoder_dependency_status_name(stb_image_decoder_dependency_status::available),
        .decoder_id = std::move(decoder_id),
        .dependency_name = "stb_image",
        .dependency_version = std::move(dependency_version),
        .header_available = true,
        .implementation_linked = true,
        .header_probe_used = false,
        .declared_supported_format_count = supported_format_count,
        .probed_supported_format_count = supported_format_count,
        .supported_format_summary = supported_format_summary,
        .fallback_reason = {},
        .memory_decode_available = true,
        .info_probe_available = true,
        .forced_rgba8_decode_available = true,
        .supported_format_matrix = std::move(supported_format_matrix),
        .diagnostic = "stb_image dependency is available for injected decode backend",
    };
}

inline stb_image_decoder_dependency_manifest make_mismatched_stb_image_decoder_dependency_manifest(
    std::string decoder_id = "stb_image_decoder",
    std::string diagnostic = "stb_image dependency is missing required decode capabilities")
{
    std::vector<stb_image_decoder_format_matrix_entry> supported_format_matrix =
        make_default_stb_image_decoder_format_matrix();
    const std::size_t supported_format_count =
        stb_image_decoder_supported_format_count(supported_format_matrix);
    const std::string supported_format_summary =
        make_stb_image_decoder_supported_format_summary(supported_format_matrix);
    return stb_image_decoder_dependency_manifest{
        .status = stb_image_decoder_dependency_status::mismatched_capability,
        .status_name = stb_image_decoder_dependency_status_name(
            stb_image_decoder_dependency_status::mismatched_capability),
        .decoder_id = std::move(decoder_id),
        .dependency_name = "stb_image",
        .dependency_version = {},
        .header_available = true,
        .implementation_linked = false,
        .header_probe_used = false,
        .declared_supported_format_count = supported_format_count,
        .probed_supported_format_count = supported_format_count,
        .supported_format_summary = supported_format_summary,
        .fallback_reason = diagnostic,
        .memory_decode_available = true,
        .info_probe_available = true,
        .forced_rgba8_decode_available = false,
        .supported_format_matrix = std::move(supported_format_matrix),
        .diagnostic = std::move(diagnostic),
    };
}

inline stb_image_decoder_dependency_manifest make_stb_image_decoder_header_dependency_manifest(
    std::string decoder_id = "stb_image_decoder")
{
    if (!stb_image_decoder_headers_available()) {
        stb_image_decoder_dependency_manifest manifest =
            make_missing_stb_image_decoder_dependency_manifest(std::move(decoder_id));
        manifest.header_probe_used = true;
        manifest.fallback_reason = "stb_image header is not available";
        return manifest;
    }

    std::vector<stb_image_decoder_format_matrix_entry> supported_format_matrix =
        make_stb_image_decoder_header_format_matrix();
    const std::size_t declared_supported_format_count =
        stb_image_decoder_header_format_count(
            supported_format_matrix,
            &stb_image_decoder_format_matrix_entry::declared_by_header);
    const std::size_t probed_supported_format_count =
        stb_image_decoder_header_format_count(
            supported_format_matrix,
            &stb_image_decoder_format_matrix_entry::probed_by_header);
#if defined(QUIZ_VULKAN_HAS_STB_IMAGE_DECODE_BACKEND) && QUIZ_VULKAN_HAS_STB_IMAGE_DECODE_BACKEND
    return stb_image_decoder_dependency_manifest{
        .status = stb_image_decoder_dependency_status::available,
        .status_name = stb_image_decoder_dependency_status_name(
            stb_image_decoder_dependency_status::available),
        .decoder_id = std::move(decoder_id),
        .dependency_name = "stb_image",
        .dependency_version = stb_image_decoder_header_version(),
        .header_available = true,
        .implementation_linked = true,
        .header_probe_used = true,
        .declared_supported_format_count = declared_supported_format_count,
        .probed_supported_format_count = probed_supported_format_count,
        .supported_format_summary = make_stb_image_decoder_supported_format_summary(supported_format_matrix),
        .fallback_reason = {},
        .memory_decode_available = true,
        .info_probe_available = true,
        .forced_rgba8_decode_available = true,
        .supported_format_matrix = std::move(supported_format_matrix),
        .diagnostic = "stb_image header decode adapter is available",
    };
#else
    const std::string fallback_reason =
        "stb_image header is available but decode implementation is not linked";
    return stb_image_decoder_dependency_manifest{
        .status = stb_image_decoder_dependency_status::mismatched_capability,
        .status_name = stb_image_decoder_dependency_status_name(
            stb_image_decoder_dependency_status::mismatched_capability),
        .decoder_id = std::move(decoder_id),
        .dependency_name = "stb_image",
        .dependency_version = stb_image_decoder_header_version(),
        .header_available = true,
        .implementation_linked = false,
        .header_probe_used = true,
        .declared_supported_format_count = declared_supported_format_count,
        .probed_supported_format_count = probed_supported_format_count,
        .supported_format_summary = make_stb_image_decoder_supported_format_summary(supported_format_matrix),
        .fallback_reason = fallback_reason,
        .memory_decode_available = false,
        .info_probe_available = false,
        .forced_rgba8_decode_available = false,
        .supported_format_matrix = std::move(supported_format_matrix),
        .diagnostic = fallback_reason + "; falling back to standard image decoders",
    };
#endif
}

class stb_image_decoder_header_dependency_probe final : public stb_image_decoder_dependency_probe_interface {
public:
    explicit stb_image_decoder_header_dependency_probe(
        std::string decoder_id = "stb_image_decoder")
        : decoder_id_(std::move(decoder_id))
    {
    }

    stb_image_decoder_dependency_manifest probe_dependency() const override
    {
        return make_stb_image_decoder_header_dependency_manifest(decoder_id_);
    }

private:
    std::string decoder_id_ = "stb_image_decoder";
};

struct stb_image_decoder_adapter_selection_result {
    stb_image_decoder_adapter_selection_status status =
        stb_image_decoder_adapter_selection_status::fallback_missing_dependency;
    std::string status_name = stb_image_decoder_adapter_selection_status_name(
        stb_image_decoder_adapter_selection_status::fallback_missing_dependency);
    std::string decoder_id = "stb_image_decoder";
    render_image_format_detection_summary format_detection;
    render_image_encoded_format detected_format = render_image_encoded_format::unknown;
    std::string detected_format_name = render_image_encoded_format_name(render_image_encoded_format::unknown);
    stb_image_decoder_dependency_status dependency_status = stb_image_decoder_dependency_status::missing;
    std::string dependency_status_name = stb_image_decoder_dependency_status_name(
        stb_image_decoder_dependency_status::missing);
    bool dependency_header_available = false;
    bool dependency_implementation_linked = false;
    bool dependency_header_probe_used = false;
    std::size_t declared_supported_format_count = 0;
    std::size_t probed_supported_format_count = 0;
    std::string supported_format_summary;
    std::string fallback_reason;
    bool dependency_available = false;
    bool dependency_capability_ready = false;
    bool format_supported_by_dependency = false;
    bool internal_decoder_available = false;
    bool prefer_internal_decoder = false;
    bool external_decode_enabled = false;
    bool ready_for_external_decode = false;
    bool fallback_to_standard_decoder_chain = true;
    std::vector<stb_image_decoder_format_matrix_entry> supported_format_matrix;
    std::string diagnostic;

    bool ok() const
    {
        return ready_for_external_decode;
    }
};

inline stb_image_decoder_adapter_selection_result select_stb_image_decoder_adapter(
    const render_image_decode_request& request,
    const stb_image_decoder_dependency_manifest& dependency)
{
    const render_image_format_detection_summary format_detection = detect_render_image_format(request);
    stb_image_decoder_adapter_selection_result selection{
        .decoder_id = dependency.decoder_id.empty() ? "stb_image_decoder" : dependency.decoder_id,
        .format_detection = format_detection,
        .detected_format = format_detection.detected_format,
        .detected_format_name = render_image_encoded_format_name(format_detection.detected_format),
        .dependency_status = dependency.status,
        .dependency_status_name = dependency.status_name.empty()
            ? stb_image_decoder_dependency_status_name(dependency.status)
            : dependency.status_name,
        .dependency_header_available = dependency.header_available,
        .dependency_implementation_linked = dependency.implementation_linked,
        .dependency_header_probe_used = dependency.header_probe_used,
        .declared_supported_format_count = dependency.declared_supported_format_count,
        .probed_supported_format_count = dependency.probed_supported_format_count,
        .supported_format_summary = dependency.supported_format_summary.empty()
            ? make_stb_image_decoder_supported_format_summary(dependency.supported_format_matrix)
            : dependency.supported_format_summary,
        .fallback_reason = dependency.fallback_reason,
        .dependency_available = dependency.dependency_available(),
        .dependency_capability_ready = dependency.capability_ready(),
        .supported_format_matrix = dependency.supported_format_matrix,
    };

    if (!dependency.dependency_available()) {
        selection.status = stb_image_decoder_adapter_selection_status::fallback_missing_dependency;
        selection.diagnostic = "stb_image dependency is missing; falling back to standard image decoders";
    } else if (!dependency.capability_ready()) {
        selection.status = stb_image_decoder_adapter_selection_status::fallback_mismatched_capability;
        selection.diagnostic = dependency.diagnostic.empty()
            ? "stb_image dependency capabilities are incomplete; falling back to standard image decoders"
            : dependency.diagnostic;
    } else if (const stb_image_decoder_format_matrix_entry* matrix_entry =
                   stb_image_decoder_format_matrix_entry_for(
                       dependency.supported_format_matrix,
                       format_detection.detected_format);
               matrix_entry != nullptr && matrix_entry->dependency_supports) {
        selection.format_supported_by_dependency = matrix_entry->dependency_supports;
        selection.internal_decoder_available = matrix_entry->internal_decoder_available;
        selection.prefer_internal_decoder = matrix_entry->prefer_internal_decoder;
        selection.external_decode_enabled = matrix_entry->external_decode_enabled;
        if (matrix_entry->prefer_internal_decoder) {
            selection.status = stb_image_decoder_adapter_selection_status::fallback_internal_decoder_preferred;
            selection.diagnostic = "stb_image adapter preserves internal "
                + render_image_encoded_format_name(format_detection.detected_format)
                + " decoder";
        } else if (matrix_entry->route_to_external()) {
            selection.status = stb_image_decoder_adapter_selection_status::ready;
            selection.ready_for_external_decode = true;
            selection.fallback_to_standard_decoder_chain = false;
            selection.diagnostic = "stb_image adapter selected for external "
                + render_image_encoded_format_name(format_detection.detected_format)
                + " decode";
        } else {
            selection.status = stb_image_decoder_adapter_selection_status::fallback_unsupported_format;
            selection.diagnostic = "stb_image adapter has disabled external route for detected format";
        }
    } else {
        selection.status = stb_image_decoder_adapter_selection_status::fallback_unsupported_format;
        selection.diagnostic = "stb_image dependency does not support detected image format";
    }

    selection.status_name = stb_image_decoder_adapter_selection_status_name(selection.status);
    selection.fallback_to_standard_decoder_chain = !selection.ready_for_external_decode;
    if (selection.diagnostic.empty()) {
        selection.diagnostic = selection.status_name;
    }
    if (selection.fallback_to_standard_decoder_chain && selection.fallback_reason.empty()) {
        selection.fallback_reason = selection.diagnostic;
    }
    return selection;
}

inline stb_image_decoder_adapter_selection_result select_stb_image_decoder_adapter(
    const render_image_decode_request& request,
    const stb_image_decoder_dependency_probe_interface& probe)
{
    return select_stb_image_decoder_adapter(request, probe.probe_dependency());
}

inline render_image_external_decoder_selection_snapshot make_render_image_external_decoder_selection_snapshot(
    const stb_image_decoder_adapter_selection_result& selection)
{
    return render_image_external_decoder_selection_snapshot{
        .diagnostics_available = true,
        .decoder_id = selection.decoder_id,
        .dependency_name = "stb_image",
        .dependency_status_name = selection.dependency_status_name,
        .selection_status_name = selection.status_name,
        .detected_format = selection.detected_format,
        .detected_format_name = selection.detected_format_name,
        .dependency_header_available = selection.dependency_header_available,
        .dependency_implementation_linked = selection.dependency_implementation_linked,
        .dependency_header_probe_used = selection.dependency_header_probe_used,
        .declared_supported_format_count = selection.declared_supported_format_count,
        .probed_supported_format_count = selection.probed_supported_format_count,
        .supported_format_summary = selection.supported_format_summary,
        .fallback_reason = selection.fallback_reason,
        .dependency_available = selection.dependency_available,
        .dependency_capability_ready = selection.dependency_capability_ready,
        .format_supported_by_dependency = selection.format_supported_by_dependency,
        .internal_decoder_available = selection.internal_decoder_available,
        .prefer_internal_decoder = selection.prefer_internal_decoder,
        .ready_for_external_decode = selection.ready_for_external_decode,
        .fallback_to_standard_decoder_chain = selection.fallback_to_standard_decoder_chain,
        .used_internal_decoder =
            selection.status == stb_image_decoder_adapter_selection_status::fallback_internal_decoder_preferred,
        .used_third_party_adapter = selection.ready_for_external_decode,
        .fallback_due_to_missing_dependency =
            selection.status == stb_image_decoder_adapter_selection_status::fallback_missing_dependency,
        .fallback_due_to_mismatched_capability =
            selection.status == stb_image_decoder_adapter_selection_status::fallback_mismatched_capability,
        .diagnostic = selection.diagnostic,
    };
}

inline third_party_image_decoder_capability make_third_party_image_decoder_capability_from_stb_selection(
    const render_image_decode_request& request,
    const stb_image_decoder_adapter_selection_result& selection)
{
    if (selection.ready_for_external_decode) {
        return make_supported_third_party_image_decoder_capability(
            request,
            selection.decoder_id,
            selection.diagnostic);
    }
    if (!selection.dependency_available) {
        third_party_image_decoder_capability capability = make_unavailable_third_party_image_decoder_capability(
            request,
            selection.decoder_id);
        capability.diagnostic = selection.diagnostic.empty()
            ? capability.diagnostic
            : selection.diagnostic;
        return capability;
    }
    return make_unsupported_third_party_image_decoder_capability(
        request,
        selection.decoder_id,
        selection.diagnostic);
}

class fake_stb_image_decoder_dependency_probe final : public stb_image_decoder_dependency_probe_interface {
public:
    fake_stb_image_decoder_dependency_probe()
        : manifest_(make_missing_stb_image_decoder_dependency_manifest())
    {
    }

    void set_manifest(stb_image_decoder_dependency_manifest manifest)
    {
        manifest_ = std::move(manifest);
    }

    void set_missing(std::string decoder_id = "fake_stb_image_decoder")
    {
        manifest_ = make_missing_stb_image_decoder_dependency_manifest(std::move(decoder_id));
    }

    void set_available(
        std::string decoder_id = "fake_stb_image_decoder",
        std::vector<stb_image_decoder_format_matrix_entry> supported_format_matrix =
            make_default_stb_image_decoder_format_matrix())
    {
        manifest_ = make_available_stb_image_decoder_dependency_manifest(
            std::move(decoder_id),
            "fake-stb-image",
            std::move(supported_format_matrix));
    }

    void set_mismatched(std::string decoder_id = "fake_stb_image_decoder")
    {
        manifest_ = make_mismatched_stb_image_decoder_dependency_manifest(std::move(decoder_id));
    }

    stb_image_decoder_dependency_manifest probe_dependency() const override
    {
        ++probe_count;
        return manifest_;
    }

    mutable std::size_t probe_count = 0;

private:
    stb_image_decoder_dependency_manifest manifest_;
};

} // namespace quiz_vulkan::render
