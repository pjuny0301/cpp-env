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

struct fake_image_texture_cache_entry_snapshot {
    render_image_texture_key key;
    render_image_texture_handle texture;
    std::size_t pixel_count = 0;
    std::size_t pixel_byte_count = 0;
    std::size_t decoded_byte_count = 0;
    std::size_t last_used_sequence = 0;
    fake_image_texture_residency residency = fake_image_texture_residency::evictable;
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
    bool capacity_exceeded = false;
    std::vector<fake_image_texture_cache_entry_snapshot> entries;
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
};

class fake_image_texture_uploader final : public image_texture_uploader_interface {
public:
    render_image_texture_upload_result upload(const render_image_texture_upload_request& request) override
    {
        upload_requests.push_back(request);

        const fake_image_texture_upload_generation_id generation_id = next_generation_id_++;
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
            });
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
            });
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
            });
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
            });
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
        });
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
            });
        }
        return snapshot;
    }

    std::vector<render_image_texture_upload_request> upload_requests;
    std::vector<fake_image_texture_upload_request_snapshot> upload_request_snapshots;
    std::vector<fake_image_texture_upload_result_snapshot> upload_result_snapshots;
    std::vector<render_image_texture_upload_result> upload_results;

private:
    render_image_texture_upload_result record_result(render_image_texture_upload_result result)
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
        });
        upload_results.push_back(result);
        return result;
    }

    render_image_texture_id next_id_ = 1;
    fake_image_texture_upload_generation_id next_generation_id_ = 1;
    std::size_t failed_upload_count_ = 0;
    std::size_t uploaded_pixel_count_ = 0;
    std::size_t uploaded_pixel_byte_count_ = 0;
    std::size_t uploaded_decoded_byte_count_ = 0;
    std::size_t staged_byte_count_ = 0;
    std::size_t attempted_staging_byte_count_ = 0;
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
            return render_image_texture_result{
                .status = render_image_texture_status::missing_source,
                .key = key,
                .texture = {},
                .cache_hit = false,
                .diagnostic = "image source kind is unsupported",
            };
        }

        if (request.source.is_remote()) {
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

        const render_image_texture_upload_result uploaded = uploader_->upload(render_image_texture_upload_request{
            .key = key,
            .sampler = request.sampler,
            .image = decoded.image,
        });
        if (!uploaded.ok()) {
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
        cache_uploaded_texture(
            key,
            fake_cached_image_texture{
                .texture = handle,
                .decode_metadata = decoded.metadata,
                .decoder_diagnostics = decoded.decoder_diagnostics,
                .pixel_count = pixel_count,
                .pixel_byte_count = pixel_byte_count,
                .decoded_byte_count = decoded_byte_count,
                .last_used_sequence = next_access_sequence_++,
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
        clear_textures();
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
        texture_residency_.insert_or_assign(key, residency);
        const auto texture = textures_.find(key);
        if (texture != textures_.end()) {
            texture->second.residency = residency;
        }
        evict_to_fit(0);
    }

    void pin_texture(const render_image_texture_key& key)
    {
        set_texture_residency(key, fake_image_texture_residency::pinned);
    }

    void unpin_texture(const render_image_texture_key& key)
    {
        set_texture_residency(key, fake_image_texture_residency::evictable);
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
            .capacity_exceeded = cached_pixel_count_ > max_cached_pixel_count_,
            .entries = {},
        };
        snapshot.entries.reserve(textures_.size());

        for (const auto& [key, entry] : textures_) {
            if (entry.residency == fake_image_texture_residency::pinned) {
                ++snapshot.pinned_texture_count;
                snapshot.pinned_pixel_count += entry.pixel_count;
            } else {
                ++snapshot.evictable_texture_count;
                snapshot.evictable_pixel_count += entry.pixel_count;
            }
            snapshot.entries.push_back(fake_image_texture_cache_entry_snapshot{
                .key = key,
                .texture = entry.texture,
                .pixel_count = entry.pixel_count,
                .pixel_byte_count = entry.pixel_byte_count,
                .decoded_byte_count = entry.decoded_byte_count,
                .last_used_sequence = entry.last_used_sequence,
                .residency = entry.residency,
            });
        }

        return snapshot;
    }

    void invalidate_source(render_image_cache_key source_key)
    {
        for (auto texture = textures_.begin(); texture != textures_.end();) {
            if (texture->first.source_key == source_key) {
                subtract_cached_entry(texture->second);
                texture = textures_.erase(texture);
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

        subtract_cached_entry(texture->second);
        textures_.erase(texture);
    }

private:
    struct fake_cached_image_texture {
        render_image_texture_handle texture;
        render_image_decode_metadata decode_metadata;
        std::vector<render_image_decoder_diagnostic> decoder_diagnostics;
        std::size_t pixel_count = 0;
        std::size_t pixel_byte_count = 0;
        std::size_t decoded_byte_count = 0;
        std::size_t last_used_sequence = 0;
        fake_image_texture_residency residency = fake_image_texture_residency::evictable;
    };

    const std::vector<std::byte>& placeholder_encoded_bytes_for(const render_image_cache_key& source_key) const
    {
        const auto source_bytes = source_placeholder_encoded_bytes_.find(source_key);
        if (source_bytes != source_placeholder_encoded_bytes_.end()) {
            return source_bytes->second;
        }
        return placeholder_encoded_bytes_;
    }

    void clear_textures()
    {
        textures_.clear();
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
        textures_.emplace(std::move(key), std::move(texture));
    }

    void erase_cached_texture(
        std::map<render_image_texture_key, fake_cached_image_texture, render_image_texture_key_less>::iterator texture,
        bool count_eviction)
    {
        subtract_cached_entry(texture->second);
        textures_.erase(texture);
        if (count_eviction) {
            ++eviction_count_;
        }
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

            erase_cached_texture(least_recently_used, true);
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
    std::size_t over_capacity_texture_count_ = 0;
    std::vector<std::byte> placeholder_encoded_bytes_ = {std::byte{0x01}};
    std::map<render_image_cache_key, std::vector<std::byte>> source_placeholder_encoded_bytes_;
    std::map<render_image_texture_key, fake_image_texture_residency, render_image_texture_key_less> texture_residency_;
    std::map<render_image_texture_key, fake_cached_image_texture, render_image_texture_key_less> textures_;
};

} // namespace quiz_vulkan::render
