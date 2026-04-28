#include "app/app_sound_router.h"

#include <concepts>
#include <optional>

namespace {

using namespace quiz_vulkan;

static_assert(requires(app_sound_route_config config) {
    { config.action_start_quiz } -> std::same_as<audio::sound_id&>;
    { config.action_navigate } -> std::same_as<audio::sound_id&>;
    { config.feedback_correct } -> std::same_as<audio::sound_id&>;
    { config.feedback_incorrect } -> std::same_as<audio::sound_id&>;
});

static_assert(requires(
    const domain::app_action& action,
    const domain::answer_record& feedback,
    domain::answer_outcome outcome,
    audio::sound_event_id event_id,
    const app_sound_route_config& config) {
    { sound_request_for_action(action, event_id, config) } -> std::same_as<std::optional<audio::sound_playback_request>>;
    { sound_request_for_feedback(feedback, event_id, config) } -> std::same_as<std::optional<audio::sound_playback_request>>;
    { sound_request_for_feedback(outcome, event_id, config) } -> std::same_as<std::optional<audio::sound_playback_request>>;
});

}  // namespace
