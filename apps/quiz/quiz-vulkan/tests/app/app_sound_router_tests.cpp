#include "app/app_sound_router.h"

#include <cassert>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::audio::sound_definition make_definition(const quiz_vulkan::audio::sound_id& id)
{
    return quiz_vulkan::audio::sound_definition{
        .id = id,
        .source_uri = "procedural://" + id,
        .bus = quiz_vulkan::audio::sound_bus::effects,
        .default_mode = quiz_vulkan::audio::sound_playback_mode::one_shot,
    };
}

quiz_vulkan::audio::sound_catalog make_sound_catalog(const quiz_vulkan::app_sound_route_config& config)
{
    quiz_vulkan::audio::sound_catalog catalog;
    catalog.fallback_definition = make_definition("quiz.fallback");
    catalog.definitions = {
        make_definition(config.action_start_quiz),
        make_definition(config.action_navigate),
        make_definition(config.action_continue_feedback),
        make_definition(config.action_previous_question),
        make_definition(config.action_skip_question),
        make_definition(config.action_mark_question_known),
        make_definition(config.action_mark_question_unknown),
        make_definition(config.feedback_correct),
        make_definition(config.feedback_incorrect),
        make_definition(config.feedback_skipped),
        make_definition(config.feedback_marked_known),
        make_definition(config.feedback_marked_unknown),
    };
    return catalog;
}

void test_feedback_outcomes_route_to_sound_requests()
{
    using namespace quiz_vulkan;

    const app_sound_route_config config;
    const domain::answer_record correct_feedback{
        .question_id = "q1",
        .outcome = domain::answer_outcome::correct,
    };

    const std::optional<audio::sound_playback_request> correct_request =
        sound_request_for_feedback(correct_feedback, 1001, config);
    require(correct_request.has_value(), "correct feedback routes to a sound request");
    require(correct_request->sound == config.feedback_correct, "correct feedback sound id is stable");
    require(correct_request->event_id == 1001, "feedback request carries event id");
    require(correct_request->mode == audio::sound_playback_mode::one_shot, "feedback request is one shot");

    const std::optional<audio::sound_playback_request> wrong_request =
        sound_request_for_feedback(domain::answer_outcome::incorrect, 1002, config);
    require(wrong_request.has_value(), "incorrect feedback routes to a sound request");
    require(wrong_request->sound == config.feedback_incorrect, "incorrect feedback sound id is stable");

    require(
        !sound_request_for_feedback(domain::answer_outcome::unanswered, 1003, config).has_value(),
        "unanswered feedback does not route to sound");
}

void test_action_events_route_without_submission_duplicates()
{
    using namespace quiz_vulkan;

    const app_sound_route_config config;

    const std::optional<audio::sound_playback_request> start_request =
        sound_request_for_action(domain::make_start_quiz_action(domain::quiz_mode::normal), 2001, config);
    require(start_request.has_value(), "start quiz action routes to sound");
    require(start_request->sound == config.action_start_quiz, "start quiz action sound id is stable");
    require(start_request->event_id == 2001, "action request carries event id");

    const std::optional<audio::sound_playback_request> deck_request =
        sound_request_for_action(domain::make_select_deck_action("math"), 2002, config);
    require(deck_request.has_value(), "select deck action routes to navigation sound");
    require(deck_request->sound == config.action_navigate, "select deck uses navigation sound id");

    const std::optional<audio::sound_playback_request> continue_request =
        sound_request_for_action(domain::make_continue_after_feedback_action(), 2003, config);
    require(continue_request.has_value(), "continue feedback action routes to sound");
    require(continue_request->sound == config.action_continue_feedback, "continue action sound id is stable");

    const std::optional<audio::sound_playback_request> submit_request =
        sound_request_for_action(domain::make_submit_option_action(0), 2004, config);
    require(!submit_request.has_value(), "submit option waits for feedback outcome sound");
}

void test_catalog_engine_queues_routed_feedback()
{
    using namespace quiz_vulkan;

    const app_sound_route_config config;
    audio::catalog_audio_engine engine(make_sound_catalog(config));

    const std::optional<audio::sound_playback_request> request =
        sound_request_for_feedback(domain::answer_outcome::correct, 3001, config);
    require(request.has_value(), "correct feedback produces request for catalog engine");

    const audio::sound_playback_result result = engine.play_sound(*request);
    require(result.queued(), "catalog audio engine queues routed feedback");
    require(result.definition.id == config.feedback_correct, "queued feedback resolves catalog definition");
    require(engine.events().size() == 1, "catalog audio engine records routed event");
    require(engine.events().front().id == 3001, "catalog event preserves routed event id");
    require(engine.events().front().sound == config.feedback_correct, "catalog event preserves routed sound id");
}

void test_null_backend_reports_no_device_for_routed_action()
{
    using namespace quiz_vulkan;

    const app_sound_route_config config;
    const std::optional<audio::sound_playback_request> request =
        sound_request_for_action(domain::make_previous_question_action(), 4001, config);
    require(request.has_value(), "previous question action produces request for null engine");

    audio::null_audio_engine engine;
    const audio::sound_playback_result unavailable = engine.play_sound(*request);
    require(
        unavailable.status == audio::sound_playback_status::backend_unavailable,
        "null audio engine reports no device for routed action");
    require(unavailable.definition.id == config.action_previous_question, "null engine preserves routed sound id");

    engine.set_muted(true);
    const audio::sound_playback_result muted = engine.play_sound(*request);
    require(muted.status == audio::sound_playback_status::muted, "null engine mute still applies to routed action");
}

void test_empty_config_entry_disables_route()
{
    using namespace quiz_vulkan;

    app_sound_route_config config;
    config.feedback_correct.clear();

    require(
        !sound_request_for_feedback(domain::answer_outcome::correct, 5001, config).has_value(),
        "empty sound id disables a feedback route");
}

}  // namespace

int main()
{
    test_feedback_outcomes_route_to_sound_requests();
    test_action_events_route_without_submission_duplicates();
    test_catalog_engine_queues_routed_feedback();
    test_null_backend_reports_no_device_for_routed_action();
    test_empty_config_entry_disables_route();
    return 0;
}
