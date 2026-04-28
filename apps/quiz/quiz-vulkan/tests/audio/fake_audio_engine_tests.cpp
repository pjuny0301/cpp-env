#include "audio/audio_engine.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

class fake_audio_engine final : public quiz_vulkan::audio::audio_engine_interface {
public:
    explicit fake_audio_engine(quiz_vulkan::audio::sound_catalog catalog)
        : catalog_(std::move(catalog))
    {
    }

    quiz_vulkan::audio::sound_playback_result play_sound(
        const quiz_vulkan::audio::sound_playback_request& request) override
    {
        play_requests.push_back(request);

        const quiz_vulkan::audio::sound_definition* definition = catalog_.find(request.sound);
        if (definition == nullptr) {
            return quiz_vulkan::audio::sound_playback_result{
                .status = quiz_vulkan::audio::sound_playback_status::missing_definition,
                .definition = catalog_.resolve(request.sound),
                .diagnostic = "sound is not registered",
            };
        }

        if (muted_) {
            events.push_back(quiz_vulkan::audio::sound_event{
                .id = request.event_id,
                .kind = quiz_vulkan::audio::sound_event_kind::play,
                .sound = request.sound,
                .muted = true,
            });
            return quiz_vulkan::audio::sound_playback_result{
                .status = quiz_vulkan::audio::sound_playback_status::muted,
                .definition = *definition,
                .diagnostic = "fake engine is muted",
            };
        }

        const quiz_vulkan::audio::sound_playback_id playback_id = next_playback_id_++;
        active_playbacks.push_back(playback_id);
        events.push_back(quiz_vulkan::audio::sound_event{
            .id = request.event_id,
            .kind = quiz_vulkan::audio::sound_event_kind::play,
            .sound = request.sound,
            .playback_id = playback_id,
        });

        return quiz_vulkan::audio::sound_playback_result{
            .status = quiz_vulkan::audio::sound_playback_status::queued,
            .playback_id = playback_id,
            .definition = *definition,
        };
    }

    void stop_sound(const quiz_vulkan::audio::sound_stop_request& request) override
    {
        stop_requests.push_back(request);
        events.push_back(quiz_vulkan::audio::sound_event{
            .kind = quiz_vulkan::audio::sound_event_kind::stop,
            .sound = request.sound,
            .playback_id = request.playback_id,
        });

        if (request.playback_id == 0 && request.all_instances) {
            active_playbacks.clear();
            return;
        }

        active_playbacks.erase(
            std::remove(active_playbacks.begin(), active_playbacks.end(), request.playback_id),
            active_playbacks.end());
    }

    void stop_all_sounds() override
    {
        events.push_back(quiz_vulkan::audio::sound_event{.kind = quiz_vulkan::audio::sound_event_kind::stop_all});
        active_playbacks.clear();
    }

    void set_muted(bool muted) override
    {
        muted_ = muted;
        events.push_back(quiz_vulkan::audio::sound_event{
            .kind = quiz_vulkan::audio::sound_event_kind::mute_changed,
            .muted = muted_,
        });
    }

    bool muted() const override
    {
        return muted_;
    }

    std::vector<quiz_vulkan::audio::sound_playback_request> play_requests;
    std::vector<quiz_vulkan::audio::sound_stop_request> stop_requests;
    std::vector<quiz_vulkan::audio::sound_event> events;
    std::vector<quiz_vulkan::audio::sound_playback_id> active_playbacks;

private:
    quiz_vulkan::audio::sound_catalog catalog_;
    quiz_vulkan::audio::sound_playback_id next_playback_id_ = 1;
    bool muted_ = false;
};

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

    fake_audio_engine engine(make_sound_catalog());
    const sound_playback_request request{
        .sound = "answer.correct",
        .event_id = 42,
        .mode = sound_playback_mode::one_shot,
    };

    const sound_playback_result result = engine.play_sound(request);
    require(result.queued(), "fake engine queues registered sound");
    require(result.playback_id == 1, "first fake playback id is stable");
    require(result.definition.id == "answer.correct", "play result carries resolved definition");
    require(engine.play_requests.size() == 1, "fake engine records play request");
    require(engine.play_requests.front().event_id == 42, "play request carries event id");
    require(engine.events.size() == 1, "fake engine emits play event");
    require(engine.events.front().kind == sound_event_kind::play, "play event records kind");
    require(engine.events.front().playback_id == result.playback_id, "play event carries playback id");
}

void test_missing_definition_uses_fallback_without_queueing()
{
    using namespace quiz_vulkan::audio;

    fake_audio_engine engine(make_sound_catalog());
    const sound_playback_result result = engine.play_sound(sound_playback_request{.sound = "missing"});

    require(result.status == sound_playback_status::missing_definition, "missing sound reports missing definition");
    require(result.playback_id == 0, "missing sound is not queued");
    require(result.definition.id == "fallback", "missing sound result carries fallback definition");
    require(engine.active_playbacks.empty(), "missing sound leaves active playback list empty");
}

void test_stop_and_mute_behavior()
{
    using namespace quiz_vulkan::audio;

    fake_audio_engine engine(make_sound_catalog());
    const sound_playback_result first = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    const sound_playback_result second = engine.play_sound(sound_playback_request{.sound = "round.timer"});
    require(first.queued() && second.queued(), "fake engine queues two sounds");
    require(engine.active_playbacks.size() == 2, "two fake playbacks are active");

    engine.stop_sound(sound_stop_request{.playback_id = first.playback_id, .sound = "answer.correct"});
    require(engine.active_playbacks.size() == 1, "stop request removes selected playback");
    require(engine.active_playbacks.front() == second.playback_id, "stop request preserves other playback");
    require(engine.stop_requests.size() == 1, "fake engine records stop request");
    require(engine.events.back().kind == sound_event_kind::stop, "stop request emits stop event");

    engine.set_muted(true);
    require(engine.muted(), "fake engine mute flag is set");
    const sound_playback_result muted_result = engine.play_sound(sound_playback_request{.sound = "answer.correct"});
    require(muted_result.status == sound_playback_status::muted, "muted engine suppresses playback");
    require(muted_result.playback_id == 0, "muted sound is not queued");
    require(engine.active_playbacks.size() == 1, "muted request does not add active playback");
    require(engine.events.back().muted, "muted play event records muted state");

    engine.stop_all_sounds();
    require(engine.active_playbacks.empty(), "stop all clears active playbacks");
    require(engine.events.back().kind == sound_event_kind::stop_all, "stop all emits stop_all event");
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
    test_null_audio_engine_is_backend_free_and_mute_aware();
    return 0;
}
