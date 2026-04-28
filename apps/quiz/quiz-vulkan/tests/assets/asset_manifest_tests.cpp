#include "assets/asset_manifest.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "asset_manifest_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::filesystem::path reset_fixture_root()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "quiz_vulkan_asset_manifest_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "fixtures" / "images");
    return root;
}

void test_manifest_normalizes_asset_uri_and_roots_fixture_path()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "  ASSET:///fixtures\\images//./front.png  ",
        .root_id = "fixture",
        .cache_revision = "v1",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result result = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "card_front", .expected_type = asset_type::image},
        resolver);

    require(result.ok(), "manifest asset resolves");
    require(result.asset.source.normalized_uri == "asset://fixtures/images/front.png", "asset uri is normalized");
    require(result.asset.cache_key == "image|asset://fixtures/images/front.png|rev=v1", "cache key includes revision");
    require(result.asset.rooted_path.has_value(), "fixture-rooted path is available");
    require(
        result.asset.rooted_path->lexically_normal()
            == (std::filesystem::absolute(fixture_root) / "fixtures" / "images" / "front.png").lexically_normal(),
        "fixture-rooted path stays under configured root");
}

void test_cache_key_is_stable_for_equivalent_normalized_uris()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "first",
        .type = asset_type::image,
        .uri = "asset:///textures//card.png",
        .cache_revision = "same",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "second",
        .type = asset_type::image,
        .uri = "ASSET://textures/./card.png",
        .cache_revision = "same",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "new_revision",
        .type = asset_type::image,
        .uri = "asset://textures/card.png",
        .cache_revision = "next",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "sound_same_uri",
        .type = asset_type::sound,
        .uri = "asset://textures/card.png",
        .cache_revision = "same",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result first = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "first", .expected_type = asset_type::image},
        resolver);
    const asset_manifest_resolve_result second = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "second", .expected_type = asset_type::image},
        resolver);
    const asset_manifest_resolve_result new_revision = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "new_revision", .expected_type = asset_type::image},
        resolver);
    const asset_manifest_resolve_result sound = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "sound_same_uri", .expected_type = asset_type::sound},
        resolver);

    require(first.ok() && second.ok() && new_revision.ok() && sound.ok(), "cache-key fixtures resolve");
    require(first.asset.cache_key == second.asset.cache_key, "equivalent uris produce stable cache keys");
    require(first.asset.cache_key != new_revision.asset.cache_key, "cache revision changes the cache key");
    require(first.asset.cache_key != sound.asset.cache_key, "asset type partitions cache keys");
}

void test_local_fixture_roots_use_normalized_relative_paths()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "local_font",
        .type = asset_type::font,
        .uri = "./fixtures\\fonts//./Inter.ttf",
        .root_id = "fixture",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result result = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "local_font", .expected_type = asset_type::font},
        resolver);

    require(result.ok(), "local fixture asset resolves");
    require(result.asset.source.normalized_uri == "fixtures/fonts/Inter.ttf", "local fixture uri is normalized");
    require(result.asset.cache_key == "font|fixtures/fonts/Inter.ttf", "local fixture cache key is stable");
    require(result.asset.rooted_path.has_value(), "local fixture path is rooted");
    require(
        result.asset.rooted_path->lexically_normal()
            == (std::filesystem::absolute(fixture_root) / "fixtures" / "fonts" / "Inter.ttf").lexically_normal(),
        "local fixture path stays under configured root");
}

void test_path_traversal_is_rejected_before_rooting()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "asset_traversal",
        .type = asset_type::image,
        .uri = "asset://fixtures/%2e%2e/secret.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "local_traversal",
        .type = asset_type::image,
        .uri = "fixtures\\..\\secret.png",
        .root_id = "fixture",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result asset_result = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "asset_traversal", .expected_type = asset_type::image},
        resolver);
    const asset_manifest_resolve_result local_result = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "local_traversal", .expected_type = asset_type::image},
        resolver);

    require(!asset_result.ok(), "asset traversal is rejected");
    require(
        asset_result.status == asset_manifest_resolve_status::asset_resolve_failed,
        "asset traversal fails during URI resolution");
    require(asset_result.diagnostic == "asset path traversal is not allowed", "asset traversal diagnostic is stable");
    require(!local_result.ok(), "local traversal is rejected");
    require(
        local_result.status == asset_manifest_resolve_status::asset_resolve_failed,
        "local traversal fails during URI resolution");
    require(local_result.diagnostic == "asset path traversal is not allowed", "local traversal diagnostic is stable");
}

void test_missing_root_and_type_mismatch_are_reported()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "font",
        .type = asset_type::font,
        .uri = "asset://fonts/Inter.ttf",
        .root_id = "missing",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result wrong_type = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "font", .expected_type = asset_type::image},
        resolver);
    const asset_manifest_resolve_result missing_root = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "font", .expected_type = asset_type::font},
        resolver);

    require(!wrong_type.ok(), "type mismatch is rejected");
    require(wrong_type.status == asset_manifest_resolve_status::type_mismatch, "type mismatch status is explicit");
    require(!missing_root.ok(), "missing root is rejected");
    require(
        missing_root.status == asset_manifest_resolve_status::root_not_configured,
        "missing root status is explicit");
}

} // namespace

int main()
{
    test_manifest_normalizes_asset_uri_and_roots_fixture_path();
    test_cache_key_is_stable_for_equivalent_normalized_uris();
    test_local_fixture_roots_use_normalized_relative_paths();
    test_path_traversal_is_rejected_before_rooting();
    test_missing_root_and_type_mismatch_are_reported();
    return 0;
}
