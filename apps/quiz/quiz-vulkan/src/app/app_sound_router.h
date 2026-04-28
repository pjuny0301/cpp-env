#pragma once

#include "audio/audio_engine.h"
#include "core/domain/app_action.hpp"
#include "core/domain/quiz_session.hpp"

#include <optional>
#include <variant>

namespace quiz_vulkan {

struct app_sound_route_config {
    audio::sound_id action_start_quiz = "quiz.action.start";
    audio::sound_id action_navigate = "quiz.action.navigate";
    audio::sound_id action_continue_feedback = "quiz.action.continue_feedback";
    audio::sound_id action_previous_question = "quiz.action.previous_question";
    audio::sound_id action_skip_question = "quiz.action.skip_question";
    audio::sound_id action_mark_question_known = "quiz.action.mark_question_known";
    audio::sound_id action_mark_question_unknown = "quiz.action.mark_question_unknown";
    audio::sound_id feedback_correct = "quiz.feedback.correct";
    audio::sound_id feedback_incorrect = "quiz.feedback.incorrect";
    audio::sound_id feedback_skipped = "quiz.feedback.skipped";
    audio::sound_id feedback_marked_known = "quiz.feedback.marked_known";
    audio::sound_id feedback_marked_unknown = "quiz.feedback.marked_unknown";
};

inline std::optional<audio::sound_playback_request> make_app_sound_request(
    const audio::sound_id& sound,
    audio::sound_event_id event_id)
{
    if (sound.empty()) {
        return std::nullopt;
    }

    return audio::sound_playback_request{
        .sound = sound,
        .event_id = event_id,
        .mode = audio::sound_playback_mode::one_shot,
    };
}

inline std::optional<audio::sound_playback_request> sound_request_for_feedback(
    domain::answer_outcome outcome,
    audio::sound_event_id event_id = 0,
    const app_sound_route_config& config = app_sound_route_config{})
{
    switch (outcome) {
    case domain::answer_outcome::correct:
        return make_app_sound_request(config.feedback_correct, event_id);
    case domain::answer_outcome::incorrect:
        return make_app_sound_request(config.feedback_incorrect, event_id);
    case domain::answer_outcome::skipped:
        return make_app_sound_request(config.feedback_skipped, event_id);
    case domain::answer_outcome::marked_known:
        return make_app_sound_request(config.feedback_marked_known, event_id);
    case domain::answer_outcome::marked_unknown:
        return make_app_sound_request(config.feedback_marked_unknown, event_id);
    case domain::answer_outcome::unanswered:
        return std::nullopt;
    }

    return std::nullopt;
}

inline std::optional<audio::sound_playback_request> sound_request_for_feedback(
    const domain::answer_record& feedback,
    audio::sound_event_id event_id = 0,
    const app_sound_route_config& config = app_sound_route_config{})
{
    return sound_request_for_feedback(feedback.outcome, event_id, config);
}

inline std::optional<audio::sound_playback_request> sound_request_for_action(
    const domain::app_action& action,
    audio::sound_event_id event_id = 0,
    const app_sound_route_config& config = app_sound_route_config{})
{
    const domain::app_action_payload& payload = action.payload;

    if (std::holds_alternative<domain::load_source_action>(payload)
        || std::holds_alternative<domain::select_deck_action>(payload)
        || std::holds_alternative<domain::select_day_action>(payload)) {
        return make_app_sound_request(config.action_navigate, event_id);
    }

    if (std::holds_alternative<domain::start_quiz_action>(payload)) {
        return make_app_sound_request(config.action_start_quiz, event_id);
    }

    if (std::holds_alternative<domain::continue_after_feedback_action>(payload)) {
        return make_app_sound_request(config.action_continue_feedback, event_id);
    }

    if (std::holds_alternative<domain::previous_question_action>(payload)) {
        return make_app_sound_request(config.action_previous_question, event_id);
    }

    if (std::holds_alternative<domain::skip_question_action>(payload)) {
        return make_app_sound_request(config.action_skip_question, event_id);
    }

    if (std::holds_alternative<domain::mark_question_known_action>(payload)) {
        return make_app_sound_request(config.action_mark_question_known, event_id);
    }

    if (std::holds_alternative<domain::mark_question_unknown_action>(payload)) {
        return make_app_sound_request(config.action_mark_question_unknown, event_id);
    }

    return std::nullopt;
}

}  // namespace quiz_vulkan
