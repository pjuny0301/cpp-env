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

class malformed_payload_decoder final : public quiz_vulkan::render::image_decoder_interface {
public:
    bool supports(const quiz_vulkan::render::render_image_decode_request&) const override
    {
        ++support_request_count;
        return true;
    }

    quiz_vulkan::render::render_image_decode_result decode(
        const quiz_vulkan::render::render_image_decode_request&) const override
    {
        ++decode_request_count;
        return quiz_vulkan::render::render_image_decode_result{
            .status = quiz_vulkan::render::render_image_decode_status::decoded,
            .image = quiz_vulkan::render::render_decoded_image{
                .width = 2,
                .height = 2,
                .pixel_format = quiz_vulkan::render::render_image_pixel_format::rgba8_srgb,
                .pixels = {
                    std::byte{0xff},
                    std::byte{0x00},
                    std::byte{0x00},
                    std::byte{0xff},
                },
            },
            .diagnostic = {},
        };
    }

    mutable int support_request_count = 0;
    mutable int decode_request_count = 0;
};

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
    require(is_valid_render_image_sampler_policy(image.sampler), "default sampler policy is valid");
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

    const render_image_resolve_result unsupported = resolver.resolve(
        render_image_resolve_request{.uri = "ftp://example.test/image.fake"});
    require(!unsupported.ok(), "unsupported uri scheme does not resolve");
    require(
        unsupported.status == render_image_resolve_status::unsupported_scheme,
        "unsupported uri reports unsupported scheme");
    require(!unsupported.diagnostic.empty(), "unsupported uri includes diagnostic");
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
    require(expected_render_decoded_image_byte_count(decoded.image) == 8, "decoded image byte count is derived");
    require(has_valid_render_decoded_image_payload(decoded.image), "decoded image payload matches dimensions");
    require(decoder.support_requests.size() == 1, "support request was recorded");
    require(decoder.decode_requests.size() == 1, "decode request was recorded");
}

void test_decoder_reports_explicit_failures()
{
    using namespace quiz_vulkan::render;

    fake_image_decoder decoder;
    const render_image_decode_request unsupported_request{
        .source = render_resolved_image_source{
            .original_uri = "asset://card.png",
            .normalized_uri = "asset://card.png",
            .kind = render_image_source_kind::asset_uri,
        },
        .encoded_bytes = {std::byte{0x01}},
    };
    require(!decoder.supports(unsupported_request), "fake decoder rejects unsupported extension");

    const render_image_decode_result unsupported = decoder.decode(unsupported_request);
    require(!unsupported.ok(), "unsupported decode does not return an image");
    require(
        unsupported.status == render_image_decode_status::unsupported_format,
        "unsupported decode reports unsupported format");
    require(!unsupported.diagnostic.empty(), "unsupported decode includes diagnostic");

    const render_image_decode_request empty_request{
        .source = render_resolved_image_source{
            .original_uri = "asset://card.fake",
            .normalized_uri = "asset://card.fake",
            .kind = render_image_source_kind::asset_uri,
        },
        .encoded_bytes = {},
    };
    require(decoder.supports(empty_request), "fake decoder support is independent of byte availability");

    const render_image_decode_result empty = decoder.decode(empty_request);
    require(!empty.ok(), "empty decode does not return an image");
    require(empty.status == render_image_decode_status::empty_input, "empty decode reports empty input");
    require(!empty.diagnostic.empty(), "empty decode includes diagnostic");

    require(decoder.support_requests.size() == 2, "failure support requests were recorded");
    require(decoder.decode_requests.size() == 2, "failure decode requests were recorded");
}

void test_sampler_policy_validation_rejects_unknown_enum_values()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy sampler;
    require(is_valid_render_image_sampler_policy(sampler), "baseline sampler policy is valid");

    sampler.min_filter = static_cast<render_image_filter>(99);
    require(!is_valid_render_image_sampler_policy(sampler), "unknown min filter is invalid");
    sampler.min_filter = render_image_filter::linear;

    sampler.mag_filter = static_cast<render_image_filter>(99);
    require(!is_valid_render_image_sampler_policy(sampler), "unknown mag filter is invalid");
    sampler.mag_filter = render_image_filter::linear;

    sampler.mipmap_mode = static_cast<render_image_mipmap_mode>(99);
    require(!is_valid_render_image_sampler_policy(sampler), "unknown mipmap mode is invalid");
    sampler.mipmap_mode = render_image_mipmap_mode::none;

    sampler.wrap_u = static_cast<render_image_wrap_mode>(99);
    require(!is_valid_render_image_sampler_policy(sampler), "unknown wrap_u mode is invalid");
    sampler.wrap_u = render_image_wrap_mode::clamp_to_edge;

    sampler.wrap_v = static_cast<render_image_wrap_mode>(99);
    require(!is_valid_render_image_sampler_policy(sampler), "unknown wrap_v mode is invalid");
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

void test_texture_cache_keys_include_all_sampler_policy_fields()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/sampler-fields.fake"})
                                                    .source;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_sampler_policy base_sampler;
    const render_image_texture_result base = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = base_sampler});
    require(base.ok(), "base sampler creates a texture");

    auto require_sampler_miss = [&](render_image_sampler_policy sampler, const char* message) {
        const render_image_texture_result result = cache.acquire_texture(
            render_image_texture_request{.source = source, .sampler = sampler});
        require(result.ok(), message);
        require(!result.cache_hit, "sampler field variant is a cache miss");
        require(result.texture.id != base.texture.id, "sampler field variant receives a distinct texture id");
        require(result.key.source_key == base.key.source_key, "sampler field variant preserves source key");
    };

    render_image_sampler_policy mag_filter_sampler = base_sampler;
    mag_filter_sampler.mag_filter = render_image_filter::nearest;
    require_sampler_miss(mag_filter_sampler, "mag filter variant creates a texture");

    render_image_sampler_policy mipmap_sampler = base_sampler;
    mipmap_sampler.mipmap_mode = render_image_mipmap_mode::nearest;
    require_sampler_miss(mipmap_sampler, "mipmap variant creates a texture");

    render_image_sampler_policy wrap_u_sampler = base_sampler;
    wrap_u_sampler.wrap_u = render_image_wrap_mode::repeat;
    require_sampler_miss(wrap_u_sampler, "wrap_u variant creates a texture");

    render_image_sampler_policy wrap_v_sampler = base_sampler;
    wrap_v_sampler.wrap_v = render_image_wrap_mode::mirrored_repeat;
    require_sampler_miss(wrap_v_sampler, "wrap_v variant creates a texture");

    const render_image_texture_result base_again = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = base_sampler});
    require(base_again.ok(), "base sampler can still be reacquired");
    require(base_again.cache_hit, "base sampler remains cached after sampler variants");
    require(base_again.texture.id == base.texture.id, "base sampler cache entry is unchanged");
}

void test_texture_cache_reuses_normalized_equivalent_cache_keys()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);
    const render_image_sampler_policy sampler;

    const render_resolved_image_source local_slashes = resolver.resolve(
        render_image_resolve_request{.uri = "  ./textures\\cards\\front.fake  "})
                                                            .source;
    const render_resolved_image_source local_canonical = resolver.resolve(
        render_image_resolve_request{.uri = "textures/cards/front.fake"})
                                                               .source;
    require(
        local_slashes.cache_key() == "textures/cards/front.fake",
        "local path cache key strips current directory and normalizes slashes");
    require(
        local_slashes.cache_key() == local_canonical.cache_key(),
        "equivalent local image refs resolve to one cache key");

    const render_image_texture_result local_first = cache.acquire_texture(
        render_image_texture_request{.source = local_slashes, .sampler = sampler});
    const render_image_texture_result local_second = cache.acquire_texture(
        render_image_texture_request{.source = local_canonical, .sampler = sampler});
    require(local_first.ok(), "first normalized local source creates a texture");
    require(local_second.ok(), "second equivalent local source returns a texture");
    require(local_second.cache_hit, "equivalent local source is a cache hit");
    require(local_second.texture.id == local_first.texture.id, "equivalent local source reuses texture id");
    require(local_second.key.source_key == local_first.key.source_key, "equivalent local source reuses texture key");

    const render_resolved_image_source asset_slashes = resolver.resolve(
        render_image_resolve_request{.uri = "  ASSET://./Deck\\cards\\front.fake  "})
                                                            .source;
    const render_resolved_image_source asset_canonical = resolver.resolve(
        render_image_resolve_request{.uri = "asset:////Deck/cards/front.fake"})
                                                               .source;
    require(
        asset_slashes.cache_key() == "asset://Deck/cards/front.fake",
        "asset uri cache key strips redundant asset slashes");
    require(
        asset_slashes.cache_key() == asset_canonical.cache_key(),
        "equivalent asset image refs resolve to one cache key");

    const render_image_texture_result asset_first = cache.acquire_texture(
        render_image_texture_request{.source = asset_slashes, .sampler = sampler});
    const render_image_texture_result asset_second = cache.acquire_texture(
        render_image_texture_request{.source = asset_canonical, .sampler = sampler});
    require(asset_first.ok(), "first normalized asset source creates a texture");
    require(asset_second.ok(), "second equivalent asset source returns a texture");
    require(asset_second.cache_hit, "equivalent asset source is a cache hit");
    require(asset_second.texture.id == asset_first.texture.id, "equivalent asset source reuses texture id");
    require(asset_second.key.source_key == asset_first.key.source_key, "equivalent asset source reuses texture key");
}

void test_texture_cache_rejects_invalid_sampler_policy_before_decode()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/invalid-sampler.fake"})
                                                    .source;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    render_image_sampler_policy invalid_sampler;
    invalid_sampler.wrap_v = static_cast<render_image_wrap_mode>(99);

    const render_image_texture_result result = cache.acquire_texture(
        render_image_texture_request{.source = source, .sampler = invalid_sampler});
    require(!result.ok(), "invalid sampler policy does not create a texture");
    require(result.status == render_image_texture_status::upload_failed, "invalid sampler reports upload failure");
    require(!result.texture.valid(), "invalid sampler returns no texture handle");
    require(!result.diagnostic.empty(), "invalid sampler includes diagnostic");
    require(decoder.support_requests.empty(), "invalid sampler fails before decoder support check");
    require(decoder.decode_requests.empty(), "invalid sampler fails before decode");
}

void test_texture_cache_release_unused_evicts_fake_entries()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/releasable.fake"})
                                                    .source;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_texture_request request{.source = source, .sampler = render_image_sampler_policy{}};
    const render_image_texture_result first = cache.acquire_texture(request);
    const render_image_texture_result cached = cache.acquire_texture(request);
    require(first.ok(), "first releasable texture request succeeds");
    require(cached.ok(), "cached releasable texture request succeeds");
    require(cached.cache_hit, "matching texture request hits before release");
    require(cached.texture.id == first.texture.id, "matching texture request reuses id before release");

    cache.release_unused();
    require(cache.release_unused_count() == 1, "release hook count increments during eviction");

    const render_image_texture_result after_release = cache.acquire_texture(request);
    require(after_release.ok(), "texture request after release succeeds");
    require(!after_release.cache_hit, "texture request after release is a cache miss");
    require(after_release.texture.id != first.texture.id, "texture request after release receives new id");
    require(after_release.key.source_key == first.key.source_key, "release preserves stable source key");
    require(decoder.support_requests.size() == 2, "release forces a second decoder support check");
    require(decoder.decode_requests.size() == 2, "release forces a second decode");
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

void test_texture_cache_propagates_decoder_failures()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source unsupported_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.png"})
                                                              .source;
    fake_image_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_texture_result unsupported = cache.acquire_texture(
        render_image_texture_request{.source = unsupported_source, .sampler = render_image_sampler_policy{}});
    require(!unsupported.ok(), "unsupported decoder source does not create a texture");
    require(
        unsupported.status == render_image_texture_status::decode_failed,
        "unsupported decoder source reports decode failure");
    require(!unsupported.diagnostic.empty(), "unsupported decoder source includes diagnostic");
    require(decoder.support_requests.size() == 1, "unsupported source reached decoder support check");
    require(decoder.decode_requests.empty(), "unsupported source does not decode");

    const render_resolved_image_source fake_source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/card.fake"})
                                                         .source;
    cache.set_placeholder_encoded_bytes({});
    const render_image_texture_result empty = cache.acquire_texture(
        render_image_texture_request{.source = fake_source, .sampler = render_image_sampler_policy{}});
    require(!empty.ok(), "decoder empty input does not create a texture");
    require(empty.status == render_image_texture_status::decode_failed, "decoder empty input reports decode failure");
    require(!empty.diagnostic.empty(), "decoder empty input diagnostic is propagated");
    require(decoder.support_requests.size() == 2, "empty input reached decoder support check");
    require(decoder.decode_requests.size() == 1, "empty input reached decoder decode");
}

void test_texture_cache_rejects_malformed_decoded_payload()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    const render_resolved_image_source source = resolver.resolve(
        render_image_resolve_request{.uri = "textures/malformed.fake"})
                                                    .source;
    malformed_payload_decoder decoder;
    fake_image_texture_cache cache(decoder);

    const render_image_texture_request request{.source = source, .sampler = render_image_sampler_policy{}};
    const render_image_texture_result first = cache.acquire_texture(request);
    require(!first.ok(), "malformed decoded payload does not create a texture");
    require(
        first.status == render_image_texture_status::upload_failed,
        "malformed decoded payload reports upload failure");
    require(!first.texture.valid(), "malformed decoded payload returns no texture handle");
    require(!first.diagnostic.empty(), "malformed decoded payload includes diagnostic");
    require(decoder.support_request_count == 1, "malformed payload reached support check");
    require(decoder.decode_request_count == 1, "malformed payload reached decode");

    const render_decoded_image malformed_image{
        .width = 2,
        .height = 2,
        .pixel_format = render_image_pixel_format::rgba8_srgb,
        .pixels = {std::byte{0xff}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff}},
    };
    require(expected_render_decoded_image_byte_count(malformed_image) == 16, "expected byte count includes all pixels");
    require(!has_valid_render_decoded_image_payload(malformed_image), "malformed payload helper rejects short bytes");

    const render_image_texture_result second = cache.acquire_texture(request);
    require(!second.ok(), "malformed decoded payload remains uncached");
    require(!second.cache_hit, "malformed decoded payload is not a cache hit");
    require(decoder.support_request_count == 2, "malformed payload retry reaches support check");
    require(decoder.decode_request_count == 2, "malformed payload retry decodes again");
}

} // namespace

int main()
{
    test_sampler_defaults_are_appended_to_render_image_ref();
    test_resolver_normalizes_without_fetching();
    test_decoder_interface_shape();
    test_decoder_reports_explicit_failures();
    test_sampler_policy_validation_rejects_unknown_enum_values();
    test_texture_cache_reuses_matching_key_and_misses_on_sampler_change();
    test_texture_cache_keys_include_all_sampler_policy_fields();
    test_texture_cache_reuses_normalized_equivalent_cache_keys();
    test_texture_cache_rejects_invalid_sampler_policy_before_decode();
    test_texture_cache_release_unused_evicts_fake_entries();
    test_texture_cache_reports_explicit_failures();
    test_texture_cache_propagates_decoder_failures();
    test_texture_cache_rejects_malformed_decoded_payload();
    return 0;
}
