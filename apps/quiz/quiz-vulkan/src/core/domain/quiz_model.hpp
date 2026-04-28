#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quiz_vulkan::domain {

enum class question_type {
    short_,
    long_,
    answer,
    blank,
    multi_answer,
    multi_blank,
    multiselect,
};

struct option {
    std::string text;
    bool is_correct = false;
};

struct content_ref {
    std::string source_id;
    std::string source_uri;
    std::string page_id;
    std::string paragraph_id;
    std::optional<std::size_t> paragraph_index;
};

struct concept_ref {
    std::string id;
    std::string title;
    std::vector<std::string> prerequisite_ids;
};

struct answer_explanation {
    std::string text;
    std::vector<std::string> concept_ids;
};

struct question {
    std::string id;
    std::string prompt;
    std::optional<std::string> long_text;
    std::optional<std::string> image_uri;
    question_type type = question_type::answer;
    std::vector<option> options;
    std::vector<std::string> accepted_answers;
    std::optional<content_ref> source;
    std::vector<std::string> concept_ids;
    std::optional<answer_explanation> explanation;
};

struct day {
    std::string id;
    std::string title;
    std::vector<question> questions;
};

struct deck {
    std::string schema_version = "quiz-deck-v1";
    std::string id;
    std::string title;
    std::string source_uri;
    std::vector<concept_ref> concepts;
    std::vector<day> days;
};

struct question_ref {
    std::string deck_id;
    std::string day_id;
    std::string question_id;
};

std::string_view to_string(question_type type);
std::optional<question_type> parse_question_type(std::string_view value);

std::string normalize_answer_text(std::string_view text);
std::vector<std::string> split_multi_answer_text(std::string_view text);

bool text_answer_matches(const question& quiz_question, std::string_view submitted_text);
bool multi_text_answer_matches(const question& quiz_question, std::string_view submitted_text);
bool multiselect_matches(
    const question& quiz_question,
    const std::vector<std::size_t>& selected_option_indexes);

const day* find_day(const deck& quiz_deck, std::string_view day_id);
const question* find_question(const deck& quiz_deck, std::string_view question_id);
std::optional<question_ref> locate_question(const deck& quiz_deck, std::string_view question_id);
std::string make_question_key(
    std::string_view deck_id,
    std::string_view day_id,
    std::string_view question_id);
std::string make_question_key(const question_ref& ref);

std::vector<const question*> collect_questions(
    const deck& quiz_deck,
    const std::optional<std::string>& day_id = std::nullopt);
std::vector<std::string> collect_question_ids(
    const deck& quiz_deck,
    const std::optional<std::string>& day_id = std::nullopt);
std::vector<std::string> collect_question_keys(
    const deck& quiz_deck,
    const std::optional<std::string>& day_id = std::nullopt);

}  // namespace quiz_vulkan::domain
