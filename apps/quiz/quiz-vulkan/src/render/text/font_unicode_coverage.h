#pragma once

#include "render/text/font_cmap_inspector.h"
#include "render/text/font_source_bytes_loader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_unicode_coverage_status {
    valid,
    missing_bytes,
    sfnt_invalid,
    cmap_invalid,
};

inline std::string render_text_font_unicode_coverage_status_name(
    const render_text_font_unicode_coverage_status status)
{
    switch (status) {
    case render_text_font_unicode_coverage_status::valid:
        return "valid";
    case render_text_font_unicode_coverage_status::missing_bytes:
        return "missing_bytes";
    case render_text_font_unicode_coverage_status::sfnt_invalid:
        return "sfnt_invalid";
    case render_text_font_unicode_coverage_status::cmap_invalid:
        return "cmap_invalid";
    }

    return "unknown";
}

inline bool font_unicode_coverage_codepoint_is_unicode_scalar(const char32_t codepoint)
{
    const std::uint32_t value = static_cast<std::uint32_t>(codepoint);
    return value <= 0x10ffffU && (value < 0xd800U || value > 0xdfffU);
}

struct render_text_font_unicode_coverage_snapshot {
    std::string source_label;
    render_text_font_unicode_coverage_status status =
        render_text_font_unicode_coverage_status::missing_bytes;
    render_text_font_sfnt_inspection sfnt;
    render_text_font_cmap_inspection cmap;
    std::vector<render_text_font_cmap_range> ranges;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_unicode_coverage_status::valid;
    }

    bool supports_codepoint(const char32_t codepoint) const
    {
        if (!ok() || !font_unicode_coverage_codepoint_is_unicode_scalar(codepoint)) {
            return false;
        }

        return std::ranges::any_of(
            ranges,
            [codepoint](const render_text_font_cmap_range& range) {
                return range.contains(codepoint);
            });
    }
};

struct render_text_font_unicode_coverage_request {
    std::span<const std::byte> bytes;
    std::string source_label;
};

class font_unicode_coverage_resolver_interface {
public:
    virtual ~font_unicode_coverage_resolver_interface() = default;

    virtual render_text_font_unicode_coverage_snapshot resolve(
        const render_text_font_unicode_coverage_request& request) const = 0;

    virtual render_text_font_unicode_coverage_snapshot resolve(
        const render_text_font_source_bytes_load_result& result) const = 0;
};

inline std::string font_unicode_coverage_source_label_for(
    const render_text_font_source_bytes_load_result& result)
{
    if (!result.resolved_path.empty()) {
        return result.resolved_path;
    }
    if (!result.cache_key.empty()) {
        return result.cache_key;
    }
    if (!result.source.source_uri.empty()) {
        return result.source.source_uri;
    }
    return result.source.family;
}

inline render_text_font_unicode_coverage_snapshot make_font_unicode_coverage_missing_bytes(
    std::string source_label,
    std::string diagnostic)
{
    if (diagnostic.empty()) {
        diagnostic = "font byte buffer is empty";
    }
    return render_text_font_unicode_coverage_snapshot{
        .source_label = std::move(source_label),
        .status = render_text_font_unicode_coverage_status::missing_bytes,
        .diagnostic = std::move(diagnostic),
    };
}

inline render_text_font_unicode_coverage_snapshot resolve_font_unicode_coverage(
    const render_text_font_unicode_coverage_request& request,
    const font_sfnt_inspector_interface& sfnt_inspector,
    const font_cmap_inspector_interface& cmap_inspector)
{
    if (request.bytes.empty()) {
        return make_font_unicode_coverage_missing_bytes(
            request.source_label,
            "font byte buffer is empty");
    }

    render_text_font_unicode_coverage_snapshot snapshot{
        .source_label = request.source_label,
    };
    snapshot.sfnt = sfnt_inspector.inspect(render_text_font_sfnt_inspect_request{
        .bytes = request.bytes,
        .source_label = request.source_label,
    });
    if (!snapshot.sfnt.ok()) {
        snapshot.status = render_text_font_unicode_coverage_status::sfnt_invalid;
        snapshot.diagnostic = "SFNT inspection failed: "
            + render_text_font_sfnt_inspect_status_name(snapshot.sfnt.status)
            + ": " + snapshot.sfnt.diagnostic;
        return snapshot;
    }

    snapshot.cmap = cmap_inspector.inspect(render_text_font_cmap_inspect_request{
        .bytes = request.bytes,
        .sfnt = snapshot.sfnt,
    });
    snapshot.ranges = snapshot.cmap.ranges;
    if (!snapshot.cmap.ok()) {
        snapshot.status = render_text_font_unicode_coverage_status::cmap_invalid;
        snapshot.diagnostic = "cmap inspection failed: "
            + render_text_font_cmap_inspect_status_name(snapshot.cmap.status)
            + ": " + snapshot.cmap.diagnostic;
        return snapshot;
    }

    snapshot.status = render_text_font_unicode_coverage_status::valid;
    snapshot.diagnostic = snapshot.cmap.diagnostic;
    return snapshot;
}

inline render_text_font_unicode_coverage_snapshot resolve_font_unicode_coverage(
    const render_text_font_unicode_coverage_request& request)
{
    const basic_font_sfnt_inspector sfnt_inspector;
    const basic_font_cmap_inspector cmap_inspector;
    return resolve_font_unicode_coverage(request, sfnt_inspector, cmap_inspector);
}

inline render_text_font_unicode_coverage_snapshot resolve_font_unicode_coverage(
    const render_text_font_source_bytes_load_result& result,
    const font_sfnt_inspector_interface& sfnt_inspector,
    const font_cmap_inspector_interface& cmap_inspector)
{
    const std::string source_label = font_unicode_coverage_source_label_for(result);
    if (!result.ok() || result.bytes.empty()) {
        std::string diagnostic = "font source bytes were not loaded";
        if (!result.diagnostic.empty()) {
            diagnostic += ": " + result.diagnostic;
        } else {
            diagnostic += ": "
                + render_text_font_source_bytes_load_status_name(result.status);
        }
        return make_font_unicode_coverage_missing_bytes(
            source_label,
            std::move(diagnostic));
    }

    return resolve_font_unicode_coverage(
        render_text_font_unicode_coverage_request{
            .bytes = std::span<const std::byte>{result.bytes.data(), result.bytes.size()},
            .source_label = source_label,
        },
        sfnt_inspector,
        cmap_inspector);
}

inline render_text_font_unicode_coverage_snapshot resolve_font_unicode_coverage(
    const render_text_font_source_bytes_load_result& result)
{
    const basic_font_sfnt_inspector sfnt_inspector;
    const basic_font_cmap_inspector cmap_inspector;
    return resolve_font_unicode_coverage(result, sfnt_inspector, cmap_inspector);
}

class basic_font_unicode_coverage_resolver final
    : public font_unicode_coverage_resolver_interface {
public:
    render_text_font_unicode_coverage_snapshot resolve(
        const render_text_font_unicode_coverage_request& request) const override
    {
        return resolve_font_unicode_coverage(request, sfnt_inspector_, cmap_inspector_);
    }

    render_text_font_unicode_coverage_snapshot resolve(
        const render_text_font_source_bytes_load_result& result) const override
    {
        return resolve_font_unicode_coverage(result, sfnt_inspector_, cmap_inspector_);
    }

private:
    basic_font_sfnt_inspector sfnt_inspector_;
    basic_font_cmap_inspector cmap_inspector_;
};

} // namespace quiz_vulkan::render
