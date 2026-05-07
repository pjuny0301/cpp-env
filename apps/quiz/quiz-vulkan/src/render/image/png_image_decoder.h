#pragma once

#include "render/image/image_decoder.h"

#include <string>
#include <string_view>
#include <utility>

namespace quiz_vulkan::render {

inline bool png_image_decoder_supports_source(const render_resolved_image_source& source)
{
    return !source.cache_key().empty() && image_decode_extension_hint(source.normalized_uri) == "png";
}

inline bool png_image_decoder_supports_request(const render_image_decode_request& request)
{
    return png_image_decoder_supports_source(request.source)
        || detect_render_image_format(request).detected_format == render_image_encoded_format::png;
}

namespace detail {

inline render_image_decode_result make_png_image_decoder_failure(
    render_image_decode_status status,
    const render_image_decode_request& request,
    std::string diagnostic)
{
    return render_image_decode_result{
        .status = status,
        .image = {},
        .diagnostic = std::move(diagnostic),
        .metadata = make_render_image_decode_metadata("png_image_decoder", request),
    };
}

inline std::string make_png_image_decoder_stage_diagnostic(
    std::string_view stage,
    std::string_view status_name,
    std::string_view diagnostic)
{
    std::string message = "png image decoder ";
    message += stage;
    message += " failed: ";
    message += status_name;
    if (!diagnostic.empty()) {
        message += ": ";
        message += diagnostic;
    }
    return message;
}

inline render_image_decode_status png_image_decoder_status_for_plan_failure(
    png_image_decode_plan_status status)
{
    switch (status) {
    case png_image_decode_plan_status::unsupported_color_type:
    case png_image_decode_plan_status::unsupported_bit_depth:
        return render_image_decode_status::unsupported_format;
    case png_image_decode_plan_status::invalid_png_structure:
    case png_image_decode_plan_status::missing_idat_payload:
    case png_image_decode_plan_status::ready:
        return render_image_decode_status::invalid_data;
    }

    return render_image_decode_status::invalid_data;
}

inline render_image_decode_status png_image_decoder_status_for_decode_boundary_failure(
    const png_image_decode_boundary_result& decode)
{
    switch (decode.status) {
    case png_image_decode_boundary_status::ready:
        return render_image_decode_status::decoded;
    case png_image_decode_boundary_status::plan_failed:
        return png_image_decoder_status_for_plan_failure(decode.plan.status);
    case png_image_decode_boundary_status::inflater_unavailable:
        return render_image_decode_status::unsupported_format;
    case png_image_decode_boundary_status::inflater_failed:
    case png_image_decode_boundary_status::row_byte_mismatch:
        return render_image_decode_status::invalid_data;
    }

    return render_image_decode_status::invalid_data;
}

inline render_image_decode_status png_image_decoder_status_for_unfilter_failure(
    png_image_unfilter_status status)
{
    switch (status) {
    case png_image_unfilter_status::decoded:
        return render_image_decode_status::decoded;
    case png_image_unfilter_status::unsupported_filter_type:
        return render_image_decode_status::unsupported_format;
    case png_image_unfilter_status::decode_boundary_failed:
    case png_image_unfilter_status::truncated_scanline_data:
    case png_image_unfilter_status::row_stride_mismatch:
        return render_image_decode_status::invalid_data;
    }

    return render_image_decode_status::invalid_data;
}

} // namespace detail

inline render_image_decode_result decode_png_image_request_with_inflater(
    const render_image_decode_request& request,
    const png_image_inflater_interface* inflater)
{
    if (request.encoded_bytes.empty()) {
        return render_image_decode_result{
            .status = render_image_decode_status::empty_input,
            .image = {},
            .diagnostic = "png image decoder requires encoded bytes",
            .metadata = make_render_image_decode_metadata("png_image_decoder", request),
        };
    }

    if (!png_image_decoder_supports_request(request)) {
        return render_image_decode_result{
            .status = render_image_decode_status::unsupported_format,
            .image = {},
            .diagnostic = "png image decoder only supports .png sources or PNG signature bytes",
            .metadata = make_render_image_decode_metadata("png_image_decoder", request),
        };
    }

    const png_image_chunk_scan_result scan = scan_png_image_chunks(request.encoded_bytes);
    if (!scan.ok()) {
        return detail::make_png_image_decoder_failure(
            render_image_decode_status::invalid_data,
            request,
            detail::make_png_image_decoder_stage_diagnostic(
                "chunk scan",
                png_image_chunk_scan_status_name(scan.status),
                scan.diagnostic));
    }

    const png_image_decode_boundary_result decode =
        decode_png_image_with_inflater(request.encoded_bytes, scan, inflater);
    if (!decode.ok() && decode.status != png_image_decode_boundary_status::row_byte_mismatch) {
        if (decode.status == png_image_decode_boundary_status::plan_failed) {
            return detail::make_png_image_decoder_failure(
                detail::png_image_decoder_status_for_decode_boundary_failure(decode),
                request,
                detail::make_png_image_decoder_stage_diagnostic(
                    "decode plan",
                    png_image_decode_plan_status_name(decode.plan.status),
                    decode.plan.diagnostic));
        }

        return detail::make_png_image_decoder_failure(
            detail::png_image_decoder_status_for_decode_boundary_failure(decode),
            request,
            detail::make_png_image_decoder_stage_diagnostic(
                "decode boundary",
                png_image_decode_boundary_status_name(decode.status),
                decode.diagnostic));
    }

    const png_image_unfilter_result unfilter =
        unfilter_png_image_rgba8_scanlines(decode.plan.plan, decode.inflate.inflated_bytes);
    if (!unfilter.ok()) {
        return detail::make_png_image_decoder_failure(
            detail::png_image_decoder_status_for_unfilter_failure(unfilter.status),
            request,
            detail::make_png_image_decoder_stage_diagnostic(
                "unfilter",
                png_image_unfilter_status_name(unfilter.status),
                unfilter.diagnostic));
    }

    render_decoded_image image = unfilter.image;
    render_image_decode_metadata metadata = make_render_image_decode_metadata(
        "png_image_decoder",
        request,
        image);

    return render_image_decode_result{
        .status = render_image_decode_status::decoded,
        .image = std::move(image),
        .diagnostic = {},
        .metadata = std::move(metadata),
    };
}

class png_image_decoder final : public image_decoder_interface {
public:
    png_image_decoder() = default;

    explicit png_image_decoder(const png_image_inflater_interface& inflater)
        : inflater_(&inflater)
    {
    }

    explicit png_image_decoder(const png_image_inflater_interface* inflater)
        : inflater_(inflater)
    {
    }

    bool supports(const render_image_decode_request& request) const override
    {
        return png_image_decoder_supports_request(request);
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        return decode_png_image_request_with_inflater(request, inflater_);
    }

private:
    const png_image_inflater_interface* inflater_ = nullptr;
};

} // namespace quiz_vulkan::render
