#pragma once

#include <cstdint>
#include <string>

namespace quiz_vulkan::audio {

using sound_id = std::string;
using sound_event_id = std::uint64_t;
using sound_playback_id = std::uint64_t;

enum class sound_event_kind {
    play,
    stop,
    stop_all,
    mute_changed,
};

struct sound_event {
    sound_event_id id = 0;
    sound_event_kind kind = sound_event_kind::play;
    sound_id sound;
    sound_playback_id playback_id = 0;
    bool muted = false;
};

} // namespace quiz_vulkan::audio
