#include "render/image/image_decoder.h"
#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_pipeline.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <string_view>
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

void append_ascii(std::vector<std::byte>& bytes, std::string_view text)
{
    for (const char value : text) {
        bytes.push_back(std::byte{static_cast<unsigned char>(value)});
    }
}

void append_byte(std::vector<std::byte>& bytes, unsigned char value)
{
    bytes.push_back(std::byte{value});
}

void append_u16_le(std::vector<std::byte>& bytes, std::uint16_t value)
{
    append_byte(bytes, static_cast<unsigned char>(value & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 8) & 0xffu));
}

void append_u32_le(std::vector<std::byte>& bytes, std::uint32_t value)
{
    append_byte(bytes, static_cast<unsigned char>(value & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 8) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 16) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 24) & 0xffu));
}

void append_i32_le(std::vector<std::byte>& bytes, std::int32_t value)
{
    append_u32_le(bytes, static_cast<std::uint32_t>(value));
}

void append_u32_be(std::vector<std::byte>& bytes, std::uint32_t value)
{
    append_byte(bytes, static_cast<unsigned char>((value >> 24) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 16) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 8) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>(value & 0xffu));
}

std::vector<std::byte> make_bytes(std::initializer_list<unsigned char> values)
{
    std::vector<std::byte> bytes;
    bytes.reserve(values.size());
    for (const unsigned char value : values) {
        append_byte(bytes, value);
    }
    return bytes;
}

std::uint32_t adler32(const std::vector<std::byte>& bytes)
{
    constexpr std::uint32_t adler_modulus = 65521u;
    std::uint32_t a = 1u;
    std::uint32_t b = 0u;
    for (std::byte value : bytes) {
        a = (a + std::to_integer<std::uint8_t>(value)) % adler_modulus;
        b = (b + a) % adler_modulus;
    }
    return (b << 16) | a;
}

std::vector<std::byte> make_bmp_bytes()
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 'B');
    append_byte(bytes, 'M');
    append_u32_le(bytes, 58);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u32_le(bytes, 54);
    append_u32_le(bytes, 40);
    append_i32_le(bytes, 1);
    append_i32_le(bytes, 1);
    append_u16_le(bytes, 1);
    append_u16_le(bytes, 24);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, 4);
    append_i32_le(bytes, 0);
    append_i32_le(bytes, 0);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, 0);
    append_byte(bytes, 3);
    append_byte(bytes, 2);
    append_byte(bytes, 1);
    append_byte(bytes, 0);
    return bytes;
}

std::vector<std::byte> make_ppm_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n1 1\n255\n");
    append_byte(bytes, 10);
    append_byte(bytes, 20);
    append_byte(bytes, 30);
    return bytes;
}

std::vector<std::byte> make_short_ppm_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n1 1\n255\n");
    append_byte(bytes, 10);
    append_byte(bytes, 20);
    return bytes;
}

void append_png_signature(std::vector<std::byte>& bytes)
{
    append_byte(bytes, 0x89);
    append_byte(bytes, 'P');
    append_byte(bytes, 'N');
    append_byte(bytes, 'G');
    append_byte(bytes, '\r');
    append_byte(bytes, '\n');
    append_byte(bytes, 0x1a);
    append_byte(bytes, '\n');
}

std::vector<std::byte> make_ihdr_data()
{
    std::vector<std::byte> data;
    append_u32_be(data, 2);
    append_u32_be(data, 2);
    append_byte(data, 8);
    append_byte(data, 6);
    append_byte(data, 0);
    append_byte(data, 0);
    append_byte(data, 0);
    return data;
}

void append_png_chunk(
    std::vector<std::byte>& bytes,
    std::string_view type_code,
    const std::vector<std::byte>& data)
{
    append_u32_be(bytes, static_cast<std::uint32_t>(data.size()));
    for (char value : type_code) {
        append_byte(bytes, static_cast<unsigned char>(value));
    }
    bytes.insert(bytes.end(), data.begin(), data.end());
    append_u32_be(bytes, 0);
}

std::vector<std::byte> make_png_bytes(std::vector<std::byte> idat_bytes)
{
    std::vector<std::byte> bytes;
    append_png_signature(bytes);
    append_png_chunk(bytes, "IHDR", make_ihdr_data());
    append_png_chunk(bytes, "IDAT", idat_bytes);
    append_png_chunk(bytes, "IEND", std::vector<std::byte>{});
    return bytes;
}

std::vector<std::byte> make_filter_none_scanlines()
{
    std::vector<std::byte> bytes;
    const std::vector<std::byte> first_row = make_bytes({1, 2, 3, 4, 5, 6, 7, 8});
    const std::vector<std::byte> second_row = make_bytes({9, 10, 11, 12, 13, 14, 15, 16});
    append_byte(bytes, 0);
    bytes.insert(bytes.end(), first_row.begin(), first_row.end());
    append_byte(bytes, 0);
    bytes.insert(bytes.end(), second_row.begin(), second_row.end());
    return bytes;
}

std::vector<std::byte> make_filter_none_scanlines_variant(unsigned char first_pixel_red)
{
    std::vector<std::byte> bytes = make_filter_none_scanlines();
    bytes[1] = std::byte{first_pixel_red};
    return bytes;
}

std::vector<std::byte> make_zlib_stored_stream(const std::vector<std::byte>& payload)
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 0x78);
    append_byte(bytes, 0x01);
    append_byte(bytes, 0x01);
    const std::uint16_t len = static_cast<std::uint16_t>(payload.size());
    append_u16_le(bytes, len);
    append_u16_le(bytes, static_cast<std::uint16_t>(len ^ 0xffffu));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    append_u32_be(bytes, adler32(payload));
    return bytes;
}

std::vector<std::byte> make_zlib_huffman_stream()
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 0x78);
    append_byte(bytes, 0x01);
    append_byte(bytes, 0x03);
    append_u32_be(bytes, adler32(std::vector<std::byte>{}));
    return bytes;
}

void set_source_bytes(
    quiz_vulkan::render::fake_image_source_bytes_loader& loader,
    std::string source_key,
    std::vector<std::byte> bytes)
{
    loader.set_source_bytes(std::move(source_key), std::move(bytes));
}

void test_standard_pipeline_uploads_bmp()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/card.bmp", make_bmp_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_pipeline_result result =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("textures/card.bmp"));

    require(result.ok(), "standard image texture pipeline uploads BMP");
    require(result.texture.decode_metadata.decoder_id == "bmp_image_decoder", "standard pipeline uses BMP decoder");
    require(result.texture.texture.width == 1, "standard pipeline preserves BMP texture width");
    require(result.texture.texture.height == 1, "standard pipeline preserves BMP texture height");
    require(result.texture.decoder_diagnostics.size() == 1, "standard pipeline records BMP candidate diagnostics");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.ready_count == 1, "standard pipeline BMP snapshot records ready count");
    require(snapshot.upload_snapshot.upload_count == 1, "standard pipeline uploads BMP once");
    require(snapshot.upload_snapshot.request_snapshots[0].decoded_byte_count == 4, "BMP upload stages RGBA bytes");
    require(snapshot.entries[0].decoder_diagnostics.size() == 1, "BMP snapshot preserves decoder diagnostics");
}

void test_standard_pipeline_uploads_ppm()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/card.ppm", make_ppm_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_pipeline_result result =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("textures/card.ppm"));

    require(result.ok(), "standard image texture pipeline uploads PPM");
    require(result.texture.decode_metadata.decoder_id == "ppm_image_decoder", "standard pipeline uses PPM decoder");
    require(result.texture.texture.width == 1, "standard pipeline preserves PPM texture width");
    require(result.texture.texture.height == 1, "standard pipeline preserves PPM texture height");
    require(result.texture.decoder_diagnostics.size() == 2, "standard pipeline records BMP and PPM diagnostics");
    require(result.texture.decoder_diagnostics[0].decoder_id == "bmp_image_decoder", "PPM checks BMP first");
    require(result.texture.decoder_diagnostics[1].decoder_id == "ppm_image_decoder", "PPM checks PPM second");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.upload_snapshot.upload_count == 1, "standard pipeline uploads PPM once");
    require(snapshot.upload_snapshot.request_snapshots[0].decoded_byte_count == 4, "PPM upload stages RGBA bytes");
}

void test_standard_pipeline_uploads_zlib_stored_png()
{
    using namespace quiz_vulkan::render;

    const std::vector<std::byte> scanlines = make_filter_none_scanlines();
    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/card.png", make_png_bytes(make_zlib_stored_stream(scanlines)));
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_pipeline_result result =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("textures/card.png"));

    require(result.ok(), "standard image texture pipeline uploads zlib-stored PNG");
    require(result.texture.decode_metadata.decoder_id == "png_image_decoder", "standard pipeline uses PNG decoder");
    require(result.texture.texture.width == 2, "standard pipeline preserves PNG texture width");
    require(result.texture.texture.height == 2, "standard pipeline preserves PNG texture height");
    require(result.texture.decoder_diagnostics.size() == 3, "standard pipeline records all PNG candidates");
    require(result.texture.decoder_diagnostics[2].decoder_id == "png_image_decoder", "PNG candidate is third");
    require(result.texture.decoder_diagnostics[2].decode_attempted, "PNG candidate is decoded");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.upload_snapshot.upload_count == 1, "standard pipeline uploads PNG once");
    require(snapshot.upload_snapshot.request_snapshots[0].width == 2, "PNG upload request records width");
    require(snapshot.upload_snapshot.request_snapshots[0].height == 2, "PNG upload request records height");
    require(snapshot.upload_snapshot.request_snapshots[0].decoded_byte_count == 16, "PNG upload stages RGBA bytes");
}

void test_standard_pipeline_reuses_cached_decode_and_upload_for_same_normalized_key()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(
        loader,
        "textures/card.png",
        make_png_bytes(make_zlib_stored_stream(make_filter_none_scanlines())));
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_pipeline_result first =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("  ./textures\\card.png  "));
    const render_image_texture_pipeline_result second =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("textures/card.png"));

    require(first.ok(), "first normalized PNG request succeeds");
    require(second.ok(), "second normalized PNG request succeeds");
    require(!first.texture.cache_hit, "first normalized PNG request uploads texture");
    require(second.texture.cache_hit, "second normalized PNG request reuses cached texture");
    require(first.resolve.source.cache_key() == "textures/card.png", "first request normalizes cache key");
    require(second.resolve.source.cache_key() == "textures/card.png", "second request uses same normalized key");
    require(first.texture.texture.id == second.texture.texture.id, "cache reuse returns same texture handle");

    const standard_image_texture_pipeline_snapshot snapshot = pipeline.standard_diagnostic_snapshot();
    require(snapshot.pipeline.acquire_count == 2, "standard reuse snapshot records both requests");
    require(snapshot.pipeline.ready_count == 2, "standard reuse snapshot records both ready results");
    require(snapshot.pipeline.cache_hit_count == 1, "standard reuse snapshot counts cache hit");
    require(snapshot.pipeline.upload_snapshot.upload_count == 1, "standard reuse uploads only once");
    require(snapshot.decoder.support_check_count == 1, "standard reuse checks decoder support only on cache miss");
    require(snapshot.decoder.decode_attempt_count == 1, "standard reuse decodes only once");
    require(snapshot.decoder.decoded_count == 1, "standard reuse records one successful decode");
    require(snapshot.decoder.failed_decode_count == 0, "standard reuse records no decode failures");
    require(snapshot.pipeline.entries.size() == 2, "standard reuse records two pipeline entries");
    require(!snapshot.pipeline.entries[0].cache_hit, "first standard reuse entry is not a cache hit");
    require(snapshot.pipeline.entries[1].cache_hit, "second standard reuse entry is a cache hit");
    require(
        snapshot.pipeline.entries[1].upload_count_before == snapshot.pipeline.entries[1].upload_count_after,
        "cache hit entry does not upload");
    require(
        snapshot.pipeline.entries[1].decode_metadata.decoder_id == "png_image_decoder",
        "cache hit preserves decoded metadata");
}

void test_standard_pipeline_decodes_and_uploads_distinct_cache_revision_after_invalidation()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(
        loader,
        "textures/card.png",
        make_png_bytes(make_zlib_stored_stream(make_filter_none_scanlines_variant(1))));
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_pipeline_result first =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("textures/card.png"));
    set_source_bytes(
        loader,
        "textures/card.png",
        make_png_bytes(make_zlib_stored_stream(make_filter_none_scanlines_variant(42))));
    pipeline.invalidate_source("textures/card.png");
    const render_image_texture_pipeline_result second =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("./textures/card.png"));

    require(first.ok(), "first cache revision PNG request succeeds");
    require(second.ok(), "second cache revision PNG request succeeds");
    require(!first.texture.cache_hit, "first cache revision is a cache miss");
    require(!second.texture.cache_hit, "second cache revision decodes after invalidation");
    require(first.resolve.source.cache_key() == second.resolve.source.cache_key(), "cache revisions share normalized key");
    require(first.texture.texture.id != second.texture.texture.id, "distinct cache revision uploads a new texture handle");

    const standard_image_texture_pipeline_snapshot snapshot = pipeline.standard_diagnostic_snapshot();
    require(snapshot.pipeline.acquire_count == 2, "cache revision snapshot records two requests");
    require(snapshot.pipeline.cache_hit_count == 0, "cache revision snapshot records no cache hits");
    require(snapshot.pipeline.invalidation_count == 1, "cache revision snapshot records invalidation");
    require(snapshot.pipeline.upload_snapshot.upload_count == 2, "cache revision uploads twice");
    require(snapshot.decoder.support_check_count == 2, "cache revision checks decoder support twice");
    require(snapshot.decoder.decode_attempt_count == 2, "cache revision decodes twice");
    require(snapshot.decoder.decoded_count == 2, "cache revision records two successful decodes");
    require(snapshot.decoder.failed_decode_count == 0, "cache revision records no decode failures");
    require(snapshot.pipeline.entries.size() == 2, "cache revision records two entries");
    require(snapshot.pipeline.entries[0].upload_count_after == 1, "first cache revision upload is recorded");
    require(snapshot.pipeline.entries[1].upload_count_before == 1, "second cache revision starts after first upload");
    require(snapshot.pipeline.entries[1].upload_count_after == 2, "second cache revision upload is recorded");
}

void test_standard_pipeline_reports_unsupported_decode_with_candidate_diagnostics()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/card.jpg", make_bytes({0xff, 0xd8, 0xff, 0xd9}));
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_pipeline_result result =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("textures/card.jpg"));

    require(!result.ok(), "standard image texture pipeline rejects unsupported JPEG");
    require(result.status == render_image_texture_pipeline_status::decode_failed, "unsupported JPEG is decode_failed");
    require(result.texture.status == render_image_texture_status::decode_failed, "texture result records decode failure");
    require(result.texture.texture.id == 0, "unsupported JPEG returns no placeholder texture");
    require(result.texture.decoder_diagnostics.size() == 3, "unsupported JPEG records all decoder candidates");
    require(result.texture.decoder_diagnostics[2].terminal_candidate, "unsupported JPEG terminates on final candidate");
    require(
        result.texture.decoder_diagnostics[2].diagnostic == "decoder chain exhausted all candidates",
        "unsupported JPEG diagnostic is deterministic");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.decode_failure_count == 1, "unsupported JPEG snapshot counts decode failure");
    require(snapshot.upload_snapshot.upload_count == 0, "unsupported JPEG does not upload");
    require(snapshot.cache_snapshot.placeholder_policy_texture_count == 0, "unsupported JPEG has no placeholder texture");

    const standard_image_texture_pipeline_snapshot standard_snapshot = pipeline.standard_diagnostic_snapshot();
    require(standard_snapshot.decoder.decode_attempt_count == 1, "unsupported JPEG attempts standard decode once");
    require(standard_snapshot.decoder.failed_decode_count == 1, "unsupported JPEG counts failed standard decode");
}

void test_standard_pipeline_reports_invalid_decode_with_candidate_diagnostics()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/bad.ppm", make_short_ppm_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_pipeline_result result =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("textures/bad.ppm"));

    require(!result.ok(), "standard image texture pipeline rejects malformed PPM");
    require(result.status == render_image_texture_pipeline_status::decode_failed, "malformed PPM is decode_failed");
    require(result.texture.texture.id == 0, "malformed PPM returns no placeholder texture");
    require(result.texture.decode_metadata.decoder_id == "ppm_image_decoder", "malformed PPM records decoder id");
    require(result.texture.decoder_diagnostics.size() == 2, "malformed PPM records BMP and PPM candidates");
    require(result.texture.decoder_diagnostics[1].decoder_id == "ppm_image_decoder", "malformed PPM records PPM");
    require(result.texture.decoder_diagnostics[1].decode_attempted, "malformed PPM attempts PPM decode");
    require(!result.diagnostic.empty(), "malformed PPM returns deterministic diagnostic text");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.decode_failure_count == 1, "malformed PPM snapshot counts decode failure");
    require(snapshot.upload_snapshot.upload_count == 0, "malformed PPM does not upload");
    require(snapshot.cache_snapshot.placeholder_policy_texture_count == 0, "malformed PPM has no placeholder texture");
}

void test_standard_pipeline_reports_invalid_png_inflater_failure()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/bad.png", make_png_bytes(make_zlib_huffman_stream()));
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_pipeline_result result =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("textures/bad.png"));

    require(!result.ok(), "standard image texture pipeline rejects unsupported PNG compression");
    require(result.status == render_image_texture_pipeline_status::decode_failed, "bad PNG is decode_failed");
    require(result.texture.texture.id == 0, "bad PNG returns no placeholder texture");
    require(result.texture.decoder_diagnostics.size() == 3, "bad PNG records all decoder candidates");
    require(result.texture.decoder_diagnostics[2].decoder_id == "png_image_decoder", "bad PNG records PNG");
    require(
        result.texture.decoder_diagnostics[2].decode_diagnostic.find("inflater_failed") != std::string::npos,
        "bad PNG diagnostic preserves inflater failure");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.decode_failure_count == 1, "bad PNG snapshot counts decode failure");
    require(snapshot.upload_snapshot.upload_count == 0, "bad PNG does not upload");

    const standard_image_texture_pipeline_snapshot standard_snapshot = pipeline.standard_diagnostic_snapshot();
    require(standard_snapshot.decoder.decode_attempt_count == 1, "bad PNG attempts standard decode once");
    require(standard_snapshot.decoder.failed_decode_count == 1, "bad PNG counts failed standard decode");
}

} // namespace

int main()
{
    test_standard_pipeline_uploads_bmp();
    test_standard_pipeline_uploads_ppm();
    test_standard_pipeline_uploads_zlib_stored_png();
    test_standard_pipeline_reuses_cached_decode_and_upload_for_same_normalized_key();
    test_standard_pipeline_decodes_and_uploads_distinct_cache_revision_after_invalidation();
    test_standard_pipeline_reports_unsupported_decode_with_candidate_diagnostics();
    test_standard_pipeline_reports_invalid_decode_with_candidate_diagnostics();
    test_standard_pipeline_reports_invalid_png_inflater_failure();
    return 0;
}
