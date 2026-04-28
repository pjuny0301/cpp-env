#include "audio/audio_engine.h"

#include <cassert>
#include <cstdio>
#include <utility>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

quiz_vulkan::audio::sound_definition definition(
    quiz_vulkan::audio::sound_id id,
    quiz_vulkan::audio::sound_bus bus,
    quiz_vulkan::audio::sound_playback_mode mode = quiz_vulkan::audio::sound_playback_mode::one_shot)
{
    return quiz_vulkan::audio::sound_definition{
        .id = std::move(id),
        .source_uri = "procedural://tone",
        .bus = bus,
        .default_mode = mode,
    };
}

quiz_vulkan::audio::sound_catalog make_catalog()
{
    quiz_vulkan::audio::sound_catalog catalog;
    catalog.fallback_definition = definition("fallback", quiz_vulkan::audio::sound_bus::effects);
    catalog.definitions.push_back(definition("answer.correct", quiz_vulkan::audio::sound_bus::effects));
    catalog.definitions.push_back(definition(
        "round.timer",
        quiz_vulkan::audio::sound_bus::music,
        quiz_vulkan::audio::sound_playback_mode::looping));
    catalog.definitions.push_back(definition("voice.prompt", quiz_vulkan::audio::sound_bus::voice));
    return catalog;
}

void test_catalog_mixer_records_queued_playback()
{
    using namespace quiz_vulkan::audio;

    catalog_audio_engine engine(make_catalog());
    engine.set_bus_gain(sound_bus::music, 0.25f);
    engine.clear_mixer_events();

    const sound_playback_result result = engine.play_sound(sound_playback_request{
        .sound = "round.timer",
        .event_id = 100,
        .mode = sound_playback_mode::looping,
    });

    require(result.queued(), "music playback is queued");
    require(engine.active_playbacks().size() == 1, "queued playback is active");
    require(engine.active_playbacks().front().bus == sound_bus::music, "active playback records bus");
    require(engine.active_playbacks().front().gain == 0.25f, "active playback records effective gain");
    require(engine.mixer_events().size() == 1, "queued playback emits mixer event");

    const sound_mixer_event& event = engine.mixer_events().front();
    require(event.kind == sound_mixer_event_kind::playback_queued, "mixer event records queued playback");
    require(event.id == 100, "mixer event preserves request event id");
    require(event.sound == "round.timer", "mixer event preserves sound id");
    require(event.playback_id == result.playback_id, "mixer event preserves playback id");
    require(event.status == sound_playback_status::queued, "mixer event records queued status");
    require(event.bus == sound_bus::music, "mixer event records bus");
    require(event.mode == sound_playback_mode::looping, "mixer event records mode");
    require(event.gain == 0.25f, "mixer event records effective gain");
    require(event.affected_playback_count == 1, "mixer event reports affected playback");
}

void test_bus_mute_and_gain_suppress_new_playback_without_clearing_active()
{
    using namespace quiz_vulkan::audio;

    catalog_audio_engine engine(make_catalog());
    const sound_playback_result active = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    require(active.queued(), "setup playback is queued");

    engine.set_bus_muted(sound_bus::effects, true);
    require(engine.bus_muted(sound_bus::effects), "effects bus mute is stored");
    require(engine.active_playbacks().size() == 1, "bus mute does not clear active playback bookkeeping");
    require(engine.active_playbacks().front().gain == 0.0f, "bus mute updates active playback effective gain");
    require(engine.mixer_events().back().kind == sound_mixer_event_kind::bus_mute_changed,
        "bus mute emits mixer event");
    require(engine.mixer_events().back().affected_playback_count == 1,
        "bus mute event reports active bus playback");

    const sound_playback_result muted = engine.play_sound(sound_playback_request{
        .sound = "answer.correct",
        .event_id = 200,
    });
    require(muted.status == sound_playback_status::muted, "bus-muted playback is suppressed");
    require(engine.active_playbacks().size() == 1, "suppressed playback is not made active");
    require(engine.mixer_events().back().kind == sound_mixer_event_kind::playback_suppressed,
        "suppressed playback emits mixer event");
    require(engine.mixer_events().back().status == sound_playback_status::muted,
        "suppressed playback event records muted status");

    engine.set_bus_muted(sound_bus::effects, false);
    engine.set_bus_gain(sound_bus::effects, 0.0f);
    require(engine.bus_gain(sound_bus::effects) == 0.0f, "zero bus gain is stored");
    require(engine.active_playbacks().front().gain == 0.0f, "zero bus gain updates active playback gain");

    const sound_playback_result silent = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    require(silent.status == sound_playback_status::muted, "zero-gain playback is suppressed");
    require(silent.diagnostic == "sound bus gain is zero", "zero-gain playback diagnostic is explicit");
}

void test_stop_events_report_affected_playbacks()
{
    using namespace quiz_vulkan::audio;

    catalog_audio_engine engine(make_catalog());
    const sound_playback_result first = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    const sound_playback_result second = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    const sound_playback_result timer = engine.play_sound(sound_playback_request{.sound = "round.timer"});
    require(first.queued() && second.queued() && timer.queued(), "setup playbacks are queued");
    engine.clear_mixer_events();

    engine.stop_sound(sound_stop_request{.sound = "answer.correct", .all_instances = true});
    require(engine.active_playbacks().size() == 1, "stop all instances leaves unrelated playback");
    require(engine.mixer_events().back().kind == sound_mixer_event_kind::playback_stopped,
        "stop emits mixer event");
    require(engine.mixer_events().back().affected_playback_count == 2,
        "stop all instances reports removed playbacks");

    engine.stop_all_sounds();
    require(engine.active_playbacks().empty(), "stop all clears active playbacks");
    require(engine.mixer_events().back().kind == sound_mixer_event_kind::all_playbacks_stopped,
        "stop all emits mixer event");
    require(engine.mixer_events().back().affected_playback_count == 1,
        "stop all reports remaining playback");
}

void test_missing_definition_and_null_backend_emit_suppressed_mixer_events()
{
    using namespace quiz_vulkan::audio;

    catalog_audio_engine catalog_engine(make_catalog());
    const sound_playback_result missing = catalog_engine.play_sound(sound_playback_request{
        .sound = "missing",
        .event_id = 300,
    });
    require(missing.status == sound_playback_status::missing_definition, "missing sound is not queued");
    require(catalog_engine.events().empty(), "missing catalog sound does not emit legacy playback event");
    require(catalog_engine.mixer_events().size() == 1, "missing catalog sound emits mixer event");
    require(catalog_engine.mixer_events().front().kind == sound_mixer_event_kind::playback_suppressed,
        "missing catalog sound is recorded as suppressed");
    require(catalog_engine.mixer_events().front().status == sound_playback_status::missing_definition,
        "missing catalog mixer event records status");
    require(catalog_engine.mixer_events().front().definition.id == "fallback",
        "missing catalog mixer event carries fallback definition");

    null_audio_engine null_engine;
    const sound_playback_result unavailable = null_engine.play_sound(sound_playback_request{
        .sound = "answer.correct",
        .event_id = 400,
    });
    require(unavailable.status == sound_playback_status::backend_unavailable,
        "null backend reports unavailable playback");
    require(null_engine.events().size() == 1, "null backend records attempted play event");
    require(null_engine.mixer_events().size() == 1, "null backend records suppressed mixer event");
    require(null_engine.mixer_events().front().status == sound_playback_status::backend_unavailable,
        "null backend mixer event records unavailable status");

    null_engine.set_muted(true);
    require(null_engine.mixer_events().back().kind == sound_mixer_event_kind::global_mute_changed,
        "null backend records mute mixer event");
    const sound_playback_result muted = null_engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    require(muted.status == sound_playback_status::muted, "null backend mute status is preserved");
    require(null_engine.mixer_events().back().status == sound_playback_status::muted,
        "null backend muted play records muted status");
}

} // namespace

int main()
{
    test_catalog_mixer_records_queued_playback();
    test_bus_mute_and_gain_suppress_new_playback_without_clearing_active();
    test_stop_events_report_affected_playbacks();
    test_missing_definition_and_null_backend_emit_suppressed_mixer_events();
    return 0;
}
