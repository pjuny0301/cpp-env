#include "render/image/image_decoder.h"
#include "render/image/image_resolver.h"
#include "render/image/image_texture_cache.h"
#include "render/render_draw_list.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

class fake_decoder final : public quiz_vulkan::render::image_decoder_interface {
public:
    bool supports(const quiz_vulkan::render::render_image_decode_request& request) const override
    {
        support_requests.push_back(request);
        return request.source.normalized_uri.ends_with(".fake");
    }

    quiz_vulkan::render::render_image_decode_result decode(
        const quiz_vulkan::render::render_image_decode_request& request) const override
    {
        decode_requests.push_back(request);
        if (request.encoded_bytes.empty()) {
            return quiz_vulkan::render::render_image_decode_result{
                .status = quiz_vulkan::render::render_image_decode_status::empty_input,
                .diagnostic = "fake decoder requires bytes",
            };
        }

        return quiz_vulkan::render::render_image_decode_result{
            .status = quiz_vulkan::render::render_image_decode_status::decoded,
            .image = quiz_vulkan::render::render_decoded_image{
                .width = 2,
                .height = 1,
                .pixel_format = quiz_vulkan::render::render_image_pixel_format::rgba8_srgb,
                .pixels = {
                    std::byte{0xff},
                    std::byte{0x00},
                    std::byte{0x00},
                    std::byte{0xff},
                    std::byte{0x00},
                    std::byte{0xff},
                    std::byte{0x00},
                    std::byte{0xff},
                },
            },
        };
    }

    mutable std::vector<quiz_vulkan::render::render_image_decode_request> support_requests;
    mutable std::vector<quiz_vulkan::render::render_image_decode_request> decode_requests;
};

class fake_texture_cache final : public quiz_vulkan::render::image_texture_cache_interface {
public:
    explicit fake_texture_cache(const fake_decoder& decoder)
        : decoder_(decoder)
    {
    }

    quiz_vulkan::render::render_image_texture_result acquire_texture(
        const quiz_vulkan::render::render_image_texture_request& request) override
    {
        const quiz_vulkan::render::render_image_texture_key key{
            .source_key = request.source.cache_key(),
            .sampler = request.sampler,
        };

        const auto existing = textures_.find(key);
        if (existing != textures_.end()) {
            return quiz_vulkan::render::render_image_texture_result{
                .status = quiz_vulkan::render::render_image_texture_status::ready,
                .key = key,
                .texture = existing->second,
                .cache_hit = true,
            };
        }

        quiz_vulkan::render::render_image_decode_request decode_request{
            .source = request.source,
            .encoded_bytes = {std::byte{0x01}},
        };
        if (!decoder_.supports(decode_request)) {
            return quiz_vulkan::render::render_image_texture_result{
                .status = quiz_vulkan::render::render_image_texture_status::decode_failed,
                .key = key,
                .diagnostic = "unsupported fake image",
            };
        }

        const quiz_vulkan::render::render_image_decode_result decoded = decoder_.decode(decode_request);
        if (!decoded.ok()) {
            return quiz_vulkan::render::render_image_texture_result{
                .status = quiz_vulkan::render::render_image_texture_status::decode_failed,
                .key = key,
                .diagnostic = decoded.diagnostic,
            };
        }

        const quiz_vulkan::render::render_image_texture_handle handle{
            .id = next_id_++,
            .revision = 1,
            .width = decoded.image.width,
            .height = decoded.image.height,
        };
        textures_.emplace(key, handle);
        return quiz_vulkan::render::render_image_texture_result{
            .status = quiz_vulkan::render::render_image_texture_status::ready,
            .key = key,
            .texture = handle,
            .cache_hit = false,
        };
    }

    void release_unused() override
    {
        ++release_unused_count;
    }

    int release_unused_count = 0;

private:
    struct key_less {
        static std::tuple<
            quiz_vulkan::render::render_image_filter,
            quiz_vulkan::render::render_image_filter,
            quiz_vulkan::render::render_image_mipmap_mode,
            quiz_vulkan::render::render_image_wrap_mode,
            quiz_vulkan::render::render_image_wrap_mode>
        sampler_tuple(const quiz_vulkan::render::render_image_sampler_policy& sampler)
        {
            return {
                sampler.min_filter,
                sampler.mag_filter,
                sampler.mipmap_mode,
                sampler.wrap_u,
                sampler.wrap_v,
            };
        }

        bool operator()(
            const quiz_vulkan::render::render_image_texture_key& left,
            const quiz_vulkan::render::render_image_texture_key& right) const
        {
            if (left.source_key != right.source_key) {
                return left.source_key < right.source_key;
            }
            return sampler_tuple(left.sampler) < sampler_tuple(right.sampler);
        }
    };

    const fake_decoder& decoder_;
    quiz_vulkan::render::render_image_texture_id next_id_ = 1;
    std::map<
        quiz_vulkan::render::render_image_texture_key,
        quiz_vulkan::render::render_image_texture_handle,
        key_less>
        textures_;
};

void test_sampler_defaults_are_appended_to_render_image_ref()
{
    using namespace quiz_vulkan::render;

    const render_image_ref image{.uri = "assets/card.fake", .alt_text = "card", .aspect_ratio = 1.6f};
    require(image.sampler.min_filter == render_image_filter::linear, "default min filter is linear");
    require(image.sampler.mag_filter == render_image_filter::linear, "default mag filter is linear");
    require(image.sampler.mipmap_mode == render_image_mipmap_mode::none, "default mipmap mode is none");
    require(image.sampler.wrap_u == render_image_wrap_mode::clamp_to_edge, "default wrap_u clamps");
    require(image.sampler.wrap_v == render_image_wrap_mode::clamp_to_edge, "default wrap_v clamps");
}

void test_resolver_normalizes_without_fetching()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_image_resolve_result local = resolver.resolve(
        render_image_resolve_request{.uri = "  ./textures\\cards\\front.fake  "});
    require(local.ok(), "local path resolves");
    require(local.source.original_uri == "  ./textures\\cards\\front.fake  ", "resolver preserves original uri");
    require(local.source.normalized_uri == "textures/cards/front.fake", "resolver trims and normalizes local path");
    require(local.source.kind == render_image_source_kind::local_path, "resolver classifies local paths");
    require(!local.source.is_remote(), "local path is not remote");

    const render_image_resolve_result remote = resolver.resolve(
        render_image_resolve_request{.uri = "HTTPS://example.test/image.fake"});
    require(remote.ok(), "remote uri resolves as a source contract");
    require(remote.source.normalized_uri == "https://example.test/image.fake", "resolver lowercases scheme");
    require(remote.source.kind == render_image_source_kind::https_uri, "resolver classifies https uris");
    require(remote.source.is_remote(), "https uri is remote");

    const render_image_resolve_result empty = resolver.resolve(render_image_resolve_request{.uri = "  "});
    require(!empty.ok(), "empty uri does not resolve");
    require(empty.status == render_image_resolve_status::empty_uri, "empty uri reports empty status");
}

void test_decoder_interface_shape()
{
    using namespace quiz_vulkan::render;

    fake_decoder decoder;
    const render_image_decode_request request{
        .source = render_resolved_image_source{
            .original_uri = "asset://card.fake",
            .normalized_uri = "asset://card.fake",
            .kind = render_image_source_kind::asset_uri,
        },
        .encoded_bytes = {std::byte{0x01}, std::byte{0x02}},
    };

    require(decoder.supports(request), "fake decoder support check uses request shape");
    const render_image_decode_result decoded = decoder.decode(request);
    require(decoded.ok(), "fake decoder returns decoded result");
    require(decoded.image.width == 2, "decoded image carries width");
    require(decoded.image.height == 1, "decoded image carries height");
    require(decoded.image.pixels.size() == 8, "decoded image carries rgba bytes");
    require(decoder.support_requests.size() == 1, "support request was recorded");
    require(decoder.decode_requests.size() == 1, "decode request was recorded");
}

void test_texture_cache_reuses_matching_key_and_misses_on_sampler_change()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.fake"})
                                                    .source;
    fake_decoder decoder;
    fake_texture_cache cache(decoder);

    const render_image_texture_request first_request{.source = source, .sampler = render_image_sampler_policy{}};
    const render_image_texture_result first = cache.acquire_texture(first_request);
    require(first.ok(), "first cache request creates a texture");
    require(!first.cache_hit, "first request is a cache miss");
    require(first.texture.id == 1, "first fake texture id is stable");

    const render_image_texture_result second = cache.acquire_texture(first_request);
    require(second.ok(), "second cache request returns a texture");
    require(second.cache_hit, "second matching request is a cache hit");
    require(second.texture.id == first.texture.id, "cache hit reuses the same fake texture");

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    const render_image_texture_result sampler_miss = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = nearest_sampler});
    require(sampler_miss.ok(), "sampler variant creates a texture");
    require(!sampler_miss.cache_hit, "different sampler policy is a cache miss");
    require(sampler_miss.texture.id != first.texture.id, "sampler variant receives a different texture");

    cache.release_unused();
    require(cache.release_unused_count == 1, "cache release hook is callable");
}

} // namespace

int main()
{
    test_sampler_defaults_are_appended_to_render_image_ref();
    test_resolver_normalizes_without_fetching();
    test_decoder_interface_shape();
    test_texture_cache_reuses_matching_key_and_misses_on_sampler_change();
    return 0;
}
