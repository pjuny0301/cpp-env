#include "quiz_session.hpp"

#include <algorithm>
#include <random>

namespace quiz_vulkan::domain {
namespace {

bool has_pending_feedback(const quiz_session& session)
{
    return session.pending_feedback.has_value();
}

void advance_current_question(quiz_session& session)
{
    if (!has_current_question(session)) {
        session.completed = true;
        return;
    }

    session.current_index += 1;
    session.completed = session.current_index >= session.question_ids.size();
}

answer_record make_record(
    const quiz_session& session,
    answer_outcome outcome,
    std::int64_t answered_at_ms)
{
    answer_record record;
    if (has_current_question(session)) {
        record.question_id = session.question_ids[session.current_index];
    }

    record.outcome = outcome;
    record.answered_at_ms = answered_at_ms;
    return record;
}

bool should_include_question(
    const question& quiz_question,
    const quiz_session_options& options,
    const learning_state_map& learning_by_question_id,
    const previous_answer_map& previous_answers)
{
    const learning_state state = state_for_question(learning_by_question_id, quiz_question.id);

    if (options.mode == quiz_mode::known) {
        return state == learning_state::known;
    }

    if (options.mode == quiz_mode::wrong_only) {
        const auto previous_answer = previous_answers.find(quiz_question.id);
        return previous_answer != previous_answers.end() && previous_answer->second == answer_outcome::incorrect;
    }

    return state != learning_state::known
        || (options.include_due_known
            && is_question_due_for_review(learning_by_question_id, quiz_question.id, options.now_ms));
}

}  // namespace

std::string_view to_string(quiz_mode mode)
{
    switch (mode) {
        case quiz_mode::normal:
            return "normal";
        case quiz_mode::random:
            return "random";
        case quiz_mode::wrong_only:
            return "wrong_only";
        case quiz_mode::known:
            return "known";
    }

    return "normal";
}

std::optional<quiz_mode> parse_quiz_mode(std::string_view value)
{
    const std::string normalized = normalize_answer_text(value);

    if (normalized == "normal") {
        return quiz_mode::normal;
    }
    if (normalized == "random") {
        return quiz_mode::random;
    }
    if (normalized == "wrong_only" || normalized == "wrong only") {
        return quiz_mode::wrong_only;
    }
    if (normalized == "known") {
        return quiz_mode::known;
    }

    return std::nullopt;
}

std::string_view to_string(answer_outcome outcome)
{
    switch (outcome) {
        case answer_outcome::unanswered:
            return "unanswered";
        case answer_outcome::correct:
            return "correct";
        case answer_outcome::incorrect:
            return "incorrect";
        case answer_outcome::skipped:
            return "skipped";
        case answer_outcome::marked_known:
            return "marked_known";
        case answer_outcome::marked_unknown:
            return "marked_unknown";
    }

    return "unanswered";
}

std::optional<answer_outcome> parse_answer_outcome(std::string_view value)
{
    const std::string normalized = normalize_answer_text(value);

    if (normalized == "unanswered") {
        return answer_outcome::unanswered;
    }
    if (normalized == "correct") {
        return answer_outcome::correct;
    }
    if (normalized == "incorrect" || normalized == "wrong") {
        return answer_outcome::incorrect;
    }
    if (normalized == "skipped") {
        return answer_outcome::skipped;
    }
    if (normalized == "marked_known" || normalized == "marked known") {
        return answer_outcome::marked_known;
    }
    if (normalized == "marked_unknown" || normalized == "marked unknown") {
        return answer_outcome::marked_unknown;
    }

    return std::nullopt;
}

std::string_view to_string(quiz_session_phase phase)
{
    switch (phase) {
        case quiz_session_phase::idle:
            return "idle";
        case quiz_session_phase::active:
            return "active";
        case quiz_session_phase::feedback:
            return "feedback";
        case quiz_session_phase::completed:
            return "completed";
    }

    return "idle";
}

quiz_session_phase phase_of(const quiz_session& session)
{
    if (!session.started) {
        return quiz_session_phase::idle;
    }

    if (session.completed) {
        return quiz_session_phase::completed;
    }

    if (session.pending_feedback.has_value()) {
        return quiz_session_phase::feedback;
    }

    return quiz_session_phase::active;
}

bool can_accept_answer(const quiz_session& session)
{
    return phase_of(session) == quiz_session_phase::active && has_current_question(session);
}

bool can_continue_feedback(const quiz_session& session)
{
    return phase_of(session) == quiz_session_phase::feedback;
}

quiz_session start_quiz_session(
    const deck& quiz_deck,
    const learning_state_map& learning_by_question_id,
    const previous_answer_map& previous_answers,
    const quiz_session_options& options)
{
    quiz_session session;
    session.mode = options.mode;
    session.started = true;

    const std::vector<const question*> source_questions = collect_questions(quiz_deck, options.day_id);
    session.question_ids.reserve(source_questions.size());

    for (const question* quiz_question : source_questions) {
        if (should_include_question(*quiz_question, options, learning_by_question_id, previous_answers)) {
            session.question_ids.push_back(quiz_question->id);
        }
    }

    if (options.shuffle || options.mode == quiz_mode::random) {
        std::mt19937 random_engine(options.random_seed);
        std::shuffle(session.question_ids.begin(), session.question_ids.end(), random_engine);
    }

    session.completed = session.question_ids.empty();
    return session;
}

bool has_current_question(const quiz_session& session)
{
    return session.started && !session.completed && session.current_index < session.question_ids.size();
}

const question* current_question(const quiz_session& session, const deck& quiz_deck)
{
    if (!has_current_question(session)) {
        return nullptr;
    }

    return find_question(quiz_deck, session.question_ids[session.current_index]);
}

std::optional<answer_record> submit_option_answer(
    quiz_session& session,
    const deck& quiz_deck,
    std::size_t option_index,
    std::int64_t answered_at_ms)
{
    if (!has_current_question(session) || has_pending_feedback(session)) {
        return std::nullopt;
    }

    const question* quiz_question = current_question(session, quiz_deck);
    if (quiz_question == nullptr) {
        return std::nullopt;
    }

    const bool is_correct = option_index < quiz_question->options.size()
        && quiz_question->options[option_index].is_correct;

    answer_record record = make_record(
        session,
        is_correct ? answer_outcome::correct : answer_outcome::incorrect,
        answered_at_ms);
    record.selected_option_indexes.push_back(option_index);

    session.answer_records.push_back(record);
    session.pending_feedback = record;
    return record;
}

std::optional<answer_record> submit_text_answer(
    quiz_session& session,
    const deck& quiz_deck,
    std::string_view submitted_text,
    std::int64_t answered_at_ms)
{
    if (!has_current_question(session) || has_pending_feedback(session)) {
        return std::nullopt;
    }

    const question* quiz_question = current_question(session, quiz_deck);
    if (quiz_question == nullptr) {
        return std::nullopt;
    }

    const bool expects_multiple_answers =
        quiz_question->type == question_type::multi_answer
        || quiz_question->type == question_type::multi_blank;
    const bool is_correct = expects_multiple_answers
        ? multi_text_answer_matches(*quiz_question, submitted_text)
        : text_answer_matches(*quiz_question, submitted_text);

    answer_record record = make_record(
        session,
        is_correct ? answer_outcome::correct : answer_outcome::incorrect,
        answered_at_ms);

    if (expects_multiple_answers) {
        record.submitted_text_answers = split_multi_answer_text(submitted_text);
    } else {
        record.submitted_text_answers.push_back(normalize_answer_text(submitted_text));
    }

    session.answer_records.push_back(record);
    session.pending_feedback = record;
    return record;
}

std::optional<answer_record> submit_multiselect_answer(
    quiz_session& session,
    const deck& quiz_deck,
    const std::vector<std::size_t>& selected_option_indexes,
    std::int64_t answered_at_ms)
{
    if (!has_current_question(session) || has_pending_feedback(session)) {
        return std::nullopt;
    }

    const question* quiz_question = current_question(session, quiz_deck);
    if (quiz_question == nullptr) {
        return std::nullopt;
    }

    const bool is_correct = multiselect_matches(*quiz_question, selected_option_indexes);

    answer_record record = make_record(
        session,
        is_correct ? answer_outcome::correct : answer_outcome::incorrect,
        answered_at_ms);
    record.selected_option_indexes = selected_option_indexes;

    session.answer_records.push_back(record);
    session.pending_feedback = record;
    return record;
}

std::optional<answer_record> skip_current_question(
    quiz_session& session,
    std::int64_t answered_at_ms)
{
    if (!has_current_question(session) || has_pending_feedback(session)) {
        return std::nullopt;
    }

    answer_record record = make_record(session, answer_outcome::skipped, answered_at_ms);
    session.answer_records.push_back(record);
    advance_current_question(session);
    return record;
}

std::optional<answer_record> mark_current_question_known(
    quiz_session& session,
    learning_state_map& learning_by_question_id,
    std::int64_t answered_at_ms,
    const learning_update_rules& rules)
{
    if (!has_current_question(session) || has_pending_feedback(session)) {
        return std::nullopt;
    }

    answer_record record = make_record(session, answer_outcome::marked_known, answered_at_ms);
    question_learning& learning = get_or_create_question_learning(
        learning_by_question_id,
        record.question_id);
    mark_question_known(learning, answered_at_ms, rules);

    session.answer_records.push_back(record);
    advance_current_question(session);
    return record;
}

std::optional<answer_record> mark_current_question_unknown(
    quiz_session& session,
    learning_state_map& learning_by_question_id,
    std::int64_t answered_at_ms)
{
    if (!has_current_question(session) || has_pending_feedback(session)) {
        return std::nullopt;
    }

    answer_record record = make_record(session, answer_outcome::marked_unknown, answered_at_ms);
    question_learning& learning = get_or_create_question_learning(
        learning_by_question_id,
        record.question_id);
    mark_question_unknown(learning, answered_at_ms);

    session.answer_records.push_back(record);
    advance_current_question(session);
    return record;
}

void go_to_previous_question(quiz_session& session)
{
    if (!session.started || session.question_ids.empty()) {
        return;
    }

    session.pending_feedback.reset();

    if (session.completed) {
        session.completed = false;
        session.current_index = session.question_ids.size() - 1;
        return;
    }

    if (session.current_index > 0) {
        session.current_index -= 1;
    }
}

void continue_after_feedback(quiz_session& session)
{
    if (!can_continue_feedback(session)) {
        return;
    }

    session.pending_feedback.reset();
    advance_current_question(session);
}

void apply_answer_to_learning(
    learning_state_map& learning_by_question_id,
    const answer_record& record,
    const learning_update_rules& rules)
{
    if (record.question_id.empty()) {
        return;
    }

    question_learning& learning = get_or_create_question_learning(
        learning_by_question_id,
        record.question_id);

    switch (record.outcome) {
        case answer_outcome::correct:
            record_correct_answer(learning, record.answered_at_ms, rules);
            return;
        case answer_outcome::incorrect:
            record_wrong_answer(learning, record.answered_at_ms, rules);
            return;
        case answer_outcome::skipped:
            record_skipped_question(learning, record.answered_at_ms);
            return;
        case answer_outcome::marked_known:
            mark_question_known(learning, record.answered_at_ms, rules);
            return;
        case answer_outcome::marked_unknown:
            mark_question_unknown(learning, record.answered_at_ms);
            return;
        case answer_outcome::unanswered:
            return;
    }
}

}  // namespace quiz_vulkan::domain
