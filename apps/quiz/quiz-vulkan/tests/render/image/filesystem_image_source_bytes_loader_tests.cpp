#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
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

std::vector<std::byte> make_ppm_1x1_encoded_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n1 1\n255\n");
    append_byte(bytes, 0x10);
    append_byte(bytes, 0x20);
    append_byte(bytes, 0x30);
    return bytes;
}

std::filesystem::path test_data_root()
{
    return std::filesystem::current_path() / "image_source_bytes_loader_test_data";
}

void reset_test_data_root(const std::filesystem::path& root)
{
    std::error_code error;
    std::filesystem::remove_all(root, error);
    require(!error, "old test data root can be removed");
    std::filesystem::create_directories(root, error);
    require(!error, "test data root can be created");
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::byte>& bytes)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    require(!error, "test data parent directory can be created");

    std::ofstream file(path, std::ios::binary);
    require(file.good(), "test fixture file can be opened");
    if (!bytes.empty()) {
        file.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    require(file.good(), "test fixture bytes can be written");
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

void test_filesystem_loader_loads_local_path_bytes()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = test_data_root();
    reset_test_data_root(root);
    const std::filesystem::path local_path = root / "textures" / "local.ppm";
    const std::vector<std::byte> expected = make_ppm_1x1_encoded_bytes();
    write_bytes(local_path, expected);

    const normalizing_image_resolver resolver;
    const render_image_resolve_result resolved = resolver.resolve(
        render_image_resolve_request{.uri = "textures/local.ppm"});
    require(resolved.ok(), "local path source resolves");
    require(resolved.source.kind == render_image_source_kind::local_path, "local path source kind is preserved");

    filesystem_image_source_bytes_loader loader(root);
    const render_image_source_bytes_load_result loaded = loader.load(
        render_image_source_bytes_load_request{.source = resolved.source});
    require(loaded.ok(), "filesystem loader loads local path bytes");
    require(loaded.status == render_image_source_bytes_load_status::loaded, "local path load reports loaded");
    require(loaded.source.cache_key() == "textures/local.ppm", "local path load preserves cache key");
    require(loaded.encoded_bytes == expected, "local path load returns file bytes");
    require(loader.load_requests.size() == 1, "filesystem loader records local load request");
}

void test_filesystem_loader_loads_file_uri_bytes()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = test_data_root();
    reset_test_data_root(root);
    const std::filesystem::path file_uri_path = root / "textures" / "file uri.ppm";
    const std::vector<std::byte> expected = make_ppm_1x1_encoded_bytes();
    write_bytes(file_uri_path, expected);

    const normalizing_image_resolver resolver;
    const render_image_resolve_result resolved = resolver.resolve(
        render_image_resolve_request{.uri = make_file_uri(file_uri_path)});
    require(resolved.ok(), "file URI source resolves");
    require(resolved.source.kind == render_image_source_kind::file_uri, "file URI source kind is preserved");

    filesystem_image_source_bytes_loader loader;
    const render_image_source_bytes_load_result loaded = loader.load(
        render_image_source_bytes_load_request{.source = resolved.source});
    if (!loaded.ok()) {
        std::fprintf(stderr, "Resolved file URI: %s\n", resolved.source.normalized_uri.c_str());
        std::fprintf(stderr, "File URI load failed: %s\n", loaded.diagnostic.c_str());
    }
    require(loaded.ok(), "filesystem loader loads file URI bytes");
    require(loaded.status == render_image_source_bytes_load_status::loaded, "file URI load reports loaded");
    require(loaded.encoded_bytes == expected, "file URI load returns file bytes");
    require(loader.load_requests.size() == 1, "filesystem loader records file URI load request");
}

void test_filesystem_loader_loads_asset_uri_bytes()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = test_data_root();
    reset_test_data_root(root);
    const std::filesystem::path asset_path = root / "textures" / "asset.ppm";
    const std::vector<std::byte> expected = make_ppm_1x1_encoded_bytes();
    write_bytes(asset_path, expected);

    const normalizing_image_resolver resolver;
    const render_image_resolve_result resolved = resolver.resolve(
        render_image_resolve_request{.uri = "ASSET://./textures/asset.ppm"});
    require(resolved.ok(), "asset URI source resolves");
    require(resolved.source.kind == render_image_source_kind::asset_uri, "asset URI source kind is preserved");
    require(resolved.source.cache_key() == "asset://textures/asset.ppm", "asset URI cache key is normalized");

    filesystem_image_source_bytes_loader loader(root);
    const render_image_source_bytes_load_result loaded = loader.load(
        render_image_source_bytes_load_request{.source = resolved.source});
    require(loaded.ok(), "filesystem loader loads asset URI bytes");
    require(loaded.status == render_image_source_bytes_load_status::loaded, "asset URI load reports loaded");
    require(loaded.encoded_bytes == expected, "asset URI load returns file bytes through base directory");
}

void test_filesystem_loader_reports_missing_file_as_placeholder_friendly_failure()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = test_data_root();
    reset_test_data_root(root);

    const normalizing_image_resolver resolver;
    const render_image_resolve_result resolved = resolver.resolve(
        render_image_resolve_request{.uri = "textures/missing.ppm"});
    require(resolved.ok(), "missing local source resolves before byte load");

    filesystem_image_source_bytes_loader loader(root);
    const render_image_source_bytes_load_result missing = loader.load(
        render_image_source_bytes_load_request{.source = resolved.source});
    require(!missing.ok(), "missing local file does not load");
    require(
        missing.status == render_image_source_bytes_load_status::missing_bytes,
        "missing local file reports missing bytes");
    require(missing.encoded_bytes.empty(), "missing local file returns no bytes");
    require(!missing.diagnostic.empty(), "missing local file includes placeholder-friendly diagnostic");
}

void test_filesystem_loader_reports_empty_file()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = test_data_root();
    reset_test_data_root(root);
    write_bytes(root / "textures" / "empty.ppm", {});

    const normalizing_image_resolver resolver;
    const render_image_resolve_result resolved = resolver.resolve(
        render_image_resolve_request{.uri = "textures/empty.ppm"});
    require(resolved.ok(), "empty local source resolves before byte load");

    filesystem_image_source_bytes_loader loader(root);
    const render_image_source_bytes_load_result empty = loader.load(
        render_image_source_bytes_load_request{.source = resolved.source});
    require(!empty.ok(), "empty local file does not load as an image source");
    require(empty.status == render_image_source_bytes_load_status::empty_bytes, "empty file reports empty bytes");
    require(empty.encoded_bytes.empty(), "empty file returns no bytes");
    require(!empty.diagnostic.empty(), "empty file includes diagnostic");
}

void test_filesystem_loader_rejects_unsupported_source_kinds()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_image_resolve_result remote = resolver.resolve(
        render_image_resolve_request{.uri = "https://example.test/image.ppm"});
    require(remote.ok(), "remote source resolves by source contract");

    filesystem_image_source_bytes_loader loader(test_data_root());
    const render_image_source_bytes_load_result remote_result = loader.load(
        render_image_source_bytes_load_request{.source = remote.source});
    require(!remote_result.ok(), "remote source does not load through filesystem loader");
    require(
        remote_result.status == render_image_source_bytes_load_status::unsupported_source,
        "remote source reports unsupported source");
    require(!remote_result.diagnostic.empty(), "remote source includes diagnostic");

    const render_resolved_image_source unsupported_source{
        .original_uri = "ftp://example.test/image.ppm",
        .normalized_uri = "ftp://example.test/image.ppm",
        .kind = render_image_source_kind::unsupported,
    };
    const render_image_source_bytes_load_result unsupported = loader.load(
        render_image_source_bytes_load_request{.source = unsupported_source});
    require(!unsupported.ok(), "unsupported source kind does not load through filesystem loader");
    require(
        unsupported.status == render_image_source_bytes_load_status::unsupported_source,
        "unsupported source kind reports unsupported source");
    require(!unsupported.diagnostic.empty(), "unsupported source kind includes diagnostic");
}

void test_filesystem_loader_reports_cache_key_diagnostics()
{
    using namespace quiz_vulkan::render;

    require(is_valid_image_source_bytes_cache_key("textures/valid.ppm"), "valid source bytes key is accepted");
    require(!is_valid_image_source_bytes_cache_key({}), "empty source bytes key is rejected");
    require(!is_valid_image_source_bytes_cache_key("textures/bad\n.ppm"), "control character source key is rejected");

    const render_resolved_image_source invalid_source{
        .original_uri = "textures/bad\n.ppm",
        .normalized_uri = std::string("textures/bad\n.ppm"),
        .kind = render_image_source_kind::local_path,
    };

    filesystem_image_source_bytes_loader loader(test_data_root());
    const render_image_source_bytes_load_result invalid = loader.load(
        render_image_source_bytes_load_request{.source = invalid_source});
    require(!invalid.ok(), "invalid cache key does not load");
    require(
        invalid.status == render_image_source_bytes_load_status::missing_source,
        "invalid cache key reports missing source");
    require(invalid.encoded_bytes.empty(), "invalid cache key returns no bytes");
    require(invalid.diagnostic.find("cache key") != std::string::npos, "invalid cache key diagnostic names cache key");
}

} // namespace

int main()
{
    test_filesystem_loader_loads_local_path_bytes();
    test_filesystem_loader_loads_file_uri_bytes();
    test_filesystem_loader_loads_asset_uri_bytes();
    test_filesystem_loader_reports_missing_file_as_placeholder_friendly_failure();
    test_filesystem_loader_reports_empty_file();
    test_filesystem_loader_rejects_unsupported_source_kinds();
    test_filesystem_loader_reports_cache_key_diagnostics();
    return 0;
}
