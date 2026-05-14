#include "render/image/image_decoder.h"
#include "render/image/image_manifest_texture_pipeline.h"
#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_cache.h"
#include "render/image/image_texture_frame_upload_handoff.h"
#include "render/image/image_texture_pipeline.h"
#include "render/image/third_party_image_decoder_adapter.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
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
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_entry_diff>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_snapshot_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_entry_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_snapshot_diff>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_packet>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_plan>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_packet>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_plan>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_packet_diff>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_plan_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_packet_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_plan_diff>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_upload_handoff_entry>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_upload_handoff_summary>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_upload_handoff_entry>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_upload_handoff_summary>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_upload_handoff_entry_diff>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_upload_handoff_summary_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_upload_handoff_entry_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_upload_handoff_summary_diff>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_summary>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_summary_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_summary>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_texture_frame_binding_summary_diff>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_external_decoder_selection_entry_diff>);
static_assert(!HasFakeCacheSnapshotField<quiz_vulkan::render::render_image_external_decoder_selection_snapshot_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_external_decoder_selection_entry_diff>);
static_assert(!HasFakeUploadSnapshotField<quiz_vulkan::render::render_image_external_decoder_selection_snapshot_diff>);

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

std::vector<std::byte> make_bytes(std::initializer_list<unsigned char> values)
{
    std::vector<std::byte> bytes;
    bytes.reserve(values.size());
    for (const unsigned char value : values) {
        append_byte(bytes, value);
    }
    return bytes;
}

void append_u16_le(std::vector<std::byte>& bytes, std::uint16_t value)
{
    append_byte(bytes, static_cast<unsigned char>(value & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 8u) & 0xffu));
}

void append_u32_le(std::vector<std::byte>& bytes, std::uint32_t value)
{
    append_byte(bytes, static_cast<unsigned char>(value & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 8u) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 16u) & 0xffu));
    append_byte(bytes, static_cast<unsigned char>((value >> 24u) & 0xffu));
}

void append_i32_le(std::vector<std::byte>& bytes, std::int32_t value)
{
    append_u32_le(bytes, static_cast<std::uint32_t>(value));
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

std::vector<std::byte> make_valid_jpeg_1x1_fixture_bytes()
{
    return make_bytes({
        0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
        0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
        0x00, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x04, 0x03, 0x02, 0x02, 0x02, 0x02, 0x05, 0x04,
        0x04, 0x03, 0x04, 0x06, 0x05, 0x06, 0x06, 0x06, 0x05, 0x06, 0x06, 0x06,
        0x07, 0x09, 0x08, 0x06, 0x07, 0x09, 0x07, 0x06, 0x06, 0x08, 0x0b, 0x08,
        0x09, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x06, 0x08, 0x0b, 0x0c, 0x0b, 0x0a,
        0x0c, 0x09, 0x0a, 0x0a, 0x0a, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x05, 0x03, 0x03, 0x05, 0x0a, 0x07, 0x06, 0x07,
        0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
        0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
        0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
        0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
        0x0a, 0x0a, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x01, 0x00, 0x01, 0x03,
        0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
        0x1f, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0xff, 0xc4, 0x00, 0xb5, 0x10, 0x00,
        0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00,
        0x00, 0x01, 0x7d, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,
        0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81,
        0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24,
        0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25,
        0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
        0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56,
        0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
        0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
        0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3,
        0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
        0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
        0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1,
        0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xff, 0xc4, 0x00,
        0x1f, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0xff, 0xc4, 0x00, 0xb5, 0x11, 0x00,
        0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00,
        0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31,
        0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08,
        0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15,
        0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18,
        0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55,
        0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84,
        0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
        0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4,
        0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
        0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
        0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xff, 0xda, 0x00,
        0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00, 0xfe,
        0x7f, 0xe8, 0xa2, 0x8a, 0x00, 0xff, 0xd9,
    });
}

std::vector<std::byte> make_bmp_24_bit_1x1_fixture_bytes()
{
    std::vector<std::byte> bytes;
    append_byte(bytes, 'B');
    append_byte(bytes, 'M');
    append_u32_le(bytes, 58u);
    append_u16_le(bytes, 0u);
    append_u16_le(bytes, 0u);
    append_u32_le(bytes, 54u);

    append_u32_le(bytes, 40u);
    append_i32_le(bytes, 1);
    append_i32_le(bytes, 1);
    append_u16_le(bytes, 1u);
    append_u16_le(bytes, 24u);
    append_u32_le(bytes, 0u);
    append_u32_le(bytes, 4u);
    append_i32_le(bytes, 2835);
    append_i32_le(bytes, 2835);
    append_u32_le(bytes, 0u);
    append_u32_le(bytes, 0u);

    append_byte(bytes, 30u);
    append_byte(bytes, 20u);
    append_byte(bytes, 10u);
    append_byte(bytes, 0u);
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

quiz_vulkan::render::fake_image_texture_pipeline_snapshot make_optional_stb_pipeline_snapshot(
    std::string uri,
    const std::vector<std::byte>& encoded_bytes,
    quiz_vulkan::render::stb_image_decoder_dependency_manifest manifest,
    bool enable_placeholder = false)
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({
        render_image_encoded_format::jpeg,
        render_image_encoded_format::bmp,
        render_image_encoded_format::ppm,
        render_image_encoded_format::png,
    });
    backend.set_decoded_image(make_rgba_1x1_image(9, 8, 7, 6));

    fake_stb_image_decoder_dependency_probe probe;
    probe.set_manifest(std::move(manifest));
    optional_third_party_image_decoder_chain decoder(backend, probe);

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes(uri, encoded_bytes);
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    if (enable_placeholder) {
        cache.set_placeholder_texture_policy(fake_image_texture_placeholder_policy{
            .enabled = true,
            .width = 2,
            .height = 2,
        });
    }
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);
    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = std::move(uri)});
    (void)result;
    return pipeline.diagnostic_snapshot();
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

void test_filesystem_pipeline_routes_real_jpeg_through_stb_upload_handoff()
{
    using namespace quiz_vulkan::render;

    const std::filesystem::path root = test_data_root();
    reset_test_data_root(root);
    const std::vector<std::byte> fixture_bytes = make_valid_jpeg_1x1_fixture_bytes();
    write_bytes(root / "textures" / "real-stb.jpg", fixture_bytes);

    const normalizing_image_resolver resolver;
    filesystem_image_source_bytes_loader loader(root);
    const stb_image_decoder_header_dependency_probe probe{"stb_image_decoder"};
    const stb_image_decoder_dependency_manifest manifest = probe.probe_dependency();
    standard_image_texture_pipeline pipeline(resolver, loader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "textures/real-stb.jpg"});

    require(result.resolve.ok(), "real JPEG pipeline resolves local file source");
    require(result.source_bytes.ok(), "real JPEG pipeline loads source bytes from filesystem");
    require(result.source_bytes.encoded_bytes == fixture_bytes, "real JPEG pipeline preserves loaded source bytes");
    require(
        result.texture.external_decoder_selection.diagnostics_available,
        "real JPEG pipeline exposes stb selection diagnostics");
    require(
        result.texture.external_decoder_selection.detected_format == render_image_encoded_format::jpeg,
        "real JPEG pipeline records detected JPEG format");

    if (manifest.status != stb_image_decoder_dependency_status::available) {
        require(!result.ok(), "standard pipeline reports decode failure when stb is unavailable");
        require(
            result.status == render_image_texture_pipeline_status::decode_failed,
            "unavailable stb path reports decode failure");
        require(
            result.texture.external_decoder_selection.fallback_to_standard_decoder_chain,
            "unavailable stb path records standard fallback");
        require(
            !result.texture.external_decoder_selection.used_third_party_adapter,
            "unavailable stb path does not report adapter use");

        const standard_image_texture_pipeline_snapshot snapshot = pipeline.standard_diagnostic_snapshot();
        require(snapshot.pipeline.decode_failure_count == 1, "unavailable stb snapshot counts decode failure");
        require(snapshot.pipeline.upload_snapshot.upload_count == 0, "unavailable stb path does not upload");
        require(snapshot.decoder.decode_attempt_count == 1, "unavailable stb path still attempts decode chain");
        require(snapshot.decoder.failed_decode_count == 1, "unavailable stb path records decoder failure");
        return;
    }

    require(result.ok(), "standard pipeline creates a texture through stb memory decode");
    require(result.status == render_image_texture_pipeline_status::ready, "real JPEG pipeline result is ready");
    require(!is_fake_image_texture_placeholder_key(result.texture.key), "real JPEG pipeline does not use placeholder");
    require(!result.texture.cache_hit, "real JPEG first acquire is a cache miss");
    require(result.texture.texture.width == 1, "real JPEG texture preserves decoded width");
    require(result.texture.texture.height == 1, "real JPEG texture preserves decoded height");
    require(result.texture.decode_metadata.decoder_id == "stb_image_decoder", "standard pipeline real JPEG decode uses stb backend");
    require(result.texture.decode_metadata.width == 1, "real JPEG metadata records width");
    require(result.texture.decode_metadata.height == 1, "real JPEG metadata records height");
    require(result.texture.decode_metadata.encoded_byte_count == fixture_bytes.size(), "real JPEG metadata records source byte count");
    require(result.texture.decode_metadata.decoded_byte_count == 4, "real JPEG metadata records RGBA byte count");
    require(result.texture.decode_metadata.size_validation.valid, "real JPEG metadata validates decoded payload");
    require(
        result.texture.decode_metadata.format_detection.detected_format == render_image_encoded_format::jpeg,
        "real JPEG metadata records detected source format");
    require(result.texture.decoder_diagnostics.size() == 1, "real JPEG path records adapter diagnostic");
    require(result.texture.decoder_diagnostics[0].decoder_id == "stb_image_decoder", "real JPEG diagnostic records stb backend");
    require(result.texture.decoder_diagnostics[0].decode_attempted, "real JPEG diagnostic records decode attempt");
    require(result.texture.external_decoder_selection.ready_for_external_decode, "real JPEG selection is external-decode ready");
    require(result.texture.external_decoder_selection.used_third_party_adapter, "real JPEG selection records adapter route");
    require(
        !result.texture.external_decoder_selection.fallback_to_standard_decoder_chain,
        "real JPEG selection records no standard fallback");

    const standard_image_texture_pipeline_snapshot standard_snapshot = pipeline.standard_diagnostic_snapshot();
    require(standard_snapshot.decoder.support_check_count == 1, "real JPEG standard decoder records support check");
    require(standard_snapshot.decoder.decode_attempt_count == 1, "real JPEG standard decoder records decode attempt");
    require(standard_snapshot.decoder.decoded_count == 1, "real JPEG standard decoder records decoded count");
    require(standard_snapshot.decoder.failed_decode_count == 0, "real JPEG standard decoder records no failures");
    require(
        standard_snapshot.decoder.last_encoded_byte_count == fixture_bytes.size(),
        "real JPEG standard decoder records source byte count");
    require(
        standard_snapshot.decoder.last_decode_status == render_image_decode_status::decoded,
        "real JPEG standard decoder records decoded status");

    const fake_image_texture_pipeline_snapshot& snapshot = standard_snapshot.pipeline;
    require(snapshot.acquire_count == 1, "real JPEG snapshot records acquire");
    require(snapshot.ready_count == 1, "real JPEG snapshot records ready");
    require(snapshot.failure_count == 0, "real JPEG snapshot records no failures");
    require(snapshot.upload_snapshot.upload_count == 1, "real JPEG snapshot records one upload");
    require(snapshot.upload_snapshot.uploaded_decoded_byte_count == 4, "real JPEG snapshot records uploaded bytes");
    require(snapshot.upload_snapshot.request_snapshots.size() == 1, "real JPEG snapshot records upload request");
    require(snapshot.upload_snapshot.request_snapshots[0].width == 1, "real JPEG upload snapshot records width");
    require(snapshot.upload_snapshot.request_snapshots[0].height == 1, "real JPEG upload snapshot records height");
    require(
        snapshot.upload_snapshot.request_snapshots[0].decoded_byte_count == 4,
        "real JPEG upload snapshot records decoded bytes");
    require(
        snapshot.upload_snapshot.request_snapshots[0].staging_byte_count == 4,
        "real JPEG upload snapshot records staging bytes");
    require(snapshot.cache_snapshot.texture_count == 1, "real JPEG pipeline caches decoded texture");
    require(snapshot.cache_snapshot.cached_decoded_byte_count == 4, "real JPEG cache records decoded RGBA bytes");
    require(snapshot.entries[0].encoded_byte_count == fixture_bytes.size(), "real JPEG snapshot records source byte count");
    require(snapshot.entries[0].selected_decoder_id == "stb_image_decoder", "real JPEG snapshot records selected stb decoder");
    require(
        snapshot.entries[0].decoder_capability_manifest.used_third_party_adapter,
        "real JPEG snapshot manifest records adapter route");
    require(
        !snapshot.entries[0].decoder_capability_manifest.fallback_used,
        "real JPEG snapshot manifest records no fallback");
    require(
        snapshot.entries[0].decoder_capability_manifest.terminal_decoder_id == "stb_image_decoder",
        "real JPEG snapshot manifest records stb terminal decoder");
    require(snapshot.entries[0].upload_count_before == 0, "real JPEG snapshot records upload count before");
    require(snapshot.entries[0].upload_count_after == 1, "real JPEG snapshot records upload count after");
    require(!snapshot.cache_snapshot.entries.empty(), "real JPEG cache snapshot exposes cache entry");

    const render_image_upload_readiness_snapshot& readiness =
        snapshot.cache_snapshot.entries[0].upload_readiness;
    require(readiness.upload_ready, "real JPEG cache entry is upload-ready");
    require(readiness.decode_metadata_matches_image, "real JPEG cache entry validates decode handoff metadata");
    require(readiness.decoded_byte_count == 4, "real JPEG readiness records decoded bytes");
    require(readiness.metadata_decoded_byte_count == 4, "real JPEG readiness records metadata decoded bytes");
    require(readiness.metadata_expected_decoded_byte_count == 4, "real JPEG readiness records expected bytes");
    require(readiness.metadata_actual_decoded_byte_count == 4, "real JPEG readiness records actual bytes");
    require(readiness.staging_byte_count == 4, "real JPEG readiness records staging bytes");
    require(
        readiness.decode_handoff_diagnostic == "decode handoff metadata matches decoded image",
        "real JPEG readiness records matching handoff diagnostic");
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

void test_upload_readiness_reports_decode_handoff_metadata()
{
    using namespace quiz_vulkan::render;

    const render_image_texture_key key{
        .source_key = "textures/handoff.jpg",
        .sampler = render_image_sampler_policy{},
    };
    const render_decoded_image image = make_rgba_1x1_image(1, 2, 3, 4);
    const render_image_decode_request request{
        .source = render_resolved_image_source{
            .original_uri = "textures/handoff.jpg",
            .normalized_uri = "textures/handoff.jpg",
            .kind = render_image_source_kind::local_path,
        },
        .encoded_bytes = make_jpeg_signature_bytes(),
    };
    const render_image_decode_metadata metadata =
        make_render_image_decode_metadata("fake_stb_decoder", request, image);

    const render_image_upload_readiness_snapshot ready =
        make_render_image_upload_readiness_snapshot(key, metadata, image);

    require(ready.payload_valid, "upload readiness accepts valid decoded payload");
    require(ready.decode_metadata_matches_image, "upload readiness records matching decode handoff");
    require(ready.upload_ready, "matching decode handoff is upload-ready");
    require(ready.metadata_decoded_byte_count == 4, "upload readiness records metadata decoded bytes");
    require(ready.metadata_expected_decoded_byte_count == 4, "upload readiness records metadata expected bytes");
    require(ready.metadata_actual_decoded_byte_count == 4, "upload readiness records metadata actual bytes");
    require(
        ready.decode_handoff_diagnostic == "decode handoff metadata matches decoded image",
        "upload readiness records matching handoff diagnostic");

    render_image_decode_metadata mismatched_metadata = metadata;
    mismatched_metadata.decoded_byte_count = 8;
    const render_image_upload_readiness_snapshot mismatched =
        make_render_image_upload_readiness_snapshot(key, mismatched_metadata, image);

    require(mismatched.payload_valid, "handoff mismatch preserves valid payload evidence");
    require(!mismatched.decode_metadata_matches_image, "upload readiness detects metadata mismatch");
    require(!mismatched.upload_ready, "metadata mismatch blocks upload readiness");
    require(mismatched.metadata_decoded_byte_count == 8, "mismatch snapshot preserves metadata byte count");
    require(mismatched.decoded_byte_count == 4, "mismatch snapshot preserves payload byte count");
    require(
        mismatched.decode_handoff_diagnostic == "decode handoff metadata does not match decoded image",
        "upload readiness records mismatch handoff diagnostic");
    require(
        mismatched.diagnostic == "decode handoff metadata does not match decoded image",
        "upload readiness surfaces handoff mismatch as primary diagnostic");
}

void test_pipeline_uses_optional_third_party_decoder_through_interface()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({render_image_encoded_format::jpeg});
    backend.set_decoded_image(make_rgba_1x1_image(9, 8, 7, 6));
    fake_stb_image_decoder_dependency_probe probe;
    probe.set_available("fake_stb_decoder");
    optional_third_party_image_decoder_chain decoder(backend, probe);

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
    require(result.texture.external_decoder_selection.diagnostics_available, "third-party pipeline exposes stb selection diagnostics");
    require(result.texture.external_decoder_selection.decoder_id == "fake_stb_decoder", "stb selection records decoder id");
    require(result.texture.external_decoder_selection.selection_status_name == "ready", "stb selection records ready status");
    require(result.texture.external_decoder_selection.dependency_status_name == "available", "stb selection records available dependency");
    require(result.texture.external_decoder_selection.detected_format == render_image_encoded_format::jpeg, "stb selection records JPEG format");
    require(result.texture.external_decoder_selection.ready_for_external_decode, "stb selection records external decode readiness");
    require(result.texture.external_decoder_selection.used_third_party_adapter, "stb selection records adapter route");
    require(!result.texture.external_decoder_selection.fallback_to_standard_decoder_chain, "stb selection records no fallback for ready adapter");
    require(uploader.upload_requests.size() == 1, "third-party pipeline uploads decoded texture once");
    require(cache.cached_texture_count() == 1, "third-party pipeline caches decoded texture");
    require(backend.decode_requests.size() == 1, "third-party backend is called once");
    require(probe.probe_count == 1, "third-party pipeline probes stb dependency once");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.acquire_count == 1, "third-party pipeline snapshot records acquire");
    require(snapshot.ready_count == 1, "third-party pipeline snapshot records ready");
    require(snapshot.upload_snapshot.upload_count == 1, "third-party pipeline snapshot records upload");
    require(snapshot.cache_snapshot.texture_count == 1, "third-party pipeline snapshot records cached texture");
    require(snapshot.entries[0].decode_metadata.decoder_id == "fake_stb_decoder", "pipeline entry records third-party decoder");
    require(snapshot.entries[0].decoder_diagnostics[0].decoder_id == "fake_stb_decoder", "pipeline entry records adapter diagnostic");
    require(
        snapshot.entries[0].external_decoder_selection.used_third_party_adapter,
        "pipeline entry records adapter-ready stb selection");
    require(
        snapshot.entries[0].decoder_capability_manifest.external_decoder_selection.used_third_party_adapter,
        "pipeline entry capability manifest carries adapter-ready stb selection");
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

    require(!snapshot.cache_snapshot.entries.empty(), "third-party pipeline snapshot exposes cache entry");
    const render_image_upload_readiness_snapshot& readiness =
        snapshot.cache_snapshot.entries[0].upload_readiness;
    require(readiness.upload_ready, "third-party cache entry is upload-ready");
    require(readiness.decode_metadata_matches_image, "third-party cache entry validates decode handoff");
    require(readiness.metadata_decoded_byte_count == 4, "third-party handoff records metadata decoded bytes");
    require(readiness.metadata_expected_decoded_byte_count == 4, "third-party handoff records expected bytes");
    require(readiness.metadata_actual_decoded_byte_count == 4, "third-party handoff records actual bytes");
    require(
        readiness.decode_handoff_diagnostic == "decode handoff metadata matches decoded image",
        "third-party cache entry records handoff diagnostic");
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

void test_stb_selection_preserves_internal_bmp_decoder_diagnostics()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({render_image_encoded_format::bmp});
    backend.set_decoded_image(make_rgba_1x1_image(1, 2, 3, 4));
    fake_stb_image_decoder_dependency_probe probe;
    probe.set_available("fake_stb_decoder");
    optional_third_party_image_decoder_chain decoder(backend, probe);

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.bmp", make_bmp_24_bit_1x1_fixture_bytes());
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "asset://textures/card.bmp"});

    require(result.ok(), "internal BMP fallback succeeds through optional decoder chain");
    require(result.texture.decode_metadata.decoder_id == "bmp_image_decoder", "internal BMP decoder remains selected");
    require(result.texture.decode_metadata.width == 1, "internal BMP decode records width");
    require(result.texture.decode_metadata.height == 1, "internal BMP decode records height");
    require(result.texture.external_decoder_selection.diagnostics_available, "BMP path exposes stb selection diagnostics");
    require(
        result.texture.external_decoder_selection.selection_status_name == "fallback_internal_decoder_preferred",
        "BMP path records internal decoder preference");
    require(result.texture.external_decoder_selection.used_internal_decoder, "BMP path records internal decoder use");
    require(!result.texture.external_decoder_selection.used_third_party_adapter, "BMP path does not report adapter use");
    require(result.texture.external_decoder_selection.fallback_to_standard_decoder_chain, "BMP path records standard fallback");
    require(result.texture.external_decoder_selection.prefer_internal_decoder, "BMP path records matrix preference");
    require(result.texture.external_decoder_selection.internal_decoder_available, "BMP path records internal availability");
    require(
        result.texture.external_decoder_selection.detected_format == render_image_encoded_format::bmp,
        "BMP path records detected format");
    require(backend.decode_requests.empty(), "internal BMP path does not call third-party backend");
    require(uploader.upload_requests.size() == 1, "internal BMP path uploads decoded texture");
    require(probe.probe_count == 1, "internal BMP path probes stb dependency once");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.entries[0].selected_decoder_id == "bmp_image_decoder", "pipeline entry records BMP decoder");
    require(
        snapshot.entries[0].external_decoder_selection.used_internal_decoder,
        "pipeline entry carries internal decoder selection");
    require(
        snapshot.entries[0].decoder_capability_manifest.external_decoder_selection.used_internal_decoder,
        "capability manifest carries internal decoder selection");
    require(
        snapshot.entries[0].decoder_capability_manifest.terminal_decoder_id == "bmp_image_decoder",
        "capability manifest records BMP terminal decoder");
    require(snapshot.entries[0].decoder_capability_manifest.fallback_used, "capability manifest records adapter fallback");
}

void test_stb_selection_missing_and_mismatched_fallback_diagnostics_reach_pipeline()
{
    using namespace quiz_vulkan::render;

    auto acquire_jpeg_with_probe =
        [](fake_stb_image_decoder_dependency_probe& probe) {
            fake_third_party_image_decoder_backend backend;
            backend.set_decoder_id("fake_stb_decoder");
            backend.set_supported_formats({render_image_encoded_format::jpeg});
            backend.set_decoded_image(make_rgba_1x1_image(1, 2, 3, 4));
            optional_third_party_image_decoder_chain decoder(backend, probe);

            const normalizing_image_resolver resolver;
            fake_image_source_bytes_loader loader;
            loader.set_source_bytes("asset://textures/card.jpg", make_jpeg_signature_bytes());
            fake_image_texture_uploader uploader;
            fake_image_texture_cache cache(decoder, uploader);
            fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

            const render_image_texture_pipeline_result result = pipeline.acquire_texture(
                render_image_texture_pipeline_request{.uri = "asset://textures/card.jpg"});
            const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();

            require(!result.ok(), "unready stb path falls back to standard chain and fails unsupported JPEG");
            require(result.status == render_image_texture_pipeline_status::decode_failed, "unready stb path reports decode failure");
            require(result.texture.external_decoder_selection.diagnostics_available, "unready stb path exposes selection diagnostics");
            require(
                result.texture.external_decoder_selection.fallback_to_standard_decoder_chain,
                "unready stb path records standard fallback");
            require(!result.texture.external_decoder_selection.used_third_party_adapter, "unready stb path does not report adapter use");
            require(result.texture.decoder_diagnostics.size() == 4, "unready stb path records adapter plus standard candidates");
            require(result.texture.decoder_diagnostics[0].decoder_id == "fake_stb_decoder", "unready stb diagnostic records adapter first");
            require(!result.texture.decoder_diagnostics[0].decode_attempted, "unready stb path does not attempt adapter decode");
            require(uploader.upload_requests.empty(), "unready stb path does not upload");
            require(backend.decode_requests.empty(), "unready stb path does not call third-party backend");
            require(snapshot.entries[0].decoder_capability_manifest.fallback_used, "unready stb manifest records fallback");
            require(
                snapshot.entries[0].decoder_capability_manifest.external_decoder_selection.selection_status_name
                    == result.texture.external_decoder_selection.selection_status_name,
                "unready stb manifest carries selection status");
            require(
                snapshot.entries[0].decoder_capability_manifest.terminal_decoder_id == "unsupported_terminal",
                "unready stb manifest records unsupported terminal");
            return result;
        };

    fake_stb_image_decoder_dependency_probe missing_probe;
    missing_probe.set_missing("fake_stb_decoder");
    const render_image_texture_pipeline_result missing_result = acquire_jpeg_with_probe(missing_probe);
    require(missing_probe.probe_count == 1, "missing stb path probes dependency once");
    require(
        missing_result.texture.external_decoder_selection.selection_status_name == "fallback_missing_dependency",
        "missing stb path records missing dependency fallback");
    require(
        missing_result.texture.external_decoder_selection.fallback_due_to_missing_dependency,
        "missing stb path records missing dependency flag");
    require(
        missing_result.texture.external_decoder_selection.diagnostic.find("missing") != std::string::npos,
        "missing stb path records missing diagnostic");

    fake_stb_image_decoder_dependency_probe mismatched_probe;
    mismatched_probe.set_mismatched("fake_stb_decoder");
    const render_image_texture_pipeline_result mismatched_result = acquire_jpeg_with_probe(mismatched_probe);
    require(mismatched_probe.probe_count == 1, "mismatched stb path probes dependency once");
    require(
        mismatched_result.texture.external_decoder_selection.selection_status_name == "fallback_mismatched_capability",
        "mismatched stb path records mismatched capability fallback");
    require(
        mismatched_result.texture.external_decoder_selection.fallback_due_to_mismatched_capability,
        "mismatched stb path records mismatched capability flag");
    require(
        mismatched_result.texture.external_decoder_selection.diagnostic.find("capabilities") != std::string::npos,
        "mismatched stb path records capability diagnostic");
}

void test_stb_selection_survives_placeholder_fallback()
{
    using namespace quiz_vulkan::render;

    fake_third_party_image_decoder_backend backend;
    backend.set_decoder_id("fake_stb_decoder");
    backend.set_supported_formats({render_image_encoded_format::jpeg});
    fake_stb_image_decoder_dependency_probe probe;
    probe.set_missing("fake_stb_decoder");
    optional_third_party_image_decoder_chain decoder(backend, probe);

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.jpg", make_jpeg_signature_bytes());
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    cache.set_placeholder_texture_policy(fake_image_texture_placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    });
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_pipeline_result result = pipeline.acquire_texture(
        render_image_texture_pipeline_request{.uri = "asset://textures/card.jpg"});

    require(result.ok(), "missing stb dependency can still return placeholder fallback");
    require(is_fake_image_texture_placeholder_key(result.texture.key), "missing stb placeholder path returns placeholder key");
    require(result.texture.external_decoder_selection.diagnostics_available, "placeholder preserves external selection diagnostics");
    require(
        result.texture.external_decoder_selection.fallback_due_to_missing_dependency,
        "placeholder preserves missing dependency fallback flag");

    const fake_image_texture_pipeline_snapshot snapshot = pipeline.diagnostic_snapshot();
    require(snapshot.entries[0].placeholder_texture, "placeholder snapshot records placeholder texture");
    require(
        snapshot.entries[0].external_decoder_selection.fallback_due_to_missing_dependency,
        "placeholder snapshot carries missing dependency fallback flag");
    require(
        snapshot.entries[0].decoder_capability_manifest.external_decoder_selection.fallback_due_to_missing_dependency,
        "placeholder manifest carries missing dependency fallback flag");
}

void test_external_decoder_selection_diff_reports_adapter_internal_and_placeholder_states()
{
    using namespace quiz_vulkan::render;

    const fake_image_texture_pipeline_snapshot adapter_snapshot = make_optional_stb_pipeline_snapshot(
        "asset://textures/card.jpg",
        make_jpeg_signature_bytes(),
        make_available_stb_image_decoder_dependency_manifest("fake_stb_decoder"));
    const fake_image_texture_pipeline_snapshot internal_snapshot = make_optional_stb_pipeline_snapshot(
        "asset://textures/card.bmp",
        make_bmp_24_bit_1x1_fixture_bytes(),
        make_available_stb_image_decoder_dependency_manifest("fake_stb_decoder"));

    const render_image_external_decoder_selection_snapshot_diff internal_diff =
        diff_render_image_external_decoder_selection_snapshots(adapter_snapshot, internal_snapshot);

    require(internal_diff.has_changes, "external selection diff detects adapter to internal change");
    require(!internal_diff.has_regression, "adapter to internal decoder change is not a fallback regression");
    require(internal_diff.before_adapter_ready_count == 1, "external selection diff counts adapter-ready before");
    require(internal_diff.after_adapter_ready_count == 0, "external selection diff counts no adapter-ready after");
    require(internal_diff.before_internal_decoder_count == 0, "external selection diff counts no internal before");
    require(internal_diff.after_internal_decoder_count == 1, "external selection diff counts internal after");
    require(internal_diff.before_fallback_count == 0, "external selection diff counts no fallback before");
    require(internal_diff.after_fallback_count == 1, "external selection diff counts internal standard fallback after");
    require(internal_diff.changed_entry_count == 1, "external selection diff records one changed entry");
    require(internal_diff.state_changed_count == 1, "external selection diff counts state change");
    require(internal_diff.selected_decoder_changed_count == 1, "external selection diff counts selected decoder change");
    require(internal_diff.selection_status_changed_count == 1, "external selection diff counts selection status change");
    require(
        internal_diff.diagnostic == "external decoder selection diff reports changes",
        "external selection diff reports neutral changes");

    const render_image_external_decoder_selection_entry_diff& internal_entry = internal_diff.entries[0];
    require(
        internal_entry.before_state == render_image_external_decoder_selection_diff_state::adapter_ready,
        "external selection entry records adapter-ready before state");
    require(
        internal_entry.after_state == render_image_external_decoder_selection_diff_state::internal_decoder,
        "external selection entry records internal decoder after state");
    require(internal_entry.before_state_name == "adapter_ready", "adapter-ready state name is stable");
    require(internal_entry.after_state_name == "internal_decoder", "internal decoder state name is stable");
    require(internal_entry.before_selected_decoder_id == "fake_stb_decoder", "entry records before adapter decoder");
    require(internal_entry.after_selected_decoder_id == "bmp_image_decoder", "entry records after BMP decoder");
    require(internal_entry.before_ready_for_external_decode, "entry records before adapter readiness");
    require(internal_entry.after_used_internal_decoder, "entry records after internal decoder flag");
    require(!internal_entry.regression, "internal decoder route is not a severity regression");

    const fake_image_texture_pipeline_snapshot placeholder_snapshot = make_optional_stb_pipeline_snapshot(
        "asset://textures/card.jpg",
        make_jpeg_signature_bytes(),
        make_missing_stb_image_decoder_dependency_manifest("fake_stb_decoder"),
        true);
    const render_image_external_decoder_selection_snapshot_diff placeholder_diff =
        diff_render_image_external_decoder_selection_snapshots(adapter_snapshot, placeholder_snapshot);

    require(placeholder_diff.has_regression, "adapter to placeholder diff records fallback regression");
    require(placeholder_diff.placeholder_regressed, "adapter to placeholder diff records placeholder regression");
    require(placeholder_diff.fallback_regressed, "adapter to placeholder diff records fallback increase");
    require(placeholder_diff.after_placeholder_count == 1, "adapter to placeholder diff counts placeholder after");
    require(
        placeholder_diff.after_missing_dependency_fallback_count == 1,
        "adapter to placeholder diff preserves missing dependency flag");
    require(placeholder_diff.placeholder_changed_count == 1, "adapter to placeholder diff counts placeholder delta");
    require(
        placeholder_diff.regression_summary
            == "adapter-ready decoder selections decreased; external decoder fallback selections increased; placeholder texture fallbacks increased",
        "adapter to placeholder diff summarizes fallback regression");

    const render_image_external_decoder_selection_entry_diff& placeholder_entry =
        placeholder_diff.entries[0];
    require(
        placeholder_entry.after_state == render_image_external_decoder_selection_diff_state::placeholder,
        "external selection entry records placeholder after state");
    require(placeholder_entry.after_state_name == "placeholder", "placeholder state name is stable");
    require(placeholder_entry.after_placeholder_texture, "external selection entry records placeholder flag");
    require(placeholder_entry.after_missing_dependency_fallback, "external selection entry records missing dependency flag");
    require(placeholder_entry.regression, "external selection entry records placeholder regression");
    require(
        placeholder_entry.diagnostic == "external decoder selection changed with fallback regression",
        "external selection entry diagnostic names fallback regression");
}

void test_external_decoder_selection_diff_reports_missing_and_version_mismatch_fallbacks()
{
    using namespace quiz_vulkan::render;

    const fake_image_texture_pipeline_snapshot missing_snapshot = make_optional_stb_pipeline_snapshot(
        "asset://textures/card.jpg",
        make_jpeg_signature_bytes(),
        make_missing_stb_image_decoder_dependency_manifest("fake_stb_decoder"));
    const fake_image_texture_pipeline_snapshot version_mismatch_snapshot = make_optional_stb_pipeline_snapshot(
        "asset://textures/card.jpg",
        make_jpeg_signature_bytes(),
        make_mismatched_stb_image_decoder_dependency_manifest(
            "fake_stb_decoder",
            "stb_image dependency version mismatch; falling back to standard image decoders"));

    const render_image_external_decoder_selection_snapshot_diff diff =
        diff_render_image_external_decoder_selection_snapshots(missing_snapshot, version_mismatch_snapshot);

    require(diff.has_changes, "external selection diff detects missing to version mismatch change");
    require(!diff.has_regression, "missing to version mismatch keeps the same fallback severity");
    require(diff.before_missing_dependency_fallback_count == 1, "external selection diff counts missing before");
    require(diff.after_missing_dependency_fallback_count == 0, "external selection diff counts no missing after");
    require(diff.before_version_mismatch_fallback_count == 0, "external selection diff counts no version mismatch before");
    require(diff.after_version_mismatch_fallback_count == 1, "external selection diff counts version mismatch after");
    require(diff.before_fallback_count == 1, "external selection diff counts fallback before");
    require(diff.after_fallback_count == 1, "external selection diff counts fallback after");
    require(diff.changed_entry_count == 1, "external selection diff records one changed fallback entry");
    require(diff.dependency_status_changed_count == 1, "external selection diff counts dependency status change");
    require(diff.selection_status_changed_count == 1, "external selection diff counts selection status change");
    require(diff.diagnostic_changed_count == 1, "external selection diff counts diagnostic change");
    require(
        diff.diagnostic == "external decoder selection diff reports changes",
        "missing to version mismatch diff reports neutral changes");

    const render_image_external_decoder_selection_entry_diff& entry = diff.entries[0];
    require(
        entry.before_state == render_image_external_decoder_selection_diff_state::missing_dependency_fallback,
        "external selection entry records missing fallback before");
    require(
        entry.after_state == render_image_external_decoder_selection_diff_state::version_mismatch_fallback,
        "external selection entry records version mismatch fallback after");
    require(entry.before_state_name == "missing_dependency_fallback", "missing fallback state name is stable");
    require(entry.after_state_name == "version_mismatch_fallback", "version mismatch state name is stable");
    require(entry.before_dependency_status_name == "missing", "entry records missing dependency status before");
    require(
        entry.after_dependency_status_name == "mismatched_capability",
        "entry records mismatched dependency status after");
    require(entry.before_selection_status_name == "fallback_missing_dependency", "entry records missing selection before");
    require(
        entry.after_selection_status_name == "fallback_mismatched_capability",
        "entry records version mismatch selection after");
    require(entry.before_missing_dependency_fallback, "entry records missing dependency fallback before");
    require(entry.after_version_mismatch_fallback, "entry records version mismatch fallback after");
    require(entry.after_diagnostic.find("version mismatch") != std::string::npos, "entry preserves version mismatch diagnostic");
    require(!entry.regression, "version mismatch fallback is not worse than missing dependency fallback");
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

void test_texture_frame_snapshot_diff_reports_unchanged_frame()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_frame_snapshot_diff diff =
        diff_render_image_texture_frame_snapshots(frame, frame);

    require(diff.ok(), "unchanged frame diff is ok");
    require(!diff.has_changes, "unchanged frame diff records no changes");
    require(!diff.has_regression, "unchanged frame diff records no regressions");
    require(diff.before_request_count == 2, "unchanged frame diff records before request count");
    require(diff.after_request_count == 2, "unchanged frame diff records after request count");
    require(diff.unchanged_entry_count == 2, "unchanged frame diff counts unchanged entries");
    require(diff.added_entry_count == 0, "unchanged frame diff records no added entries");
    require(diff.removed_entry_count == 0, "unchanged frame diff records no removed entries");
    require(diff.changed_entry_count == 0, "unchanged frame diff records no changed entries");
    require(diff.texture_handle_added_count == 0, "unchanged frame diff records no added handles");
    require(diff.texture_handle_removed_count == 0, "unchanged frame diff records no removed handles");
    require(diff.texture_handle_changed_count == 0, "unchanged frame diff records no changed handles");
    require(diff.cache_key_changed_count == 0, "unchanged frame diff records no cache key changes");
    require(diff.sampler_changed_count == 0, "unchanged frame diff records no sampler changes");
    require(diff.placeholder_delta_count == 0, "unchanged frame diff records no placeholder delta");
    require(diff.failure_delta_count == 0, "unchanged frame diff records no failure delta");
    require(diff.request_success_delta_count == 0, "unchanged frame diff records no success delta");
    require(diff.residency_pressure_delta_count == 0, "unchanged frame diff records no pressure delta");
    require(
        diff.regression_summary == "image texture frame snapshot diff has no changes",
        "unchanged frame diff regression summary is stable");
    require(
        diff.diagnostic == "image texture frame snapshot diff is unchanged",
        "unchanged frame diff diagnostic is stable");
    require(diff.entries.size() == 2, "unchanged frame diff keeps per-request entries");
    require(
        diff.entries[0].status == render_image_texture_frame_snapshot_diff_entry_status::unchanged,
        "unchanged frame diff entry status is stable");
    require(diff.entries[0].status_name == "unchanged", "unchanged frame diff entry status name is stable");
    require(!diff.entries[0].changed(), "unchanged frame diff entry changed helper is false");
    require(diff.entries[0].ok(), "unchanged frame diff entry ok helper is true");
}

void test_texture_frame_snapshot_diff_reports_regressions_and_deltas()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    cache.set_placeholder_texture_policy(placeholder_policy);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_batch_plan before_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
    });
    const render_image_texture_batch_execution_diagnostics before_execution =
        execute_render_image_texture_batch_plan(before_plan, pipeline);
    const render_image_texture_frame_snapshot before_frame =
        make_render_image_texture_frame_snapshot(before_plan, before_execution);

    const render_image_texture_batch_plan after_plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{
            render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
            render_image_ref{.uri = "   "},
            render_image_ref{.uri = "asset://textures/bad.ppm"},
        },
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics after_execution = execute_render_image_texture_batch_plan(
        after_plan,
        pipeline,
        render_image_texture_residency_budget_plan_options{
            .max_resident_pixel_count = 1,
        });
    const render_image_texture_frame_snapshot after_frame =
        make_render_image_texture_frame_snapshot(after_plan, after_execution);
    const render_image_texture_frame_snapshot_diff diff =
        diff_render_image_texture_frame_snapshots(before_frame, after_frame);

    require(!diff.ok(), "regressing frame diff is not ok");
    require(diff.has_changes, "regressing frame diff records changes");
    require(diff.has_regression, "regressing frame diff records regression");
    require(diff.before_request_count == 2, "regressing frame diff records before request count");
    require(diff.after_request_count == 3, "regressing frame diff records after request count");
    require(diff.unchanged_entry_count == 0, "regressing frame diff records no unchanged entries");
    require(diff.added_entry_count == 1, "regressing frame diff records added request");
    require(diff.removed_entry_count == 0, "regressing frame diff records no removed request");
    require(diff.changed_entry_count == 2, "regressing frame diff records changed requests");
    require(diff.texture_handle_added_count == 1, "regressing frame diff records added texture handle");
    require(diff.texture_handle_removed_count == 1, "regressing frame diff records removed texture handle");
    require(diff.texture_handle_changed_count == 1, "regressing frame diff records changed texture handle");
    require(diff.cache_key_changed_count == 1, "regressing frame diff records cache key changes");
    require(diff.sampler_changed_count == 2, "regressing frame diff records sampler changes");
    require(diff.placeholder_delta_count == 1, "regressing frame diff records placeholder delta");
    require(diff.failure_delta_count == 1, "regressing frame diff records failure status delta");
    require(diff.request_success_delta_count == 2, "regressing frame diff records success deltas");
    require(diff.residency_pressure_delta_count == 3, "regressing frame diff records pressure deltas");
    require(diff.before_ready_count == 2, "regressing frame diff records before ready count");
    require(diff.after_ready_count == 2, "regressing frame diff records after ready count");
    require(diff.before_failure_count == 0, "regressing frame diff records before failure count");
    require(diff.after_failure_count == 1, "regressing frame diff records after failure count");
    require(diff.before_placeholder_texture_count == 0, "regressing frame diff records before placeholder count");
    require(diff.after_placeholder_texture_count == 1, "regressing frame diff records after placeholder count");
    require(diff.before_missing_texture_count == 0, "regressing frame diff records before missing count");
    require(diff.after_missing_texture_count == 1, "regressing frame diff records after missing count");
    require(diff.before_renderer_handoff_ready, "regressing frame diff records before handoff ready");
    require(!diff.after_renderer_handoff_ready, "regressing frame diff records after handoff not ready");
    require(!diff.before_residency_budget_pressure, "regressing frame diff records before no pressure");
    require(diff.after_residency_budget_pressure, "regressing frame diff records after pressure");
    require(diff.renderer_handoff_regressed, "regressing frame diff records handoff regression");
    require(!diff.renderer_handoff_recovered, "regressing frame diff records no handoff recovery");
    require(!diff.request_success_regressed, "regressing frame diff records no ready-count regression");
    require(diff.failure_count_regressed, "regressing frame diff records failure regression");
    require(diff.missing_texture_regressed, "regressing frame diff records missing texture regression");
    require(diff.placeholder_regressed, "regressing frame diff records placeholder regression");
    require(diff.residency_pressure_regressed, "regressing frame diff records pressure regression");
    require(
        diff.after_residency_pressure_status == render_image_texture_residency_budget_pressure_status::over_pixel_budget,
        "regressing frame diff records after pressure status");
    require(
        diff.regression_summary
            == "renderer handoff regressed; failures increased; missing textures increased; "
               "placeholder textures increased; residency pressure increased",
        "regressing frame diff regression summary is concise and stable");
    require(
        diff.diagnostic == "image texture frame snapshot diff reports regressions",
        "regressing frame diff diagnostic is stable");
    require(diff.entries.size() == 3, "regressing frame diff records per-request entries");

    const render_image_texture_frame_entry_diff& sampler_change = diff.entries[0];
    require(
        sampler_change.status == render_image_texture_frame_snapshot_diff_entry_status::changed,
        "sampler change entry records changed status");
    require(sampler_change.status_name == "changed", "sampler change status name is stable");
    require(sampler_change.texture_handle_changed, "sampler change entry records changed texture handle");
    require(!sampler_change.texture_handle_removed, "sampler change entry records no removed texture handle");
    require(!sampler_change.cache_key_changed, "sampler change entry keeps cache key");
    require(sampler_change.sampler_changed, "sampler change entry records sampler change");
    require(sampler_change.stable_texture_cache_key_changed, "sampler change entry records stable texture key change");
    require(sampler_change.residency_pressure_changed, "sampler change entry records pressure delta");
    require(sampler_change.regression, "sampler change entry records pressure regression");
    require(!sampler_change.ok(), "sampler change entry ok reflects regression");

    const render_image_texture_frame_entry_diff& failure_change = diff.entries[1];
    require(
        failure_change.status == render_image_texture_frame_snapshot_diff_entry_status::changed,
        "failure change entry records changed status");
    require(failure_change.texture_handle_removed, "failure change entry records removed texture handle");
    require(failure_change.cache_key_changed, "failure change entry records cache key change");
    require(failure_change.sampler_changed, "failure change entry records sampler change");
    require(failure_change.request_success_changed, "failure change entry records success delta");
    require(failure_change.failure_status_changed, "failure change entry records failure status delta");
    require(failure_change.regression, "failure change entry records regression");

    const render_image_texture_frame_entry_diff& added_placeholder = diff.entries[2];
    require(
        added_placeholder.status == render_image_texture_frame_snapshot_diff_entry_status::added,
        "added placeholder entry records added status");
    require(added_placeholder.texture_handle_added, "added placeholder entry records added texture handle");
    require(added_placeholder.after_texture_id != 0, "added placeholder entry records after texture id");
    require(added_placeholder.placeholder_changed, "added placeholder entry records placeholder delta");
    require(added_placeholder.request_success_changed, "added placeholder entry records success delta");
    require(added_placeholder.residency_pressure_changed, "added placeholder entry records pressure delta");
    require(added_placeholder.regression, "added placeholder entry records placeholder pressure regression");
}

void test_texture_frame_binding_plan_maps_renderer_handoff_packets()
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
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_frame_binding_plan binding_plan =
        make_render_image_texture_frame_binding_plan(frame);

    require(binding_plan.ok(), "frame binding plan is ready when frame handoff is ready");
    require(binding_plan.request_count == 3, "frame binding plan records request count");
    require(binding_plan.current_packet_count == 3, "frame binding plan records current packet count");
    require(binding_plan.packet_count == 3, "frame binding plan records packet count");
    require(binding_plan.bindable_packet_count == 3, "frame binding plan counts bindable packets");
    require(binding_plan.ready_packet_count == 3, "frame binding plan counts ready packets");
    require(binding_plan.placeholder_packet_count == 0, "frame binding plan records no placeholders");
    require(binding_plan.failed_packet_count == 0, "frame binding plan records no failed bindings");
    require(binding_plan.removed_packet_count == 0, "frame binding plan records no removed bindings");
    require(binding_plan.added_packet_count == 0, "frame binding plan records no added frame delta");
    require(binding_plan.changed_packet_count == 0, "frame binding plan records no changed frame delta");
    require(binding_plan.unchanged_packet_count == 3, "frame binding plan records unchanged packet delta");
    require(binding_plan.readiness_changed_count == 0, "frame binding plan records no readiness delta");
    require(binding_plan.readiness_regressed_count == 0, "frame binding plan records no readiness regression");
    require(binding_plan.readiness_recovered_count == 0, "frame binding plan records no readiness recovery");
    require(binding_plan.residency_pressure_packet_count == 3, "frame binding plan counts residency pressure packets");
    require(binding_plan.renderer_handoff_ready, "frame binding plan records renderer handoff readiness");
    require(binding_plan.residency_budget_pressure, "frame binding plan records residency pressure");
    require(
        binding_plan.residency_pressure_status
            == render_image_texture_residency_budget_pressure_status::over_texture_budget,
        "frame binding plan records residency pressure status");
    require(
        binding_plan.residency_pressure_status_name == "over_texture_budget",
        "frame binding plan records pressure status name");
    require(
        binding_plan.readiness_summary == "image texture frame binding packets unchanged",
        "frame binding plan records stable unchanged summary");
    require(
        binding_plan.diagnostic == "image texture frame binding plan ready with residency budget pressure",
        "frame binding plan ready diagnostic is stable");
    require(binding_plan.packets.size() == 3, "frame binding plan exposes binding packets");

    const render_image_texture_frame_binding_packet& first = binding_plan.packets[0];
    require(first.ok(), "first binding packet is bindable");
    require(first.status == render_image_texture_frame_binding_packet_status::ready, "first packet status is ready");
    require(first.status_name == "ready", "first packet status name is stable");
    require(first.request_index == 0, "first packet records request index");
    require(first.render_image_uri == "asset://textures/card.ppm", "first packet records source uri");
    require(first.normalized_uri == "asset://textures/card.ppm", "first packet records normalized source uri");
    require(first.cache_key == "asset://textures/card.ppm", "first packet records cache key");
    require(first.source_kind == render_image_source_kind::asset_uri, "first packet records source kind");
    require(first.sampler_key == frame.entries[0].sampler_policy.stable_key_fragment, "first packet records sampler key");
    require(first.texture_id == frame.entries[0].texture_id, "first packet records public texture id");
    require(first.texture_revision == frame.entries[0].texture_revision, "first packet records texture revision");
    require(first.texture_width == 2, "first packet records width");
    require(first.texture_height == 1, "first packet records height");
    require(first.has_texture, "first packet records texture availability");
    require(first.renderer_handoff_ready, "first packet records handoff readiness");
    require(!first.placeholder_texture, "first packet records non-placeholder state");
    require(!first.failed, "first packet records non-failed state");
    require(!first.removed, "first packet records non-removed state");
    require(!first.cache_reused, "first packet records first acquire as not reused");
    require(!first.expected_cache_reuse, "first packet records no reuse expectation");
    require(first.residency_budget_pressure, "first packet records residency pressure");
    require(first.residency_pressure_status_name == "over_texture_budget", "first packet records pressure name");
    require(!first.added_in_frame, "first packet records no added delta");
    require(!first.changed_in_frame, "first packet records no changed delta");
    require(first.unchanged_in_frame, "first packet records unchanged delta");

    const render_image_texture_frame_binding_packet& second = binding_plan.packets[1];
    require(second.texture_id == first.texture_id, "second packet records reused texture id");
    require(second.cache_reused, "second packet records cache reuse");
    require(second.expected_cache_reuse, "second packet records expected cache reuse");
    require(second.stable_texture_cache_key == first.stable_texture_cache_key, "second packet shares stable key");

    const render_image_texture_frame_binding_packet& third = binding_plan.packets[2];
    require(third.texture_id != first.texture_id, "third packet records sampler-separated texture id");
    require(third.sampler_policy.uses_nearest_filtering, "third packet records nearest sampler");
    require(third.sampler_key != first.sampler_key, "third packet records sampler-separated sampler key");
    require(third.stable_texture_cache_key != first.stable_texture_cache_key, "third packet records sampler-separated stable key");
}

void test_texture_frame_binding_plan_reports_frame_deltas_and_failures()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    cache.set_placeholder_texture_policy(placeholder_policy);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_batch_plan ready_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
    });
    const render_image_texture_batch_execution_diagnostics ready_execution =
        execute_render_image_texture_batch_plan(ready_plan, pipeline);
    const render_image_texture_frame_snapshot ready_frame =
        make_render_image_texture_frame_snapshot(ready_plan, ready_execution);

    const render_image_texture_batch_plan partial_plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{
            render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
            render_image_ref{.uri = "   "},
            render_image_ref{.uri = "asset://textures/bad.ppm"},
        },
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics partial_execution =
        execute_render_image_texture_batch_plan(partial_plan, pipeline);
    const render_image_texture_frame_snapshot partial_frame =
        make_render_image_texture_frame_snapshot(partial_plan, partial_execution);
    const render_image_texture_frame_binding_plan growth_plan =
        make_render_image_texture_frame_binding_plan(ready_frame, partial_frame);

    require(!growth_plan.ok(), "growth frame binding plan reports missing current binding");
    require(growth_plan.request_count == 3, "growth binding plan records current request count");
    require(growth_plan.current_packet_count == 3, "growth binding plan records current packets");
    require(growth_plan.packet_count == 3, "growth binding plan records current packets");
    require(growth_plan.bindable_packet_count == 2, "growth binding plan counts ready and placeholder bindable packets");
    require(growth_plan.ready_packet_count == 1, "growth binding plan counts one real ready packet");
    require(growth_plan.placeholder_packet_count == 1, "growth binding plan counts placeholder packet");
    require(growth_plan.failed_packet_count == 1, "growth binding plan counts failed packet");
    require(growth_plan.added_packet_count == 1, "growth binding plan counts added packet");
    require(growth_plan.changed_packet_count == 2, "growth binding plan counts changed packets");
    require(growth_plan.removed_packet_count == 0, "growth binding plan records no removed packet");
    require(growth_plan.readiness_changed_count == 2, "growth binding plan counts readiness changes");
    require(growth_plan.readiness_regressed_count == 1, "growth binding plan counts readiness regression");
    require(growth_plan.readiness_recovered_count == 0, "growth binding plan does not treat added packets as recovery");
    require(
        growth_plan.readiness_summary == "image texture frame binding readiness regressed",
        "growth binding plan readiness summary is stable");
    require(
        growth_plan.diagnostic == "image texture frame binding plan contains failed bindings",
        "growth binding plan failure diagnostic is stable");

    const render_image_texture_frame_binding_packet& changed_ready = growth_plan.packets[0];
    require(changed_ready.status == render_image_texture_frame_binding_packet_status::ready, "changed ready packet is ready");
    require(changed_ready.changed_in_frame, "changed ready packet records frame delta");
    require(!changed_ready.readiness_regressed, "changed ready packet does not regress readiness");

    const render_image_texture_frame_binding_packet& failed_packet = growth_plan.packets[1];
    require(failed_packet.status == render_image_texture_frame_binding_packet_status::failed, "failed packet status is stable");
    require(failed_packet.status_name == "failed", "failed packet status name is stable");
    require(failed_packet.failed, "failed packet records failed state");
    require(!failed_packet.has_texture, "failed packet has no texture id");
    require(failed_packet.changed_in_frame, "failed packet records changed frame delta");
    require(failed_packet.readiness_regressed, "failed packet records readiness regression");
    require(failed_packet.render_image_uri == "   ", "failed packet preserves invalid source uri");

    const render_image_texture_frame_binding_packet& added_placeholder = growth_plan.packets[2];
    require(
        added_placeholder.status == render_image_texture_frame_binding_packet_status::placeholder,
        "added placeholder packet status is stable");
    require(added_placeholder.status_name == "placeholder", "added placeholder status name is stable");
    require(added_placeholder.ok(), "added placeholder packet remains bindable");
    require(added_placeholder.added_in_frame, "added placeholder packet records added delta");
    require(added_placeholder.placeholder_texture, "added placeholder packet records placeholder state");
    require(!added_placeholder.failed, "added placeholder packet is not failed");
    require(added_placeholder.cache_key == "asset://textures/bad.ppm", "added placeholder packet records source cache key");

    const render_image_texture_frame_binding_plan shrink_plan =
        make_render_image_texture_frame_binding_plan(partial_frame, ready_frame);
    require(shrink_plan.ok(), "shrink binding plan is renderer-ready for current frame");
    require(shrink_plan.request_count == 2, "shrink binding plan records current request count");
    require(shrink_plan.current_packet_count == 2, "shrink binding plan records current packet count");
    require(shrink_plan.packet_count == 3, "shrink binding plan keeps removed packet diagnostics");
    require(shrink_plan.ready_packet_count == 2, "shrink binding plan counts current ready packets");
    require(shrink_plan.failed_packet_count == 0, "shrink binding plan records no failed current packets");
    require(shrink_plan.removed_packet_count == 1, "shrink binding plan counts removed packet");
    require(shrink_plan.changed_packet_count == 2, "shrink binding plan counts changed current packets");
    require(shrink_plan.readiness_changed_count == 2, "shrink binding plan counts readiness changes");
    require(shrink_plan.readiness_regressed_count == 1, "shrink binding plan counts removed ready packet regression");
    require(shrink_plan.readiness_recovered_count == 1, "shrink binding plan counts recovered current packet");
    require(
        shrink_plan.diagnostic == "image texture frame binding plan ready with frame delta",
        "shrink binding plan delta diagnostic is stable");

    const render_image_texture_frame_binding_packet& removed_packet = shrink_plan.packets[2];
    require(removed_packet.status == render_image_texture_frame_binding_packet_status::removed, "removed packet status is stable");
    require(removed_packet.status_name == "removed", "removed packet status name is stable");
    require(!removed_packet.ok(), "removed packet is not bindable");
    require(removed_packet.removed, "removed packet records removed state");
    require(removed_packet.removed_from_frame, "removed packet records removed frame delta");
    require(removed_packet.has_texture, "removed packet records previous texture id");
    require(removed_packet.placeholder_texture, "removed packet preserves previous placeholder state");
    require(removed_packet.readiness_regressed, "removed packet records readiness regression");
    require(removed_packet.render_image_uri == "asset://textures/bad.ppm", "removed packet records previous source uri");
    require(removed_packet.texture_key.source_key == removed_packet.cache_key, "removed packet records previous texture key");
    require(
        render_image_texture_frame_binding_packet_status_name(render_image_texture_frame_binding_packet_status::removed)
            == "removed",
        "removed binding packet status name is stable");
}

void test_texture_frame_binding_plan_diff_reports_unchanged_bindings()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_frame_binding_plan binding_plan =
        make_render_image_texture_frame_binding_plan(frame);
    const render_image_texture_frame_binding_plan_diff diff =
        diff_render_image_texture_frame_binding_plans(binding_plan, binding_plan);

    require(diff.ok(), "unchanged binding plan diff is ok");
    require(!diff.has_changes, "unchanged binding plan diff records no changes");
    require(!diff.has_regression, "unchanged binding plan diff records no regressions");
    require(diff.before_request_count == 2, "unchanged binding diff records before request count");
    require(diff.after_request_count == 2, "unchanged binding diff records after request count");
    require(diff.before_packet_count == 2, "unchanged binding diff records before packet count");
    require(diff.after_packet_count == 2, "unchanged binding diff records after packet count");
    require(diff.unchanged_packet_count == 2, "unchanged binding diff counts unchanged packets");
    require(diff.added_packet_count == 0, "unchanged binding diff records no added packets");
    require(diff.removed_packet_count == 0, "unchanged binding diff records no removed packets");
    require(diff.changed_packet_count == 0, "unchanged binding diff records no changed packets");
    require(diff.texture_binding_added_count == 0, "unchanged binding diff records no added texture bindings");
    require(diff.texture_binding_removed_count == 0, "unchanged binding diff records no removed texture bindings");
    require(diff.texture_binding_changed_count == 0, "unchanged binding diff records no changed texture bindings");
    require(diff.readiness_changed_count == 0, "unchanged binding diff records no readiness delta");
    require(diff.placeholder_delta_count == 0, "unchanged binding diff records no placeholder delta");
    require(diff.failure_delta_count == 0, "unchanged binding diff records no failure delta");
    require(diff.sampler_policy_changed_count == 0, "unchanged binding diff records no sampler delta");
    require(diff.source_uri_changed_count == 0, "unchanged binding diff records no source uri delta");
    require(diff.stable_uri_changed_count == 0, "unchanged binding diff records no stable uri delta");
    require(diff.cache_key_changed_count == 0, "unchanged binding diff records no cache key delta");
    require(diff.stable_texture_cache_key_changed_count == 0, "unchanged binding diff records no stable texture key delta");
    require(diff.residency_pressure_delta_count == 0, "unchanged binding diff records no pressure delta");
    require(diff.before_bindable_packet_count == 2, "unchanged binding diff records before bindable count");
    require(diff.after_bindable_packet_count == 2, "unchanged binding diff records after bindable count");
    require(diff.before_renderer_handoff_ready, "unchanged binding diff records before handoff ready");
    require(diff.after_renderer_handoff_ready, "unchanged binding diff records after handoff ready");
    require(
        diff.readiness_summary == "image texture frame binding readiness unchanged",
        "unchanged binding diff readiness summary is stable");
    require(
        diff.regression_summary == "image texture frame binding plan diff has no changes",
        "unchanged binding diff regression summary is stable");
    require(
        diff.diagnostic == "image texture frame binding plan diff is unchanged",
        "unchanged binding diff diagnostic is stable");
    require(diff.entries.size() == 2, "unchanged binding diff keeps packet entries");

    const render_image_texture_frame_binding_packet_diff& first = diff.entries[0];
    require(
        first.status == render_image_texture_frame_binding_plan_diff_entry_status::unchanged,
        "unchanged binding packet diff status is stable");
    require(first.status_name == "unchanged", "unchanged binding packet status name is stable");
    require(first.before_texture_id == binding_plan.packets[0].texture_id, "unchanged packet diff records before id");
    require(first.after_texture_id == binding_plan.packets[0].texture_id, "unchanged packet diff records after id");
    require(!first.changed(), "unchanged packet changed helper is false");
    require(first.ok(), "unchanged packet ok helper is true");
    require(
        render_image_texture_frame_binding_plan_diff_entry_status_name(
            render_image_texture_frame_binding_plan_diff_entry_status::changed)
            == "changed",
        "binding plan diff status name is stable");
}

void test_texture_frame_binding_plan_diff_reports_binding_deltas()
{
    using namespace quiz_vulkan::render;

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    cache.set_placeholder_texture_policy(placeholder_policy);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    const render_image_texture_batch_plan ready_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
    });
    const render_image_texture_batch_execution_diagnostics ready_execution =
        execute_render_image_texture_batch_plan(ready_plan, pipeline);
    const render_image_texture_frame_snapshot ready_frame =
        make_render_image_texture_frame_snapshot(ready_plan, ready_execution);
    const render_image_texture_frame_binding_plan ready_binding_plan =
        make_render_image_texture_frame_binding_plan(ready_frame);

    const render_image_texture_batch_plan partial_plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{
            render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
            render_image_ref{.uri = "   "},
            render_image_ref{.uri = "asset://textures/bad.ppm"},
        },
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics partial_execution =
        execute_render_image_texture_batch_plan(partial_plan, pipeline);
    const render_image_texture_frame_snapshot partial_frame =
        make_render_image_texture_frame_snapshot(partial_plan, partial_execution);
    const render_image_texture_frame_binding_plan partial_binding_plan =
        make_render_image_texture_frame_binding_plan(partial_frame);

    const render_image_texture_frame_binding_plan_diff growth_diff =
        diff_render_image_texture_frame_binding_plans(ready_binding_plan, partial_binding_plan);

    require(!growth_diff.ok(), "growth binding diff records regressions");
    require(growth_diff.has_changes, "growth binding diff records changes");
    require(growth_diff.has_regression, "growth binding diff records regression");
    require(growth_diff.before_request_count == 2, "growth binding diff records before request count");
    require(growth_diff.after_request_count == 3, "growth binding diff records after request count");
    require(growth_diff.before_packet_count == 2, "growth binding diff records before packet count");
    require(growth_diff.after_packet_count == 3, "growth binding diff records after packet count");
    require(growth_diff.unchanged_packet_count == 0, "growth binding diff records no unchanged packets");
    require(growth_diff.added_packet_count == 1, "growth binding diff counts added packet");
    require(growth_diff.removed_packet_count == 0, "growth binding diff records no removed packet");
    require(growth_diff.changed_packet_count == 2, "growth binding diff counts changed packets");
    require(growth_diff.texture_binding_added_count == 1, "growth binding diff counts added texture binding");
    require(growth_diff.texture_binding_removed_count == 1, "growth binding diff counts removed texture binding");
    require(growth_diff.texture_binding_changed_count == 1, "growth binding diff counts changed texture binding");
    require(growth_diff.readiness_changed_count == 2, "growth binding diff counts readiness changes");
    require(growth_diff.readiness_regressed_count == 1, "growth binding diff counts readiness regression");
    require(growth_diff.readiness_recovered_count == 0, "growth binding diff records no readiness recovery");
    require(growth_diff.placeholder_delta_count == 1, "growth binding diff counts placeholder delta");
    require(growth_diff.failure_delta_count == 1, "growth binding diff counts failure delta");
    require(growth_diff.sampler_policy_changed_count == 2, "growth binding diff counts sampler policy changes");
    require(growth_diff.source_uri_changed_count == 1, "growth binding diff counts source uri change");
    require(growth_diff.stable_uri_changed_count == 1, "growth binding diff counts stable uri change");
    require(growth_diff.cache_key_changed_count == 1, "growth binding diff counts cache key change");
    require(growth_diff.stable_texture_cache_key_changed_count == 2, "growth binding diff counts stable texture key changes");
    require(growth_diff.residency_pressure_delta_count == 0, "growth binding diff records no pressure delta");
    require(growth_diff.before_bindable_packet_count == 2, "growth binding diff records before bindable packets");
    require(growth_diff.after_bindable_packet_count == 2, "growth binding diff records after bindable packets");
    require(growth_diff.before_ready_packet_count == 2, "growth binding diff records before ready packets");
    require(growth_diff.after_ready_packet_count == 1, "growth binding diff records after ready packets");
    require(growth_diff.after_placeholder_packet_count == 1, "growth binding diff records after placeholder packets");
    require(growth_diff.after_failed_packet_count == 1, "growth binding diff records after failed packets");
    require(growth_diff.before_renderer_handoff_ready, "growth binding diff records before handoff ready");
    require(!growth_diff.after_renderer_handoff_ready, "growth binding diff records after handoff not ready");
    require(growth_diff.renderer_handoff_regressed, "growth binding diff records handoff regression");
    require(growth_diff.failure_count_regressed, "growth binding diff records failure count regression");
    require(growth_diff.placeholder_count_regressed, "growth binding diff records placeholder count regression");
    require(
        growth_diff.readiness_summary == "image texture frame binding readiness regressed",
        "growth binding diff readiness summary is stable");
    require(
        growth_diff.regression_summary
            == "binding readiness regressed; binding failures increased; placeholder bindings increased",
        "growth binding diff regression summary is stable");
    require(
        growth_diff.diagnostic == "image texture frame binding plan diff reports regressions",
        "growth binding diff diagnostic is stable");
    require(growth_diff.entries.size() == 3, "growth binding diff records packet entries");

    const render_image_texture_frame_binding_packet_diff& sampler_change = growth_diff.entries[0];
    require(
        sampler_change.status == render_image_texture_frame_binding_plan_diff_entry_status::changed,
        "sampler binding diff entry records changed status");
    require(sampler_change.texture_binding_changed, "sampler binding diff records texture binding change");
    require(sampler_change.sampler_policy_changed, "sampler binding diff records sampler policy change");
    require(!sampler_change.cache_key_changed, "sampler binding diff keeps cache key");
    require(!sampler_change.stable_uri_changed, "sampler binding diff keeps stable uri");
    require(!sampler_change.readiness_regressed, "sampler binding diff does not regress readiness");
    require(sampler_change.ok(), "sampler binding diff has no regression without pressure");

    const render_image_texture_frame_binding_packet_diff& failed_change = growth_diff.entries[1];
    require(
        failed_change.status == render_image_texture_frame_binding_plan_diff_entry_status::changed,
        "failed binding diff entry records changed status");
    require(failed_change.texture_binding_removed, "failed binding diff records removed texture binding");
    require(failed_change.readiness_regressed, "failed binding diff records readiness regression");
    require(failed_change.failure_changed, "failed binding diff records failure delta");
    require(failed_change.sampler_policy_changed, "failed binding diff records sampler delta");
    require(failed_change.source_uri_changed, "failed binding diff records source uri delta");
    require(failed_change.stable_uri_changed, "failed binding diff records stable uri delta");
    require(failed_change.cache_key_changed, "failed binding diff records cache key delta");
    require(failed_change.regression, "failed binding diff records regression");

    const render_image_texture_frame_binding_packet_diff& added_placeholder = growth_diff.entries[2];
    require(
        added_placeholder.status == render_image_texture_frame_binding_plan_diff_entry_status::added,
        "added binding diff entry records added status");
    require(added_placeholder.texture_binding_added, "added binding diff records texture binding added");
    require(added_placeholder.placeholder_changed, "added binding diff records placeholder delta");
    require(added_placeholder.readiness_changed, "added binding diff records readiness delta");
    require(!added_placeholder.sampler_policy_changed, "added binding diff does not count sampler as changed");
    require(!added_placeholder.cache_key_changed, "added binding diff does not count cache key as changed");
    require(added_placeholder.regression, "added placeholder binding diff records placeholder regression");

    const render_image_texture_frame_binding_plan_diff shrink_diff =
        diff_render_image_texture_frame_binding_plans(partial_binding_plan, ready_binding_plan);
    require(shrink_diff.has_changes, "shrink binding diff records changes");
    require(shrink_diff.has_regression, "shrink binding diff records removed ready binding regression");
    require(shrink_diff.removed_packet_count == 1, "shrink binding diff counts removed packet");
    require(shrink_diff.added_packet_count == 0, "shrink binding diff records no added packets");
    require(shrink_diff.changed_packet_count == 2, "shrink binding diff counts changed packets");
    require(shrink_diff.texture_binding_removed_count == 1, "shrink binding diff counts removed texture binding");
    require(shrink_diff.texture_binding_added_count == 1, "shrink binding diff counts recovered texture binding");
    require(shrink_diff.readiness_changed_count == 2, "shrink binding diff counts readiness changes");
    require(shrink_diff.readiness_regressed_count == 1, "shrink binding diff counts removed readiness regression");
    require(shrink_diff.readiness_recovered_count == 1, "shrink binding diff counts readiness recovery");
    require(shrink_diff.failure_count_recovered, "shrink binding diff records failure count recovery");
    require(shrink_diff.placeholder_count_recovered, "shrink binding diff records placeholder count recovery");
    require(
        shrink_diff.diagnostic == "image texture frame binding plan diff reports regressions",
        "shrink binding diff diagnostic reports removed ready regression");

    const render_image_texture_frame_binding_packet_diff& removed_packet = shrink_diff.entries[2];
    require(
        removed_packet.status == render_image_texture_frame_binding_plan_diff_entry_status::removed,
        "removed binding diff entry records removed status");
    require(removed_packet.texture_binding_removed, "removed binding diff records removed texture binding");
    require(removed_packet.placeholder_changed, "removed binding diff records placeholder delta");
    require(removed_packet.readiness_regressed, "removed binding diff records readiness regression");
    require(removed_packet.before_render_image_uri == "asset://textures/bad.ppm", "removed binding diff records source uri");
}

void test_texture_frame_upload_handoff_links_bindings_to_upload_results()
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
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_frame_binding_plan binding_plan =
        make_render_image_texture_frame_binding_plan(frame);
    const render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(uploader.diagnostic_snapshot());
    const render_image_texture_frame_upload_handoff_summary handoff =
        make_render_image_texture_frame_upload_handoff_summary(frame, binding_plan, upload_result);

    require(handoff.ok(), "frame upload handoff is ready when bindings and uploads are ready");
    require(handoff.frame_request_count == 3, "frame upload handoff records frame request count");
    require(handoff.binding_packet_count == 3, "frame upload handoff records binding packets");
    require(handoff.upload_packet_count == 2, "frame upload handoff records only cache-miss upload packets");
    require(handoff.requested_texture_count == 3, "frame upload handoff records render image refs");
    require(handoff.ready_texture_count == 3, "frame upload handoff records ready texture refs");
    require(handoff.placeholder_texture_count == 0, "frame upload handoff records no placeholders");
    require(handoff.blocked_texture_count == 0, "frame upload handoff records no blocked refs");
    require(handoff.missing_upload_result_count == 0, "frame upload handoff records no missing upload packets");
    require(handoff.cache_reused_count == 1, "frame upload handoff records cache reuse");
    require(handoff.expected_cache_reuse_count == 1, "frame upload handoff records expected reuse");
    require(handoff.unique_texture_id_count == 2, "frame upload handoff deduplicates texture handles");
    require(handoff.unique_texture_cache_key_count == 2, "frame upload handoff deduplicates texture cache keys");
    require(handoff.uploaded_byte_count == 16, "frame upload handoff sums unique uploaded bytes");
    require(handoff.planned_staging_byte_count == 16, "frame upload handoff sums planned staging bytes");
    require(handoff.total_mip_level_count == 2, "frame upload handoff sums uploaded mip levels");
    require(handoff.accepted_mip_level_count == 2, "frame upload handoff records accepted mip levels");
    require(!handoff.has_blockers, "frame upload handoff has no blockers");
    require(!handoff.has_placeholders, "frame upload handoff has no placeholders");
    require(handoff.cache_key_summary.find("asset://textures/card.ppm") != std::string::npos, "frame upload handoff summarizes cache keys");
    require(handoff.sampler_summary.find("min=nearest") != std::string::npos, "frame upload handoff summarizes sampler keys");
    require(
        handoff.mip_level_summary == "accepted_mips=2; rejected_mips=0; uploaded_bytes=16",
        "frame upload handoff mip summary is stable");
    require(
        handoff.diagnostic == "image frame upload handoff ready",
        "frame upload handoff ready diagnostic is stable");
    require(handoff.entries.size() == 3, "frame upload handoff has one entry per binding packet");

    const render_image_texture_frame_upload_handoff_entry& first = handoff.entries[0];
    require(first.ok(), "first handoff entry is ready");
    require(first.status == render_image_texture_frame_upload_handoff_entry_status::ready, "first handoff status is ready");
    require(first.status_name == "ready", "first handoff status name is stable");
    require(first.request_index == 0, "first handoff entry records request index");
    require(first.render_image_uri == "asset://textures/card.ppm", "first handoff entry records uri");
    require(first.upload_result_present, "first handoff entry links upload result");
    require(first.upload_request_id == 1, "first handoff entry records upload request id");
    require(first.uploaded_byte_count == 8, "first handoff entry records uploaded bytes");
    require(first.mip_level_count == 1, "first handoff entry records mip level count");
    require(!first.placeholder_texture, "first handoff entry is not placeholder");
    require(!first.blocked, "first handoff entry is not blocked");

    const render_image_texture_frame_upload_handoff_entry& second = handoff.entries[1];
    require(second.ok(), "cache-hit handoff entry remains ready");
    require(second.cache_reused, "cache-hit handoff entry records cache reuse");
    require(second.expected_cache_reuse, "cache-hit handoff entry records expected reuse");
    require(second.texture_id == first.texture_id, "cache-hit handoff entry shares texture id");
    require(second.upload_request_id == first.upload_request_id, "cache-hit handoff entry links the reused upload packet");

    const render_image_texture_frame_upload_handoff_entry& third = handoff.entries[2];
    require(third.ok(), "sampler-separated handoff entry is ready");
    require(third.upload_request_id == 2, "sampler-separated handoff entry links second upload packet");
    require(third.texture_id != first.texture_id, "sampler-separated handoff entry records distinct texture id");
    require(third.stable_texture_cache_key != first.stable_texture_cache_key, "sampler-separated handoff entry has distinct key");

    require(
        render_image_texture_frame_upload_result_packet_for_binding_packet(upload_result, binding_plan.packets[1])
            == &upload_result.packets[0],
        "frame upload handoff lookup resolves cache-hit binding to original upload packet");
    require(
        render_image_texture_frame_upload_handoff_entry_status_name(
            render_image_texture_frame_upload_handoff_entry_status::missing_upload_result)
            == "missing_upload_result",
        "frame upload handoff status names are stable");
}

void test_texture_frame_upload_handoff_reports_placeholders_and_blockers()
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
    const render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(uploader.diagnostic_snapshot());
    const render_image_texture_frame_upload_handoff_summary handoff =
        make_render_image_texture_frame_upload_handoff_summary(frame, upload_result);

    require(!handoff.ok(), "frame upload handoff reports invalid request blocker");
    require(!handoff.renderer_handoff_ready, "frame upload handoff is not renderer-ready with blocker");
    require(handoff.frame_request_count == 2, "blocked handoff records frame request count");
    require(handoff.binding_packet_count == 2, "blocked handoff records binding packet count");
    require(handoff.upload_packet_count == 1, "blocked handoff records placeholder upload packet");
    require(handoff.requested_texture_count == 2, "blocked handoff records current requested textures");
    require(handoff.ready_texture_count == 0, "blocked handoff records no real ready textures");
    require(handoff.placeholder_texture_count == 1, "blocked handoff records placeholder texture");
    require(handoff.blocked_texture_count == 1, "blocked handoff records one blocked request");
    require(handoff.missing_upload_result_count == 1, "blocked handoff records missing upload result");
    require(handoff.uploaded_byte_count == 16, "blocked handoff records placeholder upload bytes");
    require(handoff.total_mip_level_count == 1, "blocked handoff records placeholder mip level");
    require(handoff.has_blockers, "blocked handoff has blockers");
    require(handoff.has_placeholders, "blocked handoff has placeholder");
    require(
        handoff.diagnostic == "image frame upload handoff has blocked texture uploads",
        "blocked frame upload handoff diagnostic is stable");
    require(handoff.entries.size() == 2, "blocked handoff has one entry per binding packet");

    const render_image_texture_frame_upload_handoff_entry& missing = handoff.entries[0];
    require(!missing.ok(), "missing upload handoff entry is blocked");
    require(
        missing.status == render_image_texture_frame_upload_handoff_entry_status::missing_upload_result,
        "missing upload handoff entry reports missing upload result");
    require(missing.status_name == "missing_upload_result", "missing upload status name is stable");
    require(missing.missing_upload_result, "missing upload entry carries missing flag");
    require(missing.blocked, "missing upload entry carries blocked flag");
    require(!missing.upload_result_present, "missing upload entry has no upload packet");
    require(missing.render_image_uri == "   ", "missing upload entry preserves invalid uri");

    const render_image_texture_frame_upload_handoff_entry& placeholder = handoff.entries[1];
    require(placeholder.ok(), "placeholder upload handoff entry is acceptable");
    require(
        placeholder.status == render_image_texture_frame_upload_handoff_entry_status::placeholder,
        "placeholder upload handoff status is stable");
    require(placeholder.placeholder_texture, "placeholder upload handoff carries placeholder flag");
    require(placeholder.upload_result_present, "placeholder upload handoff links upload result");
    require(placeholder.uploaded_byte_count == 16, "placeholder upload handoff records uploaded bytes");
    require(placeholder.mip_level_count == 1, "placeholder upload handoff records mip level");
    require(is_fake_image_texture_placeholder_key(placeholder.texture_key), "placeholder handoff keeps placeholder key");
}

void test_texture_frame_upload_handoff_preserves_blocked_placeholder_retry_evidence()
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
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(uploader.diagnostic_snapshot());
    require(upload_result.packets.size() == 1, "blocked placeholder fixture has one upload result packet");

    render_image_texture_upload_result_packet_snapshot& packet = upload_result.packets[0];
    packet.status = render_image_texture_upload_result_packet_status::rejected_retryable;
    packet.status_name = render_image_texture_upload_result_packet_status_name(packet.status);
    packet.upload_status = render_image_texture_upload_status::invalid_image;
    packet.upload_status_name = "invalid_image";
    packet.accepted = false;
    packet.rejected = true;
    packet.blocked = true;
    packet.retryable = true;
    packet.nonretryable_failure = false;
    packet.accepted_mip_level_count = 0;
    packet.rejected_mip_level_count = packet.mip_level_count;
    packet.uploaded_byte_count = 0;
    packet.retry_eligibility_name = "eligible";
    packet.attempt_count_for_key = 3;
    packet.failed_attempt_count_for_key = 2;
    packet.retry_after_queue_sequence_delta = 2;
    packet.next_retry_sequence = 9;
    packet.blocker_summary = "upload failed but can retry: invalid_image";

    upload_result.accepted_packet_count = 0;
    upload_result.rejected_packet_count = 1;
    upload_result.retryable_rejected_packet_count = 1;
    upload_result.blocker_count = 1;
    upload_result.has_rejections = true;
    upload_result.has_retryable_rejections = true;
    upload_result.total_uploaded_byte_count = 0;
    upload_result.accepted_mip_level_count = 0;
    upload_result.rejected_mip_level_count = packet.mip_level_count;

    const render_image_texture_frame_upload_handoff_summary handoff =
        make_render_image_texture_frame_upload_handoff_summary(frame, upload_result);

    require(!handoff.ok(), "blocked placeholder handoff is not ok");
    require(handoff.has_blockers, "blocked placeholder handoff records blockers");
    require(handoff.has_placeholders, "blocked placeholder handoff still records placeholder evidence");
    require(handoff.has_retry_blockers, "blocked placeholder handoff records retry blocker evidence");
    require(handoff.placeholder_texture_count == 1, "blocked placeholder handoff counts placeholder texture evidence");
    require(handoff.blocked_texture_count == 1, "blocked placeholder handoff counts blocked upload");
    require(handoff.retry_blocker_count == 1, "blocked placeholder handoff counts retry blocker");
    require(handoff.uploaded_byte_count == 0, "blocked placeholder handoff records no accepted upload bytes");
    require(
        handoff.retry_blocker_summary == "upload failed but can retry: invalid_image",
        "blocked placeholder retry blocker summary is stable");

    const render_image_texture_frame_upload_handoff_entry& entry = handoff.entries[0];
    require(
        entry.status == render_image_texture_frame_upload_handoff_entry_status::blocked,
        "blocked placeholder handoff entry uses blocked status");
    require(entry.placeholder_texture, "blocked placeholder entry preserves placeholder flag");
    require(entry.retryable_blocker, "blocked placeholder entry preserves retryable flag");
    require(entry.retry_eligibility_name == "eligible", "blocked placeholder entry carries retry eligibility");
    require(entry.attempt_count_for_key == 3, "blocked placeholder entry carries attempt count");
    require(entry.failed_attempt_count_for_key == 2, "blocked placeholder entry carries failed attempt count");
    require(entry.retry_after_queue_sequence_delta == 2, "blocked placeholder entry carries retry backoff delta");
    require(entry.next_retry_sequence == 9, "blocked placeholder entry carries next retry sequence");
    require(entry.blocker_summary == "upload failed but can retry: invalid_image", "blocked placeholder entry carries blocker summary");
}

void test_texture_frame_upload_handoff_distinguishes_missing_upload_and_missing_binding()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(uploader.diagnostic_snapshot());

    const render_image_texture_frame_upload_handoff_summary missing_upload =
        make_render_image_texture_frame_upload_handoff_summary(frame, render_image_texture_upload_result_snapshot{});
    require(!missing_upload.ok(), "missing upload result handoff is blocked");
    require(missing_upload.blocked_texture_count == 1, "missing upload result handoff counts blocker");
    require(missing_upload.missing_upload_result_count == 1, "missing upload result handoff has missing upload category");
    require(missing_upload.missing_frame_binding_count == 0, "missing upload result handoff does not report missing binding");
    require(missing_upload.entries.size() == 1, "missing upload result handoff keeps binding entry");
    require(
        missing_upload.entries[0].status
            == render_image_texture_frame_upload_handoff_entry_status::missing_upload_result,
        "missing upload result entry status is distinct");

    const render_image_texture_frame_upload_handoff_summary missing_binding =
        make_render_image_texture_frame_upload_handoff_summary(
            render_image_texture_frame_snapshot{},
            upload_result);
    require(!missing_binding.ok(), "missing frame binding handoff is not renderer-ready for empty frame");
    require(missing_binding.requested_texture_count == 0, "missing frame binding handoff does not count a frame request");
    require(missing_binding.blocked_texture_count == 0, "missing frame binding handoff is not a frame blocker");
    require(missing_binding.missing_upload_result_count == 0, "missing frame binding handoff does not report missing upload");
    require(missing_binding.missing_frame_binding_count == 1, "missing frame binding handoff counts orphan upload packet");
    require(missing_binding.entries.size() == 1, "missing frame binding handoff creates orphan upload entry");
    require(
        missing_binding.entries[0].status
            == render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding,
        "missing frame binding entry status is distinct");
    require(missing_binding.entries[0].upload_result_present, "missing frame binding entry carries upload result");
    require(!missing_binding.entries[0].requested, "missing frame binding entry is not a frame request");
    require(
        missing_binding.diagnostic == "image frame upload handoff has upload results without frame bindings",
        "missing frame binding diagnostic is stable");
    require(
        !render_image_texture_frame_upload_handoff_entry_status_is_blocked(
            render_image_texture_frame_upload_handoff_entry_status::missing_frame_binding),
        "missing frame binding status is not a current-frame blocker");
}

void test_texture_frame_upload_handoff_diff_reports_cache_key_and_sampler_changes()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;

    fake_image_source_bytes_loader before_loader;
    before_loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder before_decoder;
    fake_image_texture_uploader before_uploader;
    fake_image_texture_cache before_cache(before_decoder, before_uploader);
    fake_image_texture_pipeline before_pipeline(resolver, before_loader, before_cache, before_uploader);

    const render_image_texture_batch_plan before_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics before_execution =
        execute_render_image_texture_batch_plan(before_plan, before_pipeline);
    const render_image_texture_frame_snapshot before_frame =
        make_render_image_texture_frame_snapshot(before_plan, before_execution);
    const render_image_texture_frame_upload_handoff_summary before_handoff =
        make_render_image_texture_frame_upload_handoff_summary(
            before_frame,
            make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
                before_uploader.diagnostic_snapshot()));

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    fake_image_source_bytes_loader after_loader;
    after_loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder after_decoder;
    fake_image_texture_uploader after_uploader;
    fake_image_texture_cache after_cache(after_decoder, after_uploader);
    fake_image_texture_pipeline after_pipeline(resolver, after_loader, after_cache, after_uploader);

    const render_image_texture_batch_plan after_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
    });
    const render_image_texture_batch_execution_diagnostics after_execution =
        execute_render_image_texture_batch_plan(after_plan, after_pipeline);
    const render_image_texture_frame_snapshot after_frame =
        make_render_image_texture_frame_snapshot(after_plan, after_execution);
    const render_image_texture_frame_upload_handoff_summary after_handoff =
        make_render_image_texture_frame_upload_handoff_summary(
            after_frame,
            make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
                after_uploader.diagnostic_snapshot()));

    const render_image_texture_frame_upload_handoff_summary_diff diff =
        diff_render_image_texture_frame_upload_handoff_summaries(before_handoff, after_handoff);

    require(diff.ok(), "sampler handoff diff changes without regression");
    require(diff.has_changes, "sampler handoff diff records changes");
    require(!diff.has_regression, "sampler handoff diff records no regression");
    require(diff.changed_entry_count == 1, "sampler handoff diff counts changed entry");
    require(diff.cache_key_changed_count == 1, "sampler handoff diff counts texture cache key change");
    require(diff.sampler_changed_count == 1, "sampler handoff diff counts sampler policy change");
    require(diff.entries.size() == 1, "sampler handoff diff keeps one entry");

    const render_image_texture_frame_upload_handoff_entry_diff& entry = diff.entries[0];
    require(
        entry.status == render_image_texture_frame_upload_handoff_diff_entry_status::changed,
        "sampler handoff diff entry status is changed");
    require(entry.cache_key_changed, "sampler handoff diff entry records stable cache-key change");
    require(entry.sampler_changed, "sampler handoff diff entry records sampler change");
    require(!entry.blocker_changed, "sampler handoff diff entry has no blocker change");
    require(!entry.placeholder_changed, "sampler handoff diff entry has no placeholder change");
    require(!entry.regression, "sampler handoff diff entry has no regression");
    require(entry.before_stable_texture_cache_key != entry.after_stable_texture_cache_key, "sampler handoff diff changes stable texture key");
    require(entry.before_sampler_summary != entry.after_sampler_summary, "sampler handoff diff changes sampler summary");
}

void test_texture_frame_binding_summary_reports_ready_reuse_and_placeholder_states()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader ready_loader;
    ready_loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder ready_decoder;
    fake_image_texture_uploader ready_uploader;
    fake_image_texture_cache ready_cache(ready_decoder, ready_uploader);
    fake_image_texture_pipeline ready_pipeline(resolver, ready_loader, ready_cache, ready_uploader);

    const render_image_texture_batch_plan ready_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
        render_image_ref{.uri = "  ASSET:///textures\\card.ppm  "},
    });
    const render_image_texture_batch_execution_diagnostics ready_execution =
        execute_render_image_texture_batch_plan(ready_plan, ready_pipeline);
    const render_image_texture_frame_snapshot ready_frame =
        make_render_image_texture_frame_snapshot(ready_plan, ready_execution);
    const render_image_texture_frame_binding_summary ready_summary =
        make_render_image_texture_frame_binding_summary(
            make_render_image_texture_frame_upload_handoff_summary(
                ready_frame,
                make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
                    ready_uploader.diagnostic_snapshot())));

    require(ready_summary.ok(), "ready binding summary is ok");
    require(
        ready_summary.status == render_image_texture_frame_binding_summary_status::fully_upload_backed,
        "ready binding summary status is fully upload-backed");
    require(ready_summary.status_name == "fully_upload_backed", "ready binding summary status name is stable");
    require(ready_summary.fully_upload_backed, "ready binding summary records fully upload-backed state");
    require(ready_summary.requested_texture_count == 2, "ready binding summary records requested refs");
    require(ready_summary.upload_backed_count == 2, "ready binding summary counts upload-backed refs");
    require(ready_summary.placeholder_backed_count == 0, "ready binding summary records no placeholders");
    require(ready_summary.cache_reused, "ready binding summary records cache reuse");
    require(ready_summary.cache_reused_count == 1, "ready binding summary counts cache reuse");
    require(
        ready_summary.diagnostic == "image frame binding summary is fully upload-backed",
        "ready binding summary diagnostic is stable");

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    fake_image_source_bytes_loader placeholder_loader;
    placeholder_loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder placeholder_decoder;
    fake_image_texture_uploader placeholder_uploader;
    fake_image_texture_cache placeholder_cache(placeholder_decoder, placeholder_uploader);
    placeholder_cache.set_placeholder_texture_policy(placeholder_policy);
    fake_image_texture_pipeline placeholder_pipeline(resolver, placeholder_loader, placeholder_cache, placeholder_uploader);

    const render_image_texture_batch_plan placeholder_plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{render_image_ref{.uri = "asset://textures/bad.ppm"}},
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics placeholder_execution =
        execute_render_image_texture_batch_plan(placeholder_plan, placeholder_pipeline);
    const render_image_texture_frame_snapshot placeholder_frame =
        make_render_image_texture_frame_snapshot(placeholder_plan, placeholder_execution);
    const render_image_texture_frame_binding_summary placeholder_summary =
        make_render_image_texture_frame_binding_summary(
            make_render_image_texture_frame_upload_handoff_summary(
                placeholder_frame,
                make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
                    placeholder_uploader.diagnostic_snapshot())));

    require(placeholder_summary.ok(), "placeholder binding summary is ok");
    require(
        placeholder_summary.status == render_image_texture_frame_binding_summary_status::placeholder_backed,
        "placeholder binding summary status is placeholder-backed");
    require(placeholder_summary.placeholder_backed, "placeholder binding summary records placeholder-backed state");
    require(!placeholder_summary.fully_upload_backed, "placeholder binding summary is not fully upload-backed");
    require(placeholder_summary.requested_texture_count == 1, "placeholder binding summary records requested ref");
    require(placeholder_summary.upload_backed_count == 0, "placeholder binding summary records no real upload-backed refs");
    require(placeholder_summary.placeholder_backed_count == 1, "placeholder binding summary counts placeholder-backed ref");
    require(
        placeholder_summary.diagnostic == "image frame binding summary is placeholder-backed",
        "placeholder binding summary diagnostic is stable");
}

void test_texture_frame_binding_summary_reports_missing_and_retry_states()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader loader;
    loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder decoder;
    fake_image_texture_uploader uploader;
    fake_image_texture_cache cache(decoder, uploader);
    fake_image_texture_pipeline pipeline(resolver, loader, cache, uploader);

    const render_image_texture_batch_plan plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics execution =
        execute_render_image_texture_batch_plan(plan, pipeline);
    const render_image_texture_frame_snapshot frame =
        make_render_image_texture_frame_snapshot(plan, execution);
    const render_image_texture_upload_result_snapshot upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(uploader.diagnostic_snapshot());

    const render_image_texture_frame_binding_summary missing_upload_summary =
        make_render_image_texture_frame_binding_summary(
            make_render_image_texture_frame_upload_handoff_summary(
                frame,
                render_image_texture_upload_result_snapshot{}));
    require(
        missing_upload_summary.status == render_image_texture_frame_binding_summary_status::missing_upload_result,
        "binding summary reports missing upload result status");
    require(missing_upload_summary.missing_upload_result, "binding summary records missing upload result flag");
    require(missing_upload_summary.missing_upload_result_count == 1, "binding summary counts missing upload result");
    require(!missing_upload_summary.ok(), "missing upload result binding summary is not ok");

    const render_image_texture_frame_binding_summary missing_binding_summary =
        make_render_image_texture_frame_binding_summary(
            make_render_image_texture_frame_upload_handoff_summary(
                render_image_texture_frame_snapshot{},
                upload_result));
    require(
        missing_binding_summary.status == render_image_texture_frame_binding_summary_status::missing_frame_binding,
        "binding summary reports missing frame binding status");
    require(missing_binding_summary.missing_frame_binding, "binding summary records missing frame binding flag");
    require(missing_binding_summary.missing_frame_binding_count == 1, "binding summary counts missing frame binding");
    require(missing_binding_summary.requested_texture_count == 0, "missing frame binding summary has no requested refs");
    require(!missing_binding_summary.ok(), "missing frame binding summary is not ok");

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    fake_image_source_bytes_loader retry_loader;
    retry_loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder retry_decoder;
    fake_image_texture_uploader retry_uploader;
    fake_image_texture_cache retry_cache(retry_decoder, retry_uploader);
    retry_cache.set_placeholder_texture_policy(placeholder_policy);
    fake_image_texture_pipeline retry_pipeline(resolver, retry_loader, retry_cache, retry_uploader);

    const render_image_texture_batch_plan retry_plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{render_image_ref{.uri = "asset://textures/bad.ppm"}},
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics retry_execution =
        execute_render_image_texture_batch_plan(retry_plan, retry_pipeline);
    const render_image_texture_frame_snapshot retry_frame =
        make_render_image_texture_frame_snapshot(retry_plan, retry_execution);
    render_image_texture_upload_result_snapshot retry_upload_result =
        make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(retry_uploader.diagnostic_snapshot());

    render_image_texture_upload_result_packet_snapshot& retry_packet = retry_upload_result.packets[0];
    retry_packet.status = render_image_texture_upload_result_packet_status::rejected_retryable;
    retry_packet.status_name = render_image_texture_upload_result_packet_status_name(retry_packet.status);
    retry_packet.accepted = false;
    retry_packet.rejected = true;
    retry_packet.blocked = true;
    retry_packet.retryable = true;
    retry_packet.uploaded_byte_count = 0;
    retry_packet.retry_eligibility_name = "eligible";
    retry_packet.retry_after_queue_sequence_delta = 3;
    retry_packet.next_retry_sequence = 11;
    retry_packet.blocker_summary = "upload failed but can retry: invalid_image";
    retry_upload_result.accepted_packet_count = 0;
    retry_upload_result.rejected_packet_count = 1;
    retry_upload_result.retryable_rejected_packet_count = 1;
    retry_upload_result.blocker_count = 1;
    retry_upload_result.has_rejections = true;
    retry_upload_result.has_retryable_rejections = true;
    retry_upload_result.total_uploaded_byte_count = 0;

    const render_image_texture_frame_binding_summary retry_summary =
        make_render_image_texture_frame_binding_summary(
            make_render_image_texture_frame_upload_handoff_summary(retry_frame, retry_upload_result));
    require(
        retry_summary.status == render_image_texture_frame_binding_summary_status::retry_backoff_blocked,
        "binding summary reports retry backoff blocked status");
    require(retry_summary.retry_backoff_blocked, "binding summary records retry backoff flag");
    require(retry_summary.retry_backoff_blocked_count == 1, "binding summary counts retry blocker");
    require(retry_summary.placeholder_backed_count == 1, "retry binding summary preserves placeholder evidence");
    require(retry_summary.placeholder_backed, "retry binding summary records placeholder evidence");
    require(retry_summary.max_retry_after_queue_sequence_delta == 3, "binding summary records retry backoff delta");
    require(retry_summary.next_retry_sequence == 11, "binding summary records next retry sequence");
    require(
        retry_summary.retry_backoff_summary == "upload failed but can retry: invalid_image",
        "binding summary retry summary is stable");
    require(!retry_summary.ok(), "retry blocked binding summary is not ok");
}

void test_texture_frame_binding_summary_diff_reports_sampler_and_cache_key_changes()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;
    fake_image_source_bytes_loader before_loader;
    before_loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder before_decoder;
    fake_image_texture_uploader before_uploader;
    fake_image_texture_cache before_cache(before_decoder, before_uploader);
    fake_image_texture_pipeline before_pipeline(resolver, before_loader, before_cache, before_uploader);

    const render_image_texture_batch_plan before_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics before_execution =
        execute_render_image_texture_batch_plan(before_plan, before_pipeline);
    const render_image_texture_frame_snapshot before_frame =
        make_render_image_texture_frame_snapshot(before_plan, before_execution);
    const render_image_texture_frame_binding_summary before_summary =
        make_render_image_texture_frame_binding_summary(
            make_render_image_texture_frame_upload_handoff_summary(
                before_frame,
                make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
                    before_uploader.diagnostic_snapshot())));

    render_image_sampler_policy nearest_sampler;
    nearest_sampler.min_filter = render_image_filter::nearest;
    nearest_sampler.mag_filter = render_image_filter::nearest;

    fake_image_source_bytes_loader after_loader;
    after_loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder after_decoder;
    fake_image_texture_uploader after_uploader;
    fake_image_texture_cache after_cache(after_decoder, after_uploader);
    fake_image_texture_pipeline after_pipeline(resolver, after_loader, after_cache, after_uploader);

    const render_image_texture_batch_plan after_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm", .sampler = nearest_sampler},
    });
    const render_image_texture_batch_execution_diagnostics after_execution =
        execute_render_image_texture_batch_plan(after_plan, after_pipeline);
    const render_image_texture_frame_snapshot after_frame =
        make_render_image_texture_frame_snapshot(after_plan, after_execution);
    const render_image_texture_frame_binding_summary after_summary =
        make_render_image_texture_frame_binding_summary(
            make_render_image_texture_frame_upload_handoff_summary(
                after_frame,
                make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
                    after_uploader.diagnostic_snapshot())));

    const render_image_texture_frame_binding_summary_diff diff =
        diff_render_image_texture_frame_binding_summaries(before_summary, after_summary);

    require(diff.ok(), "binding summary diff with sampler change is ok");
    require(diff.has_changes, "binding summary diff records changes");
    require(!diff.has_regression, "binding summary diff records no regression");
    require(!diff.status_changed, "binding summary status is unchanged");
    require(diff.cache_key_summary_changed, "binding summary diff records cache-key change");
    require(diff.sampler_summary_changed, "binding summary diff records sampler change");
    require(diff.cache_key_or_sampler_changed, "binding summary diff records combined key/sampler change");
    require(diff.upload_backed_delta == 0, "binding summary diff records stable upload-backed count");
    require(
        diff.changed_summary == "cache keys changed; samplers changed",
        "binding summary diff changed summary is stable");
    require(
        diff.diagnostic == "image frame binding summary diff reports changes",
        "binding summary diff diagnostic is stable");
}

void test_texture_frame_upload_handoff_diff_reports_frame_to_frame_evidence()
{
    using namespace quiz_vulkan::render;

    const normalizing_image_resolver resolver;

    fake_image_source_bytes_loader ready_loader;
    ready_loader.set_source_bytes("asset://textures/card.ppm", make_ppm_2x1_fixture_bytes());
    ppm_image_decoder ready_decoder;
    fake_image_texture_uploader ready_uploader;
    fake_image_texture_cache ready_cache(ready_decoder, ready_uploader);
    fake_image_texture_pipeline ready_pipeline(resolver, ready_loader, ready_cache, ready_uploader);
    const render_image_texture_batch_plan ready_plan = plan_render_image_texture_batch(std::vector<render_image_ref>{
        render_image_ref{.uri = "asset://textures/card.ppm"},
    });
    const render_image_texture_batch_execution_diagnostics ready_execution =
        execute_render_image_texture_batch_plan(ready_plan, ready_pipeline);
    const render_image_texture_frame_snapshot ready_frame =
        make_render_image_texture_frame_snapshot(ready_plan, ready_execution);
    const render_image_texture_frame_upload_handoff_summary ready_handoff =
        make_render_image_texture_frame_upload_handoff_summary(
            ready_frame,
            make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
                ready_uploader.diagnostic_snapshot()));

    fake_image_texture_placeholder_policy placeholder_policy{
        .enabled = true,
        .width = 2,
        .height = 2,
    };
    fake_image_source_bytes_loader partial_loader;
    partial_loader.set_source_bytes("asset://textures/bad.ppm", make_short_ppm_2x1_fixture_bytes());
    ppm_image_decoder partial_decoder;
    fake_image_texture_uploader partial_uploader;
    fake_image_texture_cache partial_cache(partial_decoder, partial_uploader);
    partial_cache.set_placeholder_texture_policy(placeholder_policy);
    fake_image_texture_pipeline partial_pipeline(resolver, partial_loader, partial_cache, partial_uploader);
    const render_image_texture_batch_plan partial_plan = plan_render_image_texture_batch(
        std::vector<render_image_ref>{
            render_image_ref{.uri = "   "},
            render_image_ref{.uri = "asset://textures/bad.ppm"},
        },
        render_image_texture_batch_plan_options{.placeholder_policy = placeholder_policy});
    const render_image_texture_batch_execution_diagnostics partial_execution =
        execute_render_image_texture_batch_plan(partial_plan, partial_pipeline);
    const render_image_texture_frame_snapshot partial_frame =
        make_render_image_texture_frame_snapshot(partial_plan, partial_execution);
    const render_image_texture_frame_upload_handoff_summary partial_handoff =
        make_render_image_texture_frame_upload_handoff_summary(
            ready_frame,
            partial_frame,
            make_render_image_texture_upload_result_snapshot_from_fake_upload_snapshot(
                partial_uploader.diagnostic_snapshot()));

    require(!partial_handoff.ok(), "partial handoff is blocked before diffing");
    require(partial_handoff.has_frame_delta, "partial handoff carries frame delta evidence");
    require(partial_handoff.added_in_frame_count == 1, "partial handoff records added frame request");
    require(partial_handoff.changed_in_frame_count == 1, "partial handoff records changed frame request");
    require(partial_handoff.readiness_changed_count == 2, "partial handoff records readiness changes");
    require(partial_handoff.readiness_regressed_count == 1, "partial handoff records readiness regression");
    require(
        partial_handoff.frame_delta_summary == "added=1; changed=1; removed=0; readiness_changed=2",
        "partial handoff frame delta summary is stable");

    const render_image_texture_frame_upload_handoff_summary_diff diff =
        diff_render_image_texture_frame_upload_handoff_summaries(ready_handoff, partial_handoff);

    require(!diff.ok(), "frame upload handoff diff records regression");
    require(diff.has_changes, "frame upload handoff diff records changes");
    require(diff.has_regression, "frame upload handoff diff records regression");
    require(diff.before_requested_texture_count == 1, "handoff diff records before request count");
    require(diff.after_requested_texture_count == 2, "handoff diff records after request count");
    require(diff.requested_texture_delta == 1, "handoff diff records request delta");
    require(diff.before_ready_texture_count == 1, "handoff diff records before ready count");
    require(diff.after_ready_texture_count == 0, "handoff diff records after ready count");
    require(diff.ready_texture_delta == -1, "handoff diff records ready regression delta");
    require(diff.placeholder_texture_delta == 1, "handoff diff records placeholder delta");
    require(diff.blocked_texture_delta == 1, "handoff diff records blocked delta");
    require(diff.uploaded_byte_delta == 8, "handoff diff records uploaded byte delta");
    require(diff.added_entry_count == 1, "handoff diff records added entry");
    require(diff.changed_entry_count == 1, "handoff diff records changed entry");
    require(diff.readiness_regressed_count == 1, "handoff diff records readiness regression count");
    require(diff.placeholder_changed_count == 1, "handoff diff records placeholder change count");
    require(diff.blocker_changed_count == 1, "handoff diff records blocker change count");
    require(diff.renderer_handoff_regressed, "handoff diff records renderer handoff regression");
    require(
        diff.regression_summary
            == "upload handoff regressed; ready uploads decreased; blocked uploads increased; "
               "placeholder uploads increased",
        "handoff diff regression summary is concise and stable");
    require(
        diff.diagnostic == "image frame upload handoff diff reports regressions",
        "handoff diff diagnostic is stable");
    require(diff.entries.size() == 2, "handoff diff has one entry per changed request index");

    const render_image_texture_frame_upload_handoff_entry_diff& blocked = diff.entries[0];
    require(
        blocked.status == render_image_texture_frame_upload_handoff_diff_entry_status::changed,
        "blocked handoff diff entry records changed status");
    require(blocked.readiness_regressed, "blocked handoff diff entry records readiness regression");
    require(blocked.blocker_changed, "blocked handoff diff entry records blocker change");
    require(blocked.cache_key_changed, "blocked handoff diff entry records cache key change");
    require(blocked.regression, "blocked handoff diff entry records regression");

    const render_image_texture_frame_upload_handoff_entry_diff& added = diff.entries[1];
    require(
        added.status == render_image_texture_frame_upload_handoff_diff_entry_status::added,
        "added handoff diff entry records added status");
    require(added.placeholder_changed, "added handoff diff entry records placeholder delta");
    require(added.uploaded_byte_changed, "added handoff diff entry records uploaded byte delta");
    require(added.regression, "added handoff diff entry treats added placeholder as regression evidence");
}

} // namespace

int main()
{
    test_filesystem_pipeline_reads_ppm_fixture_and_reuses_cache();
    test_filesystem_pipeline_routes_real_jpeg_through_stb_upload_handoff();
    test_filesystem_pipeline_reports_missing_file_source_load_failed();
    test_filesystem_pipeline_reports_empty_file_source_load_failed();
    test_filesystem_pipeline_reports_malformed_ppm_decode_failed();
    test_upload_readiness_reports_decode_handoff_metadata();
    test_pipeline_uses_optional_third_party_decoder_through_interface();
    test_optional_adapter_failure_falls_back_before_texture_upload();
    test_unavailable_optional_adapter_keeps_diagnostics_without_upload();
    test_stb_selection_preserves_internal_bmp_decoder_diagnostics();
    test_stb_selection_missing_and_mismatched_fallback_diagnostics_reach_pipeline();
    test_stb_selection_survives_placeholder_fallback();
    test_external_decoder_selection_diff_reports_adapter_internal_and_placeholder_states();
    test_external_decoder_selection_diff_reports_missing_and_version_mismatch_fallbacks();
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
    test_texture_frame_snapshot_diff_reports_unchanged_frame();
    test_texture_frame_snapshot_diff_reports_regressions_and_deltas();
    test_texture_frame_binding_plan_maps_renderer_handoff_packets();
    test_texture_frame_binding_plan_reports_frame_deltas_and_failures();
    test_texture_frame_binding_plan_diff_reports_unchanged_bindings();
    test_texture_frame_binding_plan_diff_reports_binding_deltas();
    test_texture_frame_upload_handoff_links_bindings_to_upload_results();
    test_texture_frame_upload_handoff_reports_placeholders_and_blockers();
    test_texture_frame_upload_handoff_preserves_blocked_placeholder_retry_evidence();
    test_texture_frame_upload_handoff_distinguishes_missing_upload_and_missing_binding();
    test_texture_frame_upload_handoff_diff_reports_cache_key_and_sampler_changes();
    test_texture_frame_binding_summary_reports_ready_reuse_and_placeholder_states();
    test_texture_frame_binding_summary_reports_missing_and_retry_states();
    test_texture_frame_binding_summary_diff_reports_sampler_and_cache_key_changes();
    test_texture_frame_upload_handoff_diff_reports_frame_to_frame_evidence();
    return 0;
}
