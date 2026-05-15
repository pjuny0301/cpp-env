#include "render/text/font_backend_adapter.h"
#include "render/text/font_backend_dependency.h"
#include "render/text/font_unicode_coverage.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
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

std::filesystem::path quiz_vulkan_app_root_from_this_test()
{
    const std::filesystem::path test_path = std::filesystem::path(__FILE__);
    return test_path.parent_path().parent_path().parent_path().parent_path();
}

std::vector<std::filesystem::path> freetype_real_font_fixture_candidates()
{
    const std::filesystem::path app_root = quiz_vulkan_app_root_from_this_test();
    const std::filesystem::path repo_root =
        app_root.parent_path().parent_path().parent_path();
    return {
        repo_root / "build" / "external" / "lib" / "cpp" / "desktop"
            / "harfbuzz-14.2.0" / "perf" / "fonts" / "Roboto-Regular.ttf",
        repo_root / "build" / "external" / "lib" / "cpp" / "desktop"
            / "harfbuzz-14.2.0" / "test" / "api" / "fonts" / "Mplus1p-Regular.ttf",
    };
}

std::filesystem::path first_available_freetype_real_font_fixture()
{
    for (const std::filesystem::path& candidate : freetype_real_font_fixture_candidates()) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

std::vector<std::byte> read_binary_fixture_bytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    require(input.good(), "real font fixture opens for binary read");

    std::vector<std::byte> bytes;
    for (std::istreambuf_iterator<char> iter{input};
         iter != std::istreambuf_iterator<char>{};
         ++iter) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(*iter)));
    }
    return bytes;
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

std::vector<std::byte> make_font_bytes_with_cmap(std::vector<std::byte> cmap_table)
{
    return make_sfnt_bytes(
        std::string_view{"\0\1\0\0", 4U},
        required_truetype_tables(std::move(cmap_table)));
}

quiz_vulkan::render::render_text_font_source_bytes_load_result font_source_bytes_result_for(
    std::vector<std::byte> bytes,
    quiz_vulkan::render::render_text_font_source_bytes_load_status status =
        quiz_vulkan::render::render_text_font_source_bytes_load_status::loaded)
{
    using namespace quiz_vulkan::render;

    return render_text_font_source_bytes_load_result{
        .status = status,
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
        .bytes = std::move(bytes),
    };
}

quiz_vulkan::render::render_text_external_font_backend_work_readiness freetype_work_readiness_for(
    const quiz_vulkan::render::render_text_external_font_backend_manifest& manifest)
{
    using namespace quiz_vulkan::render;

    return make_render_text_external_font_backend_work_readiness(
        make_render_text_external_font_backend_header_probe_snapshot(),
        manifest,
        render_text_font_backend_selection_purpose::rasterization);
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

void test_font_face_byte_readiness_classifies_future_freetype_load_states()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> valid_bytes = make_font_bytes_with_cmap(
        wrap_cmap_subtable(
            3U,
            1U,
            make_format4_subtable({
                cmap_format4_fixture_range{.first_codepoint = 0x0041U, .last_codepoint = 0x005aU},
            })));
    const render_text_font_face_byte_readiness ready =
        inspect_render_text_font_face_byte_readiness(font_source_bytes_result_for(valid_bytes));
    require(ready.ok(), "valid bytes produce coverage-ready font face byte readiness");
    require(
        ready.status == render_text_font_face_byte_readiness_status::coverage_ready,
        "valid bytes report coverage-ready status");
    require(ready.source_loaded, "coverage-ready bytes record loaded source bytes");
    require(ready.sfnt_valid, "coverage-ready bytes record valid SFNT");
    require(ready.cmap_valid, "coverage-ready bytes record valid cmap");
    require(ready.coverage_range_count == 1U, "coverage-ready bytes record cmap coverage ranges");
    require(ready.can_attempt_freetype_load, "coverage-ready bytes can attempt future FreeType load");
    require(!ready.fallback_required, "coverage-ready bytes do not require fallback");
    require(ready.source_label == "resolved/fixture.ttf", "readiness preserves resolved source label");

    const render_text_font_face_byte_readiness missing =
        inspect_render_text_font_face_byte_readiness(
            font_source_bytes_result_for({}, render_text_font_source_bytes_load_status::missing_bytes));
    require(
        missing.status == render_text_font_face_byte_readiness_status::missing_bytes,
        "missing source bytes report missing-byte readiness");
    require(missing.fallback_required, "missing bytes require fallback");
    require(!missing.source_loaded, "missing bytes are not marked loaded");

    const render_text_font_face_byte_readiness empty =
        inspect_render_text_font_face_byte_readiness(
            font_source_bytes_result_for({}, render_text_font_source_bytes_load_status::empty_bytes));
    require(
        empty.status == render_text_font_face_byte_readiness_status::empty_bytes,
        "empty source bytes report empty-byte readiness");
    require(empty.byte_count == 0U, "empty source bytes preserve zero byte count");
    require(empty.fallback_required, "empty source bytes require fallback");

    const std::vector<std::byte> truncated_sfnt{std::byte{'O'}, std::byte{'T'}, std::byte{'T'}, std::byte{'O'}};
    const render_text_font_face_byte_readiness invalid_sfnt =
        inspect_render_text_font_face_byte_readiness(font_source_bytes_result_for(truncated_sfnt));
    require(
        invalid_sfnt.status == render_text_font_face_byte_readiness_status::invalid_sfnt,
        "truncated SFNT bytes report invalid-SFNT readiness");
    require(
        invalid_sfnt.sfnt_status == render_text_font_sfnt_inspect_status::truncated_header,
        "invalid-SFNT readiness preserves SFNT inspector status");
    require(invalid_sfnt.fallback_required, "invalid SFNT bytes require fallback");

    const std::vector<std::byte> missing_cmap_bytes = make_sfnt_bytes(
        std::string_view{"\0\1\0\0", 4U},
        required_truetype_tables_without_cmap());
    const render_text_font_face_byte_readiness missing_cmap =
        inspect_render_text_font_face_byte_readiness(font_source_bytes_result_for(missing_cmap_bytes));
    require(
        missing_cmap.status == render_text_font_face_byte_readiness_status::missing_cmap,
        "SFNT bytes without cmap report missing-cmap readiness");
    require(missing_cmap.missing_cmap, "missing-cmap readiness preserves missing cmap evidence");
    require(
        missing_cmap.sfnt_status == render_text_font_sfnt_inspect_status::missing_required_table,
        "missing-cmap readiness records SFNT required-table failure");
    require(missing_cmap.fallback_required, "missing-cmap bytes require fallback");

    const std::vector<std::byte> unsupported_cmap_bytes = make_font_bytes_with_cmap(
        wrap_cmap_subtable(3U, 1U, make_format6_subtable()));
    const render_text_font_face_byte_readiness invalid_cmap =
        inspect_render_text_font_face_byte_readiness(font_source_bytes_result_for(unsupported_cmap_bytes));
    require(
        invalid_cmap.status == render_text_font_face_byte_readiness_status::invalid_cmap,
        "unsupported cmap reports invalid-cmap readiness");
    require(
        invalid_cmap.cmap_status == render_text_font_cmap_inspect_status::unsupported_subtable_format,
        "invalid-cmap readiness preserves cmap inspector status");
    require(invalid_cmap.fallback_required, "invalid cmap bytes require fallback");

    render_text_font_source_bytes_load_result virtual_fixture =
        font_source_bytes_result_for({}, render_text_font_source_bytes_load_status::available_virtual_fixture);
    virtual_fixture.source.kind = render_text_font_source_kind::fixture_uri;
    virtual_fixture.source.source_uri = "fixture://fonts/virtual";
    virtual_fixture.source.resolved_location = "fonts/virtual";
    virtual_fixture.source.virtual_fixture = true;
    virtual_fixture.readiness.status = render_text_font_source_bytes_status::available_virtual_fixture;
    virtual_fixture.readiness.requires_io = false;
    virtual_fixture.readiness.bytes_available_without_io = true;
    const render_text_font_face_byte_readiness fallback =
        inspect_render_text_font_face_byte_readiness(virtual_fixture);
    require(
        fallback.status == render_text_font_face_byte_readiness_status::fallback_required,
        "virtual fixture bytes report fallback-required readiness");
    require(fallback.fallback_required, "fallback-required readiness records fallback requirement");
    require(!fallback.can_attempt_freetype_load, "fallback-required readiness does not attempt FreeType load");
}

void test_freetype_face_load_readiness_combines_materialized_bytes_and_backend_work()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> valid_bytes = make_font_bytes_with_cmap(
        wrap_cmap_subtable(
            3U,
            1U,
            make_format4_subtable({
                cmap_format4_fixture_range{.first_codepoint = 0x0041U, .last_codepoint = 0x005aU},
            })));
    const render_text_font_face_byte_readiness face_bytes =
        inspect_render_text_font_face_byte_readiness(font_source_bytes_result_for(valid_bytes));
    require(face_bytes.ok(), "valid materialized SFNT bytes are ready before backend loading");

    const render_text_external_font_backend_work_readiness header_only_backend =
        freetype_work_readiness_for(make_render_text_header_backed_external_font_backend_manifest());
    const render_text_freetype_face_load_readiness header_only =
        make_render_text_freetype_face_load_readiness(face_bytes, header_only_backend);
    require(header_only.face_id == 47U, "FreeType load readiness preserves face id");
    require(header_only.materialized_bytes_ready, "FreeType load readiness consumes materialized bytes");
    require(header_only.coverage_ready, "FreeType load readiness consumes cmap coverage readiness");
    require(header_only.face_byte_status == render_text_font_face_byte_readiness_status::coverage_ready, "FreeType load readiness records byte status");
    require(header_only.backend_work_status == header_only_backend.status, "FreeType load readiness records backend work status");
    require(header_only.freetype_header_available == header_only_backend.header_available, "FreeType load readiness mirrors header availability");
    require(!header_only.ok(), "header-only backend readiness cannot call FreeType yet");
    require(header_only.deterministic_fallback_required, "header-only backend readiness requires fallback");
    if (header_only_backend.header_available) {
        require(
            header_only.status == render_text_freetype_face_load_readiness_status::header_only,
            "approved FreeType header without library reports header-only load readiness");
        require(
            header_only.required_wiring.find("quiz_vulkan_freetype_external") != std::string::npos,
            "header-only readiness reports the required FreeType target wiring");
    } else {
        require(
            header_only.status == render_text_freetype_face_load_readiness_status::missing_approved_header,
            "missing approved FreeType header reports missing-header load readiness");
    }

    const render_text_freetype_face_load_readiness source_ready =
        make_render_text_freetype_face_load_readiness(
            face_bytes,
            freetype_work_readiness_for(make_render_text_known_external_font_backend_manifest(true, false, false)));
    require(
        source_ready.status == render_text_freetype_face_load_readiness_status::source_ready,
        "source-ready FreeType dependency still cannot load a face without linking");
    require(source_ready.freetype_source_available, "source-ready load readiness records source availability");
    require(!source_ready.freetype_library_linked, "source-ready load readiness records missing library link");
    require(!source_ready.can_call_freetype_new_memory_face, "source-ready load readiness cannot call FreeType");

    const render_text_freetype_face_load_readiness library_linked =
        make_render_text_freetype_face_load_readiness(
            face_bytes,
            freetype_work_readiness_for(make_render_text_known_external_font_backend_manifest(true, true, false)));
    require(
        library_linked.status == render_text_freetype_face_load_readiness_status::library_linked,
        "linked FreeType library without adapter symbols reports library-linked readiness");
    require(library_linked.freetype_library_linked, "library-linked readiness records linked library");
    require(!library_linked.freetype_adapter_ready, "library-linked readiness records missing adapter symbols");
    require(
        library_linked.required_wiring.find("FT_New_Memory_Face") != std::string::npos,
        "library-linked readiness reports the missing adapter symbol wiring");

    const render_text_freetype_face_load_readiness adapter_ready =
        make_render_text_freetype_face_load_readiness(
            face_bytes,
            freetype_work_readiness_for(make_render_text_known_external_font_backend_manifest(true, true, true)));
    require(adapter_ready.ok(), "adapter-ready FreeType backend can load materialized face bytes");
    require(
        adapter_ready.status == render_text_freetype_face_load_readiness_status::ready_for_load,
        "adapter-ready FreeType backend reports ready-for-load status");
    require(adapter_ready.freetype_adapter_ready, "adapter-ready load readiness records adapter symbols");
    require(adapter_ready.can_call_freetype_new_memory_face, "adapter-ready load readiness can call FreeType");
    require(!adapter_ready.deterministic_fallback_required, "adapter-ready load readiness does not require fallback");
    require(adapter_ready.required_wiring.empty(), "adapter-ready load readiness needs no additional wiring");
}

void test_freetype_face_load_readiness_keeps_byte_failures_ahead_of_backend_work()
{
    using namespace quiz_vulkan::render;

    const render_text_external_font_backend_work_readiness adapter_ready_backend =
        freetype_work_readiness_for(make_render_text_known_external_font_backend_manifest(true, true, true));

    const render_text_freetype_face_load_readiness missing =
        make_render_text_freetype_face_load_readiness(
            inspect_render_text_font_face_byte_readiness(
                font_source_bytes_result_for({}, render_text_font_source_bytes_load_status::missing_bytes)),
            adapter_ready_backend);
    require(
        missing.status == render_text_freetype_face_load_readiness_status::missing_bytes,
        "missing bytes block FreeType load before backend readiness");
    require(!missing.can_call_freetype_new_memory_face, "missing bytes cannot call FreeType");

    const std::vector<std::byte> truncated_sfnt{std::byte{'O'}, std::byte{'T'}, std::byte{'T'}, std::byte{'O'}};
    const render_text_freetype_face_load_readiness invalid_sfnt =
        make_render_text_freetype_face_load_readiness(
            inspect_render_text_font_face_byte_readiness(font_source_bytes_result_for(truncated_sfnt)),
            adapter_ready_backend);
    require(
        invalid_sfnt.status == render_text_freetype_face_load_readiness_status::invalid_sfnt,
        "invalid SFNT bytes block FreeType load before backend readiness");
    require(
        invalid_sfnt.sfnt_status == render_text_font_sfnt_inspect_status::truncated_header,
        "invalid SFNT load readiness preserves SFNT inspector status");

    const std::vector<std::byte> missing_cmap_bytes = make_sfnt_bytes(
        std::string_view{"\0\1\0\0", 4U},
        required_truetype_tables_without_cmap());
    const render_text_freetype_face_load_readiness missing_cmap =
        make_render_text_freetype_face_load_readiness(
            inspect_render_text_font_face_byte_readiness(font_source_bytes_result_for(missing_cmap_bytes)),
            adapter_ready_backend);
    require(
        missing_cmap.status == render_text_freetype_face_load_readiness_status::missing_cmap,
        "missing cmap bytes block FreeType load before backend readiness");
    require(
        missing_cmap.face_byte_status == render_text_font_face_byte_readiness_status::missing_cmap,
        "missing cmap load readiness preserves byte readiness status");
    require(missing_cmap.deterministic_fallback_required, "byte failure keeps deterministic fallback required");
}

void test_freetype_memory_face_adapter_preserves_byte_readiness_failures()
{
    using namespace quiz_vulkan::render;

    const render_text_freetype_memory_face_load_result missing =
        load_render_text_freetype_memory_face(render_text_freetype_memory_face_load_request{
            .source_bytes = font_source_bytes_result_for(
                {},
                render_text_font_source_bytes_load_status::missing_bytes),
        });
    require(
        missing.status == render_text_freetype_memory_face_load_status::missing_bytes,
        "FreeType adapter preserves missing-byte readiness before backend calls");
    require(
        missing.face_byte_status == render_text_font_face_byte_readiness_status::missing_bytes,
        "FreeType adapter records missing-byte face readiness");
    require(!missing.face_created, "missing bytes do not create a FreeType face");

    const render_text_freetype_memory_face_load_result empty =
        load_render_text_freetype_memory_face(render_text_freetype_memory_face_load_request{
            .source_bytes = font_source_bytes_result_for(
                {},
                render_text_font_source_bytes_load_status::empty_bytes),
        });
    require(
        empty.status == render_text_freetype_memory_face_load_status::empty_bytes,
        "FreeType adapter preserves empty-byte readiness before backend calls");
    require(!empty.face_created, "empty bytes do not create a FreeType face");

    const std::vector<std::byte> truncated_sfnt{std::byte{'O'}, std::byte{'T'}, std::byte{'T'}, std::byte{'O'}};
    const render_text_freetype_memory_face_load_result invalid_sfnt =
        load_render_text_freetype_memory_face(render_text_freetype_memory_face_load_request{
            .source_bytes = font_source_bytes_result_for(truncated_sfnt),
        });
    require(
        invalid_sfnt.status == render_text_freetype_memory_face_load_status::invalid_sfnt,
        "FreeType adapter preserves invalid-SFNT readiness before backend calls");
    require(
        invalid_sfnt.sfnt_status == render_text_font_sfnt_inspect_status::truncated_header,
        "FreeType adapter reports SFNT inspector status for invalid bytes");

    const std::vector<std::byte> missing_cmap_bytes = make_sfnt_bytes(
        std::string_view{"\0\1\0\0", 4U},
        required_truetype_tables_without_cmap());
    const render_text_freetype_memory_face_load_result missing_cmap =
        load_render_text_freetype_memory_face(render_text_freetype_memory_face_load_request{
            .source_bytes = font_source_bytes_result_for(missing_cmap_bytes),
        });
    require(
        missing_cmap.status == render_text_freetype_memory_face_load_status::missing_cmap,
        "FreeType adapter preserves missing-cmap readiness before backend calls");
    require(
        missing_cmap.face_byte_status == render_text_font_face_byte_readiness_status::missing_cmap,
        "FreeType adapter records missing-cmap face readiness");
}

void test_freetype_memory_face_adapter_loads_real_fixture_or_classifies_minimal_sfnt_failure()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path real_fixture = first_available_freetype_real_font_fixture();
    if (!real_fixture.empty()) {
        render_text_font_source_bytes_load_result source =
            font_source_bytes_result_for(read_binary_fixture_bytes(real_fixture));
        source.source.family = "FreeType Fixture";
        source.source.source_uri = real_fixture.generic_string();
        source.source.resolved_location = real_fixture.generic_string();
        source.cache_key = real_fixture.generic_string();
        source.resolved_path = real_fixture.generic_string();

        const render_text_freetype_memory_face_load_result loaded =
            load_render_text_freetype_memory_face(render_text_freetype_memory_face_load_request{
                .source_bytes = std::move(source),
            });

        if (render_text_freetype_memory_face_adapter_available()) {
            require(loaded.ok(), "real font fixture creates a FreeType memory face");
            require(
                loaded.status == render_text_freetype_memory_face_load_status::loaded,
                "real font fixture reports loaded FreeType status");
            require(loaded.face_created, "real font fixture records created FreeType face");
            require(loaded.face_count > 0, "real font fixture records FreeType face count");
            require(loaded.glyph_count > 0, "real font fixture records FreeType glyph count");
            require(
                loaded.face_byte_status == render_text_font_face_byte_readiness_status::coverage_ready,
                "real font fixture consumes coverage-ready byte evidence");
            require(!loaded.deterministic_fallback_required, "loaded FreeType face does not require fallback");
        } else {
            require(
                loaded.status == render_text_freetype_memory_face_load_status::backend_unavailable,
                "real font fixture reports backend unavailable when FreeType is not compiled in");
            require(
                loaded.face_byte_status == render_text_font_face_byte_readiness_status::coverage_ready,
                "real font fixture still reaches byte coverage readiness without the adapter");
            require(loaded.deterministic_fallback_required, "missing compiled adapter preserves fallback");
        }
        return;
    }

    const std::vector<std::byte> minimal_sfnt_bytes = make_font_bytes_with_cmap(
        wrap_cmap_subtable(
            3U,
            1U,
            make_format4_subtable({
                cmap_format4_fixture_range{.first_codepoint = 0x0041U, .last_codepoint = 0x005aU},
            })));
    const render_text_freetype_memory_face_load_result minimal =
        load_render_text_freetype_memory_face(render_text_freetype_memory_face_load_request{
            .source_bytes = font_source_bytes_result_for(minimal_sfnt_bytes),
            .source_label = "minimal-sfnt-cmap-fixture.ttf",
        });

    require(
        minimal.face_byte_status == render_text_font_face_byte_readiness_status::coverage_ready,
        "minimal SFNT fixture reaches coverage-ready byte evidence");
    if (render_text_freetype_memory_face_adapter_available()) {
        require(
            minimal.status == render_text_freetype_memory_face_load_status::freetype_new_memory_face_failed,
            "minimal SFNT fixture reaches FreeType and reports deterministic load failure");
        require(minimal.freetype_error != 0, "minimal SFNT FreeType failure records FT error code");
    } else {
        require(
            minimal.status == render_text_freetype_memory_face_load_status::backend_unavailable,
            "minimal SFNT fixture reports backend unavailable when FreeType is not compiled in");
    }
    require(!minimal.face_created, "minimal SFNT failure does not leak a created FreeType face");
    require(minimal.deterministic_fallback_required, "minimal SFNT failure preserves fallback requirement");
}

void test_catalog_adapter_converts_valid_coverage_to_descriptor_ranges()
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
            .source_label = "adapter-format12.ttf",
        });

    const font_unicode_coverage_catalog_adapter adapter;
    const std::vector<font_codepoint_range> ranges = adapter.coverage_for(coverage);
    require(ranges.size() == 3U, "adapter preserves Latin, Hangul, and non-BMP coverage ranges");
    require(ranges[0].first == 0x0041U && ranges[0].last == 0x005aU, "adapter converts Latin coverage range");
    require(ranges[1].first == 0xac00U && ranges[1].last == 0xac02U, "adapter converts Hangul coverage range");
    require(ranges[2].first == 0x1f600U && ranges[2].last == 0x1f64fU, "adapter converts non-BMP coverage range");

    const font_face_descriptor descriptor = adapter.apply_to_descriptor(
        font_face_descriptor{
            .id = 71,
            .family = "Adapter Sans",
            .source_uri = "fixture://fonts/adapter-sans",
            .version = "fixture-1",
            .license = "test-fixture",
            .weight = 400,
        },
        coverage);

    require(descriptor.coverage.size() == 3U, "adapted descriptor receives known coverage ranges");
    require(descriptor.supports_codepoint(0x0041U), "adapted descriptor supports Latin coverage");
    require(descriptor.supports_codepoint(0xac01U), "adapted descriptor supports Hangul coverage");
    require(descriptor.supports_codepoint(0x1f600U), "adapted descriptor supports non-BMP coverage");
    require(!descriptor.supports_codepoint(0x0061U), "adapted descriptor rejects codepoints outside coverage");
}

void test_catalog_adapter_keeps_missing_and_invalid_coverage_known_empty()
{
    using namespace quiz_vulkan::render;

    const font_unicode_coverage_catalog_adapter adapter;
    const render_text_font_unicode_coverage_snapshot missing{
        .source_label = "missing.ttf",
        .status = render_text_font_unicode_coverage_status::missing_bytes,
        .diagnostic = "missing bytes",
    };
    const font_face_descriptor missing_descriptor = adapter.apply_to_descriptor(
        font_face_descriptor{
            .id = 72,
            .family = "Missing Sans",
            .source_uri = "fonts/missing.ttf",
            .version = "fixture-1",
            .license = "test-fixture",
        },
        missing);

    require(missing_descriptor.coverage.size() == 1U, "missing coverage becomes an explicit known-empty range");
    require(
        font_unicode_coverage_codepoint_range_is_known_empty(missing_descriptor.coverage.front()),
        "missing coverage range is marked known-empty");
    require(!missing_descriptor.supports_codepoint(0x0041U), "missing coverage does not claim Latin support");
    require(!missing_descriptor.supports_codepoint(0xac00U), "missing coverage does not claim Hangul support");

    const render_text_font_unicode_coverage_snapshot invalid{
        .source_label = "invalid.ttf",
        .status = render_text_font_unicode_coverage_status::cmap_invalid,
        .ranges = {
            render_text_font_cmap_range{
                .first_codepoint = U'A',
                .last_codepoint = U'Z',
            },
        },
        .diagnostic = "invalid cmap",
    };
    const font_face_descriptor invalid_descriptor = adapter.apply_to_descriptor(
        font_face_descriptor{
            .id = 73,
            .family = "Invalid Sans",
            .source_uri = "fonts/invalid.ttf",
            .version = "fixture-1",
            .license = "test-fixture",
        },
        invalid);

    require(invalid_descriptor.coverage.size() == 1U, "invalid coverage ignores stale ranges");
    require(
        font_unicode_coverage_codepoint_range_is_known_empty(invalid_descriptor.coverage.front()),
        "invalid coverage range is marked known-empty");
    require(!invalid_descriptor.supports_codepoint(0x0041U), "invalid coverage does not claim Latin support");
    require(!invalid_descriptor.supports_codepoint(0x1f600U), "invalid coverage does not claim non-BMP support");
}

void test_catalog_adapter_lets_font_catalog_pick_fallback_from_adapted_coverage()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> latin_bytes = make_font_bytes_with_cmap(
        wrap_cmap_subtable(
            3U,
            1U,
            make_format4_subtable({
                cmap_format4_fixture_range{.first_codepoint = 0x0041U, .last_codepoint = 0x005aU},
            })));
    const std::vector<std::byte> fallback_bytes = make_font_bytes_with_cmap(
        wrap_cmap_subtable(
            3U,
            10U,
            make_format12_subtable({
                cmap_format12_fixture_range{.first_codepoint = 0xac00U, .last_codepoint = 0xac02U},
                cmap_format12_fixture_range{.first_codepoint = 0x1f600U, .last_codepoint = 0x1f64fU},
            })));
    const render_text_font_unicode_coverage_snapshot latin_coverage = resolve_font_unicode_coverage(
        render_text_font_unicode_coverage_request{
            .bytes = std::span<const std::byte>{latin_bytes},
            .source_label = "primary-latin.ttf",
        });
    const render_text_font_unicode_coverage_snapshot fallback_coverage = resolve_font_unicode_coverage(
        render_text_font_unicode_coverage_request{
            .bytes = std::span<const std::byte>{fallback_bytes},
            .source_label = "fallback-hangul-emoji.ttf",
        });

    const font_unicode_coverage_catalog_adapter adapter;
    font_face_catalog catalog;
    catalog.add_face(adapter.apply_to_descriptor(
        font_face_descriptor{
            .id = 81,
            .family = "Primary Sans",
            .source_uri = "fixture://fonts/primary-latin",
            .version = "fixture-1",
            .license = "test-fixture",
            .weight = 400,
        },
        latin_coverage));
    catalog.add_face(adapter.apply_to_descriptor(
        font_face_descriptor{
            .id = 82,
            .family = "Fallback Sans",
            .source_uri = "fixture://fonts/fallback-hangul-emoji",
            .version = "fixture-1",
            .license = "test-fixture",
            .weight = 400,
            .fallback = true,
        },
        fallback_coverage));

    const render_text_style primary_style{
        .font_family = "Primary Sans",
        .font_weight = 400,
    };
    const font_face_resolution latin_resolution =
        catalog.resolve_for_codepoint(primary_style, 0x0041U);
    require(latin_resolution.glyph_supported, "catalog resolves Latin from requested adapted face");
    require(!latin_resolution.used_fallback, "catalog does not fallback for requested Latin coverage");
    require(latin_resolution.resolved_face != nullptr && latin_resolution.resolved_face->id == 81U, "Latin resolves to primary face");

    const font_face_resolution hangul_resolution =
        catalog.resolve_for_codepoint(primary_style, 0xac01U);
    require(hangul_resolution.glyph_supported, "catalog resolves Hangul from adapted fallback coverage");
    require(hangul_resolution.used_fallback, "catalog reports fallback for Hangul coverage");
    require(
        hangul_resolution.resolved_face != nullptr && hangul_resolution.resolved_face->id == 82U,
        "Hangul resolves to fallback face");

    const font_face_resolution emoji_resolution =
        catalog.resolve_for_codepoint(primary_style, 0x1f600U);
    require(emoji_resolution.glyph_supported, "catalog resolves non-BMP from adapted fallback coverage");
    require(emoji_resolution.used_fallback, "catalog reports fallback for non-BMP coverage");
    require(
        emoji_resolution.resolved_face != nullptr && emoji_resolution.resolved_face->id == 82U,
        "non-BMP resolves to fallback face");
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
    test_font_face_byte_readiness_classifies_future_freetype_load_states();
    test_freetype_face_load_readiness_combines_materialized_bytes_and_backend_work();
    test_freetype_face_load_readiness_keeps_byte_failures_ahead_of_backend_work();
    test_freetype_memory_face_adapter_preserves_byte_readiness_failures();
    test_freetype_memory_face_adapter_loads_real_fixture_or_classifies_minimal_sfnt_failure();
    test_catalog_adapter_converts_valid_coverage_to_descriptor_ranges();
    test_catalog_adapter_keeps_missing_and_invalid_coverage_known_empty();
    test_catalog_adapter_lets_font_catalog_pick_fallback_from_adapted_coverage();
    return 0;
}
