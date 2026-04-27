#pragma once

#include "core/domain/app_action.hpp"
#include "core/domain/app_snapshot.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiz_vulkan {

class app_state {
public:
    app_state() = default;
    explicit app_state(std::vector<domain::deck> decks);

    const std::vector<domain::deck>& decks() const;
    const std::optional<std::string>& selected_deck_id() const;
    const std::optional<std::string>& selected_day_id() const;
    const domain::learning_state_map& learning() const;
    const std::optional<domain::quiz_session>& active_session() const;

    void set_decks(std::vector<domain::deck> decks);
    void dispatch(const domain::app_action& action, std::int64_t now_ms = 0);
    domain::app_snapshot snapshot() const;

private:
    const domain::deck* selected_deck() const;
    void clear_active_session();
    void remember_answer(const domain::answer_record& record);
    void submit_record(std::optional<domain::answer_record> record);

    std::vector<domain::deck> decks_;
    std::optional<std::string> selected_deck_id_;
    std::optional<std::string> selected_day_id_;
    std::optional<domain::quiz_session> active_session_;
    domain::learning_state_map learning_by_question_id_;
    domain::previous_answer_map previous_answers_;
    std::unordered_map<std::string, std::string> settings_;
    std::optional<std::string> error_message_;
};

} // namespace quiz_vulkan
