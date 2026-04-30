#include "assets/asset_pack_validation.h"

#include "asset_pack_validation_interface_contract_tests.cpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "asset_pack_validation_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::filesystem::path reset_fixture_root()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "quiz_vulkan_asset_pack_validation_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "fixtures" / "images");
    std::filesystem::create_directories(root / "fixtures" / "fonts");
    std::filesystem::create_directories(root / "build" / "external" / "shader_pack" / "shaders");
    return root;
}

void write_fixture_file(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

const quiz_vulkan::assets::asset_pack_validation_issue* find_issue(
    const quiz_vulkan::assets::asset_pack_validation_report& report,
    quiz_vulkan::assets::asset_pack_validation_issue_kind kind,
    std::string_view id)
{
    for (const quiz_vulkan::assets::asset_pack_validation_issue& issue : report.issues) {
        if (issue.kind == kind && issue.id == id) {
            return &issue;
        }
    }
    return nullptr;
}

void test_asset_pack_validation_reports_required_roots_missing_files_duplicates_and_external_placements()
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
        .id = "missing_font",
        .type = asset_type::font,
        .uri = "fonts/Missing.ttf",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "shaders/ui.vert.spv",
        .root_id = "external_shader_pack",
    });

    const normalizing_asset_resolver resolver;
    const asset_pack_validation_report report = validate_asset_pack(
        manifest,
        resolver,
        std::vector<asset_pack_required_root>{
            asset_pack_required_root{.root_id = "fixture"},
            asset_pack_required_root{.root_id = "external_shader_pack"},
            asset_pack_required_root{.root_id = "missing_required_root"},
        });

    require(!report.ok(), "asset pack validation reports expected issues");
    require(report.resolver_policy.entries.size() == 4U, "asset pack validation keeps resolved policy entries");
    require(report.resolver_policy.local_fixture_root_count == 3U, "asset pack validation counts fixture placements");
    require(report.resolver_policy.build_external_root_count == 1U, "asset pack validation counts external placements");

    const asset_pack_validation_issue* missing_required =
        find_issue(report, asset_pack_validation_issue_kind::missing_required_root, "missing_required_root");
    require(missing_required != nullptr, "asset pack validation reports missing required roots");

    const asset_pack_validation_issue* missing_file =
        find_issue(report, asset_pack_validation_issue_kind::missing_local_fixture_file, "missing_font");
    require(missing_file != nullptr, "asset pack validation reports missing local fixture files");
    require(
        missing_file->path == std::filesystem::absolute(fixture_root / "fixtures" / "fonts" / "Missing.ttf")
                                  .lexically_normal(),
        "missing fixture file diagnostic records rooted path");
    require(missing_file->cache_key == "font|fonts/Missing.ttf", "missing fixture diagnostic records cache key");

    const asset_pack_validation_issue* duplicate =
        find_issue(report, asset_pack_validation_issue_kind::duplicate_cache_key, "card_front");
    require(duplicate != nullptr, "asset pack validation reports duplicate cache keys");
    require(duplicate->related_id == "card_front_copy", "duplicate cache key reports sorted related id");
    require(duplicate->cache_key == "image|asset://images/front.png", "duplicate cache key report is stable");

    require(report.build_external_placements.size() == 1U, "asset pack validation reports external placements");
    require(report.build_external_placements[0].id == "ui_shader", "external placement records asset id");
    require(
        report.build_external_placements[0].root_path
            == fixture_root / "build" / "external" / "shader_pack",
        "external placement records build external root");
    require(
        report.build_external_placements[0].asset_path
            == std::filesystem::absolute(fixture_root / "build" / "external" / "shader_pack" / "shaders" / "ui.vert.spv")
                   .lexically_normal(),
        "external placement records rooted asset path");
}

void test_asset_pack_validation_passes_when_required_roots_and_fixture_files_exist()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "fixtures" / "images" / "front.png", "png");
    write_fixture_file(fixture_root / "fixtures" / "fonts" / "Inter.ttf", "ttf");

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
        .id = "body_font",
        .type = asset_type::font,
        .uri = "fonts/Inter.ttf",
        .root_id = "fixture",
    });

    const normalizing_asset_resolver resolver;
    const asset_pack_validation_report report = validate_asset_pack(
        manifest,
        resolver,
        std::vector<asset_pack_required_root>{asset_pack_required_root{.root_id = "fixture"}});

    require(report.ok(), "asset pack validation passes when fixtures and required roots exist");
    require(report.issues.empty(), "valid pack has no pack issues");
    require(report.build_external_placements.empty(), "valid fixture-only pack has no external placements");
    require(report.resolver_policy.entries.size() == 2U, "valid pack keeps resolved entries");
}

} // namespace

int main()
{
    test_asset_pack_validation_reports_required_roots_missing_files_duplicates_and_external_placements();
    test_asset_pack_validation_passes_when_required_roots_and_fixture_files_exist();
    return 0;
}
