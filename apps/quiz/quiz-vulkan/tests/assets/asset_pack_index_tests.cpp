#include "assets/asset_pack_index.h"

#include "asset_pack_index_interface_contract_tests.cpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "asset_pack_index_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::filesystem::path reset_fixture_root()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "quiz_vulkan_asset_pack_index_tests";
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

void test_asset_pack_index_supports_version_required_roots_type_lookup_and_cache_groups()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "fixtures" / "images" / "front.png", "png");
    write_fixture_file(fixture_root / "fixtures" / "fonts" / "Inter.ttf", "ttf");
    write_fixture_file(fixture_root / "fixtures" / "sounds" / "answer.wav", "wav");
    write_fixture_file(fixture_root / "fixtures" / "decks" / "main.quiz", "quiz");

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
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
        .uri = "asset://images/front.png",
        .root_id = "fixture",
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

    require(catalog.ok(), "asset pack index accepts valid pack");
    require(catalog.manifest_version.ok(), "asset pack index accepts matching manifest version");
    require(catalog.invalid_summary.ok(), "asset pack index invalid summary is valid");
    require(catalog.invalid_summary.build_external_placement_count == 1U, "index summarizes build external placements");
    require(catalog.entries.size() == 5U, "asset pack index keeps all resolved entries");

    const std::vector<asset_pack_index_entry> image_entries = catalog.entries_for_type(asset_type::image);
    require(image_entries.size() == 1U, "type-filtered index lookup returns image entries");
    require(image_entries[0].id == "card_front", "type-filtered image entries keep manifest order");
    require(catalog.entries_for_type(asset_type::generic).size() == 5U, "generic type filter returns all entries");

    const asset_pack_index_lookup_result font = catalog.lookup("body_font", asset_type::font);
    require(font.ok(), "asset pack index typed lookup finds font");
    require(font.entry.cache_key == "font|fonts/Inter.ttf", "typed lookup exposes stable font cache key");

    const asset_pack_index_lookup_result wrong_type = catalog.lookup("body_font", asset_type::image);
    require(wrong_type.status == asset_pack_index_lookup_status::type_mismatch, "typed lookup reports type mismatch");
    require(wrong_type.entry.type == asset_type::font, "type mismatch keeps the found entry");

    const asset_pack_index_cache_key_group* shader_group =
        catalog.find_cache_key_group("shader|shaders/ui.vert.spv");
    require(shader_group != nullptr, "cache-key grouping indexes shader cache keys");
    require(shader_group->entry_ids == std::vector<std::string>{"ui_shader"}, "cache-key group entry ids are stable");

    const asset_pack_index_entry* shader = catalog.find("ui_shader");
    require(shader != nullptr, "asset pack index can find shader by id");
    require(
        shader->root_space == asset_runtime_resolver_root_space::build_external,
        "asset pack index preserves build external root classification");
}

void test_asset_pack_index_reports_invalid_version_roots_duplicates_and_missing_files()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "fixtures" / "images" / "front.png", "png");

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root / "fixtures",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://images/front.png",
        .root_id = "fixture",
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

    require(!catalog.ok(), "asset pack index reports invalid pack");
    require(
        catalog.manifest_version.status == asset_pack_manifest_version_status::missing_version,
        "asset pack index reports missing manifest version");
    require(catalog.invalid_summary.manifest_version_issue_count == 1U, "invalid summary counts version issues");
    require(catalog.invalid_summary.validation_issue_count == 3U, "invalid summary counts validation issues");
    require(catalog.invalid_summary.missing_required_root_count == 1U, "invalid summary counts missing roots");
    require(catalog.invalid_summary.duplicate_cache_key_count == 1U, "invalid summary counts duplicate cache keys");
    require(
        catalog.invalid_summary.missing_local_fixture_file_count == 1U,
        "invalid summary counts missing fixture files");

    const asset_pack_index_cache_key_group* duplicate_group =
        catalog.find_cache_key_group("image|asset://images/front.png");
    require(duplicate_group != nullptr, "asset pack index groups duplicate cache keys");
    require(duplicate_group->duplicate(), "asset pack index duplicate cache key group is flagged");
    require(
        duplicate_group->entry_ids == std::vector<std::string>{"card_front", "card_front_copy"},
        "asset pack index duplicate cache key ids are sorted");

    const asset_pack_index_lookup_result missing = catalog.lookup("missing", asset_type::image);
    require(missing.status == asset_pack_index_lookup_status::missing_id, "asset pack index reports missing ids");
}

} // namespace

int main()
{
    test_asset_pack_index_supports_version_required_roots_type_lookup_and_cache_groups();
    test_asset_pack_index_reports_invalid_version_roots_duplicates_and_missing_files();
    return 0;
}
