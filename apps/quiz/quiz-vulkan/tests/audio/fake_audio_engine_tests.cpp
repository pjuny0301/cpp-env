#include "audio/audio_engine.h"

#include <cassert>
#include <cstdio>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::audio::sound_catalog make_sound_catalog()
{
    quiz_vulkan::audio::sound_catalog catalog;
    catalog.fallback_definition = quiz_vulkan::audio::sound_definition{
        .id = "fallback",
        .source_uri = "asset://sounds/fallback.wav",
        .bus = quiz_vulkan::audio::sound_bus::effects,
        .default_mode = quiz_vulkan::audio::sound_playback_mode::one_shot,
    };
    catalog.definitions.push_back(quiz_vulkan::audio::sound_definition{
        .id = "answer.correct",
        .source_uri = "asset://sounds/answer-correct.wav",
        .bus = quiz_vulkan::audio::sound_bus::effects,
        .default_mode = quiz_vulkan::audio::sound_playback_mode::one_shot,
    });
    catalog.definitions.push_back(quiz_vulkan::audio::sound_definition{
        .id = "round.timer",
        .source_uri = "asset://sounds/round-timer.wav",
        .bus = quiz_vulkan::audio::sound_bus::music,
        .default_mode = quiz_vulkan::audio::sound_playback_mode::looping,
    });
    return catalog;
}

void test_sound_catalog_find_and_resolve()
{
    const quiz_vulkan::audio::sound_catalog catalog = make_sound_catalog();

    const quiz_vulkan::audio::sound_definition* correct = catalog.find("answer.correct");
    require(correct != nullptr, "sound catalog finds registered definition");
    require(correct->source_uri == "asset://sounds/answer-correct.wav", "sound definition preserves source uri");
    require(correct->valid(), "registered sound definition is valid");
    require(catalog.find("missing") == nullptr, "sound catalog reports missing definition");
    require(catalog.resolve("round.timer").default_mode == quiz_vulkan::audio::sound_playback_mode::looping,
        "sound catalog resolves registered definition");
    require(catalog.resolve("missing").id == "fallback", "sound catalog resolves missing definition to fallback");
}

void test_request_result_and_event_shape()
{
    using namespace quiz_vulkan::audio;

    catalog_audio_engine engine(make_sound_catalog());
    const sound_playback_request request{
        .sound = "answer.correct",
        .event_id = 42,
        .mode = sound_playback_mode::one_shot,
    };

    const sound_playback_result result = engine.play_sound(request);
    require(result.queued(), "catalog engine queues registered sound");
    require(result.playback_id == 1, "first playback id is stable");
    require(result.definition.id == "answer.correct", "play result carries resolved definition");
    require(engine.catalog().find("answer.correct") != nullptr, "catalog engine exposes its source catalog");
    require(engine.active_playbacks().size() == 1, "catalog engine tracks active playback");
    require(engine.active_playbacks().front().playback_id == result.playback_id, "active playback carries id");
    require(engine.active_playbacks().front().mode == sound_playback_mode::one_shot, "active playback stores request mode");
    require(engine.events().size() == 1, "catalog engine emits play event");
    require(engine.events().front().id == 42, "play event carries request event id");
    require(engine.events().front().kind == sound_event_kind::play, "play event records kind");
    require(engine.events().front().sound == "answer.correct", "play event carries sound id");
    require(engine.events().front().playback_id == result.playback_id, "play event carries playback id");
}

void test_missing_definition_uses_fallback_without_queueing()
{
    using namespace quiz_vulkan::audio;

    catalog_audio_engine engine(make_sound_catalog());
    const sound_playback_result result = engine.play_sound(sound_playback_request{.sound = "missing"});

    require(result.status == sound_playback_status::missing_definition, "missing sound reports missing definition");
    require(result.playback_id == 0, "missing sound is not queued");
    require(result.definition.id == "fallback", "missing sound result carries fallback definition");
    require(engine.active_playbacks().empty(), "missing sound leaves active playback list empty");
    require(engine.events().empty(), "missing sound does not emit playback event");
}

void test_stop_and_mute_behavior()
{
    using namespace quiz_vulkan::audio;

    catalog_audio_engine engine(make_sound_catalog());
    const sound_playback_result first = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    const sound_playback_result second = engine.play_sound(sound_playback_request{.sound = "round.timer"});
    require(first.queued() && second.queued(), "catalog engine queues two sounds");
    require(engine.active_playbacks().size() == 2, "two playbacks are active");

    engine.stop_sound(sound_stop_request{.playback_id = first.playback_id, .sound = "answer.correct"});
    require(engine.active_playbacks().size() == 1, "stop request removes selected playback");
    require(engine.active_playbacks().front().playback_id == second.playback_id, "stop request preserves other playback");
    require(engine.events().back().kind == sound_event_kind::stop, "stop request emits stop event");
    require(engine.events().back().playback_id == first.playback_id, "stop event records selected playback");

    engine.set_muted(true);
    require(engine.muted(), "catalog engine mute flag is set");
    require(engine.events().back().kind == sound_event_kind::mute_changed, "mute change emits event");
    require(engine.events().back().muted, "mute change event records muted state");

    const sound_playback_result muted_result = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    require(muted_result.status == sound_playback_status::muted, "muted engine suppresses playback");
    require(muted_result.playback_id == 0, "muted sound is not queued");
    require(engine.active_playbacks().size() == 1, "muted request does not add active playback");
    require(engine.events().back().muted, "muted play event records muted state");

    engine.stop_all_sounds();
    require(engine.active_playbacks().empty(), "stop all clears active playbacks");
    require(engine.events().back().kind == sound_event_kind::stop_all, "stop all emits stop_all event");
}

void test_stop_all_instances_by_sound()
{
    using namespace quiz_vulkan::audio;

    catalog_audio_engine engine(make_sound_catalog());
    const sound_playback_result first = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    const sound_playback_result second = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    const sound_playback_result timer = engine.play_sound(sound_playback_request{
        .sound = "round.timer",
        .mode = sound_playback_mode::looping,
    });
    require(first.queued() && second.queued() && timer.queued(), "catalog engine queues repeated sounds");

    engine.stop_sound(sound_stop_request{.sound = "answer.correct", .all_instances = true});
    require(engine.active_playbacks().size() == 1, "stop all instances removes matching sound playbacks");
    require(engine.active_playbacks().front().sound == "round.timer", "stop all instances preserves other sounds");
    require(engine.active_playbacks().front().mode == sound_playback_mode::looping, "active playback stores looping mode");
    require(engine.events().back().kind == sound_event_kind::stop, "stop all instances emits stop event");
    require(engine.events().back().sound == "answer.correct", "stop all instances records sound id");
}

void test_event_log_can_be_cleared_without_affecting_playback()
{
    using namespace quiz_vulkan::audio;

    catalog_audio_engine engine(make_sound_catalog());
    const sound_playback_result result = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    require(result.queued(), "catalog engine queues sound before clearing events");
    require(!engine.events().empty(), "catalog engine records event before clear");

    engine.clear_events();
    require(engine.events().empty(), "catalog engine clears event log");
    require(engine.active_playbacks().size() == 1, "clearing events leaves active playback untouched");
}

void test_null_audio_engine_is_backend_free_and_mute_aware()
{
    using namespace quiz_vulkan::audio;

    null_audio_engine engine;
    const sound_playback_result unavailable = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    require(unavailable.status == sound_playback_status::backend_unavailable, "null engine reports no backend");
    require(unavailable.playback_id == 0, "null engine does not queue playback");

    engine.set_muted(true);
    const sound_playback_result muted = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    require(engine.muted(), "null engine stores mute state");
    require(muted.status == sound_playback_status::muted, "null engine reports muted playback");
}

} // namespace

int main()
{
    test_sound_catalog_find_and_resolve();
    test_request_result_and_event_shape();
    test_missing_definition_uses_fallback_without_queueing();
    test_stop_and_mute_behavior();
    test_stop_all_instances_by_sound();
    test_event_log_can_be_cleared_without_affecting_playback();
    test_null_audio_engine_is_backend_free_and_mute_aware();
    return 0;
}
