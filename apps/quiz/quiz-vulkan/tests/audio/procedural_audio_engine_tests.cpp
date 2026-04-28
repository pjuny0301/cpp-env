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

quiz_vulkan::audio::sound_definition make_definition(
    const quiz_vulkan::audio::sound_id& id,
    quiz_vulkan::audio::sound_bus bus = quiz_vulkan::audio::sound_bus::effects,
    quiz_vulkan::audio::sound_playback_mode mode = quiz_vulkan::audio::sound_playback_mode::one_shot)
{
    return quiz_vulkan::audio::sound_definition{
        .id = id,
        .source_uri = "procedural://" + id,
        .bus = bus,
        .default_mode = mode,
    };
}

quiz_vulkan::audio::sound_catalog make_catalog()
{
    quiz_vulkan::audio::sound_catalog catalog;
    catalog.fallback_definition = make_definition("fallback.silence");
    catalog.definitions.push_back(make_definition("ui.click"));
    catalog.definitions.push_back(make_definition("ui.alert", quiz_vulkan::audio::sound_bus::voice));
    catalog.definitions.push_back(make_definition(
        "music.loop",
        quiz_vulkan::audio::sound_bus::music,
        quiz_vulkan::audio::sound_playback_mode::looping));
    return catalog;
}

void test_catalog_definition_validity_and_fallback_resolution()
{
    using namespace quiz_vulkan::audio;

    const sound_definition empty_definition;
    require(!empty_definition.valid(), "empty sound definition is invalid");

    const sound_definition missing_source{.id = "ui.missing_source"};
    require(!missing_source.valid(), "definition without source uri is invalid");

    const sound_catalog catalog = make_catalog();
    const sound_definition* click = catalog.find("ui.click");
    require(click != nullptr, "catalog finds registered procedural sound");
    require(click->valid(), "registered procedural sound is valid");
    require(click->source_uri == "procedural://ui.click", "catalog preserves procedural source uri");
    require(catalog.find("missing") == nullptr, "catalog find reports absent sound");
    require(catalog.resolve("missing").id == "fallback.silence", "catalog resolve uses fallback for absent sound");
}

void test_procedural_engine_records_play_mixer_event()
{
    using namespace quiz_vulkan::audio;

    procedural_audio_engine engine(make_catalog());
    const sound_playback_result result = engine.play_sound(sound_playback_request{
        .sound = "music.loop",
        .event_id = 7001,
        .mode = sound_playback_mode::looping,
    });

    require(result.queued(), "procedural engine queues registered catalog sound");
    require(engine.events().size() == 1, "procedural engine keeps catalog event log");
    require(engine.mixer_events().size() == 1, "procedural engine emits one mixer play event");

    const sound_mixer_event& mixer_event = engine.mixer_events().front();
    require(mixer_event.event.id == 7001, "mixer play event preserves routed event id");
    require(mixer_event.event.kind == sound_event_kind::play, "mixer event records play kind");
    require(mixer_event.event.sound == "music.loop", "mixer event records sound id");
    require(mixer_event.event.playback_id == result.playback_id, "mixer event records playback id");
    require(mixer_event.definition.id == "music.loop", "mixer event carries resolved definition");
    require(mixer_event.definition.bus == sound_bus::music, "mixer event carries sound bus");
    require(mixer_event.mode == sound_playback_mode::looping, "mixer event carries playback mode");
}

void test_missing_and_muted_requests_do_not_emit_mixer_playback()
{
    using namespace quiz_vulkan::audio;

    procedural_audio_engine engine(make_catalog());
    const sound_playback_result missing = engine.play_sound(sound_playback_request{
        .sound = "missing",
        .event_id = 7101,
    });
    require(missing.status == sound_playback_status::missing_definition, "missing sound reports missing definition");
    require(engine.events().empty(), "missing sound does not emit catalog event");
    require(engine.mixer_events().empty(), "missing sound does not reach procedural mixer");

    engine.set_muted(true);
    require(engine.mixer_events().size() == 1, "mute change reaches procedural mixer");
    require(engine.mixer_events().back().event.kind == sound_event_kind::mute_changed, "mute event records kind");
    require(engine.mixer_events().back().event.muted, "mute event records muted state");

    engine.clear_events();
    engine.clear_mixer_events();
    const sound_playback_result muted = engine.play_sound(sound_playback_request{
        .sound = "ui.click",
        .event_id = 7102,
    });
    require(muted.status == sound_playback_status::muted, "muted sound reports muted status");
    require(engine.active_playbacks().empty(), "muted sound does not become active");
    require(engine.events().size() == 1, "muted sound still records catalog event");
    require(engine.events().front().muted, "muted catalog event records muted state");
    require(engine.mixer_events().empty(), "muted play request does not emit mixer playback");
}

void test_stop_requests_emit_mixer_events_for_removed_playbacks_only()
{
    using namespace quiz_vulkan::audio;

    procedural_audio_engine engine(make_catalog());
    const sound_playback_result click = engine.play_sound(sound_playback_request{.sound = "ui.click"});
    const sound_playback_result alert = engine.play_sound(sound_playback_request{.sound = "ui.alert"});
    require(click.queued() && alert.queued(), "procedural engine queues two sounds");

    engine.clear_mixer_events();
    engine.stop_sound(sound_stop_request{.playback_id = click.playback_id, .sound = "ui.click"});
    require(engine.active_playbacks().size() == 1, "single stop removes selected playback");
    require(engine.active_playbacks().front().playback_id == alert.playback_id, "single stop preserves other playback");
    require(engine.mixer_events().size() == 1, "single stop emits one mixer event");
    require(engine.mixer_events().front().event.kind == sound_event_kind::stop, "single stop mixer event records kind");
    require(
        engine.mixer_events().front().event.playback_id == click.playback_id,
        "single stop mixer event records removed playback");
    require(engine.mixer_events().front().definition.id == "ui.click", "single stop carries stopped definition");

    engine.clear_mixer_events();
    engine.stop_sound(sound_stop_request{.playback_id = 9999, .sound = "ui.click"});
    require(engine.active_playbacks().size() == 1, "unknown stop leaves active playback untouched");
    require(engine.mixer_events().empty(), "unknown stop does not emit mixer event");
}

void test_stop_all_instances_and_stop_all_emit_mixer_events()
{
    using namespace quiz_vulkan::audio;

    procedural_audio_engine engine(make_catalog());
    const sound_playback_result first = engine.play_sound(sound_playback_request{.sound = "ui.click"});
    const sound_playback_result second = engine.play_sound(sound_playback_request{.sound = "ui.click"});
    const sound_playback_result music = engine.play_sound(sound_playback_request{
        .sound = "music.loop",
        .mode = sound_playback_mode::looping,
    });
    require(first.queued() && second.queued() && music.queued(), "procedural engine queues repeated sounds");

    engine.clear_mixer_events();
    engine.stop_sound(sound_stop_request{.sound = "ui.click", .all_instances = true});
    require(engine.active_playbacks().size() == 1, "stop by sound leaves unmatched playback active");
    require(engine.active_playbacks().front().playback_id == music.playback_id, "stop by sound preserves music");
    require(engine.mixer_events().size() == 2, "stop by sound emits one mixer event per removed playback");
    require(engine.mixer_events()[0].event.playback_id == first.playback_id, "first stopped playback is recorded");
    require(engine.mixer_events()[1].event.playback_id == second.playback_id, "second stopped playback is recorded");

    engine.clear_mixer_events();
    engine.stop_all_sounds();
    require(engine.active_playbacks().empty(), "stop all clears active procedural playback state");
    require(engine.mixer_events().size() == 1, "stop all emits one mixer control event");
    require(engine.mixer_events().front().event.kind == sound_event_kind::stop_all, "stop all mixer event records kind");
}

} // namespace

int main()
{
    test_catalog_definition_validity_and_fallback_resolution();
    test_procedural_engine_records_play_mixer_event();
    test_missing_and_muted_requests_do_not_emit_mixer_playback();
    test_stop_requests_emit_mixer_events_for_removed_playbacks_only();
    test_stop_all_instances_and_stop_all_emit_mixer_events();
    return 0;
}
