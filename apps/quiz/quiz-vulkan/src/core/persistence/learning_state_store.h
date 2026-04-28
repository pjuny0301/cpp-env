#pragma once

#include "core/domain/learning_state.hpp"
#include "core/domain/quiz_session.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiz_vulkan::persistence {

using settings_map = std::unordered_map<std::string, std::string>;

struct learning_state_document {
    domain::learning_state_map learning_by_question_id;
    domain::previous_answer_map previous_answers;
    settings_map settings;
    std::vector<domain::answer_record> answer_history;
    std::uint64_t revision = 0;

    [[nodiscard]] bool empty() const noexcept
    {
        return learning_by_question_id.empty()
            && previous_answers.empty()
            && settings.empty()
            && answer_history.empty();
    }
};

enum class persistence_status {
    ok,
    not_found,
    unavailable,
    invalid_data,
    conflict,
};

struct learning_state_load_result {
    persistence_status status = persistence_status::unavailable;
    learning_state_document document;
    std::string diagnostic;

    [[nodiscard]] bool ok() const noexcept
    {
        return status == persistence_status::ok;
    }
};

struct learning_state_save_result {
    persistence_status status = persistence_status::unavailable;
    std::uint64_t revision = 0;
    std::string diagnostic;

    [[nodiscard]] bool ok() const noexcept
    {
        return status == persistence_status::ok;
    }
};

class learning_state_store_interface {
public:
    virtual ~learning_state_store_interface() = default;

    virtual learning_state_load_result load_learning_state(const std::string& deck_id) = 0;
    virtual learning_state_save_result save_learning_state(
        const std::string& deck_id,
        const learning_state_document& document) = 0;
};

} // namespace quiz_vulkan::persistence
