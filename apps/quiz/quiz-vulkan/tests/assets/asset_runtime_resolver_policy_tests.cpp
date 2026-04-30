#include "assets/asset_runtime_resolver_policy.h"

#include "asset_runtime_resolver_policy_interface_contract_tests.cpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "asset_runtime_resolver_policy_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::filesystem::path reset_fixture_root()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "quiz_vulkan_asset_runtime_resolver_policy_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "fixtures" / "images");
    std::filesystem::create_directories(root / "fixtures" / "fonts");
    std::filesystem::create_directories(root / "build" / "external" / "shader_pack" / "shaders");
    return root;
}

const quiz_vulkan::assets::asset_runtime_resolver_policy_diagnostic* find_diagnostic(
    const quiz_vulkan::assets::asset_runtime_resolver_policy_summary& summary,
    quiz_vulkan::assets::asset_runtime_resolver_policy_status status,
    std::string_view id)
{
    for (const quiz_vulkan::assets::asset_runtime_resolver_policy_diagnostic& diagnostic : summary.diagnostics) {
        if (diagnostic.status == status && diagnostic.id == id) {
            return &diagnostic;
        }
    }
    return nullptr;
}

void test_runtime_resolver_policy_preserves_manifest_lookup_order_and_cache_keys()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
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
        .uri = "ASSET:///images/./front.png",
        .root_id = "images",
        .cache_revision = "rev1",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "./fonts//Inter.ttf",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "shaders/ui.vert.spv",
        .root_id = "external_shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shared",
        .type = asset_type::image,
        .uri = "asset://images/shared.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shared",
        .type = asset_type::image,
        .uri = "asset://images/shared-alt.png",
        .root_id = "fixture",
    });

    const normalizing_asset_resolver resolver;
    const asset_runtime_resolver_policy_summary summary =
        summarize_asset_runtime_resolver_policy(manifest, resolver);

    require(!summary.ok(), "duplicate manifest ids are policy diagnostics");
    require(summary.entries.size() == 5U, "runtime resolver policy keeps all resolved entries");
    require(summary.entries[0].id == "card_front", "manifest lookup order keeps first entry first");
    require(summary.entries[1].id == "body_font", "manifest lookup order keeps local fixture entry second");
    require(summary.entries[2].id == "ui_shader", "manifest lookup order keeps build external entry third");
    require(summary.entries[3].id == "shared", "manifest lookup order keeps first duplicate before second");
    require(summary.entries[4].source_uri == "asset://images/shared-alt.png", "second duplicate remains inspectable");

    const asset_runtime_resolver_policy_entry* first_shared = summary.find_entry("shared");
    require(first_shared != nullptr, "runtime resolver policy can find entries by id");
    require(
        first_shared->source_uri == "asset://images/shared.png",
        "runtime resolver policy find_entry follows manifest first-match lookup order");

    const asset_runtime_resolver_policy_entry* card = summary.find_cache_key("image|asset://images/front.png|rev=rev1");
    require(card != nullptr, "runtime resolver policy can find entries by cache key");
    require(card->source_kind == asset_source_kind::asset_uri, "asset uri source kind is reported");
    require(card->source_uri == "asset://images/front.png", "asset uri source is normalized");
    require(card->cache_key == "image|asset://images/front.png|rev=rev1", "manifest cache revision is stable");
    require(card->root_space == asset_runtime_resolver_root_space::local_fixture, "asset uri root uses fixture space");

    const asset_runtime_resolver_policy_entry* font = summary.find_entry("body_font");
    require(font != nullptr, "runtime resolver policy includes local fixture font");
    require(font->source_kind == asset_source_kind::local_path, "local fixture source kind is reported");
    require(font->source_uri == "fonts/Inter.ttf", "local fixture source path is normalized");
    require(font->cache_key == "font|fonts/Inter.ttf", "local fixture cache key is stable");
    require(font->root_space == asset_runtime_resolver_root_space::local_fixture, "local fixture root is classified");

    const asset_runtime_resolver_policy_entry* shader = summary.find_entry("ui_shader");
    require(shader != nullptr, "runtime resolver policy includes build external shader");
    require(shader->source_kind == asset_source_kind::local_path, "build external source is still a local path");
    require(shader->root_space == asset_runtime_resolver_root_space::build_external, "build external root is classified");
    require(shader->cache_key == "shader|shaders/ui.vert.spv", "build external cache key is stable");

    require(summary.asset_uri_count == 3U, "asset uri count is deterministic");
    require(summary.local_path_count == 2U, "local path count is deterministic");
    require(summary.local_fixture_root_count == 4U, "local fixture root count is deterministic");
    require(summary.build_external_root_count == 1U, "build external root count is deterministic");

    const asset_runtime_resolver_policy_diagnostic* duplicate =
        find_diagnostic(summary, asset_runtime_resolver_policy_status::duplicate_manifest_id, "shared");
    require(duplicate != nullptr, "duplicate manifest id diagnostic is reported");
    require(duplicate->manifest_index == 4U, "duplicate manifest id diagnostic records duplicate index");
    require(duplicate->related_manifest_index == 3U, "duplicate manifest id diagnostic records first-match index");
    require(duplicate->related_id == "shared", "duplicate manifest id diagnostic records related id");
}

void test_runtime_resolver_policy_reports_traversal_and_missing_roots()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .root_path = fixture_root / "fixtures",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "asset_traversal",
        .type = asset_type::image,
        .uri = "asset://images/%2e%2e/secret.png",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "local_traversal",
        .type = asset_type::sound,
        .uri = "../sounds/secret.wav",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_root",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
        .root_id = "missing",
    });

    const normalizing_asset_resolver resolver;
    const asset_runtime_resolver_policy_summary summary =
        summarize_asset_runtime_resolver_policy(manifest, resolver);

    require(!summary.ok(), "runtime resolver policy reports rejected entries");
    require(summary.entries.empty(), "runtime resolver policy excludes unresolved entries");
    require(summary.path_traversal_rejection_count == 2U, "path traversal rejection count is deterministic");

    const asset_runtime_resolver_policy_diagnostic* asset_traversal =
        find_diagnostic(summary, asset_runtime_resolver_policy_status::path_traversal_rejected, "asset_traversal");
    require(asset_traversal != nullptr, "asset uri traversal diagnostic is reported");
    require(asset_traversal->manifest_index == 0U, "asset uri traversal diagnostic records manifest index");

    const asset_runtime_resolver_policy_diagnostic* local_traversal =
        find_diagnostic(summary, asset_runtime_resolver_policy_status::path_traversal_rejected, "local_traversal");
    require(local_traversal != nullptr, "local fixture traversal diagnostic is reported");
    require(local_traversal->manifest_index == 1U, "local traversal diagnostic records manifest index");

    const asset_runtime_resolver_policy_diagnostic* missing_root =
        find_diagnostic(summary, asset_runtime_resolver_policy_status::missing_root, "missing_root");
    require(missing_root != nullptr, "missing root diagnostic is reported");
    require(missing_root->cache_key == "deck|decks/main.quiz", "missing root diagnostic keeps stable cache key");
}

} // namespace

int main()
{
    test_runtime_resolver_policy_preserves_manifest_lookup_order_and_cache_keys();
    test_runtime_resolver_policy_reports_traversal_and_missing_roots();
    return 0;
}
