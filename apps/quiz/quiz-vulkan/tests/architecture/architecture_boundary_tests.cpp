#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct boundary_rule {
    std::string_view area;
    std::vector<std::filesystem::path> roots;
    std::vector<std::filesystem::path> excluded_files;
    std::vector<std::string_view> forbidden_tokens;
};

struct violation {
    std::filesystem::path file;
    std::string_view area;
    std::string_view token;
    std::size_t line = 0;
};

bool source_extension(const std::filesystem::path& path)
{
    const std::string extension = path.extension().string();
    return extension == ".h" || extension == ".hpp" || extension == ".cpp"
        || extension == ".inl";
}

bool header_extension(const std::filesystem::path& path)
{
    const std::string extension = path.extension().string();
    return extension == ".h" || extension == ".hpp";
}

std::vector<std::filesystem::path> source_files_under(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(root)) {
        return files;
    }

    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && source_extension(entry.path())) {
            files.push_back(entry.path());
        }
    }
    return files;
}

std::vector<std::filesystem::path> header_files_under(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(root)) {
        return files;
    }

    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && header_extension(entry.path())) {
            files.push_back(entry.path());
        }
    }
    return files;
}

std::string read_file_text(const std::filesystem::path& path)
{
    std::ifstream input(path);
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

std::string generic_relative_path(
    const std::filesystem::path& root,
    const std::filesystem::path& file)
{
    return std::filesystem::relative(file, root).generic_string();
}

bool is_excluded_file(
    const std::filesystem::path& source_root,
    const std::filesystem::path& file,
    const boundary_rule& rule)
{
    const std::filesystem::path relative_file = std::filesystem::relative(file, source_root);
    return std::find(rule.excluded_files.begin(), rule.excluded_files.end(), relative_file)
        != rule.excluded_files.end();
}

std::vector<violation> scan_rule(const std::filesystem::path& source_root, const boundary_rule& rule)
{
    std::vector<violation> violations;
    for (const std::filesystem::path& relative_root : rule.roots) {
        for (const std::filesystem::path& file : source_files_under(source_root / relative_root)) {
            if (is_excluded_file(source_root, file, rule)) {
                continue;
            }

            std::ifstream input(file);
            std::string line;
            std::size_t line_number = 0;
            while (std::getline(input, line)) {
                ++line_number;
                for (std::string_view token : rule.forbidden_tokens) {
                    if (line.find(token) != std::string::npos) {
                        violations.push_back(violation{
                            .file = std::filesystem::relative(file, source_root),
                            .area = rule.area,
                            .token = token,
                            .line = line_number,
                        });
                    }
                }
            }
        }
    }
    return violations;
}

void require_no_violations(const std::vector<violation>& violations)
{
    if (violations.empty()) {
        return;
    }

    for (const violation& item : violations) {
        std::cerr << item.area << " boundary violation in " << item.file.string()
                  << ':' << item.line << " token `" << item.token << "`\n";
    }
    std::exit(1);
}

void require_source_headers_registered_in_cmake(const std::filesystem::path& source_root)
{
    const std::string cmake_text = read_file_text(source_root / "CMakeLists.txt");
    std::vector<std::string> missing_headers;

    for (const std::filesystem::path& file : header_files_under(source_root / "src")) {
        const std::string relative_path = generic_relative_path(source_root, file);
        if (cmake_text.find(relative_path) == std::string::npos) {
            missing_headers.push_back(relative_path);
        }
    }

    if (missing_headers.empty()) {
        return;
    }

    for (const std::string& header : missing_headers) {
        std::cerr << "CMake public header registration missing for " << header << '\n';
    }
    std::exit(1);
}

} // namespace

int main()
{
    const std::filesystem::path source_root{QUIZ_VULKAN_SOURCE_ROOT};
    const std::vector<boundary_rule> rules{
        boundary_rule{
            .area = "ui-layout-render",
            .roots = {
                "src/core/ui",
                "src/core/layout",
                "src/render",
            },
            .excluded_files = {},
            .forbidden_tokens = {
                "#include \"app/",
                "#include <app/",
                "#include \"audio/",
                "#include <audio/",
                "#include \"core/domain/",
                "#include <core/domain/",
                "#include \"core/input/",
                "#include <core/input/",
                "#include \"platform/",
                "#include <platform/",
                "domain::",
                "platform::",
            },
        },
        boundary_rule{
            .area = "ui-renderer-to-vulkan-renderer",
            .roots = {
                "src/core/ui",
            },
            .excluded_files = {},
            .forbidden_tokens = {
                "#include \"render/vulkan/",
                "#include <render/vulkan/",
                "vulkan_backend::",
            },
        },
        boundary_rule{
            .area = "layout-placer-to-ui-renderer",
            .roots = {
                "src/core/layout",
            },
            .excluded_files = {},
            .forbidden_tokens = {
                "#include \"core/ui/",
                "#include <core/ui/",
                "#include \"render/",
                "#include <render/",
                "ui::",
                "vulkan_backend::",
            },
        },
        boundary_rule{
            .area = "scene-layout-data-to-upper-layers-renderer",
            .roots = {
                "src/core/scene",
            },
            .excluded_files = {},
            .forbidden_tokens = {
                "#include \"app/",
                "#include <app/",
                "#include \"audio/",
                "#include <audio/",
                "#include \"assets/",
                "#include <assets/",
                "#include \"core/domain/",
                "#include <core/domain/",
                "#include \"core/input/",
                "#include <core/input/",
                "#include \"core/ui/",
                "#include <core/ui/",
                "#include \"render/",
                "#include <render/",
                "#include \"platform/",
                "#include <platform/",
                "app::",
                "audio::",
                "assets::",
                "domain::",
                "input::",
                "ui::",
                "render::",
                "platform::",
                "vulkan_backend::",
            },
        },
        boundary_rule{
            .area = "vulkan-renderer-to-scene-ui-app",
            .roots = {
                "src/render/vulkan",
            },
            .excluded_files = {},
            .forbidden_tokens = {
                "#include \"app/",
                "#include <app/",
                "#include \"core/domain/",
                "#include <core/domain/",
                "#include \"core/input/",
                "#include <core/input/",
                "#include \"core/layout/",
                "#include <core/layout/",
                "#include \"core/scene/",
                "#include <core/scene/",
                "#include \"core/ui/",
                "#include <core/ui/",
                "domain::",
                "scene::",
                "ui::",
            },
        },
        boundary_rule{
            .area = "domain-input-audio",
            .roots = {
                "src/core/domain",
                "src/core/input",
                "src/audio",
            },
            .excluded_files = {},
            .forbidden_tokens = {
                "#include \"core/ui/",
                "#include <core/ui/",
                "#include \"render/",
                "#include <render/",
                "render::",
                "vulkan_backend::",
            },
        },
        boundary_rule{
            .area = "input-core-to-app-domain-ui-render",
            .roots = {
                "src/core/input",
            },
            .excluded_files = {},
            .forbidden_tokens = {
                "#include \"app/",
                "#include <app/",
                "#include \"audio/",
                "#include <audio/",
                "#include \"core/domain/",
                "#include <core/domain/",
                "#include \"core/layout/",
                "#include <core/layout/",
                "#include \"core/scene/",
                "#include <core/scene/",
                "#include \"core/ui/",
                "#include <core/ui/",
                "#include \"core/persistence/",
                "#include <core/persistence/",
                "#include \"core/theme/",
                "#include <core/theme/",
                "#include \"core/workers/",
                "#include <core/workers/",
                "#include \"render/",
                "#include <render/",
                "#include \"assets/",
                "#include <assets/",
                "app::",
                "domain::",
                "layout::",
                "scene::",
                "ui::",
                "render::",
                "vulkan_backend::",
                "audio::",
                "assets::",
                "persistence::",
                "theme::",
                "workers::",
            },
        },
        boundary_rule{
            .area = "asset-core-to-upper-layers",
            .roots = {
                "src/assets",
            },
            .excluded_files = {
                "src/assets/deck_source_asset_adapter.cpp",
                "src/assets/deck_source_asset_adapter.h",
            },
            .forbidden_tokens = {
                "#include \"app/",
                "#include <app/",
                "#include \"audio/",
                "#include <audio/",
                "#include \"core/domain/",
                "#include <core/domain/",
                "#include \"core/input/",
                "#include <core/input/",
                "#include \"core/layout/",
                "#include <core/layout/",
                "#include \"core/scene/",
                "#include <core/scene/",
                "#include \"core/ui/",
                "#include <core/ui/",
                "#include \"render/",
                "#include <render/",
                "#include \"platform/",
                "#include <platform/",
                "domain::",
                "scene::",
                "ui::",
                "render::",
                "vulkan_backend::",
            },
        },
        boundary_rule{
            .area = "text-engine-core-to-upper-layers",
            .roots = {
                "src/render/text",
            },
            .excluded_files = {
                "src/render/text/scene_text_metrics_adapter.h",
            },
            .forbidden_tokens = {
                "#include \"app/",
                "#include <app/",
                "#include \"audio/",
                "#include <audio/",
                "#include \"core/domain/",
                "#include <core/domain/",
                "#include \"core/input/",
                "#include <core/input/",
                "#include \"core/layout/",
                "#include <core/layout/",
                "#include \"core/scene/",
                "#include <core/scene/",
                "#include \"core/ui/",
                "#include <core/ui/",
                "#include \"platform/",
                "#include <platform/",
                "domain::",
                "scene::",
                "ui::",
                "vulkan_backend::",
            },
        },
        boundary_rule{
            .area = "image-engine-core-to-upper-layers",
            .roots = {
                "src/render/image",
            },
            .excluded_files = {},
            .forbidden_tokens = {
                "#include \"app/",
                "#include <app/",
                "#include \"audio/",
                "#include <audio/",
                "#include \"assets/",
                "#include <assets/",
                "#include \"core/domain/",
                "#include <core/domain/",
                "#include \"core/input/",
                "#include <core/input/",
                "#include \"core/layout/",
                "#include <core/layout/",
                "#include \"core/scene/",
                "#include <core/scene/",
                "#include \"core/ui/",
                "#include <core/ui/",
                "#include \"platform/",
                "#include <platform/",
                "#include \"render/vulkan/",
                "#include <render/vulkan/",
                "app::",
                "audio::",
                "assets::",
                "domain::",
                "input::",
                "layout::",
                "scene::",
                "ui::",
                "platform::",
                "vulkan_backend::",
            },
        },
        boundary_rule{
            .area = "source-to-host-external-paths",
            .roots = {
                "src",
            },
            .excluded_files = {},
            .forbidden_tokens = {
                "/mnt/c/aa",
                "C:\\aa",
                "C:/aa",
                "build/external/lib/cpp/desktop",
            },
        },
    };

    std::vector<violation> violations;
    for (const boundary_rule& rule : rules) {
        std::vector<violation> rule_violations = scan_rule(source_root, rule);
        violations.insert(violations.end(), rule_violations.begin(), rule_violations.end());
    }

    require_no_violations(violations);
    require_source_headers_registered_in_cmake(source_root);
    return 0;
}
