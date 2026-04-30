#include "assets/asset_runtime_catalog.h"

#include "asset_runtime_catalog_interface_contract_tests.cpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "asset_runtime_catalog_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::filesystem::path reset_fixture_root()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "quiz_vulkan_asset_runtime_catalog_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "packaged");
    std::filesystem::create_directories(root / "external");
    return root;
}

bool has_runtime_diagnostic(
    const quiz_vulkan::assets::runtime_asset_catalog& catalog,
    quiz_vulkan::assets::asset_manifest_validation_issue_kind kind,
    std::string_view id)
{
    for (const quiz_vulkan::assets::runtime_asset_catalog_diagnostic& diagnostic : catalog.diagnostics) {
        if (diagnostic.kind == kind && diagnostic.id == id) {
            return true;
        }
    }
    return false;
}

void test_runtime_asset_catalog_builds_typed_snapshots()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .aliases = {"images"},
        .root_path = fixture_root / "packaged",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "external",
        .root_path = fixture_root / "external",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "fonts/body.ttf",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "ASSET:///cards/./front.png",
        .root_id = "images",
        .cache_revision = "build42",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "answer_sound",
        .type = asset_type::sound,
        .uri = "sounds/answer.wav",
        .root_id = "external",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.vert.spv",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);

    require(catalog.ok(), "runtime catalog has no diagnostics for valid entries");
    require(catalog.assets.size() == 5U, "runtime catalog contains every resolved entry");

    const runtime_asset_catalog_lookup_result font = catalog.lookup_font("body_font");
    require(font.ok(), "runtime catalog finds font by id");
    require(font.asset.cache_key == "font|fonts/body.ttf", "runtime catalog exposes font cache key");
    require(font.asset.resolved_root_id == "packaged", "runtime catalog records resolved root id");
    require(font.asset.rooted_path.has_value(), "runtime catalog records rooted font path");

    const runtime_asset_catalog_lookup_result image = catalog.lookup_image("card_front");
    require(image.ok(), "runtime catalog finds image by id");
    require(image.asset.source.normalized_uri == "asset://cards/front.png", "runtime catalog snapshots normalized source");
    require(
        image.asset.cache_key == "image|asset://cards/front.png|rev=build42",
        "runtime catalog snapshots manifest cache key");
    require(image.asset.resolved_root_id == "packaged", "runtime catalog resolves root aliases to canonical ids");
    require(
        image.asset.rooted_path == std::filesystem::absolute(fixture_root / "packaged" / "cards" / "front.png")
                                      .lexically_normal(),
        "runtime catalog roots asset uri paths under configured roots");

    const runtime_asset_catalog_lookup_result sound = catalog.lookup_sound("answer_sound");
    require(sound.ok(), "runtime catalog finds sound by id");
    require(sound.asset.source.normalized_uri == "sounds/answer.wav", "runtime catalog snapshots local paths");
    require(sound.asset.resolved_root_id == "external", "runtime catalog records external root id");

    const runtime_asset_catalog_lookup_result shader = catalog.lookup_shader("ui_shader");
    require(shader.ok(), "runtime catalog finds shader by id");
    require(!shader.asset.rooted_path.has_value(), "runtime catalog allows rootless shader sources");

    const runtime_asset_catalog_lookup_result deck = catalog.lookup_deck("main_deck");
    require(deck.ok(), "runtime catalog finds deck by id");
    require(deck.asset.cache_key == "deck|decks/main.quiz", "runtime catalog exposes deck cache key");
    require(catalog.lookup("card_front", asset_type::generic).ok(), "generic runtime lookup accepts concrete assets");
}

void test_runtime_asset_catalog_reports_missing_id_and_type_mismatch()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);

    const runtime_asset_catalog_lookup_result missing = catalog.lookup_image("missing");
    require(
        missing.status == runtime_asset_catalog_lookup_status::missing_id,
        "runtime catalog reports missing ids distinctly");

    const runtime_asset_catalog_lookup_result wrong_type = catalog.lookup_sound("card_front");
    require(
        wrong_type.status == runtime_asset_catalog_lookup_status::type_mismatch,
        "runtime catalog reports type mismatches distinctly");
    require(wrong_type.asset.entry.id == "card_front", "type mismatch result includes the resolved asset snapshot");
}

void test_runtime_asset_catalog_reports_unresolved_assets_and_cache_collisions()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged_a",
        .root_path = fixture_root / "packaged",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged_b",
        .root_path = fixture_root / "external",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_root",
        .type = asset_type::image,
        .uri = "asset://cards/missing-root.png",
        .root_id = "missing",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "traversal",
        .type = asset_type::image,
        .uri = "asset://cards/%2e%2e/secret.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_a",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
        .root_id = "packaged_a",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_b",
        .type = asset_type::image,
        .uri = "ASSET:///cards/./front.png",
        .root_id = "packaged_b",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);

    require(!catalog.ok(), "runtime catalog reports diagnostics for invalid manifests");
    require(
        has_runtime_diagnostic(catalog, asset_manifest_validation_issue_kind::missing_root, "missing_root"),
        "runtime catalog reports missing roots");
    require(
        has_runtime_diagnostic(catalog, asset_manifest_validation_issue_kind::asset_resolve_failed, "traversal"),
        "runtime catalog reports unresolved assets");
    require(
        has_runtime_diagnostic(catalog, asset_manifest_validation_issue_kind::cache_key_collision, "card_b"),
        "runtime catalog reports cache key collisions");

    const runtime_asset_catalog_lookup_result missing_root = catalog.lookup_image("missing_root");
    require(
        missing_root.status == runtime_asset_catalog_lookup_status::unresolved_asset,
        "runtime catalog lookup reports missing-root assets as unresolved");

    const runtime_asset_catalog_lookup_result traversal = catalog.lookup_image("traversal");
    require(
        traversal.status == runtime_asset_catalog_lookup_status::unresolved_asset,
        "runtime catalog lookup reports path traversal assets as unresolved");

    require(catalog.lookup_image("card_a").ok(), "runtime catalog keeps first colliding asset snapshot");
    require(catalog.lookup_image("card_b").ok(), "runtime catalog keeps second colliding asset snapshot");
}

} // namespace

int main()
{
    test_runtime_asset_catalog_builds_typed_snapshots();
    test_runtime_asset_catalog_reports_missing_id_and_type_mismatch();
    test_runtime_asset_catalog_reports_unresolved_assets_and_cache_collisions();
    return 0;
}
