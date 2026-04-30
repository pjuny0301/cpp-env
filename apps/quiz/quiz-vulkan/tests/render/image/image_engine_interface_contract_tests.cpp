#include "render/image/image_decoder.h"
#include "render/image/image_resolver.h"
#include "render/image/image_source_bytes_loader.h"
#include "render/image/image_texture_cache.h"
#include "render/image/image_texture_pipeline.h"
#include "render/render_draw_list.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace quiz_vulkan;

template <typename T>
concept ImageResolverInterface = requires(
    const T& resolver,
    const render::render_image_resolve_request& request) {
    { resolver.resolve(request) } -> std::same_as<render::render_image_resolve_result>;
};

template <typename T>
concept ImageDecoderInterface = requires(
    const T& decoder,
    const render::render_image_decode_request& request) {
    { decoder.supports(request) } -> std::same_as<bool>;
    { decoder.decode(request) } -> std::same_as<render::render_image_decode_result>;
};

template <typename T>
concept ImageSourceBytesLoaderInterface = requires(
    const T& loader,
    const render::render_image_source_bytes_load_request& request) {
    { loader.load(request) } -> std::same_as<render::render_image_source_bytes_load_result>;
};

template <typename T>
concept ImageTextureCacheInterface = requires(
    T& cache,
    const render::render_image_texture_request& request) {
    { cache.acquire_texture(request) } -> std::same_as<render::render_image_texture_result>;
    { cache.release_unused() } -> std::same_as<void>;
};

template <typename T>
concept ImageTextureUploaderInterface = requires(
    T& uploader,
    const render::render_image_texture_upload_request& request) {
    { uploader.upload(request) } -> std::same_as<render::render_image_texture_upload_result>;
};

template <typename T>
concept ImageTexturePipelineInterface = requires(
    T& pipeline,
    const render::render_image_texture_pipeline_request& request) {
    { pipeline.acquire_texture(request) } -> std::same_as<render::render_image_texture_pipeline_result>;
};

static_assert(ImageResolverInterface<render::image_resolver_interface>);
static_assert(ImageResolverInterface<render::normalizing_image_resolver>);
static_assert(ImageDecoderInterface<render::image_decoder_interface>);
static_assert(ImageDecoderInterface<render::image_decoder_chain>);
static_assert(ImageDecoderInterface<render::fake_image_decoder>);
static_assert(ImageDecoderInterface<render::ppm_image_decoder>);
static_assert(ImageSourceBytesLoaderInterface<render::image_source_bytes_loader_interface>);
static_assert(ImageSourceBytesLoaderInterface<render::fake_image_source_bytes_loader>);
static_assert(ImageTextureCacheInterface<render::image_texture_cache_interface>);
static_assert(ImageTextureCacheInterface<render::fake_image_texture_cache>);
static_assert(ImageTextureUploaderInterface<render::image_texture_uploader_interface>);
static_assert(ImageTextureUploaderInterface<render::fake_image_texture_uploader>);
static_assert(ImageTexturePipelineInterface<render::image_texture_pipeline_interface>);
static_assert(ImageTexturePipelineInterface<render::fake_image_texture_pipeline>);

static_assert(requires(render::render_image_ref image) {
    { image.sampler } -> std::same_as<render::render_image_sampler_policy&>;
});

static_assert(requires(render::render_resolved_image_source source) {
    { source.cache_key() } -> std::same_as<render::render_image_cache_key>;
    { source.is_remote() } -> std::same_as<bool>;
});

static_assert(requires(const render::render_image_texture_request& request, const render::render_image_texture_key& key) {
    { render::make_render_image_texture_key(request) } -> std::same_as<render::render_image_texture_key>;
    { render::is_valid_render_image_texture_key(key) } -> std::same_as<bool>;
});

static_assert(requires(
    const render::render_image_sampler_policy& sampler,
    render::render_image_filter filter,
    render::render_image_mipmap_mode mipmap_mode,
    render::render_image_wrap_mode wrap_mode) {
    { render::is_valid_render_image_filter(filter) } -> std::same_as<bool>;
    { render::is_valid_render_image_mipmap_mode(mipmap_mode) } -> std::same_as<bool>;
    { render::is_valid_render_image_wrap_mode(wrap_mode) } -> std::same_as<bool>;
    { render::is_valid_render_image_sampler_policy(sampler) } -> std::same_as<bool>;
});

static_assert(requires(const render::render_decoded_image& image, render::render_image_pixel_format pixel_format) {
    { render::render_image_pixel_format_byte_count(pixel_format) } -> std::same_as<std::size_t>;
    { render::expected_render_decoded_image_byte_count(image) } -> std::same_as<std::size_t>;
    { render::has_valid_render_decoded_image_payload(image) } -> std::same_as<bool>;
});

static_assert(requires(
    render::render_image_decode_metadata metadata,
    render::render_image_encoded_format encoded_format,
    render::render_image_format_detection_summary format_detection,
    render::render_image_decode_size_validation size_validation,
    render::render_image_decoder_diagnostic diagnostic,
    render::render_image_decode_result decode_result,
    render::render_image_texture_result texture_result,
    render::render_image_source_bytes_load_request bytes_request,
    render::render_image_source_bytes_load_result bytes_result,
    render::render_image_source_bytes_load_status bytes_status,
    render::fake_image_source_bytes_loader source_bytes_loader,
    render::render_image_texture_upload_request upload_request,
    render::render_image_texture_upload_result upload_result,
    render::render_image_texture_upload_status upload_status,
    render::fake_image_texture_upload_generation_id upload_generation_id,
    render::fake_image_texture_upload_request_snapshot upload_request_snapshot,
    render::fake_image_texture_upload_result_snapshot upload_result_snapshot,
    render::fake_image_texture_upload_snapshot_entry upload_entry,
    render::fake_image_texture_upload_snapshot upload_snapshot,
    const render::fake_image_texture_uploader& uploader,
    render::render_image_texture_pipeline_request pipeline_request,
    render::render_image_texture_pipeline_result pipeline_result,
    render::render_image_texture_pipeline_status pipeline_status,
    render::fake_image_texture_pipeline_entry_snapshot pipeline_entry,
    render::fake_image_texture_pipeline_snapshot pipeline_snapshot,
    const render::fake_image_texture_pipeline& pipeline,
    render::fake_image_texture_pipeline& mutable_pipeline,
    render::render_image_ref image_ref,
    render::fake_image_texture_residency texture_residency,
    render::fake_image_texture_cache_entry_snapshot cache_entry,
    render::fake_image_texture_cache_snapshot cache_snapshot,
    const render::fake_image_texture_cache& cache,
    render::fake_image_texture_cache& mutable_cache,
    const render::render_resolved_image_source& source,
    const render::render_image_decode_request& request,
    const render::render_decoded_image& image) {
    { metadata.decoder_id } -> std::same_as<std::string&>;
    { metadata.encoded_byte_count } -> std::same_as<std::size_t&>;
    { metadata.width } -> std::same_as<std::size_t&>;
    { metadata.height } -> std::same_as<std::size_t&>;
    { metadata.pixel_format } -> std::same_as<render::render_image_pixel_format&>;
    { metadata.decoded_byte_count } -> std::same_as<std::size_t&>;
    { metadata.format_detection } -> std::same_as<render::render_image_format_detection_summary&>;
    { metadata.size_validation } -> std::same_as<render::render_image_decode_size_validation&>;
    { metadata.has_image() } -> std::same_as<bool>;
    { format_detection.detected_format } -> std::same_as<render::render_image_encoded_format&>;
    { format_detection.extension_hint } -> std::same_as<std::string&>;
    { format_detection.recognized_signature } -> std::same_as<bool&>;
    { format_detection.placeholder_fallback } -> std::same_as<bool&>;
    { format_detection.diagnostic } -> std::same_as<std::string&>;
    { size_validation.row_stride_byte_count } -> std::same_as<std::size_t&>;
    { size_validation.expected_decoded_byte_count } -> std::same_as<std::size_t&>;
    { size_validation.actual_decoded_byte_count } -> std::same_as<std::size_t&>;
    { size_validation.valid } -> std::same_as<bool&>;
    { size_validation.diagnostic } -> std::same_as<std::string&>;
    { render::image_decode_extension_hint("asset://card.PNG") } -> std::same_as<std::string>;
    { render::detect_render_image_format(request) } -> std::same_as<render::render_image_format_detection_summary>;
    { render::render_image_decode_pixel_format_byte_count(render::render_image_pixel_format::rgba8_srgb) }
        -> std::same_as<std::size_t>;
    { render::validate_render_image_decode_size(image) } -> std::same_as<render::render_image_decode_size_validation>;
    { render::with_placeholder_fallback(format_detection, "fake") }
        -> std::same_as<render::render_image_format_detection_summary>;
    encoded_format;
    { diagnostic.decoder_id } -> std::same_as<std::string&>;
    { diagnostic.supported } -> std::same_as<bool&>;
    { diagnostic.status } -> std::same_as<render::render_image_decode_status&>;
    { diagnostic.diagnostic } -> std::same_as<std::string&>;
    { decode_result.metadata } -> std::same_as<render::render_image_decode_metadata&>;
    { decode_result.decoder_diagnostics } -> std::same_as<std::vector<render::render_image_decoder_diagnostic>&>;
    { texture_result.decode_metadata } -> std::same_as<render::render_image_decode_metadata&>;
    { texture_result.decoder_diagnostics } -> std::same_as<std::vector<render::render_image_decoder_diagnostic>&>;
    { render::make_render_image_decode_metadata("decoder", request) } -> std::same_as<render::render_image_decode_metadata>;
    { render::make_render_image_decode_metadata("decoder", request, image) }
        -> std::same_as<render::render_image_decode_metadata>;
    { bytes_request.source } -> std::same_as<render::render_resolved_image_source&>;
    { bytes_result.status } -> std::same_as<render::render_image_source_bytes_load_status&>;
    { bytes_result.source } -> std::same_as<render::render_resolved_image_source&>;
    { bytes_result.encoded_bytes } -> std::same_as<std::vector<std::byte>&>;
    { bytes_result.diagnostic } -> std::same_as<std::string&>;
    { bytes_result.ok() } -> std::same_as<bool>;
    { source_bytes_loader.set_source_bytes(render::render_image_cache_key{}, std::vector<std::byte>{}) }
        -> std::same_as<void>;
    { source_bytes_loader.set_source_bytes(source, std::vector<std::byte>{}) } -> std::same_as<void>;
    { source_bytes_loader.clear_source_bytes(render::render_image_cache_key{}) } -> std::same_as<void>;
    { source_bytes_loader.has_source_bytes(source) } -> std::same_as<bool>;
    { render::is_valid_image_source_bytes_cache_key("asset://card.ppm") } -> std::same_as<bool>;
    bytes_status;
    { upload_request.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_request.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { upload_request.image } -> std::same_as<render::render_decoded_image&>;
    { upload_result.status } -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_result.generation_id } -> std::same_as<std::uint64_t&>;
    { upload_result.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_result.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { upload_result.texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_result.pixel_count } -> std::same_as<std::size_t&>;
    { upload_result.pixel_byte_count } -> std::same_as<std::size_t&>;
    { upload_result.decoded_byte_count } -> std::same_as<std::size_t&>;
    { upload_result.staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_result.diagnostic } -> std::same_as<std::string&>;
    { upload_result.ok() } -> std::same_as<bool>;
    { upload_request_snapshot.generation_id } -> std::same_as<render::fake_image_texture_upload_generation_id&>;
    { upload_request_snapshot.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_request_snapshot.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { upload_request_snapshot.width } -> std::same_as<std::size_t&>;
    { upload_request_snapshot.height } -> std::same_as<std::size_t&>;
    { upload_request_snapshot.pixel_format } -> std::same_as<render::render_image_pixel_format&>;
    { upload_request_snapshot.pixel_count } -> std::same_as<std::size_t&>;
    { upload_request_snapshot.pixel_byte_count } -> std::same_as<std::size_t&>;
    { upload_request_snapshot.decoded_byte_count } -> std::same_as<std::size_t&>;
    { upload_request_snapshot.staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_result_snapshot.generation_id } -> std::same_as<render::fake_image_texture_upload_generation_id&>;
    { upload_result_snapshot.status } -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_result_snapshot.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_result_snapshot.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { upload_result_snapshot.texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_result_snapshot.pixel_count } -> std::same_as<std::size_t&>;
    { upload_result_snapshot.pixel_byte_count } -> std::same_as<std::size_t&>;
    { upload_result_snapshot.decoded_byte_count } -> std::same_as<std::size_t&>;
    { upload_result_snapshot.staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_result_snapshot.diagnostic } -> std::same_as<std::string&>;
    { upload_entry.generation_id } -> std::same_as<render::fake_image_texture_upload_generation_id&>;
    { upload_entry.request } -> std::same_as<render::fake_image_texture_upload_request_snapshot&>;
    { upload_entry.result } -> std::same_as<render::fake_image_texture_upload_result_snapshot&>;
    { upload_entry.key } -> std::same_as<render::render_image_texture_key&>;
    { upload_entry.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { upload_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { upload_entry.status } -> std::same_as<render::render_image_texture_upload_status&>;
    { upload_entry.pixel_count } -> std::same_as<std::size_t&>;
    { upload_entry.pixel_byte_count } -> std::same_as<std::size_t&>;
    { upload_entry.decoded_byte_count } -> std::same_as<std::size_t&>;
    { upload_entry.staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_entry.diagnostic } -> std::same_as<std::string&>;
    { upload_snapshot.upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.failed_upload_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.uploaded_pixel_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.uploaded_pixel_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.uploaded_decoded_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.staged_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.attempted_staging_byte_count } -> std::same_as<std::size_t&>;
    { upload_snapshot.next_generation_id } -> std::same_as<render::fake_image_texture_upload_generation_id&>;
    { upload_snapshot.request_snapshots }
        -> std::same_as<std::vector<render::fake_image_texture_upload_request_snapshot>&>;
    { upload_snapshot.result_snapshots }
        -> std::same_as<std::vector<render::fake_image_texture_upload_result_snapshot>&>;
    { upload_snapshot.entries } -> std::same_as<std::vector<render::fake_image_texture_upload_snapshot_entry>&>;
    { uploader.diagnostic_snapshot() } -> std::same_as<render::fake_image_texture_upload_snapshot>;
    upload_generation_id;
    upload_status;
    { pipeline_request.uri } -> std::same_as<std::string&>;
    { pipeline_request.sampler } -> std::same_as<render::render_image_sampler_policy&>;
    { pipeline_result.status } -> std::same_as<render::render_image_texture_pipeline_status&>;
    { pipeline_result.resolve } -> std::same_as<render::render_image_resolve_result&>;
    { pipeline_result.source_bytes } -> std::same_as<render::render_image_source_bytes_load_result&>;
    { pipeline_result.texture } -> std::same_as<render::render_image_texture_result&>;
    { pipeline_result.diagnostic } -> std::same_as<std::string&>;
    { pipeline_result.ok() } -> std::same_as<bool>;
    { render::make_render_image_texture_pipeline_request("asset://card.ppm") }
        -> std::same_as<render::render_image_texture_pipeline_request>;
    { render::make_render_image_texture_pipeline_request(image_ref) }
        -> std::same_as<render::render_image_texture_pipeline_request>;
    { render::pipeline_status_for_texture_result(render::render_image_texture_status::ready) }
        -> std::same_as<render::render_image_texture_pipeline_status>;
    pipeline_status;
    { pipeline_entry.sequence } -> std::same_as<std::size_t&>;
    { pipeline_entry.request } -> std::same_as<render::render_image_texture_pipeline_request&>;
    { pipeline_entry.status } -> std::same_as<render::render_image_texture_pipeline_status&>;
    { pipeline_entry.resolve_status } -> std::same_as<render::render_image_resolve_status&>;
    { pipeline_entry.source_bytes_status } -> std::same_as<render::render_image_source_bytes_load_status&>;
    { pipeline_entry.texture_status } -> std::same_as<render::render_image_texture_status&>;
    { pipeline_entry.source_key } -> std::same_as<render::render_image_cache_key&>;
    { pipeline_entry.texture_key } -> std::same_as<render::render_image_texture_key&>;
    { pipeline_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { pipeline_entry.cache_hit } -> std::same_as<bool&>;
    { pipeline_entry.encoded_byte_count } -> std::same_as<std::size_t&>;
    { pipeline_entry.decode_metadata } -> std::same_as<render::render_image_decode_metadata&>;
    { pipeline_entry.decoder_diagnostics } -> std::same_as<std::vector<render::render_image_decoder_diagnostic>&>;
    { pipeline_entry.upload_count_before } -> std::same_as<std::size_t&>;
    { pipeline_entry.upload_count_after } -> std::same_as<std::size_t&>;
    { pipeline_entry.failed_upload_count_before } -> std::same_as<std::size_t&>;
    { pipeline_entry.failed_upload_count_after } -> std::same_as<std::size_t&>;
    { pipeline_entry.diagnostic } -> std::same_as<std::string&>;
    { pipeline_snapshot.acquire_count } -> std::same_as<std::size_t&>;
    { pipeline_snapshot.ready_count } -> std::same_as<std::size_t&>;
    { pipeline_snapshot.failure_count } -> std::same_as<std::size_t&>;
    { pipeline_snapshot.cache_hit_count } -> std::same_as<std::size_t&>;
    { pipeline_snapshot.source_load_failure_count } -> std::same_as<std::size_t&>;
    { pipeline_snapshot.decode_failure_count } -> std::same_as<std::size_t&>;
    { pipeline_snapshot.upload_failure_count } -> std::same_as<std::size_t&>;
    { pipeline_snapshot.invalidation_count } -> std::same_as<std::size_t&>;
    { pipeline_snapshot.upload_diagnostics_available } -> std::same_as<bool&>;
    { pipeline_snapshot.cache_snapshot } -> std::same_as<render::fake_image_texture_cache_snapshot&>;
    { pipeline_snapshot.upload_snapshot } -> std::same_as<render::fake_image_texture_upload_snapshot&>;
    { pipeline_snapshot.entries } -> std::same_as<std::vector<render::fake_image_texture_pipeline_entry_snapshot>&>;
    { pipeline.diagnostic_snapshot() } -> std::same_as<render::fake_image_texture_pipeline_snapshot>;
    { mutable_pipeline.invalidate_source(render::render_image_cache_key{}) } -> std::same_as<void>;
    { mutable_pipeline.invalidate_texture(render::render_image_texture_key{}) } -> std::same_as<void>;
    { cache_entry.key } -> std::same_as<render::render_image_texture_key&>;
    { cache_entry.texture } -> std::same_as<render::render_image_texture_handle&>;
    { cache_entry.pixel_count } -> std::same_as<std::size_t&>;
    { cache_entry.pixel_byte_count } -> std::same_as<std::size_t&>;
    { cache_entry.decoded_byte_count } -> std::same_as<std::size_t&>;
    { cache_entry.last_used_sequence } -> std::same_as<std::size_t&>;
    { cache_entry.residency } -> std::same_as<render::fake_image_texture_residency&>;
    { cache_snapshot.texture_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.max_cached_pixel_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.cached_pixel_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.cached_pixel_byte_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.cached_decoded_byte_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.pinned_texture_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.evictable_texture_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.pinned_pixel_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.evictable_pixel_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.eviction_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.over_capacity_texture_count } -> std::same_as<std::size_t&>;
    { cache_snapshot.capacity_exceeded } -> std::same_as<bool&>;
    { cache_snapshot.entries } -> std::same_as<std::vector<render::fake_image_texture_cache_entry_snapshot>&>;
    { cache.diagnostic_snapshot() } -> std::same_as<render::fake_image_texture_cache_snapshot>;
    { cache.eviction_count() } -> std::same_as<std::size_t>;
    { cache.over_capacity_texture_count() } -> std::same_as<std::size_t>;
    { cache.is_texture_pinned(render::render_image_texture_key{}) } -> std::same_as<bool>;
    { mutable_cache.set_texture_residency(render::render_image_texture_key{}, texture_residency) }
        -> std::same_as<void>;
    { mutable_cache.pin_texture(render::render_image_texture_key{}) } -> std::same_as<void>;
    { mutable_cache.unpin_texture(render::render_image_texture_key{}) } -> std::same_as<void>;
    texture_residency;
});

} // namespace
