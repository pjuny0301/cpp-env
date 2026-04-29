#pragma once

#include "render/image/image_decoder.h"
#include "render/image/image_types.h"

#include <cctype>
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

class fake_image_texture_cache final : public image_texture_cache_interface {
public:
    explicit fake_image_texture_cache(const image_decoder_interface& decoder)
        : decoder_(decoder)
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

        const render_image_texture_handle handle{
            .id = next_id_++,
            .revision = 1,
            .width = decoded.image.width,
            .height = decoded.image.height,
        };
        const std::size_t pixel_count = decoded.image.width * decoded.image.height;
        const std::size_t decoded_byte_count = decoded.image.pixels.size();
        if (pixel_count <= max_cached_pixel_count_) {
            evict_to_fit(pixel_count);
            cached_pixel_count_ += pixel_count;
            cached_decoded_byte_count_ += decoded_byte_count;
            textures_.emplace(
                key,
                fake_cached_image_texture{
                    .texture = handle,
                    .decode_metadata = decoded.metadata,
                    .decoder_diagnostics = decoded.decoder_diagnostics,
                    .pixel_count = pixel_count,
                    .decoded_byte_count = decoded_byte_count,
                    .last_used_sequence = next_access_sequence_++,
                });
        }
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
        std::size_t decoded_byte_count = 0;
        std::size_t last_used_sequence = 0;
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
        cached_decoded_byte_count_ = 0;
    }

    void subtract_cached_entry(const fake_cached_image_texture& entry)
    {
        cached_pixel_count_ = entry.pixel_count <= cached_pixel_count_
            ? cached_pixel_count_ - entry.pixel_count
            : 0;
        cached_decoded_byte_count_ = entry.decoded_byte_count <= cached_decoded_byte_count_
            ? cached_decoded_byte_count_ - entry.decoded_byte_count
            : 0;
    }

    void evict_to_fit(std::size_t incoming_pixel_count)
    {
        if (incoming_pixel_count > max_cached_pixel_count_) {
            clear_textures();
            return;
        }

        while (!textures_.empty() && cached_pixel_count_ > max_cached_pixel_count_ - incoming_pixel_count) {
            auto least_recently_used = textures_.begin();
            for (auto candidate = textures_.begin(); candidate != textures_.end(); ++candidate) {
                if (candidate->second.last_used_sequence < least_recently_used->second.last_used_sequence) {
                    least_recently_used = candidate;
                }
            }

            subtract_cached_entry(least_recently_used->second);
            textures_.erase(least_recently_used);
        }
    }

    const image_decoder_interface& decoder_;
    render_image_texture_id next_id_ = 1;
    std::size_t next_access_sequence_ = 1;
    int release_unused_count_ = 0;
    std::size_t max_cached_pixel_count_ = std::numeric_limits<std::size_t>::max();
    std::size_t cached_pixel_count_ = 0;
    std::size_t cached_decoded_byte_count_ = 0;
    std::vector<std::byte> placeholder_encoded_bytes_ = {std::byte{0x01}};
    std::map<render_image_cache_key, std::vector<std::byte>> source_placeholder_encoded_bytes_;
    std::map<render_image_texture_key, fake_cached_image_texture, render_image_texture_key_less> textures_;
};

} // namespace quiz_vulkan::render
