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
    require(snapshot.entries[0].upload_count_after == 1, "snapshot records first upload");
    require(snapshot.entries[1].cache_hit, "snapshot records repeat cache hit");
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
    require(snapshot.entries[0].upload_count_after == 0, "snapshot records no upload after unavailable adapter");
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
    return 0;
}
