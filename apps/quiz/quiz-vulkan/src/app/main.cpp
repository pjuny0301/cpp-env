#include "app/app.h"

#include <filesystem>
#include <iostream>
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
        << "       quiz_vulkan [artifact ...]\n";
}

bool append_argument_deck_path(
    int argc,
    char** argv,
    int& index,
    quiz_vulkan::app_config& config,
    std::string_view option_name)
{
    if (index + 1 >= argc) {
        std::cerr << option_name << " requires a path\n";
        return false;
    }

    ++index;
    config.deck_artifacts.emplace_back(std::filesystem::path{argv[index]});
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

        if (starts_with(argument, "--deck=")) {
            config.deck_artifacts.emplace_back(std::filesystem::path{argument.substr(7)});
            continue;
        }

        if (starts_with(argument, "--source=")) {
            config.deck_artifacts.emplace_back(std::filesystem::path{argument.substr(9)});
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
