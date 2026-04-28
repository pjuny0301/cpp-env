#include "render/image/image_decoder.h"
#include "render/image/image_resolver.h"
#include "render/image/image_texture_cache.h"
#include "render/render_draw_list.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
    }
    assert((condition) && message);
}

void test_sampler_defaults_are_appended_to_render_image_ref()
{
    using namespace quiz_vulkan::render;

    render_image_ref image;
    image.uri = "assets/card.fake";
    image.alt_text = "card";
    image.aspect_ratio = 1.6f;
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

    const render_image_resolve_result windows_local = resolver.resolve(
        render_image_resolve_request{.uri = "C:\\quiz\\cards\\front.fake"});
    require(windows_local.ok(), "windows local path resolves");
    require(windows_local.source.normalized_uri == "C:/quiz/cards/front.fake", "resolver preserves drive paths");
    require(windows_local.source.kind == render_image_source_kind::local_path, "resolver classifies drive paths");

    const render_image_resolve_result asset = resolver.resolve(
        render_image_resolve_request{.uri = "  ASSET://./Deck\\cards\\front.fake  "});
    require(asset.ok(), "asset uri resolves");
    require(asset.source.normalized_uri == "asset://Deck/cards/front.fake", "resolver canonicalizes asset uri");
    require(asset.source.kind == render_image_source_kind::asset_uri, "resolver classifies asset uris");
    require(!asset.source.is_remote(), "asset uri is not remote");

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

    fake_image_decoder decoder;
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
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

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
    require(cache.release_unused_count() == 1, "cache release hook is callable");
}

void test_texture_cache_reports_explicit_failures()
{
    using namespace quiz_vulkan::render;

    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_texture_result missing = cache.acquire_texture(
        render_image_texture_request{.source = {}, .sampler = render_image_sampler_policy{}});
    require(!missing.ok(), "empty source does not create a texture");
    require(missing.status == render_image_texture_status::missing_source, "empty source reports missing source");
    require(!missing.diagnostic.empty(), "empty source includes diagnostic");

    render_resolved_image_source control_character_source{
        .original_uri = "bad\n.fake",
        .normalized_uri = std::string("bad\n.fake"),
        .kind = render_image_source_kind::local_path,
    };
    const render_image_texture_result invalid_key = cache.acquire_texture(
        render_image_texture_request{.source = control_character_source, .sampler = render_image_sampler_policy{}});
    require(!invalid_key.ok(), "control character cache key does not create a texture");
    require(invalid_key.status == render_image_texture_status::missing_source, "invalid cache key reports missing source");

    const normalizing_image_resolver resolver;
    const render_resolved_image_source remote = resolver.resolve(
        render_image_resolve_request{.uri = "https://example.test/card.fake"})
                                                    .source;
    const render_image_texture_result remote_result = cache.acquire_texture(
        render_image_texture_request{.source = remote, .sampler = render_image_sampler_policy{}});
    require(!remote_result.ok(), "fake texture cache does not fetch remote image bytes");
    require(remote_result.status == render_image_texture_status::missing_source, "remote image reports missing source");
    require(decoder.support_requests.empty(), "early source failures do not reach the decoder");
}

} // namespace

int main()
{
    test_sampler_defaults_are_appended_to_render_image_ref();
    test_resolver_normalizes_without_fetching();
    test_decoder_interface_shape();
    test_texture_cache_reuses_matching_key_and_misses_on_sampler_change();
    test_texture_cache_reports_explicit_failures();
    return 0;
}
