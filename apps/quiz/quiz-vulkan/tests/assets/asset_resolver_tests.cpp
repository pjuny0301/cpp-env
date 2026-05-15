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

void test_asset_cache_key_classification_extracts_source_policy()
{
    using namespace quiz_vulkan::assets;

    const asset_cache_key_classification shader =
        classify_asset_cache_key("shader|asset://shaders/ui.vert.spv|rev=pack-2026.05");
    require(shader.ok(), "shader cache key classification is accepted");
    require(shader.status == asset_cache_key_policy_status::accepted, "shader cache key status is accepted");
    require(shader.type == asset_type::shader, "shader cache key type is parsed");
    require(shader.source_kind == asset_source_kind::asset_uri, "shader cache key source kind is parsed");
    require(shader.type_component == "shader", "shader cache key type component is exposed");
    require(
        shader.source_component == "asset://shaders/ui.vert.spv",
        "shader cache key source component is exposed");
    require(shader.revision_component == "rev=pack-2026.05", "shader cache key revision component is exposed");
    require(shader.normalized_uri == "asset://shaders/ui.vert.spv", "shader cache key uri is preserved");
    require(shader.source_path == "shaders/ui.vert.spv", "shader cache key source path is available");
    require(shader.has_cache_revision(), "shader cache key revision is detected");
    require(shader.cache_revision == "pack-2026.05", "shader cache key revision is parsed");
    require(
        asset_cache_key_is_canonical("shader|asset://shaders/ui.vert.spv|rev=pack-2026.05"),
        "canonical shader cache key is accepted by helper");

    const asset_cache_key_classification deck = classify_asset_cache_key("deck|decks/main.quiz");
    require(deck.ok(), "deck cache key classification is accepted");
    require(deck.type == asset_type::deck, "deck cache key type is parsed");
    require(deck.source_kind == asset_source_kind::local_path, "deck cache key local source is classified");
    require(deck.type_component == "deck", "deck cache key type component is exposed");
    require(deck.source_component == "decks/main.quiz", "deck cache key source component is exposed");
    require(deck.revision_component.empty(), "deck cache key has no revision component");
    require(deck.source_path == "decks/main.quiz", "deck cache key local source path is available");
    require(!deck.has_cache_revision(), "deck cache key without rev has no revision");

    const asset_cache_key_classification remote =
        classify_asset_cache_key("image|https://example.test/cards/front.png");
    require(remote.ok(), "remote image cache key classification is accepted");
    require(remote.source_kind == asset_source_kind::https_uri, "remote image cache key source kind is parsed");
    require(remote.type_component == "image", "remote cache key type component is exposed");
    require(
        remote.source_component == "https://example.test/cards/front.png",
        "remote cache key source component is exposed");
    require(remote.source_path.empty(), "remote cache keys do not claim a rooted source path");
}

void test_asset_cache_key_classification_rejects_ambiguous_or_noncanonical_keys()
{
    using namespace quiz_vulkan::assets;

    const asset_cache_key_classification empty = classify_asset_cache_key("");
    require(!empty.ok(), "empty cache key is rejected");
    require(empty.status == asset_cache_key_policy_status::empty_key, "empty cache key status is explicit");

    const asset_cache_key_classification missing_separator = classify_asset_cache_key("image");
    require(!missing_separator.ok(), "cache key without separator is rejected");
    require(
        missing_separator.status == asset_cache_key_policy_status::missing_type_separator,
        "missing separator status is explicit");

    const asset_cache_key_classification unsupported = classify_asset_cache_key("video|asset://movies/intro.mp4");
    require(!unsupported.ok(), "unsupported cache key type is rejected");
    require(
        unsupported.status == asset_cache_key_policy_status::unsupported_asset_type,
        "unsupported type status is explicit");
    require(unsupported.type_component == "video", "unsupported type component remains inspectable");

    const asset_cache_key_classification missing_source = classify_asset_cache_key("image|");
    require(!missing_source.ok(), "cache key without source uri is rejected");
    require(
        missing_source.status == asset_cache_key_policy_status::missing_source_uri,
        "missing source status is explicit");

    const asset_cache_key_classification unsupported_scheme =
        classify_asset_cache_key("image|ftp://example.test/cards/front.png");
    require(!unsupported_scheme.ok(), "cache key rejects unsupported source schemes");
    require(
        unsupported_scheme.status == asset_cache_key_policy_status::unsupported_source_scheme,
        "unsupported source scheme status is explicit");
    require(unsupported_scheme.source_kind == asset_source_kind::unsupported, "unsupported source kind is exposed");
    require(
        unsupported_scheme.source_component == "ftp://example.test/cards/front.png",
        "unsupported scheme keeps source component inspectable");

    const asset_cache_key_classification asset_slashes = classify_asset_cache_key("image|asset:///cards/front.png");
    require(!asset_slashes.ok(), "cache key requires canonical asset uri slashes");
    require(
        asset_slashes.status == asset_cache_key_policy_status::noncanonical_source_uri,
        "noncanonical slash status is explicit");

    const asset_cache_key_classification asset_dot_segment =
        classify_asset_cache_key("image|asset://cards/./front.png");
    require(!asset_dot_segment.ok(), "cache key requires normalized asset uri path segments");
    require(
        asset_dot_segment.status == asset_cache_key_policy_status::noncanonical_source_uri,
        "dot segment status is explicit");

    const asset_cache_key_classification traversal =
        classify_asset_cache_key("sound|asset://sounds/%2e%2e/secret.wav");
    require(!traversal.ok(), "cache key traversal is rejected after decoding");
    require(
        traversal.status == asset_cache_key_policy_status::path_traversal,
        "cache key traversal status is explicit");

    const asset_cache_key_classification bad_revision =
        classify_asset_cache_key("font|asset://fonts/body.ttf|version=v2");
    require(!bad_revision.ok(), "cache key rejects unknown revision fields");
    require(
        bad_revision.status == asset_cache_key_policy_status::invalid_revision,
        "unknown revision field status is explicit");
    require(
        bad_revision.source_component == "asset://fonts/body.ttf",
        "invalid revision keeps source component inspectable");
    require(
        bad_revision.revision_component == "version=v2",
        "invalid revision keeps revision component inspectable");

    const asset_cache_key_classification empty_revision =
        classify_asset_cache_key("font|asset://fonts/body.ttf|rev=");
    require(!empty_revision.ok(), "cache key rejects empty revision fields");
    require(
        empty_revision.status == asset_cache_key_policy_status::invalid_revision,
        "empty revision field status is explicit");

    const asset_cache_key_classification extra_separator =
        classify_asset_cache_key("font|asset://fonts/body.ttf|rev=v2|extra");
    require(!extra_separator.ok(), "cache key rejects additional separators after revision");
    require(
        extra_separator.status == asset_cache_key_policy_status::invalid_revision,
        "extra separator revision status is explicit");

    require(
        !asset_cache_key_is_canonical("image|asset:///cards/front.png"),
        "noncanonical cache key is rejected by helper");
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
    test_asset_cache_key_classification_extracts_source_policy();
    test_asset_cache_key_classification_rejects_ambiguous_or_noncanonical_keys();
    test_relative_local_path_normalizes_without_fetching();
    test_remote_uri_keeps_payload_and_namespaced_cache_key();
    test_path_traversal_is_rejected_after_decoding();
    test_empty_unsupported_and_absolute_inputs_are_rejected();
    return 0;
}
