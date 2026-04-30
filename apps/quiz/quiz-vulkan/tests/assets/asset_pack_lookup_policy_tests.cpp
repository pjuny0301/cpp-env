#include "assets/asset_pack_index.h"

#include "asset_pack_index_interface_contract_tests.cpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "asset_pack_lookup_policy_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::filesystem::path reset_fixture_root()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "quiz_vulkan_asset_pack_lookup_policy_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "fixtures" / "images");
    std::filesystem::create_directories(root / "fixtures" / "fonts");
    std::filesystem::create_directories(root / "fixtures" / "sounds");
    std::filesystem::create_directories(root / "fixtures" / "decks");
    std::filesystem::create_directories(root / "build" / "external" / "shader_pack" / "shaders");
    return root;
}

void write_fixture_file(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

void test_asset_pack_lookup_policy_builds_stable_snapshot_views_and_typed_lookup_reports()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "fixtures" / "images" / "front.png", "png");
    write_fixture_file(fixture_root / "fixtures" / "fonts" / "Inter.ttf", "ttf");
    write_fixture_file(fixture_root / "fixtures" / "sounds" / "answer.wav", "wav");
    write_fixture_file(fixture_root / "fixtures" / "decks" / "main.quiz", "quiz");
    write_fixture_file(fixture_root / "build" / "external" / "shader_pack" / "shaders" / "ui.vert.spv", "spv");

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .aliases = {"images"},
        .root_path = fixture_root / "fixtures",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "external_shader_pack",
        .root_path = fixture_root / "build" / "external" / "shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "fonts/Inter.ttf",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "ASSET:///images/./front.png",
        .root_id = "images",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "answer_sound",
        .type = asset_type::sound,
        .uri = "sounds/answer.wav",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "shaders/ui.vert.spv",
        .root_id = "external_shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
        .root_id = "fixture",
    });

    const normalizing_asset_resolver resolver;
    const asset_pack_index_catalog catalog = build_asset_pack_index(
        manifest,
        resolver,
        asset_pack_index_request{
            .manifest_version = "pack.v1",
            .expected_manifest_version = "pack.v1",
            .require_manifest_version = true,
            .required_roots = {
                asset_pack_required_root{.root_id = "fixture"},
                asset_pack_required_root{.root_id = "external_shader_pack"},
            },
        });

    require(catalog.ok(), "valid lookup pack is accepted");
    require(catalog.manifest_version.ok(), "valid lookup pack has a matching version");

    const asset_pack_index_catalog_snapshot_view snapshot = make_asset_pack_index_catalog_snapshot_view(catalog);
    require(snapshot.entries.size() == 5U, "snapshot view keeps all resolved entries");
    require(snapshot.entries[0].id == "body_font", "snapshot view orders font entries first");
    require(snapshot.entries[1].id == "card_front", "snapshot view orders image entries second");
    require(snapshot.entries[2].id == "answer_sound", "snapshot view orders sound entries third");
    require(snapshot.entries[3].id == "ui_shader", "snapshot view orders shader entries fourth");
    require(snapshot.entries[4].id == "main_deck", "snapshot view orders deck entries last");
    require(snapshot.entries[1].cache_key == "image|asset://images/front.png", "snapshot view keeps normalized cache keys");
    require(snapshot.cache_key_groups.size() == 5U, "snapshot view keeps stable cache-key groups");
    require(snapshot.cache_key_groups[0].cache_key == "font|fonts/Inter.ttf", "cache-key groups stay sorted by type");
    require(snapshot.cache_key_groups[3].cache_key == "shader|shaders/ui.vert.spv", "shader cache key groups remain stable");

    require(catalog.lookup_font("body_font").ok(), "typed font lookup succeeds");
    require(catalog.lookup_image("card_front").ok(), "typed image lookup succeeds");
    require(catalog.lookup_sound("answer_sound").ok(), "typed sound lookup succeeds");
    require(catalog.lookup_shader("ui_shader").ok(), "typed shader lookup succeeds");
    require(catalog.lookup_deck("main_deck").ok(), "typed deck lookup succeeds");

    const asset_pack_index_lookup_report report = summarize_asset_pack_index_lookup_requests(
        catalog,
        std::vector<asset_pack_index_lookup_request>{
            asset_pack_index_lookup_request{.id = "body_font", .expected_type = asset_type::font},
            asset_pack_index_lookup_request{.id = "missing", .expected_type = asset_type::image},
            asset_pack_index_lookup_request{.id = "body_font", .expected_type = asset_type::image},
            asset_pack_index_lookup_request{.id = "main_deck", .expected_type = asset_type::deck},
        });

    require(!report.ok(), "lookup report surfaces rejected requests");
    require(report.requests.size() == 4U, "lookup report keeps request order");
    require(report.diagnostics.size() == 4U, "lookup report keeps diagnostic order");
    require(report.found_count == 2U, "lookup report counts found requests");
    require(report.missing_id_count == 1U, "lookup report counts missing ids");
    require(report.type_mismatch_count == 1U, "lookup report counts type mismatches");
    require(report.cache_key_groups.size() == 5U, "lookup report keeps cache-key groups");

    require(report.diagnostics[0].request_index == 0U, "first lookup diagnostic keeps request index");
    require(report.diagnostics[0].ok(), "first lookup diagnostic is found");
    require(report.diagnostics[0].entry.id == "body_font", "first lookup diagnostic includes resolved entry");
    require(
        report.diagnostics[1].status == asset_pack_index_lookup_status::missing_id,
        "second lookup diagnostic reports missing ids");
    require(
        report.diagnostics[2].status == asset_pack_index_lookup_status::type_mismatch,
        "third lookup diagnostic reports type mismatches");
    require(report.diagnostics[2].entry.type == asset_type::font, "type mismatch diagnostic keeps the found entry");
    require(report.diagnostics[3].entry.id == "main_deck", "last lookup diagnostic keeps manifest order");
}

void test_asset_pack_lookup_policy_reports_invalid_summary_and_duplicate_cache_keys()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "fixtures" / "images" / "front.png", "png");

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .aliases = {"images"},
        .root_path = fixture_root / "fixtures",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "external_shader_pack",
        .root_path = fixture_root / "build" / "external" / "shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://images/front.png",
        .root_id = "images",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front_copy",
        .type = asset_type::image,
        .uri = "ASSET:///images/./front.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_sound",
        .type = asset_type::sound,
        .uri = "sounds/missing.wav",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "shaders/ui.vert.spv",
        .root_id = "external_shader_pack",
    });

    const normalizing_asset_resolver resolver;
    const asset_pack_index_catalog catalog = build_asset_pack_index(
        manifest,
        resolver,
        asset_pack_index_request{
            .expected_manifest_version = "pack.v1",
            .require_manifest_version = true,
            .required_roots = {
                asset_pack_required_root{.root_id = "fixture"},
                asset_pack_required_root{.root_id = "missing_required_root"},
            },
        });

    require(!catalog.ok(), "invalid lookup pack is rejected");
    require(
        catalog.manifest_version.status == asset_pack_manifest_version_status::missing_version,
        "lookup pack reports missing manifest version");
    require(catalog.invalid_summary.manifest_version_issue_count == 1U, "invalid summary counts manifest version issues");
    require(catalog.invalid_summary.missing_required_root_count == 1U, "invalid summary counts missing roots");
    require(catalog.invalid_summary.duplicate_cache_key_count == 1U, "invalid summary counts duplicate cache keys");
    require(
        catalog.invalid_summary.missing_local_fixture_file_count == 1U,
        "invalid summary counts missing local fixture files");
    require(catalog.invalid_summary.build_external_placement_count == 1U, "invalid summary counts external placements");

    const asset_pack_index_catalog_snapshot_view snapshot = make_asset_pack_index_catalog_snapshot_view(catalog);
    require(snapshot.entries.size() == 4U, "invalid snapshot still keeps resolved entries");
    require(snapshot.entries[0].id == "card_front", "invalid snapshot keeps deterministic type ordering");
    require(snapshot.entries[1].id == "card_front_copy", "invalid snapshot keeps duplicate cache-key order");
    require(snapshot.entries[3].id == "ui_shader", "invalid snapshot keeps shader entries last");

    const asset_pack_index_cache_key_group* duplicate_group =
        catalog.find_cache_key_group("image|asset://images/front.png");
    require(duplicate_group != nullptr, "duplicate cache-key group is present");
    require(duplicate_group->duplicate(), "duplicate cache-key group is flagged");
    require(
        duplicate_group->entry_ids == std::vector<std::string>{"card_front", "card_front_copy"},
        "duplicate cache-key group keeps sorted ids");

    const asset_pack_index_lookup_report report = summarize_asset_pack_index_lookup_requests(
        catalog,
        std::vector<asset_pack_index_lookup_request>{
            asset_pack_index_lookup_request{.id = "card_front", .expected_type = asset_type::image},
            asset_pack_index_lookup_request{.id = "missing", .expected_type = asset_type::sound},
        });

    require(!report.ok(), "lookup report reports missing ids");
    require(report.found_count == 1U, "lookup report counts found entries");
    require(report.missing_id_count == 1U, "lookup report counts missing ids in invalid packs");
}

} // namespace

int main()
{
    test_asset_pack_lookup_policy_builds_stable_snapshot_views_and_typed_lookup_reports();
    test_asset_pack_lookup_policy_reports_invalid_summary_and_duplicate_cache_keys();
    return 0;
}
