#pragma once

#include "render/image/image_decoder.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

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

} // namespace quiz_vulkan::render
