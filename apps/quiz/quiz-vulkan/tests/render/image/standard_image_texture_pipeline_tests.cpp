#include "render/image/image_decoder.h"
#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_frame_resource_packet_materialization.h"
#include "render/image/image_texture_pipeline.h"
#include "standard_image_jpeg_fixture.inl"

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

std::vector<std::byte> make_jpeg_bytes()
{
    return quiz_vulkan::render::test_fixtures::make_standard_jpeg_bytes();
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

std::vector<std::byte> make_zero_dimension_ppm_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n0 1\n255\n");
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

const quiz_vulkan::render::fake_image_texture_cache_entry_snapshot* find_cache_entry_by_source_key(
    const quiz_vulkan::render::fake_image_texture_cache_snapshot& snapshot,
    std::string_view source_key)
{
    for (const quiz_vulkan::render::fake_image_texture_cache_entry_snapshot& entry : snapshot.entries) {
        if (entry.key.source_key == source_key) {
            return &entry;
        }
    }
    return nullptr;
}

const quiz_vulkan::render::fake_image_texture_cache_entry_snapshot*
find_placeholder_cache_entry_by_requested_source_key(
    const quiz_vulkan::render::fake_image_texture_cache_snapshot& snapshot,
    std::string_view source_key)
{
    for (const quiz_vulkan::render::fake_image_texture_cache_entry_snapshot& entry : snapshot.entries) {
        if (entry.placeholder_texture && entry.requested_key.source_key == source_key) {
            return &entry;
        }
    }
    return nullptr;
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
    require(result.texture.texture.width == 1, "standard pipeline preserves BMP texture width");
    require(result.texture.texture.height == 1, "standard pipeline preserves BMP texture height");
    require(
        result.texture.external_decoder_selection.detected_format == render_image_encoded_format::bmp,
        "standard pipeline records detected BMP format");
    if (result.texture.external_decoder_selection.ready_for_external_decode) {
        require(
            result.texture.decode_metadata.decoder_id == "stb_image_decoder",
            "standard pipeline prefers stb for BMP when available");
        require(
            result.texture.external_decoder_selection.used_third_party_adapter,
            "standard pipeline records BMP adapter route");
        require(
            !result.texture.external_decoder_selection.prefer_internal_decoder,
            "standard pipeline BMP route no longer reports internal preference");
        require(result.texture.decoder_diagnostics.size() == 1, "BMP adapter route records one diagnostic");
        require(
            result.texture.decoder_diagnostics[0].decoder_id == "stb_image_decoder",
            "BMP adapter route records stb diagnostic");
    } else {
        require(
            result.texture.decode_metadata.decoder_id == "bmp_image_decoder",
            "standard pipeline falls back to BMP decoder when stb is unavailable");
        require(
            result.texture.external_decoder_selection.fallback_to_standard_decoder_chain,
            "standard pipeline BMP fallback records standard chain");
        require(
            result.texture.decoder_diagnostics.back().decoder_id == "bmp_image_decoder",
            "BMP fallback records BMP decoder diagnostic");
    }

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.ready_count == 1, "standard pipeline BMP snapshot records ready count");
    require(snapshot.upload_snapshot.upload_count == 1, "standard pipeline uploads BMP once");
    require(snapshot.upload_snapshot.request_snapshots[0].decoded_byte_count == 4, "BMP upload stages RGBA bytes");
    require(
        snapshot.entries[0].decoder_diagnostics.size() == result.texture.decoder_diagnostics.size(),
        "BMP snapshot preserves decoder diagnostics");
    require(
        snapshot.entries[0].selected_decoder_id == result.texture.decode_metadata.decoder_id,
        "BMP snapshot preserves selected decoder route");
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
    require(result.texture.texture.width == 1, "standard pipeline preserves PPM texture width");
    require(result.texture.texture.height == 1, "standard pipeline preserves PPM texture height");
    require(
        result.texture.external_decoder_selection.detected_format == render_image_encoded_format::ppm,
        "standard pipeline records detected PPM format");
    if (result.texture.external_decoder_selection.ready_for_external_decode) {
        require(
            result.texture.decode_metadata.decoder_id == "stb_image_decoder",
            "standard pipeline prefers stb for PPM when available");
        require(
            result.texture.external_decoder_selection.used_third_party_adapter,
            "standard pipeline records PPM adapter route");
        require(
            !result.texture.external_decoder_selection.prefer_internal_decoder,
            "standard pipeline PPM route no longer reports internal preference");
        require(result.texture.decoder_diagnostics.size() == 1, "PPM adapter route records one diagnostic");
        require(
            result.texture.decoder_diagnostics[0].decoder_id == "stb_image_decoder",
            "PPM adapter route records stb diagnostic");
    } else {
        require(
            result.texture.decode_metadata.decoder_id == "ppm_image_decoder",
            "standard pipeline falls back to PPM decoder when stb is unavailable");
        require(
            result.texture.external_decoder_selection.fallback_to_standard_decoder_chain,
            "standard pipeline PPM fallback records standard chain");
        require(
            result.texture.decoder_diagnostics.back().decoder_id == "ppm_image_decoder",
            "PPM fallback records PPM decoder diagnostic");
    }

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
    require(result.texture.texture.width == 2, "standard pipeline preserves PNG texture width");
    require(result.texture.texture.height == 2, "standard pipeline preserves PNG texture height");
    require(
        result.texture.external_decoder_selection.detected_format == render_image_encoded_format::png,
        "standard pipeline records detected PNG format");
    if (result.texture.external_decoder_selection.ready_for_external_decode) {
        require(
            result.texture.decode_metadata.decoder_id == "stb_image_decoder",
            "standard pipeline prefers stb for PNG when available");
        require(
            result.texture.external_decoder_selection.used_third_party_adapter,
            "standard pipeline records PNG adapter route");
        require(
            !result.texture.external_decoder_selection.prefer_internal_decoder,
            "standard pipeline PNG route no longer reports internal preference");
        require(result.texture.decoder_diagnostics.size() == 1, "PNG adapter route records one diagnostic");
        require(
            result.texture.decoder_diagnostics[0].decoder_id == "stb_image_decoder",
            "PNG adapter route records stb diagnostic");
    } else {
        require(
            result.texture.decode_metadata.decoder_id == "png_image_decoder",
            "standard pipeline falls back to PNG decoder when stb is unavailable");
        require(
            result.texture.external_decoder_selection.fallback_to_standard_decoder_chain,
            "standard pipeline PNG fallback records standard chain");
        require(
            result.texture.decoder_diagnostics.back().decoder_id == "png_image_decoder",
            "PNG fallback records PNG decoder diagnostic");
    }

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.upload_snapshot.upload_count == 1, "standard pipeline uploads PNG once");
    require(snapshot.upload_snapshot.request_snapshots[0].width == 2, "PNG upload request records width");
    require(snapshot.upload_snapshot.request_snapshots[0].height == 2, "PNG upload request records height");
    require(snapshot.upload_snapshot.request_snapshots[0].decoded_byte_count == 16, "PNG upload stages RGBA bytes");
}

void test_standard_pipeline_uploads_jpeg_through_stb_when_available()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/card.jpg", make_jpeg_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_pipeline_result result =
        pipeline.acquire_texture(make_render_image_texture_pipeline_request("textures/card.jpg"));
    const standard_image_texture_pipeline_snapshot standard_snapshot =
        pipeline.standard_diagnostic_snapshot();

    require(
        standard_snapshot.pipeline.entries.size() == 1,
        "standard JPEG pipeline records one pipeline entry");
    require(
        standard_snapshot.pipeline.entries[0].external_decoder_selection.detected_format
            == render_image_encoded_format::jpeg,
        "standard JPEG pipeline records detected JPEG format");
    require(
        !standard_snapshot.pipeline.entries[0].decoder_diagnostics.empty(),
        "standard JPEG pipeline records decoder diagnostics");
    require(
        standard_snapshot.pipeline.entries[0].decoder_diagnostics.front().decoder_id
            == "stb_image_decoder",
        "standard JPEG pipeline records STB candidate first");

    if (!standard_snapshot.pipeline.entries[0].external_decoder_selection.ready_for_external_decode) {
        require(!result.ok(), "standard JPEG pipeline fails without available STB dependency");
        require(
            standard_snapshot.pipeline.entries[0].external_decoder_selection.fallback_to_standard_decoder_chain,
            "standard JPEG pipeline records internal fallback when STB is unavailable");
        require(
            standard_snapshot.decoder.failed_decode_count == 1,
            "standard JPEG pipeline counts failed fallback decode");
        return;
    }

    require(result.ok(), "standard image texture pipeline uploads JPEG through STB");
    require(result.texture.texture.width == 1, "standard JPEG pipeline preserves decoded width");
    require(result.texture.texture.height == 1, "standard JPEG pipeline preserves decoded height");
    require(result.texture.decode_metadata.decoder_id == "stb_image_decoder", "standard JPEG pipeline selects STB");
    require(
        result.texture.external_decoder_selection.used_third_party_adapter,
        "standard JPEG pipeline records adapter route");
    require(
        result.texture.decoder_diagnostics.size() == 1,
        "standard JPEG pipeline records one successful STB diagnostic");
    require(
        standard_snapshot.pipeline.upload_snapshot.upload_count == 1,
        "standard JPEG pipeline uploads once");
    require(
        standard_snapshot.pipeline.upload_snapshot.request_snapshots[0].decoded_byte_count == 4,
        "standard JPEG pipeline stages RGBA bytes");
    require(
        standard_snapshot.pipeline.upload_snapshot.request_snapshots[0]
            .decoded_payload.stable_byte_hash != 0,
        "standard JPEG pipeline records decoded payload hash");
    require(
        standard_snapshot.pipeline.upload_snapshot.request_snapshots[0]
            .payload_layout.expected_byte_count == 4,
        "standard JPEG pipeline records upload layout byte count");
    require(
        standard_snapshot.pipeline.upload_snapshot.request_snapshots[0]
            .staging_payload_plan.total_staging_byte_count == 4,
        "standard JPEG pipeline records staging byte count");
    require(standard_snapshot.decoder.decoded_count == 1, "standard JPEG pipeline counts decoded image");
    require(standard_snapshot.decoder.failed_decode_count == 0, "standard JPEG pipeline records no decode failures");
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
        snapshot.pipeline.entries[1].decode_metadata.decoder_id == first.texture.decode_metadata.decoder_id,
        "cache hit preserves decoded metadata");
}

void test_standard_pipeline_reuses_upload_residency_across_frames_and_fallbacks()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(
        loader,
        "textures/card.png",
        make_png_bytes(make_zlib_stored_stream(make_filter_none_scanlines())));
    set_source_bytes(loader, "textures/bad.ppm", make_short_ppm_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);
    pipeline.set_placeholder_texture_policy(placeholder_policy);
    const image_texture_pipeline_interface& pipeline_interface = pipeline;
    require(
        image_texture_pipeline_upload_cache_diagnostics(pipeline_interface) != nullptr,
        "standard pipeline exposes upload/cache side interface");

    render_image_sampler_policy linear_sampler;
    linear_sampler.min_filter = render_image_filter::linear;
    linear_sampler.mag_filter = render_image_filter::linear;

    const auto make_single_image_plan = [&](std::string uri) {
        return plan_render_image_texture_batch(
            std::vector<render_image_ref>{render_image_ref{
                .uri = std::move(uri),
                .sampler = linear_sampler,
            }},
            resolver,
            render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    };

    const render_image_texture_batch_plan first_plan = make_single_image_plan("textures/card.png");
    const render_image_texture_batch_execution_diagnostics first_execution =
        execute_render_image_texture_batch_plan(first_plan, pipeline);
    const render_image_texture_frame_snapshot first_frame =
        make_render_image_texture_frame_snapshot(first_plan, first_execution);
    const standard_image_texture_pipeline_snapshot first_snapshot =
        pipeline.standard_diagnostic_snapshot();
    const render_image_texture_pipeline_upload_cache_snapshot first_side_snapshot =
        render_image_texture_pipeline_upload_cache_diagnostic_snapshot(pipeline_interface);
    const render_image_texture_upload_result_snapshot first_upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
            first_side_snapshot.upload_snapshot);
    const render_image_texture_frame_resource_packet_materialization first_materialization =
        materialize_render_image_texture_frame_resource_packets(first_frame, first_upload_result);

    require(first_plan.ok(), "first standard residency frame plans PNG request");
    require(first_execution.ok(), "first standard residency frame executes PNG request");
    require(first_materialization.ok(), "first standard residency frame materializes resource packet");
    require(first_side_snapshot.ok(), "first standard residency frame exposes side diagnostics");
    require(first_side_snapshot.upload_snapshot.upload_count == 1, "first standard residency frame uploads once");
    require(first_snapshot.decoder.decode_attempt_count == 1, "first standard residency frame decodes once");
    require(first_snapshot.decoder.decoded_count == 1, "first standard residency frame records decoded image");
    require(first_side_snapshot.cache_snapshot.texture_count == 1, "first standard residency frame has one resident texture");
    require(
        first_side_snapshot.cache_snapshot.cached_decoded_byte_count == 16,
        "first standard residency frame tracks decoded RGBA bytes");
    require(!first_execution.entries[0].cache_hit, "first standard residency frame is a cache miss");
    require(!first_execution.entries[0].placeholder_texture, "first standard residency frame is not placeholder-backed");
    require(!first_plan.entries[0].stable_texture_cache_key.empty(), "first standard residency frame has stable texture key");

    const fake_image_texture_cache_entry_snapshot* first_cache_entry =
        find_cache_entry_by_source_key(first_side_snapshot.cache_snapshot, "textures/card.png");
    require(first_cache_entry != nullptr, "first standard residency frame creates resident cache entry");
    require(first_cache_entry->upload_generation_id == 1, "first resident cache entry records upload generation");
    require(first_cache_entry->decoded_byte_count == 16, "first resident cache entry records decoded bytes");
    require(first_cache_entry->access_count == 1, "first resident cache entry records one access");
    require(!first_cache_entry->placeholder_texture, "first resident cache entry is not placeholder");
    require(
        first_materialization.entries[0].cache_record.stable_texture_cache_key
            == first_plan.entries[0].stable_texture_cache_key,
        "first materialization preserves stable cache key");
    require(
        first_materialization.entries[0].cache_record.texture_id == first_execution.entries[0].texture.id,
        "first materialization preserves uploaded texture id");
    require(
        first_materialization.entries[0].upload_record.upload_generation_id
            == first_cache_entry->upload_generation_id,
        "first materialization preserves upload generation");
    require(
        first_materialization.entries[0].upload_record.uploaded_byte_count == 16,
        "first materialization preserves uploaded RGBA byte count");

    const render_image_texture_batch_plan second_plan = make_single_image_plan("  ./textures\\card.png  ");
    const render_image_texture_batch_execution_diagnostics second_execution =
        execute_render_image_texture_batch_plan(second_plan, pipeline);
    const render_image_texture_frame_snapshot second_frame =
        make_render_image_texture_frame_snapshot(second_plan, second_execution);
    const standard_image_texture_pipeline_snapshot second_snapshot =
        pipeline.standard_diagnostic_snapshot();
    const render_image_texture_pipeline_upload_cache_snapshot second_side_snapshot =
        render_image_texture_pipeline_upload_cache_diagnostic_snapshot(pipeline_interface);
    const render_image_texture_upload_result_snapshot second_upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
            second_side_snapshot.upload_snapshot);
    const render_image_texture_frame_resource_packet_materialization second_materialization =
        materialize_render_image_texture_frame_resource_packets(second_frame, second_upload_result);

    require(second_plan.ok(), "second standard residency frame plans normalized PNG request");
    require(second_execution.ok(), "second standard residency frame executes from residency");
    require(second_materialization.ok(), "second standard residency frame materializes resident packet");
    require(second_execution.entries[0].cache_hit, "second standard residency frame hits cache");
    require(second_execution.entries[0].cache_reused, "second standard residency frame reuses residency");
    require(
        second_plan.entries[0].stable_texture_cache_key == first_plan.entries[0].stable_texture_cache_key,
        "second standard residency frame keeps stable cache key");
    require(
        second_side_snapshot.upload_snapshot.upload_count == 1,
        "second standard residency frame does not request another upload");
    require(second_snapshot.decoder.decode_attempt_count == 1, "second standard residency frame does not decode again");
    require(second_side_snapshot.cache_hit_count == 1, "standard residency snapshot counts second-frame hit");
    require(second_side_snapshot.cache_snapshot.texture_count == 1, "second standard residency frame keeps one texture resident");
    require(
        second_side_snapshot.cache_snapshot.resident_access_count == 2,
        "second standard residency frame records resident access reuse");

    const fake_image_texture_cache_entry_snapshot* second_cache_entry =
        find_cache_entry_by_source_key(second_side_snapshot.cache_snapshot, "textures/card.png");
    require(second_cache_entry != nullptr, "second standard residency frame keeps resident cache entry");
    require(
        second_cache_entry->texture.id == first_cache_entry->texture.id,
        "second resident cache entry reuses texture handle");
    require(
        second_cache_entry->upload_generation_id == first_cache_entry->upload_generation_id,
        "second resident cache entry reuses upload generation");
    require(second_cache_entry->access_count == 2, "second resident cache entry records both frame accesses");
    require(
        second_materialization.entries[0].cache_record.cache_reused,
        "second materialization records cache reuse");
    require(
        second_materialization.entries[0].upload_record.upload_generation_id
            == first_materialization.entries[0].upload_record.upload_generation_id,
        "second materialization reuses first upload generation");
    require(
        second_materialization.entries[0].upload_record.uploaded_byte_count
            == first_materialization.entries[0].upload_record.uploaded_byte_count,
        "second materialization reuses uploaded byte evidence");

    const std::size_t upload_count_before_decode_failure =
        second_side_snapshot.upload_snapshot.upload_count;
    const render_image_texture_batch_plan decode_failure_plan = make_single_image_plan("textures/bad.ppm");
    const render_image_texture_batch_execution_diagnostics decode_failure_execution =
        execute_render_image_texture_batch_plan(decode_failure_plan, pipeline);
    const standard_image_texture_pipeline_snapshot decode_failure_snapshot =
        pipeline.standard_diagnostic_snapshot();
    const render_image_texture_pipeline_upload_cache_snapshot decode_failure_side_snapshot =
        render_image_texture_pipeline_upload_cache_diagnostic_snapshot(pipeline_interface);

    require(decode_failure_plan.ok(), "decode failure residency frame plans malformed source");
    require(decode_failure_execution.ok(), "decode failure residency frame uses placeholder fallback");
    require(
        decode_failure_execution.entries[0].placeholder_texture,
        "decode failure residency frame is placeholder-backed");
    require(
        decode_failure_snapshot.decoder.failed_decode_count == 1,
        "decode failure residency frame records failed standard decode");
    require(
        decode_failure_side_snapshot.upload_snapshot.upload_count
            == upload_count_before_decode_failure + 1,
        "decode failure residency frame uploads one placeholder texture");
    require(
        decode_failure_side_snapshot.cache_snapshot.placeholder_policy_texture_count == 1,
        "decode failure residency frame records one placeholder texture");
    require(
        find_cache_entry_by_source_key(decode_failure_side_snapshot.cache_snapshot, "textures/bad.ppm")
            == nullptr,
        "decode failure residency frame does not mark failed source texture resident");
    const fake_image_texture_cache_entry_snapshot* decode_failure_placeholder =
        find_placeholder_cache_entry_by_requested_source_key(
            decode_failure_side_snapshot.cache_snapshot,
            "textures/bad.ppm");
    require(
        decode_failure_placeholder != nullptr,
        "decode failure residency frame records placeholder entry for failed source");
    require(
        decode_failure_placeholder->decoded_byte_count == 16,
        "decode failure placeholder keeps deterministic decoded bytes");

    const std::size_t upload_count_before_missing_source =
        decode_failure_side_snapshot.upload_snapshot.upload_count;
    const render_image_texture_batch_plan missing_source_plan = make_single_image_plan("textures/missing.png");
    const render_image_texture_batch_execution_diagnostics missing_source_execution =
        execute_render_image_texture_batch_plan(missing_source_plan, pipeline);
    const standard_image_texture_pipeline_snapshot missing_source_snapshot =
        pipeline.standard_diagnostic_snapshot();
    const render_image_texture_pipeline_upload_cache_snapshot missing_source_side_snapshot =
        render_image_texture_pipeline_upload_cache_diagnostic_snapshot(pipeline_interface);

    require(missing_source_plan.ok(), "missing source residency frame plans source key");
    require(!missing_source_execution.ok(), "missing source residency frame remains blocked");
    require(
        missing_source_execution.entries[0].pipeline_status
            == render_image_texture_pipeline_status::source_load_failed,
        "missing source residency frame records source load failure");
    require(
        !missing_source_execution.entries[0].placeholder_texture,
        "missing source residency frame does not synthesize cache residency");
    require(
        missing_source_side_snapshot.upload_snapshot.upload_count == upload_count_before_missing_source,
        "missing source residency frame does not upload placeholder without source bytes");
    require(
        find_cache_entry_by_source_key(missing_source_side_snapshot.cache_snapshot, "textures/missing.png")
            == nullptr,
        "missing source residency frame does not mark missing source resident");
    require(
        find_placeholder_cache_entry_by_requested_source_key(
            missing_source_side_snapshot.cache_snapshot,
            "textures/missing.png")
            == nullptr,
        "missing source residency frame does not create placeholder cache entry");
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

void test_standard_pipeline_materializes_decoded_bytes_for_resource_consumption()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/card.ppm", make_ppm_bytes());
    set_source_bytes(loader, "textures/bad.ppm", make_short_ppm_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);
    pipeline.set_placeholder_texture_policy(placeholder_policy);

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{
            render_image_ref{.uri = "textures/card.ppm", .sampler = nearest_sampler},
            render_image_ref{.uri = "textures/bad.ppm"},
        },
        resolver,
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const standard_image_texture_pipeline_snapshot standard_snapshot =
        pipeline.standard_diagnostic_snapshot();
    const render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
            standard_snapshot.pipeline.upload_snapshot);
    const render_image_texture_frame_resource_packet_materialization materialization =
        materialize_render_image_texture_frame_resource_packets(frame, upload_result);
    const render_image_texture_frame_resource_packet_consumption_summary consumption =
        make_render_image_texture_frame_resource_packet_consumption_summary(materialization);

    require(execution.ok(), "standard decoded materialization batch executes with placeholder fallback");
    require(standard_snapshot.decoder.decode_attempt_count == 2, "standard materialization decodes both cache misses");
    require(standard_snapshot.decoder.decoded_count == 1, "standard materialization records one real decoded image");
    require(standard_snapshot.decoder.failed_decode_count == 1, "standard materialization records one failed decode");
    require(upload_result.packet_count == 2, "standard materialization records real and placeholder uploads");
    require(materialization.ok(), "standard decoded resource packets materialize");
    require(materialization.placeholder_packet_count == 1, "standard materialization records placeholder packet");
    require(consumption.ok(), "standard decoded resource consumption is renderer-boundary ready");
    require(consumption.decoded_resource_ready_count == 2, "consumption records real and placeholder decoded resources");
    require(consumption.decoded_payload_byte_count == 20, "consumption sums decoded RGBA bytes");
    require(consumption.staging_payload_byte_count == 20, "consumption sums staging payload bytes");
    require(consumption.decoded_payload_hash_count == 2, "consumption records stable decoded payload hashes");
    require(
        consumption.decoded_resource_summary == "decoded_resources=2; payload_hashes=2; decoded_bytes=20; staging_bytes=20",
        "standard consumption decoded resource summary is stable");

    const render_image_texture_frame_resource_packet_consumption_entry& ready = consumption.entries[0];
    require(ready.ok(), "real decoded resource consumption entry is ready");
    require(ready.request_index == 0, "real decoded resource preserves request index");
    require(ready.decoded_payload_valid, "real decoded resource preserves payload validity");
    require(ready.decoded_payload_hash != 0, "real decoded resource preserves payload hash");
    require(ready.decoded_byte_count == 4, "real decoded resource records RGBA byte count");
    require(ready.upload_layout_byte_count == 4, "real decoded resource records layout bytes");
    require(ready.upload_layout_row_stride_byte_count == 4, "real decoded resource records row stride");
    require(ready.staging_payload_byte_count == 4, "real decoded resource records staging bytes");
    require(ready.staging_row_copy_count == 1, "real decoded resource records staging row copy");
    require(ready.upload_payload_layout_ready, "real decoded resource preserves upload layout readiness");
    require(ready.staging_payload_ready, "real decoded resource preserves staging readiness");
    require(
        ready.stable_texture_cache_key.find("textures/card.ppm") != std::string::npos,
        "real decoded resource preserves stable cache identity");
    require(
        ready.sampler_key == render_image_sampler_policy_stable_fragment(nearest_sampler),
        "real decoded resource preserves sampler identity");

    const render_image_texture_frame_resource_packet_consumption_entry& placeholder = consumption.entries[1];
    require(placeholder.ok(), "placeholder decoded resource consumption entry is ready");
    require(placeholder.placeholder_backed, "placeholder decoded resource records placeholder state");
    require(placeholder.decoded_payload_valid, "placeholder decoded resource has valid decoded bytes");
    require(placeholder.decoded_byte_count == 16, "placeholder decoded resource records generated RGBA bytes");
    require(placeholder.staging_payload_byte_count == 16, "placeholder decoded resource records staging bytes");
    require(
        is_fake_image_texture_placeholder_key(materialization.entries[1].cache_record.texture_key),
        "placeholder materialization preserves placeholder texture key");
}

void test_standard_pipeline_threads_decoded_bytes_into_draw_payloads()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/card.ppm", make_ppm_bytes());
    set_source_bytes(loader, "textures/bad.ppm", make_short_ppm_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);
    pipeline.set_placeholder_texture_policy(placeholder_policy);
    image_texture_pipeline_interface& pipeline_interface = pipeline;

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    render_draw_list draw_list;
    draw_list.commands = {
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "card-image",
            .parent_node_id = "card",
            .bounds = render_rect{.x = 0.0f, .y = 0.0f, .width = 10.0f, .height = 10.0f},
            .content_bounds = render_rect{.x = 1.0f, .y = 1.0f, .width = 8.0f, .height = 8.0f},
            .image = render_image_ref{.uri = "textures/card.ppm", .alt_text = "card", .sampler = nearest_sampler},
        },
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "fallback-image",
            .parent_node_id = "card",
            .bounds = render_rect{.x = 12.0f, .y = 0.0f, .width = 10.0f, .height = 10.0f},
            .content_bounds = render_rect{.x = 12.0f, .y = 0.0f, .width = 10.0f, .height = 10.0f},
            .image = render_image_ref{.uri = "textures/bad.ppm", .alt_text = "fallback"},
        },
    };

    const render_image_draw_list_frame_handoff_snapshot handoff =
        make_render_image_draw_list_frame_handoff_snapshot(draw_list, "decoded-draw-frame");
    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(
        handoff,
        resolver,
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline_interface);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_pipeline_upload_cache_snapshot side_snapshot =
        render_image_texture_pipeline_upload_cache_diagnostic_snapshot(pipeline_interface);
    const render_image_texture_pipeline_upload_cache_payload_summary upload_cache_summary =
        render_image_texture_pipeline_upload_cache_payload_diagnostic_summary(pipeline_interface);
    const render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
            side_snapshot.upload_snapshot);
    const render_image_texture_frame_resource_packet_plan resources =
        make_render_image_texture_frame_resource_packet_plan(frame, upload_result);
    const render_image_texture_frame_resource_packet_materialization materialization =
        materialize_render_image_texture_frame_resource_packets(resources);
    const render_image_texture_frame_resource_packet_consumption_summary consumption =
        make_render_image_texture_frame_resource_packet_consumption_summary(materialization);
    const render_image_draw_list_texture_frame_composition composition =
        make_render_image_draw_list_texture_frame_composition(handoff, plan, frame, resources);
    const render_image_renderer_texture_quad_packet_summary quad_summary =
        make_render_image_renderer_texture_quad_packet_summary_with_resource_consumption(
            make_render_image_renderer_texture_quad_packet_summary(composition),
            consumption);
    const render_image_renderer_texture_quad_draw_payload_frame payload_frame =
        make_render_image_renderer_texture_quad_draw_payload_frame_with_upload_cache_payload_summary(
            quad_summary,
            pipeline_interface);

    require(execution.ok(), "standard draw payload batch executes with placeholder fallback");
    require(upload_cache_summary.ok(), "standard upload/cache payload summary is ready");
    require(consumption.ok(), "standard draw payload consumption is renderer-ready");
    require(quad_summary.ok(), "standard draw payload quad summary is ready");
    require(payload_frame.ok(), "standard draw payload frame is consumable");
    require(payload_frame.payload_count == 2, "standard draw payload frame records both image commands");
    require(payload_frame.draw_ready_payload_count == 1, "standard draw payload frame records one real texture payload");
    require(payload_frame.placeholder_payload_count == 1, "standard draw payload frame records one placeholder payload");
    require(payload_frame.fallback_placeholder_payload_count == 0, "standard draw payload frame does not use draw-time fallback");
    require(payload_frame.decoded_resource_ready_payload_count == 2, "standard draw payload frame counts decoded resources");
    require(payload_frame.decoded_payload_byte_count == 20, "standard draw payload frame sums decoded bytes");
    require(payload_frame.staging_payload_byte_count == 20, "standard draw payload frame sums staging bytes");
    require(payload_frame.upload_cache_payload_evidence_count == 2, "standard draw payload frame attaches upload/cache evidence");
    require(payload_frame.upload_cache_payload_ready_count == 2, "standard draw payload frame counts upload/cache-ready payloads");
    require(payload_frame.upload_cache_placeholder_payload_count == 1, "standard draw payload frame counts upload/cache placeholders");
    require(payload_frame.upload_cache_payload_blocked_count == 0, "standard draw payload frame has no upload/cache blockers");
    require(payload_frame.upload_cache_decoded_byte_count == 20, "standard draw payload frame sums upload/cache decoded bytes");
    require(payload_frame.upload_cache_staging_byte_count == 20, "standard draw payload frame sums upload/cache staging bytes");
    require(payload_frame.upload_cache_uploaded_byte_count == 20, "standard draw payload frame sums upload/cache uploaded bytes");
    require(
        payload_frame.decoded_resource_summary
            == "decoded_payloads=2; payload_hashes=2; decoded_bytes=20; staging_bytes=20",
        "standard draw payload decoded summary is stable");
    require(
        payload_frame.upload_cache_payload_summary
            == "upload_cache_payloads=2; ready=2; placeholders=1; blocked=0; payload_hashes=2; decoded_bytes=20; staging_bytes=20; uploaded_bytes=20",
        "standard draw payload upload/cache summary is stable");

    const render_image_renderer_texture_quad_draw_payload& ready = payload_frame.payloads[0];
    require(ready.draw_ready, "standard draw payload keeps real texture draw-ready");
    require(ready.upload_cache_payload_evidence_present, "standard draw payload attaches upload/cache evidence");
    require(ready.upload_cache_payload_identity_matched, "standard draw payload matches upload/cache identity");
    require(ready.upload_cache_payload_ready, "standard draw payload records upload/cache readiness");
    require(ready.decoded_resource_ready, "standard draw payload preserves decoded readiness");
    require(ready.decoded_payload_hash == consumption.entries[0].decoded_payload_hash, "standard draw payload preserves decoded hash");
    require(
        ready.upload_cache_decoded_payload_hash == ready.decoded_payload_hash,
        "standard draw payload upload/cache hash matches decoded resource hash");
    require(ready.decoded_byte_count == 4, "standard draw payload records real RGBA byte count");
    require(ready.upload_cache_decoded_byte_count == 4, "standard draw payload records upload/cache decoded bytes");
    require(ready.upload_layout_byte_count == 4, "standard draw payload records upload layout bytes");
    require(ready.staging_payload_byte_count == 4, "standard draw payload records staging bytes");
    require(ready.upload_cache_uploaded_byte_count == 4, "standard draw payload records upload/cache uploaded bytes");
    require(ready.upload_generation_id == consumption.entries[0].upload_generation_id, "standard draw payload preserves upload generation");
    require(ready.stable_texture_cache_key.find("textures/card.ppm") != std::string::npos, "standard draw payload preserves cache key");
    require(
        ready.upload_cache_stable_cache_key == ready.stable_texture_cache_key,
        "standard draw payload upload/cache evidence preserves cache identity");
    require(ready.sampler_key == render_image_sampler_policy_stable_fragment(nearest_sampler), "standard draw payload preserves sampler key");

    const render_image_renderer_texture_quad_draw_payload& placeholder = payload_frame.payloads[1];
    require(placeholder.placeholder_backed, "standard draw payload keeps placeholder-backed texture");
    require(!placeholder.fallback_placeholder, "standard draw payload distinguishes uploaded placeholder from draw fallback");
    require(placeholder.upload_cache_placeholder_backed, "standard placeholder draw payload attaches upload/cache placeholder evidence");
    require(
        placeholder.upload_cache_placeholder_reason == fake_image_texture_placeholder_reason::decode_failed,
        "standard placeholder draw payload records upload/cache placeholder reason");
    require(placeholder.decoded_resource_ready, "standard placeholder draw payload has decoded placeholder bytes");
    require(placeholder.decoded_byte_count == 16, "standard placeholder draw payload records placeholder RGBA bytes");
    require(placeholder.upload_cache_decoded_byte_count == 16, "standard placeholder draw payload records upload/cache decoded bytes");
    require(placeholder.staging_payload_byte_count == 16, "standard placeholder draw payload records placeholder staging bytes");
}

void test_standard_pipeline_threads_png_decoded_bytes_into_draw_payloads()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(
        loader,
        "textures/card.png",
        make_png_bytes(make_zlib_stored_stream(make_filter_none_scanlines())));
    standard_image_texture_pipeline pipeline(resolver, loader);

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    render_draw_list draw_list;
    draw_list.commands = {
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "png-card-image",
            .parent_node_id = "card",
            .bounds = render_rect{.x = 2.0f, .y = 3.0f, .width = 12.0f, .height = 12.0f},
            .content_bounds = render_rect{.x = 3.0f, .y = 4.0f, .width = 10.0f, .height = 9.0f},
            .image = render_image_ref{
                .uri = "textures/card.png",
                .alt_text = "png card",
                .aspect_ratio = 1.0f,
                .sampler = nearest_sampler,
            },
        },
    };

    const render_image_draw_list_frame_handoff_snapshot handoff =
        make_render_image_draw_list_frame_handoff_snapshot(draw_list, "png-draw-frame");
    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(handoff, resolver);
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);
    const standard_image_texture_pipeline_snapshot standard_snapshot =
        pipeline.standard_diagnostic_snapshot();

    require(execution.ok(), "standard PNG draw payload batch executes");
    require(standard_snapshot.pipeline.entries.size() == 1, "standard PNG draw payload records one pipeline entry");
    require(
        standard_snapshot.pipeline.entries[0].external_decoder_selection.detected_format
            == render_image_encoded_format::png,
        "standard PNG draw payload records PNG selection");
    if (standard_snapshot.pipeline.entries[0].external_decoder_selection.ready_for_external_decode) {
        require(
            standard_snapshot.pipeline.entries[0].decode_metadata.decoder_id == "stb_image_decoder",
            "standard PNG draw payload uses STB when available");
        require(
            standard_snapshot.pipeline.entries[0].external_decoder_selection.used_third_party_adapter,
            "standard PNG draw payload records adapter route");
    } else {
        require(
            standard_snapshot.pipeline.entries[0].decode_metadata.decoder_id == "png_image_decoder",
            "standard PNG draw payload falls back to internal PNG decoder");
        require(
            standard_snapshot.pipeline.entries[0].external_decoder_selection.fallback_to_standard_decoder_chain,
            "standard PNG draw payload records standard fallback route");
    }

    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
            standard_snapshot.pipeline.upload_snapshot);
    const render_image_texture_frame_resource_packet_plan resources =
        make_render_image_texture_frame_resource_packet_plan(frame, upload_result);
    const render_image_texture_frame_resource_packet_materialization materialization =
        materialize_render_image_texture_frame_resource_packets(resources);
    const render_image_texture_frame_resource_packet_consumption_summary consumption =
        make_render_image_texture_frame_resource_packet_consumption_summary(materialization);
    const render_image_draw_list_texture_frame_composition composition =
        make_render_image_draw_list_texture_frame_composition(handoff, plan, frame, resources);
    const render_image_renderer_texture_quad_packet_summary quad_summary =
        make_render_image_renderer_texture_quad_packet_summary_with_resource_consumption(
            make_render_image_renderer_texture_quad_packet_summary(composition),
            consumption);
    const render_image_renderer_texture_quad_draw_payload_frame payload_frame =
        make_render_image_renderer_texture_quad_draw_payload_frame(quad_summary);

    require(materialization.ok(), "standard PNG resource packets materialize");
    require(consumption.ok(), "standard PNG resource consumption is ready");
    require(payload_frame.ok(), "standard PNG draw payload frame is ready");
    require(payload_frame.payload_count == 1, "standard PNG draw payload frame records one payload");
    require(payload_frame.draw_ready_payload_count == 1, "standard PNG draw payload is draw-ready");
    require(payload_frame.decoded_resource_ready_payload_count == 1, "standard PNG draw payload counts decoded resource");
    require(payload_frame.decoded_payload_byte_count == 16, "standard PNG draw payload sums RGBA bytes");
    require(payload_frame.staging_payload_byte_count == 16, "standard PNG draw payload sums staging bytes");
    require(payload_frame.decoded_payload_hash_count == 1, "standard PNG draw payload counts decoded hash");

    const render_image_renderer_texture_quad_draw_payload& payload = payload_frame.payloads[0];
    require(payload.draw_ready, "standard PNG payload is draw-ready");
    require(payload.draw_command_index == 0, "standard PNG payload preserves command index");
    require(payload.node_id == "png-card-image", "standard PNG payload preserves node id");
    require(payload.parent_node_id == "card", "standard PNG payload preserves parent node id");
    require(payload.bounds.width == 12.0f, "standard PNG payload preserves bounds");
    require(payload.content_bounds.width == 10.0f, "standard PNG payload preserves content bounds");
    require(payload.decoded_resource_ready, "standard PNG payload preserves decoded readiness");
    require(payload.decoded_payload_hash == consumption.entries[0].decoded_payload_hash, "standard PNG payload preserves decoded hash");
    require(payload.decoded_byte_count == 16, "standard PNG payload records decoded RGBA bytes");
    require(payload.upload_layout_byte_count == 16, "standard PNG payload records upload layout bytes");
    require(payload.staging_payload_byte_count == 16, "standard PNG payload records staging bytes");
    require(
        payload.stable_texture_cache_key.find("textures/card.png") != std::string::npos,
        "standard PNG payload preserves cache key");
    require(
        payload.sampler_key == render_image_sampler_policy_stable_fragment(nearest_sampler),
        "standard PNG payload preserves sampler identity");
}

void test_standard_pipeline_threads_stb_jpeg_decoded_bytes_into_draw_payloads()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/card.jpg", make_jpeg_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);

    render_image_sampler_policy linear_sampler;
    linear_sampler.min_filter = render_image_filter::linear;
    linear_sampler.mag_filter = render_image_filter::linear;

    render_draw_list draw_list;
    draw_list.commands = {
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "jpeg-card-image",
            .parent_node_id = "card",
            .bounds = render_rect{.x = 2.0f, .y = 3.0f, .width = 11.0f, .height = 13.0f},
            .content_bounds = render_rect{.x = 3.0f, .y = 4.0f, .width = 9.0f, .height = 10.0f},
            .image = render_image_ref{
                .uri = "textures/card.jpg",
                .alt_text = "jpeg card",
                .aspect_ratio = 1.0f,
                .sampler = linear_sampler,
            },
        },
    };

    const render_image_draw_list_frame_handoff_snapshot handoff =
        make_render_image_draw_list_frame_handoff_snapshot(draw_list, "jpeg-draw-frame");
    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(handoff, resolver);
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);
    const standard_image_texture_pipeline_snapshot standard_snapshot =
        pipeline.standard_diagnostic_snapshot();

    require(
        standard_snapshot.pipeline.entries.size() == 1,
        "standard JPEG draw payload records one pipeline entry");
    require(
        standard_snapshot.pipeline.entries[0].external_decoder_selection.detected_format
            == render_image_encoded_format::jpeg,
        "standard JPEG draw payload records JPEG selection");
    if (!standard_snapshot.pipeline.entries[0].external_decoder_selection.ready_for_external_decode) {
        require(!execution.ok(), "standard JPEG draw payload remains blocked without STB");
        require(
            standard_snapshot.pipeline.entries[0].external_decoder_selection.fallback_to_standard_decoder_chain,
            "standard JPEG draw payload records internal fallback without STB");
        return;
    }

    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
            standard_snapshot.pipeline.upload_snapshot);
    const render_image_texture_frame_resource_packet_plan resources =
        make_render_image_texture_frame_resource_packet_plan(frame, upload_result);
    const render_image_texture_frame_resource_packet_materialization materialization =
        materialize_render_image_texture_frame_resource_packets(resources);
    const render_image_texture_frame_resource_packet_consumption_summary consumption =
        make_render_image_texture_frame_resource_packet_consumption_summary(materialization);
    const render_image_draw_list_texture_frame_composition composition =
        make_render_image_draw_list_texture_frame_composition(handoff, plan, frame, resources);
    const render_image_renderer_texture_quad_packet_summary quad_summary =
        make_render_image_renderer_texture_quad_packet_summary_with_resource_consumption(
            make_render_image_renderer_texture_quad_packet_summary(composition),
            consumption);
    const render_image_renderer_texture_quad_draw_payload_frame payload_frame =
        make_render_image_renderer_texture_quad_draw_payload_frame(quad_summary);

    require(execution.ok(), "standard JPEG draw payload batch executes");
    require(materialization.ok(), "standard JPEG resource packets materialize");
    require(consumption.ok(), "standard JPEG resource consumption is ready");
    require(payload_frame.ok(), "standard JPEG draw payload frame is ready");
    require(payload_frame.payload_count == 1, "standard JPEG draw payload frame records one payload");
    require(payload_frame.draw_ready_payload_count == 1, "standard JPEG draw payload is draw-ready");
    require(payload_frame.decoded_resource_ready_payload_count == 1, "standard JPEG draw payload counts decoded resource");
    require(payload_frame.decoded_payload_byte_count == 4, "standard JPEG draw payload sums RGBA bytes");
    require(payload_frame.staging_payload_byte_count == 4, "standard JPEG draw payload sums staging bytes");
    require(payload_frame.decoded_payload_hash_count == 1, "standard JPEG draw payload counts decoded hash");

    const render_image_renderer_texture_quad_draw_payload& payload = payload_frame.payloads[0];
    require(payload.draw_ready, "standard JPEG payload is draw-ready");
    require(payload.draw_command_index == 0, "standard JPEG payload preserves command index");
    require(payload.node_id == "jpeg-card-image", "standard JPEG payload preserves node id");
    require(payload.parent_node_id == "card", "standard JPEG payload preserves parent node id");
    require(payload.bounds.width == 11.0f, "standard JPEG payload preserves bounds");
    require(payload.content_bounds.width == 9.0f, "standard JPEG payload preserves content bounds");
    require(payload.decoded_resource_ready, "standard JPEG payload preserves decoded readiness");
    require(payload.decoded_payload_hash == consumption.entries[0].decoded_payload_hash, "standard JPEG payload preserves decoded hash");
    require(payload.decoded_byte_count == 4, "standard JPEG payload records decoded RGBA bytes");
    require(payload.upload_layout_byte_count == 4, "standard JPEG payload records upload layout bytes");
    require(payload.staging_payload_byte_count == 4, "standard JPEG payload records staging bytes");
    require(
        payload.stable_texture_cache_key.find("textures/card.jpg") != std::string::npos,
        "standard JPEG payload preserves cache key");
    require(
        payload.sampler_key == render_image_sampler_policy_stable_fragment(linear_sampler),
        "standard JPEG payload preserves sampler identity");
}

void test_standard_pipeline_draw_payload_fallback_preserves_decode_blocker()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/short.ppm", make_short_ppm_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);
    image_texture_pipeline_interface& pipeline_interface = pipeline;

    render_draw_list draw_list;
    draw_list.commands = {
        render_draw_command{
            .type = render_draw_command_type::image,
            .node_id = "blocked-image",
            .parent_node_id = "root",
            .bounds = render_rect{.x = 0.0f, .y = 0.0f, .width = 8.0f, .height = 8.0f},
            .content_bounds = render_rect{.x = 0.0f, .y = 0.0f, .width = 8.0f, .height = 8.0f},
            .image = render_image_ref{.uri = "textures/short.ppm", .alt_text = "bad"},
        },
    };

    const render_image_draw_list_frame_handoff_snapshot handoff =
        make_render_image_draw_list_frame_handoff_snapshot(draw_list, "decoded-blocker-frame");
    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(handoff, resolver);
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline_interface);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_pipeline_upload_cache_snapshot side_snapshot =
        render_image_texture_pipeline_upload_cache_diagnostic_snapshot(pipeline_interface);
    const render_image_texture_pipeline_upload_cache_payload_summary upload_cache_summary =
        render_image_texture_pipeline_upload_cache_payload_diagnostic_summary(pipeline_interface);
    const render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
            side_snapshot.upload_snapshot);
    const render_image_texture_frame_resource_packet_plan resources =
        make_render_image_texture_frame_resource_packet_plan(frame, upload_result);
    const render_image_texture_frame_resource_packet_materialization materialization =
        materialize_render_image_texture_frame_resource_packets(resources);
    const render_image_texture_frame_resource_packet_consumption_summary consumption =
        make_render_image_texture_frame_resource_packet_consumption_summary(materialization);
    const render_image_draw_list_texture_frame_composition composition =
        make_render_image_draw_list_texture_frame_composition(handoff, plan, frame, resources);
    const render_image_renderer_texture_quad_packet_summary quad_summary =
        make_render_image_renderer_texture_quad_packet_summary_with_resource_consumption(
            make_render_image_renderer_texture_quad_packet_summary(composition),
            consumption);

    render_image_renderer_texture_quad_draw_payload_options options;
    options.placeholder_policy.enabled = true;
    options.placeholder_policy.width = 2;
    options.placeholder_policy.height = 2;
    options.placeholder_policy.source_key_prefix = "placeholder://decoded-draw/";
    const render_image_renderer_texture_quad_draw_payload_frame payload_frame =
        make_render_image_renderer_texture_quad_draw_payload_frame_with_upload_cache_payload_summary(
            quad_summary,
            upload_cache_summary,
            options);

    require(!execution.ok(), "standard draw payload blocker batch fails before fallback payload");
    require(!upload_cache_summary.ok(), "standard draw payload blocker upload/cache summary is blocked");
    require(!consumption.ok(), "standard draw payload blocker consumption is blocked");
    require(!quad_summary.ok(), "standard draw payload blocker quad summary is blocked");
    require(payload_frame.ok(), "draw-time placeholder policy makes blocked payload consumable");
    require(payload_frame.placeholder_payload_count == 1, "draw-time fallback produces placeholder payload");
    require(payload_frame.fallback_placeholder_payload_count == 1, "draw-time fallback is counted separately");
    require(payload_frame.has_decoded_resource_blockers, "draw-time fallback keeps decoded blocker aggregate");
    require(payload_frame.has_upload_cache_payload_blockers, "draw-time fallback keeps upload/cache blocker aggregate");
    require(payload_frame.upload_cache_payload_evidence_count == 1, "draw-time fallback attaches upload/cache blocker evidence");
    require(payload_frame.upload_cache_payload_blocked_count == 1, "draw-time fallback counts upload/cache blocker");
    require(
        payload_frame.upload_cache_payload_blocker_summary.find("decode_failed=1") != std::string::npos,
        "draw-time fallback records upload/cache decode blocker");

    const render_image_renderer_texture_quad_draw_payload& payload = payload_frame.payloads[0];
    require(payload.placeholder_backed, "blocked decoded payload becomes placeholder-backed under policy");
    require(payload.fallback_placeholder, "blocked decoded payload uses draw-time fallback");
    require(payload.upload_cache_payload_blocked, "fallback payload preserves upload/cache blocked state");
    require(
        payload.upload_cache_payload_blocker_summary.find("decode_failed=1") != std::string::npos,
        "fallback payload preserves upload/cache blocker summary");
    require(payload.decoded_resource_blocked, "fallback payload preserves decoded blocked state");
    require(
        payload.decoded_resource_blocker_summary.find("pixel data size does not match dimensions")
            != std::string::npos,
        "fallback payload preserves decoder byte-count blocker");
    require(
        payload.diagnostic.find("using deterministic placeholder texture for upload_failed") == 0,
        "fallback payload diagnostic remains placeholder-friendly");
}

void test_standard_pipeline_materialization_reports_source_and_decode_blockers()
{
    using namespace quiz_vulkan::render;

    normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    set_source_bytes(loader, "textures/card.jpg", make_bytes({0xff, 0xd8, 0xff, 0xd9}));
    set_source_bytes(loader, "textures/zero.ppm", make_zero_dimension_ppm_bytes());
    set_source_bytes(loader, "textures/short.ppm", make_short_ppm_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{
            render_image_ref{.uri = "textures/missing.ppm"},
            render_image_ref{.uri = "textures/card.jpg"},
            render_image_ref{.uri = "textures/zero.ppm"},
            render_image_ref{.uri = "textures/short.ppm"},
        },
        resolver);
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
            pipeline.standard_diagnostic_snapshot().pipeline.upload_snapshot);
    const render_image_texture_frame_resource_packet_materialization materialization =
        materialize_render_image_texture_frame_resource_packets(frame, upload_result);
    const render_image_texture_frame_resource_packet_consumption_summary consumption =
        make_render_image_texture_frame_resource_packet_consumption_summary(materialization);

    require(!execution.ok(), "standard blocker batch records failed texture requests");
    require(!materialization.ok(), "standard blocker materialization is not renderer-ready");
    require(!consumption.ok(), "standard blocker consumption summary is not ready");
    require(consumption.blocked_packet_count == 4, "standard blocker consumption records blocked packets");
    require(consumption.decoded_resource_blocked_count == 4, "standard blocker consumption counts decoded blockers");
    require(consumption.decoded_resource_ready_count == 0, "standard blocker consumption records no decoded resources");
    require(consumption.has_decoded_resource_blockers, "standard blocker consumption exposes decoded blockers");
    require(
        consumption.decoded_resource_blocker_summary.find("fake image source bytes loader has no bytes for source")
            != std::string::npos,
        "standard blocker summary preserves missing source bytes reason");
    require(
        consumption.decoded_resource_blocker_summary.find("decoder chain exhausted all candidates")
                != std::string::npos
            || consumption.decoded_resource_blocker_summary.find("stb_image decoder failed")
                != std::string::npos
            || consumption.decoded_resource_blocker_summary.find("no image decoder in the chain supports the source")
                != std::string::npos,
        "standard blocker summary preserves unsupported format reason");
    require(
        consumption.decoded_resource_blocker_summary.find("positive width, height, and max value")
            != std::string::npos,
        "standard blocker summary preserves invalid dimension reason");
    require(
        consumption.decoded_resource_blocker_summary.find("pixel data size does not match dimensions")
            != std::string::npos,
        "standard blocker summary preserves pixel byte-count mismatch reason");
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
    require(result.texture.decoder_diagnostics.size() >= 4, "unsupported JPEG records adapter and standard candidates");
    require(result.texture.decoder_diagnostics.front().decoder_id == "stb_image_decoder", "unsupported JPEG records stb diagnostic first");
    require(result.texture.decoder_diagnostics.back().terminal_candidate, "unsupported JPEG terminates on final candidate");
    require(
        result.texture.decoder_diagnostics.back().diagnostic == "decoder chain exhausted all candidates",
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
    require(result.texture.decoder_diagnostics.size() >= 3, "malformed PPM records adapter and standard candidates");
    require(result.texture.decoder_diagnostics.front().decoder_id == "stb_image_decoder", "malformed PPM records stb diagnostic first");
    require(result.texture.decoder_diagnostics.back().decoder_id == "ppm_image_decoder", "malformed PPM records PPM");
    require(result.texture.decoder_diagnostics.back().decode_attempted, "malformed PPM attempts PPM decode");
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
    require(result.texture.decoder_diagnostics.size() >= 4, "bad PNG records adapter and standard candidates");
    require(result.texture.decoder_diagnostics.front().decoder_id == "stb_image_decoder", "bad PNG records stb diagnostic first");
    require(result.texture.decoder_diagnostics.back().decoder_id == "png_image_decoder", "bad PNG records PNG");
    require(
        result.texture.decoder_diagnostics.back().decode_diagnostic.find("inflater_failed") != std::string::npos,
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
    test_standard_pipeline_uploads_jpeg_through_stb_when_available();
    test_standard_pipeline_reuses_cached_decode_and_upload_for_same_normalized_key();
    test_standard_pipeline_reuses_upload_residency_across_frames_and_fallbacks();
    test_standard_pipeline_decodes_and_uploads_distinct_cache_revision_after_invalidation();
    test_standard_pipeline_materializes_decoded_bytes_for_resource_consumption();
    test_standard_pipeline_threads_decoded_bytes_into_draw_payloads();
    test_standard_pipeline_threads_png_decoded_bytes_into_draw_payloads();
    test_standard_pipeline_threads_stb_jpeg_decoded_bytes_into_draw_payloads();
    test_standard_pipeline_draw_payload_fallback_preserves_decode_blocker();
    test_standard_pipeline_materialization_reports_source_and_decode_blockers();
    test_standard_pipeline_reports_unsupported_decode_with_candidate_diagnostics();
    test_standard_pipeline_reports_invalid_decode_with_candidate_diagnostics();
    test_standard_pipeline_reports_invalid_png_inflater_failure();
    return 0;
}
