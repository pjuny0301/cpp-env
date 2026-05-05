#include "render/image/image_decoder.h"
#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_cache.h"
#include "render/image/image_texture_pipeline.h"

#include <cassert>
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

} // namespace

int main()
{
    test_filesystem_pipeline_reads_ppm_fixture_and_reuses_cache();
    test_filesystem_pipeline_reports_missing_file_source_load_failed();
    test_filesystem_pipeline_reports_empty_file_source_load_failed();
    test_filesystem_pipeline_reports_malformed_ppm_decode_failed();
    return 0;
}
