#pragma once

#include "quiz_model.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quiz_vulkan::domain {

struct deck_artifact_parse_error {
    std::size_t line = 0;
    std::string message;
};

struct deck_artifact_load_result {
    std::optional<deck> value;
    std::vector<deck_artifact_parse_error> errors;

    [[nodiscard]] bool ok() const noexcept
    {
        return value.has_value() && errors.empty();
    }
};

deck_artifact_load_result parse_deck_artifact(std::string_view artifact_text);
deck_artifact_load_result load_deck_artifact_file(const std::filesystem::path& artifact_path);

}  // namespace quiz_vulkan::domain
