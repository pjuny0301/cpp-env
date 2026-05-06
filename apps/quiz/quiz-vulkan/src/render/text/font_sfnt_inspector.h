#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

enum class render_text_font_sfnt_inspect_status {
    valid,
    missing_bytes,
    truncated_header,
    unsupported_scaler_tag,
    invalid_table_count,
    truncated_table_directory,
    duplicate_table_record,
    table_record_out_of_bounds,
    missing_required_table,
    malformed_name_table,
};

inline std::string render_text_font_sfnt_inspect_status_name(
    const render_text_font_sfnt_inspect_status status)
{
    switch (status) {
    case render_text_font_sfnt_inspect_status::valid:
        return "valid";
    case render_text_font_sfnt_inspect_status::missing_bytes:
        return "missing_bytes";
    case render_text_font_sfnt_inspect_status::truncated_header:
        return "truncated_header";
    case render_text_font_sfnt_inspect_status::unsupported_scaler_tag:
        return "unsupported_scaler_tag";
    case render_text_font_sfnt_inspect_status::invalid_table_count:
        return "invalid_table_count";
    case render_text_font_sfnt_inspect_status::truncated_table_directory:
        return "truncated_table_directory";
    case render_text_font_sfnt_inspect_status::duplicate_table_record:
        return "duplicate_table_record";
    case render_text_font_sfnt_inspect_status::table_record_out_of_bounds:
        return "table_record_out_of_bounds";
    case render_text_font_sfnt_inspect_status::missing_required_table:
        return "missing_required_table";
    case render_text_font_sfnt_inspect_status::malformed_name_table:
        return "malformed_name_table";
    }

    return "unknown";
}

struct render_text_font_sfnt_table_record {
    std::string tag;
    std::uint32_t checksum = 0;
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

struct render_text_font_sfnt_name_record {
    std::uint16_t platform_id = 0;
    std::uint16_t encoding_id = 0;
    std::uint16_t language_id = 0;
    std::uint16_t name_id = 0;
    std::string value;
};

struct render_text_font_sfnt_inspection {
    render_text_font_sfnt_inspect_status status =
        render_text_font_sfnt_inspect_status::missing_bytes;
    std::string source_label;
    std::string scaler_tag;
    std::string scaler_tag_label;
    std::uint16_t table_count = 0;
    std::vector<render_text_font_sfnt_table_record> tables;
    std::vector<std::string> missing_required_tables;
    std::vector<render_text_font_sfnt_name_record> names;
    std::string family_name;
    std::string full_name;
    std::string diagnostic;
    bool has_cmap = false;
    bool has_head = false;
    bool has_hhea = false;
    bool has_hmtx = false;
    bool has_maxp = false;
    bool has_name = false;
    bool has_glyf = false;
    bool has_loca = false;
    bool has_cff = false;

    bool ok() const
    {
        return status == render_text_font_sfnt_inspect_status::valid;
    }

    bool has_table(const std::string_view tag) const
    {
        return std::ranges::any_of(tables, [tag](const render_text_font_sfnt_table_record& table) {
            return table.tag == tag;
        });
    }
};

struct render_text_font_sfnt_inspect_request {
    std::span<const std::byte> bytes;
    std::string source_label;
};

class font_sfnt_inspector_interface {
public:
    virtual ~font_sfnt_inspector_interface() = default;

    virtual render_text_font_sfnt_inspection inspect(
        const render_text_font_sfnt_inspect_request& request) const = 0;
};

inline std::uint16_t font_sfnt_read_u16_be(
    const std::span<const std::byte> bytes,
    const std::size_t offset)
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(std::to_integer<unsigned char>(bytes[offset])) << 8U)
        | static_cast<std::uint16_t>(std::to_integer<unsigned char>(bytes[offset + 1U])));
}

inline std::uint32_t font_sfnt_read_u32_be(
    const std::span<const std::byte> bytes,
    const std::size_t offset)
{
    return (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset])) << 24U)
        | (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 1U])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 2U])) << 8U)
        | static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 3U]));
}

inline std::string font_sfnt_tag_at(
    const std::span<const std::byte> bytes,
    const std::size_t offset)
{
    std::string tag;
    tag.reserve(4U);
    for (std::size_t index = 0; index < 4U; ++index) {
        tag.push_back(static_cast<char>(std::to_integer<unsigned char>(bytes[offset + index])));
    }
    return tag;
}

inline bool font_sfnt_range_is_inside(
    const std::size_t byte_count,
    const std::uint32_t offset,
    const std::uint32_t length)
{
    return static_cast<std::uint64_t>(offset) <= static_cast<std::uint64_t>(byte_count)
        && static_cast<std::uint64_t>(length)
            <= static_cast<std::uint64_t>(byte_count) - static_cast<std::uint64_t>(offset);
}

inline char font_sfnt_hex_digit(const unsigned value)
{
    return static_cast<char>(value < 10U ? ('0' + value) : ('A' + (value - 10U)));
}

inline std::string font_sfnt_hex_label(const std::string_view bytes)
{
    std::string label = "0x";
    label.reserve(2U + bytes.size() * 2U);
    for (const char value : bytes) {
        const unsigned byte = static_cast<unsigned char>(value);
        label.push_back(font_sfnt_hex_digit(byte >> 4U));
        label.push_back(font_sfnt_hex_digit(byte & 0x0fU));
    }
    return label;
}

inline std::string font_sfnt_scaler_tag_label(const std::string_view scaler_tag)
{
    if (scaler_tag == std::string_view{"OTTO", 4U}) {
        return "OTTO";
    }
    if (scaler_tag == std::string_view{"true", 4U}) {
        return "true";
    }
    if (scaler_tag == std::string_view{"typ1", 4U}) {
        return "typ1";
    }
    if (scaler_tag == std::string_view{"\0\1\0\0", 4U}) {
        return "0x00010000";
    }
    return font_sfnt_hex_label(scaler_tag);
}

inline bool font_sfnt_scaler_tag_is_supported(const std::string_view scaler_tag)
{
    return scaler_tag == std::string_view{"\0\1\0\0", 4U}
        || scaler_tag == std::string_view{"OTTO", 4U}
        || scaler_tag == std::string_view{"true", 4U}
        || scaler_tag == std::string_view{"typ1", 4U};
}

inline std::vector<std::string> font_sfnt_required_tables_for_scaler(
    const std::string_view scaler_tag)
{
    if (scaler_tag == std::string_view{"OTTO", 4U}) {
        return {"CFF ", "cmap", "head", "hhea", "hmtx", "maxp", "name"};
    }
    if (scaler_tag == std::string_view{"typ1", 4U}) {
        return {"cmap", "head", "hhea", "hmtx", "maxp", "name", "typ1"};
    }
    return {"cmap", "glyf", "head", "hhea", "hmtx", "loca", "maxp", "name"};
}

inline const render_text_font_sfnt_table_record* font_sfnt_find_table(
    const render_text_font_sfnt_inspection& inspection,
    const std::string_view tag)
{
    const auto match = std::ranges::find_if(
        inspection.tables,
        [tag](const render_text_font_sfnt_table_record& table) {
            return table.tag == tag;
        });
    return match == inspection.tables.end() ? nullptr : &(*match);
}

inline bool font_sfnt_decode_utf16be_name(
    const std::span<const std::byte> bytes,
    std::string& decoded)
{
    if (bytes.size() % 2U != 0U) {
        return false;
    }

    decoded.clear();
    decoded.reserve(bytes.size() / 2U);
    for (std::size_t offset = 0; offset < bytes.size(); offset += 2U) {
        const std::uint16_t unit = font_sfnt_read_u16_be(bytes, offset);
        if (unit >= 0x20U && unit <= 0x7eU) {
            decoded.push_back(static_cast<char>(unit));
        } else if (unit == 0U) {
            decoded.push_back('\0');
        } else {
            decoded.push_back('?');
        }
    }
    return true;
}

inline bool font_sfnt_decode_mac_ascii_name(
    const std::span<const std::byte> bytes,
    std::string& decoded)
{
    decoded.clear();
    decoded.reserve(bytes.size());
    for (const std::byte value : bytes) {
        const unsigned char byte = std::to_integer<unsigned char>(value);
        decoded.push_back(byte >= 0x20U && byte <= 0x7eU ? static_cast<char>(byte) : '?');
    }
    return true;
}

inline bool font_sfnt_decode_name_value(
    const std::uint16_t platform_id,
    const std::uint16_t encoding_id,
    const std::span<const std::byte> bytes,
    std::string& decoded)
{
    if (platform_id == 0U) {
        return font_sfnt_decode_utf16be_name(bytes, decoded);
    }
    if (platform_id == 3U && (encoding_id == 1U || encoding_id == 10U)) {
        return font_sfnt_decode_utf16be_name(bytes, decoded);
    }
    if (platform_id == 1U && encoding_id == 0U) {
        return font_sfnt_decode_mac_ascii_name(bytes, decoded);
    }

    decoded.clear();
    return true;
}

inline int font_sfnt_name_record_preference(const render_text_font_sfnt_name_record& record)
{
    if (record.platform_id == 3U && record.language_id == 0x0409U) {
        return 40;
    }
    if (record.platform_id == 3U) {
        return 30;
    }
    if (record.platform_id == 0U) {
        return 20;
    }
    if (record.platform_id == 1U) {
        return 10;
    }
    return 0;
}

inline std::string font_sfnt_select_name(
    const std::vector<render_text_font_sfnt_name_record>& records,
    const std::uint16_t name_id)
{
    const render_text_font_sfnt_name_record* selected = nullptr;
    int selected_score = -1;
    for (const render_text_font_sfnt_name_record& record : records) {
        if (record.name_id != name_id || record.value.empty()) {
            continue;
        }
        const int score = font_sfnt_name_record_preference(record);
        if (score > selected_score) {
            selected = &record;
            selected_score = score;
        }
    }
    return selected == nullptr ? std::string{} : selected->value;
}

inline bool font_sfnt_parse_name_table(
    const std::span<const std::byte> bytes,
    const render_text_font_sfnt_table_record& table,
    render_text_font_sfnt_inspection& inspection)
{
    if (table.length < 6U || !font_sfnt_range_is_inside(bytes.size(), table.offset, table.length)) {
        inspection.diagnostic = "SFNT name table is too small";
        return false;
    }

    const std::size_t table_start = table.offset;
    const std::size_t table_end = table_start + table.length;
    const std::uint16_t format = font_sfnt_read_u16_be(bytes, table_start);
    const std::uint16_t record_count = font_sfnt_read_u16_be(bytes, table_start + 2U);
    const std::uint16_t string_offset = font_sfnt_read_u16_be(bytes, table_start + 4U);
    if (format > 1U) {
        inspection.diagnostic = "SFNT name table format is not supported";
        return false;
    }
    if (string_offset > table.length) {
        inspection.diagnostic = "SFNT name table string storage offset is outside the table";
        return false;
    }

    const std::size_t records_start = table_start + 6U;
    const std::size_t records_size = static_cast<std::size_t>(record_count) * 12U;
    if (records_size > table.length - 6U || records_start + records_size > table_end) {
        inspection.diagnostic = "SFNT name table records exceed the table bounds";
        return false;
    }

    const std::size_t storage_start = table_start + string_offset;
    for (std::uint16_t index = 0; index < record_count; ++index) {
        const std::size_t record_offset = records_start + static_cast<std::size_t>(index) * 12U;
        const std::uint16_t platform_id = font_sfnt_read_u16_be(bytes, record_offset);
        const std::uint16_t encoding_id = font_sfnt_read_u16_be(bytes, record_offset + 2U);
        const std::uint16_t language_id = font_sfnt_read_u16_be(bytes, record_offset + 4U);
        const std::uint16_t name_id = font_sfnt_read_u16_be(bytes, record_offset + 6U);
        const std::uint16_t length = font_sfnt_read_u16_be(bytes, record_offset + 8U);
        const std::uint16_t offset = font_sfnt_read_u16_be(bytes, record_offset + 10U);

        if (static_cast<std::size_t>(offset) > table.length - string_offset
            || static_cast<std::size_t>(length) > table.length - string_offset - offset) {
            inspection.diagnostic = "SFNT name record string range exceeds the name table";
            return false;
        }

        std::string value;
        const std::span<const std::byte> encoded_name =
            bytes.subspan(storage_start + offset, length);
        if (!font_sfnt_decode_name_value(platform_id, encoding_id, encoded_name, value)) {
            inspection.diagnostic = "SFNT name record string encoding is malformed";
            return false;
        }
        if (!value.empty()) {
            inspection.names.push_back(render_text_font_sfnt_name_record{
                .platform_id = platform_id,
                .encoding_id = encoding_id,
                .language_id = language_id,
                .name_id = name_id,
                .value = std::move(value),
            });
        }
    }

    inspection.family_name = font_sfnt_select_name(inspection.names, 1U);
    inspection.full_name = font_sfnt_select_name(inspection.names, 4U);
    return true;
}

inline void font_sfnt_record_table_presence(render_text_font_sfnt_inspection& inspection)
{
    inspection.has_cmap = inspection.has_table("cmap");
    inspection.has_head = inspection.has_table("head");
    inspection.has_hhea = inspection.has_table("hhea");
    inspection.has_hmtx = inspection.has_table("hmtx");
    inspection.has_maxp = inspection.has_table("maxp");
    inspection.has_name = inspection.has_table("name");
    inspection.has_glyf = inspection.has_table("glyf");
    inspection.has_loca = inspection.has_table("loca");
    inspection.has_cff = inspection.has_table("CFF ");
}

inline render_text_font_sfnt_inspection inspect_font_sfnt_bytes(
    const std::span<const std::byte> bytes,
    std::string source_label = {})
{
    render_text_font_sfnt_inspection inspection{
        .source_label = std::move(source_label),
    };

    if (bytes.empty()) {
        inspection.status = render_text_font_sfnt_inspect_status::missing_bytes;
        inspection.diagnostic = "font byte buffer is empty";
        return inspection;
    }
    if (bytes.size() < 12U) {
        inspection.status = render_text_font_sfnt_inspect_status::truncated_header;
        inspection.diagnostic = "SFNT header requires at least 12 bytes";
        return inspection;
    }

    inspection.scaler_tag = font_sfnt_tag_at(bytes, 0U);
    inspection.scaler_tag_label = font_sfnt_scaler_tag_label(inspection.scaler_tag);
    if (!font_sfnt_scaler_tag_is_supported(inspection.scaler_tag)) {
        inspection.status = render_text_font_sfnt_inspect_status::unsupported_scaler_tag;
        inspection.diagnostic = "SFNT scaler tag is not supported: " + inspection.scaler_tag_label;
        return inspection;
    }

    inspection.table_count = font_sfnt_read_u16_be(bytes, 4U);
    if (inspection.table_count == 0U) {
        inspection.status = render_text_font_sfnt_inspect_status::invalid_table_count;
        inspection.diagnostic = "SFNT table directory is empty";
        return inspection;
    }

    const std::size_t directory_bytes = static_cast<std::size_t>(inspection.table_count) * 16U;
    if (directory_bytes > bytes.size() - 12U) {
        inspection.status = render_text_font_sfnt_inspect_status::truncated_table_directory;
        inspection.diagnostic = "SFNT table directory exceeds the font byte buffer";
        return inspection;
    }

    inspection.tables.reserve(inspection.table_count);
    for (std::uint16_t index = 0; index < inspection.table_count; ++index) {
        const std::size_t record_offset = 12U + static_cast<std::size_t>(index) * 16U;
        render_text_font_sfnt_table_record table{
            .tag = font_sfnt_tag_at(bytes, record_offset),
            .checksum = font_sfnt_read_u32_be(bytes, record_offset + 4U),
            .offset = font_sfnt_read_u32_be(bytes, record_offset + 8U),
            .length = font_sfnt_read_u32_be(bytes, record_offset + 12U),
        };

        if (inspection.has_table(table.tag)) {
            inspection.status = render_text_font_sfnt_inspect_status::duplicate_table_record;
            inspection.diagnostic = "SFNT table directory has duplicate table tag: " + table.tag;
            return inspection;
        }
        if (!font_sfnt_range_is_inside(bytes.size(), table.offset, table.length)) {
            inspection.status = render_text_font_sfnt_inspect_status::table_record_out_of_bounds;
            inspection.diagnostic = "SFNT table range exceeds the font byte buffer: " + table.tag;
            inspection.tables.push_back(std::move(table));
            return inspection;
        }

        inspection.tables.push_back(std::move(table));
    }

    font_sfnt_record_table_presence(inspection);
    for (const std::string& required_table : font_sfnt_required_tables_for_scaler(inspection.scaler_tag)) {
        if (!inspection.has_table(required_table)) {
            inspection.missing_required_tables.push_back(required_table);
        }
    }
    if (!inspection.missing_required_tables.empty()) {
        inspection.status = render_text_font_sfnt_inspect_status::missing_required_table;
        inspection.diagnostic = "SFNT font is missing required table: "
            + inspection.missing_required_tables.front();
        return inspection;
    }

    const render_text_font_sfnt_table_record* name_table = font_sfnt_find_table(inspection, "name");
    if (name_table != nullptr && !font_sfnt_parse_name_table(bytes, *name_table, inspection)) {
        inspection.status = render_text_font_sfnt_inspect_status::malformed_name_table;
        return inspection;
    }

    inspection.status = render_text_font_sfnt_inspect_status::valid;
    inspection.diagnostic = "valid SFNT font bytes";
    return inspection;
}

class basic_font_sfnt_inspector final : public font_sfnt_inspector_interface {
public:
    render_text_font_sfnt_inspection inspect(
        const render_text_font_sfnt_inspect_request& request) const override
    {
        return inspect_font_sfnt_bytes(request.bytes, request.source_label);
    }
};

} // namespace quiz_vulkan::render
