#include "app/app.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

bool starts_with(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

void print_usage()
{
    std::cout
        << "usage: quiz_vulkan [--deck <artifact>] [--deck=<artifact>]...\n"
        << "       quiz_vulkan [--deck-source <uri>] [--deck-asset-root <path>] [--deck-local-root <path>]\n"
        << "       quiz_vulkan [artifact ...]\n";
}

std::optional<std::string> next_option_value(
    int argc,
    char** argv,
    int& index,
    std::string_view option_name)
{
    if (index + 1 >= argc) {
        std::cerr << option_name << " requires a value\n";
        return std::nullopt;
    }

    ++index;
    return std::string{argv[index]};
}

bool append_argument_deck_path(
    int argc,
    char** argv,
    int& index,
    quiz_vulkan::app_config& config,
    std::string_view option_name)
{
    const std::optional<std::string> path = next_option_value(argc, argv, index, option_name);
    if (!path.has_value()) {
        return false;
    }

    config.deck_artifacts.emplace_back(std::filesystem::path{*path});
    return true;
}

bool append_argument_deck_source(
    int argc,
    char** argv,
    int& index,
    quiz_vulkan::app_config& config,
    std::string_view option_name)
{
    const std::optional<std::string> uri = next_option_value(argc, argv, index, option_name);
    if (!uri.has_value()) {
        return false;
    }

    config.deck_sources.requests.push_back(quiz_vulkan::domain::deck_source_request{
        .uri = *uri,
        .cache_key = {},
        .allow_demo_fallback = false,
    });
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    quiz_vulkan::app_config config;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            print_usage();
            return 0;
        }

        if (argument == "--deck" || argument == "--source") {
            if (!append_argument_deck_path(argc, argv, index, config, argument)) {
                return 2;
            }
            continue;
        }

        if (argument == "--deck-source") {
            if (!append_argument_deck_source(argc, argv, index, config, argument)) {
                return 2;
            }
            continue;
        }

        if (argument == "--deck-asset-root") {
            const std::optional<std::string> root = next_option_value(argc, argv, index, argument);
            if (!root.has_value()) {
                return 2;
            }
            config.deck_sources.asset_root = std::filesystem::path{*root};
            continue;
        }

        if (argument == "--deck-local-root") {
            const std::optional<std::string> root = next_option_value(argc, argv, index, argument);
            if (!root.has_value()) {
                return 2;
            }
            config.deck_sources.local_root = std::filesystem::path{*root};
            continue;
        }

        if (starts_with(argument, "--deck=")) {
            config.deck_artifacts.emplace_back(std::filesystem::path{argument.substr(7)});
            continue;
        }

        if (starts_with(argument, "--source=")) {
            config.deck_artifacts.emplace_back(std::filesystem::path{argument.substr(9)});
            continue;
        }

        if (starts_with(argument, "--deck-source=")) {
            config.deck_sources.requests.push_back(quiz_vulkan::domain::deck_source_request{
                .uri = argument.substr(14),
                .cache_key = {},
                .allow_demo_fallback = false,
            });
            continue;
        }

        if (starts_with(argument, "--deck-asset-root=")) {
            config.deck_sources.asset_root = std::filesystem::path{argument.substr(18)};
            continue;
        }

        if (starts_with(argument, "--deck-local-root=")) {
            config.deck_sources.local_root = std::filesystem::path{argument.substr(18)};
            continue;
        }

        if (!argument.empty() && argument.front() == '-') {
            std::cerr << "unknown option: " << argument << '\n';
            print_usage();
            return 2;
        }

        config.deck_artifacts.emplace_back(std::filesystem::path{argument});
    }

    quiz_vulkan::app application(std::move(config));
    return application.run();
}
