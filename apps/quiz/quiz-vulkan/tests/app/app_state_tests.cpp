#include "app/app_state.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "app_state_tests failed: " << message << '\n';
    std::exit(1);
}

quiz_vulkan::domain::question make_answer_question(
    std::string id,
    std::string prompt,
    std::string correct_answer,
    std::string wrong_answer)
{
    using namespace quiz_vulkan::domain;

    question quiz_question;
    quiz_question.id = std::move(id);
    quiz_question.prompt = std::move(prompt);
    quiz_question.type = question_type::answer;
    quiz_question.options = {
        option{std::move(correct_answer), true},
        option{std::move(wrong_answer), false},
    };
    return quiz_question;
}

quiz_vulkan::domain::deck make_test_deck()
{
    using namespace quiz_vulkan::domain;

    day first_day;
    first_day.id = "day1";
    first_day.title = "Day 1";
    first_day.questions = {
        make_answer_question("q1", "2 + 2", "4", "5"),
        make_answer_question("q2", "3 + 3", "6", "7"),
    };

    day second_day;
    second_day.id = "day2";
    second_day.title = "Day 2";
    second_day.questions = {
        make_answer_question("q3", "5 + 5", "10", "9"),
    };

    deck quiz_deck;
    quiz_deck.id = "math";
    quiz_deck.title = "Math";
    quiz_deck.days = {std::move(first_day), std::move(second_day)};
    return quiz_deck;
}

void return_to_first_question(quiz_vulkan::app_state& state, std::int64_t now_ms)
{
    state.dispatch(quiz_vulkan::domain::make_continue_after_feedback_action(), now_ms);
    state.dispatch(quiz_vulkan::domain::make_previous_question_action(), now_ms);
}

void start_first_day_quiz(quiz_vulkan::app_state& state, std::int64_t now_ms)
{
    state.dispatch(quiz_vulkan::domain::make_select_day_action("day1"));
    state.dispatch(quiz_vulkan::domain::make_start_quiz_action(quiz_vulkan::domain::quiz_mode::normal), now_ms);
}

void test_wrong_note_settings_take_effect()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});
    state.dispatch(domain::make_update_setting_action("wrong_note_enabled", "yes"));
    state.dispatch(domain::make_update_setting_action("wrong_note_threshold", "2"));
    state.dispatch(domain::make_update_setting_action("wrong_note_release_correct_streak", "2"));
    start_first_day_quiz(state, 100);

    state.dispatch(domain::make_submit_option_action(1), 110);
    require(state.learning().at("q1").state == domain::learning_state::learning, "wrong note threshold waits for second miss");

    return_to_first_question(state, 120);
    state.dispatch(domain::make_submit_option_action(1), 130);
    require(state.learning().at("q1").state == domain::learning_state::wrong_note, "wrong note threshold moves question after second miss");

    domain::app_snapshot snapshot = state.snapshot();
    require(snapshot.learning.wrong_note_count == 1, "wrong note setting updates learning summary");

    return_to_first_question(state, 140);
    state.dispatch(domain::make_submit_option_action(0), 150);
    require(state.learning().at("q1").state == domain::learning_state::wrong_note, "wrong note release waits for configured streak");

    return_to_first_question(state, 160);
    state.dispatch(domain::make_submit_option_action(0), 170);
    require(state.learning().at("q1").state == domain::learning_state::learning, "wrong note release streak restores learning state");
}

void test_invalid_setting_preserves_prior_rule()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});
    state.dispatch(domain::make_update_setting_action("wrong_note_enabled", "on"));
    state.dispatch(domain::make_update_setting_action("wrong_note_threshold", "2"));
    state.dispatch(domain::make_update_setting_action("wrong_note_threshold", "0"));

    domain::app_snapshot snapshot = state.snapshot();
    require(snapshot.error_message.has_value(), "invalid setting surfaces an error");
    require(*snapshot.error_message == "Invalid setting value for wrong_note_threshold: 0", "invalid setting error message is stable");

    start_first_day_quiz(state, 100);
    state.dispatch(domain::make_submit_option_action(1), 110);
    require(state.learning().at("q1").state == domain::learning_state::learning, "invalid setting keeps prior threshold after one miss");

    return_to_first_question(state, 120);
    state.dispatch(domain::make_submit_option_action(1), 130);
    require(state.learning().at("q1").state == domain::learning_state::wrong_note, "invalid setting keeps prior threshold after second miss");
}

void test_known_settings_update_mark_and_auto_promote()
{
    using namespace quiz_vulkan;

    app_state marked_state({make_test_deck()});
    marked_state.dispatch(domain::make_update_setting_action("known_threshold", "5"));
    marked_state.dispatch(domain::make_update_setting_action("known_review_interval_ms", "2500"));
    start_first_day_quiz(marked_state, 200);
    marked_state.dispatch(domain::make_mark_question_known_action(), 210);

    require(marked_state.learning().at("q1").state == domain::learning_state::known, "known setting still marks question known");
    require(marked_state.learning().at("q1").correct_streak == 5, "known threshold setting changes mark known streak");
    require(marked_state.learning().at("q1").due_at_ms == 2710, "known interval setting changes review schedule");

    app_state promoted_state({make_test_deck()});
    promoted_state.dispatch(domain::make_update_setting_action("known_threshold", "2"));
    promoted_state.dispatch(domain::make_update_setting_action("auto_promote_correct_answers", "true"));
    start_first_day_quiz(promoted_state, 300);

    promoted_state.dispatch(domain::make_submit_option_action(0), 310);
    require(promoted_state.learning().at("q1").state == domain::learning_state::learning, "auto promote waits for configured known threshold");

    return_to_first_question(promoted_state, 320);
    promoted_state.dispatch(domain::make_submit_option_action(0), 330);
    require(promoted_state.learning().at("q1").state == domain::learning_state::known, "auto promote uses configured known threshold");
}

} // namespace

int main()
{
    using namespace quiz_vulkan;

    app_state state({make_test_deck()});
    require(state.decks().size() == 1, "test deck is loaded");

    domain::app_snapshot snapshot = state.snapshot();
    require(snapshot.screen == domain::app_screen::deck_select, "initial screen is deck select");
    require(!snapshot.selected_deck_id.has_value(), "initial snapshot has no selected deck");
    require(snapshot.learning.question_count == 3, "initial learning summary includes all deck questions");
    require(snapshot.learning.learning_count == 3, "initial questions start in learning state");

    state.dispatch(domain::make_select_deck_action("math"));
    require(state.selected_deck_id().has_value(), "select deck stores deck id");
    require(*state.selected_deck_id() == "math", "selected deck id matches action payload");
    require(!state.selected_day_id().has_value(), "select deck clears day selection");
    require(!state.active_session().has_value(), "select deck clears active session");

    snapshot = state.snapshot();
    require(snapshot.screen == domain::app_screen::day_select, "select deck moves to day select");
    require(snapshot.selected_deck_id == state.selected_deck_id(), "snapshot exposes selected deck");

    state.dispatch(domain::make_select_day_action("day1"));
    require(state.selected_day_id().has_value(), "select day stores day id");
    require(*state.selected_day_id() == "day1", "selected day id matches action payload");

    snapshot = state.snapshot();
    require(snapshot.screen == domain::app_screen::day_select, "selected day stays on day intro screen before quiz");
    require(snapshot.selected_day_id == state.selected_day_id(), "snapshot exposes selected day");

    state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::normal), 100);
    require(state.active_session().has_value(), "start quiz creates active session");
    require(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::active, "normal quiz starts active");
    require(state.active_session()->mode == domain::quiz_mode::normal, "normal quiz stores mode");
    require(state.active_session()->question_ids.size() == 2, "normal quiz is scoped to selected day");
    require(state.active_session()->question_ids[0] == "q1", "normal quiz keeps first day question order");
    require(state.active_session()->question_ids[1] == "q2", "normal quiz excludes other days");

    snapshot = state.snapshot();
    require(snapshot.screen == domain::app_screen::quiz, "active session maps to quiz screen");
    require(snapshot.active_session.has_value(), "active snapshot exposes session");
    require(snapshot.active_session->phase == domain::quiz_session_phase::active, "snapshot exposes active phase");
    require(snapshot.active_session->question_count == 2, "snapshot exposes selected day question count");
    require(snapshot.active_session->current_question.has_value(), "snapshot exposes current question");
    require(snapshot.active_session->current_question->question_id == "q1", "snapshot starts on first question");
    require(!snapshot.active_session->current_question->options[0].reveal_correctness, "active question hides correctness");

    state.dispatch(domain::make_submit_option_action(0), 110);
    require(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::feedback, "submitted answer enters feedback");
    require(!domain::can_accept_answer(*state.active_session()), "feedback blocks duplicate answer input");
    require(state.active_session()->pending_feedback.has_value(), "feedback record is pending");
    require(state.active_session()->pending_feedback->question_id == "q1", "feedback targets submitted question");
    require(state.active_session()->pending_feedback->outcome == domain::answer_outcome::correct, "correct option records correct outcome");
    require(state.active_session()->pending_feedback->selected_option_indexes.size() == 1, "feedback records selected option");
    require(state.active_session()->pending_feedback->selected_option_indexes[0] == 0, "feedback records selected option index");
    require(state.active_session()->pending_feedback->answered_at_ms == 110, "feedback stores answer timestamp");
    require(state.active_session()->answer_records.size() == 1, "submit appends answer record");
    require(state.learning().at("q1").correct_streak == 1, "correct answer updates learning streak");
    require(state.learning().at("q1").state == domain::learning_state::learning, "single correct answer does not auto-promote");

    snapshot = state.snapshot();
    require(snapshot.screen == domain::app_screen::feedback, "pending feedback maps to feedback screen");
    require(snapshot.active_session.has_value(), "feedback snapshot exposes session");
    require(snapshot.active_session->feedback.has_value(), "feedback snapshot exposes feedback record");
    require(snapshot.active_session->feedback->outcome == domain::answer_outcome::correct, "feedback snapshot exposes outcome");
    require(snapshot.active_session->current_question.has_value(), "feedback snapshot keeps current question");
    require(snapshot.active_session->current_question->question_id == "q1", "feedback snapshot keeps submitted question");
    require(snapshot.active_session->current_question->options[0].reveal_correctness, "feedback reveals correct option");
    require(snapshot.active_session->current_question->options[0].is_correct, "feedback marks correct option");
    require(snapshot.active_session->current_question->options[1].reveal_correctness, "feedback reveals wrong option");
    require(!snapshot.active_session->current_question->options[1].is_correct, "feedback marks wrong option");

    state.dispatch(domain::make_continue_after_feedback_action(), 120);
    require(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::active, "continue returns to active phase");
    require(!state.active_session()->pending_feedback.has_value(), "continue clears pending feedback");
    require(state.active_session()->current_index == 1, "continue advances to next question");

    snapshot = state.snapshot();
    require(snapshot.screen == domain::app_screen::quiz, "continued session returns to quiz screen");
    require(snapshot.active_session->current_question->question_id == "q2", "continued session shows second question");

    state.dispatch(domain::make_previous_question_action(), 130);
    require(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::active, "previous stays active");
    require(state.active_session()->current_index == 0, "previous returns to first question");
    require(!state.active_session()->pending_feedback.has_value(), "previous clears feedback state");

    snapshot = state.snapshot();
    require(snapshot.screen == domain::app_screen::quiz, "previous question stays on quiz screen");
    require(snapshot.active_session->current_question->question_id == "q1", "previous question snapshot shows first question");

    state.dispatch(domain::make_mark_question_known_action(), 140);
    require(state.learning().at("q1").state == domain::learning_state::known, "mark known updates learning state");
    require(state.learning().at("q1").correct_streak == 3, "mark known applies known threshold streak");
    require(state.learning().at("q1").known_at_ms == 140, "mark known stores known timestamp");
    require(state.learning().at("q1").due_at_ms == 86'400'140, "mark known schedules review");
    require(state.active_session()->answer_records.size() == 2, "mark known appends answer record");
    require(state.active_session()->answer_records.back().outcome == domain::answer_outcome::marked_known, "mark known records marked_known outcome");
    require(state.active_session()->answer_records.back().question_id == "q1", "mark known record targets current question");
    require(state.active_session()->current_index == 1, "mark known advances back to second question");

    snapshot = state.snapshot();
    require(snapshot.learning.known_count == 1, "known learning summary increments");
    require(snapshot.learning.learning_count == 2, "learning summary keeps remaining questions learning");
    require(snapshot.active_session->current_question->question_id == "q2", "mark known advances snapshot to second question");

    state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::known), 150);
    require(state.active_session().has_value(), "known mode creates a session");
    require(state.active_session()->mode == domain::quiz_mode::known, "known mode stores mode");
    require(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::active, "known mode starts active when a known question exists");
    require(state.active_session()->question_ids.size() == 1, "known mode includes only known questions");
    require(state.active_session()->question_ids[0] == "q1", "known mode selects marked known question");

    snapshot = state.snapshot();
    require(snapshot.screen == domain::app_screen::quiz, "known mode active session maps to quiz screen");
    require(snapshot.active_session->mode == domain::quiz_mode::known, "known mode snapshot exposes mode");
    require(snapshot.active_session->current_question->question_id == "q1", "known mode snapshot shows known question");
    require(snapshot.active_session->current_question->learning == domain::learning_state::known, "known mode snapshot exposes known learning state");

    state.dispatch(domain::make_submit_option_action(1), 160);
    require(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::feedback, "wrong known answer enters feedback");
    require(state.active_session()->pending_feedback->outcome == domain::answer_outcome::incorrect, "wrong option records incorrect outcome");
    require(state.active_session()->pending_feedback->selected_option_indexes[0] == 1, "wrong option index is recorded");
    require(state.learning().at("q1").wrong_count == 1, "wrong answer increments wrong count");
    require(state.learning().at("q1").correct_streak == 0, "wrong answer clears correct streak");
    require(state.learning().at("q1").state == domain::learning_state::learning, "wrong answer releases known question to learning");
    require(state.learning().at("q1").known_at_ms == 0, "wrong release clears known timestamp");
    require(state.learning().at("q1").due_at_ms == 0, "wrong release clears due timestamp");

    snapshot = state.snapshot();
    require(snapshot.screen == domain::app_screen::feedback, "wrong known answer maps to feedback screen");
    require(snapshot.learning.known_count == 0, "wrong release removes known summary count");
    require(snapshot.learning.learning_count == 3, "wrong release returns all questions to learning summary");
    require(snapshot.active_session->feedback->outcome == domain::answer_outcome::incorrect, "wrong feedback snapshot exposes incorrect outcome");
    require(snapshot.active_session->current_question->learning == domain::learning_state::learning, "wrong feedback snapshot reflects released learning state");

    state.dispatch(domain::make_start_quiz_action(domain::quiz_mode::known), 170);
    require(domain::phase_of(*state.active_session()) == domain::quiz_session_phase::completed, "known mode completes when no known questions remain");
    require(state.active_session()->question_ids.empty(), "released question is excluded from known mode");
    require(state.snapshot().screen == domain::app_screen::completed, "empty known session maps to completed screen");

    state.dispatch(domain::make_select_day_action("missing"));
    snapshot = state.snapshot();
    require(snapshot.screen == domain::app_screen::error, "invalid day maps to error screen");
    require(snapshot.error_message.has_value(), "error snapshot carries message");
    require(*snapshot.error_message == "Day not found: missing", "invalid day error message is stable");
    require(state.selected_day_id().has_value() && *state.selected_day_id() == "day1", "invalid day does not replace selection");

    test_wrong_note_settings_take_effect();
    test_invalid_setting_preserves_prior_rule();
    test_known_settings_update_mark_and_auto_promote();

    return 0;
}
