#include "app/app_deck_loader.h"
#include "assets/asset_resolver.h"
#include "assets/deck_source_asset_adapter.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "app_deck_loader_tests failed: " << message << '\n';
    }
    assert((condition) && message);
}

bool contains(const std::vector<std::string>& messages, const std::string& needle)
{
    for (const std::string& message : messages) {
        if (message.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string make_deck_artifact(std::string deck_id, std::string source_uri)
{
    return
        "[deck]\n"
        "schema_version=quiz-deck-v1\n"
        "id=" + deck_id + "\n"
        "title=Loaded Deck\n"
        "source_uri=" + source_uri + "\n"
        "\n"
        "[day]\n"
        "id=day_1\n"
        "title=Day 1\n"
        "\n"
        "[question]\n"
        "id=q1\n"
        "type=answer\n"
        "prompt=What loaded this deck?\n"
        "accepted_answer=app deck loader\n";
}

void write_text_file(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    require(output.good(), "test file can be opened for writing");
    output << text;
    require(output.good(), "test file can be written");
}

std::filesystem::path reset_test_root()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "quiz_vulkan_app_deck_loader_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

class fake_deck_source_provider final : public quiz_vulkan::domain::deck_source_provider_interface {
public:
    explicit fake_deck_source_provider(quiz_vulkan::domain::deck deck)
        : deck_(std::move(deck))
    {
    }

    quiz_vulkan::domain::deck_source_result load_decks(
        const quiz_vulkan::domain::deck_source_request& request) override
    {
        last_request = request;
        quiz_vulkan::domain::deck_source_result result;
        result.status = quiz_vulkan::domain::deck_source_status::loaded;
        result.source_uri = request.uri;
        result.decks.push_back(deck_);
        return result;
    }

    quiz_vulkan::domain::deck_source_request last_request;

private:
    quiz_vulkan::domain::deck deck_;
};

quiz_vulkan::domain::deck make_test_deck(std::string id, std::string source_uri)
{
    quiz_vulkan::domain::deck deck;
    deck.id = std::move(id);
    deck.title = "Provider Deck";
    deck.source_uri = std::move(source_uri);

    quiz_vulkan::domain::day day;
    day.id = "day_1";
    day.title = "Day 1";

    quiz_vulkan::domain::question question;
    question.id = "q1";
    question.prompt = "What loaded this deck?";
    question.accepted_answers.push_back("app deck loader");

    day.questions.push_back(std::move(question));
    deck.days.push_back(std::move(day));
    return deck;
}

void test_direct_artifact_path_behavior_is_preserved()
{
    using namespace quiz_vulkan;

    const std::filesystem::path root = reset_test_root();
    const std::filesystem::path artifact_path = root / "direct" / "deck.quizdeck";
    write_text_file(artifact_path, make_deck_artifact("direct", artifact_path.string()));

    const app_deck_load_result result = load_app_decks(app_deck_load_request{
        .deck_artifacts = {artifact_path},
    });

    require(result.decks.size() == 1, "direct artifact loads one deck");
    require(result.decks.front().id == "direct", "direct artifact deck id is loaded");
    require(contains(result.messages, "loaded deck artifact: " + artifact_path.string()),
        "direct load message is preserved");
}

void test_provider_request_path_uses_deck_source_interface()
{
    using namespace quiz_vulkan;

    fake_deck_source_provider provider(make_test_deck("provider", "asset://provider.quizdeck"));
    const app_deck_load_result result = load_app_decks(app_deck_load_request{
        .deck_sources = {domain::deck_source_request{
            .uri = "asset://provider.quizdeck",
            .cache_key = "deck|asset://provider.quizdeck",
        }},
        .deck_source_provider = &provider,
    });

    require(result.decks.size() == 1, "provider request loads one deck");
    require(result.decks.front().id == "provider", "provider deck id is loaded");
    require(provider.last_request.uri == "asset://provider.quizdeck", "provider receives source uri");
    require(provider.last_request.cache_key == "deck|asset://provider.quizdeck", "provider receives cache key");
    require(contains(result.messages, "loaded deck source: asset://provider.quizdeck"),
        "provider load message is recorded");
}

void test_provider_unavailable_reports_diagnostic_without_direct_fallback()
{
    using namespace quiz_vulkan;

    const app_deck_load_result result = load_app_decks(app_deck_load_request{
        .deck_sources = {domain::deck_source_request{.uri = "asset://missing-provider.quizdeck"}},
    });

    require(result.decks.empty(), "missing provider does not load decks");
    require(contains(result.messages, "failed to load deck source: asset://missing-provider.quizdeck"),
        "missing provider failure message is recorded");
    require(contains(result.messages, "deck source provider is unavailable"),
        "missing provider diagnostic is recorded");
}

void test_asset_provider_path_keeps_external_root_and_rejects_traversal()
{
    using namespace quiz_vulkan;

    const std::filesystem::path root = reset_test_root();
    const std::filesystem::path external_root = root / "build" / "external" / "quiz" / "quiz-data";
    write_text_file(
        external_root / "mobile-decks" / "provider.quizdeck",
        make_deck_artifact("asset_provider", "asset://mobile-decks/provider.quizdeck"));

    assets::normalizing_asset_resolver resolver;
    assets::asset_deck_source_provider provider(
        resolver,
        assets::deck_source_asset_adapter_config{.asset_root = external_root});

    const app_deck_load_result loaded = load_app_decks(app_deck_load_request{
        .deck_sources = {domain::deck_source_request{.uri = "asset://mobile-decks/provider.quizdeck"}},
        .deck_source_provider = &provider,
    });

    require(loaded.decks.size() == 1, "app provider path loads from configured external root");
    require(loaded.decks.front().id == "asset_provider", "asset provider deck id is loaded");

    const app_deck_load_result rejected = load_app_decks(app_deck_load_request{
        .deck_sources = {domain::deck_source_request{.uri = "asset://mobile-decks/%2e%2e/secret.quizdeck"}},
        .deck_source_provider = &provider,
    });

    require(rejected.decks.empty(), "traversal source does not load through app provider path");
    require(contains(rejected.messages, "asset path traversal is not allowed"),
        "traversal diagnostic is propagated");
}

} // namespace

int main()
{
    test_direct_artifact_path_behavior_is_preserved();
    test_provider_request_path_uses_deck_source_interface();
    test_provider_unavailable_reports_diagnostic_without_direct_fallback();
    test_asset_provider_path_keeps_external_root_and_rejects_traversal();
    return 0;
}
