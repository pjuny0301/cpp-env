#pragma once

#include "audio/sound_event.h"

#include <string>
#include <vector>

namespace quiz_vulkan::audio {

enum class sound_bus {
    effects,
    music,
    voice,
};

enum class sound_playback_mode {
    one_shot,
    looping,
};

struct sound_definition {
    sound_id id;
    std::string source_uri;
    sound_bus bus = sound_bus::effects;
    sound_playback_mode default_mode = sound_playback_mode::one_shot;

    bool valid() const
    {
        return !id.empty() && !source_uri.empty();
    }
};

struct sound_catalog {
    sound_definition fallback_definition;
    std::vector<sound_definition> definitions;

    const sound_definition* find(const sound_id& id) const
    {
        for (const sound_definition& definition : definitions) {
            if (definition.id == id) {
                return &definition;
            }
        }
        return nullptr;
    }

    const sound_definition& resolve(const sound_id& id) const
    {
        const sound_definition* definition = find(id);
        return definition == nullptr ? fallback_definition : *definition;
    }
};

} // namespace quiz_vulkan::audio
