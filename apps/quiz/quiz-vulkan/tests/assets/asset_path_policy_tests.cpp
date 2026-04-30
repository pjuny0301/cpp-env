#include "assets/asset_path_policy.h"

#include "asset_path_policy_interface_contract_tests.cpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "asset_path_policy_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::filesystem::path reset_fixture_root()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "quiz_vulkan_asset_path_policy_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "packaged");
    return root;
}

bool has_policy_diagnostic(
    const quiz_vulkan::assets::asset_path_policy_catalog& catalog,
    quiz_vulkan::assets::asset_path_policy_status status,
    std::string_view id)
{
    for (const quiz_vulkan::assets::asset_path_policy_diagnostic& diagnostic : catalog.diagnostics) {
        if (diagnostic.status == status && diagnostic.id == id) {
            return true;
        }
    }
    return false;
}

quiz_vulkan::assets::runtime_asset_catalog_snapshot make_fake_snapshot(
    quiz_vulkan::assets::asset_type type,
    std::string_view id,
    std::string_view normalized_uri,
    quiz_vulkan::assets::asset_source_kind source_kind)
{
    using namespace quiz_vulkan::assets;

    const std::string uri(normalized_uri);
    asset_manifest_entry entry{
        .id = std::string(id),
        .type = type,
        .uri = uri,
    };
    resolved_asset_source source{
        .original_uri = uri,
        .normalized_uri = uri,
        .kind = source_kind,
        .type = type,
    };

    return runtime_asset_catalog_snapshot{
        .entry = entry,
        .source = source,
        .cache_key = make_manifest_asset_cache_key(entry, source),
    };
}

void test_path_policy_catalog_groups_supported_asset_kinds()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .aliases = {"images"},
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
        .root_id = "images",
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
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog runtime_catalog = build_runtime_asset_catalog(manifest, resolver);
    const asset_path_policy_catalog policy_catalog = build_asset_path_policy_catalog(runtime_catalog);

    require(policy_catalog.ok(), "path policy catalog accepts valid concrete assets");
    require(policy_catalog.entries.size() == 5U, "path policy catalog contains all accepted assets");

    const asset_path_policy_snapshot* font = policy_catalog.find("body_font");
    require(font != nullptr, "path policy catalog finds font");
    require(font->catalog_path == "fonts/body.ttf", "font catalog path stays under fonts");
    require(font->cache_key == "font|fonts/body.ttf", "font cache key remains stable");

    const asset_path_policy_snapshot* image = policy_catalog.find("card_front");
    require(image != nullptr, "path policy catalog finds image");
    require(image->source_path == "cards/front.png", "image source path is normalized");
    require(image->catalog_path == "images/cards/front.png", "image catalog path is grouped under images");
    require(
        image->cache_key == "image|asset://cards/front.png|rev=rev1",
        "image cache key preserves resolver normalization and revision");
    require(
        policy_catalog.find_cache_key("image|asset://cards/front.png|rev=rev1") == image,
        "path policy catalog can find entries by cache key");

    const asset_path_policy_snapshot* sound = policy_catalog.find("answer_sound");
    require(sound != nullptr, "path policy catalog finds sound");
    require(sound->catalog_path == "sounds/answer.wav", "sound catalog path stays under sounds");

    const asset_path_policy_snapshot* shader = policy_catalog.find("ui_shader");
    require(shader != nullptr, "path policy catalog finds shader");
    require(shader->catalog_path == "shaders/ui.vert.spv", "shader catalog path stays under shaders");

    const asset_path_policy_snapshot* deck = policy_catalog.find("main_deck");
    require(deck != nullptr, "path policy catalog finds deck");
    require(deck->catalog_path == "decks/main.quiz", "deck catalog path stays under decks");
}

void test_path_policy_keeps_equivalent_shader_paths_stable()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shader_a",
        .type = asset_type::shader,
        .uri = "asset://shaders/./ui.vert.spv",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "shader_b",
        .type = asset_type::shader,
        .uri = "ASSET:///shaders/ui.vert.spv",
    });

    const normalizing_asset_resolver resolver;
    const asset_path_policy_catalog policy_catalog =
        build_asset_path_policy_catalog(build_runtime_asset_catalog(manifest, resolver));

    require(policy_catalog.ok(), "equivalent shader paths do not create policy diagnostics");
    const asset_path_policy_snapshot* shader_a = policy_catalog.find("shader_a");
    const asset_path_policy_snapshot* shader_b = policy_catalog.find("shader_b");
    require(shader_a != nullptr && shader_b != nullptr, "path policy catalog contains both shader entries");
    require(shader_a->cache_key == "shader|asset://shaders/ui.vert.spv", "shader A cache key is normalized");
    require(shader_b->cache_key == shader_a->cache_key, "equivalent shader cache keys match");
    require(shader_a->catalog_path == "shaders/ui.vert.spv", "shader A catalog path is normalized");
    require(shader_b->catalog_path == shader_a->catalog_path, "equivalent shader catalog paths match");
}

void test_path_policy_rejects_traversal_and_unsupported_paths()
{
    using namespace quiz_vulkan::assets;

    const asset_path_policy_result traversal = apply_asset_path_policy(
        make_fake_snapshot(asset_type::shader, "bad_shader", "asset://shaders/%2e%2e/secret.spv", asset_source_kind::asset_uri));
    require(
        traversal.status == asset_path_policy_status::path_traversal,
        "path policy rejects asset uri traversal");

    const asset_path_policy_result local_traversal = apply_asset_path_policy(
        make_fake_snapshot(asset_type::font, "bad_font", "../fonts/evil.ttf", asset_source_kind::local_path));
    require(
        local_traversal.status == asset_path_policy_status::path_traversal,
        "path policy rejects local path traversal");

    const asset_path_policy_result generic = apply_asset_path_policy(
        make_fake_snapshot(asset_type::generic, "generic", "misc/value.bin", asset_source_kind::local_path));
    require(
        generic.status == asset_path_policy_status::unsupported_asset_type,
        "path policy requires concrete asset kinds");

    const asset_path_policy_result extension = apply_asset_path_policy(
        make_fake_snapshot(asset_type::shader, "bad_extension", "asset://shaders/ui.txt", asset_source_kind::asset_uri));
    require(
        extension.status == asset_path_policy_status::unsupported_extension,
        "path policy rejects extensions outside the asset kind rule");

    const asset_path_policy_result source_kind = apply_asset_path_policy(
        make_fake_snapshot(asset_type::image, "remote_image", "https://example.invalid/image.png", asset_source_kind::https_uri));
    require(
        source_kind.status == asset_path_policy_status::unsupported_source_kind,
        "path policy rejects non-path source kinds");

    runtime_asset_catalog_snapshot rooted = make_fake_snapshot(
        asset_type::deck,
        "bad_rooted_path",
        "decks/main.quiz",
        asset_source_kind::local_path);
    rooted.rooted_path = std::filesystem::temp_directory_path() / "quiz_vulkan_asset_path_policy_tests" / ".."
        / "main.quiz";
    const asset_path_policy_result rooted_result = apply_asset_path_policy(rooted);
    require(
        rooted_result.status == asset_path_policy_status::invalid_rooted_path,
        "path policy rejects rooted paths with parent traversal");
}

void test_path_policy_catalog_carries_runtime_diagnostics()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "traversal",
        .type = asset_type::image,
        .uri = "asset://images/%2e%2e/secret.png",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog runtime_catalog = build_runtime_asset_catalog(manifest, resolver);
    const asset_path_policy_catalog policy_catalog = build_asset_path_policy_catalog(runtime_catalog);

    require(!policy_catalog.ok(), "path policy catalog reports runtime diagnostics");
    require(
        has_policy_diagnostic(policy_catalog, asset_path_policy_status::unresolved_source, "traversal"),
        "path policy catalog preserves unresolved runtime source diagnostics");
}

} // namespace

int main()
{
    test_path_policy_catalog_groups_supported_asset_kinds();
    test_path_policy_keeps_equivalent_shader_paths_stable();
    test_path_policy_rejects_traversal_and_unsupported_paths();
    test_path_policy_catalog_carries_runtime_diagnostics();
    return 0;
}
