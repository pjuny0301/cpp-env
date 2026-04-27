#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace quiz_vulkan::domain {

enum class learning_state {
    learning,
    known,
    unknown,
    wrong_note,
};

struct question_learning {
    int correct_streak = 0;
    int wrong_count = 0;
    int review_count = 0;
    int skip_streak = 0;
    int wrong_note_correct_streak = 0;
    learning_state state = learning_state::learning;
    std::int64_t known_at_ms = 0;
    std::int64_t due_at_ms = 0;
    std::int64_t updated_at_ms = 0;
};

struct learning_update_rules {
    int known_threshold = 3;
    int wrong_release = 1;
    std::int64_t known_review_interval_ms = 86'400'000;
    int review_interval_multiplier = 2;
    bool auto_promote_correct_answers = false;
    int swipe_known_threshold = 3;
    bool wrong_note_enabled = false;
    int wrong_note_threshold = 1;
    int wrong_note_release_correct_streak = 3;
};

using learning_state_map = std::unordered_map<std::string, question_learning>;

std::string_view to_string(learning_state state);
std::optional<learning_state> parse_learning_state(std::string_view value);

const question_learning* find_question_learning(
    const learning_state_map& learning_by_question_id,
    std::string_view question_id);
question_learning& get_or_create_question_learning(
    learning_state_map& learning_by_question_id,
    std::string question_id);

learning_state state_for_question(
    const learning_state_map& learning_by_question_id,
    std::string_view question_id);
bool is_question_known(
    const learning_state_map& learning_by_question_id,
    std::string_view question_id);
bool is_question_due_for_review(const question_learning& learning, std::int64_t now_ms);
bool is_question_due_for_review(
    const learning_state_map& learning_by_question_id,
    std::string_view question_id,
    std::int64_t now_ms);

void record_correct_answer(
    question_learning& learning,
    std::int64_t updated_at_ms,
    const learning_update_rules& rules = {});
void record_wrong_answer(
    question_learning& learning,
    std::int64_t updated_at_ms,
    const learning_update_rules& rules = {});
void record_skipped_question(question_learning& learning, std::int64_t updated_at_ms);
bool can_offer_known_by_swipe(
    const question_learning& learning,
    const learning_update_rules& rules = {});
void mark_question_known(
    question_learning& learning,
    std::int64_t updated_at_ms,
    const learning_update_rules& rules = {});
void mark_question_unknown(question_learning& learning, std::int64_t updated_at_ms);

}  // namespace quiz_vulkan::domain
