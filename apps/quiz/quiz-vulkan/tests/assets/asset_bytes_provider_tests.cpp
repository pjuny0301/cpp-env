#include "assets/asset_bytes_provider.h"

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
    test_materialized_asset_bytes_integrity_fails_before_provider_for_unmaterialized_sources();
    test_materialized_asset_bytes_integrity_fails_after_provider_for_byte_count_mismatch();
    test_materialized_asset_bytes_integrity_fails_after_provider_for_metadata_mismatch();
    return 0;
}
