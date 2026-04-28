#pragma once

#include "core/domain/quiz_model.hpp"

#include <string>
#include <vector>

namespace quiz_vulkan::domain {

struct deck_source_request {
    std::string uri;
    std::string cache_key;
    bool allow_demo_fallback = false;

    [[nodiscard]] bool valid() const noexcept
    {
        return !uri.empty() || allow_demo_fallback;
    }
};

enum class deck_source_status {
    loaded,
    empty_request,
    not_found,
    invalid_data,
    unsupported_source,
    unavailable,
};

struct deck_source_diagnostic {
    std::string uri;
    std::string message;
    std::size_t line = 0;
};

struct deck_source_result {
    deck_source_status status = deck_source_status::unavailable;
    std::string source_uri;
    std::vector<deck> decks;
    std::vector<deck_source_diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept
    {
        return status == deck_source_status::loaded && !decks.empty();
    }
};

class deck_source_provider_interface {
public:
    virtual ~deck_source_provider_interface() = default;

    virtual deck_source_result load_decks(const deck_source_request& request) = 0;
};

} // namespace quiz_vulkan::domain
