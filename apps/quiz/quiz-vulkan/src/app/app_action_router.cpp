#include "app_action_router.h"

#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace quiz_vulkan {
namespace {

app_action_route_result route_action(domain::app_action action)
{
    app_action_route_result result;
    result.action = std::move(action);
    return result;
}

app_action_route_result route_error(std::string error)
{
    app_action_route_result result;
    result.error = std::move(error);
    return result;
}

std::string_view trim(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) {
        value.remove_suffix(1);
    }
    return value;
}

std::string normalize_token(std::string_view value)
{
    value = trim(value);

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

bool parse_size_t(std::string_view value, std::size_t& parsed)
{
    value = trim(value);
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

std::optional<std::vector<std::size_t>> parse_multiselect_indexes(std::string_view payload)
{
    std::vector<std::size_t> indexes;

    while (true) {
        const std::size_t comma = payload.find(',');
        const std::string_view token = comma == std::string_view::npos
            ? payload
            : payload.substr(0, comma);

        std::size_t index = 0;
        if (!parse_size_t(token, index)) {
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

std::optional<domain::quiz_mode> parse_router_quiz_mode(std::string_view payload)
{
    if (const std::optional<domain::quiz_mode> parsed = domain::parse_quiz_mode(payload)) {
        return parsed;
    }

    const std::string normalized = normalize_token(payload);
    if (normalized == "due") {
        return domain::quiz_mode::normal;
    }
    if (normalized == "wrong") {
        return domain::quiz_mode::wrong_only;
    }

    return std::nullopt;
}

app_action_route_result require_payload_action(
    const scene::scene_action_binding& binding,
    std::string_view payload_name,
    domain::app_action (*factory)(std::string))
{
    if (trim(binding.payload).empty()) {
        return route_error(std::string(binding.action_type) + " requires a non-empty " + std::string(payload_name));
    }

    return route_action(factory(binding.payload));
}

}  // namespace

bool app_action_route_result::ok() const
{
    return action.has_value() && error.empty();
}

app_action_route_result route_scene_action(
    const scene::scene_action_binding& binding,
    std::optional<std::string_view> submitted_text)
{
    if (binding.action_type.empty()) {
        return route_error("Scene action is missing an action_type");
    }

    if (binding.action_type == "load_source") {
        return require_payload_action(binding, "source URI", domain::make_load_source_action);
    }
    if (binding.action_type == "select_deck") {
        return require_payload_action(binding, "deck id", domain::make_select_deck_action);
    }
    if (binding.action_type == "select_day") {
        return require_payload_action(binding, "day id", domain::make_select_day_action);
    }

    if (binding.action_type == "start_quiz") {
        const std::optional<domain::quiz_mode> mode = parse_router_quiz_mode(binding.payload);
        if (!mode.has_value()) {
            return route_error("start_quiz requires a quiz mode payload: normal, random, known, due, wrong, wrong_only, or wrong_note");
        }
        return route_action(domain::make_start_quiz_action(*mode));
    }

    if (binding.action_type == "submit_option") {
        std::size_t option_index = 0;
        if (!parse_size_t(binding.payload, option_index)) {
            return route_error("submit_option payload must be a non-negative integer");
        }
        return route_action(domain::make_submit_option_action(option_index));
    }

    if (binding.action_type == "submit_text_answer") {
        if (!submitted_text.has_value()) {
            return route_error("submit_text_answer requires submitted text; binding payload is not answer text");
        }
        return route_action(domain::make_submit_text_answer_action(std::string(*submitted_text)));
    }

    if (binding.action_type == "submit_multiselect") {
        const std::optional<std::vector<std::size_t>> indexes = parse_multiselect_indexes(binding.payload);
        if (!indexes.has_value()) {
            return route_error("submit_multiselect payload must be comma-separated non-negative integers");
        }
        return route_action(domain::make_submit_multiselect_action(*indexes));
    }

    if (binding.action_type == "skip_question") {
        return route_action(domain::make_skip_question_action());
    }
    if (binding.action_type == "mark_question_known") {
        return route_action(domain::make_mark_question_known_action());
    }
    if (binding.action_type == "mark_question_unknown") {
        return route_action(domain::make_mark_question_unknown_action());
    }
    if (binding.action_type == "previous_question") {
        return route_action(domain::make_previous_question_action());
    }
    if (binding.action_type == "continue_after_feedback") {
        return route_action(domain::make_continue_after_feedback_action());
    }

    if (binding.action_type == "update_setting") {
        const std::size_t separator = binding.payload.find('=');
        if (separator == std::string::npos) {
            return route_error("update_setting payload must be formatted as name=value");
        }

        const std::string_view name = trim(std::string_view(binding.payload).substr(0, separator));
        const std::string_view value = trim(std::string_view(binding.payload).substr(separator + 1));
        if (name.empty() || value.empty()) {
            return route_error("update_setting payload must include both a setting name and value");
        }

        return route_action(domain::make_update_setting_action(std::string(name), std::string(value)));
    }

    return route_error("Unsupported scene action type: " + binding.action_type);
}

}  // namespace quiz_vulkan
