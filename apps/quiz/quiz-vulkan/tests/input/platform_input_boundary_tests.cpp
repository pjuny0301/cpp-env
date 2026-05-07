#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct boundary_rule {
    std::string_view area;
    std::vector<std::filesystem::path> roots;
    std::vector<std::string_view> forbidden_tokens;
};

struct violation {
    std::filesystem::path file;
    std::string_view area;
    std::string_view token;
    std::size_t line = 0;
};

std::filesystem::path source_root()
{
    const std::filesystem::path test_file = std::filesystem::path{__FILE__};
    if (test_file.is_absolute()) {
        const std::filesystem::path from_file =
            std::filesystem::weakly_canonical(test_file).parent_path().parent_path().parent_path();
        if (std::filesystem::exists(from_file / "src/core/input")) {
            return from_file;
        }
    }

    for (std::filesystem::path cursor = std::filesystem::current_path(); !cursor.empty();
         cursor = cursor.parent_path()) {
        const std::filesystem::path direct_candidate = cursor / "src/core/input";
        if (std::filesystem::exists(direct_candidate)) {
            return cursor;
        }

        const std::filesystem::path workspace_candidate = cursor / "apps/quiz/quiz-vulkan";
        if (std::filesystem::exists(workspace_candidate / "src/core/input")) {
            return workspace_candidate;
        }

        if (cursor == cursor.root_path()) {
            break;
        }
    }

    return test_file.parent_path().parent_path().parent_path();
}

bool source_extension(const std::filesystem::path& path)
{
    const std::string extension = path.extension().string();
    return extension == ".h" || extension == ".hpp" || extension == ".cpp";
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

std::vector<violation> scan_rule(const std::filesystem::path& root, const boundary_rule& rule)
{
    std::vector<violation> violations;
    for (const std::filesystem::path& relative_root : rule.roots) {
        for (const std::filesystem::path& file : source_files_under(root / relative_root)) {
            std::ifstream input(file);
            std::string line;
            std::size_t line_number = 0;
            while (std::getline(input, line)) {
                ++line_number;
                for (std::string_view token : rule.forbidden_tokens) {
                    if (line.find(token) != std::string::npos) {
                        violations.push_back(violation{
                            .file = std::filesystem::relative(file, root),
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

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "platform_input_boundary_tests failed: " << message << '\n';
    std::exit(1);
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

void test_platform_translator_adapter_stay_input_owned()
{
    const std::filesystem::path root = source_root();
    require(std::filesystem::exists(root / "src/core/input"), "source root resolves input sources");

    const boundary_rule rule{
        .area = "platform-input-translator-adapter",
        .roots = {
            "src/core/input",
        },
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
            "input_hit_test",
            "hit_test",
            "pick_result",
            "renderer",
            "app_action",
            "domain_action",
            "quiz_vulkan::app",
            "quiz_vulkan::domain",
        },
    };

    require_no_violations(scan_rule(root, rule));
}

} // namespace

int main()
{
    test_platform_translator_adapter_stay_input_owned();

    std::cout << "platform_input_boundary_tests passed\n";
    return 0;
}
