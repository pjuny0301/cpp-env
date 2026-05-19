#pragma once

#include "render/image/image_decoder.h"
#include "render/image/stb_image_decoder_selection.inl"

#include <cstddef>
#include <utility>

namespace quiz_vulkan::render {

const third_party_image_decoder_backend_interface* default_stb_image_decoder_backend();

enum class standard_image_stb_jpeg_route {
    enabled,
    disabled,
};

class standard_image_decoder_chain final : public image_decoder_interface {
public:
    standard_image_decoder_chain()
        : standard_image_decoder_chain(standard_image_stb_jpeg_route::enabled)
    {
    }

    explicit standard_image_decoder_chain(standard_image_stb_jpeg_route stb_jpeg_route)
        : stb_route_enabled_(stb_jpeg_route == standard_image_stb_jpeg_route::enabled)
        , stb_backend_(stb_route_enabled_ ? default_stb_image_decoder_backend() : nullptr)
        , png_decoder_(png_inflater_)
    {
        chain_.add_decoder("bmp_image_decoder", bmp_decoder_);
        chain_.add_decoder("ppm_image_decoder", ppm_decoder_);
        chain_.add_decoder("png_image_decoder", png_decoder_);
    }

    standard_image_decoder_chain(const standard_image_decoder_chain&) = delete;
    standard_image_decoder_chain& operator=(const standard_image_decoder_chain&) = delete;
    standard_image_decoder_chain(standard_image_decoder_chain&&) = delete;
    standard_image_decoder_chain& operator=(standard_image_decoder_chain&&) = delete;

    bool supports(const render_image_decode_request& request) const override
    {
        if (stb_route_enabled_) {
            const stb_image_decoder_adapter_selection_result stb_selection =
                select_stb_standard_format_adapter(request);
            if (stb_selection.ready_for_external_decode) {
                return true;
            }
        }
        return chain_.supports(request);
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        if (stb_route_enabled_) {
            const stb_image_decoder_adapter_selection_result stb_selection =
                select_stb_standard_format_adapter(request);
            if (stb_selection.detected_format == render_image_encoded_format::jpeg
                || stb_selection.detected_format == render_image_encoded_format::png) {
                return decode_with_optional_stb_adapter(request, stb_selection);
            }
        }
        return chain_.decode(request);
    }

    std::size_t decoder_count() const
    {
        return chain_.decoder_count() + (stb_route_enabled_ ? 1 : 0);
    }

private:
    stb_image_decoder_adapter_selection_result select_stb_standard_format_adapter(
        const render_image_decode_request& request) const
    {
        const render_image_format_detection_summary format_detection =
            detect_render_image_format(request);
        if (format_detection.detected_format != render_image_encoded_format::jpeg
            && format_detection.detected_format != render_image_encoded_format::png) {
            return stb_image_decoder_adapter_selection_result{
                .format_detection = format_detection,
                .detected_format = format_detection.detected_format,
                .detected_format_name = render_image_encoded_format_name(
                    format_detection.detected_format),
            };
        }

        return select_stb_image_decoder_adapter(request, stb_probe_);
    }

    render_image_decode_result decode_with_optional_stb_adapter(
        const render_image_decode_request& request,
        const stb_image_decoder_adapter_selection_result& stb_selection) const
    {
        const render_image_external_decoder_selection_snapshot external_selection =
            make_render_image_external_decoder_selection_snapshot(stb_selection);
        const third_party_image_decoder_capability capability =
            make_third_party_image_decoder_capability_from_stb_selection(
                request,
                stb_selection);
        render_image_decoder_diagnostic adapter_diagnostic =
            make_third_party_image_decoder_adapter_diagnostic(request, capability, 0);

        if (capability.supports_decode() && stb_backend_ != nullptr) {
            render_image_decode_result adapter_result = stb_backend_->decode(request);
            adapter_result.external_decoder_selection = external_selection;
            adapter_diagnostic.decode_attempted = true;
            adapter_diagnostic.status = adapter_result.status;
            adapter_diagnostic.decode_diagnostic = adapter_result.diagnostic.empty()
                ? "third-party image decoder adapter decoded source"
                : adapter_result.diagnostic;
            adapter_diagnostic.terminal_candidate = adapter_result.ok();
            adapter_diagnostic.diagnostic = adapter_result.ok()
                ? "third-party image decoder adapter decoded source"
                : "third-party image decoder adapter failed: "
                    + adapter_diagnostic.decode_diagnostic;
            if (adapter_result.ok()) {
                if (adapter_result.metadata.decoder_id.empty()) {
                    adapter_result.metadata =
                        make_render_image_decode_metadata(stb_selection.decoder_id, request, adapter_result.image);
                } else {
                    adapter_result.metadata.decoder_id = stb_selection.decoder_id;
                }
                adapter_result.decoder_diagnostics.insert(
                    adapter_result.decoder_diagnostics.begin(),
                    std::move(adapter_diagnostic));
                return adapter_result;
            }

            return decode_with_internal_fallback(
                request,
                external_selection,
                std::move(adapter_diagnostic));
        }

        return decode_with_internal_fallback(
            request,
            external_selection,
            std::move(adapter_diagnostic));
    }

    render_image_decode_result decode_with_internal_fallback(
        const render_image_decode_request& request,
        const render_image_external_decoder_selection_snapshot& external_selection,
        render_image_decoder_diagnostic adapter_diagnostic) const
    {
        render_image_decode_result fallback_result = chain_.decode(request);
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

    stb_image_decoder_header_dependency_probe stb_probe_;
    bool stb_route_enabled_ = true;
    const third_party_image_decoder_backend_interface* stb_backend_ = nullptr;
    bmp_image_decoder bmp_decoder_;
    ppm_image_decoder ppm_decoder_;
    png_image_zlib_inflater png_inflater_;
    png_image_decoder png_decoder_;
    image_decoder_chain chain_;
};

inline standard_image_decoder_chain make_standard_image_decoder_chain()
{
    return standard_image_decoder_chain{};
}

} // namespace quiz_vulkan::render
