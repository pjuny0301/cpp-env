#pragma once

#include "render/image/image_types.h"
#include "render/image/png_image_chunk_scanner.h"
#include "render/image/png_image_decode_boundary.h"
#include "render/image/png_image_header_inspector.h"
#include "render/image/png_image_unfilter_boundary.h"

#include <cctype>
#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

struct render_image_decode_request {
    render_resolved_image_source source;
    std::vector<std::byte> encoded_bytes;
};

enum class render_image_encoded_format {
    unknown,
    png,
    jpeg,
    ppm,
    bmp,
};

inline std::string render_image_encoded_format_name(render_image_encoded_format format)
{
    switch (format) {
    case render_image_encoded_format::unknown:
        return "unknown";
    case render_image_encoded_format::png:
        return "png";
    case render_image_encoded_format::jpeg:
        return "jpeg";
    case render_image_encoded_format::ppm:
        return "ppm";
    case render_image_encoded_format::bmp:
        return "bmp";
    }

    return "unknown";
}

inline std::string render_image_encoded_format_mime_type(render_image_encoded_format format)
{
    switch (format) {
    case render_image_encoded_format::png:
        return "image/png";
    case render_image_encoded_format::jpeg:
        return "image/jpeg";
    case render_image_encoded_format::ppm:
        return "image/x-portable-pixmap";
    case render_image_encoded_format::bmp:
        return "image/bmp";
    case render_image_encoded_format::unknown:
        return "application/octet-stream";
    }

    return "application/octet-stream";
}

inline std::string render_image_source_kind_name(render_image_source_kind kind)
{
    switch (kind) {
    case render_image_source_kind::local_path:
        return "local_path";
    case render_image_source_kind::file_uri:
        return "file_uri";
    case render_image_source_kind::asset_uri:
        return "asset_uri";
    case render_image_source_kind::http_uri:
        return "http_uri";
    case render_image_source_kind::https_uri:
        return "https_uri";
    case render_image_source_kind::data_uri:
        return "data_uri";
    case render_image_source_kind::unsupported:
        return "unsupported";
    }

    return "unknown";
}

struct render_image_format_detection_summary {
    render_image_encoded_format detected_format = render_image_encoded_format::unknown;
    std::string extension_hint;
    bool recognized_signature = false;
    bool placeholder_fallback = false;
    std::string diagnostic;
};

struct render_image_decode_size_validation {
    std::size_t row_stride_byte_count = 0;
    std::size_t expected_decoded_byte_count = 0;
    std::size_t actual_decoded_byte_count = 0;
    bool valid = false;
    std::string diagnostic;
};

enum class render_image_decode_status {
    decoded,
    empty_input,
    unsupported_format,
    invalid_data,
};

struct render_image_decode_metadata {
    std::string decoder_id;
    std::size_t encoded_byte_count = 0;
    std::size_t width = 0;
    std::size_t height = 0;
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    std::size_t decoded_byte_count = 0;
    render_image_format_detection_summary format_detection;
    render_image_decode_size_validation size_validation;

    bool has_image() const
    {
        return width != 0 && height != 0 && decoded_byte_count != 0;
    }
};

struct render_image_decoder_diagnostic {
    std::string decoder_id;
    std::size_t candidate_index = 0;
    std::size_t candidate_order = 0;
    std::size_t candidate_priority = 0;
    std::string extension_hint;
    render_image_encoded_format detected_format = render_image_encoded_format::unknown;
    std::string detected_format_name;
    std::string detected_mime_type;
    render_image_source_kind source_kind = render_image_source_kind::unsupported;
    std::string source_kind_name;
    bool recognized_signature = false;
    bool support_checked = false;
    bool supported = false;
    bool decode_attempted = false;
    bool terminal_candidate = false;
    render_image_decode_status status = render_image_decode_status::unsupported_format;
    std::string support_diagnostic;
    std::string decode_diagnostic;
    std::string diagnostic;
};

struct render_image_decode_result {
    render_image_decode_status status = render_image_decode_status::empty_input;
    render_decoded_image image;
    std::string diagnostic;
    render_image_decode_metadata metadata;
    std::vector<render_image_decoder_diagnostic> decoder_diagnostics;

    bool ok() const
    {
        return status == render_image_decode_status::decoded;
    }
};

inline std::string image_decode_extension_hint(std::string_view normalized_uri)
{
    const std::string::size_type last_dot = normalized_uri.find_last_of('.');
    if (last_dot == std::string_view::npos || last_dot + 1 >= normalized_uri.size()) {
        return {};
    }

    std::string extension(normalized_uri.substr(last_dot + 1));
    for (char& value : extension) {
        value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    }
    return extension;
}

inline bool starts_with_jpeg_signature(const std::vector<std::byte>& bytes)
{
    return bytes.size() >= 3
        && std::to_integer<unsigned char>(bytes[0]) == 0xff
        && std::to_integer<unsigned char>(bytes[1]) == 0xd8
        && std::to_integer<unsigned char>(bytes[2]) == 0xff;
}

inline bool starts_with_ppm_signature(const std::vector<std::byte>& bytes)
{
    return bytes.size() >= 2
        && std::to_integer<unsigned char>(bytes[0]) == 'P'
        && std::to_integer<unsigned char>(bytes[1]) == '6';
}

inline bool starts_with_bmp_signature(const std::vector<std::byte>& bytes)
{
    return bytes.size() >= 2
        && std::to_integer<unsigned char>(bytes[0]) == 'B'
        && std::to_integer<unsigned char>(bytes[1]) == 'M';
}

inline render_image_format_detection_summary detect_render_image_format(
    const render_image_decode_request& request)
{
    render_image_format_detection_summary summary{
        .detected_format = render_image_encoded_format::unknown,
        .extension_hint = image_decode_extension_hint(request.source.normalized_uri),
        .recognized_signature = false,
        .placeholder_fallback = false,
        .diagnostic = {},
    };

    if (request.encoded_bytes.empty()) {
        summary.diagnostic = "image format detection requires encoded bytes";
        return summary;
    }

    if (starts_with_png_signature(request.encoded_bytes)) {
        summary.detected_format = render_image_encoded_format::png;
        summary.recognized_signature = true;
        summary.diagnostic = "image format detection found PNG signature";
        return summary;
    }

    if (starts_with_jpeg_signature(request.encoded_bytes)) {
        summary.detected_format = render_image_encoded_format::jpeg;
        summary.recognized_signature = true;
        summary.diagnostic = "image format detection found JPEG signature";
        return summary;
    }

    if (starts_with_ppm_signature(request.encoded_bytes)) {
        summary.detected_format = render_image_encoded_format::ppm;
        summary.recognized_signature = true;
        summary.diagnostic = "image format detection found binary PPM signature";
        return summary;
    }

    if (starts_with_bmp_signature(request.encoded_bytes)) {
        summary.detected_format = render_image_encoded_format::bmp;
        summary.recognized_signature = true;
        summary.diagnostic = "image format detection found BMP signature";
        return summary;
    }

    if (summary.extension_hint == "bmp") {
        summary.detected_format = render_image_encoded_format::bmp;
        summary.diagnostic = "image format detection used BMP extension hint";
        return summary;
    }

    summary.diagnostic = "image format detection did not recognize encoded bytes";
    return summary;
}

inline std::size_t render_image_source_kind_decode_order(render_image_source_kind kind)
{
    switch (kind) {
    case render_image_source_kind::local_path:
    case render_image_source_kind::asset_uri:
    case render_image_source_kind::file_uri:
        return 0;
    case render_image_source_kind::data_uri:
        return 1;
    case render_image_source_kind::http_uri:
    case render_image_source_kind::https_uri:
        return 2;
    case render_image_source_kind::unsupported:
        return 3;
    }

    return 4;
}

inline std::size_t render_image_extension_decode_order(std::string_view extension_hint)
{
    if (extension_hint == "bmp" || extension_hint == "ppm" || extension_hint == "pnm") {
        return 0;
    }
    if (extension_hint == "fake") {
        return 1;
    }
    if (extension_hint == "png") {
        return 2;
    }
    if (extension_hint == "jpg" || extension_hint == "jpeg") {
        return 3;
    }
    if (extension_hint.empty()) {
        return 8;
    }
    return 9;
}

inline std::size_t render_image_encoded_format_decode_order(render_image_encoded_format format)
{
    switch (format) {
    case render_image_encoded_format::bmp:
    case render_image_encoded_format::ppm:
        return 0;
    case render_image_encoded_format::png:
        return 1;
    case render_image_encoded_format::jpeg:
        return 2;
    case render_image_encoded_format::unknown:
        return 3;
    }

    return 4;
}

inline std::size_t render_image_decoder_candidate_priority(
    const render_image_decode_request& request,
    std::size_t candidate_index)
{
    const render_image_format_detection_summary format_detection = detect_render_image_format(request);
    return render_image_source_kind_decode_order(request.source.kind) * 10000
        + render_image_extension_decode_order(format_detection.extension_hint) * 1000
        + render_image_encoded_format_decode_order(format_detection.detected_format) * 100
        + candidate_index;
}

inline render_image_decoder_diagnostic make_render_image_decoder_candidate_diagnostic(
    const render_image_decode_request& request,
    std::string decoder_id,
    std::size_t candidate_index)
{
    const render_image_format_detection_summary format_detection = detect_render_image_format(request);
    return render_image_decoder_diagnostic{
        .decoder_id = std::move(decoder_id),
        .candidate_index = candidate_index,
        .candidate_order = candidate_index + 1,
        .candidate_priority = render_image_decoder_candidate_priority(request, candidate_index),
        .extension_hint = format_detection.extension_hint,
        .detected_format = format_detection.detected_format,
        .detected_format_name = render_image_encoded_format_name(format_detection.detected_format),
        .detected_mime_type = render_image_encoded_format_mime_type(format_detection.detected_format),
        .source_kind = request.source.kind,
        .source_kind_name = render_image_source_kind_name(request.source.kind),
        .recognized_signature = format_detection.recognized_signature,
        .support_checked = false,
        .supported = false,
        .decode_attempted = false,
        .terminal_candidate = false,
        .status = render_image_decode_status::unsupported_format,
        .support_diagnostic = {},
        .decode_diagnostic = {},
        .diagnostic = "decoder candidate has not been checked",
    };
}

inline std::size_t render_image_decode_pixel_format_byte_count(render_image_pixel_format pixel_format)
{
    switch (pixel_format) {
    case render_image_pixel_format::rgba8_unorm:
    case render_image_pixel_format::rgba8_srgb:
        return 4;
    }

    return 0;
}

inline render_image_decode_size_validation validate_render_image_decode_size(
    const render_decoded_image& image)
{
    render_image_decode_size_validation validation{
        .row_stride_byte_count = 0,
        .expected_decoded_byte_count = 0,
        .actual_decoded_byte_count = image.pixels.size(),
        .valid = false,
        .diagnostic = {},
    };

    const std::size_t bytes_per_pixel = render_image_decode_pixel_format_byte_count(image.pixel_format);
    if (bytes_per_pixel == 0) {
        validation.diagnostic = "decoded image pixel format is unsupported";
        return validation;
    }
    if (image.width == 0 || image.height == 0) {
        validation.diagnostic = "decoded image dimensions are empty";
        return validation;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (image.width > max_size / bytes_per_pixel) {
        validation.diagnostic = "decoded image row stride overflows";
        return validation;
    }
    validation.row_stride_byte_count = image.width * bytes_per_pixel;
    if (validation.row_stride_byte_count > max_size / image.height) {
        validation.diagnostic = "decoded image byte size overflows";
        return validation;
    }
    validation.expected_decoded_byte_count = validation.row_stride_byte_count * image.height;
    if (validation.actual_decoded_byte_count != validation.expected_decoded_byte_count) {
        validation.diagnostic = "decoded image byte size does not match dimensions and pixel format";
        return validation;
    }

    validation.valid = true;
    return validation;
}

inline render_image_format_detection_summary with_placeholder_fallback(
    render_image_format_detection_summary summary,
    std::string_view decoder_id)
{
    summary.placeholder_fallback = true;
    switch (summary.detected_format) {
    case render_image_encoded_format::png:
        summary.diagnostic = std::string(decoder_id)
            + " detected PNG bytes and used deterministic placeholder pixels";
        break;
    case render_image_encoded_format::jpeg:
        summary.diagnostic = std::string(decoder_id)
            + " detected JPEG bytes and used deterministic placeholder pixels";
        break;
    case render_image_encoded_format::ppm:
        summary.diagnostic = std::string(decoder_id)
            + " detected PPM bytes and used deterministic placeholder pixels";
        break;
    case render_image_encoded_format::bmp:
        summary.diagnostic = std::string(decoder_id)
            + " detected BMP bytes and used deterministic placeholder pixels";
        break;
    case render_image_encoded_format::unknown:
        summary.diagnostic = std::string(decoder_id)
            + " used deterministic placeholder pixels for unknown encoded bytes";
        break;
    }
    return summary;
}

inline render_image_decode_metadata make_render_image_decode_metadata(
    std::string decoder_id,
    const render_image_decode_request& request,
    const render_decoded_image& image)
{
    return render_image_decode_metadata{
        .decoder_id = std::move(decoder_id),
        .encoded_byte_count = request.encoded_bytes.size(),
        .width = image.width,
        .height = image.height,
        .pixel_format = image.pixel_format,
        .decoded_byte_count = image.pixels.size(),
        .format_detection = detect_render_image_format(request),
        .size_validation = validate_render_image_decode_size(image),
    };
}

inline render_image_decode_metadata make_render_image_decode_metadata(
    std::string decoder_id,
    const render_image_decode_request& request)
{
    return make_render_image_decode_metadata(std::move(decoder_id), request, render_decoded_image{});
}

class image_decoder_interface {
public:
    virtual ~image_decoder_interface() = default;

    virtual bool supports(const render_image_decode_request& request) const = 0;
    virtual render_image_decode_result decode(const render_image_decode_request& request) const = 0;
};

struct image_decoder_chain_entry {
    std::string decoder_id;
    std::reference_wrapper<const image_decoder_interface> decoder;
};

class image_decoder_chain final : public image_decoder_interface {
public:
    image_decoder_chain() = default;

    explicit image_decoder_chain(std::vector<std::reference_wrapper<const image_decoder_interface>> decoders)
    {
        decoders_.reserve(decoders.size());
        for (const std::reference_wrapper<const image_decoder_interface> decoder : decoders) {
            add_decoder(decoder.get());
        }
    }

    explicit image_decoder_chain(std::vector<image_decoder_chain_entry> decoders)
        : decoders_(std::move(decoders))
    {
    }

    void add_decoder(const image_decoder_interface& decoder)
    {
        add_decoder(make_default_decoder_id(decoders_.size()), decoder);
    }

    void add_decoder(std::string decoder_id, const image_decoder_interface& decoder)
    {
        decoders_.push_back(image_decoder_chain_entry{
            .decoder_id = std::move(decoder_id),
            .decoder = std::cref(decoder),
        });
    }

    std::size_t decoder_count() const
    {
        return decoders_.size();
    }

    bool supports(const render_image_decode_request& request) const override
    {
        for (const image_decoder_chain_entry& entry : decoders_) {
            if (entry.decoder.get().supports(request)) {
                return true;
            }
        }
        return false;
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        std::vector<render_image_decoder_diagnostic> diagnostics;
        diagnostics.reserve(decoders_.size());
        for (std::size_t index = 0; index < decoders_.size(); ++index) {
            const image_decoder_chain_entry& entry = decoders_[index];
            render_image_decoder_diagnostic candidate =
                make_render_image_decoder_candidate_diagnostic(request, entry.decoder_id, index);
            candidate.support_checked = true;
            candidate.supported = entry.decoder.get().supports(request);
            candidate.support_diagnostic = candidate.supported
                ? "decoder candidate supports source"
                : "decoder candidate did not support source";
            if (!candidate.supported) {
                candidate.status = render_image_decode_status::unsupported_format;
                candidate.diagnostic = candidate.support_diagnostic;
                diagnostics.push_back(std::move(candidate));
                continue;
            }

            render_image_decode_result result = entry.decoder.get().decode(request);
            if (result.metadata.decoder_id.empty()) {
                result.metadata.decoder_id = entry.decoder_id;
            }
            candidate.decode_attempted = true;
            candidate.terminal_candidate = true;
            candidate.status = result.status;
            candidate.decode_diagnostic = result.diagnostic.empty()
                ? "decoder candidate decoded source"
                : result.diagnostic;
            candidate.diagnostic = result.ok()
                ? "decoder candidate decoded source"
                : "decoder candidate decode failed: " + candidate.decode_diagnostic;
            diagnostics.push_back(std::move(candidate));
            result.decoder_diagnostics.insert(
                result.decoder_diagnostics.begin(),
                diagnostics.begin(),
                diagnostics.end());
            return result;
        }

        if (!diagnostics.empty()) {
            diagnostics.back().terminal_candidate = true;
            diagnostics.back().diagnostic = "decoder chain exhausted all candidates";
        }

        return render_image_decode_result{
            .status = render_image_decode_status::unsupported_format,
            .image = {},
            .diagnostic = "no image decoder in the chain supports the source",
            .metadata = make_render_image_decode_metadata("image_decoder_chain", request),
            .decoder_diagnostics = std::move(diagnostics),
        };
    }

private:
    static std::string make_default_decoder_id(std::size_t index)
    {
        return "decoder[" + std::to_string(index) + "]";
    }

    std::vector<image_decoder_chain_entry> decoders_;
};

namespace detail {

inline unsigned char ppm_byte_to_uchar(std::byte value)
{
    return std::to_integer<unsigned char>(value);
}

inline bool ppm_is_ascii_space(unsigned char value)
{
    switch (value) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
    case '\f':
    case '\v':
        return true;
    default:
        return false;
    }
}

inline bool ppm_is_ascii_digit(unsigned char value)
{
    return value >= '0' && value <= '9';
}

inline void ppm_skip_whitespace_and_comments(const std::vector<std::byte>& bytes, std::size_t& offset)
{
    while (offset < bytes.size()) {
        const unsigned char value = ppm_byte_to_uchar(bytes[offset]);
        if (ppm_is_ascii_space(value)) {
            ++offset;
            continue;
        }

        if (value != '#') {
            return;
        }

        while (offset < bytes.size() && ppm_byte_to_uchar(bytes[offset]) != '\n') {
            ++offset;
        }
    }
}

inline bool ppm_parse_positive_integer(
    const std::vector<std::byte>& bytes,
    std::size_t& offset,
    std::size_t& parsed)
{
    ppm_skip_whitespace_and_comments(bytes, offset);
    if (offset >= bytes.size() || !ppm_is_ascii_digit(ppm_byte_to_uchar(bytes[offset]))) {
        return false;
    }

    std::size_t value = 0;
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    while (offset < bytes.size()) {
        const unsigned char current = ppm_byte_to_uchar(bytes[offset]);
        if (!ppm_is_ascii_digit(current)) {
            break;
        }

        const std::size_t digit = static_cast<std::size_t>(current - '0');
        if (value > (max_size - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
        ++offset;
    }

    if (value == 0) {
        return false;
    }

    parsed = value;
    return true;
}

inline bool ppm_checked_pixel_count(std::size_t width, std::size_t height, std::size_t& pixel_count)
{
    if (width == 0 || height == 0) {
        return false;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (width > max_size / height) {
        return false;
    }

    pixel_count = width * height;
    return true;
}

} // namespace detail

inline bool fake_image_decoder_supports_source(const render_resolved_image_source& source)
{
    return !source.cache_key().empty() && source.normalized_uri.ends_with(".fake");
}

inline bool ppm_image_decoder_supports_source(const render_resolved_image_source& source)
{
    return !source.cache_key().empty()
        && (source.normalized_uri.ends_with(".ppm") || source.normalized_uri.ends_with(".pnm"));
}

class ppm_image_decoder final : public image_decoder_interface {
public:
    bool supports(const render_image_decode_request& request) const override
    {
        return ppm_image_decoder_supports_source(request.source);
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        if (request.encoded_bytes.empty()) {
            return render_image_decode_result{
                .status = render_image_decode_status::empty_input,
                .image = {},
                .diagnostic = "ppm image decoder requires encoded bytes",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        if (!ppm_image_decoder_supports_source(request.source)) {
            return render_image_decode_result{
                .status = render_image_decode_status::unsupported_format,
                .image = {},
                .diagnostic = "ppm image decoder only supports .ppm or .pnm image sources",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        const std::vector<std::byte>& bytes = request.encoded_bytes;
        if (bytes.size() < 3 || detail::ppm_byte_to_uchar(bytes[0]) != 'P'
            || detail::ppm_byte_to_uchar(bytes[1]) != '6') {
            return render_image_decode_result{
                .status = render_image_decode_status::unsupported_format,
                .image = {},
                .diagnostic = "ppm image decoder requires binary P6 magic",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        if (!detail::ppm_is_ascii_space(detail::ppm_byte_to_uchar(bytes[2]))) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image header must delimit the P6 magic with whitespace",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        std::size_t offset = 2;
        std::size_t width = 0;
        std::size_t height = 0;
        std::size_t max_value = 0;
        if (!detail::ppm_parse_positive_integer(bytes, offset, width)
            || !detail::ppm_parse_positive_integer(bytes, offset, height)
            || !detail::ppm_parse_positive_integer(bytes, offset, max_value)) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image header must contain positive width, height, and max value",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        if (max_value != 255) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image decoder only supports 8-bit max value 255",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        if (offset >= bytes.size() || !detail::ppm_is_ascii_space(detail::ppm_byte_to_uchar(bytes[offset]))) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image header must end with a whitespace byte before pixel data",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }
        ++offset;

        std::size_t pixel_count = 0;
        if (!detail::ppm_checked_pixel_count(width, height, pixel_count)) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image dimensions overflow the pixel count",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
        if (pixel_count > max_size / 4) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image dimensions overflow the RGBA byte count",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }
        const std::size_t rgba_byte_count = pixel_count * 4;

        if (pixel_count > max_size / 3) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image dimensions overflow the RGB byte count",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }
        const std::size_t rgb_byte_count = pixel_count * 3;
        if (bytes.size() - offset != rgb_byte_count) {
            return render_image_decode_result{
                .status = render_image_decode_status::invalid_data,
                .image = {},
                .diagnostic = "ppm image pixel data size does not match dimensions",
                .metadata = make_render_image_decode_metadata("ppm_image_decoder", request),
            };
        }

        std::vector<std::byte> pixels;
        pixels.reserve(rgba_byte_count);
        for (std::size_t rgb_offset = offset; rgb_offset < bytes.size(); rgb_offset += 3) {
            pixels.push_back(bytes[rgb_offset]);
            pixels.push_back(bytes[rgb_offset + 1]);
            pixels.push_back(bytes[rgb_offset + 2]);
            pixels.push_back(std::byte{0xff});
        }

        render_decoded_image image{
            .width = width,
            .height = height,
            .pixel_format = render_image_pixel_format::rgba8_srgb,
            .pixels = std::move(pixels),
        };
        render_image_decode_metadata metadata = make_render_image_decode_metadata(
            "ppm_image_decoder",
            request,
            image);

        return render_image_decode_result{
            .status = render_image_decode_status::decoded,
            .image = std::move(image),
            .diagnostic = {},
            .metadata = std::move(metadata),
        };
    }
};

class fake_image_decoder final : public image_decoder_interface {
public:
    bool supports(const render_image_decode_request& request) const override
    {
        support_requests.push_back(request);
        return fake_image_decoder_supports_source(request.source);
    }

    render_image_decode_result decode(const render_image_decode_request& request) const override
    {
        decode_requests.push_back(request);
        if (request.encoded_bytes.empty()) {
            return render_image_decode_result{
                .status = render_image_decode_status::empty_input,
                .image = {},
                .diagnostic = "fake image decoder requires placeholder bytes",
                .metadata = make_render_image_decode_metadata("fake_image_decoder", request),
            };
        }

        if (!fake_image_decoder_supports_source(request.source)) {
            return render_image_decode_result{
                .status = render_image_decode_status::unsupported_format,
                .image = {},
                .diagnostic = "fake image decoder only supports .fake image sources",
                .metadata = make_render_image_decode_metadata("fake_image_decoder", request),
            };
        }

        render_decoded_image image{
            .width = 2,
            .height = 1,
            .pixel_format = render_image_pixel_format::rgba8_srgb,
            .pixels = {
                std::byte{0xff},
                std::byte{0x00},
                std::byte{0x00},
                std::byte{0xff},
                std::byte{0x00},
                std::byte{0xff},
                std::byte{0x00},
                std::byte{0xff},
            },
        };
        render_image_decode_metadata metadata = make_render_image_decode_metadata(
            "fake_image_decoder",
            request,
            image);
        metadata.format_detection = with_placeholder_fallback(
            std::move(metadata.format_detection),
            "fake_image_decoder");

        return render_image_decode_result{
            .status = render_image_decode_status::decoded,
            .image = std::move(image),
            .diagnostic = {},
            .metadata = std::move(metadata),
        };
    }

    mutable std::vector<render_image_decode_request> support_requests;
    mutable std::vector<render_image_decode_request> decode_requests;
};

} // namespace quiz_vulkan::render

#include "render/image/bmp_image_decoder.h"
