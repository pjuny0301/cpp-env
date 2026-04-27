#pragma once

#include "learning_state.hpp"
#include "quiz_model.hpp"
#include "quiz_session.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace quiz_vulkan::domain {

enum class app_screen {
    loading,
    deck_select,
    day_select,
    quiz,
    feedback,
    completed,
    error,
};

struct option_snapshot {
    std::string text;
    bool reveal_correctness = false;
    bool is_correct = false;
};

struct question_snapshot {
    std::string question_id;
    std::string prompt;
    std::optional<std::string> long_text;
    std::optional<std::string> image_uri;
    question_type type = question_type::answer;
    learning_state learning = learning_state::learning;
    std::vector<option_snapshot> options;
};

struct learning_summary {
    std::size_t question_count = 0;
    std::size_t learning_count = 0;
    std::size_t known_count = 0;
    std::size_t unknown_count = 0;
};

struct session_snapshot {
    quiz_mode mode = quiz_mode::normal;
    quiz_session_phase phase = quiz_session_phase::idle;
    std::size_t current_index = 0;
    std::size_t question_count = 0;
    bool completed = false;
    std::optional<answer_record> feedback;
    std::optional<question_snapshot> current_question;
};

struct app_snapshot {
    app_screen screen = app_screen::deck_select;
    std::vector<deck> decks;
    std::optional<std::string> selected_deck_id;
    std::optional<std::string> selected_day_id;
    learning_summary learning;
    std::optional<session_snapshot> active_session;
    std::unordered_map<std::string, std::string> settings;
    std::optional<std::string> error_message;
};

std::string_view to_string(app_screen screen);

learning_summary summarize_learning(
    const deck& quiz_deck,
    const learning_state_map& learning_by_question_id);
question_snapshot make_question_snapshot(
    const question& quiz_question,
    const learning_state_map& learning_by_question_id,
    bool reveal_correctness = false);
std::optional<session_snapshot> make_session_snapshot(
    const quiz_session& session,
    const deck& quiz_deck,
    const learning_state_map& learning_by_question_id);
app_snapshot make_app_snapshot(
    const std::vector<deck>& decks,
    const std::optional<std::string>& selected_deck_id,
    const std::optional<std::string>& selected_day_id,
    const quiz_session* active_session,
    const learning_state_map& learning_by_question_id,
    std::unordered_map<std::string, std::string> settings = {},
    std::optional<std::string> error_message = std::nullopt);

}  // namespace quiz_vulkan::domain
