#pragma once

#include "app/app_command_registry.h"
#include "core/domain/app_snapshot.hpp"
#include "core/scene/scene_layout_edit_data.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace quiz_vulkan::presentation {

inline constexpr int app_scene_script_template_schema_version = 1;
inline constexpr int app_scene_script_node_dsl_schema_version = 2;

inline constexpr std::string_view day_intro_screen_script_json = R"json({
  "schema_version": 1,
  "template": "builtin:quiz.day_intro.v1",
  "screen": "day_intro"
})json";

struct app_scene_script_data_binding {
    std::string target;
    std::string expression;
};

struct app_scene_script_repeater {
    std::string item_name;
    std::string collection;
};

struct app_scene_script_command_template {
    std::string name;
    std::map<std::string, std::string> args;
};

struct app_scene_script_event_handler_template {
    scene::scene_action_trigger trigger = scene::scene_action_trigger::press;
    std::vector<app_scene_script_command_template> commands;
    std::optional<scene::scene_action_binding> legacy_binding;
    std::string condition;
    std::string transition;
};

struct app_scene_script_transition {
    std::string name;
    float duration_seconds = 0.0f;
    std::string condition;
};

struct app_scene_script_node {
    std::string id;
    std::string parent_id;
    scene::scene_node_kind kind = scene::scene_node_kind::container;
    std::string debug_name;
    scene::scene_layout_rule layout_rule;
    scene::scene_style style;
    std::vector<scene::scene_text_run> text_runs;
    scene::scene_image_ref image;
    std::vector<app_scene_script_data_binding> bindings;
    std::optional<app_scene_script_repeater> repeater;
    std::string condition;
    std::vector<app_scene_script_event_handler_template> events;
    std::vector<app_scene_script_transition> transitions;
    scene::scene_node_semantics semantics;
    bool visible = true;
    bool input_enabled = true;
    bool has_image = false;
};

struct app_scene_script_style_definition {
    std::string id;
    scene::scene_style style;
};

struct app_scene_script_document {
    int schema_version = 0;
    std::string template_id;
    std::string screen;
    std::optional<scene::scene_route_state> route_state;
    std::string focus_id;
    std::vector<app_scene_script_style_definition> styles;
    std::vector<app_scene_script_node> nodes;
    std::vector<app_scene_script_transition> transitions;
};

struct app_scene_script_parse_result {
    std::optional<app_scene_script_document> document;
    std::string error;

    bool ok() const
    {
        return document.has_value() && error.empty();
    }
};

struct app_scene_script_validation_result {
    std::vector<std::string> errors;

    bool ok() const
    {
        return errors.empty();
    }

    std::string joined_error() const
    {
        std::string joined;
        for (const std::string& error : errors) {
            if (!joined.empty()) {
                joined += "; ";
            }
            joined += error;
        }
        return joined;
    }
};

struct app_scene_script_compile_result {
    std::optional<scene::scene_layout_patch> patch;
    std::string error;

    bool ok() const
    {
        return patch.has_value() && error.empty();
    }
};

inline scene::scene_layout_patch require_compiled_app_scene_script_patch(
    app_scene_script_compile_result result,
    std::string_view context)
{
    if (result.ok()) {
        return std::move(*result.patch);
    }

    std::string message = "app scene script compile failed";
    if (!context.empty()) {
        message += " for ";
        message += context;
    }
    if (!result.error.empty()) {
        message += ": ";
        message += result.error;
    }
    throw std::logic_error(message);
}

namespace script_detail {

inline bool json_is_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

inline std::optional<std::string> find_json_string_field(
    std::string_view json,
    std::string_view key,
    std::string& error)
{
    const std::string key_pattern = "\"" + std::string(key) + "\"";
    const std::size_t key_position = json.find(key_pattern);
    if (key_position == std::string_view::npos) {
        error = "script is missing string field: " + std::string(key);
        return std::nullopt;
    }

    std::size_t cursor = json.find(':', key_position + key_pattern.size());
    if (cursor == std::string_view::npos) {
        error = "script field is missing ':' after: " + std::string(key);
        return std::nullopt;
    }
    ++cursor;

    while (cursor < json.size() && json_is_space(json[cursor])) {
        ++cursor;
    }
    if (cursor >= json.size() || json[cursor] != '"') {
        error = "script field is not a string: " + std::string(key);
        return std::nullopt;
    }
    ++cursor;

    std::string value;
    while (cursor < json.size()) {
        const char ch = json[cursor++];
        if (ch == '"') {
            return value;
        }
        if (ch == '\\') {
            if (cursor >= json.size()) {
                error = "script string has an unfinished escape: " + std::string(key);
                return std::nullopt;
            }
            value.push_back(json[cursor++]);
            continue;
        }
        value.push_back(ch);
    }

    error = "script string is not terminated: " + std::string(key);
    return std::nullopt;
}

inline std::optional<int> find_json_int_field(
    std::string_view json,
    std::string_view key,
    std::string& error)
{
    const std::string key_pattern = "\"" + std::string(key) + "\"";
    const std::size_t key_position = json.find(key_pattern);
    if (key_position == std::string_view::npos) {
        error = "script is missing integer field: " + std::string(key);
        return std::nullopt;
    }

    std::size_t cursor = json.find(':', key_position + key_pattern.size());
    if (cursor == std::string_view::npos) {
        error = "script field is missing ':' after: " + std::string(key);
        return std::nullopt;
    }
    ++cursor;

    while (cursor < json.size() && json_is_space(json[cursor])) {
        ++cursor;
    }

    if (cursor >= json.size() || json[cursor] < '0' || json[cursor] > '9') {
        error = "script field is not a non-negative integer: " + std::string(key);
        return std::nullopt;
    }

    int value = 0;
    while (cursor < json.size() && json[cursor] >= '0' && json[cursor] <= '9') {
        value = value * 10 + (json[cursor] - '0');
        ++cursor;
    }
    return value;
}

inline std::string_view trim(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) {
        value.remove_suffix(1);
    }
    return value;
}

inline bool parse_int64(std::string_view value, std::int64_t& parsed)
{
    value = trim(value);
    if (value.empty()) {
        return false;
    }
    const char* begin = value.data();
    const char* end = begin + value.size();
    const std::from_chars_result result = std::from_chars(begin, end, parsed);
    return result.ec == std::errc{} && result.ptr == end;
}

inline std::string strip_optional_quotes(std::string_view value)
{
    value = trim(value);
    if (value.size() >= 2
        && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
        value.remove_prefix(1);
        value.remove_suffix(1);
    }
    return std::string(value);
}

inline bool is_quoted_string(std::string_view value)
{
    value = trim(value);
    return value.size() >= 2
        && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''));
}

enum class script_value_kind {
    none,
    string,
    boolean,
    integer,
};

struct script_value {
    script_value_kind kind = script_value_kind::none;
    std::string string_value;
    bool bool_value = false;
    std::int64_t int_value = 0;

    static script_value string(std::string value)
    {
        script_value result;
        result.kind = script_value_kind::string;
        result.string_value = std::move(value);
        return result;
    }

    static script_value boolean(bool value)
    {
        script_value result;
        result.kind = script_value_kind::boolean;
        result.bool_value = value;
        return result;
    }

    static script_value integer(std::int64_t value)
    {
        script_value result;
        result.kind = script_value_kind::integer;
        result.int_value = value;
        return result;
    }

    bool truthy() const
    {
        switch (kind) {
            case script_value_kind::string:
                return !string_value.empty();
            case script_value_kind::boolean:
                return bool_value;
            case script_value_kind::integer:
                return int_value != 0;
            case script_value_kind::none:
                return false;
        }
        return false;
    }

    std::string to_string() const
    {
        switch (kind) {
            case script_value_kind::string:
                return string_value;
            case script_value_kind::boolean:
                return bool_value ? "true" : "false";
            case script_value_kind::integer:
                return std::to_string(int_value);
            case script_value_kind::none:
                return {};
        }
        return {};
    }

    scene::scene_value to_scene_value() const
    {
        switch (kind) {
            case script_value_kind::string:
                return scene::scene_value(string_value);
            case script_value_kind::boolean:
                return scene::scene_value(bool_value);
            case script_value_kind::integer:
                return scene::scene_value(int_value);
            case script_value_kind::none:
                return scene::scene_value();
        }
        return scene::scene_value();
    }
};

struct option_context {
    const domain::option_snapshot* option = nullptr;
    std::size_t index = 0;
};

struct eval_context {
    const domain::app_snapshot& snapshot;
    std::optional<option_context> option;
};

inline bool evaluate_expression(
    std::string_view raw_expression,
    const eval_context& context,
    script_value& value,
    std::string& error);

inline const domain::question_snapshot* current_question(const domain::app_snapshot& snapshot)
{
    if (!snapshot.active_session.has_value() || !snapshot.active_session->current_question.has_value()) {
        return nullptr;
    }
    return &(*snapshot.active_session->current_question);
}

inline const domain::session_snapshot* current_session(const domain::app_snapshot& snapshot)
{
    return snapshot.active_session.has_value() ? &(*snapshot.active_session) : nullptr;
}

inline const domain::deck* selected_deck(const domain::app_snapshot& snapshot)
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

inline const domain::day* selected_day(const domain::deck& deck, const domain::app_snapshot& snapshot)
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

inline std::size_t deck_question_count(const domain::deck& deck)
{
    std::size_t count = 0;
    for (const domain::day& quiz_day : deck.days) {
        count += quiz_day.questions.size();
    }
    return count;
}

inline bool parse_function_arguments(
    std::string_view raw_args,
    std::vector<std::string>& args,
    std::string& error)
{
    args.clear();

    std::size_t start = 0;
    int nested_parentheses = 0;
    char quote = '\0';
    bool escaping = false;

    for (std::size_t index = 0; index <= raw_args.size(); ++index) {
        const bool at_end = index == raw_args.size();
        const char ch = at_end ? ',' : raw_args[index];

        if (!at_end && quote != '\0') {
            if (escaping) {
                escaping = false;
                continue;
            }
            if (ch == '\\') {
                escaping = true;
                continue;
            }
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (!at_end && (ch == '"' || ch == '\'')) {
            quote = ch;
            continue;
        }

        if (!at_end && ch == '(') {
            ++nested_parentheses;
            continue;
        }
        if (!at_end && ch == ')') {
            if (nested_parentheses == 0) {
                error = "script function argument has unmatched ')'";
                return false;
            }
            --nested_parentheses;
            continue;
        }

        if (ch != ',' || nested_parentheses != 0) {
            continue;
        }

        std::string_view argument = trim(raw_args.substr(start, index - start));
        if (!argument.empty()) {
            args.emplace_back(argument);
        }
        start = index + 1;
    }

    if (quote != '\0') {
        error = "script function argument has unterminated string literal";
        return false;
    }
    if (nested_parentheses != 0) {
        error = "script function argument has unmatched '('";
        return false;
    }
    return true;
}

inline bool parse_function_call(
    std::string_view raw_expression,
    std::string& name,
    std::vector<std::string>& args,
    std::string& error)
{
    raw_expression = trim(raw_expression);
    if (raw_expression.empty() || raw_expression.back() != ')') {
        return false;
    }

    const std::size_t open = raw_expression.find('(');
    if (open == std::string_view::npos) {
        return false;
    }

    std::string_view raw_name = trim(raw_expression.substr(0, open));
    if (raw_name.empty()) {
        return false;
    }
    for (unsigned char ch : raw_name) {
        if (std::isalnum(ch) == 0 && ch != '_') {
            return false;
        }
    }

    name = std::string(raw_name);
    return parse_function_arguments(raw_expression.substr(open + 1, raw_expression.size() - open - 2), args, error);
}

inline std::size_t find_top_level_pipe(std::string_view value, std::size_t start = 0)
{
    int nested_parentheses = 0;
    char quote = '\0';
    bool escaping = false;

    for (std::size_t index = start; index < value.size(); ++index) {
        const char ch = value[index];

        if (quote != '\0') {
            if (escaping) {
                escaping = false;
                continue;
            }
            if (ch == '\\') {
                escaping = true;
                continue;
            }
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (ch == '(') {
            ++nested_parentheses;
            continue;
        }
        if (ch == ')' && nested_parentheses > 0) {
            --nested_parentheses;
            continue;
        }
        if (ch == '|' && nested_parentheses == 0) {
            return index;
        }
    }

    return std::string_view::npos;
}

inline bool require_script_function_arg_count(
    std::string_view name,
    const std::vector<std::string>& args,
    std::size_t expected,
    std::string& error)
{
    if (args.size() == expected) {
        return true;
    }

    error = "script function " + std::string(name) + " expects " + std::to_string(expected)
        + " argument(s), got " + std::to_string(args.size());
    return false;
}

inline bool evaluate_script_function(
    std::string_view name,
    const std::vector<std::string>& args,
    const eval_context& context,
    script_value& value,
    std::string& error)
{
    if (name == "concat") {
        std::string rendered;
        for (const std::string& arg : args) {
            script_value arg_value;
            if (!evaluate_expression(arg, context, arg_value, error)) {
                return false;
            }
            rendered += arg_value.to_string();
        }
        value = script_value::string(std::move(rendered));
        return true;
    }

    if (name == "equals") {
        if (!require_script_function_arg_count(name, args, 2, error)) {
            return false;
        }

        script_value left;
        script_value right;
        if (!evaluate_expression(args[0], context, left, error)
            || !evaluate_expression(args[1], context, right, error)) {
            return false;
        }
        value = script_value::boolean(left.to_string() == right.to_string());
        return true;
    }

    if (name == "not") {
        if (!require_script_function_arg_count(name, args, 1, error)) {
            return false;
        }

        script_value arg_value;
        if (!evaluate_expression(args.front(), context, arg_value, error)) {
            return false;
        }
        value = script_value::boolean(!arg_value.truthy());
        return true;
    }

    if (name == "empty") {
        if (!require_script_function_arg_count(name, args, 1, error)) {
            return false;
        }

        script_value arg_value;
        if (!evaluate_expression(args.front(), context, arg_value, error)) {
            return false;
        }
        value = script_value::boolean(trim(arg_value.to_string()).empty());
        return true;
    }

    if (name == "choose") {
        if (!require_script_function_arg_count(name, args, 3, error)) {
            return false;
        }

        script_value condition_value;
        if (!evaluate_expression(args[0], context, condition_value, error)) {
            return false;
        }
        return evaluate_expression(condition_value.truthy() ? args[1] : args[2], context, value, error);
    }

    if (name == "safe_id") {
        if (args.empty() || args.size() > 2) {
            error = "script function safe_id expects 1 or 2 argument(s), got " + std::to_string(args.size());
            return false;
        }

        script_value raw_value;
        if (!evaluate_expression(args.front(), context, raw_value, error)) {
            return false;
        }

        std::string fallback = "id";
        if (args.size() == 2) {
            script_value fallback_value;
            if (!evaluate_expression(args[1], context, fallback_value, error)) {
                return false;
            }
            fallback = fallback_value.to_string();
        }

        std::string output;
        const std::string rendered_value = raw_value.to_string();
        output.reserve(rendered_value.size());
        for (unsigned char character : rendered_value) {
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
            output = std::move(fallback);
        }

        value = script_value::string(std::move(output));
        return true;
    }

    error = "unsupported script function: " + std::string(name);
    return false;
}

inline bool evaluate_path(std::string_view raw_expression, const eval_context& context, script_value& value, std::string& error)
{
    raw_expression = trim(raw_expression);
    if (is_quoted_string(raw_expression)) {
        value = script_value::string(strip_optional_quotes(raw_expression));
        return true;
    }

    std::string function_name;
    std::vector<std::string> function_args;
    if (!raw_expression.empty()
        && parse_function_call(raw_expression, function_name, function_args, error)) {
        return evaluate_script_function(function_name, function_args, context, value, error);
    }
    if (!error.empty()) {
        return false;
    }

    const std::string expression(raw_expression);
    if (expression == "true") {
        value = script_value::boolean(true);
        return true;
    }
    if (expression == "false") {
        value = script_value::boolean(false);
        return true;
    }

    std::int64_t parsed_integer = 0;
    if (parse_int64(expression, parsed_integer)) {
        value = script_value::integer(parsed_integer);
        return true;
    }

    if (expression == "question.exists") {
        value = script_value::boolean(current_question(context.snapshot) != nullptr);
        return true;
    }

    const domain::session_snapshot* session = current_session(context.snapshot);
    if (expression == "session.exists") {
        value = script_value::boolean(session != nullptr);
        return true;
    }
    if (expression == "session.mode") {
        if (session == nullptr) {
            error = "expression session.mode requires an active session";
            return false;
        }
        value = script_value::string(std::string(domain::to_string(session->mode)));
        return true;
    }
    if (expression == "session.phase") {
        if (session == nullptr) {
            error = "expression session.phase requires an active session";
            return false;
        }
        value = script_value::string(std::string(domain::to_string(session->phase)));
        return true;
    }
    if (expression == "session.current_index") {
        value = script_value::integer(session == nullptr ? 0 : static_cast<std::int64_t>(session->current_index));
        return true;
    }
    if (expression == "session.question_count") {
        value = script_value::integer(session == nullptr ? 0 : static_cast<std::int64_t>(session->question_count));
        return true;
    }
    if (expression == "session.progress") {
        if (session == nullptr) {
            error = "expression session.progress requires an active session";
            return false;
        }
        value = session->question_count == 0
            ? script_value::string("No questions")
            : script_value::string(
                "Question " + std::to_string(std::min(session->current_index + 1, session->question_count))
                + " of " + std::to_string(session->question_count));
        return true;
    }
    if (expression == "session.completed") {
        value = script_value::boolean(session != nullptr && session->completed);
        return true;
    }
    if (expression == "session.has_feedback") {
        value = script_value::boolean(session != nullptr && session->feedback.has_value());
        return true;
    }

    const domain::answer_record* feedback = session != nullptr && session->feedback.has_value()
        ? &(*session->feedback)
        : nullptr;
    if (expression == "feedback.exists") {
        value = script_value::boolean(feedback != nullptr);
        return true;
    }
    if (expression == "feedback.question_id") {
        if (feedback == nullptr) {
            error = "expression feedback.question_id requires pending feedback";
            return false;
        }
        value = script_value::string(feedback->question_id);
        return true;
    }
    if (expression == "feedback.outcome") {
        if (feedback == nullptr) {
            error = "expression feedback.outcome requires pending feedback";
            return false;
        }
        value = script_value::string(std::string(domain::to_string(feedback->outcome)));
        return true;
    }
    if (expression == "feedback.selected_option_count") {
        value = script_value::integer(feedback == nullptr ? 0 : static_cast<std::int64_t>(feedback->selected_option_indexes.size()));
        return true;
    }
    if (expression == "feedback.submitted_text_count") {
        value = script_value::integer(feedback == nullptr ? 0 : static_cast<std::int64_t>(feedback->submitted_text_answers.size()));
        return true;
    }
    if (expression == "feedback.answered_at_ms") {
        value = script_value::integer(feedback == nullptr ? 0 : feedback->answered_at_ms);
        return true;
    }

    if (expression == "settings.count") {
        value = script_value::integer(static_cast<std::int64_t>(context.snapshot.settings.size()));
        return true;
    }
    if (expression == "error.exists") {
        value = script_value::boolean(context.snapshot.error_message.has_value());
        return true;
    }
    if (expression == "error.message") {
        value = script_value::string(context.snapshot.error_message.value_or(std::string{}));
        return true;
    }

    if (expression == "learning.question_count") {
        value = script_value::integer(static_cast<std::int64_t>(context.snapshot.learning.question_count));
        return true;
    }
    if (expression == "learning.learning_count") {
        value = script_value::integer(static_cast<std::int64_t>(context.snapshot.learning.learning_count));
        return true;
    }
    if (expression == "learning.known_count") {
        value = script_value::integer(static_cast<std::int64_t>(context.snapshot.learning.known_count));
        return true;
    }
    if (expression == "learning.unknown_count") {
        value = script_value::integer(static_cast<std::int64_t>(context.snapshot.learning.unknown_count));
        return true;
    }
    if (expression == "learning.wrong_note_count") {
        value = script_value::integer(static_cast<std::int64_t>(context.snapshot.learning.wrong_note_count));
        return true;
    }
    if (expression == "learning.summary") {
        value = script_value::string(
            "Learning " + std::to_string(context.snapshot.learning.learning_count)
            + " / Known " + std::to_string(context.snapshot.learning.known_count)
            + " / Unknown " + std::to_string(context.snapshot.learning.unknown_count)
            + " / Wrong note " + std::to_string(context.snapshot.learning.wrong_note_count));
        return true;
    }

    const domain::deck* deck = selected_deck(context.snapshot);
    const domain::day* day = deck == nullptr ? nullptr : selected_day(*deck, context.snapshot);
    if (expression == "deck.count") {
        value = script_value::integer(static_cast<std::int64_t>(context.snapshot.decks.size()));
        return true;
    }
    if (expression == "selected_deck.exists") {
        value = script_value::boolean(deck != nullptr);
        return true;
    }
    if (expression == "selected_deck.id") {
        if (deck == nullptr) {
            error = "expression selected_deck.id requires a selected deck";
            return false;
        }
        value = script_value::string(deck->id);
        return true;
    }
    if (expression == "selected_deck.title") {
        if (deck == nullptr) {
            error = "expression selected_deck.title requires a selected deck";
            return false;
        }
        value = script_value::string(deck->title);
        return true;
    }
    if (expression == "selected_deck.source_uri") {
        if (deck == nullptr) {
            error = "expression selected_deck.source_uri requires a selected deck";
            return false;
        }
        value = script_value::string(deck->source_uri);
        return true;
    }
    if (expression == "selected_deck.has_source") {
        value = script_value::boolean(deck != nullptr && !deck->source_uri.empty());
        return true;
    }
    if (expression == "selected_deck.day_count") {
        value = script_value::integer(deck == nullptr ? 0 : static_cast<std::int64_t>(deck->days.size()));
        return true;
    }
    if (expression == "selected_deck.question_count") {
        value = script_value::integer(deck == nullptr ? 0 : static_cast<std::int64_t>(deck_question_count(*deck)));
        return true;
    }
    if (expression == "selected_day.exists") {
        value = script_value::boolean(day != nullptr);
        return true;
    }
    if (expression == "selected_day.id") {
        if (day == nullptr) {
            error = "expression selected_day.id requires a selected day";
            return false;
        }
        value = script_value::string(day->id);
        return true;
    }
    if (expression == "selected_day.title") {
        if (day == nullptr) {
            error = "expression selected_day.title requires a selected day";
            return false;
        }
        value = script_value::string(day->title);
        return true;
    }
    if (expression == "selected_day.question_count") {
        value = script_value::integer(day == nullptr ? 0 : static_cast<std::int64_t>(day->questions.size()));
        return true;
    }

    const domain::question_snapshot* question = current_question(context.snapshot);
    if (expression == "question.prompt") {
        if (question == nullptr) {
            error = "expression question.prompt requires an active question";
            return false;
        }
        value = script_value::string(question->prompt);
        return true;
    }
    if (expression == "question.id") {
        if (question == nullptr) {
            error = "expression question.id requires an active question";
            return false;
        }
        value = script_value::string(question->question_id);
        return true;
    }
    if (expression == "question.long_text") {
        if (question == nullptr) {
            error = "expression question.long_text requires an active question";
            return false;
        }
        value = script_value::string(question->long_text.value_or(std::string{}));
        return true;
    }
    if (expression == "question.has_long_text") {
        value = script_value::boolean(question != nullptr && question->long_text.has_value() && !question->long_text->empty());
        return true;
    }
    if (expression == "question.image_uri") {
        if (question == nullptr) {
            error = "expression question.image_uri requires an active question";
            return false;
        }
        value = script_value::string(question->image_uri.value_or(std::string{}));
        return true;
    }
    if (expression == "question.has_image") {
        value = script_value::boolean(question != nullptr && question->image_uri.has_value() && !question->image_uri->empty());
        return true;
    }
    if (expression == "question.has_options") {
        value = script_value::boolean(question != nullptr && !question->options.empty());
        return true;
    }
    if (expression == "question.option_count") {
        value = script_value::integer(question == nullptr ? 0 : static_cast<std::int64_t>(question->options.size()));
        return true;
    }
    if (expression == "question.type") {
        if (question == nullptr) {
            error = "expression question.type requires an active question";
            return false;
        }
        value = script_value::string(std::string(domain::to_string(question->type)));
        return true;
    }

    if (expression == "option.index") {
        if (!context.option.has_value()) {
            error = "expression option.index requires a repeater option item";
            return false;
        }
        value = script_value::integer(static_cast<std::int64_t>(context.option->index));
        return true;
    }
    if (expression == "option.text") {
        if (!context.option.has_value() || context.option->option == nullptr) {
            error = "expression option.text requires a repeater option item";
            return false;
        }
        value = script_value::string(context.option->option->text);
        return true;
    }
    if (expression == "option.is_correct") {
        if (!context.option.has_value() || context.option->option == nullptr) {
            error = "expression option.is_correct requires a repeater option item";
            return false;
        }
        value = script_value::boolean(context.option->option->is_correct);
        return true;
    }
    if (expression == "option.reveal_correctness") {
        if (!context.option.has_value() || context.option->option == nullptr) {
            error = "expression option.reveal_correctness requires a repeater option item";
            return false;
        }
        value = script_value::boolean(context.option->option->reveal_correctness);
        return true;
    }

    error = "unsupported script expression: " + expression;
    return false;
}

inline bool apply_script_formatter(std::string_view raw_formatter, script_value& value, std::string& error)
{
    const std::string formatter = strip_optional_quotes(raw_formatter);
    if (formatter.empty()) {
        error = "empty script formatter";
        return false;
    }

    std::string rendered = value.to_string();
    if (formatter == "string") {
        value = script_value::string(std::move(rendered));
        return true;
    }
    if (formatter == "trim") {
        value = script_value::string(std::string(trim(rendered)));
        return true;
    }
    if (formatter == "upper") {
        std::transform(rendered.begin(), rendered.end(), rendered.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        value = script_value::string(std::move(rendered));
        return true;
    }
    if (formatter == "lower") {
        std::transform(rendered.begin(), rendered.end(), rendered.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        value = script_value::string(std::move(rendered));
        return true;
    }

    error = "unsupported script formatter: " + formatter;
    return false;
}

inline bool evaluate_expression(std::string_view raw_expression, const eval_context& context, script_value& value, std::string& error)
{
    raw_expression = trim(raw_expression);
    const std::size_t first_pipe = find_top_level_pipe(raw_expression);
    if (first_pipe == std::string_view::npos) {
        return evaluate_path(raw_expression, context, value, error);
    }

    if (!evaluate_path(raw_expression.substr(0, first_pipe), context, value, error)) {
        return false;
    }

    std::size_t cursor = first_pipe + 1;
    while (cursor <= raw_expression.size()) {
        const std::size_t next_pipe = find_top_level_pipe(raw_expression, cursor);
        const std::string_view formatter = next_pipe == std::string_view::npos
            ? raw_expression.substr(cursor)
            : raw_expression.substr(cursor, next_pipe - cursor);
        if (!apply_script_formatter(formatter, value, error)) {
            return false;
        }
        if (next_pipe == std::string_view::npos) {
            return true;
        }
        cursor = next_pipe + 1;
    }

    return true;
}

inline bool extract_single_interpolation(std::string_view expression, std::string_view& inner)
{
    expression = trim(expression);
    if (expression.size() < 5 || expression.substr(0, 2) != "{{" || expression.substr(expression.size() - 2) != "}}") {
        return false;
    }
    inner = trim(expression.substr(2, expression.size() - 4));
    return true;
}

inline bool render_template(
    std::string_view templated,
    const eval_context& context,
    std::string& rendered,
    std::string& error)
{
    rendered.clear();
    std::size_t cursor = 0;
    while (cursor < templated.size()) {
        const std::size_t open = templated.find("{{", cursor);
        if (open == std::string_view::npos) {
            rendered += templated.substr(cursor);
            return true;
        }

        rendered += templated.substr(cursor, open - cursor);
        const std::size_t close = templated.find("}}", open + 2);
        if (close == std::string_view::npos) {
            error = "unterminated script binding expression";
            return false;
        }

        script_value value;
        if (!evaluate_expression(templated.substr(open + 2, close - open - 2), context, value, error)) {
            return false;
        }
        rendered += value.to_string();
        cursor = close + 2;
    }
    return true;
}

inline bool evaluate_scene_value_expression(
    std::string_view expression,
    const eval_context& context,
    scene::scene_value& value,
    std::string& error)
{
    std::string_view inner;
    if (extract_single_interpolation(expression, inner)) {
        script_value script_result;
        if (!evaluate_expression(inner, context, script_result, error)) {
            return false;
        }
        value = script_result.to_scene_value();
        return true;
    }

    std::string rendered;
    if (!render_template(expression, context, rendered, error)) {
        return false;
    }
    value = scene::scene_value(std::move(rendered));
    return true;
}

inline bool evaluate_condition(std::string_view raw_condition, const eval_context& context, bool& value, std::string& error)
{
    raw_condition = trim(raw_condition);
    if (raw_condition.empty()) {
        value = true;
        return true;
    }

    bool negate = false;
    if (raw_condition.front() == '!') {
        negate = true;
        raw_condition.remove_prefix(1);
        raw_condition = trim(raw_condition);
    }

    const std::size_t equals = raw_condition.find("==");
    if (equals != std::string_view::npos) {
        script_value left;
        if (!evaluate_expression(raw_condition.substr(0, equals), context, left, error)) {
            return false;
        }
        const std::string right = strip_optional_quotes(raw_condition.substr(equals + 2));
        value = left.to_string() == right;
        if (negate) {
            value = !value;
        }
        return true;
    }

    script_value evaluated;
    if (!evaluate_expression(raw_condition, context, evaluated, error)) {
        return false;
    }
    value = evaluated.truthy();
    if (negate) {
        value = !value;
    }
    return true;
}

inline bool evaluate_option_repeater(
    std::string_view collection,
    const eval_context& context,
    std::vector<option_context>& items,
    std::string& error)
{
    collection = trim(collection);
    if (collection != "question.options") {
        error = "unsupported script repeater collection: " + std::string(collection);
        return false;
    }

    const domain::question_snapshot* question = current_question(context.snapshot);
    if (question == nullptr) {
        error = "question.options repeater requires an active question";
        return false;
    }

    items.clear();
    items.reserve(question->options.size());
    for (std::size_t index = 0; index < question->options.size(); ++index) {
        items.push_back(option_context{&question->options[index], index});
    }
    return true;
}

inline scene::scene_route_state route_for_script_document(
    const app_scene_script_document& document,
    const domain::app_snapshot&)
{
    scene::scene_route_state route;
    route.route_id = document.template_id.empty() ? document.screen : document.template_id;
    route.screen_id = document.screen.empty() ? route.route_id : document.screen;
    route.metadata["descriptor_version"] = "app_scene_script_v2";
    route.metadata["script_template"] = document.template_id;
    route.metadata["schema_version"] = std::to_string(document.schema_version);
    return route;
}

inline bool compile_command_template(
    const app_scene_script_command_template& command_template,
    const eval_context& context,
    scene::scene_command& command,
    std::string& error)
{
    std::string rendered_name;
    if (!render_template(command_template.name, context, rendered_name, error)) {
        return false;
    }
    command.name = std::move(rendered_name);

    for (const auto& [arg_name, expression] : command_template.args) {
        scene::scene_value arg_value;
        if (!evaluate_scene_value_expression(expression, context, arg_value, error)) {
            return false;
        }
        command.args.emplace(arg_name, std::move(arg_value));
    }

    const app_command_validation_result validation = validate_scene_command(command);
    if (!validation.ok()) {
        error = validation.error;
        return false;
    }
    return true;
}

inline bool compile_event_template(
    const app_scene_script_event_handler_template& event_template,
    const eval_context& context,
    std::optional<scene::scene_event_handler>& handler,
    std::string& error)
{
    bool condition = true;
    if (!evaluate_condition(event_template.condition, context, condition, error)) {
        return false;
    }
    if (!condition) {
        handler = std::nullopt;
        return true;
    }

    std::vector<scene::scene_command> commands;
    commands.reserve(event_template.commands.size());
    for (const app_scene_script_command_template& command_template : event_template.commands) {
        scene::scene_command command;
        if (!compile_command_template(command_template, context, command, error)) {
            return false;
        }
        commands.push_back(std::move(command));
    }

    scene::scene_event_handler event_handler;
    event_handler.trigger = event_template.trigger;
    event_handler.commands = std::move(commands);
    if (event_template.legacy_binding.has_value()) {
        event_handler.legacy_binding = *event_template.legacy_binding;
    }
    if (!event_template.condition.empty()) {
        event_handler.condition = event_template.condition;
    }
    handler = std::move(event_handler);
    return true;
}

inline bool apply_node_bindings(
    const std::vector<app_scene_script_data_binding>& bindings,
    const eval_context& context,
    scene::scene_node_data& node,
    std::string& node_id,
    std::string& parent_id,
    std::string& error)
{
    for (const app_scene_script_data_binding& binding : bindings) {
        if (binding.target == "id") {
            if (!render_template(binding.expression, context, node_id, error)) {
                return false;
            }
            node.id = node_id;
            continue;
        }
        if (binding.target == "parent_id") {
            if (!render_template(binding.expression, context, parent_id, error)) {
                return false;
            }
            continue;
        }
        if (binding.target == "debug_name") {
            if (!render_template(binding.expression, context, node.debug_name, error)) {
                return false;
            }
            continue;
        }
        if (binding.target == "text") {
            std::string rendered_text;
            if (!render_template(binding.expression, context, rendered_text, error)) {
                return false;
            }
            node.text_runs.clear();
            node.text_runs.push_back({std::move(rendered_text), node.style.token});
            continue;
        }
        if (binding.target == "visible" || binding.target == "input_enabled") {
            bool rendered_bool = false;
            if (!evaluate_condition(binding.expression, context, rendered_bool, error)) {
                return false;
            }
            if (binding.target == "visible") {
                node.visible = rendered_bool;
            } else {
                node.input_enabled = rendered_bool;
            }
            continue;
        }
        if (binding.target == "style.token") {
            if (!render_template(binding.expression, context, node.style.token, error)) {
                return false;
            }
            for (scene::scene_text_run& run : node.text_runs) {
                run.style_token = node.style.token;
            }
            continue;
        }
        if (binding.target == "style.background_color") {
            if (!render_template(binding.expression, context, node.style.background_color, error)) {
                return false;
            }
            continue;
        }
        if (binding.target == "style.foreground_color") {
            if (!render_template(binding.expression, context, node.style.foreground_color, error)) {
                return false;
            }
            continue;
        }
        if (binding.target == "image.uri") {
            if (!render_template(binding.expression, context, node.image.uri, error)) {
                return false;
            }
            node.has_image = !node.image.uri.empty();
            continue;
        }
        if (binding.target == "image.alt_text") {
            if (!render_template(binding.expression, context, node.image.alt_text, error)) {
                return false;
            }
            continue;
        }

        error = "unsupported node binding target: " + binding.target;
        return false;
    }
    return true;
}

inline bool compile_node_instance(
    const app_scene_script_node& script_node,
    const eval_context& context,
    scene::scene_layout_edit_data& edit_data,
    std::set<std::string>& emitted_ids,
    std::string& error)
{
    bool condition = true;
    if (!evaluate_condition(script_node.condition, context, condition, error)) {
        return false;
    }
    if (!condition) {
        return true;
    }

    std::string node_id;
    std::string parent_id;
    if (!render_template(script_node.id, context, node_id, error)) {
        return false;
    }
    if (!render_template(script_node.parent_id, context, parent_id, error)) {
        return false;
    }
    if (node_id.empty()) {
        error = "script node emitted an empty id";
        return false;
    }
    if (!emitted_ids.insert(node_id).second) {
        error = "script node emitted duplicate id: " + node_id;
        return false;
    }

    scene::scene_node_data node;
    node.id = node_id;
    node.kind = script_node.kind;
    node.debug_name = script_node.debug_name.empty() ? "script node" : script_node.debug_name;
    node.layout_rule = script_node.layout_rule;
    node.style = script_node.style;
    node.text_runs = script_node.text_runs;
    node.image = script_node.image;
    node.has_image = script_node.has_image;
    for (scene::scene_text_run& run : node.text_runs) {
        if (run.style_token.empty()) {
            run.style_token = node.style.token;
        }
        std::string rendered_text;
        if (!render_template(run.text, context, rendered_text, error)) {
            return false;
        }
        run.text = std::move(rendered_text);
    }
    node.semantics = script_node.semantics;
    node.visible = script_node.visible;
    node.input_enabled = script_node.input_enabled;

    if (!apply_node_bindings(script_node.bindings, context, node, node_id, parent_id, error)) {
        return false;
    }

    std::vector<scene::scene_event_handler> event_handlers;
    std::optional<scene::scene_action_binding> legacy_action;
    for (const app_scene_script_event_handler_template& event_template : script_node.events) {
        std::optional<scene::scene_event_handler> handler;
        if (!compile_event_template(event_template, context, handler, error)) {
            return false;
        }
        if (handler.has_value() && !handler->empty()) {
            if (!legacy_action.has_value() && !handler->legacy_binding.empty()) {
                legacy_action = handler->legacy_binding;
            }
            event_handlers.push_back(std::move(*handler));
        }
    }

    if (legacy_action.has_value()) {
        node.action_binding = std::move(*legacy_action);
        node.has_action_binding = true;
    }
    node.event_handlers = std::move(event_handlers);
    node.has_event_handlers = !node.event_handlers.empty();

    edit_data.append_node(std::move(parent_id), std::move(node));

    for (const app_scene_script_transition& transition : script_node.transitions) {
        bool transition_condition = true;
        if (!evaluate_condition(transition.condition, context, transition_condition, error)) {
            return false;
        }
        if (transition_condition && !transition.name.empty()) {
            scene::scene_animation_state animation;
            animation.active = true;
            animation.name = transition.name;
            animation.duration_seconds = transition.duration_seconds;
            edit_data.start_transition(std::move(animation));
            break;
        }
    }

    return true;
}

inline bool compile_script_node(
    const app_scene_script_node& script_node,
    const eval_context& context,
    scene::scene_layout_edit_data& edit_data,
    std::set<std::string>& emitted_ids,
    std::string& error)
{
    if (!script_node.repeater.has_value()) {
        return compile_node_instance(script_node, context, edit_data, emitted_ids, error);
    }

    std::vector<option_context> items;
    if (!evaluate_option_repeater(script_node.repeater->collection, context, items, error)) {
        return false;
    }

    for (const option_context& item : items) {
        eval_context item_context{context.snapshot, item};
        if (!compile_node_instance(script_node, item_context, edit_data, emitted_ids, error)) {
            return false;
        }
    }
    return true;
}

inline app_scene_script_compile_result compile_node_dsl_script(
    const app_scene_script_document& document,
    const domain::app_snapshot& snapshot)
{
    app_scene_script_compile_result result;
    scene::scene_layout_edit_data edit_data("app_scene_script");
    edit_data.set_route(document.route_state.value_or(route_for_script_document(document, snapshot)));

    eval_context context{snapshot, std::nullopt};
    std::set<std::string> emitted_ids;
    for (const app_scene_script_node& node : document.nodes) {
        if (!compile_script_node(node, context, edit_data, emitted_ids, result.error)) {
            return result;
        }
    }

    for (const app_scene_script_transition& transition : document.transitions) {
        bool transition_condition = true;
        if (!evaluate_condition(transition.condition, context, transition_condition, result.error)) {
            return result;
        }
        if (transition_condition && !transition.name.empty()) {
            scene::scene_animation_state animation;
            animation.active = true;
            animation.name = transition.name;
            animation.duration_seconds = transition.duration_seconds;
            edit_data.start_transition(std::move(animation));
            break;
        }
    }

    if (!document.focus_id.empty()) {
        edit_data.set_focus(document.focus_id);
    }

    result.patch = edit_data.finish_patch();
    return result;
}

}  // namespace script_detail

inline app_scene_script_parse_result parse_app_scene_script_json(std::string_view json)
{
    app_scene_script_parse_result result;
    app_scene_script_document document;

    if (const std::optional<int> schema_version = script_detail::find_json_int_field(json, "schema_version", result.error)) {
        document.schema_version = *schema_version;
    } else {
        return result;
    }

    if (const std::optional<std::string> template_id = script_detail::find_json_string_field(json, "template", result.error)) {
        document.template_id = *template_id;
    } else {
        return result;
    }

    if (const std::optional<std::string> screen = script_detail::find_json_string_field(json, "screen", result.error)) {
        document.screen = *screen;
    } else {
        return result;
    }

    result.document = std::move(document);
    return result;
}

inline app_scene_script_validation_result validate_app_scene_script_document(const app_scene_script_document& document)
{
    app_scene_script_validation_result result;

    if (document.schema_version != app_scene_script_template_schema_version
        && document.schema_version != app_scene_script_node_dsl_schema_version) {
        result.errors.push_back("unsupported app scene script schema_version: " + std::to_string(document.schema_version));
    }

    if (document.schema_version == app_scene_script_node_dsl_schema_version && document.nodes.empty()) {
        result.errors.push_back("schema_version 2 script requires at least one node");
    }

    std::set<std::string> literal_ids;
    for (const app_scene_script_node& node : document.nodes) {
        if (script_detail::trim(node.id).empty()) {
            result.errors.push_back("script node is missing an id");
        }
        if (node.id.find("{{") == std::string::npos && !literal_ids.insert(node.id).second) {
            result.errors.push_back("script contains duplicate literal node id: " + node.id);
        }
        if (node.repeater.has_value()) {
            if (script_detail::trim(node.repeater->item_name).empty()) {
                result.errors.push_back("script repeater is missing item_name");
            }
            if (script_detail::trim(node.repeater->collection).empty()) {
                result.errors.push_back("script repeater is missing collection");
            }
        }
        for (const app_scene_script_event_handler_template& event : node.events) {
            if (event.commands.empty() && !event.legacy_binding.has_value()) {
                result.errors.push_back("script event on node " + node.id + " has no commands or legacy binding");
            }
            if (event.legacy_binding.has_value() && !is_scene_command_allowed(event.legacy_binding->action_type)) {
                result.errors.push_back("script legacy binding is not allowlisted: " + event.legacy_binding->action_type);
            }
            for (const app_scene_script_command_template& command : event.commands) {
                if (!is_scene_command_allowed(command.name)) {
                    result.errors.push_back("script command is not allowlisted: " + command.name);
                }
            }
        }
    }

    return result;
}

inline app_scene_script_compile_result compile_app_scene_script(
    const app_scene_script_document& document,
    const domain::app_snapshot& snapshot)
{
    app_scene_script_compile_result result;

    const app_scene_script_validation_result validation = validate_app_scene_script_document(document);
    if (!validation.ok()) {
        result.error = validation.joined_error();
        return result;
    }

    if (document.schema_version == app_scene_script_node_dsl_schema_version || !document.nodes.empty()) {
        return script_detail::compile_node_dsl_script(document, snapshot);
    }

    result.error = "unsupported app scene script template: " + document.template_id;
    return result;
}

}  // namespace quiz_vulkan::presentation
