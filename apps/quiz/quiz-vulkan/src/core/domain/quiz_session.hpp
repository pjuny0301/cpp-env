#pragma once

#include "learning_state.hpp"
#include "quiz_model.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace quiz_vulkan::domain {

enum class quiz_mode {
    normal,
    random,
    wrong_only,
    known,
};

enum class answer_outcome {
    unanswered,
    correct,
    incorrect,
    skipped,
    marked_unknown,
};

enum class quiz_session_phase {
    idle,
    active,
    feedback,
    completed,
};

struct answer_record {
    std::string question_id;
    answer_outcome outcome = answer_outcome::unanswered;
    std::vector<std::size_t> selected_option_indexes;
    std::vector<std::string> submitted_text_answers;
    std::int64_t answered_at_ms = 0;
};

using previous_answer_map = std::unordered_map<std::string, answer_outcome>;

struct quiz_session_options {
    std::optional<std::string> day_id;
    quiz_mode mode = quiz_mode::normal;
    unsigned int random_seed = 0;
    bool shuffle = false;
    bool include_due_known = true;
    std::int64_t now_ms = 0;
};

struct quiz_session {
    std::vector<std::string> question_ids;
    std::size_t current_index = 0;
    quiz_mode mode = quiz_mode::normal;
    std::vector<answer_record> answer_records;
    std::optional<answer_record> pending_feedback;
    bool started = false;
    bool completed = false;
};

std::string_view to_string(quiz_mode mode);
std::optional<quiz_mode> parse_quiz_mode(std::string_view value);

std::string_view to_string(answer_outcome outcome);
std::optional<answer_outcome> parse_answer_outcome(std::string_view value);
std::string_view to_string(quiz_session_phase phase);
quiz_session_phase phase_of(const quiz_session& session);
bool can_accept_answer(const quiz_session& session);
bool can_continue_feedback(const quiz_session& session);

quiz_session start_quiz_session(
    const deck& quiz_deck,
    const learning_state_map& learning_by_question_id,
    const previous_answer_map& previous_answers,
    const quiz_session_options& options = {});

bool has_current_question(const quiz_session& session);
const question* current_question(const quiz_session& session, const deck& quiz_deck);

std::optional<answer_record> submit_option_answer(
    quiz_session& session,
    const deck& quiz_deck,
    std::size_t option_index,
    std::int64_t answered_at_ms);
std::optional<answer_record> submit_text_answer(
    quiz_session& session,
    const deck& quiz_deck,
    std::string_view submitted_text,
    std::int64_t answered_at_ms);
std::optional<answer_record> submit_multiselect_answer(
    quiz_session& session,
    const deck& quiz_deck,
    const std::vector<std::size_t>& selected_option_indexes,
    std::int64_t answered_at_ms);

std::optional<answer_record> skip_current_question(
    quiz_session& session,
    std::int64_t answered_at_ms);
std::optional<answer_record> mark_current_question_unknown(
    quiz_session& session,
    learning_state_map& learning_by_question_id,
    std::int64_t answered_at_ms);

void continue_after_feedback(quiz_session& session);
void apply_answer_to_learning(
    learning_state_map& learning_by_question_id,
    const answer_record& record,
    const learning_update_rules& rules = {});

}  // namespace quiz_vulkan::domain
