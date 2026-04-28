#include "app_action.hpp"

#include <utility>

namespace quiz_vulkan::domain {

std::string_view to_string(app_action_type type)
{
    switch (type) {
        case app_action_type::load_source:
            return "load_source";
        case app_action_type::select_deck:
            return "select_deck";
        case app_action_type::select_day:
            return "select_day";
        case app_action_type::start_quiz:
            return "start_quiz";
        case app_action_type::submit_option:
            return "submit_option";
        case app_action_type::submit_text_answer:
            return "submit_text_answer";
        case app_action_type::submit_multiselect:
            return "submit_multiselect";
        case app_action_type::skip_question:
            return "skip_question";
        case app_action_type::mark_question_known:
            return "mark_question_known";
        case app_action_type::mark_question_unknown:
            return "mark_question_unknown";
        case app_action_type::previous_question:
            return "previous_question";
        case app_action_type::continue_after_feedback:
            return "continue_after_feedback";
        case app_action_type::update_setting:
            return "update_setting";
    }

    return "update_setting";
}

app_action_type type_of(const app_action& action)
{
    if (std::holds_alternative<load_source_action>(action.payload)) {
        return app_action_type::load_source;
    }
    if (std::holds_alternative<select_deck_action>(action.payload)) {
        return app_action_type::select_deck;
    }
    if (std::holds_alternative<select_day_action>(action.payload)) {
        return app_action_type::select_day;
    }
    if (std::holds_alternative<start_quiz_action>(action.payload)) {
        return app_action_type::start_quiz;
    }
    if (std::holds_alternative<submit_option_action>(action.payload)) {
        return app_action_type::submit_option;
    }
    if (std::holds_alternative<submit_text_answer_action>(action.payload)) {
        return app_action_type::submit_text_answer;
    }
    if (std::holds_alternative<submit_multiselect_action>(action.payload)) {
        return app_action_type::submit_multiselect;
    }
    if (std::holds_alternative<skip_question_action>(action.payload)) {
        return app_action_type::skip_question;
    }
    if (std::holds_alternative<mark_question_known_action>(action.payload)) {
        return app_action_type::mark_question_known;
    }
    if (std::holds_alternative<mark_question_unknown_action>(action.payload)) {
        return app_action_type::mark_question_unknown;
    }
    if (std::holds_alternative<previous_question_action>(action.payload)) {
        return app_action_type::previous_question;
    }
    if (std::holds_alternative<continue_after_feedback_action>(action.payload)) {
        return app_action_type::continue_after_feedback;
    }

    return app_action_type::update_setting;
}

app_action make_load_source_action(std::string source_uri)
{
    return app_action{load_source_action{std::move(source_uri)}};
}

app_action make_select_deck_action(std::string deck_id)
{
    return app_action{select_deck_action{std::move(deck_id)}};
}

app_action make_select_day_action(std::string day_id)
{
    return app_action{select_day_action{std::move(day_id)}};
}

app_action make_start_quiz_action(
    quiz_mode mode,
    std::optional<unsigned int> random_seed,
    bool shuffle)
{
    return app_action{start_quiz_action{mode, random_seed, shuffle}};
}

app_action make_submit_option_action(std::size_t option_index)
{
    return app_action{submit_option_action{option_index}};
}

app_action make_submit_text_answer_action(std::string answer_text)
{
    return app_action{submit_text_answer_action{std::move(answer_text)}};
}

app_action make_submit_multiselect_action(std::vector<std::size_t> option_indexes)
{
    return app_action{submit_multiselect_action{std::move(option_indexes)}};
}

app_action make_skip_question_action()
{
    return app_action{skip_question_action{}};
}

app_action make_mark_question_known_action()
{
    return app_action{mark_question_known_action{}};
}

app_action make_mark_question_unknown_action()
{
    return app_action{mark_question_unknown_action{}};
}

app_action make_previous_question_action()
{
    return app_action{previous_question_action{}};
}

app_action make_continue_after_feedback_action()
{
    return app_action{continue_after_feedback_action{}};
}

app_action make_update_setting_action(std::string name, std::string value)
{
    return app_action{update_setting_action{std::move(name), std::move(value)}};
}

}  // namespace quiz_vulkan::domain
