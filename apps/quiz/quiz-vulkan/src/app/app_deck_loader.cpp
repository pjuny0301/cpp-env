#include "app/app_deck_loader.h"

#include "core/domain/deck_artifact_loader.hpp"

#include <filesystem>
#include <iterator>
#include <sstream>
#include <string>

namespace quiz_vulkan {
namespace {

void append_artifact_error_messages(
    app_deck_load_result& output,
    const std::filesystem::path& artifact_path,
    const domain::deck_artifact_load_result& result)
{
    std::ostringstream message;
    message << "failed to load deck artifact: " << artifact_path.string();
    output.messages.push_back(message.str());

    for (const domain::deck_artifact_parse_error& error : result.errors) {
        std::ostringstream error_line;
        error_line << "  line " << error.line << ": " << error.message;
        output.messages.push_back(error_line.str());
    }
}

void load_direct_deck_artifacts(
    app_deck_load_result& output,
    const std::vector<std::filesystem::path>& deck_artifacts)
{
    for (const std::filesystem::path& artifact_path : deck_artifacts) {
        const domain::deck_artifact_load_result result = domain::load_deck_artifact_file(artifact_path);
        if (result.ok() && result.value.has_value()) {
            output.messages.push_back("loaded deck artifact: " + artifact_path.string());
            output.decks.push_back(*result.value);
            continue;
        }

        append_artifact_error_messages(output, artifact_path, result);
    }
}

std::string source_label(const domain::deck_source_request& request, const domain::deck_source_result* result = nullptr)
{
    if (result != nullptr && !result->source_uri.empty()) {
        return result->source_uri;
    }
    return request.uri;
}

void append_source_error_messages(
    app_deck_load_result& output,
    const domain::deck_source_request& request,
    const domain::deck_source_result& result)
{
    const std::string label = source_label(request, &result);
    output.messages.push_back("failed to load deck source: " + label);

    for (const domain::deck_source_diagnostic& diagnostic : result.diagnostics) {
        const std::size_t line = diagnostic.line;
        std::ostringstream error_line;
        error_line << "  line " << line << ": " << diagnostic.message;
        output.messages.push_back(error_line.str());
    }
}

void load_provider_deck_sources(
    app_deck_load_result& output,
    const std::vector<domain::deck_source_request>& deck_sources,
    domain::deck_source_provider_interface* provider)
{
    for (const domain::deck_source_request& source_request : deck_sources) {
        if (provider == nullptr) {
            output.messages.push_back("failed to load deck source: " + source_label(source_request));
            output.messages.push_back("  line 0: deck source provider is unavailable");
            continue;
        }

        domain::deck_source_result result = provider->load_decks(source_request);
        if (result.ok()) {
            output.messages.push_back("loaded deck source: " + source_label(source_request, &result));
            output.decks.insert(
                output.decks.end(),
                std::make_move_iterator(result.decks.begin()),
                std::make_move_iterator(result.decks.end()));
            continue;
        }

        append_source_error_messages(output, source_request, result);
    }
}

} // namespace

app_deck_load_result load_app_decks(const app_deck_load_request& request)
{
    app_deck_load_result result;
    load_direct_deck_artifacts(result, request.deck_artifacts);
    load_provider_deck_sources(result, request.deck_sources, request.deck_source_provider);
    return result;
}

} // namespace quiz_vulkan
