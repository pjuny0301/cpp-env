#pragma once

#include "core/domain/app_action.hpp"
#include "core/scene/scene_layout_data.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace quiz_vulkan {

struct app_command_route_result {
    std::optional<domain::app_action> action;
    std::string error;

    bool ok() const
    {
        return action.has_value() && error.empty();
    }
};

struct app_command_validation_result {
    std::string error;

    bool ok() const
    {
        return error.empty();
    }
};

namespace detail {

inline app_command_route_result route_command_action(domain::app_action action)
{
    app_command_route_result result;
    result.action = std::move(action);
    return result;
}

inline app_command_route_result route_command_error(std::string error)
{
    app_command_route_result result;
    result.error = std::move(error);
    return result;
}

inline std::string_view trim_command_token(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) {
        value.remove_suffix(1);
    }
    return value;
}

inline std::string normalize_command_token(std::string_view value)
{
    value = trim_command_token(value);

    std::string normalized;
    normalized.reserve(value.size());
    for (const char ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            normalized.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else {
            normalized.push_back(ch);
        }
    }
    return normalized;
}

inline std::optional<domain::quiz_mode> parse_command_quiz_mode(std::string_view value)
{
    if (const std::optional<domain::quiz_mode> parsed = domain::parse_quiz_mode(value)) {
        return parsed;
    }

    const std::string normalized = normalize_command_token(value);
    if (normalized == "due") {
        return domain::quiz_mode::normal;
    }
    if (normalized == "wrong") {
        return domain::quiz_mode::wrong_only;
    }

    return std::nullopt;
}

inline bool command_arg_name_matches(std::string_view actual, std::initializer_list<std::string_view> allowed)
{
    for (const std::string_view candidate : allowed) {
        if (actual == candidate) {
            return true;
        }
    }
    return false;
}

inline std::optional<std::string> validate_command_arg_names(
    const scene::scene_command& command,
    std::initializer_list<std::string_view> allowed)
{
    for (const auto& [name, value] : command.args) {
        (void)value;
        if (!command_arg_name_matches(name, allowed)) {
            return "Unsupported argument '" + name + "' for scene command: " + command.name;
        }
    }
    return std::nullopt;
}

inline const scene::scene_value* find_command_arg(
    const scene::scene_command& command,
    std::string_view preferred_key,
    std::string_view fallback_key = {})
{
    if (const scene::scene_value* value = command.find_arg(preferred_key)) {
        return value;
    }
    if (!fallback_key.empty()) {
        return command.find_arg(fallback_key);
    }
    return nullptr;
}

inline std::optional<std::string_view> command_string_arg(
    const scene::scene_command& command,
    std::string_view preferred_key,
    std::string_view fallback_key = {})
{
    const scene::scene_value* value = find_command_arg(command, preferred_key, fallback_key);
    if (value == nullptr) {
        return std::nullopt;
    }

    if (const std::string* string_value = value->string_if()) {
        return std::string_view{*string_value};
    }

    return std::nullopt;
}

inline std::optional<std::string_view> command_non_empty_string_arg(
    const scene::scene_command& command,
    std::string_view preferred_key,
    std::string_view fallback_key = {})
{
    const std::optional<std::string_view> value = command_string_arg(command, preferred_key, fallback_key);
    if (!value.has_value() || trim_command_token(*value).empty()) {
        return std::nullopt;
    }
    return trim_command_token(*value);
}

inline bool parse_command_size(std::string_view value, std::size_t& parsed)
{
    value = trim_command_token(value);
    if (value.empty()) {
        return false;
    }

    for (const char ch : value) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }

    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const std::from_chars_result result = std::from_chars(begin, end, parsed);
    return result.ec == std::errc{} && result.ptr == end;
}

inline std::optional<std::size_t> command_size_arg(
    const scene::scene_command& command,
    std::string_view preferred_key,
    std::string_view fallback_key = {})
{
    const scene::scene_value* value = find_command_arg(command, preferred_key, fallback_key);
    if (value == nullptr) {
        return std::nullopt;
    }

    if (const std::int64_t* int_value = value->int_if()) {
        if (*int_value < 0) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(*int_value);
    }

    if (const std::string* string_value = value->string_if()) {
        std::size_t parsed = 0;
        if (parse_command_size(*string_value, parsed)) {
            return parsed;
        }
    }

    return std::nullopt;
}

inline std::optional<std::vector<std::size_t>> parse_command_size_list(std::string_view payload)
{
    std::vector<std::size_t> indexes;

    while (true) {
        const std::size_t comma = payload.find(',');
        const std::string_view token = comma == std::string_view::npos
            ? payload
            : payload.substr(0, comma);

        std::size_t index = 0;
        if (!parse_command_size(token, index)) {
            return std::nullopt;
        }
        indexes.push_back(index);

        if (comma == std::string_view::npos) {
            break;
        }
        payload.remove_prefix(comma + 1);
    }

    return indexes;
}

inline std::optional<std::vector<std::size_t>> command_size_list_arg(
    const scene::scene_command& command,
    std::string_view preferred_key,
    std::string_view fallback_key = {})
{
    const std::optional<std::string_view> value = command_string_arg(command, preferred_key, fallback_key);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return parse_command_size_list(*value);
}

inline app_command_route_result require_arg_names(
    const scene::scene_command& command,
    std::initializer_list<std::string_view> allowed)
{
    if (const std::optional<std::string> error = validate_command_arg_names(command, allowed)) {
        return route_command_error(*error);
    }
    app_command_route_result result;
    result.action = domain::make_continue_after_feedback_action();
    return result;
}

inline app_command_route_result route_required_string_payload(
    const scene::scene_command& command,
    std::string_view explicit_arg,
    std::string_view legacy_payload_arg,
    std::string_view value_name,
    domain::app_action (*factory)(std::string))
{
    if (auto arg_check = require_arg_names(command, {explicit_arg, legacy_payload_arg}); !arg_check.ok()) {
        return arg_check;
    }

    const std::optional<std::string_view> value = command_non_empty_string_arg(command, explicit_arg, legacy_payload_arg);
    if (!value.has_value()) {
        return route_command_error(command.name + " command requires a non-empty " + std::string(value_name) + " argument");
    }

    return route_command_action(factory(std::string(*value)));
}

inline app_command_route_result route_no_arg_command(
    const scene::scene_command& command,
    domain::app_action (*factory)())
{
    if (auto arg_check = require_arg_names(command, {}); !arg_check.ok()) {
        return arg_check;
    }
    return route_command_action(factory());
}

}  // namespace detail

inline bool is_scene_command_allowed(std::string_view command_name)
{
    return command_name == "load_source"
        || command_name == "select_deck"
        || command_name == "select_day"
        || command_name == "start_quiz"
        || command_name == "submit_option"
        || command_name == "submit_text_answer"
        || command_name == "submit_multiselect"
        || command_name == "skip_question"
        || command_name == "mark_question_known"
        || command_name == "mark_question_unknown"
        || command_name == "previous_question"
        || command_name == "continue_after_feedback"
        || command_name == "update_setting";
}

inline app_command_route_result route_scene_command(
    const scene::scene_command& command,
    std::optional<std::string_view> submitted_text = std::nullopt)
{
    if (command.name.empty()) {
        return detail::route_command_error("Scene command is missing a name");
    }

    if (command.name == "load_source") {
        return detail::route_required_string_payload(command, "source_uri", "payload", "source URI", domain::make_load_source_action);
    }
    if (command.name == "select_deck") {
        return detail::route_required_string_payload(command, "deck_id", "payload", "deck id", domain::make_select_deck_action);
    }
    if (command.name == "select_day") {
        return detail::route_required_string_payload(command, "day_id", "payload", "day id", domain::make_select_day_action);
    }

    if (command.name == "start_quiz") {
        if (auto arg_check = detail::require_arg_names(command, {"mode", "payload"}); !arg_check.ok()) {
            return arg_check;
        }

        const std::optional<std::string_view> mode_value = detail::command_non_empty_string_arg(command, "mode", "payload");
        if (!mode_value.has_value()) {
            return detail::route_command_error("start_quiz command requires a mode string argument");
        }

        const std::optional<domain::quiz_mode> mode = detail::parse_command_quiz_mode(*mode_value);
        if (!mode.has_value()) {
            return detail::route_command_error("start_quiz command mode must be normal, random, known, due, wrong, wrong_only, or wrong_note");
        }

        return detail::route_command_action(domain::make_start_quiz_action(*mode));
    }

    if (command.name == "submit_option") {
        if (auto arg_check = detail::require_arg_names(command, {"option_index", "payload"}); !arg_check.ok()) {
            return arg_check;
        }

        const std::optional<std::size_t> option_index = detail::command_size_arg(command, "option_index", "payload");
        if (!option_index.has_value()) {
            return detail::route_command_error("submit_option command requires a non-negative option_index argument");
        }

        return detail::route_command_action(domain::make_submit_option_action(*option_index));
    }

    if (command.name == "submit_text_answer") {
        if (auto arg_check = detail::require_arg_names(command, {"answer_text", "payload"}); !arg_check.ok()) {
            return arg_check;
        }

        if (const std::optional<std::string_view> answer_text = detail::command_non_empty_string_arg(command, "answer_text")) {
            return detail::route_command_action(domain::make_submit_text_answer_action(std::string(*answer_text)));
        }
        if (!submitted_text.has_value()) {
            return detail::route_command_error("submit_text_answer command requires submitted text or an answer_text argument");
        }

        return detail::route_command_action(domain::make_submit_text_answer_action(std::string(*submitted_text)));
    }

    if (command.name == "submit_multiselect") {
        if (auto arg_check = detail::require_arg_names(command, {"option_indexes", "payload"}); !arg_check.ok()) {
            return arg_check;
        }

        const std::optional<std::vector<std::size_t>> indexes = detail::command_size_list_arg(command, "option_indexes", "payload");
        if (!indexes.has_value()) {
            return detail::route_command_error("submit_multiselect command requires comma-separated non-negative option_indexes");
        }

        return detail::route_command_action(domain::make_submit_multiselect_action(*indexes));
    }

    if (command.name == "skip_question") {
        return detail::route_no_arg_command(command, domain::make_skip_question_action);
    }
    if (command.name == "mark_question_known") {
        return detail::route_no_arg_command(command, domain::make_mark_question_known_action);
    }
    if (command.name == "mark_question_unknown") {
        return detail::route_no_arg_command(command, domain::make_mark_question_unknown_action);
    }
    if (command.name == "previous_question") {
        return detail::route_no_arg_command(command, domain::make_previous_question_action);
    }
    if (command.name == "continue_after_feedback") {
        return detail::route_no_arg_command(command, domain::make_continue_after_feedback_action);
    }

    if (command.name == "update_setting") {
        if (auto arg_check = detail::require_arg_names(command, {"name", "value", "payload"}); !arg_check.ok()) {
            return arg_check;
        }

        const std::optional<std::string_view> name = detail::command_non_empty_string_arg(command, "name");
        const std::optional<std::string_view> value = detail::command_non_empty_string_arg(command, "value");
        if (name.has_value() || value.has_value()) {
            if (!name.has_value() || !value.has_value()) {
                return detail::route_command_error("update_setting command requires both name and value arguments");
            }
            return detail::route_command_action(domain::make_update_setting_action(std::string(*name), std::string(*value)));
        }

        const std::optional<std::string_view> payload = detail::command_non_empty_string_arg(command, "payload");
        if (!payload.has_value()) {
            return detail::route_command_error("update_setting command requires name/value arguments or a name=value payload");
        }

        const std::size_t separator = payload->find('=');
        if (separator == std::string_view::npos) {
            return detail::route_command_error("update_setting command payload must be formatted as name=value");
        }

        const std::string_view payload_name = detail::trim_command_token(payload->substr(0, separator));
        const std::string_view payload_value = detail::trim_command_token(payload->substr(separator + 1));
        if (payload_name.empty() || payload_value.empty()) {
            return detail::route_command_error("update_setting command payload must include both a setting name and value");
        }

        return detail::route_command_action(domain::make_update_setting_action(std::string(payload_name), std::string(payload_value)));
    }

    return detail::route_command_error("Unsupported scene command: " + command.name);
}

inline app_command_validation_result validate_scene_command(
    const scene::scene_command& command,
    std::optional<std::string_view> submitted_text = std::nullopt)
{
    app_command_validation_result result;
    const app_command_route_result routed = route_scene_command(command, submitted_text);
    if (!routed.ok()) {
        result.error = routed.error;
    }
    return result;
}

inline std::optional<scene::scene_action_binding> legacy_action_binding_for_scene_command(
    const scene::scene_command& command,
    scene::scene_action_trigger trigger = scene::scene_action_trigger::press)
{
    if (command.name.empty() || !is_scene_command_allowed(command.name)) {
        return std::nullopt;
    }

    scene::scene_action_binding binding;
    binding.trigger = trigger;
    binding.action_type = command.name;

    if (command.name == "load_source") {
        const std::optional<std::string_view> value = detail::command_non_empty_string_arg(command, "source_uri", "payload");
        if (!value.has_value()) {
            return std::nullopt;
        }
        binding.payload = std::string(*value);
        return binding;
    }
    if (command.name == "select_deck") {
        const std::optional<std::string_view> value = detail::command_non_empty_string_arg(command, "deck_id", "payload");
        if (!value.has_value()) {
            return std::nullopt;
        }
        binding.payload = std::string(*value);
        return binding;
    }
    if (command.name == "select_day") {
        const std::optional<std::string_view> value = detail::command_non_empty_string_arg(command, "day_id", "payload");
        if (!value.has_value()) {
            return std::nullopt;
        }
        binding.payload = std::string(*value);
        return binding;
    }
    if (command.name == "start_quiz") {
        const std::optional<std::string_view> value = detail::command_non_empty_string_arg(command, "mode", "payload");
        if (!value.has_value()) {
            return std::nullopt;
        }
        binding.payload = std::string(*value);
        return binding;
    }
    if (command.name == "submit_option") {
        const std::optional<std::size_t> value = detail::command_size_arg(command, "option_index", "payload");
        if (!value.has_value()) {
            return std::nullopt;
        }
        binding.payload = std::to_string(*value);
        return binding;
    }
    if (command.name == "submit_multiselect") {
        const std::optional<std::string_view> value = detail::command_non_empty_string_arg(command, "option_indexes", "payload");
        if (!value.has_value()) {
            return std::nullopt;
        }
        binding.payload = std::string(*value);
        return binding;
    }
    if (command.name == "submit_text_answer") {
        if (const std::optional<std::string_view> value = detail::command_non_empty_string_arg(command, "payload")) {
            binding.payload = std::string(*value);
        }
        return binding;
    }
    if (command.name == "update_setting") {
        const std::optional<std::string_view> payload = detail::command_non_empty_string_arg(command, "payload");
        if (payload.has_value()) {
            binding.payload = std::string(*payload);
            return binding;
        }

        const std::optional<std::string_view> name = detail::command_non_empty_string_arg(command, "name");
        const std::optional<std::string_view> value = detail::command_non_empty_string_arg(command, "value");
        if (!name.has_value() || !value.has_value()) {
            return std::nullopt;
        }
        binding.payload = std::string(*name) + "=" + std::string(*value);
        return binding;
    }

    return binding;
}

}  // namespace quiz_vulkan
