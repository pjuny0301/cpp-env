#pragma once

#include "audio/sound_definition.h"
#include "audio/sound_event.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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

struct active_sound_playback {
    sound_playback_id playback_id = 0;
    sound_id sound;
    sound_playback_mode mode = sound_playback_mode::one_shot;
};

class catalog_audio_engine final : public audio_engine_interface {
public:
    explicit catalog_audio_engine(sound_catalog catalog)
        : catalog_(std::move(catalog))
    {
    }

    sound_playback_result play_sound(const sound_playback_request& request) override
    {
        const sound_definition* definition = catalog_.find(request.sound);
        if (definition == nullptr || !definition->valid()) {
            return sound_playback_result{
                .status = sound_playback_status::missing_definition,
                .definition = catalog_.resolve(request.sound),
                .diagnostic = "sound is not registered",
            };
        }

        if (muted_) {
            events_.push_back(sound_event{
                .id = request.event_id,
                .kind = sound_event_kind::play,
                .sound = request.sound,
                .muted = true,
            });
            return sound_playback_result{
                .status = sound_playback_status::muted,
                .definition = *definition,
                .diagnostic = "audio engine is muted",
            };
        }

        const sound_playback_id playback_id = next_playback_id_++;
        active_playbacks_.push_back(active_sound_playback{
            .playback_id = playback_id,
            .sound = request.sound,
            .mode = request.mode,
        });
        events_.push_back(sound_event{
            .id = request.event_id,
            .kind = sound_event_kind::play,
            .sound = request.sound,
            .playback_id = playback_id,
        });

        return sound_playback_result{
            .status = sound_playback_status::queued,
            .playback_id = playback_id,
            .definition = *definition,
        };
    }

    void stop_sound(const sound_stop_request& request) override
    {
        events_.push_back(sound_event{
            .kind = sound_event_kind::stop,
            .sound = request.sound,
            .playback_id = request.playback_id,
        });

        if (request.all_instances) {
            if (request.sound.empty()) {
                active_playbacks_.clear();
                return;
            }

            erase_active_playbacks(
                [sound = request.sound](const active_sound_playback& playback) {
                    return playback.sound == sound;
                });
            return;
        }

        if (request.playback_id != 0) {
            erase_active_playbacks(
                [playback_id = request.playback_id](const active_sound_playback& playback) {
                    return playback.playback_id == playback_id;
                });
        }
    }

    void stop_all_sounds() override
    {
        events_.push_back(sound_event{.kind = sound_event_kind::stop_all});
        active_playbacks_.clear();
    }

    void set_muted(bool muted) override
    {
        muted_ = muted;
        events_.push_back(sound_event{
            .kind = sound_event_kind::mute_changed,
            .muted = muted_,
        });
    }

    bool muted() const override
    {
        return muted_;
    }

    const sound_catalog& catalog() const
    {
        return catalog_;
    }

    const std::vector<sound_event>& events() const
    {
        return events_;
    }

    const std::vector<active_sound_playback>& active_playbacks() const
    {
        return active_playbacks_;
    }

    void clear_events()
    {
        events_.clear();
    }

private:
    template <typename Predicate>
    void erase_active_playbacks(Predicate predicate)
    {
        active_playbacks_.erase(
            std::remove_if(active_playbacks_.begin(), active_playbacks_.end(), predicate),
            active_playbacks_.end());
    }

    sound_catalog catalog_;
    std::vector<sound_event> events_;
    std::vector<active_sound_playback> active_playbacks_;
    sound_playback_id next_playback_id_ = 1;
    bool muted_ = false;
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
