#include "assets/asset_resolver.h"
#include "assets/deck_source_asset_adapter.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "deck_source_asset_adapter_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

std::string make_deck_artifact(std::string deck_id, std::string source_uri)
{
    return
        "[deck]\n"
        "schema_version=quiz-deck-v1\n"
        "id=" + deck_id + "\n"
        "title=Asset Loaded Deck\n"
        "source_uri=" + source_uri + "\n"
        "\n"
        "[day]\n"
        "id=day_1\n"
        "title=Day 1\n"
        "\n"
        "[question]\n"
        "id=q1\n"
        "type=answer\n"
        "prompt=What loads this deck?\n"
        "accepted_answer=asset resolver\n";
}

void write_text_file(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    require(output.good(), "test artifact can be opened for writing");
    output << text;
    require(output.good(), "test artifact can be written");
}

std::filesystem::path reset_test_root()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "quiz_vulkan_deck_source_asset_adapter_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void test_asset_uri_loads_deck_from_configured_external_root()
{
    using namespace quiz_vulkan;

    const std::filesystem::path root = reset_test_root();
    const std::filesystem::path external_root = root / "build" / "external" / "quiz" / "quiz-data";
    write_text_file(
        external_root / "mobile-decks" / "software.quizdeck",
        make_deck_artifact("software", "asset://mobile-decks/software.quizdeck"));

    const assets::normalizing_asset_resolver resolver;
    assets::asset_deck_source_provider provider(
        resolver,
        assets::deck_source_asset_adapter_config{.asset_root = external_root});

    const domain::deck_source_result result = provider.load_decks(domain::deck_source_request{
        .uri = "  ASSET:///mobile-decks\\./software.quizdeck  ",
        .cache_key = "deck|asset://mobile-decks/software.quizdeck",
    });

    require(result.ok(), "asset deck source loads");
    require(result.source_uri == "asset://mobile-decks/software.quizdeck", "result source uri is normalized");
    require(result.decks.size() == 1, "one deck is loaded");
    require(result.decks.front().id == "software", "loaded deck id matches artifact");
    require(result.decks.front().source_uri == "asset://mobile-decks/software.quizdeck", "deck source uri is parsed");
}

void test_local_uri_loads_deck_from_configured_local_root()
{
    using namespace quiz_vulkan;

    const std::filesystem::path root = reset_test_root();
    const std::filesystem::path external_root = root / "build" / "external" / "quiz" / "quiz-data";
    write_text_file(
        external_root / "local" / "decks" / "local.quizdeck",
        make_deck_artifact("local", "local/decks/local.quizdeck"));

    const assets::normalizing_asset_resolver resolver;
    assets::asset_deck_source_provider provider(
        resolver,
        assets::deck_source_asset_adapter_config{.local_root = external_root});

    const domain::deck_source_result result = provider.load_decks(domain::deck_source_request{
        .uri = "./local\\decks/./local.quizdeck",
    });

    require(result.ok(), "local deck source loads");
    require(result.source_uri == "local/decks/local.quizdeck", "local source uri is normalized");
    require(result.decks.front().id == "local", "local deck id matches artifact");
}

void test_traversal_is_rejected_before_filesystem_mapping()
{
    using namespace quiz_vulkan;

    const std::filesystem::path root = reset_test_root();
    const assets::normalizing_asset_resolver resolver;
    assets::asset_deck_source_provider provider(
        resolver,
        assets::deck_source_asset_adapter_config{.asset_root = root / "build" / "external"});

    const domain::deck_source_result result = provider.load_decks(domain::deck_source_request{
        .uri = "asset://mobile-decks/%2e%2e/secret.quizdeck",
    });

    require(!result.ok(), "traversal deck source is rejected");
    require(result.status == domain::deck_source_status::invalid_data, "traversal maps to invalid data");
    require(!result.diagnostics.empty(), "traversal result has a diagnostic");
}

void test_cache_key_mismatch_is_rejected()
{
    using namespace quiz_vulkan;

    const std::filesystem::path root = reset_test_root();
    const std::filesystem::path external_root = root / "build" / "external" / "quiz" / "quiz-data";
    write_text_file(
        external_root / "mobile-decks" / "software.quizdeck",
        make_deck_artifact("software", "asset://mobile-decks/software.quizdeck"));

    const assets::normalizing_asset_resolver resolver;
    assets::asset_deck_source_provider provider(
        resolver,
        assets::deck_source_asset_adapter_config{.asset_root = external_root});

    const domain::deck_source_result result = provider.load_decks(domain::deck_source_request{
        .uri = "asset://mobile-decks/software.quizdeck",
        .cache_key = "deck|asset://mobile-decks/other.quizdeck",
    });

    require(!result.ok(), "cache key mismatch is rejected");
    require(result.status == domain::deck_source_status::invalid_data, "cache key mismatch maps to invalid data");
}

void test_unsupported_sources_and_missing_files_report_stable_statuses()
{
    using namespace quiz_vulkan;

    const std::filesystem::path root = reset_test_root();
    const assets::normalizing_asset_resolver resolver;
    assets::asset_deck_source_provider provider(
        resolver,
        assets::deck_source_asset_adapter_config{.asset_root = root / "build" / "external"});

    const domain::deck_source_result file_uri = provider.load_decks(domain::deck_source_request{
        .uri = "file:///tmp/deck.quizdeck",
    });
    require(!file_uri.ok(), "file uri is not loaded by adapter");
    require(file_uri.status == domain::deck_source_status::unsupported_source, "file uri is unsupported");

    const domain::deck_source_result remote_uri = provider.load_decks(domain::deck_source_request{
        .uri = "https://example.test/deck.quizdeck",
    });
    require(!remote_uri.ok(), "remote uri is not loaded by adapter");
    require(remote_uri.status == domain::deck_source_status::unsupported_source, "remote uri is unsupported");

    const domain::deck_source_result missing = provider.load_decks(domain::deck_source_request{
        .uri = "asset://mobile-decks/missing.quizdeck",
    });
    require(!missing.ok(), "missing deck source does not load");
    require(missing.status == domain::deck_source_status::not_found, "missing file maps to not_found");
}

} // namespace

int main()
{
    test_asset_uri_loads_deck_from_configured_external_root();
    test_local_uri_loads_deck_from_configured_local_root();
    test_traversal_is_rejected_before_filesystem_mapping();
    test_cache_key_mismatch_is_rejected();
    test_unsupported_sources_and_missing_files_report_stable_statuses();
    return 0;
}
