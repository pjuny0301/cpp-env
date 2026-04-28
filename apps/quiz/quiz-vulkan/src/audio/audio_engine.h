#pragma once

#include "audio/sound_definition.h"
#include "audio/sound_event.h"

#include <string>

namespace quiz_vulkan::audio {

struct sound_playback_request {
    sound_id sound;
    sound_event_id event_id = 0;
    sound_playback_mode mode = sound_playback_mode::one_shot;
};

struct sound_stop_request {
    sound_playback_id playback_id = 0;
    sound_id sound;
    bool all_instances = false;
};

enum class sound_playback_status {
    queued,
    missing_definition,
    muted,
    backend_unavailable,
};

struct sound_playback_result {
    sound_playback_status status = sound_playback_status::backend_unavailable;
    sound_playback_id playback_id = 0;
    sound_definition definition;
    std::string diagnostic;

    bool queued() const
    {
        return status == sound_playback_status::queued && playback_id != 0;
    }
};

class audio_engine_interface {
public:
    virtual ~audio_engine_interface() = default;

    virtual sound_playback_result play_sound(const sound_playback_request& request) = 0;
    virtual void stop_sound(const sound_stop_request& request) = 0;
    virtual void stop_all_sounds() = 0;
    virtual void set_muted(bool muted) = 0;
    virtual bool muted() const = 0;
};

class null_audio_engine final : public audio_engine_interface {
public:
    sound_playback_result play_sound(const sound_playback_request& request) override
    {
        return sound_playback_result{
            .status = muted_ ? sound_playback_status::muted : sound_playback_status::backend_unavailable,
            .definition = sound_definition{.id = request.sound},
            .diagnostic = muted_ ? "null audio engine is muted" : "no audio backend is attached",
        };
    }

    void stop_sound(const sound_stop_request&) override
    {
    }

    void stop_all_sounds() override
    {
    }

    void set_muted(bool muted) override
    {
        muted_ = muted;
    }

    bool muted() const override
    {
        return muted_;
    }

private:
    bool muted_ = false;
};

} // namespace quiz_vulkan::audio
