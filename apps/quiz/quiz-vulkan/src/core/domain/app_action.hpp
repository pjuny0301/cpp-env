#pragma once

#include "quiz_session.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace quiz_vulkan::domain {

enum class app_action_type {
    load_source,
    select_deck,
    select_day,
    start_quiz,
    submit_option,
    submit_text_answer,
    submit_multiselect,
    skip_question,
    mark_question_unknown,
    continue_after_feedback,
    update_setting,
};

struct load_source_action {
    std::string source_uri;
};

struct select_deck_action {
    std::string deck_id;
};

struct select_day_action {
    std::string day_id;
};

struct start_quiz_action {
    quiz_mode mode = quiz_mode::normal;
    std::optional<unsigned int> random_seed;
    bool shuffle = false;
};

struct submit_option_action {
    std::size_t option_index = 0;
};

struct submit_text_answer_action {
    std::string answer_text;
};

struct submit_multiselect_action {
    std::vector<std::size_t> option_indexes;
};

struct skip_question_action {
};

struct mark_question_unknown_action {
};

struct continue_after_feedback_action {
};

struct update_setting_action {
    std::string name;
    std::string value;
};

using app_action_payload = std::variant<
    load_source_action,
    select_deck_action,
    select_day_action,
    start_quiz_action,
    submit_option_action,
    submit_text_answer_action,
    submit_multiselect_action,
    skip_question_action,
    mark_question_unknown_action,
    continue_after_feedback_action,
    update_setting_action>;

struct app_action {
    app_action_payload payload;
};

std::string_view to_string(app_action_type type);
app_action_type type_of(const app_action& action);

app_action make_load_source_action(std::string source_uri);
app_action make_select_deck_action(std::string deck_id);
app_action make_select_day_action(std::string day_id);
app_action make_start_quiz_action(
    quiz_mode mode,
    std::optional<unsigned int> random_seed = std::nullopt,
    bool shuffle = false);
app_action make_submit_option_action(std::size_t option_index);
app_action make_submit_text_answer_action(std::string answer_text);
app_action make_submit_multiselect_action(std::vector<std::size_t> option_indexes);
app_action make_skip_question_action();
app_action make_mark_question_unknown_action();
app_action make_continue_after_feedback_action();
app_action make_update_setting_action(std::string name, std::string value);

}  // namespace quiz_vulkan::domain
