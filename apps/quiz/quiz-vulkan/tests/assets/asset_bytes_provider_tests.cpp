#include "assets/asset_bytes_provider.h"
#include "assets/asset_render_resource_payload_bridge.h"

#include "asset_bytes_provider_interface_contract_tests.cpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "asset_bytes_provider_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::filesystem::path reset_fixture_root()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "quiz_vulkan_asset_bytes_provider_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "packaged" / "cards");
    std::filesystem::create_directories(root / "packaged" / "fonts");
    return root;
}

void write_fixture_file(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string bytes_to_string(const std::vector<std::byte>& bytes)
{
    std::string text;
    text.reserve(bytes.size());
    for (const std::byte value : bytes) {
        text.push_back(static_cast<char>(std::to_integer<unsigned char>(value)));
    }
    return text;
}

class counting_asset_bytes_provider final : public quiz_vulkan::assets::asset_bytes_provider_interface {
public:
    explicit counting_asset_bytes_provider(quiz_vulkan::assets::asset_bytes_load_result result = {})
        : result_(std::move(result))
    {
    }

    quiz_vulkan::assets::asset_bytes_load_result load_bytes(
        const quiz_vulkan::assets::asset_bytes_snapshot_request& request) const override
    {
        ++load_count_;
        last_snapshot_ = request.snapshot;
        return result_;
    }

    [[nodiscard]] std::size_t load_count() const
    {
        return load_count_;
    }

    [[nodiscard]] const std::optional<quiz_vulkan::assets::runtime_asset_catalog_snapshot>& last_snapshot() const
    {
        return last_snapshot_;
    }

private:
    quiz_vulkan::assets::asset_bytes_load_result result_;
    mutable std::size_t load_count_ = 0U;
    mutable std::optional<quiz_vulkan::assets::runtime_asset_catalog_snapshot> last_snapshot_;
};

bool integrity_issue_at(
    const quiz_vulkan::assets::asset_bytes_integrity_report& report,
    std::size_t index,
    quiz_vulkan::assets::asset_bytes_integrity_issue_kind kind,
    std::string_view id)
{
    return report.issues.size() > index && report.issues[index].kind == kind
        && std::string_view(report.issues[index].id) == id;
}

quiz_vulkan::assets::asset_typed_materialized_bytes_entry make_typed_summary_entry(
    std::string id,
    quiz_vulkan::assets::asset_type type,
    std::string source_uri,
    std::filesystem::path materialized_path,
    std::string content_hash,
    bool integrity_ok = true)
{
    using namespace quiz_vulkan::assets;

    asset_typed_materialized_bytes_entry entry{
        .id = std::move(id),
        .type = type,
        .cache_key = make_asset_cache_key(type, source_uri),
        .source_uri = std::move(source_uri),
        .rooted_path = materialized_path.lexically_normal(),
        .materialized_source_path = materialized_path.filename().string(),
        .materialized_path = std::move(materialized_path).lexically_normal(),
        .byte_count = 11U,
        .content_hash = std::move(content_hash),
        .materialized_status = runtime_materialized_asset_lookup_status::materialized,
        .load_status = asset_bytes_load_status::loaded,
    };

    if (!integrity_ok) {
        entry.load_status = asset_bytes_load_status::missing_bytes;
        entry.issues.push_back(asset_bytes_integrity_issue{
            .kind = asset_bytes_integrity_issue_kind::load_failed,
            .id = entry.id,
            .type = entry.type,
            .cache_key = entry.cache_key,
            .expected_cache_key = entry.cache_key,
            .source_uri = entry.source_uri,
            .expected_source_uri = entry.source_uri,
            .reported_byte_count = entry.byte_count,
            .diagnostic = "test entry intentionally fails integrity",
        });
        entry.diagnostic = "test entry intentionally fails integrity";
    }

    return entry;
}

quiz_vulkan::assets::asset_materialized_byte_payload make_payload_bundle_entry(
    std::string id,
    quiz_vulkan::assets::asset_type type,
    std::string source_uri,
    std::filesystem::path materialized_path,
    std::string bytes_text,
    std::string content_hash,
    quiz_vulkan::assets::asset_materialized_bytes_handoff_status status =
        quiz_vulkan::assets::asset_materialized_bytes_handoff_status::ready)
{
    using namespace quiz_vulkan::assets;

    std::vector<std::byte> bytes = detail::make_asset_byte_vector(bytes_text);
    if (content_hash.empty()) {
        content_hash = make_asset_bytes_content_hash(bytes);
    }

    asset_materialized_byte_payload payload{
        .id = std::move(id),
        .type = type,
        .cache_key = make_asset_cache_key(type, source_uri),
        .source_uri = std::move(source_uri),
        .rooted_path = materialized_path.lexically_normal(),
        .materialized_source_path = materialized_path.filename().string(),
        .materialized_path = std::move(materialized_path).lexically_normal(),
        .byte_count = bytes.size(),
        .content_hash = std::move(content_hash),
        .bytes = std::move(bytes),
        .status = status,
        .materialized_status = runtime_materialized_asset_lookup_status::materialized,
        .load_status = asset_bytes_load_status::loaded,
    };

    if (status == asset_materialized_bytes_handoff_status::materialization_blocked) {
        payload.rooted_path.reset();
        payload.materialized_source_path.clear();
        payload.materialized_path.clear();
        payload.byte_count = 0U;
        payload.materialized_status = runtime_materialized_asset_lookup_status::missing_rooted_path;
        payload.load_status = asset_bytes_load_status::source_not_readable;
        payload.diagnostic = "test payload is missing a rooted path";
    }

    if (status == asset_materialized_bytes_handoff_status::integrity_blocked) {
        payload.issues.push_back(asset_bytes_integrity_issue{
            .kind = asset_bytes_integrity_issue_kind::content_hash_mismatch,
            .id = payload.id,
            .type = payload.type,
            .cache_key = payload.cache_key,
            .expected_cache_key = payload.cache_key,
            .source_uri = payload.source_uri,
            .expected_source_uri = payload.source_uri,
            .reported_byte_count = payload.byte_count,
            .actual_byte_count = payload.bytes.size(),
            .reported_content_hash = payload.content_hash,
            .actual_content_hash = make_asset_bytes_content_hash(payload.bytes),
            .diagnostic = "test payload intentionally fails integrity",
        });
    }

    return payload;
}

std::vector<std::byte> make_spirv_fixture_bytes()
{
    return std::vector<std::byte>{
        static_cast<std::byte>(0x03U),
        static_cast<std::byte>(0x02U),
        static_cast<std::byte>(0x23U),
        static_cast<std::byte>(0x07U),
        static_cast<std::byte>(0x00U),
        static_cast<std::byte>(0x00U),
        static_cast<std::byte>(0x00U),
        static_cast<std::byte>(0x00U),
    };
}

quiz_vulkan::assets::asset_materialized_byte_payload make_shader_payload_bundle_entry(
    std::string id,
    std::string source_uri,
    std::filesystem::path materialized_path,
    std::vector<std::byte> bytes,
    quiz_vulkan::assets::asset_materialized_bytes_handoff_status status =
        quiz_vulkan::assets::asset_materialized_bytes_handoff_status::ready)
{
    using namespace quiz_vulkan::assets;

    asset_materialized_byte_payload payload = make_payload_bundle_entry(
        std::move(id),
        asset_type::shader,
        std::move(source_uri),
        std::move(materialized_path),
        "",
        "",
        status);
    payload.bytes = std::move(bytes);
    payload.byte_count = payload.bytes.size();
    payload.content_hash = make_asset_bytes_content_hash(payload.bytes);
    return payload;
}

bool shader_pipeline_issue_at(
    const quiz_vulkan::assets::asset_shader_materialized_byte_pipeline_entry& entry,
    std::size_t index,
    quiz_vulkan::assets::asset_shader_materialized_byte_issue_kind kind)
{
    return entry.issues.size() > index && entry.issues[index].kind == kind;
}

quiz_vulkan::assets::runtime_asset_catalog_snapshot make_card_front_snapshot(
    const std::filesystem::path& fixture_root)
{
    using namespace quiz_vulkan::assets;

    return runtime_asset_catalog_snapshot{
        .entry = asset_manifest_entry{
            .id = "card_front",
            .type = asset_type::image,
            .uri = "asset://cards/front.png",
        },
        .source = resolved_asset_source{
            .original_uri = "asset://cards/front.png",
            .normalized_uri = "asset://cards/front.png",
            .kind = asset_source_kind::asset_uri,
            .type = asset_type::image,
        },
        .cache_key = "image|asset://cards/front.png",
        .resolved_root_id = "packaged",
        .rooted_path = std::filesystem::absolute(fixture_root / "packaged" / "cards" / "front.png"),
    };
}

void test_fake_provider_loads_bytes_by_snapshot_and_catalog_id()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .root_path = fixture_root / "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "ASSET:///cards/./front.png",
        .root_id = "packaged",
        .cache_revision = "rev1",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const runtime_asset_catalog_lookup_result image = catalog.lookup_image("card_front");
    require(image.ok(), "runtime catalog resolves image for fake byte provider");

    fake_asset_bytes_provider provider;
    provider.set_text(image.asset.cache_key, "image bytes");

    const asset_bytes_load_result by_snapshot = load_asset_bytes(provider, image.asset);
    require(by_snapshot.ok(), "fake byte provider loads bytes by snapshot");
    require(by_snapshot.status == asset_bytes_load_status::loaded, "fake byte provider reports loaded status");
    require(by_snapshot.cache_key == "image|asset://cards/front.png|rev=rev1", "byte result includes cache key");
    require(by_snapshot.source_uri == "asset://cards/front.png", "byte result includes source uri");
    require(by_snapshot.byte_count == 11U, "byte result includes byte count");
    require(
        by_snapshot.content_hash == make_asset_bytes_content_hash(by_snapshot.bytes),
        "byte result includes deterministic content hash evidence");
    require(bytes_to_string(by_snapshot.bytes) == "image bytes", "fake byte provider returns registered bytes");

    const asset_bytes_load_result by_id = load_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "card_front", .expected_type = asset_type::image});
    require(by_id.ok(), "fake byte provider loads bytes by catalog id");
    require(by_id.cache_key == by_snapshot.cache_key, "catalog byte request preserves cache key");
    require(by_id.source_uri == by_snapshot.source_uri, "catalog byte request preserves source uri");
    require(by_id.content_hash == by_snapshot.content_hash, "catalog byte request preserves content hash evidence");
    require(bytes_to_string(by_id.bytes) == "image bytes", "catalog byte request returns registered bytes");
}

void test_fake_provider_reports_lookup_and_cache_misses()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_root",
        .type = asset_type::image,
        .uri = "asset://cards/missing.png",
        .root_id = "not_configured",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const fake_asset_bytes_provider provider;

    const asset_bytes_load_result missing = load_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "missing", .expected_type = asset_type::image});
    require(missing.status == asset_bytes_load_status::missing_id, "byte request reports missing catalog id");
    require(!missing.diagnostic.empty(), "missing id byte result includes diagnostic");

    const asset_bytes_load_result wrong_type = load_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "card_front", .expected_type = asset_type::sound});
    require(wrong_type.status == asset_bytes_load_status::type_mismatch, "byte request reports type mismatch");
    require(wrong_type.cache_key == "image|asset://cards/front.png", "type mismatch byte result keeps cache key");
    require(wrong_type.source_uri == "asset://cards/front.png", "type mismatch byte result keeps source uri");

    const asset_bytes_load_result unresolved = load_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "missing_root", .expected_type = asset_type::image});
    require(unresolved.status == asset_bytes_load_status::unresolved_asset, "byte request reports unresolved assets");
    require(!unresolved.diagnostic.empty(), "unresolved byte result includes diagnostic");

    const runtime_asset_catalog_lookup_result image = catalog.lookup_image("card_front");
    require(image.ok(), "runtime catalog resolves image for missing-byte test");
    const asset_bytes_load_result cache_miss = load_asset_bytes(provider, image.asset);
    require(cache_miss.status == asset_bytes_load_status::missing_bytes, "fake provider reports cache misses");
    require(cache_miss.cache_key == image.asset.cache_key, "cache miss result includes cache key");
    require(cache_miss.source_uri == image.asset.source.normalized_uri, "cache miss result includes source uri");
}

void test_local_file_provider_reads_rooted_paths_and_rejects_unreadable_sources()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "packaged" / "fonts" / "body.ttf", "font bytes");

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
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.vert.spv",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const local_file_asset_bytes_provider provider;

    const asset_bytes_load_result font = load_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "body_font", .expected_type = asset_type::font});
    require(font.ok(), "local file provider reads rooted font bytes");
    require(font.byte_count == 10U, "local file provider reports byte count");
    require(
        font.content_hash == make_asset_bytes_content_hash(font.bytes),
        "local file provider reports content hash evidence");
    require(font.cache_key == "font|fonts/body.ttf", "local file provider result includes cache key");
    require(font.source_uri == "fonts/body.ttf", "local file provider result includes source uri");
    require(bytes_to_string(font.bytes) == "font bytes", "local file provider returns file bytes");

    const asset_bytes_load_result rootless_shader = load_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "ui_shader", .expected_type = asset_type::shader});
    require(
        rootless_shader.status == asset_bytes_load_status::source_not_readable,
        "local file provider rejects rootless sources");

    const runtime_asset_catalog_snapshot unsafe_snapshot{
        .entry = asset_manifest_entry{
            .id = "unsafe",
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
    const asset_bytes_load_result unsafe = load_asset_bytes(provider, unsafe_snapshot);
    require(
        unsafe.status == asset_bytes_load_status::invalid_rooted_path,
        "local file provider rejects rooted paths with parent traversal");
}

void test_materialized_asset_bytes_read_local_files()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "packaged" / "fonts" / "body.ttf", "font bytes");

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
        .cache_revision = "v2",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const local_file_asset_bytes_provider provider;

    const asset_bytes_load_result font = load_materialized_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "body_font", .expected_type = asset_type::font});

    require(font.ok(), "materialized byte provider reads local rooted files");
    require(font.status == asset_bytes_load_status::loaded, "materialized byte load reports loaded status");
    require(font.byte_count == 10U, "materialized byte load reports byte count");
    require(
        font.content_hash == make_asset_bytes_content_hash(font.bytes),
        "materialized byte load reports content hash evidence");
    require(font.cache_key == "font|fonts/body.ttf|rev=v2", "materialized byte load preserves cache key");
    require(font.source_uri == "fonts/body.ttf", "materialized byte load preserves normalized source uri");
    require(bytes_to_string(font.bytes) == "font bytes", "materialized byte load returns local file bytes");
}

void test_materialized_asset_bytes_do_not_read_unsupported_or_unmaterialized_sources()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .root_path = fixture_root / "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
        .root_id = "packaged",
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
        .root_id = "not_configured",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const counting_asset_bytes_provider provider;

    const asset_bytes_load_result missing = load_materialized_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "missing", .expected_type = asset_type::image});
    require(missing.status == asset_bytes_load_status::missing_id, "materialized byte load reports missing ids");
    require(provider.load_count() == 0U, "missing materialized assets do not call the byte provider");

    const asset_bytes_load_result wrong_type = load_materialized_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "card_front", .expected_type = asset_type::sound});
    require(
        wrong_type.status == asset_bytes_load_status::type_mismatch,
        "materialized byte load reports type mismatches");
    require(wrong_type.cache_key == "image|asset://cards/front.png", "type mismatch keeps cache key diagnostics");
    require(wrong_type.source_uri == "asset://cards/front.png", "type mismatch keeps source uri diagnostics");
    require(provider.load_count() == 0U, "type mismatches do not call the byte provider");

    const asset_bytes_load_result unresolved = load_materialized_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "missing_root", .expected_type = asset_type::deck});
    require(
        unresolved.status == asset_bytes_load_status::unresolved_asset,
        "materialized byte load reports unresolved manifest entries");
    require(!unresolved.diagnostic.empty(), "unresolved materialized byte load includes diagnostics");
    require(provider.load_count() == 0U, "unresolved materialized assets do not call the byte provider");

    const asset_bytes_load_result rootless = load_materialized_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "rootless_shader", .expected_type = asset_type::shader});
    require(
        rootless.status == asset_bytes_load_status::source_not_readable,
        "materialized byte load rejects rootless local sources");
    require(rootless.cache_key == "shader|asset://shaders/ui.vert.spv", "rootless rejection keeps cache key");
    require(provider.load_count() == 0U, "rootless materialized assets do not call the byte provider");

    const asset_bytes_load_result remote = load_materialized_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "remote_image", .expected_type = asset_type::image});
    require(
        remote.status == asset_bytes_load_status::source_not_readable,
        "materialized byte load rejects remote sources");
    require(remote.source_uri == "https://example.test/cards/front.png", "remote rejection keeps source uri");
    require(provider.load_count() == 0U, "remote materialized assets do not call the byte provider");
}

void test_materialized_asset_bytes_surface_materialization_policy_diagnostics()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const counting_asset_bytes_provider provider;

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
    const asset_bytes_load_result unsafe = load_materialized_asset_bytes(
        provider,
        materialize_runtime_asset(unsafe_snapshot));
    require(
        unsafe.status == asset_bytes_load_status::invalid_rooted_path,
        "materialized byte load surfaces rooted path traversal rejection");
    require(unsafe.cache_key == "deck|decks/main.quiz", "rooted path rejection keeps cache key");
    require(provider.load_count() == 0U, "unsafe materialized assets do not call the byte provider");

    unsafe_snapshot.cache_key = "deck|decks/other.quiz";
    unsafe_snapshot.rooted_path = std::filesystem::absolute(fixture_root / "packaged" / "decks" / "main.quiz");
    const asset_bytes_load_result mismatched_key = load_materialized_asset_bytes(
        provider,
        materialize_runtime_asset(unsafe_snapshot));
    require(
        mismatched_key.status == asset_bytes_load_status::cache_key_mismatch,
        "materialized byte load surfaces cache key mismatches");
    require(provider.load_count() == 0U, "cache key mismatches do not call the byte provider");

    runtime_asset_catalog_snapshot source_path_mismatch_snapshot{
        .entry = asset_manifest_entry{
            .id = "wrong_shader_file",
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
    const asset_bytes_load_result source_path_mismatch = load_materialized_asset_bytes(
        provider,
        materialize_runtime_asset(source_path_mismatch_snapshot));
    require(
        source_path_mismatch.status == asset_bytes_load_status::source_path_mismatch,
        "materialized byte load surfaces source path mismatches");
    require(!source_path_mismatch.diagnostic.empty(), "source path mismatch result includes diagnostics");
    require(provider.load_count() == 0U, "source path mismatches do not call the byte provider");

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
    const asset_bytes_load_result noncanonical = load_materialized_asset_bytes(
        provider,
        asset_materialized_bytes_request{.materialized = materialize_runtime_asset(noncanonical_snapshot)});
    require(
        noncanonical.status == asset_bytes_load_status::noncanonical_cache_key,
        "materialized byte load surfaces noncanonical cache keys");
    require(!noncanonical.diagnostic.empty(), "noncanonical cache key result includes diagnostics");
    require(provider.load_count() == 0U, "noncanonical cache keys do not call the byte provider");
}

void test_materialized_asset_bytes_call_provider_after_materialization()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const std::filesystem::path quiz_data_root = fixture_root / "build" / "external" / "quiz" / "quiz-data";
    const std::filesystem::path rooted_path =
        std::filesystem::absolute(quiz_data_root / "images" / "." / "front.png");
    const std::filesystem::path materialized_path =
        std::filesystem::absolute(quiz_data_root / "images" / "front.png").lexically_normal();
    runtime_asset_catalog_snapshot snapshot{
        .entry = asset_manifest_entry{
            .id = "card_front",
            .type = asset_type::image,
            .uri = "asset://images/front.png",
        },
        .source = resolved_asset_source{
            .original_uri = "asset://images/front.png",
            .normalized_uri = "asset://images/front.png",
            .kind = asset_source_kind::asset_uri,
            .type = asset_type::image,
        },
        .cache_key = "image|asset://images/front.png",
        .resolved_root_id = "quiz_data",
        .rooted_path = rooted_path,
    };

    asset_bytes_load_result provider_result{
        .status = asset_bytes_load_status::loaded,
        .bytes = detail::make_asset_byte_vector("image bytes"),
        .byte_count = 11U,
        .cache_key = snapshot.cache_key,
        .source_uri = snapshot.source.normalized_uri,
    };
    const counting_asset_bytes_provider provider(std::move(provider_result));

    const asset_bytes_load_result loaded = load_materialized_asset_bytes(
        provider,
        materialize_runtime_asset(snapshot));

    require(loaded.ok(), "materialized byte load calls provider for materialized local assets");
    require(provider.load_count() == 1U, "materialized local assets call the byte provider once");
    require(provider.last_snapshot().has_value(), "provider receives a materialized catalog snapshot");
    require(provider.last_snapshot()->entry.id == "card_front", "provider receives the materialized asset id");
    require(
        provider.last_snapshot()->rooted_path == materialized_path,
        "provider receives the normalized materialized local path");
    require(bytes_to_string(loaded.bytes) == "image bytes", "provider result is returned after materialization");
}

void test_materialized_asset_bytes_load_build_external_quiz_data_assets()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const std::filesystem::path quiz_data_root = fixture_root / "build" / "external" / "quiz" / "quiz-data";
    write_fixture_file(quiz_data_root / "fonts" / "body.ttf", "font bytes");
    write_fixture_file(quiz_data_root / "images" / "front.png", "image bytes");
    write_fixture_file(quiz_data_root / "shaders" / "ui.vert.spv", "shader bytes");

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "quiz_data",
        .root_path = quiz_data_root,
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "fonts/body.ttf",
        .root_id = "quiz_data",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://images/front.png",
        .root_id = "quiz_data",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.vert.spv",
        .root_id = "quiz_data",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const local_file_asset_bytes_provider provider;

    const asset_bytes_load_result font = load_materialized_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "body_font", .expected_type = asset_type::font});
    require(font.ok(), "materialized byte load reads font bytes from build external quiz data");
    require(bytes_to_string(font.bytes) == "font bytes", "font bytes come from build external quiz data root");

    const asset_bytes_load_result image = load_materialized_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "card_front", .expected_type = asset_type::image});
    require(image.ok(), "materialized byte load reads image bytes from build external quiz data");
    require(bytes_to_string(image.bytes) == "image bytes", "image bytes come from build external quiz data root");

    const asset_bytes_load_result shader = load_materialized_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "ui_shader", .expected_type = asset_type::shader});
    require(shader.ok(), "materialized byte load reads shader bytes from build external quiz data");
    require(bytes_to_string(shader.bytes) == "shader bytes", "shader bytes come from build external quiz data root");
}

void test_materialized_asset_bytes_with_integrity_loads_catalog_font_image_and_shader_bytes()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "packaged" / "fonts" / "body.ttf", "font bytes");
    write_fixture_file(fixture_root / "packaged" / "cards" / "front.png", "image bytes");
    write_fixture_file(fixture_root / "packaged" / "shaders" / "ui.vert.spv", "shader bytes");

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
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.vert.spv",
        .root_id = "packaged",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const local_file_asset_bytes_provider provider;

    const asset_bytes_integrity_report font = load_materialized_asset_bytes_with_integrity(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "body_font", .expected_type = asset_type::font});
    require(font.ok(), "materialized integrity load accepts rooted font bytes");
    require(font.load.cache_key == "font|fonts/body.ttf", "font materialized integrity preserves cache key");
    require(font.load.source_uri == "fonts/body.ttf", "font materialized integrity preserves source uri");
    require(bytes_to_string(font.load.bytes) == "font bytes", "font materialized integrity returns file bytes");

    const asset_bytes_integrity_report image = load_materialized_asset_bytes_with_integrity(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "card_front", .expected_type = asset_type::image});
    require(image.ok(), "materialized integrity load accepts rooted image bytes");
    require(
        image.load.cache_key == "image|asset://cards/front.png|rev=rev1",
        "image materialized integrity preserves revised cache key");
    require(image.load.source_uri == "asset://cards/front.png", "image materialized integrity preserves source uri");
    require(bytes_to_string(image.load.bytes) == "image bytes", "image materialized integrity returns file bytes");

    const asset_bytes_integrity_report shader = load_materialized_asset_bytes_with_integrity(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "ui_shader", .expected_type = asset_type::shader});
    require(shader.ok(), "materialized integrity load accepts rooted shader-like bytes");
    require(
        shader.load.cache_key == "shader|asset://shaders/ui.vert.spv",
        "shader materialized integrity preserves cache key");
    require(
        shader.load.source_uri == "asset://shaders/ui.vert.spv",
        "shader materialized integrity preserves source uri");
    require(bytes_to_string(shader.load.bytes) == "shader bytes", "shader materialized integrity returns file bytes");
}

void test_materialized_asset_bytes_with_integrity_reports_catalog_provider_failures()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .root_path = fixture_root / "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
        .root_id = "packaged",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const runtime_asset_catalog_lookup_result image = catalog.lookup_image("card_front");
    require(image.ok(), "runtime catalog resolves image for materialized integrity failure tests");

    const asset_bytes_catalog_request request{.id = "card_front", .expected_type = asset_type::image};

    const counting_asset_bytes_provider cache_key_provider(asset_bytes_load_result{
        .status = asset_bytes_load_status::loaded,
        .bytes = detail::make_asset_byte_vector("image bytes"),
        .byte_count = 11U,
        .cache_key = "image|asset://cards/back.png",
        .source_uri = image.asset.source.normalized_uri,
    });
    const asset_bytes_integrity_report cache_key_mismatch = load_materialized_asset_bytes_with_integrity(
        cache_key_provider,
        catalog,
        request);
    require(!cache_key_mismatch.ok(), "materialized integrity reports cache key mismatches");
    require(cache_key_provider.load_count() == 1U, "cache key mismatch is checked after materialized provider load");
    require(cache_key_mismatch.issues.size() == 1U, "cache key mismatch is isolated");
    require(
        integrity_issue_at(
            cache_key_mismatch,
            0U,
            asset_bytes_integrity_issue_kind::cache_key_mismatch,
            "card_front"),
        "materialized integrity identifies cache key mismatch");
    require(
        cache_key_mismatch.issues[0].expected_cache_key == image.asset.cache_key
            && cache_key_mismatch.issues[0].cache_key == "image|asset://cards/back.png",
        "cache key mismatch includes expected and observed keys");

    const counting_asset_bytes_provider source_uri_provider(asset_bytes_load_result{
        .status = asset_bytes_load_status::loaded,
        .bytes = detail::make_asset_byte_vector("image bytes"),
        .byte_count = 11U,
        .cache_key = image.asset.cache_key,
        .source_uri = "asset://cards/back.png",
    });
    const asset_bytes_integrity_report source_uri_mismatch = load_materialized_asset_bytes_with_integrity(
        source_uri_provider,
        catalog,
        request);
    require(!source_uri_mismatch.ok(), "materialized integrity reports source uri mismatches");
    require(source_uri_provider.load_count() == 1U, "source uri mismatch is checked after materialized provider load");
    require(source_uri_mismatch.issues.size() == 1U, "source uri mismatch is isolated");
    require(
        integrity_issue_at(
            source_uri_mismatch,
            0U,
            asset_bytes_integrity_issue_kind::source_uri_mismatch,
            "card_front"),
        "materialized integrity identifies source uri mismatch");
    require(
        source_uri_mismatch.issues[0].expected_source_uri == image.asset.source.normalized_uri
            && source_uri_mismatch.issues[0].source_uri == "asset://cards/back.png",
        "source uri mismatch includes expected and observed uris");

    const counting_asset_bytes_provider byte_count_provider(asset_bytes_load_result{
        .status = asset_bytes_load_status::loaded,
        .bytes = detail::make_asset_byte_vector("image bytes"),
        .byte_count = 12U,
        .cache_key = image.asset.cache_key,
        .source_uri = image.asset.source.normalized_uri,
    });
    const asset_bytes_integrity_report byte_count_mismatch = load_materialized_asset_bytes_with_integrity(
        byte_count_provider,
        catalog,
        request);
    require(!byte_count_mismatch.ok(), "materialized integrity reports byte count mismatches");
    require(byte_count_provider.load_count() == 1U, "byte count mismatch is checked after materialized provider load");
    require(byte_count_mismatch.issues.size() == 1U, "byte count mismatch is isolated");
    require(
        integrity_issue_at(
            byte_count_mismatch,
            0U,
            asset_bytes_integrity_issue_kind::byte_count_mismatch,
            "card_front"),
        "materialized integrity identifies byte count mismatch");
    require(
        byte_count_mismatch.issues[0].reported_byte_count == 12U
            && byte_count_mismatch.issues[0].actual_byte_count == 11U,
        "byte count mismatch includes reported and actual counts");

    const counting_asset_bytes_provider empty_content_provider(asset_bytes_load_result{
        .status = asset_bytes_load_status::loaded,
        .byte_count = 0U,
        .cache_key = image.asset.cache_key,
        .source_uri = image.asset.source.normalized_uri,
    });
    const asset_bytes_integrity_report missing_content = load_materialized_asset_bytes_with_integrity(
        empty_content_provider,
        catalog,
        request);
    require(!missing_content.ok(), "materialized integrity applies the non-empty content policy");
    require(empty_content_provider.load_count() == 1U, "missing content policy is checked after provider load");
    require(missing_content.issues.size() == 1U, "missing content policy failure is isolated");
    require(
        integrity_issue_at(
            missing_content,
            0U,
            asset_bytes_integrity_issue_kind::missing_content,
            "card_front"),
        "materialized integrity identifies empty provider content");
    require(missing_content.load.ok(), "missing content policy keeps the successful provider load");
}

void test_asset_bytes_integrity_validates_load_result_byte_count_and_content()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const runtime_asset_catalog_snapshot snapshot = make_card_front_snapshot(fixture_root);
    asset_bytes_load_result load{
        .status = asset_bytes_load_status::loaded,
        .bytes = detail::make_asset_byte_vector("image bytes"),
        .byte_count = 11U,
        .content_hash = make_asset_bytes_content_hash(detail::make_asset_byte_vector("image bytes")),
        .cache_key = snapshot.cache_key,
        .source_uri = snapshot.source.normalized_uri,
    };

    const asset_bytes_integrity_report valid = validate_asset_bytes_integrity(asset_bytes_integrity_request{
        .snapshot = snapshot,
        .load = load,
    });
    require(valid.ok(), "asset byte integrity accepts matching metadata and byte counts");

    asset_bytes_load_result hash_load = load;
    hash_load.content_hash = "fnv1a64:0000000000000000";
    const asset_bytes_integrity_report hash_mismatch =
        validate_asset_bytes_integrity(asset_bytes_integrity_request{
            .snapshot = snapshot,
            .load = hash_load,
        });
    require(
        integrity_issue_at(
            hash_mismatch,
            0U,
            asset_bytes_integrity_issue_kind::content_hash_mismatch,
            "card_front"),
        "asset byte integrity reports content hash mismatches");
    require(
        hash_mismatch.issues[0].reported_content_hash == "fnv1a64:0000000000000000"
            && hash_mismatch.issues[0].actual_content_hash == make_asset_bytes_content_hash(load.bytes),
        "content hash mismatch reports both provider and actual hashes");

    load.byte_count = 12U;
    const asset_bytes_integrity_report count_mismatch =
        validate_asset_bytes_integrity(asset_bytes_integrity_request{
            .snapshot = snapshot,
            .load = load,
        });
    require(
        integrity_issue_at(
            count_mismatch,
            0U,
            asset_bytes_integrity_issue_kind::byte_count_mismatch,
            "card_front"),
        "asset byte integrity reports byte count mismatches");
    require(
        count_mismatch.issues[0].reported_byte_count == 12U && count_mismatch.issues[0].actual_byte_count == 11U,
        "byte count mismatch reports both provider count and payload size");

    load.bytes.clear();
    load.byte_count = 0U;
    load.content_hash.clear();
    const asset_bytes_integrity_report missing_content =
        validate_asset_bytes_integrity(asset_bytes_integrity_request{
            .snapshot = snapshot,
            .load = load,
        });
    require(
        integrity_issue_at(
            missing_content,
            0U,
            asset_bytes_integrity_issue_kind::missing_content,
            "card_front"),
        "asset byte integrity reports empty content by default");

    const asset_bytes_integrity_report empty_allowed =
        validate_asset_bytes_integrity(asset_bytes_integrity_request{
            .snapshot = snapshot,
            .load = load,
            .require_non_empty = false,
        });
    require(empty_allowed.ok(), "asset byte integrity can explicitly allow empty content");
}

void test_materialized_asset_bytes_cache_policy_summary_tracks_hash_and_integrity()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "packaged" / "cards" / "front.png", "image bytes");

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .root_path = fixture_root / "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
        .root_id = "packaged",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const std::vector<asset_bytes_catalog_request> requests{
        asset_bytes_catalog_request{.id = "card_front", .expected_type = asset_type::image},
    };

    const local_file_asset_bytes_provider local_provider;
    const asset_materialized_bytes_cache_policy_summary summary =
        summarize_materialized_asset_bytes_cache_policy(local_provider, catalog, requests);

    require(summary.ok(), "cache policy summary accepts matching materialized bytes");
    require(summary.request_count == 1U, "cache policy summary records request count");
    require(summary.loaded_count == 1U, "cache policy summary counts loaded byte results");
    require(summary.failed_count == 0U, "cache policy summary has no failures for matching bytes");
    require(summary.total_byte_count == 11U, "cache policy summary totals loaded byte counts");

    const asset_materialized_bytes_cache_policy_entry* entry = summary.find_entry("card_front");
    require(entry != nullptr, "cache policy summary can find entries by id");
    require(entry->ok(), "cache policy summary entry accepts matching materialized bytes");
    require(entry->cache_key == "image|asset://cards/front.png", "cache policy summary preserves cache key");
    require(entry->source_uri == "asset://cards/front.png", "cache policy summary preserves source uri");
    require(entry->materialized_source_path == "cards/front.png", "cache policy summary records source path");
    require(
        entry->materialized_path
            == std::filesystem::absolute(fixture_root / "packaged" / "cards" / "front.png").lexically_normal(),
        "cache policy summary records materialized path");
    require(entry->byte_count == 11U, "cache policy summary entry records byte count");
    require(
        entry->content_hash == make_asset_bytes_content_hash(detail::make_asset_byte_vector("image bytes")),
        "cache policy summary entry records content hash evidence");
    require(entry->issues.empty(), "cache policy summary entry has no issues for matching bytes");

    const runtime_asset_catalog_lookup_result image = catalog.lookup_image("card_front");
    require(image.ok(), "runtime catalog resolves image for cache policy mismatch summary");
    const counting_asset_bytes_provider bad_hash_provider(asset_bytes_load_result{
        .status = asset_bytes_load_status::loaded,
        .bytes = detail::make_asset_byte_vector("image bytes"),
        .byte_count = 11U,
        .content_hash = "fnv1a64:0000000000000000",
        .cache_key = image.asset.cache_key,
        .source_uri = image.asset.source.normalized_uri,
    });

    const asset_materialized_bytes_cache_policy_summary mismatch_summary =
        summarize_materialized_asset_bytes_cache_policy(bad_hash_provider, catalog, requests);
    require(!mismatch_summary.ok(), "cache policy summary rejects reported content hash mismatches");
    require(mismatch_summary.loaded_count == 1U, "cache policy summary still counts loaded mismatch payloads");
    require(mismatch_summary.failed_count == 1U, "cache policy summary counts hash mismatch failures");
    require(
        mismatch_summary.content_hash_mismatch_count == 1U,
        "cache policy summary counts content hash mismatches");
    require(
        mismatch_summary.entries[0].issues[0].reported_content_hash == "fnv1a64:0000000000000000",
        "cache policy summary keeps reported content hash diagnostics");
    require(
        mismatch_summary.entries[0].issues[0].actual_content_hash
            == make_asset_bytes_content_hash(detail::make_asset_byte_vector("image bytes")),
        "cache policy summary keeps actual content hash diagnostics");
}

void test_typed_materialized_asset_bytes_summary_groups_engine_assets()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "packaged" / "fonts" / "body.ttf", "font bytes");
    write_fixture_file(fixture_root / "packaged" / "cards" / "front.png", "image bytes");
    write_fixture_file(fixture_root / "packaged" / "sounds" / "correct.ogg", "sound bytes");
    write_fixture_file(fixture_root / "packaged" / "shaders" / "ui.vert.spv", "shader bytes");
    write_fixture_file(fixture_root / "packaged" / "decks" / "main.quiz", "deck bytes");
    write_fixture_file(fixture_root / "packaged" / "misc" / "notes.txt", "generic bytes");

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .root_path = fixture_root / "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "asset://fonts/body.ttf",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "answer_sound",
        .type = asset_type::sound,
        .uri = "asset://sounds/correct.ogg",
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
        .uri = "asset://decks/main.quiz",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "generic_notes",
        .type = asset_type::generic,
        .uri = "asset://misc/notes.txt",
        .root_id = "packaged",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const local_file_asset_bytes_provider provider;
    const asset_typed_materialized_bytes_summary summary =
        summarize_typed_materialized_asset_bytes(provider, catalog);

    require(summary.ok(), "typed materialized bytes summary accepts matching engine assets");
    require(summary.entry_count() == 5U, "typed materialized bytes summary groups five engine assets");
    require(summary.skipped_generic_count == 1U, "typed materialized bytes summary skips generic assets");
    require(summary.cache_policy.request_count == 5U, "typed summary only requests engine-facing assets");
    require(summary.cache_policy.loaded_count == 5U, "typed summary loads every engine-facing asset");
    require(summary.cache_policy.failed_count == 0U, "typed summary records no integrity failures");
    require(summary.cache_policy.total_byte_count == 54U, "typed summary totals engine-facing bytes");
    require(summary.fonts.size() == 1U, "typed summary groups fonts");
    require(summary.images.size() == 1U, "typed summary groups images");
    require(summary.sounds.size() == 1U, "typed summary groups sounds");
    require(summary.shaders.size() == 1U, "typed summary groups shaders");
    require(summary.decks.size() == 1U, "typed summary groups decks");
    require(summary.entries_for_type(asset_type::generic).empty(), "typed summary leaves generic assets ungrouped");
    require(summary.find_entry("generic_notes") == nullptr, "typed summary excludes generic entries from lookup");

    const asset_typed_materialized_bytes_entry* image = summary.find_entry("card_front");
    require(image != nullptr, "typed summary can find grouped image entries");
    require(image->ok(), "typed summary image entry keeps integrity status");
    require(image->type == asset_type::image, "typed summary image entry keeps type");
    require(image->cache_key == "image|asset://cards/front.png", "typed summary image keeps cache key");
    require(image->source_uri == "asset://cards/front.png", "typed summary image keeps source uri");
    require(image->rooted_path.has_value(), "typed summary image keeps rooted path");
    require(
        *image->rooted_path
            == std::filesystem::absolute(fixture_root / "packaged" / "cards" / "front.png").lexically_normal(),
        "typed summary image rooted path is normalized under the root");
    require(image->materialized_source_path == "cards/front.png", "typed summary image keeps source path");
    require(image->materialized_path == *image->rooted_path, "typed summary image keeps materialized path");
    require(image->byte_count == 11U, "typed summary image keeps byte count evidence");
    require(
        image->content_hash == make_asset_bytes_content_hash(detail::make_asset_byte_vector("image bytes")),
        "typed summary image keeps content hash evidence");
    require(image->issues.empty(), "typed summary image has no integrity issues");
    require(
        summary.entries_for_type(asset_type::image)[0].id == "card_front",
        "typed summary entries_for_type exposes grouped image entries");
}

void test_typed_materialized_asset_bytes_diff_tracks_engine_entry_deltas()
{
    using namespace quiz_vulkan::assets;

    asset_typed_materialized_bytes_summary before;
    before.fonts.push_back(make_typed_summary_entry(
        "body_font",
        asset_type::font,
        "asset://fonts/body.ttf",
        "/assets/fonts/body.ttf",
        "hash:font"));
    before.images.push_back(make_typed_summary_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "hash:image-old"));
    before.shaders.push_back(make_typed_summary_entry(
        "shared_program",
        asset_type::shader,
        "asset://shaders/shared.vert.spv",
        "/assets/shaders/shared.vert.spv",
        "hash:shader"));

    asset_typed_materialized_bytes_summary after;
    after.images.push_back(make_typed_summary_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front_hd.png",
        "/assets/cards/front_hd.png",
        "hash:image-new"));
    after.sounds.push_back(make_typed_summary_entry(
        "answer_sound",
        asset_type::sound,
        "asset://sounds/correct.ogg",
        "/assets/sounds/correct.ogg",
        "hash:sound"));
    after.decks.push_back(make_typed_summary_entry(
        "shared_program",
        asset_type::deck,
        "asset://decks/shared.quiz",
        "/assets/decks/shared.quiz",
        "hash:deck",
        false));

    const asset_typed_materialized_bytes_diff_summary diff =
        diff_typed_materialized_asset_bytes(before, after);

    require(!diff.empty(), "typed materialized diff reports changes");
    require(diff.change_count() == 4U, "typed materialized diff counts added removed and changed entries");
    require(diff.added.size() == 1U, "typed materialized diff records added entries");
    require(diff.removed.size() == 1U, "typed materialized diff records removed entries");
    require(diff.changed.size() == 2U, "typed materialized diff records changed entries");

    const asset_typed_materialized_bytes_diff_entry* added = diff.find_added("answer_sound");
    require(added != nullptr, "typed materialized diff finds added sound entries");
    require(
        added->kind == asset_typed_materialized_bytes_delta_kind::added,
        "typed materialized diff marks added entries");
    require(added->type == asset_type::sound, "typed materialized diff records added entry type");
    require(!added->before.has_value() && added->after.has_value(), "typed materialized diff keeps added after entry");
    require(
        added->after->cache_key == "sound|asset://sounds/correct.ogg",
        "typed materialized diff keeps added cache key evidence");

    const asset_typed_materialized_bytes_diff_entry* removed = diff.find_removed("body_font");
    require(removed != nullptr, "typed materialized diff finds removed font entries");
    require(
        removed->kind == asset_typed_materialized_bytes_delta_kind::removed,
        "typed materialized diff marks removed entries");
    require(removed->type == asset_type::font, "typed materialized diff records removed entry type");
    require(removed->before.has_value() && !removed->after.has_value(), "typed diff keeps removed before entry");
    require(
        removed->before->source_uri == "asset://fonts/body.ttf",
        "typed materialized diff keeps removed source uri evidence");

    const asset_typed_materialized_bytes_diff_entry* image = diff.find_changed("card_front");
    require(image != nullptr, "typed materialized diff finds changed image entries");
    require(
        image->kind == asset_typed_materialized_bytes_delta_kind::changed,
        "typed materialized diff marks changed entries");
    require(image->type == asset_type::image, "typed materialized diff records changed entry type");
    require(image->before.has_value() && image->after.has_value(), "typed diff keeps before and after entries");
    require(!image->type_changed, "typed materialized diff leaves stable types unchanged");
    require(image->cache_key_changed, "typed materialized diff tracks cache key deltas");
    require(image->source_uri_changed, "typed materialized diff tracks source uri deltas");
    require(image->materialized_path_changed, "typed materialized diff tracks materialized path deltas");
    require(image->content_hash_changed, "typed materialized diff tracks content hash deltas");
    require(!image->integrity_status_changed, "typed materialized diff leaves stable integrity unchanged");
    require(image->has_field_delta(), "typed materialized diff marks changed field deltas");
    require(
        image->before->content_hash == "hash:image-old" && image->after->content_hash == "hash:image-new",
        "typed materialized diff keeps before and after hash evidence");

    const asset_typed_materialized_bytes_diff_entry* moved = diff.find_changed("shared_program");
    require(moved != nullptr, "typed materialized diff finds same-id type changes");
    require(moved->type == asset_type::deck, "typed materialized diff records changed entry after type");
    require(moved->before->type == asset_type::shader, "typed materialized diff keeps before type evidence");
    require(moved->after->type == asset_type::deck, "typed materialized diff keeps after type evidence");
    require(moved->type_changed, "typed materialized diff tracks type deltas");
    require(moved->cache_key_changed, "typed materialized diff tracks cache deltas for type moves");
    require(moved->integrity_status_changed, "typed materialized diff tracks integrity status deltas");
    require(moved->after->load_status == asset_bytes_load_status::missing_bytes, "typed diff keeps load status");
}

void test_materialized_asset_bytes_handoff_summary_groups_ready_and_blocked_payloads()
{
    using namespace quiz_vulkan::assets;

    asset_typed_materialized_bytes_summary typed_summary;
    typed_summary.skipped_generic_count = 2U;
    typed_summary.fonts.push_back(make_typed_summary_entry(
        "body_font",
        asset_type::font,
        "asset://fonts/body.ttf",
        "/assets/fonts/body.ttf",
        "hash:font"));
    typed_summary.sounds.push_back(make_typed_summary_entry(
        "answer_sound",
        asset_type::sound,
        "asset://sounds/correct.ogg",
        "/assets/sounds/correct.ogg",
        "hash:sound"));
    typed_summary.decks.push_back(make_typed_summary_entry(
        "main_deck",
        asset_type::deck,
        "asset://decks/main.quiz",
        "/assets/decks/main.quiz",
        "hash:deck"));

    asset_typed_materialized_bytes_entry image = make_typed_summary_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "hash:image");
    image.issues.push_back(asset_bytes_integrity_issue{
        .kind = asset_bytes_integrity_issue_kind::content_hash_mismatch,
        .id = image.id,
        .type = image.type,
        .cache_key = image.cache_key,
        .expected_cache_key = image.cache_key,
        .source_uri = image.source_uri,
        .expected_source_uri = image.source_uri,
        .reported_byte_count = image.byte_count,
        .actual_byte_count = image.byte_count,
        .reported_content_hash = "hash:image",
        .actual_content_hash = "hash:image-actual",
        .diagnostic = "test image hash mismatch",
    });
    typed_summary.images.push_back(image);

    typed_summary.shaders.push_back(asset_typed_materialized_bytes_entry{
        .id = "rootless_shader",
        .type = asset_type::shader,
        .cache_key = "shader|asset://shaders/ui.vert.spv",
        .source_uri = "asset://shaders/ui.vert.spv",
        .materialized_status = runtime_materialized_asset_lookup_status::missing_rooted_path,
        .load_status = asset_bytes_load_status::source_not_readable,
        .diagnostic = "shader requires a materialized rooted path",
    });

    const asset_materialized_bytes_handoff_summary handoff =
        make_materialized_asset_bytes_handoff_summary(typed_summary);

    require(!handoff.ok(), "handoff summary reports blocked payloads");
    require(handoff.ready_count() == 3U, "handoff summary counts ready payloads");
    require(handoff.blocked_count() == 2U, "handoff summary counts blocked payloads");
    require(handoff.payload_count() == 5U, "handoff summary counts all typed payloads");
    require(handoff.skipped_generic_count == 2U, "handoff summary preserves skipped generic count");
    require(handoff.group_for_type(asset_type::generic).payload_count() == 0U, "handoff summary ignores generic groups");
    require(handoff.fonts.ok(), "handoff summary font group is ready");
    require(!handoff.images.ok(), "handoff summary image group reports blocked payloads");
    require(!handoff.shaders.ok(), "handoff summary shader group reports blocked payloads");
    require(handoff.sounds.ready.size() == 1U, "handoff summary groups ready sounds");
    require(handoff.decks.ready.size() == 1U, "handoff summary groups ready decks");

    const asset_materialized_bytes_handoff_payload* font = handoff.find_ready("body_font");
    require(font != nullptr, "handoff summary can find ready font payloads");
    require(font->ready(), "handoff summary marks ready payloads");
    require(font->type == asset_type::font, "handoff payload preserves type");
    require(font->cache_key == "font|asset://fonts/body.ttf", "handoff payload preserves cache key");
    require(font->source_uri == "asset://fonts/body.ttf", "handoff payload preserves source uri");
    require(font->materialized_path == std::filesystem::path("/assets/fonts/body.ttf"), "handoff payload preserves path");
    require(font->byte_count == 11U, "handoff payload preserves byte count");
    require(font->content_hash == "hash:font", "handoff payload preserves content hash");
    require(font->status == asset_materialized_bytes_handoff_status::ready, "handoff payload records ready status");
    require(font->issues.empty(), "handoff ready payload has no issues");

    const asset_materialized_bytes_handoff_payload* blocked_image = handoff.find_blocked("card_front");
    require(blocked_image != nullptr, "handoff summary can find blocked image payloads");
    require(!blocked_image->ready(), "handoff summary marks blocked image payloads");
    require(
        blocked_image->status == asset_materialized_bytes_handoff_status::integrity_blocked,
        "handoff payload records integrity blocker status");
    require(blocked_image->load_status == asset_bytes_load_status::loaded, "handoff payload preserves load status");
    require(blocked_image->materialized_path == std::filesystem::path("/assets/cards/front.png"), "blocked payload keeps path");
    require(blocked_image->content_hash == "hash:image", "blocked payload keeps hash evidence");
    require(blocked_image->issues.size() == 1U, "blocked payload preserves integrity issues");
    require(blocked_image->diagnostic.empty(), "blocked integrity payload does not invent diagnostics");

    const asset_materialized_bytes_handoff_payload* blocked_shader = handoff.find_blocked("rootless_shader");
    require(blocked_shader != nullptr, "handoff summary can find blocked shader payloads");
    require(
        blocked_shader->status == asset_materialized_bytes_handoff_status::materialization_blocked,
        "handoff payload records materialization blocker status");
    require(
        blocked_shader->materialized_status == runtime_materialized_asset_lookup_status::missing_rooted_path,
        "handoff payload preserves materialization status");
    require(
        blocked_shader->load_status == asset_bytes_load_status::source_not_readable,
        "handoff payload preserves blocked load status");
    require(blocked_shader->materialized_path.empty(), "blocked materialization payload keeps empty path");
    require(
        blocked_shader->diagnostic == "shader requires a materialized rooted path",
        "handoff blocked payload preserves diagnostic");
    require(handoff.find_ready("rootless_shader") == nullptr, "blocked payloads are not exposed as ready");
}

void test_materialized_asset_byte_payload_bundle_groups_loaded_bytes_by_type()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "packaged" / "fonts" / "body.ttf", "font bytes");
    write_fixture_file(fixture_root / "packaged" / "cards" / "front.png", "image bytes");
    write_fixture_file(fixture_root / "packaged" / "sounds" / "correct.ogg", "sound bytes");
    write_fixture_file(fixture_root / "packaged" / "shaders" / "ui.vert.spv", "shader bytes");
    write_fixture_file(fixture_root / "packaged" / "decks" / "main.quiz", "deck bytes");
    write_fixture_file(fixture_root / "packaged" / "misc" / "notes.txt", "generic bytes");

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "packaged",
        .root_path = fixture_root / "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "asset://fonts/body.ttf",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "answer_sound",
        .type = asset_type::sound,
        .uri = "asset://sounds/correct.ogg",
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
        .uri = "asset://decks/main.quiz",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "rootless_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/rootless.vert.spv",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "generic_notes",
        .type = asset_type::generic,
        .uri = "asset://misc/notes.txt",
        .root_id = "packaged",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const local_file_asset_bytes_provider provider;
    const asset_materialized_byte_payload_bundle bundle =
        make_materialized_asset_byte_payload_bundle(provider, catalog);

    require(!bundle.ok(), "payload bundle reports blocked materialized assets");
    require(bundle.ready_count() == 5U, "payload bundle counts ready typed byte payloads");
    require(bundle.blocked_count() == 1U, "payload bundle counts blocked typed byte payloads");
    require(bundle.payload_count() == 6U, "payload bundle counts every typed byte payload");
    require(bundle.skipped_generic_count == 1U, "payload bundle skips generic byte payloads");
    require(bundle.group_for_type(asset_type::generic).payload_count() == 0U, "payload bundle has no generic group");
    require(bundle.cache_policy.request_count == 6U, "payload bundle cache policy tracks typed requests");
    require(bundle.cache_policy.loaded_count == 5U, "payload bundle cache policy counts loaded bytes");
    require(bundle.cache_policy.failed_count == 1U, "payload bundle cache policy counts blocked assets");
    require(bundle.cache_policy.total_byte_count == 54U, "payload bundle cache policy totals loaded byte counts");
    require(bundle.cache_policy.load_failed_count == 1U, "payload bundle cache policy records load blockers");
    require(bundle.handoff.ready_count() == 5U, "payload bundle carries ready handoff evidence");
    require(bundle.handoff.blocked_count() == 1U, "payload bundle carries blocked handoff evidence");
    require(bundle.handoff.skipped_generic_count == 1U, "payload bundle mirrors skipped generic handoff count");

    require(bundle.fonts.ready.size() == 1U, "payload bundle groups ready font bytes");
    require(bundle.images.ready.size() == 1U, "payload bundle groups ready image bytes");
    require(bundle.sounds.ready.size() == 1U, "payload bundle groups ready sound bytes");
    require(bundle.shaders.ready.size() == 1U, "payload bundle groups ready shader bytes");
    require(bundle.shaders.blocked.size() == 1U, "payload bundle groups blocked shader payloads");
    require(bundle.decks.ready.size() == 1U, "payload bundle groups ready deck bytes");

    const asset_materialized_byte_payload* image = bundle.find_ready("card_front");
    require(image != nullptr, "payload bundle can find ready image bytes");
    require(image->ready(), "ready byte payload reports ready status");
    require(image->type == asset_type::image, "ready byte payload preserves type");
    require(image->cache_key == "image|asset://cards/front.png", "ready byte payload preserves cache key");
    require(image->source_uri == "asset://cards/front.png", "ready byte payload preserves source uri");
    require(image->rooted_path.has_value(), "ready byte payload preserves rooted path");
    require(
        *image->rooted_path
            == std::filesystem::absolute(fixture_root / "packaged" / "cards" / "front.png").lexically_normal(),
        "ready byte payload rooted path is normalized");
    require(image->materialized_source_path == "cards/front.png", "ready byte payload preserves source path");
    require(image->materialized_path == *image->rooted_path, "ready byte payload preserves materialized path");
    require(image->byte_count == image->bytes.size(), "ready byte payload byte count matches owned bytes");
    require(bytes_to_string(image->bytes) == "image bytes", "ready byte payload owns loaded image bytes");
    require(
        image->content_hash == make_asset_bytes_content_hash(image->bytes),
        "ready byte payload preserves content hash evidence");
    require(image->issues.empty(), "ready byte payload has no integrity issues");

    asset_materialized_byte_payload copied_image = *image;
    require(
        bytes_to_string(copied_image.bytes) == "image bytes",
        "materialized byte payload copy preserves owned bytes");

    const asset_materialized_byte_payload* font = bundle.find_ready("body_font");
    require(font != nullptr, "payload bundle can find ready font bytes");
    require(bytes_to_string(font->bytes) == "font bytes", "payload bundle stores font bytes");
    const asset_materialized_byte_payload* sound = bundle.find_ready("answer_sound");
    require(sound != nullptr, "payload bundle can find ready sound bytes");
    require(bytes_to_string(sound->bytes) == "sound bytes", "payload bundle stores sound bytes");
    const asset_materialized_byte_payload* shader = bundle.find_ready("ui_shader");
    require(shader != nullptr, "payload bundle can find ready shader bytes");
    require(bytes_to_string(shader->bytes) == "shader bytes", "payload bundle stores shader bytes");
    const asset_materialized_byte_payload* deck = bundle.find_ready("main_deck");
    require(deck != nullptr, "payload bundle can find ready deck bytes");
    require(bytes_to_string(deck->bytes) == "deck bytes", "payload bundle stores deck bytes");

    const asset_materialized_byte_payload* blocked_shader = bundle.find_blocked("rootless_shader");
    require(blocked_shader != nullptr, "payload bundle can find blocked shader payloads");
    require(!blocked_shader->ready(), "blocked byte payload reports blocked status");
    require(
        blocked_shader->status == asset_materialized_bytes_handoff_status::materialization_blocked,
        "blocked byte payload preserves materialization blocker status");
    require(
        blocked_shader->materialized_status == runtime_materialized_asset_lookup_status::missing_rooted_path,
        "blocked byte payload preserves materialization status");
    require(
        blocked_shader->load_status == asset_bytes_load_status::source_not_readable,
        "blocked byte payload preserves load status");
    require(blocked_shader->bytes.empty(), "blocked pre-provider payload has no byte ownership");
    require(blocked_shader->issues.size() == 1U, "blocked byte payload preserves integrity issue evidence");
    require(!blocked_shader->diagnostic.empty(), "blocked byte payload preserves diagnostics");
    require(bundle.find_ready("rootless_shader") == nullptr, "blocked byte payload is not reported as ready");
    require(bundle.cache_policy.find_entry("rootless_shader") != nullptr, "payload bundle preserves cache policy entries");
}

void test_materialized_asset_byte_payload_bundle_diff_tracks_snapshot_changes()
{
    using namespace quiz_vulkan::assets;

    asset_materialized_byte_payload_bundle before;
    before.skipped_generic_count = 1U;
    before.fonts.ready.push_back(make_payload_bundle_entry(
        "body_font",
        asset_type::font,
        "asset://fonts/body.ttf",
        "/assets/fonts/body.ttf",
        "font bytes",
        "hash:font"));
    before.images.ready.push_back(make_payload_bundle_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "image bytes",
        "hash:image-old"));
    before.shaders.blocked.push_back(make_payload_bundle_entry(
        "ui_shader",
        asset_type::shader,
        "asset://shaders/ui.vert.spv",
        "/assets/shaders/ui.vert.spv",
        "",
        "hash:shader-missing",
        asset_materialized_bytes_handoff_status::materialization_blocked));

    asset_materialized_byte_payload_bundle after;
    after.skipped_generic_count = 1U;
    asset_materialized_byte_payload changed_image = make_payload_bundle_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "image bytes!",
        "hash:image-new");
    changed_image.cache_key = "image|asset://cards/front.png|rev=2";
    after.images.ready.push_back(std::move(changed_image));
    after.sounds.ready.push_back(make_payload_bundle_entry(
        "answer_sound",
        asset_type::sound,
        "asset://sounds/correct.ogg",
        "/assets/sounds/correct.ogg",
        "sound bytes",
        "hash:sound"));
    after.shaders.ready.push_back(make_payload_bundle_entry(
        "ui_shader",
        asset_type::shader,
        "asset://shaders/ui.vert.spv",
        "/assets/shaders/ui.vert.spv",
        "shader bytes",
        "hash:shader-ready"));

    const asset_materialized_byte_payload_bundle_snapshot before_snapshot =
        snapshot_materialized_asset_byte_payload_bundle(before);
    const asset_materialized_byte_payload_bundle_snapshot after_snapshot =
        snapshot_materialized_asset_byte_payload_bundle(after);

    require(!before_snapshot.ok(), "payload snapshot reports blocked payloads");
    require(before_snapshot.payload_count() == 3U, "payload snapshot counts all before payloads");
    require(before_snapshot.ready_count() == 2U, "payload snapshot counts ready before payloads");
    require(before_snapshot.blocked_count() == 1U, "payload snapshot counts blocked before payloads");
    require(before_snapshot.skipped_generic_count == 1U, "payload snapshot preserves skipped generic count");

    const asset_materialized_byte_payload_snapshot* before_image = before_snapshot.find_payload("card_front");
    require(before_image != nullptr, "payload snapshot can find image payloads");
    require(before_image->type == asset_type::image, "payload snapshot keeps type evidence");
    require(before_image->cache_key == "image|asset://cards/front.png", "payload snapshot keeps cache key");
    require(before_image->content_hash == "hash:image-old", "payload snapshot keeps content hash evidence");
    require(before_image->byte_count == 11U, "payload snapshot keeps reported byte count");
    require(before_image->payload_byte_count == 11U, "payload snapshot keeps owned byte count");
    require(before_image->ready, "payload snapshot keeps readiness evidence");

    const asset_materialized_byte_payload_snapshot single_snapshot =
        make_materialized_asset_byte_payload_snapshot(before.images.ready[0]);
    require(
        single_snapshot.payload_byte_count == before.images.ready[0].bytes.size(),
        "single payload snapshot records byte-vector size without copying bytes");

    const asset_materialized_byte_payload_diff_summary diff =
        diff_materialized_asset_byte_payload_snapshots(before_snapshot, after_snapshot);
    const asset_materialized_byte_payload_diff_summary bundle_diff =
        diff_materialized_asset_byte_payload_bundles(before, after);

    require(!diff.empty(), "payload bundle diff reports changes");
    require(diff.change_count() == 4U, "payload bundle diff counts added removed and changed entries");
    require(bundle_diff.change_count() == diff.change_count(), "bundle diff matches snapshot diff");
    require(diff.added.size() == 1U, "payload bundle diff records added payloads");
    require(diff.removed.size() == 1U, "payload bundle diff records removed payloads");
    require(diff.changed.size() == 2U, "payload bundle diff records changed payloads");

    const asset_materialized_byte_payload_diff_entry* added = diff.find_added("answer_sound");
    require(added != nullptr, "payload bundle diff finds added sound payloads");
    require(
        added->kind == asset_materialized_byte_payload_delta_kind::added,
        "payload bundle diff marks added payloads");
    require(added->type == asset_type::sound, "payload bundle diff records added type");
    require(!added->before.has_value() && added->after.has_value(), "added payload diff only keeps after snapshot");
    require(added->after->content_hash == "hash:sound", "added payload diff keeps content hash evidence");
    require(added->after->payload_byte_count == 11U, "added payload diff keeps byte-count evidence");

    const asset_materialized_byte_payload_diff_entry* removed = diff.find_removed("body_font");
    require(removed != nullptr, "payload bundle diff finds removed font payloads");
    require(
        removed->kind == asset_materialized_byte_payload_delta_kind::removed,
        "payload bundle diff marks removed payloads");
    require(removed->type == asset_type::font, "payload bundle diff records removed type");
    require(removed->before.has_value() && !removed->after.has_value(), "removed payload diff only keeps before snapshot");
    require(removed->before->cache_key == "font|asset://fonts/body.ttf", "removed payload diff keeps cache key");

    const asset_materialized_byte_payload_diff_entry* image = diff.find_changed("card_front");
    require(image != nullptr, "payload bundle diff finds changed image payloads");
    require(
        image->kind == asset_materialized_byte_payload_delta_kind::changed,
        "payload bundle diff marks changed payloads");
    require(!image->type_changed, "payload bundle diff leaves stable types unchanged");
    require(image->cache_key_changed, "payload bundle diff tracks cache-key changes");
    require(!image->source_uri_changed, "payload bundle diff leaves stable source uri unchanged");
    require(!image->materialized_path_changed, "payload bundle diff leaves stable materialized path unchanged");
    require(image->byte_count_changed, "payload bundle diff tracks reported byte-count changes");
    require(image->payload_byte_count_changed, "payload bundle diff tracks owned-byte-count changes");
    require(image->content_hash_changed, "payload bundle diff tracks content-hash changes");
    require(!image->status_changed, "payload bundle diff leaves stable status unchanged");
    require(!image->readiness_changed, "payload bundle diff leaves stable readiness unchanged");
    require(image->has_field_delta(), "payload bundle diff reports changed fields");
    require(
        image->before->content_hash == "hash:image-old" && image->after->content_hash == "hash:image-new",
        "payload bundle diff keeps before and after hash evidence");

    const asset_materialized_byte_payload_diff_entry* shader = diff.find_changed("ui_shader");
    require(shader != nullptr, "payload bundle diff finds readiness changes");
    require(shader->status_changed, "payload bundle diff tracks blocker status changes");
    require(shader->readiness_changed, "payload bundle diff tracks readiness changes");
    require(shader->content_hash_changed, "payload bundle diff tracks blocker content hash changes");
    require(shader->payload_byte_count_changed, "payload bundle diff tracks bytes appearing after materialization");
    require(!shader->before->ready && shader->after->ready, "payload bundle diff keeps before and after readiness");
    require(
        shader->before->status == asset_materialized_bytes_handoff_status::materialization_blocked
            && shader->after->status == asset_materialized_bytes_handoff_status::ready,
        "payload bundle diff keeps before and after status");
}

void test_shader_materialized_byte_pipeline_summary_classifies_shader_payloads()
{
    using namespace quiz_vulkan::assets;

    asset_materialized_byte_payload_bundle bundle;
    bundle.images.ready.push_back(make_payload_bundle_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "image bytes",
        ""));

    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "ui_shader",
        "asset://shaders/ui.vert.spv",
        "/assets/shaders/ui.vert.spv",
        make_spirv_fixture_bytes()));

    bundle.shaders.blocked.push_back(make_shader_payload_bundle_entry(
        "rootless_shader",
        "asset://shaders/rootless.vert.spv",
        "/assets/shaders/rootless.vert.spv",
        {},
        asset_materialized_bytes_handoff_status::materialization_blocked));

    asset_materialized_byte_payload blocked_load = make_shader_payload_bundle_entry(
        "missing_shader_bytes",
        "asset://shaders/missing.frag.spv",
        "/assets/shaders/missing.frag.spv",
        {},
        asset_materialized_bytes_handoff_status::load_blocked);
    blocked_load.materialized_status = runtime_materialized_asset_lookup_status::materialized;
    blocked_load.load_status = asset_bytes_load_status::missing_bytes;
    blocked_load.diagnostic = "test provider did not return shader bytes";
    bundle.shaders.blocked.push_back(std::move(blocked_load));

    asset_materialized_byte_payload integrity_failure = make_shader_payload_bundle_entry(
        "corrupt_shader",
        "asset://shaders/corrupt.comp.spv",
        "/assets/shaders/corrupt.comp.spv",
        make_spirv_fixture_bytes(),
        asset_materialized_bytes_handoff_status::integrity_blocked);
    bundle.shaders.blocked.push_back(std::move(integrity_failure));

    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "empty_shader",
        "asset://shaders/empty.vert.spv",
        "/assets/shaders/empty.vert.spv",
        {}));
    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "text_shader",
        "asset://shaders/text.vert.spv",
        "/assets/shaders/text.vert.spv",
        detail::make_asset_byte_vector("not spirv")));
    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "duplicate_shader",
        "asset://shaders/duplicate_a.vert.spv",
        "/assets/shaders/duplicate_a.vert.spv",
        make_spirv_fixture_bytes()));
    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "duplicate_shader",
        "asset://shaders/duplicate_b.vert.spv",
        "/assets/shaders/duplicate_b.vert.spv",
        make_spirv_fixture_bytes()));

    const asset_shader_materialized_byte_pipeline_summary summary =
        summarize_shader_materialized_byte_pipeline(bundle);

    require(!summary.ok(), "shader pipeline summary reports blocked shader payloads");
    require(summary.input_shader_count == 8U, "shader pipeline summary counts input shader payloads");
    require(summary.entry_count() == 8U, "shader pipeline summary emits one entry per shader payload");
    require(summary.ready_count() == 1U, "shader pipeline summary keeps only valid shader bytes ready");
    require(summary.blocked_count() == 7U, "shader pipeline summary blocks invalid shader payloads");
    require(summary.blocked_materialization_count == 1U, "shader pipeline counts blocked materialization");
    require(summary.blocked_byte_load_count == 1U, "shader pipeline counts blocked byte loads");
    require(summary.integrity_failure_count == 1U, "shader pipeline counts integrity failures");
    require(summary.empty_shader_bytes_count == 1U, "shader pipeline counts empty shader bytes");
    require(summary.non_spirv_magic_count == 1U, "shader pipeline counts non-SPIR-V-looking .spv bytes");
    require(summary.duplicate_id_count == 2U, "shader pipeline counts duplicate shader id entries");
    require(summary.issue_count() == 7U, "shader pipeline totals all validation issues");
    require(summary.find_entry("card_front") == nullptr, "shader pipeline ignores non-shader payload groups");
    require(
        asset_shader_materialized_byte_issue_kind_name(
            asset_shader_materialized_byte_issue_kind::blocked_materialization)
            == "blocked_materialization",
        "shader pipeline issue kind names include blocked materialization");
    require(
        asset_shader_materialized_byte_issue_kind_name(
            asset_shader_materialized_byte_issue_kind::blocked_byte_load)
            == "blocked_byte_load",
        "shader pipeline issue kind names include blocked byte load");
    require(
        asset_shader_materialized_byte_issue_kind_name(asset_shader_materialized_byte_issue_kind::integrity_failure)
            == "integrity_failure",
        "shader pipeline issue kind names include integrity failure");
    require(
        asset_shader_materialized_byte_issue_kind_name(
            asset_shader_materialized_byte_issue_kind::empty_shader_bytes)
            == "empty_shader_bytes",
        "shader pipeline issue kind names include empty shader bytes");
    require(
        asset_shader_materialized_byte_issue_kind_name(asset_shader_materialized_byte_issue_kind::non_spirv_magic)
            == "non_spirv_magic",
        "shader pipeline issue kind names include SPIR-V magic failures");
    require(
        asset_shader_materialized_byte_issue_kind_name(asset_shader_materialized_byte_issue_kind::duplicate_id)
            == "duplicate_id",
        "shader pipeline issue kind names include duplicate ids");

    const asset_shader_materialized_byte_pipeline_entry* ready = summary.find_ready("ui_shader");
    require(ready != nullptr, "shader pipeline can find ready shader payloads");
    require(ready->ready(), "shader pipeline marks valid shader bytes ready");
    require(ready->type == asset_type::shader, "shader pipeline preserves shader type");
    require(ready->cache_key == "shader|asset://shaders/ui.vert.spv", "shader pipeline preserves cache key");
    require(ready->source_uri == "asset://shaders/ui.vert.spv", "shader pipeline preserves source uri");
    require(
        ready->materialized_path == std::filesystem::path("/assets/shaders/ui.vert.spv"),
        "shader pipeline preserves materialized path");
    require(ready->byte_count == 8U, "shader pipeline preserves reported byte count");
    require(ready->payload_byte_count == 8U, "shader pipeline records owned shader byte count");
    require(
        ready->content_hash == make_asset_bytes_content_hash(make_spirv_fixture_bytes()),
        "shader pipeline preserves content hash evidence");
    require(ready->payload_status == asset_materialized_bytes_handoff_status::ready, "ready shader keeps status");
    require(ready->spirv_expected, "shader pipeline recognizes .spv shader payloads");
    require(ready->spirv_magic_checked, "shader pipeline checks SPIR-V magic for non-empty .spv payloads");
    require(ready->spirv_magic_valid, "shader pipeline accepts SPIR-V magic bytes");
    require(ready->issues.empty(), "ready shader has no validation issues");
    require(
        !ready->has_issue(asset_shader_materialized_byte_issue_kind::non_spirv_magic),
        "ready shader issue lookup reports no SPIR-V magic failure");
    require(ready->diagnostic == "shader materialized byte payload ready", "ready shader diagnostic is stable");

    const asset_shader_materialized_byte_pipeline_entry* rootless = summary.find_blocked("rootless_shader");
    require(rootless != nullptr, "shader pipeline can find materialization blockers");
    require(!rootless->ready(), "shader pipeline blocks unmaterialized shader payloads");
    require(
        shader_pipeline_issue_at(
            *rootless,
            0U,
            asset_shader_materialized_byte_issue_kind::blocked_materialization),
        "shader pipeline reports blocked materialization issue");
    require(
        rootless->has_issue(asset_shader_materialized_byte_issue_kind::blocked_materialization),
        "shader pipeline issue lookup finds blocked materialization");
    require(
        rootless->materialized_status == runtime_materialized_asset_lookup_status::missing_rooted_path,
        "shader pipeline preserves materialization blocker status");
    require(rootless->payload_byte_count == 0U, "blocked materialization has no owned shader bytes");

    const asset_shader_materialized_byte_pipeline_entry* missing_bytes =
        summary.find_blocked("missing_shader_bytes");
    require(missing_bytes != nullptr, "shader pipeline can find byte-load blockers");
    require(
        shader_pipeline_issue_at(
            *missing_bytes,
            0U,
            asset_shader_materialized_byte_issue_kind::blocked_byte_load),
        "shader pipeline reports blocked byte-load issue");
    require(
        missing_bytes->load_status == asset_bytes_load_status::missing_bytes,
        "shader pipeline preserves blocked byte-load status");

    const asset_shader_materialized_byte_pipeline_entry* corrupt = summary.find_blocked("corrupt_shader");
    require(corrupt != nullptr, "shader pipeline can find integrity failures");
    require(
        shader_pipeline_issue_at(
            *corrupt,
            0U,
            asset_shader_materialized_byte_issue_kind::integrity_failure),
        "shader pipeline reports integrity failure issue");
    require(
        corrupt->payload_status == asset_materialized_bytes_handoff_status::integrity_blocked,
        "shader pipeline preserves integrity blocker status");

    const asset_shader_materialized_byte_pipeline_entry* empty = summary.find_blocked("empty_shader");
    require(empty != nullptr, "shader pipeline can find empty shader bytes");
    require(
        shader_pipeline_issue_at(*empty, 0U, asset_shader_materialized_byte_issue_kind::empty_shader_bytes),
        "shader pipeline reports empty shader bytes issue");
    require(empty->payload_byte_count == 0U, "empty shader records zero payload bytes");
    require(empty->spirv_expected, "empty .spv shader records SPIR-V expectation");
    require(!empty->spirv_magic_checked, "empty shader bytes do not produce a magic check");

    const asset_shader_materialized_byte_pipeline_entry* text_shader = summary.find_blocked("text_shader");
    require(text_shader != nullptr, "shader pipeline can find non-SPIR-V-looking bytes");
    require(
        shader_pipeline_issue_at(
            *text_shader,
            0U,
            asset_shader_materialized_byte_issue_kind::non_spirv_magic),
        "shader pipeline reports non-SPIR-V magic issue");
    require(
        text_shader->has_issue(asset_shader_materialized_byte_issue_kind::non_spirv_magic),
        "shader pipeline issue lookup finds non-SPIR-V magic failures");
    require(text_shader->spirv_magic_checked, "non-empty .spv shader bytes are magic checked");
    require(!text_shader->spirv_magic_valid, "text shader bytes fail SPIR-V magic validation");

    const asset_shader_materialized_byte_pipeline_entry* duplicate = summary.find_blocked("duplicate_shader");
    require(duplicate != nullptr, "shader pipeline can find duplicate shader ids");
    require(duplicate->duplicate_count == 2U, "shader pipeline records duplicate match count");
    require(
        shader_pipeline_issue_at(*duplicate, 0U, asset_shader_materialized_byte_issue_kind::duplicate_id),
        "shader pipeline reports duplicate shader id issue");

    std::size_t duplicate_entries = 0U;
    for (const asset_shader_materialized_byte_pipeline_entry& entry : summary.blocked) {
        if (entry.id == "duplicate_shader") {
            ++duplicate_entries;
        }
    }
    require(duplicate_entries == 2U, "shader pipeline keeps both duplicate shader entries as blocked evidence");
}

void test_shader_byte_pipeline_source_summary_combines_manifest_fallback_and_payload_evidence()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();

    asset_manifest manifest;
    manifest.roots.push_back(asset_manifest_root{
        .id = "fixture",
        .aliases = {"fallback_shaders"},
        .root_path = fixture_root / "packaged",
    });
    manifest.roots.push_back(asset_manifest_root{
        .id = "external_shader_pack",
        .root_path = fixture_root / "build" / "external" / "shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "manifest_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/manifest.vert.spv",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "external_shader",
        .type = asset_type::shader,
        .uri = "shaders/external.vert.spv",
        .root_id = "external_shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "fixture_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/fixture.vert.spv",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "fallback_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/fallback.vert.spv",
        .root_id = "fallback_shaders",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "traversal_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/%2e%2e/secret.vert.spv",
        .root_id = "fixture",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_root_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/missing-root.vert.spv",
        .root_id = "missing",
    });

    const normalizing_asset_resolver resolver;
    const asset_runtime_resolver_policy_summary resolver_policy =
        summarize_asset_runtime_resolver_policy(manifest, resolver);
    const asset_pack_index_root_selection_summary root_selection =
        summarize_asset_pack_index_root_selection(manifest);

    asset_materialized_byte_payload_bundle bundle;
    bundle.shaders.blocked.push_back(make_shader_payload_bundle_entry(
        "manifest_shader",
        "asset://shaders/manifest.vert.spv",
        "/assets/shaders/manifest.vert.spv",
        {},
        asset_materialized_bytes_handoff_status::materialization_blocked));
    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "external_shader",
        "shaders/external.vert.spv",
        "/assets/shaders/external.vert.spv",
        make_spirv_fixture_bytes()));
    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "fixture_shader",
        "asset://shaders/fixture.vert.spv",
        "/assets/shaders/fixture.vert.spv",
        make_spirv_fixture_bytes()));
    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "fallback_shader",
        "asset://shaders/fallback.vert.spv",
        "/assets/shaders/fallback.vert.spv",
        make_spirv_fixture_bytes()));
    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "stale_shader",
        "asset://shaders/stale.vert.spv",
        "/assets/shaders/stale.vert.spv",
        make_spirv_fixture_bytes()));

    const asset_shader_materialized_byte_pipeline_summary shader_pipeline =
        summarize_shader_materialized_byte_pipeline(bundle);
    const std::vector<std::string> expected_shader_ids{
        "manifest_shader",
        "external_shader",
        "fixture_shader",
        "fallback_shader",
        "traversal_shader",
        "missing_root_shader",
        "missing_shader",
        "stale_shader",
    };

    const asset_shader_byte_pipeline_source_summary source_summary =
        summarize_shader_byte_pipeline_sources(
            shader_pipeline,
            resolver_policy,
            root_selection,
            expected_shader_ids);

    require(!source_summary.ok(), "shader source summary reports blocked source evidence");
    require(source_summary.input_shader_count == 5U, "shader source summary preserves payload input count");
    require(source_summary.requested_shader_count == 8U, "shader source summary records expected shader ids");
    require(source_summary.entry_count() == 8U, "shader source summary emits requested missing source evidence");
    require(source_summary.ready_count() == 3U, "shader source summary only marks consumer-ready shader bytes ready");
    require(source_summary.blocked_count() == 5U, "shader source summary blocks source and payload problems");
    require(source_summary.manifest_source_count == 1U, "shader source summary classifies direct manifest sources");
    require(source_summary.build_external_source_count == 1U, "shader source summary classifies build external roots");
    require(source_summary.local_fixture_source_count == 1U, "shader source summary classifies local fixture roots");
    require(source_summary.fallback_source_count == 1U, "shader source summary classifies fallback root sources");
    require(source_summary.missing_source_count == 4U, "shader source summary classifies missing source evidence");
    require(source_summary.traversal_rejection_count == 1U, "shader source summary counts traversal rejections");
    require(source_summary.missing_root_count == 1U, "shader source summary counts missing roots");
    require(source_summary.missing_manifest_count == 1U, "shader source summary counts missing manifest entries");
    require(source_summary.stale_manifest_count == 1U, "shader source summary counts stale payload manifest evidence");
    require(
        asset_shader_byte_pipeline_source_kind_name(asset_shader_byte_pipeline_source_kind::fallback)
            == "fallback",
        "shader source summary exposes stable source kind names");
    require(
        asset_shader_byte_pipeline_blocker_kind_name(asset_shader_byte_pipeline_blocker_kind::traversal_rejected)
            == "traversal_rejected",
        "shader source summary exposes stable blocker names");

    const asset_shader_byte_pipeline_source_entry* external = source_summary.find_ready("external_shader");
    require(external != nullptr, "shader source summary finds build external ready shaders");
    require(external->ok(), "build external shader source is ready for consumers");
    require(
        external->source_kind == asset_shader_byte_pipeline_source_kind::build_external,
        "build external shader source kind is explicit");
    require(
        external->root_space == asset_runtime_resolver_root_space::build_external,
        "build external shader keeps root-space classification");
    require(external->manifest_entry_found, "build external shader keeps manifest evidence");
    require(external->cache_key == "shader|shaders/external.vert.spv", "build external shader keeps cache key");
    require(external->source_uri == "shaders/external.vert.spv", "build external shader keeps source uri");
    require(external->byte_count == 8U, "build external shader keeps reported byte count");
    require(external->payload_byte_count == 8U, "build external shader keeps owned byte count");
    require(
        external->content_hash == make_asset_bytes_content_hash(make_spirv_fixture_bytes()),
        "build external shader keeps content hash");
    require(
        external->materialized_byte_identity
            == "shader|shaders/external.vert.spv|hash="
                + make_asset_bytes_content_hash(make_spirv_fixture_bytes()) + "|bytes=8",
        "build external shader has stable byte identity");

    const asset_shader_byte_pipeline_source_entry* fixture = source_summary.find_ready("fixture_shader");
    require(fixture != nullptr, "shader source summary finds fixture ready shaders");
    require(
        fixture->source_kind == asset_shader_byte_pipeline_source_kind::local_fixture,
        "fixture shader source kind is explicit");
    require(
        fixture->root_space == asset_runtime_resolver_root_space::local_fixture,
        "fixture shader keeps root-space classification");

    const asset_shader_byte_pipeline_source_entry* fallback = source_summary.find_ready("fallback_shader");
    require(fallback != nullptr, "shader source summary finds fallback ready shaders");
    require(
        fallback->source_kind == asset_shader_byte_pipeline_source_kind::fallback,
        "fallback shader source kind is explicit");
    require(fallback->fallback_selected, "fallback shader records fallback selection");
    require(fallback->requested_root_id == "fallback_shaders", "fallback shader keeps requested root id");
    require(fallback->selected_root_id == "fixture", "fallback shader keeps selected root id");

    const asset_shader_byte_pipeline_source_entry* manifest_only =
        source_summary.find_blocked("manifest_shader");
    require(manifest_only != nullptr, "shader source summary finds rootless manifest shaders");
    require(
        manifest_only->source_kind == asset_shader_byte_pipeline_source_kind::manifest,
        "rootless manifest shader source kind remains manifest");
    require(
        manifest_only->blocker == asset_shader_byte_pipeline_blocker_kind::materialization_blocked,
        "rootless manifest shader records materialization blocker");
    require(manifest_only->manifest_entry_found, "rootless manifest shader keeps manifest evidence");

    const asset_shader_byte_pipeline_source_entry* traversal =
        source_summary.find_blocked("traversal_shader");
    require(traversal != nullptr, "shader source summary includes traversal rejection evidence");
    require(
        traversal->blocker == asset_shader_byte_pipeline_blocker_kind::traversal_rejected,
        "traversal shader records traversal blocker");
    require(traversal->traversal_rejected, "traversal shader exposes traversal rejection flag");
    require(
        traversal->source_kind == asset_shader_byte_pipeline_source_kind::missing_source,
        "traversal shader is not source-ready");

    const asset_shader_byte_pipeline_source_entry* missing_root =
        source_summary.find_blocked("missing_root_shader");
    require(missing_root != nullptr, "shader source summary includes missing root evidence");
    require(
        missing_root->blocker == asset_shader_byte_pipeline_blocker_kind::missing_root,
        "missing root shader records missing root blocker");
    require(
        missing_root->cache_key == "shader|asset://shaders/missing-root.vert.spv",
        "missing root shader preserves diagnostic cache key");

    const asset_shader_byte_pipeline_source_entry* missing_manifest =
        source_summary.find_blocked("missing_shader");
    require(missing_manifest != nullptr, "shader source summary includes requested missing ids");
    require(
        missing_manifest->blocker == asset_shader_byte_pipeline_blocker_kind::missing_manifest_entry,
        "missing shader records missing manifest blocker");
    require(!missing_manifest->manifest_entry_found, "missing shader records absent manifest evidence");

    const asset_shader_byte_pipeline_source_entry* stale = source_summary.find_blocked("stale_shader");
    require(stale != nullptr, "shader source summary includes stale payload evidence");
    require(
        stale->blocker == asset_shader_byte_pipeline_blocker_kind::stale_manifest_entry,
        "stale shader records stale manifest blocker");
    require(!stale->ready, "stale shader bytes are not consumer-ready without manifest evidence");
    require(!stale->materialized_byte_identity.empty(), "stale shader keeps materialized byte identity");
    require(
        stale->diagnostic == "shader byte pipeline payload has no current manifest evidence",
        "stale shader diagnostic is stable");
}

void test_shader_payload_runtime_summary_tracks_stage_revision_and_cache_identity()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const std::filesystem::path before_root = fixture_root / "runtime-before";
    const std::filesystem::path after_root = fixture_root / "runtime-after";
    const std::string spirv_a = bytes_to_string(make_spirv_fixture_bytes());
    std::string spirv_b = spirv_a;
    spirv_b.push_back(static_cast<char>(0x01));

    write_fixture_file(before_root / "external" / "shaders" / "stable.vert.spv", spirv_a);
    write_fixture_file(before_root / "external" / "shaders" / "revision.vert.spv", spirv_a);
    write_fixture_file(before_root / "external" / "shaders" / "effect.frag.spv", spirv_a);
    write_fixture_file(before_root / "external" / "shaders" / "task.comp.spv", spirv_a);
    write_fixture_file(after_root / "external" / "shaders" / "stable.vert.spv", spirv_a);
    write_fixture_file(after_root / "external" / "shaders" / "revision.vert.spv", spirv_a);
    write_fixture_file(after_root / "external" / "shaders" / "effect.frag.spv", spirv_b);
    write_fixture_file(after_root / "external" / "shaders" / "task.comp.spv", spirv_a);

    const auto make_runtime_summary =
        [](const std::filesystem::path& root, std::string shader_revision) {
            asset_manifest manifest;
            manifest.roots.push_back(asset_manifest_root{
                .id = "external_shader_pack",
                .root_path = root / "external",
            });
            manifest.entries.push_back(asset_manifest_entry{
                .id = "stable_vertex",
                .type = asset_type::shader,
                .uri = "asset://shaders/stable.vert.spv",
                .root_id = "external_shader_pack",
            });
            manifest.entries.push_back(asset_manifest_entry{
                .id = "revision_vertex",
                .type = asset_type::shader,
                .uri = "asset://shaders/revision.vert.spv",
                .root_id = "external_shader_pack",
                .cache_revision = std::move(shader_revision),
            });
            manifest.entries.push_back(asset_manifest_entry{
                .id = "changed_fragment",
                .type = asset_type::shader,
                .uri = "asset://shaders/effect.frag.spv",
                .root_id = "external_shader_pack",
                .cache_revision = "fragment-rev1",
            });
            manifest.entries.push_back(asset_manifest_entry{
                .id = "compute_shader",
                .type = asset_type::shader,
                .uri = "asset://shaders/task.comp.spv",
                .root_id = "external_shader_pack",
            });

            const normalizing_asset_resolver resolver;
            const local_file_asset_bytes_provider provider;
            const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
            const asset_materialized_byte_payload_bundle bundle =
                make_materialized_asset_byte_payload_bundle(provider, catalog);
            const asset_shader_materialized_byte_pipeline_summary shader_pipeline =
                summarize_shader_materialized_byte_pipeline(bundle);
            const asset_runtime_resolver_policy_summary resolver_policy =
                summarize_asset_runtime_resolver_policy(manifest, resolver);
            const asset_pack_index_root_selection_summary root_selection =
                summarize_asset_pack_index_root_selection(manifest);
            const asset_shader_byte_pipeline_source_summary source_summary =
                summarize_shader_byte_pipeline_sources(shader_pipeline, resolver_policy, root_selection);
            return summarize_shader_payload_runtime(source_summary);
        };

    const asset_shader_payload_runtime_summary before =
        make_runtime_summary(before_root, "shader-rev1");
    const asset_shader_payload_runtime_summary after =
        make_runtime_summary(after_root, "shader-rev2");

    require(before.ok(), "shader runtime summary starts with ready shader byte entries");
    require(after.ok(), "updated shader runtime summary starts with ready shader byte entries");
    require(before.entry_count() == 4U, "shader runtime summary emits every shader payload");
    require(before.ready_count() == 4U, "shader runtime summary keeps valid shader payloads ready");
    require(before.blocked_count() == 0U, "shader runtime summary has no blocked valid shader payloads");
    require(before.vertex_stage_count == 2U, "shader runtime summary counts vertex-like shader payloads");
    require(before.fragment_stage_count == 1U, "shader runtime summary counts fragment-like shader payloads");
    require(before.compute_stage_count == 1U, "shader runtime summary counts compute-like shader payloads");
    require(before.unknown_stage_count == 0U, "shader runtime summary classifies known shader stages");
    require(before.revisioned_count == 2U, "shader runtime summary counts revised shader cache keys");
    require(before.missing_revision_count == 2U, "shader runtime summary counts shader keys without revisions");

    const asset_shader_payload_runtime_entry* stable_before = before.find_ready("stable_vertex");
    const asset_shader_payload_runtime_entry* stable_after = after.find_ready("stable_vertex");
    require(stable_before != nullptr && stable_after != nullptr, "shader runtime summary finds stable shader payloads");
    require(stable_before->ok() && stable_after->ok(), "stable shader runtime entries are consumer-ready");
    require(
        stable_before->stage == asset_shader_payload_runtime_stage::vertex,
        "stable shader runtime entry infers vertex stage from source uri");
    require(stable_before->cache_revision.empty(), "stable shader runtime entry has no revision token");
    require(
        stable_before->runtime_identity == stable_after->runtime_identity,
        "unchanged shader bytes reuse stable runtime identity across materialized roots");
    require(
        stable_before->materialized_path != stable_after->materialized_path,
        "stable shader runtime evidence can move between materialized roots");
    require(
        stable_before->cache_key == stable_after->cache_key,
        "stable shader runtime evidence keeps the same cache key");
    require(
        stable_before->content_hash == stable_after->content_hash,
        "stable shader runtime evidence keeps the same content hash");
    require(
        stable_before->runtime_identity.find(fixture_root.string()) == std::string::npos
            && stable_after->runtime_identity.find(fixture_root.string()) == std::string::npos,
        "shader runtime identity does not leak absolute fixture paths");
    require(
        before.find_runtime_identity(stable_before->runtime_identity) == stable_before,
        "shader runtime summary can find ready entries by runtime identity");

    const asset_shader_payload_runtime_entry* revision_before = before.find_ready("revision_vertex");
    const asset_shader_payload_runtime_entry* revision_after = after.find_ready("revision_vertex");
    require(
        revision_before != nullptr && revision_after != nullptr,
        "shader runtime summary finds revisioned shader payloads");
    require(
        revision_before->cache_revision == "shader-rev1" && revision_after->cache_revision == "shader-rev2",
        "shader runtime entries expose parsed cache revisions");
    require(
        revision_before->content_hash == revision_after->content_hash,
        "revisioned shader fixture keeps identical byte hash evidence");
    require(
        revision_before->runtime_identity != revision_after->runtime_identity,
        "shader revision changes invalidate runtime identity even when bytes are unchanged");

    const asset_shader_payload_runtime_entry* fragment_before = before.find_ready("changed_fragment");
    const asset_shader_payload_runtime_entry* fragment_after = after.find_ready("changed_fragment");
    require(
        fragment_before != nullptr && fragment_after != nullptr,
        "shader runtime summary finds content-changing shader payloads");
    require(
        fragment_before->stage == asset_shader_payload_runtime_stage::fragment,
        "shader runtime entry infers fragment stage from source uri");
    require(
        fragment_before->cache_revision == fragment_after->cache_revision,
        "content-changing shader keeps stable manifest revision token");
    require(
        fragment_before->content_hash != fragment_after->content_hash,
        "content-changing shader records changed hash evidence");
    require(
        fragment_before->runtime_identity != fragment_after->runtime_identity,
        "changed shader bytes invalidate runtime identity");

    const asset_shader_payload_runtime_entry* compute = before.find_ready("compute_shader");
    require(compute != nullptr, "shader runtime summary finds compute shader payloads");
    require(
        compute->stage == asset_shader_payload_runtime_stage::compute,
        "shader runtime entry infers compute stage from source uri");
    require(
        asset_shader_payload_runtime_stage_name(compute->stage) == "compute",
        "shader runtime stage names are stable");
}

void test_render_resource_payload_bridge_matches_addresses_to_materialized_payloads()
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
        .id = "card_front",
        .type = asset_type::image,
        .uri = "asset://cards/front.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "fonts/body.ttf",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "shaders/ui.vert.spv",
        .root_id = "external_shader_pack",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "missing_image",
        .type = asset_type::image,
        .uri = "asset://cards/missing.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "wrong_type",
        .type = asset_type::image,
        .uri = "asset://cards/wrong-type.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "stale_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/stale.vert.spv",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "duplicate_a",
        .type = asset_type::image,
        .uri = "asset://cards/shared.png",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "duplicate_b",
        .type = asset_type::image,
        .uri = "ASSET:///cards/./shared.png",
    });

    const normalizing_asset_resolver resolver;
    const asset_render_resource_address_summary addresses =
        summarize_asset_render_resource_addresses(manifest, resolver);
    require(addresses.ok(), "render resource bridge fixture starts from accepted addresses");
    require(addresses.entry_count() == 8U, "render resource bridge fixture contains all addresses");

    asset_materialized_byte_payload_bundle bundle;
    bundle.images.ready.push_back(make_payload_bundle_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "image bytes",
        ""));
    bundle.fonts.ready.push_back(make_payload_bundle_entry(
        "body_font",
        asset_type::font,
        "fonts/body.ttf",
        "/assets/fonts/body.ttf",
        "font bytes",
        ""));
    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "ui_shader",
        "shaders/ui.vert.spv",
        "/assets/shaders/ui.vert.spv",
        make_spirv_fixture_bytes()));
    bundle.sounds.ready.push_back(make_payload_bundle_entry(
        "wrong_type",
        asset_type::sound,
        "asset://sounds/wrong-type.ogg",
        "/assets/sounds/wrong-type.ogg",
        "sound bytes",
        ""));
    bundle.shaders.ready.push_back(make_shader_payload_bundle_entry(
        "stale_shader",
        "asset://shaders/stale-old.vert.spv",
        "/assets/shaders/stale-old.vert.spv",
        make_spirv_fixture_bytes()));
    bundle.images.ready.push_back(make_payload_bundle_entry(
        "duplicate_a",
        asset_type::image,
        "asset://cards/shared.png",
        "/assets/cards/shared-a.png",
        "shared image a",
        ""));
    bundle.images.ready.push_back(make_payload_bundle_entry(
        "duplicate_b",
        asset_type::image,
        "asset://cards/shared.png",
        "/assets/cards/shared-b.png",
        "shared image b",
        ""));

    const asset_render_resource_payload_bridge_summary bridge =
        bridge_asset_render_resource_addresses_to_payloads(addresses, bundle);

    require(!bridge.ok(), "render resource payload bridge reports blocked handoff evidence");
    require(bridge.requested_address_count == 8U, "render resource payload bridge records requested addresses");
    require(bridge.entry_count() == 8U, "render resource payload bridge emits one result per address");
    require(bridge.accepted_count == 4U, "render resource payload bridge counts accepted matches");
    require(bridge.blocked_count() == 4U, "render resource payload bridge counts blocked matches");
    require(
        bridge.missing_materialized_bytes_count == 1U,
        "render resource payload bridge counts missing materialized bytes");
    require(bridge.type_mismatch_count == 1U, "render resource payload bridge counts type mismatches");
    require(bridge.cache_key_mismatch_count == 1U, "render resource payload bridge counts cache-key mismatches");
    require(
        bridge.duplicate_canonical_identity_count == 1U,
        "render resource payload bridge counts duplicate canonical identities");
    require(
        bridge.duplicate_materialized_payload_id_count == 0U,
        "render resource payload bridge does not report duplicate payload ids in this fixture");

    const asset_render_resource_payload_bridge_entry* image = bridge.find_accepted("card_front");
    require(image != nullptr, "render resource payload bridge finds accepted image payload");
    require(image->ok(), "accepted image bridge entry is ok");
    require(image->selection.selected(), "accepted image bridge entry keeps selection result");
    require(image->selected_snapshot.has_value(), "accepted image bridge entry keeps compact payload snapshot");
    require(image->selection.payload != nullptr, "accepted image bridge entry keeps payload pointer");
    require(
        image->selection.payload->bytes.size() == image->selected_snapshot->payload_byte_count,
        "accepted image bridge entry points to bytes without copying them into the snapshot");
    require(
        image->address.canonical_identity == "asset://cards/front.png",
        "accepted image bridge entry keeps canonical render identity");
    require(
        image->selected_snapshot->cache_key == "image|asset://cards/front.png",
        "accepted image bridge entry keeps payload cache key");
    require(
        image->selected_snapshot->content_hash
            == make_asset_bytes_content_hash(detail::make_asset_byte_vector("image bytes")),
        "accepted image bridge entry keeps content hash evidence");

    const asset_render_resource_payload_bridge_entry* font = bridge.find_accepted("body_font");
    require(font != nullptr, "render resource payload bridge finds accepted font payload");
    require(font->selected_snapshot->type == asset_type::font, "accepted font bridge entry keeps font type");
    require(
        font->address.canonical_identity == "local-fixture://packaged/fonts/body.ttf",
        "accepted font bridge entry keeps stable fixture identity");

    const asset_render_resource_payload_bridge_entry* shader = bridge.find_accepted("ui_shader");
    require(shader != nullptr, "render resource payload bridge finds accepted shader payload");
    require(shader->selected_snapshot->type == asset_type::shader, "accepted shader bridge entry keeps shader type");
    require(
        shader->address.canonical_identity
            == "build-external://external_shader_pack/shaders/ui.vert.spv",
        "accepted shader bridge entry keeps stable build-external identity");

    const asset_render_resource_payload_bridge_entry* missing = bridge.find_blocked("missing_image");
    require(missing != nullptr, "render resource payload bridge finds missing materialized bytes");
    require(
        missing->status == asset_render_resource_payload_bridge_status::missing_materialized_bytes,
        "missing image bridge entry reports missing materialized bytes");
    require(!missing->selected_snapshot.has_value(), "missing image bridge entry has no selected snapshot");

    const asset_render_resource_payload_bridge_entry* wrong_type = bridge.find_blocked("wrong_type");
    require(wrong_type != nullptr, "render resource payload bridge finds type mismatches");
    require(
        wrong_type->status == asset_render_resource_payload_bridge_status::type_mismatch,
        "wrong type bridge entry reports type mismatch");
    require(wrong_type->selection.actual_type == asset_type::sound, "wrong type bridge entry records actual type");
    require(wrong_type->selection.expected_type == asset_type::image, "wrong type bridge entry records expected type");

    const asset_render_resource_payload_bridge_entry* stale = bridge.find_blocked("stale_shader");
    require(stale != nullptr, "render resource payload bridge finds cache-key mismatches");
    require(
        stale->status == asset_render_resource_payload_bridge_status::cache_key_mismatch,
        "stale shader bridge entry reports cache-key mismatch");
    require(
        stale->selection.actual_cache_key == "shader|asset://shaders/stale-old.vert.spv",
        "stale shader bridge entry records actual payload cache key");
    require(
        stale->selection.expected_cache_key == asset_cache_key{"shader|asset://shaders/stale.vert.spv"},
        "stale shader bridge entry records expected address cache key");

    const asset_render_resource_payload_bridge_entry* duplicate_a = bridge.find_accepted("duplicate_a");
    require(duplicate_a != nullptr, "render resource payload bridge accepts first canonical identity owner");
    require(
        bridge.find_canonical_identity("asset://cards/shared.png") == duplicate_a,
        "render resource payload bridge finds first canonical identity owner deterministically");

    const asset_render_resource_payload_bridge_entry* duplicate_b = bridge.find_blocked("duplicate_b");
    require(duplicate_b != nullptr, "render resource payload bridge finds duplicate canonical identities");
    require(
        duplicate_b->status == asset_render_resource_payload_bridge_status::duplicate_canonical_identity,
        "duplicate canonical bridge entry reports duplicate identity");
    require(duplicate_b->related_id == "duplicate_a", "duplicate canonical bridge entry records first owner");
    require(!duplicate_b->selected_snapshot.has_value(), "duplicate canonical bridge entry does not select payload");
    require(
        asset_render_resource_payload_bridge_status_name(duplicate_b->status)
            == "duplicate_canonical_identity",
        "render resource payload bridge status names are stable");
}

void test_render_resource_manifest_to_payload_bridge_e2e_uses_materialized_bytes()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "packaged" / "cards" / "front.png", "image bytes");
    write_fixture_file(fixture_root / "packaged" / "fonts" / "body.ttf", "font bytes");
    write_fixture_file(fixture_root / "build" / "external" / "shader_pack" / "shaders" / "ui.vert.spv", "shader bytes");

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
        .id = "card_front",
        .type = asset_type::image,
        .uri = "ASSET:///cards/./front.png",
        .root_id = "packaged",
        .cache_revision = "image-rev1",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "asset://fonts/body.ttf",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.vert.spv",
        .root_id = "external_shader_pack",
        .cache_revision = "shader-rev2",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "bad_traversal",
        .type = asset_type::image,
        .uri = "asset://cards/%2e%2e/secret.png",
        .root_id = "packaged",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const local_file_asset_bytes_provider provider;
    const asset_typed_materialized_bytes_summary typed =
        summarize_typed_materialized_asset_bytes(provider, catalog);
    const asset_materialized_byte_payload_bundle bundle =
        make_materialized_asset_byte_payload_bundle(provider, catalog);
    const asset_render_resource_address_summary addresses =
        summarize_asset_render_resource_addresses(manifest, resolver);
    const asset_render_resource_payload_bridge_summary bridge =
        bridge_asset_render_resource_addresses_to_payloads(addresses, bundle);

    require(!catalog.ok(), "runtime catalog preserves traversal diagnostics alongside valid entries");
    require(catalog.assets.size() == 3U, "runtime catalog keeps only valid materializable render resources");
    require(catalog.find("bad_traversal") == nullptr, "runtime catalog omits traversal entries");
    require(catalog.find_diagnostic("bad_traversal") != nullptr, "runtime catalog records traversal diagnostic");

    require(typed.ok(), "typed materialized bytes summary accepts valid manifest entries");
    require(typed.entry_count() == 3U, "typed materialized bytes summary groups image font and shader entries");
    require(typed.cache_policy.loaded_count == 3U, "typed materialized bytes summary loads all valid entries");
    require(typed.cache_policy.failed_count == 0U, "typed materialized bytes summary has no valid-entry failures");
    require(typed.images.size() == 1U, "typed materialized bytes summary keeps image group");
    require(typed.fonts.size() == 1U, "typed materialized bytes summary keeps font group");
    require(typed.shaders.size() == 1U, "typed materialized bytes summary keeps shader group");

    require(bundle.ok(), "materialized byte payload bundle is ready for valid entries");
    require(bundle.ready_count() == 3U, "payload bundle keeps ready image font and shader bytes");
    require(bundle.blocked_count() == 0U, "payload bundle has no blocked valid entries");
    require(bundle.images.find_ready("card_front") != nullptr, "payload bundle exposes ready image bytes");
    require(bundle.fonts.find_ready("body_font") != nullptr, "payload bundle exposes ready font bytes");
    require(bundle.shaders.find_ready("ui_shader") != nullptr, "payload bundle exposes ready shader bytes");

    require(!addresses.ok(), "render resource address summary preserves traversal diagnostics");
    require(addresses.entry_count() == 3U, "render resource address summary emits valid addresses only");
    require(addresses.path_traversal_rejection_count == 1U, "render addresses count traversal rejection");
    require(addresses.find_entry("bad_traversal") == nullptr, "render addresses omit traversal entries");

    require(bridge.ok(), "render resource payload bridge accepts valid materialized manifest entries");
    require(bridge.requested_address_count == 3U, "bridge considers every valid render resource address");
    require(bridge.accepted_count == 3U, "bridge accepts image font and shader payloads");
    require(bridge.blocked_count() == 0U, "bridge has no blocked valid payloads");

    const asset_render_resource_payload_bridge_entry* image = bridge.find_accepted("card_front");
    require(image != nullptr, "bridge finds accepted image from manifest address");
    require(image->selected_snapshot.has_value(), "accepted image keeps compact selected payload snapshot");
    require(image->selection.payload != nullptr, "accepted image keeps pointer to materialized payload bytes");
    require(bytes_to_string(image->selection.payload->bytes) == "image bytes", "image bytes come from materialized payload");
    require(image->address.canonical_identity == "asset://cards/front.png", "image keeps canonical asset uri identity");
    require(
        image->selected_snapshot->cache_key == "image|asset://cards/front.png|rev=image-rev1",
        "image bridge keeps stable revised cache key");
    require(
        image->selected_snapshot->cache_key.find(fixture_root.string()) == std::string::npos,
        "image cache key does not leak absolute fixture paths");
    require(
        image->selected_snapshot->materialized_path
            == std::filesystem::absolute(fixture_root / "packaged" / "cards" / "front.png").lexically_normal(),
        "image bridge keeps materialized path separately from cache identity");
    require(
        image->selected_snapshot->content_hash
            == make_asset_bytes_content_hash(detail::make_asset_byte_vector("image bytes")),
        "image bridge keeps loaded byte content hash");

    const asset_render_resource_payload_bridge_entry* font = bridge.find_accepted("body_font");
    require(font != nullptr, "bridge finds accepted font from manifest address");
    require(bytes_to_string(font->selection.payload->bytes) == "font bytes", "font bytes come from materialized payload");
    require(font->address.canonical_identity == "asset://fonts/body.ttf", "font keeps canonical asset uri identity");
    require(font->selected_snapshot->cache_key == "font|asset://fonts/body.ttf", "font bridge keeps cache key");
    require(
        font->selected_snapshot->cache_key.find(fixture_root.string()) == std::string::npos,
        "font cache key does not leak absolute fixture paths");

    const asset_render_resource_payload_bridge_entry* shader = bridge.find_accepted("ui_shader");
    require(shader != nullptr, "bridge finds accepted shader from manifest address");
    require(
        bytes_to_string(shader->selection.payload->bytes) == "shader bytes",
        "shader bytes come from materialized payload");
    require(
        shader->address.canonical_identity == "asset://shaders/ui.vert.spv",
        "shader keeps canonical asset uri identity");
    require(
        shader->selected_snapshot->cache_key == "shader|asset://shaders/ui.vert.spv|rev=shader-rev2",
        "shader bridge keeps stable revised cache key");
    require(
        shader->selected_snapshot->cache_key.find(fixture_root.string()) == std::string::npos,
        "shader cache key does not leak build-external fixture paths");
    require(
        shader->selected_snapshot->materialized_path
            == std::filesystem::absolute(
                   fixture_root / "build" / "external" / "shader_pack" / "shaders" / "ui.vert.spv")
                   .lexically_normal(),
        "shader bridge keeps build-external materialized path as payload evidence");

    const runtime_materialized_asset_lookup_result missing_lookup =
        lookup_runtime_materialized_asset(catalog, runtime_materialized_asset_lookup_request{
            .id = "missing_card",
            .expected_type = asset_type::image,
        });
    require(
        missing_lookup.status == runtime_materialized_asset_lookup_status::missing_id,
        "runtime materialized lookup reports missing manifest ids");
    require(addresses.find_entry("missing_card") == nullptr, "missing manifest id has no render address");
    require(bridge.find_entry("missing_card") == nullptr, "missing manifest id has no bridge entry");

    const asset_materialized_byte_payload_selection_result missing_payload =
        select_materialized_asset_byte_payload(bundle, asset_materialized_byte_payload_selection_request{
            .id = "missing_card",
            .expected_type = asset_type::image,
        });
    require(
        missing_payload.status == asset_materialized_byte_payload_selection_status::missing_id,
        "payload selection reports missing manifest-derived payload ids");

    const runtime_materialized_asset_lookup_result wrong_lookup =
        lookup_runtime_materialized_asset(catalog, runtime_materialized_asset_lookup_request{
            .id = "card_front",
            .expected_type = asset_type::font,
        });
    require(
        wrong_lookup.status == runtime_materialized_asset_lookup_status::type_mismatch,
        "runtime materialized lookup reports mismatched typed resource kinds");

    const asset_materialized_byte_payload_selection_result wrong_selection =
        select_materialized_asset_byte_payload(bundle, asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::font,
        });
    require(
        wrong_selection.status == asset_materialized_byte_payload_selection_status::wrong_type,
        "payload selection reports mismatched typed resource kinds");
    require(wrong_selection.actual_type == asset_type::image, "wrong-kind payload selection keeps actual type evidence");

    asset_materialized_byte_payload_bundle mismatched_bundle = bundle;
    asset_materialized_byte_payload mismatched_payload = mismatched_bundle.images.ready.front();
    mismatched_bundle.images.ready.clear();
    mismatched_payload.type = asset_type::font;
    mismatched_bundle.fonts.ready.push_back(std::move(mismatched_payload));

    const asset_render_resource_payload_bridge_summary mismatched_bridge =
        bridge_asset_render_resource_addresses_to_payloads(addresses, mismatched_bundle);
    const asset_render_resource_payload_bridge_entry* wrong_bridge =
        mismatched_bridge.find_blocked("card_front");
    require(wrong_bridge != nullptr, "bridge reports mismatched typed resource kinds");
    require(
        wrong_bridge->status == asset_render_resource_payload_bridge_status::type_mismatch,
        "bridge maps mismatched typed payloads to type mismatch status");
    require(wrong_bridge->selection.actual_type == asset_type::font, "bridge keeps mismatched actual payload type");
    require(wrong_bridge->selection.expected_type == asset_type::image, "bridge keeps expected render address type");
}

void test_render_resource_materialized_cache_summary_reuses_manifest_payload_identity()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    write_fixture_file(fixture_root / "packaged" / "cards" / "front.png", "image bytes");
    write_fixture_file(fixture_root / "packaged" / "fonts" / "body.ttf", "font bytes");
    write_fixture_file(fixture_root / "build" / "external" / "shader_pack" / "shaders" / "ui.vert.spv", "shader bytes");

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
        .id = "card_front",
        .type = asset_type::image,
        .uri = "ASSET:///cards/./front.png",
        .root_id = "packaged",
        .cache_revision = "image-rev1",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "body_font",
        .type = asset_type::font,
        .uri = "asset://fonts/body.ttf",
        .root_id = "packaged",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "ui_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.vert.spv",
        .root_id = "external_shader_pack",
        .cache_revision = "shader-rev2",
    });
    manifest.entries.push_back(asset_manifest_entry{
        .id = "bad_traversal",
        .type = asset_type::image,
        .uri = "asset://cards/%2e%2e/secret.png",
        .root_id = "packaged",
    });

    const normalizing_asset_resolver resolver;
    const local_file_asset_bytes_provider provider;
    const std::vector<asset_render_resource_materialized_cache_request> requests{
        asset_render_resource_materialized_cache_request{
            .id = "card_front",
            .expected_type = asset_type::image,
        },
        asset_render_resource_materialized_cache_request{
            .id = "body_font",
            .expected_type = asset_type::font,
        },
        asset_render_resource_materialized_cache_request{
            .id = "ui_shader",
            .expected_type = asset_type::shader,
        },
        asset_render_resource_materialized_cache_request{
            .id = "bad_traversal",
            .expected_type = asset_type::image,
        },
        asset_render_resource_materialized_cache_request{
            .id = "missing_card",
            .expected_type = asset_type::image,
        },
        asset_render_resource_materialized_cache_request{
            .id = "body_font",
            .expected_type = asset_type::image,
        },
    };

    const runtime_asset_catalog first_catalog = build_runtime_asset_catalog(manifest, resolver);
    const asset_render_resource_address_summary first_addresses =
        summarize_asset_render_resource_addresses(manifest, resolver);
    const asset_materialized_byte_payload_bundle first_bundle =
        make_materialized_asset_byte_payload_bundle(provider, first_catalog);
    const asset_render_resource_materialized_cache_summary first_cache =
        make_asset_render_resource_materialized_cache_summary(first_addresses, first_bundle, requests);

    const runtime_asset_catalog second_catalog = build_runtime_asset_catalog(manifest, resolver);
    const asset_render_resource_address_summary second_addresses =
        summarize_asset_render_resource_addresses(manifest, resolver);
    const asset_materialized_byte_payload_bundle second_bundle =
        make_materialized_asset_byte_payload_bundle(provider, second_catalog);
    const asset_render_resource_materialized_cache_summary second_cache =
        make_asset_render_resource_materialized_cache_summary(second_addresses, second_bundle, requests);

    require(!first_cache.ok(), "render resource materialized cache reports blocked request evidence");
    require(first_cache.requested_count == 6U, "render resource materialized cache preserves request count");
    require(first_cache.entry_count() == 6U, "render resource materialized cache emits one result per request");
    require(first_cache.ready_count == 3U, "render resource materialized cache counts reusable payloads");
    require(first_cache.blocked_count() == 3U, "render resource materialized cache counts blocked entries");
    require(first_cache.address_rejected_count == 1U, "render resource materialized cache counts rejected addresses");
    require(
        first_cache.missing_render_resource_address_count == 1U,
        "render resource materialized cache counts missing requested ids");
    require(first_cache.type_mismatch_count == 1U, "render resource materialized cache counts request type mismatches");

    const asset_render_resource_materialized_cache_entry* first_image = first_cache.find_ready("card_front");
    const asset_render_resource_materialized_cache_entry* second_image = second_cache.find_ready("card_front");
    require(first_image != nullptr && second_image != nullptr, "render cache finds repeated image entries");
    require(first_image->ready() && second_image->ready(), "repeated image cache entries are reusable");
    require(first_image->runtime_cache_key == second_image->runtime_cache_key, "image runtime cache key is stable");
    require(first_image->cache_key == second_image->cache_key, "image manifest cache key is stable");
    require(first_image->content_hash == second_image->content_hash, "image content hash evidence is stable");
    require(first_image->byte_count == second_image->byte_count, "image byte count evidence is stable");
    require(
        first_image->payload_byte_count == second_image->payload_byte_count,
        "image payload byte count evidence is stable");
    require(
        first_image->materialized_path == second_image->materialized_path,
        "image materialized path evidence is stable");
    require(
        first_cache.find_runtime_cache_key(first_image->runtime_cache_key) == first_image,
        "render cache can look up entries by runtime cache key");
    require(
        first_image->runtime_cache_key
            == "render-resource|image|asset://cards/front.png|image|asset://cards/front.png|rev=image-rev1|hash="
                + make_asset_bytes_content_hash(detail::make_asset_byte_vector("image bytes")),
        "image runtime cache key includes resource identity cache key and content hash");
    require(
        first_image->runtime_cache_key.find(fixture_root.string()) == std::string::npos,
        "image runtime cache key does not leak absolute fixture paths");

    const asset_render_resource_materialized_cache_entry* first_font = first_cache.find_ready("body_font");
    const asset_render_resource_materialized_cache_entry* second_font = second_cache.find_ready("body_font");
    require(first_font != nullptr && second_font != nullptr, "render cache finds repeated font entries");
    require(first_font->runtime_cache_key == second_font->runtime_cache_key, "font runtime cache key is stable");
    require(first_font->canonical_identity == "asset://fonts/body.ttf", "font runtime cache preserves identity");
    require(
        first_font->runtime_cache_key.find(fixture_root.string()) == std::string::npos,
        "font runtime cache key does not leak absolute fixture paths");

    const asset_render_resource_materialized_cache_entry* first_shader = first_cache.find_ready("ui_shader");
    const asset_render_resource_materialized_cache_entry* second_shader = second_cache.find_ready("ui_shader");
    require(first_shader != nullptr && second_shader != nullptr, "render cache finds repeated shader entries");
    require(first_shader->runtime_cache_key == second_shader->runtime_cache_key, "shader runtime cache key is stable");
    require(
        first_shader->runtime_cache_key
            == "render-resource|shader|asset://shaders/ui.vert.spv|shader|asset://shaders/ui.vert.spv|rev=shader-rev2|hash="
                + make_asset_bytes_content_hash(detail::make_asset_byte_vector("shader bytes")),
        "shader runtime cache key includes revised shader identity and content hash");
    require(
        first_shader->runtime_cache_key.find(fixture_root.string()) == std::string::npos,
        "shader runtime cache key does not leak build-external fixture paths");
    require(
        first_shader->materialized_path
            == std::filesystem::absolute(
                   fixture_root / "build" / "external" / "shader_pack" / "shaders" / "ui.vert.spv")
                   .lexically_normal(),
        "shader runtime cache keeps build-external path as evidence outside the cache key");

    const asset_render_resource_materialized_cache_entry* traversal = first_cache.find_blocked("bad_traversal");
    require(traversal != nullptr, "render cache keeps traversal rejection evidence");
    require(
        traversal->status == asset_render_resource_materialized_cache_status::address_rejected,
        "render cache maps traversal to address rejection");
    require(
        traversal->diagnostic == "asset path traversal is not allowed",
        "render cache preserves traversal diagnostic");

    const asset_render_resource_materialized_cache_entry* missing = first_cache.find_blocked("missing_card");
    require(missing != nullptr, "render cache keeps missing requested id evidence");
    require(
        missing->status == asset_render_resource_materialized_cache_status::missing_render_resource_address,
        "render cache reports missing render resource addresses explicitly");
    require(
        asset_render_resource_materialized_cache_status_name(missing->status)
            == "missing_render_resource_address",
        "render cache missing-id status name is stable");

    const asset_render_resource_materialized_cache_entry* wrong_type = first_cache.find_blocked("body_font");
    require(wrong_type != nullptr, "render cache keeps request type mismatch evidence");
    require(
        wrong_type->status == asset_render_resource_materialized_cache_status::type_mismatch,
        "render cache reports request type mismatches explicitly");
    require(wrong_type->type == asset_type::font, "render cache type mismatch keeps actual manifest type");
    require(wrong_type->cache_key == "font|asset://fonts/body.ttf", "render cache type mismatch keeps cache key");
}

void test_render_resource_materialized_cache_diff_tracks_reuse_and_invalidation()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const std::filesystem::path before_root = fixture_root / "before";
    const std::filesystem::path after_root = fixture_root / "after";
    write_fixture_file(before_root / "packaged" / "cards" / "front.png", "image bytes v1");
    write_fixture_file(before_root / "packaged" / "fonts" / "body.ttf", "font bytes");
    write_fixture_file(before_root / "build" / "external" / "shader_pack" / "shaders" / "ui.vert.spv", "shader bytes");
    write_fixture_file(after_root / "packaged" / "cards" / "front.png", "image bytes v2");
    write_fixture_file(after_root / "packaged" / "fonts" / "body.ttf", "font bytes");
    write_fixture_file(after_root / "build" / "external" / "shader_pack" / "shaders" / "ui.vert.spv", "shader bytes");

    const auto make_cache = [](const std::filesystem::path& root, std::string shader_revision) {
        asset_manifest manifest;
        manifest.roots.push_back(asset_manifest_root{
            .id = "packaged",
            .root_path = root / "packaged",
        });
        manifest.roots.push_back(asset_manifest_root{
            .id = "external_shader_pack",
            .root_path = root / "build" / "external" / "shader_pack",
        });
        manifest.entries.push_back(asset_manifest_entry{
            .id = "card_front",
            .type = asset_type::image,
            .uri = "asset://cards/front.png",
            .root_id = "packaged",
            .cache_revision = "image-rev1",
        });
        manifest.entries.push_back(asset_manifest_entry{
            .id = "body_font",
            .type = asset_type::font,
            .uri = "asset://fonts/body.ttf",
            .root_id = "packaged",
        });
        manifest.entries.push_back(asset_manifest_entry{
            .id = "ui_shader",
            .type = asset_type::shader,
            .uri = "asset://shaders/ui.vert.spv",
            .root_id = "external_shader_pack",
            .cache_revision = std::move(shader_revision),
        });

        const normalizing_asset_resolver resolver;
        const local_file_asset_bytes_provider provider;
        const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
        const asset_render_resource_address_summary addresses =
            summarize_asset_render_resource_addresses(manifest, resolver);
        const asset_materialized_byte_payload_bundle bundle =
            make_materialized_asset_byte_payload_bundle(provider, catalog);
        return make_asset_render_resource_materialized_cache_summary(addresses, bundle);
    };

    const asset_render_resource_materialized_cache_summary before = make_cache(before_root, "shader-rev1");
    const asset_render_resource_materialized_cache_summary after = make_cache(after_root, "shader-rev2");
    const asset_render_resource_materialized_cache_diff_summary diff =
        diff_asset_render_resource_materialized_cache_summaries(before, after);

    require(before.ok(), "before render cache starts with reusable materialized entries");
    require(after.ok(), "after render cache starts with reusable materialized entries");
    require(!diff.empty(), "render cache diff reports materialized cache invalidation evidence");
    require(diff.change_count() == 2U, "render cache diff counts content and revision replacements");
    require(diff.invalidation_count() == 2U, "render cache diff counts replaced entries as invalidations");
    require(diff.reused.size() == 1U, "render cache diff records stable reusable entries");
    require(diff.replaced.size() == 2U, "render cache diff records replaced entries");
    require(diff.added.empty(), "render cache diff has no added entries");
    require(diff.removed.empty(), "render cache diff has no removed entries");
    require(diff.invalidated.empty(), "render cache diff has no blocked invalidations");
    require(diff.requested_delta == 0, "render cache diff keeps request count delta stable");
    require(diff.ready_delta == 0, "render cache diff keeps ready count delta stable");
    require(diff.blocked_delta == 0, "render cache diff keeps blocked count delta stable");

    const asset_render_resource_materialized_cache_diff_entry* font = diff.find_reused("body_font");
    require(font != nullptr, "render cache diff finds reused font entry");
    require(
        font->kind == asset_render_resource_materialized_cache_delta_kind::reused,
        "render cache diff marks unchanged logical identity as reused");
    require(!font->invalidates(), "reused render cache entries do not invalidate");
    require(font->before.has_value() && font->after.has_value(), "reused entry keeps before and after evidence");
    require(
        font->before->runtime_cache_key == font->after->runtime_cache_key,
        "unchanged content reuses the same runtime cache key");
    require(
        font->before->materialized_path != font->after->materialized_path,
        "reused entry can move between host materialized paths");
    require(font->materialized_path_changed, "reused entry records materialized path movement");
    require(!font->runtime_cache_key_changed, "host path movement does not alter renderer-facing cache key");
    require(!font->cache_key_changed, "host path movement does not alter manifest cache key");
    require(!font->content_hash_changed, "unchanged content keeps hash evidence stable");
    require(
        font->before->runtime_cache_key.find(fixture_root.string()) == std::string::npos
            && font->after->runtime_cache_key.find(fixture_root.string()) == std::string::npos,
        "reused runtime cache keys do not leak host paths");

    const asset_render_resource_materialized_cache_diff_entry* image = diff.find_replaced("card_front");
    require(image != nullptr, "render cache diff finds content-hash replacements");
    require(image->invalidates(), "content changes invalidate the previous render cache entry");
    require(image->before.has_value() && image->after.has_value(), "content replacement keeps before and after evidence");
    require(!image->logical_identity_changed, "content replacement keeps logical asset identity");
    require(!image->cache_key_changed, "content replacement keeps revision cache key stable");
    require(image->content_hash_changed, "content replacement records hash change");
    require(image->runtime_cache_key_changed, "content replacement creates a new runtime cache key");
    require(image->materialized_path_changed, "content replacement records materialized path movement");
    require(
        image->invalidated_runtime_cache_key == image->before->runtime_cache_key,
        "content replacement records invalidated cache key");
    require(
        image->replacement_runtime_cache_key == image->after->runtime_cache_key,
        "content replacement records replacement cache key");
    require(
        image->before->runtime_cache_key != image->after->runtime_cache_key,
        "changed content creates a distinct renderer-facing cache key");
    require(
        image->after->runtime_cache_key.find(fixture_root.string()) == std::string::npos,
        "content replacement runtime cache key does not leak host paths");
    require(
        image->diagnostic == "render resource materialized cache entry replaced by revision or content change",
        "content replacement diagnostic is stable");

    const asset_render_resource_materialized_cache_diff_entry* shader = diff.find_replaced("ui_shader");
    require(shader != nullptr, "render cache diff finds revision replacements");
    require(shader->invalidates(), "revision changes invalidate the previous render cache entry");
    require(shader->before.has_value() && shader->after.has_value(), "revision replacement keeps before and after evidence");
    require(!shader->logical_identity_changed, "revision replacement keeps logical asset identity");
    require(shader->cache_key_changed, "revision replacement records manifest cache key change");
    require(!shader->content_hash_changed, "revision replacement can keep identical content hash");
    require(shader->runtime_cache_key_changed, "revision replacement creates a new runtime cache key");
    require(
        shader->before->cache_key == "shader|asset://shaders/ui.vert.spv|rev=shader-rev1",
        "revision replacement keeps old revision cache key");
    require(
        shader->after->cache_key == "shader|asset://shaders/ui.vert.spv|rev=shader-rev2",
        "revision replacement keeps new revision cache key");
    require(
        shader->before->content_hash == shader->after->content_hash,
        "revision replacement keeps unchanged byte hash evidence");
    require(
        shader->after->runtime_cache_key.find(fixture_root.string()) == std::string::npos,
        "revision replacement runtime cache key does not leak build host paths");
    require(
        asset_render_resource_materialized_cache_delta_kind_name(shader->kind) == "replaced",
        "render cache diff delta names are stable");
}

void test_materialized_asset_byte_payload_selection_filters_payloads_and_reports_diagnostics()
{
    using namespace quiz_vulkan::assets;

    asset_materialized_byte_payload_bundle bundle;
    bundle.images.ready.push_back(make_payload_bundle_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "image bytes",
        ""));
    bundle.shaders.blocked.push_back(make_payload_bundle_entry(
        "rootless_shader",
        asset_type::shader,
        "asset://shaders/rootless.vert.spv",
        "/assets/shaders/rootless.vert.spv",
        "",
        "hash:rootless",
        asset_materialized_bytes_handoff_status::materialization_blocked));
    bundle.decks.blocked.push_back(make_payload_bundle_entry(
        "main_deck",
        asset_type::deck,
        "asset://decks/main.quiz",
        "/assets/decks/main.quiz",
        "deck bytes",
        "hash:deck-bad",
        asset_materialized_bytes_handoff_status::integrity_blocked));
    bundle.fonts.ready.push_back(make_payload_bundle_entry(
        "duplicate_payload",
        asset_type::font,
        "asset://fonts/body.ttf",
        "/assets/fonts/body.ttf",
        "font bytes",
        ""));
    bundle.sounds.ready.push_back(make_payload_bundle_entry(
        "duplicate_payload",
        asset_type::sound,
        "asset://sounds/correct.ogg",
        "/assets/sounds/correct.ogg",
        "sound bytes",
        ""));

    const asset_cache_key image_key = "image|asset://cards/front.png";
    const asset_materialized_byte_payload_selection_result selected = select_materialized_asset_byte_payload(
        bundle,
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::image,
            .expected_cache_key = image_key,
        });

    require(selected.selected(), "payload selection accepts matching ready payloads");
    require(
        selected.status == asset_materialized_byte_payload_selection_status::selected,
        "payload selection reports selected status");
    require(selected.payload != nullptr, "payload selection returns a selected payload pointer");
    require(selected.payload->id == "card_front", "payload selection returns the requested payload");
    require(selected.snapshot.has_value(), "payload selection includes compact snapshot evidence");
    require(selected.snapshot->content_hash == selected.payload->content_hash, "payload selection snapshot keeps hash");
    require(selected.snapshot->payload_byte_count == selected.payload->bytes.size(), "payload selection keeps byte count");
    require(selected.actual_type == asset_type::image, "payload selection records actual type");
    require(selected.actual_cache_key == image_key, "payload selection records actual cache key");
    require(selected.match_count == 1U, "payload selection records match count");
    require(selected.diagnostic == "materialized byte payload selected", "payload selection has stable success diagnostic");
    require(
        asset_materialized_byte_payload_selection_status_name(selected.status) == "selected",
        "payload selection status names are stable");
    require(
        materialized_asset_byte_payload_integrity_ok(*selected.payload),
        "payload selection exposes integrity status helper");

    const asset_materialized_byte_payload_selection_result missing = select_materialized_asset_byte_payload(
        bundle,
        asset_materialized_byte_payload_selection_request{.id = "missing_image", .expected_type = asset_type::image});
    require(!missing.selected(), "payload selection rejects missing ids");
    require(
        missing.status == asset_materialized_byte_payload_selection_status::missing_id,
        "payload selection reports missing id status");
    require(missing.diagnostic == "materialized byte payload id was not found", "missing id diagnostic is stable");

    const asset_materialized_byte_payload_selection_result wrong_type = select_materialized_asset_byte_payload(
        bundle,
        asset_materialized_byte_payload_selection_request{.id = "card_front", .expected_type = asset_type::sound});
    require(!wrong_type.selected(), "payload selection rejects wrong types");
    require(
        wrong_type.status == asset_materialized_byte_payload_selection_status::wrong_type,
        "payload selection reports wrong type status");
    require(wrong_type.snapshot.has_value(), "wrong type selection keeps actual snapshot evidence");
    require(wrong_type.actual_type == asset_type::image, "wrong type selection records actual type");
    require(
        wrong_type.diagnostic == "materialized byte payload type does not match the request",
        "wrong type diagnostic is stable");

    const asset_materialized_byte_payload_selection_result cache_mismatch = select_materialized_asset_byte_payload(
        bundle,
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::image,
            .expected_cache_key = asset_cache_key{"image|asset://cards/back.png"},
        });
    require(!cache_mismatch.selected(), "payload selection rejects cache key mismatches");
    require(
        cache_mismatch.status == asset_materialized_byte_payload_selection_status::cache_key_mismatch,
        "payload selection reports cache key mismatch status");
    require(cache_mismatch.actual_cache_key == image_key, "cache mismatch records actual key");
    require(
        cache_mismatch.diagnostic == "materialized byte payload cache key does not match the request",
        "cache key mismatch diagnostic is stable");

    const asset_materialized_byte_payload_selection_result blocked = select_materialized_asset_byte_payload(
        bundle,
        asset_materialized_byte_payload_selection_request{
            .id = "rootless_shader",
            .expected_type = asset_type::shader,
        });
    require(!blocked.selected(), "payload selection rejects blocked payloads by default");
    require(
        blocked.status == asset_materialized_byte_payload_selection_status::blocked_payload,
        "payload selection reports blocked payload status");
    require(blocked.snapshot.has_value() && !blocked.snapshot->ready, "blocked selection keeps readiness evidence");
    require(blocked.diagnostic == "materialized byte payload is blocked", "blocked payload diagnostic is stable");

    const asset_materialized_byte_payload_selection_result blocked_allowed = select_materialized_asset_byte_payload(
        bundle,
        asset_materialized_byte_payload_selection_request{
            .id = "rootless_shader",
            .expected_type = asset_type::shader,
            .require_ready = false,
            .require_integrity_ok = false,
        });
    require(blocked_allowed.selected(), "payload selection can explicitly allow blocked payloads");
    require(blocked_allowed.payload->status == asset_materialized_bytes_handoff_status::materialization_blocked,
        "blocked-allowed selection preserves blocker status");

    const asset_materialized_byte_payload_selection_result integrity_failure = select_materialized_asset_byte_payload(
        bundle,
        asset_materialized_byte_payload_selection_request{
            .id = "main_deck",
            .expected_type = asset_type::deck,
        });
    require(!integrity_failure.selected(), "payload selection rejects integrity failures by default");
    require(
        integrity_failure.status == asset_materialized_byte_payload_selection_status::integrity_failure,
        "payload selection reports integrity failure status");
    require(
        !materialized_asset_byte_payload_integrity_ok(bundle.decks.blocked[0]),
        "payload integrity helper identifies failed integrity");
    require(
        integrity_failure.diagnostic == "materialized byte payload has integrity issues",
        "integrity failure diagnostic is stable");

    const asset_materialized_byte_payload_selection_result duplicate = select_materialized_asset_byte_payload(
        bundle,
        asset_materialized_byte_payload_selection_request{.id = "duplicate_payload"});
    require(!duplicate.selected(), "payload selection rejects duplicate ids");
    require(
        duplicate.status == asset_materialized_byte_payload_selection_status::duplicate_id,
        "payload selection reports duplicate id status");
    require(duplicate.match_count == 2U, "payload selection records duplicate count");
    require(
        duplicate.diagnostic == "materialized byte payload id matched multiple payloads",
        "duplicate id diagnostic is stable");

    const asset_materialized_byte_payload_filter_result ready_images = filter_materialized_asset_byte_payloads(
        bundle,
        asset_materialized_byte_payload_filter{
            .type = asset_type::image,
            .ready = true,
            .integrity_ok = true,
        });
    require(!ready_images.empty(), "payload filter returns ready matching payloads");
    require(ready_images.match_count() == 1U, "payload filter counts image matches");
    require(ready_images.first_payload() == &bundle.images.ready[0], "payload filter returns payload pointers");
    require(ready_images.snapshots.size() == 1U, "payload filter returns compact snapshots");
    require(ready_images.snapshots[0].id == "card_front", "payload filter snapshot keeps id");

    const asset_materialized_byte_payload_filter_result blocked_payloads = filter_materialized_asset_byte_payloads(
        bundle,
        asset_materialized_byte_payload_filter{.ready = false});
    require(blocked_payloads.match_count() == 2U, "payload filter selects blocked payloads");

    const asset_materialized_byte_payload_filter_result integrity_failures = filter_materialized_asset_byte_payloads(
        bundle,
        asset_materialized_byte_payload_filter{.integrity_ok = false});
    require(integrity_failures.match_count() == 1U, "payload filter selects integrity failures");
    require(integrity_failures.first_payload()->id == "main_deck", "payload filter returns failed integrity payload");

    const asset_materialized_byte_payload_filter_result cache_match = filter_materialized_asset_byte_payloads(
        bundle,
        asset_materialized_byte_payload_filter{.cache_key = image_key});
    require(cache_match.match_count() == 1U, "payload filter selects cache key matches");
    require(cache_match.first_payload()->id == "card_front", "payload filter returns cache key match");

    const asset_materialized_byte_payload_filter_result id_match = filter_materialized_asset_byte_payloads(
        bundle,
        asset_materialized_byte_payload_filter{.id = std::string{"duplicate_payload"}});
    require(id_match.match_count() == 2U, "payload filter can intentionally return duplicate ids");
}

void test_materialized_asset_byte_payload_request_transaction_preserves_order_and_counts()
{
    using namespace quiz_vulkan::assets;

    asset_materialized_byte_payload_bundle bundle;
    bundle.images.ready.push_back(make_payload_bundle_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "image bytes",
        ""));
    bundle.shaders.blocked.push_back(make_payload_bundle_entry(
        "rootless_shader",
        asset_type::shader,
        "asset://shaders/rootless.vert.spv",
        "/assets/shaders/rootless.vert.spv",
        "",
        "hash:rootless",
        asset_materialized_bytes_handoff_status::materialization_blocked));
    bundle.decks.blocked.push_back(make_payload_bundle_entry(
        "main_deck",
        asset_type::deck,
        "asset://decks/main.quiz",
        "/assets/decks/main.quiz",
        "deck bytes",
        "hash:deck-bad",
        asset_materialized_bytes_handoff_status::integrity_blocked));
    bundle.fonts.ready.push_back(make_payload_bundle_entry(
        "duplicate_payload",
        asset_type::font,
        "asset://fonts/body.ttf",
        "/assets/fonts/body.ttf",
        "font bytes",
        ""));
    bundle.sounds.ready.push_back(make_payload_bundle_entry(
        "duplicate_payload",
        asset_type::sound,
        "asset://sounds/correct.ogg",
        "/assets/sounds/correct.ogg",
        "sound bytes",
        ""));

    const std::vector<asset_materialized_byte_payload_selection_request> requests{
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::image,
            .expected_cache_key = asset_cache_key{"image|asset://cards/front.png"},
        },
        asset_materialized_byte_payload_selection_request{
            .id = "rootless_shader",
            .expected_type = asset_type::shader,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "missing_image",
            .expected_type = asset_type::image,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::sound,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::image,
            .expected_cache_key = asset_cache_key{"image|asset://cards/back.png"},
        },
        asset_materialized_byte_payload_selection_request{
            .id = "main_deck",
            .expected_type = asset_type::deck,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "duplicate_payload",
        },
    };

    const asset_materialized_byte_payload_request_transaction transaction =
        make_materialized_asset_byte_payload_request_transaction(bundle, requests);

    require(!transaction.ok(), "payload request transaction reports failed requests");
    require(transaction.request_count() == requests.size(), "payload request transaction preserves request count");
    require(transaction.items.size() == requests.size(), "payload request transaction creates one item per request");
    require(transaction.summary.request_count == requests.size(), "payload request transaction summary counts requests");
    require(transaction.summary.selected_count == 1U, "payload request transaction counts selected payloads");
    require(transaction.summary.ready_count == 1U, "payload request transaction counts ready selected payloads");
    require(transaction.summary.blocked_count == 1U, "payload request transaction counts blocked selections");
    require(transaction.summary.missing_count == 1U, "payload request transaction counts missing ids");
    require(transaction.summary.wrong_type_count == 1U, "payload request transaction counts wrong types");
    require(
        transaction.summary.cache_key_mismatch_count == 1U,
        "payload request transaction counts cache key mismatches");
    require(
        transaction.summary.integrity_failure_count == 1U,
        "payload request transaction counts integrity failures");
    require(transaction.summary.duplicate_count == 1U, "payload request transaction counts duplicate ids");
    require(transaction.summary.failed_count() == 6U, "payload request transaction totals failed requests");
    require(!transaction.summary.ok(), "payload request transaction summary reports failure");

    for (std::size_t index = 0U; index < requests.size(); ++index) {
        const asset_materialized_byte_payload_request_transaction_item* item = transaction.item_at(index);
        require(item != nullptr, "payload request transaction can access items by index");
        require(item->request_index == index, "payload request transaction records input order");
        require(item->request.id == requests[index].id, "payload request transaction preserves request id order");
    }
    require(transaction.item_at(requests.size()) == nullptr, "payload request transaction rejects out-of-range access");

    const asset_materialized_byte_payload_request_transaction_item& selected = transaction.items[0];
    require(selected.selected(), "payload request transaction item exposes selected status");
    require(
        selected.selection.status == asset_materialized_byte_payload_selection_status::selected,
        "payload request transaction keeps selected result");
    require(selected.selection.payload == &bundle.images.ready[0], "payload transaction keeps payload pointer");
    require(selected.selection.snapshot.has_value(), "payload transaction keeps selection snapshot");
    require(selected.selected_snapshot.has_value(), "payload transaction exposes selected compact snapshot");
    require(selected.selected_snapshot->id == "card_front", "payload transaction selected snapshot keeps id");
    require(
        selected.selected_snapshot->payload_byte_count == bundle.images.ready[0].bytes.size(),
        "payload transaction selected snapshot keeps byte count without byte vector copy");

    const asset_materialized_byte_payload_request_transaction_item& blocked = transaction.items[1];
    require(
        blocked.selection.status == asset_materialized_byte_payload_selection_status::blocked_payload,
        "payload transaction keeps blocked result status");
    require(blocked.selection.snapshot.has_value(), "payload transaction keeps blocked snapshot evidence");
    require(!blocked.selection.snapshot->ready, "payload transaction blocked snapshot records readiness");
    require(!blocked.selected_snapshot.has_value(), "payload transaction only stores selected snapshots for successes");

    require(
        transaction.items[2].selection.status == asset_materialized_byte_payload_selection_status::missing_id,
        "payload transaction preserves missing-id result order");
    require(
        transaction.items[3].selection.status == asset_materialized_byte_payload_selection_status::wrong_type,
        "payload transaction preserves wrong-type result order");
    require(
        transaction.items[4].selection.status
            == asset_materialized_byte_payload_selection_status::cache_key_mismatch,
        "payload transaction preserves cache-key mismatch result order");
    require(
        transaction.items[5].selection.status
            == asset_materialized_byte_payload_selection_status::integrity_failure,
        "payload transaction preserves integrity failure result order");
    require(
        transaction.items[6].selection.status == asset_materialized_byte_payload_selection_status::duplicate_id,
        "payload transaction preserves duplicate-id result order");

    const asset_materialized_byte_payload_request_transaction_item* first_card =
        transaction.find_request("card_front");
    require(first_card == &transaction.items[0], "payload transaction finds the first request for an id");
    require(transaction.find_request("missing_image") == &transaction.items[2], "payload transaction finds failed requests");
    require(transaction.find_request("not_requested") == nullptr, "payload transaction reports absent requests");
}

void test_materialized_asset_byte_payload_request_transaction_review_summary_groups_by_expected_type()
{
    using namespace quiz_vulkan::assets;

    asset_materialized_byte_payload_bundle bundle;
    bundle.images.ready.push_back(make_payload_bundle_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "image bytes",
        ""));
    bundle.shaders.blocked.push_back(make_payload_bundle_entry(
        "rootless_shader",
        asset_type::shader,
        "asset://shaders/rootless.vert.spv",
        "/assets/shaders/rootless.vert.spv",
        "",
        "hash:rootless",
        asset_materialized_bytes_handoff_status::materialization_blocked));
    bundle.decks.blocked.push_back(make_payload_bundle_entry(
        "main_deck",
        asset_type::deck,
        "asset://decks/main.quiz",
        "/assets/decks/main.quiz",
        "deck bytes",
        "hash:deck-bad",
        asset_materialized_bytes_handoff_status::integrity_blocked));
    bundle.fonts.ready.push_back(make_payload_bundle_entry(
        "duplicate_payload",
        asset_type::font,
        "asset://fonts/body.ttf",
        "/assets/fonts/body.ttf",
        "font bytes",
        ""));
    bundle.sounds.ready.push_back(make_payload_bundle_entry(
        "duplicate_payload",
        asset_type::sound,
        "asset://sounds/correct.ogg",
        "/assets/sounds/correct.ogg",
        "sound bytes",
        ""));

    const std::vector<asset_materialized_byte_payload_selection_request> requests{
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::image,
            .expected_cache_key = asset_cache_key{"image|asset://cards/front.png"},
        },
        asset_materialized_byte_payload_selection_request{
            .id = "rootless_shader",
            .expected_type = asset_type::shader,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "missing_image",
            .expected_type = asset_type::image,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::sound,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::image,
            .expected_cache_key = asset_cache_key{"image|asset://cards/back.png"},
        },
        asset_materialized_byte_payload_selection_request{
            .id = "main_deck",
            .expected_type = asset_type::deck,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "duplicate_payload",
        },
    };

    const asset_materialized_byte_payload_request_transaction transaction =
        make_materialized_asset_byte_payload_request_transaction(bundle, requests);
    const asset_materialized_byte_payload_request_transaction_review_summary review =
        summarize_materialized_asset_byte_payload_request_transaction(transaction);

    require(!review.ok(), "payload transaction review summary reports failed requests");
    require(review.total.request_count == 7U, "payload transaction review keeps total request count");
    require(review.total.selected_count == 1U, "payload transaction review keeps total selected count");
    require(review.total.failed_count() == 6U, "payload transaction review keeps total failed count");
    require(review.type_count() == 5U, "payload transaction review emits expected-type groups with requests");
    require(
        review.diagnostic == "materialized byte payload request transaction review summary computed",
        "payload transaction review diagnostic is stable");
    require(
        review.find_expected_type(asset_type::font) == nullptr,
        "payload transaction review omits expected types with no requests");

    const asset_materialized_byte_payload_request_transaction_type_summary* image =
        review.find_expected_type(asset_type::image);
    require(image != nullptr, "payload transaction review groups image requests");
    require(image->expected_type == asset_type::image, "image review summary preserves expected type");
    require(image->request_indexes == std::vector<std::size_t>{0U, 2U, 4U}, "image review keeps input indexes");
    require(image->request_count == 3U, "image review counts image requests");
    require(image->selected_count == 1U, "image review counts selected image requests");
    require(image->ready_count == 1U, "image review counts ready selected image requests");
    require(image->missing_count == 1U, "image review counts missing image requests");
    require(image->cache_key_mismatch_count == 1U, "image review counts cache-key mismatches");
    require(image->failed_count() == 2U, "image review totals failed image requests");
    require(!image->ok(), "image review reports failed image requests");

    const asset_materialized_byte_payload_request_transaction_type_summary* shader =
        review.find_expected_type(asset_type::shader);
    require(shader != nullptr, "payload transaction review groups shader requests");
    require(shader->request_indexes == std::vector<std::size_t>{1U}, "shader review keeps input index");
    require(shader->blocked_count == 1U, "shader review counts blocked shader requests");
    require(shader->failed_count() == 1U, "shader review totals failed shader requests");

    const asset_materialized_byte_payload_request_transaction_type_summary* sound =
        review.find_expected_type(asset_type::sound);
    require(sound != nullptr, "payload transaction review groups sound requests");
    require(sound->request_indexes == std::vector<std::size_t>{3U}, "sound review keeps input index");
    require(sound->wrong_type_count == 1U, "sound review counts wrong-type requests");

    const asset_materialized_byte_payload_request_transaction_type_summary* deck =
        review.find_expected_type(asset_type::deck);
    require(deck != nullptr, "payload transaction review groups deck requests");
    require(deck->request_indexes == std::vector<std::size_t>{5U}, "deck review keeps input index");
    require(deck->integrity_failure_count == 1U, "deck review counts integrity failures");

    const asset_materialized_byte_payload_request_transaction_type_summary* generic =
        review.find_expected_type(asset_type::generic);
    require(generic != nullptr, "payload transaction review groups generic requests");
    require(generic->request_indexes == std::vector<std::size_t>{6U}, "generic review keeps input index");
    require(generic->duplicate_count == 1U, "generic review counts duplicate-id requests");
    require(generic->failed_count() == 1U, "generic review totals failed duplicate requests");
}

void test_materialized_asset_byte_payload_request_transaction_diff_tracks_status_and_count_deltas()
{
    using namespace quiz_vulkan::assets;

    asset_materialized_byte_payload_bundle before_bundle;
    before_bundle.images.ready.push_back(make_payload_bundle_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "image bytes",
        "hash:image-old"));
    before_bundle.shaders.blocked.push_back(make_payload_bundle_entry(
        "rootless_shader",
        asset_type::shader,
        "asset://shaders/rootless.vert.spv",
        "/assets/shaders/rootless.vert.spv",
        "",
        "hash:rootless",
        asset_materialized_bytes_handoff_status::materialization_blocked));
    before_bundle.decks.blocked.push_back(make_payload_bundle_entry(
        "main_deck",
        asset_type::deck,
        "asset://decks/main.quiz",
        "/assets/decks/main.quiz",
        "deck bytes",
        "hash:deck-bad",
        asset_materialized_bytes_handoff_status::integrity_blocked));

    asset_materialized_byte_payload_bundle after_bundle;
    after_bundle.images.ready.push_back(make_payload_bundle_entry(
        "card_front",
        asset_type::image,
        "asset://cards/front.png",
        "/assets/cards/front.png",
        "image bytes!",
        "hash:image-new"));
    after_bundle.shaders.ready.push_back(make_payload_bundle_entry(
        "rootless_shader",
        asset_type::shader,
        "asset://shaders/rootless.vert.spv",
        "/assets/shaders/rootless.vert.spv",
        "shader bytes",
        "hash:shader-ready"));
    after_bundle.sounds.ready.push_back(make_payload_bundle_entry(
        "answer_sound",
        asset_type::sound,
        "asset://sounds/correct.ogg",
        "/assets/sounds/correct.ogg",
        "sound bytes",
        "hash:sound"));

    const std::vector<asset_materialized_byte_payload_selection_request> before_requests{
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::image,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "rootless_shader",
            .expected_type = asset_type::shader,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "missing_image",
            .expected_type = asset_type::image,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::image,
            .expected_cache_key = asset_cache_key{"image|asset://cards/back.png"},
        },
        asset_materialized_byte_payload_selection_request{
            .id = "main_deck",
            .expected_type = asset_type::deck,
        },
    };
    const std::vector<asset_materialized_byte_payload_selection_request> after_requests{
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::image,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "rootless_shader",
            .expected_type = asset_type::shader,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "answer_sound",
            .expected_type = asset_type::sound,
        },
        asset_materialized_byte_payload_selection_request{
            .id = "card_front",
            .expected_type = asset_type::image,
            .expected_cache_key = asset_cache_key{"image|asset://cards/front.png"},
        },
    };

    const asset_materialized_byte_payload_request_transaction before =
        make_materialized_asset_byte_payload_request_transaction(before_bundle, before_requests);
    const asset_materialized_byte_payload_request_transaction after =
        make_materialized_asset_byte_payload_request_transaction(after_bundle, after_requests);
    const asset_materialized_byte_payload_request_transaction_diff_summary diff =
        diff_materialized_asset_byte_payload_request_transactions(before, after);

    require(!diff.empty(), "payload transaction diff reports request changes");
    require(diff.change_count() == 6U, "payload transaction diff counts added removed and changed requests");
    require(diff.added.size() == 1U, "payload transaction diff records added request ids");
    require(diff.removed.size() == 2U, "payload transaction diff records removed request ids");
    require(diff.changed.size() == 3U, "payload transaction diff records changed request statuses and snapshots");
    require(
        diff.diagnostic == "materialized byte payload request transaction diff computed",
        "payload transaction diff diagnostic is stable");

    require(diff.before_summary.request_count == 5U, "payload transaction diff preserves before summary");
    require(diff.after_summary.request_count == 4U, "payload transaction diff preserves after summary");
    require(diff.count_delta.request_delta == -1, "payload transaction diff tracks request-count delta");
    require(diff.count_delta.selected_delta == 3, "payload transaction diff tracks selected-count delta");
    require(diff.count_delta.ready_delta == 3, "payload transaction diff tracks ready-count delta");
    require(diff.count_delta.blocked_delta == -1, "payload transaction diff tracks blocked-count delta");
    require(diff.count_delta.missing_delta == -1, "payload transaction diff tracks missing-count delta");
    require(
        diff.count_delta.cache_key_mismatch_delta == -1,
        "payload transaction diff tracks cache-key mismatch delta");
    require(
        diff.count_delta.integrity_failure_delta == -1,
        "payload transaction diff tracks integrity failure delta");
    require(diff.count_delta.duplicate_delta == 0, "payload transaction diff tracks stable duplicate count");
    require(diff.count_delta.failed_delta == -4, "payload transaction diff tracks failed-count delta");
    require(!diff.count_delta.empty(), "payload transaction diff count delta reports changes");

    const asset_materialized_byte_payload_request_transaction_diff_entry* added =
        diff.find_added("answer_sound");
    require(added != nullptr, "payload transaction diff can find added requests");
    require(
        added->kind == asset_materialized_byte_payload_request_transaction_delta_kind::added_request,
        "payload transaction diff marks added requests");
    require(added->after_index.has_value() && *added->after_index == 2U, "added request keeps after index");
    require(!added->before_index.has_value(), "added request has no before index");
    require(added->after_status == asset_materialized_byte_payload_selection_status::selected,
        "added request keeps after status");
    require(added->after_selected_snapshot.has_value(), "added request keeps selected compact snapshot");
    require(added->after_selected_snapshot->id == "answer_sound", "added request snapshot keeps id");

    const asset_materialized_byte_payload_request_transaction_diff_entry* removed_missing =
        diff.find_removed("missing_image");
    require(removed_missing != nullptr, "payload transaction diff can find removed missing requests");
    require(
        removed_missing->kind == asset_materialized_byte_payload_request_transaction_delta_kind::removed_request,
        "payload transaction diff marks removed requests");
    require(
        removed_missing->before_status == asset_materialized_byte_payload_selection_status::missing_id,
        "removed missing request keeps before status");
    require(removed_missing->before_index.has_value() && *removed_missing->before_index == 2U,
        "removed request keeps before index");
    require(!removed_missing->after_index.has_value(), "removed request has no after index");

    const asset_materialized_byte_payload_request_transaction_diff_entry* removed_integrity =
        diff.find_removed("main_deck");
    require(removed_integrity != nullptr, "payload transaction diff can find removed integrity failures");
    require(
        removed_integrity->before_status == asset_materialized_byte_payload_selection_status::integrity_failure,
        "removed integrity request keeps before status");
    require(removed_integrity->before_snapshot.has_value(), "removed integrity request keeps compact snapshot");
    require(!removed_integrity->before_selected_snapshot.has_value(), "removed failed request has no selected snapshot");

    const asset_materialized_byte_payload_request_transaction_diff_entry* image = diff.find_changed("card_front");
    require(image != nullptr, "payload transaction diff can find changed image requests");
    require(image->occurrence == 0U, "payload transaction diff records first request occurrence");
    require(!image->status_changed, "payload transaction diff leaves stable selected status unchanged");
    require(image->selected_snapshot_changed, "payload transaction diff tracks selected snapshot changes");
    require(!image->readiness_changed, "payload transaction diff leaves stable readiness unchanged");
    require(image->before_selected_snapshot.has_value(), "changed request keeps before selected snapshot");
    require(image->after_selected_snapshot.has_value(), "changed request keeps after selected snapshot");
    require(
        image->before_selected_snapshot->content_hash == "hash:image-old"
            && image->after_selected_snapshot->content_hash == "hash:image-new",
        "payload transaction diff keeps before and after selected hash evidence");

    const asset_materialized_byte_payload_request_transaction_diff_entry* shader =
        diff.find_changed("rootless_shader");
    require(shader != nullptr, "payload transaction diff can find changed blocked requests");
    require(shader->status_changed, "payload transaction diff tracks blocked-to-selected status changes");
    require(shader->selected_snapshot_changed, "payload transaction diff tracks selected snapshot appearance");
    require(shader->readiness_changed, "payload transaction diff tracks readiness changes");
    require(
        shader->before_status == asset_materialized_byte_payload_selection_status::blocked_payload
            && shader->after_status == asset_materialized_byte_payload_selection_status::selected,
        "payload transaction diff keeps before and after statuses");
    require(shader->before_snapshot.has_value() && !shader->before_snapshot->ready,
        "payload transaction diff keeps blocked readiness evidence");
    require(shader->after_snapshot.has_value() && shader->after_snapshot->ready,
        "payload transaction diff keeps ready evidence");

    const asset_materialized_byte_payload_request_transaction_diff_entry* cache_mismatch = nullptr;
    for (const asset_materialized_byte_payload_request_transaction_diff_entry& entry : diff.changed) {
        if (entry.id == "card_front" && entry.occurrence == 1U) {
            cache_mismatch = &entry;
            break;
        }
    }
    require(cache_mismatch != nullptr, "payload transaction diff can distinguish repeated request ids");
    require(cache_mismatch->status_changed, "payload transaction diff tracks cache mismatch status changes");
    require(
        cache_mismatch->cache_key_mismatch_changed,
        "payload transaction diff tracks cache-key mismatch status changes");
    require(
        cache_mismatch->before_status == asset_materialized_byte_payload_selection_status::cache_key_mismatch
            && cache_mismatch->after_status == asset_materialized_byte_payload_selection_status::selected,
        "payload transaction diff keeps cache mismatch before and after statuses");
    require(
        cache_mismatch->before_index.has_value() && *cache_mismatch->before_index == 3U
            && cache_mismatch->after_index.has_value() && *cache_mismatch->after_index == 3U,
        "payload transaction diff keeps occurrence indexes for repeated ids");
}

void test_materialized_asset_bytes_integrity_fails_before_provider_for_unmaterialized_sources()
{
    using namespace quiz_vulkan::assets;

    asset_manifest manifest;
    manifest.entries.push_back(asset_manifest_entry{
        .id = "rootless_shader",
        .type = asset_type::shader,
        .uri = "asset://shaders/ui.vert.spv",
    });

    const normalizing_asset_resolver resolver;
    const runtime_asset_catalog catalog = build_runtime_asset_catalog(manifest, resolver);
    const counting_asset_bytes_provider provider(asset_bytes_load_result{
        .status = asset_bytes_load_status::loaded,
        .bytes = detail::make_asset_byte_vector("shader bytes"),
        .byte_count = 12U,
        .cache_key = "shader|asset://shaders/ui.vert.spv",
        .source_uri = "asset://shaders/ui.vert.spv",
    });

    const asset_bytes_integrity_report report = load_materialized_asset_bytes_with_integrity(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "rootless_shader", .expected_type = asset_type::shader});

    require(!report.ok(), "rootless assets fail before byte integrity can pass");
    require(report.load.status == asset_bytes_load_status::source_not_readable, "pre-provider failure preserves load status");
    require(provider.load_count() == 0U, "materialization failures do not call the byte provider");
    require(
        integrity_issue_at(report, 0U, asset_bytes_integrity_issue_kind::load_failed, "rootless_shader"),
        "pre-provider failure is reported as a load failure integrity issue");
    require(
        report.issues[0].expected_cache_key == "shader|asset://shaders/ui.vert.spv",
        "pre-provider integrity failure keeps expected cache-key diagnostics");
}

void test_materialized_asset_bytes_integrity_fails_after_provider_for_byte_count_mismatch()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const runtime_asset_catalog_snapshot snapshot = make_card_front_snapshot(fixture_root);
    const counting_asset_bytes_provider provider(asset_bytes_load_result{
        .status = asset_bytes_load_status::loaded,
        .bytes = detail::make_asset_byte_vector("image bytes"),
        .byte_count = 12U,
        .cache_key = snapshot.cache_key,
        .source_uri = snapshot.source.normalized_uri,
    });

    const asset_bytes_integrity_report report = load_materialized_asset_bytes_with_integrity(
        provider,
        materialize_runtime_asset(snapshot));

    require(!report.ok(), "byte-count mismatches fail after provider load");
    require(provider.load_count() == 1U, "byte-count integrity is checked after provider read");
    require(
        integrity_issue_at(report, 0U, asset_bytes_integrity_issue_kind::byte_count_mismatch, "card_front"),
        "post-provider byte-count mismatch is reported");
    require(report.load.ok(), "post-provider integrity failure keeps the loaded byte result");
}

void test_materialized_asset_bytes_integrity_fails_after_provider_for_metadata_mismatch()
{
    using namespace quiz_vulkan::assets;

    const std::filesystem::path fixture_root = reset_fixture_root();
    const runtime_asset_catalog_snapshot snapshot = make_card_front_snapshot(fixture_root);
    const counting_asset_bytes_provider provider(asset_bytes_load_result{
        .status = asset_bytes_load_status::loaded,
        .bytes = detail::make_asset_byte_vector("image bytes"),
        .byte_count = 11U,
        .cache_key = "image|asset://cards/back.png",
        .source_uri = "asset://cards/back.png",
    });

    const asset_bytes_integrity_report report = load_asset_bytes_with_integrity(provider, snapshot);

    require(!report.ok(), "metadata mismatches fail after provider load");
    require(provider.load_count() == 1U, "metadata integrity is checked after provider read");
    require(
        integrity_issue_at(report, 0U, asset_bytes_integrity_issue_kind::cache_key_mismatch, "card_front"),
        "post-provider cache-key mismatch is reported");
    require(
        integrity_issue_at(report, 1U, asset_bytes_integrity_issue_kind::source_uri_mismatch, "card_front"),
        "post-provider source-uri mismatch is reported");
    require(
        report.issues[0].expected_cache_key == snapshot.cache_key
            && report.issues[0].cache_key == "image|asset://cards/back.png",
        "metadata mismatch carries observed and expected cache keys");
}

} // namespace

int main()
{
    test_fake_provider_loads_bytes_by_snapshot_and_catalog_id();
    test_fake_provider_reports_lookup_and_cache_misses();
    test_local_file_provider_reads_rooted_paths_and_rejects_unreadable_sources();
    test_materialized_asset_bytes_read_local_files();
    test_materialized_asset_bytes_do_not_read_unsupported_or_unmaterialized_sources();
    test_materialized_asset_bytes_surface_materialization_policy_diagnostics();
    test_materialized_asset_bytes_call_provider_after_materialization();
    test_materialized_asset_bytes_load_build_external_quiz_data_assets();
    test_materialized_asset_bytes_with_integrity_loads_catalog_font_image_and_shader_bytes();
    test_materialized_asset_bytes_with_integrity_reports_catalog_provider_failures();
    test_asset_bytes_integrity_validates_load_result_byte_count_and_content();
    test_materialized_asset_bytes_cache_policy_summary_tracks_hash_and_integrity();
    test_typed_materialized_asset_bytes_summary_groups_engine_assets();
    test_typed_materialized_asset_bytes_diff_tracks_engine_entry_deltas();
    test_materialized_asset_bytes_handoff_summary_groups_ready_and_blocked_payloads();
    test_materialized_asset_byte_payload_bundle_groups_loaded_bytes_by_type();
    test_materialized_asset_byte_payload_bundle_diff_tracks_snapshot_changes();
    test_shader_materialized_byte_pipeline_summary_classifies_shader_payloads();
    test_shader_byte_pipeline_source_summary_combines_manifest_fallback_and_payload_evidence();
    test_shader_payload_runtime_summary_tracks_stage_revision_and_cache_identity();
    test_render_resource_payload_bridge_matches_addresses_to_materialized_payloads();
    test_render_resource_manifest_to_payload_bridge_e2e_uses_materialized_bytes();
    test_render_resource_materialized_cache_summary_reuses_manifest_payload_identity();
    test_render_resource_materialized_cache_diff_tracks_reuse_and_invalidation();
    test_materialized_asset_byte_payload_selection_filters_payloads_and_reports_diagnostics();
    test_materialized_asset_byte_payload_request_transaction_preserves_order_and_counts();
    test_materialized_asset_byte_payload_request_transaction_review_summary_groups_by_expected_type();
    test_materialized_asset_byte_payload_request_transaction_diff_tracks_status_and_count_deltas();
    test_materialized_asset_bytes_integrity_fails_before_provider_for_unmaterialized_sources();
    test_materialized_asset_bytes_integrity_fails_after_provider_for_byte_count_mismatch();
    test_materialized_asset_bytes_integrity_fails_after_provider_for_metadata_mismatch();
    return 0;
}
