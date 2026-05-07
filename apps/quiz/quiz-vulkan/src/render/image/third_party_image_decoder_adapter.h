#pragma once

#include "render/image/image_decoder.h"

#include <algorithm>
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
    std::string decoder_id = "third_party_image_decoder_adapter")
{
    return third_party_image_decoder_capability{
        .status = third_party_image_decoder_adapter_status::unavailable,
        .decoder_id = std::move(decoder_id),
        .format_detection = detect_render_image_format(request),
        .diagnostic = "third-party image decoder backend is unavailable",
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

class third_party_image_decoder_adapter final : public image_decoder_interface {
public:
    third_party_image_decoder_adapter() = default;

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

    bool supports(const render_image_decode_request& request) const override
    {
        return adapter_.has_backend() || fallback_.supports(request);
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        if (!adapter_.has_backend()) {
            return fallback_.decode(request);
        }

        const third_party_image_decoder_capability capability = adapter_.inspect(request);
        render_image_decoder_diagnostic adapter_diagnostic =
            make_third_party_image_decoder_adapter_diagnostic(request, capability, 0);
        if (!capability.supports_decode()) {
            render_image_decode_result fallback_result = fallback_.decode(request);
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
