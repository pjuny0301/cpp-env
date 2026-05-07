#pragma once

#include "render/text/font_sfnt_inspector.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_cmap_inspect_status {
    valid,
    missing_cmap_table,
    malformed_table,
    unsupported_version,
    no_unicode_subtable,
    unsupported_subtable_format,
    truncated_subtable,
};

inline std::string render_text_font_cmap_inspect_status_name(
    const render_text_font_cmap_inspect_status status)
{
    switch (status) {
    case render_text_font_cmap_inspect_status::valid:
        return "valid";
    case render_text_font_cmap_inspect_status::missing_cmap_table:
        return "missing_cmap_table";
    case render_text_font_cmap_inspect_status::malformed_table:
        return "malformed_table";
    case render_text_font_cmap_inspect_status::unsupported_version:
        return "unsupported_version";
    case render_text_font_cmap_inspect_status::no_unicode_subtable:
        return "no_unicode_subtable";
    case render_text_font_cmap_inspect_status::unsupported_subtable_format:
        return "unsupported_subtable_format";
    case render_text_font_cmap_inspect_status::truncated_subtable:
        return "truncated_subtable";
    }

    return "unknown";
}

struct render_text_font_cmap_range {
    char32_t first_codepoint = 0;
    char32_t last_codepoint = 0;

    bool contains(const char32_t codepoint) const
    {
        return first_codepoint <= codepoint && codepoint <= last_codepoint;
    }
};

struct render_text_font_cmap_inspection {
    render_text_font_cmap_inspect_status status =
        render_text_font_cmap_inspect_status::malformed_table;
    std::uint16_t selected_platform_id = 0;
    std::uint16_t selected_encoding_id = 0;
    std::uint16_t selected_format = 0;
    std::vector<render_text_font_cmap_range> ranges;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_text_font_cmap_inspect_status::valid;
    }

    bool supports_codepoint(const char32_t codepoint) const
    {
        if (!ok() || !font_cmap_is_unicode_scalar(codepoint)) {
            return false;
        }

        return std::ranges::any_of(
            ranges,
            [codepoint](const render_text_font_cmap_range& range) {
                return range.contains(codepoint);
            });
    }

private:
    static bool font_cmap_is_unicode_scalar(const char32_t codepoint)
    {
        const std::uint32_t value = static_cast<std::uint32_t>(codepoint);
        return value <= 0x10ffffU && (value < 0xd800U || value > 0xdfffU);
    }
};

struct render_text_font_cmap_inspect_request {
    std::span<const std::byte> bytes;
    render_text_font_sfnt_inspection sfnt;
};

class font_cmap_inspector_interface {
public:
    virtual ~font_cmap_inspector_interface() = default;

    virtual render_text_font_cmap_inspection inspect(
        const render_text_font_cmap_inspect_request& request) const = 0;
};

struct font_cmap_subtable_candidate {
    std::uint16_t platform_id = 0;
    std::uint16_t encoding_id = 0;
    std::uint16_t format = 0;
    std::uint32_t offset = 0;
    std::size_t record_index = 0;
    bool has_format = false;
};

inline bool font_cmap_codepoint_is_unicode_scalar(const std::uint32_t codepoint)
{
    return codepoint <= 0x10ffffU && (codepoint < 0xd800U || codepoint > 0xdfffU);
}

inline bool font_cmap_record_is_unicode(
    const std::uint16_t platform_id,
    const std::uint16_t encoding_id)
{
    if (platform_id == 0U) {
        return true;
    }
    return platform_id == 3U && (encoding_id == 1U || encoding_id == 10U);
}

inline int font_cmap_unicode_subtable_preference(const font_cmap_subtable_candidate& candidate)
{
    int preference = 0;
    if (candidate.platform_id == 3U && candidate.encoding_id == 10U) {
        preference = 300;
    } else if (candidate.platform_id == 0U) {
        preference = 250;
    } else if (candidate.platform_id == 3U && candidate.encoding_id == 1U) {
        preference = 200;
    }

    if (candidate.format == 12U) {
        preference += 1000;
    } else if (candidate.format == 4U) {
        preference += 500;
    }

    return preference;
}

inline void font_cmap_append_scalar_range(
    std::vector<render_text_font_cmap_range>& ranges,
    std::uint32_t first_codepoint,
    std::uint32_t last_codepoint)
{
    if (first_codepoint > last_codepoint || first_codepoint > 0x10ffffU) {
        return;
    }

    last_codepoint = std::min(last_codepoint, 0x10ffffU);
    if (first_codepoint <= 0xd7ffU) {
        ranges.push_back(render_text_font_cmap_range{
            .first_codepoint = static_cast<char32_t>(first_codepoint),
            .last_codepoint = static_cast<char32_t>(std::min(last_codepoint, 0xd7ffU)),
        });
    }
    if (last_codepoint >= 0xe000U) {
        ranges.push_back(render_text_font_cmap_range{
            .first_codepoint = static_cast<char32_t>(std::max(first_codepoint, 0xe000U)),
            .last_codepoint = static_cast<char32_t>(last_codepoint),
        });
    }
}

inline void font_cmap_normalize_ranges(std::vector<render_text_font_cmap_range>& ranges)
{
    std::ranges::sort(
        ranges,
        [](const render_text_font_cmap_range& lhs, const render_text_font_cmap_range& rhs) {
            return lhs.first_codepoint < rhs.first_codepoint;
        });

    std::vector<render_text_font_cmap_range> normalized;
    normalized.reserve(ranges.size());
    for (const render_text_font_cmap_range& range : ranges) {
        if (range.first_codepoint > range.last_codepoint) {
            continue;
        }
        if (normalized.empty()) {
            normalized.push_back(range);
            continue;
        }

        const std::uint32_t previous_last =
            static_cast<std::uint32_t>(normalized.back().last_codepoint);
        const std::uint32_t next_first = static_cast<std::uint32_t>(range.first_codepoint);
        if (next_first <= previous_last + 1U) {
            normalized.back().last_codepoint = std::max(
                normalized.back().last_codepoint,
                range.last_codepoint);
        } else {
            normalized.push_back(range);
        }
    }
    ranges = std::move(normalized);
}

inline bool font_cmap_subtable_range_is_inside(
    const std::uint32_t table_length,
    const std::uint32_t offset,
    const std::uint32_t length)
{
    return offset <= table_length && length <= table_length - offset;
}

inline render_text_font_cmap_inspection font_cmap_parse_format4_coverage(
    const std::span<const std::byte> bytes,
    const render_text_font_sfnt_table_record& table,
    const font_cmap_subtable_candidate& candidate,
    render_text_font_cmap_inspection inspection)
{
    if (!font_cmap_subtable_range_is_inside(table.length, candidate.offset, 6U)) {
        inspection.status = render_text_font_cmap_inspect_status::truncated_subtable;
        inspection.diagnostic = "cmap format 4 subtable header exceeds the cmap table";
        return inspection;
    }

    const std::size_t subtable_start =
        static_cast<std::size_t>(table.offset) + candidate.offset;
    const std::uint16_t length = font_sfnt_read_u16_be(bytes, subtable_start + 2U);
    if (length < 16U) {
        inspection.status = render_text_font_cmap_inspect_status::malformed_table;
        inspection.diagnostic = "cmap format 4 subtable is too small";
        return inspection;
    }
    if (!font_cmap_subtable_range_is_inside(table.length, candidate.offset, length)) {
        inspection.status = render_text_font_cmap_inspect_status::truncated_subtable;
        inspection.diagnostic = "cmap format 4 subtable length exceeds the cmap table";
        return inspection;
    }

    const std::uint16_t seg_count_x2 = font_sfnt_read_u16_be(bytes, subtable_start + 6U);
    if (seg_count_x2 == 0U || seg_count_x2 % 2U != 0U) {
        inspection.status = render_text_font_cmap_inspect_status::malformed_table;
        inspection.diagnostic = "cmap format 4 segment count is malformed";
        return inspection;
    }

    const std::uint16_t seg_count = static_cast<std::uint16_t>(seg_count_x2 / 2U);
    const std::size_t required_length = 16U + static_cast<std::size_t>(seg_count) * 8U;
    if (required_length > length) {
        inspection.status = render_text_font_cmap_inspect_status::truncated_subtable;
        inspection.diagnostic = "cmap format 4 segment arrays exceed the subtable length";
        return inspection;
    }

    const std::size_t end_code_offset = subtable_start + 14U;
    const std::size_t reserved_pad_offset = end_code_offset + static_cast<std::size_t>(seg_count) * 2U;
    const std::size_t start_code_offset = reserved_pad_offset + 2U;
    const std::uint16_t reserved_pad = font_sfnt_read_u16_be(bytes, reserved_pad_offset);
    if (reserved_pad != 0U) {
        inspection.status = render_text_font_cmap_inspect_status::malformed_table;
        inspection.diagnostic = "cmap format 4 reserved pad is non-zero";
        return inspection;
    }

    for (std::uint16_t index = 0; index < seg_count; ++index) {
        const std::uint16_t end_code =
            font_sfnt_read_u16_be(bytes, end_code_offset + static_cast<std::size_t>(index) * 2U);
        const std::uint16_t start_code =
            font_sfnt_read_u16_be(bytes, start_code_offset + static_cast<std::size_t>(index) * 2U);
        if (start_code > end_code) {
            inspection.status = render_text_font_cmap_inspect_status::malformed_table;
            inspection.diagnostic = "cmap format 4 segment start exceeds end";
            return inspection;
        }

        if (start_code == 0xffffU && end_code == 0xffffU) {
            continue;
        }
        font_cmap_append_scalar_range(inspection.ranges, start_code, end_code);
    }

    font_cmap_normalize_ranges(inspection.ranges);
    inspection.status = render_text_font_cmap_inspect_status::valid;
    inspection.diagnostic = "valid cmap format 4 Unicode coverage";
    return inspection;
}

inline render_text_font_cmap_inspection font_cmap_parse_format12_coverage(
    const std::span<const std::byte> bytes,
    const render_text_font_sfnt_table_record& table,
    const font_cmap_subtable_candidate& candidate,
    render_text_font_cmap_inspection inspection)
{
    if (!font_cmap_subtable_range_is_inside(table.length, candidate.offset, 16U)) {
        inspection.status = render_text_font_cmap_inspect_status::truncated_subtable;
        inspection.diagnostic = "cmap format 12 subtable header exceeds the cmap table";
        return inspection;
    }

    const std::size_t subtable_start =
        static_cast<std::size_t>(table.offset) + candidate.offset;
    const std::uint16_t reserved = font_sfnt_read_u16_be(bytes, subtable_start + 2U);
    const std::uint32_t length = font_sfnt_read_u32_be(bytes, subtable_start + 4U);
    const std::uint32_t group_count = font_sfnt_read_u32_be(bytes, subtable_start + 12U);
    if (reserved != 0U) {
        inspection.status = render_text_font_cmap_inspect_status::malformed_table;
        inspection.diagnostic = "cmap format 12 reserved field is non-zero";
        return inspection;
    }
    if (length < 16U) {
        inspection.status = render_text_font_cmap_inspect_status::malformed_table;
        inspection.diagnostic = "cmap format 12 subtable is too small";
        return inspection;
    }
    if (!font_cmap_subtable_range_is_inside(table.length, candidate.offset, length)) {
        inspection.status = render_text_font_cmap_inspect_status::truncated_subtable;
        inspection.diagnostic = "cmap format 12 subtable length exceeds the cmap table";
        return inspection;
    }
    if (group_count > (length - 16U) / 12U) {
        inspection.status = render_text_font_cmap_inspect_status::truncated_subtable;
        inspection.diagnostic = "cmap format 12 groups exceed the subtable length";
        return inspection;
    }

    const std::size_t groups_offset = subtable_start + 16U;
    for (std::uint32_t index = 0; index < group_count; ++index) {
        const std::size_t group_offset = groups_offset + static_cast<std::size_t>(index) * 12U;
        const std::uint32_t start_char_code = font_sfnt_read_u32_be(bytes, group_offset);
        const std::uint32_t end_char_code = font_sfnt_read_u32_be(bytes, group_offset + 4U);
        if (start_char_code > end_char_code) {
            inspection.status = render_text_font_cmap_inspect_status::malformed_table;
            inspection.diagnostic = "cmap format 12 group start exceeds end";
            return inspection;
        }
        if (!font_cmap_codepoint_is_unicode_scalar(start_char_code)
            || end_char_code > 0x10ffffU) {
            inspection.status = render_text_font_cmap_inspect_status::malformed_table;
            inspection.diagnostic = "cmap format 12 group is outside Unicode scalar range";
            return inspection;
        }

        font_cmap_append_scalar_range(inspection.ranges, start_char_code, end_char_code);
    }

    font_cmap_normalize_ranges(inspection.ranges);
    inspection.status = render_text_font_cmap_inspect_status::valid;
    inspection.diagnostic = "valid cmap format 12 Unicode coverage";
    return inspection;
}

inline const font_cmap_subtable_candidate* font_cmap_select_subtable(
    const std::vector<font_cmap_subtable_candidate>& candidates)
{
    const font_cmap_subtable_candidate* selected = nullptr;
    int selected_preference = -1;
    for (const font_cmap_subtable_candidate& candidate : candidates) {
        const int preference = font_cmap_unicode_subtable_preference(candidate);
        if (selected == nullptr
            || preference > selected_preference
            || (preference == selected_preference && candidate.record_index < selected->record_index)) {
            selected = &candidate;
            selected_preference = preference;
        }
    }
    return selected;
}

inline render_text_font_cmap_inspection inspect_font_cmap_coverage(
    const render_text_font_cmap_inspect_request& request)
{
    render_text_font_cmap_inspection inspection;
    const render_text_font_sfnt_table_record* cmap_table = font_sfnt_find_table(request.sfnt, "cmap");
    if (cmap_table == nullptr) {
        inspection.status = render_text_font_cmap_inspect_status::missing_cmap_table;
        inspection.diagnostic = "SFNT font has no cmap table";
        return inspection;
    }
    if (!font_sfnt_range_is_inside(request.bytes.size(), cmap_table->offset, cmap_table->length)) {
        inspection.status = render_text_font_cmap_inspect_status::malformed_table;
        inspection.diagnostic = "SFNT cmap table range exceeds the font byte buffer";
        return inspection;
    }

    const std::size_t table_start = cmap_table->offset;
    if (cmap_table->length < 4U) {
        inspection.status = render_text_font_cmap_inspect_status::malformed_table;
        inspection.diagnostic = "cmap table header requires at least 4 bytes";
        return inspection;
    }

    const std::uint16_t version = font_sfnt_read_u16_be(request.bytes, table_start);
    const std::uint16_t subtable_count = font_sfnt_read_u16_be(request.bytes, table_start + 2U);
    if (version != 0U) {
        inspection.status = render_text_font_cmap_inspect_status::unsupported_version;
        inspection.diagnostic = "cmap table version is not supported";
        return inspection;
    }

    const std::size_t encoding_records_size = static_cast<std::size_t>(subtable_count) * 8U;
    if (encoding_records_size > cmap_table->length - 4U) {
        inspection.status = render_text_font_cmap_inspect_status::malformed_table;
        inspection.diagnostic = "cmap encoding records exceed the table bounds";
        return inspection;
    }

    std::vector<font_cmap_subtable_candidate> candidates;
    candidates.reserve(subtable_count);
    for (std::uint16_t index = 0; index < subtable_count; ++index) {
        const std::size_t record_offset = table_start + 4U + static_cast<std::size_t>(index) * 8U;
        const std::uint16_t platform_id = font_sfnt_read_u16_be(request.bytes, record_offset);
        const std::uint16_t encoding_id = font_sfnt_read_u16_be(request.bytes, record_offset + 2U);
        const std::uint32_t subtable_offset = font_sfnt_read_u32_be(request.bytes, record_offset + 4U);
        if (!font_cmap_record_is_unicode(platform_id, encoding_id)) {
            continue;
        }

        font_cmap_subtable_candidate candidate{
            .platform_id = platform_id,
            .encoding_id = encoding_id,
            .offset = subtable_offset,
            .record_index = index,
        };
        if (font_cmap_subtable_range_is_inside(cmap_table->length, subtable_offset, 2U)) {
            candidate.format = font_sfnt_read_u16_be(
                request.bytes,
                table_start + subtable_offset);
            candidate.has_format = true;
        }
        candidates.push_back(candidate);
    }

    if (candidates.empty()) {
        inspection.status = render_text_font_cmap_inspect_status::no_unicode_subtable;
        inspection.diagnostic = "cmap table has no Unicode encoding subtable";
        return inspection;
    }

    const font_cmap_subtable_candidate* selected = font_cmap_select_subtable(candidates);
    inspection.selected_platform_id = selected->platform_id;
    inspection.selected_encoding_id = selected->encoding_id;
    inspection.selected_format = selected->format;
    if (!selected->has_format) {
        inspection.status = render_text_font_cmap_inspect_status::truncated_subtable;
        inspection.diagnostic = "cmap Unicode subtable format field exceeds the cmap table";
        return inspection;
    }

    switch (selected->format) {
    case 4U:
        return font_cmap_parse_format4_coverage(request.bytes, *cmap_table, *selected, std::move(inspection));
    case 12U:
        return font_cmap_parse_format12_coverage(request.bytes, *cmap_table, *selected, std::move(inspection));
    default:
        inspection.status = render_text_font_cmap_inspect_status::unsupported_subtable_format;
        inspection.diagnostic = "cmap Unicode subtable format is not supported";
        return inspection;
    }
}

class basic_font_cmap_inspector final : public font_cmap_inspector_interface {
public:
    render_text_font_cmap_inspection inspect(
        const render_text_font_cmap_inspect_request& request) const override
    {
        return inspect_font_cmap_coverage(request);
    }
};

} // namespace quiz_vulkan::render
