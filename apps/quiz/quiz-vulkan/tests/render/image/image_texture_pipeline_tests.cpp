#include "render/image/image_decoder.h"
#include "render/image/image_manifest_texture_pipeline.h"
#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_cache.h"
#include "render/image/image_texture_pipeline.h"
#include "render/image/third_party_image_decoder_adapter.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

template <typename T>
concept HasFakeCacheSnapshotField = requires(T value) {
    { value.cache_snapshot } -> std::same_as<quiz_vulkan::render::fake_image_texture_cache_snapshot&>;
};

template <typename T>
concept HasFakeUploadSnapshotField = requires(T value) {
    { value.upload_snapshot } -> std::same_as<quiz_vulkan::render::fake_image_texture_upload_snapshot&>;
};

static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_manifest_texture_entry_snapshot>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_manifest_texture_pipeline_snapshot>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_manifest_texture_entry_snapshot>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_manifest_texture_pipeline_snapshot>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_batch_plan_entry>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_batch_plan>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_batch_plan_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_batch_plan>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_batch_execution_entry>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_batch_execution_diagnostics>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_batch_execution_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_batch_execution_diagnostics>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_residency_budget_plan_entry>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_residency_budget_plan>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_residency_budget_plan_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_residency_budget_plan>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_residency_budget_summary>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_residency_budget_summary>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_handle_map_entry>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_handle_map_diagnostics>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_handle_map_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_handle_map_diagnostics>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_entry_snapshot>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_snapshot>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_entry_snapshot>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_snapshot>);

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

std::vector<std::byte> make_ppm_2x1_fixture_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n# deterministic image pipeline fixture\n2 1\n255\n");
    append_byte(bytes, 0xff);
    append_byte(bytes, 0x00);
    append_byte(bytes, 0x00);
    append_byte(bytes, 0x00);
    append_byte(bytes, 0xff);
    append_byte(bytes, 0x00);
    return bytes;
}

std::vector<std::byte> make_short_ppm_2x1_fixture_bytes()
{
    std::vector<std::byte> bytes;
    append_ascii(bytes, "P6\n2 1\n255\n");
    append_byte(bytes, 0xff);
    append_byte(bytes, 0x00);
    append_byte(bytes, 0x00);
    return bytes;
}

std::vector<std::byte> make_jpeg_signature_bytes()
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 0xff);
    append_byte(bytes, 0xd8);
    append_byte(bytes, 0xff);
    append_byte(bytes, 0xd9);
    return bytes;
}

quiz_vulkan::render::render_decoded_image make_rgba_1x1_image(
    unsigned char red,
    unsigned char green,
    unsigned char blue,
    unsigned char alpha)
{
    return quiz_vulkan::render::render_decoded_image{
        .width = 1,
        .height = 1,
        .pixel_format = quiz_vulkan::render::render_image_pixel_format::rgba8_srgb,
        .pixels = {
            std::byte{red},
            std::byte{green},
            std::byte{blue},
            std::byte{alpha},
        },
    };
}

std::filesystem::path test_data_root()
{
    return std::filesystem::current_path() / "image_texture_pipeline_test_data";
}

void reset_test_data_root(const std::filesystem::path& root)
{
    std::error_code error;
    std::filesystem::remove_all(root, error);
    require(!error, "old image pipeline test data root can be removed");
    std::filesystem::create_directories(root, error);
    require(!error, "image pipeline test data root can be created");
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::byte>& bytes)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    require(!error, "image pipeline fixture parent directory can be created");

    std::ofstream file(path, std::ios::binary);
    require(file.good(), "image pipeline fixture file can be opened");
    if (!bytes.empty()) {
        file.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    require(file.good(), "image pipeline fixture bytes can be written");
}

void test_filesystem_pipeline_reads_ppm_fixture_and_reuses_cache()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = test_data_root();
    reset_test_data_root(root);
    const std::vector<std::byte> fixture_bytes = make_ppm_2x1_fixture_bytes();
    write_bytes(root / "textures" / "pipeline-fixture.ppm", fixture_bytes);

    const normalizing_image_resolver resolver;
    filesystem_image_source_bytes_loader loader(root);
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    render_image_sampler_policy sampler;
    sampler.mag_filter = render_image_filter::nearest;
    sampler.wrap_u = render_image_wrap_mode::repeat;

    const render_image_texture_pipeline_request request{
        .uri = "  ./textures\\pipeline-fixture.ppm  ",
        .sampler = sampler,
    };
    const render_image_texture_pipeline_result first = pipeline.acquire_texture(request);
    const render_image_texture_pipeline_result second = pipeline.acquire_texture(request);

    require(first.ok(), "filesystem image pipeline creates a texture from a PPM fixture");
    require(first.status == render_image_texture_pipeline_status::ready, "first filesystem pipeline acquire is ready");
    require(first.resolve.ok(), "filesystem image pipeline resolves before loading");
    require(first.resolve.source.kind == render_image_source_kind::local_path, "fixture resolves as a local path");
    require(
        first.resolve.source.cache_key() == "textures/pipeline-fixture.ppm",
        "filesystem image pipeline normalizes the fixture cache key");
    require(first.source_bytes.ok(), "filesystem image pipeline loads fixture bytes");
    require(first.source_bytes.status == render_image_source_bytes_load_status::loaded, "fixture load reports loaded");
    require(first.source_bytes.encoded_bytes == fixture_bytes, "filesystem image pipeline reads fixture file bytes");
    require(first.texture.ok(), "filesystem image pipeline reaches texture cache ready state");
    require(!first.texture.cache_hit, "first filesystem image pipeline acquire is a cache miss");
    require(first.texture.texture.width == 2, "filesystem image pipeline preserves decoded fixture width");
    require(first.texture.texture.height == 1, "filesystem image pipeline preserves decoded fixture height");
    require(first.texture.key.sampler == sampler, "filesystem image pipeline preserves sampler in texture key");
    require(first.texture.decode_metadata.decoder_id == "ppm_image_decoder", "fixture decode uses the PPM decoder");
    require(first.texture.decode_metadata.width == 2, "fixture decode metadata records width");
    require(first.texture.decode_metadata.height == 1, "fixture decode metadata records height");
    require(first.texture.decode_metadata.decoded_byte_count == 8, "fixture decode metadata records RGBA byte count");
    require(
        first.texture.decode_metadata.format_detection.detected_format == render_image_encoded_format::ppm,
        "fixture decode metadata records PPM format");
    require(
        first.texture.decode_metadata.format_detection.recognized_signature,
        "fixture decode metadata records recognized PPM signature");

    require(second.ok(), "repeat filesystem image pipeline acquire succeeds");
    require(second.status == render_image_texture_pipeline_status::ready, "repeat filesystem acquire is ready");
    require(second.source_bytes.ok(), "repeat filesystem acquire still validates source bytes");
    require(second.texture.cache_hit, "repeat filesystem image pipeline acquire is a cache hit");
    require(second.texture.texture.id == first.texture.texture.id, "cache hit reuses the texture handle");
    require(loader.load_requests.size() == 2, "filesystem loader is consulted before each cache acquire");
    require(uploader.upload_requests.size() == 1, "cache hit avoids a second texture upload");
    require(cache.cached_texture_count() == 1, "filesystem image pipeline stores one cached texture");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.acquire_count == 2, "filesystem pipeline snapshot records both acquires");
    require(snapshot.ready_count == 2, "filesystem pipeline snapshot records ready count");
    require(snapshot.failure_count == 0, "filesystem pipeline snapshot records no failures");
    require(snapshot.cache_hit_count == 1, "filesystem pipeline snapshot counts the repeat cache hit");
    require(snapshot.entries.size() == 2, "filesystem pipeline snapshot records entries");
    require(snapshot.entries[0].source_bytes_status == render_image_source_bytes_load_status::loaded, "snapshot records loaded source bytes");
    require(snapshot.entries[0].encoded_byte_count == fixture_bytes.size(), "snapshot records fixture byte count");
    require(snapshot.entries[0].texture_status == render_image_texture_status::ready, "snapshot records ready texture");
    require(snapshot.entries[0].selected_decoder_id == "ppm_image_decoder", "snapshot records selected decoder");
    require(!snapshot.entries[0].cache_reused, "first snapshot records no cache reuse");
    require(!snapshot.entries[0].placeholder_texture, "first snapshot records no placeholder outcome");
    require(
        snapshot.entries[0].decoder_capability_manifest.candidates.empty(),
        "direct PPM decoder snapshot has no chain candidate list");
    require(
        snapshot.entries[0].decoder_capability_manifest.terminal_decoder_id == "ppm_image_decoder",
        "snapshot records PPM terminal decoder");
    require(snapshot.entries[0].upload_count_after == 1, "snapshot records first upload");
    require(snapshot.entries[1].cache_hit, "snapshot records repeat cache hit");
    require(snapshot.entries[1].cache_reused, "repeat snapshot records explicit cache reuse");
    require(snapshot.entries[1].selected_decoder_id == "ppm_image_decoder", "repeat snapshot preserves selected decoder");
    require(
        snapshot.entries[1].decoder_capability_manifest.terminal_decoder_id == "ppm_image_decoder",
        "repeat snapshot preserves decoder manifest terminal id from cache");
    require(snapshot.entries[1].upload_count_before == 1, "repeat snapshot observes previous upload");
    require(snapshot.entries[1].upload_count_after == 1, "repeat snapshot records unchanged upload count");
    require(snapshot.upload_snapshot.upload_count == 1, "pipeline upload snapshot records one upload");
    require(snapshot.cache_snapshot.texture_count == 1, "pipeline cache snapshot records one texture");
}

void test_filesystem_pipeline_reports_missing_file_source_load_failed()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = test_data_root();
    reset_test_data_root(root);

    const normalizing_image_resolver resolver;
    filesystem_image_source_bytes_loader loader(root);
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "textures/missing.ppm"});
    require(!result.ok(), "missing fixture file does not create a texture");
    require(
        result.status == render_image_texture_pipeline_status::source_load_failed,
        "missing fixture file reports source load failure");
    require(result.resolve.ok(), "missing fixture file still resolves before loading");
    require(
        result.source_bytes.status == render_image_source_bytes_load_status::missing_bytes,
        "missing fixture file preserves missing bytes status");
    require(result.source_bytes.encoded_bytes.empty(), "missing fixture file returns no bytes");
    require(!result.diagnostic.empty(), "missing fixture file includes pipeline diagnostic");
    require(cache.cached_texture_count() == 0, "missing fixture file does not cache a texture");
    require(uploader.upload_requests.empty(), "missing fixture file does not reach upload");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.acquire_count == 1, "missing file snapshot records acquire");
    require(snapshot.failure_count == 1, "missing file snapshot records failure");
    require(snapshot.source_load_failure_count == 1, "missing file snapshot counts source load failure");
    require(snapshot.entries[0].status == render_image_texture_pipeline_status::source_load_failed, "missing file snapshot records pipeline status");
    require(snapshot.entries[0].source_bytes_status == render_image_source_bytes_load_status::missing_bytes, "missing file snapshot records load status");
    require(snapshot.entries[0].encoded_byte_count == 0, "missing file snapshot records no encoded bytes");
    require(snapshot.entries[0].upload_count_after == 0, "missing file snapshot records no upload");
}

void test_filesystem_pipeline_reports_empty_file_source_load_failed()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = test_data_root();
    reset_test_data_root(root);
    write_bytes(root / "textures" / "empty.ppm", std::vector<std::byte>{});

    const normalizing_image_resolver resolver;
    filesystem_image_source_bytes_loader loader(root);
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "textures/empty.ppm"});
    require(!result.ok(), "empty fixture file does not create a texture");
    require(
        result.status == render_image_texture_pipeline_status::source_load_failed,
        "empty fixture file reports source load failure");
    require(
        result.source_bytes.status == render_image_source_bytes_load_status::empty_bytes,
        "empty fixture file preserves empty bytes status");
    require(result.source_bytes.encoded_bytes.empty(), "empty fixture file returns no bytes");
    require(result.diagnostic.find("empty") != std::string::npos, "empty fixture file diagnostic names empty bytes");
    require(cache.cached_texture_count() == 0, "empty fixture file does not cache a texture");
    require(uploader.upload_requests.empty(), "empty fixture file does not reach upload");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.source_load_failure_count == 1, "empty file snapshot counts source load failure");
    require(snapshot.entries[0].source_bytes_status == render_image_source_bytes_load_status::empty_bytes, "empty file snapshot records empty status");
    require(snapshot.entries[0].diagnostic.find("empty") != std::string::npos, "empty file snapshot keeps diagnostic");
}

void test_filesystem_pipeline_reports_malformed_ppm_decode_failed()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = test_data_root();
    reset_test_data_root(root);
    const std::vector<std::byte> malformed_bytes = make_short_ppm_2x1_fixture_bytes();
    write_bytes(root / "textures" / "malformed.ppm", malformed_bytes);

    const normalizing_image_resolver resolver;
    filesystem_image_source_bytes_loader loader(root);
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "textures/malformed.ppm"});
    require(!result.ok(), "malformed fixture file does not create a texture");
    require(
        result.status == render_image_texture_pipeline_status::decode_failed,
        "malformed fixture file reports decode failure");
    require(result.source_bytes.ok(), "malformed fixture file is loaded before decode failure");
    require(result.source_bytes.encoded_bytes == malformed_bytes, "malformed fixture file bytes are preserved");
    require(result.texture.status == render_image_texture_status::decode_failed, "malformed fixture preserves texture decode status");
    require(!result.diagnostic.empty(), "malformed fixture file includes decode diagnostic");
    require(result.texture.decode_metadata.decoder_id == "ppm_image_decoder", "malformed fixture records decoder id");
    require(
        result.texture.decode_metadata.encoded_byte_count == malformed_bytes.size(),
        "malformed fixture records encoded byte count");
    require(!result.texture.decode_metadata.has_image(), "malformed fixture records no decoded image");
    require(uploader.upload_requests.empty(), "malformed fixture file does not reach upload");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.decode_failure_count == 1, "malformed file snapshot counts decode failure");
    require(snapshot.source_load_failure_count == 0, "malformed file snapshot does not count source load failure");
    require(snapshot.entries[0].source_bytes_status == render_image_source_bytes_load_status::loaded, "malformed file snapshot records loaded bytes");
    require(snapshot.entries[0].texture_status == render_image_texture_status::decode_failed, "malformed file snapshot records decode failure");
    require(!snapshot.entries[0].diagnostic.empty(), "malformed file snapshot keeps diagnostic");
}

void test_pipeline_uses_optional_third_party_decoder_through_interface()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({render_image_encoded_format::jpeg});
    backend.set_decoded_image(make_rgba_1x1_image(9, 8, 7, 6));
    optional_third_party_image_decoder_chain decoder(backend);

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.jpg", make_jpeg_signature_bytes());
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "asset://textures/card.jpg"});

    require(result.ok(), "optional third-party decoder feeds image texture pipeline");
    require(result.status == render_image_texture_pipeline_status::ready, "third-party pipeline result is ready");
    require(result.texture.decode_metadata.decoder_id == "fake_stb_decoder", "third-party pipeline records decoder id");
    require(result.texture.decode_metadata.width == 1, "third-party pipeline records width");
    require(result.texture.decode_metadata.height == 1, "third-party pipeline records height");
    require(
        result.texture.decode_metadata.format_detection.detected_format == render_image_encoded_format::jpeg,
        "third-party pipeline records detected JPEG format");
    require(result.texture.decode_metadata.size_validation.valid, "third-party pipeline validates decoded payload");
    require(result.texture.decoder_diagnostics.size() == 1, "third-party pipeline records adapter diagnostic");
    require(result.texture.decoder_diagnostics[0].decoder_id == "fake_stb_decoder", "adapter diagnostic records decoder id");
    require(result.texture.decoder_diagnostics[0].decode_attempted, "adapter diagnostic records decode attempt");
    require(uploader.upload_requests.size() == 1, "third-party pipeline uploads decoded texture once");
    require(cache.cached_texture_count() == 1, "third-party pipeline caches decoded texture");
    require(backend.decode_requests.size() == 1, "third-party backend is called once");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.acquire_count == 1, "third-party pipeline snapshot records acquire");
    require(snapshot.ready_count == 1, "third-party pipeline snapshot records ready");
    require(snapshot.upload_snapshot.upload_count == 1, "third-party pipeline snapshot records upload");
    require(snapshot.cache_snapshot.texture_count == 1, "third-party pipeline snapshot records cached texture");
    require(snapshot.entries[0].decode_metadata.decoder_id == "fake_stb_decoder", "pipeline entry records third-party decoder");
    require(snapshot.entries[0].decoder_diagnostics[0].decoder_id == "fake_stb_decoder", "pipeline entry records adapter diagnostic");
    require(snapshot.entries[0].selected_decoder_id == "fake_stb_decoder", "pipeline entry records selected third-party decoder");
    require(!snapshot.entries[0].cache_reused, "third-party first acquire records no cache reuse");
    require(!snapshot.entries[0].placeholder_texture, "third-party acquire records no placeholder");
    require(
        snapshot.entries[0].decoder_capability_manifest.used_third_party_adapter,
        "pipeline entry capability manifest records adapter use");
    require(
        !snapshot.entries[0].decoder_capability_manifest.fallback_used,
        "pipeline entry capability manifest records no fallback");
    require(
        snapshot.entries[0].decoder_capability_manifest.terminal_decoder_id == "fake_stb_decoder",
        "pipeline entry capability manifest records adapter terminal decoder");
}

void test_optional_adapter_failure_falls_back_before_texture_upload()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({render_image_encoded_format::ppm});
    backend.set_result(render_image_decode_result{
        .status = render_image_decode_status::invalid_data,
        .image = {},
        .diagnostic = "fake third-party decode failed before fallback",
        .metadata = {},
    });
    optional_third_party_image_decoder_chain decoder(backend);

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "asset://textures/card.ppm"});

    require(result.ok(), "optional decoder fallback feeds image texture pipeline");
    require(result.texture.decode_metadata.decoder_id == "ppm_image_decoder", "fallback pipeline selects PPM decoder");
    require(result.texture.decoder_diagnostics.size() == 3, "fallback pipeline records adapter, BMP, and PPM diagnostics");
    require(result.texture.decoder_diagnostics[0].decoder_id == "fake_stb_decoder", "fallback pipeline records adapter first");
    require(result.texture.decoder_diagnostics[0].decode_attempted, "fallback pipeline records adapter decode attempt");
    require(!result.texture.decoder_diagnostics[0].terminal_candidate, "failed adapter is not terminal before fallback");
    require(result.texture.decoder_diagnostics[1].decoder_id == "bmp_image_decoder", "fallback pipeline records BMP candidate");
    require(result.texture.decoder_diagnostics[2].decoder_id == "ppm_image_decoder", "fallback pipeline records PPM candidate");
    require(result.texture.decoder_diagnostics[2].terminal_candidate, "fallback PPM candidate is terminal");
    require(backend.decode_requests.size() == 1, "failing adapter is called once before fallback");
    require(uploader.upload_requests.size() == 1, "fallback uploads only after standard decode succeeds");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.ready_count == 1, "fallback pipeline snapshot records ready");
    require(snapshot.decode_failure_count == 0, "fallback pipeline snapshot records no terminal decode failure");
    require(snapshot.upload_snapshot.upload_count == 1, "fallback pipeline snapshot records one upload");
    require(snapshot.entries[0].upload_count_before == 0, "fallback upload happens after decode path");
    require(snapshot.entries[0].upload_count_after == 1, "fallback upload count increments once");
    require(snapshot.entries[0].decode_metadata.decoder_id == "ppm_image_decoder", "fallback entry records standard decoder");
    require(snapshot.entries[0].selected_decoder_id == "ppm_image_decoder", "fallback entry records selected decoder");
    require(
        snapshot.entries[0].decoder_capability_manifest.used_third_party_adapter,
        "fallback entry manifest records adapter candidate");
    require(snapshot.entries[0].decoder_capability_manifest.fallback_used, "fallback entry manifest records fallback");
    require(
        snapshot.entries[0].decoder_capability_manifest.terminal_decoder_id == "ppm_image_decoder",
        "fallback entry manifest records terminal PPM decoder");
    require(
        snapshot.entries[0].decoder_fallback_reason.find("failed") != std::string::npos,
        "fallback entry records adapter failure reason");
}

void test_unavailable_optional_adapter_keeps_diagnostics_without_upload()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_available(false);
    backend.set_supported_formats({render_image_encoded_format::jpeg});
    optional_third_party_image_decoder_chain decoder(backend);

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.jpg", make_jpeg_signature_bytes());
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "asset://textures/card.jpg"});

    require(!result.ok(), "unavailable optional adapter does not produce a texture");
    require(
        result.status == render_image_texture_pipeline_status::decode_failed,
        "unavailable optional adapter reports pipeline decode failure");
    require(result.texture.status == render_image_texture_status::decode_failed, "unavailable adapter preserves texture decode failure");
    require(result.texture.decoder_diagnostics.size() == 4, "unavailable adapter records adapter plus standard diagnostics");
    require(result.texture.decoder_diagnostics[0].decoder_id == "fake_stb_decoder", "unavailable adapter diagnostic is visible");
    require(!result.texture.decoder_diagnostics[0].supported, "unavailable adapter diagnostic records unsupported candidate");
    require(
        result.texture.decoder_diagnostics[0].diagnostic.find("unavailable") != std::string::npos,
        "unavailable adapter diagnostic names unavailable backend");
    require(result.texture.decoder_diagnostics[1].decoder_id == "bmp_image_decoder", "standard diagnostics remain available");
    require(result.texture.decoder_diagnostics[3].terminal_candidate, "standard terminal diagnostic remains available");
    require(uploader.upload_requests.empty(), "unavailable adapter path does not upload");
    require(backend.decode_requests.empty(), "unavailable adapter path does not call backend decode");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.failure_count == 1, "unavailable adapter snapshot records failure");
    require(snapshot.decode_failure_count == 1, "unavailable adapter snapshot counts decode failure");
    require(snapshot.upload_snapshot.upload_count == 0, "unavailable adapter snapshot records no upload");
    require(snapshot.entries[0].decoder_diagnostics[0].decoder_id == "fake_stb_decoder", "snapshot keeps adapter diagnostic");
    require(
        snapshot.entries[0].decoder_capability_manifest.used_third_party_adapter,
        "unavailable adapter snapshot manifest records adapter");
    require(snapshot.entries[0].decoder_capability_manifest.fallback_used, "unavailable adapter snapshot manifest records fallback");
    require(
        snapshot.entries[0].decoder_capability_manifest.terminal_decoder_id == "unsupported_terminal",
        "unavailable adapter snapshot manifest records unsupported terminal");
    require(
        snapshot.entries[0].decoder_fallback_reason.find("unavailable") != std::string::npos,
        "unavailable adapter snapshot records fallback reason");
    require(snapshot.entries[0].upload_count_after == 0, "snapshot records no upload after unavailable adapter");
}

void test_pipeline_decoder_manifest_reports_placeholder_outcome()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    cache.set_placeholder_texture_policy(fake_image_texture_placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    });
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "asset://textures/bad.ppm"});

    require(result.ok(), "decode failure can produce placeholder through image texture pipeline");
    require(is_fake_image_texture_placeholder_key(result.texture.key), "placeholder result uses placeholder key");
    require(result.texture.decode_metadata.decoder_id == "fake_image_texture_placeholder_policy", "placeholder records policy decoder");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.ready_count == 1, "placeholder pipeline snapshot records ready fallback");
    require(snapshot.decode_failure_count == 0, "placeholder pipeline snapshot does not record terminal decode failure");
    require(snapshot.entries[0].placeholder_texture, "placeholder entry records placeholder outcome");
    require(
        snapshot.entries[0].placeholder_outcome.find("placeholder") != std::string::npos,
        "placeholder entry records placeholder diagnostic");
    require(
        snapshot.entries[0].selected_decoder_id == "fake_image_texture_placeholder_policy",
        "placeholder entry records selected placeholder decoder");
    require(
        !snapshot.entries[0].decoder_capability_manifest.used_third_party_adapter,
        "placeholder entry capability manifest does not report adapter use");
    require(
        snapshot.entries[0].decoder_capability_manifest.terminal_decoder_id == "fake_image_texture_placeholder_policy",
        "placeholder entry capability manifest records placeholder policy terminal id");
    require(snapshot.entries[0].cache_reused == false, "first placeholder entry is not cache reuse");
}

void test_batch_plan_normalizes_and_deduplicates_texture_requests()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;
    nearest_sampler.wrap_u = render_image_wrap_mode::repeat;

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "  ASSET:///./textures\\card.ppm  "},
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
    });

    require(plan.ok(), "batch plan accepts normalized image refs");
    require(plan.request_count == 3, "batch plan records request count");
    require(plan.planned_request_count == 3, "batch plan records planned request count");
    require(plan.invalid_request_count == 0, "batch plan records no invalid requests");
    require(plan.unique_source_key_count == 1, "batch plan deduplicates normalized source keys");
    require(plan.unique_texture_key_count == 2, "batch plan keeps sampler policy in texture key identity");
    require(plan.cache_reuse_expected_count == 1, "batch plan predicts one cache reuse");
    require(plan.planned_requests.size() == 3, "batch plan exposes planned texture pipeline requests");
    require(
        plan.planned_requests[0].uri == "asset://textures/card.ppm",
        "batch plan normalizes planned texture pipeline uri");
    require(
        plan.unique_source_keys[0] == "asset://textures/card.ppm",
        "batch plan records normalized source cache key");
    require(
        plan.entries[0].status == render_image_texture_batch_plan_entry_status::planned,
        "first batch entry is planned");
    require(plan.entries[0].ok(), "first batch entry reports ok");
    require(
        plan.entries[0].normalized_source_key == "asset://textures/card.ppm",
        "first batch entry has normalized source key");
    require(
        plan.entries[0].stable_texture_cache_key == plan.entries[1].stable_texture_cache_key,
        "matching sampler shares texture key");
    require(!plan.entries[0].duplicate_source_key, "first batch entry owns first source key");
    require(!plan.entries[0].duplicate_texture_key, "first batch entry owns first texture key");
    require(plan.entries[1].duplicate_source_key, "second batch entry sees duplicate source key");
    require(plan.entries[1].duplicate_texture_key, "second batch entry sees duplicate texture key");
    require(plan.entries[1].expects_cache_reuse, "second batch entry expects cache reuse");
    require(plan.entries[1].first_source_key_request_index == 0, "second batch entry points to first source user");
    require(plan.entries[1].first_texture_key_request_index == 0, "second batch entry points to first texture user");
    require(plan.entries[2].duplicate_source_key, "third batch entry sees duplicate source key");
    require(!plan.entries[2].duplicate_texture_key, "third batch entry keeps distinct sampler cache key");
    require(!plan.entries[2].expects_cache_reuse, "third batch entry does not reuse texture due sampler separation");
    require(
        plan.entries[2].sampler_policy.uses_nearest_filtering,
        "third batch entry records nearest sampler diagnostics");
    require(
        plan.entries[2].stable_texture_cache_key != plan.entries[0].stable_texture_cache_key,
        "sampler policy changes stable texture cache key");
}

void test_batch_plan_reports_invalid_request_reasons()
{
    using namespace quiz_vulkan::render;

    render_image_sampler_policy invalid_sampler;
    invalid_sampler.min_filter = static_cast<render_image_filter>(255);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "   "},
        render_image_ref{.uri = "ftp://example.test/card.ppm"},
        render_image_ref{.uri = "textures/../secret.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = invalid_sampler},
    });

    require(!plan.ok(), "batch plan reports invalid image refs");
    require(plan.request_count == 4, "invalid batch plan records request count");
    require(plan.planned_request_count == 0, "invalid batch plan has no planned texture requests");
    require(plan.invalid_request_count == 4, "invalid batch plan records invalid count");
    require(plan.unique_source_key_count == 0, "invalid batch plan does not count invalid source keys");
    require(plan.unique_texture_key_count == 0, "invalid batch plan does not count invalid texture keys");
    require(
        plan.diagnostic == "image texture batch plan contains invalid requests",
        "invalid batch plan records deterministic diagnostic");
    require(
        plan.entries[0].status == render_image_texture_batch_plan_entry_status::resolve_failed,
        "empty uri is a resolve failure");
    require(plan.entries[0].invalid_reason.find("empty") != std::string::npos, "empty uri invalid reason is visible");
    require(
        plan.entries[1].resolve_status == render_image_resolve_status::unsupported_scheme,
        "unsupported scheme records resolver status");
    require(
        plan.entries[1].invalid_reason.find("scheme") != std::string::npos,
        "unsupported scheme invalid reason is visible");
    require(
        plan.entries[2].status == render_image_texture_batch_plan_entry_status::path_traversal_rejected,
        "parent path segments are rejected by batch planning");
    require(
        plan.entries[2].invalid_reason.find("traversal") != std::string::npos,
        "path traversal invalid reason is visible");
    require(
        plan.entries[3].status == render_image_texture_batch_plan_entry_status::invalid_sampler,
        "invalid sampler enum is reported separately");
    require(
        plan.entries[3].invalid_reason.find("sampler") != std::string::npos,
        "invalid sampler reason is visible");
    require(
        render_image_texture_batch_plan_entry_status_name(plan.entries[3].status) == "invalid_sampler",
        "batch plan entry status name is stable");
}

void test_batch_plan_reports_placeholder_fallback_policy_without_cache_internals()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 4,
        .height = 4,
    };
    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{render_image_ref{.uri = "asset://textures/card.ppm"}},
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});

    require(plan.ok(), "placeholder-aware batch plan is valid for a valid image ref");
    require(plan.placeholder_policy_enabled, "batch plan records placeholder policy availability");
    require(plan.placeholder_policy.width == 4, "batch plan records placeholder policy width");
    require(plan.entries.size() == 1, "placeholder-aware batch plan records one entry");
    require(plan.entries[0].placeholder_policy_enabled, "batch plan entry records placeholder availability");
    require(
        plan.entries[0].fallback_placeholder_reason == fake_image_texture_placeholder_reason::decode_failed,
        "batch plan entry records decode fallback placeholder reason");
    require(
        plan.entries[0].fallback_placeholder_available,
        "batch plan entry reports fallback placeholder key availability");
    require(
        is_fake_image_texture_placeholder_key(plan.entries[0].fallback_placeholder_key),
        "batch plan entry exposes placeholder key without cache internals");
    require(
        plan.entries[0].fallback_placeholder_key.sampler == plan.entries[0].texture_key.sampler,
        "batch plan placeholder preserves sampler policy");
    require(
        plan.entries[0].fallback_placeholder_key.source_key.find("decode_failed") != std::string::npos,
        "batch plan placeholder key records fallback reason");
}

void test_batch_execution_runs_planned_requests_and_reports_cache_reuse()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "  ASSET:///textures\\card.ppm  "},
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
    });
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);

    require(execution.ok(), "batch execution succeeds for planned PPM requests");
    require(execution.request_count == 3, "batch execution records request count");
    require(execution.planned_request_count == 3, "batch execution records planned count");
    require(execution.invalid_request_count == 0, "batch execution records no invalid requests");
    require(execution.executed_request_count == 3, "batch execution runs all planned requests");
    require(execution.skipped_request_count == 0, "batch execution skips no valid requests");
    require(execution.ready_count == 3, "batch execution counts ready requests");
    require(execution.failure_count == 0, "batch execution records no failures");
    require(execution.cache_hit_count == 1, "batch execution counts cache hits");
    require(execution.cache_reuse_count == 1, "batch execution counts cache reuse");
    require(execution.cache_reuse_expected_count == 1, "batch execution carries plan cache reuse expectation");
    require(
        execution.cache_reuse_expectation_match_count == 3,
        "batch execution records matched cache reuse expectations");
    require(execution.cache_reuse_expectation_mismatch_count == 0, "batch execution records no reuse mismatch");
    require(execution.placeholder_texture_count == 0, "batch execution records no placeholder fallback");
    require(execution.all_planned_requests_executed, "batch execution records all planned requests executed");
    require(execution.residency_budget_diagnostics_available, "batch execution exposes residency budget summary");
    require(
        execution.residency_budget.unique_resident_texture_count == 2,
        "batch execution residency summary deduplicates ready textures");
    require(
        execution.residency_budget.eviction_candidate_count == 3,
        "batch execution residency summary classifies unmarked ready textures as eviction candidates");
    require(execution.residency_budget.ok(), "batch execution default residency summary is within budget");
    require(execution.entries.size() == 3, "batch execution records one entry per request");
    require(execution.entries[0].status == render_image_texture_batch_execution_entry_status::ready, "first execution is ready");
    require(execution.entries[0].executed, "first execution reached pipeline");
    require(!execution.entries[0].cache_reused, "first execution is not cache reuse");
    require(execution.entries[1].cache_reused, "second execution reuses cached texture");
    require(execution.entries[1].expected_cache_reuse, "second execution records expected reuse");
    require(execution.entries[1].cache_reuse_expectation_matched, "second execution matches reuse expectation");
    require(
        execution.entries[1].texture.id == execution.entries[0].texture.id,
        "second execution returns the same cached texture handle");
    require(!execution.entries[2].expected_cache_reuse, "third execution expects no texture cache reuse");
    require(!execution.entries[2].cache_reused, "third execution does not reuse due sampler separation");
    require(
        execution.entries[2].texture.id != execution.entries[0].texture.id,
        "third execution uploads a distinct sampler texture");
}

void test_batch_execution_threads_residency_budget_pressure_summary()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    loader.set_source_bytes("asset://textures/background.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
        render_image_ref{.uri = "asset://textures/background.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics execution = execute_render_image_texture_batch_plan(
        plan,
        pipeline,
        render_image_texture_residency_budget_plan_options{
            .visible_request_indices = {0},
            .pinned_request_indices = {1},
            .max_resident_pixel_count = 4,
            .max_resident_texture_count = 2,
        });

    require(execution.ok(), "budget-aware batch execution keeps pipeline success separate from residency pressure");
    require(
        execution.diagnostic == "image texture batch execution ready with residency budget pressure",
        "budget-aware batch execution diagnostic mentions residency pressure");
    require(execution.residency_budget_diagnostics_available, "budget-aware execution exposes residency summary");
    require(execution.residency_budget.visible_candidate_count == 1, "execution residency summary counts visible");
    require(execution.residency_budget.pinned_candidate_count == 1, "execution residency summary counts pinned");
    require(execution.residency_budget.eviction_candidate_count == 1, "execution residency summary counts eviction");
    require(execution.residency_budget.retry_candidate_count == 0, "execution residency summary records no retries");
    require(
        execution.residency_budget.unique_resident_texture_count == 3,
        "execution residency summary counts unique resident textures");
    require(
        execution.residency_budget.unique_resident_pixel_count == 6,
        "execution residency summary estimates unique resident pixels");
    require(execution.residency_budget.pixel_budget_pressure, "execution residency summary reports pixel pressure");
    require(execution.residency_budget.texture_budget_pressure, "execution residency summary reports texture pressure");
    require(execution.residency_budget.budget_pressure, "execution residency summary reports aggregate pressure");
    require(!execution.residency_budget.ok(), "execution residency summary ok reflects budget pressure");
    require(
        execution.residency_budget.pressure_status
            == render_image_texture_residency_budget_pressure_status::over_pixel_and_texture_budget,
        "execution residency summary records pressure status");
    require(
        execution.residency_budget.pressure_status_name == "over_pixel_and_texture_budget",
        "execution residency summary records pressure status name");
}

void test_batch_execution_accepts_standard_pipeline_interface()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/standard.ppm", make_ppm_2x1_fixture_bytes());
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/standard.ppm"},
        render_image_ref{.uri = "asset://textures/standard.ppm"},
    });
    image_texture_pipeline_interface& pipeline_interface = pipeline;
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline_interface);

    require(execution.ok(), "batch execution accepts standard image texture pipeline through interface");
    require(execution.executed_request_count == 2, "standard batch execution runs both requests");
    require(execution.ready_count == 2, "standard batch execution records ready count");
    require(execution.cache_reuse_expected_count == 1, "standard batch execution carries reuse expectation");
    require(execution.cache_reuse_count == 1, "standard batch execution observes cache reuse");
    require(execution.entries[1].cache_reused, "standard batch execution records repeat cache reuse");
    require(
        execution.entries[1].texture.id == execution.entries[0].texture.id,
        "standard batch execution reuses cached texture handle");
}

void test_batch_execution_skips_invalid_requests_and_counts_pipeline_failures()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "   "},
        render_image_ref{.uri = "asset://textures/bad.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);

    require(!execution.ok(), "batch execution reports invalid and failed requests");
    require(execution.request_count == 2, "failed batch execution records request count");
    require(execution.planned_request_count == 1, "failed batch execution records one planned request");
    require(execution.invalid_request_count == 1, "failed batch execution carries invalid plan count");
    require(execution.executed_request_count == 1, "failed batch execution runs only valid planned requests");
    require(execution.skipped_request_count == 1, "failed batch execution skips invalid request");
    require(execution.ready_count == 0, "failed batch execution records no ready requests");
    require(execution.failure_count == 2, "failed batch execution counts skipped and decode failures");
    require(execution.all_planned_requests_executed, "failed batch execution still runs all planned requests");
    require(
        execution.diagnostic == "image texture batch execution contains failed requests",
        "failed batch execution records deterministic diagnostic");
    require(
        execution.entries[0].status == render_image_texture_batch_execution_entry_status::skipped_invalid_request,
        "invalid batch execution entry is skipped");
    require(!execution.entries[0].executed, "invalid batch execution entry does not reach pipeline");
    require(execution.entries[0].invalid_reason.find("empty") != std::string::npos, "skipped entry keeps invalid reason");
    require(
        execution.entries[1].status == render_image_texture_batch_execution_entry_status::decode_failed,
        "bad PPM execution entry reports decode failure");
    require(execution.entries[1].executed, "valid failed entry reaches pipeline");
    require(!execution.entries[1].ready, "valid failed entry is not ready");
    require(execution.entries[1].texture_status == render_image_texture_status::decode_failed, "failed entry records texture status");
}

void test_batch_execution_reports_placeholder_fallback_without_cache_internals()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    cache.set_placeholder_texture_policy(placeholder_policy);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{render_image_ref{.uri = "asset://textures/bad.ppm"}},
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);

    require(execution.ok(), "batch execution treats placeholder fallback as ready");
    require(execution.executed_request_count == 1, "placeholder batch execution runs planned request");
    require(execution.ready_count == 1, "placeholder batch execution records ready count");
    require(execution.failure_count == 0, "placeholder batch execution records no failure");
    require(execution.placeholder_texture_count == 1, "placeholder batch execution counts placeholder fallback");
    require(
        execution.residency_budget.placeholder_texture_count == 1,
        "placeholder batch execution residency summary counts placeholder fallback");
    require(execution.entries[0].placeholder_texture, "placeholder batch execution entry records placeholder texture");
    require(
        execution.entries[0].fallback_placeholder_available,
        "placeholder batch execution entry keeps planned fallback availability");
    require(
        is_fake_image_texture_placeholder_key(execution.entries[0].fallback_placeholder_key),
        "placeholder batch execution entry keeps planned placeholder key");
    require(
        is_fake_image_texture_placeholder_key(execution.entries[0].texture_key),
        "placeholder batch execution entry records actual placeholder texture key");
    require(
        execution.entries[0].diagnostic.find("placeholder") != std::string::npos,
        "placeholder batch execution entry records placeholder diagnostic");
}

void test_residency_budget_plan_classifies_candidates_and_pressure()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    loader.set_source_bytes("asset://textures/background.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_batch_plan batch_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
        render_image_ref{.uri = "asset://textures/background.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(batch_plan, pipeline);
    require(execution.ok(), "residency budget fixture execution succeeds");

    const render_image_texture_residency_budget_plan residency = plan_render_image_texture_residency_budget(
        execution,
        render_image_texture_residency_budget_plan_options{
            .visible_request_indices = {0},
            .preload_request_indices = {1},
            .pinned_texture_keys = {execution.entries[2].texture_key},
            .max_resident_pixel_count = 4,
            .max_resident_texture_count = 2,
        });

    require(!residency.ok(), "residency budget plan reports budget pressure");
    require(residency.request_count == 4, "residency budget plan records request count");
    require(residency.executed_request_count == 4, "residency budget plan records executed count");
    require(residency.ready_count == 4, "residency budget plan records ready count");
    require(residency.visible_candidate_count == 1, "residency budget plan counts visible candidates");
    require(residency.pinned_candidate_count == 1, "residency budget plan counts pinned candidates");
    require(residency.preload_candidate_count == 1, "residency budget plan counts preload candidates");
    require(residency.eviction_candidate_count == 1, "residency budget plan counts eviction candidates");
    require(residency.retry_candidate_count == 0, "residency budget plan records no retry candidates");
    require(residency.placeholder_texture_count == 0, "residency budget plan records no placeholders");
    require(residency.unique_resident_texture_count == 3, "residency budget plan deduplicates resident textures");
    require(residency.unique_resident_pixel_count == 6, "residency budget plan estimates unique resident pixels");
    require(residency.unique_resident_rgba8_byte_count == 24, "residency budget plan estimates RGBA8 bytes");
    require(residency.pixel_budget_pressure, "residency budget plan reports pixel budget pressure");
    require(residency.texture_budget_pressure, "residency budget plan reports texture budget pressure");
    require(residency.budget_pressure, "residency budget plan reports aggregate pressure");
    require(residency.over_budget_pixel_count == 2, "residency budget plan reports pixel overage");
    require(residency.over_budget_texture_count == 1, "residency budget plan reports texture overage");
    require(
        residency.pressure_status
            == render_image_texture_residency_budget_pressure_status::over_pixel_and_texture_budget,
        "residency budget plan records combined pressure status");
    require(residency.pressure_status_name == "over_pixel_and_texture_budget", "residency pressure status name is stable");
    require(residency.entries[0].visible_candidate, "first residency entry is visible");
    require(residency.entries[0].counts_against_budget, "first residency entry contributes to budget");
    require(residency.entries[1].preload_candidate, "second residency entry is preload");
    require(residency.entries[1].duplicate_texture_key, "second residency entry is duplicate texture key");
    require(!residency.entries[1].counts_against_budget, "second residency entry does not double-count budget");
    require(residency.entries[1].first_texture_request_index == 0, "second residency entry points to first texture");
    require(residency.entries[2].pinned_candidate, "third residency entry is pinned by texture key");
    require(!residency.entries[2].eviction_candidate, "pinned residency entry is not eviction candidate");
    require(residency.entries[3].eviction_candidate, "unclassified ready entry is eviction candidate");
}

void test_residency_budget_plan_marks_retry_and_placeholder_candidates()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader retry_loader;
    retry_loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder retry_decoder;
    fake_image_texture_uploader retry_uploader;
    fake_image_texture_cache retry_cache(retry_decoder, retry_uploader);
    fake_image_texture_pipeline retry_pipeline(resolver, retry_loader, retry_cache, retry_uploader);

    const render_image_texture_batch_plan retry_batch_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "   "},
        render_image_ref{.uri = "asset://textures/bad.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics retry_execution =
        execute_render_image_texture_batch_plan(retry_batch_plan, retry_pipeline);
    const render_image_texture_residency_budget_plan retry_residency =
        plan_render_image_texture_residency_budget(retry_execution);

    require(retry_residency.retry_candidate_count == 1, "residency budget plan counts retry candidates");
    require(!retry_residency.entries[0].retry_candidate, "skipped invalid request is not retryable");
    require(!retry_residency.entries[0].executed, "skipped invalid request stays unexecuted");
    require(retry_residency.entries[1].retry_candidate, "executed decode failure is retry candidate");
    require(retry_residency.entries[1].retry_reason == "decode_failed", "retry candidate records failure reason");
    require(retry_residency.entries[1].estimated_pixel_count == 0, "retry candidate does not estimate resident pixels");

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    fake_image_source_bytes_loader placeholder_loader;
    placeholder_loader.set_source_bytes("asset://textures/placeholder.ppm", make_short_ppm_2x1_fixture_bytes());
    fake_image_texture_uploader placeholder_uploader;
    fake_image_texture_cache placeholder_cache(retry_decoder, placeholder_uploader);
    placeholder_cache.set_placeholder_texture_policy(placeholder_policy);
    fake_image_texture_pipeline placeholder_pipeline(resolver, placeholder_loader, placeholder_cache, placeholder_uploader);

    const render_image_texture_batch_plan placeholder_batch_plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{render_image_ref{.uri = "asset://textures/placeholder.ppm"}},
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics placeholder_execution =
        execute_render_image_texture_batch_plan(placeholder_batch_plan, placeholder_pipeline);
    const render_image_texture_residency_budget_plan placeholder_residency =
        plan_render_image_texture_residency_budget(placeholder_execution);

    require(placeholder_residency.placeholder_texture_count == 1, "residency budget plan counts placeholder textures");
    require(placeholder_residency.retry_candidate_count == 0, "ready placeholder is not retry candidate");
    require(placeholder_residency.entries[0].placeholder_texture, "residency entry records placeholder texture");
    require(placeholder_residency.entries[0].eviction_candidate, "unclassified placeholder is eviction candidate");
    require(placeholder_residency.unique_resident_pixel_count == 4, "placeholder contributes public texture pixels");
    require(placeholder_residency.unique_resident_rgba8_byte_count == 16, "placeholder contributes estimated RGBA8 bytes");
}

void test_texture_handle_map_records_renderer_handoff_mapping()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "  ASSET:///textures\\card.ppm  "},
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
    });
    const render_image_texture_batch_execution_diagnostics execution = execute_render_image_texture_batch_plan(
        plan,
        pipeline,
        render_image_texture_residency_budget_plan_options{
            .max_resident_texture_count = 1,
        });
    const render_image_texture_handle_map_diagnostics handle_map =
        make_render_image_texture_handle_map_diagnostics(plan, execution);

    require(handle_map.ok(), "texture handle map is ready when every request has a public texture id");
    require(handle_map.renderer_handoff_ready, "texture handle map records renderer handoff readiness");
    require(handle_map.request_count == 3, "texture handle map records request count");
    require(handle_map.mapped_count == 3, "texture handle map records mapped count");
    require(handle_map.missing_count == 0, "texture handle map records no missing handles");
    require(handle_map.placeholder_texture_count == 0, "texture handle map records no placeholders");
    require(handle_map.cache_reused_count == 1, "texture handle map records cache reuse");
    require(handle_map.unique_texture_id_count == 2, "texture handle map deduplicates repeated texture ids");
    require(handle_map.residency_budget_diagnostics_available, "texture handle map carries residency summary availability");
    require(handle_map.residency_budget_pressure, "texture handle map carries residency pressure");
    require(
        handle_map.residency_pressure_status
            == render_image_texture_residency_budget_pressure_status::over_texture_budget,
        "texture handle map carries residency pressure status");
    require(handle_map.residency_pressure_status_name == "over_texture_budget", "texture handle pressure name is stable");
    require(
        handle_map.diagnostic == "image texture handle map is ready for renderer handoff",
        "texture handle map ready diagnostic is stable");
    require(handle_map.entries.size() == 3, "texture handle map records one entry per request");

    const render_image_texture_handle_map_entry& first = handle_map.entries[0];
    require(first.ok(), "first handle map entry is mapped");
    require(first.status == render_image_texture_handle_map_entry_status::mapped, "first handle map status is mapped");
    require(first.plan_status == render_image_texture_batch_plan_entry_status::planned, "first handle map records plan status");
    require(first.execution_status == render_image_texture_batch_execution_entry_status::ready, "first handle map records execution status");
    require(first.pipeline_status == render_image_texture_pipeline_status::ready, "first handle map records pipeline status");
    require(first.request_index == 0, "first handle map entry records request index");
    require(first.render_image_uri == "asset://textures/card.ppm", "first handle map preserves render image uri");
    require(first.normalized_uri == "asset://textures/card.ppm", "first handle map records normalized uri");
    require(first.cache_key == "asset://textures/card.ppm", "first handle map records normalized source cache key");
    require(first.source_kind == render_image_source_kind::asset_uri, "first handle map records source kind");
    require(first.texture_id == execution.entries[0].texture.id, "first handle map records public texture id");
    require(first.texture_revision == execution.entries[0].texture.revision, "first handle map records texture revision");
    require(first.texture_width == 2, "first handle map records texture width");
    require(first.texture_height == 1, "first handle map records texture height");
    require(first.ready, "first handle map records ready state");
    require(!first.placeholder_texture, "first handle map records non-placeholder texture");
    require(!first.cache_reused, "first handle map records first request as cache miss");
    require(!first.expected_cache_reuse, "first handle map records no reuse expectation");
    require(first.sampler_policy.valid, "first handle map exposes valid sampler policy");
    require(first.texture_key_diagnostic.valid, "first handle map exposes valid texture key diagnostic");
    require(first.stable_texture_cache_key == plan.entries[0].stable_texture_cache_key, "first handle map records stable texture key");
    require(first.residency_budget_pressure, "first handle map entry carries residency pressure");
    require(first.residency_pressure_status_name == "over_texture_budget", "first handle map entry records pressure name");

    const render_image_texture_handle_map_entry& second = handle_map.entries[1];
    require(second.ok(), "second handle map entry is mapped");
    require(second.render_image_uri == "  ASSET:///textures\\card.ppm  ", "second handle map preserves original uri");
    require(second.normalized_uri == "asset://textures/card.ppm", "second handle map records normalized alias uri");
    require(second.texture_id == first.texture_id, "second handle map records reused texture id");
    require(second.cache_reused, "second handle map records cache reuse");
    require(second.expected_cache_reuse, "second handle map records expected cache reuse");
    require(second.stable_texture_cache_key == first.stable_texture_cache_key, "second handle map records same texture cache key");

    const render_image_texture_handle_map_entry& third = handle_map.entries[2];
    require(third.ok(), "third handle map entry is mapped");
    require(third.texture_id != first.texture_id, "third handle map records sampler-separated texture id");
    require(!third.cache_reused, "third handle map records sampler separation as non-reuse");
    require(third.sampler_policy.uses_nearest_filtering, "third handle map records nearest sampler policy");
    require(third.stable_texture_cache_key != first.stable_texture_cache_key, "third handle map records sampler-separated key");
}

void test_texture_handle_map_records_missing_and_placeholder_entries()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    cache.set_placeholder_texture_policy(placeholder_policy);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{
            render_image_ref{.uri = "   "},
            render_image_ref{.uri = "asset://textures/bad.ppm"},
        },
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);
    const render_image_texture_handle_map_diagnostics handle_map =
        make_render_image_texture_handle_map_diagnostics(plan, execution);

    require(!handle_map.ok(), "texture handle map is not renderer-ready with a skipped request");
    require(!handle_map.renderer_handoff_ready, "texture handle map records missing handoff readiness");
    require(handle_map.request_count == 2, "placeholder handle map records request count");
    require(handle_map.mapped_count == 1, "placeholder handle map records mapped placeholder");
    require(handle_map.missing_count == 1, "placeholder handle map records skipped request as missing");
    require(handle_map.placeholder_texture_count == 1, "placeholder handle map counts placeholder texture");
    require(handle_map.cache_reused_count == 0, "placeholder handle map records no cache reuse");
    require(handle_map.unique_texture_id_count == 1, "placeholder handle map counts one public texture id");
    require(!handle_map.residency_budget_pressure, "placeholder handle map records no residency pressure");
    require(handle_map.residency_pressure_status_name == "within_budget", "placeholder handle pressure name is stable");
    require(
        handle_map.diagnostic == "image texture handle map has missing texture handles",
        "placeholder handle map failure diagnostic is stable");
    require(handle_map.entries.size() == 2, "placeholder handle map keeps one entry per request");

    const render_image_texture_handle_map_entry& skipped = handle_map.entries[0];
    require(!skipped.ok(), "skipped handle map entry is not mapped");
    require(
        skipped.status == render_image_texture_handle_map_entry_status::skipped_invalid_request,
        "skipped handle map entry records skipped status");
    require(skipped.plan_status == render_image_texture_batch_plan_entry_status::resolve_failed, "skipped entry records plan failure");
    require(
        skipped.execution_status == render_image_texture_batch_execution_entry_status::skipped_invalid_request,
        "skipped entry records execution failure");
    require(skipped.render_image_uri == "   ", "skipped entry preserves original invalid uri");
    require(skipped.texture_id == 0, "skipped entry has no texture id");
    require(!skipped.ready, "skipped entry records non-ready state");
    require(!skipped.placeholder_texture, "skipped entry is not a placeholder texture");
    require(skipped.diagnostic.find("empty") != std::string::npos, "skipped entry carries invalid request diagnostic");

    const render_image_texture_handle_map_entry& placeholder = handle_map.entries[1];
    require(placeholder.ok(), "placeholder handle map entry is mapped");
    require(placeholder.status == render_image_texture_handle_map_entry_status::mapped, "placeholder entry status is mapped");
    require(placeholder.plan_status == render_image_texture_batch_plan_entry_status::planned, "placeholder entry records plan status");
    require(placeholder.execution_status == render_image_texture_batch_execution_entry_status::ready, "placeholder entry records ready execution");
    require(placeholder.render_image_uri == "asset://textures/bad.ppm", "placeholder entry preserves source uri");
    require(placeholder.normalized_uri == "asset://textures/bad.ppm", "placeholder entry records normalized source uri");
    require(placeholder.cache_key == "asset://textures/bad.ppm", "placeholder entry records requested source cache key");
    require(placeholder.texture_id != 0, "placeholder entry exposes a public texture id");
    require(placeholder.ready, "placeholder entry records ready placeholder");
    require(placeholder.placeholder_texture, "placeholder entry records placeholder flag");
    require(is_fake_image_texture_placeholder_key(placeholder.texture_key), "placeholder entry records placeholder texture key");
    require(placeholder.texture_key.source_key != placeholder.cache_key, "placeholder key stays separate from requested source key");
    require(placeholder.sampler_policy.valid, "placeholder entry exposes sampler policy");
    require(placeholder.residency_pressure_status_name == "within_budget", "placeholder entry carries residency status");
    require(
        placeholder.diagnostic.find("placeholder") != std::string::npos,
        "placeholder entry records placeholder diagnostic");
}

void test_texture_frame_snapshot_combines_public_frame_diagnostics()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
    });
    const render_image_texture_batch_execution_diagnostics execution = execute_render_image_texture_batch_plan(
        plan,
        pipeline,
        render_image_texture_residency_budget_plan_options{
            .max_resident_texture_count = 1,
        });
    const render_image_texture_handle_map_diagnostics handle_map =
        make_render_image_texture_handle_map_diagnostics(plan, execution);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution, handle_map);

    require(frame.ok(), "texture frame snapshot is ready when plan execution and handle map are ready");
    require(frame.status == render_image_texture_frame_snapshot_status::ready, "texture frame snapshot status is ready");
    require(frame.status_name == "ready", "texture frame snapshot status name is stable");
    require(frame.request_count == 3, "texture frame snapshot records request count");
    require(frame.planned_request_count == 3, "texture frame snapshot records planned requests");
    require(frame.invalid_request_count == 0, "texture frame snapshot records no invalid requests");
    require(frame.executed_request_count == 3, "texture frame snapshot records executed requests");
    require(frame.skipped_request_count == 0, "texture frame snapshot records skipped requests");
    require(frame.ready_count == 3, "texture frame snapshot records ready requests");
    require(frame.failure_count == 0, "texture frame snapshot records no execution failures");
    require(frame.mapped_texture_count == 3, "texture frame snapshot records mapped textures");
    require(frame.missing_texture_count == 0, "texture frame snapshot records no missing textures");
    require(frame.placeholder_texture_count == 0, "texture frame snapshot records no placeholders");
    require(frame.cache_reused_count == 1, "texture frame snapshot records cache reuse");
    require(frame.unique_source_key_count == 1, "texture frame snapshot records unique source keys");
    require(frame.unique_texture_cache_key_count == 2, "texture frame snapshot records sampler-separated texture keys");
    require(frame.unique_texture_id_count == 2, "texture frame snapshot records unique public texture ids");
    require(frame.unique_resident_texture_count == 2, "texture frame snapshot carries resident texture count");
    require(frame.unique_resident_pixel_count == 4, "texture frame snapshot carries resident pixel count");
    require(frame.unique_resident_rgba8_byte_count == 16, "texture frame snapshot carries resident byte estimate");
    require(frame.eviction_candidate_count == 3, "texture frame snapshot carries eviction candidate count");
    require(frame.retry_candidate_count == 0, "texture frame snapshot carries retry candidate count");
    require(frame.plan_ready, "texture frame snapshot records plan readiness");
    require(frame.execution_ready, "texture frame snapshot records execution readiness");
    require(frame.handle_map_ready, "texture frame snapshot records handle map readiness");
    require(frame.renderer_handoff_ready, "texture frame snapshot records renderer handoff readiness");
    require(frame.residency_budget_diagnostics_available, "texture frame snapshot records residency diagnostics availability");
    require(frame.residency_budget_pressure, "texture frame snapshot records residency pressure");
    require(!frame.pixel_budget_pressure, "texture frame snapshot records no pixel pressure");
    require(frame.texture_budget_pressure, "texture frame snapshot records texture pressure");
    require(
        frame.residency_pressure_status
            == render_image_texture_residency_budget_pressure_status::over_texture_budget,
        "texture frame snapshot records residency pressure status");
    require(frame.residency_pressure_status_name == "over_texture_budget", "texture frame pressure status name is stable");
    require(
        frame.diagnostic == "image texture frame snapshot ready with residency budget pressure",
        "texture frame snapshot ready diagnostic is stable");
    require(frame.entries.size() == 3, "texture frame snapshot records compact entries");

    const render_image_texture_frame_entry_snapshot& first = frame.entries[0];
    require(first.ok(), "first frame entry is renderer-handoff ready");
    require(first.request_index == 0, "first frame entry records request index");
    require(first.plan_status == render_image_texture_batch_plan_entry_status::planned, "first frame entry records plan status");
    require(first.execution_status == render_image_texture_batch_execution_entry_status::ready, "first frame entry records execution status");
    require(first.pipeline_status == render_image_texture_pipeline_status::ready, "first frame entry records pipeline status");
    require(first.source_bytes_status == render_image_source_bytes_load_status::loaded, "first frame entry records source load status");
    require(first.texture_status == render_image_texture_status::ready, "first frame entry records texture status");
    require(first.handle_status == render_image_texture_handle_map_entry_status::mapped, "first frame entry records handle status");
    require(first.render_image_uri == "asset://textures/card.ppm", "first frame entry preserves render image uri");
    require(first.normalized_uri == "asset://textures/card.ppm", "first frame entry records normalized uri");
    require(first.cache_key == "asset://textures/card.ppm", "first frame entry records cache key");
    require(first.source_kind == render_image_source_kind::asset_uri, "first frame entry records source kind");
    require(first.texture_id == handle_map.entries[0].texture_id, "first frame entry records texture id");
    require(first.texture_revision == handle_map.entries[0].texture_revision, "first frame entry records texture revision");
    require(first.texture_width == 2, "first frame entry records texture width");
    require(first.texture_height == 1, "first frame entry records texture height");
    require(first.planned, "first frame entry records planned");
    require(first.executed, "first frame entry records executed");
    require(first.ready, "first frame entry records ready");
    require(first.mapped, "first frame entry records mapped");
    require(first.renderer_handoff_ready, "first frame entry records renderer handoff readiness");
    require(!first.placeholder_texture, "first frame entry records non-placeholder");
    require(!first.cache_reused, "first frame entry records first acquire as non-reuse");
    require(!first.expected_cache_reuse, "first frame entry records no reuse expectation");
    require(first.sampler_policy.valid, "first frame entry exposes sampler policy");
    require(first.stable_texture_cache_key == handle_map.entries[0].stable_texture_cache_key, "first frame entry records stable key");
    require(first.residency_budget_pressure, "first frame entry records pressure");
    require(first.residency_pressure_status_name == "over_texture_budget", "first frame entry records pressure name");

    const render_image_texture_frame_entry_snapshot& second = frame.entries[1];
    require(second.ok(), "second frame entry is renderer-handoff ready");
    require(second.texture_id == first.texture_id, "second frame entry records reused texture id");
    require(second.cache_reused, "second frame entry records cache reuse");
    require(second.expected_cache_reuse, "second frame entry records expected cache reuse");
    require(second.stable_texture_cache_key == first.stable_texture_cache_key, "second frame entry records same stable key");

    const render_image_texture_frame_entry_snapshot& third = frame.entries[2];
    require(third.ok(), "third frame entry is renderer-handoff ready");
    require(third.texture_id != first.texture_id, "third frame entry records sampler-separated texture id");
    require(third.sampler_policy.uses_nearest_filtering, "third frame entry records nearest sampler");
    require(third.stable_texture_cache_key != first.stable_texture_cache_key, "third frame entry records sampler-separated key");
}

void test_texture_frame_snapshot_records_partial_placeholder_frame()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    cache.set_placeholder_texture_policy(placeholder_policy);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{
            render_image_ref{.uri = "   "},
            render_image_ref{.uri = "asset://textures/bad.ppm"},
        },
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);

    require(!frame.ok(), "partial texture frame snapshot is not ok");
    require(frame.status == render_image_texture_frame_snapshot_status::partial, "partial frame snapshot status is partial");
    require(frame.status_name == "partial", "partial frame snapshot status name is stable");
    require(frame.request_count == 2, "partial frame snapshot records request count");
    require(frame.planned_request_count == 1, "partial frame snapshot records planned count");
    require(frame.invalid_request_count == 1, "partial frame snapshot records invalid count");
    require(frame.executed_request_count == 1, "partial frame snapshot records executed count");
    require(frame.skipped_request_count == 1, "partial frame snapshot records skipped count");
    require(frame.ready_count == 1, "partial frame snapshot records ready placeholder count");
    require(frame.failure_count == 1, "partial frame snapshot records skipped failure count");
    require(frame.mapped_texture_count == 1, "partial frame snapshot records mapped placeholder");
    require(frame.missing_texture_count == 1, "partial frame snapshot records missing skipped request");
    require(frame.placeholder_texture_count == 1, "partial frame snapshot records placeholder count");
    require(frame.unique_texture_id_count == 1, "partial frame snapshot records one public texture id");
    require(frame.unique_resident_texture_count == 1, "partial frame snapshot records one resident texture");
    require(frame.unique_resident_pixel_count == 4, "partial frame snapshot records placeholder pixels");
    require(frame.eviction_candidate_count == 1, "partial frame snapshot records placeholder eviction candidate");
    require(frame.retry_candidate_count == 0, "partial frame snapshot records no retry candidate");
    require(!frame.plan_ready, "partial frame snapshot records plan not ready");
    require(!frame.execution_ready, "partial frame snapshot records execution not ready");
    require(!frame.handle_map_ready, "partial frame snapshot records handle map not ready");
    require(!frame.renderer_handoff_ready, "partial frame snapshot records missing handoff");
    require(!frame.residency_budget_pressure, "partial frame snapshot records no pressure");
    require(
        frame.diagnostic == "image texture frame snapshot is partial",
        "partial frame snapshot diagnostic is stable");
    require(frame.entries.size() == 2, "partial frame snapshot records entries");

    const render_image_texture_frame_entry_snapshot& skipped = frame.entries[0];
    require(!skipped.ok(), "skipped frame entry is not renderer-handoff ready");
    require(!skipped.planned, "skipped frame entry records unplanned request");
    require(!skipped.executed, "skipped frame entry records not executed");
    require(!skipped.ready, "skipped frame entry records not ready");
    require(!skipped.mapped, "skipped frame entry records not mapped");
    require(skipped.texture_id == 0, "skipped frame entry has no texture id");
    require(skipped.handle_status == render_image_texture_handle_map_entry_status::skipped_invalid_request, "skipped frame entry records handle status");
    require(skipped.diagnostic.find("empty") != std::string::npos, "skipped frame entry keeps invalid diagnostic");

    const render_image_texture_frame_entry_snapshot& placeholder = frame.entries[1];
    require(placeholder.ok(), "placeholder frame entry is renderer-handoff ready");
    require(placeholder.planned, "placeholder frame entry records planned");
    require(placeholder.executed, "placeholder frame entry records executed");
    require(placeholder.ready, "placeholder frame entry records ready");
    require(placeholder.mapped, "placeholder frame entry records mapped");
    require(placeholder.placeholder_texture, "placeholder frame entry records placeholder");
    require(placeholder.texture_id != 0, "placeholder frame entry exposes texture id");
    require(placeholder.cache_key == "asset://textures/bad.ppm", "placeholder frame entry keeps requested cache key");
    require(is_fake_image_texture_placeholder_key(placeholder.texture_key), "placeholder frame entry records placeholder texture key");
    require(placeholder.texture_key.source_key != placeholder.cache_key, "placeholder frame entry separates placeholder key from source key");
}

} // namespace

int main()
{
    test_filesystem_pipeline_reads_ppm_fixture_and_reuses_cache();
    test_filesystem_pipeline_reports_missing_file_source_load_failed();
    test_filesystem_pipeline_reports_empty_file_source_load_failed();
    test_filesystem_pipeline_reports_malformed_ppm_decode_failed();
    test_pipeline_uses_optional_third_party_decoder_through_interface();
    test_optional_adapter_failure_falls_back_before_texture_upload();
    test_unavailable_optional_adapter_keeps_diagnostics_without_upload();
    test_pipeline_decoder_manifest_reports_placeholder_outcome();
    test_batch_plan_normalizes_and_deduplicates_texture_requests();
    test_batch_plan_reports_invalid_request_reasons();
    test_batch_plan_reports_placeholder_fallback_policy_without_cache_internals();
    test_batch_execution_runs_planned_requests_and_reports_cache_reuse();
    test_batch_execution_threads_residency_budget_pressure_summary();
    test_batch_execution_accepts_standard_pipeline_interface();
    test_batch_execution_skips_invalid_requests_and_counts_pipeline_failures();
    test_batch_execution_reports_placeholder_fallback_without_cache_internals();
    test_residency_budget_plan_classifies_candidates_and_pressure();
    test_residency_budget_plan_marks_retry_and_placeholder_candidates();
    test_texture_handle_map_records_renderer_handoff_mapping();
    test_texture_handle_map_records_missing_and_placeholder_entries();
    test_texture_frame_snapshot_combines_public_frame_diagnostics();
    test_texture_frame_snapshot_records_partial_placeholder_frame();
    return 0;
}
