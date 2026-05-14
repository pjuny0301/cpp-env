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

void test_runtime_materialized_lookup_resolves_rooted_paths_for_all_asset_types()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .root_path = fixture_root / "packaged",
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
        .root_id = "packaged",
        .cache_revision = "rev1",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "answer_sound",
        .type = asset_type::sound,
        .uri = "sounds/answer.wav",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.vert.spv",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
        .root_id = "packaged",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);

    const runtime_materialized_asset_lookup_result font = lookup_runtime_materialized_asset(
        catalog,
        runtime_materialized_asset_lookup_request{.id = "body_font", .expected_type = asset_type::font});
    require(font.ok(), "materialized lookup accepts rooted font assets");
    require(font.materialized.source_path == "fonts/body.ttf", "font materialized source path is stable");
    require(
        font.materialized.local_path
            == std::filesystem::absolute(fixture_root / "packaged" / "fonts" / "body.ttf").lexically_normal(),
        "font materialized local path is rooted");

    const runtime_materialized_asset_lookup_result image = lookup_runtime_materialized_asset(
        catalog,
        runtime_materialized_asset_lookup_request{.id = "card_front", .expected_type = asset_type::image});
    require(image.ok(), "materialized lookup accepts rooted image assets");
    require(image.materialized.source_path == "cards/front.png", "image materialized source path strips asset scheme");
    require(image.materialized.cache_key_policy.has_cache_revision(), "image materialized lookup preserves revisions");
    require(image.materialized.cache_key_policy.cache_revision == "rev1", "image materialized revision is parsed");
    require(
        image.materialized.local_path
            == std::filesystem::absolute(fixture_root / "packaged" / "cards" / "front.png").lexically_normal(),
        "image materialized local path is rooted");

    const runtime_materialized_asset_lookup_result sound = lookup_runtime_materialized_asset(
        catalog,
        runtime_materialized_asset_lookup_request{.id = "answer_sound", .expected_type = asset_type::sound});
    require(sound.ok(), "materialized lookup accepts rooted sound assets");
    require(sound.materialized.cache_key_policy.type == asset_type::sound, "sound cache key type is classified");
    require(sound.materialized.source_path == "sounds/answer.wav", "sound materialized source path is stable");

    const runtime_materialized_asset_lookup_result shader = lookup_runtime_materialized_asset(
        catalog,
        runtime_materialized_asset_lookup_request{.id = "ui_shader", .expected_type = asset_type::shader});
    require(shader.ok(), "materialized lookup accepts rooted shader assets");
    require(shader.materialized.source_path == "shaders/ui.vert.spv", "shader materialized source path is stable");
    require(shader.materialized.cache_key_policy.source_kind == asset_source_kind::asset_uri, "shader source kind is classified");

    const runtime_materialized_asset_lookup_result deck = lookup_runtime_materialized_asset(
        catalog,
        runtime_materialized_asset_lookup_request{.id = "main_deck", .expected_type = asset_type::deck});
    require(deck.ok(), "materialized lookup accepts rooted deck assets");
    require(deck.materialized.source_path == "decks/main.quiz", "deck materialized source path is stable");
    require(
        deck.materialized.local_path
            == std::filesystem::absolute(fixture_root / "packaged" / "decks" / "main.quiz").lexically_normal(),
        "deck materialized local path is rooted");
}

void test_runtime_materialized_lookup_reports_lookup_and_path_diagnostics()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .root_path = fixture_root / "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "rootless_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.vert.spv",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "remote_image",
        .type = asset_type::image,
        .uri = "https://example.test/cards/front.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_root",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
        .root_id = "missing",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);

    const runtime_materialized_asset_lookup_result missing = lookup_runtime_materialized_asset(
        catalog,
        runtime_materialized_asset_lookup_request{.id = "missing", .expected_type = asset_type::image});
    require(
        missing.status == runtime_materialized_asset_lookup_status::missing_id,
        "materialized lookup reports missing ids");
    require(!missing.diagnostic.empty(), "missing materialized lookup includes diagnostics");

    const runtime_materialized_asset_lookup_result wrong_type = lookup_runtime_materialized_asset(
        catalog,
        runtime_materialized_asset_lookup_request{.id = "rootless_shader", .expected_type = asset_type::sound});
    require(
        wrong_type.status == runtime_materialized_asset_lookup_status::type_mismatch,
        "materialized lookup reports type mismatches");
    require(
        wrong_type.materialized.asset.entry.id == "rootless_shader",
        "type mismatch materialized lookup keeps the catalog snapshot");

    const runtime_materialized_asset_lookup_result unresolved = lookup_runtime_materialized_asset(
        catalog,
        runtime_materialized_asset_lookup_request{.id = "missing_root", .expected_type = asset_type::deck});
    require(
        unresolved.status == runtime_materialized_asset_lookup_status::unresolved_asset,
        "materialized lookup reports unresolved manifest entries");

    const runtime_materialized_asset_lookup_result rootless = lookup_runtime_materialized_asset(
        catalog,
        runtime_materialized_asset_lookup_request{.id = "rootless_shader", .expected_type = asset_type::shader});
    require(
        rootless.status == runtime_materialized_asset_lookup_status::missing_rooted_path,
        "materialized lookup requires rooted local paths");

    const runtime_materialized_asset_lookup_result remote = lookup_runtime_materialized_asset(
        catalog,
        runtime_materialized_asset_lookup_request{.id = "remote_image", .expected_type = asset_type::image});
    require(
        remote.status == runtime_materialized_asset_lookup_status::unsupported_source_kind,
        "materialized lookup rejects remote sources before local file materialization");

    runtime_asset_catalog_snapshot unsafe_snapshot{
        .entry = asset_manifest_entry{
            .id = "unsafe_deck",
            .type = asset_type::deck,
            .uri = "decks/main.quiz",
        },
        .source = resolved_asset_source{
            .original_uri = "decks/main.quiz",
            .normalized_uri = "decks/main.quiz",
            .kind = asset_source_kind::local_path,
            .type = asset_type::deck,
        },
        .cache_key = "deck|decks/main.quiz",
        .resolved_root_id = "packaged",
        .rooted_path = fixture_root / "packaged" / ".." / "main.quiz",
    };
    const runtime_materialized_asset_lookup_result unsafe = materialize_runtime_asset(unsafe_snapshot);
    require(
        unsafe.status == runtime_materialized_asset_lookup_status::invalid_rooted_path,
        "materialized snapshot rejects unsafe rooted paths");

    unsafe_snapshot.cache_key = "deck|decks/other.quiz";
    unsafe_snapshot.rooted_path = std::filesystem::absolute(fixture_root / "packaged" / "decks" / "main.quiz");
    const runtime_materialized_asset_lookup_result mismatched_key = materialize_runtime_asset(unsafe_snapshot);
    require(
        mismatched_key.status == runtime_materialized_asset_lookup_status::cache_key_mismatch,
        "materialized snapshot rejects mismatched cache keys");

    runtime_asset_catalog_snapshot source_path_mismatch_snapshot{
        .entry = asset_manifest_entry{
            .id = "wrong_file",
            .type = asset_type::shader,
            .uri = "asset://shaders/ui.vert.spv",
        },
        .source = resolved_asset_source{
            .original_uri = "asset://shaders/ui.vert.spv",
            .normalized_uri = "asset://shaders/ui.vert.spv",
            .kind = asset_source_kind::asset_uri,
            .type = asset_type::shader,
        },
        .cache_key = "shader|asset://shaders/ui.vert.spv",
        .resolved_root_id = "packaged",
        .rooted_path = std::filesystem::absolute(fixture_root / "packaged" / "shaders" / "other.vert.spv"),
    };
    const runtime_materialized_asset_lookup_result source_path_mismatch =
        materialize_runtime_asset(source_path_mismatch_snapshot);
    require(
        source_path_mismatch.status == runtime_materialized_asset_lookup_status::source_path_mismatch,
        "materialized snapshot rejects rooted paths that do not match the normalized source path");
    require(!source_path_mismatch.diagnostic.empty(), "source path mismatch includes diagnostics");

    runtime_asset_catalog_snapshot noncanonical_snapshot{
        .entry = asset_manifest_entry{
            .id = "noncanonical_image",
            .type = asset_type::image,
            .uri = "asset:///cards/front.png",
        },
        .source = resolved_asset_source{
            .original_uri = "asset:///cards/front.png",
            .normalized_uri = "asset:///cards/front.png",
            .kind = asset_source_kind::asset_uri,
            .type = asset_type::image,
        },
        .cache_key = "image|asset:///cards/front.png",
        .resolved_root_id = "packaged",
        .rooted_path = std::filesystem::absolute(fixture_root / "packaged" / "cards" / "front.png"),
    };
    const runtime_materialized_asset_lookup_result noncanonical = materialize_runtime_asset(noncanonical_snapshot);
    require(
        noncanonical.status == runtime_materialized_asset_lookup_status::noncanonical_cache_key,
        "materialized snapshot rejects noncanonical cache keys");
}

} // namespace

int main()
{
    test_runtime_asset_catalog_builds_typed_snapshots();
    test_runtime_asset_catalog_reports_missing_id_and_type_mismatch();
    test_runtime_asset_catalog_reports_unresolved_assets_and_cache_collisions();
    test_runtime_materialized_lookup_resolves_rooted_paths_for_all_asset_types();
    test_runtime_materialized_lookup_reports_lookup_and_path_diagnostics();
    return 0;
}
