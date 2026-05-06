#include "render/text/font_source_bytes_loader.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

void append_ascii(std::vector<std::byte>& bytes, std::string_view text)
{
    for (const char value : text) {
        bytes.push_back(std::byte{static_cast<unsigned char>(value)});
    }
}

void append_byte(std::vector<std::byte>& bytes, unsigned char value)
{
    bytes.push_back(std::byte{value});
}

std::vector<std::byte> make_fake_font_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "fake-font");
    append_byte(bytes, 0);
    append_byte(bytes, 1);
    append_byte(bytes, 2);
    return bytes;
}

std::filesystem::path text_font_source_bytes_test_root()
{
    return std::filesystem::current_path() / "text_font_source_bytes_loader_test_data";
}

void reset_text_font_source_bytes_test_root(const std::filesystem::path& root)
{
    std::error_code error;
    std::filesystem::remove_all(root, error);
    require(!error, "old text font source bytes test data can be removed");
    std::filesystem::create_directories(root, error);
    require(!error, "text font source bytes test data root can be created");
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::byte>& bytes)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    require(!error, "text font source bytes fixture parent can be created");

    std::ofstream file(path, std::ios::binary);
    require(file.good(), "text font source bytes fixture file can be opened");
    if (!bytes.empty()) {
        file.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    require(file.good(), "text font source bytes fixture bytes can be written");
}

std::string percent_encode_file_uri_path(std::string_view path)
{
    std::string encoded;
    encoded.reserve(path.size());
    for (const char value : path) {
        switch (value) {
        case ' ':
            encoded += "%20";
            break;
        case '#':
            encoded += "%23";
            break;
        case '%':
            encoded += "%25";
            break;
        default:
            encoded.push_back(value);
            break;
        }
    }
    return encoded;
}

std::string make_file_uri(const std::filesystem::path& path)
{
    const std::string generic = std::filesystem::absolute(path).generic_string();
    const std::string encoded = percent_encode_file_uri_path(generic);
    if (generic.size() >= 2 && generic[1] == ':') {
        return "file:///" + encoded;
    }
    if (!generic.empty() && generic.front() == '/') {
        return "file://" + encoded;
    }
    return "file:///" + encoded;
}

void test_filesystem_font_source_bytes_loader_loads_file_path_and_file_uri()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = text_font_source_bytes_test_root();
    reset_text_font_source_bytes_test_root(root);
    const std::filesystem::path local_path = root / "fonts" / "local font.ttf";
    const std::vector<std::byte> expected = make_fake_font_bytes();
    write_bytes(local_path, expected);

    const font_source_resolution local_source = resolve_font_source(
        font_face_descriptor{
            .id = 31,
            .family = "Fixture Local",
            .source_uri = "fonts/local font.ttf",
            .version = "fixture-1",
            .license = "test-fixture",
        },
        root.generic_string());
    filesystem_font_source_bytes_loader loader(root);
    const render_text_font_source_bytes_load_result loaded = loader.load(
        render_text_font_source_bytes_load_request{.source = local_source});
    require(loaded.ok(), "filesystem font source bytes loader loads local path bytes");
    require(
        loaded.status == render_text_font_source_bytes_load_status::loaded,
        "local font source load reports loaded");
    require(loaded.bytes == expected, "local font source load returns fixture bytes");
    require(loaded.source.face_id == 31, "local font source load preserves face id");
    require(loaded.readiness.status == render_text_font_source_bytes_status::pending_file_load, "local load preserves readiness");
    require(loaded.readiness.requires_io, "local load readiness records IO requirement");
    require(loaded.cache_key == local_source.resolved_location, "local load reports readiness cache key");
    require(
        std::filesystem::path(loaded.resolved_path).lexically_normal() == local_path.lexically_normal(),
        "local font source load reports resolved path");
    require(loader.load_requests.size() == 1, "filesystem font source loader records load request");

    const std::filesystem::path file_uri_path = root / "fonts" / "file uri.ttf";
    write_bytes(file_uri_path, expected);
    const font_source_resolution file_uri_source = resolve_font_source(font_face_descriptor{
        .id = 32,
        .family = "Fixture URI",
        .source_uri = make_file_uri(file_uri_path),
        .version = "fixture-1",
        .license = "test-fixture",
    });
    const render_text_font_source_bytes_load_result loaded_file_uri = loader.load(
        render_text_font_source_bytes_load_request{.source = file_uri_source});
    require(loaded_file_uri.ok(), "filesystem font source bytes loader loads file URI bytes");
    require(loaded_file_uri.bytes == expected, "file URI font source load returns fixture bytes");
    require(
        std::filesystem::path(loaded_file_uri.resolved_path).lexically_normal() == file_uri_path.lexically_normal(),
        "file URI font source load reports resolved path");
    require(loader.load_requests.size() == 2, "filesystem font source loader records file URI request");
}

void test_filesystem_font_source_bytes_loader_follows_non_file_readiness_policy()
{
    using namespace quiz_vulkan::render;

    filesystem_font_source_bytes_loader loader(text_font_source_bytes_test_root());

    const font_source_resolution fixture = resolve_font_source(font_face_descriptor{
        .id = 33,
        .family = "Fixture Sans",
        .source_uri = "fixture://fonts/fixture-sans-regular",
        .version = "fixture-1",
        .license = "test-fixture",
    });
    const render_text_font_source_bytes_load_result fixture_result = loader.load(
        render_text_font_source_bytes_load_request{.source = fixture});
    require(!fixture_result.ok(), "fixture font source does not read bytes from filesystem");
    require(
        fixture_result.status == render_text_font_source_bytes_load_status::available_virtual_fixture,
        "fixture font source load follows virtual readiness policy");
    require(fixture_result.bytes.empty(), "fixture font source load returns no concrete bytes");
    require(
        fixture_result.readiness.status == render_text_font_source_bytes_status::available_virtual_fixture,
        "fixture font source load preserves readiness");
    require(fixture_result.readiness.bytes_available_without_io, "fixture font source readiness stays available without IO");

    const font_source_resolution unknown = resolve_font_source(font_face_descriptor{
        .id = 34,
        .family = "Fixture Remote",
        .source_uri = "https://example.test/font.ttf",
        .version = "fixture-1",
        .license = "test-fixture",
    });
    const render_text_font_source_bytes_load_result unsupported = loader.load(
        render_text_font_source_bytes_load_request{.source = unknown});
    require(!unsupported.ok(), "unknown font source does not read bytes");
    require(
        unsupported.status == render_text_font_source_bytes_load_status::unsupported_source,
        "unknown font source load follows unsupported readiness policy");
    require(
        unsupported.readiness.status == render_text_font_source_bytes_status::unsupported_source,
        "unknown font source load preserves unsupported readiness");

    const render_text_font_source_bytes_load_result missing = loader.load(
        render_text_font_source_bytes_load_request{.source = font_source_resolution{}});
    require(!missing.ok(), "missing font source does not read bytes");
    require(
        missing.status == render_text_font_source_bytes_load_status::missing_source,
        "missing font source load follows missing readiness policy");
    require(
        missing.readiness.status == render_text_font_source_bytes_status::missing_source,
        "missing font source load preserves missing readiness");
}

void test_filesystem_font_source_bytes_loader_rejects_bad_cache_keys_and_traversal()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = text_font_source_bytes_test_root();
    reset_text_font_source_bytes_test_root(root);
    filesystem_font_source_bytes_loader loader(root);

    const font_source_resolution bad_cache_key = resolve_font_source(
        font_face_descriptor{
            .id = 35,
            .family = "Fixture Bad",
            .source_uri = "fonts/bad\nfont.ttf",
            .version = "fixture-1",
            .license = "test-fixture",
        },
        root.generic_string());
    const render_text_font_source_bytes_load_result invalid = loader.load(
        render_text_font_source_bytes_load_request{.source = bad_cache_key});
    require(!invalid.ok(), "font source load rejects invalid cache key");
    require(
        invalid.status == render_text_font_source_bytes_load_status::invalid_cache_key,
        "invalid cache key reports invalid cache key status");
    require(invalid.bytes.empty(), "invalid cache key returns no bytes");
    require(invalid.diagnostic.find("cache key") != std::string::npos, "invalid cache key diagnostic names cache key");

    const font_source_resolution traversal = resolve_font_source(
        font_face_descriptor{
            .id = 36,
            .family = "Fixture Escape",
            .source_uri = "../escape.ttf",
            .version = "fixture-1",
            .license = "test-fixture",
        },
        root.generic_string());
    const render_text_font_source_bytes_load_result blocked = loader.load(
        render_text_font_source_bytes_load_request{.source = traversal});
    require(!blocked.ok(), "font source load rejects path traversal");
    require(
        blocked.status == render_text_font_source_bytes_load_status::path_traversal_blocked,
        "path traversal reports blocked status");
    require(blocked.bytes.empty(), "blocked traversal returns no bytes");
    require(
        blocked.diagnostic.find("escapes") != std::string::npos,
        "path traversal diagnostic names base escape");
}

void test_filesystem_font_source_bytes_loader_reports_missing_and_empty_files()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = text_font_source_bytes_test_root();
    reset_text_font_source_bytes_test_root(root);
    filesystem_font_source_bytes_loader loader(root);

    const font_source_resolution missing_source = resolve_font_source(
        font_face_descriptor{
            .id = 37,
            .family = "Fixture Missing",
            .source_uri = "fonts/missing.ttf",
            .version = "fixture-1",
            .license = "test-fixture",
        },
        root.generic_string());
    const render_text_font_source_bytes_load_result missing = loader.load(
        render_text_font_source_bytes_load_request{.source = missing_source});
    require(!missing.ok(), "missing font file does not load");
    require(
        missing.status == render_text_font_source_bytes_load_status::missing_bytes,
        "missing font file reports missing bytes");
    require(!missing.diagnostic.empty(), "missing font file includes diagnostic");

    const std::filesystem::path empty_path = root / "fonts" / "empty.ttf";
    write_bytes(empty_path, {});
    const font_source_resolution empty_source = resolve_font_source(
        font_face_descriptor{
            .id = 38,
            .family = "Fixture Empty",
            .source_uri = "fonts/empty.ttf",
            .version = "fixture-1",
            .license = "test-fixture",
        },
        root.generic_string());
    const render_text_font_source_bytes_load_result empty = loader.load(
        render_text_font_source_bytes_load_request{.source = empty_source});
    require(!empty.ok(), "empty font file does not load as bytes");
    require(
        empty.status == render_text_font_source_bytes_load_status::empty_bytes,
        "empty font file reports empty bytes");
    require(empty.bytes.empty(), "empty font file returns no bytes");
}

} // namespace

int main()
{
    test_filesystem_font_source_bytes_loader_loads_file_path_and_file_uri();
    test_filesystem_font_source_bytes_loader_follows_non_file_readiness_policy();
    test_filesystem_font_source_bytes_loader_rejects_bad_cache_keys_and_traversal();
    test_filesystem_font_source_bytes_loader_reports_missing_and_empty_files();
    return 0;
}
