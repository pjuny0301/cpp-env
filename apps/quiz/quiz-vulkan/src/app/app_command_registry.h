#pragma once

#include "core/domain/app_action.hpp"
#include "core/scene/scene_layout_data.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace quiz_vulkan {

struct app_command_route_result {
    std::optional<domain::app_action> action;
    std::string error;

    bool ok() const
    {
        return action.has_value() && error.empty();
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

}  // namespace detail

inline app_command_route_result route_scene_command(const scene::scene_command& command)
{
    if (command.name.empty()) {
        return detail::route_command_error("Scene command is missing a name");
    }

    if (command.name == "start_quiz") {
        const std::optional<std::string_view> mode_value = detail::command_string_arg(command, "mode", "payload");
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
        const std::optional<std::size_t> option_index = detail::command_size_arg(command, "option_index", "payload");
        if (!option_index.has_value()) {
            return detail::route_command_error("submit_option command requires a non-negative option_index argument");
        }

        return detail::route_command_action(domain::make_submit_option_action(*option_index));
    }

    if (command.name == "continue_after_feedback") {
        return detail::route_command_action(domain::make_continue_after_feedback_action());
    }

    return detail::route_command_error("Unsupported scene command: " + command.name);
}

}  // namespace quiz_vulkan
