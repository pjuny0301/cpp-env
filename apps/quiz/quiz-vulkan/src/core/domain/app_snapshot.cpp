#include "app_snapshot.hpp"

#include <algorithm>
#include <utility>

namespace quiz_vulkan::domain {
namespace {

const deck* find_deck(const std::vector<deck>& decks, const std::optional<std::string>& deck_id)
{
    if (!deck_id.has_value()) {
        return decks.empty() ? nullptr : &decks.front();
    }

    const auto match = std::find_if(
        decks.begin(),
        decks.end(),
        [&deck_id](const deck& candidate_deck) {
            return candidate_deck.id == *deck_id;
        });

    return match == decks.end() ? nullptr : &(*match);
}

app_screen screen_for_snapshot(const app_snapshot& snapshot)
{
    if (snapshot.error_message.has_value()) {
        return app_screen::error;
    }

    if (snapshot.active_session.has_value()) {
        if (snapshot.active_session->feedback.has_value()) {
            return app_screen::feedback;
        }

        return snapshot.active_session->completed ? app_screen::completed : app_screen::quiz;
    }

    return snapshot.selected_deck_id.has_value() ? app_screen::day_select : app_screen::deck_select;
}

}  // namespace

std::string_view to_string(app_screen screen)
{
    switch (screen) {
        case app_screen::loading:
            return "loading";
        case app_screen::deck_select:
            return "deck_select";
        case app_screen::day_select:
            return "day_select";
        case app_screen::quiz:
            return "quiz";
        case app_screen::feedback:
            return "feedback";
        case app_screen::completed:
            return "completed";
        case app_screen::error:
            return "error";
    }

    return "deck_select";
}

learning_summary summarize_learning(
    const deck& quiz_deck,
    const learning_state_map& learning_by_question_id)
{
    learning_summary summary;

    for (const question* quiz_question : collect_questions(quiz_deck)) {
        summary.question_count += 1;

        switch (state_for_question(learning_by_question_id, quiz_question->id)) {
            case learning_state::learning:
                summary.learning_count += 1;
                break;
            case learning_state::known:
                summary.known_count += 1;
                break;
            case learning_state::unknown:
                summary.unknown_count += 1;
                break;
            case learning_state::wrong_note:
                summary.wrong_note_count += 1;
                break;
        }
    }

    return summary;
}

question_snapshot make_question_snapshot(
    const question& quiz_question,
    const learning_state_map& learning_by_question_id,
    bool reveal_correctness)
{
    question_snapshot snapshot;
    snapshot.question_id = quiz_question.id;
    snapshot.prompt = quiz_question.prompt;
    snapshot.long_text = quiz_question.long_text;
    snapshot.image_uri = quiz_question.image_uri;
    snapshot.type = quiz_question.type;
    snapshot.learning = state_for_question(learning_by_question_id, quiz_question.id);
    snapshot.options.reserve(quiz_question.options.size());

    for (const option& answer_option : quiz_question.options) {
        snapshot.options.push_back(option_snapshot{
            answer_option.text,
            reveal_correctness,
            reveal_correctness && answer_option.is_correct,
        });
    }

    return snapshot;
}

std::optional<session_snapshot> make_session_snapshot(
    const quiz_session& session,
    const deck& quiz_deck,
    const learning_state_map& learning_by_question_id)
{
    if (!session.started) {
        return std::nullopt;
    }

    session_snapshot snapshot;
    snapshot.mode = session.mode;
    snapshot.phase = phase_of(session);
    snapshot.current_index = session.current_index;
    snapshot.question_count = session.question_ids.size();
    snapshot.completed = session.completed;
    snapshot.feedback = session.pending_feedback;

    const question* quiz_question = current_question(session, quiz_deck);
    if (quiz_question != nullptr) {
        const bool reveal_correctness = session.pending_feedback.has_value()
            && session.pending_feedback->question_id == quiz_question->id;
        snapshot.current_question = make_question_snapshot(
            *quiz_question,
            learning_by_question_id,
            reveal_correctness);
    }

    return snapshot;
}

app_snapshot make_app_snapshot(
    const std::vector<deck>& decks,
    const std::optional<std::string>& selected_deck_id,
    const std::optional<std::string>& selected_day_id,
    const quiz_session* active_session,
    const learning_state_map& learning_by_question_id,
    std::unordered_map<std::string, std::string> settings,
    std::optional<std::string> error_message)
{
    app_snapshot snapshot;
    snapshot.decks = decks;
    snapshot.selected_deck_id = selected_deck_id;
    snapshot.selected_day_id = selected_day_id;
    snapshot.settings = std::move(settings);
    snapshot.error_message = std::move(error_message);

    const deck* selected_deck = find_deck(snapshot.decks, selected_deck_id);
    if (selected_deck != nullptr) {
        snapshot.learning = summarize_learning(*selected_deck, learning_by_question_id);

        if (active_session != nullptr) {
            snapshot.active_session = make_session_snapshot(
                *active_session,
                *selected_deck,
                learning_by_question_id);
        }
    }

    snapshot.screen = screen_for_snapshot(snapshot);
    return snapshot;
}

}  // namespace quiz_vulkan::domain
