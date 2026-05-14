#pragma once

#include "render/image/image_decoder.h"

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

enum class third_party_image_decoder_adapter_status {
    unavailable,
    unsupported_format,
    supported,
};

inline std::string third_party_image_decoder_adapter_status_name(
    third_party_image_decoder_adapter_status status)
{
    switch (status) {
    case third_party_image_decoder_adapter_status::unavailable:
        return "unavailable";
    case third_party_image_decoder_adapter_status::unsupported_format:
        return "unsupported_format";
    case third_party_image_decoder_adapter_status::supported:
        return "supported";
    }

    return "unknown";
}

struct third_party_image_decoder_capability {
    third_party_image_decoder_adapter_status status =
        third_party_image_decoder_adapter_status::unavailable;
    std::string decoder_id = "third_party_image_decoder_adapter";
    render_image_format_detection_summary format_detection;
    std::string diagnostic;

    bool available() const
    {
        return status != third_party_image_decoder_adapter_status::unavailable;
    }

    bool supports_decode() const
    {
        return status == third_party_image_decoder_adapter_status::supported;
    }
};

class third_party_image_decoder_backend_interface {
public:
    virtual ~third_party_image_decoder_backend_interface() = default;

    virtual third_party_image_decoder_capability inspect(
        const render_image_decode_request& request) const = 0;
    virtual render_image_decode_result decode(
        const render_image_decode_request& request) const = 0;
};

inline third_party_image_decoder_capability make_unavailable_third_party_image_decoder_capability(
    const render_image_decode_request& request,
    std::string decoder_id = "third_party_image_decoder_adapter",
    std::string diagnostic = "third-party image decoder backend is unavailable")
{
    return third_party_image_decoder_capability{
        .status = third_party_image_decoder_adapter_status::unavailable,
        .decoder_id = std::move(decoder_id),
        .format_detection = detect_render_image_format(request),
        .diagnostic = std::move(diagnostic),
    };
}

inline third_party_image_decoder_capability make_unsupported_third_party_image_decoder_capability(
    const render_image_decode_request& request,
    std::string decoder_id,
    std::string diagnostic = "third-party image decoder backend does not support this source")
{
    return third_party_image_decoder_capability{
        .status = third_party_image_decoder_adapter_status::unsupported_format,
        .decoder_id = std::move(decoder_id),
        .format_detection = detect_render_image_format(request),
        .diagnostic = std::move(diagnostic),
    };
}

inline third_party_image_decoder_capability make_supported_third_party_image_decoder_capability(
    const render_image_decode_request& request,
    std::string decoder_id,
    std::string diagnostic = "third-party image decoder backend supports this source")
{
    return third_party_image_decoder_capability{
        .status = third_party_image_decoder_adapter_status::supported,
        .decoder_id = std::move(decoder_id),
        .format_detection = detect_render_image_format(request),
        .diagnostic = std::move(diagnostic),
    };
}

inline render_image_decoder_diagnostic make_third_party_image_decoder_adapter_diagnostic(
    const render_image_decode_request& request,
    const third_party_image_decoder_capability& capability,
    std::size_t candidate_index)
{
    render_image_decoder_diagnostic diagnostic =
        make_render_image_decoder_candidate_diagnostic(request, capability.decoder_id, candidate_index);
    diagnostic.support_checked = true;
    diagnostic.supported = capability.supports_decode();
    diagnostic.support_diagnostic = capability.diagnostic;
    diagnostic.status = capability.supports_decode()
        ? render_image_decode_status::invalid_data
        : render_image_decode_status::unsupported_format;
    diagnostic.diagnostic = capability.diagnostic;
    return diagnostic;
}

inline void reindex_render_image_decoder_diagnostics(
    const render_image_decode_request& request,
    std::vector<render_image_decoder_diagnostic>& diagnostics,
    std::size_t candidate_index_offset)
{
    for (render_image_decoder_diagnostic& diagnostic : diagnostics) {
        diagnostic.candidate_index += candidate_index_offset;
        diagnostic.candidate_order = diagnostic.candidate_index + 1;
        diagnostic.candidate_priority =
            render_image_decoder_candidate_priority(request, diagnostic.candidate_index);
    }
}

enum class render_image_decoder_capability_candidate_kind {
    third_party_adapter,
    bmp,
    ppm,
    png,
    unsupported_terminal,
};

inline std::string render_image_decoder_capability_candidate_kind_name(
    render_image_decoder_capability_candidate_kind kind)
{
    switch (kind) {
    case render_image_decoder_capability_candidate_kind::third_party_adapter:
        return "third_party_adapter";
    case render_image_decoder_capability_candidate_kind::bmp:
        return "bmp";
    case render_image_decoder_capability_candidate_kind::ppm:
        return "ppm";
    case render_image_decoder_capability_candidate_kind::png:
        return "png";
    case render_image_decoder_capability_candidate_kind::unsupported_terminal:
        return "unsupported_terminal";
    }

    return "unknown";
}

enum class render_image_decoder_capability_candidate_status {
    unavailable,
    unsupported_format,
    supported,
    decode_failed,
    decoded,
    unsupported_terminal,
};

inline std::string render_image_decoder_capability_candidate_status_name(
    render_image_decoder_capability_candidate_status status)
{
    switch (status) {
    case render_image_decoder_capability_candidate_status::unavailable:
        return "unavailable";
    case render_image_decoder_capability_candidate_status::unsupported_format:
        return "unsupported_format";
    case render_image_decoder_capability_candidate_status::supported:
        return "supported";
    case render_image_decoder_capability_candidate_status::decode_failed:
        return "decode_failed";
    case render_image_decoder_capability_candidate_status::decoded:
        return "decoded";
    case render_image_decoder_capability_candidate_status::unsupported_terminal:
        return "unsupported_terminal";
    }

    return "unknown";
}

struct render_image_decoder_capability_candidate_snapshot {
    std::size_t candidate_index = 0;
    std::size_t candidate_order = 0;
    render_image_decoder_capability_candidate_kind kind =
        render_image_decoder_capability_candidate_kind::unsupported_terminal;
    std::string kind_name;
    std::string decoder_id;
    render_image_decoder_capability_candidate_status status =
        render_image_decoder_capability_candidate_status::unsupported_format;
    std::string status_name;
    render_image_decode_status decode_status = render_image_decode_status::unsupported_format;
    bool support_checked = false;
    bool supported = false;
    bool decode_attempted = false;
    bool terminal_candidate = false;
    std::string diagnostic;
};

struct render_image_decoder_capability_manifest {
    render_image_format_detection_summary format_detection;
    std::vector<render_image_decoder_capability_candidate_snapshot> candidates;
    render_image_external_decoder_selection_snapshot external_decoder_selection;
    bool used_third_party_adapter = false;
    bool fallback_used = false;
    bool decoded = false;
    std::string terminal_decoder_id;
    render_image_decoder_capability_candidate_kind terminal_kind =
        render_image_decoder_capability_candidate_kind::unsupported_terminal;
    std::string terminal_kind_name = render_image_decoder_capability_candidate_kind_name(
        render_image_decoder_capability_candidate_kind::unsupported_terminal);
    render_image_decoder_capability_candidate_status terminal_status =
        render_image_decoder_capability_candidate_status::unsupported_format;
    std::string terminal_status_name = render_image_decoder_capability_candidate_status_name(
        render_image_decoder_capability_candidate_status::unsupported_format);
    render_image_decode_status terminal_decode_status = render_image_decode_status::unsupported_format;
    std::string diagnostic;
};

inline render_image_decoder_capability_candidate_kind render_image_decoder_capability_kind_for(
    const render_image_decoder_diagnostic& diagnostic)
{
    if (diagnostic.decoder_id == "bmp_image_decoder") {
        return render_image_decoder_capability_candidate_kind::bmp;
    }
    if (diagnostic.decoder_id == "ppm_image_decoder") {
        return render_image_decoder_capability_candidate_kind::ppm;
    }
    if (diagnostic.decoder_id == "png_image_decoder") {
        return render_image_decoder_capability_candidate_kind::png;
    }
    return render_image_decoder_capability_candidate_kind::third_party_adapter;
}

inline render_image_decoder_capability_candidate_status render_image_decoder_capability_status_for(
    const render_image_decoder_diagnostic& diagnostic)
{
    if (!diagnostic.supported) {
        if (diagnostic.diagnostic.find("unavailable") != std::string::npos
            || diagnostic.support_diagnostic.find("unavailable") != std::string::npos) {
            return render_image_decoder_capability_candidate_status::unavailable;
        }
        return render_image_decoder_capability_candidate_status::unsupported_format;
    }
    if (!diagnostic.decode_attempted) {
        return render_image_decoder_capability_candidate_status::supported;
    }
    if (diagnostic.status == render_image_decode_status::decoded) {
        return render_image_decoder_capability_candidate_status::decoded;
    }
    return render_image_decoder_capability_candidate_status::decode_failed;
}

inline render_image_decoder_capability_candidate_snapshot make_render_image_decoder_capability_candidate_snapshot(
    const render_image_decoder_diagnostic& diagnostic)
{
    const render_image_decoder_capability_candidate_kind kind =
        render_image_decoder_capability_kind_for(diagnostic);
    const render_image_decoder_capability_candidate_status status =
        render_image_decoder_capability_status_for(diagnostic);
    return render_image_decoder_capability_candidate_snapshot{
        .candidate_index = diagnostic.candidate_index,
        .candidate_order = diagnostic.candidate_order,
        .kind = kind,
        .kind_name = render_image_decoder_capability_candidate_kind_name(kind),
        .decoder_id = diagnostic.decoder_id,
        .status = status,
        .status_name = render_image_decoder_capability_candidate_status_name(status),
        .decode_status = diagnostic.status,
        .support_checked = diagnostic.support_checked,
        .supported = diagnostic.supported,
        .decode_attempted = diagnostic.decode_attempted,
        .terminal_candidate = diagnostic.terminal_candidate,
        .diagnostic = diagnostic.diagnostic,
    };
}

inline render_image_decoder_capability_candidate_snapshot make_render_image_decoder_unsupported_terminal_snapshot(
    std::size_t candidate_index,
    std::string diagnostic)
{
    constexpr render_image_decoder_capability_candidate_kind kind =
        render_image_decoder_capability_candidate_kind::unsupported_terminal;
    constexpr render_image_decoder_capability_candidate_status status =
        render_image_decoder_capability_candidate_status::unsupported_terminal;
    return render_image_decoder_capability_candidate_snapshot{
        .candidate_index = candidate_index,
        .candidate_order = candidate_index + 1,
        .kind = kind,
        .kind_name = render_image_decoder_capability_candidate_kind_name(kind),
        .decoder_id = "unsupported_terminal",
        .status = status,
        .status_name = render_image_decoder_capability_candidate_status_name(status),
        .decode_status = render_image_decode_status::unsupported_format,
        .support_checked = true,
        .supported = false,
        .decode_attempted = false,
        .terminal_candidate = true,
        .diagnostic = std::move(diagnostic),
    };
}

inline render_image_decoder_capability_manifest make_render_image_decoder_capability_manifest(
    const render_image_decode_request& request,
    const render_image_decode_result& result)
{
    render_image_decoder_capability_manifest manifest{
        .format_detection = detect_render_image_format(request),
        .candidates = {},
        .external_decoder_selection = result.external_decoder_selection,
        .used_third_party_adapter = false,
        .fallback_used = false,
        .decoded = result.ok(),
        .terminal_decoder_id = {},
        .terminal_kind = render_image_decoder_capability_candidate_kind::unsupported_terminal,
        .terminal_kind_name = render_image_decoder_capability_candidate_kind_name(
            render_image_decoder_capability_candidate_kind::unsupported_terminal),
        .terminal_status = render_image_decoder_capability_candidate_status::unsupported_format,
        .terminal_status_name = render_image_decoder_capability_candidate_status_name(
            render_image_decoder_capability_candidate_status::unsupported_format),
        .terminal_decode_status = result.status,
        .diagnostic = result.diagnostic,
    };
    manifest.candidates.reserve(result.decoder_diagnostics.size() + 1);
    for (const render_image_decoder_diagnostic& diagnostic : result.decoder_diagnostics) {
        render_image_decoder_capability_candidate_snapshot candidate =
            make_render_image_decoder_capability_candidate_snapshot(diagnostic);
        if (candidate.kind == render_image_decoder_capability_candidate_kind::third_party_adapter) {
            manifest.used_third_party_adapter = true;
        } else if (manifest.used_third_party_adapter) {
            manifest.fallback_used = true;
        }
        if (candidate.terminal_candidate) {
            manifest.terminal_decoder_id = candidate.decoder_id;
            manifest.terminal_kind = candidate.kind;
            manifest.terminal_kind_name = candidate.kind_name;
            manifest.terminal_status = candidate.status;
            manifest.terminal_status_name = candidate.status_name;
            manifest.terminal_decode_status = candidate.decode_status;
        }
        manifest.candidates.push_back(std::move(candidate));
    }

    if (!result.ok()
        && !manifest.candidates.empty()
        && manifest.candidates.back().kind != render_image_decoder_capability_candidate_kind::unsupported_terminal
        && manifest.candidates.back().diagnostic == "decoder chain exhausted all candidates") {
        manifest.candidates.back().terminal_candidate = false;
        render_image_decoder_capability_candidate_snapshot terminal =
            make_render_image_decoder_unsupported_terminal_snapshot(
                manifest.candidates.back().candidate_index + 1,
                manifest.candidates.back().diagnostic);
        manifest.terminal_decoder_id = terminal.decoder_id;
        manifest.terminal_kind = terminal.kind;
        manifest.terminal_kind_name = terminal.kind_name;
        manifest.terminal_status = terminal.status;
        manifest.terminal_status_name = terminal.status_name;
        manifest.terminal_decode_status = terminal.decode_status;
        manifest.candidates.push_back(std::move(terminal));
    }

    if (manifest.terminal_decoder_id.empty()) {
        manifest.terminal_decoder_id = result.metadata.decoder_id.empty()
            ? "unsupported_terminal"
            : result.metadata.decoder_id;
        if (result.ok()) {
            manifest.terminal_status = render_image_decoder_capability_candidate_status::decoded;
            manifest.terminal_status_name = render_image_decoder_capability_candidate_status_name(
                manifest.terminal_status);
        }
    }
    if (manifest.diagnostic.empty() && !manifest.candidates.empty()) {
        manifest.diagnostic = manifest.candidates.back().diagnostic;
    }
    return manifest;
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
            true),
        make_stb_image_decoder_format_matrix_entry(
            render_image_encoded_format::bmp,
            true,
            true,
            true),
        make_stb_image_decoder_format_matrix_entry(
            render_image_encoded_format::ppm,
            true,
            true,
            true),
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

class stb_image_decoder_backend final : public third_party_image_decoder_backend_interface {
public:
    explicit stb_image_decoder_backend(std::string decoder_id = "stb_image_decoder");

    third_party_image_decoder_capability inspect(
        const render_image_decode_request& request) const override;
    render_image_decode_result decode(
        const render_image_decode_request& request) const override;

private:
    render_image_decode_result make_failure(
        render_image_decode_status status,
        const render_image_decode_request& request,
        std::string diagnostic) const;

    std::string decoder_id_ = "stb_image_decoder";
};

const third_party_image_decoder_backend_interface* default_stb_image_decoder_backend();

class third_party_image_decoder_adapter final : public image_decoder_interface {
public:
    third_party_image_decoder_adapter();

    explicit third_party_image_decoder_adapter(
        const third_party_image_decoder_backend_interface& backend)
        : backend_(&backend)
    {
    }

    explicit third_party_image_decoder_adapter(
        const third_party_image_decoder_backend_interface* backend)
        : backend_(backend)
    {
    }

    bool has_backend() const
    {
        return backend_ != nullptr;
    }

    third_party_image_decoder_capability inspect(
        const render_image_decode_request& request) const
    {
        if (backend_ == nullptr) {
            return make_unavailable_third_party_image_decoder_capability(request);
        }

        third_party_image_decoder_capability capability = backend_->inspect(request);
        if (capability.decoder_id.empty()) {
            capability.decoder_id = "third_party_image_decoder_adapter";
        }
        capability.format_detection = detect_render_image_format(request);
        if (capability.diagnostic.empty()) {
            capability.diagnostic = third_party_image_decoder_adapter_status_name(capability.status);
        }
        return capability;
    }

    bool supports(const render_image_decode_request& request) const override
    {
        return inspect(request).supports_decode();
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        return decode_with_capability(request, inspect(request));
    }

    render_image_decode_result decode_with_capability(
        const render_image_decode_request& request,
        const third_party_image_decoder_capability& capability) const
    {
        if (request.encoded_bytes.empty()) {
            return make_failure(
                render_image_decode_status::empty_input,
                request,
                capability.decoder_id,
                "third-party image decoder adapter requires encoded bytes");
        }
        if (!capability.available()) {
            return make_failure(
                render_image_decode_status::unsupported_format,
                request,
                capability.decoder_id,
                capability.diagnostic.empty()
                    ? "third-party image decoder backend is unavailable"
                    : capability.diagnostic);
        }
        if (!capability.supports_decode()) {
            return make_failure(
                render_image_decode_status::unsupported_format,
                request,
                capability.decoder_id,
                capability.diagnostic.empty()
                    ? "third-party image decoder backend does not support this source"
                    : capability.diagnostic);
        }
        if (backend_ == nullptr) {
            return make_failure(
                render_image_decode_status::unsupported_format,
                request,
                capability.decoder_id,
                "third-party image decoder backend is unavailable");
        }

        render_image_decode_result result = backend_->decode(request);
        const std::string decoder_id = capability.decoder_id.empty()
            ? "third_party_image_decoder_adapter"
            : capability.decoder_id;
        if (result.ok()) {
            result.metadata = make_render_image_decode_metadata(decoder_id, request, result.image);
            if (!result.metadata.size_validation.valid) {
                return make_failure(
                    render_image_decode_status::invalid_data,
                    request,
                    decoder_id,
                    "third-party image decoder adapter decoded invalid image payload: "
                        + result.metadata.size_validation.diagnostic);
            }
            return result;
        }

        if (result.metadata.decoder_id.empty()) {
            result.metadata = make_render_image_decode_metadata(decoder_id, request);
        } else {
            result.metadata.decoder_id = decoder_id;
        }
        if (result.diagnostic.empty()) {
            result.diagnostic = "third-party image decoder backend failed";
        }
        return result;
    }

private:
    static render_image_decode_result make_failure(
        render_image_decode_status status,
        const render_image_decode_request& request,
        std::string decoder_id,
        std::string diagnostic)
    {
        return render_image_decode_result{
            .status = status,
            .image = {},
            .diagnostic = std::move(diagnostic),
            .metadata = make_render_image_decode_metadata(std::move(decoder_id), request),
        };
    }

    const third_party_image_decoder_backend_interface* backend_ = nullptr;
};

class optional_third_party_image_decoder_chain final : public image_decoder_interface {
public:
    explicit optional_third_party_image_decoder_chain(
        const third_party_image_decoder_backend_interface& backend)
        : adapter_(backend)
    {
    }

    explicit optional_third_party_image_decoder_chain(
        const third_party_image_decoder_backend_interface* backend)
        : adapter_(backend)
    {
    }

    optional_third_party_image_decoder_chain(
        const third_party_image_decoder_backend_interface& backend,
        const stb_image_decoder_dependency_probe_interface& stb_probe)
        : adapter_(backend)
        , stb_probe_(&stb_probe)
    {
    }

    optional_third_party_image_decoder_chain(
        const third_party_image_decoder_backend_interface* backend,
        const stb_image_decoder_dependency_probe_interface* stb_probe)
        : adapter_(backend)
        , stb_probe_(stb_probe)
    {
    }

    bool supports(const render_image_decode_request& request) const override
    {
        return adapter_.has_backend() || fallback_.supports(request);
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        if (!adapter_.has_backend()) {
            return fallback_.decode(request);
        }

        const stb_image_decoder_adapter_selection_result stb_selection = stb_probe_ == nullptr
            ? stb_image_decoder_adapter_selection_result{}
            : select_stb_image_decoder_adapter(request, *stb_probe_);
        const render_image_external_decoder_selection_snapshot external_selection = stb_probe_ == nullptr
            ? render_image_external_decoder_selection_snapshot{}
            : make_render_image_external_decoder_selection_snapshot(stb_selection);
        const third_party_image_decoder_capability capability = stb_probe_ == nullptr
            ? adapter_.inspect(request)
            : make_third_party_image_decoder_capability_from_stb_selection(request, stb_selection);
        render_image_decoder_diagnostic adapter_diagnostic =
            make_third_party_image_decoder_adapter_diagnostic(request, capability, 0);
        if (!capability.supports_decode()) {
            render_image_decode_result fallback_result = fallback_.decode(request);
            fallback_result.external_decoder_selection = external_selection;
            reindex_render_image_decoder_diagnostics(
                request,
                fallback_result.decoder_diagnostics,
                1);
            fallback_result.decoder_diagnostics.insert(
                fallback_result.decoder_diagnostics.begin(),
                std::move(adapter_diagnostic));
            return fallback_result;
        }

        render_image_decode_result adapter_result =
            adapter_.decode_with_capability(request, capability);
        adapter_result.external_decoder_selection = external_selection;
        adapter_diagnostic.decode_attempted = true;
        adapter_diagnostic.status = adapter_result.status;
        adapter_diagnostic.decode_diagnostic = adapter_result.diagnostic.empty()
            ? "third-party image decoder adapter decoded source"
            : adapter_result.diagnostic;
        adapter_diagnostic.terminal_candidate = adapter_result.ok();
        adapter_diagnostic.diagnostic = adapter_result.ok()
            ? "third-party image decoder adapter decoded source"
            : "third-party image decoder adapter failed: " + adapter_diagnostic.decode_diagnostic;
        if (adapter_result.ok()) {
            adapter_result.decoder_diagnostics.insert(
                adapter_result.decoder_diagnostics.begin(),
                std::move(adapter_diagnostic));
            return adapter_result;
        }

        render_image_decode_result fallback_result = fallback_.decode(request);
        fallback_result.external_decoder_selection = external_selection;
        reindex_render_image_decoder_diagnostics(
            request,
            fallback_result.decoder_diagnostics,
            1);
        fallback_result.decoder_diagnostics.insert(
            fallback_result.decoder_diagnostics.begin(),
            std::move(adapter_diagnostic));
        return fallback_result;
    }

    std::size_t decoder_count() const
    {
        return fallback_.decoder_count() + (adapter_.has_backend() ? 1 : 0);
    }

private:
    third_party_image_decoder_adapter adapter_;
    const stb_image_decoder_dependency_probe_interface* stb_probe_ = nullptr;
    standard_image_decoder_chain fallback_;
};

class fake_third_party_image_decoder_backend final : public third_party_image_decoder_backend_interface {
public:
    void set_decoder_id(std::string decoder_id)
    {
        decoder_id_ = std::move(decoder_id);
    }

    void set_available(bool available)
    {
        available_ = available;
    }

    void set_supported_formats(std::vector<render_image_encoded_format> formats)
    {
        supported_formats_ = std::move(formats);
    }

    void set_result(render_image_decode_result result)
    {
        result_ = std::move(result);
    }

    void set_decoded_image(render_decoded_image image)
    {
        result_ = render_image_decode_result{
            .status = render_image_decode_status::decoded,
            .image = std::move(image),
            .diagnostic = {},
            .metadata = {},
        };
    }

    third_party_image_decoder_capability inspect(
        const render_image_decode_request& request) const override
    {
        inspect_requests.push_back(request);
        if (!available_) {
            return make_unavailable_third_party_image_decoder_capability(request, decoder_id_);
        }

        const render_image_format_detection_summary format_detection =
            detect_render_image_format(request);
        const bool supported = std::find(
            supported_formats_.begin(),
            supported_formats_.end(),
            format_detection.detected_format) != supported_formats_.end();
        if (!supported) {
            return make_unsupported_third_party_image_decoder_capability(
                request,
                decoder_id_,
                "fake third-party image decoder does not support "
                    + render_image_encoded_format_name(format_detection.detected_format));
        }

        return make_supported_third_party_image_decoder_capability(
            request,
            decoder_id_,
            "fake third-party image decoder supports "
                + render_image_encoded_format_name(format_detection.detected_format));
    }

    render_image_decode_result decode(
        const render_image_decode_request& request) const override
    {
        decode_requests.push_back(request);
        return result_;
    }

    mutable std::vector<render_image_decode_request> inspect_requests;
    mutable std::vector<render_image_decode_request> decode_requests;

private:
    std::string decoder_id_ = "fake_third_party_image_decoder";
    bool available_ = true;
    std::vector<render_image_encoded_format> supported_formats_;
    render_image_decode_result result_{
        .status = render_image_decode_status::invalid_data,
        .image = {},
        .diagnostic = "fake third-party image decoder result was not configured",
        .metadata = {},
    };
};

} // namespace quiz_vulkan::render
