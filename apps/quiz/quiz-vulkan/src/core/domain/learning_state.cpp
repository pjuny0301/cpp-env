#include "learning_state.hpp"

#include "quiz_model.hpp"

#include <utility>

namespace quiz_vulkan::domain {
namespace {

std::int64_t next_review_interval_ms(
    const question_learning& learning,
    const learning_update_rules& rules)
{
    std::int64_t interval = rules.known_review_interval_ms;
    const int review_count = learning.review_count < 0 ? 0 : learning.review_count;
    const int multiplier = rules.review_interval_multiplier < 1 ? 1 : rules.review_interval_multiplier;

    for (int index = 0; index < review_count && index < 16; ++index) {
        interval *= multiplier;
    }

    return interval;
}

void schedule_known_review(
    question_learning& learning,
    std::int64_t updated_at_ms,
    const learning_update_rules& rules)
{
    learning.known_at_ms = learning.known_at_ms == 0 ? updated_at_ms : learning.known_at_ms;
    learning.due_at_ms = updated_at_ms + next_review_interval_ms(learning, rules);
}

void clear_known_review(question_learning& learning)
{
    learning.known_at_ms = 0;
    learning.due_at_ms = 0;
    learning.review_count = 0;
}

}  // namespace

std::string_view to_string(learning_state state)
{
    switch (state) {
        case learning_state::learning:
            return "learning";
        case learning_state::known:
            return "known";
        case learning_state::unknown:
            return "unknown";
    }

    return "learning";
}

std::optional<learning_state> parse_learning_state(std::string_view value)
{
    const std::string normalized = normalize_answer_text(value);

    if (normalized == "learning") {
        return learning_state::learning;
    }
    if (normalized == "known") {
        return learning_state::known;
    }
    if (normalized == "unknown") {
        return learning_state::unknown;
    }

    return std::nullopt;
}

const question_learning* find_question_learning(
    const learning_state_map& learning_by_question_id,
    std::string_view question_id)
{
    const auto match = learning_by_question_id.find(std::string(question_id));
    return match == learning_by_question_id.end() ? nullptr : &match->second;
}

question_learning& get_or_create_question_learning(
    learning_state_map& learning_by_question_id,
    std::string question_id)
{
    return learning_by_question_id[std::move(question_id)];
}

learning_state state_for_question(
    const learning_state_map& learning_by_question_id,
    std::string_view question_id)
{
    const question_learning* learning = find_question_learning(learning_by_question_id, question_id);
    return learning == nullptr ? learning_state::learning : learning->state;
}

bool is_question_known(
    const learning_state_map& learning_by_question_id,
    std::string_view question_id)
{
    return state_for_question(learning_by_question_id, question_id) == learning_state::known;
}

bool is_question_due_for_review(const question_learning& learning, std::int64_t now_ms)
{
    return learning.state == learning_state::known
        && learning.due_at_ms > 0
        && now_ms >= learning.due_at_ms;
}

bool is_question_due_for_review(
    const learning_state_map& learning_by_question_id,
    std::string_view question_id,
    std::int64_t now_ms)
{
    const question_learning* learning = find_question_learning(learning_by_question_id, question_id);
    return learning != nullptr && is_question_due_for_review(*learning, now_ms);
}

void record_correct_answer(
    question_learning& learning,
    std::int64_t updated_at_ms,
    const learning_update_rules& rules)
{
    learning.correct_streak += 1;
    learning.wrong_count = 0;
    learning.updated_at_ms = updated_at_ms;

    if (learning.state == learning_state::known) {
        learning.review_count += 1;
        schedule_known_review(learning, updated_at_ms, rules);
        return;
    }

    if ((learning.state == learning_state::learning || learning.state == learning_state::unknown)
        && learning.correct_streak >= rules.known_threshold) {
        learning.state = learning_state::known;
        learning.review_count = 0;
        schedule_known_review(learning, updated_at_ms, rules);
    }
}

void record_wrong_answer(
    question_learning& learning,
    std::int64_t updated_at_ms,
    const learning_update_rules& rules)
{
    learning.correct_streak = 0;
    learning.wrong_count += 1;
    learning.updated_at_ms = updated_at_ms;

    if (learning.state == learning_state::known && learning.wrong_count >= rules.wrong_release) {
        learning.state = learning_state::learning;
        clear_known_review(learning);
    }
}

void mark_question_unknown(question_learning& learning, std::int64_t updated_at_ms)
{
    learning.correct_streak = 0;
    learning.wrong_count = 0;
    learning.state = learning_state::unknown;
    clear_known_review(learning);
    learning.updated_at_ms = updated_at_ms;
}

}  // namespace quiz_vulkan::domain
