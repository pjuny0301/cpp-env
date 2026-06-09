#pragma once

#include "app/app_quiz_screens.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace quiz_vulkan::presentation {

inline constexpr std::string_view day_intro_screen_script_json = R"json({
  "schema_version": 1,
  "template": "builtin:quiz.day_intro.v1",
  "screen": "day_intro"
})json";

struct app_scene_script_document {
    int schema_version = 0;
    std::string template_id;
    std::string screen;
};

struct app_scene_script_parse_result {
    std::optional<app_scene_script_document> document;
    std::string error;

    bool ok() const
    {
        return document.has_value() && error.empty();
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

namespace detail {

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

}  // namespace detail

inline app_scene_script_parse_result parse_app_scene_script_json(std::string_view json)
{
    app_scene_script_parse_result result;
    app_scene_script_document document;

    if (const std::optional<int> schema_version = detail::find_json_int_field(json, "schema_version", result.error)) {
        document.schema_version = *schema_version;
    } else {
        return result;
    }

    if (const std::optional<std::string> template_id = detail::find_json_string_field(json, "template", result.error)) {
        document.template_id = *template_id;
    } else {
        return result;
    }

    if (const std::optional<std::string> screen = detail::find_json_string_field(json, "screen", result.error)) {
        document.screen = *screen;
    } else {
        return result;
    }

    result.document = std::move(document);
    return result;
}

inline app_scene_script_compile_result compile_app_scene_script(
    const app_scene_script_document& document,
    const domain::app_snapshot& snapshot)
{
    app_scene_script_compile_result result;

    if (document.schema_version != 1) {
        result.error = "unsupported app scene script schema_version: " + std::to_string(document.schema_version);
        return result;
    }

    if (document.template_id == "builtin:quiz.day_intro.v1" && document.screen == "day_intro") {
        result.patch = make_day_intro_screen_patch(snapshot);
        return result;
    }

    result.error = "unsupported app scene script template: " + document.template_id;
    return result;
}

}  // namespace quiz_vulkan::presentation
