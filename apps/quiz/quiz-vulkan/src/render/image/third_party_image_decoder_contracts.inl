#pragma once

#include "render/image/image_decoder.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

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

} // namespace quiz_vulkan::render
