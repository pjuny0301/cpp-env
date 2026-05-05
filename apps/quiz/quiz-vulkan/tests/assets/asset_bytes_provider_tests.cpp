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
    require(bytes_to_string(by_snapshot.bytes) == "image bytes", "fake byte provider returns registered bytes");

    const asset_bytes_load_result by_id = load_asset_bytes(
        provider,
        catalog,
        asset_bytes_catalog_request{.id = "card_front", .expected_type = asset_type::image});
    require(by_id.ok(), "fake byte provider loads bytes by catalog id");
    require(by_id.cache_key == by_snapshot.cache_key, "catalog byte request preserves cache key");
    require(by_id.source_uri == by_snapshot.source_uri, "catalog byte request preserves source uri");
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
    runtime_asset_catalog_snapshot snapshot{
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
    require(bytes_to_string(loaded.bytes) == "image bytes", "provider result is returned after materialization");
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
        .cache_key = snapshot.cache_key,
        .source_uri = snapshot.source.normalized_uri,
    };

    const asset_bytes_integrity_report valid = validate_asset_bytes_integrity(asset_bytes_integrity_request{
        .snapshot = snapshot,
        .load = load,
    });
    require(valid.ok(), "asset byte integrity accepts matching metadata and byte counts");

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
    test_asset_bytes_integrity_validates_load_result_byte_count_and_content();
    test_materialized_asset_bytes_integrity_fails_before_provider_for_unmaterialized_sources();
    test_materialized_asset_bytes_integrity_fails_after_provider_for_byte_count_mismatch();
    test_materialized_asset_bytes_integrity_fails_after_provider_for_metadata_mismatch();
    return 0;
}
