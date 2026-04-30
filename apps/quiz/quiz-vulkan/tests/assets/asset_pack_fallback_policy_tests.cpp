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
        std::cerr << "asset_pack_fallback_policy_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::filesystem::path reset_fixture_root()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "quiz_vulkan_asset_pack_fallback_policy_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "fixtures" / "images");
    std::filesystem::create_directories(root / "fixtures" / "fonts");
    std::filesystem::create_directories(root / "fixtures" / "sounds");
    std::filesystem::create_directories(root / "build" / "external" / "shader_pack" / "shaders");
    return root;
}

void write_fixture_file(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

const quiz_vulkan::assets::asset_pack_index_root_selection_entry* find_root_selection_entry(
    const quiz_vulkan::assets::asset_pack_index_root_selection_summary& summary,
    std::string_view id)
{
    for (const quiz_vulkan::assets::asset_pack_index_root_selection_entry& entry : summary.entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

void test_asset_pack_fallback_policy_tracks_alias_fallback_and_missing_preferred_roots()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "fixtures" / "images" / "front.png", "png");
    write_fixture_file(fixture_root / "fixtures" / "fonts" / "Inter.ttf", "ttf");
    write_fixture_file(fixture_root / "fixtures" / "sounds" / "answer.wav", "wav");
    write_fixture_file(fixture_root / "build" / "external" / "shader_pack" / "shaders" / "ui.vert.spv", "spv");

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .aliases = {"images", "shared"},
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
        .id = "body_font",
        .type = asset_type::font,
        .uri = "fonts/Inter.ttf",
        .root_id = "packaged",
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
        .uri = "shaders/ui.vert.spv",
        .root_id = "external_shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
    });

    const normalizing_asset_resolver resolver;
    const asset_pack_index_catalog catalog = build_asset_pack_index(
        manifest,
        resolver,
        asset_pack_index_request{
            .manifest_version = "pack.v2",
            .expected_manifest_version = "pack.v2",
            .require_manifest_version = true,
            .required_roots = {
                asset_pack_required_root{.root_id = "packaged"},
                asset_pack_required_root{.root_id = "external_shader_pack"},
            },
        });

    require(catalog.ok(), "fallback policy pack remains valid");

    const asset_pack_index_root_selection_summary root_selection =
        summarize_asset_pack_index_root_selection(manifest);
    require(root_selection.entries.size() == 5U, "root selection summary keeps manifest order");
    require(root_selection.direct_root_count == 3U, "root selection counts direct roots");
    require(root_selection.fallback_root_count == 1U, "root selection counts alias fallback roots");
    require(root_selection.missing_preferred_root_count == 0U, "root selection has no missing preferred roots");
    require(root_selection.no_root_requested_count == 1U, "root selection counts rootless entries");

    const asset_pack_index_root_selection_entry* image_selection = find_root_selection_entry(root_selection, "card_front");
    require(image_selection != nullptr, "root selection finds alias-backed entry");
    require(
        image_selection->status == asset_pack_index_root_selection_status::selected_fallback_root,
        "alias-backed root selection is marked as fallback");
    require(image_selection->selected_root_id == "packaged", "alias-backed selection records canonical root id");

    const asset_pack_index_root_selection_entry* font_selection = find_root_selection_entry(root_selection, "body_font");
    require(font_selection != nullptr, "root selection finds preferred-root entry");
    require(
        font_selection->status == asset_pack_index_root_selection_status::selected_direct_root,
        "preferred-root selection is marked as direct");

    const asset_pack_index_root_selection_entry* rootless_selection =
        find_root_selection_entry(root_selection, "main_deck");
    require(rootless_selection != nullptr, "root selection finds rootless entry");
    require(
        rootless_selection->status == asset_pack_index_root_selection_status::no_root_requested,
        "rootless entries are reported separately");

    const asset_pack_index_lookup_policy_summary lookup_policy = summarize_asset_pack_index_lookup_policy(
        manifest,
        catalog,
        std::vector<asset_pack_index_lookup_request>{
            asset_pack_index_lookup_request{.id = "card_front", .expected_type = asset_type::image},
            asset_pack_index_lookup_request{.id = "missing", .expected_type = asset_type::font},
            asset_pack_index_lookup_request{.id = "main_deck", .expected_type = asset_type::deck},
        });

    require(!lookup_policy.ok(), "lookup policy reports missing ids in the lookup summary");
    require(lookup_policy.lookup.found_count == 2U, "lookup policy counts found lookups");
    require(lookup_policy.lookup.missing_id_count == 1U, "lookup policy counts missing lookups");
    require(lookup_policy.root_selection.fallback_root_count == 1U, "lookup policy keeps fallback root summary");
}

void test_asset_pack_fallback_policy_reports_missing_preferred_roots_in_lookup_policy()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "fixtures" / "images" / "front.png", "png");

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .aliases = {"images"},
        .root_path = fixture_root / "fixtures",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://images/front.png",
        .root_id = "images",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_sound",
        .type = asset_type::sound,
        .uri = "sounds/missing.wav",
        .root_id = "missing_root",
    });

    const normalizing_asset_resolver resolver;
    const asset_pack_index_catalog catalog = build_asset_pack_index(
        manifest,
        resolver,
        asset_pack_index_request{
            .expected_manifest_version = "pack.v2",
            .require_manifest_version = true,
            .required_roots = {asset_pack_required_root{.root_id = "packaged"}},
        });

    require(!catalog.ok(), "missing preferred root keeps pack invalid");
    require(
        catalog.invalid_summary.missing_required_root_count == 0U,
        "missing preferred roots are not conflated with required-root validation");

    const asset_pack_index_root_selection_summary root_selection =
        summarize_asset_pack_index_root_selection(manifest);
    require(root_selection.missing_preferred_root_count == 1U, "missing preferred root is summarized");

    const asset_pack_index_lookup_policy_summary lookup_policy = summarize_asset_pack_index_lookup_policy(
        manifest,
        catalog,
        std::vector<asset_pack_index_lookup_request>{
            asset_pack_index_lookup_request{.id = "card_front", .expected_type = asset_type::image},
            asset_pack_index_lookup_request{.id = "missing_sound", .expected_type = asset_type::sound},
        });

    require(!lookup_policy.ok(), "lookup policy reports missing preferred roots when they exist");
    require(
        lookup_policy.root_selection.missing_preferred_root_count == 1U,
        "lookup policy keeps missing preferred root count");
    require(
        lookup_policy.root_selection.entries[1].status == asset_pack_index_root_selection_status::missing_preferred_root,
        "lookup policy preserves missing preferred root diagnostic order");
}

} // namespace

int main()
{
    test_asset_pack_fallback_policy_tracks_alias_fallback_and_missing_preferred_roots();
    test_asset_pack_fallback_policy_reports_missing_preferred_roots_in_lookup_policy();
    return 0;
}
