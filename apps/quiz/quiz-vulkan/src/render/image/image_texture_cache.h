#pragma once

#include "render/image/image_decoder.h"
#include "render/image/image_types.h"

#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace quiz_vulkan::render {

struct render_image_texture_request {
    render_resolved_image_source source;
    render_image_sampler_policy sampler;
};

enum class render_image_texture_status {
    ready,
    missing_source,
    decode_failed,
    upload_failed,
};

struct render_image_texture_result {
    render_image_texture_status status = render_image_texture_status::missing_source;
    render_image_texture_key key;
    render_image_texture_handle texture;
    bool cache_hit = false;
    std::string diagnostic;
    render_image_decode_metadata decode_metadata;
    std::vector<render_image_decoder_diagnostic> decoder_diagnostics;

    bool ok() const
    {
        return status == render_image_texture_status::ready && texture.valid();
    }
};

class image_texture_cache_interface {
public:
    virtual ~image_texture_cache_interface() = default;

    virtual render_image_texture_result acquire_texture(const render_image_texture_request& request) = 0;
    virtual void release_unused() = 0;
};

inline render_image_texture_key make_render_image_texture_key(const render_image_texture_request& request)
{
    return render_image_texture_key{
        .source_key = request.source.cache_key(),
        .sampler = request.sampler,
    };
}

inline bool is_valid_render_image_cache_key(std::string_view cache_key)
{
    if (cache_key.empty()) {
        return false;
    }

    for (const char value : cache_key) {
        if (std::iscntrl(static_cast<unsigned char>(value)) != 0) {
            return false;
        }
    }
    return true;
}

inline bool is_valid_render_image_texture_key(const render_image_texture_key& key)
{
    return is_valid_render_image_cache_key(key.source_key);
}

inline bool is_valid_render_image_filter(render_image_filter filter)
{
    switch (filter) {
    case render_image_filter::nearest:
    case render_image_filter::linear:
        return true;
    }

    return false;
}

inline bool is_valid_render_image_mipmap_mode(render_image_mipmap_mode mode)
{
    switch (mode) {
    case render_image_mipmap_mode::none:
    case render_image_mipmap_mode::nearest:
    case render_image_mipmap_mode::linear:
        return true;
    }

    return false;
}

inline bool is_valid_render_image_wrap_mode(render_image_wrap_mode mode)
{
    switch (mode) {
    case render_image_wrap_mode::clamp_to_edge:
    case render_image_wrap_mode::repeat:
    case render_image_wrap_mode::mirrored_repeat:
        return true;
    }

    return false;
}

inline bool is_valid_render_image_sampler_policy(const render_image_sampler_policy& sampler)
{
    return is_valid_render_image_filter(sampler.min_filter)
        && is_valid_render_image_filter(sampler.mag_filter)
        && is_valid_render_image_mipmap_mode(sampler.mipmap_mode)
        && is_valid_render_image_wrap_mode(sampler.wrap_u)
        && is_valid_render_image_wrap_mode(sampler.wrap_v);
}

inline std::string render_image_filter_name(render_image_filter filter)
{
    switch (filter) {
    case render_image_filter::nearest:
        return "nearest";
    case render_image_filter::linear:
        return "linear";
    }

    return "unknown";
}

inline std::string render_image_mipmap_mode_name(render_image_mipmap_mode mode)
{
    switch (mode) {
    case render_image_mipmap_mode::none:
        return "none";
    case render_image_mipmap_mode::nearest:
        return "nearest";
    case render_image_mipmap_mode::linear:
        return "linear";
    }

    return "unknown";
}

inline std::string render_image_wrap_mode_name(render_image_wrap_mode mode)
{
    switch (mode) {
    case render_image_wrap_mode::clamp_to_edge:
        return "clamp_to_edge";
    case render_image_wrap_mode::repeat:
        return "repeat";
    case render_image_wrap_mode::mirrored_repeat:
        return "mirrored_repeat";
    }

    return "unknown";
}

enum class render_image_texture_color_space {
    unknown,
    linear,
    srgb,
};

inline render_image_texture_color_space render_image_texture_color_space_for(
    render_image_pixel_format pixel_format)
{
    switch (pixel_format) {
    case render_image_pixel_format::rgba8_unorm:
        return render_image_texture_color_space::linear;
    case render_image_pixel_format::rgba8_srgb:
        return render_image_texture_color_space::srgb;
    }

    return render_image_texture_color_space::unknown;
}

inline std::string render_image_texture_color_space_name(render_image_texture_color_space color_space)
{
    switch (color_space) {
    case render_image_texture_color_space::unknown:
        return "unknown";
    case render_image_texture_color_space::linear:
        return "linear";
    case render_image_texture_color_space::srgb:
        return "srgb";
    }

    return "unknown";
}

inline std::string render_image_sampler_policy_stable_fragment(
    const render_image_sampler_policy& sampler)
{
    return "min=" + render_image_filter_name(sampler.min_filter)
        + ";mag=" + render_image_filter_name(sampler.mag_filter)
        + ";mipmap=" + render_image_mipmap_mode_name(sampler.mipmap_mode)
        + ";wrap_u=" + render_image_wrap_mode_name(sampler.wrap_u)
        + ";wrap_v=" + render_image_wrap_mode_name(sampler.wrap_v);
}

struct render_image_sampler_policy_diagnostic {
    render_image_sampler_policy sampler;
    std::string min_filter;
    std::string mag_filter;
    std::string mipmap_mode;
    std::string wrap_u;
    std::string wrap_v;
    std::string stable_key_fragment;
    bool valid = false;
    bool uses_nearest_filtering = false;
    bool uses_linear_filtering = false;
    bool uses_mipmaps = false;
    bool repeats_u = false;
    bool repeats_v = false;
    bool clamps_u = false;
    bool clamps_v = false;
    std::string diagnostic;
};

inline render_image_sampler_policy_diagnostic make_render_image_sampler_policy_diagnostic(
    const render_image_sampler_policy& sampler)
{
    const bool valid = is_valid_render_image_sampler_policy(sampler);
    return render_image_sampler_policy_diagnostic{
        .sampler = sampler,
        .min_filter = render_image_filter_name(sampler.min_filter),
        .mag_filter = render_image_filter_name(sampler.mag_filter),
        .mipmap_mode = render_image_mipmap_mode_name(sampler.mipmap_mode),
        .wrap_u = render_image_wrap_mode_name(sampler.wrap_u),
        .wrap_v = render_image_wrap_mode_name(sampler.wrap_v),
        .stable_key_fragment = render_image_sampler_policy_stable_fragment(sampler),
        .valid = valid,
        .uses_nearest_filtering = sampler.min_filter == render_image_filter::nearest
            || sampler.mag_filter == render_image_filter::nearest
            || sampler.mipmap_mode == render_image_mipmap_mode::nearest,
        .uses_linear_filtering = sampler.min_filter == render_image_filter::linear
            || sampler.mag_filter == render_image_filter::linear
            || sampler.mipmap_mode == render_image_mipmap_mode::linear,
        .uses_mipmaps = sampler.mipmap_mode != render_image_mipmap_mode::none,
        .repeats_u = sampler.wrap_u == render_image_wrap_mode::repeat
            || sampler.wrap_u == render_image_wrap_mode::mirrored_repeat,
        .repeats_v = sampler.wrap_v == render_image_wrap_mode::repeat
            || sampler.wrap_v == render_image_wrap_mode::mirrored_repeat,
        .clamps_u = sampler.wrap_u == render_image_wrap_mode::clamp_to_edge,
        .clamps_v = sampler.wrap_v == render_image_wrap_mode::clamp_to_edge,
        .diagnostic = valid
            ? "sampler policy is valid"
            : "sampler policy contains unsupported enum value",
    };
}

struct render_image_texture_key_diagnostic {
    render_image_texture_key key;
    render_image_sampler_policy_diagnostic sampler_policy;
    std::string stable_cache_key;
    bool valid = false;
    std::string diagnostic;
};

inline render_image_texture_key_diagnostic make_render_image_texture_key_diagnostic(
    const render_image_texture_key& key)
{
    render_image_sampler_policy_diagnostic sampler_policy =
        make_render_image_sampler_policy_diagnostic(key.sampler);
    const bool valid_source_key = is_valid_render_image_texture_key(key);
    std::string diagnostic;
    if (!valid_source_key) {
        diagnostic = "image texture key source is empty or contains control characters";
    } else if (!sampler_policy.valid) {
        diagnostic = sampler_policy.diagnostic;
    } else {
        diagnostic = "image texture key is stable";
    }
    const bool valid = valid_source_key && sampler_policy.valid;

    return render_image_texture_key_diagnostic{
        .key = key,
        .sampler_policy = std::move(sampler_policy),
        .stable_cache_key = "source=" + key.source_key + "|" + render_image_sampler_policy_stable_fragment(key.sampler),
        .valid = valid,
        .diagnostic = std::move(diagnostic),
    };
}

inline std::size_t render_image_pixel_format_byte_count(render_image_pixel_format pixel_format)
{
    switch (pixel_format) {
    case render_image_pixel_format::rgba8_unorm:
    case render_image_pixel_format::rgba8_srgb:
        return 4;
    }

    return 0;
}

inline std::size_t expected_render_decoded_image_byte_count(const render_decoded_image& image)
{
    const std::size_t bytes_per_pixel = render_image_pixel_format_byte_count(image.pixel_format);
    if (bytes_per_pixel == 0 || image.width == 0 || image.height == 0) {
        return 0;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (image.width > max_size / image.height) {
        return 0;
    }
    const std::size_t pixel_count = image.width * image.height;
    if (pixel_count > max_size / bytes_per_pixel) {
        return 0;
    }
    return pixel_count * bytes_per_pixel;
}

inline bool has_valid_render_decoded_image_payload(const render_decoded_image& image)
{
    const std::size_t expected_byte_count = expected_render_decoded_image_byte_count(image);
    return expected_byte_count != 0 && image.pixels.size() == expected_byte_count;
}

struct render_image_upload_readiness_snapshot {
    render_image_texture_key key;
    render_image_texture_key_diagnostic key_diagnostic;
    render_image_sampler_policy_diagnostic sampler_policy;
    render_image_decode_metadata decode_metadata;
    render_image_texture_color_space color_space = render_image_texture_color_space::unknown;
    std::string color_space_name;
    bool placeholder_fallback = false;
    bool payload_valid = false;
    bool upload_ready = false;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    std::string diagnostic;
};

inline std::size_t render_image_checked_pixel_count(const render_decoded_image& image)
{
    if (image.width == 0 || image.height == 0) {
        return 0;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    if (image.width > max_size / image.height) {
        return 0;
    }
    return image.width * image.height;
}

inline render_image_upload_readiness_snapshot make_render_image_upload_readiness_snapshot(
    const render_image_texture_key& key,
    const render_image_decode_metadata& decode_metadata,
    const render_decoded_image& image)
{
    render_image_texture_key_diagnostic key_diagnostic = make_render_image_texture_key_diagnostic(key);
    render_image_sampler_policy_diagnostic sampler_policy = key_diagnostic.sampler_policy;
    const render_image_texture_color_space color_space =
        render_image_texture_color_space_for(image.pixel_format);
    const bool payload_valid = has_valid_render_decoded_image_payload(image);
    const std::size_t decoded_byte_count = image.pixels.size();
    std::string diagnostic;
    if (!key_diagnostic.valid) {
        diagnostic = key_diagnostic.diagnostic;
    } else if (color_space == render_image_texture_color_space::unknown) {
        diagnostic = "image texture upload color space is unsupported";
    } else if (image.empty()) {
        diagnostic = "decoded image is empty";
    } else if (!payload_valid) {
        diagnostic = "decoded image payload size does not match dimensions and format";
    } else {
        diagnostic = "decoded image is upload-ready";
    }
    const bool upload_ready = key_diagnostic.valid && payload_valid
        && color_space != render_image_texture_color_space::unknown;

    return render_image_upload_readiness_snapshot{
        .key = key,
        .key_diagnostic = std::move(key_diagnostic),
        .sampler_policy = std::move(sampler_policy),
        .decode_metadata = decode_metadata,
        .color_space = color_space,
        .color_space_name = render_image_texture_color_space_name(color_space),
        .placeholder_fallback = decode_metadata.format_detection.placeholder_fallback,
        .payload_valid = payload_valid,
        .upload_ready = upload_ready,
        .pixel_count = render_image_checked_pixel_count(image),
        .pixel_byte_count = expected_render_decoded_image_byte_count(image),
        .decoded_byte_count = decoded_byte_count,
        .staging_byte_count = payload_valid ? decoded_byte_count : 0,
        .diagnostic = std::move(diagnostic),
    };
}

struct render_image_texture_key_less {
    static std::tuple<
        render_image_filter,
        render_image_filter,
        render_image_mipmap_mode,
        render_image_wrap_mode,
        render_image_wrap_mode>
    sampler_tuple(const render_image_sampler_policy& sampler)
    {
        return {
            sampler.min_filter,
            sampler.mag_filter,
            sampler.mipmap_mode,
            sampler.wrap_u,
            sampler.wrap_v,
        };
    }

    bool operator()(const render_image_texture_key& left, const render_image_texture_key& right) const
    {
        if (left.source_key != right.source_key) {
            return left.source_key < right.source_key;
        }
        return sampler_tuple(left.sampler) < sampler_tuple(right.sampler);
    }
};

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

enum class fake_image_texture_placeholder_reason {
    none,
    resolve_failed,
    source_load_failed,
    decode_failed,
    upload_failed,
};

inline std::string fake_image_texture_placeholder_reason_name(
    fake_image_texture_placeholder_reason reason)
{
    switch (reason) {
    case fake_image_texture_placeholder_reason::none:
        return "none";
    case fake_image_texture_placeholder_reason::resolve_failed:
        return "resolve_failed";
    case fake_image_texture_placeholder_reason::source_load_failed:
        return "source_load_failed";
    case fake_image_texture_placeholder_reason::decode_failed:
        return "decode_failed";
    case fake_image_texture_placeholder_reason::upload_failed:
        return "upload_failed";
    }

    return "unknown";
}

struct fake_image_texture_placeholder_policy {
    bool enabled = false;
    std::size_t width = 2;
    std::size_t height = 2;
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    std::string source_key_prefix = "placeholder://image/";
};

inline std::string fake_image_texture_placeholder_source_fragment(
    std::string_view source_key)
{
    if (source_key.empty()) {
        return "empty";
    }

    std::string fragment;
    fragment.reserve(source_key.size());
    for (const char value : source_key) {
        fragment.push_back(
            std::iscntrl(static_cast<unsigned char>(value)) != 0 ? '_' : value);
    }
    return fragment;
}

inline render_image_cache_key make_fake_image_texture_placeholder_source_key(
    const fake_image_texture_placeholder_policy& policy,
    fake_image_texture_placeholder_reason reason,
    const render_image_cache_key& source_key)
{
    return policy.source_key_prefix + fake_image_texture_placeholder_reason_name(reason)
        + "/" + fake_image_texture_placeholder_source_fragment(source_key);
}

inline render_image_texture_key make_fake_image_texture_placeholder_key(
    const fake_image_texture_placeholder_policy& policy,
    fake_image_texture_placeholder_reason reason,
    const render_image_texture_key& requested_key)
{
    return render_image_texture_key{
        .source_key = make_fake_image_texture_placeholder_source_key(
            policy,
            reason,
            requested_key.source_key),
        .sampler = requested_key.sampler,
    };
}

inline bool is_fake_image_texture_placeholder_key(const render_image_texture_key& key)
{
    return key.source_key.starts_with("placeholder://image/");
}

inline render_decoded_image make_fake_image_texture_placeholder_decoded_image(
    const fake_image_texture_placeholder_policy& policy)
{
    render_decoded_image image{
        .width = policy.width,
        .height = policy.height,
        .pixel_format = policy.pixel_format,
        .pixels = {},
    };
    const std::size_t byte_count = expected_render_decoded_image_byte_count(image);
    if (byte_count == 0) {
        return {};
    }

    image.pixels.resize(byte_count);
    for (std::size_t pixel = 0; pixel < policy.width * policy.height; ++pixel) {
        const std::size_t offset = pixel * 4;
        const bool bright = pixel % 2 == 0;
        image.pixels[offset] = bright ? std::byte{0xff} : std::byte{0x40};
        image.pixels[offset + 1] = std::byte{0x00};
        image.pixels[offset + 2] = bright ? std::byte{0xff} : std::byte{0x40};
        image.pixels[offset + 3] = std::byte{0xff};
    }
    return image;
}

struct fake_image_texture_placeholder_snapshot {
    std::size_t sequence = 0;
    fake_image_texture_placeholder_reason reason = fake_image_texture_placeholder_reason::none;
    render_image_texture_key requested_key;
    render_image_texture_key placeholder_key;
    render_image_sampler_policy sampler;
    render_image_texture_handle texture;
    std::uint64_t upload_generation_id = 0;
    bool cache_hit = false;
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::string diagnostic;
};

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

struct render_image_texture_upload_request {
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    render_decoded_image image;
};

enum class render_image_texture_upload_status {
    uploaded,
    invalid_key,
    invalid_sampler,
    unsupported_format,
    invalid_image,
};

struct render_image_texture_upload_result {
    render_image_texture_upload_status status = render_image_texture_upload_status::invalid_image;
    std::uint64_t generation_id = 0;
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    render_image_texture_handle texture;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    std::string diagnostic;

    bool ok() const
    {
        return status == render_image_texture_upload_status::uploaded && texture.valid();
    }
};

class image_texture_uploader_interface {
public:
    virtual ~image_texture_uploader_interface() = default;

    virtual render_image_texture_upload_result upload(
        const render_image_texture_upload_request& request) = 0;
};

using fake_image_texture_upload_generation_id = std::uint64_t;

enum class fake_image_texture_upload_retry_eligibility {
    not_needed,
    eligible,
    ineligible,
};

inline std::string fake_image_texture_upload_retry_eligibility_name(
    fake_image_texture_upload_retry_eligibility eligibility)
{
    switch (eligibility) {
    case fake_image_texture_upload_retry_eligibility::not_needed:
        return "not_needed";
    case fake_image_texture_upload_retry_eligibility::eligible:
        return "eligible";
    case fake_image_texture_upload_retry_eligibility::ineligible:
        return "ineligible";
    }

    return "unknown";
}

inline bool is_retryable_render_image_texture_upload_status(
    render_image_texture_upload_status status)
{
    return status == render_image_texture_upload_status::invalid_image;
}

inline std::size_t fake_image_texture_upload_retry_backoff_sequence_delta(
    std::size_t failed_attempt_count_for_key)
{
    if (failed_attempt_count_for_key == 0) {
        return 0;
    }

    constexpr std::size_t max_backoff = 16;
    if (failed_attempt_count_for_key > 5) {
        return max_backoff;
    }

    const std::size_t backoff = std::size_t{1} << (failed_attempt_count_for_key - 1);
    return backoff < max_backoff ? backoff : max_backoff;
}

struct fake_image_texture_upload_retry_snapshot {
    fake_image_texture_upload_generation_id generation_id = 0;
    render_image_texture_key key;
    render_image_texture_upload_status status = render_image_texture_upload_status::invalid_image;
    fake_image_texture_upload_retry_eligibility eligibility =
        fake_image_texture_upload_retry_eligibility::ineligible;
    std::size_t attempt_count_for_key = 0;
    std::size_t failed_attempt_count_for_key = 0;
    std::size_t retry_after_queue_sequence_delta = 0;
    std::size_t next_retry_sequence = 0;
    std::string diagnostic;
};

struct fake_image_texture_upload_request_snapshot {
    fake_image_texture_upload_generation_id generation_id = 0;
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    std::size_t width = 0;
    std::size_t height = 0;
    render_image_pixel_format pixel_format = render_image_pixel_format::rgba8_srgb;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    std::size_t enqueue_sequence = 0;
    std::size_t queue_depth_before_enqueue = 0;
    std::size_t queue_depth_after_enqueue = 0;
    std::size_t attempt_count_for_key = 0;
};

struct fake_image_texture_upload_result_snapshot {
    fake_image_texture_upload_generation_id generation_id = 0;
    render_image_texture_upload_status status = render_image_texture_upload_status::invalid_image;
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    render_image_texture_handle texture;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    std::string diagnostic;
    std::size_t completion_sequence = 0;
    std::size_t queue_depth_after_completion = 0;
    fake_image_texture_upload_retry_snapshot retry;
};

struct fake_image_texture_upload_snapshot_entry {
    fake_image_texture_upload_generation_id generation_id = 0;
    fake_image_texture_upload_request_snapshot request;
    fake_image_texture_upload_result_snapshot result;
    render_image_texture_key key;
    render_image_sampler_policy sampler;
    render_image_texture_handle texture;
    render_image_texture_upload_status status = render_image_texture_upload_status::invalid_image;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t staging_byte_count = 0;
    std::string diagnostic;
    fake_image_texture_upload_retry_snapshot retry;
};

struct fake_image_texture_upload_queue_entry_snapshot {
    std::size_t enqueue_sequence = 0;
    std::size_t completion_sequence = 0;
    fake_image_texture_upload_generation_id generation_id = 0;
    render_image_texture_key key;
    render_image_texture_upload_status status = render_image_texture_upload_status::invalid_image;
    render_image_texture_handle texture;
    bool completed = false;
    std::size_t staging_byte_count = 0;
    std::size_t queue_depth_before_enqueue = 0;
    std::size_t queue_depth_after_enqueue = 0;
    std::size_t queue_depth_after_completion = 0;
    std::string diagnostic;
    fake_image_texture_upload_retry_snapshot retry;
};

struct fake_image_texture_upload_snapshot {
    std::size_t upload_count = 0;
    std::size_t failed_upload_count = 0;
    std::size_t uploaded_pixel_count = 0;
    std::size_t uploaded_pixel_byte_count = 0;
    std::size_t uploaded_decoded_byte_count = 0;
    std::size_t staged_byte_count = 0;
    std::size_t attempted_staging_byte_count = 0;
    fake_image_texture_upload_generation_id next_generation_id = 1;
    std::vector<fake_image_texture_upload_request_snapshot> request_snapshots;
    std::vector<fake_image_texture_upload_result_snapshot> result_snapshots;
    std::vector<fake_image_texture_upload_snapshot_entry> entries;
    std::size_t enqueued_upload_count = 0;
    std::size_t completed_upload_count = 0;
    std::size_t pending_upload_count = 0;
    std::size_t next_queue_sequence = 1;
    std::vector<fake_image_texture_upload_queue_entry_snapshot> queue_entries;
    std::size_t retryable_upload_count = 0;
    std::size_t nonretryable_upload_count = 0;
    std::size_t completed_without_retry_count = 0;
    std::vector<fake_image_texture_upload_retry_snapshot> retry_snapshots;
};

class fake_image_texture_uploader final : public image_texture_uploader_interface {
public:
    render_image_texture_upload_result upload(const render_image_texture_upload_request& request) override
    {
        upload_requests.push_back(request);

        const fake_image_texture_upload_generation_id generation_id = next_generation_id_++;
        const std::size_t enqueue_sequence = next_upload_queue_sequence_++;
        const std::size_t queue_depth_before_enqueue = pending_upload_count_;
        ++pending_upload_count_;
        const std::size_t queue_depth_after_enqueue = pending_upload_count_;
        const std::size_t attempt_count_for_key = ++upload_attempt_count_by_key_[request.key];
        const std::size_t pixel_byte_count = expected_render_decoded_image_byte_count(request.image);
        const std::size_t decoded_byte_count = request.image.pixels.size();
        std::size_t pixel_count = 0;
        if (request.image.width != 0 && request.image.height != 0
            && request.image.width <= std::numeric_limits<std::size_t>::max() / request.image.height) {
            pixel_count = request.image.width * request.image.height;
        }
        const std::size_t staging_byte_count = has_valid_render_decoded_image_payload(request.image)
            ? decoded_byte_count
            : 0;

        upload_request_snapshots.push_back(fake_image_texture_upload_request_snapshot{
            .generation_id = generation_id,
            .key = request.key,
            .sampler = request.sampler,
            .width = request.image.width,
            .height = request.image.height,
            .pixel_format = request.image.pixel_format,
            .pixel_count = pixel_count,
            .pixel_byte_count = pixel_byte_count,
            .decoded_byte_count = decoded_byte_count,
            .staging_byte_count = staging_byte_count,
            .enqueue_sequence = enqueue_sequence,
            .queue_depth_before_enqueue = queue_depth_before_enqueue,
            .queue_depth_after_enqueue = queue_depth_after_enqueue,
            .attempt_count_for_key = attempt_count_for_key,
        });

        if (!is_valid_render_image_texture_key(request.key)) {
            return record_result(render_image_texture_upload_result{
                .status = render_image_texture_upload_status::invalid_key,
                .generation_id = generation_id,
                .key = request.key,
                .sampler = request.sampler,
                .texture = {},
                .pixel_count = pixel_count,
                .pixel_byte_count = pixel_byte_count,
                .decoded_byte_count = decoded_byte_count,
                .staging_byte_count = staging_byte_count,
                .diagnostic = "image texture upload key is empty or contains control characters",
            },
                enqueue_sequence,
                queue_depth_after_enqueue,
                attempt_count_for_key);
        }

        if (request.key.sampler != request.sampler || !is_valid_render_image_sampler_policy(request.sampler)) {
            return record_result(render_image_texture_upload_result{
                .status = render_image_texture_upload_status::invalid_sampler,
                .generation_id = generation_id,
                .key = request.key,
                .sampler = request.sampler,
                .texture = {},
                .pixel_count = pixel_count,
                .pixel_byte_count = pixel_byte_count,
                .decoded_byte_count = decoded_byte_count,
                .staging_byte_count = staging_byte_count,
                .diagnostic = "image texture upload sampler policy is invalid or does not match the texture key",
            },
                enqueue_sequence,
                queue_depth_after_enqueue,
                attempt_count_for_key);
        }

        if (render_image_pixel_format_byte_count(request.image.pixel_format) == 0) {
            return record_result(render_image_texture_upload_result{
                .status = render_image_texture_upload_status::unsupported_format,
                .generation_id = generation_id,
                .key = request.key,
                .sampler = request.sampler,
                .texture = {},
                .pixel_count = pixel_count,
                .pixel_byte_count = pixel_byte_count,
                .decoded_byte_count = decoded_byte_count,
                .staging_byte_count = staging_byte_count,
                .diagnostic = "image texture upload pixel format is unsupported",
            },
                enqueue_sequence,
                queue_depth_after_enqueue,
                attempt_count_for_key);
        }

        if (!has_valid_render_decoded_image_payload(request.image)) {
            return record_result(render_image_texture_upload_result{
                .status = render_image_texture_upload_status::invalid_image,
                .generation_id = generation_id,
                .key = request.key,
                .sampler = request.sampler,
                .texture = {},
                .pixel_count = pixel_count,
                .pixel_byte_count = pixel_byte_count,
                .decoded_byte_count = decoded_byte_count,
                .staging_byte_count = staging_byte_count,
                .diagnostic = "image texture upload payload size does not match dimensions and format",
            },
                enqueue_sequence,
                queue_depth_after_enqueue,
                attempt_count_for_key);
        }

        return record_result(render_image_texture_upload_result{
            .status = render_image_texture_upload_status::uploaded,
            .generation_id = generation_id,
            .key = request.key,
            .sampler = request.sampler,
            .texture = render_image_texture_handle{
                .id = next_id_++,
                .revision = 1,
                .width = request.image.width,
                .height = request.image.height,
            },
            .pixel_count = pixel_count,
            .pixel_byte_count = pixel_byte_count,
            .decoded_byte_count = decoded_byte_count,
            .staging_byte_count = staging_byte_count,
            .diagnostic = {},
        },
            enqueue_sequence,
            queue_depth_after_enqueue,
            attempt_count_for_key);
    }

    fake_image_texture_upload_snapshot diagnostic_snapshot() const
    {
        fake_image_texture_upload_snapshot snapshot{
            .upload_count = upload_results.size(),
            .failed_upload_count = failed_upload_count_,
            .uploaded_pixel_count = uploaded_pixel_count_,
            .uploaded_pixel_byte_count = uploaded_pixel_byte_count_,
            .uploaded_decoded_byte_count = uploaded_decoded_byte_count_,
            .staged_byte_count = staged_byte_count_,
            .attempted_staging_byte_count = attempted_staging_byte_count_,
            .next_generation_id = next_generation_id_,
            .request_snapshots = upload_request_snapshots,
            .result_snapshots = upload_result_snapshots,
            .entries = {},
            .enqueued_upload_count = upload_requests.size(),
            .completed_upload_count = upload_results.size(),
            .pending_upload_count = pending_upload_count_,
            .next_queue_sequence = next_upload_queue_sequence_,
            .queue_entries = upload_queue_entries_,
            .retryable_upload_count = retryable_upload_count_,
            .nonretryable_upload_count = nonretryable_upload_count_,
            .completed_without_retry_count = completed_without_retry_count_,
            .retry_snapshots = upload_retry_snapshots_,
        };
        snapshot.entries.reserve(upload_results.size());
        for (std::size_t index = 0; index < upload_results.size(); ++index) {
            const render_image_texture_upload_result& result = upload_results[index];
            snapshot.entries.push_back(fake_image_texture_upload_snapshot_entry{
                .generation_id = result.generation_id,
                .request = upload_request_snapshots[index],
                .result = upload_result_snapshots[index],
                .key = result.key,
                .sampler = result.sampler,
                .texture = result.texture,
                .status = result.status,
                .pixel_count = result.pixel_count,
                .pixel_byte_count = result.pixel_byte_count,
                .decoded_byte_count = result.decoded_byte_count,
                .staging_byte_count = result.staging_byte_count,
                .diagnostic = result.diagnostic,
                .retry = upload_result_snapshots[index].retry,
            });
        }
        return snapshot;
    }

    std::vector<render_image_texture_upload_request> upload_requests;
    std::vector<fake_image_texture_upload_request_snapshot> upload_request_snapshots;
    std::vector<fake_image_texture_upload_result_snapshot> upload_result_snapshots;
    std::vector<render_image_texture_upload_result> upload_results;

private:
    render_image_texture_upload_result record_result(
        render_image_texture_upload_result result,
        std::size_t enqueue_sequence,
        std::size_t queue_depth_after_enqueue,
        std::size_t attempt_count_for_key)
    {
        attempted_staging_byte_count_ += result.staging_byte_count;
        if (result.ok()) {
            uploaded_pixel_count_ += result.pixel_count;
            uploaded_pixel_byte_count_ += result.pixel_byte_count;
            uploaded_decoded_byte_count_ += result.decoded_byte_count;
            staged_byte_count_ += result.staging_byte_count;
        } else {
            ++failed_upload_count_;
        }

        const std::size_t completion_sequence = next_upload_queue_sequence_++;
        if (pending_upload_count_ != 0) {
            --pending_upload_count_;
        }
        const std::size_t queue_depth_after_completion = pending_upload_count_;
        const fake_image_texture_upload_retry_snapshot retry = make_retry_snapshot(
            result,
            attempt_count_for_key,
            completion_sequence);

        upload_result_snapshots.push_back(fake_image_texture_upload_result_snapshot{
            .generation_id = result.generation_id,
            .status = result.status,
            .key = result.key,
            .sampler = result.sampler,
            .texture = result.texture,
            .pixel_count = result.pixel_count,
            .pixel_byte_count = result.pixel_byte_count,
            .decoded_byte_count = result.decoded_byte_count,
            .staging_byte_count = result.staging_byte_count,
            .diagnostic = result.diagnostic,
            .completion_sequence = completion_sequence,
            .queue_depth_after_completion = queue_depth_after_completion,
            .retry = retry,
        });
        upload_queue_entries_.push_back(fake_image_texture_upload_queue_entry_snapshot{
            .enqueue_sequence = enqueue_sequence,
            .completion_sequence = completion_sequence,
            .generation_id = result.generation_id,
            .key = result.key,
            .status = result.status,
            .texture = result.texture,
            .completed = true,
            .staging_byte_count = result.staging_byte_count,
            .queue_depth_before_enqueue = queue_depth_after_enqueue == 0 ? 0 : queue_depth_after_enqueue - 1,
            .queue_depth_after_enqueue = queue_depth_after_enqueue,
            .queue_depth_after_completion = queue_depth_after_completion,
            .diagnostic = result.diagnostic,
            .retry = retry,
        });
        upload_retry_snapshots_.push_back(retry);
        upload_results.push_back(result);
        return result;
    }

    fake_image_texture_upload_retry_snapshot make_retry_snapshot(
        const render_image_texture_upload_result& result,
        std::size_t attempt_count_for_key,
        std::size_t completion_sequence)
    {
        std::size_t failed_attempt_count_for_key = failed_upload_attempt_count_by_key_[result.key];
        if (!result.ok()) {
            failed_attempt_count_for_key = ++failed_upload_attempt_count_by_key_[result.key];
        }

        if (result.ok()) {
            ++completed_without_retry_count_;
            return fake_image_texture_upload_retry_snapshot{
                .generation_id = result.generation_id,
                .key = result.key,
                .status = result.status,
                .eligibility = fake_image_texture_upload_retry_eligibility::not_needed,
                .attempt_count_for_key = attempt_count_for_key,
                .failed_attempt_count_for_key = failed_attempt_count_for_key,
                .retry_after_queue_sequence_delta = 0,
                .next_retry_sequence = 0,
                .diagnostic = "image texture upload succeeded; retry is not needed",
            };
        }

        if (is_retryable_render_image_texture_upload_status(result.status)) {
            ++retryable_upload_count_;
            const std::size_t backoff = fake_image_texture_upload_retry_backoff_sequence_delta(
                failed_attempt_count_for_key);
            return fake_image_texture_upload_retry_snapshot{
                .generation_id = result.generation_id,
                .key = result.key,
                .status = result.status,
                .eligibility = fake_image_texture_upload_retry_eligibility::eligible,
                .attempt_count_for_key = attempt_count_for_key,
                .failed_attempt_count_for_key = failed_attempt_count_for_key,
                .retry_after_queue_sequence_delta = backoff,
                .next_retry_sequence = completion_sequence + backoff,
                .diagnostic = "image texture upload can retry after decoded payload changes",
            };
        }

        ++nonretryable_upload_count_;
        return fake_image_texture_upload_retry_snapshot{
            .generation_id = result.generation_id,
            .key = result.key,
            .status = result.status,
            .eligibility = fake_image_texture_upload_retry_eligibility::ineligible,
            .attempt_count_for_key = attempt_count_for_key,
            .failed_attempt_count_for_key = failed_attempt_count_for_key,
            .retry_after_queue_sequence_delta = 0,
            .next_retry_sequence = 0,
            .diagnostic = "image texture upload failure is structural and is not retryable",
        };
    }

    render_image_texture_id next_id_ = 1;
    fake_image_texture_upload_generation_id next_generation_id_ = 1;
    std::size_t next_upload_queue_sequence_ = 1;
    std::size_t pending_upload_count_ = 0;
    std::size_t failed_upload_count_ = 0;
    std::size_t retryable_upload_count_ = 0;
    std::size_t nonretryable_upload_count_ = 0;
    std::size_t completed_without_retry_count_ = 0;
    std::size_t uploaded_pixel_count_ = 0;
    std::size_t uploaded_pixel_byte_count_ = 0;
    std::size_t uploaded_decoded_byte_count_ = 0;
    std::size_t staged_byte_count_ = 0;
    std::size_t attempted_staging_byte_count_ = 0;
    std::map<render_image_texture_key, std::size_t, render_image_texture_key_less> upload_attempt_count_by_key_;
    std::map<render_image_texture_key, std::size_t, render_image_texture_key_less> failed_upload_attempt_count_by_key_;
    std::vector<fake_image_texture_upload_queue_entry_snapshot> upload_queue_entries_;
    std::vector<fake_image_texture_upload_retry_snapshot> upload_retry_snapshots_;
};

class fake_image_texture_cache final : public image_texture_cache_interface {
public:
    explicit fake_image_texture_cache(const image_decoder_interface& decoder)
        : decoder_(decoder)
        , uploader_(&internal_uploader_)
    {
    }

    fake_image_texture_cache(const image_decoder_interface& decoder, image_texture_uploader_interface& uploader)
        : decoder_(decoder)
        , uploader_(&uploader)
    {
    }

    render_image_texture_result acquire_texture(const render_image_texture_request& request) override
    {
        const render_image_texture_key key = make_render_image_texture_key(request);
        if (!is_valid_render_image_texture_key(key)) {
            return render_image_texture_result{
                .status = render_image_texture_status::missing_source,
                .key = key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = "image texture cache key is empty or contains control characters",
            };
        }

        if (request.source.kind == render_image_source_kind::unsupported) {
            if (placeholder_texture_policy_.enabled) {
                return acquire_placeholder_texture(
                    request,
                    fake_image_texture_placeholder_reason::resolve_failed,
                    "image source kind is unsupported");
            }
            return render_image_texture_result{
                .status = render_image_texture_status::missing_source,
                .key = key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = "image source kind is unsupported",
            };
        }

        if (request.source.is_remote()) {
            if (placeholder_texture_policy_.enabled) {
                return acquire_placeholder_texture(
                    request,
                    fake_image_texture_placeholder_reason::source_load_failed,
                    "fake image texture cache does not fetch remote images");
            }
            return render_image_texture_result{
                .status = render_image_texture_status::missing_source,
                .key = key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = "fake image texture cache does not fetch remote images",
            };
        }

        if (!is_valid_render_image_sampler_policy(request.sampler)) {
            return render_image_texture_result{
                .status = render_image_texture_status::upload_failed,
                .key = key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = "image sampler policy contains unsupported enum value",
            };
        }

        const auto existing = textures_.find(key);
        if (existing != textures_.end()) {
            existing->second.last_used_sequence = next_access_sequence_++;
            ++existing->second.access_count;
            return render_image_texture_result{
                .status = render_image_texture_status::ready,
                .key = key,
                .texture = existing->second.texture,
                .cache_hit = true,
                .diagnostic = {},
                .decode_metadata = existing->second.decode_metadata,
                .decoder_diagnostics = existing->second.decoder_diagnostics,
            };
        }

        const render_image_decode_request decode_request{
            .source = request.source,
            .encoded_bytes = placeholder_encoded_bytes_for(key.source_key),
        };
        if (!decoder_.supports(decode_request)) {
            if (placeholder_texture_policy_.enabled) {
                return acquire_placeholder_texture(
                    request,
                    fake_image_texture_placeholder_reason::decode_failed,
                    "image decoder does not support source");
            }
            return render_image_texture_result{
                .status = render_image_texture_status::decode_failed,
                .key = key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = "image decoder does not support source",
            };
        }

        const render_image_decode_result decoded = decoder_.decode(decode_request);
        if (!decoded.ok()) {
            if (placeholder_texture_policy_.enabled) {
                return acquire_placeholder_texture(
                    request,
                    fake_image_texture_placeholder_reason::decode_failed,
                    decoded.diagnostic);
            }
            return render_image_texture_result{
                .status = render_image_texture_status::decode_failed,
                .key = key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = decoded.diagnostic,
                .decode_metadata = decoded.metadata,
                .decoder_diagnostics = decoded.decoder_diagnostics,
            };
        }

        if (decoded.image.empty()) {
            if (placeholder_texture_policy_.enabled) {
                return acquire_placeholder_texture(
                    request,
                    fake_image_texture_placeholder_reason::upload_failed,
                    "decoded image is empty");
            }
            return render_image_texture_result{
                .status = render_image_texture_status::upload_failed,
                .key = key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = "decoded image is empty",
                .decode_metadata = decoded.metadata,
                .decoder_diagnostics = decoded.decoder_diagnostics,
            };
        }

        if (!has_valid_render_decoded_image_payload(decoded.image)) {
            if (placeholder_texture_policy_.enabled) {
                return acquire_placeholder_texture(
                    request,
                    fake_image_texture_placeholder_reason::upload_failed,
                    "decoded image pixel payload size does not match dimensions and format");
            }
            return render_image_texture_result{
                .status = render_image_texture_status::upload_failed,
                .key = key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = "decoded image pixel payload size does not match dimensions and format",
                .decode_metadata = decoded.metadata,
                .decoder_diagnostics = decoded.decoder_diagnostics,
            };
        }

        const render_image_upload_readiness_snapshot upload_readiness =
            make_render_image_upload_readiness_snapshot(key, decoded.metadata, decoded.image);
        const render_image_texture_upload_result uploaded = uploader_->upload(render_image_texture_upload_request{
            .key = key,
            .sampler = request.sampler,
            .image = decoded.image,
        });
        if (!uploaded.ok()) {
            if (placeholder_texture_policy_.enabled) {
                return acquire_placeholder_texture(
                    request,
                    fake_image_texture_placeholder_reason::upload_failed,
                    uploaded.diagnostic);
            }
            return render_image_texture_result{
                .status = render_image_texture_status::upload_failed,
                .key = key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = uploaded.diagnostic,
                .decode_metadata = decoded.metadata,
                .decoder_diagnostics = decoded.decoder_diagnostics,
            };
        }

        const render_image_texture_handle handle = uploaded.texture;
        const std::size_t pixel_count = uploaded.pixel_count;
        const std::size_t pixel_byte_count = uploaded.pixel_byte_count;
        const std::size_t decoded_byte_count = uploaded.decoded_byte_count;
        const std::size_t access_sequence = next_access_sequence_++;
        cache_uploaded_texture(
            key,
            fake_cached_image_texture{
                .texture = handle,
                .decode_metadata = decoded.metadata,
                .decoder_diagnostics = decoded.decoder_diagnostics,
                .upload_readiness = upload_readiness,
                .upload_generation_id = uploaded.generation_id,
                .pixel_count = pixel_count,
                .pixel_byte_count = pixel_byte_count,
                .decoded_byte_count = decoded_byte_count,
                .first_used_sequence = access_sequence,
                .last_used_sequence = access_sequence,
                .access_count = 1,
                .residency = configured_residency_for(key),
            });
        return render_image_texture_result{
            .status = render_image_texture_status::ready,
            .key = key,
            .texture = handle,
            .cache_hit = false,
            .diagnostic = {},
            .decode_metadata = decoded.metadata,
            .decoder_diagnostics = decoded.decoder_diagnostics,
        };
    }

    void release_unused() override
    {
        ++release_unused_count_;
        clear_textures(fake_image_texture_eviction_reason::release_unused);
    }

    int release_unused_count() const
    {
        return release_unused_count_;
    }

    void set_placeholder_encoded_bytes(std::vector<std::byte> encoded_bytes)
    {
        placeholder_encoded_bytes_ = std::move(encoded_bytes);
    }

    void set_placeholder_encoded_bytes_for_source(
        render_image_cache_key source_key,
        std::vector<std::byte> encoded_bytes)
    {
        source_placeholder_encoded_bytes_.insert_or_assign(std::move(source_key), std::move(encoded_bytes));
    }

    void set_placeholder_texture_policy(fake_image_texture_placeholder_policy policy)
    {
        placeholder_texture_policy_ = std::move(policy);
    }

    const fake_image_texture_placeholder_policy& placeholder_texture_policy() const
    {
        return placeholder_texture_policy_;
    }

    void set_max_cached_pixel_count(std::size_t max_cached_pixel_count)
    {
        max_cached_pixel_count_ = max_cached_pixel_count;
        evict_to_fit(0);
    }

    std::size_t max_cached_pixel_count() const
    {
        return max_cached_pixel_count_;
    }

    std::size_t cached_texture_count() const
    {
        return textures_.size();
    }

    std::size_t cached_pixel_count() const
    {
        return cached_pixel_count_;
    }

    std::size_t cached_decoded_byte_count() const
    {
        return cached_decoded_byte_count_;
    }

    std::size_t cached_pixel_byte_count() const
    {
        return cached_pixel_byte_count_;
    }

    std::size_t eviction_count() const
    {
        return eviction_count_;
    }

    std::size_t over_capacity_texture_count() const
    {
        return over_capacity_texture_count_;
    }

    void set_texture_residency(
        const render_image_texture_key& key,
        fake_image_texture_residency residency)
    {
        set_texture_residency(
            key,
            residency,
            fake_image_texture_residency_transition_reason::policy_configured);
    }

    void pin_texture(const render_image_texture_key& key)
    {
        set_texture_residency(
            key,
            fake_image_texture_residency::pinned,
            fake_image_texture_residency_transition_reason::pinned);
    }

    void unpin_texture(const render_image_texture_key& key)
    {
        set_texture_residency(
            key,
            fake_image_texture_residency::evictable,
            fake_image_texture_residency_transition_reason::unpinned);
    }

    bool is_texture_pinned(const render_image_texture_key& key) const
    {
        const auto texture = textures_.find(key);
        if (texture != textures_.end()) {
            return texture->second.residency == fake_image_texture_residency::pinned;
        }
        return configured_residency_for(key) == fake_image_texture_residency::pinned;
    }

    fake_image_texture_cache_snapshot diagnostic_snapshot() const
    {
        fake_image_texture_cache_snapshot snapshot{
            .texture_count = textures_.size(),
            .max_cached_pixel_count = max_cached_pixel_count_,
            .cached_pixel_count = cached_pixel_count_,
            .cached_pixel_byte_count = cached_pixel_byte_count_,
            .cached_decoded_byte_count = cached_decoded_byte_count_,
            .pinned_texture_count = 0,
            .evictable_texture_count = 0,
            .pinned_pixel_count = 0,
            .evictable_pixel_count = 0,
            .eviction_count = eviction_count_,
            .over_capacity_texture_count = over_capacity_texture_count_,
            .upload_ready_texture_count = 0,
            .placeholder_fallback_texture_count = 0,
            .next_access_sequence = next_access_sequence_,
            .resident_access_count = 0,
            .capacity_eviction_count = eviction_count_,
            .release_eviction_count = release_eviction_count_,
            .invalidation_eviction_count = invalidation_eviction_count_,
            .capacity_exceeded = cached_pixel_count_ > max_cached_pixel_count_,
            .entries = {},
            .evictions = eviction_snapshots_,
            .residency_transition_count = residency_transition_snapshots_.size(),
            .residency_transitions = residency_transition_snapshots_,
            .placeholder_policy_enabled = placeholder_texture_policy_.enabled,
            .placeholder_policy = placeholder_texture_policy_,
            .placeholder_policy_texture_count = 0,
            .placeholder_policy_request_count = placeholder_policy_request_count_,
            .placeholder_policy_cache_hit_count = placeholder_policy_cache_hit_count_,
            .placeholder_policy_upload_count = placeholder_policy_upload_count_,
            .placeholder_snapshots = placeholder_snapshots_,
        };
        snapshot.entries.reserve(textures_.size());

        for (const auto& [key, entry] : textures_) {
            snapshot.resident_access_count += entry.access_count;
            if (entry.upload_readiness.upload_ready) {
                ++snapshot.upload_ready_texture_count;
            }
            if (entry.upload_readiness.placeholder_fallback) {
                ++snapshot.placeholder_fallback_texture_count;
            }
            if (entry.placeholder_texture) {
                ++snapshot.placeholder_policy_texture_count;
            }
            if (entry.residency == fake_image_texture_residency::pinned) {
                ++snapshot.pinned_texture_count;
                snapshot.pinned_pixel_count += entry.pixel_count;
            } else {
                ++snapshot.evictable_texture_count;
                snapshot.evictable_pixel_count += entry.pixel_count;
            }
            snapshot.entries.push_back(fake_image_texture_cache_entry_snapshot{
                .key = key,
                .key_diagnostic = entry.upload_readiness.key_diagnostic,
                .sampler_policy = entry.upload_readiness.sampler_policy,
                .texture = entry.texture,
                .upload_readiness = entry.upload_readiness,
                .upload_generation_id = entry.upload_generation_id,
                .first_used_sequence = entry.first_used_sequence,
                .last_used_sequence = entry.last_used_sequence,
                .access_count = entry.access_count,
                .resident_lifetime_sequence_count = resident_lifetime_sequence_count(entry),
                .pixel_count = entry.pixel_count,
                .pixel_byte_count = entry.pixel_byte_count,
                .decoded_byte_count = entry.decoded_byte_count,
                .residency = entry.residency,
                .placeholder_texture = entry.placeholder_texture,
                .placeholder_reason = entry.placeholder_reason,
                .requested_key = entry.requested_key,
                .placeholder_diagnostic = entry.placeholder_diagnostic,
            });
        }

        return snapshot;
    }

    void invalidate_source(render_image_cache_key source_key)
    {
        for (auto texture = textures_.begin(); texture != textures_.end();) {
            if (texture->first.source_key == source_key) {
                texture = erase_cached_texture(
                    texture,
                    fake_image_texture_eviction_reason::source_invalidation);
                continue;
            }
            if (texture->second.placeholder_texture
                && texture->second.requested_key.source_key == source_key) {
                texture = erase_cached_texture(
                    texture,
                    fake_image_texture_eviction_reason::source_invalidation);
                continue;
            }
            ++texture;
        }
    }

    void invalidate_texture(const render_image_texture_key& key)
    {
        const auto texture = textures_.find(key);
        if (texture == textures_.end()) {
            return;
        }

        erase_cached_texture(texture, fake_image_texture_eviction_reason::texture_invalidation);
    }

private:
    struct fake_cached_image_texture {
        render_image_texture_handle texture;
        render_image_decode_metadata decode_metadata;
        std::vector<render_image_decoder_diagnostic> decoder_diagnostics;
        render_image_upload_readiness_snapshot upload_readiness;
        std::uint64_t upload_generation_id = 0;
        std::size_t pixel_count = 0;
        std::size_t pixel_byte_count = 0;
        std::size_t decoded_byte_count = 0;
        std::size_t first_used_sequence = 0;
        std::size_t last_used_sequence = 0;
        std::size_t access_count = 0;
        fake_image_texture_residency residency = fake_image_texture_residency::evictable;
        bool placeholder_texture = false;
        fake_image_texture_placeholder_reason placeholder_reason = fake_image_texture_placeholder_reason::none;
        render_image_texture_key requested_key;
        std::string placeholder_diagnostic;
    };

    const std::vector<std::byte>& placeholder_encoded_bytes_for(const render_image_cache_key& source_key) const
    {
        const auto source_bytes = source_placeholder_encoded_bytes_.find(source_key);
        if (source_bytes != source_placeholder_encoded_bytes_.end()) {
            return source_bytes->second;
        }
        return placeholder_encoded_bytes_;
    }

    static std::string placeholder_texture_diagnostic(
        fake_image_texture_placeholder_reason reason,
        const std::string& cause)
    {
        std::string diagnostic = "using deterministic placeholder texture for "
            + fake_image_texture_placeholder_reason_name(reason);
        if (!cause.empty()) {
            diagnostic += ": " + cause;
        }
        return diagnostic;
    }

    static render_image_decode_metadata make_placeholder_decode_metadata(
        const render_image_texture_key& placeholder_key,
        fake_image_texture_placeholder_reason reason,
        const render_decoded_image& image)
    {
        render_image_decode_request request{
            .source = render_resolved_image_source{
                .original_uri = placeholder_key.source_key,
                .normalized_uri = placeholder_key.source_key,
                .kind = render_image_source_kind::asset_uri,
            },
            .encoded_bytes = {},
        };
        render_image_decode_metadata metadata = make_render_image_decode_metadata(
            "fake_image_texture_placeholder_policy",
            request,
            image);
        metadata.format_detection.placeholder_fallback = true;
        metadata.format_detection.diagnostic = "placeholder texture policy synthesized "
            + fake_image_texture_placeholder_reason_name(reason)
            + " pixels";
        return metadata;
    }

    void record_placeholder_snapshot(
        fake_image_texture_placeholder_reason reason,
        const render_image_texture_key& requested_key,
        const render_image_texture_key& placeholder_key,
        const render_image_sampler_policy& sampler,
        render_image_texture_handle texture,
        std::uint64_t upload_generation_id,
        bool cache_hit,
        std::size_t pixel_count,
        std::size_t pixel_byte_count,
        std::size_t decoded_byte_count,
        std::string diagnostic)
    {
        placeholder_snapshots_.push_back(fake_image_texture_placeholder_snapshot{
            .sequence = next_placeholder_sequence_++,
            .reason = reason,
            .requested_key = requested_key,
            .placeholder_key = placeholder_key,
            .sampler = sampler,
            .texture = texture,
            .upload_generation_id = upload_generation_id,
            .cache_hit = cache_hit,
            .width = texture.width,
            .height = texture.height,
            .pixel_count = pixel_count,
            .pixel_byte_count = pixel_byte_count,
            .decoded_byte_count = decoded_byte_count,
            .diagnostic = std::move(diagnostic),
        });
    }

    render_image_texture_result acquire_placeholder_texture(
        const render_image_texture_request& request,
        fake_image_texture_placeholder_reason reason,
        std::string cause)
    {
        ++placeholder_policy_request_count_;

        const render_image_texture_key requested_key = make_render_image_texture_key(request);
        const render_image_texture_key placeholder_key = make_fake_image_texture_placeholder_key(
            placeholder_texture_policy_,
            reason,
            requested_key);
        const std::string diagnostic = placeholder_texture_diagnostic(reason, cause);

        const auto existing = textures_.find(placeholder_key);
        if (existing != textures_.end()) {
            existing->second.last_used_sequence = next_access_sequence_++;
            ++existing->second.access_count;
            ++placeholder_policy_cache_hit_count_;
            record_placeholder_snapshot(
                reason,
                requested_key,
                placeholder_key,
                request.sampler,
                existing->second.texture,
                existing->second.upload_generation_id,
                true,
                existing->second.pixel_count,
                existing->second.pixel_byte_count,
                existing->second.decoded_byte_count,
                diagnostic);
            return render_image_texture_result{
                .status = render_image_texture_status::ready,
                .key = placeholder_key,
                .texture = existing->second.texture,
                .cache_hit = true,
                .diagnostic = diagnostic,
                .decode_metadata = existing->second.decode_metadata,
                .decoder_diagnostics = existing->second.decoder_diagnostics,
            };
        }

        const render_decoded_image placeholder_image =
            make_fake_image_texture_placeholder_decoded_image(placeholder_texture_policy_);
        const render_image_decode_metadata metadata = make_placeholder_decode_metadata(
            placeholder_key,
            reason,
            placeholder_image);
        const std::vector<render_image_decoder_diagnostic> decoder_diagnostics{
            render_image_decoder_diagnostic{
                .decoder_id = metadata.decoder_id,
                .supported = true,
                .status = render_image_decode_status::decoded,
                .diagnostic = diagnostic,
            },
        };

        if (!has_valid_render_decoded_image_payload(placeholder_image)) {
            record_placeholder_snapshot(
                reason,
                requested_key,
                placeholder_key,
                request.sampler,
                {},
                0,
                false,
                render_image_checked_pixel_count(placeholder_image),
                expected_render_decoded_image_byte_count(placeholder_image),
                placeholder_image.pixels.size(),
                "placeholder texture policy produced an invalid placeholder image");
            return render_image_texture_result{
                .status = render_image_texture_status::upload_failed,
                .key = placeholder_key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = "placeholder texture policy produced an invalid placeholder image",
                .decode_metadata = metadata,
                .decoder_diagnostics = decoder_diagnostics,
            };
        }

        const render_image_upload_readiness_snapshot upload_readiness =
            make_render_image_upload_readiness_snapshot(placeholder_key, metadata, placeholder_image);
        const render_image_texture_upload_result uploaded = uploader_->upload(render_image_texture_upload_request{
            .key = placeholder_key,
            .sampler = request.sampler,
            .image = placeholder_image,
        });
        if (!uploaded.ok()) {
            record_placeholder_snapshot(
                reason,
                requested_key,
                placeholder_key,
                request.sampler,
                uploaded.texture,
                uploaded.generation_id,
                false,
                uploaded.pixel_count,
                uploaded.pixel_byte_count,
                uploaded.decoded_byte_count,
                uploaded.diagnostic);
            return render_image_texture_result{
                .status = render_image_texture_status::upload_failed,
                .key = placeholder_key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = uploaded.diagnostic,
                .decode_metadata = metadata,
                .decoder_diagnostics = decoder_diagnostics,
            };
        }

        ++placeholder_policy_upload_count_;
        const std::size_t access_sequence = next_access_sequence_++;
        cache_uploaded_texture(
            placeholder_key,
            fake_cached_image_texture{
                .texture = uploaded.texture,
                .decode_metadata = metadata,
                .decoder_diagnostics = decoder_diagnostics,
                .upload_readiness = upload_readiness,
                .upload_generation_id = uploaded.generation_id,
                .pixel_count = uploaded.pixel_count,
                .pixel_byte_count = uploaded.pixel_byte_count,
                .decoded_byte_count = uploaded.decoded_byte_count,
                .first_used_sequence = access_sequence,
                .last_used_sequence = access_sequence,
                .access_count = 1,
                .residency = configured_residency_for(placeholder_key),
                .placeholder_texture = true,
                .placeholder_reason = reason,
                .requested_key = requested_key,
                .placeholder_diagnostic = diagnostic,
            });
        record_placeholder_snapshot(
            reason,
            requested_key,
            placeholder_key,
            request.sampler,
            uploaded.texture,
            uploaded.generation_id,
            false,
            uploaded.pixel_count,
            uploaded.pixel_byte_count,
            uploaded.decoded_byte_count,
            diagnostic);

        return render_image_texture_result{
            .status = render_image_texture_status::ready,
            .key = placeholder_key,
            .texture = uploaded.texture,
            .cache_hit = false,
            .diagnostic = diagnostic,
            .decode_metadata = metadata,
            .decoder_diagnostics = decoder_diagnostics,
        };
    }

    static std::size_t resident_lifetime_sequence_count(const fake_cached_image_texture& entry)
    {
        if (entry.first_used_sequence == 0 || entry.last_used_sequence < entry.first_used_sequence) {
            return 0;
        }
        return entry.last_used_sequence - entry.first_used_sequence + 1;
    }

    void clear_textures(fake_image_texture_eviction_reason reason)
    {
        for (auto texture = textures_.begin(); texture != textures_.end();) {
            texture = erase_cached_texture(texture, reason);
        }
        cached_pixel_count_ = 0;
        cached_pixel_byte_count_ = 0;
        cached_decoded_byte_count_ = 0;
    }

    void subtract_cached_entry(const fake_cached_image_texture& entry)
    {
        cached_pixel_count_ = entry.pixel_count <= cached_pixel_count_
            ? cached_pixel_count_ - entry.pixel_count
            : 0;
        cached_pixel_byte_count_ = entry.pixel_byte_count <= cached_pixel_byte_count_
            ? cached_pixel_byte_count_ - entry.pixel_byte_count
            : 0;
        cached_decoded_byte_count_ = entry.decoded_byte_count <= cached_decoded_byte_count_
            ? cached_decoded_byte_count_ - entry.decoded_byte_count
            : 0;
    }

    bool has_capacity_for(std::size_t incoming_pixel_count) const
    {
        return incoming_pixel_count <= max_cached_pixel_count_
            && cached_pixel_count_ <= max_cached_pixel_count_ - incoming_pixel_count;
    }

    fake_image_texture_residency configured_residency_for(const render_image_texture_key& key) const
    {
        const auto residency = texture_residency_.find(key);
        return residency == texture_residency_.end() ? fake_image_texture_residency::evictable : residency->second;
    }

    void set_texture_residency(
        const render_image_texture_key& key,
        fake_image_texture_residency residency,
        fake_image_texture_residency_transition_reason reason)
    {
        const auto texture = textures_.find(key);
        const bool resident = texture != textures_.end();
        const fake_image_texture_residency previous_residency = resident
            ? texture->second.residency
            : configured_residency_for(key);

        texture_residency_.insert_or_assign(key, residency);
        if (resident) {
            texture->second.residency = residency;
            record_residency_transition(key, &texture->second, previous_residency, residency, reason, true);
        } else {
            record_residency_transition(key, nullptr, previous_residency, residency, reason, false);
        }
        evict_to_fit(0);
    }

    void cache_uploaded_texture(render_image_texture_key key, fake_cached_image_texture texture)
    {
        if (texture.residency == fake_image_texture_residency::evictable
            && texture.pixel_count > max_cached_pixel_count_) {
            ++over_capacity_texture_count_;
            return;
        }

        evict_to_fit(texture.pixel_count);
        if (texture.residency == fake_image_texture_residency::evictable && !has_capacity_for(texture.pixel_count)) {
            ++over_capacity_texture_count_;
            return;
        }

        cached_pixel_count_ += texture.pixel_count;
        cached_pixel_byte_count_ += texture.pixel_byte_count;
        cached_decoded_byte_count_ += texture.decoded_byte_count;
        const fake_image_texture_residency inserted_residency = texture.residency;
        auto [inserted, did_insert] = textures_.emplace(std::move(key), std::move(texture));
        if (did_insert) {
            record_residency_transition(
                inserted->first,
                &inserted->second,
                inserted_residency,
                inserted_residency,
                fake_image_texture_residency_transition_reason::cache_inserted,
                true);
        }
    }

    std::map<render_image_texture_key, fake_cached_image_texture, render_image_texture_key_less>::iterator
    erase_cached_texture(
        std::map<render_image_texture_key, fake_cached_image_texture, render_image_texture_key_less>::iterator texture,
        fake_image_texture_eviction_reason reason)
    {
        const render_image_texture_key key = texture->first;
        const fake_cached_image_texture entry = texture->second;
        subtract_cached_entry(texture->second);
        auto next = texture;
        ++next;
        textures_.erase(texture);

        record_eviction_snapshot(key, entry, reason);
        if (reason == fake_image_texture_eviction_reason::capacity) {
            ++eviction_count_;
        } else if (reason == fake_image_texture_eviction_reason::release_unused) {
            ++release_eviction_count_;
        } else {
            ++invalidation_eviction_count_;
        }
        return next;
    }

    void record_eviction_snapshot(
        const render_image_texture_key& key,
        const fake_cached_image_texture& entry,
        fake_image_texture_eviction_reason reason)
    {
        eviction_snapshots_.push_back(fake_image_texture_eviction_snapshot{
            .sequence = next_eviction_sequence_++,
            .reason = reason,
            .key = key,
            .texture = entry.texture,
            .upload_generation_id = entry.upload_generation_id,
            .first_used_sequence = entry.first_used_sequence,
            .last_used_sequence = entry.last_used_sequence,
            .access_count = entry.access_count,
            .resident_lifetime_sequence_count = resident_lifetime_sequence_count(entry),
            .pixel_count = entry.pixel_count,
            .pixel_byte_count = entry.pixel_byte_count,
            .decoded_byte_count = entry.decoded_byte_count,
            .residency = entry.residency,
            .diagnostic = "image texture evicted by " + fake_image_texture_eviction_reason_name(reason),
        });
    }

    void record_residency_transition(
        const render_image_texture_key& key,
        const fake_cached_image_texture* entry,
        fake_image_texture_residency previous_residency,
        fake_image_texture_residency new_residency,
        fake_image_texture_residency_transition_reason reason,
        bool resident)
    {
        residency_transition_snapshots_.push_back(fake_image_texture_residency_transition_snapshot{
            .sequence = next_residency_transition_sequence_++,
            .reason = reason,
            .key = key,
            .texture = entry == nullptr ? render_image_texture_handle{} : entry->texture,
            .upload_generation_id = entry == nullptr ? 0 : entry->upload_generation_id,
            .previous_residency = previous_residency,
            .new_residency = new_residency,
            .resident = resident,
            .changed = previous_residency != new_residency,
            .last_used_sequence = entry == nullptr ? 0 : entry->last_used_sequence,
            .pixel_count = entry == nullptr ? 0 : entry->pixel_count,
            .pixel_byte_count = entry == nullptr ? 0 : entry->pixel_byte_count,
            .decoded_byte_count = entry == nullptr ? 0 : entry->decoded_byte_count,
            .diagnostic = "image texture residency transitioned by "
                + fake_image_texture_residency_transition_reason_name(reason),
        });
    }

    void evict_to_fit(std::size_t incoming_pixel_count)
    {
        while (!textures_.empty() && !has_capacity_for(incoming_pixel_count)) {
            auto least_recently_used = textures_.end();
            for (auto candidate = textures_.begin(); candidate != textures_.end(); ++candidate) {
                if (candidate->second.residency == fake_image_texture_residency::pinned) {
                    continue;
                }
                if (least_recently_used == textures_.end()
                    || candidate->second.last_used_sequence < least_recently_used->second.last_used_sequence) {
                    least_recently_used = candidate;
                }
            }

            if (least_recently_used == textures_.end()) {
                return;
            }

            erase_cached_texture(least_recently_used, fake_image_texture_eviction_reason::capacity);
        }
    }

    const image_decoder_interface& decoder_;
    fake_image_texture_uploader internal_uploader_;
    image_texture_uploader_interface* uploader_ = nullptr;
    std::size_t next_access_sequence_ = 1;
    int release_unused_count_ = 0;
    std::size_t max_cached_pixel_count_ = std::numeric_limits<std::size_t>::max();
    std::size_t cached_pixel_count_ = 0;
    std::size_t cached_pixel_byte_count_ = 0;
    std::size_t cached_decoded_byte_count_ = 0;
    std::size_t eviction_count_ = 0;
    std::size_t release_eviction_count_ = 0;
    std::size_t invalidation_eviction_count_ = 0;
    std::size_t over_capacity_texture_count_ = 0;
    std::size_t next_eviction_sequence_ = 1;
    std::size_t next_residency_transition_sequence_ = 1;
    std::size_t next_placeholder_sequence_ = 1;
    std::size_t placeholder_policy_request_count_ = 0;
    std::size_t placeholder_policy_cache_hit_count_ = 0;
    std::size_t placeholder_policy_upload_count_ = 0;
    fake_image_texture_placeholder_policy placeholder_texture_policy_;
    std::vector<std::byte> placeholder_encoded_bytes_ = {std::byte{0x01}};
    std::map<render_image_cache_key, std::vector<std::byte>> source_placeholder_encoded_bytes_;
    std::map<render_image_texture_key, fake_image_texture_residency, render_image_texture_key_less> texture_residency_;
    std::map<render_image_texture_key, fake_cached_image_texture, render_image_texture_key_less> textures_;
    std::vector<fake_image_texture_eviction_snapshot> eviction_snapshots_;
    std::vector<fake_image_texture_residency_transition_snapshot> residency_transition_snapshots_;
    std::vector<fake_image_texture_placeholder_snapshot> placeholder_snapshots_;
};

} // namespace quiz_vulkan::render
