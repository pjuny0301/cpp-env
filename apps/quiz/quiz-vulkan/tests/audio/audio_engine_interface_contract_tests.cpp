#include "audio/audio_engine.h"

#include <concepts>
#include <string>
#include <vector>

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
static_assert(AudioEngineInterface<audio::catalog_audio_engine>);
static_assert(AudioEngineInterface<audio::procedural_audio_engine>);
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

static_assert(requires(audio::sound_mixer_state mixer, audio::sound_bus bus) {
    { mixer.bus_mix(bus) } -> std::same_as<audio::sound_bus_mix&>;
    { mixer.effective_gain(bus) } -> std::same_as<float>;
    { mixer.audible(bus) } -> std::same_as<bool>;
});

static_assert(requires(
    audio::catalog_audio_engine catalog_engine,
    audio::null_audio_engine null_engine,
    audio::sound_bus bus,
    float gain,
    bool muted) {
    { catalog_engine.set_bus_gain(bus, gain) } -> std::same_as<void>;
    { catalog_engine.bus_gain(bus) } -> std::same_as<float>;
    { catalog_engine.set_bus_muted(bus, muted) } -> std::same_as<void>;
    { catalog_engine.bus_muted(bus) } -> std::same_as<bool>;
    { catalog_engine.mixer_state() } -> std::same_as<audio::sound_mixer_state>;
    { catalog_engine.mixer_events() } -> std::same_as<const std::vector<audio::sound_mixer_event>&>;
    { catalog_engine.clear_mixer_events() } -> std::same_as<void>;
    { null_engine.events() } -> std::same_as<const std::vector<audio::sound_event>&>;
    { null_engine.mixer_events() } -> std::same_as<const std::vector<audio::sound_mixer_event>&>;
    { null_engine.clear_mixer_events() } -> std::same_as<void>;
});

} // namespace
