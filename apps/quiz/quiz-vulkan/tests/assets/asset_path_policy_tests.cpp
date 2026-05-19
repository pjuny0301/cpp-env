#include "assets/asset_path_policy.h"

#include "asset_path_policy_interface_contract_tests.cpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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

const quiz_vulkan::assets::asset_path_policy_diagnostic* find_policy_diagnostic(
    const quiz_vulkan::assets::asset_path_policy_catalog& catalog,
    quiz_vulkan::assets::asset_path_policy_status status,
    std::string_view id)
{
    for (const quiz_vulkan::assets::asset_path_policy_diagnostic& diagnostic : catalog.diagnostics) {
        if (diagnostic.status == status && diagnostic.id == id) {
            return &diagnostic;
        }
    }
    return nullptr;
}

const quiz_vulkan::assets::asset_render_resource_address_diagnostic* find_render_resource_address_diagnostic(
    const quiz_vulkan::assets::asset_render_resource_address_summary& summary,
    quiz_vulkan::assets::asset_render_resource_address_status status,
    std::string_view id)
{
    for (const quiz_vulkan::assets::asset_render_resource_address_diagnostic& diagnostic : summary.diagnostics) {
        if (diagnostic.status == status && diagnostic.id == id) {
            return &diagnostic;
        }
    }
    return nullptr;
}

const quiz_vulkan::assets::asset_path_policy_kind_counts* find_kind_counts(
    const std::vector<quiz_vulkan::assets::asset_path_policy_kind_counts>& counts,
    quiz_vulkan::assets::asset_type type)
{
    for (const quiz_vulkan::assets::asset_path_policy_kind_counts& count : counts) {
        if (count.type == type) {
            return &count;
        }
    }
    return nullptr;
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

void test_render_resource_address_summary_preserves_stable_asset_identities()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .root_path = fixture_root / "packaged",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "external_shader_pack",
        .root_path = fixture_root / "build" / "external" / "shader_pack",
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
        .cache_revision = "r1",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "shaders/ui.vert.spv",
        .root_id = "external_shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "answer_sound",
        .type = asset_type::sound,
        .uri = "asset://sounds/correct.ogg",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "generic_blob",
        .type = asset_type::generic,
        .uri = "misc/blob.bin",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "bad_traversal",
        .type = asset_type::image,
        .uri = "asset://cards/%2e%2e/secret.png",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_root_shader",
        .type = asset_type::shader,
        .uri = "shaders/missing-root.vert.spv",
        .root_id = "missing",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "remote_image",
        .type = asset_type::image,
        .uri = "https://example.invalid/cards/front.png",
    });

    const normalizing_asset_resolver resolver;
    const asset_render_resource_address_summary summary =
        summarize_asset_render_resource_addresses(manifest, resolver);

    require(!summary.ok(), "render resource address summary reports blocked address evidence");
    require(summary.entry_count() == 5U, "render resource address summary keeps accepted engine resources");
    require(summary.diagnostic_count() == 4U, "render resource address summary records rejected resources");
    require(summary.accepted_count == 5U, "render resource accepted count is deterministic");
    require(summary.rejected_count == 4U, "render resource rejected count is deterministic");
    require(summary.canonical_asset_uri_count == 2U, "render resource summary counts canonical asset uris");
    require(summary.local_file_count == 3U, "render resource summary counts stable local file identities");
    require(summary.local_fixture_boundary_count == 1U, "render resource summary counts local fixture boundaries");
    require(summary.build_external_boundary_count == 1U, "render resource summary counts build external boundaries");
    require(summary.path_traversal_rejection_count == 1U, "render resource summary counts traversal rejection");
    require(summary.unsupported_asset_type_count == 1U, "render resource summary counts unsupported asset types");
    require(summary.cache_key_component_mismatch_count == 0U, "render resource cache components match resolved sources");
    require(asset_render_resource_address_type_supported(asset_type::font), "fonts are render-addressable assets");
    require(asset_render_resource_address_type_supported(asset_type::image), "images are render-addressable assets");
    require(asset_render_resource_address_type_supported(asset_type::sound), "sounds are render-addressable assets");
    require(asset_render_resource_address_type_supported(asset_type::shader), "shaders are render-addressable assets");
    require(asset_render_resource_address_type_supported(asset_type::deck), "decks are render-addressable assets");
    require(!asset_render_resource_address_type_supported(asset_type::generic), "generic blobs are not render-addressable");

    const asset_render_resource_address_entry* image = summary.find_entry("card_front");
    require(image != nullptr, "render resource summary finds image address");
    require(image->ok(), "image render resource address is accepted");
    require(
        image->address_kind == asset_render_resource_address_kind::canonical_asset_uri,
        "asset uri images keep canonical asset identity");
    require(image->canonical_identity == "asset://cards/front.png", "image identity is normalized asset uri");
    require(image->source_uri == "asset://cards/front.png", "image source uri is normalized");
    require(image->source_path == "cards/front.png", "image source path strips asset scheme");
    require(image->cache_key == "image|asset://cards/front.png|rev=r1", "image cache key keeps revision");
    require(image->cache_key_components.type_component == "image", "image cache key exposes type component");
    require(
        image->cache_key_components.source_component == "asset://cards/front.png",
        "image cache key exposes source component");
    require(image->cache_key_components.revision_component == "rev=r1", "image cache key exposes revision token");
    require(image->cache_key_components.cache_revision == "r1", "image cache key exposes revision value");
    require(image->cache_key_components.has_cache_revision(), "image cache components report revision presence");
    require(
        summary.find_cache_key("image|asset://cards/front.png|rev=r1") == image,
        "render resource summary can find addresses by cache key");
    require(
        summary.find_canonical_identity("asset://cards/front.png") == image,
        "render resource summary can find addresses by canonical identity");

    const asset_render_resource_address_entry* font = summary.find_entry("body_font");
    require(font != nullptr, "render resource summary finds font address");
    require(
        font->address_kind == asset_render_resource_address_kind::local_fixture_file,
        "font rooted under packaged fixture is classified as local fixture");
    require(
        font->canonical_identity == "local-fixture://packaged/fonts/body.ttf",
        "font local identity is stable and root-relative");
    require(font->cache_key == "font|fonts/body.ttf", "font cache key does not include absolute fixture root");
    require(
        font->cache_key.find(fixture_root.generic_string()) == std::string::npos,
        "font cache key does not leak absolute fixture path");

    const asset_render_resource_address_entry* shader = summary.find_entry("ui_shader");
    require(shader != nullptr, "render resource summary finds shader address");
    require(
        shader->address_kind == asset_render_resource_address_kind::build_external_file,
        "shader rooted under build/external is classified as build external");
    require(
        shader->canonical_identity == "build-external://external_shader_pack/shaders/ui.vert.spv",
        "shader build external identity is root-relative");
    require(shader->cache_key == "shader|shaders/ui.vert.spv", "shader cache key is source-relative");
    require(
        shader->cache_key.find("build/external") == std::string::npos,
        "shader cache key does not leak build output boundary");

    const asset_render_resource_address_entry* sound = summary.find_entry("answer_sound");
    require(sound != nullptr, "render resource summary finds sound address");
    require(
        sound->address_kind == asset_render_resource_address_kind::canonical_asset_uri,
        "sound asset uri keeps canonical address kind");
    require(sound->cache_key_components.type == asset_type::sound, "sound cache key components preserve type");

    const asset_render_resource_address_entry* deck = summary.find_entry("main_deck");
    require(deck != nullptr, "render resource summary finds deck address");
    require(deck->address_kind == asset_render_resource_address_kind::local_file, "rootless deck is local file identity");
    require(deck->canonical_identity == "local://decks/main.quiz", "rootless deck identity is source-relative");

    const asset_render_resource_address_diagnostic* generic =
        find_render_resource_address_diagnostic(
            summary,
            asset_render_resource_address_status::unsupported_asset_type,
            "generic_blob");
    require(generic != nullptr, "render resource summary reports unsupported generic entries");

    const asset_render_resource_address_diagnostic* traversal =
        find_render_resource_address_diagnostic(
            summary,
            asset_render_resource_address_status::path_traversal_rejected,
            "bad_traversal");
    require(traversal != nullptr, "render resource summary reports traversal rejection");
    require(
        asset_render_resource_address_status_name(traversal->status) == "path_traversal_rejected",
        "render resource status names are stable");

    const asset_render_resource_address_diagnostic* missing_root =
        find_render_resource_address_diagnostic(
            summary,
            asset_render_resource_address_status::missing_root,
            "missing_root_shader");
    require(missing_root != nullptr, "render resource summary reports missing roots");
    require(
        missing_root->cache_key == "shader|shaders/missing-root.vert.spv",
        "missing root diagnostics keep stable cache key evidence");

    const asset_render_resource_address_diagnostic* remote =
        find_render_resource_address_diagnostic(
            summary,
            asset_render_resource_address_status::unsupported_source_kind,
            "remote_image");
    require(remote != nullptr, "render resource summary reports unsupported remote sources");
    require(remote->source_kind == asset_source_kind::https_uri, "remote diagnostic preserves source kind");
    require(
        asset_render_resource_address_kind_name(asset_render_resource_address_kind::build_external_file)
            == "build_external_file",
        "render resource address kind names are stable");
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

    require(!policy_catalog.ok(), "equivalent shader paths report duplicate catalog paths");
    const asset_path_policy_snapshot* shader_a = policy_catalog.find("shader_a");
    const asset_path_policy_snapshot* shader_b = policy_catalog.find("shader_b");
    require(shader_a != nullptr && shader_b != nullptr, "path policy catalog contains both shader entries");
    require(shader_a->cache_key == "shader|asset://shaders/ui.vert.spv", "shader A cache key is normalized");
    require(shader_b->cache_key == shader_a->cache_key, "equivalent shader cache keys match");
    require(shader_a->catalog_path == "shaders/ui.vert.spv", "shader A catalog path is normalized");
    require(shader_b->catalog_path == shader_a->catalog_path, "equivalent shader catalog paths match");
    const asset_path_policy_diagnostic* duplicate =
        find_policy_diagnostic(policy_catalog, asset_path_policy_status::duplicate_catalog_path, "shader_b");
    require(duplicate != nullptr, "equivalent shader path reports duplicate catalog path diagnostic");
    require(duplicate->related_id == "shader_a", "duplicate catalog path diagnostic records related id");
    require(duplicate->catalog_path == "shaders/ui.vert.spv", "duplicate diagnostic records catalog path");
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

void test_path_policy_counts_duplicates_and_sorts_snapshot_view()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "bad_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.txt",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "image_b",
        .type = asset_type::image,
        .uri = "ASSET:///cards/./front.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "fonts/body.ttf",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "image_a",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
    });

    const normalizing_asset_resolver resolver;
    const asset_path_policy_catalog policy_catalog =
        build_asset_path_policy_catalog(build_runtime_asset_catalog(manifest, resolver));
    const asset_path_policy_catalog_snapshot_view snapshot_view =
        make_asset_path_policy_catalog_snapshot_view(policy_catalog);

    require(!policy_catalog.ok(), "path policy catalog reports duplicate and rejected entries");
    require(snapshot_view.entries.size() == 4U, "snapshot view keeps accepted entries");
    require(snapshot_view.diagnostics.size() == 2U, "snapshot view keeps policy diagnostics");

    require(snapshot_view.entries[0].id == "body_font", "sorted snapshot view orders font entries first");
    require(snapshot_view.entries[1].id == "image_a", "sorted snapshot view orders image entries by catalog path and id");
    require(snapshot_view.entries[2].id == "image_b", "sorted snapshot view preserves deterministic duplicate order");
    require(snapshot_view.entries[3].id == "main_deck", "sorted snapshot view orders deck entries after images");

    const asset_path_policy_diagnostic& duplicate = snapshot_view.diagnostics[0];
    require(
        duplicate.status == asset_path_policy_status::duplicate_catalog_path,
        "sorted diagnostics include duplicate catalog path first by asset kind");
    require(duplicate.id == "image_a", "duplicate diagnostic records later duplicate id");
    require(duplicate.related_id == "image_b", "duplicate diagnostic records first catalog path owner");
    require(duplicate.catalog_path == "images/cards/front.png", "duplicate diagnostic records stable catalog path");

    const asset_path_policy_diagnostic& rejected_shader = snapshot_view.diagnostics[1];
    require(
        rejected_shader.status == asset_path_policy_status::unsupported_extension,
        "sorted diagnostics include rejected shader extension");
    require(rejected_shader.id == "bad_shader", "rejected shader diagnostic records id");

    const asset_path_policy_kind_counts* font_counts = find_kind_counts(snapshot_view.kind_counts, asset_type::font);
    const asset_path_policy_kind_counts* image_counts = find_kind_counts(snapshot_view.kind_counts, asset_type::image);
    const asset_path_policy_kind_counts* shader_counts = find_kind_counts(snapshot_view.kind_counts, asset_type::shader);
    const asset_path_policy_kind_counts* deck_counts = find_kind_counts(snapshot_view.kind_counts, asset_type::deck);

    require(font_counts != nullptr, "snapshot view includes font counts");
    require(font_counts->accepted_count == 1U && font_counts->rejected_count == 0U, "font counts are stable");
    require(image_counts != nullptr, "snapshot view includes image counts");
    require(image_counts->accepted_count == 2U && image_counts->rejected_count == 1U, "image counts include duplicates");
    require(shader_counts != nullptr, "snapshot view includes shader counts");
    require(
        shader_counts->accepted_count == 0U && shader_counts->rejected_count == 1U,
        "shader counts include rejected entries");
    require(deck_counts != nullptr, "snapshot view includes deck counts");
    require(deck_counts->accepted_count == 1U && deck_counts->rejected_count == 0U, "deck counts are stable");

    require(policy_catalog.find_catalog_path("images/cards/front.png") != nullptr, "catalog path lookup is available");
}

void test_manifest_path_policy_validation_summary_reports_manifest_totals_and_duplicates()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "fonts/body.ttf",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "image_a",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "bad_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.txt",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "answer_sound",
        .type = asset_type::sound,
        .uri = "asset://sounds/%2e%2e/answer.wav",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "image_b",
        .type = asset_type::image,
        .uri = "ASSET:///cards/./front.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "main_deck",
        .type = asset_type::deck,
        .uri = "decks/main.quiz",
    });

    const normalizing_asset_resolver resolver;
    const asset_manifest_path_policy_validation_summary summary =
        validate_asset_manifest_path_policy(manifest, resolver);

    require(!summary.ok(), "manifest path policy summary reports diagnostics and duplicates");
    require(summary.policy_catalog.entries.size() == 4U, "manifest summary keeps accepted policy entries");
    require(summary.policy_catalog.diagnostics.size() == 3U, "manifest summary keeps policy diagnostics");
    require(summary.snapshot_view.entries.size() == 4U, "manifest summary includes sorted snapshot entries");
    require(summary.snapshot_view.diagnostics.size() == 3U, "manifest summary includes sorted snapshot diagnostics");

    const asset_path_policy_kind_counts* font_counts = find_kind_counts(summary.manifest_kind_counts, asset_type::font);
    const asset_path_policy_kind_counts* image_counts =
        find_kind_counts(summary.manifest_kind_counts, asset_type::image);
    const asset_path_policy_kind_counts* sound_counts =
        find_kind_counts(summary.manifest_kind_counts, asset_type::sound);
    const asset_path_policy_kind_counts* shader_counts =
        find_kind_counts(summary.manifest_kind_counts, asset_type::shader);
    const asset_path_policy_kind_counts* deck_counts = find_kind_counts(summary.manifest_kind_counts, asset_type::deck);

    require(font_counts != nullptr, "manifest summary includes font totals");
    require(font_counts->accepted_count == 1U && font_counts->rejected_count == 0U, "manifest font totals are stable");
    require(image_counts != nullptr, "manifest summary includes image totals");
    require(
        image_counts->accepted_count == 2U && image_counts->rejected_count == 1U,
        "manifest image totals include duplicate catalog diagnostics");
    require(sound_counts != nullptr, "manifest summary includes sound totals");
    require(
        sound_counts->accepted_count == 0U && sound_counts->rejected_count == 1U,
        "manifest sound totals map unresolved source diagnostics back to manifest type");
    require(shader_counts != nullptr, "manifest summary includes shader totals");
    require(
        shader_counts->accepted_count == 0U && shader_counts->rejected_count == 1U,
        "manifest shader totals include path policy rejections");
    require(deck_counts != nullptr, "manifest summary includes deck totals");
    require(deck_counts->accepted_count == 1U && deck_counts->rejected_count == 0U, "manifest deck totals are stable");

    require(summary.duplicate_sources.size() == 1U, "manifest summary reports duplicate sources");
    require(summary.duplicate_sources[0].type == asset_type::image, "duplicate source report records kind");
    require(
        summary.duplicate_sources[0].source_uri == "asset://cards/front.png",
        "duplicate source report records normalized source");
    require(
        summary.duplicate_sources[0].entry_ids == std::vector<std::string>{"image_a", "image_b"},
        "duplicate source report sorts entry ids");

    require(summary.duplicate_catalog_paths.size() == 1U, "manifest summary reports duplicate catalog paths");
    require(
        summary.duplicate_catalog_paths[0].catalog_path == "images/cards/front.png",
        "duplicate catalog path report records catalog path");
    require(
        summary.duplicate_catalog_paths[0].entry_ids == std::vector<std::string>{"image_a", "image_b"},
        "duplicate catalog path report sorts entry ids");

    require(summary.duplicate_cache_keys.size() == 1U, "manifest summary reports duplicate cache keys");
    require(
        summary.duplicate_cache_keys[0].cache_key == "image|asset://cards/front.png",
        "duplicate cache key report records normalized cache key");
    require(
        summary.duplicate_cache_keys[0].entry_ids == std::vector<std::string>{"image_a", "image_b"},
        "duplicate cache key report sorts entry ids");
}

} // namespace

int main()
{
    test_path_policy_catalog_groups_supported_asset_kinds();
    test_render_resource_address_summary_preserves_stable_asset_identities();
    test_path_policy_keeps_equivalent_shader_paths_stable();
    test_path_policy_rejects_traversal_and_unsupported_paths();
    test_path_policy_catalog_carries_runtime_diagnostics();
    test_path_policy_counts_duplicates_and_sorts_snapshot_view();
    test_manifest_path_policy_validation_summary_reports_manifest_totals_and_duplicates();
    return 0;
}
