#pragma once

#include "core/scene/scene_layout_data.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace quiz_vulkan::presentation {

inline constexpr std::string_view app_shell_role = "app_shell";
inline constexpr std::string_view quiz_question_stage_role = "quiz_question_stage";
inline constexpr std::string_view quiz_question_header_role = "quiz_question_header";
inline constexpr std::string_view quiz_question_prompt_role = "quiz_question_prompt";
inline constexpr std::string_view quiz_question_body_role = "quiz_question_body";
inline constexpr std::string_view quiz_question_image_role = "quiz_question_image";
inline constexpr std::string_view quiz_option_group_role = "quiz_option_group";
inline constexpr std::string_view quiz_option_role = "quiz_option";
inline constexpr std::string_view quiz_feedback_role = "quiz_feedback";
inline constexpr std::string_view quiz_answer_input_role = "quiz_answer_input";
inline constexpr std::string_view quiz_answer_dock_role = "quiz_answer_dock";
inline constexpr std::string_view quiz_controls_role = "quiz_controls";

inline constexpr std::string_view quiz_stage_property = "quiz.stage";
inline constexpr std::string_view quiz_feedback_property = "quiz.feedback";
inline constexpr std::string_view quiz_option_state_property = "quiz.option_state";
inline constexpr std::string_view quiz_question_length_property = "quiz.question_length";
inline constexpr std::string_view quiz_option_index_property = "quiz.option_index";
inline constexpr std::string_view quiz_reveal_correctness_property = "quiz.reveal_correctness";
inline constexpr std::string_view quiz_accepts_keyboard_input_property = "quiz.accepts_keyboard_input";

enum class quiz_scene_stage {
    none,
    question,
    feedback,
    completed,
};

enum class quiz_feedback_state {
    none,
    correct,
    incorrect,
    skipped,
    marked_unknown,
};

enum class quiz_option_state {
    idle,
    selected,
    correct,
    incorrect,
    disabled,
};

enum class quiz_question_length_class {
    unspecified,
    short_question,
    long_question,
};

inline const char* to_string(quiz_scene_stage stage)
{
    switch (stage) {
        case quiz_scene_stage::none:
            return "none";
        case quiz_scene_stage::question:
            return "question";
        case quiz_scene_stage::feedback:
            return "feedback";
        case quiz_scene_stage::completed:
            return "completed";
    }

    return "none";
}

inline const char* to_string(quiz_feedback_state feedback)
{
    switch (feedback) {
        case quiz_feedback_state::none:
            return "none";
        case quiz_feedback_state::correct:
            return "correct";
        case quiz_feedback_state::incorrect:
            return "incorrect";
        case quiz_feedback_state::skipped:
            return "skipped";
        case quiz_feedback_state::marked_unknown:
            return "marked_unknown";
    }

    return "none";
}

inline const char* to_string(quiz_option_state state)
{
    switch (state) {
        case quiz_option_state::idle:
            return "idle";
        case quiz_option_state::selected:
            return "selected";
        case quiz_option_state::correct:
            return "correct";
        case quiz_option_state::incorrect:
            return "incorrect";
        case quiz_option_state::disabled:
            return "disabled";
    }

    return "idle";
}

inline const char* to_string(quiz_question_length_class length_class)
{
    switch (length_class) {
        case quiz_question_length_class::unspecified:
            return "unspecified";
        case quiz_question_length_class::short_question:
            return "short";
        case quiz_question_length_class::long_question:
            return "long";
    }

    return "unspecified";
}

inline quiz_question_length_class classify_quiz_question_length(
    const std::string& prompt,
    const std::string& body = std::string(),
    std::size_t long_threshold = 120)
{
    return prompt.size() + body.size() >= long_threshold
        ? quiz_question_length_class::long_question
        : quiz_question_length_class::short_question;
}

inline void set_semantic_string(
    scene::scene_node_semantics& semantics,
    std::string_view key,
    std::string_view value)
{
    semantics.set_property(std::string(key), scene::scene_value(std::string(value)));
}

inline void set_semantic_bool(
    scene::scene_node_semantics& semantics,
    std::string_view key,
    bool value)
{
    semantics.set_property(std::string(key), scene::scene_value(value));
}

inline void set_semantic_index(
    scene::scene_node_semantics& semantics,
    std::string_view key,
    std::size_t value)
{
    semantics.set_property(std::string(key), scene::scene_value(value));
}

inline scene::scene_node_semantics make_quiz_semantics(std::string_view role)
{
    scene::scene_node_semantics semantics;
    semantics.role = std::string(role);
    return semantics;
}

inline bool has_semantic_role(const scene::scene_node_semantics& semantics, std::string_view role)
{
    return semantics.role == role;
}

inline bool accepts_keyboard_input(const scene::scene_node_semantics& semantics)
{
    const bool* value = semantics.bool_property(quiz_accepts_keyboard_input_property);
    return value != nullptr && *value;
}

inline std::string semantic_string_or(
    const scene::scene_node_semantics& semantics,
    std::string_view key,
    std::string fallback)
{
    const std::string* value = semantics.string_property(key);
    return value == nullptr ? std::move(fallback) : *value;
}

} // namespace quiz_vulkan::presentation
