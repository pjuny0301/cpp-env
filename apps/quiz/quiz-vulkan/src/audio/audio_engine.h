#pragma once

#include "audio/sound_definition.h"
#include "audio/sound_event.h"

#include <algorithm>
#include <cstddef>
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

struct sound_bus_mix {
    float gain = 1.0f;
    bool muted = false;
};

struct sound_mixer_state {
    bool muted = false;
    sound_bus_mix effects;
    sound_bus_mix music;
    sound_bus_mix voice;

    sound_bus_mix& bus_mix(sound_bus bus)
    {
        switch (bus) {
        case sound_bus::effects:
            return effects;
        case sound_bus::music:
            return music;
        case sound_bus::voice:
            return voice;
        }

        return effects;
    }

    const sound_bus_mix& bus_mix(sound_bus bus) const
    {
        switch (bus) {
        case sound_bus::effects:
            return effects;
        case sound_bus::music:
            return music;
        case sound_bus::voice:
            return voice;
        }

        return effects;
    }

    float effective_gain(sound_bus bus) const
    {
        const sound_bus_mix& mix = bus_mix(bus);
        return muted || mix.muted ? 0.0f : mix.gain;
    }

    bool audible(sound_bus bus) const
    {
        return effective_gain(bus) > 0.0f;
    }
};

enum class sound_mixer_event_kind {
    playback_queued,
    playback_suppressed,
    playback_stopped,
    all_playbacks_stopped,
    global_mute_changed,
    bus_mute_changed,
    bus_gain_changed,
};

struct sound_mixer_event {
    sound_event event;
    sound_event_id id = 0;
    sound_mixer_event_kind kind = sound_mixer_event_kind::playback_queued;
    sound_id sound;
    sound_playback_id playback_id = 0;
    sound_playback_status status = sound_playback_status::backend_unavailable;
    sound_definition definition;
    sound_bus bus = sound_bus::effects;
    sound_playback_mode mode = sound_playback_mode::one_shot;
    float gain = 1.0f;
    bool muted = false;
    std::size_t affected_playback_count = 0;
    std::string diagnostic;
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
    sound_bus bus = sound_bus::effects;
    float gain = 1.0f;
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
            const sound_definition& fallback = catalog_.resolve(request.sound);
            const std::string diagnostic = "sound is not registered";
            mixer_events_.push_back(sound_mixer_event{
                .id = request.event_id,
                .kind = sound_mixer_event_kind::playback_suppressed,
                .sound = request.sound,
                .playback_id = 0,
                .status = sound_playback_status::missing_definition,
                .definition = fallback,
                .bus = fallback.bus,
                .mode = request.mode,
                .gain = 0.0f,
                .muted = false,
                .affected_playback_count = 0,
                .diagnostic = diagnostic,
            });
            return sound_playback_result{
                .status = sound_playback_status::missing_definition,
                .definition = fallback,
                .diagnostic = diagnostic,
            };
        }

        const float gain = mixer_.effective_gain(definition->bus);
        if (!mixer_.audible(definition->bus)) {
            const sound_bus_mix& mix = mixer_.bus_mix(definition->bus);
            const std::string diagnostic = muted_
                ? "audio engine is muted"
                : (mix.muted ? "sound bus is muted" : "sound bus gain is zero");
            events_.push_back(sound_event{
                .id = request.event_id,
                .kind = sound_event_kind::play,
                .sound = request.sound,
                .playback_id = 0,
                .muted = true,
            });
            mixer_events_.push_back(sound_mixer_event{
                .id = request.event_id,
                .kind = sound_mixer_event_kind::playback_suppressed,
                .sound = request.sound,
                .playback_id = 0,
                .status = sound_playback_status::muted,
                .definition = *definition,
                .bus = definition->bus,
                .mode = request.mode,
                .gain = gain,
                .muted = true,
                .affected_playback_count = 0,
                .diagnostic = diagnostic,
            });
            return sound_playback_result{
                .status = sound_playback_status::muted,
                .definition = *definition,
                .diagnostic = diagnostic,
            };
        }

        const sound_playback_id playback_id = next_playback_id_++;
        active_playbacks_.push_back(active_sound_playback{
            .playback_id = playback_id,
            .sound = request.sound,
            .mode = request.mode,
            .bus = definition->bus,
            .gain = gain,
        });
        events_.push_back(sound_event{
            .id = request.event_id,
            .kind = sound_event_kind::play,
            .sound = request.sound,
            .playback_id = playback_id,
            .muted = false,
        });
        mixer_events_.push_back(sound_mixer_event{
            .id = request.event_id,
            .kind = sound_mixer_event_kind::playback_queued,
            .sound = request.sound,
            .playback_id = playback_id,
            .status = sound_playback_status::queued,
            .definition = *definition,
            .bus = definition->bus,
            .mode = request.mode,
            .gain = gain,
            .muted = false,
            .affected_playback_count = 1,
            .diagnostic = {},
        });

        return sound_playback_result{
            .status = sound_playback_status::queued,
            .playback_id = playback_id,
            .definition = *definition,
            .diagnostic = {},
        };
    }

    void stop_sound(const sound_stop_request& request) override
    {
        const std::size_t affected_count = count_stop_matches(request);
        events_.push_back(sound_event{
            .id = 0,
            .kind = sound_event_kind::stop,
            .sound = request.sound,
            .playback_id = request.playback_id,
            .muted = false,
        });
        mixer_events_.push_back(sound_mixer_event{
            .id = 0,
            .kind = sound_mixer_event_kind::playback_stopped,
            .sound = request.sound,
            .playback_id = request.playback_id,
            .status = sound_playback_status::queued,
            .definition = catalog_.resolve(request.sound),
            .bus = catalog_.resolve(request.sound).bus,
            .mode = sound_playback_mode::one_shot,
            .gain = 0.0f,
            .muted = muted_,
            .affected_playback_count = affected_count,
            .diagnostic = {},
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
        const std::size_t affected_count = active_playbacks_.size();
        events_.push_back(sound_event{
            .id = 0,
            .kind = sound_event_kind::stop_all,
            .sound = {},
            .playback_id = 0,
            .muted = false,
        });
        mixer_events_.push_back(sound_mixer_event{
            .id = 0,
            .kind = sound_mixer_event_kind::all_playbacks_stopped,
            .sound = {},
            .playback_id = 0,
            .status = sound_playback_status::queued,
            .definition = {},
            .bus = sound_bus::effects,
            .mode = sound_playback_mode::one_shot,
            .gain = 0.0f,
            .muted = muted_,
            .affected_playback_count = affected_count,
            .diagnostic = {},
        });
        active_playbacks_.clear();
    }

    void set_muted(bool muted) override
    {
        muted_ = muted;
        mixer_.muted = muted_;
        refresh_active_gains();
        events_.push_back(sound_event{
            .id = 0,
            .kind = sound_event_kind::mute_changed,
            .sound = {},
            .playback_id = 0,
            .muted = muted_,
        });
        mixer_events_.push_back(sound_mixer_event{
            .id = 0,
            .kind = sound_mixer_event_kind::global_mute_changed,
            .sound = {},
            .playback_id = 0,
            .status = muted_ ? sound_playback_status::muted : sound_playback_status::queued,
            .definition = {},
            .bus = sound_bus::effects,
            .mode = sound_playback_mode::one_shot,
            .gain = mixer_.effective_gain(sound_bus::effects),
            .muted = muted_,
            .affected_playback_count = active_playbacks_.size(),
            .diagnostic = {},
        });
    }

    bool muted() const override
    {
        return muted_;
    }

    void set_bus_gain(sound_bus bus, float gain)
    {
        sound_bus_mix& mix = mixer_.bus_mix(bus);
        mix.gain = std::max(0.0f, gain);
        refresh_active_bus_gain(bus);
        mixer_events_.push_back(sound_mixer_event{
            .id = 0,
            .kind = sound_mixer_event_kind::bus_gain_changed,
            .sound = {},
            .playback_id = 0,
            .status = mixer_.audible(bus) ? sound_playback_status::queued : sound_playback_status::muted,
            .definition = {},
            .bus = bus,
            .mode = sound_playback_mode::one_shot,
            .gain = mixer_.effective_gain(bus),
            .muted = !mixer_.audible(bus),
            .affected_playback_count = count_active_bus(bus),
            .diagnostic = {},
        });
    }

    float bus_gain(sound_bus bus) const
    {
        return mixer_.bus_mix(bus).gain;
    }

    void set_bus_muted(sound_bus bus, bool muted)
    {
        mixer_.bus_mix(bus).muted = muted;
        refresh_active_bus_gain(bus);
        mixer_events_.push_back(sound_mixer_event{
            .id = 0,
            .kind = sound_mixer_event_kind::bus_mute_changed,
            .sound = {},
            .playback_id = 0,
            .status = mixer_.audible(bus) ? sound_playback_status::queued : sound_playback_status::muted,
            .definition = {},
            .bus = bus,
            .mode = sound_playback_mode::one_shot,
            .gain = mixer_.effective_gain(bus),
            .muted = muted,
            .affected_playback_count = count_active_bus(bus),
            .diagnostic = {},
        });
    }

    bool bus_muted(sound_bus bus) const
    {
        return mixer_.bus_mix(bus).muted;
    }

    sound_mixer_state mixer_state() const
    {
        return mixer_;
    }

    const sound_catalog& catalog() const
    {
        return catalog_;
    }

    const std::vector<sound_event>& events() const
    {
        return events_;
    }

    const std::vector<sound_mixer_event>& mixer_events() const
    {
        return mixer_events_;
    }

    const std::vector<active_sound_playback>& active_playbacks() const
    {
        return active_playbacks_;
    }

    void clear_events()
    {
        events_.clear();
    }

    void clear_mixer_events()
    {
        mixer_events_.clear();
    }

private:
    void refresh_active_gains()
    {
        for (active_sound_playback& playback : active_playbacks_) {
            playback.gain = mixer_.effective_gain(playback.bus);
        }
    }

    void refresh_active_bus_gain(sound_bus bus)
    {
        for (active_sound_playback& playback : active_playbacks_) {
            if (playback.bus == bus) {
                playback.gain = mixer_.effective_gain(bus);
            }
        }
    }

    std::size_t count_active_bus(sound_bus bus) const
    {
        return static_cast<std::size_t>(std::count_if(
            active_playbacks_.begin(),
            active_playbacks_.end(),
            [bus](const active_sound_playback& playback) {
                return playback.bus == bus;
            }));
    }

    std::size_t count_stop_matches(const sound_stop_request& request) const
    {
        if (request.all_instances) {
            if (request.sound.empty()) {
                return active_playbacks_.size();
            }

            return static_cast<std::size_t>(std::count_if(
                active_playbacks_.begin(),
                active_playbacks_.end(),
                [sound = request.sound](const active_sound_playback& playback) {
                    return playback.sound == sound;
                }));
        }

        if (request.playback_id == 0) {
            return 0;
        }

        return static_cast<std::size_t>(std::count_if(
            active_playbacks_.begin(),
            active_playbacks_.end(),
            [playback_id = request.playback_id](const active_sound_playback& playback) {
                return playback.playback_id == playback_id;
            }));
    }

    template <typename Predicate>
    void erase_active_playbacks(Predicate predicate)
    {
        active_playbacks_.erase(
            std::remove_if(active_playbacks_.begin(), active_playbacks_.end(), predicate),
            active_playbacks_.end());
    }

    sound_catalog catalog_;
    sound_mixer_state mixer_;
    std::vector<sound_event> events_;
    std::vector<sound_mixer_event> mixer_events_;
    std::vector<active_sound_playback> active_playbacks_;
    sound_playback_id next_playback_id_ = 1;
    bool muted_ = false;
};

class procedural_audio_engine final : public audio_engine_interface {
public:
    explicit procedural_audio_engine(sound_catalog catalog)
        : catalog_engine_(std::move(catalog))
    {
    }

    sound_playback_result play_sound(const sound_playback_request& request) override
    {
        const sound_playback_result result = catalog_engine_.play_sound(request);
        if (result.status == sound_playback_status::queued) {
            mixer_events_.push_back(sound_mixer_event{
                .event = sound_event{
                    .id = request.event_id,
                    .kind = sound_event_kind::play,
                    .sound = request.sound,
                    .playback_id = result.playback_id,
                },
                .definition = result.definition,
                .mode = request.mode,
            });
        }
        return result;
    }

    void stop_sound(const sound_stop_request& request) override
    {
        const std::vector<active_sound_playback> removed_playbacks = matching_active_playbacks(request);
        catalog_engine_.stop_sound(request);

        for (const active_sound_playback& playback : removed_playbacks) {
            mixer_events_.push_back(sound_mixer_event{
                .event = sound_event{
                    .kind = sound_event_kind::stop,
                    .sound = playback.sound,
                    .playback_id = playback.playback_id,
                },
                .definition = catalog_engine_.catalog().resolve(playback.sound),
                .mode = playback.mode,
            });
        }
    }

    void stop_all_sounds() override
    {
        catalog_engine_.stop_all_sounds();
        mixer_events_.push_back(sound_mixer_event{
            .event = sound_event{.kind = sound_event_kind::stop_all},
        });
    }

    void set_muted(bool muted) override
    {
        catalog_engine_.set_muted(muted);
        mixer_events_.push_back(sound_mixer_event{
            .event = sound_event{
                .kind = sound_event_kind::mute_changed,
                .muted = muted,
            },
        });
    }

    bool muted() const override
    {
        return catalog_engine_.muted();
    }

    const sound_catalog& catalog() const
    {
        return catalog_engine_.catalog();
    }

    const std::vector<sound_event>& events() const
    {
        return catalog_engine_.events();
    }

    const std::vector<active_sound_playback>& active_playbacks() const
    {
        return catalog_engine_.active_playbacks();
    }

    const std::vector<sound_mixer_event>& mixer_events() const
    {
        return mixer_events_;
    }

    void clear_events()
    {
        catalog_engine_.clear_events();
    }

    void clear_mixer_events()
    {
        mixer_events_.clear();
    }

private:
    std::vector<active_sound_playback> matching_active_playbacks(const sound_stop_request& request) const
    {
        std::vector<active_sound_playback> matches;
        for (const active_sound_playback& playback : catalog_engine_.active_playbacks()) {
            if (request.all_instances) {
                if (request.sound.empty() || playback.sound == request.sound) {
                    matches.push_back(playback);
                }
                continue;
            }

            if (request.playback_id != 0 && playback.playback_id == request.playback_id) {
                matches.push_back(playback);
            }
        }
        return matches;
    }

    catalog_audio_engine catalog_engine_;
    std::vector<sound_mixer_event> mixer_events_;
};

class null_audio_engine final : public audio_engine_interface {
public:
    sound_playback_result play_sound(const sound_playback_request& request) override
    {
        const sound_playback_status status = muted_ ? sound_playback_status::muted : sound_playback_status::backend_unavailable;
        events_.push_back(sound_event{
            .id = request.event_id,
            .kind = sound_event_kind::play,
            .sound = request.sound,
            .playback_id = 0,
            .muted = muted_,
        });
        mixer_events_.push_back(sound_mixer_event{
            .id = request.event_id,
            .kind = sound_mixer_event_kind::playback_suppressed,
            .sound = request.sound,
            .playback_id = 0,
            .status = status,
            .definition = sound_definition{
                .id = request.sound,
                .source_uri = {},
                .bus = sound_bus::effects,
                .default_mode = request.mode,
            },
            .bus = sound_bus::effects,
            .mode = request.mode,
            .gain = 0.0f,
            .muted = muted_,
            .affected_playback_count = 0,
            .diagnostic = muted_ ? "null audio engine is muted" : "no audio backend is attached",
        });
        return sound_playback_result{
            .status = status,
            .definition = sound_definition{
                .id = request.sound,
                .source_uri = {},
                .bus = sound_bus::effects,
                .default_mode = request.mode,
            },
            .diagnostic = muted_ ? "null audio engine is muted" : "no audio backend is attached",
        };
    }

    void stop_sound(const sound_stop_request& request) override
    {
        events_.push_back(sound_event{
            .id = 0,
            .kind = sound_event_kind::stop,
            .sound = request.sound,
            .playback_id = request.playback_id,
            .muted = muted_,
        });
        mixer_events_.push_back(sound_mixer_event{
            .id = 0,
            .kind = sound_mixer_event_kind::playback_stopped,
            .sound = request.sound,
            .playback_id = request.playback_id,
            .status = sound_playback_status::backend_unavailable,
            .definition = sound_definition{
                .id = request.sound,
                .source_uri = {},
                .bus = sound_bus::effects,
                .default_mode = sound_playback_mode::one_shot,
            },
            .bus = sound_bus::effects,
            .mode = sound_playback_mode::one_shot,
            .gain = 0.0f,
            .muted = muted_,
            .affected_playback_count = 0,
            .diagnostic = "no audio backend is attached",
        });
    }

    void stop_all_sounds() override
    {
        events_.push_back(sound_event{
            .id = 0,
            .kind = sound_event_kind::stop_all,
            .sound = {},
            .playback_id = 0,
            .muted = muted_,
        });
        mixer_events_.push_back(sound_mixer_event{
            .id = 0,
            .kind = sound_mixer_event_kind::all_playbacks_stopped,
            .sound = {},
            .playback_id = 0,
            .status = sound_playback_status::backend_unavailable,
            .definition = {},
            .bus = sound_bus::effects,
            .mode = sound_playback_mode::one_shot,
            .gain = 0.0f,
            .muted = muted_,
            .affected_playback_count = 0,
            .diagnostic = "no audio backend is attached",
        });
    }

    void set_muted(bool muted) override
    {
        muted_ = muted;
        events_.push_back(sound_event{
            .id = 0,
            .kind = sound_event_kind::mute_changed,
            .sound = {},
            .playback_id = 0,
            .muted = muted_,
        });
        mixer_events_.push_back(sound_mixer_event{
            .id = 0,
            .kind = sound_mixer_event_kind::global_mute_changed,
            .sound = {},
            .playback_id = 0,
            .status = muted_ ? sound_playback_status::muted : sound_playback_status::backend_unavailable,
            .definition = {},
            .bus = sound_bus::effects,
            .mode = sound_playback_mode::one_shot,
            .gain = 0.0f,
            .muted = muted_,
            .affected_playback_count = 0,
            .diagnostic = {},
        });
    }

    bool muted() const override
    {
        return muted_;
    }

    const std::vector<sound_event>& events() const
    {
        return events_;
    }

    const std::vector<sound_mixer_event>& mixer_events() const
    {
        return mixer_events_;
    }

    void clear_events()
    {
        events_.clear();
    }

    void clear_mixer_events()
    {
        mixer_events_.clear();
    }

private:
    std::vector<sound_event> events_;
    std::vector<sound_mixer_event> mixer_events_;
    bool muted_ = false;
};

} // namespace quiz_vulkan::audio
