#include "render/text/font_unicode_coverage.h"

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

std::vector<std::byte> make_font_bytes_with_cmap(std::vector<std::byte> cmap_table)
{
    return make_sfnt_bytes(
        std::string_view{"\0\1\0\0", 4U},
        required_truetype_tables(std::move(cmap_table)));
}

class valid_sfnt_without_cmap_inspector final
    : public quiz_vulkan::render::font_sfnt_inspector_interface {
public:
    quiz_vulkan::render::render_text_font_sfnt_inspection inspect(
        const quiz_vulkan::render::render_text_font_sfnt_inspect_request& request) const override
    {
        return quiz_vulkan::render::render_text_font_sfnt_inspection{
            .status = quiz_vulkan::render::render_text_font_sfnt_inspect_status::valid,
            .source_label = request.source_label,
            .scaler_tag = std::string{"\0\1\0\0", 4U},
            .scaler_tag_label = "0x00010000",
            .diagnostic = "valid injected SFNT without cmap",
        };
    }
};

void test_resolver_reports_format4_latin_and_hangul_coverage()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_font_bytes_with_cmap(
        wrap_cmap_subtable(
            3U,
            1U,
            make_format4_subtable({
                cmap_format4_fixture_range{.first_codepoint = 0x0041U, .last_codepoint = 0x005aU},
                cmap_format4_fixture_range{.first_codepoint = 0xac00U, .last_codepoint = 0xd7a3U},
            })));
    const basic_font_unicode_coverage_resolver resolver;
    const render_text_font_unicode_coverage_snapshot coverage = resolver.resolve(
        render_text_font_unicode_coverage_request{
            .bytes = std::span<const std::byte>{bytes},
            .source_label = "fixture-format4.ttf",
        });

    require(coverage.ok(), "format 4 coverage resolves as valid");
    require(
        coverage.status == render_text_font_unicode_coverage_status::valid,
        "format 4 coverage reports valid status");
    require(coverage.source_label == "fixture-format4.ttf", "coverage preserves raw source label");
    require(coverage.sfnt.ok(), "coverage exposes valid SFNT diagnostics");
    require(coverage.cmap.ok(), "coverage exposes valid cmap diagnostics");
    require(coverage.cmap.selected_format == 4U, "coverage records selected format 4 cmap");
    require(coverage.ranges.size() == 2U, "format 4 coverage exposes normalized ranges");
    require(coverage.supports_codepoint(U'A'), "format 4 coverage supports Latin codepoint");
    require(coverage.supports_codepoint(static_cast<char32_t>(0xb098U)), "format 4 coverage supports Hangul codepoint");
    require(!coverage.supports_codepoint(static_cast<char32_t>(0x1f600U)), "format 4 coverage excludes non-BMP codepoint");
    require(
        coverage.diagnostic == "valid cmap format 4 Unicode coverage",
        "valid coverage diagnostic is inherited from cmap inspector");
}

void test_resolver_reports_format12_non_bmp_coverage()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_font_bytes_with_cmap(
        wrap_cmap_subtable(
            3U,
            10U,
            make_format12_subtable({
                cmap_format12_fixture_range{.first_codepoint = 0x0041U, .last_codepoint = 0x005aU},
                cmap_format12_fixture_range{.first_codepoint = 0xac00U, .last_codepoint = 0xac02U},
                cmap_format12_fixture_range{.first_codepoint = 0x1f600U, .last_codepoint = 0x1f64fU},
            })));

    const render_text_font_unicode_coverage_snapshot coverage = resolve_font_unicode_coverage(
        render_text_font_unicode_coverage_request{
            .bytes = std::span<const std::byte>{bytes},
            .source_label = "fixture-format12.ttf",
        });

    require(coverage.ok(), "format 12 coverage resolves as valid");
    require(coverage.cmap.selected_format == 12U, "coverage records selected format 12 cmap");
    require(coverage.supports_codepoint(U'Z'), "format 12 coverage supports Latin codepoint");
    require(coverage.supports_codepoint(static_cast<char32_t>(0xac01U)), "format 12 coverage supports Hangul codepoint");
    require(coverage.supports_codepoint(static_cast<char32_t>(0x1f600U)), "format 12 coverage supports non-BMP codepoint");
    require(!coverage.supports_codepoint(static_cast<char32_t>(0xd800U)), "coverage rejects surrogate codepoints");
}

void test_resolver_accepts_loaded_font_source_bytes_result()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_font_bytes_with_cmap(
        wrap_cmap_subtable(
            3U,
            1U,
            make_format4_subtable({
                cmap_format4_fixture_range{.first_codepoint = 0x0041U, .last_codepoint = 0x005aU},
            })));
    const render_text_font_source_bytes_load_result loaded{
        .status = render_text_font_source_bytes_load_status::loaded,
        .source = font_source_resolution{
            .face_id = 47,
            .family = "Fixture Sans",
            .source_uri = "fonts/fixture.ttf",
            .kind = render_text_font_source_kind::file_path,
            .resolved_location = "fonts/fixture.ttf",
            .can_attempt_load = true,
        },
        .readiness = font_source_bytes_readiness{
            .face_id = 47,
            .cache_key = "fonts/fixture.ttf",
            .source_kind = render_text_font_source_kind::file_path,
            .status = render_text_font_source_bytes_status::pending_file_load,
            .cacheable = true,
            .requires_io = true,
        },
        .cache_key = "fonts/fixture.ttf",
        .resolved_path = "resolved/fixture.ttf",
        .bytes = bytes,
    };

    const basic_font_unicode_coverage_resolver resolver;
    const render_text_font_unicode_coverage_snapshot coverage = resolver.resolve(loaded);

    require(coverage.ok(), "loaded font source bytes resolve as coverage");
    require(coverage.source_label == "resolved/fixture.ttf", "loaded coverage prefers resolved path as source label");
    require(coverage.supports_codepoint(U'A'), "loaded coverage supports expected codepoint");
}

void test_resolver_reports_missing_bytes()
{
    using namespace quiz_vulkan::render;

    const basic_font_unicode_coverage_resolver resolver;
    const render_text_font_unicode_coverage_snapshot coverage = resolver.resolve(
        render_text_font_unicode_coverage_request{
            .bytes = {},
            .source_label = "missing.ttf",
        });

    require(!coverage.ok(), "missing bytes do not resolve as valid coverage");
    require(
        coverage.status == render_text_font_unicode_coverage_status::missing_bytes,
        "missing bytes report missing bytes status");
    require(coverage.source_label == "missing.ttf", "missing bytes preserve source label");
    require(coverage.ranges.empty(), "missing bytes expose no coverage ranges");
    require(coverage.diagnostic == "font byte buffer is empty", "missing bytes diagnostic is stable");

    const render_text_font_source_bytes_load_result missing_load{
        .status = render_text_font_source_bytes_load_status::missing_bytes,
        .source = font_source_resolution{
            .family = "Missing Sans",
            .source_uri = "fonts/missing.ttf",
            .kind = render_text_font_source_kind::file_path,
            .resolved_location = "fonts/missing.ttf",
            .can_attempt_load = true,
        },
        .cache_key = "fonts/missing.ttf",
        .diagnostic = "font source file does not exist",
    };
    const render_text_font_unicode_coverage_snapshot missing_loaded = resolver.resolve(missing_load);
    require(
        missing_loaded.status == render_text_font_unicode_coverage_status::missing_bytes,
        "missing loaded bytes report missing bytes status");
    require(
        missing_loaded.diagnostic.find("font source file does not exist") != std::string::npos,
        "missing loaded bytes preserve loader diagnostic");
}

void test_resolver_propagates_invalid_sfnt()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes{std::byte{'O'}, std::byte{'T'}, std::byte{'T'}, std::byte{'O'}};
    const render_text_font_unicode_coverage_snapshot coverage = resolve_font_unicode_coverage(
        render_text_font_unicode_coverage_request{
            .bytes = std::span<const std::byte>{bytes},
            .source_label = "broken.otf",
        });

    require(!coverage.ok(), "invalid SFNT bytes do not resolve as coverage");
    require(
        coverage.status == render_text_font_unicode_coverage_status::sfnt_invalid,
        "invalid SFNT bytes report SFNT invalid status");
    require(
        coverage.sfnt.status == render_text_font_sfnt_inspect_status::truncated_header,
        "coverage preserves SFNT inspection status");
    require(
        coverage.diagnostic.find("truncated_header") != std::string::npos,
        "coverage diagnostic names SFNT status");
}

void test_resolver_preserves_unsupported_cmap_status()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes = make_font_bytes_with_cmap(
        wrap_cmap_subtable(3U, 1U, make_format6_subtable()));
    const render_text_font_unicode_coverage_snapshot coverage = resolve_font_unicode_coverage(
        render_text_font_unicode_coverage_request{
            .bytes = std::span<const std::byte>{bytes},
            .source_label = "unsupported-cmap.ttf",
        });

    require(!coverage.ok(), "unsupported cmap does not resolve as valid coverage");
    require(
        coverage.status == render_text_font_unicode_coverage_status::cmap_invalid,
        "unsupported cmap reports cmap invalid status");
    require(
        coverage.cmap.status == render_text_font_cmap_inspect_status::unsupported_subtable_format,
        "coverage preserves unsupported cmap status");
    require(coverage.cmap.selected_format == 6U, "coverage preserves unsupported cmap format");
    require(coverage.ranges.empty(), "unsupported cmap exposes no coverage ranges");
    require(
        coverage.diagnostic.find("unsupported_subtable_format") != std::string::npos,
        "coverage diagnostic names cmap status");
}

void test_resolver_preserves_missing_cmap_status_from_valid_sfnt_diagnostics()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> bytes{std::byte{1}};
    const valid_sfnt_without_cmap_inspector sfnt_inspector;
    const basic_font_cmap_inspector cmap_inspector;
    const render_text_font_unicode_coverage_snapshot coverage = resolve_font_unicode_coverage(
        render_text_font_unicode_coverage_request{
            .bytes = std::span<const std::byte>{bytes},
            .source_label = "missing-cmap.ttf",
        },
        sfnt_inspector,
        cmap_inspector);

    require(!coverage.ok(), "missing cmap does not resolve as valid coverage");
    require(
        coverage.status == render_text_font_unicode_coverage_status::cmap_invalid,
        "valid SFNT with missing cmap reports cmap invalid status");
    require(
        coverage.cmap.status == render_text_font_cmap_inspect_status::missing_cmap_table,
        "coverage preserves missing cmap status");
    require(
        coverage.diagnostic.find("missing_cmap_table") != std::string::npos,
        "coverage diagnostic names missing cmap status");
}

} // namespace

int main()
{
    test_resolver_reports_format4_latin_and_hangul_coverage();
    test_resolver_reports_format12_non_bmp_coverage();
    test_resolver_accepts_loaded_font_source_bytes_result();
    test_resolver_reports_missing_bytes();
    test_resolver_propagates_invalid_sfnt();
    test_resolver_preserves_unsupported_cmap_status();
    test_resolver_preserves_missing_cmap_status_from_valid_sfnt_diagnostics();
    return 0;
}
