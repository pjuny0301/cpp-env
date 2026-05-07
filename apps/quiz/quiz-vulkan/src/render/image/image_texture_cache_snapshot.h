#pragma once

#include "render/image/image_texture_diagnostics.h"
#include "render/image/image_texture_placeholder_policy.h"
#include "render/image/image_types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace quiz_vulkan::render {

enum class fake_image_texture_residency {
    evictable,
    pinned,
};

enum class fake_image_texture_eviction_reason {
    capacity,
    release_unused,
    source_invalidation,
    texture_invalidation,
};

inline std::string fake_image_texture_eviction_reason_name(fake_image_texture_eviction_reason reason)
{
    switch (reason) {
    case fake_image_texture_eviction_reason::capacity:
        return "capacity";
    case fake_image_texture_eviction_reason::release_unused:
        return "release_unused";
    case fake_image_texture_eviction_reason::source_invalidation:
        return "source_invalidation";
    case fake_image_texture_eviction_reason::texture_invalidation:
        return "texture_invalidation";
    }

    return "unknown";
}

struct fake_image_texture_cache_entry_snapshot {
    render_image_texture_key key;
    render_image_texture_key_diagnostic key_diagnostic;
    render_image_sampler_policy_diagnostic sampler_policy;
    render_image_texture_handle texture;
    render_image_upload_readiness_snapshot upload_readiness;
    std::uint64_t upload_generation_id = 0;
    std::size_t first_used_sequence = 0;
    std::size_t last_used_sequence = 0;
    std::size_t access_count = 0;
    std::size_t resident_lifetime_sequence_count = 0;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    fake_image_texture_residency residency = fake_image_texture_residency::evictable;
    bool placeholder_texture = false;
    fake_image_texture_placeholder_reason placeholder_reason = fake_image_texture_placeholder_reason::none;
    render_image_texture_key requested_key;
    std::string placeholder_diagnostic;
};

struct fake_image_texture_eviction_snapshot {
    std::size_t sequence = 0;
    fake_image_texture_eviction_reason reason = fake_image_texture_eviction_reason::capacity;
    render_image_texture_key key;
    render_image_texture_handle texture;
    std::uint64_t upload_generation_id = 0;
    std::size_t first_used_sequence = 0;
    std::size_t last_used_sequence = 0;
    std::size_t access_count = 0;
    std::size_t resident_lifetime_sequence_count = 0;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    fake_image_texture_residency residency = fake_image_texture_residency::evictable;
    std::string diagnostic;
};

enum class fake_image_texture_residency_transition_reason {
    cache_inserted,
    policy_configured,
    pinned,
    unpinned,
};

inline std::string fake_image_texture_residency_transition_reason_name(
    fake_image_texture_residency_transition_reason reason)
{
    switch (reason) {
    case fake_image_texture_residency_transition_reason::cache_inserted:
        return "cache_inserted";
    case fake_image_texture_residency_transition_reason::policy_configured:
        return "policy_configured";
    case fake_image_texture_residency_transition_reason::pinned:
        return "pinned";
    case fake_image_texture_residency_transition_reason::unpinned:
        return "unpinned";
    }

    return "unknown";
}

struct fake_image_texture_residency_transition_snapshot {
    std::size_t sequence = 0;
    fake_image_texture_residency_transition_reason reason =
        fake_image_texture_residency_transition_reason::policy_configured;
    render_image_texture_key key;
    render_image_texture_handle texture;
    std::uint64_t upload_generation_id = 0;
    fake_image_texture_residency previous_residency = fake_image_texture_residency::evictable;
    fake_image_texture_residency new_residency = fake_image_texture_residency::evictable;
    bool resident = false;
    bool changed = false;
    std::size_t last_used_sequence = 0;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::string diagnostic;
};

struct fake_image_texture_cache_snapshot {
    std::size_t texture_count = 0;
    std::size_t max_cached_pixel_count = 0;
    std::size_t cached_pixel_count = 0;
    std::size_t cached_pixel_byte_count = 0;
    std::size_t cached_decoded_byte_count = 0;
    std::size_t pinned_texture_count = 0;
    std::size_t evictable_texture_count = 0;
    std::size_t pinned_pixel_count = 0;
    std::size_t evictable_pixel_count = 0;
    std::size_t eviction_count = 0;
    std::size_t over_capacity_texture_count = 0;
    std::size_t upload_ready_texture_count = 0;
    std::size_t placeholder_fallback_texture_count = 0;
    std::size_t next_access_sequence = 1;
    std::size_t resident_access_count = 0;
    std::size_t capacity_eviction_count = 0;
    std::size_t release_eviction_count = 0;
    std::size_t invalidation_eviction_count = 0;
    bool capacity_exceeded = false;
    std::vector<fake_image_texture_cache_entry_snapshot> entries;
    std::vector<fake_image_texture_eviction_snapshot> evictions;
    std::size_t residency_transition_count = 0;
    std::vector<fake_image_texture_residency_transition_snapshot> residency_transitions;
    bool placeholder_policy_enabled = false;
    fake_image_texture_placeholder_policy placeholder_policy;
    std::size_t placeholder_policy_texture_count = 0;
    std::size_t placeholder_policy_request_count = 0;
    std::size_t placeholder_policy_cache_hit_count = 0;
    std::size_t placeholder_policy_upload_count = 0;
    std::vector<fake_image_texture_placeholder_snapshot> placeholder_snapshots;
};

} // namespace quiz_vulkan::render
