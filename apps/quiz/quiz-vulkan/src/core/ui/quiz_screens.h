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

namespace quiz_vulkan::ui {

enum class quiz_screen_kind {
    home,
    deck_view,
    day_intro,
    quiz_active,
    quiz_results,
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
        case quiz_screen_kind::home:
            return "home";
        case quiz_screen_kind::deck_view:
            return "deck_view";
        case quiz_screen_kind::day_intro:
            return "day_intro";
        case quiz_screen_kind::quiz_active:
            return "quiz_active";
        case quiz_screen_kind::quiz_results:
            return "quiz_results";
    }

    return "home";
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
        case domain::answer_outcome::marked_unknown:
            return "Marked unknown";
        case domain::answer_outcome::unanswered:
            return "Unanswered";
    }

    return "Unanswered";
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

inline scene::scene_route_state route_for(
    quiz_screen_kind screen,
    const domain::app_snapshot& snapshot)
{
    const std::string name = screen_name(screen);

    scene::scene_route_state route;
    route.route_id = name;
    route.screen_id = name;
    route.metadata["domain_screen"] = app_screen_name(snapshot.screen);
    route.metadata["deck_count"] = std::to_string(snapshot.decks.size());

    if (snapshot.selected_deck_id.has_value()) {
        route.metadata["selected_deck_id"] = *snapshot.selected_deck_id;
    }
    if (snapshot.selected_day_id.has_value()) {
        route.metadata["selected_day_id"] = *snapshot.selected_day_id;
    }
    if (snapshot.active_session.has_value()) {
        route.metadata["quiz_mode"] = quiz_mode_name(snapshot.active_session->mode);
        route.metadata["session_phase"] = session_phase_name(snapshot.active_session->phase);
        route.metadata["question_count"] = std::to_string(snapshot.active_session->question_count);
    }

    return route;
}

inline void append_screen_shell(
    scene::scene_layout_edit_data& edit_data,
    quiz_screen_kind screen,
    const domain::app_snapshot& snapshot)
{
    edit_data.set_route(route_for(screen, snapshot));

    scene::scene_node_data root = node(
        to_string_copy(quiz_screens_root_id),
        scene::scene_node_kind::container,
        "quiz screens root",
        vertical_rule(14.0f, {24.0f, 24.0f, 24.0f, 24.0f}),
        style("screen", "#101820", "#f6f7f9"));
    root.layout_rule.clip_children = true;
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
        + " / Unknown " + std::to_string(summary.unknown_count);
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

inline void append_feedback_banner(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    const domain::answer_record& feedback)
{
    const bool is_correct = feedback.outcome == domain::answer_outcome::correct;
    scene::scene_node_data banner = node(
        "quiz_active_feedback",
        scene::scene_node_kind::text,
        "feedback",
        fixed_height_rule(44.0f, {12.0f, 10.0f, 12.0f, 10.0f}),
        style(is_correct ? "feedback_correct" : "feedback_needs_work", is_correct ? "#28503a" : "#5a3d2c", "#ffffff", 6.0f));
    banner.text_runs.push_back({display_outcome(feedback.outcome), banner.style.token});
    edit_data.append_node(to_string_copy(parent_id), std::move(banner));
}

inline void append_question_options(
    scene::scene_layout_edit_data& edit_data,
    std::string_view parent_id,
    const domain::question_snapshot& question,
    bool feedback_visible)
{
    const std::string section_id = "quiz_active_options";
    append_section(edit_data, parent_id, section_id, 8.0f);

    for (std::size_t index = 0; index < question.options.size(); ++index) {
        const domain::option_snapshot& option = question.options[index];
        std::string style_token = "button";
        if (option.reveal_correctness) {
            style_token = option.is_correct ? "option_correct" : "option_incorrect";
        }

        const std::string option_id = "quiz_active_option_" + std::to_string(index);
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
    const domain::question_snapshot& question)
{
    const std::string input_id = "quiz_active_text_answer";
    scene::scene_node_data input = node(
        input_id,
        scene::scene_node_kind::input,
        "text answer input",
        fixed_height_rule(48.0f, {12.0f, 12.0f, 12.0f, 12.0f}),
        style("text_input", "#162532", "#ffffff", 6.0f));
    input.text_runs.push_back({"Type answer", "text_input"});
    edit_data.append_node(to_string_copy(parent_id), std::move(input));
    edit_data.bind_action(input_id, change_action("submit_text_answer", question.question_id));

    append_button(
        edit_data,
        parent_id,
        "quiz_active_submit_text",
        "Submit answer",
        press_action("submit_text_answer", question.question_id),
        "button");
}

} // namespace detail

inline quiz_screen_kind screen_kind_for_snapshot(const domain::app_snapshot& snapshot)
{
    switch (snapshot.screen) {
        case domain::app_screen::deck_select:
        case domain::app_screen::loading:
        case domain::app_screen::error:
            return quiz_screen_kind::home;
        case domain::app_screen::day_select:
            return snapshot.selected_day_id.has_value() ? quiz_screen_kind::day_intro : quiz_screen_kind::deck_view;
        case domain::app_screen::quiz:
        case domain::app_screen::feedback:
            return quiz_screen_kind::quiz_active;
        case domain::app_screen::completed:
            return quiz_screen_kind::quiz_results;
    }

    return quiz_screen_kind::home;
}

inline void build_home_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    detail::append_screen_shell(edit_data, quiz_screen_kind::home, snapshot);
    detail::append_header(
        edit_data,
        detail::quiz_screens_root_id,
        "home",
        "Quiz Vulkan",
        std::to_string(snapshot.decks.size()) + " decks available");
    detail::append_error_banner(edit_data, detail::quiz_screens_root_id, "home", snapshot);
    detail::append_learning_summary(edit_data, detail::quiz_screens_root_id, "home", snapshot.learning);

    detail::append_section(edit_data, detail::quiz_screens_root_id, "home_decks", 8.0f);
    detail::append_text(edit_data, "home_decks", "home_decks_label", "Decks", "section_label", "#d8e1e7");

    if (snapshot.decks.empty()) {
        detail::append_empty_state(edit_data, "home_decks", "home_decks", "No decks loaded");
        return;
    }

    for (std::size_t index = 0; index < snapshot.decks.size(); ++index) {
        const domain::deck& deck = snapshot.decks[index];
        const std::string deck_id = "home_deck_" + detail::safe_id(deck.id, std::to_string(index));
        detail::append_button(
            edit_data,
            "home_decks",
            deck_id,
            deck.title + " - " + detail::deck_summary(deck),
            detail::press_action("select_deck", deck.id),
            "deck_button");
    }

    edit_data.set_focus("home_deck_" + detail::safe_id(snapshot.decks.front().id, "0"));
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
    detail::append_start_mode_button(edit_data, "day_intro_modes", "day_intro", domain::quiz_mode::normal, "Start due questions");
    detail::append_start_mode_button(edit_data, "day_intro_modes", "day_intro", domain::quiz_mode::random, "Random order");
    detail::append_start_mode_button(edit_data, "day_intro_modes", "day_intro", domain::quiz_mode::wrong_only, "Review wrong answers");
    detail::append_start_mode_button(edit_data, "day_intro_modes", "day_intro", domain::quiz_mode::known, "Review known questions");
    edit_data.set_focus("day_intro_start_normal");
}

inline void build_quiz_active_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    detail::append_screen_shell(edit_data, quiz_screen_kind::quiz_active, snapshot);

    if (!snapshot.active_session.has_value()) {
        detail::append_header(edit_data, detail::quiz_screens_root_id, "quiz_active", "Quiz");
        detail::append_empty_state(edit_data, detail::quiz_screens_root_id, "quiz_active", "No active quiz");
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
        "quiz_active",
        progress,
        "Mode: " + detail::quiz_mode_name(session.mode) + " / Phase: " + detail::session_phase_name(session.phase));

    if (!session.current_question.has_value()) {
        detail::append_empty_state(edit_data, detail::quiz_screens_root_id, "quiz_active", "No current question");
        return;
    }

    const domain::question_snapshot& question = *session.current_question;
    const bool feedback_visible = session.feedback.has_value();
    if (feedback_visible) {
        detail::append_feedback_banner(edit_data, detail::quiz_screens_root_id, *session.feedback);
    }

    detail::append_pill(
        edit_data,
        detail::quiz_screens_root_id,
        "quiz_active_learning_state",
        detail::question_type_name(question.type) + " / " + detail::learning_state_name(question.learning),
        "question_meta");
    detail::append_text(edit_data, detail::quiz_screens_root_id, "quiz_active_prompt", question.prompt, "prompt", "#ffffff");

    if (question.long_text.has_value() && !question.long_text->empty()) {
        detail::append_text(edit_data, detail::quiz_screens_root_id, "quiz_active_long_text", *question.long_text, "body", "#d8e1e7");
    }

    if (question.image_uri.has_value() && !question.image_uri->empty()) {
        scene::scene_node_data image = detail::node(
            "quiz_active_image",
            scene::scene_node_kind::image,
            "question image",
            detail::image_rule(180.0f),
            detail::style("image_frame", "#162532", "#ffffff", 6.0f));
        image.image.uri = *question.image_uri;
        image.image.alt_text = question.prompt;
        image.image.aspect_ratio = 1.777778f;
        image.has_image = true;
        edit_data.append_node(detail::to_string_copy(detail::quiz_screens_root_id), std::move(image));
    }

    if (detail::uses_option_answers(question)) {
        detail::append_question_options(edit_data, detail::quiz_screens_root_id, question, feedback_visible);
        if (!feedback_visible && !question.options.empty()) {
            edit_data.set_focus("quiz_active_option_0");
        }
    } else if (!feedback_visible) {
        detail::append_text_answer_input(edit_data, detail::quiz_screens_root_id, question);
        edit_data.set_focus("quiz_active_text_answer");
    }

    if (feedback_visible) {
        detail::append_button(
            edit_data,
            detail::quiz_screens_root_id,
            "quiz_active_continue",
            "Continue",
            detail::press_action("continue_after_feedback"),
            "button");
        edit_data.set_focus("quiz_active_continue");
        return;
    }

    detail::append_section(edit_data, detail::quiz_screens_root_id, "quiz_active_controls", 8.0f);
    detail::append_button(
        edit_data,
        "quiz_active_controls",
        "quiz_active_skip",
        "Skip",
        detail::press_action("skip_question"),
        "secondary_button");
    detail::append_button(
        edit_data,
        "quiz_active_controls",
        "quiz_active_mark_unknown",
        "Mark unknown",
        detail::press_action("mark_question_unknown"),
        "secondary_button");
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
    detail::append_start_mode_button(edit_data, "quiz_results_actions", "quiz_results", domain::quiz_mode::normal, "Restart due questions");
    detail::append_start_mode_button(edit_data, "quiz_results_actions", "quiz_results", domain::quiz_mode::wrong_only, "Review wrong answers");
    detail::append_start_mode_button(edit_data, "quiz_results_actions", "quiz_results", domain::quiz_mode::known, "Review known questions");
    edit_data.set_focus("quiz_results_start_normal");
}

inline void build_quiz_screen(const domain::app_snapshot& snapshot, scene::scene_layout_edit_data& edit_data)
{
    switch (screen_kind_for_snapshot(snapshot)) {
        case quiz_screen_kind::home:
            build_home_screen(snapshot, edit_data);
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
        case quiz_screen_kind::quiz_results:
            build_quiz_results_screen(snapshot, edit_data);
            return;
    }
}

inline scene::scene_layout_patch make_home_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("home_screen");
    build_home_screen(snapshot, edit_data);
    return edit_data.finish_patch();
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

inline scene::scene_layout_patch make_quiz_results_screen_patch(const domain::app_snapshot& snapshot)
{
    scene::scene_layout_edit_data edit_data("quiz_results_screen");
    build_quiz_results_screen(snapshot, edit_data);
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
            case quiz_screen_kind::home:
                build_home_screen(snapshot_, edit_data);
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
            case quiz_screen_kind::quiz_results:
                build_quiz_results_screen(snapshot_, edit_data);
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
        : quiz_screen_scene_modifier(quiz_screen_kind::home, std::move(snapshot))
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

class quiz_results_screen_modifier final : public quiz_screen_scene_modifier {
public:
    explicit quiz_results_screen_modifier(domain::app_snapshot snapshot)
        : quiz_screen_scene_modifier(quiz_screen_kind::quiz_results, std::move(snapshot))
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

} // namespace quiz_vulkan::ui
