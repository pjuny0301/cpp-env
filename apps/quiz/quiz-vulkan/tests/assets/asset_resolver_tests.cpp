#include "assets/asset_resolver.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

void test_asset_uri_normalizes_to_package_path()
{
    using namespace quiz_vulkan::assets;

    const normalizing_asset_resolver resolver;
    const asset_resolve_result result = resolver.resolve(asset_resolve_request{
        .type = asset_type::image,
        .uri = "  ASSET:///textures\\cards//./front.png  ",
    });

    require(result.ok(), "asset uri resolves");
    require(result.source.original_uri == "  ASSET:///textures\\cards//./front.png  ", "original uri is preserved");
    require(result.source.normalized_uri == "asset://textures/cards/front.png", "asset uri is canonicalized");
    require(result.source.kind == asset_source_kind::asset_uri, "asset uri kind is classified");
    require(result.source.is_packaged_asset(), "asset uri is packaged");
    require(!result.source.is_remote(), "asset uri is not remote");
    require(result.source.cache_key() == "image|asset://textures/cards/front.png", "cache key includes type and uri");
}

void test_asset_cache_key_partitions_types()
{
    using namespace quiz_vulkan::assets;

    const std::string uri = "asset://shared/menu";
    const std::vector<asset_cache_key> keys{
        make_asset_cache_key(asset_type::font, uri),
        make_asset_cache_key(asset_type::image, uri),
        make_asset_cache_key(asset_type::sound, uri),
        make_asset_cache_key(asset_type::shader, uri),
        make_asset_cache_key(asset_type::deck, uri),
    };

    require(keys[0] == "font|asset://shared/menu", "font cache key is namespaced");
    require(keys[1] == "image|asset://shared/menu", "image cache key is namespaced");
    require(keys[2] == "sound|asset://shared/menu", "sound cache key is namespaced");
    require(keys[3] == "shader|asset://shared/menu", "shader cache key is namespaced");
    require(keys[4] == "deck|asset://shared/menu", "deck cache key is namespaced");
    for (std::size_t left = 0; left < keys.size(); ++left) {
        for (std::size_t right = left + 1; right < keys.size(); ++right) {
            require(keys[left] != keys[right], "cache keys for different asset types do not collide");
        }
    }
}

void test_relative_local_path_normalizes_without_fetching()
{
    using namespace quiz_vulkan::assets;

    const normalizing_asset_resolver resolver;
    const asset_resolve_result result = resolver.resolve(asset_resolve_request{
        .type = asset_type::font,
        .uri = "./fonts//Inter/./Regular.ttf",
    });

    require(result.ok(), "relative local path resolves");
    require(result.source.kind == asset_source_kind::local_path, "relative local path kind is classified");
    require(result.source.normalized_uri == "fonts/Inter/Regular.ttf", "relative local path is normalized");
    require(result.source.cache_key() == "font|fonts/Inter/Regular.ttf", "local path cache key includes type");
}

void test_remote_uri_keeps_payload_and_namespaced_cache_key()
{
    using namespace quiz_vulkan::assets;

    const normalizing_asset_resolver resolver;
    const asset_resolve_result result = resolver.resolve(asset_resolve_request{
        .type = asset_type::deck,
        .uri = "HTTPS://example.test/decks/daily.quiz",
    });

    require(result.ok(), "https uri resolves as a source contract");
    require(result.source.kind == asset_source_kind::https_uri, "https uri kind is classified");
    require(result.source.is_remote(), "https uri is remote");
    require(result.source.normalized_uri == "https://example.test/decks/daily.quiz", "https scheme is lowercase");
    require(result.source.cache_key() == "deck|https://example.test/decks/daily.quiz", "remote key includes type");
}

void test_path_traversal_is_rejected_after_decoding()
{
    using namespace quiz_vulkan::assets;

    const normalizing_asset_resolver resolver;
    const std::vector<std::string> traversal_uris{
        "asset://../secret.png",
        "asset://textures/../secret.png",
        "asset://textures/%2e%2e/secret.png",
        "asset://textures/..%2fsecret.png",
        "../sounds/secret.wav",
        "sounds\\..\\secret.wav",
        "file:///assets/../secret.wav",
    };

    for (const std::string& uri : traversal_uris) {
        const asset_resolve_result result = resolver.resolve(asset_resolve_request{
            .type = asset_type::generic,
            .uri = uri,
        });
        require(!result.ok(), "path traversal uri is rejected");
        require(result.status == asset_resolve_status::path_traversal, "path traversal status is explicit");
    }
}

void test_empty_unsupported_and_absolute_inputs_are_rejected()
{
    using namespace quiz_vulkan::assets;

    const normalizing_asset_resolver resolver;
    const asset_resolve_result empty = resolver.resolve(asset_resolve_request{.uri = "  "});
    require(!empty.ok(), "empty uri is rejected");
    require(empty.status == asset_resolve_status::empty_uri, "empty uri reports empty status");

    const asset_resolve_result unsupported = resolver.resolve(asset_resolve_request{.uri = "ftp://example.test/a.bin"});
    require(!unsupported.ok(), "unsupported scheme is rejected");
    require(unsupported.status == asset_resolve_status::unsupported_scheme, "unsupported scheme status is explicit");

    const asset_resolve_result absolute = resolver.resolve(asset_resolve_request{.uri = "/tmp/asset.bin"});
    require(!absolute.ok(), "absolute local path is rejected");
    require(absolute.status == asset_resolve_status::absolute_path, "absolute path status is explicit");

    const asset_resolve_result drive = resolver.resolve(asset_resolve_request{.uri = "C:\\assets\\font.ttf"});
    require(!drive.ok(), "drive-qualified local path is rejected");
    require(drive.status == asset_resolve_status::absolute_path, "drive-qualified path status is explicit");
}

} // namespace

int main()
{
    test_asset_uri_normalizes_to_package_path();
    test_asset_cache_key_partitions_types();
    test_relative_local_path_normalizes_without_fetching();
    test_remote_uri_keeps_payload_and_namespaced_cache_key();
    test_path_traversal_is_rejected_after_decoding();
    test_empty_unsupported_and_absolute_inputs_are_rejected();
    return 0;
}
