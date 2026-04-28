#include "assets/asset_manifest.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

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

bool has_issue(
    const quiz_vulkan::assets::asset_manifest_validation_result& result,
    quiz_vulkan::assets::asset_manifest_validation_issue_kind kind,
    std::string_view id)
{
    for (const quiz_vulkan::assets::asset_manifest_validation_issue& issue : result.issues) {
        if (issue.kind == kind && issue.id == id) {
            return true;
        }
    }
    return false;
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

void test_manifest_root_alias_resolves_to_canonical_root()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .aliases = {"fixture_alias", "images"},
        .root_path = fixture_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "aliased_card",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
        .root_id = "images",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_resolve_result result = resolve_asset_manifest_entry(
        manifest,
        asset_manifest_resolve_request{.id = "aliased_card", .expected_type = asset_type::image},
        resolver);

    require(result.ok(), "root alias resolves manifest entry");
    require(result.asset.rooted_path.has_value(), "aliased root produces rooted path");
    require(
        result.asset.rooted_path->lexically_normal()
            == (std::filesystem::absolute(fixture_root) / "cards" / "front.png").lexically_normal(),
        "root alias uses the canonical root path");
}

void test_manifest_validation_reports_duplicate_ids_and_missing_roots()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root,
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root / "other",
    });
    manifest.roots.push_back(asset_manifest_root{.id = "broken"});
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shared",
        .type = asset_type::image,
        .uri = "asset://images/card.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shared",
        .type = asset_type::image,
        .uri = "asset://images/other.png",
        .root_id = "missing",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "empty_uri",
        .type = asset_type::image,
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(!result.ok(), "invalid manifest reports validation issues");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::duplicate_root_id, "fixture"),
        "duplicate root id is reported");
    require(has_issue(result, asset_manifest_validation_issue_kind::invalid_root, "broken"), "invalid root is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::duplicate_entry_id, "shared"),
        "duplicate entry id is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::missing_root, "shared"),
        "missing root reference is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::invalid_entry, "empty_uri"),
        "invalid entry is reported");
}

void test_manifest_validation_reports_root_alias_collisions()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .aliases = {"images", "shared"},
        .root_path = fixture_root / "fixture",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "shared",
        .root_path = fixture_root / "shared",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "alternate",
        .aliases = {"images"},
        .root_path = fixture_root / "alternate",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "self_alias",
        .aliases = {"self_alias"},
        .root_path = fixture_root / "self_alias",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "empty_alias",
        .aliases = {""},
        .root_path = fixture_root / "empty_alias",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(!result.ok(), "root alias collisions make manifest validation fail");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::duplicate_root_id, "shared"),
        "root id collision with prior alias is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::duplicate_root_id, "images"),
        "root alias collision with prior alias is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::duplicate_root_id, "self_alias"),
        "root alias collision with its own id is reported");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::invalid_root, "empty_alias"),
        "empty root alias is reported");
}

void test_manifest_validation_reports_resolver_failures()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "traversal",
        .type = asset_type::image,
        .uri = "asset://images/%2e%2e/secret.png",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(!result.ok(), "resolver failures make manifest validation fail");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::asset_resolve_failed, "traversal"),
        "path traversal resolver failure is reported");
}

void test_manifest_validation_detects_rooted_cache_key_collisions()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture_a",
        .root_path = root / "a",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture_b",
        .root_path = root / "b",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_a",
        .type = asset_type::image,
        .uri = "images/card.png",
        .root_id = "fixture_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_b",
        .type = asset_type::image,
        .uri = "./images/./card.png",
        .root_id = "fixture_b",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(!result.ok(), "rooted cache-key collision makes manifest validation fail");
    require(
        has_issue(result, asset_manifest_validation_issue_kind::cache_key_collision, "card_b"),
        "cache-key collision is reported on later entry");
}

void test_manifest_validation_allows_equivalent_aliases_to_same_rooted_path()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_a",
        .type = asset_type::image,
        .uri = "images/card.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_b",
        .type = asset_type::image,
        .uri = "./images/./card.png",
        .root_id = "fixture",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_validation_result result = validate_asset_manifest(manifest, resolver);

    require(result.ok(), "equivalent aliases to the same rooted path validate");
}

} // namespace

int main()
{
    test_manifest_normalizes_asset_uri_and_roots_fixture_path();
    test_cache_key_is_stable_for_equivalent_normalized_uris();
    test_local_fixture_roots_use_normalized_relative_paths();
    test_path_traversal_is_rejected_before_rooting();
    test_missing_root_and_type_mismatch_are_reported();
    test_manifest_root_alias_resolves_to_canonical_root();
    test_manifest_validation_reports_duplicate_ids_and_missing_roots();
    test_manifest_validation_reports_root_alias_collisions();
    test_manifest_validation_reports_resolver_failures();
    test_manifest_validation_detects_rooted_cache_key_collisions();
    test_manifest_validation_allows_equivalent_aliases_to_same_rooted_path();
    return 0;
}
