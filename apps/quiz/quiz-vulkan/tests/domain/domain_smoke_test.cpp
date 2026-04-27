#include "core/domain/app_action.hpp"
#include "core/domain/app_snapshot.hpp"
#include "core/domain/learning_state.hpp"
#include "core/domain/quiz_model.hpp"
#include "core/domain/quiz_session.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using namespace quiz_vulkan::domain;

namespace {

void require(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    std::cerr << "domain_smoke_test failed: " << message << '\n';
    std::exit(1);
}

deck make_test_deck()
{
    question option_question;
    option_question.id = "q_option";
    option_question.prompt = "Pick the capital";
    option_question.type = question_type::answer;
    option_question.source = content_ref{
        "software-engineering",
        "quiz-data/mobile-decks/software-engineering-exam-prep-v2/day-1-git.json",
        "day-1",
        "paragraph-1",
        std::optional<std::size_t>{0},
    };
    option_question.concept_ids = {"git-vcs"};
    option_question.explanation = answer_explanation{
        "A VCS records change history and makes recovery, comparison, and blame possible.",
        {"git-vcs"},
    };
    option_question.options = {
        option{"Busan", false},
        option{"Seoul", true},
    };

    question multi_text_question;
    multi_text_question.id = "q_multi_text";
    multi_text_question.prompt = "Name both colors";
    multi_text_question.type = question_type::multi_answer;
    multi_text_question.accepted_answers = {"red", "blue"};

    question multiselect_question;
    multiselect_question.id = "q_multiselect";
    multiselect_question.prompt = "Pick both vowels";
    multiselect_question.type = question_type::multiselect;
    multiselect_question.options = {
        option{"a", true},
        option{"b", false},
        option{"e", true},
    };

    day test_day;
    test_day.id = "day_1";
    test_day.title = "Day 1";
    test_day.questions = {
        option_question,
        multi_text_question,
        multiselect_question,
    };

    deck test_deck;
    test_deck.id = "deck_1";
    test_deck.title = "Deck 1";
    test_deck.source_uri = "quiz-data/mobile-decks/software-engineering-exam-prep-v2";
    test_deck.concepts = {
        concept_ref{"git-vcs", "Version control system", {}},
        concept_ref{"git-objects", "Git object database", {"git-vcs"}},
    };
    test_deck.days = {test_day};
    return test_deck;
}

void verify_contract_metadata(const deck& test_deck)
{
    require(test_deck.schema_version == "quiz-deck-v1", "deck exposes schema version");
    require(test_deck.source_uri == "quiz-data/mobile-decks/software-engineering-exam-prep-v2", "deck stores source uri");
    require(test_deck.concepts.size() == 2, "deck stores concept graph nodes");
    require(test_deck.concepts[1].prerequisite_ids[0] == "git-vcs", "concept stores prerequisites");

    const question* option_question = find_question(test_deck, "q_option");
    require(option_question != nullptr, "contract metadata question exists");
    require(option_question->source.has_value(), "question stores source reference");
    require(option_question->source->paragraph_id == "paragraph-1", "source reference stores paragraph id");
    require(option_question->source->paragraph_index.has_value(), "source reference stores paragraph index");
    require(option_question->concept_ids.size() == 1 && option_question->concept_ids[0] == "git-vcs", "question stores concept ids");
    require(option_question->explanation.has_value(), "question stores answer explanation");
    require(option_question->explanation->concept_ids[0] == "git-vcs", "explanation stores concept ids");

    const std::optional<question_ref> located = locate_question(test_deck, "q_option");
    require(located.has_value(), "locate_question returns deck/day/question ref");
    require(located->deck_id == "deck_1", "located question stores deck id");
    require(located->day_id == "day_1", "located question stores day id");
    require(located->question_id == "q_option", "located question stores question id");
    require(make_question_key(*located) == "deck_1::day_1::q_option", "question key matches app learning key shape");

    const std::vector<std::string> question_keys = collect_question_keys(test_deck);
    require(question_keys.size() == 3, "collect_question_keys returns all questions");
    require(question_keys[0] == "deck_1::day_1::q_option", "question keys preserve source order");
}

void verify_answer_matching(const deck& test_deck)
{
    const question* multi_text = find_question(test_deck, "q_multi_text");
    const question* multiselect = find_question(test_deck, "q_multiselect");

    require(multi_text != nullptr, "expected multi text question");
    require(multiselect != nullptr, "expected multiselect question");
    require(normalize_answer_text("  Seoul\nCity  ") == "seoul city", "normalizes typed answers");
    require(multi_text_answer_matches(*multi_text, " Blue; RED "), "multi typed answers match as a set");
    require(!multi_text_answer_matches(*multi_text, "Blue"), "multi typed answers require the full set");
    require(multiselect_matches(*multiselect, std::vector<std::size_t>{2, 0}), "multiselect matches exact correct set");
    require(!multiselect_matches(*multiselect, std::vector<std::size_t>{0, 1, 2}), "multiselect rejects extra option");
}

void verify_quiz_mode_strings()
{
    require(to_string(quiz_mode::wrong_only) == "wrong_only", "wrong only mode string stays stable");
    require(to_string(quiz_mode::wrong_note) == "wrong_note", "wrong note mode string is stable");
    require(parse_quiz_mode("wrong_only") == quiz_mode::wrong_only, "wrong only mode parses underscore form");
    require(parse_quiz_mode("wrong only") == quiz_mode::wrong_only, "wrong only mode parses spaced form");
    require(parse_quiz_mode("wrong_note") == quiz_mode::wrong_note, "wrong note mode parses underscore form");
    require(parse_quiz_mode("wrong note") == quiz_mode::wrong_note, "wrong note mode parses spaced form");
    require(parse_quiz_mode("wrong-note") == quiz_mode::wrong_note, "wrong note mode parses hyphen form");
}

void verify_learning_updates()
{
    const learning_update_rules rules{
        3,
        1,
        1'000,
        2,
    };
    question_learning learning;
    record_correct_answer(learning, 10, rules);
    require(learning.state == learning_state::learning, "one correct answer stays learning");
    record_correct_answer(learning, 20, rules);
    require(learning.state == learning_state::learning, "two correct answers stay learning by default");
    record_correct_answer(learning, 25, rules);
    require(learning.state == learning_state::learning, "correct streak does not auto-promote when swipe-only known is default");

    mark_question_known(learning, 25, rules);
    require(learning.state == learning_state::known, "manual swipe known action promotes to known");
    require(learning.known_at_ms == 25, "known promotion stores known timestamp");
    require(learning.due_at_ms == 1025, "known promotion schedules first review");
    require(!is_question_due_for_review(learning, 1024), "known question is not due before due timestamp");
    require(is_question_due_for_review(learning, 1025), "known question is due at due timestamp");

    record_correct_answer(learning, 1025, rules);
    require(learning.state == learning_state::known, "correct due review keeps known state");
    require(learning.review_count == 1, "correct due review increments review count");
    require(learning.due_at_ms == 3025, "correct due review schedules longer interval");

    record_wrong_answer(learning, 3030, rules);
    require(learning.state == learning_state::learning, "wrong answer releases known to learning");
    require(learning.due_at_ms == 0, "wrong answer clears known review due timestamp");
    mark_question_unknown(learning, 40);
    require(learning.state == learning_state::unknown, "manual mark sets unknown");
    require(learning.wrong_count == 0, "manual unknown clears wrong count");

    question_learning skipped_learning;
    record_skipped_question(skipped_learning, 80);
    record_skipped_question(skipped_learning, 90);
    require(!can_offer_known_by_swipe(skipped_learning, rules), "two swipe skips do not offer known by default");
    record_skipped_question(skipped_learning, 100);
    require(can_offer_known_by_swipe(skipped_learning, rules), "third swipe skip can offer known");

    learning_update_rules wrong_note_rules = rules;
    wrong_note_rules.wrong_note_enabled = true;
    question_learning wrong_learning;
    record_wrong_answer(wrong_learning, 110, wrong_note_rules);
    require(wrong_learning.state == learning_state::wrong_note, "wrong note enabled moves wrong answer to wrong note");
    record_correct_answer(wrong_learning, 120, wrong_note_rules);
    record_correct_answer(wrong_learning, 130, wrong_note_rules);
    require(wrong_learning.state == learning_state::wrong_note, "wrong note waits for release streak");
    record_correct_answer(wrong_learning, 140, wrong_note_rules);
    require(wrong_learning.state == learning_state::learning, "wrong note releases after correct streak");

    learning_update_rules auto_rules = rules;
    auto_rules.auto_promote_correct_answers = true;
    question_learning auto_learning;
    record_correct_answer(auto_learning, 50, auto_rules);
    record_correct_answer(auto_learning, 60, auto_rules);
    record_correct_answer(auto_learning, 70, auto_rules);
    require(auto_learning.state == learning_state::known, "auto mode can still promote by correct streak");
}

void verify_session_filters(const deck& test_deck)
{
    learning_state_map learning_by_question_id;
    learning_by_question_id["q_multiselect"].state = learning_state::known;

    previous_answer_map previous_answers;
    previous_answers["q_multi_text"] = answer_outcome::incorrect;
    previous_answers["q_multiselect"] = answer_outcome::correct;

    quiz_session normal_session = start_quiz_session(
        test_deck,
        learning_by_question_id,
        previous_answers);
    require(normal_session.question_ids.size() == 2, "normal session hides known questions");
    require(normal_session.question_ids[0] == "q_option", "normal session keeps source order");
    require(normal_session.question_ids[1] == "q_multi_text", "normal session includes learning questions");

    learning_by_question_id["q_multiselect"].due_at_ms = 1000;
    quiz_session_options due_options;
    due_options.now_ms = 1000;
    quiz_session due_session = start_quiz_session(
        test_deck,
        learning_by_question_id,
        previous_answers,
        due_options);
    require(due_session.question_ids.size() == 3, "normal session includes due known questions");
    require(due_session.question_ids[2] == "q_multiselect", "due known question keeps source order");

    quiz_session_options known_options;
    known_options.mode = quiz_mode::known;
    quiz_session known_session = start_quiz_session(
        test_deck,
        learning_by_question_id,
        previous_answers,
        known_options);
    require(known_session.question_ids.size() == 1, "known mode includes known questions only");
    require(known_session.question_ids[0] == "q_multiselect", "known mode selected known question");

    quiz_session_options wrong_options;
    wrong_options.mode = quiz_mode::wrong_only;
    quiz_session wrong_session = start_quiz_session(
        test_deck,
        learning_by_question_id,
        previous_answers,
        wrong_options);
    require(wrong_session.question_ids.size() == 1, "wrong mode uses previous result record");
    require(wrong_session.question_ids[0] == "q_multi_text", "wrong mode selected prior incorrect question");

    learning_state_map wrong_note_learning;
    wrong_note_learning["q_multi_text"].state = learning_state::wrong_note;
    wrong_note_learning["q_multiselect"].state = learning_state::known;

    previous_answer_map wrong_note_previous_answers;
    wrong_note_previous_answers["q_option"] = answer_outcome::incorrect;
    wrong_note_previous_answers["q_multi_text"] = answer_outcome::correct;

    quiz_session_options wrong_note_options;
    wrong_note_options.mode = quiz_mode::wrong_note;
    quiz_session wrong_note_session = start_quiz_session(
        test_deck,
        wrong_note_learning,
        wrong_note_previous_answers,
        wrong_note_options);
    require(wrong_note_session.question_ids.size() == 1, "wrong note mode includes wrong-note questions only");
    require(wrong_note_session.question_ids[0] == "q_multi_text", "wrong note mode selected wrong-note question");

    quiz_session empty_wrong_note_session = start_quiz_session(
        test_deck,
        learning_by_question_id,
        previous_answers,
        wrong_note_options);
    require(empty_wrong_note_session.question_ids.empty(), "empty wrong note mode has no questions");
    require(empty_wrong_note_session.completed, "empty wrong note mode starts completed");
}

void verify_session_flow_and_snapshot(const deck& test_deck)
{
    learning_state_map learning_by_question_id;
    previous_answer_map previous_answers;
    quiz_session session = start_quiz_session(test_deck, learning_by_question_id, previous_answers);
    require(phase_of(session) == quiz_session_phase::active, "started session begins active");
    require(can_accept_answer(session), "active session accepts answers");

    std::optional<answer_record> first_record = submit_option_answer(session, test_deck, 1, 100);
    require(first_record.has_value(), "submit option returns a record");
    require(first_record->outcome == answer_outcome::correct, "submit option evaluates correctness");
    require(session.pending_feedback.has_value(), "submit option leaves feedback pending");
    require(phase_of(session) == quiz_session_phase::feedback, "submitted answer enters feedback phase");
    require(!can_accept_answer(session), "feedback phase blocks duplicate answers");
    require(can_continue_feedback(session), "feedback phase can continue");

    apply_answer_to_learning(learning_by_question_id, *first_record);
    apply_answer_to_learning(learning_by_question_id, *first_record);
    apply_answer_to_learning(learning_by_question_id, *first_record);
    require(
        learning_by_question_id["q_option"].correct_streak == 3,
        "answer records update learning state without auto known promotion");

    app_snapshot feedback_snapshot = make_app_snapshot(
        std::vector<deck>{test_deck},
        std::optional<std::string>{"deck_1"},
        std::optional<std::string>{"day_1"},
        &session,
        learning_by_question_id);
    require(feedback_snapshot.screen == app_screen::feedback, "pending feedback maps to feedback screen");
    require(feedback_snapshot.active_session.has_value(), "snapshot includes active session");
    require(feedback_snapshot.active_session->phase == quiz_session_phase::feedback, "snapshot exposes session phase");
    require(feedback_snapshot.active_session->current_question.has_value(), "snapshot includes current question");
    require(
        feedback_snapshot.active_session->current_question->options[1].reveal_correctness,
        "feedback snapshot reveals correctness");

    continue_after_feedback(session);
    require(session.current_index == 1, "continue advances after feedback");
    require(!session.pending_feedback.has_value(), "continue clears feedback");
    require(phase_of(session) == quiz_session_phase::active, "continue returns to active phase");

    std::optional<answer_record> second_record = submit_text_answer(session, test_deck, "red, blue", 200);
    require(second_record.has_value(), "submit text returns a record");
    require(second_record->outcome == answer_outcome::correct, "multi text answer evaluates correctness");
    continue_after_feedback(session);

    std::optional<answer_record> third_record = skip_current_question(session, 300);
    require(third_record.has_value(), "skip returns a record");
    require(third_record->outcome == answer_outcome::skipped, "skip records skipped outcome");
    require(session.completed, "skip advances and completes final question");
    require(phase_of(session) == quiz_session_phase::completed, "final skip completes session");

    go_to_previous_question(session);
    require(!session.completed, "previous question reopens completed session");
    require(session.current_index == 2, "previous question from completed session targets final question");

    std::optional<answer_record> known_record = mark_current_question_known(session, learning_by_question_id, 400);
    require(known_record.has_value(), "mark known returns a record");
    require(known_record->outcome == answer_outcome::marked_known, "mark known records marked_known outcome");
    require(is_question_known(learning_by_question_id, "q_multiselect"), "mark known updates learning map");
}

void verify_actions()
{
    app_action action = make_start_quiz_action(quiz_mode::known, std::optional<unsigned int>{123U}, true);
    require(type_of(action) == app_action_type::start_quiz, "start quiz action reports type");

    const auto* payload = std::get_if<start_quiz_action>(&action.payload);
    require(payload != nullptr, "start quiz action stores payload");
    require(payload->mode == quiz_mode::known, "start quiz action stores mode");
    require(payload->random_seed.has_value() && *payload->random_seed == 123U, "start quiz action stores seed");
    require(payload->shuffle, "start quiz action stores shuffle flag");

    app_action mark_known = make_mark_question_known_action();
    require(type_of(mark_known) == app_action_type::mark_question_known, "mark known action reports type");

    app_action previous = make_previous_question_action();
    require(type_of(previous) == app_action_type::previous_question, "previous action reports type");
}

}  // namespace

int main()
{
    const deck test_deck = make_test_deck();

    verify_contract_metadata(test_deck);
    verify_answer_matching(test_deck);
    verify_quiz_mode_strings();
    verify_learning_updates();
    verify_session_filters(test_deck);
    verify_session_flow_and_snapshot(test_deck);
    verify_actions();

    std::cout << "domain_smoke_test passed\n";
    return 0;
}
