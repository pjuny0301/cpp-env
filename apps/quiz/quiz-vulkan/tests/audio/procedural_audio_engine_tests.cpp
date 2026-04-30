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
    engine.set_bus_gain(sound_bus::music, 0.5f);
    engine.clear_mixer_events();

    const sound_playback_result result = engine.play_sound(sound_playback_request{
        .sound = "music.loop",
        .event_id = 7001,
        .mode = sound_playback_mode::looping,
    });

    require(result.queued(), "procedural engine queues registered catalog sound");
    require(engine.events().size() == 1, "procedural engine keeps catalog event log");
    require(engine.mixer_events().size() == 1, "procedural engine emits one mixer play event");

    const sound_mixer_event& mixer_event = engine.mixer_events().front();
    require(mixer_event.id == 7001, "mixer play event preserves routed event id");
    require(mixer_event.kind == sound_mixer_event_kind::playback_queued, "mixer event records queued kind");
    require(mixer_event.sound == "music.loop", "mixer event records sound id");
    require(mixer_event.playback_id == result.playback_id, "mixer event records playback id");
    require(mixer_event.event.id == 7001, "legacy mixer event preserves routed event id");
    require(mixer_event.event.kind == sound_event_kind::play, "legacy mixer event records play kind");
    require(mixer_event.event.sound == "music.loop", "legacy mixer event records sound id");
    require(mixer_event.event.playback_id == result.playback_id, "legacy mixer event records playback id");
    require(mixer_event.status == sound_playback_status::queued, "mixer event records queued status");
    require(mixer_event.definition.id == "music.loop", "mixer event carries resolved definition");
    require(mixer_event.bus == sound_bus::music, "mixer event carries sound bus");
    require(mixer_event.gain == 0.5f, "mixer event carries effective bus gain");
    require(mixer_event.mode == sound_playback_mode::looping, "mixer event carries playback mode");
}

void test_missing_and_muted_requests_emit_suppressed_mixer_events()
{
    using namespace quiz_vulkan::audio;

    procedural_audio_engine engine(make_catalog());
    const sound_playback_result missing = engine.play_sound(sound_playback_request{
        .sound = "missing",
        .event_id = 7101,
    });
    require(missing.status == sound_playback_status::missing_definition, "missing sound reports missing definition");
    require(engine.events().empty(), "missing sound does not emit catalog event");
    require(engine.mixer_events().size() == 1, "missing sound emits suppressed mixer event");
    require(engine.mixer_events().front().kind == sound_mixer_event_kind::playback_suppressed,
        "missing mixer event records suppressed kind");
    require(engine.mixer_events().front().status == sound_playback_status::missing_definition,
        "missing mixer event records missing status");

    engine.set_muted(true);
    require(engine.mixer_events().size() == 2, "mute change reaches procedural mixer");
    require(engine.mixer_events().back().kind == sound_mixer_event_kind::global_mute_changed,
        "mute event records kind");
    require(engine.mixer_events().back().muted, "mute event records muted state");
    require(engine.mixer_events().back().event.kind == sound_event_kind::mute_changed,
        "legacy mute event records kind");
    require(engine.mixer_events().back().event.muted, "legacy mute event records muted state");

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
    require(engine.mixer_events().size() == 1, "muted play request emits suppressed mixer event");
    require(engine.mixer_events().front().kind == sound_mixer_event_kind::playback_suppressed,
        "muted mixer event records suppressed kind");
    require(engine.mixer_events().front().status == sound_playback_status::muted,
        "muted mixer event records muted status");
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
    require(engine.mixer_events().front().kind == sound_mixer_event_kind::playback_stopped,
        "single stop mixer event records kind");
    require(engine.mixer_events().front().playback_id == click.playback_id,
        "single stop mixer event records removed playback");
    require(engine.mixer_events().front().event.kind == sound_event_kind::stop,
        "legacy stop mixer event records kind");
    require(engine.mixer_events().front().event.playback_id == click.playback_id,
        "legacy stop mixer event records removed playback");
    require(engine.mixer_events().front().affected_playback_count == 1,
        "single stop mixer event records affected count");
    require(engine.mixer_events().front().definition.id == "ui.click", "single stop carries stopped definition");

    engine.clear_mixer_events();
    engine.stop_sound(sound_stop_request{.playback_id = 9999, .sound = "ui.click"});
    require(engine.active_playbacks().size() == 1, "unknown stop leaves active playback untouched");
    require(engine.mixer_events().size() == 1, "unknown stop emits no-op mixer event");
    require(engine.mixer_events().front().affected_playback_count == 0,
        "unknown stop reports zero affected playbacks");
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
    require(engine.mixer_events().size() == 1, "stop by sound emits aggregate mixer event");
    require(engine.mixer_events().front().kind == sound_mixer_event_kind::playback_stopped,
        "stop by sound mixer event records kind");
    require(engine.mixer_events().front().sound == "ui.click", "stop by sound records requested sound");
    require(engine.mixer_events().front().affected_playback_count == 2,
        "stop by sound records removed playback count");

    engine.clear_mixer_events();
    engine.stop_all_sounds();
    require(engine.active_playbacks().empty(), "stop all clears active procedural playback state");
    require(engine.mixer_events().size() == 1, "stop all emits one mixer control event");
    require(engine.mixer_events().front().kind == sound_mixer_event_kind::all_playbacks_stopped,
        "stop all mixer event records kind");
    require(engine.mixer_events().front().affected_playback_count == 1,
        "stop all records remaining playback count");
    (void)first;
    (void)second;
}

void test_bus_mixer_controls_flow_through_procedural_backend()
{
    using namespace quiz_vulkan::audio;

    procedural_audio_engine engine(make_catalog());
    const sound_playback_result click = engine.play_sound(sound_playback_request{.sound = "ui.click"});
    const sound_playback_result music = engine.play_sound(sound_playback_request{.sound = "music.loop"});
    require(click.queued() && music.queued(), "procedural engine queues effects and music sounds");

    engine.clear_mixer_events();
    engine.set_bus_muted(sound_bus::effects, true);
    require(engine.bus_muted(sound_bus::effects), "procedural engine stores effects mute");
    require(engine.active_playbacks().front().gain == 0.0f, "effects mute updates active playback gain");
    require(engine.active_playbacks().back().gain == 1.0f, "effects mute leaves music gain unchanged");
    require(engine.mixer_events().size() == 1, "bus mute emits one mixer event");
    require(engine.mixer_events().front().kind == sound_mixer_event_kind::bus_mute_changed,
        "bus mute event records kind");
    require(engine.mixer_events().front().affected_playback_count == 1,
        "bus mute event records affected effects playback count");

    const sound_playback_result muted = engine.play_sound(sound_playback_request{.sound = "ui.click"});
    require(muted.status == sound_playback_status::muted, "muted bus suppresses new procedural playback");
    require(engine.mixer_events().back().kind == sound_mixer_event_kind::playback_suppressed,
        "muted bus playback emits suppressed event");

    engine.set_bus_muted(sound_bus::effects, false);
    engine.set_bus_gain(sound_bus::effects, 0.25f);
    require(engine.bus_gain(sound_bus::effects) == 0.25f, "procedural engine stores effects gain");
    require(engine.mixer_state().effective_gain(sound_bus::effects) == 0.25f,
        "procedural engine exposes effective bus gain");
}

} // namespace

int main()
{
    test_catalog_definition_validity_and_fallback_resolution();
    test_procedural_engine_records_play_mixer_event();
    test_missing_and_muted_requests_emit_suppressed_mixer_events();
    test_stop_requests_emit_mixer_events_for_removed_playbacks_only();
    test_stop_all_instances_and_stop_all_emit_mixer_events();
    test_bus_mixer_controls_flow_through_procedural_backend();
    return 0;
}
