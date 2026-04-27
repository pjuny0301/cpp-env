#include "quiz_model.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace quiz_vulkan::domain {
namespace {

bool is_separator(char value)
{
    return value == ',' || value == ';' || value == '\n';
}

std::vector<std::string> normalized_expected_answers(const question& quiz_question)
{
    std::vector<std::string> answers;
    answers.reserve(quiz_question.accepted_answers.size() + quiz_question.options.size());

    for (const std::string& answer : quiz_question.accepted_answers) {
        std::string normalized = normalize_answer_text(answer);
        if (!normalized.empty()) {
            answers.push_back(std::move(normalized));
        }
    }

    if (answers.empty()) {
        for (const option& answer_option : quiz_question.options) {
            if (!answer_option.is_correct) {
                continue;
            }

            std::string normalized = normalize_answer_text(answer_option.text);
            if (!normalized.empty()) {
                answers.push_back(std::move(normalized));
            }
        }
    }

    std::sort(answers.begin(), answers.end());
    answers.erase(std::unique(answers.begin(), answers.end()), answers.end());
    return answers;
}

}  // namespace

std::string_view to_string(question_type type)
{
    switch (type) {
        case question_type::short_:
            return "short";
        case question_type::long_:
            return "long";
        case question_type::answer:
            return "answer";
        case question_type::blank:
            return "blank";
        case question_type::multi_answer:
            return "multi_answer";
        case question_type::multi_blank:
            return "multi_blank";
        case question_type::multiselect:
            return "multiselect";
    }

    return "answer";
}

std::optional<question_type> parse_question_type(std::string_view value)
{
    const std::string normalized = normalize_answer_text(value);

    if (normalized == "short") {
        return question_type::short_;
    }
    if (normalized == "long") {
        return question_type::long_;
    }
    if (normalized == "answer") {
        return question_type::answer;
    }
    if (normalized == "blank") {
        return question_type::blank;
    }
    if (normalized == "multi_answer" || normalized == "multi answer") {
        return question_type::multi_answer;
    }
    if (normalized == "multi_blank" || normalized == "multi blank") {
        return question_type::multi_blank;
    }
    if (normalized == "multiselect" || normalized == "multi_select" || normalized == "multi select") {
        return question_type::multiselect;
    }

    return std::nullopt;
}

std::string normalize_answer_text(std::string_view text)
{
    std::string normalized;
    normalized.reserve(text.size());

    bool pending_space = false;
    for (unsigned char raw_character : text) {
        if (std::isspace(raw_character) != 0) {
            pending_space = !normalized.empty();
            continue;
        }

        if (pending_space) {
            normalized.push_back(' ');
            pending_space = false;
        }

        normalized.push_back(static_cast<char>(std::tolower(raw_character)));
    }

    return normalized;
}

std::vector<std::string> split_multi_answer_text(std::string_view text)
{
    std::vector<std::string> answers;
    std::size_t start_index = 0;

    for (std::size_t index = 0; index <= text.size(); ++index) {
        if (index != text.size() && !is_separator(text[index])) {
            continue;
        }

        std::string normalized = normalize_answer_text(text.substr(start_index, index - start_index));
        if (!normalized.empty()) {
            answers.push_back(std::move(normalized));
        }

        start_index = index + 1;
    }

    return answers;
}

bool text_answer_matches(const question& quiz_question, std::string_view submitted_text)
{
    const std::string normalized_submitted_text = normalize_answer_text(submitted_text);
    if (normalized_submitted_text.empty()) {
        return false;
    }

    const std::vector<std::string> expected_answers = normalized_expected_answers(quiz_question);
    return std::find(
               expected_answers.begin(),
               expected_answers.end(),
               normalized_submitted_text) != expected_answers.end();
}

bool multi_text_answer_matches(const question& quiz_question, std::string_view submitted_text)
{
    std::vector<std::string> submitted_answers = split_multi_answer_text(submitted_text);
    std::sort(submitted_answers.begin(), submitted_answers.end());
    submitted_answers.erase(std::unique(submitted_answers.begin(), submitted_answers.end()), submitted_answers.end());

    const std::vector<std::string> expected_answers = normalized_expected_answers(quiz_question);
    return !expected_answers.empty() && submitted_answers == expected_answers;
}

bool multiselect_matches(
    const question& quiz_question,
    const std::vector<std::size_t>& selected_option_indexes)
{
    std::set<std::size_t> expected_indexes;
    for (std::size_t index = 0; index < quiz_question.options.size(); ++index) {
        if (quiz_question.options[index].is_correct) {
            expected_indexes.insert(index);
        }
    }

    std::set<std::size_t> selected_indexes;
    for (std::size_t index : selected_option_indexes) {
        if (index >= quiz_question.options.size()) {
            return false;
        }

        selected_indexes.insert(index);
    }

    return !expected_indexes.empty() && selected_indexes == expected_indexes;
}

const day* find_day(const deck& quiz_deck, std::string_view day_id)
{
    const auto match = std::find_if(
        quiz_deck.days.begin(),
        quiz_deck.days.end(),
        [day_id](const day& candidate_day) {
            return candidate_day.id == day_id;
        });

    return match == quiz_deck.days.end() ? nullptr : &(*match);
}

const question* find_question(const deck& quiz_deck, std::string_view question_id)
{
    for (const day& candidate_day : quiz_deck.days) {
        const auto match = std::find_if(
            candidate_day.questions.begin(),
            candidate_day.questions.end(),
            [question_id](const question& candidate_question) {
                return candidate_question.id == question_id;
            });

        if (match != candidate_day.questions.end()) {
            return &(*match);
        }
    }

    return nullptr;
}

std::optional<question_ref> locate_question(const deck& quiz_deck, std::string_view question_id)
{
    for (const day& candidate_day : quiz_deck.days) {
        const auto match = std::find_if(
            candidate_day.questions.begin(),
            candidate_day.questions.end(),
            [question_id](const question& candidate_question) {
                return candidate_question.id == question_id;
            });

        if (match != candidate_day.questions.end()) {
            return question_ref{quiz_deck.id, candidate_day.id, match->id};
        }
    }

    return std::nullopt;
}

std::string make_question_key(
    std::string_view deck_id,
    std::string_view day_id,
    std::string_view question_id)
{
    std::string key;
    key.reserve(deck_id.size() + day_id.size() + question_id.size() + 4);
    key.append(deck_id);
    key.append("::");
    key.append(day_id);
    key.append("::");
    key.append(question_id);
    return key;
}

std::string make_question_key(const question_ref& ref)
{
    return make_question_key(ref.deck_id, ref.day_id, ref.question_id);
}

std::vector<const question*> collect_questions(
    const deck& quiz_deck,
    const std::optional<std::string>& day_id)
{
    std::vector<const question*> questions;

    if (day_id.has_value()) {
        const day* selected_day = find_day(quiz_deck, *day_id);
        if (selected_day == nullptr) {
            return questions;
        }

        questions.reserve(selected_day->questions.size());
        for (const question& quiz_question : selected_day->questions) {
            questions.push_back(&quiz_question);
        }

        return questions;
    }

    for (const day& quiz_day : quiz_deck.days) {
        for (const question& quiz_question : quiz_day.questions) {
            questions.push_back(&quiz_question);
        }
    }

    return questions;
}

std::vector<std::string> collect_question_ids(
    const deck& quiz_deck,
    const std::optional<std::string>& day_id)
{
    const std::vector<const question*> questions = collect_questions(quiz_deck, day_id);

    std::vector<std::string> question_ids;
    question_ids.reserve(questions.size());

    for (const question* quiz_question : questions) {
        question_ids.push_back(quiz_question->id);
    }

    return question_ids;
}

std::vector<std::string> collect_question_keys(
    const deck& quiz_deck,
    const std::optional<std::string>& day_id)
{
    std::vector<std::string> question_keys;

    for (const day& quiz_day : quiz_deck.days) {
        if (day_id.has_value() && quiz_day.id != *day_id) {
            continue;
        }

        question_keys.reserve(question_keys.size() + quiz_day.questions.size());
        for (const question& quiz_question : quiz_day.questions) {
            question_keys.push_back(make_question_key(quiz_deck.id, quiz_day.id, quiz_question.id));
        }
    }

    return question_keys;
}

}  // namespace quiz_vulkan::domain
