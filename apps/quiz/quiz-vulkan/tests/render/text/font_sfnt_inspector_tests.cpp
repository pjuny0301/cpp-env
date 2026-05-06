#include "render/text/font_sfnt_inspector.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct sfnt_fixture_table {
    std::string tag;
    std::vector<std::byte> bytes;
};

struct name_fixture_record {
    std::uint16_t platform_id = 3;
    std::uint16_t encoding_id = 1;
    std::uint16_t language_id = 0x0409;
    std::uint16_t name_id = 0;
    std::string value;
};

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

void append_byte(std::vector<std::byte>& bytes, unsigned char value)
{
    bytes.push_back(std::byte{value});
}

void append_tag(std::vector<std::byte>& bytes, std::string_view tag)
{
    require(tag.size() == 4U, "SFNT fixture tags are four bytes");
    for (const char value : tag) {
        append_byte(bytes, static_cast<unsigned char>(value));
    }
}

void append_u16_be(std::vector<std::byte>& bytes, std::uint16_t value)
{
    append_byte(bytes, static_cast<unsigned char>((value >> 8U) & 0xffU));
    append_byte(bytes, static_cast<unsigned char>(value & 0xffU));
}

void append_u32_be(std::vector<std::byte>& bytes, std::uint32_t value)
{
    append_byte(bytes, static_cast<unsigned char>((value >> 24U) & 0xffU));
    append_byte(bytes, static_cast<unsigned char>((value >> 16U) & 0xffU));
    append_byte(bytes, static_cast<unsigned char>((value >> 8U) & 0xffU));
    append_byte(bytes, static_cast<unsigned char>(value & 0xffU));
}

void patch_u32_be(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset] = std::byte{static_cast<unsigned char>((value >> 24U) & 0xffU)};
    bytes[offset + 1U] = std::byte{static_cast<unsigned char>((value >> 16U) & 0xffU)};
    bytes[offset + 2U] = std::byte{static_cast<unsigned char>((value >> 8U) & 0xffU)};
    bytes[offset + 3U] = std::byte{static_cast<unsigned char>(value & 0xffU)};
}

std::vector<std::byte> encode_name_value(const name_fixture_record& record)
{
    std::vector<std::byte> encoded;
    if (record.platform_id == 3U || record.platform_id == 0U) {
        for (const char value : record.value) {
            append_u16_be(encoded, static_cast<std::uint16_t>(static_cast<unsigned char>(value)));
        }
        return encoded;
    }

    for (const char value : record.value) {
        append_byte(encoded, static_cast<unsigned char>(value));
    }
    return encoded;
}

std::vector<std::byte> make_name_table(std::vector<name_fixture_record> records)
{
    std::vector<std::vector<std::byte>> encoded_records;
    encoded_records.reserve(records.size());
    for (const name_fixture_record& record : records) {
        encoded_records.push_back(encode_name_value(record));
    }

    std::vector<std::byte> bytes;
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, static_cast<std::uint16_t>(records.size()));
    const std::uint16_t string_offset = static_cast<std::uint16_t>(6U + records.size() * 12U);
    append_u16_be(bytes, string_offset);

    std::uint16_t storage_offset = 0;
    for (std::size_t index = 0; index < records.size(); ++index) {
        const name_fixture_record& record = records[index];
        const std::vector<std::byte>& encoded = encoded_records[index];
        append_u16_be(bytes, record.platform_id);
        append_u16_be(bytes, record.encoding_id);
        append_u16_be(bytes, record.language_id);
        append_u16_be(bytes, record.name_id);
        append_u16_be(bytes, static_cast<std::uint16_t>(encoded.size()));
        append_u16_be(bytes, storage_offset);
        storage_offset = static_cast<std::uint16_t>(storage_offset + encoded.size());
    }

    for (const std::vector<std::byte>& encoded : encoded_records) {
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    }
    return bytes;
}

std::vector<std::byte> minimal_table_bytes()
{
    return {std::byte{0}};
}

std::vector<sfnt_fixture_table> required_truetype_tables(std::vector<std::byte> name_table)
{
    return {
        sfnt_fixture_table{.tag = "cmap", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "glyf", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "head", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "hhea", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "hmtx", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "loca", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "maxp", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "name", .bytes = std::move(name_table)},
    };
}

std::vector<sfnt_fixture_table> required_cff_tables(std::vector<std::byte> name_table)
{
    return {
        sfnt_fixture_table{.tag = "CFF ", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "cmap", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "head", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "hhea", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "hmtx", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "maxp", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "name", .bytes = std::move(name_table)},
    };
}

std::vector<std::byte> make_sfnt_bytes(
    std::string_view scaler_tag,
    const std::vector<sfnt_fixture_table>& tables)
{
    require(tables.size() <= static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()), "SFNT fixture table count fits u16");

    std::vector<std::byte> bytes;
    append_tag(bytes, scaler_tag);
    append_u16_be(bytes, static_cast<std::uint16_t>(tables.size()));
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 0U);

    const std::size_t directory_offset = bytes.size();
    bytes.resize(bytes.size() + tables.size() * 16U);

    for (std::size_t index = 0; index < tables.size(); ++index) {
        const sfnt_fixture_table& table = tables[index];
        while (bytes.size() % 4U != 0U) {
            append_byte(bytes, 0);
        }

        const std::size_t table_offset = bytes.size();
        const std::size_t record_offset = directory_offset + index * 16U;
        for (std::size_t tag_index = 0; tag_index < 4U; ++tag_index) {
            bytes[record_offset + tag_index] = std::byte{static_cast<unsigned char>(table.tag[tag_index])};
        }
        patch_u32_be(bytes, record_offset + 4U, 0U);
        patch_u32_be(bytes, record_offset + 8U, static_cast<std::uint32_t>(table_offset));
        patch_u32_be(bytes, record_offset + 12U, static_cast<std::uint32_t>(table.bytes.size()));

        bytes.insert(bytes.end(), table.bytes.begin(), table.bytes.end());
    }

    return bytes;
}

std::vector<std::byte> make_valid_name_table()
{
    return make_name_table({
        name_fixture_record{.platform_id = 1, .encoding_id = 0, .language_id = 0, .name_id = 1, .value = "Mac Family"},
        name_fixture_record{.platform_id = 3, .encoding_id = 1, .language_id = 0x0409, .name_id = 1, .value = "Fixture Sans"},
        name_fixture_record{.platform_id = 3, .encoding_id = 1, .language_id = 0x0409, .name_id = 4, .value = "Fixture Sans Regular"},
    });
}

std::vector<std::byte> make_out_of_bounds_table_directory()
{
    std::vector<std::byte> bytes;
    append_tag(bytes, std::string_view{"\0\1\0\0", 4U});
    append_u16_be(bytes, 1U);
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 0U);
    append_tag(bytes, "cmap");
    append_u32_be(bytes, 0U);
    append_u32_be(bytes, 1024U);
    append_u32_be(bytes, 4U);
    return bytes;
}

void test_inspector_accepts_truetype_sfnt_and_extracts_names()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_sfnt_bytes(
        std::string_view{"\0\1\0\0", 4U},
        required_truetype_tables(make_valid_name_table()));

    const basic_font_sfnt_inspector inspector;
    const render_text_font_sfnt_inspection inspected = inspector.inspect(
        render_text_font_sfnt_inspect_request{
            .bytes = std::span<const std::byte>{bytes},
            .source_label = "fixture.ttf",
        });

    require(inspected.ok(), "TrueType SFNT fixture inspects as valid");
    require(inspected.status == render_text_font_sfnt_inspect_status::valid, "valid TrueType fixture reports valid status");
    require(inspected.source_label == "fixture.ttf", "SFNT inspector preserves source label");
    require(inspected.scaler_tag_label == "0x00010000", "TrueType scaler tag label is stable");
    require(inspected.table_count == 8U, "TrueType fixture reports table count");
    require(inspected.has_cmap && inspected.has_glyf && inspected.has_loca, "TrueType fixture reports outline tables");
    require(!inspected.has_cff, "TrueType fixture does not report CFF table");
    require(inspected.has_table("name"), "SFNT table lookup reports name table");
    require(inspected.family_name == "Fixture Sans", "SFNT name table prefers Windows English family name");
    require(inspected.full_name == "Fixture Sans Regular", "SFNT name table extracts full font name");
    require(inspected.names.size() == 3U, "SFNT name table records decoded names");
    require(inspected.diagnostic == "valid SFNT font bytes", "valid SFNT diagnostic is stable");
}

void test_inspector_accepts_cff_opentype_required_tables()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_sfnt_bytes(
        "OTTO",
        required_cff_tables(make_valid_name_table()));
    const render_text_font_sfnt_inspection inspected = inspect_font_sfnt_bytes(bytes);

    require(inspected.ok(), "CFF OpenType SFNT fixture inspects as valid");
    require(inspected.scaler_tag_label == "OTTO", "OpenType CFF scaler tag label is stable");
    require(inspected.table_count == 7U, "OpenType CFF fixture reports table count");
    require(inspected.has_cff, "OpenType CFF fixture reports CFF table");
    require(!inspected.has_glyf && !inspected.has_loca, "OpenType CFF fixture does not require TrueType outlines");
    require(inspected.missing_required_tables.empty(), "OpenType CFF fixture satisfies required tables");
    require(inspected.family_name == "Fixture Sans", "OpenType CFF fixture extracts family name");
}

void test_inspector_reports_header_and_scaler_failures()
{
    using namespace quiz_vulkan::render;

    const render_text_font_sfnt_inspection empty = inspect_font_sfnt_bytes(std::span<const std::byte>{});
    require(!empty.ok(), "empty bytes do not inspect as SFNT");
    require(empty.status == render_text_font_sfnt_inspect_status::missing_bytes, "empty bytes report missing bytes");
    require(empty.diagnostic == "font byte buffer is empty", "empty bytes diagnostic is stable");

    const std::vector<std::byte> short_header{std::byte{'O'}, std::byte{'T'}, std::byte{'T'}, std::byte{'O'}};
    const render_text_font_sfnt_inspection truncated = inspect_font_sfnt_bytes(short_header);
    require(
        truncated.status == render_text_font_sfnt_inspect_status::truncated_header,
        "short bytes report truncated SFNT header");

    const std::vector<std::byte> unsupported = make_sfnt_bytes("BAD!", {});
    const render_text_font_sfnt_inspection bad_scaler = inspect_font_sfnt_bytes(unsupported);
    require(
        bad_scaler.status == render_text_font_sfnt_inspect_status::unsupported_scaler_tag,
        "unsupported scaler tag is rejected");
    require(bad_scaler.scaler_tag_label == "0x42414421", "unsupported scaler tag has stable hex label");

    const std::vector<std::byte> no_tables = make_sfnt_bytes("OTTO", {});
    const render_text_font_sfnt_inspection invalid_count = inspect_font_sfnt_bytes(no_tables);
    require(
        invalid_count.status == render_text_font_sfnt_inspect_status::invalid_table_count,
        "SFNT table count of zero is rejected");
}

void test_inspector_reports_directory_and_table_bounds()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> truncated_directory;
    append_tag(truncated_directory, "OTTO");
    append_u16_be(truncated_directory, 1U);
    append_u16_be(truncated_directory, 0U);
    append_u16_be(truncated_directory, 0U);
    append_u16_be(truncated_directory, 0U);
    const render_text_font_sfnt_inspection truncated = inspect_font_sfnt_bytes(truncated_directory);
    require(
        truncated.status == render_text_font_sfnt_inspect_status::truncated_table_directory,
        "truncated table directory is rejected");

    const render_text_font_sfnt_inspection out_of_bounds =
        inspect_font_sfnt_bytes(make_out_of_bounds_table_directory());
    require(
        out_of_bounds.status == render_text_font_sfnt_inspect_status::table_record_out_of_bounds,
        "table range beyond font bytes is rejected");
    require(out_of_bounds.tables.size() == 1U, "out-of-bounds result keeps offending table record");
    require(out_of_bounds.tables.front().tag == "cmap", "out-of-bounds diagnostic keeps table tag");

    const std::vector<std::byte> duplicate = make_sfnt_bytes(
        "OTTO",
        {
            sfnt_fixture_table{.tag = "cmap", .bytes = minimal_table_bytes()},
            sfnt_fixture_table{.tag = "cmap", .bytes = minimal_table_bytes()},
        });
    const render_text_font_sfnt_inspection duplicate_result = inspect_font_sfnt_bytes(duplicate);
    require(
        duplicate_result.status == render_text_font_sfnt_inspect_status::duplicate_table_record,
        "duplicate table tags are rejected");
}

void test_inspector_reports_required_table_policy()
{
    using namespace quiz_vulkan::render;

    std::vector<sfnt_fixture_table> tables = required_truetype_tables(make_valid_name_table());
    tables.erase(tables.begin());
    const render_text_font_sfnt_inspection inspected = inspect_font_sfnt_bytes(
        make_sfnt_bytes(std::string_view{"\0\1\0\0", 4U}, tables));

    require(!inspected.ok(), "missing required TrueType table rejects SFNT bytes");
    require(
        inspected.status == render_text_font_sfnt_inspect_status::missing_required_table,
        "missing required table reports policy status");
    require(inspected.missing_required_tables.size() == 1U, "missing table list records one table");
    require(inspected.missing_required_tables.front() == "cmap", "missing table list names cmap");
    require(inspected.diagnostic == "SFNT font is missing required table: cmap", "missing table diagnostic is stable");
}

void test_inspector_reports_malformed_name_table()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> malformed_name;
    append_u16_be(malformed_name, 0U);
    append_u16_be(malformed_name, 1U);
    append_u16_be(malformed_name, 6U);

    const render_text_font_sfnt_inspection inspected = inspect_font_sfnt_bytes(
        make_sfnt_bytes(std::string_view{"\0\1\0\0", 4U}, required_truetype_tables(malformed_name)));

    require(!inspected.ok(), "malformed name table rejects SFNT bytes");
    require(
        inspected.status == render_text_font_sfnt_inspect_status::malformed_name_table,
        "malformed name table reports name status");
    require(
        inspected.diagnostic == "SFNT name table records exceed the table bounds",
        "malformed name diagnostic is stable");
}

} // namespace

int main()
{
    test_inspector_accepts_truetype_sfnt_and_extracts_names();
    test_inspector_accepts_cff_opentype_required_tables();
    test_inspector_reports_header_and_scaler_failures();
    test_inspector_reports_directory_and_table_bounds();
    test_inspector_reports_required_table_policy();
    test_inspector_reports_malformed_name_table();
    return 0;
}
