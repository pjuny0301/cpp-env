#include "audio/audio_engine.h"

#include <concepts>
#include <string>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept AudioEngineInterface = requires(
    T& engine,
    const audio::sound_playback_request& play_request,
    const audio::sound_stop_request& stop_request,
    bool muted) {
    { engine.play_sound(play_request) } -> std::same_as<audio::sound_playback_result>;
    { engine.stop_sound(stop_request) } -> std::same_as<void>;
    { engine.stop_all_sounds() } -> std::same_as<void>;
    { engine.set_muted(muted) } -> std::same_as<void>;
    { engine.muted() } -> std::same_as<bool>;
};

static_assert(AudioEngineInterface<audio::audio_engine_interface>);
static_assert(AudioEngineInterface<audio::null_audio_engine>);

static_assert(requires(audio::sound_catalog catalog, const audio::sound_id& id) {
    { catalog.find(id) } -> std::same_as<const audio::sound_definition*>;
    { catalog.resolve(id) } -> std::same_as<const audio::sound_definition&>;
});

static_assert(requires(audio::sound_definition definition) {
    { definition.valid() } -> std::same_as<bool>;
    { definition.id } -> std::same_as<audio::sound_id&>;
    { definition.source_uri } -> std::same_as<std::string&>;
});

} // namespace
