#pragma once

#include "core/domain/app_snapshot.hpp"
#include "core/scene/scene_modifier.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quiz_vulkan::presentation {

enum class quiz_screen_kind {
    deck_list,
    deck_view,
    day_intro,
    quiz_active,
    quiz_feedback,
    quiz_results,
    settings,
    error,
};

struct quiz_screen_descriptor {
    quiz_screen_kind kind = quiz_screen_kind::deck_list;
    scene::scene_route_state route;
    std::string title;
    std::string subtitle;
    std::string root_node_id;
    std::string focus_node_id;
    bool input_locked = false;
    bool shows_settings = false;
    bool shows_error = false;
};

namespace detail {

inline constexpr std::string_view quiz_screens_root_id = "quiz_screens_root";

inline std::string to_string_copy(std::string_view value)
{
    return std::string(value);
}

inline std::string screen_name(quiz_screen_kind screen)
{
    switch (screen) {
        case quiz_screen_kind::deck_list:
            return "deck_list";
        case quiz_screen_kind::deck_view:
            return "deck_view";
        case quiz_screen_kind::day_intro:
            return "day_intro";
        case quiz_screen_kind::quiz_active:
            return "quiz_active";
        case quiz_screen_kind::quiz_feedback:
            return "quiz_feedback";
        case quiz_screen_kind::quiz_results:
            return "quiz_results";
        case quiz_screen_kind::settings:
            return "settings";
        case quiz_screen_kind::error:
            return "error";
    }

    return "deck_list";
}

inline std::string app_screen_name(domain::app_screen screen)
{
    switch (screen) {
        case domain::app_screen::loading:
            return "loading";
        case domain::app_screen::deck_select:
            return "deck_select";
        case domain::app_screen::day_select:
            return "day_select";
        case domain::app_screen::quiz:
            return "quiz";
        case domain::app_screen::feedback:
            return "feedback";
        case domain::app_screen::completed:
            return "completed";
        case domain::app_screen::error:
            return "error";
    }

    return "deck_select";
}

inline std::string quiz_mode_name(domain::quiz_mode mode)
{
    switch (mode) {
        case domain::quiz_mode::normal:
            return "normal";
        case domain::quiz_mode::random:
            return "random";
        case domain::quiz_mode::wrong_only:
            return "wrong_only";
        case domain::quiz_mode::wrong_note:
            return "wrong_note";
        case domain::quiz_mode::known:
            return "known";
    }

    return "normal";
}

inline std::string session_phase_name(domain::quiz_session_phase phase)
{
    switch (phase) {
        case domain::quiz_session_phase::idle:
            return "idle";
        case domain::quiz_session_phase::active:
            return "active";
        case domain::quiz_session_phase::feedback:
            return "feedback";
        case domain::quiz_session_phase::completed:
            return "completed";
    }

    return "idle";
}

inline std::string question_type_name(domain::question_type type)
{
    switch (type) {
        case domain::question_type::short_:
            return "short";
        case domain::question_type::long_:
            return "long";
        case domain::question_type::answer:
            return "answer";
        case domain::question_type::blank:
            return "blank";
        case domain::question_type::multi_answer:
            return "multi_answer";
        case domain::question_type::multi_blank:
            return "multi_blank";
        case domain::question_type::multiselect:
            return "multiselect";
    }

    return "answer";
}

inline std::string learning_state_name(domain::learning_state state)
{
    switch (state) {
        case domain::learning_state::learning:
            return "learning";
        case domain::learning_state::known:
            return "known";
        case domain::learning_state::unknown:
            return "unknown";
        case domain::learning_state::wrong_note:
            return "wrong_note";
    }

    return "learning";
}

inline std::string display_outcome(domain::answer_outcome outcome)
{
    switch (outcome) {
        case domain::answer_outcome::correct:
            return "Correct";
        case domain::answer_outcome::incorrect:
            return "Incorrect";
        case domain::answer_outcome::skipped:
            return "Skipped";
        case domain::answer_outcome::marked_known:
            return "Marked known";
        case domain::answer_outcome::marked_unknown:
            return "Marked unknown";
        case domain::answer_outcome::unanswered:
            return "Unanswered";
    }

    return "Unanswered";
}

inline std::string answer_outcome_name(domain::answer_outcome outcome)
{
    switch (outcome) {
        case domain::answer_outcome::correct:
            return "correct";
        case domain::answer_outcome::incorrect:
            return "incorrect";
        case domain::answer_outcome::skipped:
            return "skipped";
        case domain::answer_outcome::marked_known:
            return "marked_known";
        case domain::answer_outcome::marked_unknown:
            return "marked_unknown";
        case domain::answer_outcome::unanswered:
            return "unanswered";
    }

    return "unanswered";
}

inline std::string bool_string(bool value)
{
    return value ? "true" : "false";
}

inline bool setting_equals(
    const domain::app_snapshot& snapshot,
    std::string_view key,
    std::string_view expected)
{
    const auto found = snapshot.settings.find(to_string_copy(key));
    return found != snapshot.settings.end() && found->second == expected;
}

inline bool settings_screen_requested(const domain::app_snapshot& snapshot)
{
    return setting_equals(snapshot, "ui_screen", "settings")
        || setting_equals(snapshot, "ui.screen", "settings")
        || setting_equals(snapshot, "route", "settings");
}

inline std::string safe_id(std::string_view value, std::string_view fallback)
{
    std::string output;
    output.reserve(value.size());

    for (unsigned char character : value) {
        if (std::isalnum(character) != 0) {
            output.push_back(static_cast<char>(std::tolower(character)));
            continue;
        }

        if (output.empty() || output.back() != '_') {
            output.push_back('_');
        }
    }

    while (!output.empty() && output.back() == '_') {
        output.pop_back();
    }

    if (output.empty()) {
        output = to_string_copy(fallback);
    }

    return output;
}

inline scene::scene_style style(
    std::string token,
    std::string background_color = {},
    std::string foreground_color = "#f6f7f9",
    float border_radius = 0.0f,
    float opacity = 1.0f)
{
    scene::scene_style result;
    result.token = std::move(token);
    result.background_color = std::move(background_color);
    result.foreground_color = std::move(foreground_color);
    result.border_radius = border_radius;
    result.opacity = opacity;
    return result;
}

inline scene::scene_layout_rule vertical_rule(float gap = 0.0f, scene::scene_edges padding = {})
{
    scene::scene_layout_rule rule;
    rule.mode = scene::scene_layout_mode::vertical;
    rule.padding = padding;
    rule.gap = gap;
    rule.horizontal_alignment = scene::scene_alignment::stretch;
    rule.vertical_alignment = scene::scene_alignment::start;
    return rule;
}

inline scene::scene_layout_rule horizontal_rule(float gap = 0.0f, scene::scene_edges padding = {})
{
    scene::scene_layout_rule rule;
    rule.mode = scene::scene_layout_mode::horizontal;
    rule.padding = padding;
    rule.gap = gap;
    rule.horizontal_alignment = scene::scene_alignment::stretch;
    rule.vertical_alignment = scene::scene_alignment::start;
    return rule;
}

inline scene::scene_layout_rule fixed_height_rule(float height, scene::scene_edges padding = {})
{
    scene::scene_layout_rule rule;
    rule.has_height = true;
    rule.height = height;
    rule.padding = padding;
    rule.horizontal_alignment = scene::scene_alignment::stretch;
    rule.vertical_alignment = scene::scene_alignment::start;
    return rule;
}

inline scene::scene_layout_rule image_rule(float height)
{
    scene::scene_layout_rule rule;
    rule.has_height = true;
    rule.height = height;
    rule.horizontal_alignment = scene::scene_alignment::stretch;
    rule.vertical_alignment = scene::scene_alignment::start;
    return rule;
}

inline scene::scene_node_data node(
    std::string id,
    scene::scene_node_kind kind,
    std::string debug_name,
    scene::scene_layout_rule layout_rule = {},
    scene::scene_style node_style = {})
{
    scene::scene_node_data result;
    result.id = std::move(id);
    result.kind = kind;
    result.debug_name = std::move(debug_name);
    result.layout_rule = layout_rule;
    result.style = std::move(node_style);
    return result;
}

inline scene::scene_action_binding press_action(std::string action_type, std::string payload = {})
{
    scene::scene_action_binding action;
    action.trigger = scene::scene_action_trigger::press;
    action.action_type = std::move(action_type);
    action.payload = std::move(payload);
    return action;
}

inline scene::scene_action_binding change_action(std::string action_type, std::string payload = {})
{
    scene::scene_action_binding action;
    action.trigger = scene::scene_action_trigger::change;
    action.action_type = std::move(action_type);
    action.payload = std::move(payload);
    return action;
}

inline scene::scene_quiz_stage quiz_stage_for_screen(quiz_screen_kind screen)
{
    return screen == quiz_screen_kind::quiz_feedback
        ? scene::scene_quiz_stage::feedback
        : scene::scene_quiz_stage::question;
}

inline scene::scene_quiz_feedback_state feedback_state_for_outcome(domain::answer_outcome outcome)
{
    switch (outcome) {
        case domain::answer_outcome::correct:
            return scene::scene_quiz_feedback_state::correct;
        case domain::answer_outcome::incorrect:
            return scene::scene_quiz_feedback_state::incorrect;
        case domain::answer_outcome::skipped:
            return scene::scene_quiz_feedback_state::skipped;
        case domain::answer_outcome::marked_unknown:
            return scene::scene_quiz_feedback_state::marked_unknown;
        case domain::answer_outcome::marked_known:
        case domain::answer_outcome::unanswered:
            return scene::scene_quiz_feedback_state::none;
    }

    return scene::scene_quiz_feedback_state::none;
}

inline scene::scene_node_semantics quiz_node_semantics(
    scene::scene_node_role role,
    scene::scene_quiz_stage stage,
    const domain::question_snapshot& question)
{
    scene::scene_node_semantics semantics;
    semantics.role = role;
    semantics.quiz.stage = stage;
    semantics.quiz.question_length = scene::classify_question_length(
        question.prompt,
        question.long_text.value_or(std::string{}));
    return semantics;
}

inline void append_text(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string id,
    std::string text,
    std::string style_token = "body",
    std::string foreground = "#f6f7f9")
{
    scene::scene_node_data text_node = node(
        std::move(id),
        scene::scene_node_kind::text,
        "text",
        {},
        style(std::move(style_token), {}, std::move(foreground)));
    text_node.layout_rule.horizontal_alignment = scene::scene_alignment::start;
    text_node.text_runs.push_back({std::move(text), text_node.style.token});
    edit_data.append_node(to_string_copy(parent_id), std::move(text_node));
}

inline void append_section(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string id,
    float gap = 8.0f)
{
    scene::scene_node_data section = node(
        std::move(id),
        scene::scene_node_kind::container,
        "section",
        vertical_rule(gap),
        style("section"));
    edit_data.append_node(to_string_copy(parent_id), std::move(section));
}

inline void append_button(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string id,
    std::string label,
    scene::scene_action_binding action,
    std::string style_token = "button")
{
    const std::string action_node_id = id;
    scene::scene_node_data button = node(
        std::move(id),
        scene::scene_node_kind::input,
        "button",
        fixed_height_rule(44.0f, {12.0f, 10.0f, 12.0f, 10.0f}),
        style(std::move(style_token), "#29445a", "#ffffff", 6.0f));
    button.text_runs.push_back({std::move(label), button.style.token});
    edit_data.append_node(to_string_copy(parent_id), std::move(button));
    edit_data.bind_action(action_node_id, std::move(action));
}

inline void append_pill(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string id,
    std::string label,
    std::string style_token)
{
    scene::scene_node_data pill = node(
        std::move(id),
        scene::scene_node_kind::text,
        "pill",
        fixed_height_rule(32.0f, {10.0f, 7.0f, 10.0f, 7.0f}),
        style(std::move(style_token), "#1f2f3d", "#edf4f8", 6.0f));
    pill.text_runs.push_back({std::move(label), pill.style.token});
    edit_data.append_node(to_string_copy(parent_id), std::move(pill));
}

inline std::string layout_contract_name(quiz_screen_kind screen)
{
    switch (screen) {
        case quiz_screen_kind::deck_list:
            return "deck_grid";
        case quiz_screen_kind::deck_view:
            return "day_list";
        case quiz_screen_kind::day_intro:
            return "day_intro_actions";
        case quiz_screen_kind::quiz_active:
            return "quiz_question";
        case quiz_screen_kind::quiz_feedback:
            return "quiz_feedback";
        case quiz_screen_kind::quiz_results:
            return "results_actions";
        case quiz_screen_kind::settings:
            return "settings_list";
        case quiz_screen_kind::error:
            return "error_recovery";
    }

    return "deck_grid";
}

inline scene::scene_route_state route_for(
    quiz_screen_kind screen,
    const domain::app_snapshot& snapshot)
{
    const std::string name = screen_name(screen);

    scene::scene_route_state route;
    route.route_id = name;
    route.screen_id = name;
    route.metadata["descriptor_version"] = "quiz_screen_descriptor_v1";
    route.metadata["screen_kind"] = name;
    route.metadata["layout_contract"] = layout_contract_name(screen);
    route.metadata["domain_screen"] = app_screen_name(snapshot.screen);
    route.metadata["deck_count"] = std::to_string(snapshot.decks.size());
    route.metadata["settings_count"] = std::to_string(snapshot.settings.size());
    route.metadata["has_error"] = bool_string(snapshot.error_message.has_value());
    route.metadata["learning_question_count"] = std::to_string(snapshot.learning.question_count);
    route.metadata["learning_count"] = std::to_string(snapshot.learning.learning_count);
    route.metadata["known_count"] = std::to_string(snapshot.learning.known_count);
    route.metadata["unknown_count"] = std::to_string(snapshot.learning.unknown_count);
    route.metadata["wrong_note_count"] = std::to_string(snapshot.learning.wrong_note_count);
    route.metadata["keyboard_safe_layout"] = bool_string(
        screen == quiz_screen_kind::quiz_active || screen == quiz_screen_kind::quiz_feedback);
    const bool has_pending_feedback = snapshot.active_session.has_value() && snapshot.active_session->feedback.has_value();
    route.metadata["input_locked"] = bool_string(
        screen == quiz_screen_kind::quiz_feedback
        || screen == quiz_screen_kind::quiz_results
        || screen == quiz_screen_kind::settings
        || screen == quiz_screen_kind::error
        || has_pending_feedback);

    if (snapshot.selected_deck_id.has_value()) {
        route.metadata["selected_deck_id"] = *snapshot.selected_deck_id;
    }
    if (snapshot.selected_day_id.has_value()) {
        route.metadata["selected_day_id"] = *snapshot.selected_day_id;
    }

    const domain::deck* selected_deck = nullptr;
    if (snapshot.selected_deck_id.has_value()) {
        const auto deck_match = std::find_if(
            snapshot.decks.begin(),
            snapshot.decks.end(),
            [&snapshot](const domain::deck& candidate) {
                return candidate.id == *snapshot.selected_deck_id;
            });
        if (deck_match != snapshot.decks.end()) {
            selected_deck = &(*deck_match);
        }
    }

    if (selected_deck != nullptr) {
        std::size_t selected_deck_question_count = 0;
        for (const domain::day& quiz_day : selected_deck->days) {
            selected_deck_question_count += quiz_day.questions.size();
        }

        route.metadata["selected_deck_title"] = selected_deck->title;
        route.metadata["selected_deck_day_count"] = std::to_string(selected_deck->days.size());
        route.metadata["selected_deck_question_count"] = std::to_string(selected_deck_question_count);

        if (snapshot.selected_day_id.has_value()) {
            const auto day_match = std::find_if(
                selected_deck->days.begin(),
                selected_deck->days.end(),
                [&snapshot](const domain::day& candidate) {
                    return candidate.id == *snapshot.selected_day_id;
                });
            if (day_match != selected_deck->days.end()) {
                route.metadata["selected_day_title"] = day_match->title;
                route.metadata["selected_day_question_count"] = std::to_string(day_match->questions.size());
            }
        }
    }

    if (snapshot.active_session.has_value()) {
        const domain::session_snapshot& session = *snapshot.active_session;
        route.metadata["quiz_mode"] = quiz_mode_name(session.mode);
        route.metadata["session_phase"] = session_phase_name(session.phase);
        route.metadata["question_count"] = std::to_string(session.question_count);
        route.metadata["current_index"] = std::to_string(session.current_index);
        route.metadata["completed"] = bool_string(session.completed);
        route.metadata["has_feedback"] = bool_string(session.feedback.has_value());

        if (session.current_question.has_value()) {
            const domain::question_snapshot& question = *session.current_question;
            route.metadata["question_id"] = question.question_id;
            route.metadata["question_type"] = question_type_name(question.type);
            route.metadata["question_learning_state"] = learning_state_name(question.learning);
            route.metadata["question_option_count"] = std::to_string(question.options.size());
            route.metadata["question_has_image"] = bool_string(question.image_uri.has_value() && !question.image_uri->empty());
            route.metadata["question_has_long_text"] = bool_string(question.long_text.has_value() && !question.long_text->empty());
        }

        if (session.feedback.has_value()) {
            route.metadata["feedback_outcome"] = answer_outcome_name(session.feedback->outcome);
            route.metadata["feedback_question_id"] = session.feedback->question_id;
            route.metadata["feedback_selected_option_count"] = std::to_string(session.feedback->selected_option_indexes.size());
            route.metadata["feedback_submitted_text_count"] = std::to_string(session.feedback->submitted_text_answers.size());
        }
    } else {
        route.metadata["has_feedback"] = "false";
    }

    return route;
}

inline quiz_screen_descriptor descriptor_for(
    quiz_screen_kind screen,
    const domain::app_snapshot& snapshot);

inline void append_screen_shell(
    scene::scene_layout_edit_data& edit_data,
    quiz_screen_kind screen,
    const domain::app_snapshot& snapshot)
{
    edit_data.set_route(descriptor_for(screen, snapshot).route);

    scene::scene_node_data root = node(
        to_string_copy(quiz_screens_root_id),
        scene::scene_node_kind::container,
        "quiz screens root",
        vertical_rule(14.0f, {24.0f, 24.0f, 24.0f, 24.0f}),
        style("screen", "#101820", "#f6f7f9"));
    root.layout_rule.respect_safe_area = true;
    root.layout_rule.avoid_keyboard = true;
    root.layout_rule.clip_children = true;
    root.semantics.role = scene::scene_node_role::app_shell;
    edit_data.append_node("", std::move(root));
}

inline void append_header(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string_view prefix,
    std::string title,
    std::string subtitle = {})
{
    const std::string section_id = to_string_copy(prefix) + "_header";
    append_section(edit_data, parent_id, section_id, 4.0f);
    append_text(edit_data, section_id, to_string_copy(prefix) + "_title", std::move(title), "heading", "#ffffff");
    if (!subtitle.empty()) {
        append_text(edit_data, section_id, to_string_copy(prefix) + "_subtitle", std::move(subtitle), "muted", "#aeb9c2");
    }
}

inline void append_empty_state(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string_view prefix,
    std::string message)
{
    append_text(edit_data, parent_id, to_string_copy(prefix) + "_empty_state", std::move(message), "muted", "#aeb9c2");
}

inline const domain::deck* find_selected_deck(const domain::app_snapshot& snapshot)
{
    if (!snapshot.selected_deck_id.has_value()) {
        return nullptr;
    }

    const auto found = std::find_if(
        snapshot.decks.begin(),
        snapshot.decks.end(),
        [&snapshot](const domain::deck& candidate) {
            return candidate.id == *snapshot.selected_deck_id;
        });

    return found == snapshot.decks.end() ? nullptr : &(*found);
}

inline const domain::day* find_selected_day(const domain::deck& deck, const domain::app_snapshot& snapshot)
{
    if (!snapshot.selected_day_id.has_value()) {
        return nullptr;
    }

    const auto found = std::find_if(
        deck.days.begin(),
        deck.days.end(),
        [&snapshot](const domain::day& candidate) {
            return candidate.id == *snapshot.selected_day_id;
        });

    return found == deck.days.end() ? nullptr : &(*found);
}

inline bool uses_option_answers(const domain::question_snapshot& question);

inline quiz_screen_descriptor descriptor_for(
    quiz_screen_kind screen,
    const domain::app_snapshot& snapshot)
{
    quiz_screen_descriptor descriptor;
    descriptor.kind = screen;
    descriptor.route = route_for(screen, snapshot);
    descriptor.root_node_id = to_string_copy(quiz_screens_root_id);
    descriptor.input_locked = descriptor.route.metadata["input_locked"] == "true";
    descriptor.shows_settings = screen == quiz_screen_kind::settings;
    descriptor.shows_error = snapshot.error_message.has_value() || screen == quiz_screen_kind::error;

    const domain::deck* selected_deck = find_selected_deck(snapshot);
    const domain::day* selected_day = selected_deck == nullptr ? nullptr : find_selected_day(*selected_deck, snapshot);

    switch (screen) {
        case quiz_screen_kind::deck_list:
            descriptor.title = "Decks";
            descriptor.subtitle = std::to_string(snapshot.decks.size()) + " decks available";
            if (!snapshot.decks.empty()) {
                descriptor.focus_node_id = "deck_list_deck_" + safe_id(snapshot.decks.front().id, "0");
            }
            break;
        case quiz_screen_kind::deck_view:
            descriptor.title = selected_deck == nullptr ? "Select a deck" : selected_deck->title;
            descriptor.subtitle = selected_deck == nullptr ? std::string{} : descriptor.route.metadata["selected_deck_question_count"] + " questions";
            if (selected_deck != nullptr && !selected_deck->days.empty()) {
                descriptor.focus_node_id = "deck_view_day_" + safe_id(selected_deck->days.front().id, "0");
            } else if (selected_deck != nullptr) {
                descriptor.focus_node_id = "deck_view_start_all";
            }
            break;
        case quiz_screen_kind::day_intro:
            descriptor.title = selected_day == nullptr ? "Choose a day" : selected_day->title;
            descriptor.subtitle = selected_deck == nullptr || selected_day == nullptr
                ? std::string{}
                : selected_deck->title + " / " + std::to_string(selected_day->questions.size()) + " questions";
            descriptor.focus_node_id = selected_day == nullptr ? std::string{} : "day_intro_start_normal";
            break;
        case quiz_screen_kind::quiz_active:
        case quiz_screen_kind::quiz_feedback:
            descriptor.title = screen == quiz_screen_kind::quiz_feedback ? "Feedback" : "Quiz";
            if (snapshot.active_session.has_value()) {
                const domain::session_snapshot& session = *snapshot.active_session;
                descriptor.subtitle = "Mode: " + quiz_mode_name(session.mode) + " / Phase: " + session_phase_name(session.phase);
                if (screen == quiz_screen_kind::quiz_feedback) {
                    descriptor.focus_node_id = "quiz_feedback_continue";
                } else if (session.current_question.has_value() && uses_option_answers(*session.current_question) && !session.current_question->options.empty()) {
                    descriptor.focus_node_id = "quiz_active_option_0";
                } else {
                    descriptor.focus_node_id = "quiz_active_text_answer";
                }
            }
            break;
        case quiz_screen_kind::quiz_results:
            descriptor.title = "Quiz results";
            descriptor.subtitle = "Completed "
                + (snapshot.active_session.has_value() ? std::to_string(snapshot.active_session->question_count) : std::string("0"))
                + " questions";
            descriptor.focus_node_id = "quiz_results_start_normal";
            break;
        case quiz_screen_kind::settings:
            descriptor.title = "Settings";
            descriptor.subtitle = std::to_string(snapshot.settings.size()) + " entries";
            descriptor.focus_node_id = "settings_close";
            break;
        case quiz_screen_kind::error:
            descriptor.title = "Error";
            descriptor.subtitle = snapshot.error_message.value_or("Unknown error");
            if (!snapshot.decks.empty()) {
                descriptor.focus_node_id = "error_deck_" + safe_id(snapshot.decks.front().id, "0");
            }
            break;
    }

    descriptor.route.metadata["title"] = descriptor.title;
    descriptor.route.metadata["subtitle"] = descriptor.subtitle;
    if (!descriptor.focus_node_id.empty()) {
        descriptor.route.metadata["focus_node_id"] = descriptor.focus_node_id;
    }

    return descriptor;
}

inline std::size_t question_count(const domain::deck& deck)
{
    std::size_t count = 0;
    for (const domain::day& quiz_day : deck.days) {
        count += quiz_day.questions.size();
    }
    return count;
}

inline std::string deck_summary(const domain::deck& deck)
{
    return std::to_string(deck.days.size()) + " days / " + std::to_string(question_count(deck)) + " questions";
}

inline std::string learning_summary_text(const domain::learning_summary& summary)
{
    return "Learning " + std::to_string(summary.learning_count)
        + " / Known " + std::to_string(summary.known_count)
        + " / Unknown " + std::to_string(summary.unknown_count)
        + " / Wrong note " + std::to_string(summary.wrong_note_count);
}

inline void append_learning_summary(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string_view prefix,
    const domain::learning_summary& summary)
{
    const std::string row_id = to_string_copy(prefix) + "_learning_summary";
    scene::scene_node_data row = node(
        row_id,
        scene::scene_node_kind::container,
        "learning summary",
        horizontal_rule(8.0f),
        style("summary_row"));
    edit_data.append_node(to_string_copy(parent_id), std::move(row));

    append_pill(edit_data, row_id, to_string_copy(prefix) + "_learning_count", "Learning " + std::to_string(summary.learning_count), "learning_pill");
    append_pill(edit_data, row_id, to_string_copy(prefix) + "_known_count", "Known " + std::to_string(summary.known_count), "known_pill");
    append_pill(edit_data, row_id, to_string_copy(prefix) + "_unknown_count", "Unknown " + std::to_string(summary.unknown_count), "unknown_pill");
    append_pill(edit_data, row_id, to_string_copy(prefix) + "_wrong_note_count", "Wrong note " + std::to_string(summary.wrong_note_count), "wrong_note_pill");
}

inline void append_error_banner(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string_view prefix,
    const domain::app_snapshot& snapshot)
{
    if (!snapshot.error_message.has_value()) {
        return;
    }

    scene::scene_node_data banner = node(
        to_string_copy(prefix) + "_error_banner",
        scene::scene_node_kind::text,
        "error banner",
        fixed_height_rule(42.0f, {12.0f, 10.0f, 12.0f, 10.0f}),
        style("error", "#553135", "#ffffff", 6.0f));
    banner.text_runs.push_back({"Error: " + *snapshot.error_message, "error"});
    edit_data.append_node(to_string_copy(parent_id), std::move(banner));
}

inline bool uses_option_answers(const domain::question_snapshot& question)
{
    return !question.options.empty()
        && (question.type == domain::question_type::answer
            || question.type == domain::question_type::multiselect
            || question.type == domain::question_type::short_);
}

inline void append_start_mode_button(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string_view prefix,
    domain::quiz_mode mode,
    std::string label)
{
    const std::string mode_name = quiz_mode_name(mode);
    append_button(
        edit_data,
        parent_id,
        to_string_copy(prefix) + "_start_" + mode_name,
        std::move(label),
        press_action("start_quiz", mode_name),
        "button");
}

inline void tag_mode_action_group(scene::scene_layout_edit_data& edit_data, std::string_view node_id)
{
    scene::scene_node_semantics semantics;
    semantics.role = scene::scene_node_role::quiz_controls;
    semantics.label = "Quiz modes";
    edit_data.set_semantics(to_string_copy(node_id), std::move(semantics));
}

inline void append_learning_mode_buttons(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string_view prefix,
    const domain::learning_summary& summary)
{
    if (summary.known_count > 0) {
        append_start_mode_button(edit_data, parent_id, prefix, domain::quiz_mode::known, "Known questions");
    }

    if (summary.wrong_note_count > 0) {
        append_start_mode_button(edit_data, parent_id, prefix, domain::quiz_mode::wrong_note, "Wrong notes");
    }
}

inline void append_feedback_banner(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string_view prefix,
    const domain::answer_record& feedback)
{
    const bool is_correct = feedback.outcome == domain::answer_outcome::correct;
    scene::scene_node_data banner = node(
        to_string_copy(prefix) + "_feedback",
        scene::scene_node_kind::text,
        "feedback",
        fixed_height_rule(44.0f, {12.0f, 10.0f, 12.0f, 10.0f}),
        style(is_correct ? "feedback_correct" : "feedback_needs_work", is_correct ? "#28503a" : "#5a3d2c", "#ffffff", 6.0f));
    banner.text_runs.push_back({display_outcome(feedback.outcome), banner.style.token});
    banner.semantics.role = scene::scene_node_role::quiz_feedback;
    banner.semantics.quiz.stage = scene::scene_quiz_stage::feedback;
    banner.semantics.quiz.feedback = feedback_state_for_outcome(feedback.outcome);
    edit_data.append_node(to_string_copy(parent_id), std::move(banner));
}

inline void append_question_options(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string_view prefix,
    const domain::question_snapshot& question,
    bool feedback_visible)
{
    const std::string section_id = to_string_copy(prefix) + "_options";
    append_section(edit_data, parent_id, section_id, 8.0f);
    edit_data.set_semantics(
        section_id,
        quiz_node_semantics(
            scene::scene_node_role::quiz_option_group,
            feedback_visible ? scene::scene_quiz_stage::feedback : scene::scene_quiz_stage::question,
            question));

    for (std::size_t index = 0; index < question.options.size(); ++index) {
        const domain::option_snapshot& option = question.options[index];
        std::string style_token = "button";
        if (option.reveal_correctness) {
            style_token = option.is_correct ? "option_correct" : "option_incorrect";
        }

        const std::string option_id = to_string_copy(prefix) + "_option_" + std::to_string(index);
        scene::scene_node_data option_node = node(
            option_id,
            scene::scene_node_kind::input,
            "answer option",
            fixed_height_rule(44.0f, {12.0f, 10.0f, 12.0f, 10.0f}),
            style(style_token, option.is_correct && option.reveal_correctness ? "#28503a" : "#29445a", "#ffffff", 6.0f));
        if (option.reveal_correctness && !option.is_correct) {
            option_node.style.background_color = "#49333b";
        }
        option_node.text_runs.push_back({option.text, option_node.style.token});
        option_node.input_enabled = !feedback_visible;
        option_node.semantics = quiz_node_semantics(
            scene::scene_node_role::quiz_option,
            feedback_visible ? scene::scene_quiz_stage::feedback : scene::scene_quiz_stage::question,
            question);
        option_node.semantics.quiz.option_index = index;
        if (option.reveal_correctness) {
            option_node.semantics.quiz.option_state = option.is_correct
                ? scene::scene_quiz_option_state::correct
                : scene::scene_quiz_option_state::incorrect;
            option_node.semantics.quiz.reveal_correctness = true;
        } else if (feedback_visible) {
            option_node.semantics.quiz.option_state = scene::scene_quiz_option_state::disabled;
        }
        edit_data.append_node(section_id, std::move(option_node));

        if (!feedback_visible) {
            const std::string action_type = question.type == domain::question_type::multiselect
                ? "submit_multiselect"
                : "submit_option";
            edit_data.bind_action(option_id, press_action(action_type, std::to_string(index)));
        }
    }
}

inline void append_text_answer_input(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    std::string_view prefix,
    const domain::question_snapshot& question)
{
    const std::string dock_id = to_string_copy(prefix) + "_answer_dock";
    const std::string input_id = to_string_copy(prefix) + "_text_answer";

    scene::scene_layout_rule dock_rule = vertical_rule(8.0f);
    dock_rule.has_height = true;
    dock_rule.height = 100.0f;
    dock_rule.anchor_to_keyboard = true;

    scene::scene_node_data dock = node(
        dock_id,
        scene::scene_node_kind::container,
        "answer dock",
        dock_rule,
        style("answer_dock"));
    dock.semantics = quiz_node_semantics(
        scene::scene_node_role::quiz_answer_dock,
        scene::scene_quiz_stage::question,
        question);
    edit_data.append_node(to_string_copy(parent_id), std::move(dock));

    scene::scene_node_data input = node(
        input_id,
        scene::scene_node_kind::input,
        "text answer input",
        fixed_height_rule(48.0f, {12.0f, 12.0f, 12.0f, 12.0f}),
        style("text_input", "#162532", "#ffffff", 6.0f));
    input.text_runs.push_back({"Type answer", "text_input"});
    input.semantics = quiz_node_semantics(
        scene::scene_node_role::quiz_answer_input,
        scene::scene_quiz_stage::question,
        question);
    input.semantics.quiz.accepts_keyboard_input = true;
    edit_data.append_node(dock_id, std::move(input));
    edit_data.bind_action(input_id, change_action("submit_text_answer", question.question_id));

    append_button(
        edit_data,
        dock_id,
        to_string_copy(prefix) + "_submit_text",
        "Submit answer",
        press_action("submit_text_answer", question.question_id),
        "button");
}

} // namespace detail

inline quiz_screen_kind screen_kind_for_snapshot(const domain::app_snapshot& snapshot)
{
    if (snapshot.screen == domain::app_screen::error) {
        return quiz_screen_kind::error;
    }

    if (detail::settings_screen_requested(snapshot)) {
        return quiz_screen_kind::settings;
    }

    switch (snapshot.screen) {
        case domain::app_screen::deck_select:
        case domain::app_screen::loading:
            return quiz_screen_kind::deck_list;
        case domain::app_screen::day_select:
            return snapshot.selected_day_id.has_value() ? quiz_screen_kind::day_intro : quiz_screen_kind::deck_view;
        case domain::app_screen::quiz:
            return quiz_screen_kind::quiz_active;
        case domain::app_screen::feedback:
            return quiz_screen_kind::quiz_feedback;
        case domain::app_screen::completed:
            return quiz_screen_kind::quiz_results;
        case domain::app_screen::error:
            return quiz_screen_kind::error;
    }

    return quiz_screen_kind::deck_list;
}

inline quiz_screen_descriptor describe_quiz_screen(
    quiz_screen_kind screen,
    const domain::app_snapshot& snapshot)
{
    return detail::descriptor_for(screen, snapshot);
}

inline quiz_screen_descriptor describe_quiz_screen(const domain::app_snapshot& snapshot)
{
    return describe_quiz_screen(screen_kind_for_snapshot(snapshot), snapshot);
}

inline void build_deck_list_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    detail::append_screen_shell(edit_data, quiz_screen_kind::deck_list, snapshot);
    detail::append_header(
        edit_data,
        detail::quiz_screens_root_id,
        "deck_list",
        "Decks",
        std::to_string(snapshot.decks.size()) + " decks available");
    detail::append_learning_summary(edit_data, detail::quiz_screens_root_id, "deck_list", snapshot.learning);

    detail::append_section(edit_data, detail::quiz_screens_root_id, "deck_list_decks", 8.0f);
    detail::append_text(edit_data, "deck_list_decks", "deck_list_decks_label", "Decks", "section_label", "#d8e1e7");

    if (snapshot.decks.empty()) {
        detail::append_empty_state(edit_data, "deck_list_decks", "deck_list_decks", "No decks loaded");
        return;
    }

    for (std::size_t index = 0; index < snapshot.decks.size(); ++index) {
        const domain::deck& deck = snapshot.decks[index];
        const std::string deck_id = "deck_list_deck_" + detail::safe_id(deck.id, std::to_string(index));
        detail::append_button(
            edit_data,
            "deck_list_decks",
            deck_id,
            deck.title + " - " + detail::deck_summary(deck),
            detail::press_action("select_deck", deck.id),
            "deck_button");
    }

    edit_data.set_focus("deck_list_deck_" + detail::safe_id(snapshot.decks.front().id, "0"));
}

inline void build_home_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    build_deck_list_screen(snapshot, edit_data);
}

inline void build_deck_view_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    detail::append_screen_shell(edit_data, quiz_screen_kind::deck_view, snapshot);

    const domain::deck* selected_deck = detail::find_selected_deck(snapshot);
    if (selected_deck == nullptr) {
        detail::append_header(edit_data, detail::quiz_screens_root_id, "deck_view", "Select a deck");
        detail::append_empty_state(edit_data, detail::quiz_screens_root_id, "deck_view", "No selected deck");
        return;
    }

    detail::append_header(
        edit_data,
        detail::quiz_screens_root_id,
        "deck_view",
        selected_deck->title,
        detail::deck_summary(*selected_deck));
    detail::append_learning_summary(edit_data, detail::quiz_screens_root_id, "deck_view", snapshot.learning);
    detail::append_button(
        edit_data,
        detail::quiz_screens_root_id,
        "deck_view_start_all",
        "Start all due questions",
        detail::press_action("start_quiz", "normal"),
        "button");

    detail::append_section(edit_data, detail::quiz_screens_root_id, "deck_view_days", 8.0f);
    detail::append_text(edit_data, "deck_view_days", "deck_view_days_label", "Days", "section_label", "#d8e1e7");

    if (selected_deck->days.empty()) {
        detail::append_empty_state(edit_data, "deck_view_days", "deck_view_days", "No days in this deck");
        edit_data.set_focus("deck_view_start_all");
        return;
    }

    for (std::size_t index = 0; index < selected_deck->days.size(); ++index) {
        const domain::day& quiz_day = selected_deck->days[index];
        const std::string day_id = "deck_view_day_" + detail::safe_id(quiz_day.id, std::to_string(index));
        detail::append_button(
            edit_data,
            "deck_view_days",
            day_id,
            quiz_day.title + " - " + std::to_string(quiz_day.questions.size()) + " questions",
            detail::press_action("select_day", quiz_day.id),
            "day_button");
    }

    edit_data.set_focus("deck_view_day_" + detail::safe_id(selected_deck->days.front().id, "0"));
}

inline void build_day_intro_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    detail::append_screen_shell(edit_data, quiz_screen_kind::day_intro, snapshot);

    const domain::deck* selected_deck = detail::find_selected_deck(snapshot);
    const domain::day* selected_day = selected_deck == nullptr ? nullptr : detail::find_selected_day(*selected_deck, snapshot);
    if (selected_deck == nullptr || selected_day == nullptr) {
        detail::append_header(edit_data, detail::quiz_screens_root_id, "day_intro", "Choose a day");
        detail::append_empty_state(edit_data, detail::quiz_screens_root_id, "day_intro", "No selected day");
        return;
    }

    detail::append_header(
        edit_data,
        detail::quiz_screens_root_id,
        "day_intro",
        selected_day->title,
        selected_deck->title + " / " + std::to_string(selected_day->questions.size()) + " questions");
    detail::append_text(
        edit_data,
        detail::quiz_screens_root_id,
        "day_intro_summary",
        detail::learning_summary_text(snapshot.learning),
        "muted",
        "#aeb9c2");

    detail::append_section(edit_data, detail::quiz_screens_root_id, "day_intro_modes", 8.0f);
    detail::tag_mode_action_group(edit_data, "day_intro_modes");
    detail::append_start_mode_button(edit_data, "day_intro_modes", "day_intro", domain::quiz_mode::normal, "Start due questions");
    detail::append_start_mode_button(edit_data, "day_intro_modes", "day_intro", domain::quiz_mode::random, "Random order");
    detail::append_learning_mode_buttons(edit_data, "day_intro_modes", "day_intro", snapshot.learning);
    edit_data.set_focus("day_intro_start_normal");
}

inline void build_quiz_session_screen(
    const domain::app_snapshot& snapshot,
    scene::scene_layout_edit_data& edit_data,
    quiz_screen_kind screen,
    std::string_view prefix)
{
    detail::append_screen_shell(edit_data, screen, snapshot);

    if (!snapshot.active_session.has_value()) {
        detail::append_header(edit_data, detail::quiz_screens_root_id, prefix, screen == quiz_screen_kind::quiz_feedback ? "Feedback" : "Quiz");
        detail::append_empty_state(edit_data, detail::quiz_screens_root_id, prefix, "No active quiz");
        return;
    }

    const domain::session_snapshot& session = *snapshot.active_session;
    const std::string progress = session.question_count == 0
        ? "No questions"
        : "Question " + std::to_string(std::min(session.current_index + 1, session.question_count))
            + " of " + std::to_string(session.question_count);

    detail::append_header(
        edit_data,
        detail::quiz_screens_root_id,
        prefix,
        progress,
        "Mode: " + detail::quiz_mode_name(session.mode) + " / Phase: " + detail::session_phase_name(session.phase));

    if (!session.current_question.has_value()) {
        detail::append_empty_state(edit_data, detail::quiz_screens_root_id, prefix, "No current question");
        return;
    }

    const domain::question_snapshot& question = *session.current_question;
    const bool feedback_visible = screen == quiz_screen_kind::quiz_feedback || session.feedback.has_value();
    const scene::scene_quiz_stage quiz_stage = detail::quiz_stage_for_screen(screen);
    if (feedback_visible) {
        if (session.feedback.has_value()) {
            detail::append_feedback_banner(edit_data, detail::quiz_screens_root_id, prefix, *session.feedback);
        } else {
            detail::append_empty_state(edit_data, detail::quiz_screens_root_id, prefix, "No pending feedback");
        }
    }

    detail::append_pill(
        edit_data,
        detail::quiz_screens_root_id,
        detail::to_string_copy(prefix) + "_learning_state",
        detail::question_type_name(question.type) + " / " + detail::learning_state_name(question.learning),
        "question_meta");

    const std::string prompt_id = detail::to_string_copy(prefix) + "_prompt";
    detail::append_text(edit_data, detail::quiz_screens_root_id, prompt_id, question.prompt, "prompt", "#ffffff");
    edit_data.set_semantics(
        prompt_id,
        detail::quiz_node_semantics(scene::scene_node_role::quiz_question_prompt, quiz_stage, question));

    if (question.long_text.has_value() && !question.long_text->empty()) {
        const std::string body_id = detail::to_string_copy(prefix) + "_long_text";
        detail::append_text(edit_data, detail::quiz_screens_root_id, body_id, *question.long_text, "body", "#d8e1e7");
        edit_data.set_semantics(
            body_id,
            detail::quiz_node_semantics(scene::scene_node_role::quiz_question_body, quiz_stage, question));
    }

    if (question.image_uri.has_value() && !question.image_uri->empty()) {
        scene::scene_node_data image = detail::node(
            detail::to_string_copy(prefix) + "_image",
            scene::scene_node_kind::image,
            "question image",
            detail::image_rule(180.0f),
            detail::style("image_frame", "#162532", "#ffffff", 6.0f));
        image.image.uri = *question.image_uri;
        image.image.alt_text = question.prompt;
        image.image.aspect_ratio = 1.777778f;
        image.has_image = true;
        image.semantics = detail::quiz_node_semantics(scene::scene_node_role::quiz_question_image, quiz_stage, question);
        edit_data.append_node(detail::to_string_copy(detail::quiz_screens_root_id), std::move(image));
    }

    if (detail::uses_option_answers(question)) {
        detail::append_question_options(edit_data, detail::quiz_screens_root_id, prefix, question, feedback_visible);
        if (!feedback_visible && !question.options.empty()) {
            edit_data.set_focus(detail::to_string_copy(prefix) + "_option_0");
        }
    } else if (!feedback_visible) {
        detail::append_text_answer_input(edit_data, detail::quiz_screens_root_id, prefix, question);
        edit_data.set_focus(detail::to_string_copy(prefix) + "_text_answer");
    }

    if (feedback_visible) {
        detail::append_button(
            edit_data,
            detail::quiz_screens_root_id,
            detail::to_string_copy(prefix) + "_continue",
            "Continue",
            detail::press_action("continue_after_feedback"),
            "button");
        edit_data.set_focus(detail::to_string_copy(prefix) + "_continue");
        return;
    }

    const std::string controls_id = detail::to_string_copy(prefix) + "_controls";
    detail::append_section(edit_data, detail::quiz_screens_root_id, controls_id, 8.0f);
    detail::append_button(
        edit_data,
        controls_id,
        detail::to_string_copy(prefix) + "_skip",
        "Skip",
        detail::press_action("skip_question"),
        "secondary_button");
    detail::append_button(
        edit_data,
        controls_id,
        detail::to_string_copy(prefix) + "_mark_unknown",
        "Mark unknown",
        detail::press_action("mark_question_unknown"),
        "secondary_button");
}

inline void build_quiz_active_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    build_quiz_session_screen(snapshot, edit_data, quiz_screen_kind::quiz_active, "quiz_active");
}

inline void build_quiz_feedback_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    build_quiz_session_screen(snapshot, edit_data, quiz_screen_kind::quiz_feedback, "quiz_feedback");
}

inline void build_quiz_results_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    detail::append_screen_shell(edit_data, quiz_screen_kind::quiz_results, snapshot);

    const std::string completed_count = snapshot.active_session.has_value()
        ? std::to_string(snapshot.active_session->question_count)
        : std::string("0");
    detail::append_header(
        edit_data,
        detail::quiz_screens_root_id,
        "quiz_results",
        "Quiz results",
        "Completed " + completed_count + " questions");
    detail::append_learning_summary(edit_data, detail::quiz_screens_root_id, "quiz_results", snapshot.learning);

    detail::append_section(edit_data, detail::quiz_screens_root_id, "quiz_results_actions", 8.0f);
    detail::tag_mode_action_group(edit_data, "quiz_results_actions");
    detail::append_start_mode_button(edit_data, "quiz_results_actions", "quiz_results", domain::quiz_mode::normal, "Restart due questions");
    detail::append_learning_mode_buttons(edit_data, "quiz_results_actions", "quiz_results", snapshot.learning);
    edit_data.set_focus("quiz_results_start_normal");
}

inline void build_settings_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    detail::append_screen_shell(edit_data, quiz_screen_kind::settings, snapshot);
    detail::append_header(
        edit_data,
        detail::quiz_screens_root_id,
        "settings",
        "Settings",
        std::to_string(snapshot.settings.size()) + " entries");

    detail::append_learning_summary(edit_data, detail::quiz_screens_root_id, "settings", snapshot.learning);
    detail::append_section(edit_data, detail::quiz_screens_root_id, "settings_entries", 6.0f);
    detail::append_text(edit_data, "settings_entries", "settings_entries_label", "App settings", "section_label", "#d8e1e7");

    if (snapshot.settings.empty()) {
        detail::append_empty_state(edit_data, "settings_entries", "settings_entries", "No settings configured");
    } else {
        std::vector<std::pair<std::string, std::string>> entries;
        entries.reserve(snapshot.settings.size());
        for (const auto& [key, value] : snapshot.settings) {
            entries.push_back({key, value});
        }
        std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });

        std::size_t index = 0;
        for (const auto& [key, value] : entries) {
            detail::append_text(
                edit_data,
                "settings_entries",
                "settings_entry_" + detail::safe_id(key, std::to_string(index)),
                key + " = " + value,
                "setting_value",
                "#d8e1e7");
            ++index;
        }
    }

    detail::append_button(
        edit_data,
        detail::quiz_screens_root_id,
        "settings_close",
        "Back to decks",
        detail::press_action("update_setting", "ui_screen=deck_list"),
        "secondary_button");
    edit_data.set_focus("settings_close");
}

inline void build_error_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    detail::append_screen_shell(edit_data, quiz_screen_kind::error, snapshot);
    detail::append_header(
        edit_data,
        detail::quiz_screens_root_id,
        "error",
        "Error",
        snapshot.error_message.value_or("Unknown error"));
    detail::append_error_banner(edit_data, detail::quiz_screens_root_id, "error", snapshot);

    detail::append_section(edit_data, detail::quiz_screens_root_id, "error_recovery", 8.0f);
    detail::append_text(edit_data, "error_recovery", "error_recovery_label", "Recovery", "section_label", "#d8e1e7");

    if (snapshot.decks.empty()) {
        detail::append_empty_state(edit_data, "error_recovery", "error_recovery", "Load a source before starting");
        return;
    }

    for (std::size_t index = 0; index < snapshot.decks.size(); ++index) {
        const domain::deck& deck = snapshot.decks[index];
        const std::string deck_id = "error_deck_" + detail::safe_id(deck.id, std::to_string(index));
        detail::append_button(
            edit_data,
            "error_recovery",
            deck_id,
            "Open " + deck.title,
            detail::press_action("select_deck", deck.id),
            "deck_button");
    }

    edit_data.set_focus("error_deck_" + detail::safe_id(snapshot.decks.front().id, "0"));
}

inline void build_quiz_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    switch (screen_kind_for_snapshot(snapshot)) {
        case quiz_screen_kind::deck_list:
            build_deck_list_screen(snapshot, edit_data);
            return;
        case quiz_screen_kind::deck_view:
            build_deck_view_screen(snapshot, edit_data);
            return;
        case quiz_screen_kind::day_intro:
            build_day_intro_screen(snapshot, edit_data);
            return;
        case quiz_screen_kind::quiz_active:
            build_quiz_active_screen(snapshot, edit_data);
            return;
        case quiz_screen_kind::quiz_feedback:
            build_quiz_feedback_screen(snapshot, edit_data);
            return;
        case quiz_screen_kind::quiz_results:
            build_quiz_results_screen(snapshot, edit_data);
            return;
        case quiz_screen_kind::settings:
            build_settings_screen(snapshot, edit_data);
            return;
        case quiz_screen_kind::error:
            build_error_screen(snapshot, edit_data);
            return;
    }
}

inline scene::scene_layout_patch make_deck_list_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("deck_list_screen");
    build_deck_list_screen(snapshot, edit_data);
    return edit_data.finish_patch();
}

inline scene::scene_layout_patch make_home_screen_patch(const domain::app_snapshot& snapshot)
{
    return make_deck_list_screen_patch(snapshot);
}

inline scene::scene_layout_patch make_deck_view_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("deck_view_screen");
    build_deck_view_screen(snapshot, edit_data);
    return edit_data.finish_patch();
}

inline scene::scene_layout_patch make_day_intro_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("day_intro_screen");
    build_day_intro_screen(snapshot, edit_data);
    return edit_data.finish_patch();
}

inline scene::scene_layout_patch make_quiz_active_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("quiz_active_screen");
    build_quiz_active_screen(snapshot, edit_data);
    return edit_data.finish_patch();
}

inline scene::scene_layout_patch make_quiz_feedback_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("quiz_feedback_screen");
    build_quiz_feedback_screen(snapshot, edit_data);
    return edit_data.finish_patch();
}

inline scene::scene_layout_patch make_quiz_results_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("quiz_results_screen");
    build_quiz_results_screen(snapshot, edit_data);
    return edit_data.finish_patch();
}

inline scene::scene_layout_patch make_settings_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("settings_screen");
    build_settings_screen(snapshot, edit_data);
    return edit_data.finish_patch();
}

inline scene::scene_layout_patch make_error_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("error_screen");
    build_error_screen(snapshot, edit_data);
    return edit_data.finish_patch();
}

inline scene::scene_layout_patch make_quiz_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("quiz_screen");
    build_quiz_screen(snapshot, edit_data);
    return edit_data.finish_patch();
}

class quiz_screen_scene_modifier : public scene::scene_modifier {
public:
    quiz_screen_scene_modifier(quiz_screen_kind screen, domain::app_snapshot snapshot)
        : screen_(screen)
        , snapshot_(std::move(snapshot))
    {
    }

    void set_screen(quiz_screen_kind screen)
    {
        screen_ = screen;
    }

    quiz_screen_kind screen() const
    {
        return screen_;
    }

    void set_snapshot(domain::app_snapshot snapshot)
    {
        snapshot_ = std::move(snapshot);
    }

    const domain::app_snapshot& snapshot() const
    {
        return snapshot_;
    }

    void modify(const scene::scene_modifier_context& context, scene::scene_layout_edit_data& edit_data) override
    {
        if (context.current_scene != nullptr && context.current_scene->contains_node(detail::to_string_copy(detail::quiz_screens_root_id))) {
            edit_data.remove_node(detail::to_string_copy(detail::quiz_screens_root_id));
        }

        switch (screen_) {
            case quiz_screen_kind::deck_list:
                build_deck_list_screen(snapshot_, edit_data);
                return;
            case quiz_screen_kind::deck_view:
                build_deck_view_screen(snapshot_, edit_data);
                return;
            case quiz_screen_kind::day_intro:
                build_day_intro_screen(snapshot_, edit_data);
                return;
            case quiz_screen_kind::quiz_active:
                build_quiz_active_screen(snapshot_, edit_data);
                return;
            case quiz_screen_kind::quiz_feedback:
                build_quiz_feedback_screen(snapshot_, edit_data);
                return;
            case quiz_screen_kind::quiz_results:
                build_quiz_results_screen(snapshot_, edit_data);
                return;
            case quiz_screen_kind::settings:
                build_settings_screen(snapshot_, edit_data);
                return;
            case quiz_screen_kind::error:
                build_error_screen(snapshot_, edit_data);
                return;
        }
    }

private:
    quiz_screen_kind screen_;
    domain::app_snapshot snapshot_;
};

class app_snapshot_screen_modifier final : public scene::scene_modifier {
public:
    explicit app_snapshot_screen_modifier(domain::app_snapshot snapshot)
        : snapshot_(std::move(snapshot))
    {
    }

    void set_snapshot(domain::app_snapshot snapshot)
    {
        snapshot_ = std::move(snapshot);
    }

    const domain::app_snapshot& snapshot() const
    {
        return snapshot_;
    }

    void modify(const scene::scene_modifier_context& context, scene::scene_layout_edit_data& edit_data) override
    {
        if (context.current_scene != nullptr && context.current_scene->contains_node(detail::to_string_copy(detail::quiz_screens_root_id))) {
            edit_data.remove_node(detail::to_string_copy(detail::quiz_screens_root_id));
        }

        build_quiz_screen(snapshot_, edit_data);
    }

private:
    domain::app_snapshot snapshot_;
};

class home_screen_modifier final : public quiz_screen_scene_modifier {
public:
    explicit home_screen_modifier(domain::app_snapshot snapshot)
        : quiz_screen_scene_modifier(quiz_screen_kind::deck_list, std::move(snapshot))
    {
    }
};

class deck_list_screen_modifier final : public quiz_screen_scene_modifier {
public:
    explicit deck_list_screen_modifier(domain::app_snapshot snapshot)
        : quiz_screen_scene_modifier(quiz_screen_kind::deck_list, std::move(snapshot))
    {
    }
};

class deck_view_screen_modifier final : public quiz_screen_scene_modifier {
public:
    explicit deck_view_screen_modifier(domain::app_snapshot snapshot)
        : quiz_screen_scene_modifier(quiz_screen_kind::deck_view, std::move(snapshot))
    {
    }
};

class day_intro_screen_modifier final : public quiz_screen_scene_modifier {
public:
    explicit day_intro_screen_modifier(domain::app_snapshot snapshot)
        : quiz_screen_scene_modifier(quiz_screen_kind::day_intro, std::move(snapshot))
    {
    }
};

class quiz_active_screen_modifier final : public quiz_screen_scene_modifier {
public:
    explicit quiz_active_screen_modifier(domain::app_snapshot snapshot)
        : quiz_screen_scene_modifier(quiz_screen_kind::quiz_active, std::move(snapshot))
    {
    }
};

class quiz_feedback_screen_modifier final : public quiz_screen_scene_modifier {
public:
    explicit quiz_feedback_screen_modifier(domain::app_snapshot snapshot)
        : quiz_screen_scene_modifier(quiz_screen_kind::quiz_feedback, std::move(snapshot))
    {
    }
};

class quiz_results_screen_modifier final : public quiz_screen_scene_modifier {
public:
    explicit quiz_results_screen_modifier(domain::app_snapshot snapshot)
        : quiz_screen_scene_modifier(quiz_screen_kind::quiz_results, std::move(snapshot))
    {
    }
};

class settings_screen_modifier final : public quiz_screen_scene_modifier {
public:
    explicit settings_screen_modifier(domain::app_snapshot snapshot)
        : quiz_screen_scene_modifier(quiz_screen_kind::settings, std::move(snapshot))
    {
    }
};

class error_screen_modifier final : public quiz_screen_scene_modifier {
public:
    explicit error_screen_modifier(domain::app_snapshot snapshot)
        : quiz_screen_scene_modifier(quiz_screen_kind::error, std::move(snapshot))
    {
    }
};

inline std::shared_ptr<scene::scene_modifier> make_app_snapshot_screen_modifier(domain::app_snapshot snapshot)
{
    return std::make_shared<app_snapshot_screen_modifier>(std::move(snapshot));
}

inline std::shared_ptr<scene::scene_modifier> make_quiz_screen_modifier(
    quiz_screen_kind screen,
    domain::app_snapshot snapshot)
{
    return std::make_shared<quiz_screen_scene_modifier>(screen, std::move(snapshot));
}

} // namespace quiz_vulkan::presentation
