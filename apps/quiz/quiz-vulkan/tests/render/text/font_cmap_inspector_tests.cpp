#include "render/text/font_cmap_inspector.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct sfnt_fixture_table {
    std::string tag;
    std::vector<std::byte> bytes;
};

struct cmap_format4_fixture_range {
    std::uint16_t first_codepoint = 0;
    std::uint16_t last_codepoint = 0;
};

struct cmap_format12_fixture_range {
    std::uint32_t first_codepoint = 0;
    std::uint32_t last_codepoint = 0;
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

void patch_u16_be(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = std::byte{static_cast<unsigned char>((value >> 8U) & 0xffU)};
    bytes[offset + 1U] = std::byte{static_cast<unsigned char>(value & 0xffU)};
}

void patch_u32_be(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset] = std::byte{static_cast<unsigned char>((value >> 24U) & 0xffU)};
    bytes[offset + 1U] = std::byte{static_cast<unsigned char>((value >> 16U) & 0xffU)};
    bytes[offset + 2U] = std::byte{static_cast<unsigned char>((value >> 8U) & 0xffU)};
    bytes[offset + 3U] = std::byte{static_cast<unsigned char>(value & 0xffU)};
}

std::vector<std::byte> minimal_table_bytes()
{
    return {std::byte{0}};
}

std::vector<std::byte> make_empty_name_table()
{
    std::vector<std::byte> bytes;
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 6U);
    return bytes;
}

std::vector<sfnt_fixture_table> required_truetype_tables(std::vector<std::byte> cmap_table)
{
    return {
        sfnt_fixture_table{.tag = "cmap", .bytes = std::move(cmap_table)},
        sfnt_fixture_table{.tag = "glyf", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "head", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "hhea", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "hmtx", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "loca", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "maxp", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "name", .bytes = make_empty_name_table()},
    };
}

std::vector<sfnt_fixture_table> required_truetype_tables_without_cmap()
{
    return {
        sfnt_fixture_table{.tag = "glyf", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "head", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "hhea", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "hmtx", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "loca", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "maxp", .bytes = minimal_table_bytes()},
        sfnt_fixture_table{.tag = "name", .bytes = make_empty_name_table()},
    };
}

std::vector<std::byte> make_sfnt_bytes(
    std::string_view scaler_tag,
    const std::vector<sfnt_fixture_table>& tables)
{
    require(
        tables.size() <= static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()),
        "SFNT fixture table count fits u16");

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
            bytes[record_offset + tag_index] =
                std::byte{static_cast<unsigned char>(table.tag[tag_index])};
        }
        patch_u32_be(bytes, record_offset + 4U, 0U);
        patch_u32_be(bytes, record_offset + 8U, static_cast<std::uint32_t>(table_offset));
        patch_u32_be(bytes, record_offset + 12U, static_cast<std::uint32_t>(table.bytes.size()));

        bytes.insert(bytes.end(), table.bytes.begin(), table.bytes.end());
    }

    return bytes;
}

std::vector<std::byte> wrap_cmap_subtable(
    std::uint16_t platform_id,
    std::uint16_t encoding_id,
    const std::vector<std::byte>& subtable)
{
    std::vector<std::byte> bytes;
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 1U);
    append_u16_be(bytes, platform_id);
    append_u16_be(bytes, encoding_id);
    append_u32_be(bytes, 12U);
    bytes.insert(bytes.end(), subtable.begin(), subtable.end());
    return bytes;
}

std::vector<std::byte> make_format4_subtable(std::vector<cmap_format4_fixture_range> ranges)
{
    ranges.push_back(cmap_format4_fixture_range{
        .first_codepoint = 0xffffU,
        .last_codepoint = 0xffffU,
    });

    const std::uint16_t segment_count = static_cast<std::uint16_t>(ranges.size());
    std::vector<std::byte> bytes;
    append_u16_be(bytes, 4U);
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, static_cast<std::uint16_t>(segment_count * 2U));
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 0U);
    for (const cmap_format4_fixture_range& range : ranges) {
        append_u16_be(bytes, range.last_codepoint);
    }
    append_u16_be(bytes, 0U);
    for (const cmap_format4_fixture_range& range : ranges) {
        append_u16_be(bytes, range.first_codepoint);
    }
    for (std::uint16_t index = 0; index < segment_count; ++index) {
        append_u16_be(bytes, 1U);
    }
    for (std::uint16_t index = 0; index < segment_count; ++index) {
        append_u16_be(bytes, 0U);
    }
    patch_u16_be(bytes, 2U, static_cast<std::uint16_t>(bytes.size()));
    return bytes;
}

std::vector<std::byte> make_format12_subtable(std::vector<cmap_format12_fixture_range> ranges)
{
    std::vector<std::byte> bytes;
    append_u16_be(bytes, 12U);
    append_u16_be(bytes, 0U);
    append_u32_be(bytes, 0U);
    append_u32_be(bytes, 0U);
    append_u32_be(bytes, static_cast<std::uint32_t>(ranges.size()));
    for (const cmap_format12_fixture_range& range : ranges) {
        append_u32_be(bytes, range.first_codepoint);
        append_u32_be(bytes, range.last_codepoint);
        append_u32_be(bytes, 1U);
    }
    patch_u32_be(bytes, 4U, static_cast<std::uint32_t>(bytes.size()));
    return bytes;
}

std::vector<std::byte> make_format6_subtable()
{
    std::vector<std::byte> bytes;
    append_u16_be(bytes, 6U);
    append_u16_be(bytes, 10U);
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 0U);
    append_u16_be(bytes, 0U);
    return bytes;
}

quiz_vulkan::render::render_text_font_cmap_inspection inspect_cmap_fixture(
    const std::vector<std::byte>& cmap_table)
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> sfnt_bytes = make_sfnt_bytes(
        std::string_view{"\0\1\0\0", 4U},
        required_truetype_tables(cmap_table));
    const render_text_font_sfnt_inspection sfnt = inspect_font_sfnt_bytes(sfnt_bytes);
    require(sfnt.ok(), "cmap test fixture has a valid SFNT table directory");
    return inspect_font_cmap_coverage(render_text_font_cmap_inspect_request{
        .bytes = std::span<const std::byte>{sfnt_bytes},
        .sfnt = sfnt,
    });
}

void test_cmap_format4_reports_bmp_coverage_ranges()
{
    using namespace quiz_vulkan::render;

    const render_text_font_cmap_inspection inspected = inspect_cmap_fixture(
        wrap_cmap_subtable(
            3U,
            1U,
            make_format4_subtable({
                cmap_format4_fixture_range{.first_codepoint = 0x0041U, .last_codepoint = 0x005aU},
                cmap_format4_fixture_range{.first_codepoint = 0x0061U, .last_codepoint = 0x007aU},
            })));

    require(inspected.ok(), "format 4 cmap fixture reports valid coverage");
    require(inspected.status == render_text_font_cmap_inspect_status::valid, "format 4 status is valid");
    require(inspected.selected_platform_id == 3U, "format 4 selects Windows platform");
    require(inspected.selected_encoding_id == 1U, "format 4 selects Unicode BMP encoding");
    require(inspected.selected_format == 4U, "format 4 records selected format");
    require(inspected.ranges.size() == 2U, "format 4 exposes BMP coverage ranges");
    require(inspected.ranges.front().contains(static_cast<char32_t>(0x0041U)), "range contains first Latin codepoint");
    require(inspected.supports_codepoint(static_cast<char32_t>(0x005aU)), "format 4 supports Latin uppercase endpoint");
    require(!inspected.supports_codepoint(static_cast<char32_t>(0x0040U)), "format 4 rejects codepoint before coverage");
    require(!inspected.supports_codepoint(static_cast<char32_t>(0x1f600U)), "format 4 does not cover non-BMP codepoints");
    require(
        inspected.diagnostic == "valid cmap format 4 Unicode coverage",
        "format 4 coverage diagnostic is stable");
}

void test_cmap_format4_reports_hangul_bmp_coverage()
{
    using namespace quiz_vulkan::render;

    const render_text_font_cmap_inspection inspected = inspect_cmap_fixture(
        wrap_cmap_subtable(
            3U,
            1U,
            make_format4_subtable({
                cmap_format4_fixture_range{.first_codepoint = 0xac00U, .last_codepoint = 0xd7a3U},
            })));

    require(inspected.ok(), "format 4 Hangul cmap fixture reports valid coverage");
    require(inspected.ranges.size() == 1U, "Hangul coverage is reported as one range");
    require(inspected.supports_codepoint(static_cast<char32_t>(0xac00U)), "Hangul coverage includes first syllable");
    require(inspected.supports_codepoint(static_cast<char32_t>(0xb098U)), "Hangul coverage includes interior syllable");
    require(inspected.supports_codepoint(static_cast<char32_t>(0xd7a3U)), "Hangul coverage includes last syllable");
    require(!inspected.supports_codepoint(static_cast<char32_t>(0xd7a4U)), "Hangul coverage excludes next codepoint");
}

void test_cmap_format12_reports_non_bmp_coverage()
{
    using namespace quiz_vulkan::render;

    const render_text_font_cmap_inspection inspected = inspect_cmap_fixture(
        wrap_cmap_subtable(
            3U,
            10U,
            make_format12_subtable({
                cmap_format12_fixture_range{.first_codepoint = 0x1f600U, .last_codepoint = 0x1f64fU},
            })));

    require(inspected.ok(), "format 12 cmap fixture reports valid coverage");
    require(inspected.selected_platform_id == 3U, "format 12 selects Windows platform");
    require(inspected.selected_encoding_id == 10U, "format 12 selects full Unicode encoding");
    require(inspected.selected_format == 12U, "format 12 records selected format");
    require(inspected.ranges.size() == 1U, "format 12 exposes grouped coverage range");
    require(inspected.supports_codepoint(static_cast<char32_t>(0x1f600U)), "format 12 supports first non-BMP codepoint");
    require(inspected.supports_codepoint(static_cast<char32_t>(0x1f64fU)), "format 12 supports last non-BMP codepoint");
    require(!inspected.supports_codepoint(static_cast<char32_t>(0x1f680U)), "format 12 rejects codepoint outside group");
    require(
        inspected.diagnostic == "valid cmap format 12 Unicode coverage",
        "format 12 coverage diagnostic is stable");
}

void test_cmap_reports_missing_cmap_table()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> sfnt_bytes = make_sfnt_bytes(
        std::string_view{"\0\1\0\0", 4U},
        required_truetype_tables_without_cmap());
    const render_text_font_sfnt_inspection sfnt = inspect_font_sfnt_bytes(sfnt_bytes);
    require(
        sfnt.status == render_text_font_sfnt_inspect_status::missing_required_table,
        "missing cmap SFNT fixture reports missing required table");

    const render_text_font_cmap_inspection inspected = inspect_font_cmap_coverage(
        render_text_font_cmap_inspect_request{
            .bytes = std::span<const std::byte>{sfnt_bytes},
            .sfnt = sfnt,
        });

    require(!inspected.ok(), "missing cmap table is not valid coverage");
    require(
        inspected.status == render_text_font_cmap_inspect_status::missing_cmap_table,
        "missing cmap table reports cmap status");
    require(inspected.diagnostic == "SFNT font has no cmap table", "missing cmap diagnostic is stable");
}

void test_cmap_reports_malformed_and_truncated_tables()
{
    using namespace quiz_vulkan::render;

    const render_text_font_cmap_inspection malformed = inspect_cmap_fixture({
        std::byte{0},
        std::byte{0},
        std::byte{0},
        std::byte{1},
    });
    require(!malformed.ok(), "malformed cmap table is not valid coverage");
    require(
        malformed.status == render_text_font_cmap_inspect_status::malformed_table,
        "truncated cmap encoding records report malformed table");
    require(
        malformed.diagnostic == "cmap encoding records exceed the table bounds",
        "malformed cmap diagnostic is stable");

    std::vector<std::byte> truncated_subtable;
    append_u16_be(truncated_subtable, 0U);
    append_u16_be(truncated_subtable, 1U);
    append_u16_be(truncated_subtable, 3U);
    append_u16_be(truncated_subtable, 1U);
    append_u32_be(truncated_subtable, 12U);
    append_u16_be(truncated_subtable, 4U);
    append_u16_be(truncated_subtable, 40U);
    append_u16_be(truncated_subtable, 0U);

    const render_text_font_cmap_inspection truncated = inspect_cmap_fixture(truncated_subtable);
    require(!truncated.ok(), "truncated cmap subtable is not valid coverage");
    require(
        truncated.status == render_text_font_cmap_inspect_status::truncated_subtable,
        "truncated format 4 subtable reports truncated subtable");
    require(truncated.selected_format == 4U, "truncated subtable records selected format");
}

void test_cmap_reports_no_unicode_subtable()
{
    using namespace quiz_vulkan::render;

    const render_text_font_cmap_inspection inspected = inspect_cmap_fixture(
        wrap_cmap_subtable(
            1U,
            0U,
            make_format4_subtable({
                cmap_format4_fixture_range{.first_codepoint = 0x0041U, .last_codepoint = 0x005aU},
            })));

    require(!inspected.ok(), "non-Unicode cmap subtable is not valid Unicode coverage");
    require(
        inspected.status == render_text_font_cmap_inspect_status::no_unicode_subtable,
        "non-Unicode cmap subtable reports no Unicode subtable");
    require(
        inspected.diagnostic == "cmap table has no Unicode encoding subtable",
        "no Unicode subtable diagnostic is stable");
}

void test_cmap_reports_unsupported_subtable_format()
{
    using namespace quiz_vulkan::render;

    const render_text_font_cmap_inspection inspected = inspect_cmap_fixture(
        wrap_cmap_subtable(3U, 1U, make_format6_subtable()));

    require(!inspected.ok(), "unsupported Unicode cmap format is not valid coverage");
    require(
        inspected.status == render_text_font_cmap_inspect_status::unsupported_subtable_format,
        "unsupported Unicode cmap format reports unsupported status");
    require(inspected.selected_platform_id == 3U, "unsupported subtable records selected platform");
    require(inspected.selected_encoding_id == 1U, "unsupported subtable records selected encoding");
    require(inspected.selected_format == 6U, "unsupported subtable records selected format");
    require(
        inspected.diagnostic == "cmap Unicode subtable format is not supported",
        "unsupported subtable diagnostic is stable");
}

void test_cmap_reports_unsupported_table_version()
{
    using namespace quiz_vulkan::render;

    std::vector<std::byte> cmap_table;
    append_u16_be(cmap_table, 1U);
    append_u16_be(cmap_table, 0U);
    const render_text_font_cmap_inspection inspected = inspect_cmap_fixture(cmap_table);

    require(!inspected.ok(), "unsupported cmap version is not valid coverage");
    require(
        inspected.status == render_text_font_cmap_inspect_status::unsupported_version,
        "unsupported cmap version reports unsupported version status");
    require(inspected.diagnostic == "cmap table version is not supported", "unsupported version diagnostic is stable");
}

} // namespace

int main()
{
    test_cmap_format4_reports_bmp_coverage_ranges();
    test_cmap_format4_reports_hangul_bmp_coverage();
    test_cmap_format12_reports_non_bmp_coverage();
    test_cmap_reports_missing_cmap_table();
    test_cmap_reports_malformed_and_truncated_tables();
    test_cmap_reports_no_unicode_subtable();
    test_cmap_reports_unsupported_subtable_format();
    test_cmap_reports_unsupported_table_version();
    return 0;
}
